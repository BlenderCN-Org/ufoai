/**
 * @file m_node_item.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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
#include "../cl_map.h"
#include "m_main.h"
#include "m_draw.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_inventory.h"
#include "m_tooltip.h"
#include "m_nodes.h"
#include "m_node_model.h"

/**
 * @todo need a cleanup/marge/refactoring with MN_DrawItemNode
 */
static void MN_DrawItemNode2 (menuNode_t *node)
{
	menu_t *menu = node->menu;
	const objDef_t *od;
	const char* ref = MN_GetReferenceString(node->menu, node->dataImageOrModel);

	if (!ref || ref[0] == '\0')
		return;

	od = INVSH_GetItemByIDSilent(ref);
	if (od) {
		MN_DrawItemNode(node, ref);
	} else {
		const aircraft_t *aircraft = AIR_GetAircraft(ref);
		if (aircraft) {
			assert(aircraft->tech);
			MN_DrawModelNode(menu, node, ref, aircraft->tech->mdl);
		} else {
			Com_Printf("Unknown item: '%s'\n", ref);
		}
	}
}

void MN_RegisterNodeItem (nodeBehaviour_t *behaviour)
{
	behaviour->name = "item";
	behaviour->draw = MN_DrawItemNode2;
}
