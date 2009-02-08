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
#include "r_sphere.h"
#include "r_error.h"
#include "r_draw.h"
#include "r_mesh.h"
#include "r_overlay.h"

extern const float STANDARD_3D_ZOOM;

image_t *shadow;
image_t *blood[MAX_DEATH];

/* console font */
static image_t *draw_chars;

/**
 * @brief Loads some textures and init the 3d globe
 * @sa R_Init
 */
void R_DrawInitLocal (void)
{
	int i;

	shadow = R_FindImage("pics/sfx/shadow", it_effect);
	if (shadow == r_noTexture)
		Com_Printf("Could not find shadow image in game pics/sfx directory!\n");
	for (i = 0; i < MAX_DEATH; i++) {
		blood[i] = R_FindImage(va("pics/sfx/blood_%i", i), it_effect);
		if (blood[i] == r_noTexture)
			Com_Printf("Could not find blood_%i image in game pics/sfx directory!\n", i);
	}

	draw_chars = R_FindImage("pics/conchars", it_chars);
	if (draw_chars == r_noTexture)
		Sys_Error("Could not find conchars image in game pics directory!\n");
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
	R_BindTexture(draw_chars->texnum);

	R_EnableBlend(qtrue);

	/* alter the array pointers */
	glVertexPointer(2, GL_SHORT, 0, char_verts);
	glTexCoordPointer(2, GL_FLOAT, 0, char_texcoords);

	glDrawArrays(GL_QUADS, 0, char_index / 2);

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
	if (img == r_noTexture)
		Sys_Error("Could not find the searched image: %s\n", name);

	R_BindTexture(img->texnum);

	if (img->width == width && img->height == height) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img->width, img->height, GL_RGBA, GL_UNSIGNED_BYTE, frame);
	} else {
		/* Reallocate the texture */
		img->width = width;
		img->height = height;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame);
	}
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
const image_t *R_RegisterPic (const char *name)
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
	if (gl == r_noTexture)
		return NULL;
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
	const image_t *gl = R_RegisterPic(pic);
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
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(x, y);
	glTexCoord2f(1, 0);
	glVertex2f(x + w, y);
	glTexCoord2f(1, 1);
	glVertex2f(x + w, y + h);
	glTexCoord2f(0, 1);
	glVertex2f(x, y + h);
	glEnd();
}

static float image_texcoords[4 * 2];
static short image_verts[4 * 2];

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
 */
const image_t *R_DrawNormPic (float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, const char *name)
{
	float nw, nh, x1, x2, x3, x4, y1, y2, y3, y4;
	const image_t *image;

	image = R_RegisterPic(name);
	if (!image) {
		Com_Printf("Can't find pic: %s\n", name);
		return NULL;
	}

	/* normalize to the screen resolution */
	x1 = x * viddef.rx;
	y1 = y * viddef.ry;

	/* provided width and height (if any) take precedence */
	if (w)
		nw = w * viddef.rx;
	else
		nw = 0;

	if (h)
		nh = h * viddef.ry;
	else
		nh = 0;

	/* horizontal texture mapping */
	if (sh) {
		if (!w)
			nw = (sh - sl) * viddef.rx;
		sh /= image->width;
	} else {
		if (!w)
			nw = ((float)image->width - sl) * viddef.rx;
		sh = 1.0f;
	}
	sl /= image->width;

	/* vertical texture mapping */
	if (th) {
		if (!h)
			nh = (th - tl) * viddef.ry;
		th /= image->height;
	} else {
		if (!h)
			nh = ((float)image->height - tl) * viddef.ry;
		th = 1.0f;
	}
	tl /= image->height;

	/* alignment */
	if (align > 0 && align < ALIGN_LAST) {
		/* horizontal (0 is left) */
		switch (align % 3) {
		case 1:
			x1 -= nw * 0.5;
			break;
		case 2:
			x1 -= nw;
			break;
		}

		/* vertical (0 is upper) */
		switch ((align % 9) / 3) {
		case 1:
			y1 -= nh * 0.5;
			break;
		case 2:
			y1 -= nh;
			break;
		}
	}

	/* fill the rest of the coordinates to make a rectangle */
	x4 = x1;
	x3 = x2 = x1 + nw;
	y2 = y1;
	y4 = y3 = y1 + nh;

	/* slanting */
	if (align >= 9 && align < ALIGN_LAST) {
		x1 += nh;
		x2 += nh;
	}

	image_texcoords[0] = sl;
	image_texcoords[1] = tl;
	image_texcoords[2] = sh;
	image_texcoords[3] = tl;
	image_texcoords[4] = sh;
	image_texcoords[5] = th;
	image_texcoords[6] = sl;
	image_texcoords[7] = th;
	image_verts[0] = x1;
	image_verts[1] = y1;
	image_verts[2] = x2;
	image_verts[3] = y2;
	image_verts[4] = x3;
	image_verts[5] = y3;
	image_verts[6] = x4;
	image_verts[7] = y4;

	/* alter the array pointers */
	glVertexPointer(2, GL_SHORT, 0, image_verts);
	glTexCoordPointer(2, GL_FLOAT, 0, image_texcoords);

	if (blend)
		R_EnableBlend(qtrue);

	R_BindTexture(image->texnum);

	glDrawArrays(GL_QUADS, 0, 4);

	if (blend)
		R_EnableBlend(qfalse);

	/* and restore them */
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	return image;
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

	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	switch (align) {
	case ALIGN_CL:
		glVertex2f(nx, ny);
		glVertex2f(nx + nh, ny);
		glVertex2f(nx + nh, ny - nw);
		glVertex2f(nx, ny - nw);
		break;
	case ALIGN_CC:
		glVertex2f(nx, ny);
		glVertex2f(nx + nh, ny - nh);
		glVertex2f(nx + nh, ny - nw - nh);
		glVertex2f(nx, ny - nw);
		break;
	case ALIGN_UC:
		glVertex2f(nx, ny);
		glVertex2f(nx + nw, ny);
		glVertex2f(nx + nw - nh, ny + nh);
		glVertex2f(nx - nh, ny + nh);
		break;
	default:
		glVertex2f(nx, ny);
		glVertex2f(nx + nw, ny);
		glVertex2f(nx + nw, ny + nh);
		glVertex2f(nx, ny + nh);
		break;
	}

	glEnd();
	R_ColorBlend(NULL);
	glEnable(GL_TEXTURE_2D);
}

void R_DrawRect(int x, int y, int w, int h, const vec4_t color, float lineWidth, int pattern)
{
	float nx, ny, nw, nh;

	nx = x * viddef.rx;
	ny = y * viddef.ry;
	nw = w * viddef.rx;
	nh = h * viddef.ry;

	R_ColorBlend(color);

	glDisable(GL_TEXTURE_2D);
	glLineWidth(lineWidth);
	glLineStipple(2, pattern);
	glEnable(GL_LINE_STIPPLE);

	glBegin(GL_LINE_LOOP);
	glVertex2f(nx, ny);
	glVertex2f(nx + nw, ny);
	glVertex2f(nx + nw, ny + nh);
	glVertex2f(nx, ny + nh);
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glLineWidth(1.0f);
	glDisable(GL_LINE_STIPPLE);

	R_ColorBlend(NULL);
}

/**
 * @brief Draw the day and night images of a flat geoscape
 * multitexture feature is used to blend the images
 * @sa R_Draw3DGlobe
 * @param[in] map The geoscape map to draw (can be changed in the campaign definition)
 * @param[in] iz The zoomlevel of the geoscape - see ccs.zoom
 * @param[in] cx The x texture coordinate
 * @param[in] cy The y texture coordinate
 * @param[in] p
 * @param[in] q
 * @param[in] x The x position of the geoscape node
 * @param[in] y The y position of the geoscape node
 * @param[in] w The width of the geoscape node
 * @param[in] h The height of the geoscape node
 */
void R_DrawFlatGeoscape (int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map)
{
	static float lastQ = 0.0f;
	image_t *gl;
	float geoscape_texcoords[4 * 2];
	short geoscape_verts[4 * 2];

	/* normalize */
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	/* load day image */
	gl = R_FindImage(va("pics/geoscape/%s_day", map), it_wrappic);
	if (gl == r_noTexture)
		Sys_Error("Could not load geoscape day image");

	/* alter the array pointers */
	glVertexPointer(2, GL_SHORT, 0, geoscape_verts);
	glTexCoordPointer(2, GL_FLOAT, 0, geoscape_texcoords);

	geoscape_texcoords[0] = cx - iz;
	geoscape_texcoords[1] = cy - iz;
	geoscape_texcoords[2] = cx + iz;
	geoscape_texcoords[3] = cy - iz;
	geoscape_texcoords[4] = cx + iz;
	geoscape_texcoords[5] = cy + iz;
	geoscape_texcoords[6] = cx - iz;
	geoscape_texcoords[7] = cy + iz;

	geoscape_verts[0] = nx;
	geoscape_verts[1] = ny;
	geoscape_verts[2] = nx + nw;
	geoscape_verts[3] = ny;
	geoscape_verts[4] = nx + nw;
	geoscape_verts[5] = ny + nh;
	geoscape_verts[6] = nx;
	geoscape_verts[7] = ny + nh;

	/* draw day image */
	R_BindTexture(gl->texnum);
	glDrawArrays(GL_QUADS, 0, 4);

	R_EnableBlend(qtrue);

	/* draw night map */
	gl = R_FindImage(va("pics/geoscape/%s_night", map), it_wrappic);
	/* maybe the campaign map doesn't have a night image */
	if (gl != r_noTexture) {
		float geoscape_nighttexcoords[4 * 2];

		R_BindTexture(gl->texnum);
		R_EnableTexture(&texunit_lightmap, qtrue);

		geoscape_nighttexcoords[0] = geoscape_texcoords[0] + p;
		geoscape_nighttexcoords[1] = geoscape_texcoords[1];
		geoscape_nighttexcoords[2] = geoscape_texcoords[2] + p;
		geoscape_nighttexcoords[3] = geoscape_texcoords[3];
		geoscape_nighttexcoords[4] = geoscape_texcoords[4] + p;
		geoscape_nighttexcoords[5] = geoscape_texcoords[5];
		geoscape_nighttexcoords[6] = geoscape_texcoords[6] + p;
		geoscape_nighttexcoords[7] = geoscape_texcoords[7];

		R_SelectTexture(&texunit_lightmap);

		glTexCoordPointer(2, GL_FLOAT, 0, geoscape_nighttexcoords);

		R_BindTexture(r_dayandnightTexture->texnum);
		if (lastQ != q) {
			R_CalcAndUploadDayAndNightTexture(q);
			lastQ = q;
		}

		R_SelectTexture(&texunit_diffuse);

		glDrawArrays(GL_QUADS, 0, 4);

		R_EnableTexture(&texunit_lightmap, qfalse);
	}

	/* draw nation overlay */
	if (r_geoscape_overlay->integer & OVERLAY_NATION) {
		gl = R_FindImage(va("pics/geoscape/%s_nations_overlay", map), it_wrappic);
		if (gl == r_noTexture)
			Sys_Error("Could not load geoscape nation overlay image");

		/* draw day image */
		R_BindTexture(gl->texnum);
		glDrawArrays(GL_QUADS, 0, 4);
	}

	/* draw XVI image */
	if (r_geoscape_overlay->integer & OVERLAY_XVI) {
		R_BindTexture(r_xviTexture->texnum);
		glDrawArrays(GL_QUADS, 0, 4);
	}

	/* draw radar image */
	if (r_geoscape_overlay->integer & OVERLAY_RADAR) {
		R_BindTexture(r_radarTexture->texnum);
		glDrawArrays(GL_QUADS, 0, 4);
	}

	R_EnableBlend(qfalse);

	/* and restore them */
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
}

/**
 * @brief Draw the background picture of airfight node.
 * @param[in] x The x position of the air fight node.
 * @param[in] y The y position of the air fight node.
 * @param[in] w The width of the air fight node.
 * @param[in] h The height of the air fight node.
 * @param[in] cx x texture coordinates that should be displayed in the center of the node.
 * @param[in] cy y texture coordinates that should be displayed in the center of the node.
 */
void R_DrawAirFightBackground (int x, int y, int w, int h, float cx, float cy, float iz)
{
	image_t *gl;
	float geoscape_texcoords[4 * 2];
	short geoscape_verts[4 * 2];

	/* normalize */
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	/* load day image */
	gl = R_FindImage("pics/airfight/forest1", it_wrappic);

	if (gl == r_noTexture)
		Sys_Error("Could not load geoscape day image");

	/* alter the array pointers */
	glVertexPointer(2, GL_SHORT, 0, geoscape_verts);
	glTexCoordPointer(2, GL_FLOAT, 0, geoscape_texcoords);

	geoscape_texcoords[0] = cx - iz;
	geoscape_texcoords[1] = cy - iz;
	geoscape_texcoords[2] = cx + iz;
	geoscape_texcoords[3] = cy - iz;
	geoscape_texcoords[4] = cx + iz;
	geoscape_texcoords[5] = cy + iz;
	geoscape_texcoords[6] = cx - iz;
	geoscape_texcoords[7] = cy + iz;

	geoscape_verts[0] = nx;
	geoscape_verts[1] = ny;
	geoscape_verts[2] = nx + nw;
	geoscape_verts[3] = ny;
	geoscape_verts[4] = nx + nw;
	geoscape_verts[5] = ny + nh;
	geoscape_verts[6] = nx;
	geoscape_verts[7] = ny + nh;

	/* draw day image */
	R_BindTexture(gl->texnum);
	glDrawArrays(GL_QUADS, 0, 4);
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

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LINE_SMOOTH);
	R_EnableBlend(qtrue);

	R_Color(color);

	assert(radius > thickness);

	/* scale it */
	radius *= viddef.rx;
	thickness *= viddef.rx;

	/* store the matrix - we are using glTranslate */
	glPushMatrix();

	/* translate the position */
	glTranslated(mid[0], mid[1], mid[2]);

	if (thickness <= 1) {
		glBegin(GL_LINE_STRIP);
		for (theta = 0.0f; theta <= 2.0f * M_PI; theta += M_PI / (radius * accuracy)) {
			glVertex3f(radius * cos(theta), radius * sin(theta), 0.0);
		}
		glEnd();
	} else {
		glBegin(GL_TRIANGLE_STRIP);
		for (theta = 0; theta <= 2 * M_PI; theta += M_PI / (radius * accuracy)) {
			glVertex3f(radius * cos(theta), radius * sin(theta), 0);
			glVertex3f(radius * cos(theta - M_PI / (radius * accuracy)), radius * sin(theta - M_PI / (radius * accuracy)), 0);
			glVertex3f((radius - thickness) * cos(theta - M_PI / (radius * accuracy)), (radius - thickness) * sin(theta - M_PI / (radius * accuracy)), 0);
			glVertex3f((radius - thickness) * cos(theta), (radius - thickness) * sin(theta), 0);
		}
		glEnd();
	}

	glPopMatrix();

	R_Color(NULL);

	R_EnableBlend(qfalse);
	glDisable(GL_LINE_SMOOTH);
	glEnable(GL_TEXTURE_2D);
}

#define CIRCLE_LINE_COUNT	40

/**
 * @brief Draws a circle out of lines
 * @param[in] x X screen coordinate
 * @param[in] y Y screen coordinate
 * @param[in] radius Radius of the circle
 * @param[in] color The color of the circle lines
 * @param[in] fill Fill the circle with the given color
 * @param[in] thickness The thickness of the lines
 * @sa R_DrawPtlCircle
 * @sa R_DrawLineStrip
 */
void R_DrawCircle2D (int x, int y, float radius, qboolean fill, const vec4_t color, float thickness)
{
	int i;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_TEXTURE_2D);
	R_EnableBlend(qtrue);
	R_Color(color);

	if (thickness > 0.0)
		glLineWidth(thickness);

	if (fill)
		glBegin(GL_TRIANGLE_STRIP);
	else
		glBegin(GL_LINE_LOOP);

	/* Create a vertex at the exact position specified by the start angle. */
	glVertex2f(x + radius, y);

	for (i = 0; i < CIRCLE_LINE_COUNT; i++) {
		const float angle = (i * 2 * M_PI) / CIRCLE_LINE_COUNT;
		glVertex2f(x + radius * cos(angle), y - radius * sin(angle));

		/* When filling we're drawing triangles so we need to
		 * create a vertex in the middle of the vertex to fill
		 * the entire pie slice/circle. */
		if (fill)
			glVertex2f(x, y);
	}

	glVertex2f(x + radius * cos(2 * M_PI), y - radius * sin(2 * M_PI));
	glEnd();
	glEnable(GL_TEXTURE_2D);
	R_EnableBlend(qfalse);
	R_Color(NULL);

	glPopAttrib();
}

#define MAX_LINEVERTS 256

static void inline R_Draw2DArray (int points, int *verts, GLenum mode)
{
	int i;

	/* fit it on screen */
	if (points > MAX_LINEVERTS * 2)
		points = MAX_LINEVERTS * 2;

	/* set vertex array pointer */
	glVertexPointer(2, GL_SHORT, 0, r_state.vertex_array_2d);

	for (i = 0; i < points * 2; i += 2) {
		r_state.vertex_array_2d[i] = verts[i] * viddef.rx;
		r_state.vertex_array_2d[i + 1] = verts[i + 1] * viddef.ry;
	}

	glDisable(GL_TEXTURE_2D);
	glDrawArrays(mode, 0, points);
	glEnable(GL_TEXTURE_2D);
	glVertexPointer(3, GL_FLOAT, 0, r_state.vertex_array_3d);
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
 * @brief Draws one line with only one start and one end point
 * @sa R_DrawCircle
 * @sa R_DrawLineStrip
 */
void R_DrawLine (int *verts, float thickness)
{
	if (thickness > 0.0)
		glLineWidth(thickness);

	R_Draw2DArray(2, verts, GL_LINES);

	if (thickness > 0.0)
		glLineWidth(1.0);
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
void R_Draw3DMapMarkers (vec3_t angles, float zoom, vec3_t position, const char *model, int skin)
{
	modelInfo_t mi;
	vec3_t model_center;

	memset(&mi, 0, sizeof(mi));

	mi.model = R_RegisterModelShort(model);
	if (!mi.model) {
		Com_Printf("Could not find model '%s'\n", model);
		return;
	}
	mi.name = model;

	mi.origin = position;
	mi.angles = angles;
	mi.scale = NULL;
	mi.skin = skin;

	model_center[0] = MARKER_SIZE * zoom;
	model_center[1] = MARKER_SIZE * zoom;
	model_center[2] = MARKER_SIZE * zoom; /** @todo */
	mi.center = model_center;

	R_DrawModelDirect(&mi, NULL, NULL);
}

/**
 * @brief Center position of skybox along z-axis. This is used to make sure we see only the inside of Skybox.
 * @sa R_DrawStarfield
 * @sa R_SetupGL2D
 */
const float SKYBOX_DEPTH = -9999.0f;

/**
 * @brief Half size of Skybox.
 * @note The bigger, the less perspective default you'll have, but the more you'll
 * zoom on the texture (and see it's default).
 * @sa R_DrawStarfield
 * @sa R_SetupGL2D
 */
#define SKYBOX_HALFSIZE 800.0f

static const float starFieldVerts[] = {
	/* face 1 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 2 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,

	/* face 3 */
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 4 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,

	/* face 5 */
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,

	/* face 6 */
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, +SKYBOX_HALFSIZE,
	+SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE,
	-SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE, -SKYBOX_HALFSIZE
};

static const float starFieldTexCoords[] = {
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
	0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,
};

/**
 * @brief Bind and draw starfield.
 * @param[in] texnum The texture id (already uploaded of course)
 * @note We draw a skybox: the camera is inside a cube rotating at earth rotation speed
 * (stars seems to rotate because we see earth as idle, but in reality stars are statics
 * and earth rotate around itself)
 * @sa R_SetupGL2D
 * @sa R_Draw3DGlobe
 */
static void R_DrawStarfield (int texnum, const vec3_t pos, const vec3_t rotate, float p)
{
	vec3_t angle;		/**< Angle of rotation of starfield */

	/* go to a new matrix */
	glPushMatrix();

	/* we must center the skybox on the camera border of view, and not on the earth, in order
	 * to see only the inside of the cube */
	glTranslatef(pos[0], pos[1], -SKYBOX_DEPTH);

	/* rotates starfield: only time and rotation of earth around itself causes starfield to rotate. */
	VectorSet(angle, rotate[0] - p * todeg, rotate[1], rotate[2]);
	glRotatef(angle[YAW], 1, 0, 0);
	glRotatef(angle[ROLL], 0, 1, 0);
	glRotatef(angle[PITCH], 0, 0, 1);

	R_BindTexture(texnum);

	/* alter the array pointers */
	glVertexPointer(3, GL_FLOAT, 0, starFieldVerts);
	glTexCoordPointer(2, GL_FLOAT, 0, starFieldTexCoords);

	/* draw the cube */
	glDrawArrays(GL_QUADS, 0, 24);

	/* restore previous matrix */
	glPopMatrix();
}

/**
 * @brief responsible for drawing the 3d globe on geoscape
 * @param[in] x menu node x position
 * @param[in] y menu node y position
 * @param[in] w menu node widht
 * @param[in] h menu node height
 * @param[in] rotate the rotate angle of the globe
 * @param[in] zoom the current globe zoon
 * @param[in] map the prefix of the map to use (image must be at base/pics/menu/<map>_[day|night])
 * @sa R_DrawFlatGeoscape
 * @sa R_SphereGenerate
 */
void R_Draw3DGlobe (int x, int y, int w, int h, int day, int second, const vec3_t rotate, float zoom, const char *map, qboolean disableSolarRender)
{
	/* globe scaling */
	const float fullscale = zoom / STANDARD_3D_ZOOM;
	vec4_t lightPos;
	const vec4_t diffuseLightColor = {2.0f, 2.0f, 2.0f, 2.0f};
	const vec4_t ambientLightColor = {0.2f, 0.2f, 0.2f, 0.2f};
	float p;
	/* set distance of the moon to make it static on starfield when time is stoped.
	 * this distance should be used for any celestial body considered at infinite location (sun, moon) */
	const float celestialDist = 1.37f * SKYBOX_HALFSIZE;
	const float moonSize = 0.025f;

	image_t *starfield, *background, *sun;
	/* normalize */
	const float nx = x * viddef.rx;
	const float ny = y * viddef.ry;
	const float nw = w * viddef.rx;
	const float nh = h * viddef.ry;

	const vec3_t earthPos = {nx + nw / 2, ny + nh / 2, 0};	/* Earth center is in the middle of node.
								* Due to Orthographic view, this is also camera position */
	vec3_t moonPos;

	vec3_t v, v1, rotationAxis;

	/* Compute light position in absolute frame */
	const float q = (day % DAYS_PER_YEAR + (float) (second / (float)SECONDS_PER_DAY)) * 2 * M_PI / DAYS_PER_YEAR;	/* sun rotation (year) */
	const float a = cos(q) * SIN_ALPHA;	/* due to earth obliquity */
	const float sqrta = sqrt(0.5f * (1 - a * a));
	p = (float) (second / (float)SECONDS_PER_DAY) * 2 * M_PI - 0.5f * M_PI;		/* earth rotation (day) */
	Vector4Set(lightPos, cos(p) * sqrta, -sin(p) * sqrta, a, 0);

	/* Then rotate it in the relative frame of player view, to get sun position (no need to rotate
	 * lightpos: all models will be rotated after light effect is applied) */
	VectorSet(v, lightPos[1], lightPos[0], lightPos[2]);
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v1, rotationAxis, v, -rotate[PITCH]);
	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v, rotationAxis, v1, -rotate[YAW]);

	R_EnableBlend(qtrue);

	starfield = R_FindImage(va("pics/geoscape/%s_stars", map), it_wrappic);
	if (starfield != r_noTexture)
		R_DrawStarfield(starfield->texnum, earthPos, rotate, p);

	/* load sun image */
	sun = R_FindImage("pics/geoscape/map_sun", it_pic);
	if (sun != r_noTexture && v[2] < 0 && !disableSolarRender) {
		R_DrawTexture(sun->texnum, earthPos[0] - 64 * viddef.rx + celestialDist * v[1] * viddef.rx , earthPos[1] - 64 * viddef.ry + celestialDist * v[0] * viddef.ry, 128 * viddef.rx, 128 * viddef.ry);
	}

	/* Draw atmosphere */
	background = R_FindImage("pics/geoscape/map_background", it_pic);
	if (background != r_noTexture) {
		const float bgZoom = zoom;
		/* Force height to make sure the image is a circle (and not an ellipse) */
		const int halfHeight = 768.0f * viddef.ry;
		R_DrawTexture(background->texnum,  earthPos[0] - nw / 2 * bgZoom, earthPos[1] - halfHeight / 2 * bgZoom, nw * bgZoom, halfHeight * bgZoom);
	}

	R_EnableBlend(qfalse);

	/* load earth image */
	r_globeEarth.texture = R_FindImage(va("pics/geoscape/%s_day", map), it_wrappic);
	if (r_globeEarth.texture == r_noTexture) {
		Com_Printf("Could not find pics/geoscape/%s_day\n", map);
		return;
	}

	/* load moon image */
	r_globeMoon.texture = R_FindImage(va("pics/geoscape/%s_moon", map), it_wrappic);

	/* globe texture scaling */
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	/* scale the textures */
	glScalef(2, 1, 1);
	glMatrixMode(GL_MODELVIEW);

	/* calculate position of the moon (it rotates around earth with a period of
	 * about 24.9 h, and we must take day into account to avoid moon to "jump"
	 * every time the day is changing) */
	p = (float) ((day % 249) + second / (24.9f * (float)SECONDS_PER_HOUR)) * 2 * M_PI;
	VectorSet(moonPos, cos(p) * sqrta, - sin(p) * sqrta, a);
	VectorSet(v, moonPos[1], moonPos[0], moonPos[2]);
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v1, rotationAxis, v, -rotate[PITCH]);
	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v, rotationAxis, v1, -rotate[YAW]);
	VectorSet(moonPos, earthPos[0] + celestialDist * v[1], earthPos[1] + celestialDist * v[0], -celestialDist * v[2]);

	/* enable the lighting */
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLightColor);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLightColor);

	/* Enable depth (to draw the moon behind the earth if needed) */
	glEnable(GL_DEPTH_TEST);

	/* draw the globe */
	R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, lightPos);
	/* load nation overlay */
	if (r_geoscape_overlay->integer & OVERLAY_NATION) {
		r_globeEarth.overlay = R_FindImage(va("pics/geoscape/%s_nations_overlay", map), it_wrappic);
		if (r_globeEarth.overlay == r_noTexture)
			Sys_Error("Could not load geoscape nation overlay image");
		R_EnableBlend(qtrue);
		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, lightPos);
		R_EnableBlend(qfalse);
		r_globeEarth.overlay = NULL;
	}
	if (r_geoscape_overlay->integer & OVERLAY_XVI) {
		assert(r_xviTexture);
		r_globeEarth.overlay = r_xviTexture;
		R_EnableBlend(qtrue);
		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, lightPos);
		R_EnableBlend(qfalse);
		r_globeEarth.overlay = NULL;
	}
	if (r_geoscape_overlay->integer & OVERLAY_RADAR) {
		assert(r_radarTexture);
		r_globeEarth.overlay = r_radarTexture;
		R_EnableBlend(qtrue);
		R_SphereRender(&r_globeEarth, earthPos, rotate, fullscale, lightPos);
		R_EnableBlend(qfalse);
		r_globeEarth.overlay = NULL;
	}

	/* draw the moon */
	if (r_globeMoon.texture != r_noTexture && moonPos[2] > 0 && !disableSolarRender)
		R_SphereRender(&r_globeMoon, moonPos, rotate, moonSize , NULL);

	/* Disable depth */
	glDisable(GL_DEPTH_TEST);

	/* disable 3d geoscape lighting */
	glDisable(GL_LIGHTING);

	/* restore the previous matrix */
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}

/**
 * @brief draw a panel from a texture as we can see on the image
 * The function is inline because there are often 3 or 5 const param, with it a lot of var became const too
 * @image html http://ufoai.ninex.info/wiki/images/Inline_draw_panel.png
 * @param[in] pos Position of the output panel
 * @param[in] size Size of the output panel
 * @param[in] texture Texture contain the template of the panel
 * @param[in] blend True if the texture must use alpha chanel for per pixel transparency
 * @param[in] texX Position x of the panel template into the texture
 * @param[in] texY Position y of the panel template into the texture
 * @param[in] panelDef Array of seven elements define the panel template used in the texture.
 * From the first to the last: left width, mid width, right width,
 * top height, mid height, bottom height, and marge
 * @todo can we improve the code? is it need?
 */
void R_DrawPanel (const vec2_t pos, const vec2_t size, const char *texture, qboolean blend, int texX, int texY, const int *panelDef)
{
	const int leftWidth = panelDef[0];
	const int midWidth = panelDef[1];
	const int rightWidth = panelDef[2];
	const int topHeight = panelDef[3];
	const int midHeight = panelDef[4];
	const int bottomHeight = panelDef[5];
	const int marge =  panelDef[6];

	/** @todo merge texX and texY here */
	const int firstPos = 0;
	const int secondPos = firstPos + leftWidth + marge;
	const int thirdPos = secondPos + midWidth + marge;
	const int firstPosY = 0;
	const int secondPosY = firstPosY + topHeight + marge;
	const int thirdPosY = secondPosY + midHeight + marge;

	int y, h;

	/* draw top (from left to right) */
	R_DrawNormPic(pos[0], pos[1], leftWidth, topHeight, texX + firstPos + leftWidth, texY + firstPosY + topHeight,
		texX + firstPos, texY + firstPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + leftWidth, pos[1], size[0] - leftWidth - rightWidth, topHeight, texX + secondPos + midWidth, texY + firstPosY + topHeight,
		texX + secondPos, texY + firstPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - rightWidth, pos[1], rightWidth, topHeight, texX + thirdPos + rightWidth, texY + firstPosY + topHeight,
		texX + thirdPos, texY + firstPosY, ALIGN_UL, blend, texture);

	/* draw middle (from left to right) */
	y = pos[1] + topHeight;
	h = size[1] - topHeight - bottomHeight; /*< height of middle */
	R_DrawNormPic(pos[0], y, leftWidth, h, texX + firstPos + leftWidth, texY + secondPosY + midHeight,
		texX + firstPos, texY + secondPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + leftWidth, y, size[0] - leftWidth - rightWidth, h, texX + secondPos + midWidth, texY + secondPosY + midHeight,
		texX + secondPos, texY + secondPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - rightWidth, y, rightWidth, h, texX + thirdPos + rightWidth, texY + secondPosY + midHeight,
		texX + thirdPos, texY + secondPosY, ALIGN_UL, blend, texture);

	/* draw bottom (from left to right) */
	y = pos[1] + size[1] - bottomHeight;
	R_DrawNormPic(pos[0], y, leftWidth, bottomHeight, texX + firstPos + leftWidth, texY + thirdPosY + bottomHeight,
		texX + firstPos, texY + thirdPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + leftWidth, y, size[0] - leftWidth - rightWidth, bottomHeight, texX + secondPos + midWidth, texY + thirdPosY + bottomHeight,
		texX + secondPos, texY + thirdPosY, ALIGN_UL, blend, texture);
	R_DrawNormPic(pos[0] + size[0] - bottomHeight, y, rightWidth, bottomHeight, texX + thirdPos + rightWidth, texY + thirdPosY + bottomHeight,
		texX + thirdPos, texY + thirdPosY, ALIGN_UL, blend, texture);
}

/**
 * @brief Force to draw only on a rect
 * @note Don't forget to call @c R_EndClipRect
 * @sa R_EndClipRect
 */
void R_BeginClipRect (int x, int y, int width, int height)
{
	glScissor(x * viddef.rx, (VID_NORM_HEIGHT - (y + height)) * viddef.ry, width * viddef.rx, height * viddef.ry);
	glEnable(GL_SCISSOR_TEST);
}

/**
 * @sa R_BeginClipRect
 */
void R_EndClipRect (void)
{
	glDisable(GL_SCISSOR_TEST);
}
