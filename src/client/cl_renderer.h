/**
 * @file cl_renderer.h
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/ref.h
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

#ifndef CLIENT_REF_H
#define CLIENT_REF_H

#include "../common/common.h"

#include <SDL.h>

#define EARTH_RADIUS 8192.0f
#define MOON_RADIUS 1024.0f

#define VID_NORM_WIDTH		1024
#define VID_NORM_HEIGHT		768

#define	MAX_ENTITIES	512
#define MAX_PTL_ART		512
#define MAX_PTLS		2048

#define MAX_ANIMLIST	8

typedef struct animState_s {
	int frame, oldframe;
	float backlerp;				/**< linear interpolation from previous frame */
	int time, dt;
	int mesh;

	byte list[MAX_ANIMLIST];
	byte lcur, ladd;
	byte change;
} animState_t;

/*============================================================================= */

#define MAX_GL_LIGHTS 8

typedef struct {
	vec3_t origin;
	vec4_t color;
	vec4_t ambient;
	float intensity;
} light_t;

typedef struct {
	struct model_s *model;
	const char *name;				/**< model path */

	float *origin;			/**< pointer to node/menumodel origin */
	float *angles;			/**< pointer to node/menumodel angles */
	float *scale;			/**< pointer to node/menumodel scale */
	float *center;			/**< pointer to node/menumodel center */

	int frame, oldframe;	/**< animation frames */
	float backlerp;			/**< linear interpolation from previous frame */

	int skin;				/**< skin number */
	int mesh;				/**< which mesh? md2 models only have one mesh */
	float *color;
} modelInfo_t;


typedef struct ptlCmd_s {
	byte cmd;
	byte type;
	int ref;
} ptlCmd_t;

typedef struct ptlDef_s {
	char name[MAX_VAR];
	ptlCmd_t *init, *run, *think, *round, *physics;
} ptlDef_t;

typedef struct ptlArt_s {
	byte type;
	byte frame;
	char name[MAX_VAR];
	int skin;
	char *art;
} ptlArt_t;

typedef struct ptl_s {
	/* used by ref */
	qboolean inuse;			/**< particle active? */
	qboolean invis;			/**< is this particle invisible */

	ptlArt_t *pic; 			/**< Picture link. */
	ptlArt_t *model;		/**< Model link. */

	byte blend;				/**< blend mode */
	byte style;				/**< style mode */
	vec2_t size;
	vec3_t scale;
	vec4_t color;
	vec3_t s;			/**< current position */
	vec3_t origin;		/**< start position - set initial s position to get this value */
	vec3_t offset;
	vec3_t angles;
	vec3_t lightColor;
	float lightIntensity;
	int levelFlags;
	short stipplePattern;	/**< the GL_LINE_STIPPLE pattern */

	int skin;		/**< model skin to use for this particle */

	struct ptl_s* children;	/**< list of children */
	struct ptl_s* next;		/**< next peer in list */
	struct ptl_s* parent;   /**< pointer to parent */

	/* private */
	ptlDef_t *ctrl;
	int startTime;
	int frame, endFrame;
	float fps;	/**< how many frames per second (animate) */
	float lastFrame;	/**< time (in seconds) when the think function was last executed (perhaps this can be used to delay or speed up particle actions). */
	float tps; /**< think per second - call the think function tps times each second, the first call at 1/tps seconds */
	float lastThink;
	byte thinkFade, frameFade;
	float t;	/**< time that the particle has been active already */
	float dt;	/**< time increment for rendering this particle (delta time) */
	float life;	/**< specifies how long a particle will be active (seconds) */
	int rounds;	/**< specifies how many rounds a particle will be active */
	int roundsCnt;
	vec3_t a;	/**< acceleration vector */
	vec3_t v;	/**< velocity vector */
	vec3_t omega;	/**< the rotation vector for the particle (newAngles = oldAngles + frametime * omega) */
	qboolean physics;	/**< basic physics */
	qboolean autohide;	/**< only draw the particle if the current position is
						 * not higher than the current level (useful for weather
						 * particles) */
	qboolean stayalive;	/**< used for physics particles that hit the ground */
	qboolean weather;	/**< used to identify weather particles (can be switched
						 * off via cvar cl_particleweather) */
} ptl_t;

typedef struct {
	int x, y, width, height;	/**< in virtual screen coordinates */
	float fov_x, fov_y;
	float vieworg[3];
	float viewangles[3];
	float time;					/**< time is used to auto animate */
	int rdflags;				/**< RDF_NOWORLDMODEL, etc */
	int worldlevel;
	int brush_count, alias_count;

	const char *mapZone;	/**< used to replace textures in base assembly */
} refdef_t;

extern refdef_t refdef;

struct model_s *R_RegisterModelShort(const char *name);
struct image_s *R_RegisterPic(const char *name);

void R_Color(const float *rgba);
void R_ColorBlend(const float *rgba);

void R_ModBeginLoading(const char *tiles, qboolean day, const char *pos, const char *mapName);
void R_ModEndLoading(void);
void R_SwitchModelMemPoolTag(void);

void R_LoadTGA(const char *name, byte ** pic, int *width, int *height);
void R_LoadImage(const char *name, byte **pic, int *width, int *height);

void R_FontRegister(const char *name, int size, const char *path, const char *style);
int R_FontDrawString(const char *fontID, int align, int x, int y, int absX, int absY, int maxWidth, int maxHeight, const int lineHeight, const char *c, int box_height, int scroll_pos, int *cur_line, qboolean increaseLine);
void R_FontLength(const char *font, char *c, int *width, int *height);

#endif /* CLIENT_REF_H */
