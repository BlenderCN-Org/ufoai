/**
 * @file r_lightmap.h
 * @brief lightmap definitions
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/ref_gl/gl_local.h
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

#ifndef R_LIGHTMAP_H
#define R_LIGHTMAP_H

#define	LIGHTMAP_BLOCK_WIDTH	256
#define	LIGHTMAP_BLOCK_HEIGHT	256

void R_BlendLightmaps(const model_t* mod);
void R_CreateSurfaceLightmap(mBspSurface_t * surf);

typedef struct lightmap_sample_s {
	vec3_t point;
	vec3_t color;
} lightmap_sample_t;

extern lightmap_sample_t r_lightmap_sample;
void R_LightPoint(vec3_t p);

#endif
