/**
 * @file m_node_image.c
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

/**
 * @brief Handled alfer the end of the load of the node from the script (all data and/or child are set)
 */
static void MN_ImageNodeLoaded (menuNode_t *node) {
	/* update the size when its possible */
	if (node->size[0] == 0 && node->size[1] == 0) {
		if (node->texl[0] != 0 || node->texh[0]) {
			node->size[0] = node->texh[0] - node->texl[0];
			node->size[1] = node->texh[1] - node->texl[1];
		}
	}
#ifdef DEBUG
	if (node->size[0] == 0 && node->size[1] == 0) {
		if (node->click || node->rclick || node->mouseIn || node->mouseOut || node->wheelUp || node->wheelDown || node->wheel || node->mclick) {
			Com_DPrintf(DEBUG_CLIENT, "Node '%s.%s' is an active image without size\n", node->menu->name, node->name);
		}
	}
#endif
}


void MN_DrawImageNode(menuNode_t *node)
{
	vec2_t size;
	vec2_t nodepos;
	const int time = cl.time;

	const char* imageName = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (!imageName || imageName[0] == '\0')
		return;

	MN_GetNodeAbsPos(node, nodepos);
	/* HACK for ekg pics */
	if (!Q_strncmp(node->name, "ekg_", 4)) {
		int pt;

		if (node->name[4] == 'm')
			pt = Cvar_VariableInteger("mn_morale") / 2;
		else
			pt = Cvar_VariableInteger("mn_hp");
		node->texl[1] = (3 - (int) (pt < 60 ? pt : 60) / 20) * 32;
		node->texh[1] = node->texl[1] + 32;
		node->texl[0] = -(int) (0.01 * (node->name[4] - 'a') * time) % 64;
		node->texh[0] = node->texl[0] + node->size[0];
	}
	if ((node->size[0] && !node->size[1])) {
		float scale;
		int w, h;

		R_DrawGetPicSize(&w, &h, imageName);
		scale = w / node->size[0];
		Vector2Set(size, node->size[0], h / scale);
	} else if ((node->size[1] && !node->size[0])) {
		float scale;
		int w, h;

		R_DrawGetPicSize(&w, &h, imageName);
		scale = h / node->size[1];
		Vector2Set(size, w / scale, node->size[1]);
	} else {
		Vector2Copy(node->size, size);
	}
	R_DrawNormPic(nodepos[0], nodepos[1], size[0], size[1],
		node->texh[0], node->texh[1], node->texl[0], node->texl[1], node->align, node->blend, imageName);
}

void MN_RegisterNodeImage (nodeBehaviour_t* behaviour)
{
	behaviour->name = "pic";
	behaviour->draw = MN_DrawImageNode;
	behaviour->loaded = MN_ImageNodeLoaded;
}
