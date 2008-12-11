/**
 * @file m_node_special.c
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
#include "../m_input.h"
#include "../m_timer.h"
#include "../m_actions.h"
#include "../../cl_input.h"
#include "../../cl_keys.h"
#include "m_node_zone.h"
#include "m_node_window.h"

static menuTimer_t *capturedTimer = NULL;

static void MN_ZoneNodeRepeat (menuNode_t *node, menuTimer_t *timer)
{
	if (node->onClick) {
		MN_ExecuteEventActions(node, node->onClick);
	}
}

static void MN_ZoneNodeDown (menuNode_t *node, int x, int y, int button)
{
	/** @todo remove that when the input handler is updated */
	if (node->disabled)
		return;
	if (!node->repeat)
		return;
	if (button == K_MOUSE1) {
		MN_SetMouseCapture(node);
		capturedTimer = MN_AllocTimer(node, node->clickDelay, MN_ZoneNodeRepeat);
		MN_TimerStart (capturedTimer);
	}
}

static void MN_ZoneNodeUp (menuNode_t *node, int x, int y, int button)
{
	/** @todo remove that when the input handler is updated */
	if (node->disabled)
		return;
	if (button == K_MOUSE1) {
		if (capturedTimer) {
			MN_TimerRelease(capturedTimer);
			capturedTimer = NULL;
		}
		MN_MouseRelease();
	}
}

/**
 * @brief Call before the script initialized the node
 */
static void MN_ZoneNodeLoading (menuNode_t *node)
{
	node->clickDelay = 1000;
}

/**
 * @brief Call after the script initialized the node
 */
static void MN_ZoneNodeLoaded (menuNode_t *node)
{
	menu_t * menu = node->menu;
	if (!Q_strncmp(node->name, "render", 6)) {
		if (!menu->renderNode)
			menu->renderNode = node;
		else
			Com_Printf("MN_ParseMenuBody: second render node ignored (menu \"%s\")\n", menu->name);
	} else if (!Q_strncmp(node->name, "popup", 5)) {
		if (!menu->popupNode)
			menu->popupNode = node;
		else
			Com_Printf("MN_ParseMenuBody: second popup node ignored (menu \"%s\")\n", menu->name);
	}
}

static const value_t properties[] = {
	{"repeat", V_BOOL, offsetof(menuNode_t, repeat), MEMBER_SIZEOF(menuNode_t, repeat)},
	{"clickdelay", V_INT, offsetof(menuNode_t, clickDelay), MEMBER_SIZEOF(menuNode_t, clickDelay)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterZoneNode (nodeBehaviour_t *behaviour)
{
	memset(behaviour, 0, sizeof(behaviour));
	behaviour->name = "zone";
	behaviour->id = MN_ZONE;
	behaviour->loading = MN_ZoneNodeLoading;
	behaviour->loaded = MN_ZoneNodeLoaded;
	behaviour->mouseDown = MN_ZoneNodeDown;
	behaviour->mouseUp = MN_ZoneNodeUp;
	behaviour->properties = properties;
}
