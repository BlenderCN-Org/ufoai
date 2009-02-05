/**
 * @file m_font.h
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

#ifndef CLIENT_MENU_M_FONT_H
#define CLIENT_MENU_M_FONT_H

#include "node/m_node_window.h"

typedef struct font_s {
	char *name;
	int size;
	char *style;
	char *path;
} font_t;

extern font_t *fontSmall;
extern font_t *fontBig;

/* will return the size and the path for each font */
const char *MN_GetFont(const menuNode_t *const node);
/* this is the function where all the sdl_ttf fonts are parsed */
void MN_ParseFont(const char *name, const char **text);
void MN_InitFonts(void);

#endif
