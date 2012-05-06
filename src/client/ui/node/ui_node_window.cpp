/**
 * @file
 * @todo move it as an inheritance of panel behaviour?
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../ui_main.h"
#include "../ui_parse.h"
#include "../ui_font.h"
#include "../ui_nodes.h"
#include "../ui_internal.h"
#include "../ui_render.h"
#include "../ui_sprite.h"
#include "ui_node_window.h"
#include "ui_node_panel.h"
#include "ui_node_custombutton.h"
#include "ui_node_abstractnode.h"

#include "../../client.h" /* gettext _() */

#define EXTRADATA_TYPE windowExtraData_t
#define EXTRADATA(node) UI_EXTRADATA(node, EXTRADATA_TYPE)
#define EXTRADATACONST(node) UI_EXTRADATACONST(node, EXTRADATA_TYPE)

/* constants defining all tile of the texture */

#define LEFT_WIDTH 20
#define MID_WIDTH 1
#define RIGHT_WIDTH 19

#define TOP_HEIGHT 46
#define MID_HEIGHT 1
#define BOTTOM_HEIGHT 19

#define MARGE 3

static const int CONTROLS_IMAGE_DIMENSIONS = 25;
static const int CONTROLS_PADDING = 18;

static const int windowTemplate[] = {
	LEFT_WIDTH, MID_WIDTH, RIGHT_WIDTH,
	TOP_HEIGHT, MID_HEIGHT, BOTTOM_HEIGHT,
	MARGE
};

static const vec4_t modalBackground = {0, 0, 0, 0.6};
static const vec4_t anamorphicBorder = {0, 0, 0, 1};

/**
 * @brief Get a node from child index
 * @return A child node by his name, else NULL
 */
uiNode_t* UI_WindowNodeGetIndexedChild (uiNode_t* const node, const char* childName)
{
	node_index_t *a;
	unsigned int hash;

	hash = Com_HashKey(childName, INDEXEDCHILD_HASH_SIZE);
	for (a = EXTRADATA(node).index_hash[hash]; a; a = a->hash_next) {
		if (Q_streq(childName, a->node->name)) {
			return a->node;
		}
	}
	return NULL;
}

/**
 * @brief Add a node to the child index
 */
qboolean UI_WindowNodeAddIndexedNode (uiNode_t* const node, uiNode_t* const child)
{
	node_index_t *a;
	unsigned int hash;

	hash = Com_HashKey(child->name, INDEXEDCHILD_HASH_SIZE);
	for (a = EXTRADATA(node).index_hash[hash]; a; a = a->hash_next) {
		if (Q_streq(child->name, a->node->name)) {
			/** @todo display a warning, we must not override a node name here */
			break;
		}
	}

	if (!a) {
		a = Mem_PoolAllocType(node_index_t, ui_sysPool);
		a->next = EXTRADATA(node).index;
		a->hash_next = EXTRADATA(node).index_hash[hash];
		EXTRADATA(node).index_hash[hash] = a;
		EXTRADATA(node).index = a;
	}

	return qfalse;
}

/**
 * @brief Remove a node from the child index
 */
qboolean UI_WindowNodeRemoveIndexedNode (uiNode_t* const node, uiNode_t* const child)
{
	/** @todo FIXME implement it */
	return qfalse;
}

/**
 * @brief Check if a window is fullscreen or not
 */
qboolean UI_WindowIsFullScreen (const uiNode_t* const node)
{
	assert(UI_NodeInstanceOf(node, "window"));
	return EXTRADATACONST(node).isFullScreen;
}

void uiWindowNode::draw (uiNode_t *node)
{
	const char* image;
	const char* text;
	vec2_t pos;
	const char *font = UI_GetFontFromNode(node);

	UI_GetNodeAbsPos(node, pos);

	/* black border for anamorphic mode */
	/** @todo it should be over the window */
	/** @todo why not using glClear here with glClearColor set to black here? */
	if (UI_WindowIsFullScreen(node)) {
		/* top */
		if (pos[1] != 0)
			UI_DrawFill(0, 0, viddef.virtualWidth, pos[1], anamorphicBorder);
		/* left-right */
		if (pos[0] != 0)
			UI_DrawFill(0, pos[1], pos[0], node->box.size[1], anamorphicBorder);
		if (pos[0] + node->box.size[0] < viddef.virtualWidth) {
			const int width = viddef.virtualWidth - (pos[0] + node->box.size[0]);
			UI_DrawFill(viddef.virtualWidth - width, pos[1], width, node->box.size[1], anamorphicBorder);
		}
		/* bottom */
		if (pos[1] + node->box.size[1] < viddef.virtualHeight) {
			const int height = viddef.virtualHeight - (pos[1] + node->box.size[1]);
			UI_DrawFill(0, viddef.virtualHeight - height, viddef.virtualWidth, height, anamorphicBorder);
		}
	}

	/* darker background if last window is a modal */
	if (EXTRADATA(node).modal && ui_global.windowStack[ui_global.windowStackPos - 1] == node)
		UI_DrawFill(0, 0, viddef.virtualWidth, viddef.virtualHeight, modalBackground);

	/* draw the background */
	image = UI_GetReferenceString(node, node->image);
	if (image)
		UI_DrawPanel(pos, node->box.size, image, 0, 0, windowTemplate);

	if (EXTRADATA(node).background) {
		UI_DrawSpriteInBox(qfalse, EXTRADATA(node).background, SPRITE_STATUS_NORMAL, pos[0], pos[1], node->box.size[0], node->box.size[1]);
	}

	/* draw the title */
	text = UI_GetReferenceString(node, node->text);
	if (text)
		UI_DrawStringInBox(font, ALIGN_CC, pos[0] + node->padding, pos[1] + node->padding, node->box.size[0] - node->padding - node->padding, TOP_HEIGHT + 10 - node->padding - node->padding, text, LONGLINES_PRETTYCHOP);

	/* embedded timer */
	if (EXTRADATA(node).onTimeOut && EXTRADATA(node).timeOut) {
		if (EXTRADATA(node).lastTime == 0)
			EXTRADATA(node).lastTime = CL_Milliseconds();
		if (EXTRADATA(node).lastTime + EXTRADATA(node).timeOut < CL_Milliseconds()) {
			/* allow to reset timeOut on the event, and restart it, with an uptodate lastTime */
			EXTRADATA(node).lastTime = 0;
			Com_DPrintf(DEBUG_CLIENT, "UI_WindowNodeDraw: timeout for node '%s'\n", node->name);
			UI_ExecuteEventActions(node, EXTRADATA(node).onTimeOut);
		}
	}
}

void uiWindowNode::doLayout (uiNode_t *node)
{
	if (!node->invalidated)
		return;

	/* use a the space */
	if (EXTRADATA(node).fill) {
		if (node->box.size[0] != viddef.virtualWidth) {
			node->box.size[0] = viddef.virtualWidth;
		}
		if (node->box.size[1] != viddef.virtualHeight) {
			node->box.size[1] = viddef.virtualHeight;
		}
	}

	/* move fullscreen window on the center of the screen */
	if (UI_WindowIsFullScreen(node)) {
		node->box.pos[0] = (int) ((viddef.virtualWidth - node->box.size[0]) / 2);
		node->box.pos[1] = (int) ((viddef.virtualHeight - node->box.size[1]) / 2);
	}

	/** @todo check and fix here window outside the screen */

	if (EXTRADATA(node).starLayout) {
		UI_StarLayout(node);
	}

	/* super */
	uiLocatedNode::doLayout(node);
}

/**
 * @brief Called when we init the node on the screen
 * @todo we can move generic code into abstract node
 */
void uiWindowNode::onWindowOpened (uiNode_t *node, linkedList_t *params)
{
	uiNode_t *child;

	/* init the embeded timer */
	EXTRADATA(node).lastTime = CL_Milliseconds();

	/* init child */
	for (child = node->firstChild; child; child = child->next) {
		UI_Node_WindowOpened(child, NULL);
	}

	/* script callback */
	if (EXTRADATA(node).onWindowOpened)
		UI_ExecuteEventActionsEx(node, EXTRADATA(node).onWindowOpened, params);

	UI_Invalidate(node);
}

/**
 * @brief Called when we close the node on the screen
 * @todo we can move generic code into abstract node
 */
void uiWindowNode::onWindowClosed (uiNode_t *node)
{
	uiNode_t *child;

	/* close child */
	for (child = node->firstChild; child; child = child->next) {
		UI_Node_WindowClosed(child);
	}

	/* script callback */
	if (EXTRADATA(node).onWindowClosed)
		UI_ExecuteEventActions(node, EXTRADATA(node).onWindowClosed);
}

/**
 * @brief Called at the begin of the load from script
 */
void uiWindowNode::onLoading (uiNode_t *node)
{
	node->box.size[0] = VID_NORM_WIDTH;
	node->box.size[1] = VID_NORM_HEIGHT;
	node->font = "f_big";
	node->padding = 5;
}

/**
 * @brief Called at the end of the load from script
 */
void uiWindowNode::onLoaded (uiNode_t *node)
{
	static const char* closeCommand = "ui_close <path:root>;";

	/* create a drag zone, if it is requested */
	if (EXTRADATA(node).dragButton) {
		uiNode_t *control = UI_AllocNode("move_window_button", "controls", node->dynamic);
		control->root = node;
		control->image = NULL;
		/** @todo Once @c image_t is known on the client, use @c image->width resp. @c image->height here */
		control->box.size[0] = node->box.size[0];
		control->box.size[1] = TOP_HEIGHT;
		control->box.pos[0] = 0;
		control->box.pos[1] = 0;
		control->tooltip = _("Drag to move window");
		UI_AppendNode(node, control);
	}

	/* create a close button, if it is requested */
	if (EXTRADATA(node).closeButton) {
		uiNode_t *button = UI_AllocNode("close_window_button", "button", node->dynamic);
		const int positionFromRight = CONTROLS_PADDING;

		button->root = node;
		UI_NodeSetProperty(button, UI_GetPropertyFromBehaviour(button->behaviour, "icon"), "icons/system_close");
		/** @todo Once @c image_t is known on the client, use @c image->width resp. @c image->height here */
		button->box.size[0] = CONTROLS_IMAGE_DIMENSIONS;
		button->box.size[1] = CONTROLS_IMAGE_DIMENSIONS;
		button->box.pos[0] = node->box.size[0] - positionFromRight - button->box.size[0];
		button->box.pos[1] = CONTROLS_PADDING;
		button->tooltip = _("Close the window");
		button->onClick = UI_AllocStaticCommandAction(closeCommand);
		UI_AppendNode(node, button);
	}

	EXTRADATA(node).isFullScreen = node->box.size[0] == VID_NORM_WIDTH
			&& node->box.size[1] == VID_NORM_HEIGHT;

	if (EXTRADATA(node).starLayout)
		UI_Invalidate(node);

#ifdef DEBUG
	if (node->box.size[0] < LEFT_WIDTH + MID_WIDTH + RIGHT_WIDTH || node->box.size[1] < TOP_HEIGHT + MID_HEIGHT + BOTTOM_HEIGHT)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s' too small. It can create graphical bugs\n", node->name);
#endif
}

void uiWindowNode::clone (const uiNode_t *source, uiNode_t *clone)
{
	/* clean up index */
	EXTRADATA(clone).index = NULL;
	OBJZERO(EXTRADATA(clone).index_hash);
}

/**
 * @brief Get the noticePosition from a window node.
 * @param node A window node
 * @return A position, else NULL if no notice position
 */
vec_t *UI_WindowNodeGetNoticePosition(uiNode_t* node)
{
	if (Vector2Empty(EXTRADATA(node).noticePos))
		return NULL;
	return EXTRADATA(node).noticePos;
}

/**
 * @brief True if the window is a drop down.
 * @param node A window node
 * @return True if the window is a drop down.
 */
qboolean UI_WindowIsDropDown(uiNode_t const* const node)
{
	return EXTRADATACONST(node).dropdown;
}

/**
 * @brief True if the window is a modal.
 * @param node A window node
 * @return True if the window is a modal.
 */
qboolean UI_WindowIsModal(uiNode_t const* const node)
{
	return EXTRADATACONST(node).modal;
}

/**
 * @brief Add a key binding to a window node.
 * Window node store key bindings for his node child.
 * @param node A window node
 * @param binding Key binding to link with the window (structure should not be already linked somewhere)
 * @todo Rework that function to remove possible wrong use of that function
 */
void UI_WindowNodeRegisterKeyBinding (uiNode_t* node, uiKeyBinding_t *binding)
{
	assert(UI_NodeInstanceOf(node, "window"));
	binding->next = EXTRADATA(node).keyList;
	EXTRADATA(node).keyList = binding;
}

const uiKeyBinding_t *binding;

/**
 * @brief Search a a key binding from a window node.
 * Window node store key bindings for his node child.
 * @param node A window node
 * @param key A key code, either K_ value or lowercase ascii
 */
uiKeyBinding_t *UI_WindowNodeGetKeyBinding (uiNode_t const* const node, unsigned int key)
{
	uiKeyBinding_t *binding = EXTRADATACONST(node).keyList;
	assert(UI_NodeInstanceOf(node, "window"));
	while (binding) {
		if (binding->key == key)
			break;
		binding = binding->next;
	}
	return binding;
}

void UI_RegisterWindowNode (uiBehaviour_t *behaviour)
{
	behaviour->name = "window";
	behaviour->manager = new uiWindowNode();
	behaviour->extraDataSize = sizeof(EXTRADATA_TYPE);

	/* Texture to use. The texture is a cut of 9 portions
	 * (left, middle, right x top, middle, bottom). Between all this elements,
	 * we use a margin of 3 pixels (purple mark into the sample).
	 * Graphically we see only a 1 pixel margin, because, for a problem of
	 * lossy compression of texture it's not nice to have a pure transparent
	 * pixel near the last colored one, when we cut or stretch textures.
	 * @image html http://ufoai.org/wiki/images/Popup_alpha_tile.png
	 */
	UI_RegisterOveridedNodeProperty(behaviour, "image");

	/* In windows where notify messages appear (like e.g. the video options window when you have to restart the game until
	 * the settings take effects) you can define the position of those messages with this option. */
	UI_RegisterExtradataNodeProperty(behaviour, "noticepos", V_POS, windowExtraData_t, noticePos);
	/* Create subnode allowing to move the window when we click on the header. Updating this attribute at the runtime will change nothing. */
	UI_RegisterExtradataNodeProperty(behaviour, "dragbutton", V_CPPBOOL, windowExtraData_t, dragButton);
	/* Add a button on the top right the window to close it. Updating this attribute at the runtime will change nothing. */
	UI_RegisterExtradataNodeProperty(behaviour, "closebutton", V_CPPBOOL, windowExtraData_t, closeButton);
	/* If true, the user can't select something outside the modal window. He must first close the window. */
	UI_RegisterExtradataNodeProperty(behaviour, "modal", V_CPPBOOL, windowExtraData_t, modal);
	/* If true, if the user click outside the window, it will close it. */
	UI_RegisterExtradataNodeProperty(behaviour, "dropdown", V_CPPBOOL, windowExtraData_t, dropdown);
	/* If true, the user can't use ''ESC'' key to close the window. */
	UI_RegisterExtradataNodeProperty(behaviour, "preventtypingescape", V_CPPBOOL, windowExtraData_t, preventTypingEscape);
	/* If true, the window is filled according to the widescreen. */
	UI_RegisterExtradataNodeProperty(behaviour, "fill", V_CPPBOOL, windowExtraData_t, fill);
	/* If true, when the window size change, the window content position is updated according to the "star" layout.
	 * @todo Need more documentation.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "starlayout", V_CPPBOOL, windowExtraData_t, starLayout);

	/* This property control milliseconds between each calls of <code>onEvent</code>.
	 * If value is 0 (the default value) nothing is called. We can change the
	 * value at the runtime.
	 */
	UI_RegisterExtradataNodeProperty(behaviour, "timeout", V_INT,windowExtraData_t, timeOut);

	/* Invoked when the window is added to the rendering stack. */
	UI_RegisterExtradataNodeProperty(behaviour, "onWindowOpened", V_UI_ACTION, windowExtraData_t, onWindowOpened);
	/* Invoked when the window is removed from the rendering stack. */
	UI_RegisterExtradataNodeProperty(behaviour, "onWindowClosed", V_UI_ACTION, windowExtraData_t, onWindowClosed);
	/* Invoked periodically. See <code>timeout</code>. */
	UI_RegisterExtradataNodeProperty(behaviour, "onEvent", V_UI_ACTION, windowExtraData_t, onTimeOut);
	/* Invoked after all UI scripts are loaded. */
	UI_RegisterExtradataNodeProperty(behaviour, "onScriptLoaded", V_UI_ACTION, windowExtraData_t, onScriptLoaded);

	/* Sprite used to display the background */
	UI_RegisterExtradataNodeProperty(behaviour, "background", V_UI_SPRITEREF, EXTRADATA_TYPE, background);
}
