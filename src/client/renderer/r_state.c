/**
 * @file r_image.c
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

#include "r_local.h"
#include "r_program.h"
#include "r_error.h"

/* useful for particles, pics, etc.. */
const float default_texcoords[] = {
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0
};

/**
 * @brief Returns qfalse if the texunit is not supported
 */
qboolean R_SelectTexture (gltexunit_t *texunit)
{
	if (texunit == r_state.active_texunit)
		return qtrue;

	/* not supported */
	if (texunit->texture >= r_config.maxTextureUnits + GL_TEXTURE0_ARB)
		return qfalse;

	r_state.active_texunit = texunit;

	qglActiveTexture(texunit->texture);
	qglClientActiveTexture(texunit->texture);
	return qtrue;
}

void R_BindTexture (int texnum)
{
	if (texnum == r_state.active_texunit->texnum)
		return;

	assert(texnum > 0);

	r_state.active_texunit->texnum = texnum;

	glBindTexture(GL_TEXTURE_2D, texnum);
	R_CheckError();
}

void R_BindLightmapTexture (GLuint texnum)
{
	if (texnum == texunit_lightmap.texnum)
		return;  /* small optimization to save state changes */

	R_SelectTexture(&texunit_lightmap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);
}

void R_BindDeluxemapTexture (GLuint texnum)
{
	if (texnum == texunit_deluxemap.texnum)
		return;  /* small optimization to save state changes */

	R_SelectTexture(&texunit_deluxemap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);
}

void R_BindNormalmapTexture (GLuint texnum)
{
	/* small optimization to save state changes  */
	if (texnum == texunit_normalmap.texnum)
		return;

	R_SelectTexture(&texunit_normalmap);

	R_BindTexture(texnum);

	R_SelectTexture(&texunit_diffuse);
}

void R_BindArray (GLenum target, GLenum type, const void *array)
{
	switch (target) {
	case GL_VERTEX_ARRAY:
		glVertexPointer(3, type, 0, array);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		glTexCoordPointer(2, type, 0, array);
		break;
	case GL_COLOR_ARRAY:
		glColorPointer(4, type, 0, array);
		break;
	case GL_NORMAL_ARRAY:
		glNormalPointer(type, 0, array);
		break;
	case GL_TANGENT_ARRAY:
		R_AttributePointer("TANGENT", 4, array);
		break;
	}
}

/**
 * @brief Binds the appropriate shared vertex array to the specified target.
 */
void R_BindDefaultArray (GLenum target)
{
	switch (target) {
	case GL_VERTEX_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.vertex_array_3d);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.active_texunit->texcoord_array);
		break;
	case GL_COLOR_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.color_array);
		break;
	case GL_NORMAL_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.normal_array);
		break;
	case GL_TANGENT_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.tangent_array);
		break;
	}
}

void R_BindBuffer (GLenum target, GLenum type, GLuint id)
{
	if (!qglBindBuffer)
		return;

	if (!r_vertexbuffers->integer)
		return;

	qglBindBuffer(GL_ARRAY_BUFFER, id);

	if (type && id)  /* assign the array pointer as well */
		R_BindArray(target, type, NULL);
}

void R_BlendFunc (GLenum src, GLenum dest)
{
	if (r_state.blend_src == src && r_state.blend_dest == dest)
		return;

	r_state.blend_src = src;
	r_state.blend_dest = dest;

	glBlendFunc(src, dest);
}

void R_EnableBlend (qboolean enable)
{
	if (r_state.blend_enabled == enable)
		return;

	r_state.blend_enabled = enable;

	if (enable) {
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
	} else {
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}
}

void R_EnableAlphaTest (qboolean enable)
{
	if (r_state.alpha_test_enabled == enable)
		return;

	r_state.alpha_test_enabled = enable;

	if (enable)
		glEnable(GL_ALPHA_TEST);
	else
		glDisable(GL_ALPHA_TEST);
}

void R_EnableTexture (gltexunit_t *texunit, qboolean enable)
{
	if (enable == texunit->enabled)
		return;

	texunit->enabled = enable;

	R_SelectTexture(texunit);

	if (enable) {
		/* activate texture unit */
		glEnable(GL_TEXTURE_2D);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		if (texunit == &texunit_lightmap) {
			if (r_lightmap->integer)
				R_TexEnv(GL_REPLACE);
			else
				R_TexEnv(GL_MODULATE);
		}
	} else {
		/* disable on the second texture unit */
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	R_SelectTexture(&texunit_diffuse);
}

void R_EnableColorArray (qboolean enable)
{
	if (r_state.color_array_enabled == enable)
		return;

	r_state.color_array_enabled = enable;

	if (enable)
		glEnableClientState(GL_COLOR_ARRAY);
	else
		glDisableClientState(GL_COLOR_ARRAY);
}

/**
 * @brief Enables hardware-accelerated lighting with the specified program.  This
 * should be called after any texture units which will be active for lighting
 * have been enabled.
 */
void R_EnableLighting (r_program_t *program, qboolean enable)
{
	if (!r_programs->integer)
		return;

	if (enable && (!program || !program->id))
		return;

	if (!r_lights->integer || r_state.lighting_enabled == enable)
		return;

	r_state.lighting_enabled = enable;

	if (enable) {  /* toggle state */
		R_UseProgram(program);

		glEnableClientState(GL_NORMAL_ARRAY);
	} else {
		glDisableClientState(GL_NORMAL_ARRAY);

		R_UseProgram(NULL);
	}
}

static inline void R_UseMaterial (material_t *material)
{
	static float last_b, last_p, last_s, last_h;
	float b, p, s, h;

	if (r_state.active_material == material)
		return;

	r_state.active_material = material;

	if (!r_state.active_material)
		return;

	b = r_state.active_material->bump * r_bumpmap->value;
	if (b != last_b)
		R_ProgramParameter1f("BUMP", b);
	last_b = b;

	p = r_state.active_material->parallax * r_parallax->value;
	if (p != last_p)
		R_ProgramParameter1f("PARALLAX", p);
	last_p = p;

	h = r_state.active_material->hardness * r_hardness->value;
	if (h != last_h)
		R_ProgramParameter1f("HARDNESS", h);
	last_h = h;

	s = r_state.active_material->specular * r_specular->value;
	if (s != last_s)
		R_ProgramParameter1f("SPECULAR", s);
	last_s = s;
}

/**
 * @brief Enables bumpmapping while updating program parameters to reflect the
 * specified material.
 */
void R_EnableBumpmap (material_t *material, qboolean enable)
{
	if (!r_state.lighting_enabled)
		return;

	if (!r_bumpmap->value)
		return;

	R_UseMaterial(material);

	if (r_state.bumpmap_enabled == enable)
		return;

	r_state.bumpmap_enabled = enable;

	if (enable) {  /* toggle state */
		R_EnableAttribute("TANGENT");
		R_ProgramParameter1i("BUMPMAP", 1);
	} else {
		R_DisableAttribute("TANGENT");
		R_ProgramParameter1i("BUMPMAP", 0);
	}
}

void R_EnableWarp (r_program_t *program, qboolean enable)
{
	if (!r_programs->integer)
		return;

	if (enable && (!program || !program->id))
		return;

	if (!r_warp->integer || r_state.warp_enabled == enable)
		return;

	r_state.warp_enabled = enable;

	R_SelectTexture(&texunit_lightmap);

	if (enable) {
		glEnable(GL_TEXTURE_2D);
		R_BindTexture(r_warpTexture->texnum);
		R_UseProgram(program);
	} else {
		glDisable(GL_TEXTURE_2D);
		R_UseProgram(NULL);
	}

	R_SelectTexture(&texunit_diffuse);
}

#define FOG_START	300.0
#define FOG_END		2500.0

void R_EnableFog (qboolean enable)
{
	if (!r_fog->integer || r_state.fog_enabled == enable)
		return;

	r_state.fog_enabled = qfalse;

	if (enable) {
		if ((refdef.weather & WEATHER_FOG) || r_fog->integer == 2) {
			r_state.fog_enabled = qtrue;

			glFogfv(GL_FOG_COLOR, refdef.fog_color);
			glFogf(GL_FOG_DENSITY, 1.0);
			glEnable(GL_FOG);
		}
	} else {
		glFogf(GL_FOG_DENSITY, 0.0);
		glDisable(GL_FOG);
	}
}

/**
 * @sa R_Setup3D
 */
static void MYgluPerspective (GLdouble zNear, GLdouble zFar)
{
	GLdouble xmin, xmax, ymin, ymax, yaspect = (double) refdef.height / refdef.width;

	if (r_isometric->integer) {
		glOrtho(-10 * refdef.fov_x, 10 * refdef.fov_x, -10 * refdef.fov_x * yaspect, 10 * refdef.fov_x * yaspect, -zFar, zFar);
	} else {
		xmax = zNear * tan(refdef.fov_x * M_PI / 360.0);
		xmin = -xmax;

		ymin = xmin * yaspect;
		ymax = xmax * yaspect;

		glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
	}
}

/**
 * @sa R_Setup2D
 */
void R_Setup3D (void)
{
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(refdef.x * viddef.width / viddef.width);
	x2 = ceil((refdef.x + refdef.width) * viddef.width / viddef.width);
	y = floor(viddef.height - refdef.y * viddef.height / viddef.height);
	y2 = ceil(viddef.height - (refdef.y + refdef.height) * viddef.height / viddef.height);

	w = x2 - x;
	h = y - y2;

	glViewport(x, y2, w, h);
	R_CheckError();

	/* set up projection matrix */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	MYgluPerspective(4.0, MAX_WORLD_WIDTH);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90.0, 1.0, 0.0, 0.0);	/* put Z going up */
	glRotatef(90.0, 0.0, 0.0, 1.0);	/* put Z going up */
	glRotatef(-refdef.viewangles[2], 1.0, 0.0, 0.0);
	glRotatef(-refdef.viewangles[0], 0.0, 1.0, 0.0);
	glRotatef(-refdef.viewangles[1], 0.0, 0.0, 1.0);
	glTranslatef(-refdef.vieworg[0], -refdef.vieworg[1], -refdef.vieworg[2]);

	/* retrieve the resulting matrix for other manipulations  */
	glGetFloatv(GL_MODELVIEW_MATRIX, r_locals.world_matrix);

	r_state.ortho = qfalse;

	/* set vertex array pointer */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);

	glEnable(GL_DEPTH_TEST);

	R_CheckError();
}

extern const float SKYBOX_DEPTH;

/**
 * @sa R_Setup3D
 */
void R_Setup2D (void)
{
	/* set 2D virtual screen size */
	glViewport(0, 0, viddef.width, viddef.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	/* switch to orthographic (2 dimensional) projection
	 * don't draw anything before skybox */
	glOrtho(0, viddef.width, viddef.height, 0, 9999.0f, SKYBOX_DEPTH);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	r_state.ortho = qtrue;

	/* bind default vertex array */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_Color(NULL);

	glEnable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);

	R_CheckError();
}

/* global ambient lighting */
static const vec4_t ambient = {
	0.0, 0.0, 0.0, 1.0
};

/* material reflection */
static const vec4_t material = {
	1.0, 1.0, 1.0, 1.0
};

void R_SetDefaultState (void)
{
	int i;

	glClearColor(0, 0, 0, 0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	/* setup vertex array pointers */
	glEnableClientState(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_EnableColorArray(qtrue);
	R_BindDefaultArray(GL_COLOR_ARRAY);
	R_EnableColorArray(qfalse);

	glEnableClientState(GL_NORMAL_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	/* reset gl error state */
	R_CheckError();

	/* setup texture units */
	for (i = 0; i < r_config.maxTextureUnits && i < MAX_GL_TEXUNITS; i++) {
		gltexunit_t *tex = &r_state.texunits[i];
		tex->texture = GL_TEXTURE0_ARB + i;

		R_EnableTexture(tex, qtrue);

		R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

		if (i > 0)  /* turn them off for now */
			R_EnableTexture(tex, qfalse);

		R_CheckError();
	}

	R_SelectTexture(&texunit_diffuse);
	/* alpha test parameters */
	glAlphaFunc(GL_GREATER, 0.01f);

	/* fog parameters */
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, FOG_START);
	glFogf(GL_FOG_END, FOG_END);

	/* polygon offset parameters */
	glPolygonOffset(1, 1);

	/* alpha blend parameters */
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* reset gl error state */
	R_CheckError();
}

void R_TexEnv (GLenum mode)
{
	if (mode == r_state.active_texunit->texenv)
		return;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
	r_state.active_texunit->texenv = mode;
}

const vec4_t color_white = {1, 1, 1, 1};

/**
 * @brief Change the color to given value
 * @param[in] rgba A pointer to a vec4_t with rgba color value
 * @note To reset the color let rgba be NULL
 */
void R_Color (const vec4_t rgba)
{
	const float *color;
	if (rgba)
		color = rgba;
	else
		color = color_white;

	glColor4fv(color);
	R_CheckError();
}
