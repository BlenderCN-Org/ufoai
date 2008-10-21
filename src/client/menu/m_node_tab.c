/**
 * @file m_node_tab.c
 * @todo add disabled status into @c selectBoxOptions_t update parser and this code with it.
 * @todo add an icon into @c selectBoxOptions_t update parser and this code with it.
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
#include "m_main.h"
#include "m_parse.h"
#include "m_font.h"
#include "m_input.h"

typedef enum {
	MN_TAB_NOTHING = 0,
	MN_TAB_NORMAL = 1,
	MN_TAB_SELECTED = 2,
	MN_TAB_HILIGHTED = 3,
	MN_TAB_DISABLED = 4
} mn_tab_type_t;

static const int TILE_WIDTH = 33;
static const int TILE_HEIGHT = 36;
static const int TILE_SIZE = 40;

/**
 * @brief Adds a new tab option to a tab node
 * @sa MN_TAB
 * @return @c NULL if menuTabs is 'full' - otherwise pointer to the selectBoxOption
 * @param[in] node The node (must be of type MN_TAB) where you want to append
 * the option
 * @note You have to add the values manually to the option pointer
 */
selectBoxOptions_t* MN_AddTabOption (menuNode_t *node)
{
	selectBoxOptions_t *tabOption;

	assert(node->type == MN_TAB);

	if (mn.numSelectBoxes >= MAX_SELECT_BOX_OPTIONS) {
		Com_Printf("MN_AddSelectboxOption: numSelectBoxes exceeded - increase MAX_SELECT_BOX_OPTIONS\n");
		return NULL;
	}

	/* initial options entry */
	if (!node->options)
		node->options = &mn.menuSelectBoxes[mn.numSelectBoxes];
	else {
		/* link it in */
		for (tabOption = node->options; tabOption->next; tabOption = tabOption->next) {}
		tabOption->next = &mn.menuSelectBoxes[mn.numSelectBoxes];
		tabOption->next->next = NULL;
	}

	tabOption = &mn.menuSelectBoxes[mn.numSelectBoxes++];
	node->height++;

	return tabOption;
}

/**
 * @brief Return a tab located at a screen position
 * @param[in] node A tab node
 * @param[in] x The x position of the screen to test
 * @param[in] y The x position of the screen to test
 * @return A selectBoxOptions_t, or NULL if nothing.
 * @todo improve the test when we are on a junction
 */
static selectBoxOptions_t* MN_TabNodeTabAtPosition (const menuNode_t *node, int x, int y)
{
	const char *font;
	selectBoxOptions_t* tabOption;

	/* Bounded box test */
	if (x < node->pos[0] || y < node->pos[1])
		return NULL;
	x -= node->pos[0], y -= node->pos[1];
	if (x > node->size[0] || y > node->size[1])
		return NULL;

	font = MN_GetFont(node->menu, node);

	/* Text box test */
	for (tabOption = node->options; tabOption; tabOption = tabOption->next) {
		int fontWidth;

		if (x < TILE_WIDTH)
			return NULL;

		/* @todo use LONGLINES_CHOP once rendering is done that way */
		R_FontTextSize(font, _(tabOption->label), 0, LONGLINES_WRAP, &fontWidth, NULL, NULL);
		if (x < TILE_WIDTH + fontWidth)
			return tabOption;

		x -= TILE_WIDTH + fontWidth;
	}
	return NULL;
}

/**
 * @brief Handles selectboxes clicks
 * @sa MN_SELECTBOX
 */
static void MN_TabNodeClick (menuNode_t * node, int x, int y)
{
	selectBoxOptions_t* newOption;
	const char *ref;

	newOption = MN_TabNodeTabAtPosition(node, x, y);
	if (newOption == NULL)
		return;

	ref = MN_GetReferenceString(node->menu, node->dataModelSkinOrCVar);
	/* Is we click on the already active tab? */
	if (!Q_strcmp(newOption->value, ref))
		return;

	/* the cvar string is stored in dataModelSkinOrCVar
	 * no cvar given? */
	if (!node->dataModelSkinOrCVar || !*(char*)node->dataModelSkinOrCVar) {
		Com_Printf("MN_TabNodeClick: node '%s' doesn't have a valid cvar assigned (menu %s)\n", node->name, node->menu->name);
		return;
	}

	/* no cvar? */
	if (Q_strncmp((const char *)node->dataModelSkinOrCVar, "*cvar", 5))
		return;

	/* only execute the click stuff if the selectbox is active */
	if (node->state) {
		const char *cvarName = &((const char *)node->dataModelSkinOrCVar)[6];
		MN_SetCvar(cvarName, newOption->value, 0);
		if (newOption->action[0] != '\0') {
#ifdef DEBUG
			if (newOption->action[strlen(newOption->action) - 1] != ';') {
				Com_Printf("selectbox option with none terminated action command");
			}
#endif
			Cbuf_AddText(newOption->action);
		}
	}
}

/**
 * @brief Normalized access to the texture structure of tab to display the plain part of a tab
 * @param[in] image The normalized tab texture to use
 * @param[in] x The upper-left position of the screen to draw the texture
 * @param[in] y The upper-left position of the screen to draw the texture
 * @param[in] width The width size of the screen to use (strech)
 * @param[in] type The status of the tab we display
 */
static inline void MN_DrawTabNodePlain (const char *image, int x, int y, int width, mn_tab_type_t type)
{
	R_DrawNormPic(x, y, width, TILE_HEIGHT, TILE_WIDTH + TILE_SIZE * 0, TILE_HEIGHT + TILE_SIZE * type,
		0 + TILE_SIZE * 0, 0 + TILE_SIZE * type, ALIGN_UL, qtrue, image);
}

/**
 * @brief Normalized access to the texture structure of tab to display a junction between each tabs
 * @param[in] image The normalized tab texture to use
 * @param[in] x The upper-left position of the screen to draw the texture
 * @param[in] y The upper-left position of the screen to draw the texture
 * @param[in] leftType The status of the left tab of the junction we display
 * @param[in] rightType The status of the right tab of the junction we display
 */
static inline void MN_DrawTabNodeJunction (const char *image, int x, int y, mn_tab_type_t leftType, mn_tab_type_t rightType)
{
	R_DrawNormPic(x, y, TILE_WIDTH, TILE_HEIGHT, TILE_WIDTH + TILE_SIZE * (1 + rightType), TILE_HEIGHT + TILE_SIZE * leftType,
		0 + TILE_SIZE * (1 + rightType), 0 + TILE_SIZE * leftType, ALIGN_UL, qtrue, image);
}

static void MN_DrawTabNode (menuNode_t *node)
{
	mn_tab_type_t lastStatus = MN_TAB_NOTHING;
	selectBoxOptions_t* tabOption;
	selectBoxOptions_t* overMouseOption = NULL;
	const char *ref;
	const char *font;
	int currentX;

	const char* image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (!image)
		image = "menu/tab";

	ref = MN_GetReferenceString(node->menu, node->dataModelSkinOrCVar);
	font = MN_GetFont(node->menu, node);

	if (node->state) {
		overMouseOption = MN_TabNodeTabAtPosition(node, mousePosX, mousePosY);
	}
	currentX = node->pos[0];
	tabOption = node->options;

	while (tabOption) {
		int fontHeight, fontWidth;
		/* Check the status of the current tab */
		mn_tab_type_t status = MN_TAB_NORMAL;
		if (!Q_strcmp(tabOption->value, ref)) {
			status = MN_TAB_SELECTED;
		} else if (tabOption == overMouseOption) {
			status = MN_TAB_HILIGHTED;
		}

		/* Display */
		MN_DrawTabNodeJunction(image, currentX, node->pos[1], lastStatus, status);
		currentX += TILE_WIDTH;

		R_FontTextSize(font, _(tabOption->label), 0, LONGLINES_WRAP, &fontWidth, &fontHeight, NULL);
		MN_DrawTabNodePlain(image, currentX, node->pos[1], fontWidth, status);
		R_FontDrawString(font, 0, currentX, node->pos[1] + ((node->size[1] - fontHeight) / 2),
			currentX, node->pos[1], fontWidth, TILE_HEIGHT,
			0, _(tabOption->label), 0, 0, NULL, qfalse, LONGLINES_WRAP);
		currentX += fontWidth;

		/* Next */
		tabOption = tabOption->next;
		lastStatus = status;
	}

	/* Display last junction and end of header */
	MN_DrawTabNodeJunction(image, currentX, node->pos[1], lastStatus, MN_TAB_NOTHING);
	currentX += TILE_WIDTH;
	if (currentX < node->pos[0] + node->size[0])
		MN_DrawTabNodePlain(image, currentX, node->pos[1], node->pos[0] + node->size[0] - currentX, MN_TAB_NOTHING);
}

void MN_RegisterNodeTab (nodeBehaviour_t *behaviour)
{
	behaviour->name = "tab";
	behaviour->draw = MN_DrawTabNode;
	behaviour->leftClick = MN_TabNodeClick;
}
