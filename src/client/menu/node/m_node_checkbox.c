/**
 * @file m_node_checkbox.c
 * @code
 * checkbox check_item {
 *   cvar "*cvar mn_serverday"
 *   pos  "410 100"
 * }
 * @endcode
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

#include "../m_nodes.h"
#include "../m_parse.h"
#include "../m_main.h"
#include "../m_actions.h"
#include "../m_render.h"
#include "m_node_checkbox.h"
#include "m_node_abstractnode.h"

#define EXTRADATA(node) (node->u.abstractvalue)

static void MN_CheckBoxNodeDraw (menuNode_t* node)
{
	const float value = MN_GetReferenceFloat(node, EXTRADATA(node).value);
	vec2_t pos;
	const char *image = MN_GetReferenceString(node, node->image);
	int texx, texy;

	/* image set? */
	if (!image || image[0] == '\0')
		return;

	/* outer status */
	if (node->disabled) {
		texy = 96;
	} else if (node->state) {
		texy = 32;
	} else {
		texy = 0;
	}

	/* inner status */
	if (value == 0) {
		texx = 0;
	} else if (value > 0) {
		texx = 32;
	} else { /* value < 0 */
		texx = 64;
	}

	MN_GetNodeAbsPos(node, pos);
	MN_DrawNormImageByName(pos[0], pos[1], node->size[0], node->size[1],
		texx + node->size[0], texy + node->size[1], texx, texy, image);
}

/**
 * @brief Activate the node. Can be used without the mouse (ie. a button will execute onClick)
 */
static void MN_CheckBoxNodeActivate (menuNode_t *node)
{
	const float last = MN_GetReferenceFloat(node, EXTRADATA(node).value);
	float value;

	if (node->disabled)
		return;

	/* update value */
	value = (last > 0) ? 0 : 1;
	if (last == value)
		return;

	/* save result */
	EXTRADATA(node).lastdiff = value - last;
	if (!strncmp(EXTRADATA(node).value, "*cvar:", 6)) {
		MN_SetCvar(&((char*)EXTRADATA(node).value)[6], NULL, value);
	} else {
		*(float*) EXTRADATA(node).value = value;
	}

	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief Handles checkboxes clicks
 */
static void MN_CheckBoxNodeClick (menuNode_t * node, int x, int y)
{
	MN_CheckBoxNodeActivate(node);
}

/**
 * @brief Handled before the begin of the load of the node from the script
 */
static void MN_CheckBoxNodeLoading (menuNode_t *node)
{
}

static const value_t properties[] = {
	{"toggle", V_UI_NODEMETHOD, ((size_t) MN_CheckBoxNodeActivate), 0},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterCheckBoxNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "checkbox";
	behaviour->extends = "abstractvalue";
	behaviour->draw = MN_CheckBoxNodeDraw;
	behaviour->leftClick = MN_CheckBoxNodeClick;
	behaviour->loading = MN_CheckBoxNodeLoading;
	behaviour->activate = MN_CheckBoxNodeActivate;
	behaviour->properties = properties;
}
