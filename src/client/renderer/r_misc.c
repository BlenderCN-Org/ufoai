/**
 * @file r_misc.c
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

static const byte gridtexture[8][8] = {
	{1, 1, 1, 1, 1, 1, 1, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 1, 1, 1, 1, 1, 1, 1},
};

#define MISC_TEXTURE_SIZE 16
void R_InitMiscTexture (void)
{
	int x, y;
	byte data[MISC_TEXTURE_SIZE][MISC_TEXTURE_SIZE][4];

	memset(data, 0, sizeof(data));

	/* also use this for bad textures, but without alpha */
	for (x = 0; x < 8; x++) {
		for (y = 0; y < 8; y++) {
			data[y][x][0] = gridtexture[x][y] * 255;
			data[y][x][3] = 255;
		}
	}
	r_noTexture = R_LoadImageData("***r_notexture***", (byte *) data, 8, 8, it_effect);

	for (x = 0; x < MISC_TEXTURE_SIZE; x++) {
		for (y = 0; y < MISC_TEXTURE_SIZE; y++) {
			data[y][x][0] = rand() % 255;
			data[y][x][1] = rand() % 255;
			data[y][x][2] = rand() % 48;
			data[y][x][3] = rand() % 48;
		}
	}
	r_warpTexture = R_LoadImageData("***r_warptexture***", (byte *)data, MISC_TEXTURE_SIZE, MISC_TEXTURE_SIZE, it_effect);

	/* empty pic in the texture chain for cinematic frames */
	R_LoadImageData("***cinematic***", NULL, VID_NORM_WIDTH, VID_NORM_HEIGHT, it_effect);
}


/*
==============================================================================
SCREEN SHOTS
==============================================================================
*/

enum {
	SSHOTTYPE_JPG,
	SSHOTTYPE_PNG,
	SSHOTTYPE_TGA,
	SSHOTTYPE_TGA_COMP,
};

void R_ScreenShot_f (void)
{
	char	checkName[MAX_OSPATH];
	int		type, shotNum, quality = 100;
	const char	*ext;
	byte	*buffer;
	qFILE	f;

	/* Find out what format to save in */
	if (Cmd_Argc() > 1)
		ext = Cmd_Argv(1);
	else
		ext = r_screenshot_format->string;

	if (!Q_strcasecmp (ext, "png"))
		type = SSHOTTYPE_PNG;
	else if (!Q_strcasecmp (ext, "jpg"))
		type = SSHOTTYPE_JPG;
	else
		type = SSHOTTYPE_TGA_COMP;

	/* Set necessary values */
	switch (type) {
	case SSHOTTYPE_TGA:
	case SSHOTTYPE_TGA_COMP:
		Com_Printf("Taking TGA screenshot...\n");
		ext = "tga";
		break;

	case SSHOTTYPE_PNG:
		Com_Printf("Taking PNG screenshot...\n");
		ext = "png";
		break;

	case SSHOTTYPE_JPG:
		if (Cmd_Argc() == 3)
			quality = atoi(Cmd_Argv(2));
		else
			quality = r_screenshot_jpeg_quality->integer;
		if (quality > 100 || quality <= 0)
			quality = 100;

		Com_Printf("Taking JPG screenshot (at %i%% quality)...\n", quality);
		ext = "jpg";
		break;
	}

	/* Find a file name to save it to */
	for (shotNum = 0; shotNum < 1000; shotNum++) {
		Com_sprintf(checkName, MAX_OSPATH, "%s/scrnshot/ufo%i%i.%s", FS_Gamedir(), shotNum / 10, shotNum % 10, ext);
		f.f = fopen(checkName, "rb");
		if (!f.f)
			break;
		fclose(f.f);
	}

	FS_CreatePath(checkName);

	memset(&f, 0, sizeof(f));

	/* Open it */
	FS_OpenFileWrite(checkName, &f);

	if (!f.f) {
		Com_Printf("R_ScreenShot_f: Couldn't create file: %s\n", checkName);
		return;
	}

	if (shotNum == 1000) {
		Com_Printf("R_ScreenShot_f: screenshot limit (of 1000) exceeded!\n");
		FS_CloseFile(&f);
		return;
	}

	/* Allocate room for a copy of the framebuffer */
	buffer = Mem_PoolAlloc(viddef.width * viddef.height * 3, vid_imagePool, 0);
	if (!buffer) {
		Com_Printf("R_ScreenShot_f: Could not allocate %i bytes for screenshot!\n", viddef.width * viddef.height * 3);
		FS_CloseFile(&f);
		return;
	}

	/* Read the framebuffer into our storage */
	glReadPixels(0, 0, viddef.width, viddef.height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	R_CheckError();

	/* Write */
	switch (type) {
	case SSHOTTYPE_TGA:
		R_WriteTGA(&f, buffer, viddef.width, viddef.height);
		break;

	case SSHOTTYPE_TGA_COMP:
		R_WriteCompressedTGA(&f, buffer, viddef.width, viddef.height);
		break;

	case SSHOTTYPE_PNG:
		R_WritePNG(&f, buffer, viddef.width, viddef.height);
		break;

	case SSHOTTYPE_JPG:
		R_WriteJPG(&f, buffer, viddef.width, viddef.height, quality);
		break;
	}

	/* Finish */
	FS_CloseFile(&f);
	Mem_Free(buffer);

	Com_Printf("Wrote %s\n", checkName);
}
