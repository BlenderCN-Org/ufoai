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

#if !defined(INCLUDED_SURFACEDIALOG_H)
#define INCLUDED_SURFACEDIALOG_H

#include <gtk/gtk.h>
#include "selectable.h"

void SurfaceInspector_Construct ();
void SurfaceInspector_Destroy ();
void SurfaceInspector_FitTexture (void);

void FlipTextureX();
void FlipTextureY();

void SelectedFaces_copyTexture ();
void SelectedFaces_pasteTexture ();

void Scene_applyClosestTexture (SelectionTest& test);
void Scene_copyClosestTexture (SelectionTest& test);

// the increment we are using for the surface inspector (this is saved in the prefs)
struct si_globals_t
{
		float shift[2];
		float scale[2];
		float rotate;

		bool m_bSnapTToGrid;

		si_globals_t () :
			m_bSnapTToGrid(false)
		{
			shift[0] = 8.0f;
			shift[1] = 8.0f;
			scale[0] = 0.5f;
			scale[1] = 0.5f;
			rotate = 45.0f;
		}
};
extern si_globals_t g_si_globals;

GtkWidget *SurfaceInspector_constructNotebookTab (void);

#endif
