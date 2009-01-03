/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

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

#include "environment.h"

#include "stream/textstream.h"
#include "stream/stringstream.h"
#include "os/path.h"

#include <glib.h>

static CopiedString home_path;
static CopiedString app_path;

const char* environment_get_home_path (void)
{
	return home_path.c_str();
}

const char* environment_get_app_path (void)
{
	return app_path.c_str();
}


#ifdef _WIN32
#define RADIANT_HOME "UFOAI/"
#else
#define RADIANT_HOME ".ufoai/"
#endif
#define RADIANT_DIRECTORY "radiant/"

void environment_init (void)
{
	StringOutputStream path(256);
	path << DirectoryCleaned(g_get_home_dir()) << RADIANT_HOME << RADIANT_DIRECTORY;
	g_mkdir_with_parents(path.c_str(), 0775);
	home_path = path.c_str();

	path.clear();
#ifdef PKGDATADIR
	if (g_file_test(PKGDATADIR"/"RADIANT_DIRECTORY, (GFileTest)G_FILE_TEST_IS_DIR) && g_path_is_absolute(path.c_str())
		app_path << PKGDATADIR"/"RADIANT_DIRECTORY;
	else
#endif
	{
		gchar *currentDir = g_get_current_dir();
		path.clear();
		path << DirectoryCleaned(currentDir);
		app_path = path.c_str();
		g_free(currentDir);
	}
}
