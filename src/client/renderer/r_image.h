/**
 * @file r_image.h
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

#ifndef R_IMAGE_H
#define R_IMAGE_H

/*
skins will be outline flood filled and mip mapped
pics and sprites with alpha will be outline flood filled
pic won't be mip mapped

model skin
sprite frame
wall texture
pic
*/

typedef enum {
	it_chars,
	it_effect,
	it_static,
	it_pic,
	it_wrappic,

	/** the following are freed with every mapchange */
	it_world,
	it_lightmap,
	it_deluxemap,
	it_normalmap,
	it_material,
	it_skin
} imagetype_t;

typedef struct image_s {
	char name[MAX_QPATH];				/**< game path, including extension, must be first */
	imagetype_t type;
	int width, height;					/**< source image dimensions */
	int upload_width, upload_height;	/**< dimensions after power of two and picmip */
	int registration_sequence;			/**< 0 = free */
	struct mBspSurface_s *texturechain;	/**< for sort-by-texture world drawing */
	unsigned int texnum;				/**< gl texture binding */
	qboolean has_alpha;
	material_t material;
	struct image_s *normalmap;			/**< normalmap texture  */
} image_t;

extern int registration_sequence;

#define MAX_GL_TEXTURES		1024
#define MAX_GL_LIGHTMAPS	256
#define MAX_GL_DELUXEMAPS	256

#define TEXNUM_LIGHTMAPS	MAX_GL_TEXTURES
#define TEXNUM_DELUXEMAPS	(TEXNUM_LIGHTMAPS + MAX_GL_LIGHTMAPS)

extern image_t r_images[MAX_GL_TEXTURES];
extern int r_numImages;

void R_WritePNG(qFILE *f, byte *buffer, int width, int height);
void R_WriteJPG(qFILE *f, byte *buffer, int width, int height, int quality);
void R_WriteTGA(qFILE *f, byte *buffer, int width, int height);
void R_WriteCompressedTGA(qFILE *f, byte *buffer, int width, int height);

void R_SoftenTexture(byte *in, int width, int height, int bpp);

void R_ImageList_f(void);
void R_InitImages(void);
void R_ShutdownImages(void);
void R_FreeUnusedImages(void);
void R_ImageClearMaterials(void);
void R_CalcAndUploadDayAndNightTexture(float q);
void R_FilterTexture(unsigned *in, int width, int height, imagetype_t type);
void R_TextureMode(const char *string);
void R_TextureAlphaMode(const char *string);
void R_TextureSolidMode(const char *string);
void R_IncreaseXVILevel(const vec2_t pos);
void R_InitializeXVIOverlay(const char *mapname, byte *data, int width, int height);
byte* R_XVIMapCopy(int *width, int *height);
void R_CreateRadarOverlay(void);
void R_InitializeRadarOverlay(qboolean source);
void R_AddRadarCoverage(const vec2_t pos, float innerRadius, float outerRadius, qboolean source);
void R_UploadRadarCoverage(qboolean smooth);

image_t *R_LoadImageData(const char *name, byte * pic, int width, int height, imagetype_t type);
#ifdef DEBUG
image_t *R_FindImageDebug(const char *pname, imagetype_t type, const char *file, int line);
#define R_FindImage(pname,type) R_FindImageDebug(pname, type, __FILE__, __LINE__ )
#else
image_t *R_FindImage(const char *pname, imagetype_t type);
#endif

#define MAX_ENVMAPTEXTURES 2
extern image_t *r_envmaptextures[MAX_ENVMAPTEXTURES];

extern image_t *shadow;	/**< draw this when actor is alive */
extern image_t *blood[MAX_DEATH]; /**< draw this when actor is dead */
extern image_t *r_noTexture;
extern image_t *r_warpTexture;
extern image_t *r_dayandnightTexture;
extern image_t *r_xviTexture;
extern image_t *r_radarTexture;

#endif
