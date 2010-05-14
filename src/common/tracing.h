/**
 * @file tracing.h
 * @brief Tracing functions
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef COMMON_TRACING_H
#define COMMON_TRACING_H


#include "../shared/typedefs.h"

/*==============================================================
GLOBAL TYPES
==============================================================*/
#if defined(COMPILE_MAP)
  #define TR_TILE_TYPE			dMapTile_t
  #define TR_PLANE_TYPE			dBspPlane_t
  #define TR_PLANE2_TYPE		plane_t
  #define TR_NODE_TYPE			dBspNode_t
  #define TR_LEAF_TYPE			dBspLeaf_t
  #define TR_BRUSH_TYPE			dBspBrush_t
  #define TR_BRUSHSIDE_TYPE		dBspBrushSide_t
#elif defined(COMPILE_UFO)
  #define TR_TILE_TYPE			mapTile_t
  #define TR_PLANE_TYPE			cBspPlane_t
  #define TR_PLANE2_TYPE		cBspPlane_t
  #define TR_NODE_TYPE			cBspNode_t
  #define TR_LEAF_TYPE			cBspLeaf_t
  #define TR_BRUSH_TYPE			cBspBrush_t
  #define TR_BRUSHSIDE_TYPE		cBspBrushSide_t
#else
  #error Either COMPILE_MAP or COMPILE_UFO must be defined in order for tracing.c to work.
#endif

/**
 * mask to trace against all the different visible levels (1-8) (resp. (1<<[0-7])
 */
#define TRACING_ALL_VISIBLE_LEVELS 0x1FF

/** a trace is returned when a box is swept through the world */
typedef struct trace_s{
	qboolean allsolid;			/**< if true, plane is not valid */
	qboolean startsolid;		/**< if true, the initial point was in a solid area */
	float fraction;				/**< distance traveled, 1.0 = didn't hit anything, 0.0 Inside of a brush */
	vec3_t endpos;				/**< final position along line */
	TR_PLANE_TYPE plane;		/**< surface normal at impact */
	cBspSurface_t *surface;	    /**< surface hit */
	int planenum;				/**< index of the plane hit, used for map debugging */
	int contentFlags;			/**< contents on other side of surface hit */
	int leafnum;
	int mapTile;				/**< the map tile we hit something */
	struct le_s *le;			/**< not set by CM_*() functions */
	struct edict_s *ent;		/**< not set by CM_*() functions */
} trace_t;

/* This attempts to make the box tracing code thread safe. */
typedef struct boxtrace_s {
	vec3_t start, end;
	vec3_t mins, maxs;
	vec3_t absmins, absmaxs;
	vec3_t extents;

	trace_t trace;
	int contents;
	int rejects;
	qboolean ispoint;			/* optimized case */

	TR_TILE_TYPE *tile;
} boxtrace_t;

typedef struct box_s {
	vec3_t mins, maxs;
} box_t;

/*==============================================================
BOX AND LINE TRACING
==============================================================*/

extern int c_traces, c_brush_traces;
extern TR_TILE_TYPE *curTile;
extern TR_TILE_TYPE mapTiles[MAX_MAPTILES];
extern int numTiles;
extern tnode_t *tnode_p;

int TR_BoxOnPlaneSide(const vec3_t mins, const vec3_t maxs, const TR_PLANE_TYPE *plane);
int TR_HeadnodeForBox(mapTile_t *tile, const vec3_t mins, const vec3_t maxs);

void TR_BuildTracingNode_r(int node, int level);

trace_t TR_HintedTransformedBoxTrace(TR_TILE_TYPE *tile, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int headnode, const int brushmask, const int brushreject, const vec3_t origin, const vec3_t angles, const vec3_t rmaShift, const float fraction);
#define TR_TransformedBoxTrace(tile, start, end, mins, maxs, headnode, brushmask, brushreject, origin, angles) TR_HintedTransformedBoxTrace(tile, start, end, mins, maxs, headnode, brushmask, brushreject, origin, angles, vec3_origin, 1.0f);

#ifdef COMPILE_MAP
trace_t TR_SingleTileBoxTrace(const vec3_t start, const vec3_t end, const box_t* traceBox, const int levelmask, const int brushmask, const int brushreject);
qboolean TR_TestLineSingleTile(const vec3_t start, const vec3_t stop, int *headhint);
#else
trace_t TR_CompleteBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int levelmask, int brushmask, int brushreject);
#endif

qboolean TR_TestLine(const vec3_t start, const vec3_t stop, const int levelmask);
qboolean TR_TestLineDM(const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask);

#endif /* COMMON_TRACING_H */
