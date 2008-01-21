/**
 * @file m_node_model.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../client.h"
#include "m_node_model.h"
#include "m_parse.h"

/**
 * @brief Add a menu link to menumodel definition for faster access
 * @note Called after all menus are parsed - only once
 */
void MN_LinkMenuModels (void)
{
	int i, j;
	menuModel_t *m;

	for (i = 0; i < mn.numMenuModels; i++) {
		m = &mn.menuModels[i];
		for (j = 0; j < m->menuTransformCnt; j++) {
			m->menuTransform[j].menuPtr = MN_GetMenu(m->menuTransform[j].menuID);
			if (m->menuTransform[j].menuPtr == NULL)
				Com_Printf("Could not find menu '%s' as requested by menumodel '%s'", m->menuTransform[j].menuID, m->id);
			/* we don't need this anymore */
			Mem_Free(m->menuTransform[j].menuID);
			m->menuTransform[j].menuID = NULL;
		}
	}
}

/**
 * @brief Returns pointer to menu model
 * @param[in] menuModel menu model id from script files
 * @return menuModel_t pointer
 */
menuModel_t *MN_GetMenuModel (const char *menuModel)
{
	int i;
	menuModel_t *m;

	for (i = 0; i < mn.numMenuModels; i++) {
		m = &mn.menuModels[i];
		if (!Q_strncmp(m->id, menuModel, MAX_VAR))
			return m;
	}
	return NULL;
}

void MN_ListMenuModels_f (void)
{
	int i;

	/* search for menumodels with same name */
	Com_Printf("menu models: %i\n", mn.numMenuModels);
	for (i = 0; i < mn.numMenuModels; i++)
		Com_Printf("id: %s\n...model: %s\n...need: %s\n\n", mn.menuModels[i].id, mn.menuModels[i].model, mn.menuModels[i].need);
}


#ifdef DEBUG
/**
 * @brief This function allows you inline transforming of mdoels.
 * @note Changes you made are lost on quit
 */
static void MN_SetModelTransform_f (void)
{
	menuNode_t *node;
	const char *command, *nodeID;
	float x, y ,z;
	vec3_t value;

	/* not initialized yet - commandline? */
	if (mn.menuStackPos <= 0)
		return;

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s <node> <x> <y> <z>\n", Cmd_Argv(0));
		return;
	}

	command = Cmd_Argv(0);
	nodeID = Cmd_Argv(1);
	x = atof(Cmd_Argv(2));
	y = atof(Cmd_Argv(3));
	z = atof(Cmd_Argv(4));

	VectorSet(value, x, y, z);

	/* search the node */
	node = MN_GetNode(MN_GetActiveMenu(), nodeID);
	if (!node) {
		/* didn't find node -> "kill" action and print error */
		Com_Printf("MN_SetModelTransform_f: node \"%s\" doesn't exist\n", nodeID);
		return;
	} else if (node->type != MN_MODEL) {
		Com_Printf("MN_SetModelTransform_f: node \"%s\" isn't a model node\n", nodeID);
		return;
	}

	if (!Q_strcmp(command, "debug_mnscale")) {
		VectorCopy(value, node->scale);
	} else if (!Q_strcmp(command, "debug_mnangles")) {
		VectorCopy(value, node->angles);
	}
}
#endif

void MN_NodeModelInit (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_mnscale", MN_SetModelTransform_f, "Transform model from command line.");
	Cmd_AddCommand("debug_mnangles", MN_SetModelTransform_f, "Transform model from command line.");
#endif
	Cmd_AddCommand("menumodelslist", MN_ListMenuModels_f, NULL);
}

void MN_DrawModelNode (menuNode_t *node, const char *ref, const char *source)
{
	int i;
	modelInfo_t mi;
	modelInfo_t pmi;
	animState_t *as;
	char *anim;					/* model anim state */
	menuModel_t *menuModel = NULL, *menuModelParent = NULL;

	/* if true we have to reset the anim state and make sure the correct model is shown */
	qboolean updateModel = qfalse;

	/* set model properties */
	if (!*source)
		return;
	node->menuModel = MN_GetMenuModel(source);

	/* direct model name - no menumodel definition */
	if (!node->menuModel) {
		/* prevent the searching for a menumodel def in the next frame */
		menuModel = NULL;
		mi.model = R_RegisterModelShort(source);
		mi.name = source;
		if (!mi.model) {
			Com_Printf("Could not find model '%s'\n", source);
			return;
		}
	} else
		menuModel = node->menuModel;

	/* check whether the cvar value changed */
	if (Q_strncmp(node->oldRefValue, source, sizeof(node->oldRefValue))) {
		Q_strncpyz(node->oldRefValue, source, sizeof(node->oldRefValue));
		updateModel = qtrue;
	}

	mi.origin = node->origin;
	mi.angles = node->angles;
	mi.scale = node->scale;
	mi.center = node->center;
	mi.color = node->color;

	/* autoscale? */
	if (!node->scale[0]) {
		mi.scale = NULL;
		mi.center = node->size;
	}

	do {
		/* no animation */
		mi.frame = 0;
		mi.oldframe = 0;
		mi.backlerp = 0;
		if (menuModel) {
			assert(menuModel->model);
			mi.model = R_RegisterModelShort(menuModel->model);
			if (!mi.model) {
				menuModel = menuModel->next;
				/* list end */
				if (!menuModel)
					break;
				continue;
			}

			mi.skin = menuModel->skin;
			mi.name = menuModel->model;

			/* set mi pointers to menuModel */
			mi.origin = menuModel->origin;
			mi.angles = menuModel->angles;
			mi.center = menuModel->center;
			mi.color = menuModel->color;
			mi.scale = menuModel->scale;

			/* no tag and no parent means - base model or single model */
			if (!menuModel->tag && !menuModel->parent) {
				if (menuModel->menuTransformCnt) {
					for (i = 0; i < menuModel->menuTransformCnt; i++) {
						if (node->menu == menuModel->menuTransform[i].menuPtr) {
							/* Use menu scale if defined. */
							if (menuModel->menuTransform[i].useScale) {
								VectorCopy(menuModel->menuTransform[i].scale, mi.scale);
							} else {
								VectorCopy(node->scale, mi.scale);
							}

							/* Use menu angles if defined. */
							if (menuModel->menuTransform[i].useAngles) {
								VectorCopy(menuModel->menuTransform[i].angles, mi.angles);
							} else {
								VectorCopy(node->angles, mi.angles);
							}

							/* Use menu origin if defined. */
							if (menuModel->menuTransform[i].useOrigin) {
								VectorAdd(node->origin, menuModel->menuTransform[i].origin, mi.origin);
							} else {
								VectorCopy(node->origin, mi.origin);
							}
							break;
						}
					}
					/* not for this menu */
					if (i == menuModel->menuTransformCnt) {
						VectorCopy(node->scale, mi.scale);
						VectorCopy(node->angles, mi.angles);
						VectorCopy(node->origin, mi.origin);
					}
				} else {
					VectorCopy(node->scale, mi.scale);
					VectorCopy(node->angles, mi.angles);
					VectorCopy(node->origin, mi.origin);
				}
				Vector4Copy(node->color, mi.color);
				VectorCopy(node->center, mi.center);

				/* get the animation given by menu node properties */
				if (node->data[MN_DATA_ANIM_OR_FONT] && *(char *) node->data[MN_DATA_ANIM_OR_FONT]) {
					ref = MN_GetReferenceString(node->menu, node->data[MN_DATA_ANIM_OR_FONT]);
				/* otherwise use the standard animation from modelmenu defintion */
				} else
					ref = menuModel->anim;

				/* only base models have animations */
				if (ref && *ref) {
					as = &menuModel->animState;
					anim = R_AnimGetName(as, mi.model);
					/* initial animation or animation change */
					if (!anim || (anim && Q_strncmp(anim, ref, MAX_VAR)))
						R_AnimChange(as, mi.model, ref);
					else
						R_AnimRun(as, mi.model, cls.frametime * 1000);

					mi.frame = as->frame;
					mi.oldframe = as->oldframe;
					mi.backlerp = as->backlerp;
				}
				R_DrawModelDirect(&mi, NULL, NULL);
			/* tag and parent defined */
			} else {
				/* place this menumodel part on an already existing menumodel tag */
				assert(menuModel->parent);
				assert(menuModel->tag);
				menuModelParent = MN_GetMenuModel(menuModel->parent);
				if (!menuModelParent) {
					Com_Printf("Menumodel: Could not get the menuModel '%s'\n", menuModel->parent);
					break;
				}
				pmi.model = R_RegisterModelShort(menuModelParent->model);
				if (!pmi.model) {
					Com_Printf("Menumodel: Could not get the model '%s'\n", menuModelParent->model);
					break;
				}

				pmi.name = menuModelParent->model;
				pmi.origin = menuModelParent->origin;
				pmi.angles = menuModelParent->angles;
				pmi.scale = menuModelParent->scale;
				pmi.center = menuModelParent->center;
				pmi.color = menuModelParent->color;

				/* autoscale? */
				if (!mi.scale[0]) {
					mi.scale = NULL;
					mi.center = node->size;
				}

				as = &menuModelParent->animState;
				pmi.frame = as->frame;
				pmi.oldframe = as->oldframe;
				pmi.backlerp = as->backlerp;

				R_DrawModelDirect(&mi, &pmi, menuModel->tag);
			}
			menuModel = menuModel->next;
		} else {
			/* get skin */
			if (node->data[MN_DATA_MODEL_SKIN_OR_CVAR] && *(char *) node->data[MN_DATA_MODEL_SKIN_OR_CVAR])
				mi.skin = atoi(MN_GetReferenceString(node->menu, node->data[MN_DATA_MODEL_SKIN_OR_CVAR]));
			else
				mi.skin = 0;

			/* do animations */
			if (node->data[MN_DATA_ANIM_OR_FONT] && *(char *) node->data[MN_DATA_ANIM_OR_FONT]) {
				ref = MN_GetReferenceString(node->menu, node->data[MN_DATA_ANIM_OR_FONT]);
				if (updateModel) {
					/* model has changed but mem is already reserved in pool */
					if (node->data[MN_DATA_MODEL_ANIMATION_STATE]) {
						Mem_Free(node->data[MN_DATA_MODEL_ANIMATION_STATE]);
						node->data[MN_DATA_MODEL_ANIMATION_STATE] = NULL;
					}
				}
				if (!node->data[MN_DATA_MODEL_ANIMATION_STATE]) {
					as = (animState_t *) Mem_PoolAlloc(sizeof(animState_t), cl_genericPool, CL_TAG_NONE);
					R_AnimChange(as, mi.model, ref);
					node->data[MN_DATA_MODEL_ANIMATION_STATE] = as;
				} else {
					/* change anim if needed */
					as = node->data[MN_DATA_MODEL_ANIMATION_STATE];
					anim = R_AnimGetName(as, mi.model);
					if (anim && Q_strncmp(anim, ref, MAX_VAR))
						R_AnimChange(as, mi.model, ref);
					R_AnimRun(as, mi.model, cls.frametime * 1000);
				}

				mi.frame = as->frame;
				mi.oldframe = as->oldframe;
				mi.backlerp = as->backlerp;
			}

			/* place on tag */
			if (node->data[MN_DATA_MODEL_TAG]) {
				menuNode_t *search;
				char parent[MAX_VAR];
				char *tag;

				Q_strncpyz(parent, MN_GetReferenceString(node->menu, node->data[MN_DATA_MODEL_TAG]), MAX_VAR);
				tag = parent;
				/* tag "menuNodeName modelTag" */
				while (*tag && *tag != ' ')
					tag++;
				/* split node name and tag */
				*tag++ = 0;

				for (search = node->menu->firstNode; search != node && search; search = search->next)
					if (search->type == MN_MODEL && !Q_strncmp(search->name, parent, MAX_VAR)) {
						char model[MAX_VAR];

						Q_strncpyz(model, MN_GetReferenceString(node->menu, search->data[MN_DATA_STRING_OR_IMAGE_OR_MODEL]), MAX_VAR);

						pmi.model = R_RegisterModelShort(model);
						if (!pmi.model)
							break;

						pmi.name = model;
						pmi.origin = search->origin;
						pmi.angles = search->angles;
						pmi.scale = search->scale;
						pmi.center = search->center;
						pmi.color = search->color;

						/* autoscale? */
						if (!node->scale[0]) {
							mi.scale = NULL;
							mi.center = node->size;
						}

						as = search->data[MN_DATA_MODEL_ANIMATION_STATE];
						pmi.frame = as->frame;
						pmi.oldframe = as->oldframe;
						pmi.backlerp = as->backlerp;
						R_DrawModelDirect(&mi, &pmi, tag);
						break;
					}
			} else
				R_DrawModelDirect(&mi, NULL, NULL);
		}
	/* for normal models (normal = no menumodel definition) this
	 * menuModel pointer is null - the do-while loop is only
	 * ran once */
	} while (menuModel != NULL);
}
