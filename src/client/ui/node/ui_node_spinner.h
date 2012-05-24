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


#ifndef CLIENT_UI_UI_NODE_SPINNER_H
#define CLIENT_UI_UI_NODE_SPINNER_H

#include "ui_node_abstractvalue.h"

class uiSpinnerNode : public uiAbstractValueNode {
	void draw(uiNode_t* node) OVERRIDE;
	void onLoading(uiNode_t* node) OVERRIDE;
	void onMouseDown(uiNode_t* node, int x, int y, int button) OVERRIDE;
	void onMouseUp(uiNode_t* node, int x, int y, int button) OVERRIDE;
	void onCapturedMouseLost(uiNode_t* node) OVERRIDE;
	bool onScroll(uiNode_t* node, int deltaX, int deltaY) OVERRIDE;
public:
	void repeat (uiNode_t *node, struct uiTimer_s *timer);
protected:
	bool isPositionIncrease(uiNode_t *node, int x, int y);
	bool step (uiNode_t *node, bool down);
};

struct spinnerExtraData_t {
	abstractValueExtraData_s super;

	uiSprite_t *background;	/**< Link to the background */
	uiSprite_t *bottomIcon;		/**< Link to the icon used for the bottom button */
	uiSprite_t *topIcon;		/**< Link to the icon used for the top button */
	int mode;					/**< The way the node react to input (see spinnerMode_t) */
};

void UI_RegisterSpinnerNode(uiBehaviour_t *behaviour);

#endif
