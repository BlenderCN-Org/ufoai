/**
 * @file
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

#ifndef CLIENT_UI_UI_NODE_WINDOW_H
#define CLIENT_UI_UI_NODE_WINDOW_H

#include "../../../shared/mathlib.h"
#include "ui_node_abstractnode.h"

/* prototype */
struct uiNode_t;
struct uiAction_s;
struct uiBehaviour_t;
struct uiKeyBinding_s;

class uiWindowNode : public uiLocatedNode {
	void draw(uiNode_t* node) OVERRIDE;
	void doLayout(uiNode_t* node) OVERRIDE;
	void onLoading(uiNode_t* node) OVERRIDE;
	void onLoaded(uiNode_t* node) OVERRIDE;
	void onWindowOpened(uiNode_t* node, linkedList_t *params) OVERRIDE;
	void onWindowClosed(uiNode_t* node) OVERRIDE;
	void clone(uiNode_t const* source, uiNode_t* clone) OVERRIDE;
};

#define INDEXEDCHILD_HASH_SIZE 32

typedef struct node_index_s {
	uiNode_t* node;
	struct node_index_s *hash_next;
	struct node_index_s *next;
} node_index_t;

/**
 * @brief extradata for the window node
 */
typedef struct {
	int eventTime;
	vec2_t noticePos; 				/**< the position where the cl.msgText messages are rendered */
	bool dragButton;			/**< If true, we init the window with a header to move it */
	bool closeButton;			/**< If true, we init the window with a header button to close it */
	bool preventTypingEscape;	/**< If true, we can't use ESC button to close the window */
	bool modal;					/**< If true, we can't click outside the window */
	bool dropdown;				/**< very special property force the window to close if we click outside */
	bool isFullScreen;			/**< Internal data to allow fullscreen windows without the same size */
	bool fill;					/**< If true, use all the screen space allowed */
	bool starLayout;			/**< If true, do a star layout (move child into a corner according to his num) */

	int timeOut;					/**< ms value until calling onTimeOut (see cl.time) */
	int lastTime;					/**< when a window was pushed this value is set to cl.time */

	uiNode_t* parent;	/**< to create child window */

	struct uiKeyBinding_s *keyList;	/** list of key binding */

	/** @todo think about converting it to action instead of node */
	struct uiAction_s *onWindowOpened; 	/**< Invoked when the window is added to the rendering stack */
	struct uiAction_s *onWindowClosed;	/**< Invoked when the window is removed from the rendering stack */
	struct uiAction_s *onTimeOut;	/**< Call when the own timer of the window out */
	struct uiAction_s *onScriptLoaded;	/**< Invoked after all UI scripts are loaded */

	node_index_t *index;
	node_index_t *index_hash[INDEXEDCHILD_HASH_SIZE];

	/** Sprite used as a background */
	uiSprite_t *background;
} windowExtraData_t;

void UI_RegisterWindowNode(uiBehaviour_t *behaviour);

qboolean UI_WindowIsFullScreen(uiNode_t const* window);
qboolean UI_WindowIsDropDown(uiNode_t const* window);
qboolean UI_WindowIsModal(uiNode_t const* window);
void UI_WindowNodeRegisterKeyBinding(uiNode_t* window, struct uiKeyBinding_s *binding);
struct uiKeyBinding_s *UI_WindowNodeGetKeyBinding(uiNode_t const* node, unsigned int key);
vec_t *UI_WindowNodeGetNoticePosition(uiNode_t* node);
/* child index */
uiNode_t* UI_WindowNodeGetIndexedChild(uiNode_t* node, const char* childName);
qboolean UI_WindowNodeAddIndexedNode(uiNode_t* node, uiNode_t* child);
qboolean UI_WindowNodeRemoveIndexedNode(uiNode_t* node, uiNode_t* child);

#endif
