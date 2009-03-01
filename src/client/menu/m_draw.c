/**
 * @file m_draw.c
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

#include "m_main.h"
#include "m_nodes.h"
#include "m_internal.h"
#include "m_draw.h"
#include "m_actions.h"
#include "m_font.h"
#include "m_input.h"
#include "m_timer.h" /* define MN_HandleTimers */
#include "m_dragndrop.h"
#include "m_tooltip.h"
#include "node/m_node_abstractnode.h"

#include "../client.h"
#include "../renderer/r_draw.h"

#ifdef DEBUG
static cvar_t *mn_debug;
#endif

static cvar_t *mn_show_tooltips;
static const int TOOLTIP_DELAY = 500; /* delay that msecs before showing tooltip */
static qboolean tooltipVisible = qfalse;
static menuTimer_t *tooltipTimer;

/**
 * @brief Node we will draw over
 * @sa MN_CaptureDrawOver
 * @sa nodeBehaviour_t.drawOverMenu
 */
static menuNode_t *drawOverNode = NULL;

/**
 * @brief Capture a node we will draw over all nodes per menu
 * @note The node must be captured every frames
 * @todo it can be better to capture the draw over only one time (need new event like mouseEnter, mouseLeave)
 */
void MN_CaptureDrawOver (menuNode_t *node)
{
	drawOverNode = node;
}

#ifdef DEBUG

static int debugTextPositionY = 0;
static int debugPositionX = 0;
#define DEBUG_PANEL_WIDTH 300

static void MN_HilightNode (const menuNode_t *node, const vec4_t color)
{
	static const vec4_t grey = {0.7, 0.7, 0.7, 1.0};
	vec2_t pos;
	int width;
	int lineDefinition[4];
	const char* text;

	if (node->parent) {
		MN_HilightNode (node->parent, grey);
	}

	MN_GetNodeAbsPos(node, pos);

	text = va("%s (%s)", node->name, node->behaviour->name);
	R_FontTextSize("f_small_bold", text, DEBUG_PANEL_WIDTH, LONGLINES_PRETTYCHOP, &width, NULL, NULL, NULL);

	R_ColorBlend(color);
	R_FontDrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX+20, debugTextPositionY, DEBUG_PANEL_WIDTH, DEBUG_PANEL_WIDTH, 0, text, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;

	if (debugPositionX != 0) {
		lineDefinition[0] = debugPositionX + 20;
		lineDefinition[2] = pos[0] + node->size[0];
	} else {
		lineDefinition[0] = debugPositionX + 20 + width;
		lineDefinition[2] = pos[0];
	}
	lineDefinition[1] = debugTextPositionY - 5;
	lineDefinition[3] = pos[1];
	R_DrawLine(lineDefinition, 1);
	R_ColorBlend(NULL);

	/* exclude rect */
	if (node->excludeRectNum) {
		int i;
		vec4_t trans = {1, 1, 1, 1};
		Vector4Copy(color, trans);
		trans[3] = trans[3] / 2;
		for (i = 0; i < node->excludeRectNum; i++) {
			const int x = pos[0] + node->excludeRect[i].pos[0];
			const int y = pos[1] + node->excludeRect[i].pos[1];
			R_DrawFill(x, y, node->excludeRect[i].size[0], node->excludeRect[i].size[1], ALIGN_UL, trans);
		}
	}

	/* bounded box */
	R_DrawRect(pos[0] - 1, pos[1] - 1, node->size[0] + 2, node->size[1] + 2, color, 2.0, 0x3333);
}

/**
 * @brief Prints active node names for debugging
 */
static void MN_DrawDebugMenuNodeNames (void)
{
	static const vec4_t red = {1.0, 0.0, 0.0, 1.0};
	static const vec4_t green = {0.0, 0.5, 0.0, 1.0};
	static const vec4_t white = {1, 1.0, 1.0, 1.0};
	static const vec4_t background = {0.0, 0.0, 0.0, 0.5};
	menuNode_t *hoveredNode;
	int stackPosition;

	debugTextPositionY = 100;

	/* x panel position with hysteresis */
	if (mousePosX < viddef.virtualWidth / 3)
		debugPositionX = viddef.virtualWidth - DEBUG_PANEL_WIDTH;
	if (mousePosX > 2 * viddef.virtualWidth / 3)
		debugPositionX = 0;

	/* background */
	R_DrawFill(debugPositionX, debugTextPositionY, DEBUG_PANEL_WIDTH, VID_NORM_HEIGHT - debugTextPositionY - 100, ALIGN_UL, background);

	/* menu stack */
	R_ColorBlend(white);
	R_FontDrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "menu stack:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
	debugTextPositionY += 15;
	for (stackPosition = 0; stackPosition < mn.menuStackPos; stackPosition++) {
		menuNode_t *menu = mn.menuStack[stackPosition];
		R_ColorBlend(white);
		R_FontDrawString("f_small_bold", ALIGN_UL, debugPositionX+20, debugTextPositionY, debugPositionX+20, debugTextPositionY, 200, 200, 0, menu->name, 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;
	}

	/* hovered node */
	hoveredNode = MN_GetHoveredNode();
	if (hoveredNode) {
		R_ColorBlend(white);
		R_FontDrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "-----------------------", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;

		R_ColorBlend(white);
		R_FontDrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "hovered node:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
		debugTextPositionY += 15;
		MN_HilightNode(hoveredNode, red);
	}

	/* target node */
	if (MN_DNDIsDragging()) {
		menuNode_t *targetNode = MN_DNDGetTargetNode();
		if (targetNode) {
			R_ColorBlend(white);
			R_FontDrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "-----------------------", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
			debugTextPositionY += 15;

			R_ColorBlend(green);
			R_FontDrawString("f_small_bold", ALIGN_UL, debugPositionX, debugTextPositionY, debugPositionX, debugTextPositionY, 200, 200, 0, "drag and drop target node:", 0, 0, NULL, qfalse, LONGLINES_PRETTYCHOP);
			debugTextPositionY += 15;
			MN_HilightNode(targetNode, green);
		}
	}
	R_ColorBlend(NULL);
}
#endif


static void MN_CheckTooltipDelay (menuNode_t *node, menuTimer_t *timer)
{
	tooltipVisible = qtrue;
	MN_TimerStop(timer);
}

static void MN_DrawNode (menuNode_t *node)
{
	menuNode_t *child;

	/* skip invisible, virtual, and undrawable nodes */
	if (node->invis || node->behaviour->isVirtual)
		return;
	/* if construct */
	if (!MN_CheckVisibility(node))
		return;

	/** @todo remove it when its possible:
	 * we can create a 'box' node with this properties,
	 * but we often dont need it */
	/* check node size x and y value to check whether they are zero */
	if (node->size[0] && node->size[1]) {
		vec2_t pos;
		MN_GetNodeAbsPos(node, pos);
		if (node->bgcolor[3] != 0)
			R_DrawFill(pos[0], pos[1], node->size[0], node->size[1], 0, node->bgcolor);

		if (node->border && node->bordercolor[3] != 0) {
			R_DrawRect(pos[0], pos[1], node->size[0], node->size[1],
				node->bordercolor, node->border, 0xFFFF);
		}
	}

	/* draw the node */
	if (node->behaviour->draw) {
		node->behaviour->draw(node);
	}

	/* draw all child */
	for (child = node->firstChild; child; child = child->next) {
		MN_DrawNode(child);
	}
}

/**
 * @brief Draws the menu stack
 * @sa SCR_UpdateScreen
 */
void MN_Draw (void)
{
	menuNode_t *hoveredNode;
	menuNode_t *menu;
	int pos;
	qboolean mouseMoved = qfalse;

	MN_HandleTimers();

	assert(mn.menuStackPos >= 0);

	mouseMoved = MN_CheckMouseMove();
	hoveredNode = MN_GetHoveredNode();

	/* handle delay time for tooltips */
	if (mouseMoved && tooltipVisible) {
		MN_TimerStop(tooltipTimer);
		tooltipVisible = qfalse;
	} else if (!tooltipVisible && !mouseMoved && !tooltipTimer->isRunning && mn_show_tooltips->integer && hoveredNode) {
		MN_TimerStart(tooltipTimer);
	}

	/* under a fullscreen, menu should not be visible */
	pos = MN_GetLastFullScreenWindow();
	if (pos < 0)
		return;

	/* draw all visible menus */
	for (;pos < mn.menuStackPos; pos++) {
		menu = mn.menuStack[pos];

		/* update the layout */
		menu->behaviour->doLayout(menu);

		drawOverNode = NULL;

		MN_DrawNode(menu);

		/* draw a node over the menu */
		if (drawOverNode && drawOverNode->behaviour->drawOverMenu) {
			drawOverNode->behaviour->drawOverMenu(drawOverNode);
		}
	}

	/* draw tooltip */
	if (hoveredNode && tooltipVisible && !MN_DNDIsDragging()) {
		if (hoveredNode->behaviour->drawTooltip) {
			hoveredNode->behaviour->drawTooltip(hoveredNode, mousePosX, mousePosY);
		} else {
			MN_Tooltip(hoveredNode, mousePosX, mousePosY);
		}
	}

	/* draw a special notice */
	menu = MN_GetActiveMenu();
	if (cl.time < cl.msgTime) {
		if (menu && (menu->u.window.noticePos[0] || menu->u.window.noticePos[1]))
			MN_DrawNotice(menu->u.window.noticePos[0], menu->u.window.noticePos[1]);
		else
			MN_DrawNotice(500, 110);
	}

#ifdef DEBUG
	/* debug information */
	if (mn_debug->integer == 2) {
		MN_DrawDebugMenuNodeNames();
	}
#endif
}

void MN_DrawCursor (void)
{
	MN_DrawDragAndDrop(mousePosX, mousePosY);
}

/**
 * @brief Displays a message over all menus.
 * @sa HUD_DisplayMessage
 * @param[in] time is a ms values
 * @param[in] text text is already translated here
 */
void MN_DisplayNotice (const char *text, int time)
{
	cl.msgTime = cl.time + time;
	Q_strncpyz(cl.msgText, text, sizeof(cl.msgText));
}

void MN_InitDraw (void)
{
#ifdef DEBUG
	mn_debug = Cvar_Get("debug_menu", "0", 0, "Prints node names for debugging purposes - valid values are 1 and 2");
#endif
	mn_show_tooltips = Cvar_Get("mn_show_tooltips", "1", CVAR_ARCHIVE, "Show tooltips in menus and hud");
	tooltipTimer = MN_AllocTimer(NULL, TOOLTIP_DELAY, MN_CheckTooltipDelay);
}
