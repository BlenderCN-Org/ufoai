/**
 * @file sv_game.c
 * @brief Interface to the game library.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_game.c
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "server.h"

game_export_t *ge;
static qboolean pfe_pending = qfalse;
static int pfe_mask = 0;
static struct dbuffer *pfe_msg = NULL;
struct dbuffer *sv_msg = NULL;

/**
 * @brief Debug print to server console
 */
static void PF_dprintf (const char *fmt, ...)
{
	char msg[1024];
	va_list argptr;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf("%s", msg);
}


/**
 * @brief Print to a single client
 * @sa SV_BroadcastPrintf
 */
static void PF_cprintf (player_t * player, int level, const char *fmt, ...)
{
	char msg[1024];
	va_list argptr;
	int n;

	if (level == PRINT_NONE)
		return;

	n = 0;
	if (player) {
		n = player->num;
		if (n < 0 || n >= ge->maxplayersperteam)
			return;
	}

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	msg[sizeof(msg)-1] = 0;

	if (player)
		SV_ClientPrintf(svs.clients + n, level, "%s", msg);
	else
		Com_Printf("%s", msg);
}

static void PF_error (const char *fmt, ...) __attribute__((noreturn));
/**
 * @brief Abort the server with a game error
 */
static void PF_error (const char *fmt, ...)
{
	char msg[1024];
	va_list argptr;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	msg[sizeof(msg)-1] = 0;

	Com_Error(ERR_DROP, "Game Error: %s", msg);
}


/**
 * @note Also sets mins and maxs for inline bmodels
 * @sa CM_InlineModel
 */
static void PF_SetModel (edict_t * ent, const char *name)
{
	cBspModel_t *mod;

	assert(ent);

	if (!name)
		Com_Error(ERR_DROP, "PF_SetModel: NULL");

	ent->modelindex = SV_ModelIndex(name);

	/* if it is an inline model, get the size information for it */
	if (name[0] == '*') {
		mod = CM_InlineModel(name);
		assert(mod);
		VectorCopy(ent->origin, mod->origin);
		VectorCopy(mod->mins, ent->mins);
		VectorCopy(mod->maxs, ent->maxs);
		ent->solid = SOLID_BSP;
		SV_LinkEdict(ent);
	}
}

/**
 * @sa CL_ParseConfigString
 */
static void PF_Configstring (int index, const char *val)
{
	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Com_Error(ERR_DROP, "configstring: bad index %i", index);

	if (!val)
		val = "";

	/* change the string in sv */
	/* there may be overflows in i==CS_TILES - but thats ok */
	/* see definition of configstrings and MAX_TILESTRINGS */
	switch (index) {
	case CS_TILES:
	case CS_POSITIONS:
		Q_strncpyz(sv.configstrings[index], val, MAX_TOKEN_CHARS*MAX_TILESTRINGS);
		break;
	default:
		Q_strncpyz(sv.configstrings[index], val, MAX_TOKEN_CHARS);
		break;
	}

	if (Com_ServerState() != ss_loading) {	/* send the update to everyone */
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, svc_configstring);
		NET_WriteShort(msg, index);
		NET_WriteString(msg, val);

		SV_Multicast(~0, msg);
	}
}

static void PF_WriteChar (char c)
{
	NET_WriteChar(pfe_msg, c);
}

static void PF_WriteByte (unsigned char c)
{
	NET_WriteByte(pfe_msg, c);
}

/**
 * @brief Use this if the value might change and you need the position in the buffer
 */
static char* PF_WriteDummyByte (unsigned char c)
{
	char *pos = pfe_msg->end;
	NET_WriteByte(pfe_msg, c);
	return pos;
}

static void PF_WriteShort (int c)
{
	NET_WriteShort(pfe_msg, c);
}

static void PF_WriteLong (int c)
{
	NET_WriteLong(pfe_msg, c);
}

static void PF_WriteString (const char *s)
{
	NET_WriteString(pfe_msg, s);
}

static void PF_WritePos (vec3_t pos)
{
	NET_WritePos(pfe_msg, pos);
}

static void PF_WriteGPos (pos3_t pos)
{
	NET_WriteGPos(pfe_msg, pos);
}

static void PF_WriteDir (vec3_t dir)
{
	NET_WriteDir(pfe_msg, dir);
}

static void PF_WriteAngle (float f)
{
	NET_WriteAngle(pfe_msg, f);
}

static void PF_WriteFormat (const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	NET_V_WriteFormat(pfe_msg, format, ap);
	va_end(ap);
}

static int PF_ReadChar (void)
{
	return NET_ReadChar(sv_msg);
}

static int PF_ReadByte (void)
{
	return NET_ReadByte(sv_msg);
}

static int PF_ReadShort (void)
{
	return NET_ReadShort(sv_msg);
}

static int PF_ReadLong (void)
{
	return NET_ReadLong(sv_msg);
}

static char *PF_ReadString (void)
{
	return NET_ReadString(sv_msg);
}

static void PF_ReadPos (vec3_t pos)
{
	NET_ReadPos(sv_msg, pos);
}

static void PF_ReadGPos (pos3_t pos)
{
	NET_ReadGPos(sv_msg, pos);
}

static void PF_ReadDir (vec3_t vector)
{
	NET_ReadDir(sv_msg, vector);
}

static float PF_ReadAngle (void)
{
	return NET_ReadAngle(sv_msg);
}

static void PF_ReadData (void *buffer, int size)
{
	NET_ReadData(sv_msg, buffer, size);
}

/**
 * @sa NET_V_ReadFormat
 */
static void PF_ReadFormat (const char *format, ...)
{
	va_list ap;

	assert(format);
	if (!*format)	/* PA_NULL */
		return;
	va_start(ap, format);
	NET_V_ReadFormat(sv_msg, format, ap);
	va_end(ap);
}

/**
 * @sa gi.EndEvents
 */
static void PF_EndEvents (void)
{
	if (!pfe_pending)
		return;

	NET_WriteByte(pfe_msg, EV_NULL);
	SV_Multicast(pfe_mask, pfe_msg);
	pfe_pending = qfalse;
	/* freed in SV_Multicast */
	pfe_msg = NULL;
}

/**
 * @sa gi.AddEvent
 */
static void PF_AddEvent (int mask, int eType)
{
	/* finish the last event */
	if (pfe_pending)
		PF_EndEvents();

	/* start the new event */
	pfe_pending = qtrue;
	pfe_mask = mask;
	pfe_msg = new_dbuffer();
	NET_WriteByte(pfe_msg, svc_event);

	/* write header */
	NET_WriteByte(pfe_msg, eType);
}

/**
 * @brief Makes sure the game DLL does not use client, or signed tags
 */
static void *GI_TagAlloc (int size, int tagNum)
{
	if (tagNum < 0)
		tagNum *= -1;

	return _Mem_Alloc(size, qtrue, sv_gameSysPool, tagNum, "GAME DLL", 0);
}

static void GI_MemFree (void *ptr)
{
	_Mem_Free(ptr, "GAME DLL", -1);
}


/**
 * @brief Makes sure the game DLL does not use client, or signed tags
 */
static void GI_FreeTags (int tagNum)
{
	if (tagNum < 0)
		tagNum *= -1;

	_Mem_FreeTag(sv_gameSysPool, tagNum, "GAME DLL", 0);
}

/**
 * @brief Called when either the entire server is being killed, or it is changing to a different game directory.
 * @sa ShutdownGame
 */
void SV_ShutdownGameProgs (void)
{
	uint32_t size;

	if (!ge)
		return;
	ge->Shutdown();
	Sys_UnloadGame();
	size = Mem_PoolSize(sv_gameSysPool);
	if (size > 0)
		Com_Printf("WARNING: Game memory leak (%u bytes)\n", size);
	ge = NULL;
}

/**
 * @brief Init the game subsystem for a new map
 */
void SV_InitGameProgs (void)
{
	game_import_t import;

	/* unload anything we have now */
	if (ge)
		SV_ShutdownGameProgs();

	/* load a new game dll */
	import.bprintf = SV_BroadcastPrintf;
	import.dprintf = PF_dprintf;
	import.cprintf = PF_cprintf;
	import.error = PF_error;

	import.trace = SV_Trace;
	import.linkentity = SV_LinkEdict;
	import.unlinkentity = SV_UnlinkEdict;
	import.boxedicts = SV_AreaEdicts;

	import.TestLine = CM_TestLine;
	import.GrenadeTarget = Com_GrenadeTarget;

	import.MoveCalc = Grid_MoveCalc;
	import.MoveStore = Grid_MoveStore;
	import.MoveLength = Grid_MoveLength;
	import.MoveNext = Grid_MoveNext;
	import.GridHeight = Grid_Height;
	import.GridFall = Grid_Fall;
	import.GridPosToVec = Grid_PosToVec;
	import.GridRecalcRouting = Grid_RecalcRouting;

	import.modelindex = SV_ModelIndex;

	import.setmodel = PF_SetModel;

	import.configstring = PF_Configstring;
	import.PositionedSound = SV_StartSound;

	import.PointContents = SV_PointContents;
	import.GetFootstepSound = SV_GetFootstepSound;

	import.FS_Gamedir = FS_Gamedir;

	import.WriteChar = PF_WriteChar;
	import.WriteByte = PF_WriteByte;
	import.WriteDummyByte = PF_WriteDummyByte;
	import.WriteShort = PF_WriteShort;
	import.WriteLong = PF_WriteLong;
	import.WriteString = PF_WriteString;
	import.WritePos = PF_WritePos;
	import.WriteGPos = PF_WriteGPos;
	import.WriteDir = PF_WriteDir;
	import.WriteAngle = PF_WriteAngle;
	import.WriteFormat = PF_WriteFormat;

	import.EndEvents = PF_EndEvents;
	import.AddEvent = PF_AddEvent;

	import.ReadChar = PF_ReadChar;
	import.ReadByte = PF_ReadByte;
	import.ReadShort = PF_ReadShort;
	import.ReadLong = PF_ReadLong;
	import.ReadString = PF_ReadString;
	import.ReadPos = PF_ReadPos;
	import.ReadGPos = PF_ReadGPos;
	import.ReadDir = PF_ReadDir;
	import.ReadAngle = PF_ReadAngle;
	import.ReadData = PF_ReadData;
	import.ReadFormat = PF_ReadFormat;

	import.GetCharacterValues = Com_GetCharacterValues;

	import.TagMalloc = GI_TagAlloc;
	import.TagFree = GI_MemFree;
	import.FreeTags = GI_FreeTags;

	import.cvar = Cvar_Get;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;
	import.cvar_string = Cvar_VariableString;

	import.argc = Cmd_Argc;
	import.argv = Cmd_Argv;
	import.args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

	import.seed = Sys_Milliseconds();
	import.csi = &csi;

	/* import the server routing table */
	import.map = (void *) &svMap;

	ge = Sys_GetGameAPI(&import);

	if (!ge)
		Com_Error(ERR_DROP, "failed to load game library");
	if (ge->apiversion != GAME_API_VERSION)
		Com_Error(ERR_DROP, "game is version %i, not %i", ge->apiversion, GAME_API_VERSION);

	ge->Init();
}
