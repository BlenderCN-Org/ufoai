/**
 * @file m_node_abstractoption.h
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

#ifndef CLIENT_MENU_M_NODE_ABSTRACTOPTION_H
#define CLIENT_MENU_M_NODE_ABSTRACTOPTION_H

#include "../../../shared/shared.h"
#include "m_node_abstractscrollable.h"

struct menuAction_s;
struct menuOption_s;

typedef struct {
	/* link to shared data (can be used if internal data is null) */
	int dataId;							/**< Shared data id where we can find option */
	int versionId;						/**< Cached version of the shared data, to check update */

	/* link to internal data */
	struct menuOption_s *first;			/**< first option */

	/* information */
	struct menuOption_s *selected;		/**< current selected option */
	struct menuOption_s *hovered;		/**< current hovered option */
	int count;							/**< number of elements */
	int lineHeight;

	menuScroll_t scrollY;				/**< Scroll position, if need */

	struct menuAction_s *onViewChange;	/**< called when view change (number of elements...) */
} optionExtraData_t;

struct menuNode_s;
struct nodeBehaviour_s;

struct menuOption_s* MN_NodeAppendOption(struct menuNode_s *node, struct menuOption_s* option);
void MN_OptionNodeSortOptions(struct menuNode_s *node);
void MN_RegisterAbstractOptionNode(struct nodeBehaviour_s *behaviour);

#endif
