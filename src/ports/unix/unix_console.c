/**
 * @file unix_console.c
 * @brief console functions for *nix ports
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

#ifdef HAVE_CURSES
#include "unix_curses.h"
#endif /* HAVE_CURSES */

void Sys_ShowConsole (qboolean show)
{
}

/**
 * @brief Shutdown the console
 */
void Sys_ConsoleShutdown (void)
{
#ifdef HAVE_CURSES
	Curses_Shutdown();
#endif
}

/**
 * @brief initialise the console
 */
void Sys_ConsoleInit (void)
{
#ifdef HAVE_CURSES
	Curses_Init();
#endif /* HAVE_CURSES */
}

const char *Sys_ConsoleInput (void)
{
#ifdef HAVE_CURSES
	return Curses_Input();
#else
	static qboolean stdin_active = qtrue;
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (!sv_dedicated || !sv_dedicated->integer)
		return NULL;

	if (!stdin_active)
		return NULL;

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select(1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
		return NULL;

	len = read(0, text, sizeof(text));
	if (len == 0) { /* eof! */
		stdin_active = qfalse;
		return NULL;
	}

	if (len < 1)
		return NULL;
	text[len - 1] = '\0'; /* rip off the \n and terminate */

	return text;
#endif /* HAVE_CURSES */
}

void Sys_ConsoleOutput (const char *string)
{
#ifdef HAVE_CURSES
	Curses_Output(string);
#else
	char text[2048];
	int i, j;

	i = j = 0;

	/* skip color char */
	if (*string == 1)
		string++;

	/* strip high bits */
	while (string[j]) {
		text[i] = string[j] & SCHAR_MAX;

		/* strip low bits */
		if (text[i] >= 32 || text[i] == '\n' || text[i] == '\t')
			i++;

		j++;

		if (i == sizeof(text) - 2) {
			text[i++] = '\n';
			break;
		}
	}
	text[i] = 0;

	fputs(string, stdout);
#endif /* HAVE_CURSES */
}
