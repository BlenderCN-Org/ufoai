/**
 * @file cp_base_callbacks.c
 * @brief Menu related console command callbacks
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
#include "../cl_game.h"
#include "../cl_menu.h"
#include "../menu/m_popup.h"
#include "../renderer/r_draw.h"
#include "cp_campaign.h"
#include "cp_base_callbacks.h"
#include "cp_base.h"
#include "cp_map.h"
#include "cp_popup.h"

/** @brief Used from menu scripts as parameter for mn_select_base */
#define CREATE_NEW_BASE_ID -1

static cvar_t *mn_base_title;
static cvar_t *cl_start_buildings;
static building_t *buildingConstructionList[MAX_BUILDINGS];

/**
 * @brief Handles the list of constructable buildings.
 * @param[in] base The base to update the building list for
 * @param[in] building Add this building to the construction list
 * @note Called everytime a building was constructed and thus maybe other buildings
 * get available. The content is updated everytime B_BuildingInit is called
 * (i.e everytime the buildings-list is displayed/updated)
 */
static void B_BuildingAddToList (base_t *base, building_t *building)
{
	int count;
	assert(base);
	assert(building);
	assert(building->name);

	count = LIST_Count(base->buildingList);
	LIST_AddPointer(&base->buildingList, _(building->name));
	buildingConstructionList[count] = building->tpl;
}

/**
 * @brief Called when a base is opened or a new base is created on geoscape.
 * For a new base the baseID is -1.
 */
static void B_SelectBase_f (void)
{
	int baseID;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseID>\n", Cmd_Argv(0));
		return;
	}
	baseID = atoi(Cmd_Argv(1));
	/* check against MAX_BASES here! - only -1 will create a new base
	 * if we would check against ccs.numBases here, a click on the base summary
	 * base nodes would try to select unfounded bases */
	if (baseID >= 0 && baseID < MAX_BASES) {
		base_t *base = B_GetFoundedBaseByIDX(baseID);
		/* don't create a new base if the index was valid */
		if (base)
			B_SelectBase(base);
	} else if (baseID == CREATE_NEW_BASE_ID) {
		/* create a new base */
		B_SelectBase(NULL);
	}
}

/**
 * @brief Cycles to the next base.
 * @sa B_PrevBase
 * @sa B_SelectBase_f
 */
static void B_NextBase_f (void)
{
	int baseID;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	baseID = (base->idx + 1) % ccs.numBases;
	base = B_GetFoundedBaseByIDX(baseID);
	if (base)
		B_SelectBase(base);
}

/**
 * @brief Cycles to the previous base.
 * @sa B_NextBase
 * @sa B_SelectBase_f
 */
static void B_PrevBase_f (void)
{
	int baseID;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	baseID = base->idx;
	if (baseID > 0)
		baseID--;
	else
		baseID = ccs.numBases - 1;

	base = B_GetFoundedBaseByIDX(baseID);
	if (base)
		B_SelectBase(base);
}

/**
 * @brief Sets the title of the base to a cvar to prepare the rename menu.
 */
static void B_SetBaseTitle_f (void)
{
	Com_DPrintf(DEBUG_CLIENT, "INS_SetBaseTitle_f: #bases: %i\n", ccs.numBases);
	if (ccs.numBases < MAX_BASES) {
		char baseName[MAX_VAR];
		int i = 2;
		int j;

		if (ccs.numBases > 0)
			do {
				j = 0;
				Com_sprintf(baseName, lengthof(baseName), _("Base #%i"), i);
				while (j <= ccs.numBases && strcmp(baseName, ccs.bases[j++].name));
			} while (i++ <= ccs.numBases && j <= ccs.numBases);
		else
			Q_strncpyz(baseName, _("Home"), lengthof(baseName));

		Cvar_Set("mn_base_title", baseName);
	} else {
		MS_AddNewMessage(_("Notice"), _("You've reached the base limit."), qfalse, MSG_STANDARD, NULL);
		MN_PopMenu(qfalse);		/* remove the new base popup */
	}
}

/**
 * @brief Constructs a new base.
 * @sa B_NewBase
 */
static void B_BuildBase_f (void)
{
	const nation_t *nation;
	base_t *base = B_GetFirstUnfoundedBase();
	if (!base)
		return;

	assert(!base->founded);

	if (ccs.credits - ccs.curCampaign->basecost > 0) {
		const char *baseName = mn_base_title->string;
		if (baseName[0] == '\0')
			baseName = "Base";

		/* set up the base with buildings that have the autobuild flag set */
		B_SetUpBase(base, cp_start_employees->integer, cl_start_buildings->integer, newBasePos);

		ccs.numBases++;
		ccs.campaignStats.basesBuild++;
		ccs.mapAction = MA_NONE;
		CL_UpdateCredits(ccs.credits - ccs.curCampaign->basecost);
		Q_strncpyz(base->name, baseName, sizeof(base->name));
		nation = MAP_GetNation(base->pos);
		if (nation)
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new base has been built: %s (nation: %s)"), mn_base_title->string, _(nation->name));
		else
			Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("A new base has been built: %s"), mn_base_title->string);
		MS_AddNewMessage(_("Base built"), cp_messageBuffer, qfalse, MSG_CONSTRUCTION, NULL);
		B_ResetAllStatusAndCapacities(base, qtrue);
		AL_FillInContainment(base);
		PR_UpdateProductionCap(base);

		B_UpdateBaseCount();
		B_SelectBase(base);
	} else {
		if (r_geoscape_overlay->integer & OVERLAY_RADAR)
			MAP_SetOverlay("radar");
		if (ccs.mapAction == MA_NEWBASE)
			ccs.mapAction = MA_NONE;

		Com_sprintf(popupText, sizeof(popupText), _("Not enough credits to set up a new base."));
		CP_PopupList(_("Notice"), popupText);
	}
}

/**
 * @brief Creates console command to change the name of a base.
 * Copies the value of the cvar mn_base_title over as the name of the
 * current selected base
 */
static void B_ChangeBaseName_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	/* maybe called without base initialized or active */
	if (!base)
		return;

	Q_strncpyz(base->name, Cvar_GetString("mn_base_title"), sizeof(base->name));
}

/**
 * @brief Resets the currently selected building.
 *
 * Is called e.g. when leaving the build-menu
 */
static void B_ResetBuildingCurrent_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() == 2)
		ccs.instant_build = atoi(Cmd_Argv(1));

	B_ResetBuildingCurrent(base);
}

/**
 * @brief Initialises base.
 * @note This command is executed in the init node of the base menu.
 * It is called everytime the base menu pops up and sets the cvars.
 * @todo integrate building status tooltips from B_CheckBuildingStatusForMenu_f into this one
 */
static void B_BaseInit_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();
	qboolean baseUnderAttack;

	if (!base)
		return;

	baseUnderAttack = (base->baseStatus == BASE_UNDER_ATTACK);

	/* make sure the credits cvar is up-to-date */
	CL_UpdateCredits(ccs.credits);

	Cvar_SetValue("mn_base_num_aircraft", base->numAircraftInBase);

	MN_ExecuteConfunc("mn_buildings_reset");
	/* activate or deactivate the aircraft button */
	if (AIR_AircraftAllowed(base) && base->numAircraftInBase)
		MN_ExecuteConfunc("update_aircraft false \"%s\"", _("Manage your aircraft"));
	else {
		if (base->numAircraftInBase == 0)
			MN_ExecuteConfunc("update_aircraft true \"%s\"", _("Buy or produce at least one aircraft first."));
		else
			MN_ExecuteConfunc("update_aircraft true \"%s\"", va(_("Build a %s or %s first."), _("Small Hangar"), _("Large Hangar")));
	}

	if (BS_BuySellAllowed(base))
		MN_ExecuteConfunc("update_buysell false \"%s\"", _("Buy/Sell equipment"));
	else {
		if (baseUnderAttack)
			MN_ExecuteConfunc("update_buysell true \"%s\"", _("Base is under attack, you can't access this building !"));
		else
			MN_ExecuteConfunc("update_buysell true \"%s\"", va(_("Build a %s first."), _("Storage")));
	}

	if (ccs.numBases > 1 && !baseUnderAttack)
		MN_ExecuteConfunc("update_transfer false \"%s\"", _("Transfer equipment or personnel"));
	else {
		if (baseUnderAttack)
			MN_ExecuteConfunc("update_transfer true \"%s\"", _("Base is under attack, you can't access this building !"));
		else
			MN_ExecuteConfunc("update_transfer true \"%s\"", _("Build at least a second base to transfer equipment or personnel"));
	}

	if (RS_ResearchAllowed(base))
		MN_ExecuteConfunc("update_research false \"%s\"", _("Research new technology"));
	else {
		if (baseUnderAttack)
			MN_ExecuteConfunc("update_research true \"%s\"", _("Base is under attack, you can't access this building !"));
		else
			MN_ExecuteConfunc("update_research true \"%s\"", va(_("Build a %s first."), _("Laboratory")));
	}

	if (PR_ProductionAllowed(base)) {
		MN_ExecuteConfunc("update_prod false \"%s\"", _("Produce new equipment"));
	} else {
		if (baseUnderAttack)
			MN_ExecuteConfunc("update_prod true \"%s\"", _("Base is under attack, you can't access this building !"));
		else
			MN_ExecuteConfunc("update_prod true \"%s\"", va(_("Build a %s first."), _("Workshop")));
	}

	if (E_HireAllowed(base))
		MN_ExecuteConfunc("update_hire false \"%s\"", _("Hire employees"));
	else {
		if (baseUnderAttack)
			MN_ExecuteConfunc("update_hire true \"%s\"", _("Base is under attack, you can't access this building !"));
		else
			MN_ExecuteConfunc("update_hire true \"%s\"", va(_("Build %s first."), _("Living Quarters")));
	}

	if (AC_ContainmentAllowed(base))
		MN_ExecuteConfunc("update_containment false \"%s\"", _("Deal with Aliens in Alien Containment"));
	else
		MN_ExecuteConfunc("update_containment true \"%s\"", va(_("Build a %s first."), _("Containment")));

	if (HOS_HospitalAllowed(base))
		MN_ExecuteConfunc("update_hospital false \"%s\"", _("Medical and Surgery operations"));
	else {
		if (baseUnderAttack)
			MN_ExecuteConfunc("update_hospital true \"%s\"", _("Base is under attack, you can't access this building !"));
		else
			MN_ExecuteConfunc("update_hospital true \"%s\"", va(_("Build a %s first."), _("Hospital")));
	}
}

/**
 * @brief On destroy function for several building type.
 * @note this function is only used for sanity checks, and send to related function depending on building type.
 * @pre Functions below will be called AFTER the building is actually destroyed.
 * @sa B_BuildingDestroy_f
 * @todo Why does this exist? why is this not part of B_BuildingDestroy?
 */
static void B_BuildingOnDestroy_f (void)
{
	int baseIdx, buildingType;
	base_t *base;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <baseIdx> <buildingType>\n", Cmd_Argv(0));
		return;
	}

	buildingType = atoi(Cmd_Argv(2));
	if (buildingType < 0 || buildingType >= MAX_BUILDING_TYPE) {
		Com_Printf("B_BuildingOnDestroy_f: buildingType '%i' outside limits\n", buildingType);
		return;
	}

	baseIdx = atoi(Cmd_Argv(1));

	if (baseIdx < 0 || baseIdx >= MAX_BASES) {
		Com_Printf("B_BuildingOnDestroy_f: %i is outside bounds\n", baseIdx);
		return;
	}

	base = B_GetFoundedBaseByIDX(baseIdx);
	if (base) {
		switch (buildingType) {
		case B_WORKSHOP:
			PR_UpdateProductionCap(base);
			break;
		case B_STORAGE:
			B_RemoveItemsExceedingCapacity(base);
			break;
		case B_ALIEN_CONTAINMENT:
			if (base->capacities[CAP_ALIENS].cur - base->capacities[CAP_ALIENS].max > 0)
				AL_RemoveAliens(base, NULL, (base->capacities[CAP_ALIENS].cur - base->capacities[CAP_ALIENS].max), AL_RESEARCH);
			break;
		case B_LAB:
			RS_RemoveScientistsExceedingCapacity(base);
			break;
		case B_HANGAR: /* the Dropship Hangar */
		case B_SMALL_HANGAR:
			B_RemoveAircraftExceedingCapacity(base, buildingType);
			break;
		case B_UFO_HANGAR:
		case B_UFO_SMALL_HANGAR:
			B_RemoveUFOsExceedingCapacity(base, buildingType);
			break;
		case B_QUARTERS:
			E_DeleteEmployeesExceedingCapacity(base);
			break;
		case B_ANTIMATTER:
			B_RemoveAntimatterExceedingCapacity(base);
			break;
		default:
			/* handled in a seperate function, or number of buildings have no impact
			 * on how the building works */
			break;
		}
	} else
		Com_Printf("B_BuildingOnDestroy_f: base %i is not founded\n", baseIdx);
}

/**
 * @brief Update the building-list.
 * @sa B_BuildingInit_f
 */
static void B_BuildingInit (base_t* base)
{
	int i;

	/* maybe someone call this command before the bases are parsed?? */
	if (!base)
		return;

	Com_DPrintf(DEBUG_CLIENT, "B_BuildingInit: Updating b-list for '%s' (%i)\n", base->name, base->idx);
	Com_DPrintf(DEBUG_CLIENT, "B_BuildingInit: Buildings in base: %i\n", ccs.numBuildings[base->idx]);

	/* delete the whole linked list - it's rebuild in the loop below */
	LIST_Delete(&base->buildingList);

	for (i = 0; i < ccs.numBuildingTemplates; i++) {
		building_t *tpl = &ccs.buildingTemplates[i];
		/* make an entry in list for this building */

		if (tpl->visible) {
			const int numSameBuildings = B_GetNumberOfBuildingsInBaseByTemplate(base, tpl);

			if (tpl->moreThanOne) {
				/* skip if limit of BASE_SIZE*BASE_SIZE exceeded */
				if (numSameBuildings >= BASE_SIZE * BASE_SIZE)
					continue;
			} else if (numSameBuildings > 0) {
				continue;
			}

			/* if the building is researched add it to the list */
			if (RS_IsResearched_ptr(tpl->tech)) {
				B_BuildingAddToList(base, tpl);
			} else {
				Com_DPrintf(DEBUG_CLIENT, "Building not researched yet %s (tech idx: %i)\n",
					tpl->id, tpl->tech ? tpl->tech->idx : 0);
			}
		}
	}
	if (base->buildingCurrent)
		B_DrawBuilding(base, base->buildingCurrent);

	MN_RegisterLinkedListText(TEXT_BUILDINGS, base->buildingList);
}

/**
 * @brief Script command binding for B_BuildingInit
 */
static void B_BuildingInit_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	B_BuildingInit(base);
}

/**
 * @brief Opens the UFOpedia for the current selected building.
 */
static void B_BuildingInfoClick_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (base->buildingCurrent)
		UP_OpenWith(base->buildingCurrent->pedia);
}

/**
 * @brief Script function for clicking the building list text field.
 */
static void B_BuildingClick_f (void)
{
	int num, count;
	building_t *building;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* which building? */
	num = atoi(Cmd_Argv(1));

	Com_DPrintf(DEBUG_CLIENT, "B_BuildingClick_f: listnumber %i base %i\n", num, base->idx);

	count = LIST_Count(base->buildingList);
	if (num > count || num < 0) {
		Com_DPrintf(DEBUG_CLIENT, "B_BuildingClick_f: max exceeded %i/%i\n", num, count);
		return;
	}

	building = buildingConstructionList[num];

	base->buildingCurrent = building;
	B_DrawBuilding(base, building);

	ccs.baseAction = BA_NEWBUILDING;
}

/**
 * @brief We are doing the real destroy of a building here
 * @sa B_BuildingDestroy
 * @sa B_NewBuilding
 */
static void B_BuildingDestroy_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || !base->buildingCurrent)
		return;

	B_BuildingDestroy(base, base->buildingCurrent);

	B_ResetBuildingCurrent(base);
}

/**
 * @brief Console callback for B_BuildingStatus
 * @sa B_BuildingStatus
 */
static void B_BuildingStatus_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	/* maybe someone called this command before the buildings are parsed?? */
	if (!base || !base->buildingCurrent)
		return;

	B_BuildingStatus(base, base->buildingCurrent);
}

/**
 * @brief Builds a base map for tactical combat.
 * @sa SV_AssembleMap
 * @sa CP_BaseAttackChooseBase
 * @note Do we need day and night maps here, too? Sure!
 * @todo Search a empty field and add a alien craft there, also add alien
 * spawn points around the craft, also some trees, etc. for their cover
 * @todo Add soldier spawn points, the best place is quarters.
 * @todo We need to get rid of the tunnels to nirvana.
 */
static void B_AssembleMap_f (void)
{
	int row, col;
	char baseMapPart[1024];
	building_t *entry;
	char maps[2024];
	char coords[2048];
	int setUnderAttack = 0, baseID = 0;
	base_t* base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() < 2)
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <baseID> <setUnderAttack>\n", Cmd_Argv(0));
	else {
		if (Cmd_Argc() == 3)
			setUnderAttack = atoi(Cmd_Argv(2));
		baseID = atoi(Cmd_Argv(1));
		if (baseID < 0 || baseID >= ccs.numBases) {
			Com_DPrintf(DEBUG_CLIENT, "Invalid baseID: %i\n", baseID);
			return;
		}
		base = B_GetBaseByIDX(baseID);
	}

	if (!base) {
		Com_Printf("B_AssembleMap_f: No base to assemble\n");
		return;
	}

	/* reset menu text */
	MN_ResetData(TEXT_STANDARD);

	*maps = '\0';
	*coords = '\0';

	/* reset the used flag */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			if (base->map[row][col].building) {
				entry = base->map[row][col].building;
				entry->used = 0;
			}
		}

	/** @todo If a building is still under construction, it will be assembled as a finished part.
	 * Otherwise we need mapparts for all the maps under construction. */
	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++) {
			baseMapPart[0] = '\0';

			if (base->map[row][col].building) {
				entry = base->map[row][col].building;

				/* basemaps with needs are not (like the images in B_DrawBase) two maps - but one */
				/* this is why we check the used flag and continue if it was set already */
				if (!entry->used && entry->needs) {
					entry->used = 1;
				} else if (entry->needs) {
					Com_DPrintf(DEBUG_CLIENT, "B_AssembleMap_f: '%s' needs '%s' (used: %i)\n", entry->id, entry->needs, entry->used);
					entry->used = 0;
					continue;
				}

				if (entry->mapPart)
					Com_sprintf(baseMapPart, sizeof(baseMapPart), "b/%s", entry->mapPart);
				else
					Com_Printf("B_AssembleMap_f: Error - map has no mapPart set. Building '%s'\n'", entry->id);
			} else
				Q_strncpyz(baseMapPart, "b/empty", sizeof(baseMapPart));

			if (*baseMapPart) {
				Q_strcat(maps, baseMapPart, sizeof(maps));
				Q_strcat(maps, " ", sizeof(maps));
				/* basetiles are 16 units in each direction
				 * 512 / UNIT_SIZE = 16
				 * 512 is the size in the mapeditor and the worldplane for a
				 * single base map tile */
				Q_strcat(coords, va("%i %i %i ", col * 16, (BASE_SIZE - row - 1) * 16, 0), sizeof(coords));
			}
		}
	/* set maxlevel for base attacks */
	cl.mapMaxLevelBase = 6;

	SAV_QuickSave();

	Cbuf_AddText(va("map day \"%s\" \"%s\"\n", maps, coords));
}

/**
 * @brief Builds a random base
 *
 * call B_AssembleMap with a random base over script command 'base_assemble_rand'
 */
static void B_AssembleRandomBase_f (void)
{
	int setUnderAttack = 0;
	int randomBase = rand() % ccs.numBases;
	if (Cmd_Argc() < 2)
		Com_DPrintf(DEBUG_CLIENT, "Usage: %s <setUnderAttack>\n", Cmd_Argv(0));
	else
		setUnderAttack = atoi(Cmd_Argv(1));

	if (!ccs.bases[randomBase].founded) {
		Com_Printf("Base with id %i was not founded or already destroyed\n", randomBase);
		return;
	}

	Cbuf_AddText(va("base_assemble %i %i\n", randomBase, setUnderAttack));
}

/**
 * @brief Checks why a button in base menu is disabled, and create a popup to inform player
 */
static void B_CheckBuildingStatusForMenu_f (void)
{
	int i, num;
	int baseIdx;
	const char *buildingID;
	building_t *building;
	base_t *base = B_GetCurrentSelectedBase();

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s buildingID\n", Cmd_Argv(0));
		return;
	}

	buildingID = Cmd_Argv(1);
	building = B_GetBuildingTemplate(buildingID);

	if (!building || !base)
		return;

	/* Maybe base is under attack ? */
	if (base->baseStatus == BASE_UNDER_ATTACK) {
		Com_sprintf(popupText, sizeof(popupText), _("Base is under attack, you can't access this building !"));
		MN_Popup(_("Notice"), popupText);
		return;
	}

	baseIdx = base->idx;

	if (building->buildingType == B_HANGAR) {
		/* this is an exception because you must have a small or large hangar to enter aircraft menu */
		Com_sprintf(popupText, sizeof(popupText), _("You need at least one Hangar (and its dependencies) to use aircraft."));
		MN_Popup(_("Notice"), popupText);
		return;
	}

	num = B_GetNumberOfBuildingsInBaseByBuildingType(base, building->buildingType);
	if (num > 0) {
		int numUnderConstruction;
		/* maybe all buildings of this type are under construction ? */
		B_CheckBuildingTypeStatus(base, building->buildingType, B_STATUS_UNDER_CONSTRUCTION, &numUnderConstruction);
		if (numUnderConstruction == num) {
			int minDay = 99999;
			/* Find the building whose construction will finish first */
			for (i = 0; i < ccs.numBuildings[baseIdx]; i++) {
				if (ccs.buildings[baseIdx][i].buildingType == building->buildingType
					&& ccs.buildings[baseIdx][i].buildingStatus == B_STATUS_UNDER_CONSTRUCTION
					&& minDay > ccs.buildings[baseIdx][i].buildTime - (ccs.date.day - ccs.buildings[baseIdx][i].timeStart))
					minDay = ccs.buildings[baseIdx][i].buildTime - (ccs.date.day - ccs.buildings[baseIdx][i].timeStart);
			}
			Com_sprintf(popupText, sizeof(popupText), ngettext("Construction of building will be over in %i day.\nPlease wait to enter.", "Construction of building will be over in %i days.\nPlease wait to enter.",
				minDay), minDay);
			MN_Popup(_("Notice"), popupText);
			return;
		}

		if (!B_CheckBuildingDependencesStatus(base, building)) {
			building_t *dependenceBuilding = building->dependsBuilding;
			assert(building->dependsBuilding);
			if (B_GetNumberOfBuildingsInBaseByBuildingType(base, dependenceBuilding->buildingType) <= 0) {
				/* the dependence of the building is not built */
				Com_sprintf(popupText, sizeof(popupText), _("You need a building %s to make building %s functional."), _(dependenceBuilding->name), _(building->name));
				MN_Popup(_("Notice"), popupText);
				return;
			} else {
				/* maybe the dependence of the building is under construction
				 * note that we can't use B_STATUS_UNDER_CONSTRUCTION here, because this value
				 * is not use for every building (for exemple Command Centre) */
				for (i = 0; i < ccs.numBuildings[baseIdx]; i++) {
					if (ccs.buildings[baseIdx][i].buildingType == dependenceBuilding->buildingType
					 && ccs.buildings[baseIdx][i].buildTime > (ccs.date.day - ccs.buildings[baseIdx][i].timeStart)) {
						Com_sprintf(popupText, sizeof(popupText), _("Building %s is not finished yet, and is needed to use building %s."),
							_(dependenceBuilding->name), _(building->name));
						MN_Popup(_("Notice"), popupText);
						return;
					}
				}
				/* the dependence is built but doesn't work - must be because of their dependendes */
				Com_sprintf(popupText, sizeof(popupText), _("Make sure that the dependencies of building %s (%s) are operational, so that building %s may be used."),
					_(dependenceBuilding->name), _(dependenceBuilding->dependsBuilding->name), _(building->name));
				MN_Popup(_("Notice"), popupText);
				return;
			}
		}
		/* all buildings are OK: employees must be missing */
		if (building->buildingType == B_WORKSHOP && E_CountHired(base, EMPL_WORKER) <= 0) {
			Com_sprintf(popupText, sizeof(popupText), _("You need to recruit %s to use building %s."),
				E_GetEmployeeString(EMPL_WORKER), _(building->name));
			MN_Popup(_("Notice"), popupText);
			return;
		} else if (building->buildingType == B_LAB && E_CountHired(base, EMPL_SCIENTIST) <= 0) {
			Com_sprintf(popupText, sizeof(popupText), _("You need to recruit %s to use building %s."),
				E_GetEmployeeString(EMPL_SCIENTIST), _(building->name));
			MN_Popup(_("Notice"), popupText);
			return;
		}
	} else {
		Com_sprintf(popupText, sizeof(popupText), _("Build a %s first."), _(building->name));
		MN_Popup(_("Notice"), popupText);
		return;
	}
}

/** BaseSummary Callbacks: */

/**
 * @brief Open menu for basesummary.
 */
static void BaseSummary_SelectBase_f (void)
{
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <baseid>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	Cbuf_AddText(va("mn_pop; mn_select_base %i; mn_push basesummary\n", i));
}

/**
 * @brief Base Summary menu init function.
 * @note Command to call this: basesummary_init
 * @note Should be called whenever the Base Summary menu gets active.
 */
static void BaseSummary_Init_f (void)
{
	static char textStatsBuffer[1024];
	static char textInfoBuffer[256];
	int i;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base) {
		Com_Printf("No base selected\n");
		return;
	} else {
		baseCapacities_t cap;
		production_queue_t *queue;
		technology_t *tech;
		int totalEmployees = 0;
		int tmp;

		/* wipe away old buffers */
		textStatsBuffer[0] = textInfoBuffer[0] = 0;

		Q_strcat(textInfoBuffer, _("^BAircraft\n"), sizeof(textInfoBuffer));
		for (i = 0; i <= MAX_HUMAN_AIRCRAFT_TYPE; i++)
			Q_strcat(textInfoBuffer, va("\t%s:\t\t\t\t%i\n", AIR_GetAircraftString(i),
				AIR_CountTypeInBase(base, i)), sizeof(textInfoBuffer));

		Q_strcat(textInfoBuffer, "\n", sizeof(textInfoBuffer));

		Q_strcat(textInfoBuffer, _("^BEmployees\n"), sizeof(textInfoBuffer));
		for (i = 0; i < MAX_EMPL; i++) {
			tmp = E_CountHired(base, i);
			totalEmployees += tmp;
			Q_strcat(textInfoBuffer, va("\t%s:\t\t\t\t%i\n", E_GetEmployeeString(i), tmp), sizeof(textInfoBuffer));
		}

		/* link into the menu */
		MN_RegisterText(TEXT_STANDARD, textInfoBuffer);

		Q_strcat(textStatsBuffer, _("^BBuildings\t\t\t\t\t\tCapacity\t\t\t\tAmount\n"), sizeof(textStatsBuffer));
		for (i = 0; i < ccs.numBuildingTemplates; i++) {
			const building_t* b = &ccs.buildingTemplates[i];

			/* only show already researched buildings */
			if (!RS_IsResearched_ptr(b->tech))
				continue;

			cap = B_GetCapacityFromBuildingType(b->buildingType);
			if (cap == MAX_CAP)
				continue;

			if (!B_GetNumberOfBuildingsInBaseByBuildingType(base, b->buildingType))
				continue;

			/* Check if building is functional (see comments in B_UpdateBaseCapacities) */
			if (B_GetBuildingStatus(base, b->buildingType)) {
				Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i/%i", _(b->name),
					base->capacities[cap].cur, base->capacities[cap].max), sizeof(textStatsBuffer));
			} else {
				if (b->buildingStatus == B_STATUS_UNDER_CONSTRUCTION) {
					const int daysLeft = b->timeStart + b->buildTime - ccs.date.day;
					/* if there is no day left the status should not be B_STATUS_UNDER_CONSTRUCTION */
					assert(daysLeft);
					Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i %s", _(b->name), daysLeft, ngettext("day", "days", daysLeft)), sizeof(textStatsBuffer));
				} else {
					Q_strcat(textStatsBuffer, va("%s:\t\t\t\t\t\t%i/%i", _(b->name), base->capacities[cap].cur, 0), sizeof(textStatsBuffer));
				}
			}
			Q_strcat(textStatsBuffer, va("\t\t\t\t%i\n", B_GetNumberOfBuildingsInBaseByBuildingType(base, b->buildingType)), sizeof(textStatsBuffer));
		}

		Q_strcat(textStatsBuffer, "\n", sizeof(textStatsBuffer));

		Q_strcat(textStatsBuffer, _("^BProduction\t\t\t\t\t\tQuantity\t\t\t\tPercent\n"), sizeof(textStatsBuffer));
		queue = &ccs.productions[base->idx];
		if (queue->numItems > 0) {
			for (i = 0; i < queue->numItems; i++) {
				const production_t *production = &queue->items[i];
				const objDef_t *objDef = production->item;
				const aircraft_t *aircraft = production->aircraft;
				const char *name = (objDef) ? objDef->name : _(aircraft->name);
				assert(name);

				/** @todo use the same method as we do in PR_ProductionInfo */
				Q_strcat(textStatsBuffer, va(_("%s\t\t\t\t\t\t%d\t\t\t\t%.2f%%\n"), name,
					production->amount, production->percentDone * 100), sizeof(textStatsBuffer));
			}
		} else {
			Q_strcat(textStatsBuffer, _("Nothing\n"), sizeof(textStatsBuffer));
		}

		Q_strcat(textStatsBuffer, "\n", sizeof(textStatsBuffer));

		Q_strcat(textStatsBuffer, _("^BResearch\t\t\t\t\t\tScientists\t\t\t\tPercent\n"), sizeof(textStatsBuffer));
		tmp = 0;
		for (i = 0; i < ccs.numTechnologies; i++) {
			tech = RS_GetTechByIDX(i);
			if (tech->base == base && (tech->statusResearch == RS_RUNNING || tech->statusResearch == RS_PAUSED)) {
				Q_strcat(textStatsBuffer, va(_("%s\t\t\t\t\t\t%d\t\t\t\t%1.2f%%\n"), _(tech->name),
					tech->scientists, (1 - tech->time / tech->overalltime) * 100), sizeof(textStatsBuffer));
				tmp++;
			}
		}
		if (!tmp)
			Q_strcat(textStatsBuffer, _("Nothing\n"), sizeof(textStatsBuffer));

		/* link into the menu */
		MN_RegisterText(TEXT_STATS_BASESUMMARY, textStatsBuffer);
	}
}

/** Init/Shutdown functions */

/** @todo unify the names into mn_base_* */
void B_InitCallbacks (void)
{
	mn_base_title = Cvar_Get("mn_base_title", "", 0, NULL);
	cl_start_buildings = Cvar_Get("cl_start_buildings", "1", CVAR_ARCHIVE, "Start with initial buildings in your first base");
	Cvar_Set("mn_base_cost", va(_("%i c"), ccs.curCampaign->basecost));
	Cvar_SetValue("mn_base_count", ccs.numBases);
	Cvar_SetValue("mn_base_max", MAX_BASES);

	Cmd_AddCommand("mn_prev_base", B_PrevBase_f, "Go to the previous base");
	Cmd_AddCommand("mn_next_base", B_NextBase_f, "Go to the next base");
	Cmd_AddCommand("mn_select_base", B_SelectBase_f, "Select a founded base by index");
	Cmd_AddCommand("mn_build_base", B_BuildBase_f, NULL);
	Cmd_AddCommand("mn_set_base_title", B_SetBaseTitle_f, NULL);
	Cmd_AddCommand("base_changename", B_ChangeBaseName_f, "Called after editing the cvar base name");
	Cmd_AddCommand("base_init", B_BaseInit_f, NULL);
	Cmd_AddCommand("base_assemble", B_AssembleMap_f, "Called to assemble the current selected base");
	Cmd_AddCommand("base_assemble_rand", B_AssembleRandomBase_f, NULL);
	Cmd_AddCommand("building_init", B_BuildingInit_f, NULL);
	Cmd_AddCommand("building_status", B_BuildingStatus_f, NULL);
	Cmd_AddCommand("building_destroy", B_BuildingDestroy_f, "Function to destroy a building (select via right click in baseview first)");
	Cmd_AddCommand("building_ufopedia", B_BuildingInfoClick_f, "Opens the UFOpedia for the current selected building");
	Cmd_AddCommand("check_building_status", B_CheckBuildingStatusForMenu_f, "Create a popup to inform player why he can't use a button");
	Cmd_AddCommand("buildings_click", B_BuildingClick_f, "Opens the building information window in construction mode");
	Cmd_AddCommand("reset_building_current", B_ResetBuildingCurrent_f, NULL);
	Cmd_AddCommand("building_ondestroy", B_BuildingOnDestroy_f, "Destroy a building");
	Cmd_AddCommand("basesummary_init", BaseSummary_Init_f, "Init function for Base Statistics menu");
	Cmd_AddCommand("basesummary_selectbase", BaseSummary_SelectBase_f, "Opens Base Statistics menu in base");
}

/** @todo unify the names into mn_base_* */
void B_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("basesummary_init");
	Cmd_RemoveCommand("basesummary_selectbase");
	Cmd_RemoveCommand("mn_prev_base");
	Cmd_RemoveCommand("mn_next_base");
	Cmd_RemoveCommand("mn_select_base");
	Cmd_RemoveCommand("mn_build_base");
	Cmd_RemoveCommand("base_changename");
	Cmd_RemoveCommand("mn_set_base_title");
	Cmd_RemoveCommand("base_init");
	Cmd_RemoveCommand("base_assemble");
	Cmd_RemoveCommand("base_assemble_rand");
	Cmd_RemoveCommand("building_init");
	Cmd_RemoveCommand("building_status");
	Cmd_RemoveCommand("building_destroy");
	Cmd_RemoveCommand("building_ufopedia");
	Cmd_RemoveCommand("check_building_status");
	Cmd_RemoveCommand("buildings_click");
	Cmd_RemoveCommand("reset_building_current");
	Cmd_RemoveCommand("building_ondestroy");
	Cvar_Delete("mn_base_max");
	Cvar_Delete("mn_base_cost");
	Cvar_Delete("mn_base_title");
	Cvar_Delete("mn_base_id");
	Cvar_Delete("mn_base_count");
}
