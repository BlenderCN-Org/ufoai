/**
 * @file cl_market.c
 * @brief Single player market stuff.
 * @note Buy/Sell menu functions prefix: BS_
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
#include "../mxml/mxml_ufoai.h"
#include "../cl_menu.h"
#include "../menu/m_popup.h"
#include "cl_campaign.h"
#include "cl_market.h"
#include "cp_market_callbacks.h"

/** @brief Max amount of aircraft type calculated for the market. */
static const int MAX_AIRCRAFT_SUPPLY = 8;


/**
 * @brief Calculates amount of aircraft in base and on the market.
 * @param[in] base The base to get the storage amount from
 * @param[in] airCharId Aircraft id (type).
 * @param[in] inbase True if function has to return storage, false - when supply (market).
 * @return Amount of aircraft in base or amount of aircraft on the market.
 */
int AIR_GetStorageSupply (const base_t *base, const char *airCharId, qboolean inbase)
{
	const aircraft_t *aircraft;
	int i, j;
	int amount = 0, storage = 0, supply;

	/* Get storage amount in baseCurrent. */
	for (j = 0, aircraft = base->aircraft; j < base->numAircraftInBase; j++, aircraft++) {
		if (!Q_strcmp(aircraft->id, airCharId))
			storage++;
	}
	/* Get supply amount (global). */
	for (j = 0; j < MAX_BASES; j++) {
		base = B_GetFoundedBaseByIDX(j);
		if (!base)
			continue;
		for (i = 0, aircraft = base->aircraft; i < base->numAircraftInBase; i++, aircraft++) {
			if (!Q_strcmp(aircraft->id, airCharId))
				amount++;
		}
	}
	if (amount < MAX_AIRCRAFT_SUPPLY)
		supply = MAX_AIRCRAFT_SUPPLY - amount;
	else
		supply = 0;

	if (inbase)
		return storage;
	else
		return supply;
}

/**
 * @brief Buy items.
 * @param[in] base Pointer to the base where items are bought.
 * @param[in] item Pointer to the item to buy.
 * @param[in] number Number of items to buy.
 */
qboolean BS_CheckAndDoBuyItem (base_t* base, const objDef_t *item, int number)
{
	int numItems;

	assert(base);
	assert(item);

	/* you can't buy more items than there are on market */
	numItems = min(number, ccs.eMarket.num[item->idx]);

	/* you can't buy more items than you have credits for */
	/** @todo Handle items with price 0 better */
	if (ccs.eMarket.ask[item->idx])
		numItems = min(numItems, ccs.credits / ccs.eMarket.ask[item->idx]);
	if (numItems <= 0)
		return qfalse;

	/* you can't buy more items than you have room for */
	/** @todo Handle items with size 0 better */
	if (item->size)
		numItems = min(numItems, (base->capacities[CAP_ITEMS].max - base->capacities[CAP_ITEMS].cur) / item->size);
	/* make sure that numItems is > 0 (can be negative because capacities.cur may be greater than
	 * capacities.max if storage is disabled or if alien items have been collected on mission */
	if (numItems <= 0) {
		MN_Popup(_("Not enough storage space"), _("You cannot buy this item.\nNot enough space in storage.\nBuild more storage facilities."));
		return qfalse;
	}

	B_UpdateStorageAndCapacity(base, item, numItems, qfalse, qfalse);
	ccs.eMarket.num[item->idx] -= numItems;
	CL_UpdateCredits(ccs.credits - ccs.eMarket.ask[item->idx] * numItems);
	return qtrue;
}


/**
 * @brief Update storage, the market, and the player's credits
 * @note Don't update capacity here because we can sell items directly from aircraft (already removed from storage).
 */
void BS_ProcessCraftItemSale (const base_t *base, const objDef_t *craftitem, const int numItems)
{
	if (craftitem) {
		ccs.eMarket.num[craftitem->idx] += numItems;
		CL_UpdateCredits(ccs.credits + (ccs.eMarket.bid[craftitem->idx] * numItems));
	}
}

/**
 * @brief Save callback for savegames
 * @sa BS_LoadXML
 * @sa SAV_GameSaveXML
 */
qboolean BS_SaveXML (mxml_node_t *parent)
{
	int i;
	mxml_node_t *node;
	/* store market */
	node = mxml_AddNode(parent, "market");
	for (i = 0; i < MAX_OBJDEFS; i++) {
		if (csi.ods[i].id[0] != '\0') {
			mxml_node_t * snode = mxml_AddNode(node, "element");
			mxml_AddString(snode, "id", csi.ods[i].id);
			mxml_AddInt(snode, "num", ccs.eMarket.num[i]);
			mxml_AddInt(snode, "bid", ccs.eMarket.bid[i]);
			mxml_AddInt(snode, "ask", ccs.eMarket.ask[i]);
			mxml_AddDouble(snode, "evo", ccs.eMarket.currentEvolution[i]);
			mxml_AddBool(snode, "autosell", ccs.autosell[i]);
		}
	}

	return qtrue;
}
/**
 * @brief Save callback for savegames
 * @sa BS_Load
 * @sa SAV_GameSave
 */
qboolean BS_Save (sizebuf_t* sb, void* data)
{
	int i;

	/* store market */
	for (i = 0; i < presaveArray[PRE_NUMODS]; i++) {
		MSG_WriteString(sb, csi.ods[i].id);
		MSG_WriteLong(sb, ccs.eMarket.num[i]);
		MSG_WriteLong(sb, ccs.eMarket.bid[i]);
		MSG_WriteLong(sb, ccs.eMarket.ask[i]);
		MSG_WriteFloat(sb, ccs.eMarket.currentEvolution[i]);
		MSG_WriteByte(sb, ccs.autosell[i]);
	}

	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa BS_Save
 * @sa SAV_GameLoad
 */
qboolean BS_LoadXML (mxml_node_t *parent)
{
	int i;
	mxml_node_t *node, *snode;
	node = mxml_GetNode(parent, "market");
	if (!node)
		return qfalse;
	for(i = 0, snode = mxml_GetNode(node, "element"); i < MAX_OBJDEFS; i++, snode = mxml_GetNextNode(snode, node, "element")) {
		if (snode) {
			const char *s = mxml_GetString(snode, "id");
			objDef_t *od = INVSH_GetItemByID(s);
			if (!od) {
				Com_Printf("BS_Load: Could not find item '%s'\n", s);
			} else {
				ccs.eMarket.num[od->idx] = mxml_GetInt(snode, "num", 0);
				ccs.eMarket.bid[od->idx] = mxml_GetInt(snode, "bid", 0);
				ccs.eMarket.ask[od->idx] = mxml_GetInt(snode, "ask", 0);
				ccs.eMarket.currentEvolution[od->idx] = mxml_GetDouble(snode, "evo", 0.0);
				ccs.autosell[od->idx] = mxml_GetBool(snode, "autosell", qfalse);
			}
		}
	}

	return qtrue;
}
/**
 * @brief Load callback for savegames
 * @sa BS_Save
 * @sa SAV_GameLoad
 */
qboolean BS_Load (sizebuf_t* sb, void* data)
{
	int i;

	/* read market */
	for (i = 0; i < presaveArray[PRE_NUMODS]; i++) {
		const char *s = MSG_ReadString(sb);
		objDef_t *od = INVSH_GetItemByID(s);
		if (!od) {
			Com_Printf("BS_Load: Could not find item '%s'\n", s);
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
			MSG_ReadFloat(sb);
			MSG_ReadByte(sb);
		} else {
			ccs.eMarket.num[od->idx] = MSG_ReadLong(sb);
			ccs.eMarket.bid[od->idx] = MSG_ReadLong(sb);
			ccs.eMarket.ask[od->idx] = MSG_ReadLong(sb);
			ccs.eMarket.currentEvolution[od->idx] = MSG_ReadFloat(sb);
			ccs.autosell[od->idx] = MSG_ReadByte(sb);
		}
	}

	return qtrue;
}

/**
 * @brief Returns true if you can buy or sell equipment
 * @sa B_BaseInit_f
 */
qboolean BS_BuySellAllowed (const base_t* base)
{
	if (base->baseStatus != BASE_UNDER_ATTACK
	 && B_GetBuildingStatus(base, B_STORAGE)) {
		return qtrue;
	} else {
		return qfalse;
	}
}
