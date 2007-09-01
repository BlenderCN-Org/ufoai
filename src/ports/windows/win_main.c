/**
 * @file win_main.c
 * @brief Windows system functions
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

#include "../../common/common.h"
#include "win_local.h"
#include "resource.h"
#include <fcntl.h>
#include <float.h>
#include <direct.h>
#include <io.h>

#if _MSC_VER >= 1300 /* >= MSVC 7.0 */
#include <dbghelp.h>
# ifdef DEBUG
# include <intrin.h>
# endif
#endif

static HMODULE hSh32 = NULL;
static FARPROC procShell_NotifyIcon = NULL;
static NOTIFYICONDATA pNdata;

qboolean s_win95, s_winxp, s_vista;

qboolean ActiveApp;
qboolean Minimized;

uint32_t sys_msg_time;
uint32_t sys_frame_time;

/* Main window handle for life of the client */
HWND cl_hwnd;
/* Main server console handle for life of the server */
static HWND hwnd_Server;

static int consoleBufferPointer = 0;
static byte consoleFullBuffer[16384];

static sizebuf_t console_buffer;
static byte console_buff[8192];

static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
const char		*argv[MAX_NUM_ARGVS];

int SV_CountPlayers(void);

cvar_t* sys_priority;
cvar_t* sys_affinity;
cvar_t* sys_os;

/*
===============================================================================
SYSTEM IO
===============================================================================
*/

/**
 * @brief
 * @sa Sys_DisableTray
 */
void Sys_EnableTray (void)
{
	memset(&pNdata, 0, sizeof(pNdata));

	pNdata.cbSize = sizeof(NOTIFYICONDATA);
#ifdef DEDICATED_ONLY
	pNdata.hWnd = hwnd_Server;
#else
	pNdata.hWnd = cl_hwnd;
#endif
	pNdata.uID = 0;
	pNdata.uCallbackMessage = WM_USER + 4;
	GetWindowText(pNdata.hWnd, pNdata.szTip, sizeof(pNdata.szTip)-1);
	pNdata.hIcon = LoadIcon(global_hInstance, MAKEINTRESOURCE(IDI_ICON2));
	pNdata.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

	hSh32 = LoadLibrary("shell32.dll");
	procShell_NotifyIcon = GetProcAddress(hSh32, "Shell_NotifyIcon");

	procShell_NotifyIcon(NIM_ADD, &pNdata);

	Com_Printf("Minimize to tray enabled.\n");
}

/**
 * @brief
 * @sa Sys_EnableTray
 */
void Sys_DisableTray (void)
{
#ifdef DEDICATED_ONLY
	ShowWindow(hwnd_Server, SW_RESTORE);
#else
	ShowWindow(cl_hwnd, SW_RESTORE);
#endif
	procShell_NotifyIcon(NIM_DELETE, &pNdata);

	if (hSh32)
		FreeLibrary(hSh32);

	procShell_NotifyIcon = NULL;

	Com_Printf("Minimize to tray disabled.\n");
}

/**
 * @brief
 */
void Sys_Minimize (void)
{
#ifdef DEDICATED_ONLY
	SendMessage(hwnd_Server, WM_ACTIVATE, MAKELONG(WA_INACTIVE,1), 0);
#else
	SendMessage(cl_hwnd, WM_ACTIVATE, MAKELONG(WA_INACTIVE,1), 0);
#endif
}

/**
 * @brief
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char text[1024];
	int ret;

	CL_Shutdown();
	Qcommon_Shutdown();

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	if (strlen(text) < 900)
		strcat(text, "\n\nWould you like to debug? (DEVELOPERS ONLY!)\n");

rebox:;

	ret = MessageBox(NULL, text, "UFO:AI Fatal Error", MB_ICONEXCLAMATION | MB_YESNO);

	if (ret == IDYES) {
		ret = MessageBox(NULL, "Please attach your debugger now to prevent the built in exception handler from catching the breakpoint. When ready, press Yes to cause a breakpoint or No to cancel.", "UFO:AI Fatal Error", MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2);
		if (ret == IDYES)
			Sys_DebugBreak();
		else
			goto rebox;
	}

	if (qwclsemaphore)
		CloseHandle(qwclsemaphore);

	ExitProcess(0xDEAD);
}

/**
 * @brief
 */
void Sys_Quit (void)
{
	timeEndPeriod(1);

	CL_Shutdown();
	Qcommon_Shutdown();
	CloseHandle(qwclsemaphore);

	if (procShell_NotifyIcon)
		procShell_NotifyIcon(NIM_DELETE, &pNdata);

	/* exit(0) */
	ExitProcess(0);
}


/**
 * @brief
 */
static void WinError (void)
{
	LPVOID lpMsgBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
	);

	/* Display the string. */
	MessageBox(NULL, (char*)lpMsgBuf, "GetLastError", MB_OK|MB_ICONINFORMATION);

	/* Free the buffer. */
	LocalFree(lpMsgBuf);
}

/**
 * @brief
 */
static void ServerWindowProcCommandExecute (void)
{
	int			ret;
	char		buff[1024];

	*(DWORD *)&buff = sizeof(buff)-2;

	ret = (int)SendDlgItemMessage(hwnd_Server, IDC_COMMAND, EM_GETLINE, 1, (LPARAM)buff);
	if (!ret)
		return;

	buff[ret] = '\n';
	buff[ret+1] = '\0';
	Sys_ConsoleOutput(buff);
	Cbuf_AddText(buff);
	SendDlgItemMessage(hwnd_Server, IDC_COMMAND, WM_SETTEXT, 0, (LPARAM)"");
}

/**
 * @brief
 */
static LRESULT ServerWindowProcCommand (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT idItem = LOWORD(wParam);
	UINT wNotifyCode = HIWORD(wParam);

	switch (idItem) {
	case IDOK:
		switch (wNotifyCode) {
		case BN_CLICKED:
			ServerWindowProcCommandExecute();
			break;
		}
	}
	return FALSE;
}

/**
 * @brief
 */
static LRESULT CALLBACK ServerWindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_COMMAND:
		return ServerWindowProcCommand(hwnd, message, wParam, lParam);
	case WM_ENDSESSION:
		Cbuf_AddText ("quit exiting due to Windows shutdown.\n");
		return TRUE;
	case WM_CLOSE:
		if (SV_CountPlayers()) {
			int ays = MessageBox(hwnd_Server, "There are still players on the server! Really shut it down?", "WARNING!", MB_YESNO + MB_ICONEXCLAMATION);
			if (ays == IDNO)
				return TRUE;
		}
		Cbuf_AddText ("quit terminated by local request.\n");
		return FALSE;
	case WM_CREATE:
		SetTimer(hwnd_Server, 1, 1000, NULL);
		break;
	case WM_ACTIVATE:
		{
			int minimized = (BOOL)HIWORD(wParam);

			if (Minimized && !minimized) {
				int len;
				SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, WM_SETTEXT, 0, (LPARAM)consoleFullBuffer);
				len = (int)SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_GETLINECOUNT, 0, 0);
				SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, EM_LINESCROLL, 0, len);
			}

			Minimized = minimized;
			if (procShell_NotifyIcon) {
				if (minimized && LOWORD(wParam) == WA_INACTIVE) {
					Minimized = qtrue;
					ShowWindow(hwnd_Server, SW_HIDE);
					return FALSE;
				}
			}
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
	case WM_USER + 4:
		if (lParam == WM_LBUTTONDBLCLK) {
			ShowWindow(hwnd_Server, SW_RESTORE);
			SetForegroundWindow(hwnd_Server);
			SetFocus(GetDlgItem(hwnd_Server, IDC_COMMAND));
		}
		return FALSE;
	}

	return FALSE;
}

/**
 * @brief Get current user
 */
const char *Sys_GetCurrentUser (void)
{
#if _MSC_VER >= 1300 /* >= MSVC 7.0 */
	static char s_userName[1024];
	unsigned long size = sizeof(s_userName);

	if (!GetUserName(s_userName, &size))
		Q_strncpyz(s_userName, "player", sizeof(s_userName));

	if (!s_userName[0])
		Q_strncpyz(s_userName, "player", sizeof(s_userName));

	return s_userName;
#else
	return "player";
#endif
}

/**
 * @brief Get current working dir
 */
char *Sys_Cwd (void)
{
	static char cwd[MAX_OSPATH];

	if (_getcwd(cwd, sizeof(cwd) - 1) == NULL)
		return NULL;
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/**
 * @brief De-Normalize path (remove all /)
 * @sa Sys_NormPath
 */
void Sys_OSPath (char* path)
{
	char *tmp = path;

	while (*tmp) {
		if (*tmp == '/')
			*tmp = '\\';
		else
			*tmp = tolower(*tmp);
		tmp++;
	}
}

/**
 * @brief Normalize path (remove all \\ )
 */
void Sys_NormPath (char* path)
{
	char *tmp = path;

	while (*tmp) {
		if (*tmp == '\\')
			*tmp = '/';
		else
			*tmp = tolower(*tmp);
		tmp++;
	}
}

/**
 * @brief Get the home directory in Application Data
 * @note Forced for Windows Vista
 * @note Use the cvar fs_usehomedir if you want to use the homedir for other
 * Windows versions, too
 */
char *Sys_GetHomeDirectory (void)
{
	TCHAR szPath[MAX_PATH];
	static char path[MAX_OSPATH];
	FARPROC qSHGetFolderPath;
	HMODULE shfolder;

	shfolder = LoadLibrary("shfolder.dll");

	if (shfolder == NULL) {
		Com_Printf("Unable to load SHFolder.dll\n");
		return NULL;
	}

	qSHGetFolderPath = GetProcAddress(shfolder, "SHGetFolderPathA");
	if (qSHGetFolderPath == NULL) {
		Com_Printf("Unable to find SHGetFolderPath in SHFolder.dll\n");
		FreeLibrary(shfolder);
		return NULL;
	}

	if (!SUCCEEDED(qSHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, szPath))) {
		Com_Printf("Unable to detect CSIDL_APPDATA\n");
		FreeLibrary(shfolder);
		return NULL;
	}

	Q_strncpyz(path, szPath, sizeof(path));
	Q_strcat(path, "\\UFOAI", sizeof(path));
	FreeLibrary(shfolder);

	if (!CreateDirectory(path, NULL)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			Com_Printf("Unable to create directory \"%s\"\n", path);
			return NULL;
		}
	}
	return path;
}

/**
 * @brief
 */
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	sys_affinity = Cvar_Get("sys_affinity", "1", CVAR_ARCHIVE, "Which core to use - 1 = only first, 2 = only second, 3 = both");
	sys_priority = Cvar_Get("sys_priority", "1", CVAR_ARCHIVE, "Process priority - 0 = normal, 1 = high, 2 = realtime");

#if 0
	/* allocate a named semaphore on the client so the */
	/* front end can tell if it is alive */

	/* mutex will fail if semephore already exists */
	qwclsemaphore = CreateMutex(
		NULL,         /* Security attributes */
		0,            /* owner       */
		"qwcl"); /* Semaphore name      */
	if (!qwclsemaphore)
		Sys_Error("QWCL is already running on this system");
	CloseHandle (qwclsemaphore);

	qwclsemaphore = CreateSemaphore(
		NULL,         /* Security attributes */
		0,            /* Initial count       */
		1,            /* Maximum count       */
		"qwcl"); /* Semaphore name      */
#endif

	timeBeginPeriod(1);

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4) /* at least win nt 4 */
		Sys_Error("UFO: AI requires windows version 4 or greater");
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s) /* win3.x with win32 extensions */
		Sys_Error("UFO: AI doesn't run on Win32s");
	else if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) /* win95, 98, me */
		s_win95 = qtrue;
	else if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) { /* win nt, xp */
		if (vinfo.dwMajorVersion == 5)
			s_winxp = qtrue;
		else if (vinfo.dwMajorVersion == 6)
			s_vista = qtrue;
	}

	if (s_win95)
		sys_os = Cvar_Get("sys_os", "win95", CVAR_SERVERINFO, NULL);
	else if (s_winxp)
		sys_os = Cvar_Get("sys_os", "winXP", CVAR_SERVERINFO, NULL);
	else if (s_vista)
		sys_os = Cvar_Get("sys_os", "winVista", CVAR_SERVERINFO, NULL);
	else
		sys_os = Cvar_Get("sys_os", "win", CVAR_SERVERINFO, NULL);

	if (sv_dedicated->integer) {
		HICON hIcon;
		hwnd_Server = CreateDialog(global_hInstance, MAKEINTRESOURCE(IDD_SERVER_GUI), NULL, (DLGPROC)ServerWindowProc);

		if (!hwnd_Server) {
			WinError();
			Sys_Error("Couldn't create dedicated server window. GetLastError() = %d", (int)GetLastError());
		}

		SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, EM_SETREADONLY, TRUE, 0);

		SZ_Init(&console_buffer, console_buff, sizeof(console_buff));
		console_buffer.allowoverflow = qtrue;

		hIcon = (HICON)LoadImage(global_hInstance,
			MAKEINTRESOURCE(IDI_ICON2),
			IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON),
			GetSystemMetrics(SM_CYSMICON),
			0);

		if (hIcon)
			SendMessage(hwnd_Server, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

		UpdateWindow(hwnd_Server);
		SetForegroundWindow(hwnd_Server);
		SetFocus(GetDlgItem (hwnd_Server, IDC_COMMAND));
	}
}

/**
 * @brief
 */
char *Sys_ConsoleInput (void)
{
	return NULL;
}

/**
 * @brief
 */
void Sys_UpdateConsoleBuffer (void)
{
	if (console_buffer.cursize) {
		int len, buflen;

		buflen = console_buffer.cursize + 1024;

		if (consoleBufferPointer + buflen >= sizeof(consoleFullBuffer)) {
			int		moved;
			char	*p = consoleFullBuffer + buflen;
			char	*q;

			while (p[0] && p[0] != '\n')
				p++;
			p++;
			q = (consoleFullBuffer + buflen);
			moved = (buflen + (int)(p - q));
			memmove(consoleFullBuffer, consoleFullBuffer + moved, consoleBufferPointer - moved);
			consoleBufferPointer -= moved;
			consoleFullBuffer[consoleBufferPointer] = '\0';
		}

		memcpy(consoleFullBuffer+consoleBufferPointer, console_buffer.data, console_buffer.cursize);
		consoleBufferPointer += (console_buffer.cursize - 1);

		if (!Minimized) {
			SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, WM_SETTEXT, 0, (LPARAM)consoleFullBuffer);
			len = (int)SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, EM_GETLINECOUNT, 0, 0);
			SendDlgItemMessage(hwnd_Server, IDC_CONSOLE, EM_LINESCROLL, 0, len);
		}

		SZ_Clear (&console_buffer);
	}
}

/**
 * @brief Print text to the dedicated console
 */
void Sys_ConsoleOutput (const char *string)
{
	char text[2048];
	const char *p;
	char *s;

	if (!sv_dedicated || !sv_dedicated->integer)
		return;

	p = string;
	s = text;

	while (p[0]) {
		if (p[0] == '\n') {
			*s++ = '\r';
		}

		/* r1: strip high bits here */
		*s = (p[0]) & SCHAR_MAX;

		if (s[0] >= 32 || s[0] == '\n' || s[0] == '\t')
			s++;

		p++;

		if ((s - text) >= sizeof(text) - 2) {
			*s++ = '\n';
			break;
		}
	}
	s[0] = '\0';

	SZ_Print(&console_buffer, text);
	Sys_UpdateConsoleBuffer();
}


/**
 * @brief Send Key_Event calls
 */
void Sys_SendKeyEvents (void)
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
		if (!GetMessage(&msg, NULL, 0, 0))
			Sys_Quit();
		sys_msg_time = msg.time;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	/* grab frame time */
	sys_frame_time = timeGetTime();	/* FIXME: should this be at start? */
}


/**
 * @brief
 */
char *Sys_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	if (OpenClipboard(NULL) != 0) {
		HANDLE hClipboardData;

		if ((hClipboardData = GetClipboardData(CF_TEXT)) != 0 ) {
			if ((cliptext = (char*)GlobalLock(hClipboardData)) != 0) {
				data = (char*)malloc( GlobalSize(hClipboardData) + 1);
				strcpy(data, cliptext);
				GlobalUnlock(hClipboardData);
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
==============================================================================
WINDOWS CRAP
==============================================================================
*/

/**
 * @brief
 */
void Sys_AppActivate (void)
{
#ifndef DEDICATED_ONLY
	ShowWindow(cl_hwnd, SW_RESTORE);
	SetForegroundWindow(cl_hwnd);
#endif
}

/*
========================================================================
GAME DLL
========================================================================
*/

static HINSTANCE game_library;

/**
 * @brief
 */
void Sys_UnloadGame (void)
{
	if (!FreeLibrary(game_library))
		Com_Error(ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

/**
 * @brief Loads the game dll
 */
game_export_t *Sys_GetGameAPI (game_import_t *parms)
{
	GetGameApi_t GetGameAPI;
	char	name[MAX_OSPATH];
	const char	*path;

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	/* run through the search paths */
	path = NULL;
	while (1) {
		path = FS_NextPath(path);
		if (!path)
			break;		/* couldn't find one anywhere */
		Com_sprintf(name, sizeof(name), "%s/game.dll", path);
		game_library = LoadLibrary(name);
		if (game_library) {
			Com_DPrintf(DEBUG_SYSTEM, "LoadLibrary (%s)\n", name);
			break;
		} else {
			Com_DPrintf(DEBUG_SYSTEM, "LoadLibrary (%s) failed\n", name);
		}
	}

	if (!game_library) {
		Com_Printf("Could not find any valid game lib\n");
		return NULL;
	}

	GetGameAPI = (GetGameApi_t)GetProcAddress(game_library, "GetGameAPI");
	if (!GetGameAPI) {
		Sys_UnloadGame();
		Com_Printf("Could not load game lib '%s'\n", name);
		return NULL;
	}

	return GetGameAPI(parms);
}


/**
 * @brief
 */
static void ParseCommandLine (LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS)) {
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine) {
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine) {
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

}

HINSTANCE global_hInstance;

/**
 * @brief
 */
static void FixWorkingDirectory (void)
{
	char	*p;
	char	curDir[MAX_PATH];

	GetModuleFileName(NULL, curDir, sizeof(curDir)-1);

	p = strrchr(curDir, '\\');
	p[0] = 0;

	if (strlen(curDir) > (MAX_OSPATH - MAX_QPATH))
		Sys_Error("Current path is too long. Please move your UFO:AI installation to a shorter path.");

	SetCurrentDirectory(curDir);
}

/**
 * @brief Switch to one processor usage for windows system with more than
 * one processor
 */
void Sys_SetAffinityAndPriority (void)
{
	SYSTEM_INFO sysInfo;
	/* 1 - use core #0
	 * 2 - use core #1
	 * 3 - use cores #0 and #1 */
	DWORD_PTR procAffinity = 0;
	HANDLE proc = GetCurrentProcess();

	if (sys_priority->modified) {
		if (sys_priority->integer < 0)
			Cvar_SetValue("sys_priority", 0);
		else if (sys_priority->integer > 2)
			Cvar_SetValue("sys_priority", 2);

		sys_priority->modified = qfalse;

		switch (sys_priority->integer) {
		case 0:
			SetPriorityClass(proc, NORMAL_PRIORITY_CLASS);
			Com_Printf("Priority changed to NORMAL\n");
			break;
		case 1:
			SetPriorityClass(proc, HIGH_PRIORITY_CLASS);
			Com_Printf("Priority changed to HIGH\n");
			break;
		default:
			SetPriorityClass(proc, REALTIME_PRIORITY_CLASS);
			Com_Printf("Priority changed to REALTIME\n");
			break;
		}
	}

	if (sys_affinity->modified) {
		GetSystemInfo(&sysInfo);
		Com_Printf("Found %i processors\n", (int)sysInfo.dwNumberOfProcessors);
		sys_affinity->modified = qfalse;
		if (sysInfo.dwNumberOfProcessors > 1) {
			switch (sys_affinity->integer) {
			case 1:
				Com_Printf("Only use the first core\n");
				procAffinity = 1;
				break;
			case 2:
				Com_Printf("Only use the second core\n");
				procAffinity = 2;
				break;
			case 3:
				Com_Printf("Use both cores\n");
				procAffinity = 3;
				break;
			}
		} else {
			Com_Printf("...only use one processor\n");
		}
		SetThreadAffinityMask(proc, procAffinity);
	}

	CloseHandle(proc);
}

/**
 * @brief
 */
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#if 0
	MSG msg;
#endif

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;

	ParseCommandLine(lpCmdLine);

	/* always change to the current working dir */
	FixWorkingDirectory();

	Qcommon_Init(argc, argv);

	/* main window message loop */
	while (1) {
		/* if at a full screen console, don't update unless needed */
		if (Minimized)
			Sys_Sleep(1);

#if 1
		Sys_SendKeyEvents();
#else
		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			if (!GetMessage(&msg, NULL, 0, 0))
				Com_Quit();
			sys_msg_time = msg.time;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
#endif

#ifndef __MINGW32__
		_controlfp(_PC_24, _MCW_PC);
#endif
		Qcommon_Frame();
	}

	/* never gets here */
	return FALSE;
}

/**
 * @brief Calls the win32 sleep function
 */
void Sys_Sleep (int milliseconds)
{
	if (milliseconds < 1)
		milliseconds = 1;
	Sleep(milliseconds);
}

/**
 * @brief
 * @sa Sys_FreeLibrary
 * @sa Sys_GetProcAddress
 */
void *Sys_LoadLibrary (const char *name, int flags)
{
	char path[MAX_OSPATH];
	HMODULE lib;

	/* first try cpu string */
	Com_sprintf(path, sizeof(path), "%s_"CPUSTRING".dll", name);
	lib = LoadLibrary(path);
	if (lib)
		return lib;

	/* now the general lib */
	Com_sprintf(path, sizeof(path), "%s.dll", name);
	lib = LoadLibrary(path);
	if (lib)
		return lib;

	Com_Printf("Could not load %s\n", name);
	return NULL;
}

/**
 * @brief
 * @sa Sys_LoadLibrary
 */
void Sys_FreeLibrary (void *libHandle)
{
	if (!libHandle)
		Com_Error(ERR_DROP, "Sys_FreeLibrary: No valid handle given");
	if (!FreeLibrary((HMODULE)libHandle))
		Com_Error(ERR_DROP, "Sys_FreeLibrary: dlclose() failed");
}

/**
 * @brief
 * @sa Sys_LoadLibrary
 */
void *Sys_GetProcAddress (void *libHandle, const char *procName)
{
	if (!libHandle)
		Com_Error(ERR_DROP, "Sys_GetProcAddress: No valid libHandle given");
	return GetProcAddress((HMODULE)libHandle, procName);
}
