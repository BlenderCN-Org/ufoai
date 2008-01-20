/**
 * @file m_input.c
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
#include "m_menu.h"

#include "m_main.h"
#include "m_parse.h"
#include "m_input.h"
#include "m_inventory.h"
#include "m_node_text.h"

/**
 * @sa IN_Parse
 */
qboolean MN_CursorOnMenu (int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next)
			if (MN_CheckNodeZone(node, x, y)) {
				/* found an element */
				/*MN_FocusSetNode(node);*/
				return qtrue;
			}

		if (menu->renderNode) {
			/* don't care about non-rendered windows */
			if (menu->renderNode->invis)
				return qtrue;
			else
				return qfalse;
		}
	}

	return qfalse;
}

/**
 * @brief Handles checkboxes clicks
 */
static void MN_CheckboxClick (menuNode_t * node)
{
	int value;

	assert(node->data[MN_DATA_MODEL_SKIN_OR_CVAR]);
	/* no cvar? */
	if (Q_strncmp(node->data[MN_DATA_MODEL_SKIN_OR_CVAR], "*cvar", 5))
		return;

	value = Cvar_VariableInteger(&((char*)node->data[MN_DATA_MODEL_SKIN_OR_CVAR])[6]) ^ 1;
	Cvar_SetValue(&((char*)node->data[MN_DATA_MODEL_SKIN_OR_CVAR])[6], value);
}

/**
 * @brief Handles selectboxes clicks
 * @sa MN_SELECTBOX
 */
static void MN_SelectboxClick (menu_t * menu, menuNode_t * node, int y)
{
	selectBoxOptions_t* selectBoxOption;
	int clickedAtOption;

	clickedAtOption = (y - node->pos[1]);

	if (node->size[1])
		clickedAtOption = (clickedAtOption - node->size[1]) / node->size[1];
	else
		clickedAtOption = (clickedAtOption - SELECTBOX_DEFAULT_HEIGHT) / SELECTBOX_DEFAULT_HEIGHT; /* default height - see selectbox.tga */

	if (clickedAtOption < 0 || clickedAtOption >= node->height)
		return;

	/* the cvar string is stored in data[MN_DATA_MODEL_SKIN_OR_CVAR] */
	/* no cvar given? */
	if (!node->data[MN_DATA_MODEL_SKIN_OR_CVAR] || !*(char*)node->data[MN_DATA_MODEL_SKIN_OR_CVAR]) {
		Com_Printf("MN_SelectboxClick: node '%s' doesn't have a valid cvar assigned (menu %s)\n", node->name, node->menu->name);
		return;
	}

	/* no cvar? */
	if (Q_strncmp((char*)node->data[MN_DATA_MODEL_SKIN_OR_CVAR], "*cvar", 5))
		return;

	/* only execute the click stuff if the selectbox is active */
	if (node->state) {
		selectBoxOption = node->options;
		for (; clickedAtOption > 0 && selectBoxOption; selectBoxOption = selectBoxOption->next) {
			clickedAtOption--;
		}
		if (selectBoxOption) {
			/* strip '*cvar ' from data[0] - length is already checked above */
			Cvar_Set(&((char*)node->data[MN_DATA_MODEL_SKIN_OR_CVAR])[6], selectBoxOption->value);
			if (*selectBoxOption->action) {
#ifdef DEBUG
				if (selectBoxOption->action[strlen(selectBoxOption->action)-1] != ';')
					Com_Printf("selectbox option with none terminated action command");
#endif
				Cbuf_AddText(selectBoxOption->action);
			}
		}
	}
}

/**
 * @brief Handles the bar cvar values
 * @sa Key_Message
 */
static void MN_BarClick (menu_t * menu, menuNode_t * node, int x)
{
	char var[MAX_VAR];
	float frac, min;

	if (!node->mousefx)
		return;

	Q_strncpyz(var, node->data[2], sizeof(var));
	/* no cvar? */
	if (Q_strncmp(var, "*cvar", 5))
		return;

	/* normalize it */
	frac = (float) (x - node->pos[0]) / node->size[0];
	/* in the case of MN_BAR the first three data array values are float values - see menuDataValues_t */
	min = MN_GetReferenceFloat(menu, node->data[1]);
	Cvar_SetValue(&var[6], min + frac * (MN_GetReferenceFloat(menu, node->data[0]) - min));
}

/**
 * @brief Activates the model rotation
 * @note set the mouse space to MS_ROTATE
 * @sa rotateAngles
 */
static void MN_ModelClick (menuNode_t * node)
{
	mouseSpace = MS_ROTATE;
	/* modify node->angles (vec3_t) if you rotate the model */
	rotateAngles = node->angles;
}


/**
 * @brief Calls the script command for a text node that is clickable
 * @note The node must have the click parameter
 * @sa MN_TextRightClick
 */
static void MN_TextClick (menuNode_t * node, int mouseOver)
{
	char cmd[MAX_VAR];
	Com_sprintf(cmd, sizeof(cmd), "%s_click", node->name);
	if (Cmd_Exists(cmd))
		Cbuf_AddText(va("%s %i\n", cmd, mouseOver - 1));
}

/**
 * @brief Calls the script command for a text node that is clickable via right mouse button
 * @note The node must have the rclick parameter
 * @sa MN_TextClick
 */
static void MN_TextRightClick (menuNode_t * node, int mouseOver)
{
	char cmd[MAX_VAR];
	Com_sprintf(cmd, sizeof(cmd), "%s_rclick", node->name);
	if (Cmd_Exists(cmd))
		Cbuf_AddText(va("%s %i\n", cmd, mouseOver - 1));
}

/**
 * @brief Is called everytime one clickes on a menu/screen. Then checks if anything needs to be executed in the earea of the click (e.g. button-commands, inventory-handling, geoscape-stuff, etc...)
 * @sa MN_ModelClick
 * @sa MN_TextRightClick
 * @sa MN_TextClick
 * @sa MN_Drag
 * @sa MN_BarClick
 * @sa MN_CheckboxClick
 * @sa MN_BaseMapClick
 * @sa MAP_3DMapClick
 * @sa MAP_MapClick
 * @sa MN_ExecuteActions
 * @sa MN_RightClick
 * @sa Key_Message
 * @sa CL_MessageMenu_f
 * @note inline editing of cvars (e.g. save dialog) is done in Key_Message
 */
void MN_Click (int x, int y)
{
	menuNode_t *node, *execute_node = NULL;
	menu_t *menu;
	int sp, mouseOver;
	qboolean clickedInside = qfalse;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		execute_node = NULL;
		for (node = menu->firstNode; node; node = node->next) {
			if (node->type != MN_CONTAINER && node->type != MN_CHECKBOX
			 && node->type != MN_SELECTBOX && !node->click)
				continue;

			/* check whether mouse is over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);

			if (!mouseOver)
				continue;

			/* check whether we clicked at least on one menu node */
			clickedInside = qtrue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_CONTAINER:
				MN_Drag(node, x, y, qfalse);
				break;
			case MN_BAR:
				MN_BarClick(menu, node, x);
				break;
			case MN_BASEMAP:
				MN_BaseMapClick(node, x, y);
				break;
			case MN_MAP:
				MAP_MapClick(node, x, y);
				break;
			case MN_CHECKBOX:
				MN_CheckboxClick(node);
				break;
			case MN_SELECTBOX:
				MN_SelectboxClick(menu, node, y);
				break;
			case MN_MODEL:
				MN_ModelClick(node);
				break;
			case MN_TEXT:
				MN_TextClick(node, mouseOver);
				break;
			default:
				/* Save the action for later execution. */
				if (node->click && (node->click->type != EA_NULL))
					execute_node = node;
				break;
			}
		}

		/* Only execute the last-found (i.e. from the top-most displayed node) action found.
		 * Especially needed for button-nodes that are (partially) overlayed and all have actions defined.
		 * e.g. the firemode checkboxes.
		 */
		if (execute_node) {
			MN_ExecuteActions(menu, execute_node->click);
			if (execute_node->repeat) {
				mouseSpace = MS_LHOLD;
				mn.mouseRepeat.menu = menu;
				mn.mouseRepeat.action = execute_node->click;
				mn.mouseRepeat.nexttime = cls.realtime + 500;	/* second "event" after 0.5 sec */
			}
		}

		/* @todo: maybe we should also check sp == mn.menuStackPos here */
		if (!clickedInside && menu->leaveNode)
			MN_ExecuteActions(menu, menu->leaveNode->click);

		/* don't care about non-rendered windows */
		if (menu->renderNode || menu->popupNode)
			return;
	}
}

/**
 * @sa MAP_ResetAction
 * @sa MN_TextRightClick
 * @sa MN_ExecuteActions
 * @sa MN_Click
 * @sa MN_MiddleClick
 * @sa MN_MouseWheel
 */
void MN_RightClick (int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			/* no right click for this node defined */
			if (node->type != MN_CONTAINER && !node->rclick)
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_CONTAINER:
				MN_Drag(node, x, y, qtrue);
				break;
			case MN_BASEMAP:
				MN_BaseMapRightClick(node, x, y);
				break;
			case MN_MAP:
				MAP_ResetAction();
				if (!cl_3dmap->integer)
					mouseSpace = MS_SHIFTMAP;
				else
					mouseSpace = MS_SHIFT3DMAP;
				break;
			case MN_TEXT:
				MN_TextRightClick(node, mouseOver);
				break;
			default:
				MN_ExecuteActions(menu, node->rclick);
				break;
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}

/**
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MouseWheel
 */
void MN_MiddleClick (int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			/* no middle click for this node defined */
			if (!node->mclick)
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_MAP:
				mouseSpace = MS_ZOOMMAP;
				break;
			default:
				MN_ExecuteActions(menu, node->mclick);
				break;
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}

/**
 * @brief Called when we are in menu mode and scroll via mousewheel
 * @note The geoscape zooming code is here, too (also in CL_ParseInput)
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 * @sa MN_Click
 * @sa MN_RightClick
 * @sa MN_MiddleClick
 * @sa CL_ZoomInQuant
 * @sa CL_ZoomOutQuant
 */
void MN_MouseWheel (qboolean down, int x, int y)
{
	menuNode_t *node;
	menu_t *menu;
	int sp, mouseOver;

	sp = mn.menuStackPos;

	while (sp > 0) {
		menu = mn.menuStack[--sp];
		for (node = menu->firstNode; node; node = node->next) {
			/* no middle click for this node defined */
			/* both wheelUp & wheelDown required */
			if (!node->wheel && !(node->wheelUp && node->wheelDown))
				continue;

			/* check whether mouse if over this node */
			mouseOver = MN_CheckNodeZone(node, x, y);
			if (!mouseOver)
				continue;

			/* found a node -> do actions */
			switch (node->type) {
			case MN_MAP:
				ccs.zoom *= pow(0.995, (down ? 10: -10));
				if (ccs.zoom < cl_mapzoommin->value)
					ccs.zoom = cl_mapzoommin->value;
				else if (ccs.zoom > cl_mapzoommax->value)
					ccs.zoom = cl_mapzoommax->value;

				if (!cl_3dmap->integer) {
					if (ccs.center[1] < 0.5 / ccs.zoom)
						ccs.center[1] = 0.5 / ccs.zoom;
					if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
						ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
				}
				break;
			case MN_TEXT:
				if (node->wheelUp && node->wheelDown) {
					MN_ExecuteActions(menu, (down ? node->wheelDown : node->wheelUp));
				} else {
					MN_TextScroll(node, (down ? 1 : -1));
					/* they can also have script commands assigned */
					MN_ExecuteActions(menu, node->wheel);
				}
				break;
			default:
				if (node->wheelUp && node->wheelDown)
					MN_ExecuteActions(menu, (down ? node->wheelDown : node->wheelUp));
				else
					MN_ExecuteActions(menu, node->wheel);
				break;
			}
		}

		if (menu->renderNode || menu->popupNode)
			/* don't care about non-rendered windows */
			return;
	}
}
