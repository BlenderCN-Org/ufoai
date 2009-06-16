/**
 * @file m_actions.c
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

#include "m_main.h"
#include "m_internal.h"
#include "m_parse.h"
#include "m_input.h"
#include "m_actions.h"
#include "node/m_node_abstractnode.h"

#include "../client.h"

/**
 * @brief read a property name from an input buffer to an output
 * @return last position into the input buffer if we find property, else NULL
 */
static inline const char* MN_GenCommandReadProperty (const char* input, char* output, int outputSize)
{
	assert(input[0] == '<');
	outputSize--;
	input++;

	while (outputSize && *input != '\0' && *input != ' ' && *input != '>') {
		*output++ = *input++;
		outputSize--;
	}

	if (input[0] != '>')
		return NULL;

	output[0] = '\0';
	return ++input;
}

/**
 * @brief Replace injection identifiers (e.g. <eventParam>) by a value
 * @note The injection identifier can be every node value - e.g. <image> or <width>.
 * It's also possible to do something like
 * @code cmd "set someCvar <min>/<max>"
 */
const char* MN_GenInjectedString (const menuNode_t* source, qboolean useCmdParam, const char* input, qboolean addNewLine)
{
	static char cmd[256];
	int length = sizeof(cmd) - (addNewLine ? 2 : 1);
	static char propertyName[MAX_VAR];
	const char *cin = input;
	char *cout = cmd;

	while (length && cin[0] != '\0') {
		if (cin[0] == '<') {
			/* read propertyName between '<' and '>' */
			const char *next = MN_GenCommandReadProperty(cin, propertyName, sizeof(propertyName));
			if (next) {
				/* cvar injection */
				if (!strncmp(propertyName, "cvar:", 5)) {
					const cvar_t *cvar = Cvar_Get(propertyName + 5, "", 0, NULL);
					const int l = snprintf(cout, length, "%s", cvar->string);
					cout += l;
					cin = next;
					length -= l;
					continue;

				} else if (!strncmp(propertyName, "node:", 5)) {
					const char *path = propertyName + 5;
					menuNode_t *node;
					const value_t *property;
					const char* string;
					int l;
					MN_ReadNodePath(path, source, &node, &property);
					if (!node) {
						Com_Printf("MN_GenInjectedString: Node '%s' wasn't found; '' returned\n", path);
						string = "";
					} else if (!property) {
						Com_Printf("MN_GenInjectedString: Property '%s' wasn't found; '' returned\n", path);
						string = "";
					} else {
						string = MN_GetStringFromNodeProperty(node, property);
						if (string == NULL) {
							Com_Printf("MN_GenInjectedString: String getter for '%s' property do not exists; '' injected\n", path);
							string = "";
						}
					}

					l = snprintf(cout, length, "%s", string);
					cout += l;
					cin = next;
					length -= l;
					continue;

				/* source path injection */
				} else if (!strncmp(propertyName, "path:", 5)) {
					if (source) {
						const char *command = propertyName + 5;
						const menuNode_t *node = NULL;
						if (!strcmp(command, "root"))
							node = source->root;
						else if (!strcmp(command, "this"))
							node = source;
						else if (!strcmp(command, "parent"))
							node = source->parent;
						else
							Com_Printf("MN_GenCommand: Command '%s' for path injection unknown\n", command);

						if (node) {
							const int l = snprintf(cout, length, "%s", MN_GetPath(node));
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}

				/* no prefix */
				} else {
					/* source property injection */
					if (source) {
						/* find property definition */
						const value_t *property = MN_GetPropertyFromBehaviour(source->behaviour, propertyName);
						if (property) {
							const char* value;
							int l;
							/* inject the property value */
							value = MN_GetStringFromNodeProperty(source, property);
							if (value == NULL)
								value = "";
							l = snprintf(cout, length, "%s", value);
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}

					/* param injection */
					if (useCmdParam) {
						int arg;
						const int checked = sscanf(propertyName, "%d", &arg);
						if (checked == 1 && Cmd_Argc() >= arg) {
							const int l = snprintf(cout, length, "%s", Cmd_Argv(arg));
							cout += l;
							cin = next;
							length -= l;
							continue;
						}
					}
				}
			}
		}
		*cout++ = *cin++;
		length--;
	}

	/* is buffer too small? */
	assert(cin[0] == '\0');

	if (addNewLine)
		*cout++ = '\n';

	*cout++ = '\0';

	return cmd;
}

static inline void MN_ExecuteSetAction (const menuNode_t* source, qboolean useCmdParam, const menuAction_t* action)
{
	const char* path;
	menuNode_t *node;
	const value_t *property;
	const menuAction_t *left;
	menuAction_t *right;

	left = action->d.nonTerminal.left;
	if (left == NULL) {
		Com_Printf("MN_ExecuteSetAction: Action without left operand skiped.\n");
		return;
	}

	right = action->d.nonTerminal.right;
	if (right == NULL) {
		Com_Printf("MN_ExecuteSetAction: Action without right operand skiped.\n");
		return;
	}

	if (left->type == EA_VALUE_CVARNAME || left->type == EA_VALUE_CVARNAME_WITHINJECTION) {
		const char *cvarName;
		const char* textValue;

		if (left->type == EA_VALUE_CVARNAME)
			cvarName = left->d.terminal.d1.string;
		else
			cvarName = MN_GenInjectedString(source, useCmdParam, left->d.terminal.d1.string, qfalse);

		textValue = MN_GetStringFromExpression(right, source);

		if (textValue[0] == '_')
			textValue = _(textValue + 1);

		Cvar_ForceSet(cvarName, textValue);
		return;
	}

	/* search the node */
	if (left->type == EA_VALUE_PATHPROPERTY)
		path = left->d.terminal.d1.string;
	else if (left->type == EA_VALUE_PATHPROPERTY_WITHINJECTION)
		path = MN_GenInjectedString(source, useCmdParam, left->d.terminal.d1.string, qfalse);
	else
		Com_Error(ERR_FATAL, "MN_ExecuteSetAction: Property setter with wrong type '%d'", left->type);

	MN_ReadNodePath(path, source, &node, &property);
	if (!node) {
		Com_Printf("MN_ExecuteSetAction: node \"%s\" doesn't exist (source: %s)\n", path, MN_GetPath(source));
		return;
	}
	if (!property) {
		Com_Printf("MN_ExecuteSetAction: property \"%s\" doesn't exist (source: %s)\n", path, MN_GetPath(source));
		return;
	}

	/* decode RAW value */
	if (right->type == EA_VALUE_RAW) {
		void *mem = ((byte *) node + property->ofs);
		if ((property->type & V_UI_MASK) == V_NOT_UI)
			Com_SetValue(node, right->d.terminal.d1.data, property->type, property->ofs, property->size);
		else if ((property->type & V_UI_MASK) == V_UI_CVAR) {
			MN_FreeStringProperty(*(void**)mem);
			switch (property->type & V_BASETYPEMASK) {
			case V_FLOAT:
				**(float **) mem = *(float*) right->d.terminal.d1.data;
				break;
			case V_INT:
				**(int **) mem = *(int*) right->d.terminal.d1.data;
				break;
			default:
				*(byte **) mem = right->d.terminal.d1.data;
			}
		} else if (property->type == V_UI_ACTION) {
			*(menuAction_t**) mem = (menuAction_t*) right->d.terminal.d1.data;
		} else if (property->type == V_UI_ICONREF) {
			*(menuIcon_t**) mem = (menuIcon_t*) right->d.terminal.d1.data;
		} else {
			Com_Error(ERR_FATAL, "MN_ExecuteSetAction: Property type '%d' unsupported", property->type);
		}
		return;
	}

	/* else it is an expression */
	/** @todo we should improve if when the prop is a boolean/int/float */
	else {
		const char* value = MN_GetStringFromExpression(right, source);
		MN_NodeSetProperty(node, property, value);
		return;
	}
}

static void MN_ExecuteInjectedActions(const menuNode_t* source, qboolean useCmdParam, const menuAction_t* firstAction);

/**
 * @brief Execute an action from a source
 * @param[in] useCmdParam If true, inject every where its possible command line param
 */
static void MN_ExecuteInjectedAction (const menuNode_t* source, qboolean useCmdParam, const menuAction_t* action)
{
	switch (action->type) {
	case EA_NULL:
		/* do nothing */
		break;

	case EA_CMD:
		/* execute a command */
		if (action->d.terminal.d1.string)
			Cbuf_AddText(MN_GenInjectedString(source, useCmdParam, action->d.terminal.d1.string, qtrue));
		break;

	case EA_CALL:
		/* call another function */
		MN_ExecuteInjectedActions((const menuNode_t *) action->d.terminal.d1.data, qfalse, *(menuAction_t **) action->d.terminal.d2.data);
		break;

	case EA_ASSIGN:
		MN_ExecuteSetAction(source, useCmdParam, action);
		break;

	case EA_IF:
		if (MN_GetBooleanFromExpression(action->d.nonTerminal.left, source)) {
			MN_ExecuteInjectedActions(source, useCmdParam, action->d.nonTerminal.right);
			return;
		}
		action = action->next;
		while (action && action->type == EA_ELIF) {
			if (MN_GetBooleanFromExpression(action->d.nonTerminal.left, source)) {
				MN_ExecuteInjectedActions(source, useCmdParam, action->d.nonTerminal.right);
				return;
			}
			action = action->next;
		}
		if (action && action->type == EA_ELSE) {
			MN_ExecuteInjectedActions(source, useCmdParam, action->d.nonTerminal.right);
		}
		break;

	case EA_ELSE:
	case EA_ELIF:
		/* previous EA_IF execute this action */
		break;

	default:
		Com_Error(ERR_FATAL, "unknown action type");
	}
}

static void MN_ExecuteInjectedActions (const menuNode_t* source, qboolean useCmdParam, const menuAction_t* firstAction)
{
	static int callnumber = 0;
	const menuAction_t *action;
	if (callnumber++ > 20) {
		Com_Printf("MN_ExecuteInjectedActions: Possible recursion\n");
		return;
	}
	for (action = firstAction; action; action = action->next) {
		MN_ExecuteInjectedAction(source, useCmdParam, action);
	}
	callnumber--;
}

/**
 * @brief allow to inject command param into cmd of confunc command
 */
void MN_ExecuteConFuncActions (const menuNode_t* source, const menuAction_t* firstAction)
{
	MN_ExecuteInjectedActions(source, qtrue, firstAction);
}

void MN_ExecuteEventActions (const menuNode_t* source, const menuAction_t* firstAction)
{
	MN_ExecuteInjectedActions(source, qfalse, firstAction);
}

/**
 * @brief Test if a string use an injection syntax
 * @return True if we find in the string a syntaxe "<" {thing without space} ">"
 */
qboolean MN_IsInjectedString (const char *string)
{
	const char *c = string;
	assert(string);
	while (*c != '\0') {
		if (*c == '<') {
			const char *d = c + 1;
			if (*d != '>') {
				while (*d) {
					if (*d == '>')
						return qtrue;
					if (*d == ' ' || *d == '\t' || *d == '\n' || *d == '\r')
						break;
					d++;
				}
			}
		}
		c++;
	}
	return qfalse;
}

/**
 * @brief Free a string property if it is allocated into mn_dynStringPool
 * @sa mn_dynStringPool
 */
void MN_FreeStringProperty (void* pointer)
{
	/* skip const string */
	if ((uintptr_t)mn.adata <= (uintptr_t)pointer && (uintptr_t)pointer < (uintptr_t)mn.adata + (uintptr_t)mn.adataize)
		return;

	/* skip pointer out of mn_dynStringPool */
	if (!_Mem_AllocatedInPool(mn_dynStringPool, pointer))
		return;

	Mem_Free(pointer);
}

/**
 * @brief Allocate and initialize a command action
 * @param[in] command A command for the action
 * @return An initialised action
 */
menuAction_t* MN_AllocCommandAction (char *command)
{
	menuAction_t* action = MN_AllocAction();
	action->type = EA_CMD;
	action->d.terminal.d1.string = command;
	return action;
}

/**
 * @brief Set a new action to a @c menuAction_t pointer
 * @param[in] type Only @c EA_CMD is supported
 * @param[in] data The data for this action - in case of @c EA_CMD this is the commandline
 * @note You first have to free existing node actions - only free those that are
 * not static in @c mn.menuActions array
 * @todo we should create a function to free the memory. We can use a tag in the Mem_PoolAlloc
 * calls and use use Mem_FreeTag.
 */
void MN_PoolAllocAction (menuAction_t** action, int type, const void *data)
{
	if (*action)
		Com_Error(ERR_FATAL, "There is already an action assigned");
	*action = (menuAction_t *)Mem_PoolAlloc(sizeof(**action), mn_sysPool, 0);
	(*action)->type = type;
	switch (type) {
	case EA_CMD:
		(*action)->d.terminal.d1.string = Mem_PoolStrDup((const char *)data, mn_sysPool, 0);
		break;
	default:
		Com_Error(ERR_FATAL, "Action type %i is not yet implemented", type);
	}
}

/**
 * @brief add a call of a function into a nodfe event
 */
static void MN_AddListener_f (void)
{
	menuNode_t *node;
	menuNode_t *function;
	const value_t *property;
	menuAction_t *action;
	menuAction_t *lastAction;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <pathnode@event> <pathnode>\n", Cmd_Argv(0));
		return;
	}

	MN_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);
	if (node == NULL) {
		Com_Printf("MN_AddListener_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}
	if (property == NULL || property->type != V_UI_ACTION) {
		Com_Printf("MN_AddListener_f: '%s' property not found, or is not an event.\n", Cmd_Argv(1));
		return;
	}

	function = MN_GetNodeByPath(Cmd_Argv(2));
	if (function == NULL) {
		Com_Printf("MN_AddListener_f: '%s' node not found.\n", Cmd_Argv(2));
		return;
	}

	/* create the action */
	action = (menuAction_t*) Mem_PoolAlloc(sizeof(*action), mn_sysPool, 0);
	action->type = EA_CALL;
	action->d.terminal.d1.data = (void*) function;
	action->d.terminal.d2.data = (void*) &function->onClick;
	action->next = NULL;

	/* insert the action */
	lastAction = *(menuAction_t**)((char*)node + property->ofs);
	while (lastAction)
		lastAction = lastAction->next;
	if (lastAction)
		lastAction->next = action;
	else
		*(menuAction_t**)((char*)node + property->ofs) = action;
}

/**
 * @brief add a call of a function from a nodfe event
 */
static void MN_RemoveListener_f (void)
{
	menuNode_t *node;
	menuNode_t *function;
	const value_t *property;
	void *data;
	menuAction_t *lastAction;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <pathnode@event> <pathnode>\n", Cmd_Argv(0));
		return;
	}

	MN_ReadNodePath(Cmd_Argv(1), NULL, &node, &property);
	if (node == NULL) {
		Com_Printf("MN_RemoveListener_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}
	if (property == NULL || property->type != V_UI_ACTION) {
		Com_Printf("MN_RemoveListener_f: '%s' property not found, or is not an event.\n", Cmd_Argv(1));
		return;
	}

	function = MN_GetNodeByPath(Cmd_Argv(2));
	if (function == NULL) {
		Com_Printf("MN_RemoveListener_f: '%s' node not found.\n", Cmd_Argv(2));
		return;
	}

	/* data we must remove */
	data = (void*) &function->onClick;

	/* remove the action */
	lastAction = *(menuAction_t**)((char*)node + property->ofs);
	if (lastAction) {
		menuAction_t *tmp = NULL;
		if (lastAction->d.terminal.d2.data == data) {
			tmp = lastAction;
			*(menuAction_t**)((char*)node + property->ofs) = lastAction->next;
		} else {
			while (lastAction->next) {
				if (lastAction->next->d.terminal.d2.data == data)
					break;
				lastAction = lastAction->next;
			}
			if (lastAction->next) {
				tmp = lastAction->next;
				lastAction->next = lastAction->next->next;
			}
		}
		if (tmp)
			Mem_Free(tmp);
		else
			Com_Printf("MN_RemoveListener_f: '%s' into '%s' not found.\n", Cmd_Argv(2), Cmd_Argv(1));
	}
}

void MN_InitActions (void)
{
	Cmd_AddCommand("mn_addlistener", MN_AddListener_f, "Add a function into a node event");
	Cmd_AddCommand("mn_removelistener", MN_RemoveListener_f, "Remove a function from a node event");
}
