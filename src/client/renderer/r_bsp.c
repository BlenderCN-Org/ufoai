/**
 * @file r_bsp.c
 * @brief BSP model code
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
#include "r_lightmap.h"
#include "r_material.h"

/*
=============================================================
BRUSH MODELS
=============================================================
*/

#define BACKFACE_EPSILON 0.01


/**
 * @brief Returns true if the box is completely outside the frustum
 */
static qboolean R_CullBox (const vec3_t mins, const vec3_t maxs)
{
	int i;

	if (r_nocull->integer)
		return qfalse;

	for (i = 0; i < 4; i++)
		if (TR_BoxOnPlaneSide(mins, maxs, &r_locals.frustum[i]) == PSIDE_BACK)
			return qtrue;
	return qfalse;
}

/**
 * @brief Checks whether the model is inside the view
 * @sa R_CullBox
 */
qboolean R_CullBspModel (const entity_t *e)
{
	vec3_t mins, maxs;

	/* no surfaces */
	if (!e->model->bsp.nummodelsurfaces)
		return qtrue;

	if (VectorNotEmpty(e->angles)) {
		int i;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - e->model->radius;
			maxs[i] = e->origin[i] + e->model->radius;
		}
	} else {
		VectorAdd(e->origin, e->model->mins, mins);
		VectorAdd(e->origin, e->model->maxs, maxs);
	}

	return R_CullBox(mins, maxs);
}

/**
 * @brief
 * @param[in] modelorg relative to viewpoint
 */
static void R_DrawBspModelSurfaces (const entity_t *e, const vec3_t modelorg)
{
	int i;
	float dot;
	mBspSurface_t *surf;

	surf = &e->model->bsp.surfaces[e->model->bsp.firstmodelsurface];

	for (i = 0; i < e->model->bsp.nummodelsurfaces; i++, surf++) {
		/* find which side of the surf we are on  */
		if (AXIAL(surf->plane)
			dot = modelorg[surf->plane->type] - surf->plane->dist;
		else
			dot = DotProduct(modelorg, surf->plane->normal) - surf->plane->dist;

		if (((surf->flags & MSURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(surf->flags & MSURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
			/* visible flag for rendering */
			surf->frame = r_locals.frame;
	}

	R_DrawOpaqueSurfaces(e->model->bsp.opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(e->model->bsp.opaque_warp_surfaces);

	R_DrawAlphaTestSurfaces(e->model->bsp.alpha_test_surfaces);

	R_EnableBlend(qtrue);

	R_DrawBlendSurfaces(e->model->bsp.blend_surfaces);

	R_DrawBlendWarpSurfaces(e->model->bsp.blend_warp_surfaces);

	R_DrawMaterialSurfaces(e->model->bsp.material_surfaces);

	R_EnableBlend(qfalse);
}

/**
 * @brief Draws a brush model
 * @note E.g. a func_breakable or func_door
 */
void R_DrawBrushModel (const entity_t * e)
{
	/* relative to viewpoint */
	vec3_t modelorg;

	/* set the relative origin, accounting for rotation if necessary */
	VectorSubtract(refdef.vieworg, e->origin, modelorg);
	if (VectorNotEmpty(e->angles)) {
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);

		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	glPushMatrix();
	R_TransformForEntity(e);

	R_DrawBspModelSurfaces(e, modelorg);

	/* show model bounding box */
	if (r_showbox->integer) {
		vec3_t bbox[8];
		int i;

		/* compute a full bounding box */
		for (i = 0; i < 8; i++) {
			bbox[i][0] = (i & 1) ? e->model->mins[0] : e->model->maxs[0];
			bbox[i][1] = (i & 2) ? e->model->mins[1] : e->model->maxs[1];
			bbox[i][2] = (i & 4) ? e->model->mins[2] : e->model->maxs[2];
		}
		R_EntityDrawBBox(bbox);
	}

	glPopMatrix();
}


/*
=============================================================
WORLD MODEL
=============================================================
*/

/**
 * @brief Draw normals for bsp surfaces
 */
void R_DrawBspNormals (int tile)
{
	int i, j, k;
	const mBspSurface_t *surf;
	const mBspModel_t *bsp;
	const vec4_t color = {1.0, 0.0, 0.0, 1.0};

	if (!r_shownormals->integer)
		return;

	R_EnableTexture(&texunit_diffuse, qfalse);

	R_Color(color);

	k = 0;
	bsp = &r_mapTiles[tile]->bsp;
	surf = bsp->surfaces;
	for (i = 0; i < bsp->numsurfaces; i++, surf++) {
		if (surf->frame != r_locals.frame)
			continue; /* not visible */

		if (surf->texinfo->flags & SURF_WARP)
			continue;  /* don't care */

		if (r_shownormals->integer > 1 && !(surf->texinfo->flags & SURF_PHONG))
			continue;  /* don't care */

		/* avoid overflows, draw in batches */
		if (k > MAX_GL_ARRAY_LENGTH - 512) {
			glDrawArrays(GL_LINES, 0, k / 3);
			k = 0;
		}

		for (j = 0; j < surf->numedges; j++) {
			vec3_t end;
			const GLfloat *vertex = &bsp->verts[(surf->index + j) * 3];
			const GLfloat *normal = &bsp->normals[(surf->index + j) * 3];

			VectorMA(vertex, 12.0, normal, end);

			memcpy(&r_state.vertex_array_3d[k], vertex, sizeof(vec3_t));
			memcpy(&r_state.vertex_array_3d[k + 3], end, sizeof(vec3_t));
			k += sizeof(vec3_t) / sizeof(vec_t) * 2;
			if (k > MAX_GL_ARRAY_LENGTH)
				Com_Error(ERR_DROP, "R_DrawBspNormals: Overflow in array buffer");
		}
	}

	glDrawArrays(GL_LINES, 0, k / 3);

	R_EnableTexture(&texunit_diffuse, qtrue);

	R_Color(NULL);
}

/**
 * @sa R_DrawWorld
 * @sa R_RecurseWorld
 * @param[in] tile The maptile (map assembly)
 */
static void R_RecursiveWorldNode (mBspNode_t * node, int tile)
{
	int i, side, sidebit;
	mBspSurface_t *surf;
	float dot;

	if (node->contents == CONTENTS_SOLID)
		return;					/* solid */

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;					/* culled out */

	/* if a leaf node, draw stuff */
	if (node->contents > CONTENTS_NODE)
		return;

	/* pathfinding nodes are invalid here */
	assert(node->plane);

	/* node is just a decision point, so go down the apropriate sides
	 * find which side of the node we are on */
	if (r_isometric->integer) {
		dot = -DotProduct(r_locals.forward, node->plane->normal);
	} else if (!AXIAL(node->plane)) {
		dot = DotProduct(refdef.vieworg, node->plane->normal) - node->plane->dist;
	} else {
		dot = refdef.vieworg[node->plane->type] - node->plane->dist;
	}

	if (dot >= 0) {
		side = 0;
		sidebit = 0;
	} else {
		side = 1;
		sidebit = MSURF_PLANEBACK;
	}

	/* recurse down the children, front side first */
	R_RecursiveWorldNode(node->children[side], tile);

	surf = r_mapTiles[tile]->bsp.surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++) {
		/* visible (front) side */
		if ((surf->flags & MSURF_PLANEBACK) == sidebit)
			surf->frame = r_locals.frame;
	}

	/* recurse down the back side */
	R_RecursiveWorldNode(node->children[!side], tile);
}

/**
 * @sa R_GetLevelSurfaceLists
 * @param[in] tile The maptile (map assembly)
 * @sa R_ModLoadNodes about pathfinding nodes
 */
static void R_RecurseWorld (mBspNode_t * node, int tile)
{
	/* skip special pathfinding nodes */
	if (!node->plane) {
		R_RecurseWorld(node->children[0], tile);
		R_RecurseWorld(node->children[1], tile);
	} else {
		R_RecursiveWorldNode(node, tile);
	}
}


/**
 * @brief Fills the surface chains for the current worldlevel and hide other levels
 * @sa cvar cl_worldlevel
 */
void R_GetLevelSurfaceLists (void)
{
	int i, tile, mask;

	r_locals.frame++;

	if (!r_drawworld->integer)
		return;

	mask = 1 << refdef.worldlevel;

	for (tile = 0; tile < r_numMapTiles; tile++) {
		/* don't draw weaponclip, actorclip and stepon */
		for (i = 0; i <= LEVEL_LASTVISIBLE; i++) {
			/* check the worldlevel flags */
			if (i && !(i & mask))
				continue;

			if (!r_mapTiles[tile]->bsp.submodels[i].numfaces)
				continue;

			R_RecurseWorld(r_mapTiles[tile]->bsp.nodes + r_mapTiles[tile]->bsp.submodels[i].headnode, tile);
		}
		if (r_drawspecialbrushes->integer) {
			for (; i < LEVEL_MAX; i++) {
				/** @todo numfaces and headnode might get screwed up in some cases (segfault) */
				if (r_mapTiles[tile]->bsp.submodels[i].numfaces <= 0)
					continue;

				R_RecurseWorld(r_mapTiles[tile]->bsp.nodes + r_mapTiles[tile]->bsp.submodels[i].headnode, tile);
			}
		}
	}
}
