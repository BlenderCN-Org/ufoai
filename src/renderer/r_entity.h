/**
 * @file r_entity.h
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

#ifndef R_ENTITY_H
#define R_ENTITY_H

/** @brief entity transform */
typedef struct {
	qboolean done;
	qboolean processing;
	float matrix[16];
} transform_t;

typedef struct entity_s {
	struct model_s *model;		/**< opaque type outside refresh */
	vec3_t angles;

	vec3_t origin;
	vec3_t oldorigin;

	vec3_t mins, maxs;

	/* tag positioning */
	struct entity_s *tagent;	/**< pointer to the parent entity */
	const char *tagname;				/**< name of the tag */

	/* misc */
	int skinnum;

	float alpha;				/**< ignore if RF_TRANSLUCENT isn't set */

	int flags;

	animState_t as;

	transform_t transform;

	struct entity_s *next;		/**< for chaining */
} entity_t;

void R_AddEntity(entity_t * ent);
entity_t *R_GetFreeEntity(void);
entity_t *R_GetEntity(int id);
void R_EntityComputeBoundingBox(const vec3_t mins, const vec3_t maxs, vec4_t bbox[8]);
void R_EntityDrawlBBox(vec4_t bbox[8]);

extern int r_numEntities;

#endif
