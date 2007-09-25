/**
 * @file infostring.c
 * @brief Info string handling
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "../common/common.h"
#include "infostring.h"

/**
 * @brief Searches the string for the given key and returns the associated value, or an empty string.
 * @param[in] s The string you want to extract the keyvalue from
 * @param[in] key The key you want to extract the value for
 * @sa Info_SetValueForKey
 * @return The value or empty string - never NULL
 */
const char *Info_ValueForKey (const char *s, const char *key)
{
	char pkey[512];
	/* use two buffers so compares */
	static char value[2][512];

	/* work without stomping on each other */
	static int valueindex = 0;
	char *o;

	valueindex ^= 1;
	if (*s == '\\' && *s != '\n')
		s++;
	while (1) {
		o = pkey;
		while (*s != '\\' && *s != '\n') {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s != '\n' && *s)
			*o++ = *s++;
		*o = 0;

		if (!Q_stricmp(key, pkey))
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

/**
 * @brief Searches through s for key and remove is
 * @param[in] s String to search key in
 * @param[in] key String to search for in s
 * @sa Info_SetValueForKey
 */
void Info_RemoveKey (char *s, const char *key)
{
	char *start;
	char pkey[512];
	char value[512];
	char *o;

	if (strstr(key, "\\")) {
/*		Com_Printf("Can't use a key with a \\\n"); */
		return;
	}

	while (1) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!Q_strncmp(key, pkey, sizeof(pkey))) {
			strcpy(start, s);	/* remove this part */
			return;
		}

		if (!*s)
			return;
	}

}


/**
 * @brief Some characters are illegal in info strings because they can mess up the server's parsing
 */
qboolean Info_Validate (const char *s)
{
	if (strstr(s, "\""))
		return qfalse;
	if (strstr(s, ";"))
		return qfalse;
	return qtrue;
}

/**
 * @brief Adds a new entry into string with given value.
 * @note Removed any old version of the key
 * @param[in] s
 * @sa Info_RemoveKey
 */
void Info_SetValueForKey (char *s, const char *key, const char *value)
{
	char newi[MAX_INFO_STRING];

	if (strstr(key, "\\") || strstr(value, "\\")) {
		Com_Printf("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr(key, ";")) {
		Com_Printf("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr(key, "\"") || strstr(value, "\"")) {
		Com_Printf("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_INFO_KEY - 1 || strlen(value) > MAX_INFO_KEY - 1) {
		Com_Printf("Keys and values must be < 64 characters.\n");
		return;
	}
	Info_RemoveKey(s, key);
	if (!value || !strlen(value))
		return;

	Com_sprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	Q_strcat(newi, s, sizeof(newi));
	Q_strncpyz(s, newi, MAX_INFO_STRING);
}


void Info_Print (const char *s)
{
	char key[512];
	char value[512];
	char *o;
	int l;

	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20) {
			memset(o, ' ', 20 - l);
			key[20] = 0;
		} else
			*o = 0;
		Com_Printf("%s", key);

		if (!*s) {
			Com_Printf("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf("%s\n", value);
	}
}
