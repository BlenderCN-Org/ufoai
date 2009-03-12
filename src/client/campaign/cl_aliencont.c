/**
 * @file cl_aliencont.c
 * @brief Deals with the Alien Containment stuff.
 * @note Collecting and managing aliens functions prefix: AL_
 * @note Alien Containment menu functions prefix: AC_
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "../cl_le.h"
#include "../mxml/mxml_ufoai.h"
#include "cl_campaign.h"
#include "cl_aliencont_callbacks.h"

/**
 * Collecting aliens functions for aircraft
 */

/**
 * @brief Returns the alien cargo for the given aircraft
 * @param[in] aircraft The aircraft to get the cargo for
 * @note It's assumed that aircraft is a valid aircraft pointer
 * @returns The aircraft cargo for (dead or alive) aliens
 */
aliensTmp_t *AL_GetAircraftAlienCargo (const aircraft_t *aircraft)
{
	assert(aircraft);
	return ccs.aliencargo[aircraft->idx];
}

/**
 * @brief Returns the amount of different alien races on board of the given aircraft
 * @param[in] aircraft The aircraft to return the amount of different alien races for
 * @sa AL_SetAircraftAlienCargoTypes
 * @note It's assumed that aircraft is a valid aircraft pointer
 * @return The amount of different alien races
 */
int AL_GetAircraftAlienCargoTypes (const aircraft_t *aircraft)
{
	assert(aircraft);
	return ccs.alientypes[aircraft->idx];
}

/**
 * @brief Sets the value of how many different alien races the aircraft has collected
 * @param[in] aircraft The aircraft to set the different alien races amount for
 * @param[in] alienCargoTypes The different alien race amount
 * @note dead or alive doesn't matter here
 * @note It's assumed that aircraft is a valid aircraft pointer
 * @return The amount of alien races the aircraft holds at the moment
 * @sa AL_GetAircraftAlienCargoTypes
 */
int AL_SetAircraftAlienCargoTypes (const aircraft_t *aircraft, int alienCargoTypes)
{
	assert(aircraft);
	ccs.alientypes[aircraft->idx] = alienCargoTypes;
	return ccs.alientypes[aircraft->idx];
}

/**
 * @brief Searches an existing index in the alien cargo of an aircraft, or returns the next free index of
 * the alien cargo if the team defintion wasn't found in the current alien cargo.
 * @param[in] aircraft The aircraft that should have the given team definition in its alien cargo
 * @param[in] teamDef The team definition that should be searched for
 * @return The index of the team definition in the alien cargo of the given aircraft
 */
static inline int AL_GetCargoIndexForTeamDefintion (const aircraft_t *aircraft, const teamDef_t *teamDef)
{
	aliensTmp_t *cargo = AL_GetAircraftAlienCargo(aircraft);
	const int alienCargoTypes = AL_GetAircraftAlienCargoTypes(aircraft);
	int i;

	for (i = 0; i < alienCargoTypes; i++, cargo++) {
		if (cargo->teamDef == teamDef)
			break;
	}

	/* in case teamdef wasn't found, return the next free index */
	assert(i < MAX_CARGO);
	return i;
}

/**
 * @brief Adds an alientype to an aircraft cargo
 * @param[in] aircraft The aircraft that owns the alien cargo to add the alien race to
 * @param[in] teamDef The team definition of the alien race to add to the alien cargo container of the
 * given aircraft
 * @param[in] amount The amount of aliens of the given race (@c teamDef ) that should be added to
 * the alien cargo
 * @param[in] dead true for cases where the aliens should be added as dead to the alien cargo - false for
 * living aliens
 * @todo Return false for cases where the alien race could not be added to the alien cargo of the aircraft
 * @returns Currently always true
 */
qboolean AL_AddAlienTypeToAircraftCargo (const aircraft_t *aircraft, const teamDef_t *teamDef, int amount, qboolean dead)
{
	aliensTmp_t *cargo = AL_GetAircraftAlienCargo(aircraft);
	const int alienCargoTypes = AL_GetAircraftAlienCargoTypes(aircraft);
	const int index = AL_GetCargoIndexForTeamDefintion(aircraft, teamDef);

	if (!cargo[index].teamDef)
		AL_SetAircraftAlienCargoTypes(aircraft, alienCargoTypes + 1);
	cargo[index].teamDef = teamDef;

	if (dead)
		cargo[index].amount_dead += amount;
	else
		cargo[index].amount_alive += amount;

	return qtrue;
}

/**
 * General Collecting aliens functions
 */

/**
 * @brief Prepares Alien Containment - names, states, and zeroed amount.
 * @param[in] base Pointer to the base with AC.
 * @sa B_BuildBase
 * @sa AL_AddAliens
 */
void AL_FillInContainment (base_t *base)
{
	int i, counter = 0;
	aliensCont_t *containment;

	assert(base);
	containment = base->alienscont;

	for (i = 0; i < csi.numTeamDefs; i++) {
		if (!CHRSH_IsTeamDefAlien(&csi.teamDef[i]))
			continue;
		if (counter >= MAX_ALIENCONT_CAP)
			Sys_Error("Overflow in AL_FillInContainment");
		containment[counter].teamDef = &csi.teamDef[i];	/* Link to global race index. */
		containment[counter].amount_alive = 0;
		containment[counter].amount_dead = 0;
		/* for sanity checking */
		containment[counter].tech = RS_GetTechByID(csi.teamDef[i].tech);
		if (!containment[counter].tech)
			Sys_Error("Could not find a valid tech for '%s'\n", containment[i].teamDef->name);
		counter++;
		Com_DPrintf(DEBUG_CLIENT, "AL_FillInContainment: type: %s tech-index: %i\n", containment[i].teamDef->name, containment[i].tech->idx);
	}
	base->capacities[CAP_ALIENS].cur = 0;
}

/**
 * @brief Index of alien race to its name.
 * @param[in] teamDefIdx Index of alien race in teamDef array.
 * @return name (untranslated) or NULL if no definition found.
 */
const char *AL_AlienTypeToName (int teamDefIdx)
{
	if (teamDefIdx < 0 || teamDefIdx >= csi.numTeamDefs) {
		Com_Printf("AL_AlienTypeToName: invalid team index %i\n", teamDefIdx);
		return NULL;
	}
	return csi.teamDef[teamDefIdx].name;
}

/**
 * @brief Collecting stunned aliens and alien bodies after the mission.
 * @param[in] aircraft Pointer to the aircraft with cargo.
 * @sa CL_ParseResults
 * @sa CL_GameAutoGo
 */
void AL_CollectingAliens (aircraft_t *aircraft)
{
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse)
			continue;
		if (LE_IsActor(le) && le->team == TEAM_ALIEN) {
			assert(le->teamDef);

			if (LE_IsStunned(le))
				AL_AddAlienTypeToAircraftCargo(aircraft, le->teamDef, 1, qfalse);
			else if (LE_IsDead(le))
				AL_AddAlienTypeToAircraftCargo(aircraft, le->teamDef, 1, qtrue);
		}
	}
}

/**
 * @brief Puts alien cargo into Alien Containment.
 * @param[in] aircraft Aircraft transporting cargo to homebase.
 * @sa CL_AircraftReturnedToHomeBase
 * @sa AL_FillInContainment
 * @note an event mail about missing breathing tech will be triggered if necessary.
 */
void AL_AddAliens (aircraft_t *aircraft)
{
	int i, j, k, alienCargoTypes;
	const objDef_t *alienBreathingObjDef;
	base_t *tobase;
	const aliensTmp_t *cargo;
	qboolean messageAlreadySet = qfalse;
	qboolean alienBreathing = qfalse;
	qboolean limit = qfalse;

	assert(aircraft);
	tobase = aircraft->homebase;
	assert(tobase);

	if (!B_GetBuildingStatus(tobase, B_ALIEN_CONTAINMENT)) {
		MS_AddNewMessage(_("Notice"), _("You cannot process aliens yet. Alien Containment not ready in this base."), qfalse, MSG_STANDARD, NULL);
		return;
	}

	cargo = AL_GetAircraftAlienCargo(aircraft);
	alienCargoTypes = AL_GetAircraftAlienCargoTypes(aircraft);

	alienBreathing = RS_IsResearched_ptr(RS_GetTechByID("rs_alien_breathing"));
	alienBreathingObjDef = INVSH_GetItemByID("brapparatus");
	if (!alienBreathingObjDef)
		Sys_Error("AL_AddAliens: Could not get brapparatus item definition");

	for (i = 0; i < alienCargoTypes; i++) {
		for (j = 0; j < ccs.numAliensTD; j++) {
			assert(tobase->alienscont[j].teamDef);
			assert(cargo[i].teamDef);
			if (tobase->alienscont[j].teamDef == cargo[i].teamDef) {
				tobase->alienscont[j].amount_dead += cargo[i].amount_dead;
				/* Add breathing apparatuses to aircraft cargo so that they are processed with other collected items */
				AII_CollectItem(aircraft, alienBreathingObjDef, cargo[i].amount_dead);
				if (cargo[i].amount_alive <= 0)
					continue;
				if (!alienBreathing && !cargo[i].teamDef->robot) {
					/* We can not store living (i.e. no robots or dead bodies) aliens without rs_alien_breathing tech */
					tobase->alienscont[j].amount_dead += cargo[i].amount_alive;
					/* Add breathing apparatuses as well */
					AII_CollectItem(aircraft, alienBreathingObjDef, cargo[i].amount_alive);
					/* only once */
					if (!messageAlreadySet) {
						MS_AddNewMessage(_("Notice"), _("You can't hold live aliens yet. Aliens died."), qfalse, MSG_DEATH, NULL);
						messageAlreadySet = qtrue;
					}
					if (!ccs.breathingMailSent) {
						Cmd_ExecuteString("addeventmail alienbreathing");
						ccs.breathingMailSent = qtrue;
					}
				} else {
					for (k = 0; k < cargo[i].amount_alive; k++) {
						/* Check base capacity. */
						if (AL_CheckAliveFreeSpace(tobase, NULL, 1)) {
							AL_ChangeAliveAlienNumber(tobase, &(tobase->alienscont[j]), 1);
						} else {
							/* Every exceeding alien is killed
							 * Display a message only when first one is killed */
							if (!limit) {
								tobase->capacities[CAP_ALIENS].cur = tobase->capacities[CAP_ALIENS].max;
								MS_AddNewMessage(_("Notice"), _("You don't have enough space in Alien Containment. Some aliens got killed."), qfalse, MSG_STANDARD, NULL);
								limit = qtrue;
							}
							/* Just kill aliens which don't fit the limit. */
							tobase->alienscont[j].amount_dead++;
							AII_CollectItem(aircraft, alienBreathingObjDef, 1);
						}
					}
					/* only once */
					if (!messageAlreadySet) {
						MS_AddNewMessage(_("Notice"), _("You've captured new aliens."), qfalse, MSG_STANDARD, NULL);
						messageAlreadySet = qtrue;
					}
				}
				break;
			}
		}
	}

	for (i = 0; i < ccs.numAliensTD; i++) {
		technology_t *tech = tobase->alienscont[i].tech;
#ifdef DEBUG
		if (!tech)
			Sys_Error("AL_AddAliens: Failed to initialize the tech for '%s'\n", tobase->alienscont[i].teamDef->name);
#endif
		/* we need this to let RS_Collected_ return true */
		if (tobase->alienscont[i].amount_alive + tobase->alienscont[i].amount_dead > 0)
			RS_MarkCollected(tech);
#ifdef DEBUG
		/* print all of them */
		if (tobase->alienscont[i].amount_alive > 0)
			Com_DPrintf(DEBUG_CLIENT, "AL_AddAliens alive: %s amount: %i\n", tobase->alienscont[i].teamDef->name, tobase->alienscont[i].amount_alive);
		if (tobase->alienscont[i].amount_dead > 0)
			Com_DPrintf(DEBUG_CLIENT, "AL_AddAliens bodies: %s amount: %i\n", tobase->alienscont[i].teamDef->name, tobase->alienscont[i].amount_dead);
#endif
	}

	/* we shouldn't have any more aliens on the aircraft after this */
	AL_SetAircraftAlienCargoTypes(aircraft, 0);
}

/**
 * @brief Removes alien(s) from Alien Containment.
 * @param[in,out] base Pointer to the base where we will perform action (remove, add, ... aliens).
 * @param[in] alienType Type of the alien (a teamDef_t pointer)
 * @param[in] amount Amount of aliens to be removed.
 * @param[in] action Type of action (see alienCalcType_t).
 * @sa AC_KillAll_f
 * @sa AC_KillOne_f
 * @note Call with NULL name when no matters what type to remove.
 * @todo integrate this with research system
 */
void AL_RemoveAliens (base_t *base, const teamDef_t *alienType, int amount, const alienCalcType_t action)
{
	int j, toremove;
	int maxamount = 0; /* amount (of alien type), which is max in Containment) */
	int maxidx = 0;
	aliensCont_t *containment;

	assert(base);
	containment = base->alienscont;

	switch (action) {
	case AL_RESEARCH:
		if (!alienType) {
			/* Search for the type of alien, which has max amount
			 * in Alien Containment, then remove (amount). */
			while (amount > 0) {
				/* Find the type with maxamount. */
				for (j = 0; j < ccs.numAliensTD; j++) {
					if (maxamount < containment[j].amount_alive) {
						maxamount = containment[j].amount_alive;
						maxidx = j;
					}
				}
				if (maxamount == 0) {
					/* That should never happen. */
					Com_Printf("AL_RemoveAliens: unable to find alive aliens\n");
					return;
				}
				if (maxamount == 1) {
					/* If only one here, just remove. */
					AL_ChangeAliveAlienNumber(base, &containment[maxidx], -1);
					containment[maxidx].amount_dead++;
					--amount;
				} else {
					/* If more than one, remove the amount. */
					toremove = maxamount - 1;
					if (toremove > amount)
						toremove = amount;
					AL_ChangeAliveAlienNumber(base, &containment[maxidx], -toremove);
					containment[maxidx].amount_dead += toremove;
					amount -= toremove;
				}
			}
		}
		break;
	case AL_KILL:
		/* We ignore 2nd and 3rd parameter of AL_RemoveAliens() here. */
		for (j = 0; j < ccs.numAliensTD; j++) {
			if (containment[j].amount_alive > 0) {
				containment[j].amount_dead += containment[j].amount_alive;
				AL_ChangeAliveAlienNumber(base, &containment[j], -containment[j].amount_alive);
			}
		}
		break;
	case AL_KILLONE:
		/* We ignore 3rd parameter of AL_RemoveAliens() here. */
		for (j = 0; j < ccs.numAliensTD; j++) {
			assert(containment[j].teamDef);
			if (containment[j].teamDef == alienType) {
				if (containment[j].amount_alive == 0)
					return;
				/* We are killing only one here, so we
				 * don't care about amount param.   */
				AL_ChangeAliveAlienNumber(base, &containment[j], -1);
				containment[j].amount_dead++;
				break;
			}
		}
		break;
	case AL_ADDALIVE:
		/* We ignore 3rd parameter of AL_RemoveAliens() here: add only 1 alien */
		if (!AL_CheckAliveFreeSpace(base, NULL, 1)) {
			return; /* stop because we will else exceed the max of aliens */
		}
		for (j = 0; j < ccs.numAliensTD; j++) {
			assert(containment[j].teamDef);
			if (containment[j].teamDef == alienType) {
				AL_ChangeAliveAlienNumber(base, &containment[j], 1);
				break;
			}
		}
		/**@todo why should we update actual visible containment here? */
		/* aliencontCurrent = &containment[j]; */
		break;
	case AL_ADDDEAD:
		for (j = 0; j < ccs.numAliensTD; j++) {
			assert(containment[j].teamDef);
			if (containment[j].teamDef == alienType) {
				containment[j].amount_dead++;
				break;
			}
		}
		/**@todo why should we update actual visible containment here? */
		/* aliencontCurrent = &containment[j]; */
		break;
	default:
		break;
	}
}

/**
 * @brief Get index of alien.
 * @param[in] alienType Pointer to alien type.
 * @return Index of alien in alien containment (so less than @c ccs.numAliensTD)
 * @note It does NOT return the global team index from @c csi.teamDef array.
 * That would be @c alienType->idx
 * @sa RS_AssignTechLinks
 * @sa AL_GetAlienGlobalIDX
 */
static int AL_GetAlienIDX (const teamDef_t *alienType)
{
	int i, index;

	index = 0;
	for (i = 0; i < csi.numTeamDefs; i++) {
		if (alienType == &csi.teamDef[i])
			return index;
		if (CHRSH_IsTeamDefAlien(&csi.teamDef[i]))
			index++;
	}

	Com_Printf("AL_GetAlienIDX: Alien \"%s\" not found!\n", alienType->id);
	return -1;
}

/**
 * @brief Returns global alien index.
 * @param[in] idx Alien index in Alien Containment.
 * @return Global alien index in csi.teamDef array.
 * @sa AL_GetAlienIDX
 */
int AL_GetAlienGlobalIDX (int idx)
{
	int i, counter = 0;

	for (i = 0; i < csi.numTeamDefs; i++) {
		if (CHRSH_IsTeamDefAlien(&csi.teamDef[i])) {
			if (counter == idx)
				return i;
			counter++;
		}
	}
	Com_Printf("AL_GetAlienGlobalIDX: Alien with AC index %i not found!\n", idx);
	return -1;
}

/**
 * @brief Get amount of live aliens or alien bodies stored in Containment.
 * @param[in] alienType The alien type to check for
 * @param[in] base The base to count in
 * @param[in] reqtype Requirement type (RS_LINK_ALIEN/RS_LINK_ALIEN_DEAD).
 * @return Amount of desired alien/body.
 * @sa RS_RequirementsMet
 * @sa RS_CheckCollected
 */
int AL_GetAlienAmount (const teamDef_t *alienType, requirementType_t reqtype, const base_t *base)
{
	const aliensCont_t *containment;
	int alienTypeIndex;

	assert(alienType);
	assert(base);
	alienTypeIndex = AL_GetAlienIDX(alienType);
	assert(alienTypeIndex >= 0);
	containment = &base->alienscont[alienTypeIndex];

	switch (reqtype) {
	case RS_LINK_ALIEN:
		return containment->amount_alive;
	case RS_LINK_ALIEN_DEAD:
		return containment->amount_dead;
	default:
		return containment->amount_dead;
	}
}

/**
 * @brief Counts live aliens in base.
 * @param[in] base Pointer to the base
 * @return amount of all live aliens stored in containment
 * @note must not return 0 if hasBuilding[B_ALIEN_CONTAINMENT] is qfalse: used to update capacity
 * @sa AL_ChangeAliveAlienNumber
 * @sa B_ResetAllStatusAndCapacities_f
 */
int AL_CountInBase (const base_t *base)
{
	int j;
	int amount = 0;

	assert(base);

	for (j = 0; j < ccs.numAliensTD; j++) {
		if (base->alienscont[j].teamDef)
			amount += base->alienscont[j].amount_alive;
	}

	return amount;
}

/**
 * @brief Add / Remove live aliens to Alien Containment.
 * @param[in] base Pointer to the base where Alien Cont. should be checked.
 * @param[in] containment Pointer to the containment
 * @param[in] num Number of alien to be added/removed
 * @pre free space has already been checked
 */
void AL_ChangeAliveAlienNumber (base_t *base, aliensCont_t *containment, int num)
{
	assert(base);
	assert(containment);

	/* Just a sanity check -- should never be reached */
	if (!AL_CheckAliveFreeSpace(base, containment, num))
		Com_Error(ERR_DROP, "AL_ChangeAliveAlienNumber: Can't add/remove %i live aliens, (capacity: %i/%i, Alien Containment Status: %i)\n",
			num, base->capacities[CAP_ALIENS].cur, base->capacities[CAP_ALIENS].max,
			B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT));

	containment->amount_alive += num;
	base->capacities[CAP_ALIENS].cur += num;

#ifdef DEBUG
	if (base->capacities[CAP_ALIENS].cur != AL_CountInBase(base))
		Com_Printf("AL_ChangeAliveAlienNumber: Wrong capacity in Alien containment: %i instead of %i\n",
			base->capacities[CAP_ALIENS].cur, AL_CountInBase(base));
#endif
}

/**
 * @brief Check if live aliens can be added/removed to Alien Containment.
 * @param[in] base Pointer to the base where Alien Cont. should be checked.
 * @param[in] containment Pointer to the containment (may be @c NULL when adding
 * aliens or if you don't care about alien type of alien you're removing)
 * @param[in] num Number of alien to be added/removed
 * @return qtrue if action may be performed in base
 */
qboolean AL_CheckAliveFreeSpace (const base_t *base, const aliensCont_t *containment, const int num)
{
	/* you need Alien Containment and it's dependencies to handle aliens */
	if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT))
		return qfalse;

	if (num > 0) {
		/* We add aliens */
		if (base->capacities[CAP_ALIENS].cur + num > base->capacities[CAP_ALIENS].max)
			return qfalse;
	} else {
		if (base->capacities[CAP_ALIENS].cur + num < 0)
			return qfalse;
		if (containment && (containment->amount_alive + num < 0))
			return qfalse;
	}

	return qtrue;
}

/**
 * Menu functions
 */

/**
 * @brief Counts live aliens in all bases.
 * @note This should be called whenever you add or remove
 * @note aliens from alien containment.
 * @return amount of all live aliens stored in containments
 * @sa CL_AircraftReturnedToHomeBase
 * @sa AC_Init_f
 */
int AL_CountAll (void)
{
	int i, j;
	int amount = 0;

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT))
			continue;
		for (j = 0; j < ccs.numAliensTD; j++) {
			if (base->alienscont[j].teamDef)
				amount += base->alienscont[j].amount_alive;
		}
	}
	return amount;
}

/**
 * @brief Kill all aliens in given base.
 * @param[in] base The base in which you want to kill all aliens
 * @sa AC_KillAll_f
 */
void AC_KillAll (base_t *base)
{
	int i;
	qboolean aliens = qfalse;

	assert(base);

	/* Are there aliens here at all? */
	for (i = 0; i < ccs.numAliensTD; i++) {
		if (base->alienscont[i].amount_alive > 0) {
			aliens = qtrue;
			break;
		}
	}

	/* No aliens, return. */
	if (!aliens)
		return;

	AL_RemoveAliens(base, NULL, 0, AL_KILL);
}

#ifdef DEBUG
/**
 * @brief Add a single alien of a given type.
 */
static void AC_AddOne_f (void)
{
	const char *alienName;
	teamDef_t *alienType;
	aliensCont_t *containment;
	qboolean updateAlive;
	int j;

	/* Can be called from everywhere. */
	if (!baseCurrent) {
		return;
	}

	/* arg parsing */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <alientype> [1](dead)\n", Cmd_Argv(0));
		return;
	}

	alienName = Cmd_Argv(1);
	alienType = Com_GetTeamDefinitionByID(alienName);

	if (!alienType) {
		Com_Printf("AC_AddOne_f: Team definition '%s' does not exist.\n", alienName);
		return;
	}

	/* Check that alientType exists */
	containment = baseCurrent->alienscont;
	for (j = 0; j < ccs.numAliensTD; j++) {
		assert(containment[j].teamDef);
		if (containment[j].teamDef == alienType)
			break;
	}
	if (j == ccs.numAliensTD) {
		Com_Printf("AC_AddOne_f: Alien Type '%s' does not exist. Available choices are:\n", alienName);
		for (j = 0; j < ccs.numAliensTD; j++)
			Com_Printf("\t* %s\n", containment[j].teamDef->name);
		return;
	}

	if ((Cmd_Argc() == 3) && (atoi(Cmd_Argv(2)) == 1)) {
		updateAlive = qfalse;
	} else {
		updateAlive = qtrue;
	}

	/* update alien counter*/
	if (B_GetBuildingStatus(baseCurrent, B_ALIEN_CONTAINMENT)) {
		containment = baseCurrent->alienscont;
	} else {
		return;
	}

	/**@todo is AL_RemoveAliens a wrong name for this or why do we remove them on AC_AddOne? */
	/* call the function that actually changes the persistent datastructure */
	if (updateAlive) {
		AL_RemoveAliens(baseCurrent, alienType, 1, AL_ADDALIVE);
	} else {
		AL_RemoveAliens(baseCurrent, alienType, 1, AL_ADDDEAD);
	}
	/* Reinit menu to display proper values. */
	/* AC_UpdateMenu(baseCurrent); */
	/** @todo do we need this update here? is it better to move this debug function to callbacks? */
}
#endif

/**
 * @brief Defines commands and cvars for the alien containment menu(s).
 * @sa MN_InitStartup
 */
void AC_InitStartup (void)
{
	/* add commands */
#ifdef DEBUG
	Cmd_AddCommand("debug_addalientocont", AC_AddOne_f, "Add one alien of a given type");
#endif
	AC_InitCallbacks();
}

/**
 * @brief Savecallback for saving in XML Format
 * @sa AC_LoadXML
 * @sa B_SaveXML
 * @sa SAV_GameSaveXML
 */
qboolean AC_SaveXML (mxml_node_t * parent)
{
	mxml_node_t *aliencont;

	aliencont = mxml_AddNode(parent, "aliencont");
	mxml_AddInt(aliencont, "ccs.breathingMailSent", ccs.breathingMailSent);

	return qtrue;
}

/**
 * @brief Load callback for savin in XML Format
 * @sa AC_LoadXML
 * @sa B_SaveXML
 * @sa SAV_GameLoadXML
 */
qboolean AC_LoadXML (mxml_node_t * parent)
{
	mxml_node_t *aliencont;

	aliencont = mxml_GetNode(parent, "aliencont");
	if (!aliencont) {
		Com_Printf("Error: AlienCont Node wasn't found in savegame\n");
		return qfalse;
	}
	ccs.breathingMailSent = mxml_GetInt(aliencont, "ccs.breathingMailSent", 0);

	return qtrue;
}

/**
 * @brief Returns true if the current base is able to handle captured aliens
 * @sa B_BaseInit_f
 * @note Alien cont. must be accessible during base attack to kill aliens.
 */
qboolean AC_ContainmentAllowed (const base_t* base)
{
	if (B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		return qtrue;
	} else {
		return qfalse;
	}
}
