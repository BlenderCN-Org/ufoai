/**
 * @file r_shadows.c
 * @brief shadow functions
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

vec4_t s_lerped[MD2_MAX_VERTS];
static vec3_t shadevector;
float shadelight[3];

static int c_shadow_volumes = 0;

/*
=============================================
  Dynamic Planar Shadows
  Original Code By Psychospaz
  Modificated By Vortex and Kirk Barnes
=============================================
*/

/**
 * @brief
 */
static void vectoangles (vec3_t value1, vec3_t angles)
{
	float forward;
	float yaw, pitch;

	if (value1[1] == 0 && value1[0] == 0) {
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	} else {
		/* PMM - fixed to correct for pitch of 0 */
		if (value1[0])
			yaw = (atan2(value1[1], value1[0]) * todeg);
		else if (value1[1] > 0)
			yaw = 90;
		else
			yaw = 270;

		if (yaw < 0)
			yaw += 360;

		forward = sqrt(value1[0] * value1[0] + value1[1] * value1[1]);
		pitch = (atan2(value1[2], forward) * todeg);
		if (pitch < 0)
			pitch += 360;
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}

static qboolean nolight;
/**
 * @brief
 */
static void R_ShadowLight (vec3_t pos, vec3_t lightAdd)
{
	int i;
	dlight_t *dl;
	float shadowdist;
	float add;
	vec3_t dist;
	vec3_t angle;

	nolight = qfalse;
	VectorClear(lightAdd);		/* clear planar shadow vector */

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	/* add dynamic light shadow angles */
	dl = refdef.dlights;
	for (i = 0; i < refdef.num_dlights; i++, dl++) {
/*		if (dl->spotlight) // spotlights */
/*			continue; */

		VectorSubtract(dl->origin, pos, dist);
		add = sqrt(dl->intensity - VectorLength(dist));
		VectorNormalize(dist);
		if (add > 0) {
			VectorScale(dist, add, dist);
			VectorAdd(lightAdd, dist, lightAdd);
		}
	}

	shadowdist = VectorNormalize(lightAdd);
	if (shadowdist > 1)
		shadowdist = 1;
	if (shadowdist <= 0) {
		/* old style static shadow */
		/* scaled nolight shadow */
		nolight = qtrue;
		return;
	} else {
		/* shadow from dynamic lights */
		vectoangles(lightAdd, angle);
		angle[YAW] -= currententity->angles[YAW];
	}
	AngleVectors(angle, dist, NULL, NULL);
	VectorScale(dist, shadowdist, lightAdd);
}

/**
 * @brief
 */
static void R_DrawAliasShadow (entity_t * e, mdl_md2_t * paliashdr, int posenum)
{
	dtrivertx_t *verts;
	int *order;
	vec3_t point;
	float height, lheight, alpha;
	int count;
	vec4_t color = {0, 0, 0, 0.5};

	dAliasFrame_t *frame;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	/* calculate model lighting and setup shadow transparenty */
	R_LightPoint(currententity->origin, shadelight);

	alpha = 1 - (shadelight[0] + shadelight[1] + shadelight[2]);

	if (alpha <= 0.3)
		alpha = 0.3;

	lheight = currententity->origin[2];

	frame = (dAliasFrame_t *) ((byte *) paliashdr + paliashdr->ofs_frames + currententity->as.frame * paliashdr->framesize);

	verts = frame->verts;
	height = 0;
	order = (int *) ((byte *) paliashdr + paliashdr->ofs_glcmds);
	height = -lheight + 0.1;

	if (nolight)
		qglScalef(1.1, 1.1, 1);	/* scale  nolight shadow by Kirk Barnes */

	color[3] = alpha;
	R_Color(color);
	qglPolygonOffset(-2.0, 1.0);	/* shadow on the floor c14 */
	qglEnable(GL_POLYGON_OFFSET_FILL);

	qglEnable(GL_STENCIL_TEST);	/* stencil buffered shadow by MrG */
	qglStencilFunc(GL_EQUAL, 1, 2);
	R_CheckError();
	qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
	R_CheckError();

	qglBlendFunc(GL_SRC_ALPHA_SATURATE, GL_ONE_MINUS_SRC_ALPHA);
	R_CheckError();

	while (1) {
		/* get the vertex count and primitive type */
		count = *order++;
		if (!count)
			break;				/* done */
		if (count < 0) {
			count = -count;
			qglBegin(GL_TRIANGLE_FAN);
		} else
			qglBegin(GL_TRIANGLE_STRIP);

		do {
			assert(order[2] < MD2_MAX_VERTS);
			/* normals and vertexes come from the frame list */
			memcpy(point, s_lerped[order[2]], sizeof(point));
			point[0] -= shadevector[0] * (point[2] + lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
			qglVertex3fv(point);
			order += 3;
		} while (--count);
		qglEnd();
	}

	qglDisable(GL_STENCIL_TEST);
	qglDisable(GL_POLYGON_OFFSET_FILL);
	R_Color(NULL);
}

/**
 * @brief
 * @sa R_RenderVolumes
 */
static void BuildShadowVolume (mdl_md2_t * hdr, vec3_t light, float projectdistance)
{
	dtriangle_t *ot, *tris;
	mAliasNeighbors_t	*neighbors;
	int i, j;
	qboolean trianglefacinglight[MD2_MAX_TRIANGLES];
	vec3_t v0, v1, v2, v3;
	dAliasFrame_t *frame;
	dtrivertx_t *verts;

	frame = (dAliasFrame_t *) ((byte *) hdr + hdr->ofs_frames + currententity->as.frame * hdr->framesize);
	verts = frame->verts;

	ot = tris = (dtriangle_t *) ((unsigned char *) hdr + hdr->ofs_tris);

	for (i = 0; i < hdr->num_tris; i++, tris++) {
		for (j = 0; j < 3; j++) {
			v0[j] = s_lerped[tris->index_xyz[0]][j];
			v1[j] = s_lerped[tris->index_xyz[1]][j];
			v2[j] = s_lerped[tris->index_xyz[2]][j];
		}

		trianglefacinglight[i] = (light[0] - v0[0]) * ((v0[1] - v1[1]) * (v2[2] - v1[2]) - (v0[2] - v1[2]) * (v2[1] - v1[1]))
			+ (light[1] - v0[1]) * ((v0[2] - v1[2]) * (v2[0] - v1[0]) - (v0[0] - v1[0]) * (v2[2] - v1[2]))
			+ (light[2] - v0[2]) * ((v0[0] - v1[0]) * (v2[1] - v1[1]) - (v0[1] - v1[1]) * (v2[0] - v1[0])) > 0;
	}

	qglBegin(GL_QUADS);
	for (i = 0, tris = ot, neighbors = currentmodel->alias.neighbors; i < hdr->num_tris; i++, tris++, neighbors++) {
		if (!trianglefacinglight[i])
			continue;

		if (neighbors->n[0] < 0 || !trianglefacinglight[neighbors->n[0]]) {
			for (j = 0; j < 3; j++) {
				v0[j] = s_lerped[tris->index_xyz[1]][j];
				v1[j] = s_lerped[tris->index_xyz[0]][j];

				v2[j] = v1[j] + ((v1[j] - light[j]) * projectdistance);
				v3[j] = v0[j] + ((v0[j] - light[j]) * projectdistance);
			}

			qglVertex3fv(v0);
			qglVertex3fv(v1);
			qglVertex3fv(v2);
			qglVertex3fv(v3);
		}

		if (neighbors->n[1] < 0 || !trianglefacinglight[neighbors->n[1]]) {
			for (j = 0; j < 3; j++) {
				v0[j] = s_lerped[tris->index_xyz[2]][j];
				v1[j] = s_lerped[tris->index_xyz[1]][j];

				v2[j] = v1[j] + ((v1[j] - light[j]) * projectdistance);
				v3[j] = v0[j] + ((v0[j] - light[j]) * projectdistance);
			}

			qglVertex3fv(v0);
			qglVertex3fv(v1);
			qglVertex3fv(v2);
			qglVertex3fv(v3);
		}

		if (neighbors->n[2] < 0 || !trianglefacinglight[neighbors->n[2]]) {
			for (j = 0; j < 3; j++) {
				v0[j] = s_lerped[tris->index_xyz[0]][j];
				v1[j] = s_lerped[tris->index_xyz[2]][j];

				v2[j] = v1[j] + ((v1[j] - light[j]) * projectdistance);
				v3[j] = v0[j] + ((v0[j] - light[j]) * projectdistance);
			}
			qglVertex3fv(v0);
			qglVertex3fv(v1);
			qglVertex3fv(v2);
			qglVertex3fv(v3);
		}
	}
	qglEnd();

	/* cap the volume */
	qglBegin(GL_TRIANGLES);
	for (i = 0, tris = ot; i < hdr->num_tris; i++, tris++) {
		if (trianglefacinglight[i]) {
			for (j = 0; j < 3; j++) {
				v0[j] = s_lerped[tris->index_xyz[0]][j];
				v1[j] = s_lerped[tris->index_xyz[1]][j];
				v2[j] = s_lerped[tris->index_xyz[2]][j];
			}
			qglVertex3fv(v0);
			qglVertex3fv(v1);
			qglVertex3fv(v2);
			continue;
		}
		for (j = 0; j < 3; j++) {
			v0[j] = s_lerped[tris->index_xyz[0]][j];
			v1[j] = s_lerped[tris->index_xyz[1]][j];
			v2[j] = s_lerped[tris->index_xyz[2]][j];

			v0[j] = v0[j] + ((v0[j] - light[j]) * projectdistance);
			v1[j] = v1[j] + ((v1[j] - light[j]) * projectdistance);
			v2[j] = v2[j] + ((v2[j] - light[j]) * projectdistance);
		}

		qglVertex3fv(v0);
		qglVertex3fv(v1);
		qglVertex3fv(v2);
	}
	qglEnd();
}

/**
 * @brief
 * @sa R_DrawAliasShadowVolume
 * @sa BuildShadowVolume
 */
static void R_RenderVolumes (mdl_md2_t * paliashdr, vec3_t lightdir, int projdist)
{
	int incr = r_state.stencil_wrap ? GL_INCR_WRAP_EXT : GL_INCR;
	int decr = r_state.stencil_wrap ? GL_DECR_WRAP_EXT : GL_DECR;

	if (r_state.ati_separate_stencil) {	/* ati separate stensil support for r300+ by Kirk Barnes */
		qglDisable(GL_CULL_FACE);
		qglStencilOpSeparateATI(GL_BACK, GL_KEEP, incr, GL_KEEP);
		R_CheckError();
		qglStencilOpSeparateATI(GL_FRONT, GL_KEEP, decr, GL_KEEP);
		R_CheckError();
		BuildShadowVolume(paliashdr, lightdir, projdist);
		qglEnable(GL_CULL_FACE);
	} else if (r_state.stencil_two_side) {	/* two side stensil support for nv30+ by Kirk Barnes */
		qglDisable(GL_CULL_FACE);
		qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
		R_CheckError();
		qglActiveStencilFaceEXT(GL_BACK);
		R_CheckError();
		qglStencilOp(GL_KEEP, incr, GL_KEEP);
		R_CheckError();
		qglActiveStencilFaceEXT(GL_FRONT);
		R_CheckError();
		qglStencilOp(GL_KEEP, decr, GL_KEEP);
		R_CheckError();
		BuildShadowVolume(paliashdr, lightdir, projdist);
		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
		qglEnable(GL_CULL_FACE);
	} else {
		/* decrement stencil if backface is behind depthbuffer */
		qglCullFace(GL_BACK); /* quake is backwards, this culls front faces */
		qglStencilOp(GL_KEEP, incr, GL_KEEP);
		R_CheckError();
		BuildShadowVolume(paliashdr, lightdir, projdist);

		/* increment stencil if frontface is behind depthbuffer */
		qglCullFace(GL_FRONT); /* quake is backwards, this culls back faces */
		qglStencilOp(GL_KEEP, decr, GL_KEEP);
		R_CheckError();
		BuildShadowVolume(paliashdr, lightdir, projdist);
	}
}

/**
 * @brief
 * @sa R_DrawShadowVolume
 */
static void R_DrawAliasShadowVolume (mdl_md2_t * paliashdr, int posenumm)
{
	int *order, i, o, dist;
	vec3_t light, temp;
	dtriangle_t *t, *tris;
	int worldlight = 0;
	vec4_t color = {1, 0, 0, 1};

	dAliasFrame_t *frame, *oldframe;
	dtrivertx_t *ov, *verts;
	dlight_t *l;
	float cost, sint, is, it;
	int projected_distance;

	c_shadow_volumes = 0;
	l = refdef.dlights;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;
	if (currentmodel->alias.noshadow)
		return;

	t = tris = (dtriangle_t *) ((unsigned char *) paliashdr + paliashdr->ofs_tris);

	frame = (dAliasFrame_t *) ((byte *) paliashdr + paliashdr->ofs_frames + currententity->as.frame * paliashdr->framesize);
	verts = frame->verts;

	oldframe = (dAliasFrame_t *) ((byte *) paliashdr + paliashdr->ofs_frames + currententity->as.oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *) ((byte *) paliashdr + paliashdr->ofs_glcmds);

	cost = cos(-currententity->angles[1] / 180 * M_PI), sint = sin(-currententity->angles[1] / 180 * M_PI);

/*	if (!ri.IsVisible(refdef.vieworg, currententity->origin)) return; */

	qglPushAttrib(GL_STENCIL_BUFFER_BIT); /* save stencil buffer */
	R_CheckError();

	if (r_shadow_debug_volume->integer)
		R_Color(color);
	else {
		qglColorMask(0, 0, 0, 0);
		R_CheckError();
	}

	if (r_state.stencil_two_side)
		qglEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

	qglEnable(GL_STENCIL_TEST);
	R_CheckError();

	qglDepthMask(GL_FALSE);
	qglDepthFunc(GL_LESS);

	if (r_state.ati_separate_stencil)
		qglStencilFuncSeparateATI(GL_EQUAL, GL_EQUAL, 1, 2);
	else
		qglStencilFunc(GL_EQUAL, 1, 2);
	R_CheckError();

	qglStencilOp(GL_KEEP, GL_KEEP, (r_state.stencil_wrap ? GL_INCR_WRAP_EXT : GL_INCR));
	R_CheckError();

	for (i = 0; i < refdef.num_dlights; i++, l++) {
		if ((l->origin[0] == currententity->origin[0]) && (l->origin[1] == currententity->origin[1]) && (l->origin[2] == currententity->origin[2]))
			continue;
		VectorSubtract(currententity->origin, l->origin, temp);
		dist = sqrt(DotProduct(temp, temp));

		if (dist > 200)
			continue;

		/* lights origin in relation to the entity */
		for (o = 0; o < 3; o++)
			light[o] = -currententity->origin[o] + l->origin[o];

		is = light[0], it = light[1];
		light[0] = (cost * (is - 0) + sint * (0 - it) + 0);
		light[1] = (cost * (it - 0) + sint * (is - 0) + 0);
		light[2] += 8;

		c_shadow_volumes++;
		worldlight++;
	}

	if (!worldlight) {
		/*old style static shadow vector */
		VectorSet(light, 130, 0, 200);
		is = light[0], it = light[1];
		light[0] = (cost * (is - 0) + sint * (0 - it) + 0);
		light[1] = (cost * (it - 0) + sint * (is - 0) + 0);
		light[2] += 8;
		c_shadow_volumes++;
		projected_distance = 1;
	} else
		projected_distance = 25;

	R_RenderVolumes(paliashdr, light, projected_distance);

	if (r_state.stencil_two_side)
		qglDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);

	qglDisable(GL_STENCIL_TEST);

	if (r_shadow_debug_volume->integer)
		R_Color(NULL);
	else {
		qglColorMask(1, 1, 1, 1);
		R_CheckError();
	}

	qglPopAttrib(); /* restore stencil buffer */
	R_CheckError();
	qglStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	R_CheckError();
	qglDepthMask(GL_TRUE);
	qglDepthFunc(GL_LEQUAL);
/*	Com_DPrintf(DEBUG_RENDERER, "worldlight: %i - c_shadow_volumes: %i - dlights: %i\n", worldlight, c_shadow_volumes, refdef.num_dlights);*/
}

/**
 * @brief
 * @sa R_DrawShadowVolume
 * @sa R_CastShadow
 */
void R_DrawShadow (entity_t * e)
{
	mdl_md2_t *paliashdr;

	dAliasFrame_t *frame, *oldframe;
	dtrivertx_t *v, *ov, *verts;
	int *order;
	float frontlerp;
	vec3_t move, delta, vectors[3];
	vec3_t frontv, backv;
	int i;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	assert(currentmodel->type == mod_alias_md2);
	paliashdr = (mdl_md2_t *) currentmodel->alias.extraData;

	frame = (dAliasFrame_t *) ((byte *) paliashdr + paliashdr->ofs_frames + currententity->as.frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (dAliasFrame_t *) ((byte *) paliashdr + paliashdr->ofs_frames + currententity->as.oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *) ((byte *) paliashdr + paliashdr->ofs_glcmds);

	frontlerp = 1.0 - currententity->as.backlerp;

	/* move should be the delta back to the previous frame * backlerp */
	VectorSubtract(currententity->oldorigin, currententity->origin, delta);
	AngleVectors(currententity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct(delta, vectors[0]);	/* forward */
	move[1] = -DotProduct(delta, vectors[1]);	/* left */
	move[2] = DotProduct(delta, vectors[2]);	/* up */

	VectorAdd(move, oldframe->translate, move);

	for (i = 0; i < 3; i++) {
		move[i] = currententity->as.backlerp * move[i] + frontlerp * frame->translate[i];
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = currententity->as.backlerp * oldframe->scale[i];
	}

/*	R_LerpVerts(paliashdr->num_xyz, v, ov, s_lerped[0], move, frontv, backv, 0); */

	/*|RF_NOSHADOW */
	if (!(currententity->flags & RF_TRANSLUCENT)) {
		qglPushMatrix();
		{
			vec3_t end;

			qglDisable(GL_TEXTURE_2D);
			RSTATE_ENABLE_BLEND
			qglDepthMask(GL_FALSE);
			qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);
			qglRotatef(e->angles[1], 0, 0, 1);
			VectorSet(end, currententity->origin[0], currententity->origin[1], currententity->origin[2] - 2048);
			/* FIXME: go through other rTiles, too */
			RecursiveLightPoint(rTiles[0], rTiles[0]->bsp.nodes, currententity->origin, end);
			R_ShadowLight(currententity->origin, shadevector);
			R_DrawAliasShadow(e, paliashdr, currententity->as.frame);
			qglDepthMask(GL_TRUE);
		}
		qglEnable(GL_TEXTURE_2D);
		RSTATE_DISABLE_BLEND
		qglPopMatrix();
	}
}

/**
 * @brief
 * @sa R_DrawAliasMD2Model
 * @sa R_DrawAliasMD3Model
 * @sa R_CastShadow
 */
void R_DrawShadowVolume (entity_t * e)
{
	mdl_md2_t *paliashdr;
	dAliasFrame_t *frame, *oldframe;
	dtrivertx_t *v, *ov, *verts;
	float frontlerp;
	vec3_t move, delta, vectors[3];
	vec3_t frontv, backv;
	int i;
	int *order;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	assert(currentmodel->type == mod_alias_md2);
	paliashdr = (mdl_md2_t *) currentmodel->alias.extraData;

	frame = (dAliasFrame_t *) ((byte *) paliashdr + paliashdr->ofs_frames + currententity->as.frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (dAliasFrame_t *) ((byte *) paliashdr + paliashdr->ofs_frames + currententity->as.oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *) ((byte *) paliashdr + paliashdr->ofs_glcmds);

	frontlerp = 1.0 - currententity->as.backlerp;

	/* move should be the delta back to the previous frame * backlerp */
	VectorSubtract(currententity->oldorigin, currententity->origin, delta);
	AngleVectors(currententity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct(delta, vectors[0]);	/* forward */
	move[1] = -DotProduct(delta, vectors[1]);	/* left */
	move[2] = DotProduct(delta, vectors[2]);	/* up */

	VectorAdd(move, oldframe->translate, move);

	for (i = 0; i < 3; i++) {
		move[i] = currententity->as.backlerp * move[i] + frontlerp * frame->translate[i];
		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = currententity->as.backlerp * oldframe->scale[i];
	}

/*	R_LerpVerts(paliashdr->num_xyz, v, ov, s_lerped[0], move, frontv, backv, 0); */

/*	|RF_NOSHADOW|RF_NOSHADOW2 */
	if (!(currententity->flags & RF_TRANSLUCENT)) {
		qglPushMatrix();
		qglDisable(GL_TEXTURE_2D);
		qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);
		qglRotatef(e->angles[1], 0, 0, 1);
		R_DrawAliasShadowVolume(paliashdr, currententity->as.frame);
		qglEnable(GL_TEXTURE_2D);
		qglPopMatrix();
	}
}

/**
 * @brief
 * @sa R_Flash
 */
void R_ShadowBlend (void)
{
	vec4_t color = {0, 0, 0, 0.5};
	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (r_shadows->integer < 2)
		return;

	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, 1, 1, 0, -99999, 99999);
	R_CheckError();

	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	RSTATE_DISABLE_ALPHATEST
	RSTATE_ENABLE_BLEND
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	R_CheckError();
	qglDepthMask(GL_FALSE);
	qglDisable(GL_TEXTURE_2D);
	R_Color(color);
	R_CheckError();

	qglEnable(GL_STENCIL_TEST);
	R_CheckError();
	qglStencilFunc(GL_EQUAL, 1, 2);
	R_CheckError();

	qglBegin(GL_TRIANGLES);
	qglVertex2f(-5, -5);
	qglVertex2f(10, -5);
	qglVertex2f(-5, 10);
	qglEnd();

	RSTATE_DISABLE_BLEND
	qglEnable(GL_TEXTURE_2D);
	RSTATE_ENABLE_ALPHATEST
	qglDisable(GL_STENCIL_TEST);
	R_CheckError();
	qglDepthMask(GL_TRUE);

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();

	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();
}
