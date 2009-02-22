/**
 * @file m_node_tbar.c
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

#include "../../client.h"
#include "../../renderer/r_draw.h"
#include "../m_nodes.h"
#include "../m_parse.h"
#include "m_node_tbar.h"
#include "m_node_abstractvalue.h"
#include "m_node_abstractnode.h"

#define TEXTURE_WIDTH 250.0

static void MN_TBarNodeDraw (menuNode_t *node)
{
	/* dataImageOrModel is the texture name */
	float shx;
	vec2_t nodepos;
	const char* ref = MN_GetReferenceString(node, node->image);
	float pointWidth;
	float width;
	if (!ref || ref[0] == '\0')
		return;

	MN_GetNodeAbsPos(node, nodepos);

	pointWidth = TEXTURE_WIDTH / 100.0;	/* relative to the texture */

	{
		float ps;
		const float min = MN_GetReferenceFloat(node, node->u.abstractvalue.min);
		const float max = MN_GetReferenceFloat(node, node->u.abstractvalue.max);
		float value = MN_GetReferenceFloat(node, node->u.abstractvalue.value);
		/* clamp the value */
		if (value > max)
			value = max;
		if (value < min)
			value = min;
		ps = (value - min) / (max - min) * 100;
		shx = node->texl[0];	/* left gap to the texture */
		shx += round(ps * pointWidth); /* add size from 0..TEXTURE_WIDTH */
	}

	width = (shx * node->size[0]) / TEXTURE_WIDTH;

	R_DrawNormPic(nodepos[0], nodepos[1], width, node->size[1],
		shx, node->texh[1], node->texl[0], node->texl[1], ALIGN_UL, node->blend, ref);
}

static void MN_TBarNodeLoaded (menuNode_t *node)
{
	if (node->size[0] == 0 && node->size[1] == 0) {
		Com_Printf("MN_TBarNodeLoaded: Please fix the size of the node %s.%s (it should be '250 16')", node->menu->name, node->name);
		node->size[0] = 250;
		node->size[1] = 16;
	}
	if (node->u.abstractvalue.max == NULL) {
		float *f;
		Com_Printf("MN_TBarNodeLoaded: Please fix a max value to the node %s.%s (it should be '100')", node->menu->name, node->name);
		f = MN_AllocFloat(1);
		*f = 100;
		node->u.abstractvalue.max = (void*)f;
	}
}

static const value_t properties[] = {
	{"gap_width", V_INT, offsetof(menuNode_t, gapWidth), MEMBER_SIZEOF(menuNode_t, gapWidth)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterTBarNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "tbar";
	behaviour->extends = "abstractvalue";
	behaviour->draw = MN_TBarNodeDraw;
	behaviour->loaded = MN_TBarNodeLoaded;
	behaviour->properties = properties;
}
