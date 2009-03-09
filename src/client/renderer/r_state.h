/**
 * @file r_state.h
 * @brief
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef R_STATE_H
#define R_STATE_H

#include "r_program.h"
#include "r_material.h"

/* vertex arrays are used for many things */
#define MAX_GL_ARRAY_LENGTH 0x40000
extern const float default_texcoords[];

/** @brief texunits maintain multitexture state */
typedef struct gltexunit_s {
	qboolean enabled;	/**< GL_TEXTURE_2D on / off */
	GLenum texture;		/**< e.g. GL_TEXTURE0_ARB */
	GLint texnum;		/**< e.g 123 */
	GLenum texenv;		/**< e.g. GL_MODULATE */
	GLfloat texcoord_array[MAX_GL_ARRAY_LENGTH * 2];
} gltexunit_t;

#define MAX_GL_TEXUNITS		4

/* these are defined for convenience */
#define texunit_diffuse		r_state.texunits[0]
#define texunit_lightmap	r_state.texunits[1]
#define texunit_deluxemap	r_state.texunits[2]
#define texunit_normalmap	r_state.texunits[3]

typedef struct {
	qboolean fullscreen;

	/* arrays */
	GLfloat vertex_array_3d[MAX_GL_ARRAY_LENGTH * 3];
	GLshort vertex_array_2d[MAX_GL_ARRAY_LENGTH * 2];
	GLfloat color_array[MAX_GL_ARRAY_LENGTH * 4];
	GLfloat normal_array[MAX_GL_ARRAY_LENGTH * 3];
	GLfloat tangent_array[MAX_GL_ARRAY_LENGTH * 3];

	/* multitexture texunits */
	gltexunit_t texunits[MAX_GL_TEXUNITS];

	/* texunit in use */
	gltexunit_t *active_texunit;

	r_shader_t shaders[MAX_SHADERS];
	r_program_t programs[MAX_PROGRAMS];
	r_program_t *default_program;
	r_program_t *warp_program;
	r_program_t *active_program;

	vec4_t color;

	/* blend function */
	GLenum blend_src, blend_dest;

	qboolean ortho;

	material_t *active_material;

	/* states */
	qboolean blend_enabled;
	qboolean color_array_enabled;
	qboolean alpha_test_enabled;
	qboolean lighting_enabled;
	qboolean bumpmap_enabled;
	qboolean warp_enabled;
	qboolean fog_enabled;
} rstate_t;

extern rstate_t r_state;

void R_StatePrint(void);

void R_SetDefaultState(void);
void R_SetupGL2D(void);
void R_SetupGL3D(void);

void R_TexEnv(GLenum value);
void R_BlendFunc(GLenum src, GLenum dest);

qboolean R_SelectTexture(gltexunit_t *texunit);

void R_BindTexture(int texnum);
void R_BindLightmapTexture(GLuint texnum);
void R_BindDeluxemapTexture(GLuint texnum);
void R_BindNormalmapTexture(GLuint texnum);
void R_BindBuffer(GLenum target, GLenum type, GLuint id);
void R_BindArray(GLenum target, GLenum type, void *array);
void R_BindDefaultArray(GLenum target);

void R_EnableTexture(gltexunit_t *texunit, qboolean enable);
void R_EnableBlend(qboolean enable);
void R_EnableAlphaTest(qboolean enable);
void R_EnableColorArray(qboolean enable);
void R_EnableLighting(r_program_t *program, qboolean enable);
void R_EnableBumpmap(material_t *material, qboolean enable);
void R_EnableWarp(r_program_t *program, qboolean enable);
void R_EnableFog(qboolean enable);

#endif
