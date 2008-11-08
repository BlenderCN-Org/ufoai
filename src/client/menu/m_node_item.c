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
#include "m_dragndrop.h"
#include "m_tooltip.h"
#include "m_nodes.h"
#include "m_node_model.h"
#include "m_node_item.h"
#include "m_node_container.h"

static void MN_DrawItemNode (menuNode_t *node, const char *itemName)
{
	int i;
	item_t item = {1, NULL, NULL, 0, 0}; /* 1 so it's not red-ish; fake item anyway */
	const vec4_t color = {1, 1, 1, 1};
	vec2_t pos;

	for (i = 0; i < csi.numODs; i++)
		if (!Q_strncmp(itemName, csi.ods[i].id, MAX_VAR))
			break;
	if (i == csi.numODs)
		return;

	item.t = &csi.ods[i];

	/* We position the model of the item ourself (in the middle of the item
	 * node). See the "-1, -1" parameter of MN_DrawItem. */
	MN_GetNodeAbsPos(node, pos);
	pos[0] += node->size[0] / 2.0;
	pos[1] += node->size[1] / 2.0;
	MN_DrawItem(pos, &item, -1, -1, node->scale, color);
}

/**
 * @todo need a cleanup/marge/refactoring with MN_DrawItemNode
 */
static void MN_DrawItemNode2 (menuNode_t *node)
{
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
			MN_DrawModelNode(node, ref, aircraft->tech->mdl);
		} else {
			Com_Printf("Unknown item: '%s'\n", ref);
		}
	}
}

void MN_RegisterItemNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "item";
	behaviour->draw = MN_DrawItemNode2;
}
