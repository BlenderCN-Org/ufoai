/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(INCLUDED_QE3_H)
#define INCLUDED_QE3_H

#include <string>

//
// system functions
//
void Sys_SetTitle (const std::string& text, bool modified);

void RunBSP (const char* name);

void QE_InitVFS ();

void QE_brushCountChanged ();
void QE_entityCountChanged ();

bool ConfirmModified (const std::string&title);

// most of the QE globals are stored in this structure
typedef struct
{
		/*!
		 win32: engine full path.
		 unix: user home full path + engine dir.
		 */
		std::string m_userEnginePath;
		/*!
		 cache for m_userEnginePath + mod subdirectory.
		 */
		std::string m_userGamePath;

} QEGlobals_t;

extern QEGlobals_t g_qeglobals;

class SimpleCounter;
extern SimpleCounter g_brushCount;
extern SimpleCounter g_entityCount;

#endif
