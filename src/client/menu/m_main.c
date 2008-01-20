/**
 * @file m_main.c
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
#include "../client.h"

menuGlobal_t mn;
static cvar_t *mn_escpop;

/**
 * @brief Returns the number of currently renderer menus on the menustack
 * @note Checks for a render node - if invis is true there, it's the last
 * visible menu
 */
int MN_GetVisibleMenuCount (void)
{
	/* stack pos */
	int sp = mn.menuStackPos;
	while (sp > 0)
		if (mn.menuStack[--sp]->renderNode)
			break;
	/*Com_DPrintf(DEBUG_CLIENT, "visible menus on stacks: %i\n", sp);*/
	return sp;
}

/**
 * @brief Remove the menu from the menu stack
 * @param[in] menu The menu to remove from the stack
 * @sa MN_PushMenuDelete
 */
static void MN_DeleteMenuFromStack (menu_t * menu)
{
	int i;

	for (i = 0; i < mn.menuStackPos; i++)
		if (mn.menuStack[i] == menu) {
			/* @todo don't leave the loop even if we found it - there still
			 * may be other copies around in the stack of the same menu
			 * @sa MN_PushCopyMenu_f */
			for (mn.menuStackPos--; i < mn.menuStackPos; i++)
				mn.menuStack[i] = mn.menuStack[i + 1];
			return;
		}
}

/**
 * @brief Push a menu onto the menu stack
 * @param[in] name Name of the menu to push onto menu stack
 * @param[in] delete Delete the menu from the menu stack before readd it
 * @return pointer to menu_t
 */
static menu_t* MN_PushMenuDelete (const char *name, qboolean delete)
{
	int i;
	menuNode_t *node;

	MN_FocusRemove();

	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strncmp(mn.menus[i].name, name, MAX_VAR)) {
			/* found the correct add it to stack or bring it on top */
			if (delete)
				MN_DeleteMenuFromStack(&mn.menus[i]);

			if (mn.menuStackPos < MAX_MENUSTACK)
				mn.menuStack[mn.menuStackPos++] = &mn.menus[i];
			else
				Com_Printf("Menu stack overflow\n");

			/* initialize it */
			if (mn.menus[i].initNode)
				MN_ExecuteActions(&mn.menus[i], mn.menus[i].initNode->click);

			if (cls.key_dest == key_input && msg_mode == MSG_MENU)
				Key_Event(K_ENTER, qtrue, cls.realtime);
			Key_SetDest(key_game);

			for (node = mn.menus[i].firstNode; node; node = node->next) {
				/* if there is a timeout value set, init the menu with current
				 * client time */
				if (node->timeOut)
					node->timePushed = cl.time;
			}

			return mn.menus + i;
		}

	Com_Printf("Didn't find menu \"%s\"\n", name);
	return NULL;
}

/**
 * @brief Complete function for mn_push
 * @sa Cmd_AddParamCompleteFunction
 * @sa MN_PushMenu
 * @note Does not really complete the input - but shows at least all parsed menus
 */
int MN_CompletePushMenu (const char *partial, const char **match)
{
	int i, matches = 0;
	const char *localMatch[MAX_COMPLETE];
	size_t len;

	len = strlen(partial);
	if (!len) {
		for (i = 0; i < mn.numMenus; i++)
			Com_Printf("%s\n", mn.menus[i].name);
		return 0;
	}

	localMatch[matches] = NULL;

	/* check for partial matches */
	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strncmp(partial, mn.menus[i].name, len)) {
			Com_Printf("%s\n", mn.menus[i].name);
			localMatch[matches++] = mn.menus[i].name;
			if (matches >= MAX_COMPLETE)
				break;
		}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

/**
 * @brief Push a menu onto the menu stack
 * @param[in] name Name of the menu to push onto menu stack
 * @return pointer to menu_t
 */
menu_t* MN_PushMenu (const char *name)
{
	return MN_PushMenuDelete(name, qtrue);
}

/**
 * @brief Console function to push a menu onto the menu stack
 * @sa MN_PushMenu
 */
static void MN_PushMenu_f (void)
{
	if (Cmd_Argc() > 1)
		MN_PushMenu(Cmd_Argv(1));
	else
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
}

/**
 * @brief Console function to hide the HUD in battlescape mode
 * Note: relies on a "nohud" menu existing
 * @sa MN_PushMenu
 */
static void MN_PushNoHud_f (void)
{
	/* can't hide hud if we are not in battlescape */
	if (!CL_OnBattlescape())
		return;

	MN_PushMenu("nohud");
}

/**
 * @brief Console function to push a menu, without deleting its copies
 * @sa MN_PushMenu
 */
static void MN_PushCopyMenu_f (void)
{
	if (Cmd_Argc() > 1) {
		Cvar_SetValue("mn_escpop", mn_escpop->value + 1);
		MN_PushMenuDelete(Cmd_Argv(1), qfalse);
	} else {
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
	}
}


/**
 * @brief Pops a menu from the menu stack
 * @param[in] all If true pop all menus from stack
 * @sa MN_PopMenu_f
 */
void MN_PopMenu (qboolean all)
{
	/* make sure that we end all input buffers */
	if (cls.key_dest == key_input && msg_mode == MSG_MENU)
		Key_Event(K_ENTER, qtrue, cls.realtime);

	MN_FocusRemove();

	if (all)
		while (mn.menuStackPos > 0) {
			mn.menuStackPos--;
			if (mn.menuStack[mn.menuStackPos]->closeNode)
				MN_ExecuteActions(mn.menuStack[mn.menuStackPos], mn.menuStack[mn.menuStackPos]->closeNode->click);
		}

	if (mn.menuStackPos > 0) {
		mn.menuStackPos--;
		if (mn.menuStack[mn.menuStackPos]->closeNode)
			MN_ExecuteActions(mn.menuStack[mn.menuStackPos], mn.menuStack[mn.menuStackPos]->closeNode->click);
	}

	if (!all && mn.menuStackPos == 0) {
		if (!Q_strncmp(mn.menuStack[0]->name, mn_main->string, MAX_VAR)) {
			if (*mn_active->string)
				MN_PushMenu(mn_active->string);
			if (!mn.menuStackPos)
				MN_PushMenu(mn_main->string);
		} else {
			if (*mn_main->string)
				MN_PushMenu(mn_main->string);
			if (!mn.menuStackPos)
				MN_PushMenu(mn_active->string);
		}
	}

	Key_SetDest(key_game);

	/* when we leave a menu and a menu cinematic is running... we should stop it */
	if (cls.playingCinematic == CIN_STATUS_MENU)
		CIN_StopCinematic();
}

/**
 * @brief Console function to pop a menu from the menu stack
 * @sa MN_PopMenu
 * @note The cvar mn_escpop defined how often the MN_PopMenu function is called.
 * This is useful for e.g. nodes that doesn't have a render node (for example: video options)
 */
static void MN_PopMenu_f (void)
{
	if (Cmd_Argc() < 2 || Q_strncmp(Cmd_Argv(1), "esc", 3)) {
		MN_PopMenu(qfalse);
	} else {
		int i;

		for (i = 0; i < mn_escpop->integer; i++)
			MN_PopMenu(qfalse);

		Cvar_Set("mn_escpop", "1");
	}
}

/**
 * @brief Returns the current active menu from the menu stack or NULL if there is none
 * @return menu_t pointer from menu stack
 */
menu_t* MN_GetActiveMenu (void)
{
	return (mn.menuStackPos > 0 ? mn.menuStack[mn.menuStackPos - 1] : NULL);
}

/**
 * @brief Searches all menus for the specified one
 * @param[in] name If name is NULL this function will return the current menu
 * on the stack - otherwise it will search the hole stack for a menu with the
 * id name
 * @return NULL if not found or no menu on the stack
 */
menu_t *MN_GetMenu (const char *name)
{
	int i;

	/* get the current menu */
	if (name == NULL)
		return MN_GetActiveMenu();

	for (i = 0; i < mn.numMenus; i++)
		if (!Q_strncmp(mn.menus[i].name, name, MAX_VAR))
			return &mn.menus[i];

	Sys_Error("Could not find menu '%s'\n", name);
	return NULL;
}


/**
 * @brief This will reinit the current visible menu
 * @note also available as script command mn_reinit
 */
static void MN_ReinitCurrentMenu_f (void)
{
	menu_t* menu = MN_GetActiveMenu();
	/* initialize it */
	if (menu) {
		MN_ExecuteActions(menu, menu->initNode->click);
		Com_DPrintf(DEBUG_CLIENT, "Reinit %s\n", menu->name);
	}
}

static void MN_Modify_f (void)
{
	float value;

	if (Cmd_Argc() < 5)
		Com_Printf("Usage: %s <name> <amount> <min> <max>\n", Cmd_Argv(0));

	value = Cvar_VariableValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(3));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(4));

	Cvar_SetValue(Cmd_Argv(1), value);
}


static void MN_ModifyWrap_f (void)
{
	float value;

	if (Cmd_Argc() < 5)
		Com_Printf("Usage: %s <name> <amount> <min> <max>\n", Cmd_Argv(0));

	value = Cvar_VariableValue(Cmd_Argv(1));
	value += atof(Cmd_Argv(2));
	if (value < atof(Cmd_Argv(3)))
		value = atof(Cmd_Argv(4));
	else if (value > atof(Cmd_Argv(4)))
		value = atof(Cmd_Argv(3));

	Cvar_SetValue(Cmd_Argv(1), value);
}


static void MN_ModifyString_f (void)
{
	qboolean next;
	const char *current, *list;
	char *tp;
	char token[MAX_VAR], last[MAX_VAR], first[MAX_VAR];
	int add;

	if (Cmd_Argc() < 4)
		Com_Printf("Usage: %s <name> <amount> <list>\n", Cmd_Argv(0));

	current = Cvar_VariableString(Cmd_Argv(1));
	add = atoi(Cmd_Argv(2));
	list = Cmd_Argv(3);
	last[0] = 0;
	first[0] = 0;
	next = qfalse;

	while (add) {
		tp = token;
		while (*list && *list != ':')
			*tp++ = *list++;
		if (*list)
			list++;
		*tp = 0;

		if (*token && !first[0])
			Q_strncpyz(first, token, MAX_VAR);

		if (!*token) {
			if (add < 0 || next)
				Cvar_Set(Cmd_Argv(1), last);
			else
				Cvar_Set(Cmd_Argv(1), first);
			return;
		}

		if (next) {
			Cvar_Set(Cmd_Argv(1), token);
			return;
		}

		if (!Q_strncmp(token, current, MAX_VAR)) {
			if (add < 0) {
				if (last[0])
					Cvar_Set(Cmd_Argv(1), last);
				else
					Cvar_Set(Cmd_Argv(1), first);
				return;
			} else
				next = qtrue;
		}
		Q_strncpyz(last, token, MAX_VAR);
	}
}


/**
 * @brief Shows the corresponding strings in menu
 * Example: Optionsmenu - fullscreen: yes
 */
static void MN_Translate_f (void)
{
	qboolean next;
	const char *current, *list;
	char *orig, *trans;
	char original[MAX_VAR], translation[MAX_VAR];

	if (Cmd_Argc() < 4) {
		Com_Printf("Usage: %s <source> <dest> <list>\n", Cmd_Argv(0));
		return;
	}

	current = Cvar_VariableString(Cmd_Argv(1));
	list = Cmd_Argv(3);
	next = qfalse;

	while (*list) {
		orig = original;
		while (*list && *list != ':')
			*orig++ = *list++;
		*orig = 0;
		list++;

		trans = translation;
		while (*list && *list != ',')
			*trans++ = *list++;
		*trans = 0;
		list++;

		if (!Q_strncmp(current, original, MAX_VAR)) {
			Cvar_Set(Cmd_Argv(2), _(translation));
			return;
		}
	}

	/* nothing found, copy value */
	Cvar_Set(Cmd_Argv(2), _(current));
}

/**
 * @brief Calls script function on cvar change
 * @note This is for inline editing of cvar values
 * The cvarname_changed and cvarname_aborted function are called,
 * the editing is activated and ended here
 * @note If you want to differ between a changed value of a cvar and
 * the abort of a cvar value change (e.g. for singleplayer savegames
 * you don't want to save the game when the player hits esc) you can
 * define the cvarname_aborted confunc to handle this case, too.
 * Done by the script command msgmenu [?|!|:][cvarname]
 * @sa Key_Message
 * @sa CL_ChangeName_f
 */
static void CL_MessageMenu_f (void)
{
	static char nameBackup[MAX_CVAR_EDITING_LENGTH];
	static char cvarName[MAX_VAR];
	const char *msg;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <msg>[cvarname]: msg is a cvarname prefix - one of [?|!|:]\n", Cmd_Argv(0));
		return;
	}

	msg = Cmd_Argv(1);
	switch (msg[0]) {
	case '?':
		/* start */
		Cbuf_AddText("messagemenu\n");
		Q_strncpyz(cvarName, msg + 1, sizeof(cvarName));
		Q_strncpyz(nameBackup, Cvar_VariableString(cvarName), mn_inputlength->integer);
		Q_strncpyz(msg_buffer, nameBackup, sizeof(msg_buffer));
		msg_bufferlen = strlen(nameBackup);
		break;
	case '\'':
		if (!*cvarName)
			break;
		/* abort without doing anything */
		nameBackup[0] = cvarName[0] = '\0';
		break;
	case '!':
		if (!*cvarName)
			break;
		/* cancel */
		Cvar_ForceSet(cvarName, nameBackup);
		/* call trigger function */
		if (Cmd_Exists(va("%s_aborted", cvarName))) {
			Cbuf_AddText(va("%s_aborted\n", cvarName));
		} else {
			Cbuf_AddText(va("%s_changed\n", cvarName));
		}
		/* don't restore this the next time */
		nameBackup[0] = cvarName[0] = '\0';
		break;
	case ':':
		if (!*cvarName)
			break;
		/* end */
		Cvar_ForceSet(cvarName, msg + 1);
		/* call trigger function */
		Cbuf_AddText(va("%s_changed\n", cvarName));
		nameBackup[0] = cvarName[0] = '\0';
		break;
	default:
		if (!*cvarName)
			break;
		/* continue */
		Cvar_ForceSet(cvarName, msg);
		break;
	}
}

/**
 * @brief Check the if conditions for a given node
 * @sa MN_DrawMenus
 * @sa V_IF
 * @returns qfalse if the node is not drawn due to not meet if conditions
 */
qboolean MN_CheckCondition (menuNode_t *node)
{
	if (node->depends.var) {
		if (!node->depends.cvar || Q_strcmp(node->depends.cvar->name, node->depends.var))
			node->depends.cvar = Cvar_Get(node->depends.var, node->depends.value ? node->depends.value : "", 0, "Menu if condition cvar");
		assert(node->depends.cvar);

		/* menuIfCondition_t */
		switch (node->depends.cond) {
		case IF_EQ:
			assert(node->depends.value);
			if (atof(node->depends.value) != node->depends.cvar->value)
				return qfalse;
			break;
		case IF_LE:
			assert(node->depends.value);
			if (node->depends.cvar->value > atof(node->depends.value))
				return qfalse;
			break;
		case IF_GE:
			assert(node->depends.value);
			if (node->depends.cvar->value < atof(node->depends.value))
				return qfalse;
			break;
		case IF_GT:
			assert(node->depends.value);
			if (node->depends.cvar->value <= atof(node->depends.value))
				return qfalse;
			break;
		case IF_LT:
			assert(node->depends.value);
			if (node->depends.cvar->value >= atof(node->depends.value))
				return qfalse;
			break;
		case IF_NE:
			assert(node->depends.value);
			if (node->depends.cvar->value == atof(node->depends.value))
				return qfalse;
			break;
		case IF_EXISTS:
			assert(node->depends.cvar->string);
			if (!*node->depends.cvar->string)
				return qfalse;
			break;
		case IF_STR_EQ:
			assert(node->depends.value);
			assert(node->depends.cvar->string);
			if (Q_strncmp(node->depends.cvar->string, node->depends.value, MAX_VAR))
				return qfalse;
			break;
		case IF_STR_NE:
			assert(node->depends.value);
			assert(node->depends.cvar->string);
			if (!Q_strncmp(node->depends.cvar->string, node->depends.value, MAX_VAR))
				return qfalse;
			break;
		default:
			Sys_Error("Unknown condition for if statement: %i\n", node->depends.cond);
			break;
		}
	}
	return qtrue;
}

/**
 * @brief Reset and free the menu data hunk
 * @note Even called in case of an error when CL_Shutdown was called - maybe even
 * before CL_InitLocal (and thus MN_ResetMenus) was called
 * @sa CL_Shutdown
 * @sa MN_ResetMenus
 */
void MN_Shutdown (void)
{
	if (mn.adataize)
		Mem_Free(mn.adata);
	mn.adata = NULL;
	mn.adataize = 0;
}

#define MENU_HUNK_SIZE 0x40000

void MN_Init (void)
{
	/* reset menu structures */
	memset(&mn, 0, sizeof(mn));

	/* add cvars */
	mn_escpop = Cvar_Get("mn_escpop", "1", 0, NULL);

	/* add menu comamnds */
	Cmd_AddCommand("mn_reinit", MN_ReinitCurrentMenu_f, "This will reinit the current menu (recall the init function)");
	Cmd_AddCommand("mn_modify", MN_Modify_f, NULL);
	Cmd_AddCommand("mn_modifywrap", MN_ModifyWrap_f, NULL);
	Cmd_AddCommand("mn_modifystring", MN_ModifyString_f, NULL);
	Cmd_AddCommand("mn_translate", MN_Translate_f, NULL);
	Cmd_AddCommand("msgmenu", CL_MessageMenu_f, "Activates the inline cvar editing");

	Cmd_AddCommand("mn_push", MN_PushMenu_f, "Push a menu to the menustack");
	Cmd_AddParamCompleteFunction("mn_push", MN_CompletePushMenu);
	Cmd_AddCommand("mn_push_copy", MN_PushCopyMenu_f, NULL);
	Cmd_AddCommand("mn_pop", MN_PopMenu_f, "Pops the current menu from the stack");
	Cmd_AddCommand("hidehud", MN_PushNoHud_f, _("Hide the HUD (press ESC to reactivate HUD)"));

	/* 256kb - FIXME: Get rid of adata, curadata and adataize */
	mn.adataize = MENU_HUNK_SIZE;
	mn.adata = (byte*)Mem_PoolAlloc(mn.adataize, cl_menuSysPool, CL_TAG_MENU);
	mn.curadata = mn.adata;

	MN_InitNodes();
	MN_MessageInit();
}

