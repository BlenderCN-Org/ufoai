/**
 * @file win_local.h
 * @brief Win32-specific UFO header file
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

#ifndef WIN_LOCAL_H
#define WIN_LOCAL_H

#ifdef _MSC_VER
#pragma warning( disable : 4229 )  /* mgraph gets this */
#endif

#include <windows.h>
#include <shlobj.h>
#include "../../client/snd_loc.h"

#ifndef DWORD_PTR
# ifndef _WIN64
#  define DWORD_PTR DWORD
# endif
#endif

#define	WINDOW_STYLE	(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE)

extern HINSTANCE global_hInstance;

extern HWND cl_hwnd;
extern qboolean ActiveApp, Minimized;

void IN_Activate(qboolean active);
void IN_MouseEvent(int mstate);

int IN_MapKey(int wParam, int lParam);

#endif /* WIN_LOCAL_H */
