/**
 * @file m_node_window.c
 * @note this file is about menu function. Its not yet a real node,
 * but it may become one. Think the code like that will help to merge menu and node.
 * @note It used 'window' instead of 'menu', because a menu is not this king of widget
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

#include "../m_main.h"
#include "../m_parse.h"
#include "../m_nodes.h"
#include "../m_internal.h"
#include "../m_render.h"
#include "m_node_window.h"
#include "m_node_panel.h"
#include "m_node_abstractnode.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA(node) node->u.window

/* constants defining all tile of the texture */

#define LEFT_WIDTH 20
#define MID_WIDTH 1
#define RIGHT_WIDTH 19

#define TOP_HEIGHT 46
#define MID_HEIGHT 1
#define BOTTOM_HEIGHT 19

#define MARGE 3

static const nodeBehaviour_t const *localBehaviour;

static const int CONTROLS_IMAGE_DIMENSIONS = 17;
static const int CONTROLS_PADDING = 22;
static const int CONTROLS_SPACING = 5;

static const int windowTemplate[] = {
	LEFT_WIDTH, MID_WIDTH, RIGHT_WIDTH,
	TOP_HEIGHT, MID_HEIGHT, BOTTOM_HEIGHT,
	MARGE
};

static const vec4_t modalBackground = {0, 0, 0, 0.6};
static const vec4_t anamorphicBorder = {0, 0, 0, 1};

/**
 * @brief Check if a window is fullscreen or not
 */
qboolean MN_WindowIsFullScreen (menuNode_t* const node)
{
	assert(MN_NodeInstanceOf(node, "window"));
	return EXTRADATA(node).isFullScreen;
}

static void MN_WindowNodeDraw (menuNode_t *node)
{
	const char* image;
	const char* text;
	vec2_t pos;

	MN_GetNodeAbsPos(node, pos);

	/* black border for anamorphic mode */
	/** @todo it should be over the window */
	/** @todo why not using glClear here with glClearColor set to black here? */
	if (MN_WindowIsFullScreen(node)) {
		/* top */
		if (pos[1] != 0)
			MN_DrawFill(0, 0, viddef.virtualWidth, pos[1], anamorphicBorder);
		/* left-right */
		if (pos[0] != 0)
			MN_DrawFill(0, pos[1], pos[0], node->size[1], anamorphicBorder);
		if (pos[0] + node->size[0] < viddef.virtualWidth) {
			const int width = viddef.virtualWidth - (pos[0] + node->size[0]);
			MN_DrawFill(viddef.virtualWidth - width, pos[1], width, node->size[1], anamorphicBorder);
		}
		/* bottom */
		if (pos[1] + node->size[1] < viddef.virtualHeight) {
			const int height = viddef.virtualHeight - (pos[1] + node->size[1]);
			MN_DrawFill(0, viddef.virtualHeight - height, viddef.virtualWidth, height, anamorphicBorder);
		}
	}

	/* darker background if last window is a modal */
	if (EXTRADATA(node).modal && mn.windowStack[mn.windowStackPos - 1] == node)
		MN_DrawFill(0, 0, viddef.virtualWidth, viddef.virtualHeight, modalBackground);

	/* draw the background */
	image = MN_GetReferenceString(node, node->image);
	if (image)
		MN_DrawPanel(pos, node->size, image, 0, 0, windowTemplate);

	/* draw the title */
	text = MN_GetReferenceString(node, node->text);
	if (text)
		MN_DrawStringInBox(node, ALIGN_CC, pos[0] + node->padding, pos[1] + node->padding, node->size[0] - node->padding - node->padding, TOP_HEIGHT + 10 - node->padding - node->padding, text, LONGLINES_PRETTYCHOP);

	/* embedded timer */
	if (EXTRADATA(node).onTimeOut && node->timeOut) {
		if (node->lastTime == 0)
			node->lastTime = cls.realtime;
		if (node->lastTime + node->timeOut < cls.realtime) {
			node->lastTime = 0;	/**< allow to reset timeOut on the event, and restart it, with an uptodate lastTime */
			Com_DPrintf(DEBUG_CLIENT, "MN_DrawMenus: timeout for node '%s'\n", node->name);
			MN_ExecuteEventActions(node, EXTRADATA(node).onTimeOut);
		}
	}
}

/**
 * @brief map for star layout from num to align
 */
static const align_t starlayoutmap[] = {
	ALIGN_UL,
	ALIGN_UC,
	ALIGN_UR,
	ALIGN_CL,
	ALIGN_CC,
	ALIGN_CR,
	ALIGN_LL,
	ALIGN_LC,
	ALIGN_LR,
};

/**
 * @brief Do a star layout with child according to there num
 * @note 1=top-left 2=top-middle 3=top-right
 * 4=middle-left 5=middle-middle 6=middle-right
 * 7=bottom-left 8=bottom-middle 9=bottom-right
 * 10=fill
 * @todo Move it into panel node when its possible
 */
static void MN_WindowNodeDoStarLayout (menuNode_t *node)
{
	menuNode_t *child;
	for (child = node->firstChild; child; child = child->next) {
		vec2_t source;
		vec2_t destination;
		align_t align;

		if (child->num <= 0 || child->num > 10)
			continue;

		if (child->num == 10) {
			child->pos[0] = 0;
			child->pos[1] = 0;
			child->size[0] = node->size[0];
			child->size[1] = node->size[1];
			MN_Invalidate(child);
			continue;
		}

		align = starlayoutmap[child->num - 1];
		MN_NodeGetPoint(node, destination, align);
		MN_NodeRelativeToAbsolutePoint(node, destination);
		MN_NodeGetPoint(child, source, align);
		MN_NodeRelativeToAbsolutePoint(child, source);
		child->pos[0] += destination[0] - source[0];
		child->pos[1] += destination[1] - source[1];
	}
}

static void MN_WindowNodeDoLayout (menuNode_t *node)
{
	qboolean resized = qfalse;

	if (!node->invalidated)
		return;

	/* use a the space */
	if (EXTRADATA(node).fill) {
		if (node->size[0] != viddef.virtualWidth) {
			node->size[0] = viddef.virtualWidth;
			resized = qtrue;
		}
		if (node->size[1] != viddef.virtualHeight) {
			node->size[1] = viddef.virtualHeight;
			resized = qtrue;
		}
	}

	/* move fullscreen menu on the center of the screen */
	if (MN_WindowIsFullScreen(node)) {
		node->pos[0] = (int) ((viddef.virtualWidth - node->size[0]) / 2);
		node->pos[1] = (int) ((viddef.virtualHeight - node->size[1]) / 2);
	}

	/** @todo check and fix here window outside the screen */

	if (resized) {
		if (EXTRADATA(node).starLayout) {
			MN_WindowNodeDoStarLayout(node);
		}
	}

	/* super */
	localBehaviour->super->doLayout(node);
}

/**
 * @brief Called when we init the node on the screen
 * @todo we can move generic code into abstract node
 */
static void MN_WindowNodeInit (menuNode_t *node)
{
	menuNode_t *child;

	/* init the embeded timer */
	node->lastTime = cls.realtime;

	/* init child */
	for (child = node->firstChild; child; child = child->next) {
		if (child->behaviour->init) {
			child->behaviour->init(child);
		}
	}

	/* script callback */
	if (EXTRADATA(node).onInit)
		MN_ExecuteEventActions(node, EXTRADATA(node).onInit);

	MN_Invalidate(node);
}

/**
 * @brief Called at the begin of the load from script
 */
static void MN_WindowNodeLoading (menuNode_t *node)
{
	node->size[0] = VID_NORM_WIDTH;
	node->size[1] = VID_NORM_HEIGHT;
	node->font = "f_big";
	node->padding = 5;
}

void MN_WindowNodeSetRenderNode (menuNode_t *node, menuNode_t *renderNode)
{
	if (!MN_NodeInstanceOf(node, "window")) {
		Com_Printf("MN_WindowNodeSetRenderNode: '%s' node is not an 'window'.\n", MN_GetPath(node));
		return;
	}

	if (EXTRADATA(node).renderNode) {
		Com_Printf("MN_WindowNodeSetRenderNode: second render node ignored (\"%s\")\n", MN_GetPath(renderNode));
		return;
	}

	EXTRADATA(node).renderNode = renderNode;
}

/**
 * @brief Called at the end of the load from script
 */
static void MN_WindowNodeLoaded (menuNode_t *node)
{
	static char* closeCommand = "mn_close <path:root>;";

	/* if it need, construct the drag button */
	if (EXTRADATA(node).dragButton) {
		menuNode_t *control = MN_AllocStaticNode("controls");
		Q_strncpyz(control->name, "move_window_button", sizeof(control->name));
		control->root = node;
		control->image = NULL;
		/** @todo Once @c image_t is known on the client, use @c image->width resp. @c image->height here */
		control->size[0] = node->size[0];
		control->size[1] = TOP_HEIGHT;
		control->pos[0] = 0;
		control->pos[1] = 0;
		control->tooltip = _("Drag to move window");
		MN_AppendNode(node, control);
	}

	/* if the menu should have a close button, add it here */
	if (EXTRADATA(node).closeButton) {
		menuNode_t *button = MN_AllocStaticNode("pic");
		const int positionFromRight = CONTROLS_PADDING;
		Q_strncpyz(button->name, "close_window_button", sizeof(button->name));
		button->root = node;
		button->image = "ui/close";
		/** @todo Once @c image_t is known on the client, use @c image->width resp. @c image->height here */
		button->size[0] = CONTROLS_IMAGE_DIMENSIONS;
		button->size[1] = CONTROLS_IMAGE_DIMENSIONS;
		button->pos[0] = node->size[0] - positionFromRight - button->size[0];
		button->pos[1] = CONTROLS_PADDING;
		button->tooltip = _("Close the window");
		button->onClick = MN_AllocStaticCommandAction(closeCommand);
		MN_AppendNode(node, button);
	}

	if (node->size[0] == VID_NORM_WIDTH && node->size[1] == VID_NORM_HEIGHT) {
		EXTRADATA(node).isFullScreen = qtrue;
	}

#ifdef DEBUG
	if (node->size[0] < LEFT_WIDTH + MID_WIDTH + RIGHT_WIDTH || node->size[1] < TOP_HEIGHT + MID_HEIGHT + BOTTOM_HEIGHT)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical bugs\n", node->name);
#endif
}

static void MN_WindowNodeClone (const menuNode_t *source, menuNode_t *clone)
{
	/** @todo anyway we should remove soon renderNode */
	if (source->u.window.renderNode != NULL) {
		Com_Printf("MN_WindowNodeClone: Do not inherite window using a render node. Render node ignored (\"%s\")\n", MN_GetPath(clone));
		clone->u.window.renderNode = NULL;
	}
}

/**
 * @brief Valid properties for a window node (called yet 'menu')
 */
static const value_t windowNodeProperties[] = {
	{"noticepos", V_POS, MN_EXTRADATA_OFFSETOF(windowExtraData_t, noticePos), MEMBER_SIZEOF(windowExtraData_t, noticePos)},
	{"dragbutton", V_BOOL, MN_EXTRADATA_OFFSETOF(windowExtraData_t, dragButton), MEMBER_SIZEOF(windowExtraData_t, dragButton)},
	{"closebutton", V_BOOL, MN_EXTRADATA_OFFSETOF(windowExtraData_t, closeButton), MEMBER_SIZEOF(windowExtraData_t, closeButton)},
	{"modal", V_BOOL, MN_EXTRADATA_OFFSETOF(windowExtraData_t, modal), MEMBER_SIZEOF(windowExtraData_t, modal)},
	{"dropdown", V_BOOL, MN_EXTRADATA_OFFSETOF(windowExtraData_t, dropdown), MEMBER_SIZEOF(windowExtraData_t, dropdown)},
	{"preventtypingescape", V_BOOL, MN_EXTRADATA_OFFSETOF(windowExtraData_t, preventTypingEscape), MEMBER_SIZEOF(windowExtraData_t, preventTypingEscape)},
	{"fill", V_BOOL, MN_EXTRADATA_OFFSETOF(windowExtraData_t, fill), MEMBER_SIZEOF(windowExtraData_t, fill)},
	{"starlayout", V_BOOL, MN_EXTRADATA_OFFSETOF(windowExtraData_t, starLayout), MEMBER_SIZEOF(windowExtraData_t, starLayout)},
	{"timeout", V_INT, offsetof(menuNode_t, timeOut), MEMBER_SIZEOF(menuNode_t, timeOut)},

	{"oninit", V_UI_ACTION, MN_EXTRADATA_OFFSETOF(windowExtraData_t, onInit), MEMBER_SIZEOF(windowExtraData_t, onInit)},
	{"onclose", V_UI_ACTION, MN_EXTRADATA_OFFSETOF(windowExtraData_t, onClose), MEMBER_SIZEOF(windowExtraData_t, onClose)},
	{"onevent", V_UI_ACTION, MN_EXTRADATA_OFFSETOF(windowExtraData_t, onTimeOut), MEMBER_SIZEOF(windowExtraData_t, onTimeOut)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterWindowNode (nodeBehaviour_t *behaviour)
{
	localBehaviour = behaviour;
	behaviour->name = "window";
	behaviour->loading = MN_WindowNodeLoading;
	behaviour->loaded = MN_WindowNodeLoaded;
	behaviour->init = MN_WindowNodeInit;
	behaviour->draw = MN_WindowNodeDraw;
	behaviour->doLayout = MN_WindowNodeDoLayout;
	behaviour->clone = MN_WindowNodeClone;
	behaviour->properties = windowNodeProperties;
	behaviour->extraDataSize = sizeof(windowExtraData_t);
}
