/**
 * @file r_entity.c
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
#include "r_entity.h"
#include "r_mesh_anim.h"
#include "r_error.h"

int r_numEntities;
static entity_t r_entities[MAX_ENTITIES];

#define HIGHLIGHT_START_Z 22
#define HIGHTLIGHT_SIZE 18
static const vec3_t r_highlightVertices[HIGHTLIGHT_SIZE] = {
	{4, 4, HIGHLIGHT_START_Z + 0},
	{0, 0, HIGHLIGHT_START_Z + 16},
	{8, 0, HIGHLIGHT_START_Z + 16},
	{4, 4, HIGHLIGHT_START_Z + 0},
	{0, 0, HIGHLIGHT_START_Z + 16},
	{0, 8, HIGHLIGHT_START_Z + 16},
	{4, 4, HIGHLIGHT_START_Z + 0},
	{0, 8, HIGHLIGHT_START_Z + 16},
	{8, 8, HIGHLIGHT_START_Z + 16},
	{4, 4, HIGHLIGHT_START_Z + 0},
	{8, 8, HIGHLIGHT_START_Z + 16},
	{8, 0, HIGHLIGHT_START_Z + 16},
	{0, 0, HIGHLIGHT_START_Z + 16},
	{0, 8, HIGHLIGHT_START_Z + 16},
	{8, 0, HIGHLIGHT_START_Z + 16},
	{0, 8, HIGHLIGHT_START_Z + 16},
	{8, 0, HIGHLIGHT_START_Z + 16},
	{8, 8, HIGHLIGHT_START_Z + 16},
};
/**
 * @brief Used to draw actor highlights over the actors
 * @sa RF_HIGHLIGHT
 */
static inline void R_DrawHighlight (const entity_t * e)
{
	glDisable(GL_TEXTURE_2D);
	R_Color(NULL);
	memcpy(r_state.vertex_array_3d, r_highlightVertices, sizeof(r_highlightVertices));
	glDrawArrays(GL_TRIANGLES, 0, HIGHTLIGHT_SIZE);
	glEnable(GL_TEXTURE_2D);
}

/**
 * @brief Compute the bouding box for an entity out of the mins, maxs
 * @sa R_EntityDrawBBox
 */
void R_EntityComputeBoundingBox (const vec3_t mins, const vec3_t maxs, vec3_t bbox[8])
{
	int i;

	/* compute a full bounding box */
	for (i = 0; i < 8; i++) {
		bbox[i][0] = (i & 1) ? mins[0] : maxs[0];
		bbox[i][1] = (i & 2) ? mins[1] : maxs[1];
		bbox[i][2] = (i & 4) ? mins[2] : maxs[2];
	}
}

void R_TransformForEntity (const entity_t *e)
{
	glTranslatef(e->origin[0], e->origin[1], e->origin[2]);

	glRotatef(e->angles[YAW], 0, 0, 1);
	glRotatef(e->angles[PITCH], 0, 1, 0);
	glRotatef(e->angles[ROLL], 1, 0, 0);
}

/**
 * @brief Draws the model bounding box
 * @sa R_EntityComputeBoundingBox
 */
void R_EntityDrawBBox (vec3_t bbox[8])
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* Draw top and sides */
	glBegin(GL_TRIANGLE_STRIP);
	glVertex3fv(bbox[2]);
	glVertex3fv(bbox[1]);
	glVertex3fv(bbox[0]);
	glVertex3fv(bbox[1]);
	glVertex3fv(bbox[4]);
	glVertex3fv(bbox[5]);
	glVertex3fv(bbox[1]);
	glVertex3fv(bbox[7]);
	glVertex3fv(bbox[3]);
	glVertex3fv(bbox[2]);
	glVertex3fv(bbox[7]);
	glVertex3fv(bbox[6]);
	glVertex3fv(bbox[2]);
	glVertex3fv(bbox[4]);
	glVertex3fv(bbox[0]);
	glEnd();

	/* Draw bottom */
	glBegin(GL_TRIANGLE_STRIP);
	glVertex3fv(bbox[4]);
	glVertex3fv(bbox[6]);
	glVertex3fv(bbox[7]);
	glEnd();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

/**
 * @brief Draws the field marker entity is specified in cl_actor.c CL_AddTargeting
 * @sa CL_AddTargeting
 * @sa RF_BOX
 */
static void R_DrawBox (const entity_t * e)
{
	vec3_t upper, lower;
	float dx, dy;
	const vec4_t color = {e->angles[0], e->angles[1], e->angles[2], e->alpha};

	glDisable(GL_TEXTURE_2D);
	if (!r_wire->integer)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_LINE_SMOOTH);

	R_Color(color);

	if (VectorNotEmpty(e->mins) && VectorNotEmpty(e->maxs)) {
		vec3_t bbox[8];
		R_EntityComputeBoundingBox(e->mins, e->maxs, bbox);
		R_EntityDrawBBox(bbox);
	} else {
		VectorCopy(e->origin, lower);
		VectorCopy(e->origin, upper);
		upper[2] = e->oldorigin[2];

		dx = e->oldorigin[0] - e->origin[0];
		dy = e->oldorigin[1] - e->origin[1];

		glBegin(GL_QUAD_STRIP);
		glVertex3fv(lower);
		glVertex3fv(upper);
		lower[0] += dx;
		upper[0] += dx;
		glVertex3fv(lower);
		glVertex3fv(upper);
		lower[1] += dy;
		upper[1] += dy;
		glVertex3fv(lower);
		glVertex3fv(upper);
		lower[0] -= dx;
		upper[0] -= dx;
		glVertex3fv(lower);
		glVertex3fv(upper);
		lower[1] -= dy;
		upper[1] -= dy;
		glVertex3fv(lower);
		glVertex3fv(upper);
		glEnd();
	}

	glDisable(GL_LINE_SMOOTH);
	if (!r_wire->integer)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glEnable(GL_TEXTURE_2D);

	R_Color(NULL);
}

/**
 * @brief Draws a marker on the ground to indicate pathing CL_AddPathingBox
 * @sa CL_AddPathing
 * @sa RF_BOX
 */
static void R_DrawFloor (const entity_t * e)
{
	vec3_t upper, lower;
	float dx, dy;
	const vec4_t color = {e->angles[0], e->angles[1], e->angles[2], e->alpha};

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LINE_SMOOTH);

	R_Color(color);

	VectorCopy(e->origin, lower);
	VectorCopy(e->origin, upper);

	dx = PLAYER_WIDTH * 2;
	dy = e->oldorigin[2];

	upper[2] += dy;

	glBegin(GL_QUAD_STRIP);
	glVertex3fv(lower);
	glVertex3fv(upper);
	lower[0] += dx;
	upper[0] += dx;
	glVertex3fv(lower);
	glVertex3fv(upper);
	lower[1] += dx;
	upper[1] += dx;
	glVertex3fv(lower);
	glVertex3fv(upper);
	lower[0] -= dx;
	upper[0] -= dx;
	glVertex3fv(lower);
	glVertex3fv(upper);
	lower[1] -= dx;
	upper[1] -= dx;
	glVertex3fv(lower);
	glVertex3fv(upper);
	glEnd();

	lower[2] += dy;
	upper[1] += dx;

	glBegin(GL_QUAD_STRIP);
	glVertex3fv(lower);
	glVertex3fv(upper);
	lower[0] += dx;
	upper[0] += dx;
	glVertex3fv(lower);
	glVertex3fv(upper);
	glEnd();
	glDisable(GL_LINE_SMOOTH);

	glEnable(GL_TEXTURE_2D);

	R_Color(NULL);
}

/**
 * @brief Draws an arrow between two points
 * @sa CL_AddArrow
 * @sa RF_BOX
 */
static void R_DrawArrow (const entity_t * e)
{
	vec3_t upper, mid, lower;
	const vec4_t color = {e->angles[0], e->angles[1], e->angles[2], e->alpha};

	VectorCopy(e->origin, upper);
	upper[0] += 2;

	VectorCopy(e->origin, mid);
	mid[1] += 2;

	VectorCopy(e->origin, lower);
	lower[2] += 2;

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LINE_SMOOTH);

	R_Color(color);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3fv(e->oldorigin);
	glVertex3fv(upper);
	glVertex3fv(mid);
	glVertex3fv(lower);
	glEnd();

	glDisable(GL_LINE_SMOOTH);
	glEnable(GL_TEXTURE_2D);

	R_Color(NULL);
}

/**
 * @brief Draws shadow and highlight effects for the entities (actors)
 * @note The origins are already transformed
 */
static void R_DrawEntityEffects (void)
{
	int i;

	for (i = 0; i < r_numEntities; i++) {
		const entity_t *e = &r_entities[i];

		if (e->flags <= RF_BOX)
			continue;

		glPushMatrix();
		glMultMatrixf(e->transform.matrix);

		/* draw a highlight icon over this entity */
		if (e->flags & RF_HIGHLIGHT)
			R_DrawHighlight(e);

		if (r_shadows->integer && (e->flags & (RF_SHADOW | RF_BLOOD))) {
			/** @todo Shouldn't we get the texture type from the team-definition somehow? */
			if (e->flags & RF_SHADOW)
				R_BindTexture(shadow->texnum);
			else
				R_BindTexture(blood[e->state % STATE_DEAD]->texnum);

			glBegin(GL_QUADS);
			glTexCoord2f(0.0, 1.0);
			glVertex3f(-18.0, 14.0, -28.5);
			glTexCoord2f(1.0, 1.0);
			glVertex3f(10.0, 14.0, -28.5);
			glTexCoord2f(1.0, 0.0);
			glVertex3f(10.0, -14.0, -28.5);
			glTexCoord2f(0.0, 0.0);
			glVertex3f(-18.0, -14.0, -28.5);
			glEnd();

			R_CheckError();
		}

		if (e->flags & (RF_SELECTED | RF_ALLIED | RF_MEMBER)) {
			/* draw the circles for team-members and allied troops */
			vec4_t color = {1, 1, 1, 1};
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_TEXTURE_2D);
			glEnable(GL_LINE_SMOOTH);

			if (e->flags & RF_MEMBER) {
				if (e->flags & RF_SELECTED)
					Vector4Set(color, 0, 1, 0, 1);
				else
					Vector4Set(color, 0, 1, 0, 0.3);
			} else if (e->flags & RF_ALLIED)
				Vector4Set(color, 0, 0.5, 1, 0.3);
			else
				Vector4Set(color, 0, 1, 0, 1);

			R_Color(color);

#if 0
			glLineWidth(10.0f);
			glLineStipple(2, 0x00FF);
			glEnable(GL_LINE_STIPPLE);
#endif

			/* circle points */
			glBegin(GL_LINE_STRIP);
			glVertex3f(10.0, 0.0, -27.0);
			glVertex3f(7.0, -7.0, -27.0);
			glVertex3f(0.0, -10.0, -27.0);
			glVertex3f(-7.0, -7.0, -27.0);
			glVertex3f(-10.0, 0.0, -27.0);
			glVertex3f(-7.0, 7.0, -27.0);
			glVertex3f(0.0, 10.0, -27.0);
			glVertex3f(7.0, 7.0, -27.0);
			glVertex3f(10.0, 0.0, -27.0);
			glEnd();
			R_CheckError();

#if 0
			glLineWidth(1.0f);
			glDisable(GL_LINE_STIPPLE);
#endif
			glDisable(GL_LINE_SMOOTH);
			glEnable(GL_TEXTURE_2D);
			glEnable(GL_DEPTH_TEST);
		}
		glPopMatrix();
	}

	R_Color(NULL);
}

/**
 * @sa R_DrawEntities
 * @sa R_DrawBrushModel
 */
static void R_DrawBspEntities (const entity_t *ents)
{
	const entity_t *e;

	if (!ents)
		return;

	e = ents;

	while (e) {
		R_DrawBrushModel(e);
		e = e->next;
	}
}

/**
 * @sa R_DrawEntities
 */
static void R_DrawMeshEntities (const entity_t *ents)
{
	const entity_t *e;

	e = ents;

	while (e) {
		if (e->flags & RF_BOX) {
			R_DrawBox(e);
		} else if (e->flags & RF_PATH) {
			R_DrawFloor(e);
		} else if (e->flags & RF_ARROW) {
			R_DrawArrow(e);
		} else {
			switch (e->model->type) {
			case mod_alias_dpm:
			case mod_alias_md2:
			case mod_alias_md3:
				R_DrawAliasModel(e);
				break;
			default:
				break;
			}
		}
		e = e->next;
	}
}

/**
 * @sa R_DrawEntities
 */
static void R_DrawOpaqueMeshEntities (entity_t *ents)
{
	if (!ents)
		return;

	if (!(refdef.rdflags & RDF_NOWORLDMODEL))
		R_EnableLighting(r_state.default_program, qtrue);
	R_DrawMeshEntities(ents);
	if (!(refdef.rdflags & RDF_NOWORLDMODEL))
		R_EnableLighting(NULL, qfalse);
}

/**
 * @sa R_DrawEntities
 */
static void R_DrawBlendMeshEntities (entity_t *ents)
{
	if (!ents)
		return;

	R_EnableBlend(qtrue);
	R_DrawMeshEntities(ents);
	R_EnableBlend(qfalse);
}

/**
 * @brief Draw replacement model (e.g. when model wasn't found)
 * @sa R_DrawNullEntities
 */
static void R_DrawNullModel (const entity_t *e)
{
	int i;

	glPushMatrix();
	glMultMatrixf(e->transform.matrix);

	glDisable(GL_TEXTURE_2D);

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, -16);
	for (i = 0; i <= 4; i++)
		glVertex3f(16 * cos(i * M_PI / 2), 16 * sin(i * M_PI / 2), 0);
	glEnd();

	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, 16);
	for (i = 4; i >= 0; i--)
		glVertex3f(16 * cos(i * M_PI / 2), 16 * sin(i * M_PI / 2), 0);
	glEnd();

	glPopMatrix();

	glEnable(GL_TEXTURE_2D);
}

/**
 * @brief Draw entities which models couldn't be loaded
 */
static void R_DrawNullEntities (const entity_t *ents)
{
	const entity_t *e;

	if (!ents)
		return;

	e = ents;

	while (e) {
		R_DrawNullModel(e);
		e = e->next;
	}
}

/**
 * @brief Calculates transformation matrix for the model and its tags
 * @note The transformation matrix is only calculated once
 */
static float *R_CalcTransform (entity_t * e)
{
	vec3_t angles;
	transform_t *t;
	float *mp;
	float mt[16], mc[16];
	int i;

	/* check if this entity is already transformed */
	t = &e->transform;

	if (t->processing)
		Sys_Error("Ring in entity transformations!\n");

	if (t->done)
		return t->matrix;

	/* process this matrix */
	t->processing = qtrue;
	mp = NULL;

	/* do parent object transformations first */
	if (e->tagent) {
		/* parent transformation */
		mp = R_CalcTransform(e->tagent);

		/* tag transformation */
		if (e->tagent->model && e->tagent->model->alias.tagdata) {
			const dMD2tag_t *taghdr = (const dMD2tag_t *) e->tagent->model->alias.tagdata;
			/* find the right tag */
			const char *name = (const char *) taghdr + taghdr->ofs_names;
			for (i = 0; i < taghdr->num_tags; i++, name += MD2_MAX_TAGNAME) {
				if (!strcmp(name, e->tagname)) {
					float interpolated[16];
					/* found the tag (matrix) */
					const float *tag = (const float *) ((const byte *) taghdr + taghdr->ofs_tags);
					tag += i * 16 * taghdr->num_frames;

					/* do interpolation */
					R_InterpolateTransform(&e->tagent->as, taghdr->num_frames, tag, interpolated);

					/* transform */
					GLMatrixMultiply(mp, interpolated, mt);
					mp = mt;

					break;
				}
			}
		}
	}

	/* fill in edge values */
	mc[3] = mc[7] = mc[11] = 0.0;
	mc[15] = 1.0;

	/* add rotation */
	VectorCopy(e->angles, angles);
	AngleVectors(angles, &mc[0], &mc[4], &mc[8]);

	/* add translation */
	mc[12] = e->origin[0];
	mc[13] = e->origin[1];
	mc[14] = e->origin[2];

	/* flip an axis */
	VectorScale(&mc[4], -1, &mc[4]);

	/* combine transformations */
	if (mp)
		GLMatrixMultiply(mp, mc, t->matrix);
	else
		memcpy(t->matrix, mc, sizeof(float) * 16);

	/* we're done */
	t->done = qtrue;
	t->processing = qfalse;

	return t->matrix;
}

/**
 * @brief Perform a frustum cull check for a given entity
 * @param[in,out] e The entity to perform the frustum cull check for
 * @returns qfalse if visible, qtrue is the origin of the entity is outside the
 * current frustum view
 */
static qboolean R_CullEntity (entity_t *e)
{
	if (r_nocull->integer)
		return qfalse;

	if (!e->model)  /* don't bother culling null model ents */
		return qfalse;

	if (e->model->type == mod_bsp_submodel)
		return R_CullBspModel(e);
	else
		return R_CullMeshModel(e);
}

/**
 * @brief Draw entities like models and cursor box
 * @sa R_RenderFrame
 */
void R_DrawEntities (void)
{
	int i;
	entity_t **chain;
	entity_t *r_bsp_entities, *r_opaque_mesh_entities;
	entity_t *r_blend_mesh_entities, *r_null_entities;
	image_t *skin;

	if (!r_drawentities->integer)
		return;

	r_bsp_entities = r_opaque_mesh_entities =
		r_blend_mesh_entities = r_null_entities = NULL;

	for (i = 0; i < r_numEntities; i++) {
		entity_t *e = &r_entities[i];

		/* frustum cull check - but not while we are in e.g. sequence mode */
		if (!(refdef.rdflags & RDF_NOWORLDMODEL) && R_CullEntity(e))
			continue;

		R_CalcTransform(e);

		if (!e->model) {
			if (e->flags & RF_BOX || e->flags & RF_PATH || e->flags & RF_ARROW)
				chain = &r_blend_mesh_entities;
			else
				chain = &r_null_entities;
		} else {
			switch (e->model->type) {
			case mod_bsp_submodel:
				chain = &r_bsp_entities;
				break;
			case mod_alias_dpm:
			case mod_alias_md2:
			case mod_alias_md3:
				skin = R_AliasModelState(e->model, &e->as.mesh, &e->as.frame, &e->as.oldframe, &e->skinnum);
				if (skin == NULL) {
					Com_Printf("Model '%s' is broken\n", e->model->name);
					continue;
				}
				if (skin->has_alpha || e->flags & RF_TRANSLUCENT)
					chain = &r_blend_mesh_entities;
				else
					chain = &r_opaque_mesh_entities;
				break;
			default:
				Sys_Error("Unknown model type in R_DrawEntities entity chain: %i", e->model->type);
				break;
			}
		}
		e->next = *chain;
		*chain = e;
	}

	R_DrawBspEntities(r_bsp_entities);
	R_DrawOpaqueMeshEntities(r_opaque_mesh_entities);
	R_DrawBlendMeshEntities(r_blend_mesh_entities);
	R_Color(NULL);
	R_DrawNullEntities(r_null_entities);

	R_EnableBlend(qtrue);
	R_DrawEntityEffects();
	R_EnableBlend(qfalse);
}

/**
 * @brief Get the next free entry in the entity list (the last one)
 * @note This can't overflow, because R_AddEntity checks the bounds
 * @sa R_AddEntity
 */
entity_t *R_GetFreeEntity (void)
{
	assert(r_numEntities < MAX_ENTITIES);
	return &r_entities[r_numEntities];
}

/**
 * @brief Returns a specific entity from the list
 */
entity_t *R_GetEntity (int id)
{
	if (id < 0 || id >= r_numEntities)
		return NULL;
	return &r_entities[id];
}

/**
 * @sa R_GetFreeEntity
 */
void R_AddEntity (entity_t *ent)
{
	if (r_numEntities >= MAX_ENTITIES) {
		Com_Printf("R_AddEntity: MAX_ENTITIES exceeded\n");
		return;
	}

	/* don't add the bsp tiles from random map assemblies */
	if (ent->model && ent->model->type == mod_bsp)
		return;

	r_entities[r_numEntities++] = *ent;
}
