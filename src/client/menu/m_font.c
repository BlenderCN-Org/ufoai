/**
 * @file m_font.c
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
#include "m_font.h"
#include "m_parse.h"

#define MAX_FONTS 16
static int numFonts = 0;
static font_t fonts[MAX_FONTS];

font_t *fontBig;
font_t *fontSmall;

static const value_t fontValues[] = {
	{"font", V_TRANSLATION_MANUAL_STRING, offsetof(font_t, path), 0},
	{"size", V_INT, offsetof(font_t, size), MEMBER_SIZEOF(font_t, size)},
	{"style", V_CLIENT_HUNK_STRING, offsetof(font_t, style), 0},

	{NULL, V_NULL, 0},
};

static void MN_RegisterFont (const font_t *font)
{
	const char *path = _(font->path);

	if (!path)
		Sys_Error("...font without path (font %s)\n", font->name);

	if (FS_CheckFile(path) == -1)
		Sys_Error("...font file %s does not exist (font %s)\n", path, font->name);

	R_FontRegister(font->name, font->size, path, font->style);
}

/**
 * @sa CL_ParseClientData
 */
void MN_ParseFont (const char *name, const char **text)
{
	font_t *font;
	const char *errhead = "MN_ParseFont: unexpected end of file (font";
	const char *token;
	int i;
	const value_t *v = NULL;

	/* search for font with same name */
	for (i = 0; i < numFonts; i++)
		if (!Q_strncmp(fonts[i].name, name, MAX_VAR)) {
			Com_Printf("MN_ParseFont: font \"%s\" with same name found, second ignored\n", name);
			return;
		}

	if (numFonts >= MAX_FONTS) {
		Com_Printf("MN_ParseFont: Max fonts reached\n");
		return;
	}

	/* initialize the menu */
	font = &fonts[numFonts];
	memset(font, 0, sizeof(*font));

	font->name = Mem_PoolStrDup(name, cl_menuSysPool, CL_TAG_MENU);

	if (!Q_strcmp(font->name, "f_small"))
		fontSmall = font;
	else if (!Q_strcmp(font->name, "f_big"))
		fontBig = font;

	Com_DPrintf(DEBUG_CLIENT, "...found font %s (%i)\n", font->name, numFonts);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseFont: font \"%s\" without body ignored\n", name);
		return;
	}

	numFonts++;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = fontValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				switch (v->type) {
				case V_TRANSLATION_MANUAL_STRING:
					token++;
				case V_CLIENT_HUNK_STRING:
					Mem_PoolStrDupTo(token, (char**) ((char*)font + (int)v->ofs), cl_menuSysPool, CL_TAG_MENU);
					break;
				default:
					Com_ParseValue(font, token, v->type, v->ofs, v->size);
					break;
				}
				break;
			}

		if (!v->string)
			Com_Printf("MN_ParseFont: unknown token \"%s\" ignored (font %s)\n", token, name);
	} while (*text);

	MN_RegisterFont(font);
}

/**
 * @brief Return the font for a specific node or default font
 * @param[in] m The current menu pointer - if NULL we will use the current mn.menuStack
 * @param[in] n The node to get the font for - if NULL f_small is returned
 * @return char pointer with font name (default is f_small)
 */
const char *MN_GetFont (const menu_t *m, const menuNode_t *const n)
{
	if (!n || n->dataAnimOrFont) {
		if (!m)
			m = MN_GetActiveMenu();

		return MN_GetReferenceString(m, n->dataAnimOrFont);
	}
	return "f_small";
}

/**
 * @brief after a vid restart we have to reinit the fonts
 */
void MN_InitFonts (void)
{
	int i;

	Com_Printf("...registering %i fonts\n", numFonts);
	for (i = 0; i < numFonts; i++)
		MN_RegisterFont(&fonts[i]);
}
