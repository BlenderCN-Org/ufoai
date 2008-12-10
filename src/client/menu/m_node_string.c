/**
 * @file m_node_string.c
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

#include "m_nodes.h"
#include "m_font.h"
#include "m_parse.h"
#include "../client.h"
#include "m_node_string.h"

static void MN_StringNodeDraw (menuNode_t *node)
{
	vec2_t nodepos;
	menu_t *menu = node->menu;
	const char *font = MN_GetFont(menu, node);
	const char* ref = MN_GetReferenceString(node->menu, node->text);
	if (!ref)
		return;
	MN_GetNodeAbsPos(node, nodepos);

	/* blinking */
	R_ColorBlend(node->color);
	if (node->size[0] == 0) {
		/** @todo (menu) should this wrap or chop long lines? */
		R_FontDrawString(font, node->textalign, nodepos[0], nodepos[1], nodepos[0], nodepos[1], node->size[0], 0, node->texh[0], ref, 0, 0, NULL, qfalse, 0);
	}
	else {
		R_FontDrawStringInBox(font, node->textalign, nodepos[0], nodepos[1], node->size[0], node->size[1], ref, node->longlines);
	}

	R_ColorBlend(NULL);
}

static void MN_StringNodeLoaded (menuNode_t *node)
{
	/* normalize node position */
	if (node->align != ALIGN_UL) {
		const int horiz_align = node->align % 3; /* left, center, right */
		const int vert_align = node->align / 3;  /* top, center, bottom */

		node->textalign = node->align;

		node->pos[0] -= ((node->size[0] * horiz_align) / 2);
		node->pos[1] -= ((node->size[1] * vert_align) / 2);

		node->align = ALIGN_UL;
		/** @todo in few time, we can add a warning to request an update of the node */
	}
}

static void MN_StringNodeLoading (menuNode_t *node)
{
	Vector4Set(node->color, 1.0, 1.0, 1.0, 1.0);
}

void MN_RegisterStringNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "string";
	behaviour->id = MN_STRING;
	behaviour->draw = MN_StringNodeDraw;
	behaviour->loading = MN_StringNodeLoading;
	behaviour->loaded = MN_StringNodeLoaded;
}
