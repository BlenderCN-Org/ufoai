/**
 * @file m_parse.h
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

#ifndef CLIENT_MENU_M_PARSE_H
#define CLIENT_MENU_M_PARSE_H

#include "m_node_window.h"

void MN_ParseMenu(const char *name, const char **text);
void MN_ParseMenuModel(const char *name, const char **text);
float MN_GetReferenceFloat(const menu_t* const menu, void *ref);
const char *MN_GetReferenceString(const menu_t* const menu, const char *ref);

qboolean MN_ScriptSanityCheck(void);

#define V_MENU_COPY 0x100 /**< flag to request a memory copy into the menu structure */

#endif
