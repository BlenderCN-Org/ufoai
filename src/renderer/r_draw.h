/**
 * @file r_draw.h
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

#ifndef R_DRAW_H
#define R_DRAW_H

int R_DrawNormPic(float x, float y, float w, float h, float sh, float th, float sl, float tl, int align, qboolean blend, const char *name);
void R_DrawChar(int x, int y, int c);
void R_DrawChars(void);
void R_DrawFill(int x, int y, int w, int h, int align, const vec4_t color);
void R_Draw3DGlobe(int x, int y, int w, int h, int day, int second, vec3_t rotate, float zoom, const char *map);
void R_Draw3DMapMarkers(vec3_t angles, float zoom, vec3_t position, const char *model);
int R_DrawImagePixelData(const char *name, byte *frame, int width, int height);
void R_DrawTexture(int texnum, int x, int y, int w, int h);
void R_DrawGetPicSize(int *w, int *h, const char *name);
void R_DrawFlatGeoscape(int x, int y, int w, int h, float p, float q, float cx, float cy, float iz, const char *map);
void R_DrawLineStrip(int points, int *verts);
void R_DrawLineLoop(int points, int *verts);
void R_DrawCircle (vec3_t mid, float radius, const vec4_t color, int thickness);
void R_DrawPolygon(int points, int *verts);

#endif
