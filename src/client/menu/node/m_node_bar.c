/**
 * @file m_node_bar.c
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
#include "../m_input.h"
#include "../../cl_keys.h"
#include "m_node_bar.h"
#include "m_node_abstractvalue.h"
#include "m_node_abstractnode.h"

static void MN_BarNodeDraw (menuNode_t *node)
{
	vec4_t color;
	float fac, bar_width;
	vec2_t nodepos;
	const float min = MN_GetReferenceFloat(node, node->u.abstractvalue.min);
	const float max = MN_GetReferenceFloat(node, node->u.abstractvalue.max);
	const float value = MN_GetReferenceFloat(node, node->u.abstractvalue.value);

	MN_GetNodeAbsPos(node, nodepos);
	VectorScale(node->color, 0.8, color);
	color[3] = node->color[3];

	fac = node->size[0] / (max - min);
	bar_width = (value - min) * fac;
	R_DrawFill(nodepos[0], nodepos[1], bar_width, node->size[1], ALIGN_UL, node->state ? color : node->color);
}

/**
 * @brief Called when the node is captured by the mouse
 */
static void MN_BarNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	char var[MAX_VAR];
	vec2_t pos;

	MN_NodeAbsoluteToRelativePos(node, &x, &y);

	if (x < 0)
		x = 0;
	else if (x > node->size[0])
		x = node->size[0];

	MN_GetNodeAbsPos(node, pos);
	Q_strncpyz(var, node->u.abstractvalue.value, sizeof(var));
	/* no cvar? */
	if (!Q_strncmp(var, "*cvar", 5)) {
		/* normalize it */
		const float frac = (float) x / node->size[0];
		const float min = MN_GetReferenceFloat(node, node->u.abstractvalue.min);
		const float value = min + frac * (MN_GetReferenceFloat(node, node->u.abstractvalue.max) - min);
		MN_SetCvar(&var[6], NULL, value);
	}
}

static void MN_BarNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	if (!node->mousefx || node->disabled)
		return;

	if (button == K_MOUSE1) {
		MN_SetMouseCapture(node);
		MN_BarNodeCapturedMouseMove(node, x, y);
	}
}

static void MN_BarNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (button == K_MOUSE1)
		MN_MouseRelease();
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_BarNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
}

void MN_RegisterBarNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "bar";
	behaviour->extends = "abstractvalue";
	behaviour->draw = MN_BarNodeDraw;
	behaviour->loading = MN_BarNodeLoading;
	behaviour->mouseDown = MN_BarNodeMouseDown;
	behaviour->mouseUp = MN_BarNodeMouseUp;
	behaviour->capturedMouseMove = MN_BarNodeCapturedMouseMove;
}
