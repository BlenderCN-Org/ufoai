/**
 * @file m_node_textentry.c
 * @todo must we need to use command to interact with keyboard?
 * @todo allow to edit text without any cvar
 * @todo add a custom max size
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
#include "m_node_textentry.h"
#include "m_parse.h"
#include "m_drawutil.h"
#include "m_font.h"
#include "m_input.h"
#include "m_actions.h"
#include "../client.h"

static const int TILE_SIZE = 64;
static const int CORNER_SIZE = 17;
static const int MID_SIZE = 1;
static const int MARGE = 3;

static const char CURSOR = '|';		/**< Use as the cursor when we edit the text */
static const char HIDECHAR = '*';	/**< use as a mask for password */

/* prototype */
static void MN_TextEntryNodeRemoveFocus(menuNode_t *node);
static void MN_TextEntryNodeSetFocus(menuNode_t *node);
static void MN_TextEntryNodeSetFocus (menuNode_t *node);

/* global data */
static char cmdChanged[MAX_VAR];
static char cmdAborted[MAX_VAR];

/**
 * @brief fire the change event
 */
static inline void MN_TextEntryNodeFireChange (menuNode_t *node)
{
	/* fire change event */
	if (node->onChange) {
		MN_ExecuteEventActions(node, node->onChange);
	}
}

/**
 * @brief fire the abort event
 */
static inline void MN_TextEntryNodeFireAbort (menuNode_t *node)
{
	/* fire change event */
	if (node->u.textentry.onAbort) {
		MN_ExecuteEventActions(node, node->u.textentry.onAbort);
	}
}

/**
 * @brief callback from the keyboard
 */
static void MN_TextEntryNodeKeyboardChanged_f ()
{
	menuNode_t *node = (menuNode_t *) Cmd_Userdata();
	MN_TextEntryNodeRemoveFocus(node);
	MN_TextEntryNodeFireChange(node);
}

/**
 * @brief callback from the keyboard
 */
static void MN_TextEntryNodeKeyboardAborted_f ()
{
	menuNode_t *node = (menuNode_t *) Cmd_Userdata();
	MN_TextEntryNodeRemoveFocus(node);
	MN_TextEntryNodeFireAbort(node);
}

/**
 * @brief force edition of a textentry node
 * @note the textentry must be on the active menu
 */
static void MN_EditTextEntry_f (void)
{
	menuNode_t *node;
	const char* name;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <textentrynode>\n", Cmd_Argv(0));
		return;
	}

	name = Cmd_Argv(1);
	node = MN_GetNode(MN_GetActiveMenu(), name);
	if (!node) {
		Com_Printf("MN_EditTextEntry_f: node '%s' dont exists on the current active menu '%s'\n", name, MN_GetActiveMenu()->name);
		return;
	}
	MN_TextEntryNodeSetFocus(node);
}

/**
 * @todo save last existing commands, to restitute it
 */
static void MN_TextEntryNodeSetFocus (menuNode_t *node)
{
	MN_SetMouseCapture(node);

	/* register keyboard callback */
	snprintf(cmdChanged, sizeof(cmdChanged), "%s_changed", &((char*)node->text)[6]);
	if (Cmd_Exists(cmdChanged)) {
		Sys_Error("MN_TextEntryNodeSetFocus: '%s' already used, code down yet allow context restitution. Plese clean up your script.\n", cmdChanged);
	}
	Cmd_AddCommand(cmdChanged, MN_TextEntryNodeKeyboardChanged_f, "Text entry callback");
	Cmd_AddUserdata(cmdChanged, node);
	snprintf(cmdAborted, sizeof(cmdAborted), "%s_aborted", &((char*)node->text)[6]);
	if (Cmd_Exists(cmdAborted)) {
		Sys_Error("MN_TextEntryNodeSetFocus: '%s' already used, code down yet allow context restitution. Plese clean up your script.\n", cmdAborted);
	}
	Cmd_AddCommand(cmdAborted, MN_TextEntryNodeKeyboardAborted_f, "Text entry callback");
	Cmd_AddUserdata(cmdAborted, node);

	/* start typing */
	Cbuf_AddText(va("mn_msgedit ?%s\n", &((char*)node->text)[6]));
}

static void MN_TextEntryNodeRemoveFocus (menuNode_t *node)
{
	Cmd_RemoveCommand(cmdChanged);
	Cmd_RemoveCommand(cmdAborted);
	MN_MouseRelease();
}

/**
 * @todo remove the "mouse capture" for a "focus", maybe better
 */
static void MN_TextEntryNodeClick (menuNode_t *node, int x, int y)
{
	if (node->disabled)
		return;

	/* no cvar */
	if (!node->text)
		return;
	if (Q_strncmp(node->text, "*cvar", 5))
		return;

	if (!MN_GetMouseCapture()) {
		if (node->onClick) {
			MN_ExecuteEventActions(node, node->onClick);
		}
		MN_TextEntryNodeSetFocus(node);
	} else {
		int x = mousePosX;
		int y = mousePosY;
		MN_NodeAbsoluteToRelativePos(node, &x, &y);
		if (x < 0 || y < 0 || x > node->pos[0] || y > node->pos[1]) {
			/* keyboard, please stop */
			if (node->u.textentry.clickOutAbort) {
				Cbuf_AddText("mn_msgedit !\n");
			} else {
				Cbuf_AddText("mn_msgedit .\n");
			}
		}
	}
}

static void MN_TextEntryNodeDraw (menuNode_t *node)
{
	const char *text;
	const char *font;
	int texX, texY;
	const float *textColor;
	const char *image;
	vec2_t pos;
	static vec4_t disabledColor = {0.5, 0.5, 0.5, 1.0};

	if (node->disabled) {
		/** @todo need custom color when button is disabled */
		textColor = disabledColor;
		texX = TILE_SIZE;
		texY = TILE_SIZE;
	} else if (node->state || MN_GetMouseCapture() == node) {
		textColor = node->selectedColor;
		texX = TILE_SIZE;
		texY = 0;
	} else {
		textColor = node->color;
		texX = 0;
		texY = 0;
	}

	MN_GetNodeAbsPos(node, pos);

	image = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	if (image) {
		MN_DrawPanel(pos, node->size, image, node->blend, texX, texY, CORNER_SIZE, MID_SIZE, MARGE);
	}

	text = MN_GetReferenceString(node->menu, node->text);
	if (text != NULL) {
		/** @todo we dont need to edit the text to draw the cursor */
		if (MN_GetMouseCapture() == node) {
			if (cl.time % 1000 < 500) {
				text = va("%s%c", text, CURSOR);
			}
		}

		if (node->u.textentry.isPassword) {
			char *c = va("%s", text);
			text = c;
			/* hide the text */
			/** @todo does it work with Unicode :/ dont we create to char? */
			while (*c != '\0') {
				*c++ = HIDECHAR;
			}
			/* replace the cursor */
			if (MN_GetMouseCapture() == node) {
				if (cl.time % 1000 < 500) {
					*--c = CURSOR;
				}
			}
		}

		if (*text != '\0') {
			font = MN_GetFont(node->menu, node);
			R_ColorBlend(textColor);
			R_FontDrawStringInBox(font, node->textalign,
				pos[0] + node->padding, pos[1] + node->padding,
				node->size[0] - node->padding - node->padding, node->size[1] - node->padding - node->padding,
				text, LONGLINES_PRETTYCHOP);
			R_ColorBlend(NULL);
		}
	}

}

/**
 * @brief Call before the script initialisation of the node
 */
static void MN_TextEntryNodeLoading (menuNode_t *node)
{
	node->padding = 8;
	node->textalign = ALIGN_CL;
	Vector4Set(node->color, 1, 1, 1, 1);
	Vector4Set(node->selectedColor, 1, 1, 1, 1);
}

static void MN_TextEntryNodeInitBehaviour (nodeBehaviour_t *behaviour)
{
	Cmd_AddCommand("mn_edittextentry", MN_EditTextEntry_f, "Force edition of the textentry.");
}

static const value_t properties[] = {
	{"ispassword", V_BOOL, offsetof(menuNode_t, u.textentry.isPassword), MEMBER_SIZEOF(menuNode_t, u.textentry.isPassword) },
	{"clickoutabort", V_BOOL, offsetof(menuNode_t, u.textentry.clickOutAbort), MEMBER_SIZEOF(menuNode_t, u.textentry.clickOutAbort)},
	{"abort", V_SPECIAL_ACTION, offsetof(menuNode_t, u.textentry.onAbort), MEMBER_SIZEOF(menuNode_t, u.textentry.onAbort)},
	{NULL, V_NULL, 0, 0}
};

void MN_RegisterTextEntryNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "textentry";
	behaviour->id = MN_TEXTENTRY;
	behaviour->leftClick = MN_TextEntryNodeClick;
	behaviour->draw = MN_TextEntryNodeDraw;
	behaviour->loading = MN_TextEntryNodeLoading;
	behaviour->properties = properties;
	behaviour->initBehaviour = MN_TextEntryNodeInitBehaviour;
}
