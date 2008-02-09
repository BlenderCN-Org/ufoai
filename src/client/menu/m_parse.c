/**
 * @file m_parse.c
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
#include "m_parse.h"
#include "m_main.h"
#include "m_actions.h"
#include "m_inventory.h"

/** @brief valid node event ids */
static const char *ne_strings[NE_NUM_NODEEVENT] = {
	"",
	"click",
	"rclick",
	"mclick",
	"wheel",
	"in",
	"out",
	"whup",
	"whdown"
};

static size_t const ne_values[NE_NUM_NODEEVENT] = {
	0,
	offsetof(menuNode_t, click),
	offsetof(menuNode_t, rclick),
	offsetof(menuNode_t, mclick),
	offsetof(menuNode_t, wheel),
	offsetof(menuNode_t, mouseIn),
	offsetof(menuNode_t, mouseOut),
	offsetof(menuNode_t, wheelUp),
	offsetof(menuNode_t, wheelDown)
};

/* =========================================================== */

/** @brief valid properties for a menu node */
static const value_t nps[] = {
	{"invis", V_BOOL, offsetof(menuNode_t, invis), MEMBER_SIZEOF(menuNode_t, invis)},
	{"mousefx", V_BOOL, offsetof(menuNode_t, mousefx), MEMBER_SIZEOF(menuNode_t, mousefx)},
	{"blend", V_BOOL, offsetof(menuNode_t, blend), MEMBER_SIZEOF(menuNode_t, blend)},
	{"texh", V_POS, offsetof(menuNode_t, texh), MEMBER_SIZEOF(menuNode_t, texh)},
	{"texl", V_POS, offsetof(menuNode_t, texl), MEMBER_SIZEOF(menuNode_t, texl)},
	{"border", V_INT, offsetof(menuNode_t, border), MEMBER_SIZEOF(menuNode_t, border)},
	{"padding", V_INT, offsetof(menuNode_t, padding), MEMBER_SIZEOF(menuNode_t, padding)},
	{"pos", V_POS, offsetof(menuNode_t, pos), MEMBER_SIZEOF(menuNode_t, pos)},
	{"size", V_POS, offsetof(menuNode_t, size), MEMBER_SIZEOF(menuNode_t, size)},
	{"format", V_POS, offsetof(menuNode_t, texh), MEMBER_SIZEOF(menuNode_t, texh)},
	{"origin", V_VECTOR, offsetof(menuNode_t, origin), MEMBER_SIZEOF(menuNode_t, origin)},
	{"center", V_VECTOR, offsetof(menuNode_t, center), MEMBER_SIZEOF(menuNode_t, center)},
	{"scale", V_VECTOR, offsetof(menuNode_t, scale), MEMBER_SIZEOF(menuNode_t, scale)},
	{"angles", V_VECTOR, offsetof(menuNode_t, angles), MEMBER_SIZEOF(menuNode_t, angles)},
	{"num", V_INT, offsetof(menuNode_t, num), MEMBER_SIZEOF(menuNode_t, num)},
	{"height", V_INT, offsetof(menuNode_t, height), MEMBER_SIZEOF(menuNode_t, height)},
	{"text_scroll", V_INT, offsetof(menuNode_t, textScroll), MEMBER_SIZEOF(menuNode_t, textScroll)},
	{"timeout", V_INT, offsetof(menuNode_t, timeOut), MEMBER_SIZEOF(menuNode_t, timeOut)},
	{"timeout_once", V_BOOL, offsetof(menuNode_t, timeOutOnce), MEMBER_SIZEOF(menuNode_t, timeOutOnce)},
	{"bgcolor", V_COLOR, offsetof(menuNode_t, bgcolor), MEMBER_SIZEOF(menuNode_t, bgcolor)},
	{"bordercolor", V_COLOR, offsetof(menuNode_t, bordercolor), MEMBER_SIZEOF(menuNode_t, bordercolor)},
	{"key", V_STRING, offsetof(menuNode_t, key), 0},
	/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
	{"tooltip", V_STRING, -5, 0},	/* translated in MN_Tooltip */
	{"image", V_STRING, 0, 0},
	{"roq", V_STRING, 0, 0},
	{"md2", V_STRING, 0, 0},
	{"anim", V_STRING, -1, 0},
	{"tag", V_STRING, -2, 0},
	{"cvar", V_STRING, -3, 0},	/* for selectbox */
	{"skin", V_STRING, -3, 0},
	/* -4 is animation state */
	{"string", V_LONGSTRING, 0, 0},	/* no gettext here - this can be a cvar, too */
	{"font", V_STRING, -1, 0},
	{"max", V_FLOAT, 0, 0},
	{"min", V_FLOAT, -1, 0},
	{"current", V_FLOAT, -2, 0},
	{"weapon", V_STRING, 0, 0},
	{"color", V_COLOR, offsetof(menuNode_t, color), MEMBER_SIZEOF(menuNode_t, color)},
	{"align", V_ALIGN, offsetof(menuNode_t, align), MEMBER_SIZEOF(menuNode_t, align)},
	{"if", V_IF, offsetof(menuNode_t, depends), 0},
	{"repeat", V_BOOL, offsetof(menuNode_t, repeat), MEMBER_SIZEOF(menuNode_t, repeat)},
	{"scrollbar", V_BOOL, offsetof(menuNode_t, scrollbar), MEMBER_SIZEOF(menuNode_t, scrollbar)},
	{"scrollbarleft", V_BOOL, offsetof(menuNode_t, scrollbarLeft), MEMBER_SIZEOF(menuNode_t, scrollbarLeft)},

	{NULL, V_NULL, 0, 0},
};

/** @brief valid properties for a select box */
static const value_t selectBoxValues[] = {
	{"label", V_TRANSLATION_MANUAL_STRING, offsetof(selectBoxOptions_t, label), 0},
	{"action", V_STRING, offsetof(selectBoxOptions_t, action), 0},
	{"value", V_STRING, offsetof(selectBoxOptions_t, value), 0},

	{NULL, V_NULL, 0, 0},
};

/** @brief valid properties for a menu model definition */
static const value_t menuModelValues[] = {
	{"model", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, model), 0},
	{"need", V_NULL, 0, 0},
	{"menutransform", V_NULL, 0, 0},
	{"anim", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, anim), 0},
	{"skin", V_INT, offsetof(menuModel_t, skin), sizeof(int)},
	{"origin", V_VECTOR, offsetof(menuModel_t, origin), sizeof(vec3_t)},
	{"center", V_VECTOR, offsetof(menuModel_t, center), sizeof(vec3_t)},
	{"scale", V_VECTOR, offsetof(menuModel_t, scale), sizeof(vec3_t)},
	{"angles", V_VECTOR, offsetof(menuModel_t, angles), sizeof(vec3_t)},
	{"color", V_COLOR, offsetof(menuModel_t, color), sizeof(vec4_t)},
	{"tag", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, tag), 0},
	{"parent", V_CLIENT_HUNK_STRING, offsetof(menuModel_t, parent), 0},

	{NULL, V_NULL, 0, 0},
};

/* =========================================================== */

/** @brief node type strings */
static const char *nt_strings[MN_NUM_NODETYPE] = {
	"",
	"confunc",
	"cvarfunc",
	"func",
	"zone",
	"pic",
	"string",
	"text",
	"bar",
	"model",
	"container",
	"item",
	"map",
	"basemap",
	"baselayout",
	"checkbox",
	"selectbox",
	"linestrip",
	"cinematic",
	"textlist"
};

/** @brief valid node event actions */
static const char *ea_strings[EA_NUM_EVENTACTION] = {
	"",
	"cmd",

	"",
	"*",
	"&"
};

/**
 * @brief
 * @sa MN_ExecuteActions
 */
static qboolean MN_ParseAction (menuNode_t *menuNode, menuAction_t *action, const char **text, const const char **token)
{
	const char *errhead = "MN_ParseAction: unexpected end of file (in event)";
	menuAction_t *lastAction;
	menuNode_t *node;
	qboolean found;
	const value_t *val;
	int i;

	lastAction = NULL;

	do {
		/* get new token */
		*token = COM_EParse(text, errhead, NULL);
		if (!*token)
			return qfalse;

		/* get actions */
		do {
			found = qfalse;

			/* standard function execution */
			for (i = 0; i < EA_CALL; i++)
				if (!Q_stricmp(*token, ea_strings[i])) {
/*					Com_Printf("   %s", *token); */

					/* add the action */
					if (lastAction) {
						if (mn.numActions >= MAX_MENUACTIONS)
							Sys_Error("MN_ParseAction: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
						action = &mn.menuActions[mn.numActions++];
						memset(action, 0, sizeof(menuAction_t));
						lastAction->next = action;
					}
					action->type = i;

					if (ea_values[i] != V_NULL) {
						/* get parameter values */
						*token = COM_EParse(text, errhead, NULL);
						if (!*text)
							return qfalse;

/*						Com_Printf(" %s", *token); */

						/* get the value */
						action->data = mn.curadata;
						mn.curadata += Com_ParseValue(mn.curadata, *token, ea_values[i], 0, 0);
					}

/*					Com_Printf("\n"); */

					/* get next token */
					*token = COM_EParse(text, errhead, NULL);
					if (!*text)
						return qfalse;

					lastAction = action;
					found = qtrue;
					break;
				}

			/* node property setting */
			switch (**token) {
			case '*':
/*				Com_Printf("   %s", *token); */

				/* add the action */
				if (lastAction) {
					if (mn.numActions >= MAX_MENUACTIONS)
						Sys_Error("MN_ParseAction: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
					action = &mn.menuActions[mn.numActions++];
					memset(action, 0, sizeof(menuAction_t));
					lastAction->next = action;
				}
				action->type = EA_NODE;

				/* get the node name */
				action->data = mn.curadata;

				strcpy((char *) mn.curadata, &(*token)[1]);
				mn.curadata += ALIGN(strlen((char *) mn.curadata) + 1);

				/* get the node property */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

/*				Com_Printf(" %s", *token); */

				for (val = nps, i = 0; val->type; val++, i++)
					if (!Q_stricmp(*token, val->string))
						break;

				action->scriptValues = val;

				if (!val->type) {
					Com_Printf("MN_ParseAction: token \"%s\" isn't a node property (in event)\n", *token);
					mn.curadata = action->data;
					if (lastAction) {
						lastAction->next = NULL;
						mn.numActions--;
					}
					break;
				}

				/* get the value */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

/*				Com_Printf(" %s\n", *token); */

				mn.curadata += Com_ParseValue(mn.curadata, *token, val->type, 0, val->size);

				/* get next token */
				*token = COM_EParse(text, errhead, NULL);
				if (!*text)
					return qfalse;

				lastAction = action;
				found = qtrue;
				break;
			case '&':
				action->type = EA_VAR;
				break;
			}

			/* function calls */
			for (node = mn.menus[mn.numMenus - 1].firstNode; node; node = node->next)
				if ((node->type == MN_FUNC || node->type == MN_CONFUNC || node->type == MN_CVARFUNC)
					&& !Q_strncmp(node->name, *token, sizeof(node->name))) {
/*					Com_Printf("   %s\n", node->name); */

					/* add the action */
					if (lastAction) {
						if (mn.numActions >= MAX_MENUACTIONS)
							Sys_Error("MN_ParseAction: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
						action = &mn.menuActions[mn.numActions++];
						memset(action, 0, sizeof(menuAction_t));
						lastAction->next = action;
					}
					action->type = EA_CALL;

					action->data = mn.curadata;
					*(menuAction_t ***) mn.curadata = &node->click;
					mn.curadata += ALIGN(sizeof(menuAction_t *));

					/* get next token */
					*token = COM_EParse(text, errhead, NULL);
					if (!*text)
						return qfalse;

					lastAction = action;
					found = qtrue;
					break;
				}
		} while (found);

		/* test for end or unknown token */
		if (**token == '}') {
			/* finished */
			return qtrue;
		} else {
			if (!Q_strcmp(*token, "timeout")) {
				/* get new token */
				*token = COM_EParse(text, errhead, NULL);
				if (!*token || **token == '}') {
					Com_Printf("MN_ParseAction: timeout with no value (in event) (node: %s)\n", menuNode->name);
					return qfalse;
				}
				menuNode->timeOut = atoi(*token);
			} else {
				/* unknown token, print message and continue */
				Com_Printf("MN_ParseAction: unknown token \"%s\" ignored (in event) (node: %s, menu %s)\n", *token, menuNode->name, menuNode->menu->name);
			}
		}
	} while (*text);

	return qfalse;
}

/**
 * @sa MN_ParseMenuBody
 */
static qboolean MN_ParseNodeBody (menuNode_t * node, const char **text, const char **token)
{
	const char *errhead = "MN_ParseNodeBody: unexpected end of file (node";
	qboolean found;
	const value_t *val;
	int i;

	/* functions are a special case */
	if (node->type == MN_CONFUNC || node->type == MN_FUNC || node->type == MN_CVARFUNC) {
		menuAction_t **action;

		/* add new actions to end of list */
		action = &node->click;
		for (; *action; action = &(*action)->next);

		if (mn.numActions >= MAX_MENUACTIONS)
			Sys_Error("MN_ParseNodeBody: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
		*action = &mn.menuActions[mn.numActions++];
		memset(*action, 0, sizeof(menuAction_t));

		if (node->type == MN_CONFUNC) {
			/* don't add a callback twice */
			if (!Cmd_Exists(node->name))
				Cmd_AddCommand(node->name, MN_Command_f, "Confunc callback");
			else
				Com_DPrintf(DEBUG_CLIENT, "MN_ParseNodeBody: skip confunc '%s' - already added (menu %s)\n", node->name, node->menu->name);
		}

		return MN_ParseAction(node, *action, text, token);
	}

	do {
		/* get new token */
		*token = COM_EParse(text, errhead, node->name);
		if (!*text)
			return qfalse;

		/* get properties, events and actions */
		do {
			found = qfalse;

			for (val = nps; val->type; val++)
				if (!Q_strcmp(*token, val->string)) {
					node->scriptValues = val;

/*					Com_Printf("  %s", *token); */

					if (val->type != V_NULL) {
						/* get parameter values */
						*token = COM_EParse(text, errhead, node->name);
						if (!*text)
							return qfalse;

/*						Com_Printf(" %s", *token); */

						/* get the value */
						/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
						if ((val->ofs > 0) && (val->ofs < (size_t)-5)) {
							if (Com_ParseValue(node, *token, val->type, val->ofs, val->size) == -1)
								Com_Printf("MN_ParseNodeBody: Wrong size for value %s\n", val->string);
						} else {
							/* a reference to data is handled like this */
/* 							Com_Printf("%i %s\n", val->ofs, *token); */
							node->data[-((int)val->ofs)] = mn.curadata;
							/* references are parsed as string */
							if (**token == '*')
								mn.curadata += Com_ParseValue(mn.curadata, *token, V_STRING, 0, 0);
							else
								mn.curadata += Com_ParseValue(mn.curadata, *token, val->type, 0, val->size);
						}
					}

/*					Com_Printf("\n"); */

					/* get next token */
					*token = COM_EParse(text, errhead, node->name);
					if (!*text)
						return qfalse;

					found = qtrue;
					break;
				}

			for (i = 0; i < NE_NUM_NODEEVENT; i++)
				if (!Q_strcmp(*token, ne_strings[i])) {
					menuAction_t **action;

/*					Com_Printf("  %s\n", *token); */

					/* add new actions to end of list */
					action = (menuAction_t **) ((byte *) node + ne_values[i]);
					for (; *action; action = &(*action)->next);

					if (mn.numActions >= MAX_MENUACTIONS)
						Sys_Error("MN_ParseNodeBody: MAX_MENUACTIONS exceeded (%i)\n", mn.numActions);
					*action = &mn.menuActions[mn.numActions++];
					memset(*action, 0, sizeof(menuAction_t));

					/* get the action body */
					*token = COM_EParse(text, errhead, node->name);
					if (!*text)
						return qfalse;

					if (**token == '{') {
						MN_ParseAction(node, *action, text, token);

						/* get next token */
						*token = COM_EParse(text, errhead, node->name);
						if (!*text)
							return qfalse;
					}

					found = qtrue;
					break;
				}
		} while (found);

		/* test for end or unknown token */
		if (**token == '}') {
			/* finished */
			return qtrue;
		} else if (!Q_strncmp(*token, "excluderect", 12)) {
			/* get parameters */
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			if (**token != '{') {
				Com_Printf("MN_ParseNodeBody: node with bad excluderect ignored (node \"%s\", menu %s)\n", node->name, node->menu->name);
				continue;
			}

			do {
				*token = COM_EParse(text, errhead, node->name);
				if (!*text)
					return qfalse;
				if (!Q_strcmp(*token, "pos")) {
					*token = COM_EParse(text, errhead, node->name);
					if (!*text)
						return qfalse;
					Com_ParseValue(&node->exclude[node->excludeNum], *token, V_POS, offsetof(excludeRect_t, pos), sizeof(vec2_t));
				} else if (!Q_strcmp(*token, "size")) {
					*token = COM_EParse(text, errhead, node->name);
					if (!*text)
						return qfalse;
					Com_ParseValue(&node->exclude[node->excludeNum], *token, V_POS, offsetof(excludeRect_t, size), sizeof(vec2_t));
				}
			} while (**token != '}');
			if (node->excludeNum < MAX_EXLUDERECTS - 1)
				node->excludeNum++;
			else
				Com_Printf("MN_ParseNodeBody: exluderect limit exceeded (max: %i)\n", MAX_EXLUDERECTS);
		/* for MN_SELECTBOX */
		} else if (!Q_strncmp(*token, "option", 7)) {
			/* get parameters */
			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			Q_strncpyz(mn.menuSelectBoxes[mn.numSelectBoxes].id, *token, sizeof(mn.menuSelectBoxes[mn.numSelectBoxes].id));
			Com_DPrintf(DEBUG_CLIENT, "...found selectbox: '%s'\n", *token);

			*token = COM_EParse(text, errhead, node->name);
			if (!*text)
				return qfalse;
			if (**token != '{') {
				Com_Printf("MN_ParseNodeBody: node with bad option definition ignored (node \"%s\", menu %s)\n", node->name, node->menu->name);
				continue;
			}

			if (mn.numSelectBoxes >= MAX_SELECT_BOX_OPTIONS) {
				FS_SkipBlock(text);
				Com_Printf("MN_ParseNodeBody: Too many option entries for node %s (menu %s)\n", node->name, node->menu->name);
				return qfalse;
			}

			/**
			 * the options data can be defined like this
			 * @code
			 * option string {
			 *  value "value"
			 *  action "command string"
			 * }
			 * @endcode
			 * The strings will appear in the drop down list of the select box
			 * if you select the 'string', the 'cvarname' will be set to 'value'
			 * and if action is defined (which is a console/script command string)
			 * this one is executed on each selection
			 */

			do {
				*token = COM_EParse(text, errhead, node->name);
				if (!*text)
					return qfalse;
				if (**token == '}')
					break;
				for (val = selectBoxValues; val->string; val++)
					if (!Q_strncmp(*token, val->string, sizeof(val->string))) {
						/* get parameter values */
						*token = COM_EParse(text, errhead, node->name);
						if (!*text)
							return qfalse;
						Com_ParseValue(&mn.menuSelectBoxes[mn.numSelectBoxes], *token, val->type, val->ofs, val->size);
						break;
					}
				if (!val->string)
					Com_Printf("MN_ParseNodeBody: unknown options value: '%s' - ignore it\n", *token);
			} while (**token != '}');
			MN_AddSelectboxOption(node);
		} else {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseNodeBody: unknown token \"%s\" ignored (node \"%s\", menu %s)\n", *token, node->name, node->menu->name);
		}
	} while (*text);

	return qfalse;
}

/**
 * @sa MN_ParseNodeBody
 */
static qboolean MN_ParseMenuBody (menu_t * menu, const char **text)
{
	const char *errhead = "MN_ParseMenuBody: unexpected end of file (menu";
	const char *token;
	qboolean found;
	menuNode_t *node, *lastNode, *iNode;
	int i;

	lastNode = NULL;

	/* if inheriting another menu, link in the super menu's nodes */
	for (node = menu->firstNode; node; node = node->next) {
		if (mn.numNodes >= MAX_MENUNODES)
			Sys_Error("MAX_MENUNODES exceeded\n");
		iNode = &mn.menuNodes[mn.numNodes++];
		*iNode = *node;
		/* link it in */
		if (lastNode)
			lastNode->next = iNode;
		else
			menu->firstNode = iNode;
		lastNode = iNode;
	}

	lastNode = NULL;

	do {
		/* get new token */
		token = COM_EParse(text, errhead, menu->name);
		if (!*text)
			return qfalse;

		/* get node type */
		do {
			found = qfalse;

			for (i = 0; i < MN_NUM_NODETYPE; i++)
				if (!Q_strcmp(token, nt_strings[i])) {
					/* found node */
					/* get name */
					token = COM_EParse(text, errhead, menu->name);
					if (!*text)
						return qfalse;

					/* test if node already exists */
					for (node = menu->firstNode; node; node = node->next) {
						if (!Q_strncmp(token, node->name, sizeof(node->name))) {
							if (node->type != i)
								Com_Printf("MN_ParseMenuBody: node prototype type change (menu \"%s\")\n", menu->name);
							Com_DPrintf(DEBUG_CLIENT, "... over-riding node %s in menu %s\n", node->name, menu->name);
							/* reset action list of node */
							node->click = NULL;
							break;
						}
						lastNode = node;
					}

					/* initialize node */
					if (!node) {
						if (mn.numNodes >= MAX_MENUNODES)
							Sys_Error("MAX_MENUNODES exceeded\n");
						node = &mn.menuNodes[mn.numNodes++];
						memset(node, 0, sizeof(menuNode_t));
						node->menu = menu;
						Q_strncpyz(node->name, token, sizeof(node->name));

						/* link it in */
						if (lastNode)
							lastNode->next = node;
						else
							menu->firstNode = node;

						lastNode = node;
					}

					node->type = i;
					/* node default values */
					node->padding = 3;
					node->textLineSelected = -1; /**< Invalid/no line selected per default. */

/*					Com_Printf(" %s %s\n", nt_strings[i], *token); */

					/* check for special nodes */
					switch (node->type) {
					case MN_FUNC:
						if (!Q_strncmp(node->name, "init", 4)) {
							if (!menu->initNode)
								menu->initNode = node;
							else
								Com_Printf("MN_ParseMenuBody: second init function ignored (menu \"%s\")\n", menu->name);
						} else if (!Q_strncmp(node->name, "close", 5)) {
							if (!menu->closeNode)
								menu->closeNode = node;
							else
								Com_Printf("MN_ParseMenuBody: second close function ignored (menu \"%s\")\n", menu->name);
						} else if (!Q_strncmp(node->name, "event", 5)) {
							if (!menu->eventNode) {
								menu->eventNode = node;
								menu->eventNode->timeOut = 2000; /* default value */
							} else
								Com_Printf("MN_ParseMenuBody: second event function ignored (menu \"%s\")\n", menu->name);
						} else if (!Q_strncmp(node->name, "leave", 5)) {
							if (!menu->leaveNode) {
								menu->leaveNode = node;
							} else
								Com_Printf("MN_ParseMenuBody: second leave function ignored (menu \"%s\")\n", menu->name);
						}
						break;
					case MN_ZONE:
						if (!Q_strncmp(node->name, "render", 6)) {
							if (!menu->renderNode)
								menu->renderNode = node;
							else
								Com_Printf("MN_ParseMenuBody: second render node ignored (menu \"%s\")\n", menu->name);
						} else if (!Q_strncmp(node->name, "popup", 5)) {
							if (!menu->popupNode)
								menu->popupNode = node;
							else
								Com_Printf("MN_ParseMenuBody: second popup node ignored (menu \"%s\")\n", menu->name);
						}
						break;
					case MN_CONTAINER:
						node->mousefx = C_UNDEFINED;
						break;
					}

					/* set standard texture coordinates */
/*					node->texl[0] = 0; node->texl[1] = 0; */
/*					node->texh[0] = 1; node->texh[1] = 1; */

					/* get parameters */
					token = COM_EParse(text, errhead, menu->name);
					if (!*text)
						return qfalse;

					if (*token == '{') {
						if (!MN_ParseNodeBody(node, text, &token)) {
							Com_Printf("MN_ParseMenuBody: node with bad body ignored (menu \"%s\")\n", menu->name);
							mn.numNodes--;
							continue;
						}

						token = COM_EParse(text, errhead, menu->name);
						if (!*text)
							return qfalse;
					}

					/* set standard color */
					if (!node->color[3])
						Vector4Set(node->color, 1, 1, 1, 1);

					found = qtrue;
					break;
				}
		} while (found);

		/* test for end or unknown token */
		if (*token == '}') {
			/* finished */
			return qtrue;
		} else {
			/* unknown token, print message and continue */
			Com_Printf("MN_ParseMenuBody: unknown token \"%s\" ignored (menu \"%s\")\n", token, menu->name);
		}

	} while (*text);

	return qfalse;
}

/**
 * @brief parses the models.ufo and all files where menu_models are defined
 * @sa CL_ParseClientData
 */
void MN_ParseMenuModel (const char *name, const char **text)
{
	menuModel_t *menuModel;
	const char *token;
	int i;
	const value_t *v = NULL;
	const char *errhead = "MN_ParseMenuModel: unexpected end of file (names ";

	/* search for menumodels with same name */
	for (i = 0; i < mn.numMenuModels; i++)
		if (!Q_strcmp(mn.menuModels[i].id, name)) {
			Com_Printf("MN_ParseMenuModel: menu_model \"%s\" with same name found, second ignored\n", name);
			return;
		}

	if (mn.numMenuModels >= MAX_MENUMODELS) {
		Com_Printf("MN_ParseMenuModel: Max menu models reached\n");
		return;
	}

	/* initialize the menu */
	menuModel = &mn.menuModels[mn.numMenuModels];
	memset(menuModel, 0, sizeof(menuModel_t));

	Vector4Set(menuModel->color, 1, 1, 1, 1);

	menuModel->id = Mem_PoolStrDup(name, cl_menuSysPool, CL_TAG_MENU);
	Com_DPrintf(DEBUG_CLIENT, "Found menu model %s (%i)\n", menuModel->id, mn.numMenuModels);

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenuModel: menu \"%s\" without body ignored\n", menuModel->id);
		return;
	}

	mn.numMenuModels++;

	do {
		/* get the name type */
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
		if (*token == '}')
			break;

		for (v = menuModelValues; v->string; v++)
			if (!Q_strncmp(token, v->string, sizeof(v->string))) {
				/* found a definition */
				if (!Q_strncmp(v->string, "need", 4)) {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					menuModel->next = MN_GetMenuModel(token);
					if (!menuModel->next)
						Com_Printf("Could not find menumodel %s", token);
					menuModel->need = Mem_PoolStrDup(token, cl_menuSysPool, CL_TAG_MENU);
				} else if (!Q_strncmp(v->string, "menutransform", 13)) {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;
					if (*token != '{') {
						Com_Printf("Error in menumodel '%s' menutransform definition\n", name);
						break;
					}
					do {
						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}')
							break;
						menuModel->menuTransform[menuModel->menuTransformCnt].menuID = Mem_PoolStrDup(token, cl_menuSysPool, CL_TAG_MENU);

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing scale float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useScale = qfalse;
						} else {
							Com_ParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].scale, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useScale = qtrue;
						}

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing angles float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useAngles = qfalse;
						} else {
							Com_ParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].angles, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useAngles = qtrue;
						}

						token = COM_EParse(text, errhead, name);
						if (!*text)
							return;
						if (*token == '}') {
							Com_Printf("Error in menumodel '%s' menutransform definition - missing origin float value\n", name);
							break;
						}
						if (*token == '#') {
							menuModel->menuTransform[menuModel->menuTransformCnt].useOrigin = qfalse;
						} else {
							Com_ParseValue(&menuModel->menuTransform[menuModel->menuTransformCnt].origin, token, V_VECTOR, 0, sizeof(vec3_t));
							menuModel->menuTransform[menuModel->menuTransformCnt].useOrigin = qtrue;
						}

						menuModel->menuTransformCnt++;
					} while (*token != '}'); /* dummy condition - break is earlier here */
				} else {
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					switch (v->type) {
					case V_CLIENT_HUNK_STRING:
						Mem_PoolStrDupTo(token, (char**) ((char*)menuModel + (int)v->ofs), cl_menuSysPool, CL_TAG_MENU);
						break;
					default:
						Com_ParseValue(menuModel, token, v->type, v->ofs, v->size);
					}
				}
				break;
			}

		if (!v->string)
			Com_Printf("MN_ParseMenuModel: unknown token \"%s\" ignored (menu_model %s)\n", token, name);

	} while (*text);
}

/**
 * @sa CL_ParseClientData
 */
void MN_ParseMenu (const char *name, const char **text)
{
	menu_t *menu;
	menuNode_t *node;
	const char *token;
	int i;

	/* search for menus with same name */
	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strncmp(name, mn.menus[i].name, MAX_VAR))
			break;

	if (i < mn.numMenus) {
		Com_Printf("MN_ParseMenus: menu \"%s\" with same name found, second ignored\n", name);
		return;
	}

	if (mn.numMenus >= MAX_MENUS) {
		Sys_Error("MN_ParseMenu: max menus exceeded (%i) - ignore '%s'\n", MAX_MENUS, name);
		return;	/* never reached */
	}

	/* initialize the menu */
	menu = &mn.menus[mn.numMenus++];
	memset(menu, 0, sizeof(menu_t));

	Q_strncpyz(menu->name, name, sizeof(menu->name));

	/* get it's body */
	token = COM_Parse(text);

	/* does this menu inherit data from another menu? */
	if (!Q_strncmp(token, "extends", 7)) {
		menu_t *superMenu;
		token = COM_Parse(text);
		Com_DPrintf(DEBUG_CLIENT, "MN_ParseMenus: menu \"%s\" inheriting menu \"%s\"\n", name, token);
		superMenu = MN_GetMenu(token);
		if (!superMenu)
			Sys_Error("MN_ParseMenu: menu '%s' can't inherit from menu '%s' - because '%s' was not found\n", name, token, token);
		memcpy(menu, superMenu, sizeof(menu_t));
		Q_strncpyz(menu->name, name, sizeof(menu->name));
		token = COM_Parse(text);
	}

	if (!*text || *token != '{') {
		Com_Printf("MN_ParseMenus: menu \"%s\" without body ignored\n", menu->name);
		mn.numMenus--;
		return;
	}

	/* parse it's body */
	if (!MN_ParseMenuBody(menu, text)) {
		Com_Printf("MN_ParseMenus: menu \"%s\" with bad body ignored\n", menu->name);
		mn.numMenus--;
		return;
	}

	for (node = menu->firstNode; node; node = node->next)
		if (node->num >= MAX_MENUTEXTS)
			Sys_Error("Error in menu %s - max menu num exeeded (num: %i, max: %i) in node '%s'", menu->name, node->num, MAX_MENUTEXTS, node->name);

}

/**
 * @sa COM_MacroExpandString
 */
const char *MN_GetReferenceString (const menu_t* const menu, char *ref)
{
	if (!ref)
		return NULL;
	if (*ref == '*') {
		char ident[MAX_VAR];
		const char *text, *token;
		char command[MAX_VAR];
		char param[MAX_VAR];

		/* get the reference and the name */
		text = COM_MacroExpandString(ref);
		if (text)
			return text;

		text = ref + 1;
		token = COM_Parse(&text);
		if (!text)
			return NULL;
		Q_strncpyz(ident, token, MAX_VAR);
		token = COM_Parse(&text);
		if (!text)
			return NULL;

		if (!Q_strncmp(ident, "binding", 7)) {
			/* get the cvar value */
			if (*text && *text <= ' ') {
				/* check command and param */
				Q_strncpyz(command, token, MAX_VAR);
				token = COM_Parse(&text);
				Q_strncpyz(param, token, MAX_VAR);
				/*Com_sprintf(token, MAX_VAR, "%s %s", command, param);*/
			}
			return Key_GetBinding(token, (cls.state != ca_active ? KEYSPACE_MENU : KEYSPACE_GAME));
		} else {
			menuNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(menu, ident);
			if (!refNode)
				return NULL;

			/* get the property */
			for (val = nps; val->type; val++)
				if (!Q_stricmp(token, val->string))
					break;

			if (!val->type)
				return NULL;

			/* get the string */
			/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
			if ((val->ofs > 0) && (val->ofs < (size_t)-5))
				return Com_ValueToStr(refNode, val->type, val->ofs);
			else
				return Com_ValueToStr(refNode->data[-((int)val->ofs)], val->type, 0);
		}
	} else if (*ref == '_') {
		ref++;
		return _(ref);
	} else {
		/* just get the data */
		return ref;
	}
}

float MN_GetReferenceFloat (const menu_t* const menu, void *ref)
{
	if (!ref)
		return 0.0;
	if (*(char *) ref == '*') {
		char ident[MAX_VAR];
		const char *token, *text;

		/* get the reference and the name */
		text = (char *) ref + 1;
		token = COM_Parse(&text);
		if (!text)
			return 0.0;
		Q_strncpyz(ident, token, MAX_VAR);
		token = COM_Parse(&text);
		if (!text)
			return 0.0;

		if (!Q_strncmp(ident, "cvar", 4)) {
			/* get the cvar value */
			return Cvar_VariableValue(token);
		} else {
			menuNode_t *refNode;
			const value_t *val;

			/* draw a reference to a node property */
			refNode = MN_GetNode(menu, ident);
			if (!refNode)
				return 0.0;

			/* get the property */
			for (val = nps; val->type; val++)
				if (!Q_stricmp(token, val->string))
					break;

			if (val->type != V_FLOAT)
				return 0.0;

			/* get the string */
			/* 0, -1, -2, -3, -4, -5 fills the data array in menuNode_t */
			if ((val->ofs > 0) && (val->ofs < (size_t)-5))
				return *(float *) ((byte *) refNode + val->ofs);
			else
				return *(float *) refNode->data[-((int)val->ofs)];
		}
	} else {
		/* just get the data */
		return *(float *) ref;
	}
}

/**
 * @brief Checks the parsed menus for errors
 * @return false if there are errors - true otherwise
 */
qboolean MN_ScriptSanityCheck (void)
{
	int i, error = 0;
	menuNode_t* node;

	for (i = 0, node = mn.menuNodes; i < mn.numNodes; i++, node++) {
		switch (node->type) {
		case MN_TEXT:
			if (!node->height) {
				Com_Printf("MN_ParseNodeBody: node '%s' (menu: %s) has no height value but is a text node\n", node->name, node->menu->name);
				error++;
			} else if (node->texh[0] && node->height != (int)(node->size[1] / node->texh[0])) {
				/* if node->texh[0] == 0, the height of the font is used */
				Com_Printf("MN_ParseNodeBody: height value (%i) of node '%s' (menu: %s) differs from size (%.0f) and format (%.0f) values\n",
					node->height, node->name, node->menu->name, node->size[1], node->texh[0]);
				error++;
			}
			break;
		default:
			break;
		}
	}

	if (!error)
		return qtrue;
	else
		return qfalse;
}
