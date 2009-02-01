/**
 * @file cl_campaign.c
 * @brief Single player campaign control.
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "../cl_global.h"
#include "../cl_game.h"
#include "../cl_team.h"
#include "cl_mapfightequip.h"
#include "cl_airfightmap.h"
#include "cp_hospital.h"
#include "cp_hospital_callbacks.h"
#include "cp_base_callbacks.h"
#include "cp_team.h"
#include "cp_team_callbacks.h"
#include "../cl_actor.h"
#include "cl_popup.h"
#include "../cl_view.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "cl_uforecovery.h"
#include "cl_alienbase.h"
#include "../menu/m_popup.h"
#include "../menu/node/m_node_container.h"
#include "cl_installation.h"
#include "cp_alien_interest.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"
#include "cp_nations.h"
#include "cp_time.h"
#include "cp_xvi.h"
#include "cp_aircraft_callbacks.h"
#include  "cl_fightequip_callbacks.h"
#include "../renderer/r_draw.h"
#include "../renderer/r_overlay.h"

/* public vars */
campaign_t *curCampaign;			/**< Current running campaign */
ccs_t ccs;
base_t *baseCurrent;				/**< Pointer to current base. */
stats_t campaignStats;
missionResults_t missionresults;

campaign_t campaigns[MAX_CAMPAIGNS];
int numCampaigns;
salary_t salaries[MAX_CAMPAIGNS];
cvar_t *cl_campaign;

/**
 * @brief Check if a map may be selected for mission.
 * @param[in] mission Pointer to the mission where mapDef should be added
 * @param[in] pos position of the mission (NULL if the position will be chosen afterwards)
 * @param[in] mapIdx idx of the map in csi.mds[]
 * @param[in] ufoCrashed Search the mission definition for crash ufo id if true
 * @return qfalse if map is not selectable
 */
static qboolean CP_MapIsSelectable (mission_t *mission, int mapIdx, const vec2_t pos, qboolean ufoCrashed)
{
	mapDef_t *md;

	assert(mapIdx >= 0);
	assert(mapIdx < csi.numMDs);

	md = &csi.mds[mapIdx];
	if (md->storyRelated)
		return qfalse;

	if (pos && !MAP_PositionFitsTCPNTypes(pos, md->terrains, md->cultures, md->populations, NULL))
		return qfalse;

	if (!mission->ufo) {
		/* a mission without UFO should not use a map with UFO */
		if (md->ufos)
			return qfalse;
	} else {
		/* A mission with UFO should use a map with UFO
		 * first check that list is not empty */
		if (!md->ufos)
			return qfalse;
		if (ufoCrashed) {
			if (!LIST_ContainsString(md->ufos, UFO_CrashedTypeToShortName(mission->ufo->ufotype)))
				return qfalse;
		} else {
			if (!LIST_ContainsString(md->ufos, UFO_TypeToShortName(mission->ufo->ufotype)))
				return qfalse;
		}
	}

	return qtrue;
}

/**
 * @brief Choose a map for given mission.
 * @param[in|out] mission Pointer to the mission where a new map should be added
 * @param[in] pos position of the mission (NULL if the position will be chosen afterwards)
 * @param[in] ufoCrashed true if the ufo is crashed
 * @return qfalse if could not set mission
 */
qboolean CP_ChooseMap (mission_t *mission, const vec2_t pos, qboolean ufoCrashed)
{
	int i;
	int maxHits = 1;	/**< Total number of maps fulfilling mission conditions. */
	int hits = 0;		/**< Number of maps fulfilling mission conditions and that appeared less often during game. */
	int minMissionAppearance = 9999;
	int randomNum;

	mission->mapDef = NULL;

	/* Set maxHits and hits. */
	while (maxHits) {
		maxHits = 0;
		for (i = 0; i < csi.numMDs; i++) {
			mapDef_t *md;

			/* Check if mission fulfill conditions */
			if (!CP_MapIsSelectable(mission, i, pos, ufoCrashed))
				continue;

			maxHits++;
			md = &csi.mds[i];
			if (md->timesAlreadyUsed < minMissionAppearance) {
				/* at least one fulfilling mission as been used less time than minMissionAppearance:
				 * restart the loop with this number of time.
				 * note: this implies that hits must be > 0 after the loop */
				hits = 0;
				minMissionAppearance = md->timesAlreadyUsed;
				break;
			} else if (md->timesAlreadyUsed == minMissionAppearance)
				hits++;
		}

		if (i >= csi.numMDs) {
			/* We scanned all maps in memory without finding a map used less than minMissionAppearance: exit while loop */
			break;
		}
	}

	if (!maxHits) {
		/* no map fulfill the conditions */
		if (ufoCrashed) {
			/* default map for crashsite mission is the crashsite random map assembly */
			mission->mapDef = Com_GetMapDefinitionByID("ufocrash");
			if (!mission->mapDef)
				Sys_Error("Could not find mapdef ufocrash");
			return qtrue;
		} else {
			Com_Printf("CP_ChooseMap: Could not find map with required conditions:\n");
			Com_Printf("  ufo: %s -- pos: ", mission->ufo ? UFO_TypeToShortName(mission->ufo->ufotype) : "none");
			if (pos)
				Com_Printf("%s",MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)) ? " (in water) " : "");
			if (pos)
				Com_Printf("(%.02f, %.02f)\n", pos[0], pos[1]);
			else
				Com_Printf("none\n");
			return qfalse;
		}
	}

	/* If we reached this point, that means that at least 1 map fulfills the conditions of the mission */
	assert(hits);
	assert(hits < csi.numMDs);

	/* set number of mission to select randomly between 0 and hits - 1 */
	randomNum = rand() % hits;

	/* Select mission mission number 'randomnumber' that fulfills the conditions */
	for (i = 0; i < csi.numMDs; i++) {
		mapDef_t *md;

		/* Check if mission fulfill conditions */
		if (!CP_MapIsSelectable(mission, i, pos, ufoCrashed))
			continue;

		md = &csi.mds[i];
		if (md->timesAlreadyUsed > minMissionAppearance)
			continue;

		/* There shouldn't be mission fulfilling conditions used less time than minMissionAppearance */
		assert(md->timesAlreadyUsed == minMissionAppearance);

		if (!randomNum)
			break;
		else
			randomNum--;
	}

	/* A mission must have been selected */
	assert(i < csi.numMDs);

	mission->mapDef = &csi.mds[i];
	Com_DPrintf(DEBUG_CLIENT, "Selected map '%s' (among %i possible maps)\n", mission->mapDef->id, hits);

	return qtrue;
}


/**
 * @brief Spawn a new alien base mission after it has been discovered.
 */
void CP_SpawnAlienBaseMission (alienBase_t *alienBase)
{
	mission_t *mission;

	CP_CreateNewMission(INTERESTCATEGORY_ALIENBASE, qtrue);
	mission = CP_GetLastMissionAdded();
	if (!mission) {
		Com_Printf("CP_SpawnAlienBaseMission: Could not add mission, abort\n");
		return;
	}

	mission->stage = STAGE_BASE_DISCOVERED;
	mission->data = (void *) alienBase;

	mission->mapDef = Com_GetMapDefinitionByID("alienbase");
	if (!mission->mapDef)
		Sys_Error("Could not find mapdef alienbase");

	Vector2Copy(alienBase->pos, mission->pos);

	Com_sprintf(mission->location, sizeof(mission->location), _("Alien base"));

	/* Alien base stay until it's destroyed */
	CP_MissionDisableTimeLimit(mission);
	/* mission appear on geoscape, player can go there */
	CP_MissionAddToGeoscape(mission, qfalse);
}


/**
 * @brief Function to handle the campaign end
 */
void CP_EndCampaign (qboolean won)
{
	CP_CampaignExit();
	assert(!GAME_CP_IsRunning());

	if (won)
		Cvar_Set("mn_afterdrop", "endgame");
	else
		Cvar_Set("mn_afterdrop", "lostgame");
	Com_Drop();
}

/**
 * @brief Checks whether the player has lost the campaign
 */
void CP_CheckLostCondition (qboolean lost, const mission_t* mission, int civiliansKilled)
{
	qboolean endCampaign = qfalse;
	/* fraction of nation that can be below min happiness before the game is lost */
	const float nationBelowLimitPercentage = 0.5f;

	assert(curCampaign);

	if (!endCampaign && ccs.credits < -curCampaign->negativeCreditsUntilLost) {
		MN_RegisterText(TEXT_STANDARD, _("You've gone too far into debt."));
		endCampaign = qtrue;
	}

	/** @todo Should we make the campaign lost when a player loses all his bases?
	 * until he has set up a base again, the aliens might have invaded the whole
	 * world ;) - i mean, removing the credits check here. */
	if (!gd.numBases && ccs.credits < curCampaign->basecost - curCampaign->negativeCreditsUntilLost) {
		MN_RegisterText(TEXT_STANDARD, _("You've lost your bases and don't have enough money to build new ones."));
		endCampaign = qtrue;
	}

	if (!endCampaign) {
		if (CP_GetAverageXVIRate() > curCampaign->maxAllowedXVIRateUntilLost) {
			MN_RegisterText(TEXT_STANDARD, _("You have failed in your charter to protect Earth."
				" Our home and our people have fallen to the alien infection. Only a handful"
				" of people on Earth remain human, and the remaining few no longer have a"
				" chance to stem the tide. Your command is no more; PHALANX is no longer"
				" able to operate as a functioning unit. Nothing stands between the aliens"
				" and total victory."));
			endCampaign = qtrue;
		} else {
			/* check for nation happiness */
			int j, nationBelowLimit = 0;
			for (j = 0; j < gd.numNations; j++) {
				const nation_t *nation = &gd.nations[j];
				if (nation->stats[0].happiness < curCampaign->minhappiness) {
					nationBelowLimit++;
				}
			}
			if (nationBelowLimit >= nationBelowLimitPercentage * gd.numNations) {
				/* lost the game */
				MN_RegisterText(TEXT_STANDARD, _("Under your command, PHALANX operations have"
					" consistently failed to protect nations."
					" The UN, highly unsatisfied with your performance, has decided to remove"
					" you from command and subsequently disbands the PHALANX project as an"
					" effective task force. No further attempts at global cooperation are made."
					" Earth's nations each try to stand alone against the aliens, and eventually"
					" fall one by one."));
				endCampaign = qtrue;
			}
		}
	}

	if (endCampaign) {
		CP_EndCampaign(qfalse);
	}
}

/* Initial fraction of the population in the country where a mission has been lost / won */
#define XVI_LOST_START_PERCENTAGE	0.20f
#define XVI_WON_START_PERCENTAGE	0.05f

/**
 * @brief Updates each nation's happiness and mission win/loss stats.
 * Should be called at the completion or expiration of every mission.
 * The nation where the mission took place will be most affected,
 * surrounding nations will be less affected.
 * @todo This function needs to be updated/simplified once mission scoring is implemented.
 * The mission score will directly affect the nation happiness as per
 * happiness += missionScore * missionWeight
 * where missionScore is the relative score for the mission (a value between -1 and 1) and
 * missionWeight is the absolute effect this mission has (a function of the maximum absolute score possible for the mission)
 */
void CL_HandleNationData (qboolean lost, int civiliansSurvived, int civiliansKilled, int aliensSurvived, int aliensKilled, mission_t * mis)
{
	int i, is_on_Earth = 0;
	const int civilianSum = civiliansKilled + civiliansSurvived;
	const float civilianRatio = civilianSum ? (float)civiliansSurvived / (float)civilianSum : 0.;
	const int alienSum = aliensKilled + aliensSurvived;
	const float alienRatio = alienSum ? (float)aliensKilled / (float)alienSum : 0.;
	const float performance = civilianRatio * 0.5 + alienRatio * 0.5;

	if (lost) {
		campaignStats.missionsLost++;
		ccs.civiliansKilled += civiliansKilled;
	} else
		campaignStats.missionsWon++;

	for (i = 0; i < gd.numNations; i++) {
		nation_t *nation = &gd.nations[i];
		const float difficultyModifier = (float)curCampaign->difficulty * 0.01f;
		float delta_happiness = 0.0f;

		if (lost) {
			if (nation == ccs.battleParameters.nation) {
				/* Strong negative reaction (happiness_factor must be < 0) */
				delta_happiness = - 0.3f * (1.0f - performance);
				is_on_Earth++;
			} else {
				/* Minor negative reaction (5 times lower than if the failed mission were in the nation) */
				delta_happiness = - 0.06f * (1.0f - performance);
			}
		} else {
			if (nation == ccs.battleParameters.nation) {
				/* Strong positive reaction (happiness_factor must be > 0) */
				delta_happiness = 0.3f * performance;
				is_on_Earth++;
			} else {
				/* Minor positive reaction (5 times lower than if the mission were in the nation) */
				delta_happiness = 0.06f * performance;
			}
		}
		/* One mission can only change nation happiness so much in either direction */
		/* the happiness you can gain depends on the difficulty of the campaign */
		if (delta_happiness < - HAPPINESS_MAX_MISSION_IMPACT - difficultyModifier)
			delta_happiness = - HAPPINESS_MAX_MISSION_IMPACT - difficultyModifier;
		else if (delta_happiness > HAPPINESS_MAX_MISSION_IMPACT + difficultyModifier)
			delta_happiness = HAPPINESS_MAX_MISSION_IMPACT + difficultyModifier;

		/* update happiness */
		NAT_SetHappiness(nation, nation->stats[0].happiness + delta_happiness);
	}
	if (!is_on_Earth)
		Com_DPrintf(DEBUG_CLIENT, "CL_HandleNationData: Warning, mission '%s' located in an unknown country '%s'.\n", mis->id, ccs.battleParameters.nation ? ccs.battleParameters.nation->id : "no nation");
	else if (is_on_Earth > 1)
		Com_DPrintf(DEBUG_CLIENT, "CL_HandleNationData: Error, mission '%s' located in many countries '%s'.\n", mis->id, ccs.battleParameters.nation->id);
}

/**
 * @brief Check for mission start, of change of status of mission
 */
static void CP_CheckEvents (void)
{
	const linkedList_t *list = ccs.missions;

	for (; list; list = list->next) {
		mission_t *mission = (mission_t *)list->data;
		if (CP_CheckMissionLimitedInTime(mission) && Date_LaterThan(ccs.date, mission->finalDate)) {
			CP_MissionStageEnd(mission);
		}
	}

	/* Humans start to attacking player. */
	if (!ccs.humansAttackActivated && ccs.overallInterest > 450) {
		ccs.humansAttackActivated = qtrue;
		/* Mark prequesite of "rs_enemy_on_earth" as met. */
		RS_ResearchFinish(RS_GetTechByID("rs_enemy_on_earth_event"));
	}

	/** XVI infection begins.
	 * This triggers cp_start_xvi_spreading (aka CP_StartXVISpreading_f)
	 * as well after news_new_twist is marked as researched)
	 * @todo We could actually call CP_StartXVISpreading_f directly here as well ... dunno if it helps?
	 *		It's at least more script-able if we keep it as-is. Not that that says much ;) */
	if (!ccs.XVISpreadActivated && ccs.overallInterest > 400) {
		/* Mark prequesite of "news_new_twist" as met. */
		RS_ResearchFinish(RS_GetTechByID(XVI_EVENT_NAME));
	}
}

/* =========================================================== */

/**
 * @brief Converts a number of second into a char to display.
 * @param[in] second Number of second.
 * @todo Abstract the code into an extra function (DateConvertSeconds?) see also CL_DateConvertLong
 */
const char* CL_SecondConvert (int second)
{
	static char buffer[6];
	const int hour = second / SECONDS_PER_HOUR;
	const int min = (second - hour * SECONDS_PER_HOUR) / 60;
	Com_sprintf(buffer, sizeof(buffer), "%2i:%02i", hour, min);
	return buffer;
}

static const int monthLength[MONTHS_PER_YEAR] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
 * @brief Converts a number of days into the number of the current month and the current day in this month.
 * @note The seconds from "date" are ignored here.
 * @note The function always starts calculation from Jan. and also catches new years.
 * @param[in] date Contains the number of days to be converted.
 * @param[out] day The day in the month above.
 * @param[out] month The month. [1-12]
 */
void CL_DateConvert (const date_t * date, byte *day, byte *month, short *year)
{
	byte i;
	int d;

	/* Get the year */
	*year = date->day / DAYS_PER_YEAR;

	/* Get the days in the year. */
	d = date->day % DAYS_PER_YEAR;

	/* Subtract days until no full month is left. */
	for (i = 0; i < MONTHS_PER_YEAR; i++) {
		if (d < monthLength[i])
			break;
		d -= monthLength[i];
	}

	/* Prepare return values. */
	*day = d + 1;
	*month = i + 1;	/**< Return month in range [1-12] */
	assert(*month >= 1 && *month <= MONTHS_PER_YEAR);
	assert(*day >= 1 && *day <= monthLength[i]);
}

/**
 * @brief Converts a number of years+months+days into an "day" integer as used in date_t.
 * @param[in] years The number of years to sum up.
 * @param[in] months The number of months to sum up [1-12]
 * @param[in] days The number of days to sum up.
 * @return The total number of days.
 */
int CL_DateCreateDay (const short years, const byte months, const byte days)
{
	int i;
	int day;

	/* Add days of years */
	day = DAYS_PER_YEAR * years;

	/* Add days until no full month is left. */
	for (i = 0; i < months; i++)
		day += monthLength[i];

	day += days - 1;

	return day;
}

/**
 * @brief Converts a number of hours+minutes+seconds into an "sec" integer as used in date_t.
 * @param[in] hours The number of hours to sum up.
 * @param[in] minutes The number of minutes to sum up.
 * @param[in] seconds The number of seconds to sum up.
 * @return The total number of seconds.
 */
int CL_DateCreateSeconds (byte hours, byte minutes, byte seconds)
{
	int sec;

	/* Add seconds of the hours */
	sec = SECONDS_PER_HOUR * hours;

	/* Add seconds of the minutes. */
	sec += 60 * minutes;

	/* Add the rest of the seconds */
	sec += seconds;

	return sec;
}

/**
 * @brief Converts a date from the engine in a (longer) human-readable format.
 * @param[in] date Contains the date to be converted.
 * @param[out] dateLong The converted date.
  */
void CL_DateConvertLong (const date_t * date, dateLong_t * dateLong)
{
	CL_DateConvert(date, &dateLong->day, &dateLong->month, &dateLong->year);
	/**@todo Make this conversion a function as well (DateConvertSeconds?) See also CL_SecondConvert */
	dateLong->hour = date->sec / SECONDS_PER_HOUR;
	dateLong->min = (date->sec - dateLong->hour * SECONDS_PER_HOUR) / 60;
	dateLong->sec = date->sec - dateLong->hour * SECONDS_PER_HOUR - dateLong->min * 60;
}

/**
 * @brief sets market prices at start of the game
 * @sa CP_CampaignInit
 * @sa BS_Load (Market load function)
 * @param[in] load It this an attempt to init the market for a savegame?
 */
void CP_InitMarket (qboolean load)
{
	int i;

	assert(curCampaign);

	/* find the relevant markets */
	curCampaign->marketDef = INV_GetEquipmentDefinitionByID(curCampaign->market);
	if (!curCampaign->marketDef)
		Sys_Error("CP_InitMarket: Could not find market equipment '%s' as given in the campaign definition of '%s'\n",
			curCampaign->id, curCampaign->market);
	curCampaign->asymptoticMarketDef = INV_GetEquipmentDefinitionByID(curCampaign->asymptoticMarket);
	if (!curCampaign->asymptoticMarketDef)
		Sys_Error("CP_InitMarket: Could not find market equipment '%s' as given in the campaign definition of '%s'\n",
			curCampaign->id, curCampaign->asymptoticMarket);

	/* the savegame loading process will get the following values from savefile */
	if (load)
		return;

	for (i = 0; i < csi.numODs; i++) {
		if (ccs.eMarket.ask[i] == 0) {
			ccs.eMarket.ask[i] = csi.ods[i].price;
			ccs.eMarket.bid[i] = floor(ccs.eMarket.ask[i] * BID_FACTOR);
		}

		if (!curCampaign->marketDef->num[i])
			continue;

		if (!RS_IsResearched_ptr(csi.ods[i].tech) && curCampaign->marketDef->num[i] > 0)
			Com_Printf("CP_InitMarket: Could not add item %s to the market - not marked as researched in campaign %s\n", csi.ods[i].id, curCampaign->id);
		else
			/* the other relevant values were already set above */
			ccs.eMarket.num[i] = curCampaign->marketDef->num[i];
	}
}

/**
 * @brief make number of items change every day.
 * @sa CL_CampaignRun
 * @sa daily called
 * @note This function makes items number on market slowly reach the asymptotic number of items defined in equipment.ufo
 * If an item has just been researched, it's not available on market until RESEARCH_LIMIT_DELAY days is reached.
 */
static void CL_CampaignRunMarket (void)
{
	int i;

	assert(curCampaign->marketDef);
	assert(curCampaign->asymptoticMarketDef);

	for (i = 0; i < csi.numODs; i++) {
		technology_t *tech = csi.ods[i].tech;
		const float TYPICAL_TIME = 10.f;			/**< Number of days to reach the asymptotic number of items */
		const int RESEARCH_LIMIT_DELAY = 30;		/**< Numbers of days after end of research to wait in order to have
													 * items added on market */
		int asymptoticNumber;

		if (!tech)
			Sys_Error("No tech that provides '%s'\n", csi.ods[i].id);

		if (RS_IsResearched_ptr(tech) && (curCampaign->marketDef->num[i] != 0 || ccs.date.day > tech->researchedDate.day + RESEARCH_LIMIT_DELAY)) {
			/* if items are researched for more than RESEARCH_LIMIT_DELAY or was on the initial market,
			 * there number tend to the value defined in equipment.ufo.
			 * This value is the asymptotic value if it is not 0, or initial value else */
			asymptoticNumber = curCampaign->asymptoticMarketDef->num[i] ? curCampaign->asymptoticMarketDef->num[i] : curCampaign->marketDef->num[i];
		} else {
			/* items that have just been researched don't appear on market, but they can disappear */
			asymptoticNumber = 0;
		}

		/* Store the evolution of the market in currentEvolution */
		ccs.eMarket.currentEvolution[i] += (asymptoticNumber - ccs.eMarket.num[i]) / TYPICAL_TIME;

		/* Check if new items appeared or disappeared on market */
		if (fabs(ccs.eMarket.currentEvolution[i]) >= 1.0f) {
			const int num = (int)(ccs.eMarket.currentEvolution[i]);
			ccs.eMarket.num[i] += num;
			ccs.eMarket.currentEvolution[i] -= num;
		}
		if (ccs.eMarket.num[i] < 0)
			ccs.eMarket.num[i] = 0;
	}
}

/**
 * @brief delay between actions that must be executed independently of time scale
 * @sa RADAR_CheckUFOSensored
 * @sa UFO_UpdateAlienInterestForAllBasesAndInstallations
 * @sa AB_UpdateStealthForAllBase
 */
const int DETECTION_INTERVAL = (SECONDS_PER_HOUR / 2);

/**
 * @brief Called every frame when we are in geoscape view
 * @note Called for node types MN_MAP and MN_3DMAP
 * @sa MN_DrawMenus
 * @sa CP_NationHandleBudget
 * @sa B_UpdateBaseData
 * @sa CL_CampaignRunAircraft
 * @sa CP_CheckEvents
 */
void CL_CampaignRun (void)
{
	const int currentinterval = (int)floor(ccs.date.sec) % DETECTION_INTERVAL;
	int checks, dt, i;
	int timeAlreadyFlied = 0;	/**< Time already flied by UFO or aircraft due to detection each detectioninterval */

	/* temporary check to find out bug: [1999099] no pilots available */
	assert(gd.numEmployees[EMPL_PILOT]);

	/* advance time */
	ccs.timer += cls.frametime * gd.gameTimeScale;
	checks = currentinterval + (int)floor(ccs.timer);
	checks = (int)(checks / DETECTION_INTERVAL);
	dt = DETECTION_INTERVAL - currentinterval;

	/* Execute every actions that needs to be independant of time speed : every DETECTION_INTERVAL
	 *	- Run UFOs and craft at least every DETECTION_INTERVAL. If detection occurred, break.
	 *	- Check if any new mission is detected
	 *	- Update stealth value of phalanx bases and installations ; alien bases */
	for (i = 0; i < checks; i++) {
		qboolean detection;
		UFO_CampaignRunUFOs(dt);
		CL_CampaignRunAircraft(dt, qfalse);
		detection = CP_CheckNewMissionDetectedOnGeoscape();

		/* Update alien interest for bases */
		UFO_UpdateAlienInterestForAllBasesAndInstallations();

		/* Update how phalanx troop know alien bases */
		AB_UpdateStealthForAllBase();

		timeAlreadyFlied += dt;
		detection |= UFO_CampaignCheckEvents();
		if (detection) {
			ccs.timer = (i + 1) * DETECTION_INTERVAL - currentinterval;
			break;
		}
		dt = DETECTION_INTERVAL;
	}

	if (ccs.timer >= 1.0) {
		/* calculate new date */
		int currenthour;
		dateLong_t date;

		dt = (int)floor(ccs.timer);
		currenthour = (int)floor(ccs.date.sec / SECONDS_PER_HOUR);
		ccs.date.sec += dt;
		ccs.timer -= dt;

		/* compute hourly events  */
		/* (this may run multiple times if the time stepping is > 1 hour at a time) */
		while (currenthour < (int)floor(ccs.date.sec / SECONDS_PER_HOUR)) {
			currenthour++;
			RS_ResearchRun();
			PR_ProductionRun();
			UR_ProcessActive();
			AII_UpdateInstallationDelay();
			AII_RepairAircraft();
			TR_TransferCheck();
			CP_IncreaseAlienInterest();
		}

		/* daily events */
		while (ccs.date.sec > SECONDS_PER_DAY) {
			ccs.date.sec -= SECONDS_PER_DAY;
			ccs.date.day++;
			/* every day */
			B_UpdateBaseData();
			INS_UpdateInstallationData();
			HOS_HospitalRun();
			BDEF_ReloadBattery();
			CP_SpawnNewMissions();
			CP_SpreadXVI();
			NAT_UpdateHappinessForAllNations();
			AB_BaseSearchedByNations();
			CL_CampaignRunMarket();
		}

		/** check for campaign events
		 *  aircraft and UFO already moved during radar detection (see above),
		 *  just make them move the missing part -- if any */
		UFO_CampaignRunUFOs(dt - timeAlreadyFlied);
		/* must be called even if dt = timeAlreadyFlied in order to update radar overlay */
		CL_CampaignRunAircraft(dt - timeAlreadyFlied, qtrue);
		UFO_CampaignCheckEvents();
		AIRFIGHT_CampaignRunBaseDefense(dt);
		CP_CheckEvents();
		CP_CheckLostCondition(qtrue, NULL, 0);
		AIRFIGHT_CampaignRunProjectiles(dt);
		/* Check if there is a base attack mission */
		CP_CheckBaseAttacks_f();

		/* set time cvars */
		CL_DateConvertLong(&ccs.date, &date);
		/* every first day of a month */
		if (date.day == 1 && gd.fund && gd.numBases) {
			CP_NationBackupMonthlyData();
			CP_NationHandleBudget();
			gd.fund = qfalse;
		} else if (date.day > 1)
			gd.fund = qtrue;

		CP_UpdateXVIMapButton();
		UP_GetUnreadMails();
		CL_UpdateTime();
	}
}


/* =========================================================== */


#define MAX_CREDITS 10000000
/**
 * @brief Sets credits and update mn_credits cvar
 *
 * Checks whether credits are bigger than MAX_CREDITS
 */
void CL_UpdateCredits (int credits)
{
	/* credits */
	if (credits > MAX_CREDITS)
		credits = MAX_CREDITS;
	ccs.credits = credits;
	Cvar_Set("mn_credits", va(_("%i c"), ccs.credits));
}

#define MAX_STATS_BUFFER 2048
/**
 * @brief Shows the current stats from stats_t stats
 */
static void CL_StatsUpdate_f (void)
{
	char *pos;
	static char statsBuffer[MAX_STATS_BUFFER];
	int hired[MAX_EMPL];
	int i, j, costs = 0, sum = 0;

	/* delete buffer */
	memset(statsBuffer, 0, sizeof(statsBuffer));
	memset(hired, 0, sizeof(hired));

	pos = statsBuffer;

	/* missions */
	MN_RegisterText(TEXT_STATS_MISSION, pos);
	Com_sprintf(pos, MAX_STATS_BUFFER, _("Won:\t%i\nLost:\t%i\n\n"), campaignStats.missionsWon, campaignStats.missionsLost);

	/* bases */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_BASES, pos);
	Com_sprintf(pos, (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos), _("Built:\t%i\nActive:\t%i\nAttacked:\t%i\n"),
		campaignStats.basesBuild, gd.numBases, campaignStats.basesAttacked),

	/* installations */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_INSTALLATIONS, pos);
	for (i = 0; i < gd.numInstallations; i++) {
		const installation_t *inst = &gd.installations[i];
		Q_strcat(pos, va(_("%s\n"), inst->name), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* nations */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_NATIONS, pos);
	for (i = 0; i < gd.numNations; i++) {
		Q_strcat(pos, va(_("%s\t%s\n"), _(gd.nations[i].name), NAT_GetHappinessString(&gd.nations[i])), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs */
	for (i = 0; i < gd.numEmployees[EMPL_SCIENTIST]; i++) {
		if (gd.employees[EMPL_SCIENTIST][i].hired) {
			costs += SALARY_SCIENTIST_BASE + gd.employees[EMPL_SCIENTIST][i].chr.score.rank * SALARY_SCIENTIST_RANKBONUS;
			hired[EMPL_SCIENTIST]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		if (gd.employees[EMPL_SOLDIER][i].hired) {
			costs += SALARY_SOLDIER_BASE + gd.employees[EMPL_SOLDIER][i].chr.score.rank * SALARY_SOLDIER_RANKBONUS;
			hired[EMPL_SOLDIER]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_WORKER]; i++) {
		if (gd.employees[EMPL_WORKER][i].hired) {
			costs += SALARY_WORKER_BASE + gd.employees[EMPL_WORKER][i].chr.score.rank * SALARY_WORKER_RANKBONUS;
			hired[EMPL_WORKER]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_PILOT]; i++) {
		if (gd.employees[EMPL_PILOT][i].hired) {
			costs += SALARY_PILOT_BASE + gd.employees[EMPL_PILOT][i].chr.score.rank * SALARY_PILOT_RANKBONUS;
			hired[EMPL_PILOT]++;
		}
	}
	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		if (gd.employees[EMPL_ROBOT][i].hired) {
			costs += SALARY_ROBOT_BASE + gd.employees[EMPL_ROBOT][i].chr.score.rank * SALARY_ROBOT_RANKBONUS;
			hired[EMPL_ROBOT]++;
		}
	}

	/* employees - this is between the two costs parts to count the hired employees */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_EMPLOYEES, pos);
	for (i = 0; i < MAX_EMPL; i++) {
		Q_strcat(pos, va(_("%s\t%i\n"), E_GetEmployeeString(i), hired[i]), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}

	/* costs - second part */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_STATS_COSTS, pos);
	Q_strcat(pos, va(_("Employees:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	costs = 0;
	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		for (j = 0; j < base->numAircraftInBase; j++) {
			costs += base->aircraft[j].price * SALARY_AIRCRAFT_FACTOR / SALARY_AIRCRAFT_DIVISOR;
		}
	}
	Q_strcat(pos, va(_("Aircraft:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		costs = SALARY_BASE_UPKEEP;	/* base cost */
		for (j = 0; j < gd.numBuildings[i]; j++) {
			costs += gd.buildings[i][j].varCosts;
		}
		Q_strcat(pos, va(_("Base (%s):\t%i c\n"), base->name, costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}

	costs = SALARY_ADMIN_INITIAL + gd.numEmployees[EMPL_SOLDIER] * SALARY_ADMIN_SOLDIER + gd.numEmployees[EMPL_WORKER] * SALARY_ADMIN_WORKER + gd.numEmployees[EMPL_SCIENTIST] * SALARY_ADMIN_SCIENTIST + gd.numEmployees[EMPL_PILOT] * SALARY_ADMIN_PILOT + gd.numEmployees[EMPL_ROBOT] * SALARY_ADMIN_ROBOT;
	Q_strcat(pos, va(_("Administrative costs:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	sum += costs;

	if (ccs.credits < 0) {
		const float interest = ccs.credits * SALARY_DEBT_INTEREST;

		costs = (int)ceil(interest);
		Q_strcat(pos, va(_("Debt:\t%i c\n"), costs), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
		sum += costs;
	}
	Q_strcat(pos, va(_("\n\t-------\nSum:\t%i c\n"), sum), (ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* campaign */
	pos += (strlen(pos) + 1);
	MN_RegisterText(TEXT_GENERIC, pos);
	Q_strcat(pos, va(_("Max. allowed debts: %ic\n"), curCampaign->negativeCreditsUntilLost),
		(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));

	/* only show the xvi spread data when it's available */
	if (ccs.XVISpreadActivated) {
		Q_strcat(pos, va(_("Max. allowed eXtraterrestial Viral Infection: %i%%\n"
			"Current eXtraterrestial Viral Infection: %i%%"),
			curCampaign->maxAllowedXVIRateUntilLost,
			CP_GetAverageXVIRate()),
			(ptrdiff_t)(&statsBuffer[MAX_STATS_BUFFER] - pos));
	}
}

/**
 * @brief Load callback for campaign data
 * @sa CP_Save
 * @sa SAV_GameSave
 * @sa CP_SpawnCrashSiteMission
 */
qboolean CP_Load (sizebuf_t *sb, void *data)
{
	int i, j, num;
	const char *name, *missionId;

	/* read campaign name */
	name = MSG_ReadString(sb);

	for (i = 0, curCampaign = campaigns; i < numCampaigns; i++, curCampaign++)
		if (!Q_strncmp(name, curCampaign->id, MAX_VAR))
			break;

	if (i == numCampaigns) {
		Com_Printf("......campaign \"%s\" doesn't exist.\n", name);
		curCampaign = NULL;
		return qfalse;
	}

	CP_CampaignInit(qtrue);

	/* init the map images and reset the map actions */
	MAP_Init();

	memset(&ccs, 0, sizeof(ccs));

	gd.fund = MSG_ReadByte(sb);
	gd.nextUCN = MSG_ReadShort(sb);

	/* read date */
	ccs.date.day = MSG_ReadLong(sb);
	ccs.date.sec = MSG_ReadLong(sb);

	/* read map view */
	ccs.center[0] = MSG_ReadFloat(sb);
	ccs.center[1] = MSG_ReadFloat(sb);
	ccs.angles[0] = MSG_ReadFloat(sb);
	ccs.angles[1] = MSG_ReadFloat(sb);
	ccs.zoom = MSG_ReadFloat(sb);

	/* restore the overlay.
	 * do not use Cvar_SetValue, because this function check if value->string are equal to skip calculation
	 * and we never set r_geoscape_overlay->string in game: r_geoscape_overlay won't be updated if the loaded
	 * value is 0 (and that's a problem if you're loading a game when r_geoscape_overlay is set to another value */
	r_geoscape_overlay->integer = MSG_ReadShort(sb);
	radarOverlayWasSet = MSG_ReadShort(sb);

	/* read credits */
	CL_UpdateCredits(MSG_ReadLong(sb));

	/* store interest values */
	ccs.lastInterestIncreaseDelay = MSG_ReadShort(sb);
	ccs.lastMissionSpawnedDelay = MSG_ReadShort(sb);
	ccs.overallInterest = MSG_ReadShort(sb);
	for (i = 0; i < presaveArray[PRE_MAXCAT]; i++)
		ccs.interest[i] = MSG_ReadShort(sb);

	/* read other campaign data */
	ccs.civiliansKilled = MSG_ReadShort(sb);
	ccs.aliensKilled = MSG_ReadShort(sb);
	ccs.XVISpreadActivated = MSG_ReadByte(sb);
	ccs.XVIShowMap = MSG_ReadByte(sb);
	ccs.humansAttackActivated = MSG_ReadByte(sb);

	CP_UpdateXVIMapButton();

	/* read missions */
	num = MSG_ReadShort(sb);
	for (i = 0; i < num; i++) {
		mission_t mission;
		int ufoIdx;

		memset(&mission, 0, sizeof(mission));

		name = MSG_ReadString(sb);
		if (name[0] != '\0') {
			mission.mapDef = Com_GetMapDefinitionByID(name);
			if (!mission.mapDef) {
				Com_Printf("......mapdef \"%s\" doesn't exist.\n", name);
				return qfalse;
			}
			mission.mapDef->timesAlreadyUsed = MSG_ReadLong(sb);
		} else
			mission.mapDef = NULL;
		Q_strncpyz(mission.id, MSG_ReadString(sb), sizeof(mission.id));
		mission.active = MSG_ReadByte(sb);
		Q_strncpyz(mission.onwin, MSG_ReadString(sb), sizeof(mission.onwin));
		Q_strncpyz(mission.onlose, MSG_ReadString(sb), sizeof(mission.onlose));
		mission.category = MSG_ReadShort(sb);
		mission.stage = MSG_ReadShort(sb);
		switch (mission.category) {
		case INTERESTCATEGORY_BASE_ATTACK:
			if (mission.stage == STAGE_MISSION_GOTO || mission.stage == STAGE_BASE_ATTACK) {
				/* Load IDX of base under attack */
				const int baseidx = MSG_ReadByte(sb);
				base_t *base = B_GetBaseByIDX(baseidx);
				assert(base);
				if (mission.stage == STAGE_BASE_ATTACK && base->baseStatus != BASE_UNDER_ATTACK)
					Com_Printf("......warning: base %i (%s) is supposedly under attack but base status doesn't match!\n", baseidx, base->name);
				mission.data = (void *) base;
			}
			break;
		case INTERESTCATEGORY_BUILDING:
		case INTERESTCATEGORY_SUPPLY:
			if (mission.stage >= STAGE_MISSION_GOTO) {
				const int baseidx = MSG_ReadByte(sb);
				if (baseidx != BYTES_NONE) {
					/* don't check baseidx value here: alien bases are not loaded yet */
					alienBase_t *alienBase = AB_GetBase(baseidx, 0);
					if (alienBase)
						mission.data = (void *) alienBase;
					else
						Com_Printf("Error while loading Alien Base mission (mission %i)\n", i);
				}
			}
			break;
		default:
			break;
		}
		Q_strncpyz(mission.location, MSG_ReadString(sb), sizeof(mission.location));
		mission.initialOverallInterest = MSG_ReadShort(sb);
		mission.initialIndividualInterest = MSG_ReadShort(sb);
		mission.startDate.day = MSG_ReadLong(sb);
		mission.startDate.sec = MSG_ReadLong(sb);
		mission.finalDate.day = MSG_ReadLong(sb);
		mission.finalDate.sec = MSG_ReadLong(sb);
		mission.pos[0] = MSG_ReadFloat(sb);
		mission.pos[1] = MSG_ReadFloat(sb);
		ufoIdx = MSG_ReadShort(sb);
		if (ufoIdx <= -1)
			mission.ufo = NULL;
		else
			mission.ufo = gd.ufos + ufoIdx;
		mission.onGeoscape = MSG_ReadByte(sb);

		/* Add mission to global array */
		LIST_Add(&ccs.missions, (byte*) &mission, sizeof(mission_t));
	}

	/* read ccs.battleParameters */
	memset(&ccs.battleParameters, 0, sizeof(ccs.battleParameters));
	missionId = MSG_ReadString(sb);
	if (missionId[0] != '\0') {
		ccs.battleParameters.mission = CP_GetMissionById(missionId);
		name = MSG_ReadString(sb);
		/* get corresponding category */
		for (i = 0; i < gd.numAlienCategories; i++)
			if (!Q_strncmp(name, gd.alienCategories[i].id, sizeof(gd.alienCategories[i].id))) {
				break;
			}
		if (i >= gd.numAlienCategories) {
			Com_Printf("CP_Load: alien category def \"%s\" doesn't exist\n", name);
			return qfalse;
		}
		j = MSG_ReadShort(sb);
		if (j >= INTERESTCATEGORY_MAX) {
			Com_Printf("CP_Load: Undefined alien team (category '%s', group '%i')\n", name, j);
			return qfalse;
		}
		ccs.battleParameters.alienTeamGroup = &gd.alienCategories[i].alienTeamGroups[j];
		name = MSG_ReadString(sb);
		if (name[0] != '\0')
			ccs.battleParameters.param = Mem_PoolStrDup(name, cl_localPool, 0);
		else
			ccs.battleParameters.param = NULL;
		Q_strncpyz(ccs.battleParameters.alienEquipment, MSG_ReadString(sb), sizeof(ccs.battleParameters.alienEquipment));
		Q_strncpyz(ccs.battleParameters.civTeam, MSG_ReadString(sb), sizeof(ccs.battleParameters.civTeam));
		ccs.battleParameters.day = MSG_ReadByte(sb);
		ccs.battleParameters.ugv = MSG_ReadShort(sb);
		ccs.battleParameters.aliens = MSG_ReadShort(sb);
		ccs.battleParameters.civilians = MSG_ReadShort(sb);
	}

	/* stores the select mission on geoscape */
	missionId = MSG_ReadString(sb);
	if (missionId[0] != '\0')
		ccs.selectedMission = CP_GetMissionById(missionId);
	else
		ccs.selectedMission = NULL;

	/* and now fix the mission pointers for e.g. ufocrash sites
	 * this is needed because the base load function which loads the aircraft
	 * doesn't know anything (at that stage) about the new missions that were
	 * add in this load function */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		for (j = 0; j < base->numAircraftInBase; j++) {
			if (base->aircraft[j].status == AIR_MISSION) {
				assert(base->aircraft[j].missionID);
				base->aircraft[j].mission = CP_GetMissionById(base->aircraft[j].missionID);

				/* not found */
				if (!base->aircraft[j].mission) {
					Com_Printf("Could not link mission '%s' in aircraft\n", base->aircraft[j].missionID);
					Mem_Free(base->aircraft[j].missionID);
					base->aircraft[j].missionID = NULL;
					return qfalse;
				}
				Mem_Free(base->aircraft[j].missionID);
				base->aircraft[j].missionID = NULL;
			}
		}
	}

	return qtrue;
}

/**
 * @brief Save callback for campaign data
 * @sa CP_Load
 * @sa SAV_GameSave
 */
qboolean CP_Save (sizebuf_t *sb, void *data)
{
	int i;
	const linkedList_t *list = ccs.missions;

	/* store campaign name */
	MSG_WriteString(sb, curCampaign->id);

	MSG_WriteByte(sb, gd.fund);
	MSG_WriteShort(sb, gd.nextUCN);

	/* store date */
	MSG_WriteLong(sb, ccs.date.day);
	MSG_WriteLong(sb, ccs.date.sec);

	/* store map view */
	MSG_WriteFloat(sb, ccs.center[0]);
	MSG_WriteFloat(sb, ccs.center[1]);
	MSG_WriteFloat(sb, ccs.angles[0]);
	MSG_WriteFloat(sb, ccs.angles[1]);
	MSG_WriteFloat(sb, ccs.zoom);

	/* save the overlay state */
	MSG_WriteShort(sb, r_geoscape_overlay->integer);
	MSG_WriteShort(sb, radarOverlayWasSet);

	/* store credits */
	MSG_WriteLong(sb, ccs.credits);

	/* store interest values */
	MSG_WriteShort(sb, ccs.lastInterestIncreaseDelay);
	MSG_WriteShort(sb, ccs.lastMissionSpawnedDelay);
	MSG_WriteShort(sb, ccs.overallInterest);
	for (i = 0; i < presaveArray[PRE_MAXCAT]; i++)
		MSG_WriteShort(sb, ccs.interest[i]);

	/* store other campaign data */
	MSG_WriteShort(sb, ccs.civiliansKilled);
	MSG_WriteShort(sb, ccs.aliensKilled);
	MSG_WriteByte(sb, ccs.XVISpreadActivated);
	MSG_WriteByte(sb, ccs.XVIShowMap);
	MSG_WriteByte(sb, ccs.humansAttackActivated);

	/* store missions */
	MSG_WriteShort(sb, CP_CountMission());
	for (; list; list = list->next) {
		const mission_t *mission = (mission_t *)list->data;

		if (mission->mapDef) {
			MSG_WriteString(sb, mission->mapDef->id);
			MSG_WriteLong(sb, mission->mapDef->timesAlreadyUsed);
		} else
			MSG_WriteString(sb, "");
		MSG_WriteString(sb, mission->id);
		MSG_WriteByte(sb, mission->active);
		MSG_WriteString(sb, mission->onwin);
		MSG_WriteString(sb, mission->onlose);
		MSG_WriteShort(sb, mission->category);
		MSG_WriteShort(sb, mission->stage);
		switch (mission->category) {
		case INTERESTCATEGORY_BASE_ATTACK:
			if (mission->stage == STAGE_MISSION_GOTO || mission->stage == STAGE_BASE_ATTACK) {
				const base_t *base;
				/* save IDX of base under attack if required */
				base = (base_t*)mission->data;
				assert(base);
				MSG_WriteByte(sb, base->idx);
			}
			break;
		case INTERESTCATEGORY_BUILDING:
		case INTERESTCATEGORY_SUPPLY:
			if (mission->stage >= STAGE_MISSION_GOTO) {
				const alienBase_t *base;
				/* save IDX of base under attack if required */
				base = (alienBase_t*)mission->data;
				if (base)
					MSG_WriteByte(sb, base->idx);
				else
					/* there may be no base is the mission is a subverting government */
					MSG_WriteByte(sb, BYTES_NONE);
			}
			break;
		default:
			break;
		}
		MSG_WriteString(sb, mission->location);
		MSG_WriteShort(sb, mission->initialOverallInterest);
		MSG_WriteShort(sb, mission->initialIndividualInterest);
		MSG_WriteLong(sb, mission->startDate.day);
		MSG_WriteLong(sb, mission->startDate.sec);
		MSG_WriteLong(sb, mission->finalDate.day);
		MSG_WriteLong(sb, mission->finalDate.sec);
		MSG_WriteFloat(sb, mission->pos[0]);
		MSG_WriteFloat(sb, mission->pos[1]);
		if (mission->ufo)
			MSG_WriteShort(sb, mission->ufo - gd.ufos);
		else
			MSG_WriteShort(sb, -1);
		MSG_WriteByte(sb, mission->onGeoscape);
	}

	/* store ccs.battleParameters */
	if (ccs.battleParameters.mission) {
		MSG_WriteString(sb, ccs.battleParameters.mission->id);
		MSG_WriteString(sb, gd.alienCategories[ccs.battleParameters.alienTeamGroup->categoryIdx].id);
		MSG_WriteShort(sb, ccs.battleParameters.alienTeamGroup->idx);
		MSG_WriteString(sb, (ccs.battleParameters.param ? ccs.battleParameters.param : ""));
		MSG_WriteString(sb, ccs.battleParameters.alienEquipment);
		MSG_WriteString(sb, ccs.battleParameters.civTeam);
		MSG_WriteByte(sb, ccs.battleParameters.day);
		MSG_WriteShort(sb, ccs.battleParameters.ugv);
		MSG_WriteShort(sb, ccs.battleParameters.aliens);
		MSG_WriteShort(sb, ccs.battleParameters.civilians);
	} else
		MSG_WriteString(sb, "");

	/* stores the select mission on geoscape */
	MSG_WriteString(sb, ccs.selectedMission ? ccs.selectedMission->id : "");

	return qtrue;
}


qboolean STATS_Save (sizebuf_t* sb, void* data)
{
	MSG_WriteShort(sb, campaignStats.missionsWon);
	MSG_WriteShort(sb, campaignStats.missionsLost);
	MSG_WriteShort(sb, campaignStats.basesBuild);
	MSG_WriteShort(sb, campaignStats.basesAttacked);
	MSG_WriteShort(sb, campaignStats.interceptions);
	MSG_WriteShort(sb, campaignStats.soldiersLost);
	MSG_WriteShort(sb, campaignStats.soldiersNew);
	MSG_WriteShort(sb, campaignStats.killedAliens);
	MSG_WriteShort(sb, campaignStats.rescuedCivilians);
	MSG_WriteShort(sb, campaignStats.researchedTechnologies);
	MSG_WriteShort(sb, campaignStats.moneyInterceptions);
	MSG_WriteShort(sb, campaignStats.moneyBases);
	MSG_WriteShort(sb, campaignStats.moneyResearch);
	MSG_WriteShort(sb, campaignStats.moneyWeapons);
	return qtrue;
}

qboolean STATS_Load (sizebuf_t* sb, void* data)
{
	campaignStats.missionsWon = MSG_ReadShort(sb);
	campaignStats.missionsLost = MSG_ReadShort(sb);
	campaignStats.basesBuild = MSG_ReadShort(sb);
	campaignStats.basesAttacked = MSG_ReadShort(sb);
	campaignStats.interceptions = MSG_ReadShort(sb);
	campaignStats.soldiersLost = MSG_ReadShort(sb);
	campaignStats.soldiersNew = MSG_ReadShort(sb);
	campaignStats.killedAliens = MSG_ReadShort(sb);
	campaignStats.rescuedCivilians = MSG_ReadShort(sb);
	campaignStats.researchedTechnologies = MSG_ReadShort(sb);
	campaignStats.moneyInterceptions = MSG_ReadShort(sb);
	campaignStats.moneyBases = MSG_ReadShort(sb);
	campaignStats.moneyResearch = MSG_ReadShort(sb);
	campaignStats.moneyWeapons = MSG_ReadShort(sb);
	return qtrue;
}

/**
 * @brief Starts a selected mission
 * @note Checks whether a dropship is near the landing zone and whether
 * it has a team on board
 * @sa CP_SetMissionVars
 */
void CP_StartSelectedMission (void)
{
	mission_t *mis;
	aircraft_t *aircraft;
	base_t *base;

	if (!GAME_CP_IsRunning() || !cls.missionaircraft)
		return;

	aircraft = cls.missionaircraft;
	base = CP_GetMissionBase();

	if (!ccs.selectedMission)
		ccs.selectedMission = aircraft->mission;

	if (!ccs.selectedMission) {
		Com_DPrintf(DEBUG_CLIENT, "No ccs.selectedMission\n");
		return;
	}

	mis = ccs.selectedMission;
	assert(mis);
	assert(aircraft);

	/* Before we start, we should clear the missionresults array. */
	memset(&missionresults, 0, sizeof(missionresults));

	/* Various sanity checks. */
	if (!mis->active) {
		Com_DPrintf(DEBUG_CLIENT, "CP_StartSelectedMission: Dropship not near landing zone: mis->active: %i\n", mis->active);
		return;
	}
	if (aircraft->teamSize <= 0) {
		Com_DPrintf(DEBUG_CLIENT, "CP_StartSelectedMission: No team in dropship. teamSize=%i\n", aircraft->teamSize);
		return;
	}

	CP_CreateBattleParameters(mis);
	CP_SetMissionVars(mis);
	/* Set the states of mission Cvars to proper values. */
	Cvar_SetValue("mission_uforecovered", 0);

	/* manage inventory */
	ccs.eMission = base->storage; /* copied, including arrays inside! */
	CL_CleanTempInventory(base);
	CL_ReloadAndRemoveCarried(aircraft, &ccs.eMission);
	/* remove inventory of any old temporary LEs */
	LE_Cleanup();

	CP_StartMissionMap(mis);
}

/**
 * @brief Calculates the win probability for an auto mission
 * @todo This needs work - also take mis->initialIndividualInterest into account?
 * @returns a float value that is between 0 and 1
 * @param[in] mis The mission we are calculating the probability for
 * @param[in] base The base we are trying to defend in case of a base attack mission
 * @param[in] aircraft Your aircraft that has reached the mission location in case
 * this is no base attack mission
 */
static float CP_GetWinProbabilty (const mission_t *mis, const base_t *base, const aircraft_t *aircraft)
{
	float winProbability;

	if (mis->stage != STAGE_BASE_ATTACK) {
		assert(aircraft);

		switch (mis->category) {
		case INTERESTCATEGORY_TERROR_ATTACK:
			/* very hard to win this */
			/** @todo change the formular here to reflect the above comment */
			winProbability = exp((0.5 - .15 * curCampaign->difficulty) * aircraft->teamSize - ccs.battleParameters.aliens);
			break;
		case INTERESTCATEGORY_XVI:
			/* not that hard to win this, they want to spread xvi - no real terror mission */
			/** @todo change the formular here to reflect the above comment */
			winProbability = exp((0.5 - .15 * curCampaign->difficulty) * aircraft->teamSize - ccs.battleParameters.aliens);
			break;
		default:
			/** @todo change the formular here to reflect the above comments */
			winProbability = exp((0.5 - .15 * curCampaign->difficulty) * aircraft->teamSize - ccs.battleParameters.aliens);
			break;
		}
		Com_DPrintf(DEBUG_CLIENT, "Aliens: %i - Soldiers: %i -- probability to win: %.02f\n", ccs.battleParameters.aliens, aircraft->teamSize, winProbability);

		return winProbability;
	} else {
		linkedList_t *hiredSoldiers = NULL;
		linkedList_t *ugvs = NULL;
		linkedList_t *listPos;
		const int numSoldiers = E_GetHiredEmployees(base, EMPL_SOLDIER, &hiredSoldiers);
		const int numUGVs = E_GetHiredEmployees(base, EMPL_ROBOT, &ugvs);

		assert(base);

		/* a base defence mission can only be won if there are soldiers that
		 * defend the attacked base */
		if (numSoldiers || numUGVs) {
			float increaseWinProbability = 1.0f;
			listPos = hiredSoldiers;
			while (listPos) {
				const employee_t *employee = (employee_t *)listPos->data;
				/* don't use an employee that is currently being transfered */
				if (E_EmployeeIsCurrentlyInBase(employee)) {
					const character_t *chr = &employee->chr;
					const chrScoreGlobal_t *score = &chr->score;
					/* if the soldier was ever on a mission */
					if (score->assignedMissions) {
						const rank_t *rank = &gd.ranks[score->rank];
						/* @sa CHRSH_CharGetMaxExperiencePerMission */
						if (score->experience[SKILL_CLOSE] > 70) { /** @todo fix this value */
							increaseWinProbability *= rank->factor;
						}
					}
				}
				listPos = listPos->next;
			}
			/* now handle the ugvs */
			listPos = ugvs;
			while (listPos) {
				const employee_t *employee = (employee_t *)listPos->data;
				/* don't use an employee that is currently being transfered */
				if (E_EmployeeIsCurrentlyInBase(employee)) {
					const character_t *chr = &employee->chr;
					const chrScoreGlobal_t *score = &chr->score;
					const rank_t *rank = &gd.ranks[score->rank];
					/* @sa CHRSH_CharGetMaxExperiencePerMission */
					if (score->experience[SKILL_CLOSE] > 70) { /** @todo fix this value */
						increaseWinProbability *= rank->factor;
					}
				}
				listPos = listPos->next;
			}

			winProbability = exp((0.5 - .15 * curCampaign->difficulty) * numSoldiers - ccs.battleParameters.aliens);
			winProbability += increaseWinProbability;

			Com_DPrintf(DEBUG_CLIENT, "Aliens: %i - Soldiers: %i - UGVs: %i -- probability to win: %.02f\n",
				ccs.battleParameters.aliens, numSoldiers, numUGVs, winProbability);

			LIST_Delete(&hiredSoldiers);
			LIST_Delete(&ugvs);

			return winProbability;
		} else {
			/* No soldier to defend the base */
			Com_DPrintf(DEBUG_CLIENT, "Aliens: %i - Soldiers: 0  -- battle lost\n", ccs.battleParameters.aliens);
			return 0.0f;
		}
	}
}

/**
 * @brief Collect alien bodies for auto missions
 * @note collect all aliens as dead ones
 */
static void CL_AutoMissionAlienCollect (aircraft_t *aircraft)
{
	int i;
	int aliens = ccs.battleParameters.aliens;

	if (!aliens)
		return;

	MS_AddNewMessage(_("Notice"), _("Collected dead alien bodies"), qfalse, MSG_STANDARD, NULL);

	while (aliens > 0) {
		for (i = 0; i < ccs.battleParameters.alienTeamGroup->numAlienTeams; i++) {
			const teamDef_t *teamDef = ccs.battleParameters.alienTeamGroup->alienTeams[i];
			const int addDeadAlienAmount = aliens > 1 ? rand() % aliens : aliens;
			if (!addDeadAlienAmount)
				continue;
			assert(i < MAX_CARGO);
			assert(ccs.battleParameters.alienTeamGroup->alienTeams[i]);
			AL_AddAlienTypeToAircraftCargo(aircraft, teamDef, addDeadAlienAmount, qtrue);
			aliens -= addDeadAlienAmount;
			if (!aliens)
				break;
		}
	}
}

/**
 * @brief Handles the auto mission for none storyrelated missions or missions
 * that failed to assembly
 * @sa GAME_CP_MissionAutoGo_f
 * @sa CL_Drop
 * @sa AL_CollectingAliens
 */
void CL_GameAutoGo (mission_t *mis)
{
	qboolean won;
	float winProbability;
	/* maybe ccs.interceptAircraft is changed in some functions we call here
	 * so store a local pointer to guarantee that we access the right aircraft
	 * note that ccs.interceptAircraft is a fake aircraft for base attack missions */
	aircraft_t *aircraft = ccs.interceptAircraft;

	assert(mis);

	CP_CreateBattleParameters(mis);

	if (!ccs.interceptAircraft) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GameAutoGo: No update after automission\n");
		return;
	}

	if (mis->stage != STAGE_BASE_ATTACK) {
		if (!mis->active) {
			MS_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"), qfalse, MSG_STANDARD, NULL);
			return;
		} else if (mis->mapDef->storyRelated) {
			Com_DPrintf(DEBUG_CLIENT, "You have to play this mission, because it's story related\n");
			/* ensure, that the automatic button is no longer visible */
			Cvar_Set("cp_mission_autogo_available", "0");
			return;
		}

		winProbability = CP_GetWinProbabilty(mis, NULL, aircraft);
	} else {
		winProbability = CP_GetWinProbabilty(mis, (base_t *)mis->data, NULL);
	}

	MN_PopMenu(qfalse);

	won = frand() < winProbability;

	/* update nation opinions */
	if (won) {
		int civiliansKilled = 0; /** @todo fill this for the case you won the game */
		CL_HandleNationData(!won, ccs.battleParameters.civilians, 0, 0, ccs.battleParameters.aliens, ccs.selectedMission);
		CP_CheckLostCondition(!won, mis, civiliansKilled);
	} else {
		CL_HandleNationData(!won, 0, ccs.battleParameters.civilians, ccs.battleParameters.aliens, 0, ccs.selectedMission);
		CP_CheckLostCondition(!won, mis, ccs.battleParameters.civilians);
	}

	CL_AutoMissionAlienCollect(aircraft);

	/* onwin and onlose triggers */
	CP_ExecuteMissionTrigger(ccs.selectedMission, won);

	/* if a UFO has been recovered, send it to a base */
	if (won && missionresults.recovery)
		Cmd_ExecuteString("cp_uforecoverystore");

	/* handle base attack mission */
	if (ccs.selectedMission->stage == STAGE_BASE_ATTACK) {
		const base_t *base = (base_t*)ccs.selectedMission->data;
		assert(base);

		if (won) {
			/* fake an aircraft return to collect goods and aliens */
			CL_AircraftReturnedToHomeBase(ccs.interceptAircraft);

			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("Defence of base: %s successful!"), base->name);
			MS_AddNewMessage(_("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			CP_BaseAttackMissionIsFailure(ccs.selectedMission);
			/** @todo @sa AIRFIGHT_ProjectileHitsBase notes */
		} else
			CP_BaseAttackMissionLeave(ccs.selectedMission);
	} else {
		AIR_AircraftReturnToBase(ccs.interceptAircraft);
		if (won)
			CP_MissionIsOver(ccs.selectedMission);
	}

	if (won)
		MS_AddNewMessage(_("Notice"), _("You've won the battle"), qfalse, MSG_STANDARD, NULL);
	else
		MS_AddNewMessage(_("Notice"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);

	MAP_ResetAction();
}

/**
 * @brief Update employeer stats after mission.
 * @param[in] won Determines whether we won the mission or not.
 * @note Soldier promotion is being done here.
 * @sa GAME_CP_Results_f
 */
void CL_UpdateCharacterStats (const base_t *base, int won, const aircraft_t *aircraft)
{
	int i, j;

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: base: '%s' numTeamList: %i\n",
		base->name, cl.numTeamList);

	assert(aircraft);

	/* only soldiers have stats and ranks, ugvs not */
	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++)
		if (AIR_IsEmployeeInAircraft(&gd.employees[EMPL_SOLDIER][i], aircraft)) {
			character_t *chr = &gd.employees[EMPL_SOLDIER][i].chr;
			assert(chr);
			if (!gd.employees[EMPL_SOLDIER][i].hired) {
				Sys_Error("Employee %s is reported as being on the aircraft (%s), but he is not hired (%i/%i)",
					chr->name, aircraft->id, i, gd.numEmployees[EMPL_SOLDIER]);
			}
			assert(gd.employees[EMPL_SOLDIER][i].baseHired == aircraft->homebase);

			Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: searching for soldier: %i\n", i);

			/* Remember the number of assigned mission for this character. */
			chr->score.assignedMissions++;

			/** @todo use chrScore_t to determine negative influence on soldier here,
			 * like killing too many civilians and teammates can lead to unhire and disband
			 * such soldier, or maybe rank degradation. */

			/* Check if the soldier meets the requirements for a higher rank
			 * and do a promotion. */
			if (gd.numRanks >= 2) {
				for (j = gd.numRanks - 1; j > chr->score.rank; j--) {
					const rank_t *rank = &gd.ranks[j];
					/** @todo (Zenerka 20080301) extend ranks and change calculations here. */
					if (rank->type == EMPL_SOLDIER && (chr->score.skills[ABILITY_MIND] >= rank->mind)
						&& (chr->score.kills[KILLED_ALIENS] >= rank->killed_enemies)
						&& ((chr->score.kills[KILLED_CIVILIANS] + chr->score.kills[KILLED_TEAM]) <= rank->killed_others)) {
						chr->score.rank = j;
						if (chr->HP > 0)
							Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s has been promoted to %s.\n"), chr->name, _(rank->name));
						else
							Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s has been awarded the posthumous rank of %s\nfor inspirational gallantry in the face of overwhelming odds.\n"), chr->name, _(rank->name));
						MS_AddNewMessage(_("Soldier promoted"), cp_messageBuffer, qfalse, MSG_PROMOTION, NULL);
						break;
					}
				}
			}
		}
	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateCharacterStats: Done\n");
}

#ifdef DEBUG
/**
 * @brief Debug function to add one item of every type to base storage and mark them collected.
 * @note Command to call this: debug_additems
 */
static void CL_DebugAllItems_f (void)
{
	int i;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);

	for (i = 0; i < csi.numODs; i++) {
		objDef_t *obj = &csi.ods[i];
		if (!obj->weapon && !obj->numWeapons)
			continue;
		B_UpdateStorageAndCapacity(base, obj, 1, qfalse, qtrue);
		if (base->storage.num[i] > 0) {
			technology_t *tech = obj->tech;
			if (!tech)
				Sys_Error("CL_DebugAllItems_f: No tech for %s / %s\n", obj->id, obj->name);
			RS_MarkCollected(tech);
		}
	}
}

/**
 * @brief Debug function to show items in base storage.
 * @note Command to call this: debug_listitem
 */
static void CL_DebugShowItems_f (void)
{
	int i;
	base_t *base;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i >= gd.numBases) {
		Com_Printf("invalid baseID (%s)\n", Cmd_Argv(1));
		return;
	}
	base = B_GetBaseByIDX(i);

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *obj = &csi.ods[i];
		if (!obj->tech)
			Sys_Error("CL_DebugAllItems_f: No tech for %s\n", obj->id);
		Com_Printf("%i. %s: %i\n", i, obj->id, base->storage.num[i]);
	}
}

/**
 * @brief Debug function to set the credits to max
 */
static void CL_DebugFullCredits_f (void)
{
	CL_UpdateCredits(MAX_CREDITS);
}

/**
 * @brief Debug function to add 5 new unhired employees of each type
 * @note called with debug_addemployees
 */
static void CL_DebugNewEmployees_f (void)
{
	int j;
	nation_t *nation = &gd.nations[0];	/**< This is just a debuging function, nation does not matter */

	for (j = 0; j < 5; j++)
		/* Create a scientist */
		E_CreateEmployee(EMPL_SCIENTIST, nation, NULL);

	for (j = 0; j < 5; j++)
		/* Create a pilot. */
		E_CreateEmployee(EMPL_PILOT, nation, NULL);

	for (j = 0; j < 5; j++)
		/* Create a soldier. */
		E_CreateEmployee(EMPL_SOLDIER, nation, NULL);

	for (j = 0; j < 5; j++)
		/* Create a worker. */
		E_CreateEmployee(EMPL_WORKER, nation, NULL);

	for (j = 0; j < 5; j++)
		/* Create a ares  ugv. */
		E_CreateEmployee(EMPL_ROBOT, nation, CL_GetUGVByID("ugv_ares_w"));

	for (j = 0; j < 5; j++)
		/* Create a phoenix  ugv. */
		E_CreateEmployee(EMPL_ROBOT, nation, CL_GetUGVByID("ugv_phoenix"));
}

/**
 * @brief Debug function to increase the kills and test the ranks
 */
static void CL_DebugChangeCharacterStats_f (void)
{
	int i, j;
	character_t *chr;

	if (!baseCurrent)
		return;

	for (i = 0; i < gd.numEmployees[EMPL_SOLDIER]; i++) {
		employee_t *employee = &gd.employees[EMPL_SOLDIER][i];

		if (!employee->hired && employee->baseHired != baseCurrent)
			continue;

		chr = &(employee->chr);
		assert(chr);

		for (j = 0; j < KILLED_NUM_TYPES; j++)
			chr->score.kills[j]++;
	}
	if (baseCurrent->aircraftCurrent)
		CL_UpdateCharacterStats(baseCurrent, 1, baseCurrent->aircraftCurrent);
}

/**
 * @brief Scriptfunction to list all parsed nations with their current values
 * @note called with debug_listnation
 */
static void CL_NationList_f (void)
{
	int i;
	for (i = 0; i < gd.numNations; i++) {
		Com_Printf("Nation ID: %s\n", gd.nations[i].id);
		Com_Printf("...max-funding %i c\n", gd.nations[i].maxFunding);
		Com_Printf("...alienFriendly %0.2f\n", gd.nations[i].stats[0].alienFriendly);
		Com_Printf("...happiness %0.2f\n", gd.nations[i].stats[0].happiness);
		Com_Printf("...xviInfection %i\n", gd.nations[i].stats[0].xviInfection);
		Com_Printf("...max-soldiers %i\n", gd.nations[i].maxSoldiers);
		Com_Printf("...max-scientists %i\n", gd.nations[i].maxScientists);
		Com_Printf("...color r:%.2f g:%.2f b:%.2f a:%.2f\n", gd.nations[i].color[0], gd.nations[i].color[1], gd.nations[i].color[2], gd.nations[i].color[3]);
		Com_Printf("...pos x:%.0f y:%.0f\n", gd.nations[i].pos[0], gd.nations[i].pos[1]);
	}
}
#endif

/* ===================================================================== */

/* these commands are only available in singleplayer */
static const cmdList_t game_commands[] = {
	{"airequip_updatemenu", AIM_AircraftEquipMenuUpdate_f, "Init function for the aircraft equip menu"},
	{"airequip_list_click", AIM_AircraftEquipMenuClick_f, NULL},
	{"airequip_slot_select", AIM_AircraftEquipSlotSelect_f, NULL},
	{"airequip_zone_select", AIM_AircraftEquipZoneSelect_f, NULL},
	{"airequip_add_item", AIM_AircraftEquipAddItem_f, "Add item to slot"},
	{"airequip_del_item", AIM_AircraftEquipDeleteItem_f, "Remove item from slot"},
	{"add_battery", BDEF_AddBattery_f, "Add a new battery to base"},
	{"remove_battery", BDEF_RemoveBattery_f, "Remove a battery from base"},
	{"basedef_initmenu", BDEF_MenuInit_f, "Inits base defence menu"},
	{"basedef_updatemenu", BDEF_BaseDefenseMenuUpdate_f, "Inits base defence menu"},
	{"basedef_slot_list_click", BDEF_ListClick_f, "Inits base defence menu"},
	{"basedef_list_click", AIM_AircraftEquipMenuClick_f, NULL},
	{"update_base_radar_coverage", RADAR_UpdateBaseRadarCoverage_f, "Update base radar coverage"},
	{"addeventmail", CL_EventAddMail_f, "Add a new mail (event trigger) - e.g. after a mission"},
	{"stats_update", CL_StatsUpdate_f, NULL},
	{"combatzoom_exit", MAP_CombatZoomExit_f, "Exit combat zoom mode."},
	{"airfightmap_init", AFM_Init_f, "Exit air fight map mode."},
	{"airfightmap_exit", AFM_Exit_f, "Exit air fight map mode."},
	{"game_go", CP_StartSelectedMission, NULL},
	{"game_timestop", CL_GameTimeStop, NULL},
	{"game_timeslow", CL_GameTimeSlow, NULL},
	{"game_timefast", CL_GameTimeFast, NULL},
	{"game_settimeid", CL_SetGameTime_f, NULL},
	{"mn_mapaction_reset", MAP_ResetAction, NULL},
	{"map_center", MAP_CenterOnPoint_f, "Centers the geoscape view on items on the geoscape - and cycle through them"},
	{"map_zoom", MAP_Zoom_f, NULL},
	{"map_scroll", MAP_Scroll_f, NULL},
	{"cp_start_xvi_spreading", CP_StartXVISpreading_f, "Start XVI spreading"},
#ifdef DEBUG
	{"debug_listnation", CL_NationList_f, "List all nations on the game console"},
	{"debug_listaircraft", AIR_ListAircraft_f, "Debug function to list all aircraft in all bases"},
	{"debug_fullcredits", CL_DebugFullCredits_f, "Debug function to give the player full credits"},
	{"debug_addemployees", CL_DebugNewEmployees_f, "Debug function to add 5 new unhired employees of each type"},
	{"debug_additems", CL_DebugAllItems_f, "Debug function to add one item of every type to base storage and mark related tech collected"},
	{"debug_listitem", CL_DebugShowItems_f, "Debug function to show all items in base storage"},
#endif
	{NULL, NULL, NULL}
};

/**
 * @brief registers callback commands that are used by campaign
 * @todo callbacks should be registered on menu push
 * (what about sideeffects for commands that are called from different menus?)
 * @sa CP_AddCampaignCommands
 * @sa CP_RemoveCampaignCallbackCommands
 */
static void CP_AddCampaignCallbackCommands (void)
{
	AIM_InitCallbacks();
	AIR_InitCallbacks();
	B_InitCallbacks();
	CP_TEAM_InitCallbacks();
	HOS_InitCallbacks();
}

static void CP_AddCampaignCommands (void)
{
	const cmdList_t *commands;

	for (commands = game_commands; commands->name; commands++)
		Cmd_AddCommand(commands->name, commands->function, commands->description);

	CP_AddCampaignCallbackCommands();
}

/**
 * @brief registers callback commands that are used by campaign
 * @todo callbacks should be removed on menu pop
 * (what about sideeffects for commands that are called from different menus?)
 * @sa CP_AddCampaignCommands
 * @sa CP_RemoveCampaignCallbackCommands
 */
static void CP_RemoveCampaignCallbackCommands (void)
{
	AIM_ShutdownCallbacks();
	AIR_ShutdownCallbacks();
	B_ShutdownCallbacks();
	CP_TEAM_ShutdownCallbacks();
	HOS_ShutdownCallbacks();
}

static void CP_RemoveCampaignCommands (void)
{
	const cmdList_t *commands;

	for (commands = game_commands; commands->name; commands++)
		Cmd_RemoveCommand(commands->name);

	CP_RemoveCampaignCallbackCommands();
}

/**
 * @brief Called at new game and load game
 * @param[in] load qtrue if we are loading game, qfalse otherwise
 */
void CP_CampaignInit (qboolean load)
{
	assert(curCampaign);

	/** @todo are all these needed on every load?
	 * what about RS_InitTree? how often must this be done? */
	RS_InitTree(load);		/**< Initialise all data in the research tree. */

	/* now check the parsed values for errors that are not catched at parsing stage */
	if (!load)
		CL_ScriptSanityCheck();

	CP_AddCampaignCommands();

	CL_GameTimeStop();

	/* Init popup and map/geoscape */
	CL_PopupInit();

	CP_XVIInit();

	if (load) {
		/** @todo move into CP_Load? */
		CP_InitMarket(load);
		return;
	}

	/* initialise view angle for 3D geoscape so that europe is seen */
	ccs.angles[YAW] = GLOBE_ROTATE;
	/* initialise date */
	ccs.date = curCampaign->date;

	MAP_Init();

	gd.numAircraft = 0;

	B_NewBases();
	INS_NewInstallations();
	PR_ProductionInit();

	/* get day */
	while (ccs.date.sec > SECONDS_PER_DAY) {
		ccs.date.sec -= SECONDS_PER_DAY;
		ccs.date.day++;
	}

	/* set map view */
	ccs.center[0] = ccs.center[1] = 0.5;
	ccs.zoom = 1.0;

	CL_UpdateCredits(curCampaign->credits);

	/* Initialize alien interest */
	CL_ResetAlienInterest();

	/* Initialize XVI overlay */
	Cvar_SetValue("mn_xvimap", ccs.XVIShowMap);
	R_InitializeXVIOverlay(curCampaign->map, NULL, 0, 0);

	/* Reset alien bases */
	AB_ResetAlienBases();

	MN_PopMenu(qtrue);
	Cvar_Set("mn_main", "singleplayerInGame");
	Cvar_Set("mn_active", "map");
	MN_PushMenu("map", NULL);

	/* create a base as first step */
	B_SelectBase(NULL);

	Cmd_ExecuteString("addeventmail prolog");

	/* Spawn first missions of the game */
	CP_InitializeSpawningDelay();

	/* now check the parsed values for errors that are not catched at parsing stage */
	CL_ScriptSanityCheckCampaign();
}

void CP_CampaignExit (void)
{
	if (!curCampaign)
		return;

	SV_Shutdown("Game exit", qfalse);
	CL_Disconnect();

	/* singleplayer commands are no longer available */
	Com_DPrintf(DEBUG_CLIENT, "Remove game commands\n");
	CP_RemoveCampaignCommands();

	Com_Printf("Shutdown campaign\n");
	CL_ResetSinglePlayerData();
}

/**
 * @brief Returns the campaign pointer from global campaign array
 * @param name Name of the campaign
 * @return campaign_t pointer to campaign with name or NULL if not found
 */
campaign_t* CL_GetCampaign (const char* name)
{
	campaign_t* campaign;
	int i;

	for (i = 0, campaign = campaigns; i < numCampaigns; i++, campaign++)
		if (!Q_strncmp(name, campaign->id, MAX_VAR))
			break;

	if (i == numCampaigns) {
		Com_Printf("CL_GetCampaign: Campaign \"%s\" doesn't exist.\n", name);
		return NULL;
	}
	return campaign;
}

/**
 * @brief Will clear most of the parsed singleplayer data
 * @sa INVSH_InitInventory
 * @sa CL_ReadSinglePlayerData
 */
void CL_ResetSinglePlayerData (void)
{
	int i;

	memset(&gd, 0, sizeof(gd));
	memset(&campaignStats, 0, sizeof(campaignStats));

	LIST_Delete(&ccs.missions);
	memset(&invList, 0, sizeof(invList));
	cp_messageStack = NULL;

	/* cleanup dynamic mails */
	CL_FreeDynamicEventMail();

	Mem_FreePool(cl_localPool);

	/* called to flood the hash list - because the parse tech function
	 * was maybe already called */
	RS_ResetTechs();
	E_ResetEmployees();
	INVSH_InitInventory(invList);
	/* Count Alien team definitions. */
	for (i = 0; i < csi.numTeamDefs; i++) {
		if (CHRSH_IsTeamDefAlien(&csi.teamDef[i]))
			gd.numAliensTD++;
	}
}

#ifdef DEBUG
/**
 * @brief Show campaign stats in console
 * call this function via debug_listcampaign
 */
static void CP_CampaignStats_f (void)
{
	if (!GAME_CP_IsRunning()) {
		Com_Printf("No campaign active\n");
		return;
	}

	Com_Printf("Campaign id: %s\n", curCampaign->id);
	Com_Printf("..research list: %s\n", curCampaign->researched);
	Com_Printf("..equipment: %s\n", curCampaign->equipment);
	Com_Printf("..team: %i\n", curCampaign->team);

	Com_Printf("..salaries:\n");
	Com_Printf("...soldier_base: %i\n", SALARY_SOLDIER_BASE);
	Com_Printf("...soldier_rankbonus: %i\n", SALARY_SOLDIER_RANKBONUS);
	Com_Printf("...worker_base: %i\n", SALARY_WORKER_BASE);
	Com_Printf("...worker_rankbonus: %i\n", SALARY_WORKER_RANKBONUS);
	Com_Printf("...scientist_base: %i\n", SALARY_SCIENTIST_BASE);
	Com_Printf("...scientist_rankbonus: %i\n", SALARY_SCIENTIST_RANKBONUS);
	Com_Printf("...pilot_base: %i\n", SALARY_PILOT_BASE);
	Com_Printf("...pilot_rankbonus: %i\n", SALARY_PILOT_RANKBONUS);
	Com_Printf("...robot_base: %i\n", SALARY_ROBOT_BASE);
	Com_Printf("...robot_rankbonus: %i\n", SALARY_ROBOT_RANKBONUS);
	Com_Printf("...aircraft_factor: %i\n", SALARY_AIRCRAFT_FACTOR);
	Com_Printf("...aircraft_divisor: %i\n", SALARY_AIRCRAFT_DIVISOR);
	Com_Printf("...base_upkeep: %i\n", SALARY_BASE_UPKEEP);
	Com_Printf("...admin_initial: %i\n", SALARY_ADMIN_INITIAL);
	Com_Printf("...admin_soldier: %i\n", SALARY_ADMIN_SOLDIER);
	Com_Printf("...admin_worker: %i\n", SALARY_ADMIN_WORKER);
	Com_Printf("...admin_scientist: %i\n", SALARY_ADMIN_SCIENTIST);
	Com_Printf("...admin_pilot: %i\n", SALARY_ADMIN_PILOT);
	Com_Printf("...admin_robot: %i\n", SALARY_ADMIN_ROBOT);
	Com_Printf("...debt_interest: %.5f\n", SALARY_DEBT_INTEREST);
}
#endif /* DEBUG */

/**
 * @brief Returns "homebase" of the mission.
 * @note see struct client_static_s
 * @note This might be @c NULL for skirmish and multiplayer
 */
base_t *CP_GetMissionBase (void)
{
	if (!cls.missionaircraft)
		Sys_Error("CP_GetMissionBase: No missionaircraft given");
	if (GAME_IsCampaign() && !cls.missionaircraft->homebase)
		Sys_Error("CP_GetMissionBase: Missionaircraft has no homebase set");
	return cls.missionaircraft->homebase;
}

/**
 * @brief Determines a random position on Geoscape
 * @param[out] pos The position that will be overwritten. pos[0] is within -180, +180. pos[1] within -90, +90.
 * @param[in] noWater True if the position should not be on water
 * @sa CP_GetRandomPosOnGeoscapeWithParameters
 * @note The random positions should be roughly uniform thanks to the non-uniform distribution used.
 * @note This function always returns a value.
 */
void CP_GetRandomPosOnGeoscape (vec2_t pos, qboolean noWater)
{
	do {
		pos[0] = (frand() - 0.5f) * 360.0f;
		pos[1] = asin((frand() - 0.5f) * 2.0f) * todeg;
	} while (noWater && MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)));

	Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscape: Get random position on geoscape %.2f:%.2f\n", pos[0], pos[1]);
}

/**
 * @brief Determines a random position on Geoscape that fulfills certain criteria given via parameters
 * @param[out] pos The position that will be overwritten with the random point fulfilling the criterias. pos[0] is within -180, +180. pos[1] within -90, +90.
 * @param[in] terrainTypes A linkedList_t containing a list of strings determining the acceptable terrain types (e.g. "grass") May be NULL.
 * @param[in] cultureTypes A linkedList_t containing a list of strings determining the acceptable culture types (e.g. "western") May be NULL.
 * @param[in] populationTypes A linkedList_t containing a list of strings determining the acceptable population types (e.g. "suburban") May be NULL.
 * @param[in] nations A linkedList_t containing a list of strings determining the acceptable nations (e.g. "asia"). May be NULL
 * @return true if a location was found, otherwise false
 * @note There may be no position fitting the parameters. The higher RASTER, the lower the probability to find a position.
 * @sa LIST_AddString
 * @sa LIST_Delete
 * @note When all parameters are NULL, the algorithm assumes that it does not need to include "water" terrains when determining a random position
 * @note You should rather use CP_GetRandomPosOnGeoscape if there are no parameters (except water) to choose a random position
 */
qboolean CP_GetRandomPosOnGeoscapeWithParameters (vec2_t pos, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations)
{
	float x, y;
	int num;
	int randomNum;

	/* RASTER might reduce amount of tested locations to get a better performance */
	/**< Number of points in latitude and longitude that will be tested. Therefore, the total number of position tried
	 * will be numPoints * numPoints */
	const float numPoints = 360.0 / RASTER;
	/* RASTER is minimizing the amount of locations, so an offset is introduced to enable access to all locations, depending on a random factor */
	const float offsetX = frand() * RASTER;
	const float offsetY = -1.0 + frand() * 2.0 / numPoints;
	vec2_t posT;
	int hits = 0;

	/* check all locations for suitability in 2 iterations */
	/* prepare 1st iteration */

	/* ITERATION 1 */
	for (y = 0; y < numPoints; y++) {
		const float posY = asin(2.0 * y / numPoints + offsetY) * todeg;	/* Use non-uniform distribution otherwise we favour the poles */
		for (x = 0; x < numPoints; x++) {
			const float posX = x * RASTER - 180.0 + offsetX;

			Vector2Set(posT, posX, posY);

			if (MAP_PositionFitsTCPNTypes(posT, terrainTypes, cultureTypes, populationTypes, nations)) {
				/* the location given in pos belongs to the terrain, culture, population types and nations
				 * that are acceptable, so count it */
				/** @todo - cache the counted hits */
				hits++;
			}
		}
	}

	/* if there have been no hits, the function failed to find a position */
	if (hits == 0)
		return qfalse;

	/* the 2nd iteration goes through the locations again, but does so only until a random point */
	/* prepare 2nd iteration */
	randomNum = num = rand() % hits;

	/* ITERATION 2 */
	for (y = 0; y < numPoints; y++) {
		const float posY = asin(2.0 * y / numPoints + offsetY) * todeg;
		for (x = 0; x < numPoints; x++) {
			const float posX = x * RASTER - 180.0 + offsetX;

			Vector2Set(posT,posX,posY);

			if (MAP_PositionFitsTCPNTypes(posT, terrainTypes, cultureTypes, populationTypes, nations)) {
				num--;

				if (num < 1) {
					Vector2Set(pos, posX, posY);
					Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscapeWithParameters: New random coords for a mission are %.0f:%.0f, chosen as #%i out of %i possible locations\n",
						pos[0], pos[1], randomNum, hits);
					return qtrue;
				}
			}
		}
	}

	Com_DPrintf(DEBUG_CLIENT, "CP_GetRandomPosOnGeoscapeWithParameters: New random coords for a mission are %.0f:%.0f, chosen as #%i out of %i possible locations\n",
		pos[0], pos[1], num, hits);

	/* Make sure that position is within bounds */
	assert(pos[0] >= -180);
	assert(pos[0] <= 180);
	assert(pos[1] >= -90);
	assert(pos[1] <= 90);

	return qtrue;
}

/** @todo remove me */
void CP_InitStartup (void)
{
	cl_campaign = Cvar_Get("cl_campaign", "main", 0, "Which is the current selected campaign id");

	/* commands */
#ifdef DEBUG
	Cmd_AddCommand("debug_statsupdate", CL_DebugChangeCharacterStats_f, "Debug function to increase the kills and test the ranks");
	Cmd_AddCommand("debug_listcampaign", CP_CampaignStats_f, "Print campaign stats to game console");
#endif
	Cmd_AddCommand("check_baseattacks", CP_CheckBaseAttacks_f, "Check if baseattack mission available and start it.");

	CP_MissionsInit();
	MS_MessageInit();
}
