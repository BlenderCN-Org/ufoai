/**
 * @file cl_inventory.h
 * @brief Header file for inventory handling and Equipment menu.
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

#ifndef CLIENT_CL_INVENTORY_H
#define CLIENT_CL_INVENTORY_H

#define MAX_ASSEMBLIES	16
#define MAX_COMP	32
#define ANTIMATTER_SIZE 10
/**
 * @brief The definition of a "components" entry (i.e. an assembly of several items) parsed from a ufo-file.
 * @sa INV_ParseComponents
 */
typedef struct components_s {
	char assembly_id[MAX_VAR];	/**< The name of the assemly (i.e. the UFO) */
	int assembly_idx;			/**< Index of object in csi.ods. */

	int time;	/**< The time (in hours) until the disassembly is finished. */

	int numItemtypes;					/**< Number of item-types listed below. (max is MAX_COMP) */
	char item_id[MAX_COMP][MAX_VAR];	/**< id of the item (in ods[]). */
/**	int item_idx[MAX_COMP];			< @todo index of item in ods[] array */
	int item_amount[MAX_COMP];			/**< How many items of this type are in this assembly. */
	int item_amount2[MAX_COMP];		/**< How many items of this type are in this assembly when it crashed (max-value?). */
} components_t;

void INV_ParseComponents(const char *name, const char **text);

void INV_CollectingItems(int won);
void INV_SellOrAddItems(aircraft_t *aircraft);
void INV_EnableAutosell(technology_t *tech);
void INV_InitialEquipment(base_t *base, campaign_t* campaign);
components_t *INV_GetComponentsByItemIdx(int itemIdx);
int INV_DisassemblyItem(base_t *base, components_t *comp, qboolean calculate);
void INV_ManageAntimatter(base_t *base, int amount, qboolean add);
qboolean INV_MoveItem(base_t* base, inventory_t* inv, int toContainer, int px, int py, int fromContainer, int fromX, int fromY);

#ifdef DEBUG
void INV_InventoryList_f(void);
#endif

#endif /* CLIENT_CL_INVENTORY_H */

