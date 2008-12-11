/**
 * @file m_node_vscrollbar.c
 * @todo implement click on bottom and top element of the scroll
 * @todo implement click on bottom-to-slider and top-to-slider element of the scroll
 * @todo implement hilight
 * @todo implement disabled
 * @todo robustness
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
#include "../m_actions.h"
#include "../m_input.h"
#include "../../cl_input.h"
#include "../../cl_keys.h"
#include "m_node_abstractscrollbar.h"
#include "m_node_vscrollbar.h"

static const int TILE_WIDTH = 32;
static const int TILE_HEIGHT = 18;
static const int ELEMENT_WIDTH = 27;
static const int ELEMENT_HEIGHT = 16;

#define EXTRADATA(node) (node->u.abstractscrollbar)

/**
 * @brief Set the position of the scrollbar to a value
 */
static void MN_VScrollbarNodeSet (menuNode_t *node, int value)
{
	int pos = value;

	if (pos < 0) {
		pos = 0;
	} else if (pos > EXTRADATA(node).fullsize - EXTRADATA(node).viewsize) {
		pos = EXTRADATA(node).fullsize - EXTRADATA(node).viewsize;
	}

	/* nothing change */
	if (EXTRADATA(node).pos == pos)
		return;

	/* update status */
	EXTRADATA(node).lastdiff = pos - EXTRADATA(node).pos;
	EXTRADATA(node).pos = pos;

	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

static int oldPos;
static int oldMouseY;

static void MN_VScrollbarNodeMouseDown (menuNode_t *node, int x, int y, int button)
{
	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return;
	if (button == K_MOUSE1) {
		MN_SetMouseCapture(node);

		/* save start value */
		oldMouseY = y;
		oldPos = EXTRADATA(node).pos;
	}
}

static void MN_VScrollbarNodeMouseUp (menuNode_t *node, int x, int y, int button)
{
	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return;
	if (button == K_MOUSE1)
		MN_MouseRelease();
}

/**
 * @todo call when the user wheel the mouse over the node
 */
static void MN_VScrollbarNodeWheel (menuNode_t *node, qboolean down, int x, int y)
{
	const int diff = (down)?1:-1;

	/** @todo remove that when the input handler is updated */
	if (node->disabled)
		return;

	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize < EXTRADATA(node).viewsize)
		return;

	MN_VScrollbarNodeSet(node, EXTRADATA(node).pos + diff);
}

/**
 * @brief Called when the node is captured by the mouse
 */
static void MN_VScrollbarNodeCapturedMouseMove (menuNode_t *node, int x, int y)
{
	const int posSize = EXTRADATA(node).fullsize;
	const int graphicSize = node->size[1] - (4 * ELEMENT_HEIGHT);
	int pos = 0;

	/* compute mouse mouse */
	y -= oldMouseY;

	/* compute pos projection */
	pos = oldPos + (((float)y * (float)posSize) / (float)graphicSize);

	MN_VScrollbarNodeSet(node, pos);
}

/**
 * @brief Call to draw the node
 */
static void MN_VScrollbarNodeDraw (menuNode_t *node)
{
	vec2_t pos;
	int y = 0;
	int texX = 0;
	int texY = 0;
	const char *texture;

	MN_GetNodeAbsPos(node, pos);
	y = pos[1];

	texture = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (!texture) {
		return;
	}

	if (EXTRADATA(node).fullsize == 0 || EXTRADATA(node).fullsize <= EXTRADATA(node).viewsize) {
		texX = TILE_WIDTH * 3;

		/* top */
		R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, node->blend, texture);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* top to bottom */
		R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, node->size[1] - (ELEMENT_HEIGHT * 2),
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, node->blend, texture);
		texY += TILE_HEIGHT * 5;
		y += node->size[1] - (ELEMENT_HEIGHT * 2);
		assert(y == pos[1] + node->size[1] - ELEMENT_HEIGHT);

		/* bottom */
		R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, node->blend, texture);

	} else {
		const int cuttableSize = node->size[1] - (ELEMENT_HEIGHT * 4);
		const int low = cuttableSize * ((float)(EXTRADATA(node).pos + 0) / (float)EXTRADATA(node).fullsize);
		const int mid = cuttableSize * ((float)(EXTRADATA(node).viewsize) / (float)EXTRADATA(node).fullsize);
		const int hi = cuttableSize - low - mid;

		/* top */
		R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, node->blend, texture);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* top to slider */
		if (low) {
			R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, low,
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
				ALIGN_UL, node->blend, texture);
		}
		texY += TILE_HEIGHT;
		y += low;

		/* top slider */
		R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, node->blend, texture);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* middle slider */
		if (mid) {
			R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, mid,
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
				ALIGN_UL, node->blend, texture);
		}
		texY += TILE_HEIGHT;
		y += mid;

		/* bottom slider */
		R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, node->blend, texture);
		texY += TILE_HEIGHT;
		y += ELEMENT_HEIGHT;

		/* slider to bottom */
		if (hi) {
			R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, hi,
				texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
				ALIGN_UL, node->blend, texture);
		}
		texY += TILE_HEIGHT;
		y += hi;
		assert(y == pos[1] + node->size[1] - ELEMENT_HEIGHT);

		/* bottom */
		R_DrawNormPic(pos[0], y, ELEMENT_WIDTH, ELEMENT_HEIGHT,
			texX + ELEMENT_WIDTH, texY + ELEMENT_HEIGHT, texX, texY,
			ALIGN_UL, node->blend, texture);
	}

}

static void MN_VScrollbarNodeLoaded (menuNode_t *node)
{
	node->size[0] = 27;
#ifdef DEBUG
	if (node->size[1] - (ELEMENT_HEIGHT * 4) < 0)
		Com_DPrintf(DEBUG_CLIENT, "Node '%s.%s' too small. It can create graphical glitches\n", node->menu->name, node->name);
#endif
}

void MN_RegisterVScrollbarNode (nodeBehaviour_t *behaviour)
{
	/* inheritance */
	MN_RegisterAbstractScrollbarNode(behaviour);
	/* overwrite */
	behaviour->name = "vscrollbar";
	behaviour->extends = "abstractscrollbar";
	behaviour->id = MN_VSCROLLBAR;
	behaviour->mouseWheel = MN_VScrollbarNodeWheel;
	behaviour->mouseDown = MN_VScrollbarNodeMouseDown;
	behaviour->mouseUp = MN_VScrollbarNodeMouseUp;
	behaviour->capturedMouseMove = MN_VScrollbarNodeCapturedMouseMove;
	behaviour->draw = MN_VScrollbarNodeDraw;
	behaviour->loaded = MN_VScrollbarNodeLoaded;
}
