/**
 * @file cp_aircraft_callbacks.c
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
#include "../cl_team.h" /* for CL_UpdateActorAircraftVar */
#include "../menu/m_main.h"
#include "../menu/m_nodes.h"
#include "../menu/m_popup.h"
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_aircraft_callbacks.h"
#include "cp_aircraft.h"
#include "cp_team.h"
#include "cp_mapfightequip.h" /* for AII_GetSlotItems */

/**
 * @brief Script function for AIR_AircraftReturnToBase
 * @note Sends the current aircraft back to homebase (called from aircraft menu).
 * @note This function updates some cvars for current aircraft.
 * @sa GAME_CP_MissionAutoGo_f
 * @sa GAME_CP_Results_f
 */
static void AIM_AircraftReturnToBase_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (base && base->aircraftCurrent) {
		AIR_AircraftReturnToBase(base->aircraftCurrent);
		AIR_AircraftSelect(base->aircraftCurrent);
	}
}

/**
 * @brief Select an aircraft from a base, by ID
 * @sa AIR_AircraftSelect
 * @sa AIM_PrevAircraft_f
 * @sa AIM_NextAircraft_f
 */
static void AIM_SelectAircraft_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (Cmd_Argc() < 2) {
		if (base->aircraftCurrent)
			AIR_AircraftSelect(base->aircraftCurrent);
		return;
	} else {
		const int i = atoi(Cmd_Argv(1));
		aircraft_t *aircraft = AIR_GetAircraftFromBaseByIDXSafe(base, i);
		if (aircraft != NULL)
			AIR_AircraftSelect(aircraft);
	}
}

/**
 * @brief Switch to next aircraft in base.
 * @sa AIR_AircraftSelect
 * @sa AIM_PrevAircraft_f
 */
static void AIM_NextAircraft_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || base->numAircraftInBase <= 0)
		return;

	if (!base->aircraftCurrent || base->aircraftCurrent == &base->aircraft[base->numAircraftInBase - 1])
		base->aircraftCurrent = &base->aircraft[0];
	else
		base->aircraftCurrent++;

	AIR_AircraftSelect(base->aircraftCurrent);
}

/**
 * @brief Switch to previous aircraft in base.
 * @sa AIR_AircraftSelect
 * @sa AIM_NextAircraft_f
 */
static void AIM_PrevAircraft_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base || base->numAircraftInBase <= 0)
		return;

	if (!base->aircraftCurrent || base->aircraftCurrent == &base->aircraft[0])
		base->aircraftCurrent = &base->aircraft[base->numAircraftInBase - 1];
	else
		base->aircraftCurrent--;

	AIR_AircraftSelect(base->aircraftCurrent);
}

/**
 * @brief Starts an aircraft or stops the current mission and let the aircraft idle around.
 */
static void AIM_AircraftStart_f (void)
{
	aircraft_t *aircraft;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (!base->aircraftCurrent) {
		Com_DPrintf(DEBUG_CLIENT, "Error - there is no current aircraft in this base\n");
		return;
	}

	/* Aircraft cannot start without Command Centre operational. */
	if (!B_GetBuildingStatus(base, B_COMMAND)) {
		MN_Popup(_("Notice"), _("No operational Command Centre in this base.\n\nAircraft can not start.\n"));
		return;
	}

	aircraft = base->aircraftCurrent;

	/* Aircraft cannot start without a pilot. */
	if (!aircraft->pilot) {
		MN_Popup(_("Notice"), _("There is no pilot assigned to this aircraft.\n\nAircraft can not start.\n"));
		return;
	}

	if (AIR_IsAircraftInBase(aircraft)) {
		/* reload its ammunition */
		AII_ReloadWeapon(aircraft);
	}
	MS_AddNewMessage(_("Notice"), _("Aircraft started"), qfalse, MSG_STANDARD, NULL);
	aircraft->status = AIR_IDLE;

	MAP_SelectAircraft(aircraft);
	/* Return to geoscape. */
	MN_PopMenu(qfalse);
	MN_PopMenu(qfalse);
}

#define SOLDIER_EQUIP_MENU_BUTTON_NO_AIRCRAFT_IN_BASE 1
#define SOLDIER_EQUIP_MENU_BUTTON_NO_SOLDIES_AVAILABLE 2
#define SOLDIER_EQUIP_MENU_BUTTON_OK 3
/**
 * @brief Determines the state of the equip soldier menu button:
 * @returns SOLIDER_EQUIP_MENU_BUTTON_NO_AIRCRAFT_IN_BASE if no aircraft in base
 * @returns SOLIDER_EQUIP_MENU_BUTTON_NO_SOLDIES_AVAILABLE if no soldiers available
 * @returns SOLIDER_EQUIP_MENU_BUTTON_OK if none of the above is true
 */
static int CL_EquipSoldierState (const aircraft_t * aircraft)
{
	if (!AIR_IsAircraftInBase(aircraft)) {
		return SOLDIER_EQUIP_MENU_BUTTON_NO_AIRCRAFT_IN_BASE;
	} else {
		if (E_CountHired(aircraft->homebase, EMPL_SOLDIER) <= 0)
			return SOLDIER_EQUIP_MENU_BUTTON_NO_SOLDIES_AVAILABLE;
		else
			return SOLDIER_EQUIP_MENU_BUTTON_OK;
	}
}

/**
 * @brief Returns the amount of assigned items for a given slot of a given aircraft
 * @param[in] type This is the slot type to get the amount of assigned items for
 * @param[in] aircraft The aircraft to count the items for (may not be NULL)
 * @return The amount of assigned items for the given slot
 */
static int AIR_GetSlotItems (aircraftItemType_t type, const aircraft_t *aircraft)
{
	int i, max, cnt = 0;
	const aircraftSlot_t *slot;

	assert(aircraft);

	switch (type) {
	case AC_ITEM_SHIELD:
		if (aircraft->shield.item)
			return 1;
		else
			return 0;
		break;
	case AC_ITEM_WEAPON:
		slot = aircraft->weapons;
		max = MAX_AIRCRAFTSLOT;
		break;
	case AC_ITEM_ELECTRONICS:
		slot = aircraft->electronics;
		max = MAX_AIRCRAFTSLOT;
		break;
	default:
		Com_Printf("AIR_GetSlotItems: Unknown type of slot : %i", type);
		return 0;
	}

	for (i = 0; i < max; i++)
		if (slot[i].item)
			cnt++;

	return cnt;
}

/**
 * @brief Sets aircraftCurrent and updates related cvars and menutexts.
 * @param[in] aircraft Pointer to given aircraft that should be selected in the menu.
 */
void AIR_AircraftSelect (aircraft_t* aircraft)
{
	static char aircraftInfo[256];
	base_t *base;
	int id;

	if (aircraft != NULL)
		base = aircraft->homebase;
	else
		base = NULL;

	if (!base || !base->numAircraftInBase) {
		MN_ResetData(TEXT_AIRCRAFT_INFO);
		return;
	}

	assert(aircraft);
	assert(aircraft->homebase == base);
	CL_UpdateActorAircraftVar(aircraft, EMPL_SOLDIER);

	Cvar_SetValue("mn_equipsoldierstate", CL_EquipSoldierState(aircraft));
	Cvar_Set("mn_aircraftstatus", AIR_AircraftStatusToName(aircraft));
	Cvar_Set("mn_aircraftinbase", AIR_IsAircraftInBase(aircraft) ? "1" : "0");
	Cvar_Set("mn_aircraftname", aircraft->name);
	if (!aircraft->tech)
		Com_Error(ERR_DROP, "No technology assigned to aircraft '%s'", aircraft->id);
	Cvar_Set("mn_aircraft_model", aircraft->tech->mdl);

	/* generate aircraft info text */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), _("Speed:\t%i km/h\n"),
		CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED));
	Q_strcat(aircraftInfo, va(_("Fuel:\t%i/%i\n"), CL_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
		CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Operational range:\t%i km\n"), CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE] *
		aircraft->stats[AIR_STATS_SPEED], AIR_STATS_OP_RANGE)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Weapons:\t%i on %i\n"), AIR_GetSlotItems(AC_ITEM_WEAPON, aircraft), aircraft->maxWeapons), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Armour:\t%i on 1\n"), AIR_GetSlotItems(AC_ITEM_SHIELD, aircraft)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Electronics:\t%i on %i"), AIR_GetSlotItems(AC_ITEM_ELECTRONICS, aircraft), aircraft->maxElectronics), sizeof(aircraftInfo));

	MN_RegisterText(TEXT_AIRCRAFT_INFO, aircraftInfo);

	/* compute the ID and... */
	for (id = 0; id < base->numAircraftInBase; id++) {
		if (aircraft == &base->aircraft[id])
			break;
	}

	/* the aircraft must really be in this base */
	assert(id != base->numAircraftInBase);

	base->aircraftCurrent = aircraft;
	Cvar_SetValue("mn_aircraft_id", id);

	/* ...update the GUI */
	MN_ExecuteConfunc("aircraft_change %i", id);
}

/**
 * @brief Update TEXT_AIRCRAFT_LIST with the current base aircraft names
 */
static void AIR_AircraftUpdateList_f (void)
{
	linkedList_t *list = NULL;
	int i;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	for (i = 0; i < base->numAircraftInBase; i++) {
		const aircraft_t * aircraft = &base->aircraft[i];
		LIST_AddString(&list, aircraft->name);
	}

	MN_RegisterLinkedListText(TEXT_AIRCRAFT_LIST, list);
}

void AIR_InitCallbacks (void)
{
	/* menu aircraft */
	Cmd_AddCommand("aircraft_start", AIM_AircraftStart_f, NULL);
	/* menu aircraft_equip, aircraft */
	Cmd_AddCommand("mn_next_aircraft", AIM_NextAircraft_f, NULL);
	Cmd_AddCommand("mn_prev_aircraft", AIM_PrevAircraft_f, NULL);
	Cmd_AddCommand("mn_select_aircraft", AIM_SelectAircraft_f, NULL);
	/* menu aircraft, popup_transferbaselist */
	Cmd_AddCommand("aircraft_return", AIM_AircraftReturnToBase_f, "Sends the current aircraft back to homebase");
	/* menu aircraft, aircraft_equip, aircraft_soldier */
	Cmd_AddCommand("aircraft_update_list", AIR_AircraftUpdateList_f, NULL);
}

void AIR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("aircraft_start");
	Cmd_RemoveCommand("mn_next_aircraft");
	Cmd_RemoveCommand("mn_prev_aircraft");
	Cmd_RemoveCommand("mn_select_aircraft");
	Cmd_RemoveCommand("aircraft_return");
	Cmd_RemoveCommand("aircraft_update_list");
}
