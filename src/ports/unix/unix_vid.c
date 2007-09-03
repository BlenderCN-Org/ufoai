/**
 * @file unix_vid.c
 * @brief Main windowed and fullscreen graphics interface module.
 * @note This module is used for the OpenGL rendering versions of the UFO refresh engine.
 */

/*
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

#include <assert.h>
#include <dlfcn.h> /* ELF dl loader */
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
/* #include <uuid/uuid.h> */

#include "../../client/client.h"

#include "unix_input.h"

/* Structure containing functions exported from refresh DLL */
refexport_t	re;

/* Console variables that we need to access from this module */
extern cvar_t *vid_xpos;			/* X coordinate of window position */
extern cvar_t *vid_ypos;			/* Y coordinate of window position */
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_grabmouse;
extern cvar_t *vid_gamma;
extern cvar_t *vid_ref;			/* Name of Refresh DLL loaded */

/* Global variables used internally by this module */
extern viddef_t viddef;				/* global video state; used by other modules */
void		*reflib_library;		/* Handle to refresh DLL */
qboolean	reflib_active = qfalse;

/*
==========================================================================
DLL GLUE
==========================================================================
*/

int maxVidModes;
/**
 * @brief
 */
const vidmode_t vid_modes[] =
{
	{ 320, 240,   0 },
	{ 400, 300,   1 },
	{ 512, 384,   2 },
	{ 640, 480,   3 },
	{ 800, 600,   4 },
	{ 960, 720,   5 },
	{ 1024, 768,  6 },
	{ 1152, 864,  7 },
	{ 1280, 1024, 8 },
	{ 1600, 1200, 9 },
	{ 2048, 1536, 10 },
	{ 1024,  480, 11 }, /* Sony VAIO Pocketbook */
	{ 1152,  768, 12 }, /* Apple TiBook */
	{ 1280,  854, 13 }, /* Apple TiBook */
	{ 640,  400, 14 }, /* generic 16:10 widescreen*/
	{ 800,  500, 15 }, /* as found modern */
	{ 1024,  640, 16 }, /* notebooks    */
	{ 1280,  800, 17 },
	{ 1680, 1050, 18 },
	{ 1920, 1200, 19 },
	{ 1400, 1050, 20 }, /* samsung x20 */
	{ 1440, 900, 21 }
};

/**
 * @brief
 */
static void VID_NewWindow (int width, int height)
{
	viddef.width  = width;
	viddef.height = height;

	viddef.rx = (float)width  / VID_NORM_WIDTH;
	viddef.ry = (float)height / VID_NORM_HEIGHT;
}

/**
 * @brief
 */
static void VID_FreeReflib (void)
{
	if (reflib_library) {
		IN_Shutdown();
#ifndef REF_HARD_LINKED
		Sys_FreeLibrary(reflib_library);
#endif
	}

	memset(&re, 0, sizeof(re));
	reflib_library = NULL;
	reflib_active = qfalse;
}

/**
 * @brief
 */
static qboolean VID_LoadRefresh (const char *name)
{
	refimport_t ri;
	GetRefAPI_t GetRefAPI;
	qboolean restart = qfalse;
	extern uid_t saved_euid;

	if (reflib_active) {
#if 0
		IN_Shutdown();
#endif
		re.Shutdown();
		VID_FreeReflib();
		restart = qtrue;
	}

	Com_Printf("------- Loading %s -------\n", name);

	/*regain root */
	seteuid(saved_euid);

	if ((reflib_library = Sys_LoadLibrary(name, 0)) == 0)
		return qfalse;

	Com_Printf("Sys_LoadLibrary (\"%s\")\n", name);

	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_CreatePath = FS_CreatePath;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_WriteFile = FS_WriteFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_CheckFile = FS_CheckFile;
	ri.FS_ListFiles = FS_ListFiles;
	ri.FS_Gamedir = FS_Gamedir;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Cvar_ForceSet = Cvar_ForceSet;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_NewWindow = VID_NewWindow;
	ri.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;
	ri.CL_GetFontData = CL_GetFontData;

	ri.genericPool = &vid_genericPool;
	ri.imagePool = &vid_imagePool;
	ri.lightPool = &vid_lightPool;
	ri.modelPool = &vid_modelPool;

	ri.TagMalloc = VID_TagAlloc;
	ri.TagFree = VID_MemFree;
	ri.FreeTags = VID_FreeTags;

	if ((GetRefAPI = (void *) dlsym(reflib_library, "GetRefAPI")) == 0)
		Com_Error(ERR_FATAL, "dlsym failed on %s", name);

	re = GetRefAPI(ri);

	if (re.api_version != API_VERSION) {
		VID_FreeReflib();
		Com_Error(ERR_FATAL, "%s has incompatible api_version", name);
	}

	if (re.Init(0, 0) == qfalse) {
		re.Shutdown();
		VID_FreeReflib();
		return qfalse;
	}

	Real_IN_Init(reflib_library);

	/* give up root now */
	setreuid(getuid(), getuid());
	setegid(getgid());

	/* vid_restart */
	if (restart)
		CL_InitFonts();

	Com_Printf("------------------------------------\n");

	reflib_active = qtrue;

	return qtrue;
}

/**
 * @brief This function gets called once just before drawing each frame, and it's sole purpose in life
 * is to check to see if any of the video mode parameters have changed, and if they have to
 * update the rendering DLL and/or video mode to match.
 */
void VID_CheckChanges (void)
{
	char name[MAX_VAR];

	if (vid_ref->modified)
		S_StopAllSounds();

	while (vid_ref->modified) {
		/* refresh has changed */
		vid_ref->modified = qfalse;
		vid_fullscreen->modified = qtrue;
		cl.refresh_prepped = qfalse;
		cls.disable_screen = qtrue;
		Com_sprintf(name, sizeof(name), "ref_%s", vid_ref->string);

		if (!VID_LoadRefresh(name)) {
			Cmd_ExecuteString("condump ref_debug");

			Com_Error(ERR_FATAL, "Couldn't initialize OpenGL renderer!\nConsult ref_debug.txt for further information.");
		}
		cls.disable_screen = qfalse;
	}
}

/**
 * @brief
 */
void Sys_Vid_Init (void)
{
	/* Create the video variables so we know how to start the graphics drivers */
	vid_ref = Cvar_Get("vid_ref", "sdl", CVAR_ARCHIVE, "Video renderer");

	maxVidModes = VID_NUM_MODES;
}

/**
 * @brief
 */
void VID_Shutdown (void)
{
	if (reflib_active) {
		if (KBD_Close_fp)
			KBD_Close_fp();
		KBD_Close_fp = NULL;
		IN_Shutdown();
		re.Shutdown();
		VID_FreeReflib();
	}
}
