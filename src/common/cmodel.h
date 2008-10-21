/**
 * @file cmodel.h
 * @brief Common model code header (for bsp and others)
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

/*==============================================================
CMODEL
==============================================================*/
#include "../common/qfiles.h"
#include "pqueue.h"


extern vec3_t map_min, map_max;

void CM_LoadMap(const char *tiles, const char *pos, unsigned *checksum);
int CheckBSPFile(const char *filename);
cBspModel_t *CM_InlineModel(const char *name);
void CM_SetInlineModelOrientation(const char *name, const vec3_t origin, const vec3_t angles);

int CM_NumClusters(void);
int CM_NumInlineModels(void);
char *CM_EntityString(void);



/*==============================================================
CMODEL BOX TRACING
==============================================================*/

/** creates a clipping hull for an arbitrary box */
int CM_HeadnodeForBox(int tile, const vec3_t mins, const vec3_t maxs);
trace_t CM_CompleteBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int tile, int headnode, int brushmask, int brushrejects, const vec3_t origin, const vec3_t angles);
trace_t CM_TransformedBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int tile, int headnode, int brushmask, int brushrejects, const vec3_t origin, const vec3_t angles);
qboolean CM_TestLineWithEnt(const vec3_t start, const vec3_t stop, const int levelmask, const char **entlist);
qboolean CM_EntTestLine(const vec3_t start, const vec3_t stop, const int levelmask);
qboolean CM_EntTestLineDM(const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask);
trace_t CM_EntCompleteBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int levelmask, int brushmask, int brushreject);

/*==========================================================
GRID ORIENTED MOVEMENT AND SCANNING
==========================================================*/

extern struct routing_s svMap[ACTOR_MAX_SIZE], clMap[ACTOR_MAX_SIZE];
extern struct pathing_s svPathMap, clPathMap;

void Grid_DumpWholeServerMap_f(void);
void Grid_DumpWholeClientMap_f(void);
void Grid_RecalcBoxRouting (struct routing_s *map, pos3_t min, pos3_t max);
void Grid_RecalcRouting(struct routing_s *map, const char *name, const char **list);
void Grid_DumpDVTable(struct pathing_s *path);
void Grid_MoveMark (struct routing_s *map, const int actor_size, struct pathing_s *path, pos3_t pos, int crouching_state, const int dir, priorityQueue_t *pqueue);
void Grid_MoveCalc(struct routing_s *map, const int actor_size, struct pathing_s *path, pos3_t from, int crouching_state, int distance, byte ** fb_list, int fb_length);
void Grid_MoveStore(struct pathing_s *path);
pos_t Grid_MoveLength(struct pathing_s *path, pos3_t to, int crouching_state, qboolean stored);
int Grid_MoveNext(struct routing_s *map, const int actor_size, struct pathing_s *path, pos3_t from, int crouching_state);
int Grid_Height(struct routing_s *map, const int actor_size, const pos3_t pos);
unsigned int Grid_Ceiling (struct routing_s *map, const int actor_size, const pos3_t pos);
int Grid_Floor(struct routing_s *map, const int actor_size, const pos3_t pos);
pos_t Grid_StepUp(struct routing_s *map, const int actor_size, const pos3_t pos);
int Grid_TUsUsed(int dir);
int Grid_Filled(struct routing_s *map, const int actor_size, pos3_t pos);
pos_t Grid_Fall(struct routing_s *map, const int actor_size, pos3_t pos);
void Grid_PosToVec(struct routing_s *map, const int actor_size, pos3_t pos, vec3_t vec);


/*==========================================================
MISC WORLD RELATED
==========================================================*/

float Com_GrenadeTarget(const vec3_t from, const vec3_t at, float speed, qboolean launched, qboolean rolled, vec3_t v0);
