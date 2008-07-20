/**
 * @file r_model.h
 * @brief Brush model header file
 * @note d*_t structures are on-disk representations
 * @note m*_t structures are in-memory
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

#ifndef R_MODEL_H
#define R_MODEL_H

#include "r_entity.h"
#include "r_model_alias.h"
#include "r_model_brush.h"
#include "r_model_dpm.h"
#include "r_model_md2.h"
#include "r_model_md3.h"

/**
 * @brief All supported model formats
 * @sa mod_extensions
 */
typedef enum {mod_bad, mod_bsp, mod_alias_md2, mod_alias_md3, mod_alias_dpm} modtype_t;

typedef struct model_s {
	/** the name needs to be the first entry in the struct */
	char name[MAX_QPATH];

	modtype_t type;	/**< model type */

	int flags;

	/** volume occupied by the model graphics */
	vec3_t mins, maxs;
	float radius;

	/** solid volume for clipping */
	qboolean clipbox;
	vec3_t clipmins, clipmaxs;

	mBspModel_t bsp;

	/** for alias models and skins */
	mAliasModel_t alias;
} model_t;

/*============================================================================ */

#define MAX_MOD_KNOWN   512

void R_ModClearAll(void);
void R_ModModellist_f(void);
void R_ModDrawNullModel(entity_t *e);
image_t* R_AliasModelState(const model_t *mod, int *mesh, int *frame, int *oldFrame, int *skin);
image_t* R_AliasModelGetSkin(const model_t* mod, const char *skin);
void R_DrawAliasFrameLerp(const mAliasModel_t* mod, const mAliasMesh_t *mesh, float backlerp, int framenum, int oldframenum);
void R_DrawAliasModel(const entity_t *e);
void R_ShutdownModels(void);

extern model_t *r_mapTiles[MAX_MAPTILES];
extern int r_numMapTiles;

extern model_t r_models[MAX_MOD_KNOWN];
extern int r_numModels;

extern model_t r_modelsInline[MAX_MOD_KNOWN];
extern int r_numModelsInline;

#endif
