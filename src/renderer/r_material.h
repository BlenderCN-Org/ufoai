/**
 * @file r_material.h
 * @brief
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

#ifndef R_MATERIAL_H
#define R_MATERIAL_H

/* flags will persist on the stage structures but may also bubble
 * up to the material flags to determine render eligibility */
#define STAGE_TEXTURE		(1 << 0)
#define STAGE_ENVMAP		(1 << 1)
#define STAGE_BLEND			(1 << 2)
#define STAGE_COLOR			(1 << 3)
#define STAGE_PULSE			(1 << 4)
#define STAGE_STRETCH		(1 << 5)
#define STAGE_ROTATE		(1 << 6)
#define STAGE_SCROLL_S		(1 << 7)
#define STAGE_SCROLL_T		(1 << 8)
#define STAGE_SCALE_S		(1 << 9)
#define STAGE_SCALE_T		(1 << 10)
#define STAGE_TERRAIN		(1 << 11)
#define STAGE_LIGHTMAP		(1 << 12)
#define STAGE_ANIM			(1 << 13)

/* set on stages with valid render passes */
#define STAGE_RENDER 		(1 << 31)

/* frame based animation, lerp between consecutive images */
#define MAX_ANIM_FRAMES 8

#define UPDATE_THRESHOLD 0.02

typedef struct rotate_s {
	float hz, dhz;
	float dsin, dcos;
} rotate_t;

typedef struct blendmode_s {
	GLenum src, dest;
} blendmode_t;

typedef struct pulse_s {
	float hz, dhz;
} pulse_t;

typedef struct stretch_s {
	float hz, dhz;
	float amp, damp;
} stretch_t;

typedef struct scroll_s {
	float s, t;
	float ds, dt;
} scroll_t;

typedef struct scale_s {
	float s, t;
} scale_t;

typedef struct terrain_s {
	float floor, ceil;
	float height;
} terrain_t;

typedef struct anim_s {
	int num_frames;
	struct image_s *images[MAX_ANIM_FRAMES];
	float fps;
	float dtime;
	int dframe;
} anim_t;

typedef struct materialStage_s {
	unsigned flags;
	struct image_s *image;
	blendmode_t blend;
	vec3_t color;
	pulse_t pulse;
	stretch_t stretch;
	rotate_t rotate;
	scroll_t scroll;
	scale_t scale;
	terrain_t terrain;
	anim_t anim;
	struct materialStage_s *next;
} materialStage_t;

typedef struct material_s {
	unsigned flags;
	float time;
	materialStage_t *stages;
	int num_stages;
} material_t;

void R_LoadMaterials(const char *map);

#endif
