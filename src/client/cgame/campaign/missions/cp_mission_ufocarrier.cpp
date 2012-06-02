/**
 * @file
 * @brief Campaign mission
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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

#include "../../../cl_shared.h"
#include "../cp_campaign.h"
#include "../cp_map.h"
#include "../cp_ufo.h"
#include "../cp_missions.h"
#include "../cp_time.h"
#include "../cp_xvi.h"
#include "../cp_alien_interest.h"

static mission_t *CP_GetCarrierMission (void)
{
	MIS_Foreach(mission) {
		if (mission->category == INTERESTCATEGORY_UFOCARRIER)
			return mission;
	}

	return NULL;
}

/**
 * @brief Start UFO-Carrier mission.
 */
static void CP_UFOCarrierMissionStart (mission_t *mission)
{
	mission->idx = ++ccs.campaignStats.missions;
	mission->finalDate = ccs.date;
	mission->stage = STAGE_RECON_AIR;
}

static void CP_UFOCarrierMissionUpdate (mission_t *mission)
{
	/* delay the next update for some time */
	const date_t delay = {2, 0};
	mission->finalDate = Date_Add(ccs.date, delay);

	if (INS_HasType(INSTALLATION_ORBIT)) {
		cgi->UI_PopupButton(_("UFO-Carrier"), _("The Carrier UFO is in range. Should we launch the missiles?"),
			"ui_pop;", _("Cancel"), _("Don't attack the UFO-Carrier"),
			"cp_attack_ufocarrier;ui_pop;", _("Attack"), _("Attack the UFO-Carrier"),
			NULL, NULL, NULL);
	}
}

/**
 * @brief Determine what action should be performed when a UFOCarriering mission stage ends.
 * @param[in] mission Pointer to the mission which stage ended.
 */
void CP_UFOCarrierNextStage (mission_t *mission)
{
	switch (mission->stage) {
	case STAGE_NOT_ACTIVE:
		CP_UFOCarrierMissionStart(mission);
		break;
	default:
		CP_UFOCarrierMissionUpdate(mission);
		break;
	}
}

/**
 * @brief Spawns a UFO-Carrier mission
 */
void CP_SpawnUFOCarrier_f (void)
{
	/* only one carrier missions is allowed */
	if (CP_GetCarrierMission() != NULL)
		return;

	const installationTemplate_t *installationTemplate = INS_GetInstallationTemplateByType(INSTALLATION_ORBIT);
	if (!installationTemplate)
		return;

	if (Cmd_Argc() == 3) {
		const vec2_t pos = { atof(Cmd_Argv(1)), atof(Cmd_Argv(2)) };
		if (!MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN, NULL))) {
			INS_Build(installationTemplate, pos, _(installationTemplate->name));
			MAP_CenterPosition(pos);
		}
	}

	CP_CreateNewMission(INTERESTCATEGORY_UFOCARRIER, true);
}

/**
 * @brief Decide whether you hit and destroyed the carrier and spawns a new
 * carrier crash site mission
 */
void CP_AttackUFOCarrier_f (void)
{
	mission_t *mission = CP_GetCarrierMission();
	if (mission == NULL)
		return;

	if (!INS_HasType(INSTALLATION_ORBIT))
		return;

	/* check max amount - we can't win if we can't add the ufo */
	if (ccs.numUFOs >= MAX_UFOONGEOSCAPE)
		return;

	const aircraft_t* ufoTemplate = UFO_GetTemplate(UFO_CARRIER);
	aircraft_t *ufo = UFO_CreateFromTemplate(ufoTemplate);
	if (ufo == NULL) {
		Com_Error(ERR_DROP, "Could not add UFO-Carrier to geoscape");
		return;
	}
	ufo->mission = mission;
	mission->ufo = ufo;
	CP_GetRandomPosOnGeoscape(ufo->pos, true);
	CP_SpawnCrashSiteMission(ufo);
	if (mission->mapDef != NULL) {
		Com_Printf("spawned mapdef: %s\n", mission->mapDef->id);
	}
}
