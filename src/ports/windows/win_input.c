/**
 * @file win_input.c
 * @brief windows mouse code
 * 02/21/97 JCB Added extended DirectInput code to support external controllers.
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

#include "../../client/client.h"
#include "win_local.h"

extern	uint32_t sys_msg_time;

cvar_t *in_mouse;
extern cvar_t *vid_grabmouse;
qboolean in_appactive;

/*
============================================================
KEYBOARD CONTROL
============================================================
*/

HKL kbLayout;

/**
 * @brief Map from windows to ufo keynums
 */
int IN_MapKey (int wParam, int lParam)
{
	static const byte scanToKey[128] = {
		0,			K_ESCAPE,	'1',		'2',		'3',		'4',		'5',		'6',
		'7',		'8',		'9',		'0',		'-',		'=',		K_BACKSPACE,9,		// 0
		'q',		'w',		'e',		'r',		't',		'y',		'u',		'i',
		'o',		'p',		'[',		']',		K_ENTER,	K_CTRL,		'a',		's',	// 1
		'd',		'f',		'g',		'h',		'j',		'k',		'l',		';',
		'\'',		'`',		K_SHIFT,	'\\',		'z',		'x',		'c',		'v',	// 2
		'b',		'n',		'm',		',',		'.',		'/',		K_SHIFT,	'*',
		K_ALT,		' ',		K_CAPSLOCK,	K_F1,		K_F2,		K_F3,		K_F4,		K_F5,	// 3
		K_F6,		K_F7,		K_F8,		K_F9,		K_F10,		K_PAUSE,	0,			K_HOME,
		K_UPARROW,	K_PGUP,		K_KP_MINUS,	K_LEFTARROW,K_KP_5,		K_RIGHTARROW,K_KP_PLUS,	K_END,	// 4
		K_DOWNARROW,K_PGDN,		K_INS,		K_DEL,		0,			0,			0,			K_F11,
		K_F12,		0,			0,			0,			0,			K_APPS,		0,			0,		// 5
		0,			0,			0,			0,			0,			0,			0,			0,
		0,			0,			0,			0,			0,			0,			0,			0,		// 6
		0,			0,			0,			0,			0,			0,			0,			0,
		0,			0,			0,			0,			0,			0,			0,			0		// 7
	};
	int		modified;
	int		scanCode;
	byte	kbState[256];
	byte	result[4];

	/* new stuff */
	switch (wParam) {
	case VK_TAB:
		return K_TAB;
	case VK_RETURN:
		return K_ENTER;
	case VK_ESCAPE:
		return K_ESCAPE;
	case VK_SPACE:
		return K_SPACE;

	case VK_BACK:
		return K_BACKSPACE;
	case VK_UP:
		return K_UPARROW;
	case VK_DOWN:
		return K_DOWNARROW;
	case VK_LEFT:
		return K_LEFTARROW;
	case VK_RIGHT:
		return K_RIGHTARROW;

/*	case VK_LMENU:
	case VK_RMENU:*/
	case VK_MENU:
		return K_ALT;

/*	case VK_LCONTROL:
	case VK_RCONTROL:*/
	case VK_CONTROL:
		return K_CTRL;

	case VK_LSHIFT:
	case VK_RSHIFT:
	case VK_SHIFT:
		return K_SHIFT;

	case VK_CAPITAL:
		return K_CAPSLOCK;

	case VK_F1:
		return K_F1;
	case VK_F2:
		return K_F2;
	case VK_F3:
		return K_F3;
	case VK_F4:
		return K_F4;
	case VK_F5:
		return K_F5;
	case VK_F6:
		return K_F6;
	case VK_F7:
		return K_F7;
	case VK_F8:
		return K_F8;
	case VK_F9:
		return K_F9;
	case VK_F10:
		return K_F10;
	case VK_F11:
		return K_F11;
	case VK_F12:
		return K_F12;

	case VK_INSERT:
		return K_INS;
	case VK_DELETE:
		return K_DEL;
	case VK_NEXT:
		return K_PGDN;
	case VK_PRIOR:
		return K_PGUP;
	case VK_HOME:
		return K_HOME;
	case VK_END:
		return K_END;

	case VK_RWIN:
	case VK_LWIN:
		return K_SUPER;

	case VK_NUMPAD7:
		return K_KP_HOME;
	case VK_NUMPAD8:
		return K_KP_UPARROW;
	case VK_NUMPAD9:
		return K_KP_PGUP;
	case VK_NUMPAD4:
		return K_KP_LEFTARROW;
	case VK_NUMPAD5:
		return K_KP_5;
	case VK_NUMPAD6:
		return K_KP_RIGHTARROW;
	case VK_NUMPAD1:
		return K_KP_END;
	case VK_NUMPAD2:
		return K_KP_DOWNARROW;
	case VK_NUMPAD3:
		return K_KP_PGDN;
	case VK_NUMPAD0:
		return K_KP_INS;
	case VK_DECIMAL:
		return K_KP_DEL;
	case VK_DIVIDE:
		return K_KP_SLASH;
	case VK_SUBTRACT:
		return K_KP_MINUS;
	case VK_ADD:
		return K_KP_PLUS;

	case VK_PAUSE:
		return K_PAUSE;

	default:
		break;
	}

	/* old stuff */
	modified = (lParam >> 16) & 255;
	if (modified < 128) {
		modified = scanToKey[modified];
		if (lParam & (1 << 24)) {
			switch (modified) {
			case 0x0D:
				return K_KP_ENTER;
			case 0x2F:
				return K_KP_SLASH;
			case 0xAF:
				return K_KP_PLUS;
			}
		} else {
			switch (modified) {
			case K_HOME:
				return K_KP_HOME;
			case K_UPARROW:
				return K_KP_UPARROW;
			case K_PGUP:
				return K_KP_PGUP;
			case K_LEFTARROW:
				return K_KP_LEFTARROW;
			case K_RIGHTARROW:
				return K_KP_RIGHTARROW;
			case K_END:
				return K_KP_END;
			case K_DOWNARROW:
				return K_KP_DOWNARROW;
			case K_PGDN:
				return K_KP_PGDN;
			case K_INS:
				return K_KP_INS;
			case K_DEL:
				return K_KP_DEL;
			}
		}
	}

	/* get the VK_ keyboard state */
	if (!GetKeyboardState(kbState))
		return modified;

	/* convert ascii */
	scanCode = (lParam >> 16) & 255;
	if (ToAsciiEx(wParam, scanCode, kbState, (uint16_t *)result, 0, kbLayout) < 1)
		return modified;

	return result[0];
}


/*
============================================================
MOUSE CONTROL
============================================================
*/

/* mouse variables */
static int mouse_oldbuttonstate;
static POINT current_pos;
static qboolean mouseactive = qfalse;	/* false when not focus app */
static qboolean restore_spi = qfalse;
static qboolean mouseinitialized = qfalse;
static int originalmouseparms[3], newmouseparms[3] = {0, 0, 1};
static qboolean mouseparmsvalid = qfalse;
static RECT window_rect;

/**
 * @brief Called when the window gains focus or changes in some way
 * @sa IN_DeactivateMouse
 */
static void IN_ActivateMouse (void)
{
	int width, height;
	int window_center_x, window_center_y;

	if (!mouseinitialized)
		return;
	if (!in_mouse->integer) {
		mouseactive = qfalse;
		return;
	}
	if (mouseactive)
		return;

	mouseactive = qtrue;

	if (mouseparmsvalid)
		restore_spi = SystemParametersInfo(SPI_SETMOUSE, 0, newmouseparms, 0);

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);

	GetWindowRect(cl_hwnd, &window_rect);
	if (window_rect.left < 0)
		window_rect.left = 0;
	if (window_rect.top < 0)
		window_rect.top = 0;
	if (window_rect.right >= width)
		window_rect.right = width - 1;
	if (window_rect.bottom >= height - 1)
		window_rect.bottom = height - 1;

	window_center_x = (window_rect.right + window_rect.left) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	SetCursorPos(window_center_x, window_center_y);

	if (vid_grabmouse->integer) {
		SetCapture(cl_hwnd);
		ClipCursor(&window_rect);
	}
	while (ShowCursor(FALSE) >= 0);
}


/**
 * @brief Called when the window loses focus
 * @sa IN_ActivateMouse
 */
static void IN_DeactivateMouse (void)
{
	if (!mouseinitialized)
		return;
	if (!mouseactive)
		return;

	if (restore_spi)
		SystemParametersInfo(SPI_SETMOUSE, 0, originalmouseparms, 0);

	mouseactive = qfalse;

	if (vid_grabmouse->integer) {
		ClipCursor(NULL);
		ReleaseCapture();
	}
	while (ShowCursor(TRUE) < 0);
}


/**
 * @brief
 */
void IN_StartupMouse (void)
{
	mouseinitialized = qtrue;
	mouseparmsvalid = SystemParametersInfo(SPI_GETMOUSE, 0, originalmouseparms, 0);
}

/**
 * @brief
 */
void IN_MouseEvent (int mstate)
{
	int i;

	if (!mouseinitialized)
		return;

	/* perform button actions for 3 mouse buttons */
	for (i = 0; i < 3; i++) {
		if ((mstate & (1<<i)) && !(mouse_oldbuttonstate & (1 << i)))
			Key_Event(K_MOUSE1 + i, qtrue, sys_msg_time);

		if (!(mstate & (1<<i)) && (mouse_oldbuttonstate & (1 << i)))
			Key_Event(K_MOUSE1 + i, qfalse, sys_msg_time);
	}

	if (vid_grabmouse->modified) {
		if (!vid_grabmouse->integer) {
			ClipCursor(NULL);
			ReleaseCapture();
		} else {
			SetCapture(cl_hwnd);
			ClipCursor(&window_rect);
		}
		vid_grabmouse->modified = qfalse;
	}

	mouse_oldbuttonstate = mstate;
}


/**
 * @brief
 */
void IN_GetMousePos (int *mx, int *my)
{
	if (!mouseactive || !GetCursorPos(&current_pos)
	 || (window_rect.right == window_rect.left) || (window_rect.bottom == window_rect.top)) {
		*mx = VID_NORM_WIDTH / 2;
		*my = VID_NORM_HEIGHT / 2;
	} else {
		*mx = VID_NORM_WIDTH * (current_pos.x - window_rect.left) / (window_rect.right - window_rect.left);
		*my = VID_NORM_HEIGHT * (current_pos.y - window_rect.top) / (window_rect.bottom - window_rect.top);
	}
}


/*
=========================================================================
VIEW CENTERING
=========================================================================
*/

/**
 * @brief
 */
void IN_Init (void)
{
	/* mouse variables */
	in_mouse = Cvar_Get("in_mouse", "1", CVAR_ARCHIVE, NULL);

	IN_StartupMouse();
	kbLayout = GetKeyboardLayout(0);
}

/**
 * @brief
 * @sa IN_Activate
 */
void IN_Shutdown (void)
{
	IN_DeactivateMouse();
}


/**
 * @brief Called when the main window gains or loses focus.
 * @note The window may have been destroyed and recreated between a deactivate and an activate.
 * @sa IN_Shutdown
 */
void IN_Activate (qboolean active)
{
	in_appactive = active;
	mouseactive = !active;		/* force a new window check or turn off */
}


/**
 * @brief Called every frame, even if not generating commands
 */
void IN_Frame (void)
{
	if (!mouseinitialized)
		return;

	if (!in_mouse || !in_appactive) {
		IN_DeactivateMouse();
		return;
	}

	IN_ActivateMouse();
}
