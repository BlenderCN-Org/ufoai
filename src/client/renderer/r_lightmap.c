/**
 * @file r_lightmap.c
 * In video memory, lightmaps are chunked into NxN RGBA blocks.
 * In the bsp, they are just RGB, and we retrieve them using floating point for precision.
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
#include "r_entity.h"
#include "r_lightmap.h"

lightmaps_t r_lightmaps;

static void R_UploadLightmapBlock (void)
{
	if (r_lightmaps.lightmap_texnum == MAX_GL_LIGHTMAPS) {
		Com_Printf("R_UploadLightmapBlock: MAX_GL_LIGHTMAPS reached.\n");
		return;
	}

	R_BindTexture(r_lightmaps.lightmap_texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGBA, GL_UNSIGNED_BYTE, r_lightmaps.sample_buffer);

	R_CheckError();

	r_lightmaps.lightmap_texnum++;

	if (r_lightmaps.deluxemap_texnum == MAX_GL_DELUXEMAPS) {
		Com_Printf("R_UploadLightmapBlock: MAX_GL_DELUXEMAPS reached.\n");
		return;
	}

	R_BindTexture(r_lightmaps.deluxemap_texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_lightmaps.size, r_lightmaps.size,
		0, GL_RGBA, GL_UNSIGNED_BYTE, r_lightmaps.direction_buffer);

	r_lightmaps.deluxemap_texnum++;

	/* clear the allocation block and buffers */
	memset(r_lightmaps.allocated, 0, r_lightmaps.size * sizeof(unsigned));
	memset(r_lightmaps.sample_buffer, 0, r_lightmaps.size * r_lightmaps.size * sizeof(unsigned));
	memset(r_lightmaps.direction_buffer, 0, r_lightmaps.size * r_lightmaps.size * sizeof(unsigned));
}

/**
 * @brief returns a texture number and the position inside it
 */
static qboolean R_AllocLightmapBlock (int w, int h, int *x, int *y)
{
	int i, j;
	int best;

	best = r_lightmaps.size;

	for (i = 0; i < r_lightmaps.size - w; i++) {
		int best2 = 0;

		for (j = 0; j < w; j++) {
			if (r_lightmaps.allocated[i + j] >= best)
				break;
			if (r_lightmaps.allocated[i + j] > best2)
				best2 = r_lightmaps.allocated[i + j];
		}
		/* this is a valid spot */
		if (j == w) {
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > r_lightmaps.size)
		return qfalse;

	for (i = 0; i < w; i++)
		r_lightmaps.allocated[*x + i] = best + h;

	return qtrue;
}

/**
 * @brief Fullbridght lightmap
 * @sa R_BuildLightmap
 */
static void R_BuildDefaultLightmap (mBspSurface_t *surf, byte *sout, byte *dout, int stride)
{
	int i, j;

	const int smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	const int tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;

	stride -= (smax * 4);

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {
			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;
			sout[3] = 255;

			sout += LIGHTMAP_BLOCK_BYTES;

			dout[0] = 127;
			dout[1] = 127;
			dout[2] = 255;
			dout[3] = 255;

			dout += DELUXEMAP_BLOCK_BYTES;
		}
	}

	Vector4Set(surf->color, 1.0, 1.0, 1.0, 1.0);
}

/**
 * @brief Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGBA to the specified destinations.
 * @sa R_BuildDefaultLightmap
 */
static void R_BuildLightmap (mBspSurface_t *surf, byte *sout, byte *dout, int stride)
{
	unsigned int i, j;
	byte *lightmap, *lm, *l, *deluxemap, *dm;

	const int smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	const int tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;
	const int size = smax * tmax;
	stride -= (smax * LIGHTMAP_BLOCK_BYTES);

	lightmap = (byte *)Mem_PoolAlloc(size * LIGHTMAP_BLOCK_BYTES, vid_lightPool, 0);
	lm = lightmap;

	deluxemap = (byte *)Mem_PoolAlloc(size * DELUXEMAP_BLOCK_BYTES, vid_lightPool, 0);
	dm = deluxemap;

	/* convert the raw lightmap samples to floating point and scale them */
	for (i = j = 0; i < size; i++, lm += LIGHTMAP_BLOCK_BYTES, dm += DELUXEMAP_BLOCK_BYTES) {
		lm[0] = surf->samples[j++];
		lm[1] = surf->samples[j++];
		lm[2] = surf->samples[j++];
		lm[3] = 255;  /* pad alpha */

		/* read in directional samples for deluxe mapping as well */
		dm[0] = surf->samples[j++];
		dm[1] = surf->samples[j++];
		dm[2] = surf->samples[j++];
		dm[3] = 255;  /* pad alpha */
	}

	/* apply modulate, contrast, resolve average surface color, etc.. */
	R_FilterTexture((unsigned *)lightmap, smax, tmax, it_lightmap);


	if (surf->texinfo->flags & (SURF_BLEND33 | SURF_ALPHATEST))
		surf->color[3] = 0.25;
	else if (surf->texinfo->flags & SURF_BLEND66)
		surf->color[3] = 0.50;
	else
		surf->color[3] = 1.0;

	/* soften it if it's sufficiently large */
	if (r_soften->integer && size > 128)
		for (i = 0; i < 4; i++) {
			R_SoftenTexture(lightmap, smax, tmax, LIGHTMAP_BLOCK_BYTES);
			R_SoftenTexture(deluxemap, smax, tmax, DELUXEMAP_BLOCK_BYTES);
		}

	/* the final lightmap is uploaded to the card via the strided lightmap
	 * block, and also cached on the surface for fast point lighting lookups */

	surf->lightmap = (byte *)Mem_PoolAlloc(size * LIGHTMAP_BYTES, vid_lightPool, 0);
	l = surf->lightmap;
	lm = lightmap;
	dm = deluxemap;

	for (i = 0; i < tmax; i++, sout += stride, dout += stride) {
		for (j = 0; j < smax; j++) {
			/* copy the lightmap to the strided block */
			sout[0] = lm[0];
			sout[1] = lm[1];
			sout[2] = lm[2];
			sout[3] = lm[3];
			sout += LIGHTMAP_BLOCK_BYTES;

			/* and to the surface, discarding alpha */
			l[0] = lm[0];
			l[1] = lm[1];
			l[2] = lm[2];
			l += LIGHTMAP_BYTES;

			lm += LIGHTMAP_BLOCK_BYTES;

			/* lastly copy the deluxemap to the strided block */
			dout[0] = dm[0];
			dout[1] = dm[1];
			dout[2] = dm[2];
			dout[3] = dm[3];
			dout += DELUXEMAP_BLOCK_BYTES;

			dm += DELUXEMAP_BLOCK_BYTES;
		}
	}

	Mem_Free(lightmap);
	Mem_Free(deluxemap);
}

/**
 * @sa R_ModLoadSurfaces
 */
void R_CreateSurfaceLightmap (mBspSurface_t * surf)
{
	int smax, tmax;
	byte *samples, *directions;

	if (!(surf->flags & MSURF_LIGHTMAP))
		return;

	smax = (surf->stextents[0] / surf->lightmap_scale) + 1;
	tmax = (surf->stextents[1] / surf->lightmap_scale) + 1;

	if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)) {
		/* upload the last block */
		R_UploadLightmapBlock();
		if (!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t))
			Com_Error(ERR_DROP, "R_CreateSurfaceLightmap: Consecutive calls to R_AllocLightmapBlock(%d,%d) failed (lightmap_scale: %i)\n", smax, tmax, surf->lightmap_scale);
	}

	surf->lightmap_texnum = r_lightmaps.lightmap_texnum;
	surf->deluxemap_texnum = r_lightmaps.deluxemap_texnum;

	samples = r_lightmaps.sample_buffer;
	samples += (surf->light_t * r_lightmaps.size + surf->light_s) * LIGHTMAP_BLOCK_BYTES;

	directions = r_lightmaps.direction_buffer;
	directions += (surf->light_t * r_lightmaps.size + surf->light_s) * DELUXEMAP_BLOCK_BYTES;

	if (!surf->samples)  /* make it fullbright */
		R_BuildDefaultLightmap(surf, samples, directions, r_lightmaps.size * LIGHTMAP_BLOCK_BYTES);
	else  /* or light it properly */
		R_BuildLightmap(surf, samples, directions, r_lightmaps.size * LIGHTMAP_BLOCK_BYTES);
}

/**
 * @sa R_ModBeginLoading
 * @sa R_EndBuildingLightmaps
 */
void R_BeginBuildingLightmaps (void)
{
	/* users can tune lightmap size for their card */
	r_lightmaps.size = r_maxlightmap->integer;

	r_lightmaps.allocated = (unsigned *)Mem_PoolAlloc(r_lightmaps.size *
		sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.sample_buffer = (byte *)Mem_PoolAlloc(
		r_lightmaps.size * r_lightmaps.size * sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.direction_buffer = (byte *)Mem_PoolAlloc(
		r_lightmaps.size * r_lightmaps.size * sizeof(unsigned), vid_lightPool, 0);

	r_lightmaps.lightmap_texnum = TEXNUM_LIGHTMAPS;
	r_lightmaps.deluxemap_texnum = TEXNUM_DELUXEMAPS;
}

/**
 * @sa R_BeginBuildingLightmaps
 * @sa R_ModBeginLoading
 */
void R_EndBuildingLightmaps (void)
{
	/* upload the pending lightmap block */
	R_UploadLightmapBlock();
}


/**
 * @brief Moves the given mins/maxs volume through the world from start to end.
 * @param[in] start Start vector to start the trace from
 * @param[in] end End vector to stop the trace at
 * @param[in] mins Bounding box used for tracing
 * @param[in] maxs Bounding box used for tracing
 * @param[in] contentmask Searched content the trace should watch for
 */
static void R_Trace (vec3_t start, vec3_t end, float size, int contentmask)
{
	vec3_t mins, maxs;
	float frac;
	trace_t tr;
	int i;

	r_locals.tracenum++;

	if (r_locals.tracenum > 0xffff)  /* avoid overflows */
		r_locals.tracenum = 0;

	VectorSet(mins, -size, -size, -size);
	VectorSet(maxs, size, size, size);

	refdef.trace = TR_CompleteBoxTrace(start, end, mins, maxs, 0x1FF, contentmask, 0);

	frac = refdef.trace.fraction;

	/* check bsp models */
	for (i = 0; i < r_numEntities; i++) {
		entity_t *ent = R_GetEntity(i);
		const model_t *m = ent->model;

		if (!m || m->type != mod_bsp_submodel)
			continue;

		tr = TR_TransformedBoxTrace(start, end, mins, maxs, &mapTiles[m->bsp.maptile], m->bsp.firstnode,
				contentmask, 0, ent->origin, ent->angles);

		if (tr.fraction < frac) {
			refdef.trace = tr;
			refdef.trace_ent = ent;

			frac = tr.fraction;
		}
	}
}


/**
 * @brief Clip to all surfaces within the specified range, accumulating static lighting
 * color to the specified vector in the event of an intersection.
 * @todo This is not yet working because we are using some special nodes for
 * pathfinding @sa BuildNodeChildren - and these nodes don't have a plane assigned
 */
static qboolean R_LightPoint_ (const int tile, const int firstsurface, const int numsurfaces, const vec3_t point, vec3_t color)
{
	mBspSurface_t *surf;
	int i, width, sample;
	float s, t;

	/* resolve the surface that was impacted */
	surf = r_mapTiles[tile]->bsp.surfaces + firstsurface;

	for (i = 0; i < numsurfaces; i++, surf++) {
		const mBspTexInfo_t *tex = surf->texinfo;

		if (!(surf->flags & MSURF_LIGHTMAP))
			continue;	/* no lightmap */

#if 0
		/** @todo Texture names don't match - because image_t holds the full path */
		if (strcmp(refdef.trace.surface->name, tex->image->name))
			continue;	/* different material */

		if (!VectorCompare(refdef.trace.plane.normal, surf->plane->normal))
			continue;	/* facing the wrong way */
#endif

		if (surf->tracenum == r_locals.tracenum)
			continue;	/* already checked this trace */

		surf->tracenum = r_locals.tracenum;

		s = DotProduct(point, tex->vecs[0]) + tex->vecs[0][3] - surf->stmins[0];
		t = DotProduct(point, tex->vecs[1]) + tex->vecs[1][3] - surf->stmins[1];

		if ((s < 0.0 || s > surf->stextents[0]) || (t < 0.0 || t > surf->stextents[1]))
			continue;

		/* we've hit, resolve the texture coordinates */
		s /= surf->lightmap_scale;
		t /= surf->lightmap_scale;

		/* resolve the lightmap at intersection */
		width = (surf->stextents[0] / surf->lightmap_scale) + 1;
		sample = (int)(3 * ((int)t * width + (int)s));

		/* and convert it to floating point */
		VectorScale((&surf->lightmap[sample]), 1.0 / 255.0, color);
		return qtrue;
	}

	return qfalse;
}

/**
* @sa R_LightPoint_
*/
void R_LightPoint (const vec3_t point, static_lighting_t *lighting)
{
	vec3_t start, end;

	/* clear it */
	memset(lighting, 0, sizeof(*lighting));

	VectorCopy(point, start);
	VectorCopy(point, end);
	end[2] -= 256.0;

	R_Trace(start, end, 0.0, MASK_SOLID);

	/* didn't hit anything */
	if (!refdef.trace.leafnum) {
		/** @todo use worldspawn light and ambient settings to get a better value here */
		VectorSet(lighting->color, 0.5, 0.5, 0.5);
		return;
	}

	/* maptile is not lit */
	if (!r_mapTiles[refdef.trace.mapTile]->bsp.lightdata) {
		VectorSet(lighting->color, 1.0, 1.0, 1.0);
		return;
	}

	VectorCopy(refdef.trace.endpos, lighting->point);
	VectorCopy(refdef.trace.plane.normal, lighting->normal);

	/* clip to all surfaces of the bsp entity */
	if (refdef.trace_ent) {
		VectorSubtract(refdef.trace.endpos,
				refdef.trace_ent->origin, refdef.trace.endpos);

		R_LightPoint_(refdef.trace_ent->model->bsp.maptile,
				refdef.trace_ent->model->bsp.firstmodelsurface,
				refdef.trace_ent->model->bsp.nummodelsurfaces,
				refdef.trace.endpos, lighting->color);
	} else {
		mBspLeaf_t *leaf = &r_mapTiles[refdef.trace.mapTile]->bsp.leafs[refdef.trace.leafnum];
		mBspNode_t *node = leaf->parent;
		/** @todo this doesn't work yet - node is always(?) null */
		while (node) {
			if (R_LightPoint_(refdef.trace.mapTile, node->firstsurface, node->numsurfaces, refdef.trace.endpos, lighting->color))
				break;
			node = node->parent;
		}
	}
}
