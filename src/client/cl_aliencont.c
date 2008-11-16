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

#include "client.h"
#include "cl_global.h"

/** Statics for menu. */
static int numAliensOnList = 0;			/**< Number of aliens on AC menu list. */
static const aliensCont_t* aliencontCurrent;		/**< Current selected Alien Containment. */

/** @brief First line in aliencont menu. */
static int alienContFirstEntry;

static void AC_UpdateMenu(const base_t *base);

/**
 * Collecting aliens functions
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
		if (!csi.teamDef[i].alien)
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
	int i, j;
	le_t *le = NULL;
	aliensTmp_t *cargo;

	cargo = aircraft->aliencargo;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse)
			continue;
		if ((le->type == ET_ACTOR || le->type == ET_ACTORHIDDEN || le->type == ET_ACTOR2x2) && le->team == TEAM_ALIEN) {
			if (!le->teamDef) {
				Com_Printf("AL_CollectingAliens: Can't collect alien with no teamDef assigned\n");
				continue;
			}

			if (LE_IsDead(le) || LE_IsStunned(le)) {
				for (j = 0; j < aircraft->alientypes; j++) {
					/* Search alien type and increase amount */
					assert(cargo[j].teamDef);
					assert(le->teamDef);
					if (cargo[j].teamDef == le->teamDef) {
						/* Search stunned first. */
						if (LE_IsStunned(le)) {
							/* alive alien */
							cargo[j].amount_alive++;
							Com_DPrintf(DEBUG_CLIENT, "Counting: alive %s count: %i\n", le->teamDef->name, cargo[j].amount_alive);
						} else {
							/* alien body */
							cargo[j].amount_dead++;
							Com_DPrintf(DEBUG_CLIENT, "Counting: dead %s count: %i\n", le->teamDef->name, cargo[j].amount_dead);
						}
						break;
					}
				}
				if (j == aircraft->alientypes) {
					/* otherwise add new alien type */
					cargo[j].teamDef = le->teamDef;
					/* Search stunned first. */
					if (LE_IsStunned(le)) {
						/* live alien */
						cargo[j].amount_alive++;
						Com_DPrintf(DEBUG_CLIENT, "Adding: alive %s count: %i\n", le->teamDef->name, cargo[j].amount_alive);
					} else {
						/* alien body */
						cargo[j].amount_dead++;
						Com_DPrintf(DEBUG_CLIENT, "Adding: dead %s count: %i\n", le->teamDef->name, cargo[j].amount_dead);
					}
					aircraft->alientypes++;
				}
			}
		}
	}

	/* print all of them */
	for (i = 0; i < aircraft->alientypes; i++) {
		if (cargo[i].amount_dead > 0)
			Com_DPrintf(DEBUG_CLIENT, "Collecting alien bodies... type: %s amount: %i\n", cargo[i].teamDef->name, cargo[i].amount_dead);
		if (cargo[i].amount_alive > 0)
			Com_DPrintf(DEBUG_CLIENT, "Aliens captured alive... type: %s amount: %i\n", cargo[i].teamDef->name, cargo[i].amount_alive);
	}
}

/**
 * @brief Puts alien cargo into Alien Containment.
 * @param[in] aircraft Aircraft transporting cargo to homebase.
 * @sa CL_AircraftReturnedToHomeBase
 * @sa AL_FillInContainment
 */
void AL_AddAliens (aircraft_t *aircraft)
{
	int i, j, k;
	const objDef_t *alBrOd;
	base_t *tobase;
	const aliensTmp_t *cargo;
	qboolean messageAlreadySet = qfalse;
	qboolean alienBreathing = qfalse;
	qboolean limit = qfalse;

	assert(aircraft);
	tobase = aircraft->homebase;
	assert(tobase);

	if (!B_GetBuildingStatus(tobase, B_ALIEN_CONTAINMENT)) {
		MN_AddNewMessage(_("Notice"), _("You cannot process aliens yet. Alien Containment not ready in this base."), qfalse, MSG_STANDARD, NULL);
		return;
	}

	cargo = aircraft->aliencargo;

	alienBreathing = RS_IsResearched_ptr(RS_GetTechByID("rs_alien_breathing"));
	alBrOd = INVSH_GetItemByID("brapparatus");
	if (!alBrOd)
		Sys_Error("AL_AddAliens: Could not get brapparatus item definition");

	for (i = 0; i < aircraft->alientypes; i++) {
		for (j = 0; j < gd.numAliensTD; j++) {
			assert(tobase->alienscont[j].teamDef);
			assert(cargo[i].teamDef);
			if (tobase->alienscont[j].teamDef == cargo[i].teamDef) {
				tobase->alienscont[j].amount_dead += cargo[i].amount_dead;
				/* Add breathing apparatuses to aircraft cargo so that they are processed with other collected items */
				INV_CollectItem(aircraft, alBrOd, cargo[i].amount_dead);
				if (cargo[i].amount_alive <= 0)
					continue;
				if (!alienBreathing && !cargo[i].teamDef->robot) {
					/* We can not store living (i.e. no robots or dead bodies) aliens without rs_alien_breathing tech */
					tobase->alienscont[j].amount_dead += cargo[i].amount_alive;
					/* Add breathing apparatuses as well */
					INV_CollectItem(aircraft, alBrOd, cargo[i].amount_alive);
					/* only once */
					if (!messageAlreadySet) {
						MN_AddNewMessage(_("Notice"), _("You cannot hold live aliens yet. Aliens died."), qfalse, MSG_DEATH, NULL);
						messageAlreadySet = qtrue;
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
								MN_AddNewMessage(_("Notice"), _("You don't have enough space in Alien Containment. Some aliens got killed."), qfalse, MSG_STANDARD, NULL);
								limit = qtrue;
							}
							/* Just kill aliens which don't fit the limit. */
							tobase->alienscont[j].amount_dead++;
							INV_CollectItem(aircraft, alBrOd, 1);
						}
					}
					/* only once */
					if (!messageAlreadySet) {
						MN_AddNewMessage(_("Notice"), _("You've captured new aliens."), qfalse, MSG_STANDARD, NULL);
						messageAlreadySet = qtrue;
					}
				}
				break;
			}
		}
	}

	for (i = 0; i < gd.numAliensTD; i++) {
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
	aircraft->alientypes = 0;
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
				for (j = 0; j < gd.numAliensTD; j++) {
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
		for (j = 0; j < gd.numAliensTD; j++) {
			if (containment[j].amount_alive > 0) {
				containment[j].amount_dead += containment[j].amount_alive;
				AL_ChangeAliveAlienNumber(base, &containment[j], -containment[j].amount_alive);
			}
		}
		break;
	case AL_KILLONE:
		/* We ignore 3rd parameter of AL_RemoveAliens() here. */
		for (j = 0; j < gd.numAliensTD; j++) {
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
		for (j = 0; j < gd.numAliensTD; j++) {
			assert(containment[j].teamDef);
			if (containment[j].teamDef == alienType) {
				AL_ChangeAliveAlienNumber(base, &containment[j], 1);
				break;
			}
		}
		aliencontCurrent = &containment[j];
		break;
	case AL_ADDDEAD:
		for (j = 0; j < gd.numAliensTD; j++) {
			assert(containment[j].teamDef);
			if (containment[j].teamDef == alienType) {
				containment[j].amount_dead++;
				break;
			}
		}
		aliencontCurrent = &containment[j];
		break;
	default:
		break;
	}
}

/**
 * @brief Get index of alien.
 * @param[in] alienType Pointer to alien type.
 * @return Index of alien in alien containment (so less than @c gd.numAliensTD)
 * @note It does NOT return the global team index from @c csi.teamDef array.
 * That would be @c alienType->idx
 * @sa RS_AssignTechLinks
 * @sa AL_GetAlienGlobalIdx
 */
static int AL_GetAlienIdx (const teamDef_t *alienType)
{
	int i, index;

	index = 0;
	for (i = 0; i < csi.numTeamDefs; i++) {
		if (alienType == &csi.teamDef[i])
			return index;
		if (csi.teamDef[i].alien)
			index++;
	}

	Com_Printf("AL_GetAlienIdx: Alien \"%s\" not found!\n", alienType->id);
	return -1;
}

/**
 * @brief Returns global alien index.
 * @param[in] idx Alien index in Alien Containment.
 * @return Global alien index in csi.teamDef array.
 * @sa AL_GetAlienIdx
 */
int AL_GetAlienGlobalIdx (int idx)
{
	int i, counter = 0;

	for (i = 0; i < csi.numTeamDefs; i++) {
		if (csi.teamDef[i].alien) {
			if (counter == idx)
				return i;
			counter++;
		}
	}
	Com_Printf("AL_GetAlienGlobalIdx: Alien with AC index %i not found!\n", idx);
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
	alienTypeIndex = AL_GetAlienIdx(alienType);
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

	for (j = 0; j < gd.numAliensTD; j++) {
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

	/* Just a check -- should never be reached */
	if (!AL_CheckAliveFreeSpace(base, containment, num)) {
		Com_Printf("AL_ChangeAliveAlienNumber: Can't add/remove %i live aliens, (capacity: %i/%i, Alien Containment Status: %i)\n",
			num, base->capacities[CAP_ALIENS].cur, base->capacities[CAP_ALIENS].max,
			B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT));
		return;
	}

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
		for (j = 0; j < gd.numAliensTD; j++) {
			if (base->alienscont[j].teamDef)
				amount += base->alienscont[j].amount_alive;
		}
	}
	return amount;
}

/**
 * @brief Counts killed or captured aliens of given type in all bases.
 * @param[in] alienidx
 * @param[in] alive boolean whether the alien is alive or already dead
 * @return amount of killed aliens of given type
 */
static int AL_CountForMenu (int alienidx, qboolean alive)
{
	int i;
	int amount = 0;

	assert(alienidx >= 0);
	assert(alienidx < MAX_ALIENCONT_CAP);

	for (i = 0; i < MAX_BASES; i++) {
		const base_t const *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		if (!B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT))
			continue;
		if (base->alienscont[alienidx].teamDef) {
			if (alive == qfalse)
				amount += base->alienscont[alienidx].amount_dead;
			else
				amount += base->alienscont[alienidx].amount_alive;
		}
	}
	return amount;
}

/**
 * @brief Open menu for next Alien Containment.
 * @sa AC_PrevAC_f
 */
static void AC_NextAC_f (void)
{
	base_t *base;
	qboolean found = qfalse;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign || !aliencontCurrent)
		return;

	/* Get next base */
	if (baseCurrent >= gd.bases + MAX_BASES - 1)
		base = &gd.bases[0];
	else
		base = baseCurrent + 1;

	/* Loop until we hit the original base. */
	while (base != baseCurrent) {
		if (base->founded && base->hasBuilding[B_ALIEN_CONTAINMENT] &&
			B_CheckBuildingTypeStatus(base, B_ALIEN_CONTAINMENT, B_STATUS_WORKING, NULL)) {
			found = qtrue;
			break;
		}

		/* Get next base */
		if (base >= gd.bases + MAX_BASES - 1)
			base = &gd.bases[0];	/* Wrap around from last to first base. */
		else
			base++;
	}

	if (!found)
		base = baseCurrent;

	if (!base->founded)
		return;
	else
		Cbuf_AddText(va("mn_pop;mn_select_base %i;mn_push aliencont\n", base->idx));
}

/**
 * @brief Open menu for previous Alien Containment.
 * @sa AC_NextAC_f
 */
static void AC_PrevAC_f (void)
{
	qboolean found = qfalse;
	const base_t *base;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign || !aliencontCurrent)
		return;

	/* Get previous base */
	if (baseCurrent <= gd.bases)
		base = &gd.bases[MAX_BASES - 1];
	else
		base = baseCurrent - 1;

	/* Loop until we hit the original base. */
	while (base != baseCurrent) {
		if (base->founded && base->hasBuilding[B_ALIEN_CONTAINMENT] &&
			B_CheckBuildingTypeStatus(base, B_ALIEN_CONTAINMENT, B_STATUS_WORKING, NULL)) {
			found = qtrue;
			break;
		}

		/* Get next base */
		if (base <= gd.bases)
			base = &gd.bases[MAX_BASES - 1];	/* Wrap around from first to last base. */
		else
			base--;
	}

	if (!found)
		base = baseCurrent;

	if (!base->founded)
		return;
	else
		Cbuf_AddText(va("mn_pop;mn_select_base %i;mn_push aliencont\n", base->idx));
}

/**
 * @brief Call UFOpedia for selected alien.
 */
static void AC_OpenUFOpedia_f (void)
{
	const technology_t *tech;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign || !aliencontCurrent)
		return;

	tech = aliencontCurrent->tech;

	/* Should never happen. */
	if (!tech) {
		Com_Printf("AC_OpenUFOpedia_f: No tech pointer set!\n");
		return;
	}

	if (RS_IsResearched_ptr(tech))
		UP_OpenWith(tech->id);
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
	for (i = 0; i < gd.numAliensTD; i++) {
		if (base->alienscont[i].amount_alive > 0) {
			aliens = qtrue;
			break;
		}
	}

	/* No aliens, return. */
	if (!aliens)
		return;

	AL_RemoveAliens(base, NULL, 0, AL_KILL);

	/* Reinit menu to display proper values. */
	AC_UpdateMenu(base);
}

/**
 * @brief Kill all aliens in current base.
 * @sa AC_KillAll
 */
static void AC_KillAll_f (void)
{
	/* Can be called from everywhere. */
	if (!baseCurrent ||!curCampaign)
		return;

	AC_KillAll(baseCurrent);
}

/**
 * @brief Kill single alien of a given type.
 */
static void AC_KillOne_f (void)
{
	int num, i, step;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* which item from the list? */
	num = atoi(Cmd_Argv(1));
	if (num >= numAliensOnList || num < 0) {
		Com_DPrintf(DEBUG_CLIENT, "AC_KillOne_f: max exceeded %i/%i\n", num, numAliensOnList);
		return;
	}

	if (B_GetBuildingStatus(baseCurrent, B_ALIEN_CONTAINMENT)) {
		aliensCont_t *containment = baseCurrent->alienscont;
		for (i = 0, step = 0; i < gd.numAliensTD; i++) {
			if (!containment[i].amount_alive && !containment[i].amount_dead)
				continue;
			if (step == num) {
				num = i;
				break;
			}
			step++;
		}
		AL_RemoveAliens(baseCurrent, containment[num].teamDef, 1, AL_KILLONE);
		/* Reinit menu to display proper values. */
		AC_UpdateMenu(baseCurrent);
	}
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
	if (!baseCurrent || !curCampaign) {
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
	for (j = 0; j < gd.numAliensTD; j++) {
		assert(containment[j].teamDef);
		if (containment[j].teamDef == alienType)
			break;
	}
	if (j == gd.numAliensTD) {
		Com_Printf("AC_AddOne_f: Alien Type '%s' does not exist. Available choices are:\n", alienName);
		for (j = 0; j < gd.numAliensTD; j++)
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

	/* call the function that actually changes the persisten datastructure */
	if (updateAlive) {
		AL_RemoveAliens(baseCurrent, alienType, 1, AL_ADDALIVE);
	} else {
		AL_RemoveAliens(baseCurrent, alienType, 1, AL_ADDDEAD);
	}
	/* Reinit menu to display proper values. */
	AC_UpdateMenu(baseCurrent);
}
#endif

/**
 * @brief Open research menu.
 */
static void AC_ResearchAlien_f (void)
{
	const technology_t *tech;

	/* Can be called from everywhere. */
	if (!baseCurrent || !curCampaign || !aliencontCurrent)
		return;

	tech = aliencontCurrent->tech;
	if (!tech)
		Sys_Error("aliencontCurrent without tech pointer");

	if (!RS_IsResearched_ptr(tech))
		MN_PushMenu("research");
}

static void AC_AlienClick (const base_t *base, int num)
{
	if (num >= numAliensOnList || num < 0) {
		Com_DPrintf(DEBUG_CLIENT, "AC_AlienClick: max exceeded %i/%i\n", num, numAliensOnList);
		return;
	}

	if (B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		const aliensCont_t *containment = base->alienscont;
		int i, step;

		for (i = 0, step = 0; i < gd.numAliensTD; i++) {
			if (!containment[i].amount_alive && !containment[i].amount_dead)
				continue;
			if (step == num) {
				num = i;
				break;
			}
			step++;
		}

		aliencontCurrent = &base->alienscont[num];
		assert(aliencontCurrent->tech);
		Cvar_Set("mn_al_alienimage", aliencontCurrent->tech->image);
		assert(aliencontCurrent->teamDef);
		Cvar_Set("mn_al_alientype", _(aliencontCurrent->teamDef->name));
		Cvar_SetValue("mn_al_alive", AL_CountForMenu(aliencontCurrent->teamDef->idx, qtrue));
		Cvar_SetValue("mn_al_dead", AL_CountForMenu(aliencontCurrent->teamDef->idx, qfalse));
	}
}

/**
 * @brief Click function for aliencont menu list.
 * @sa AC_InitStartup
 */
static void AC_AlienClick_f (void)
{
	int num;

	if (Cmd_Argc() < 2 || !baseCurrent) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	/* which item from the list? */
	num = atoi(Cmd_Argv(1));

	Com_DPrintf(DEBUG_CLIENT, "AC_AlienClick_f: listnumber %i\n", num);
	AC_AlienClick(baseCurrent, num);
}


/**
 * @brief Maximal entries in aliencont menu.
 * @sa MAX_TEAMDEFS
 */
#define AC_MENU_MAX_ENTRIES 12

/** @brief Number of entries in a line of the aliencont menu. */
#define AC_MENU_LINE_ENTRIES 2

/**
 * @brief Updates the alienscont menu.
 */
static void AC_UpdateMenu (const base_t *base)
{
	int i, j;

	Cvar_Set("mn_al_alientype", "");
	Cvar_Set("mn_al_alienimage", "");
	Cvar_SetValue("mn_al_dead", 0);
	Cvar_SetValue("mn_al_alive", 0);
	Cvar_SetValue("mn_al_capacity", base->capacities[CAP_ALIENS].cur);
	Cvar_SetValue("mn_al_capacity_max", base->capacities[CAP_ALIENS].max);

	/* Reset list. */
	MN_ExecuteConfunc("aliencont_clear");
	if (B_GetBuildingStatus(base, B_ALIEN_CONTAINMENT)) {
		const aliensCont_t *containment = base->alienscont;
		for (i = 0, j = 0; i < gd.numAliensTD; i++) {
			if ((j >= alienContFirstEntry) && (j < AC_MENU_MAX_ENTRIES)) {
				if (containment[i].teamDef) {
					const technology_t *tech = containment[i].tech;
					if (!tech) {
						Com_Printf("AC_UpdateMenu: Tech entry for containment %i not set!\n", i);
						/* to let the click function still work */
						continue;
					}
					if (!aliencontCurrent) {
						aliencontCurrent = &containment[i];
					}
					if (containment[i].amount_alive > 0 || containment[i].amount_dead > 0) {
						/* Generate a list entry. */
						if (RS_IsResearched_ptr(tech)) {
							Cvar_Set(va("mn_ac_statusstr%i", j), _("Already researched"));
						} else {
							Cvar_Set(va("mn_ac_statusstr%i", j), _("Needs autopsy!"));
							if (!containment[i].amount_dead) {
								MN_ExecuteConfunc(va("aliencontkill%i", j));
							} else {
								MN_ExecuteConfunc(va("aliencontneedautopsy%i", j));
							}
						}
						Cvar_SetValue(va("mn_ac_progress%i", j), (1 - tech->time / tech->overalltime) * 100);
						/* Display name in the correct list-entry. */
						Cvar_Set(va("mn_ac_name%i", j), _(containment[i].teamDef->name));
						/* Display amount of dead aliens in the correct list-entry. */
						Cvar_SetValue(va("mn_ac_dead%i", j), containment[i].amount_dead);
						/* Display number of live aliens in the correct list-entry. */
						Cvar_SetValue(va("mn_ac_alive%i", j), containment[i].amount_alive);
						j++;
					}
				}
			}
		}

		numAliensOnList = j;

		for (; j < AC_MENU_MAX_ENTRIES; j++) {
			Cvar_Set(va("mn_ac_statusstr%i", j), _("Free slot"));
			Cvar_Set(va("mn_ac_name%i", j), _("None"));
			Cvar_Set(va("mn_ac_dead%i", j), "");
			Cvar_Set(va("mn_ac_alive%i", j), "");
			Cvar_SetValue(va("mn_ac_progress%i", j), 0);
		}
	}

	/** @todo Select the containment we (maybe) just clicked again */
	AC_AlienClick(base, 0);
}


/**
 * @brief Alien containment menu init function.
 * @note Command to call this: aliencont_init
 * @note Should be called whenever the alien containment menu gets active.
 */
static void AC_Init_f (void)
{
	/* Reset the aliencont list. */
	numAliensOnList = 0;

	if (!baseCurrent) {
		Com_Printf("No base selected\n");
		return;
	}

	AC_UpdateMenu(baseCurrent);
}

/**
 * @brief Click function for scrolling up the aliencont list.
 */
static void AC_ListUp_f (void)
{
	if (!baseCurrent)
		return;

	if (alienContFirstEntry >= AC_MENU_LINE_ENTRIES)
		alienContFirstEntry -= AC_MENU_LINE_ENTRIES;

	AC_UpdateMenu(baseCurrent);
}

/**
 * @brief Click function for scrolling down the aliencont list.
 */
static void AC_ListDown_f (void)
{
	if (!baseCurrent)
		return;

	if (alienContFirstEntry + AC_MENU_MAX_ENTRIES < numAliensOnList)
		alienContFirstEntry += AC_MENU_LINE_ENTRIES;

	AC_UpdateMenu(baseCurrent);
}

/**
 * @brief Defines commands and cvars for the alien containment menu(s).
 * @sa MN_InitStartup
 */
void AC_InitStartup (void)
{
	/* add commands */
	Cmd_AddCommand("aliencont_init", AC_Init_f, "Init function for alien containment menu");
	Cmd_AddCommand("aliencont_click", AC_AlienClick_f, "Click function for aliencont list");
	Cmd_AddCommand("aliencont_pedia", AC_OpenUFOpedia_f, "Opens UFOpedia entry for selected alien");
	Cmd_AddCommand("aliencont_killall", AC_KillAll_f, "Kills all aliens in current base");
	Cmd_AddCommand("aliencont_research", AC_ResearchAlien_f, "Opens research menu");
	Cmd_AddCommand("aliencont_nextbase", AC_NextAC_f, "Opens Alien Containment menu in next base");
	Cmd_AddCommand("aliencont_prevbase", AC_PrevAC_f, "Opens Alien Containment menu in previous base");
	Cmd_AddCommand("aliencont_killone", AC_KillOne_f, "Kills one alien of a given type");
	Cmd_AddCommand("aliencont_list_up", AC_ListUp_f, "Scroll up function for aliencont list");
	Cmd_AddCommand("aliencont_list_down", AC_ListDown_f, "Scroll down function for aliencont list");
#ifdef DEBUG
	Cmd_AddCommand("debug_addalientocont", AC_AddOne_f, "Add one alien of a given type");
#endif
	aliencontCurrent = NULL;
}

/**
 * @brief Save callback for savegames
 * @sa AC_Load
 * @sa B_Save
 * @sa SAV_GameSave
 */
qboolean AC_Save (sizebuf_t* sb, void* data)
{
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa AC_Save
 * @sa B_Load
 * @sa SAV_GameLoad
 */
qboolean AC_Load (sizebuf_t* sb, void* data)
{
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
