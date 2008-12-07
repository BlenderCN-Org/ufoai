/**
 * @file r_model_brush.h
 * @brief brush model loading
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

#ifndef R_MODEL_BRUSH_H
#define R_MODEL_BRUSH_H

/*
==============================================================================
BRUSH MODELS
==============================================================================
*/


/** in memory representation */
typedef struct mBspVertex_s {
	vec3_t position;
	vec3_t normal;
} mBspVertex_t;

typedef struct mBspHeader_s {
	vec3_t mins, maxs;
	float radius;
	int headnode;
	int visleafs;				/**< not including the solid leaf 0 */
	int firstface, numfaces;
} mBspHeader_t;

#define	MSURF_PLANEBACK		1
#define	MSURF_LIGHTMAP		2

typedef struct mBspEdge_s {
	unsigned short v[2];
} mBspEdge_t;

typedef struct mBspTexInfo_s {
	float vecs[2][4];		/**< [s/t][xyz offset] */
	int flags;
	image_t *image;
} mBspTexInfo_t;

typedef struct mBspSurface_s {
	cBspPlane_t *plane;
	int flags;
	int tile;				/**< index in r_mapTiles (loaded bsp map index) this surface belongs, to */

	int frame;	/**< used to decide whether this surface should be drawn */

	/** look up in model->surfedges[], negative numbers are backwards edges */
	int firstedge;
	int numedges;

	short stmins[2];		/**< st min coordinates */
	short stmaxs[2];			/**< st max coordinates */
	vec2_t stcenter;
	vec2_t stextents;

	vec3_t center;
	vec4_t color;
	vec3_t normal;

	int light_s, light_t;		/**< gl lightmap coordinates */
	int lightmap_scale;

	GLuint index;

	mBspTexInfo_t *texinfo;

	int lightmap_texnum;
	int deluxemap_texnum;
	byte style;
	byte *samples;				/**< lightmap samples - only used at loading time */
	byte *lightmap;				/**< finalized lightmap samples, cached for lookups */

	int lightframe;				/**< dynamic lighting frame */
	int lights;					/**< bitmask of dynamic light sources */
} mBspSurface_t;

/* surfaces are assigned to arrays based on their primary rendering type
 * and then sorted by world texnum to reduce binds */
typedef struct msurfaces_s {
	mBspSurface_t **surfaces;
	int count;
} mBspSurfaces_t;

#define opaque_surfaces			sorted_surfaces[0]
#define opaque_warp_surfaces	sorted_surfaces[1]
#define alpha_test_surfaces		sorted_surfaces[2]
#define blend_surfaces			sorted_surfaces[3]
#define blend_warp_surfaces		sorted_surfaces[4]
#define material_surfaces		sorted_surfaces[5]

#define NUM_SURFACES_ARRAYS		6

#define R_SurfaceToSurfaces(surfs, surf)\
	(surfs)->surfaces[(surfs)->count++] = surf

#define CONTENTS_NO_LEAF -1

typedef struct mBspNode_s {
	/* common with leaf */
	int contents;				/**< -1, to differentiate from leafs */
	float minmaxs[6];			/**< for bounding box culling */

	struct mBspNode_s *parent;

	/* node specific */
	cBspPlane_t *plane;
	struct mBspNode_s *children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;

	struct model_s *model;
} mBspNode_t;

typedef struct mBspLeaf_s {
	/* common with node */
	int contents;				/**< will be a negative contents number */

	float minmaxs[6];			/**< for bounding box culling */

	struct mnode_s *parent;

	struct model_s *model;
} mBspLeaf_t;

/** @brief brush model */
typedef struct mBspModel_s {
	/* range of surface numbers in this (sub)model */
	int firstmodelsurface, nummodelsurfaces;
	int maptile;		/**< the maptile the surface indices belongs to */

	int numsubmodels;
	mBspHeader_t *submodels;

	int numplanes;
	cBspPlane_t *planes;

	int numleafs;				/**< number of visible leafs, not counting 0 */
	mBspLeaf_t *leafs;

	int numvertexes;
	mBspVertex_t *vertexes;

	int numedges;
	mBspEdge_t *edges;

	int numnodes;
	int firstnode;
	mBspNode_t *nodes;

	int numtexinfo;
	mBspTexInfo_t *texinfo;

	int numsurfaces;
	mBspSurface_t *surfaces;

	int numsurfedges;
	int *surfedges;

	/* vertex arrays */
	GLfloat *verts;
	GLfloat *texcoords;
	GLfloat *lmtexcoords;
	GLfloat *tangents;
	GLfloat *normals;

	/* vertex buffer objects */
	GLuint vertex_buffer;
	GLuint texcoord_buffer;
	GLuint lmtexcoord_buffer;
	GLuint tangent_buffer;
	GLuint normal_buffer;

	byte lightquant;
	byte *lightdata;

	/* sorted surfaces arrays */
	mBspSurfaces_t *sorted_surfaces[NUM_SURFACES_ARRAYS];
} mBspModel_t;

#endif /* R_MODEL_BRUSH_H */
