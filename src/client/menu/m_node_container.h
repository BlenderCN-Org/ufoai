/**
 * @file m_node_container.h
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

#ifndef CLIENT_MENU_M_NODE_CONTAINER_H
#define CLIENT_MENU_M_NODE_CONTAINER_H

struct base_s;

#include "m_nodes.h"

void MN_RegisterContainerNode(nodeBehaviour_t *behaviour);
void MN_FindContainer(menuNode_t* const node);
invList_t *MN_GetItemFromScrollableContainer (const menuNode_t* const node, int mouseX, int mouseY, int* contX, int* contY);
void MN_Drag(const menuNode_t* const node, struct base_s *base, int x, int y, qboolean rightClick);
void MN_DrawItem(menuNode_t *node, const vec3_t org, const item_t *item, int x, int y, const vec3_t scale, const vec4_t color);
void MN_GetItemTooltip(item_t item, char *tooltiptext, size_t string_maxlength);

#endif
