/**
 * @file r_draw.c
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
#include "r_error.h"

image_t *shadow;
image_t *blood;

int r_globeEarthList;
#define GLOBE_TRIS 60
#define GLOBE_TEXES (GLOBE_TRIS+1)*(GLOBE_TRIS+1)*4
#define GLOBE_VERTS (GLOBE_TRIS+1)*(GLOBE_TRIS+1)*6

/**
 * @brief Initialize the globe chain arrays
 */
static void R_InitGlobeEarthChain (void)
{
	static float globetexes[GLOBE_TEXES];
	static float globeverts[GLOBE_VERTS];

	const float drho = M_PI / GLOBE_TRIS;
	const float dtheta = M_PI / (GLOBE_TRIS / 2);

	const float ds = 1.0f / (GLOBE_TRIS * 2);
	const float dt = 1.0f / GLOBE_TRIS;

	float t = 1.0f;
	float s = 0.0f;

	int i;
	int j;

	int vertspos = 0;
	int texespos = 0;

	for (i = 0; i < GLOBE_TRIS; i++) {
		float rho = (float) i * drho;
		float srho = (float) (sin(rho));
		float crho = (float) (cos(rho));
		float srhodrho = (float) (sin(rho + drho));
		float crhodrho = (float) (cos(rho + drho));

		s = 0.0f;

		for (j = 0; j <= GLOBE_TRIS; j++) {
			float theta = (j == GLOBE_TRIS) ? 0.0f : j * dtheta;
			float stheta = (float) (-sin(theta));
			float ctheta = (float) (cos(theta));

			globetexes[texespos++] = s;
			globetexes[texespos++] = (t - dt);

			globeverts[vertspos++] = stheta * srhodrho * r_3dmapradius->value;
			globeverts[vertspos++] = ctheta * srhodrho * r_3dmapradius->value;
			globeverts[vertspos++] = crhodrho * r_3dmapradius->value;

			globetexes[texespos++] = s;
			globetexes[texespos++] = t;

			globeverts[vertspos++] = stheta * srho * r_3dmapradius->value;
			globeverts[vertspos++] = ctheta * srho * r_3dmapradius->value;
			globeverts[vertspos++] = crho * r_3dmapradius->value;

			s += ds;
		}

		t -= dt;
	}

	vertspos = 0;
	texespos = 0;

	/* build the sphere display list */
	r_globeEarthList = qglGenLists(1);
	R_CheckError();

	qglNewList(r_globeEarthList, GL_COMPILE);
	R_CheckError();

	qglEnable(GL_NORMALIZE);

	for (i = 0; i < GLOBE_TRIS; i++) {
		qglBegin(GL_TRIANGLE_STRIP);

		for (j = 0; j <= GLOBE_TRIS; j++) {
			qglNormal3fv(&globeverts[vertspos]);
			qglTexCoord2fv(&globetexes[texespos]);
			qglVertex3fv(&globeverts[vertspos]);

			texespos += 2;
			vertspos += 3;

			qglNormal3fv(&globeverts[vertspos]);
			qglTexCoord2fv(&globetexes[texespos]);
			qglVertex3fv(&globeverts[vertspos]);

			texespos += 2;
			vertspos += 3;
		}

		qglEnd();
		R_CheckError();
	}

	qglDisable(GL_NORMALIZE);

	qglEndList();
}

/* console font */
static image_t *draw_chars[2];

/**
 * @brief Loads some textures and init the 3d globe
 * @sa R_Init
 */
void R_DrawInitLocal (void)
{
	shadow = R_FindImage("pics/sfx/shadow", it_pic);
	if (shadow == r_notexture)
		Com_Printf("Could not find shadow image in game pics/sfx directory!\n");
	blood = R_FindImage("pics/sfx/blood", it_pic);
	if (blood == r_notexture)
		Com_Printf("Could not find blood image in game pics/sfx directory!\n");
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	R_CheckError();
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	R_CheckError();
	/* load console characters (don't bilerp characters) */

	draw_chars[0] = R_FindImage("pics/conchars", it_pic);
	if (draw_chars[0] == r_notexture)
		Sys_Error("Could not find conchars image in game pics directory!\n");
	draw_chars[1] = R_FindImage("pics/conchars_small", it_pic);
	if (draw_chars[1] == r_notexture)
		Com_Printf("Could not find conchars2 image in game pics directory!\n");

	R_InitGlobeEarthChain();
}

#define MAX_CHARS 8192

/* chars are batched per frame so that they are drawn in one shot */
static float char_texcoords[MAX_CHARS * 4 * 2];
static short char_verts[MAX_CHARS * 4 * 2];
static int char_index = 0;

/**
 * @brief Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void R_DrawChar (int x, int y, int num)
{
	int row, col;
	float frow, fcol;

	num &= 255;

	if ((num & 127) == ' ')		/* space */
		return;

	if (y <= -con_fontHeight)
		return;					/* totally off screen */

	if (char_index >= MAX_CHARS * 8)
		return;

	row = (int) num >> 4;
	col = (int) num & 15;

	/* 0.0625 => 16 cols (conchars images) */
	frow = row * 0.0625;
	fcol = col * 0.0625;

	assert(con_font->integer < 2);
	R_BindTexture(draw_chars[con_font->integer]->texnum);

	char_texcoords[char_index + 0] = fcol;
	char_texcoords[char_index + 1] = frow;
	char_texcoords[char_index + 2] = fcol + 0.0625;
	char_texcoords[char_index + 3] = frow;
	char_texcoords[char_index + 4] = fcol + 0.0625;
	char_texcoords[char_index + 5] = frow + 0.0625;
	char_texcoords[char_index + 6] = fcol;
	char_texcoords[char_index + 7] = frow + 0.0625;

	char_verts[char_index + 0] = x;
	char_verts[char_index + 1] = y;
	char_verts[char_index + 2] = x + con_fontWidth;
	char_verts[char_index + 3] = y;
	char_verts[char_index + 4] = x + con_fontWidth;
	char_verts[char_index + 5] = y + con_fontHeight;
	char_verts[char_index + 6] = x;
	char_verts[char_index + 7] = y + con_fontHeight;

	char_index += 8;
}

void R_DrawChars (void)
{
	R_BindTexture(draw_chars[con_font->integer]->texnum);

	R_EnableBlend(qtrue);

	/* alter the array pointers */
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, char_texcoords);
	R_BindArray(GL_VERTEX_ARRAY, GL_SHORT, char_verts);

	qglDrawArrays(GL_QUADS, 0, char_index / 2);

	char_index = 0;

	R_EnableBlend(qfalse);

	/* and restore them */
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
 }

/**
 * @brief Uploads image data
 * @param[in] name The name of the texture to use for this data
 * @param[in] frame The frame data that is uploaded
 * @param[in] width The width of the texture
 * @param[in] height The height of the texture
 * @return the texture number of the uploaded images
 */
int R_DrawImagePixelData (const char *name, byte *frame, int width, int height)
{
	image_t *img;

	img = R_FindImage(name, it_pic);
	if (img == r_notexture)
		Sys_Error("Could not find the searched image: %s\n", name);

	R_BindTexture(img->texnum);

	if (img->width == width && img->height == height) {
		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img->width, img->height, GL_RGBA, GL_UNSIGNED_BYTE, frame);
	} else {
		/* Reallocate the texture */
		img->width = width;
		img->height = height;
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame);
	}
	R_CheckError();
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	R_CheckError();
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	R_CheckError();

	return img->texnum;
}

/**
 * @brief Searches for an image in the image array
 * @param[in] name The name of the image
 * @note the imagename can contain a / or \ (relative to gamedir/) - otherwise it's relative to gamedir/pics
 * @note name may not be null and has to be longer than 4 chars
 * @return NULL on error or image_t pointer on success
 * @sa R_FindImage
 */
image_t *R_RegisterPic (const char *name)
{
	image_t *gl;
	char fullname[MAX_QPATH];

	if (name[0] != '*' && name[1] != '*') {
		if (name[0] != '/' && name[0] != '\\')
			Com_sprintf(fullname, sizeof(fullname), "pics/%s", name);
		else
			Q_strncpyz(fullname, name + 1, MAX_QPATH);
	} else
		Q_strncpyz(fullname, name, MAX_QPATH);

	gl = R_FindImage(fullname, it_pic);
	return gl;
}

/**
 * @brief Fills w and h with the width and height of a given pic
 * @note if w and h are -1 the pic was not found
 * @param[out] w Pointer to int value for width
 * @param[out] h Pointer to int value for height
 * @param[in] pic The name of the pic to get the values for
 * @sa R_RegisterPic
 */
void R_DrawGetPicSize (int *w, int *h, const char *pic)
{
	image_t *gl;

	gl = R_RegisterPic(pic);
	if (!gl) {
		*w = *h = -1;
		return;
	}
	*w = gl->width;
	*h = gl->height;
}

/**
 * @brief Bind and draw a texture
 * @param[in] texnum The texture id (already uploaded of course)
 * @param[in] x normalized x value on the screen
 * @param[in] y normalized y value on the screen
 * @param[in] w normalized width value
 * @param[in] h normalized height value
 */
void R_DrawTexture (int texnum, int x, int y, int w, int h)
{
	R_BindTexture(texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(0, 0);
	qglVertex2f(x, y);
	qglTexCoord2f(1, 0);
	qglVertex2f(x + w, y);
	qglTexCoord2f(1, 1);
	qglVertex2f(x + w, y + h);
	qglTexCoord2f(0, 1);
	qglVertex2f(x, y + h);
	qglEnd();
}

/**
 * @brief Draws an image or parts of it
 * @param[in] x X position to draw the image to
 * @param[in] y Y position to draw the image to
 * @param[in] w Width of the image
 * @param[in] h Height of the image
 * @param[in] sh Right x corner coord of the square to draw
 * @param[in] th Lower y corner coord of the square to draw
 * @param[in] sl Left x corner coord of the square to draw
 * @param[in] tl Upper y corner coord of the square to draw
 * @param[in] align The alignment we should use for placing the image onto the screen (see align_t)
 * @param[in] blend Enable the blend mode (for alpha channel images)
 * @param[in] name The name of the image - relative to base/pics
 * @sa R_RegisterPic
 * @note All these parameter are normalized to VID_NORM_WIDTH and VID_NORM_HEIGHT
 * they are adjusted in this function
 * @todo: Return the image_t pointer (gl) once image_t is known everywhere
 */
int R_DrawNormPic (float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, const char *name)
{
	float nx, ny, nw = 0.0, nh = 0.0;
	image_t *gl;

	gl = R_RegisterPic(name);
	if (!gl) {
		Com_Printf("Can't find pic: %s\n", name);
		return 0;
	}

	/* normalize */
	nx = x * viddef.rx;
	ny = y * viddef.ry;

	if (w && h) {
		nw = w * viddef.rx;
		nh = h * viddef.ry;
	}

	if (sh && th) {
		if (!w || !h) {
			nw = (sh - sl) * viddef.rx;
			nh = (th - tl) * viddef.ry;
		}
		sl = sl / (float)gl->width;
		sh = sh / (float)gl->width;
		tl = tl / (float)gl->height;
		th = th / (float)gl->height;
	} else {
		if (!w || !h) {
			nw = (float)gl->width * viddef.rx;
			nh = (float)gl->height * viddef.ry;
		}
		sh = 1.0f;
		th = 1.0f;
	}

	/* get alignement */
	/* (do nothing if left aligned or unknown) */
	if (align > 0 && align < ALIGN_LAST) {
		switch (align % 3) {
		case 1:
			nx -= nw * 0.5;
			break;
		case 2:
			nx -= nw;
			break;
		}

		switch (align / 3) {
		case 1:
			ny -= nh * 0.5;
			break;
		case 2:
			ny -= nh;
			break;
		}
	}

	if (blend)
		R_EnableBlend(qtrue);

	R_BindTexture(gl->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(sl, tl);
	qglVertex2f(nx, ny);
	qglTexCoord2f(sh, tl);
	qglVertex2f(nx + nw, ny);
	qglTexCoord2f(sh, th);
	qglVertex2f(nx + nw, ny + nh);
	qglTexCoord2f(sl, th);
	qglVertex2f(nx, ny + nh);
	qglEnd();

	if (blend)
		R_EnableBlend(qfalse);

	return nw;
}

/**
 * @brief Fills a box of pixels with a single color
 */
void R_DrawFill (int x, int y, int w, int h, int align, const vec4_t color)
{
	float nx, ny, nw, nh;

	nx = x * viddef.rx;
	ny = y * viddef.ry;
	nw = w * viddef.rx;
	nh = h * viddef.ry;

	R_ColorBlend(color);

	qglDisable(GL_TEXTURE_2D);
	qglBegin(GL_QUADS);

	switch (align) {
	case ALIGN_CL:
		qglVertex2f(nx, ny);
		qglVertex2f(nx + nh, ny);
		qglVertex2f(nx + nh, ny - nw);
		qglVertex2f(nx, ny - nw);
		break;
	case ALIGN_CC:
		qglVertex2f(nx, ny);
		qglVertex2f(nx + nh, ny - nh);
		qglVertex2f(nx + nh, ny - nw - nh);
		qglVertex2f(nx, ny - nw);
		break;
	case ALIGN_UC:
		qglVertex2f(nx, ny);
		qglVertex2f(nx + nw, ny);
		qglVertex2f(nx + nw - nh, ny + nh);
		qglVertex2f(nx - nh, ny + nh);
		break;
	default:
		qglVertex2f(nx, ny);
		qglVertex2f(nx + nw, ny);
		qglVertex2f(nx + nw, ny + nh);
		qglVertex2f(nx, ny + nh);
		break;
	}

	qglEnd();
	R_ColorBlend(NULL);
	qglEnable(GL_TEXTURE_2D);
}

static float lastQ;
/**
 * @brief Draw the day and night images of a flat geoscape
 * multitexture feature is used to blend the images
 * @sa R_Draw3DGlobe
 */
void R_DrawFlatGeoscape (int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map)
{
	image_t *gl;
	float nx, ny, nw, nh;

	/* normalize */
	nx = x * viddef.rx;
	ny = y * viddef.ry;
	nw = w * viddef.rx;
	nh = h * viddef.ry;

	/* load day image */
	gl = R_FindImage(va("pics/menu/%s_day", map), it_wrappic);

	/* draw day image */
	R_BindTexture(gl->texnum);
	qglBegin(GL_QUADS);
	qglTexCoord2f(cx - iz, cy - iz);
	qglVertex2f(nx, ny);
	qglTexCoord2f(cx + iz, cy - iz);
	qglVertex2f(nx + nw, ny);
	qglTexCoord2f(cx + iz, cy + iz);
	qglVertex2f(nx + nw, ny + nh);
	qglTexCoord2f(cx - iz, cy + iz);
	qglVertex2f(nx, ny + nh);
	qglEnd();

	gl = R_FindImage(va("pics/menu/%s_night", map), it_wrappic);
	/* maybe the campaign map doesn't have a night image */
	if (gl == r_notexture)
		return;

	/* init combiner */
	R_EnableBlend(qtrue);

	R_SelectTexture(&r_state.texture_texunit);
	R_BindTexture(gl->texnum);

	R_SelectTexture(&r_state.lightmap_texunit);
	if (!r_dayandnighttexture || lastQ != q) {
		R_CalcDayAndNight(q);
		lastQ = q;
	}

	assert(r_dayandnighttexture);

	R_BindTexture(r_dayandnighttexture->texnum);
	qglEnable(GL_TEXTURE_2D);

	/* draw night image */
	qglBegin(GL_QUADS);
	qglMultiTexCoord2f(GL_TEXTURE0_ARB, cx - iz, cy - iz);
	qglMultiTexCoord2f(GL_TEXTURE1_ARB, p + cx - iz, cy - iz);
	qglVertex2f(nx, ny);
	qglMultiTexCoord2f(GL_TEXTURE0_ARB, cx + iz, cy - iz);
	qglMultiTexCoord2f(GL_TEXTURE1_ARB, p + cx + iz, cy - iz);
	qglVertex2f(nx + nw, ny);
	qglMultiTexCoord2f(GL_TEXTURE0_ARB, cx + iz, cy + iz);
	qglMultiTexCoord2f(GL_TEXTURE1_ARB, p + cx + iz, cy + iz);
	qglVertex2f(nx + nw, ny + nh);
	qglMultiTexCoord2f(GL_TEXTURE0_ARB, cx - iz, cy + iz);
	qglMultiTexCoord2f(GL_TEXTURE1_ARB, p + cx - iz, cy + iz);
	qglVertex2f(nx, ny + nh);
	qglEnd();

	/* reset mode */
	qglDisable(GL_TEXTURE_2D);
	R_SelectTexture(&r_state.texture_texunit);

	R_EnableBlend(qfalse);
}

/**
 * @brief Draws a circle out of lines
 * @param[in] mid Center of the circle
 * @param[in] radius Radius of the circle
 * @param[in] color The color of the circle lines
 * @sa R_DrawPtlCircle
 * @sa R_DrawLineStrip
 */
void R_DrawCircle (vec3_t mid, float radius, const vec4_t color, int thickness)
{
	float theta;
	const float accuracy = 5.0f;

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_LINE_SMOOTH);
	R_EnableBlend(qtrue);

	R_Color(color);

	assert(radius > thickness);

	/* scale it */
	radius *= viddef.rx;
	thickness *= viddef.rx;

	/* store the matrix - we are using glTranslate */
	qglPushMatrix();

	/* translate the position */
	qglTranslated(mid[0], mid[1], mid[2]);

	if (thickness <= 1) {
		qglBegin(GL_LINE_STRIP);
		for (theta = 0.0f; theta <= 2.0f * M_PI; theta += M_PI / (radius * accuracy)) {
			qglVertex3f(radius * cos(theta), radius * sin(theta), 0.0);
		}
		qglEnd();
	} else {
		qglBegin(GL_TRIANGLE_STRIP);
		for (theta = 0; theta <= 2 * M_PI; theta += M_PI / (radius * accuracy)) {
			qglVertex3f(radius * cos(theta), radius * sin(theta), 0);
			qglVertex3f(radius * cos(theta - M_PI / (radius * accuracy)), radius * sin(theta - M_PI / (radius * accuracy)), 0);
			qglVertex3f((radius - thickness) * cos(theta - M_PI / (radius * accuracy)), (radius - thickness) * sin(theta - M_PI / (radius * accuracy)), 0);
			qglVertex3f((radius - thickness) * cos(theta), (radius - thickness) * sin(theta), 0);
		}
		qglEnd();
	}

	qglPopMatrix();

	R_Color(NULL);

	R_EnableBlend(qfalse);
	qglDisable(GL_LINE_SMOOTH);
	qglEnable(GL_TEXTURE_2D);
}

#define MAX_LINEVERTS 256

static void inline R_Draw2DArray (int points, int *verts, GLenum mode)
{
	int i;

	/* fit it on screen */
	if (points > MAX_LINEVERTS * 2)
		points = MAX_LINEVERTS * 2;

	/* set vertex array pointer */
	qglVertexPointer(2, GL_SHORT, 0, r_state.vertex_array_2d);

	for (i = 0; i < points * 2; i += 2) {
		r_state.vertex_array_2d[i] = verts[i] * viddef.rx;
		r_state.vertex_array_2d[i + 1] = verts[i + 1] * viddef.ry;
	}

	qglDisable(GL_TEXTURE_2D);
	qglDrawArrays(mode, 0, points);
	qglEnable(GL_TEXTURE_2D);
	qglVertexPointer(3, GL_FLOAT, 0, r_state.vertex_array_3d);
}

/**
 * @brief 2 dimensional line strip
 * @sa R_DrawCircle
 * @sa R_DrawLineLoop
 */
void R_DrawLineStrip (int points, int *verts)
{
	R_Draw2DArray(points, verts, GL_LINE_STRIP);
}

/**
 * @sa R_DrawCircle
 * @sa R_DrawLineStrip
 */
void R_DrawLineLoop (int points, int *verts)
{
	R_Draw2DArray(points, verts, GL_LINE_LOOP);
}


/**
 * @sa R_DrawCircle
 * @sa R_DrawLineStrip
 * @sa R_DrawLineLoop
 */
void R_DrawPolygon (int points, int *verts)
{
	R_Draw2DArray(points, verts, GL_POLYGON);
}

#define MARKER_SIZE 60.0
/**
 * @brief Draw a 3D Marker on the 3D geoscape
 * @sa MAP_Draw3DMarkerIfVisible
 */
void R_Draw3DMapMarkers (vec3_t angles, float zoom, vec3_t position, const char *model)
{
	modelInfo_t mi;
	char path[MAX_QPATH] = "";
	vec2_t model_center;

	memset(&mi, 0, sizeof(modelInfo_t));

	Com_sprintf(path, sizeof(path), "geoscape/%s", model);
	mi.model = R_RegisterModelShort(path);
	if (!mi.model) {
		Com_Printf("Could not find model '%s'\n", path);
		return;
	}
	mi.name = path;

	mi.origin = position;
	mi.angles = angles;
	mi.scale = NULL;

	model_center[0] = MARKER_SIZE * zoom;
	model_center[1] = MARKER_SIZE * zoom;
	mi.center = model_center;

	R_DrawModelDirect(&mi, NULL, NULL);
}

/**
 * @brief responsible for drawing the 3d globe on geoscape
 * @param[in] x menu node x position
 * @param[in] y menu node y position
 * @param[in] w menu node widht
 * @param[in] h menu node height
 * @param[in] p
 * @param[in] q
 * @param[in] rotate the rotate angle of the globe
 * @param[in] zoom the current globe zoon
 * @param[in] map the prefix of the map to use (image must be at base/pics/menu/<map>_[day|night])
 * @sa R_DrawFlatGeoscape
 * @sa R_InitGlobeEarthChain
 */
void R_Draw3DGlobe (int x, int y, int w, int h, float p, float q, vec3_t rotate, float zoom, const char *map)
{
	/* globe scaling */
	const float fullscale = zoom / 4.0f;
	vec4_t lightPos;
	const vec4_t diffuseLightColor = {1.0f, 1.0f, 1.0f, 1.0f};
	const vec4_t ambientLightColor = {0.2f, 0.2f, 0.2f, 0.2f};
	float a;

	image_t* geoscape, *starfield, *background, *sun;
	float nx, ny, nw, nh;
	float centerx, centery;

	vec3_t v, v1, rotationAxis;

	/* normalize */
	nx = x * viddef.rx;
	ny = y * viddef.ry;
	nw = w * viddef.rx;
	nh = h * viddef.ry;

	/* center of screen */
	centerx = nx + nw / 2;
	centery = ny + nh / 2;

	starfield = R_FindImage(va("pics/menu/%s_stars", map), it_pic);
	if (geoscape != r_notexture) {
		R_DrawTexture(starfield->texnum, nx, ny, nw, nh);
	}

	background = R_FindImage("pics/menu/map_background", it_pic);
	if (background != r_notexture) {
		const float bgZoom = zoom * 10.0f;
		/* Force height to make sure the image is a circle (and not an ellipse) */
		const int halfHeight = 768.0f * viddef.ry;
		qglEnable(GL_BLEND);
		R_DrawTexture(background->texnum,  centerx - nw / 2 * bgZoom, centery - halfHeight / 2 * bgZoom, nw * bgZoom, halfHeight * bgZoom);
		qglDisable(GL_BLEND);
	}

	/* add the light */
	a = cos(q) * SIN_ALPHA;
	Vector4Set(lightPos, cos(p) * sqrt(0.5f * (1 - a * a)), -sin(p) * sqrt(0.5f * (1 - a * a)), a, 0);

	VectorSet(v, lightPos[1], lightPos[0], lightPos[2]);

	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v1, rotationAxis, v, - rotate[PITCH]);

	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v, rotationAxis, v1, -rotate[YAW]);

	/* load sun image */
	sun = R_FindImage("pics/menu/map_sun", it_pic);
	if (sun != r_notexture && (v[2] < 0)) {
		const float sunZoom = 1000.0f;
		qglEnable(GL_BLEND);
		R_DrawTexture(sun->texnum, centerx - 128 * viddef.rx + 1000 * v[1] * viddef.rx , centery - 128 * viddef.ry + sunZoom * v[0] * viddef.ry, 256 * viddef.rx, 256 * viddef.ry);
		qglDisable(GL_BLEND);
	}

	/* load day image */
	geoscape = R_FindImage(va("pics/menu/%s_day", map), it_wrappic);
	if (geoscape == r_notexture) {
		Com_Printf("Could not find pics/menu/%s_day\n", map);
		return;
	}

	/* globe texture scaling */
	/* previous releases made a tiled version of the globe texture. here i just shrink it using the
	 * texture matrix, for much the same effect */
	qglMatrixMode(GL_TEXTURE);
	qglLoadIdentity();
	/* scale the texture */
	qglScalef(2, 1, 1);
	qglMatrixMode(GL_MODELVIEW);

	/* background */
	/* go to a new matrix */
	qglPushMatrix();

	/* center it */
	qglTranslatef(nx + nw / 2, ny + nh / 2, 0);
	R_CheckError();

	/* flatten the sphere */
	/* this will also scale the normal vectors */
	qglScalef(fullscale * viddef.rx, fullscale * viddef.ry, fullscale);
	R_CheckError();

	/* rotate the globe as given in ccs.angles */
	qglRotatef(rotate[YAW], 1, 0, 0);
	qglRotatef(rotate[ROLL], 0, 1, 0);
	qglRotatef(rotate[PITCH], 0, 0, 1);

	/* solid globe texture */
	qglBindTexture(GL_TEXTURE_2D, geoscape->texnum);
	R_CheckError();

	qglEnable(GL_CULL_FACE);

	/* enable the lighting */
	qglEnable(GL_LIGHTING);
	qglEnable(GL_LIGHT0);
	qglLightfv(GL_LIGHT0, GL_POSITION, lightPos);
	qglLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLightColor);
	qglLightfv(GL_LIGHT0, GL_AMBIENT, ambientLightColor);

	/* draw the sphere */
	qglCallList(r_globeEarthList);
	R_CheckError();

	qglDisable(GL_CULL_FACE);

	/* disable 3d geoscape lighting */
	qglDisable(GL_LIGHTING);

	/* restore the previous matrix */
	qglPopMatrix();
	qglMatrixMode(GL_TEXTURE);
	qglLoadIdentity();
	qglMatrixMode(GL_MODELVIEW);
}
