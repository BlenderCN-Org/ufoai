/**
 * @file cl_menu.c
 * @brief Client menu functions.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "client.h"
#include "cl_global.h"
#include "menu/m_main.h"
#include "menu/m_popup.h"

/**
 * @brief Update the menu values with current gametype values
 */
static void MN_UpdateGametype_f (void)
{
	Com_SetGameType();
}

/**
 * @brief Switch to the next multiplayer game type
 * @sa MN_PrevGametype_f
 */
static void MN_ChangeGametype_f (void)
{
	int i, newType;
	gametype_t* gt;
	mapDef_t *md;
	const char *newGameTypeID = NULL;
	qboolean next = qtrue;

	/* no types defined or parsed */
	if (numGTs == 0)
		return;

	md = &csi.mds[ccs.multiplayerMapDefinitionIndex];
	if (!md || !md->multiplayer) {
		Com_Printf("MN_ChangeGametype_f: No mapdef for the map\n");
		return;
	}

	/* previous? */
	if (!Q_strcmp(Cmd_Argv(0), "mn_prevgametype")) {
		next = qfalse;
	}

	if (md->gameTypes) {
		linkedList_t *list = md->gameTypes;
		linkedList_t *old = NULL;
		while (list) {
			if (!Q_strcmp((const char*)list->data, gametype->string)) {
				if (next) {
					if (list->next)
						newGameTypeID = (const char *)list->next->data;
					else
						newGameTypeID = (const char *)md->gameTypes->data;
				} else {
					/* only one or the first entry */
					if (old) {
						newGameTypeID = (const char *)old->data;
					} else {
						while (list->next)
							list = list->next;
						newGameTypeID = (const char *)list->data;
					}
				}
				break;
			}
			old = list;
			list = list->next;
		}
		/* current value is not valid for this map, set to first valid gametype */
		if (!list)
			newGameTypeID = (const char *)md->gameTypes->data;
	} else {
		for (i = 0; i < numGTs; i++) {
			gt = &gts[i];
			if (!Q_strncmp(gt->id, gametype->string, MAX_VAR)) {
				if (next) {
					newType = (i + 1);
					if (newType >= numGTs)
						newType = 0;
				} else {
					newType = (i - 1);
					if (newType < 0)
						newType = numGTs - 1;
				}

				newGameTypeID = gts[newType].id;
				break;
			}
		}
	}
	if (newGameTypeID) {
		Cvar_Set("gametype", newGameTypeID);
		Com_SetGameType();
	}
}

/**
 * @brief Starts a server and checks if the server loads a team unless he is a dedicated
 * server admin
 */
static void MN_StartServer_f (void)
{
	char map[MAX_VAR];
	mapDef_t *md;
	cvar_t* mn_serverday = Cvar_Get("mn_serverday", "1", 0, "Decides whether the server starts the day or the night version of the selected map");
	aircraft_t *aircraft;

	if (ccs.singleplayer)
		return;

	aircraft = AIR_AircraftGetFromIdx(0);
	assert(aircraft);

	if (!sv_dedicated->integer && !B_GetNumOnTeam(aircraft)) {
		Com_Printf("MN_StartServer_f: Multiplayer team not loaded, please choose your team now.\n");
		Cmd_ExecuteString("assign_initial");
		return;
	}

	if (Cvar_VariableInteger("sv_teamplay")
		&& Cvar_VariableValue("sv_maxsoldiersperplayer") > Cvar_VariableValue("sv_maxsoldiersperteam")) {
		MN_Popup(_("Settings doesn't make sense"), _("Set soldiers per player lower than soldiers per team"));
		return;
	}

	md = &csi.mds[ccs.multiplayerMapDefinitionIndex];
	if (!md || !md->multiplayer)
		return;
	assert(md->map);

	Com_sprintf(map, sizeof(map), "map %s%c %s", md->map, mn_serverday->integer ? 'd' : 'n', md->param ? md->param : "");

	/* let the (local) server know which map we are running right now */
	csi.currentMD = md;

	Cmd_ExecuteString(map);

	Cvar_Set("mn_main", "multiplayerInGame");
	MN_PushMenu("multiplayer_wait");
	Cvar_Set("mn_active", "multiplayer_wait");
}

/**
 * @brief Determine the position and size of render
 * @param[in] menu : use its position and size properties
 */
void MN_SetViewRect (const menu_t* menu)
{
	menuNode_t* menuNode = menu ? (menu->renderNode ? menu->renderNode : (menu->popupNode ? menu->popupNode : NULL)): NULL;

	if (!menuNode) {
		/* render the full screen */
		viddef.x = viddef.y = 0;
		viddef.viewWidth = viddef.width;
		viddef.viewHeight = viddef.height;
	} else if (menuNode->invis) {
		/* don't draw the scene */
		viddef.x = viddef.y = 0;
		viddef.viewWidth = viddef.viewHeight = 0;
	} else {
		/* the menu has a view size specified */
		viddef.x = menuNode->pos[0] * viddef.rx;
		viddef.y = menuNode->pos[1] * viddef.ry;
		viddef.viewWidth = menuNode->size[0] * viddef.rx;
		viddef.viewHeight = menuNode->size[1] * viddef.ry;
	}
}


/**
 * @brief Left click on the basemap
 */
void MN_BaseMapClick (menuNode_t *node, int x, int y)
{
	int row, col;
	building_t	*entry;

	assert(baseCurrent);

	if (baseCurrent->buildingCurrent && baseCurrent->buildingCurrent->buildingStatus == B_STATUS_NOT_SET) {
		for (row = 0; row < BASE_SIZE; row++)
			for (col = 0; col < BASE_SIZE; col++)
				if (baseCurrent->map[row][col] == BASE_FREESLOT && x >= baseCurrent->posX[row][col]
					&& x < baseCurrent->posX[row][col] + node->size[0] / BASE_SIZE && y >= baseCurrent->posY[row][col]
					&& y < baseCurrent->posY[row][col] + node->size[1] / BASE_SIZE) {
					/* Set position for a new building */
					B_SetBuildingByClick(row, col);
					return;
				}
	}

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (baseCurrent->map[row][col] > BASE_FREESLOT && x >= baseCurrent->posX[row][col]
				&& x < baseCurrent->posX[row][col] + node->size[0] / BASE_SIZE && y >= baseCurrent->posY[row][col]
				&& y < baseCurrent->posY[row][col] + node->size[1] / BASE_SIZE) {
				entry = B_GetBuildingByIdx(baseCurrent, baseCurrent->map[row][col]);
				if (!entry)
					Sys_Error("MN_BaseMapClick: no entry at %i:%i\n", x, y);

				if (*entry->onClick) {
					baseCurrent->buildingCurrent = entry;
					Cmd_ExecuteString(va("%s %i", entry->onClick, baseCurrent->idx));
					baseCurrent->buildingCurrent = NULL;
					gd.baseAction = BA_NONE;
				}
#if 0
				else {
					/* Click on building : display its properties in the building menu */
					MN_PushMenu("buildings");
					baseCurrent->buildingCurrent = entry;
					B_BuildingStatus(baseCurrent, baseCurrent->buildingCurrent);
				}
#else
				else
					UP_OpenWith(entry->pedia);
#endif
				return;
			}
}

/**
 * @brief Right click on the basemap
 */
void MN_BaseMapRightClick (menuNode_t *node, int x, int y)
{
	int row, col;
	building_t	*entry;

	assert(baseCurrent);

	for (row = 0; row < BASE_SIZE; row++)
		for (col = 0; col < BASE_SIZE; col++)
			if (baseCurrent->map[row][col] > BASE_FREESLOT && x >= baseCurrent->posX[row][col]
				&& x < baseCurrent->posX[row][col] + node->size[0] / BASE_SIZE && y >= baseCurrent->posY[row][col]
				&& y < baseCurrent->posY[row][col] + node->size[1] / BASE_SIZE) {
				entry = B_GetBuildingByIdx(baseCurrent, baseCurrent->map[row][col]);
				if (!entry)
					Sys_Error("MN_BaseMapClick: no entry at %i:%i\n", x, y);
				B_MarkBuildingDestroy(baseCurrent, entry);
				return;
			}
}

/**
 * @brief Prints a list of tab and newline seperated string to keylist char array that hold the key and the command desc
 */
static void MN_InitKeyList_f (void)
{
	static char keylist[2048];
	int i;

	*keylist = '\0';

	for (i = 0; i < K_LAST_KEY; i++)
		if (keybindings[i] && keybindings[i][0]) {
			Com_Printf("%s - %s\n", Key_KeynumToString(i), keybindings[i]);
			Q_strcat(keylist, va("%s\t%s\n", Key_KeynumToString(i), Cmd_GetCommandDesc(keybindings[i])), sizeof(keylist));
		}

	mn.menuText[TEXT_LIST] = keylist;
}

/**
 * @brief Prints the map info
 */
static void MN_MapInfo (int step)
{
	int i = 0;
	mapDef_t *md;

	if (!csi.numMDs)
		return;

	ccs.multiplayerMapDefinitionIndex += step;

	if (ccs.multiplayerMapDefinitionIndex < 0)
		ccs.multiplayerMapDefinitionIndex = csi.numMDs - 1;

	ccs.multiplayerMapDefinitionIndex %= csi.numMDs;

	if (!ccs.singleplayer) {
		while (!csi.mds[ccs.multiplayerMapDefinitionIndex].multiplayer) {
			i++;
			ccs.multiplayerMapDefinitionIndex += (step ? step : 1);
			if (ccs.multiplayerMapDefinitionIndex < 0)
				ccs.multiplayerMapDefinitionIndex = csi.numMDs - 1;
			ccs.multiplayerMapDefinitionIndex %= csi.numMDs;
			if (i >= csi.numMDs)
				Sys_Error("MN_MapInfo: There is no multiplayer map in any mapdef\n");
		}
	}

	md = &csi.mds[ccs.multiplayerMapDefinitionIndex];

	Cvar_Set("mn_svmapname", md->map);
	if (FS_CheckFile(va("pics/maps/shots/%s.jpg", md->map)) != -1)
		Cvar_Set("mn_mappic", va("maps/shots/%s.jpg", md->map));
	else
		Cvar_Set("mn_mappic", va("maps/shots/na.jpg"));

	if (FS_CheckFile(va("pics/maps/shots/%s_2.jpg", md->map)) != -1)
		Cvar_Set("mn_mappic2", va("maps/shots/%s_2.jpg", md->map));
	else
		Cvar_Set("mn_mappic2", va("maps/shots/na.jpg"));

	if (FS_CheckFile(va("pics/maps/shots/%s_3.jpg", md->map)) != -1)
		Cvar_Set("mn_mappic3", va("maps/shots/%s_3.jpg", md->map));
	else
		Cvar_Set("mn_mappic3", va("maps/shots/na.jpg"));

	if (!ccs.singleplayer) {
		if (md->gameTypes) {
			linkedList_t *list = md->gameTypes;
			char buf[256] = "";
			while (list) {
				Q_strcat(buf, va("%s ", (const char *)list->data), sizeof(buf));
				list = list->next;
			}
			Cvar_Set("mn_mapgametypes", buf);
			list = md->gameTypes;
			while (list) {
				/* check whether current selected gametype is a valid one */
				if (!Q_strcmp((const char*)list->data, gametype->string)) {
					break;
				}
				list = list->next;
			}
			if (!list)
				MN_ChangeGametype_f();
		} else {
			Cvar_Set("mn_mapgametypes", _("all"));
		}
	}
}

static void MN_GetMaps_f (void)
{
	MN_MapInfo(0);
}

static void MN_NextMap_f (void)
{
	MN_MapInfo(1);
}

static void MN_PrevMap_f (void)
{
	MN_MapInfo(-1);
}

/**
 * @brief Initialize the menu data hunk, add cvars and commands
 * @note Also calls the 'reset' functions for production, basemanagement,
 * aliencontainmenu, employee, hospital and a lot more subfunctions
 * @note This function is called once
 * @sa MN_Shutdown
 * @sa B_ResetBaseManagement
 * @sa CL_InitLocal
 */
void MN_ResetMenus (void)
{
	/* add commands and cvars */
	Cvar_Set("mn_main", "main");
	Cvar_Set("mn_sequence", "sequence");

	/* print the keybindings to mn.menuText */
	Cmd_AddCommand("mn_init_keylist", MN_InitKeyList_f, NULL);

	Cmd_AddCommand("mn_startserver", MN_StartServer_f, NULL);
	Cmd_AddCommand("mn_updategametype", MN_UpdateGametype_f, "Update the menu values with current gametype values");
	Cmd_AddCommand("mn_nextgametype", MN_ChangeGametype_f, "Switch to the next multiplayer game type");
	Cmd_AddCommand("mn_prevgametype", MN_ChangeGametype_f, "Switch to the previous multiplayer game type");
	Cmd_AddCommand("mn_getmaps", MN_GetMaps_f, "The initial map to show");
	Cmd_AddCommand("mn_nextmap", MN_NextMap_f, "Switch to the next multiplayer map");
	Cmd_AddCommand("mn_prevmap", MN_PrevMap_f, "Switch to the previous multiplayer map");

	MN_Init();

	/* reset ufopedia, basemanagement and other subsystems */
	UP_ResetUfopedia();
	B_ResetBaseManagement();
	RS_ResetResearch();
	PR_ResetProduction();
	E_Reset();
	HOS_Reset();
	AC_Reset();
	MAP_ResetAction();
	UFO_Reset();
	TR_Reset();
	BaseSummary_Reset();
}
