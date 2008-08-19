/**
 * @file bsp.h
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

#ifndef UFO2MAP_BSP_H
#define UFO2MAP_BSP_H

#include <assert.h>

#include "common/shared.h"
#include "common/cmdlib.h"
#include "common/scriplib.h"
#include "common/polylib.h"
#include "common/bspfile.h"
#include "common/imagelib.h"

#include "../../common/tracing.h"


extern int entity_num;

extern plane_t mapplanes[MAX_MAP_PLANES];
extern int nummapplanes;

extern int nummapbrushes;
extern mapbrush_t mapbrushes[MAX_MAP_BRUSHES];

extern int nummapbrushsides;
extern side_t brushsides[MAX_MAP_SIDES];
extern brush_texture_t side_brushtextures[MAX_MAP_SIDES];

extern int brush_start, brush_end;

byte GetLevelFlagsFromBrush(const mapbrush_t *brush);
void LoadMapFile(const char *filename);
void WriteMapFile(const char *filename);
int FindFloatPlane(vec3_t normal, vec_t dist);

/*============================================================================= */

/* textures.c */

typedef struct {
	char	name[MAX_QPATH];
	int		surfaceFlags;
	int		value;
	int		contentFlags;
	qboolean	footstepMarked; /**< only print it once to the footsteps file */
	qboolean	materialMarked; /**< only print it once to the material file */
} textureref_t;


extern	textureref_t	textureref[MAX_MAP_TEXTURES];
int	FindMiptex(const char *name);
int TexinfoForBrushTexture(plane_t *plane, brush_texture_t *bt, const vec3_t origin, qboolean isTerrain);

/* csg.c */

int MapBrushesBounds(const int startbrush, const int endbrush, const int level, const vec3_t clipmins, const vec3_t clipmaxs, vec3_t mins, vec3_t maxs);
bspbrush_t *MakeBspBrushList(int startbrush, int endbrush, int level, vec3_t clipmins, vec3_t clipmaxs);
bspbrush_t *ChopBrushes(bspbrush_t *head);

/* brushbsp.c */

bspbrush_t *CopyBrush(const bspbrush_t *brush);
void SplitBrush(bspbrush_t *brush, int planenum, bspbrush_t **front, bspbrush_t **back);
bspbrush_t *AllocBrush(int numsides);
int	CountBrushList(bspbrush_t *brushes);
void FreeBrush(bspbrush_t *brushes);
void FreeBrushList(bspbrush_t *brushes);
qboolean WindingIsTiny(winding_t *w);
tree_t *BrushBSP(bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);
void WriteBSPBrushMap(const char *name, const bspbrush_t *list);

/* portals.c */

int VisibleContents(int contents);
void MarkVisibleSides(tree_t *tree, int start, int end);
void FreePortal(portal_t *p);
void MakeTreePortals(tree_t *tree);
void RemovePortalFromNode(portal_t *portal, node_t *l);

/*============================================================================= */

/* writebsp.c */

void SetModelNumbers(void);

void BeginBSPFile(void);
int WriteBSP(node_t *headnode);
void EndBSPFile(const char *filename);
void BeginModel(int entityNum);
void EndModel(void);
void EmitBrushes(void);
void EmitPlanes(void);

/* faces.c */

void MakeFaces(node_t *headnode);
void FixTjuncs(node_t *headnode);
int GetEdge(int v1, int v2, const face_t *f);
void FreeFace(face_t *f);

/* tree.c */

void FreeTree(tree_t *tree);

/* trace.c */

void MakeTracingNodes(int levels);
void CloseTracingNodes(void);
qboolean TestContents(const vec3_t pos);

/* levels.c */

extern const vec3_t v_epsilon;
extern vec3_t worldMins, worldMaxs;

void PushInfo(void);
void PopInfo(void);
void ProcessLevel(unsigned int levelnum);
void PruneNodes(node_t *node);

/* routing.c */

void DoRouting(void);

/* bsp.c */

void ProcessModels(const char *filename);

#endif /* UFO2MAP_BSP_H */
