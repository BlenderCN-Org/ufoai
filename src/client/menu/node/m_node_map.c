/**
 * @file m_node_map.c
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
#include "../../campaign/cl_map.h"
#include "../m_parse.h"
#include "../m_nodes.h"
#include "../m_input.h"
#include "m_node_model.h"
#include "m_node_map.h"

static void MN_MapNodeDraw (menuNode_t *node)
{
	if (curCampaign) {
		/* don't run the campaign in console mode */
		if (cls.key_dest != key_console)
			CL_CampaignRun();	/* advance time */
		MAP_DrawMap(node); /* Draw geoscape */
	}
}

typedef enum {
	MODE_NULL,
	MODE_SHIFT2DMAP,
	MODE_SHIFT3DMAP,
	MODE_ZOOMMAP,
} mapDragMode_t;

static int oldMousePosX = 0;
static int oldMousePosY = 0;
static mapDragMode_t mode = MODE_NULL;

static void MN_MapNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	switch (mode) {
	case MODE_SHIFT2DMAP:
	{
		int i;
		const float zoom = 0.5 / ccs.zoom;
		/* shift the map */
		ccs.center[0] -= (float) (mousePosX - oldMousePosX) / (ccs.mapSize[0] * ccs.zoom);
		ccs.center[1] -= (float) (mousePosY - oldMousePosY) / (ccs.mapSize[1] * ccs.zoom);
		for (i = 0; i < 2; i++) {
			/* clamp to min/max values */
			while (ccs.center[i] < 0.0)
				ccs.center[i] += 1.0;
			while (ccs.center[i] > 1.0)
				ccs.center[i] -= 1.0;
		}
		if (ccs.center[1] < zoom)
			ccs.center[1] = zoom;
		if (ccs.center[1] > 1.0 - zoom)
			ccs.center[1] = 1.0 - zoom;
		break;
	}

	case MODE_SHIFT3DMAP:
		/* rotate a model */
		ccs.angles[PITCH] += ROTATE_SPEED * (mousePosX - oldMousePosX) / ccs.zoom;
		ccs.angles[YAW] -= ROTATE_SPEED * (mousePosY - oldMousePosY) / ccs.zoom;

		/* clamp the angles */
		while (ccs.angles[YAW] > 180.0)
			ccs.angles[YAW] -= 360.0;
		while (ccs.angles[YAW] < -180.0)
			ccs.angles[YAW] += 360.0;

		while (ccs.angles[PITCH] > 180.0)
			ccs.angles[PITCH] -= 360.0;
		while (ccs.angles[PITCH] < -180.0)
			ccs.angles[PITCH] += 360.0;
		break;
	case MODE_ZOOMMAP:
	{
		const float zoom = 0.5 / ccs.zoom;
		/* zoom the map */
		ccs.zoom *= pow(0.995, mousePosY - oldMousePosY);
		if (ccs.zoom < cl_mapzoommin->value)
			ccs.zoom = cl_mapzoommin->value;
		else if (ccs.zoom > cl_mapzoommax->value)
			ccs.zoom = cl_mapzoommax->value;

		if (ccs.center[1] < zoom)
			ccs.center[1] = zoom;
		if (ccs.center[1] > 1.0 - zoom)
			ccs.center[1] = 1.0 - zoom;
		break;
	}
	default:
		assert(qfalse);
	}
	oldMousePosX = x;
	oldMousePosY = y;
}

static void MN_MapNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	/* finish the last drag before */
	if (mode != MODE_NULL)
		return;

	switch (button) {
	case K_MOUSE2:
		if (!ccs.combatZoomOn || !ccs.combatZoomedUFO) {
			MN_SetMouseCapture(node);
			if (!cl_3dmap->integer)
				mode = MODE_SHIFT2DMAP;
			else
				mode = MODE_SHIFT3DMAP;
			MAP_StopSmoothMovement();
			oldMousePosX = x;
			oldMousePosY = y;
		}
		break;
	case K_MOUSE3:
		MN_SetMouseCapture(node);
		mode = MODE_ZOOMMAP;
		oldMousePosX = x;
		oldMousePosY = y;
		break;
	}
}

static void MN_MapNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (mode == MODE_NULL)
		return;

	switch (button) {
	case K_MOUSE2:
		if (mode == MODE_SHIFT3DMAP || mode == MODE_SHIFT2DMAP) {
			mode = MODE_NULL;
			MN_MouseRelease();
		}
		break;
	case K_MOUSE3:
		if (mode == MODE_ZOOMMAP) {
			mode = MODE_NULL;
			MN_MouseRelease();
		}
		break;
	}
}

static void MN_MapNodeMouseWheel (menuNode_t *node, qboolean down, int x, int y)
{
	if (ccs.combatZoomOn  && ccs.combatZoomedUFO) {
		return;
	}

	ccs.zoom *= pow(0.995, (down ? 10: -10));
	if (ccs.zoom < cl_mapzoommin->value)
		ccs.zoom = cl_mapzoommin->value;
	else if (ccs.zoom > cl_mapzoommax->value) {
		ccs.zoom = cl_mapzoommax->value;
		if (!down) {
			MAP_TurnCombatZoomOn();
		}
	}

	if (!cl_3dmap->integer) {
		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
	}
	MAP_StopSmoothMovement();
}

/**
 * @brief Called before loading. Used to set default attribute values
 */
static void MN_MapNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1, 1, 1, 1);
}

void MN_RegisterMapNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "map";
	behaviour->draw = MN_MapNodeDraw;
	behaviour->leftClick = MAP_MapClick;
	behaviour->mouseDown = MN_MapNodeMouseDown;
	behaviour->mouseUp = MN_MapNodeMouseUp;
	behaviour->capturedMouseMove = MN_MapNodeCapturedMouseMove;
	behaviour->mouseWheel = MN_MapNodeMouseWheel;
	behaviour->loading = MN_MapNodeLoading;
}
