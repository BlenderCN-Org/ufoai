/**
 * @file cp_mapfightequip.c
 * @brief contains everything related to equiping slots of aircraft or base
 * @note Base defence functions prefix: BDEF_
 * @note Aircraft items slots functions prefix: AIM_
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
#include "../menu/m_main.h"
#include "../menu/node/m_node_text.h"
#include "cp_campaign.h"
#include "cp_mapfightequip.h"
#include "cp_fightequip_callbacks.h"
#include "cp_ufo.h"
#include "cp_map.h"

/**
 * @brief Returns a list of craftitem technologies for the given type.
 * @note This list is terminated by a NULL pointer.
 * @param[in] type Type of the craft-items to return.
 */
technology_t **AII_GetCraftitemTechsByType (int type)
{
	static technology_t *techList[MAX_TECHNOLOGIES];
	int i, j = 0;

	for (i = 0; i < csi.numODs; i++) {
		objDef_t *aircraftitem = &csi.ods[i];
		if (aircraftitem->craftitem.type == type) {
			assert(j < MAX_TECHNOLOGIES);
			techList[j] = aircraftitem->tech;
			j++;
		}
		/* j+1 because last item has to be NULL */
		if (j + 1 >= MAX_TECHNOLOGIES) {
			Com_Printf("AII_GetCraftitemTechsByType: MAX_TECHNOLOGIES limit hit.\n");
			break;
		}
	}
	/* terminate the list */
	techList[j] = NULL;
	return techList;
}

/**
 * @brief Returns craftitem weight based on size.
 * @param[in] od Pointer to objDef_t object being craftitem.
 * @return itemWeight_t
 * @sa AII_WeightToName
 */
itemWeight_t AII_GetItemWeightBySize (const objDef_t *od)
{
	assert(od);
	assert(od->craftitem.type >= 0);

	if (od->size < 50)
		return ITEM_LIGHT;
	else if (od->size < 100)
		return ITEM_MEDIUM;
	else
		return ITEM_HEAVY;
}

/**
 * @brief Check if an aircraft item should or should not be displayed in airequip menu
 * @param[in] Pointer to an aircraft slot (can be base/installation too)
 * @param[in] tech Pointer to the technology to test
 * @return qtrue if the craft item should be displayed, qfalse else
 */
qboolean AIM_SelectableCraftItem (const aircraftSlot_t *slot, const technology_t *tech)
{
	objDef_t *item;

	if (!slot)
		return qfalse;

	if (!RS_IsResearched_ptr(tech))
		return qfalse;

	item = AII_GetAircraftItemByID(tech->provides);
	if (!item)
		return qfalse;

	if (item->craftitem.type >= AC_ITEM_AMMO) {
		const objDef_t *weapon = slot->item;
		int k;
		if (slot->nextItem != NULL)
			weapon = slot->nextItem;

		if (weapon == NULL)
			return qfalse;

		/* Is the ammo is usable with the slot */
		for (k = 0; k < weapon->numAmmos; k++) {
			const objDef_t *usable = weapon->ammos[k];
			if (usable && item->idx == usable->idx)
				break;
		}
		if (k >= weapon->numAmmos)
			return qfalse;
	}

	/** @todo maybe this isn't working, aircraft slot type can't be an AMMO */
	if (slot->type >= AC_ITEM_AMMO) {
		/** @todo This only works for ammo that is usable in exactly one weapon
		 * check the weap_idx array and not only the first value */
		if (!slot->nextItem && item->weapons[0] != slot->item)
			return qfalse;

		/* are we trying to change ammos for nextItem? */
		if (slot->nextItem && item->weapons[0] != slot->nextItem)
			return qfalse;
	}

	/* you can install an item only if its weight is small enough for the slot */
	if (AII_GetItemWeightBySize(item) > slot->size)
		return qfalse;

	/* you can't install an item that you don't possess
	 * unlimited ammo don't need to be possessed
	 * installations always have weapon and ammo */
	if (slot->aircraft) {
		if (slot->aircraft->homebase->storage.num[item->idx] <= 0 && !item->notOnMarket  && !item->craftitem.unlimitedAmmo)
			return qfalse;
	} else if (slot->base) {
		if (slot->base->storage.num[item->idx] <= 0 && !item->notOnMarket && !item->craftitem.unlimitedAmmo)
			return qfalse;
	}

	/* you can't install an item that does not have an installation time (alien item)
	 * except for ammo which does not have installation time */
	if (item->craftitem.installationTime == -1 && slot->type < AC_ITEM_AMMO)
		return qfalse;

	return qtrue;
}

/**
 * @brief Checks to see if the pilot is in any aircraft at this base.
 * @param[in] base Which base has the aircraft to search for the employee in.
 * @param[in] type Which employee to search for.
 * @return qtrue or qfalse depending on if the employee was found on the base aircraft.
 */
qboolean AIM_PilotAssignedAircraft (const base_t* base, const employee_t* pilot)
{
	int i;
	qboolean found = qfalse;

	for (i = 0; i < base->numAircraftInBase; i++) {
		const aircraft_t *aircraft = &base->aircraft[i];
		if (aircraft->pilot == pilot) {
			found = qtrue;
			break;
		}
	}

	return found;
}

/**
 * @brief Adds a defence system to base.
 * @param[in] basedefType Base defence type (see basedefenceType_t)
 * @param[in] base Pointer to the base in which the battery will be added
 * @sa BDEF_RemoveBattery
 */
void BDEF_AddBattery (basedefenceType_t basedefType, base_t* base)
{
	switch (basedefType) {
	case BASEDEF_MISSILE:
		if (base->numBatteries >= MAX_BASE_SLOT) {
			Com_Printf("BDEF_AddBattery: too many missile batteries in base\n");
			return;
		}
		base->batteries[base->numBatteries].autofire = qtrue;
		base->numBatteries++;
		break;
	case BASEDEF_LASER:
		if (base->numLasers >= MAX_BASE_SLOT) {
			Com_Printf("BDEF_AddBattery: too many laser batteries in base\n");
			return;
		}
		/* slots has unlimited ammo */
		base->lasers[base->numLasers].slot.ammoLeft = AMMO_STATUS_UNLIMITED;
		base->lasers[base->numLasers].autofire = qtrue;
		base->numLasers++;
		break;
	default:
		Com_Printf("BDEF_AddBattery: unknown type of base defence system.\n");
	}
}

/**
 * @brief Reload the battery of every bases
 * @todo we should define the number of ammo to reload and the period of reloading in the .ufo file
 */
void BDEF_ReloadBattery (void)
{
	int i, j;

	/* Reload all ammos of aircraft */
	for (i = 0; i < MAX_BASES; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		for (j = 0; j < base->numBatteries; j++) {
			if (base->batteries[j].slot.ammoLeft >= 0 && base->batteries[j].slot.ammoLeft < 20)
				base->batteries[j].slot.ammoLeft++;
		}
	}
}

/**
 * @brief Remove a base defence sytem from base.
 * @param[in] basedefType (see basedefenceType_t)
 * @param[in] idx idx of the battery to destroy (-1 if this is random)
 * @sa BDEF_AddBattery
 */
void BDEF_RemoveBattery (base_t *base, basedefenceType_t basedefType, int idx)
{
	assert(base);

	/* Select the type of base defence system to destroy */
	switch (basedefType) {
	case BASEDEF_MISSILE: /* this is a missile battery */
		/* we must have at least one missile battery to remove it */
		assert(base->numBatteries > 0);
		if (idx < 0)
			idx = rand() % base->numBatteries;
		REMOVE_ELEM(base->batteries, idx, base->numBatteries);
		/* just for security */
		AII_InitialiseSlot(&base->batteries[base->numBatteries].slot, NULL, base, NULL, AC_ITEM_BASE_MISSILE);
		break;
	case BASEDEF_LASER: /* this is a laser battery */
		/* we must have at least one laser battery to remove it */
		assert(base->numLasers > 0);
		if (idx < 0)
			idx = rand() % base->numLasers;
		REMOVE_ELEM(base->lasers, idx, base->numLasers);
		/* just for security */
		AII_InitialiseSlot(&base->lasers[base->numLasers].slot, NULL, base, NULL, AC_ITEM_BASE_LASER);
		break;
	default:
		Com_Printf("BDEF_RemoveBattery_f: unknown type of base defence system.\n");
	}
}

/**
 * @brief Initialise all values of base slot defence.
 * @param[in] base Pointer to the base which needs initalisation of its slots.
 */
void BDEF_InitialiseBaseSlots (base_t *base)
{
	int i;

	for (i = 0; i < MAX_BASE_SLOT; i++) {
		AII_InitialiseSlot(&base->batteries[i].slot, NULL, base, NULL, AC_ITEM_BASE_MISSILE);
		AII_InitialiseSlot(&base->lasers[i].slot, NULL, base, NULL, AC_ITEM_BASE_LASER);
		base->batteries[i].target = NULL;
		base->lasers[i].target = NULL;
	}
}

/**
 * @brief Initialise all values of installation slot defence.
 * @param[in] Pointer to the installation which needs initalisation of its slots.
 */
void BDEF_InitialiseInstallationSlots (installation_t *installation)
{
	int i;

	for (i = 0; i < installation->installationTemplate->maxBatteries; i++) {
		AII_InitialiseSlot(&installation->batteries[i].slot, NULL, NULL, installation, AC_ITEM_BASE_MISSILE);
		installation->batteries[i].target = NULL;
	}
}


/**
 * @brief Update the installation delay of one slot.
 * @param[in] base Pointer to the base to update the storage and capacity for
 * @param[in] aircraft Pointer to the aircraft (NULL if a base is updated)
 * @param[in] slot Pointer to the slot to update
 * @sa AII_AddItemToSlot
 */
static void AII_UpdateOneInstallationDelay (base_t* base, installation_t* installation, aircraft_t *aircraft, aircraftSlot_t *slot)
{
	assert(base || installation);

	/* if the item is already installed, nothing to do */
	if (slot->installationTime == 0)
		return;
	else if (slot->installationTime > 0) {
		/* the item is being installed */
		slot->installationTime--;
		/* check if installation is over */
		if (slot->installationTime <= 0) {
			/* Update stats values */
			if (aircraft) {
				AII_UpdateAircraftStats(aircraft);
				Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
						_("%s was successfully installed into aircraft %s at %s."),
						_(slot->item->name), _(aircraft->name), aircraft->homebase->name);
			} else if (installation) {
				Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("%s was successfully installed at installation %s."),
						_(slot->item->name), installation->name);
			} else {
				Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("%s was successfully installed at %s."),
						_(slot->item->name), base->name);
			}
			MSO_CheckAddNewMessage(NT_INSTALLATION_INSTALLED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
		}
	} else if (slot->installationTime < 0) {
		const objDef_t *olditem;

		/* the item is being removed */
		slot->installationTime++;
		if (slot->installationTime >= 0) {
#ifdef DEBUG
			if (aircraft && aircraft->homebase != base)
				Sys_Error("AII_UpdateOneInstallationDelay: aircraft->homebase and base pointers are out of sync\n");
#endif
			olditem = slot->item;
			AII_RemoveItemFromSlot(base, slot, qfalse);
			if (aircraft) {
				AII_UpdateAircraftStats(aircraft);
				/* Only stop time and post a notice, if no new item to install is assigned */
				if (!slot->item) {
					Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
							_("%s was successfully removed from aircraft %s at %s."),
							_(olditem->name), _(aircraft->name), base->name);
					MSO_CheckAddNewMessage(NT_INSTALLATION_REMOVED, _("Notice"), cp_messageBuffer, qfalse,
							MSG_STANDARD, NULL);
				} else {
					Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
							_ ("%s was successfully removed, starting installation of %s into aircraft %s at %s"),
							_(olditem->name), _(slot->item->name), _(aircraft->name), base->name);
					MSO_CheckAddNewMessage(NT_INSTALLATION_REPLACE, _("Notice"), cp_messageBuffer, qfalse,
							MSG_STANDARD, NULL);
				}
			} else if (!slot->item) {
				if (installation) {
					Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer),
							_("%s was successfully removed from installation %s."),
							_(olditem->name), installation->name);
				} else {
					Com_sprintf(cp_messageBuffer, lengthof(cp_messageBuffer), _("%s was successfully removed from %s."),
							_(olditem->name), base->name);
				}
				MSO_CheckAddNewMessage(NT_INSTALLATION_REMOVED, _("Notice"), cp_messageBuffer, qfalse, MSG_STANDARD, NULL);
			}
		}
	}
}

/**
 * @brief Update the installation delay of all slots of a given aircraft.
 * @note hourly called
 * @sa CL_CampaignRun
 * @sa AII_UpdateOneInstallationDelay
 */
void AII_UpdateInstallationDelay (void)
{
	int i, j, k;

	for (j = 0; j < MAX_INSTALLATIONS; j++) {
		installation_t *installation = INS_GetFoundedInstallationByIDX(j);
		if (!installation)
			continue;

		/* Update base */
		for (k = 0; k < installation->installationTemplate->maxBatteries; k++)
			AII_UpdateOneInstallationDelay(NULL, installation, NULL, &installation->batteries[k].slot);
	}

	for (j = 0; j < MAX_BASES; j++) {
		aircraft_t *aircraft;
		base_t *base = B_GetFoundedBaseByIDX(j);
		if (!base)
			continue;

		/* Update base */
		for (k = 0; k < base->numBatteries; k++)
			AII_UpdateOneInstallationDelay(base, NULL, NULL, &base->batteries[k].slot);
		for (k = 0; k < base->numLasers; k++)
			AII_UpdateOneInstallationDelay(base, NULL, NULL, &base->lasers[k].slot);

		/* Update each aircraft */
		for (i = 0, aircraft = (aircraft_t *) base->aircraft; i < base->numAircraftInBase; i++, aircraft++)
			if (aircraft->homebase) {
				assert(aircraft->homebase == base);
				if (AIR_IsAircraftInBase(aircraft)) {
					/* Update electronics delay */
					for (k = 0; k < aircraft->maxElectronics; k++)
						AII_UpdateOneInstallationDelay(base, NULL, aircraft, aircraft->electronics + k);

					/* Update weapons delay */
					for (k = 0; k < aircraft->maxWeapons; k++)
						AII_UpdateOneInstallationDelay(base, NULL, aircraft, aircraft->weapons + k);

					/* Update shield delay */
					AII_UpdateOneInstallationDelay(base, NULL, aircraft, &aircraft->shield);
				}
			}
	}
}

/**
 * @brief Auto add ammo corresponding to weapon, if there is enough in storage.
 * @param[in] slot Pointer to the slot where you want to add ammo
 * @sa AIM_AircraftEquipAddItem_f
 * @sa AII_RemoveItemFromSlot
 */
void AIM_AutoAddAmmo (aircraftSlot_t *slot)
{
	int k;
	const objDef_t *item;

	assert(slot);

	/* Get the weapon (either current weapon or weapon to install after this one is removed) */
	item = (slot->nextItem) ? slot->nextItem : slot->item;

	if (!item)
		return;

	if (item->craftitem.type > AC_ITEM_WEAPON)
		return;

	/* don't try to add ammo to a slot that already has ammo */
	if ((slot->nextItem) ? slot->nextAmmo : slot->ammo)
		return;

	/* Try every ammo usable with this weapon until we find one we have in storage */
	for (k = 0; k < item->numAmmos; k++) {
		const objDef_t *ammo = item->ammos[k];
		if (ammo) {
			const technology_t *ammo_tech = ammo->tech;
			if (ammo_tech && AIM_SelectableCraftItem(slot, ammo_tech)) {
				AII_AddAmmoToSlot((ammo->notOnMarket || ammo->craftitem.unlimitedAmmo) ? NULL : (slot->aircraft) ? slot->aircraft->homebase : slot->base, ammo_tech, slot);
				break;
			}
		}
	}
}

/**
 * @brief Move the item in the slot (or optionally its ammo only) to the base storage.
 * @note if there is another item to install after removal, begin this installation.
 * @param[in] base The base to add the item to (may be NULL if item shouldn't be removed from any base).
 * @param[in] slot The slot to remove the item from.
 * @param[in] ammo qtrue if we want to remove only ammo. qfalse if the whole item should be removed.
 * @sa AII_AddItemToSlot
 * @sa AII_AddAmmoToSlot
 * @sa AII_RemoveNextItemFromSlot
 */
void AII_RemoveItemFromSlot (base_t* base, aircraftSlot_t *slot, qboolean ammo)
{
	assert(slot);

	if (ammo) {
		/* only remove the ammo */
		if (slot->ammo) {
			if (base && !slot->ammo->craftitem.unlimitedAmmo)
				B_UpdateStorageAndCapacity(base, slot->ammo, 1, qfalse, qfalse);
			slot->ammo = NULL;
		}
	} else if (slot->item) {
		/* remove ammo */
		AII_RemoveItemFromSlot(base, slot, qtrue);
		if (base)
			B_UpdateStorageAndCapacity(base, slot->item, 1, qfalse, qfalse);
		/* the removal is over */
		if (slot->nextItem) {
			/* there is anoter item to install after this one */
			slot->item = slot->nextItem;
			/* we already removed nextItem from storage when it has been added to slot: don't use B_UpdateStorageAndCapacity */
			slot->ammo = slot->nextAmmo;
			if (slot->ammo) {
				if (!slot->ammo->craftitem.unlimitedAmmo)
					slot->ammoLeft = slot->ammo->ammo;
				else
					slot->ammoLeft = AMMO_STATUS_UNLIMITED;
			}
			slot->installationTime = slot->item->craftitem.installationTime;
			slot->nextItem = NULL;
			slot->nextAmmo = NULL;
		} else {
			slot->item = NULL;
			slot->installationTime = 0;
		}
	}
}

/**
 * @brief Cancel replacing item, move nextItem (or optionally its ammo only) back to the base storage.
 * @param[in] base The base to add the item to (may be NULL if item shouldn't be removed from any base).
 * @param[in] slot The slot to remove the item from.
 * @param[in] ammo qtrue if we want to remove only ammo. qfalse if the whole item should be removed.
 * @sa AII_AddItemToSlot
 * @sa AII_AddAmmoToSlot
 * @sa AII_RemoveItemFromSlot
 */
void AII_RemoveNextItemFromSlot (base_t* base, aircraftSlot_t *slot, qboolean ammo)
{
	assert(slot);

	if (ammo) {
		/* only remove the ammo */
		if (slot->nextAmmo) {
			if (base && !slot->nextAmmo->craftitem.unlimitedAmmo)
				B_UpdateStorageAndCapacity(base, slot->nextAmmo, 1, qfalse, qfalse);
			slot->nextAmmo = NULL;
		}
	} else if (slot->nextItem) {
		/* Remove nextItem */
		if (base)
			B_UpdateStorageAndCapacity(base, slot->nextItem, 1, qfalse, qfalse);
		slot->nextItem = NULL;
		/* also remove ammo if any */
		if (slot->nextAmmo)
			AII_RemoveNextItemFromSlot(base, slot, qtrue);
	}
}

/**
 * @brief Add an ammo to an aircraft weapon slot
 * @note No check for the _type_ of item is done here, so it must be done before.
 * @param[in] base Pointer to the base which provides items (NULL if items shouldn't be removed of storage)
 * @param[in] tech Pointer to the tech to add to slot
 * @param[in] slot Pointer to the slot where you want to add ammos
 * @sa AII_AddItemToSlot
 */
qboolean AII_AddAmmoToSlot (base_t* base, const technology_t *tech, aircraftSlot_t *slot)
{
	const objDef_t *ammo;
	const objDef_t *item;
	int k;

	if (slot == NULL || slot->item == NULL)
		return qfalse;

	assert(tech);

	ammo = AII_GetAircraftItemByID(tech->provides);
	if (!ammo) {
		Com_Printf("AII_AddAmmoToSlot: Could not add item (%s) to slot\n", tech->provides);
		return qfalse;
	}

	item = (slot->nextItem) ? slot->nextItem : slot->item;

	/* Is the ammo is usable with the slot */
	for (k = 0; k < item->numAmmos; k++) {
		const objDef_t *usable = item->ammos[k];
		if (usable && ammo->idx == usable->idx)
			break;
	}
	if (k >= item->numAmmos)
		return qfalse;

	/* the base pointer can be null here - e.g. in case you are equipping a UFO
	 * and base ammo defence are not stored in storage */
	if (base && ammo->craftitem.type <= AC_ITEM_AMMO) {
		if (base->storage.num[ammo->idx] <= 0) {
			Com_Printf("AII_AddAmmoToSlot: No more ammo of this type to equip (%s)\n", ammo->id);
			return qfalse;
		}
	}

	/* remove any applied ammo in the current slot */
	if (slot->nextItem) {
		if (slot->nextAmmo)
		AII_RemoveNextItemFromSlot(base, slot, qtrue);
		slot->nextAmmo = ammo;
	} else {
		/* you shouldn't be able to have nextAmmo set if you don't have nextItem set */
		assert(!slot->nextAmmo);
		AII_RemoveItemFromSlot(base, slot, qtrue);
		slot->ammo = ammo;
	}

	/* the base pointer can be null here - e.g. in case you are equipping a UFO */
	if (base && !ammo->craftitem.unlimitedAmmo)
		B_UpdateStorageAndCapacity(base, ammo, -1, qfalse, qfalse);

	/* proceed only if we are changing ammo of current weapon */
	if (slot->nextItem)
		return qtrue;
	/* some weapons have unlimited ammo */
	if (ammo->craftitem.unlimitedAmmo) {
		slot->ammoLeft = AMMO_STATUS_UNLIMITED;
	} else if (slot->aircraft && base)
		AII_ReloadWeapon(slot->aircraft);

	return qtrue;
}

/**
 * @brief Add an item to an aircraft slot
 * @param[in] base Pointer to the base where item will be removed (NULL for ufos, unlimited ammos or while loading game)
 * @param[in] tech Pointer to the tech that will be added in this slot.
 * @param[in] slot Pointer to the aircraft, base, or installation slot.
 * @param[in] nextItem False if we are changing current item in slot, true if this is the item to install
 * after current removal is over.
 * @note No check for the _type_ of item is done here.
 * @sa AII_UpdateOneInstallationDelay
 * @sa AII_AddAmmoToSlot
 */
qboolean AII_AddItemToSlot (base_t *base, const technology_t *tech, aircraftSlot_t *slot, qboolean nextItem)
{
	const objDef_t *item;

	assert(slot);
	assert(tech);

	item = AII_GetAircraftItemByID(tech->provides);
	if (!item) {
		Com_Printf("AII_AddItemToSlot: Could not add item (%s) to slot\n", tech->provides);
		return qfalse;
	}

	/* Sanity check : the type of the item should be the same than the slot type */
	if (slot->type != item->craftitem.type) {
		Com_Printf("AII_AddItemToSlot: Type of the item to install (%s -- %i) doesn't match type of the slot (%i)\n", item->id, item->craftitem.type, slot->type);
		return qfalse;
	}

#ifdef DEBUG
	/* Sanity check : the type of the item cannot be an ammo */
	/* note that this should never be reached because a slot type should never be an ammo
	 * , so the test just before should be wrong */
	if (item->craftitem.type >= AC_ITEM_AMMO) {
		Com_Printf("AII_AddItemToSlot: Type of the item to install (%s) should be a weapon, a shield, or electronics (no ammo)\n", item->id);
		return qfalse;
	}
#endif

	/* the base pointer can be null here - e.g. in case you are equipping a UFO */
	if (base) {
		if (base->storage.num[item->idx] <= 0) {
			Com_Printf("AII_AddItemToSlot: No more item of this type to equip (%s)\n", item->id);
			return qfalse;
		}
	}

	if (slot->size >= AII_GetItemWeightBySize(item)) {
		if (nextItem)
			slot->nextItem = item;
		else {
			slot->item = item;
			slot->installationTime = item->craftitem.installationTime;
		}
		/* the base pointer can be null here - e.g. in case you are equipping a UFO
		 * Remove item even for nextItem, this way we are sure we won't use the same item
		 * for another aircraft. */
		if (base)
			B_UpdateStorageAndCapacity(base, item, -1, qfalse, qfalse);
	} else {
		Com_Printf("AII_AddItemToSlot: Could not add item '%s' to slot %i (slot-size: %i - item-weight: %i)\n",
			item->id, slot->idx, slot->size, item->size);
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Auto Add weapon and ammo to an aircraft.
 * @param[in] aircraft Pointer to the aircraft
 * @note This is used to auto equip interceptor of first base.
 * @sa B_SetUpBase
 */
void AIM_AutoEquipAircraft (aircraft_t *aircraft)
{
	int i;
	objDef_t *item;
	const technology_t *tech = RS_GetTechByID("rs_craft_weapon_sparrowhawk");

	if (!tech)
		Com_Error(ERR_DROP, "Could not get tech rs_craft_weapon_sparrowhawk");

	assert(aircraft);
	assert(aircraft->homebase);

	item = AII_GetAircraftItemByID(tech->provides);
	if (!item)
		return;

	for (i = 0; i < aircraft->maxWeapons; i++) {
		aircraftSlot_t *slot = &aircraft->weapons[i];
		if (slot->size < AII_GetItemWeightBySize(item))
			continue;
		if (aircraft->homebase->storage.num[item->idx] <= 0)
			continue;
		AII_AddItemToSlot(aircraft->homebase, tech, slot, qfalse);
		AIM_AutoAddAmmo(slot);
		slot->installationTime = 0;
	}

	/* Fill slots too small for sparrowhawk with shiva */
	tech = RS_GetTechByID("rs_craft_weapon_shiva");

	if (!tech)
		Com_Error(ERR_DROP, "Could not get tech rs_craft_weapon_shiva");

	item = AII_GetAircraftItemByID(tech->provides);

	if (!item)
		return;

	for (i = 0; i < aircraft->maxWeapons; i++) {
		aircraftSlot_t *slot = &aircraft->weapons[i];
		if (slot->size < AII_GetItemWeightBySize(item))
			continue;
		if (aircraft->homebase->storage.num[item->idx] <= 0)
			continue;
		if (slot->item)
			continue;
		AII_AddItemToSlot(aircraft->homebase, tech, slot, qfalse);
		AIM_AutoAddAmmo(slot);
		slot->installationTime = 0;
	}

	AII_UpdateAircraftStats(aircraft);
}

/**
 * @brief Initialise values of one slot of an aircraft or basedefence common to all types of items.
 * @param[in] slot	Pointer to the slot to initialize.
 * @param[in] aircraftTemplate	Pointer to aircraft template.
 * @param[in] base	Pointer to base.
 * @param[in] type
 */
void AII_InitialiseSlot (aircraftSlot_t *slot, aircraft_t *aircraft, base_t *base, installation_t *installation, aircraftItemType_t type)
{
	assert((!base && aircraft) || (base && !aircraft) || (installation && !aircraft));	/* Only one of them is allowed. */
	assert((!base && installation) || (base && !installation) || (!base && !installation)); /* Only one of them is allowed or neither. */

	memset(slot, 0, sizeof(slot)); /* all values to 0 */
	slot->aircraft = aircraft;
	slot->base = base;
	slot->installation = installation;
	slot->item = NULL;
	slot->ammo = NULL;
	slot->nextAmmo = NULL;
	slot->size = ITEM_HEAVY;
	slot->nextItem = NULL;
	slot->type = type;
	slot->ammoLeft = AMMO_STATUS_NOT_SET; /** sa BDEF_AddBattery: it needs to be AMMO_STATUS_NOT_SET and not 0 @sa B_SaveBaseSlots */
	slot->installationTime = 0;
}

/**
 * @brief Check if item in given slot should change one aircraft stat.
 * @param[in] slot Pointer to the slot containing the item
 * @param[in] stat the stat that should be checked
 * @return qtrue if the item should change the stat.
 */
static qboolean AII_CheckUpdateAircraftStats (const aircraftSlot_t *slot, int stat)
{
	assert(slot);

	/* there's no item */
	if (!slot->item)
		return qfalse;

	/* you can not have advantages from items if it is being installed or removed, but only disavantages */
	if (slot->installationTime != 0) {
		const objDef_t *item = slot->item;
		if (item->craftitem.stats[stat] > 1.0f) /* advantages for relative and absolute values */
			return qfalse;
	}

	return qtrue;
}

/**
 * @brief returns the aircraftSlot of a base at an index or the first free slot
 * @param[in] base Pointer to base
 * @param[in] type defence type, see aircraftItemType_t
 * @param[in] idx index of aircraftslot
 * @returns the aircraftSlot at the index idx if idx non-negative the first free slot otherwise
 */
aircraftSlot_t *BDEF_GetBaseSlotByIDX (base_t *base, aircraftItemType_t type, int idx)
{
	switch (type) {
	case AC_ITEM_BASE_MISSILE:
		if (idx < 0) {			/* returns the first free slot on negative */
			int i;
			for (i = 0; i < base->numBatteries; i++)
				if (!base->batteries[i].slot.item && !base->batteries[i].slot.nextItem)
					return &base->batteries[i].slot;
		} else if (idx < base->numBatteries)
			return &base->batteries[idx].slot;
		break;
	case AC_ITEM_BASE_LASER:
		if (idx < 0) {			/* returns the first free slot on negative */
			int i;
			for (i = 0; i < base->numLasers; i++)
				if (!base->lasers[i].slot.item && !base->lasers[i].slot.nextItem)
					return &base->lasers[i].slot;
		} else if (idx < base->numLasers)
			return &base->lasers[idx].slot;
		break;
	default:
		break;
	}
	return NULL;
}

/**
 * @brief returns the aircraftSlot of an installaion at an index or the first free slot
 * @param[in] installation Pointer to the installation
 * @param[in] type defence type, see aircraftItemType_t
 * @param[in] idx index of aircraftslot
 * @returns the aircraftSlot at the index idx if idx non-negative the first free slot otherwise
 */
aircraftSlot_t *BDEF_GetInstallationSlotByIDX (installation_t *installation, aircraftItemType_t type, int idx)
{
	switch (type) {
	case AC_ITEM_BASE_MISSILE:
		if (idx < 0) {			/* returns the first free slot on negative */
			int i;
			for (i = 0; i < installation->numBatteries; i++)
				if (!installation->batteries[i].slot.item && !installation->batteries[i].slot.nextItem)
					return &installation->batteries[i].slot;
		} else if (idx < installation->numBatteries)
			return &installation->batteries[idx].slot;
		break;
	default:
		break;
	}
	return NULL;
}

/**
 * @brief returns the aircraftSlot of an aircraft at an index or the first free slot
 * @param[in] aircraft Pointer to aircraft
 * @param[in] type base defence type, see aircraftItemType_t
 * @param[in] idx index of aircraftslot
 * @returns the aircraftSlot at the index idx if idx non-negative the first free slot otherwise
 */
aircraftSlot_t *AII_GetAircraftSlotByIDX (aircraft_t *aircraft, aircraftItemType_t type, int idx)
{
	switch (type) {
	case AC_ITEM_WEAPON:
		if (idx < 0) {			/* returns the first free slot on negative */
			int i;
			for (i = 0; i < aircraft->maxWeapons; i++)
				if (!aircraft->weapons[i].item && !aircraft->weapons[i].nextItem)
					return &aircraft->weapons[i];
		} else if (idx < aircraft->maxWeapons)
			return &aircraft->weapons[idx];
		break;
	case AC_ITEM_SHIELD:
		if (idx == 0 || ((idx < 0) && !aircraft->shield.item && !aircraft->shield.nextItem))	/* returns the first free slot on negative */
			return &aircraft->shield;
		break;
	case AC_ITEM_ELECTRONICS:
		if (idx < 0) {			/* returns the first free slot on negative */
			int i;
			for (i = 0; i < aircraft->maxElectronics; i++)
				if (!aircraft->electronics[i].item && !aircraft->electronics[i].nextItem)
					return &aircraft->electronics[i];
		} else if (idx < aircraft->maxElectronics)
			return &aircraft->electronics[idx];
		break;
	default:
		break;
	}
	return NULL;
}


/**
 * @brief Get the maximum weapon range of aircraft.
 * @param[in] slot Pointer to the aircrafts weapon slot list.
 * @param[in] maxSlot maximum number of weapon slots in aircraft.
 * @return Maximum weapon range for this aircaft as an angle.
 */
float AIR_GetMaxAircraftWeaponRange (const aircraftSlot_t *slot, int maxSlot)
{
	int i;
	float range = 0.0f;

	assert(slot);

	/* We choose the usable weapon with the biggest range */
	for (i = 0; i < maxSlot; i++) {
		const aircraftSlot_t *weapon = slot + i;
		const objDef_t *ammo = weapon->ammo;

		if (!ammo)
			continue;

		/* make sure this item is useable */
		if (!AII_CheckUpdateAircraftStats(slot, AIR_STATS_WRANGE))
			continue;

		/* select this weapon if this is the one with the longest range */
		if (ammo->craftitem.stats[AIR_STATS_WRANGE] > range) {
			range = ammo->craftitem.stats[AIR_STATS_WRANGE];
		}
	}
	return range;
}

/**
 * @brief Repair aircraft.
 * @note Hourly called.
 */
void AII_RepairAircraft (void)
{
	int baseIDX, aircraftIDX;
	const int REPAIR_PER_HOUR = 1;	/**< Number of damage points repaired per hour */

	for (baseIDX = 0; baseIDX < MAX_BASES; baseIDX++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIDX);
		if (!base)
			continue;

		for (aircraftIDX = 0; aircraftIDX < base->numAircraftInBase; aircraftIDX++) {
			aircraft_t *aircraft = &base->aircraft[aircraftIDX];

			if (!AIR_IsAircraftInBase(aircraft))
				continue;
			aircraft->damage = min(aircraft->damage + REPAIR_PER_HOUR, aircraft->stats[AIR_STATS_DAMAGE]);
		}
	}
}

/**
 * @brief Update the value of stats array of an aircraft.
 * @param[in] aircraft Pointer to the aircraft
 * @note This should be called when an item starts to be added/removed and when addition/removal is over.
 */
void AII_UpdateAircraftStats (aircraft_t *aircraft)
{
	int i, currentStat;
	const aircraft_t *source;
	const objDef_t *item;

	assert(aircraft);

	source = aircraft->tpl;

	for (currentStat = 0; currentStat < AIR_STATS_MAX; currentStat++) {
		/* we scan all the stats except AIR_STATS_WRANGE (see below) */
		if (currentStat == AIR_STATS_WRANGE)
			continue;

		/* initialise value */
		aircraft->stats[currentStat] = source->stats[currentStat];

		/* modify by electronics (do nothing if the value of stat is 0) */
		for (i = 0; i < aircraft->maxElectronics; i++) {
			if (!AII_CheckUpdateAircraftStats(&aircraft->electronics[i], currentStat))
				continue;
			item = aircraft->electronics[i].item;
			if (fabs(item->craftitem.stats[currentStat]) > 2.0f)
				aircraft->stats[currentStat] += (int) item->craftitem.stats[currentStat];
			else if (!equal(item->craftitem.stats[currentStat], 0))
				aircraft->stats[currentStat] *= item->craftitem.stats[currentStat];
		}

		/* modify by weapons (do nothing if the value of stat is 0)
		 * note that stats are not modified by ammos */
		for (i = 0; i < aircraft->maxWeapons; i++) {
			if (!AII_CheckUpdateAircraftStats(&aircraft->weapons[i], currentStat))
				continue;
			item = aircraft->weapons[i].item;
			if (fabs(item->craftitem.stats[currentStat]) > 2.0f)
				aircraft->stats[currentStat] += item->craftitem.stats[currentStat];
			else if (!equal(item->craftitem.stats[currentStat], 0))
				aircraft->stats[currentStat] *= item->craftitem.stats[currentStat];
		}

		/* modify by shield (do nothing if the value of stat is 0) */
		if (AII_CheckUpdateAircraftStats(&aircraft->shield, currentStat)) {
			item = aircraft->shield.item;
			if (fabs(item->craftitem.stats[currentStat]) > 2.0f)
				aircraft->stats[currentStat] += item->craftitem.stats[currentStat];
			else if (!equal(item->craftitem.stats[currentStat], 0))
				aircraft->stats[currentStat] *= item->craftitem.stats[currentStat];
		}
	}

	/* now we update AIR_STATS_WRANGE (this one is the biggest range of every ammo) */
	aircraft->stats[AIR_STATS_WRANGE] = 1000.0f * AIR_GetMaxAircraftWeaponRange(aircraft->weapons, aircraft->maxWeapons);

	/* check that aircraft hasn't too much fuel (caused by removal of fuel pod) */
	if (aircraft->fuel > aircraft->stats[AIR_STATS_FUELSIZE])
		aircraft->fuel = aircraft->stats[AIR_STATS_FUELSIZE];

	/* check that aircraft hasn't too much HP (caused by removal of armour) */
	if (aircraft->damage > aircraft->stats[AIR_STATS_DAMAGE])
		aircraft->damage = aircraft->stats[AIR_STATS_DAMAGE];

	/* check that speed of the aircraft is positive */
	if (aircraft->stats[AIR_STATS_SPEED] < 1)
		aircraft->stats[AIR_STATS_SPEED] = 1;

	/* Update aircraft state if needed */
	if (aircraft->status == AIR_HOME && aircraft->fuel < aircraft->stats[AIR_STATS_FUELSIZE])
		aircraft->status = AIR_REFUEL;
}

/**
 * @brief Check if base or installation weapon can shoot
 * @param[in] weapons Pointer to the weapon array of the base.
 * @param[in] numWeapons Pointer to the number of weapon in this base.
 * @return qtrue if the base can fight, qfalse else
 * @sa AII_BaseCanShoot
 */
static qboolean AII_WeaponsCanShoot (const baseWeapon_t *weapons, int numWeapons)
{
	int i;

	for (i = 0; i < numWeapons; i++) {
		if (AIRFIGHT_CheckWeapon(&weapons[i].slot, 0) != AIRFIGHT_WEAPON_CAN_NEVER_SHOOT)
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Check if the base has weapon and ammo
 * @param[in] base Pointer to the base you want to check (may not be NULL)
 * @return qtrue if the base can shoot, qflase else
 * @sa AII_AircraftCanShoot
 */
int AII_BaseCanShoot (const base_t *base)
{
	assert(base);

	/* If we can shoot with missile defences */
	if (B_GetBuildingStatus(base, B_DEFENCE_MISSILE)
	 && AII_WeaponsCanShoot(base->batteries, base->numBatteries))
	 	return qtrue;

	/* If we can shoot with beam defences */
	if (B_GetBuildingStatus(base, B_DEFENCE_LASER)
	 && AII_WeaponsCanShoot(base->lasers, base->numLasers))
	 	return qtrue;

	return qfalse;
}

/**
 * @brief Check if the installation has a weapon and ammo
 * @param[in] installation Pointer to the installation you want to check (may not be NULL)
 * @return qtrue if the installation can shoot, qflase else
 * @sa AII_AircraftCanShoot
 */
qboolean AII_InstallationCanShoot (const installation_t *installation)
{
	assert(installation);

	if (installation->founded && installation->installationStatus == INSTALLATION_WORKING
	 && installation->installationTemplate->maxBatteries > 0) {
		/* installation is working and has battery */
		return AII_WeaponsCanShoot(installation->batteries, installation->installationTemplate->maxBatteries);
	}

	return qfalse;
}

/**
 * @brief Chooses a target for surface to air defences automatically
 * @param[in,out] weapons Weapons array
 * @param[in] maxWeapons Number of weapons
 */
static void BDEF_AutoTarget (baseWeapon_t *weapons, int maxWeapons)
{
	const installation_t *inst;
	const base_t *base;
	aircraft_t *closestCraft = NULL;
	float minCraftDistance = -1;
	aircraft_t *closestAttacker = NULL;
	float minAttackerDistance = -1;
	const aircraftSlot_t *slot;
	int i;

	if (maxWeapons <= 0)
		return;

	slot = &weapons[0].slot;
	/** Check if it's a Base or an Installation */
	if (slot->installation) {
		inst = slot->installation;
		base = NULL;
	} else if (slot->base) {
		base = slot->base;
		inst = NULL;
	} else
		Com_Error(ERR_DROP, "BDEF_AutoSelectTarget: slot doesn't belong to any base or installation");

	/** Get closest UFO(s) */
	for (i = 0; i < ccs.numUFOs; i++) {
		aircraft_t *ufo = &ccs.ufos[i];
		float distance;

		if (!UFO_IsUFOSeenOnGeoscape(ufo))
			continue;
		distance = MAP_GetDistance((inst) ? inst->pos : base->pos, ufo->pos);
		if (minCraftDistance < 0 || minCraftDistance > distance) {
			minCraftDistance = distance;
			closestCraft = ufo;
		}
		if ((minAttackerDistance < 0 || minAttackerDistance > distance) && ufo->mission
		 && ((base && ufo->mission->category == INTERESTCATEGORY_BASE_ATTACK && ufo->mission->data == base)
		 || (inst && ufo->mission->category == INTERESTCATEGORY_INTERCEPT && ufo->mission->data == inst))) {
			minAttackerDistance = distance;
			closestAttacker = ufo;
		}
	}

	/** Loop weaponslots */
	for (i = 0; i < maxWeapons; i++) {
		slot = &weapons[i].slot;
		/** skip if autofire is disabled */
		if (!weapons[i].autofire)
			continue;
		/** skip if no weapon or ammo assigned */
		if (!slot->item || !slot->ammo)
			continue;
		/** skip if weapon installation not yet finished */
		if (slot->installationTime > 0)
			continue;
		/** skip if no more ammo left */
		/** @note it's not really needed but it's cheaper not to check ufos in this case */
		if (slot->ammoLeft <= 0 && slot->ammoLeft != AMMO_STATUS_UNLIMITED)
			continue;

		if (closestAttacker) {
			const int test = AIRFIGHT_CheckWeapon(slot, minAttackerDistance);
			if (test != AIRFIGHT_WEAPON_CAN_NEVER_SHOOT
			 && test != AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT
			 && (minAttackerDistance <= slot->ammo->craftitem.stats[AIR_STATS_WRANGE]))
			 	weapons[i].target = closestAttacker;
		} else if (closestCraft) {
			const int test = AIRFIGHT_CheckWeapon(slot, minCraftDistance);
			if (test != AIRFIGHT_WEAPON_CAN_NEVER_SHOOT
			 && test != AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT
			 && (minCraftDistance <= slot->ammo->craftitem.stats[AIR_STATS_WRANGE]))
			 	weapons[i].target = closestCraft;
		}
	}
}

void BDEF_AutoSelectTarget (void)
{
	int i;

	for (i = 0; i < ccs.numBases; i++) {
		base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;

		BDEF_AutoTarget(base->batteries, base->numBatteries);
		BDEF_AutoTarget(base->lasers, base->numLasers);
	}

	for (i = 0; i < ccs.numInstallations; i++) {
		installation_t *inst = INS_GetFoundedInstallationByIDX(i);
		if (!inst)
			continue;

		BDEF_AutoTarget(inst->batteries, inst->numBatteries);
	}
}

/**
 * @brief Translate a weight int to a translated string
 * @sa itemWeight_t
 * @sa AII_GetItemWeightBySize
 */
const char* AII_WeightToName (itemWeight_t weight)
{
	switch (weight) {
	case ITEM_LIGHT:
		return _("Light weight");
		break;
	case ITEM_MEDIUM:
		return _("Medium weight");
		break;
	case ITEM_HEAVY:
		return _("Heavy weight");
		break;
	default:
		return _("Unknown weight");
		break;
	}
}

/**
 * @brief resets aircraftSlots' backreference pointers for aircraft
 * @param[in] aircraft Pointer to the aircraft
 */
void AII_CorrectAircraftSlotPointers (aircraft_t *aircraft)
{
	int i;

	assert(aircraft);

	for (i = 0; i < aircraft->maxWeapons; i++) {
		aircraft->weapons[i].aircraft = aircraft;
		aircraft->weapons[i].base = NULL;
		aircraft->weapons[i].installation = NULL;
	}
	for (i = 0; i < aircraft->maxElectronics; i++) {
		aircraft->electronics[i].aircraft = aircraft;
		aircraft->electronics[i].base = NULL;
		aircraft->electronics[i].installation = NULL;
	}
	aircraft->shield.aircraft = aircraft;
	aircraft->shield.base = NULL;
	aircraft->shield.installation = NULL;
}
