/**
 * @file
 * @brief Single player market stuff.
 * @note Buy/Sell functions prefix: BS_
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

#include "../../cl_shared.h"
#include "cp_campaign.h"
#include "cp_market.h"
#include "cp_popup.h"
#include "save/save_market.h"

#define BS_GetMarket() (&ccs.eMarket)

/**
 * @brief Check if an item is on market
 * @param[in] item Pointer to the item to check
 * @note this function doesn't check if the item is available on market (buyable > 0)
 */
qboolean BS_IsOnMarket (const objDef_t *item)
{
	assert(item);
	return !(item->isVirtual || item->notOnMarket);
}

/**
 * @brief Get the number of items of the given type on the market
 * @param[in] od the item (objDef) to search the market for
 * @return The amount of items for the given type
 */
int BS_GetItemOnMarket (const objDef_t *od)
{
	const market_t *market = BS_GetMarket();
	return BS_IsOnMarket(od) ? market->numItems[od->idx] : 0;
}

/**
 * @brief Internal function to add items to the market
 * @param[in] od Object definition (the item itself)
 * @param[in] amount Non-negative number of items to add
 */
static void BS_AddItemToMarket (const objDef_t *od, int amount)
{
	market_t *market = BS_GetMarket();
	assert(amount >= 0);
	market->numItems[od->idx] += amount;
}

/**
 * @brief Internal function to remove items from the market
 * @param[in] od Object definition (the item itself)
 * @param[in] amount Non-negative number of items to remove
 */
static void BS_RemoveItemFromMarket (const objDef_t *od, int amount)
{
	market_t *market = BS_GetMarket();

	assert(amount >= 0);

	market->numItems[od->idx] = std::max(market->numItems[od->idx] - amount, 0);
}

/**
 * @brief Get the price for an item that you want to sell on the market
 * @param[in] od The item to sell
 * @return The price of the item
 */
int BS_GetItemSellingPrice (const objDef_t *od)
{
	const market_t *market = BS_GetMarket();
	return market->bidItems[od->idx];
}

/**
 * @brief Get the price for an item that you want to buy on the market
 * @param[in] od The item to buy
 * @return The price of the item
 */
int BS_GetItemBuyingPrice (const objDef_t *od)
{
	const market_t *market = BS_GetMarket();
	return market->askItems[od->idx];
}

/**
 * @brief Checks whether a given aircraft should appear on the market
 * @param aircraft The aircraft to check
 * @return @c true if the aircraft should appear on the market
 */
qboolean BS_AircraftIsOnMarket (const aircraft_t *aircraft)
{
	if (aircraft->type == AIRCRAFT_UFO)
		return qfalse;
	if (aircraft->price == -1)
		return qfalse;

	return  qtrue;
}

/**
 * @brief Get the number of aircraft of the given type on the market
 * @param[in] aircraft The aircraft to search the market for
 * @return The amount of aircraft for the given type
 */
int BS_GetAircraftOnMarket (const aircraft_t *aircraft)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	const market_t *market = BS_GetMarket();

	return BS_AircraftIsOnMarket(aircraft) ? market->numAircraft[type] : 0;
}

/**
 * @brief Internal function to add aircraft to the market
 * @param[in] aircraft Aircraft template definition
 * @param[in] amount Non-negative number of aircraft to add
 */
static void BS_AddAircraftToMarket (const aircraft_t *aircraft, int amount)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	market_t *market = BS_GetMarket();
	assert(amount >= 0);
	assert(aircraft->type != AIRCRAFT_UFO);
	market->numAircraft[type] += amount;
}

/**
 * @brief Internal function to remove aircraft from the market
 * @param[in] aircraft Aircraft template definition
 * @param[in] amount Non-negative number of aircraft to remove
 */
static void BS_RemoveAircraftFromMarket (const aircraft_t *aircraft, int amount)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	market_t *market = BS_GetMarket();

	assert(amount >= 0);
	assert(aircraft->type != AIRCRAFT_UFO);

	market->numAircraft[type] = std::max(market->numAircraft[type] - amount, 0);
}

/**
 * @brief Get the price for an aircraft that you want to sell on the market
 * @param[in] aircraft The aircraft to sell
 * @return The price of the aircraft
 */
int BS_GetAircraftSellingPrice (const aircraft_t *aircraft)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	const market_t *market = BS_GetMarket();
	int sellPrice = market->bidAircraft[type];

	assert(aircraft->type != AIRCRAFT_UFO);

	if (aircraft->tpl != aircraft) {
		int i;

		if (aircraft->shield.item)
			sellPrice += BS_GetItemSellingPrice(aircraft->shield.item);
		if (aircraft->shield.ammo)
			sellPrice += BS_GetItemSellingPrice(aircraft->shield.ammo);

		for (i = 0; i < aircraft->maxWeapons; i++) {
			if (aircraft->weapons[i].item)
				sellPrice += BS_GetItemSellingPrice(aircraft->weapons[i].item);
			if (aircraft->weapons[i].ammo)
				sellPrice += BS_GetItemSellingPrice(aircraft->weapons[i].ammo);
		}

		for (i = 0; i < aircraft->maxElectronics; i++) {
			if (aircraft->electronics[i].item)
				sellPrice += BS_GetItemSellingPrice(aircraft->electronics[i].item);
			if (aircraft->electronics[i].ammo)
				sellPrice += BS_GetItemSellingPrice(aircraft->electronics[i].ammo);
		}
	}
	return sellPrice;
}

/**
 * @brief Get the price for an aircraft that you want to buy on the market
 * @param[in] aircraft The aircraft to buy
 * @return The price of the aircraft
 */
int BS_GetAircraftBuyingPrice (const aircraft_t *aircraft)
{
	const humanAircraftType_t type = Com_DropShipShortNameToID(aircraft->id);
	const market_t *market = BS_GetMarket();
	assert(aircraft->type != AIRCRAFT_UFO);
	return market->askAircraft[type];
}

/**
 * @brief Update storage, the market, and the player's credits
 * @note Don't update capacity here because we can sell items directly from aircraft (already removed from storage).
 */
static void BS_ProcessCraftItemSale (const objDef_t *craftitem, const int numItems)
{
	if (craftitem) {
		BS_AddItemToMarket(craftitem, numItems);
		CP_UpdateCredits(ccs.credits + BS_GetItemSellingPrice(craftitem) * numItems);
	}
}

/**
 * @brief Buys an aircraft
 * @param[in] aircraft The aircraft template to buy
 * @param[out] base Base to buy at
 * @return @c true if the aircraft could get bought, @c false otherwise
 */
qboolean BS_BuyAircraft (const aircraft_t *aircraftTemplate, base_t *base)
{
	int freeSpace;
	int price;

	if (!base)
		Com_Error(ERR_DROP, "BS_BuyAircraft: No base given.");
	if (!aircraftTemplate)
		Com_Error(ERR_DROP, "BS_BuyAircraft: No aircraft template given.");

	if (!B_GetBuildingStatus(base, B_COMMAND))
		return qfalse;
	/* We cannot buy aircraft if there is no power in our base. */
	if (!B_GetBuildingStatus(base, B_POWER))
		return qfalse;
	/* We cannot buy aircraft without any hangar. */
	if (!AIR_AircraftAllowed(base))
		return qfalse;

	/* Check free space in hangars. */
	freeSpace = AIR_CalculateHangarStorage(aircraftTemplate, base, 0);
	if (freeSpace <= 0)
		return qfalse;

	price = BS_GetAircraftBuyingPrice(aircraftTemplate);
	if (ccs.credits < price)
		return qfalse;

	/* Hangar capacities are being updated in AIR_NewAircraft().*/
	BS_RemoveAircraftFromMarket(aircraftTemplate, 1);
	CP_UpdateCredits(ccs.credits - price);
	AIR_NewAircraft(base, aircraftTemplate);

	return qtrue;
}

/**
 * @brief Sells the given aircraft with all the equipment.
 * @param aircraft The aircraft to sell
 * @return @c true if the aircraft could get sold, @c false otherwise
 */
qboolean BS_SellAircraft (aircraft_t *aircraft)
{
	int j;

	if (AIR_GetTeamSize(aircraft) > 0)
		return qfalse;

	if (!AIR_IsAircraftInBase(aircraft))
		return qfalse;

	/* sell off any items which are mounted on it */
	for (j = 0; j < aircraft->maxWeapons; j++) {
		const aircraftSlot_t *slot = &aircraft->weapons[j];
		BS_ProcessCraftItemSale(slot->item, 1);
		BS_ProcessCraftItemSale(slot->ammo, 1);
	}

	BS_ProcessCraftItemSale(aircraft->shield.item, 1);
	/* there should be no ammo here, but checking can't hurt */
	BS_ProcessCraftItemSale(aircraft->shield.ammo, 1);

	for (j = 0; j < aircraft->maxElectronics; j++) {
		const aircraftSlot_t *slot = &aircraft->electronics[j];
		BS_ProcessCraftItemSale(slot->item, 1);
		/* there should be no ammo here, but checking can't hurt */
		BS_ProcessCraftItemSale(slot->ammo, 1);
	}

	/* the capacities are also updated here */
	BS_AddAircraftToMarket(aircraft, 1);
	CP_UpdateCredits(ccs.credits + BS_GetAircraftSellingPrice(aircraft));
	AIR_DeleteAircraft(aircraft);

	return qtrue;
}

/**
 * @brief Buys the given UGV
 * @param[in] ugv The ugv template of the UGV to buy
 * @param[out] base Base to buy at
 * @return @c true if the ugv could get bought, @c false otherwise
 * @TODO Implement this correctly once we have UGV
 */
qboolean BS_BuyUGV (const ugv_t *ugv, base_t *base)
{
	const objDef_t *ugvWeapon;

	if (!ugv)
		Com_Error(ERR_DROP, "BS_BuyUGV: Called on NULL UGV!");
	if (!base)
		Com_Error(ERR_DROP, "BS_BuyUGV: Called on NULL base!");
	ugvWeapon = INVSH_GetItemByID(ugv->weapon);
	if (!ugvWeapon)
		Com_Error(ERR_DROP, "BS_BuyItem_f: Could not get weapon '%s' for ugv/tank '%s'.", ugv->weapon, ugv->id);

	if (ccs.credits < ugv->price)
		return qfalse;
	if (E_CountUnhiredRobotsByType(ugv) <= 0)
		return qfalse;
	if (BS_GetItemOnMarket(ugvWeapon) <= 0)
		return qfalse;
	if (CAP_GetFreeCapacity(base, CAP_ITEMS) < UGV_SIZE + ugvWeapon->size)
		return qfalse;
	if (!E_HireRobot(base, ugv))
		return qfalse;

	BS_RemoveItemFromMarket(ugvWeapon, 1);
	CP_UpdateCredits(ccs.credits - ugv->price);
	B_UpdateStorageAndCapacity(base, ugvWeapon, 1, qfalse);

	return qtrue;
}

/**
 * @brief Sells the given UGV with all the equipment.
 * @param robot The employee record of the UGV to sell
 * @return @c true if the ugv could get sold, @c false otherwise
 * @TODO Implement this correctly once we have UGV
 */
qboolean BS_SellUGV (employee_t *robot)
{
	const objDef_t *ugvWeapon;
	const ugv_t *ugv;
	base_t *base;

	if (!robot)
		Com_Error(ERR_DROP, "Selling NULL UGV!");
	if (!robot->ugv)
		Com_Error(ERR_DROP, "Selling invalid UGV with UCN: %i", robot->chr.ucn);
	ugv = robot->ugv;
	base = robot->baseHired;

	/* Check if we have a weapon for this ugv in the market to sell it. */
	ugvWeapon = INVSH_GetItemByID(ugv->weapon);
	if (!ugvWeapon)
		Com_Error(ERR_DROP, "BS_BuyItem_f: Could not get wepaon '%s' for ugv/tank '%s'.", ugv->weapon, ugv->id);

	if (!E_UnhireEmployee(robot)) {
		/** @todo message - Couldn't fire employee. */
		Com_DPrintf(DEBUG_CLIENT, "Couldn't sell/fire robot/ugv.\n");
		return qfalse;
	}

	BS_AddItemToMarket(ugvWeapon, 1);
	CP_UpdateCredits(ccs.credits + ugv->price);
	B_UpdateStorageAndCapacity(base, ugvWeapon, -1, qfalse);

	return qtrue;
}

/**
 * @brief Buys items from the market
 * @param[in] od pointer to the item (Object Definition record)
 * @param[out] base Base to buy at
 * @param[in ] count Number of items to buy
 * @return @c true if the ugv could get bought, @c false otherwise
 */
qboolean BS_BuyItem (const objDef_t *od, base_t *base, int count)
{
	if (!od)
		Com_Error(ERR_DROP, "BS_BuyItem: Called on NULL objDef!");
	if (!base)
		Com_Error(ERR_DROP, "BS_BuyItem: Called on NULL base!");

	if (!BS_IsOnMarket(od))
		return qfalse;
	if (count <= 0)
		return qfalse;
	if (ccs.credits < BS_GetItemBuyingPrice(od) * count)
		return qfalse;
	if (BS_GetItemOnMarket(od) < count)
		return qfalse;
	if (CAP_GetFreeCapacity(base, CAP_ITEMS) < od->size * count)
		return qfalse;

	B_UpdateStorageAndCapacity(base, od, count, qfalse);
	BS_RemoveItemFromMarket(od, count);
	CP_UpdateCredits(ccs.credits - BS_GetItemBuyingPrice(od) * count);

	return qtrue;
}

/**
 * @brief Sells items from the market
 * @param[in] od pointer to the item (Object Definition record)
 * @param[out] base Base to sell at
 * @param[in ] count Number of items to sell
 * @return @c true if the ugv could get sold, @c false otherwise
 */
qboolean BS_SellItem (const objDef_t *od, base_t *base, int count)
{
	if (!od)
		Com_Error(ERR_DROP, "BS_SellItem: Called on NULL objDef!");

	if (!BS_IsOnMarket(od))
		return qfalse;
	if (count <= 0)
		return qfalse;

	if (base) {
		if (B_ItemInBase(od, base) < count)
			return qfalse;
		B_UpdateStorageAndCapacity(base, od, -1 * count, qfalse);
	}

	BS_AddItemToMarket(od, count);
	CP_UpdateCredits(ccs.credits + BS_GetItemSellingPrice(od) * count);

	return qtrue;
}

/**
 * @brief Save callback for savegames
 * @param[out] parent XML Node structure, where we write the information to
 * @sa BS_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean BS_SaveXML (xmlNode_t *parent)
{
	int i;
	xmlNode_t *node;
	const market_t *market = BS_GetMarket();

	/* store market */
	node = XML_AddNode(parent, SAVE_MARKET_MARKET);
	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (BS_IsOnMarket(od)) {
			xmlNode_t * snode = XML_AddNode(node, SAVE_MARKET_ITEM);
			XML_AddString(snode, SAVE_MARKET_ID, od->id);
			XML_AddIntValue(snode, SAVE_MARKET_NUM, market->numItems[i]);
			XML_AddIntValue(snode, SAVE_MARKET_BID, market->bidItems[i]);
			XML_AddIntValue(snode, SAVE_MARKET_ASK, market->askItems[i]);
			XML_AddDoubleValue(snode, SAVE_MARKET_EVO, market->currentEvolutionItems[i]);
			XML_AddBoolValue(snode, SAVE_MARKET_AUTOSELL, market->autosell[i]);
		}
	}
	for (i = 0; i < AIRCRAFTTYPE_MAX; i++) {
		if (market->bidAircraft[i] > 0 || market->askAircraft[i] > 0) {
			xmlNode_t * snode = XML_AddNode(node, SAVE_MARKET_AIRCRAFT);
			const char *shortName = Com_DropShipTypeToShortName((humanAircraftType_t)i);
			XML_AddString(snode, SAVE_MARKET_ID, shortName);
			XML_AddIntValue(snode, SAVE_MARKET_NUM, market->numAircraft[i]);
			XML_AddIntValue(snode, SAVE_MARKET_BID, market->bidAircraft[i]);
			XML_AddIntValue(snode, SAVE_MARKET_ASK, market->askAircraft[i]);
			XML_AddDoubleValue(snode, SAVE_MARKET_EVO, market->currentEvolutionAircraft[i]);
		}
	}
	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @param[in] parent XML Node structure, where we get the information from
 * @sa BS_Save
 * @sa SAV_GameLoad
 */
qboolean BS_LoadXML (xmlNode_t *parent)
{
	xmlNode_t *node, *snode;
	market_t *market = BS_GetMarket();

	node = XML_GetNode(parent, SAVE_MARKET_MARKET);
	if (!node)
		return qfalse;

	for (snode = XML_GetNode(node, SAVE_MARKET_ITEM); snode; snode = XML_GetNextNode(snode, node, SAVE_MARKET_ITEM)) {
		const char *s = XML_GetString(snode, SAVE_MARKET_ID);
		const objDef_t *od = INVSH_GetItemByID(s);

		if (!od) {
			Com_Printf("BS_LoadXML: Could not find item '%s'\n", s);
			continue;
		}

		market->numItems[od->idx] = XML_GetInt(snode, SAVE_MARKET_NUM, 0);
		market->bidItems[od->idx] = XML_GetInt(snode, SAVE_MARKET_BID, 0);
		market->askItems[od->idx] = XML_GetInt(snode, SAVE_MARKET_ASK, 0);
		market->currentEvolutionItems[od->idx] = XML_GetDouble(snode, SAVE_MARKET_EVO, 0.0);
		market->autosell[od->idx] = XML_GetBool(snode, SAVE_MARKET_AUTOSELL, qfalse);
	}
	for (snode = XML_GetNode(node, SAVE_MARKET_AIRCRAFT); snode; snode = XML_GetNextNode(snode, node, SAVE_MARKET_AIRCRAFT)) {
		const char *s = XML_GetString(snode, SAVE_MARKET_ID);
		const humanAircraftType_t type = Com_DropShipShortNameToID(s);

		market->numAircraft[type] = XML_GetInt(snode, SAVE_MARKET_NUM, 0);
		market->bidAircraft[type] = XML_GetInt(snode, SAVE_MARKET_BID, 0);
		market->askAircraft[type] = XML_GetInt(snode, SAVE_MARKET_ASK, 0);
		market->currentEvolutionAircraft[type] = XML_GetDouble(snode, SAVE_MARKET_EVO, 0.0);
	}

	return qtrue;
}

/**
 * @brief sets market prices at start of the game
 * @sa CP_CampaignInit
 * @sa B_SetUpFirstBase
 * @sa BS_Load (Market load function)
 */
void BS_InitMarket (const campaign_t *campaign)
{
	int i;
	market_t *market = BS_GetMarket();

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		if (market->askItems[i] == 0) {
			market->askItems[i] = od->price;
			market->bidItems[i] = floor(market->askItems[i] * BID_FACTOR);
		}

		if (campaign->marketDef->numItems[i] <= 0)
			continue;

		if (RS_IsResearched_ptr(RS_GetTechForItem(od))) {
			/* the other relevant values were already set above */
			market->numItems[i] = campaign->marketDef->numItems[i];
		} else {
			Com_Printf("BS_InitMarket: Could not add item %s to the market - not marked as researched in campaign %s\n",
					od->id, campaign->id);
		}
	}

	for (i = 0; i < AIRCRAFTTYPE_MAX; i++) {
		const char* name = Com_DropShipTypeToShortName((humanAircraftType_t)i);
		const aircraft_t *aircraft = AIR_GetAircraft(name);
		if (market->askAircraft[i] == 0) {
			market->askAircraft[i] = aircraft->price;
			market->bidAircraft[i] = floor(market->askAircraft[i] * BID_FACTOR);
		}

		if (campaign->marketDef->numAircraft[i] <= 0)
			continue;

		if (RS_IsResearched_ptr(aircraft->tech)) {
			/* the other relevant values were already set above */
			market->numAircraft[i] = campaign->marketDef->numAircraft[i];
		} else {
			Com_Printf("BS_InitMarket: Could not add aircraft %s to the market - not marked as researched in campaign %s\n",
					aircraft->id, campaign->id);
		}
	}
}

/**
 * @brief make number of items change every day.
 * @sa CP_CampaignRun
 * @sa daily called
 * @note This function makes items number on market slowly reach the asymptotic number of items defined in equipment.ufo
 * If an item has just been researched, it's not available on market until RESEARCH_LIMIT_DELAY days is reached.
 */
void CP_CampaignRunMarket (campaign_t *campaign)
{
	int i;
	const float TYPICAL_TIME = 10.f;			/**< Number of days to reach the asymptotic number of items */
	const int RESEARCH_LIMIT_DELAY = 30;		/**< Numbers of days after end of research to wait in order to have
												 * items added on market */
	market_t *market = BS_GetMarket();

	assert(campaign->marketDef);
	assert(campaign->asymptoticMarketDef);

	for (i = 0; i < csi.numODs; i++) {
		const objDef_t *od = INVSH_GetItemByIDX(i);
		const technology_t *tech = RS_GetTechForItem(od);
		int asymptoticNumber;

		if (RS_IsResearched_ptr(tech) && (campaign->marketDef->numItems[i] != 0 || ccs.date.day > tech->researchedDate.day + RESEARCH_LIMIT_DELAY)) {
			/* if items are researched for more than RESEARCH_LIMIT_DELAY or was on the initial market,
			 * there number tend to the value defined in equipment.ufo.
			 * This value is the asymptotic value if it is not 0, or initial value else */
			asymptoticNumber = campaign->asymptoticMarketDef->numItems[i] ? campaign->asymptoticMarketDef->numItems[i] : campaign->marketDef->numItems[i];
		} else {
			/* items that have just been researched don't appear on market, but they can disappear */
			asymptoticNumber = 0;
		}

		/* Store the evolution of the market in currentEvolution */
		market->currentEvolutionItems[i] += (asymptoticNumber - market->numItems[i]) / TYPICAL_TIME;

		/* Check if new items appeared or disappeared on market */
		if (fabs(market->currentEvolutionItems[i]) >= 1.0f) {
			const int num = (int)(market->currentEvolutionItems[i]);
			if (num >= 0)
				BS_AddItemToMarket(od, num);
			else
				BS_RemoveItemFromMarket(od, -num);
			market->currentEvolutionItems[i] -= num;
		}
	}

	for (i = 0; i < AIRCRAFTTYPE_MAX; i++) {
		const humanAircraftType_t type = (humanAircraftType_t)i;
		const char *aircraftID = Com_DropShipTypeToShortName(type);
		const aircraft_t* aircraft = AIR_GetAircraft(aircraftID);
		const technology_t *tech = aircraft->tech;
		int asymptoticNumber;

		if (RS_IsResearched_ptr(tech) && (campaign->marketDef->numAircraft[i] != 0 || ccs.date.day > tech->researchedDate.day + RESEARCH_LIMIT_DELAY)) {
			/* if aircraft is researched for more than RESEARCH_LIMIT_DELAY or was on the initial market,
			 * there number tend to the value defined in equipment.ufo.
			 * This value is the asymptotic value if it is not 0, or initial value else */
			asymptoticNumber = campaign->asymptoticMarketDef->numAircraft[i] ? campaign->asymptoticMarketDef->numAircraft[i] : campaign->marketDef->numAircraft[i];
		} else {
			/* items that have just been researched don't appear on market, but they can disappear */
			asymptoticNumber = 0;
		}
		/* Store the evolution of the market in currentEvolution */
		market->currentEvolutionAircraft[i] += (asymptoticNumber - market->numAircraft[i]) / TYPICAL_TIME;

		/* Check if new items appeared or disappeared on market */
		if (fabs(market->currentEvolutionAircraft[i]) >= 1.0f) {
			const int num = (int)(market->currentEvolutionAircraft[i]);
			if (num >= 0)
				BS_AddAircraftToMarket(aircraft, num);
			else
				BS_RemoveAircraftFromMarket(aircraft, -num);
			market->currentEvolutionAircraft[i] -= num;
		}
	}
}

/**
 * @brief Returns true if you can buy or sell equipment
 * @param[in] base Pointer to base to check on
 * @sa B_BaseInit_f
 */
qboolean BS_BuySellAllowed (const base_t* base)
{
	return !B_IsUnderAttack(base) && B_GetBuildingStatus(base, B_STORAGE);
}
