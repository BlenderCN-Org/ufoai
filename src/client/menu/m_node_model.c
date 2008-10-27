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
#include "m_nodes.h"
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
 * @brief This function allows you inline transforming of models.
 * @note Changes you made are lost on quit
 */
static void MN_SetModelTransform_f (void)
{
	menuNode_t *node;
	menuModel_t *model;
	const char *command, *nodeOrMenuID, *menuModel;
	float x, y ,z;
	vec3_t value;

	/* not initialized yet - commandline? */
	if (mn.menuStackPos <= 0)
		return;

	if (Cmd_Argc() < 5) {
		Com_Printf("Usage: %s [<model> <menu>] | [<node>] <x> <y> <z>\n", Cmd_Argv(0));
		Com_Printf("<model> <menu> is needed for menumodel definitions\n");
		Com_Printf("<node> is needed for 'normal' models\n");
		return;
	}

	if (Cmd_Argc() == 5) {
		command = Cmd_Argv(0);
		menuModel = NULL;
		nodeOrMenuID = Cmd_Argv(1);
		x = atof(Cmd_Argv(2));
		y = atof(Cmd_Argv(3));
		z = atof(Cmd_Argv(4));
	} else {
		command = Cmd_Argv(0);
		menuModel = Cmd_Argv(1);
		nodeOrMenuID = Cmd_Argv(2);
		x = atof(Cmd_Argv(3));
		y = atof(Cmd_Argv(4));
		z = atof(Cmd_Argv(5));
	}

	VectorSet(value, x, y, z);

	if (menuModel) {
		model = MN_GetMenuModel(menuModel);
		if (!model) {
			Com_Printf("MN_SetModelTransform_f: model \"%s\" wasn't found\n", menuModel);
			return;
		}

		if (model->menuTransformCnt) {
			const menu_t *menu = MN_GetActiveMenu();
			int i;

			for (i = 0; i < model->menuTransformCnt; i++) {
				if (menu == model->menuTransform[i].menuPtr) {
					if (!Q_strcmp(command, "debug_mnscale")) {
						VectorCopy(value, model->menuTransform[i].scale);
					} else if (!Q_strcmp(command, "debug_mnangles")) {
						VectorCopy(value, model->menuTransform[i].angles);
					} else if (!Q_strcmp(command, "debug_mnorigin")) {
						VectorCopy(value, model->menuTransform[i].origin);
					}
					break;
				}
			}
		} else {
			Com_Printf("MN_SetModelTransform_f: no entry in menumodel '%s' for menu '%s'\n", menuModel, nodeOrMenuID);
			return;
		}
	} else {
		/* search the node */
		node = MN_GetNode(MN_GetActiveMenu(), nodeOrMenuID);
		if (!node) {
			/* didn't find node -> "kill" action and print error */
			Com_Printf("MN_SetModelTransform_f: node \"%s\" doesn't exist\n", nodeOrMenuID);
			return;
		} else if (node->type != MN_MODEL) {
			Com_Printf("MN_SetModelTransform_f: node \"%s\" isn't a model node\n", nodeOrMenuID);
			return;
		}

		if (!Q_strcmp(command, "debug_mnscale")) {
			VectorCopy(value, node->scale);
		} else if (!Q_strcmp(command, "debug_mnangles")) {
			VectorCopy(value, node->u.model.angles);
		} else if (!Q_strcmp(command, "debug_mnorigin")) {
			VectorCopy(value, node->u.model.origin);
		}
	}
}
#endif

void MN_NodeModelInit (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_mnscale", MN_SetModelTransform_f, "Transform model from command line.");
	Cmd_AddCommand("debug_mnangles", MN_SetModelTransform_f, "Transform model from command line.");
	Cmd_AddCommand("debug_mnorigin", MN_SetModelTransform_f, "Transform model from command line.");
#endif
	Cmd_AddCommand("menumodelslist", MN_ListMenuModels_f, NULL);
}

static void MN_DrawModelNode2 (menuNode_t *node)
{
	menu_t *menu = node->menu;
	const char* ref = MN_GetReferenceString(node->menu, node->dataImageOrModel);
	char source[MAX_VAR];
	if (ref == NULL || ref[0] == '\0')
		source[0] = '\0';
	else
		Q_strncpyz(source, ref, sizeof(source));
	MN_DrawModelNode(menu, node, ref, source);
}

/**
 * @todo Menu models should inherit the node values from their parent
 */
void MN_DrawModelNode (const menu_t* menu, menuNode_t *node, const char *ref, const char *source)
{
	int i;
	modelInfo_t mi;
	modelInfo_t pmi;
	animState_t *as;
	const char *anim;					/* model anim state */
	menuModel_t *menuModel, *menuModelParent = NULL;
	/* if true we have to reset the anim state and make sure the correct model is shown */
	qboolean updateModel = qfalse;
	vec2_t nodepos;
	static vec3_t nodeorigin;
	static vec3_t pmiorigin;

	if (source[0] == '\0')
		return;

	MN_GetNodeAbsPos(node, nodepos);
	nodeorigin[0] = node->u.model.origin[0] - node->pos[0] + nodepos[0];
	nodeorigin[1] = node->u.model.origin[1] - node->pos[1] + nodepos[1];
	nodeorigin[2] = node->u.model.origin[2];

	menuModel = node->u.model.menuModel = MN_GetMenuModel(source);

	/* direct model name - no menumodel definition */
	if (!menuModel) {
		/* prevent the searching for a menumodel def in the next frame */
		menuModel = NULL;
		mi.model = R_RegisterModelShort(source);
		mi.name = source;
		if (!mi.model) {
			Com_Printf("Could not find model '%s'\n", source);
			return;
		}
	}

	/* check whether the cvar value changed */
	if (Q_strncmp(node->u.model.oldRefValue, source, sizeof(node->u.model.oldRefValue))) {
		Q_strncpyz(node->u.model.oldRefValue, source, sizeof(node->u.model.oldRefValue));
		updateModel = qtrue;
	}

	mi.origin = nodeorigin;
	mi.angles = node->u.model.angles;
	mi.scale = node->scale;
	mi.center = node->u.model.center;
	mi.color = node->color;
	mi.mesh = 0;

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
						if (menu == menuModel->menuTransform[i].menuPtr) {
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
								VectorCopy(node->u.model.angles, mi.angles);
							}

							/* Use menu origin if defined. */
							if (menuModel->menuTransform[i].useOrigin) {
								VectorAdd(nodeorigin, menuModel->menuTransform[i].origin, mi.origin);
							} else {
								VectorCopy(nodeorigin, mi.origin);
							}
							break;
						}
					}
					/* not for this menu */
					if (i == menuModel->menuTransformCnt) {
						VectorCopy(node->scale, mi.scale);
						VectorCopy(node->u.model.angles, mi.angles);
						VectorCopy(nodeorigin, mi.origin);
					}
				} else {
					VectorCopy(node->scale, mi.scale);
					VectorCopy(node->u.model.angles, mi.angles);
					VectorCopy(nodeorigin, mi.origin);
				}
				Vector4Copy(node->color, mi.color);
				VectorCopy(node->u.model.center, mi.center);

				/* get the animation given by menu node properties */
				if (node->dataAnimOrFont && *(char *) node->dataAnimOrFont) {
					ref = MN_GetReferenceString(menu, node->dataAnimOrFont);
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
				pmi.origin = pmiorigin;
				pmi.origin[0] = menuModelParent->origin[0] + mi.origin[0];
				pmi.origin[1] = menuModelParent->origin[1] + mi.origin[1];
				pmi.origin[2] = menuModelParent->origin[2];
				/* don't count menuoffset twice for tagged models */
				mi.origin[0] -= node->menu->pos[0];
				mi.origin[1] -= node->menu->pos[1];
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
				if (!as)
					Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", pmi.name, ref);
				pmi.frame = as->frame;
				pmi.oldframe = as->oldframe;
				pmi.backlerp = as->backlerp;

				R_DrawModelDirect(&mi, &pmi, menuModel->tag);
			}
			menuModel = menuModel->next;
		} else {
			/* get skin */
			if (node->dataModelSkinOrCVar && *(char *) node->dataModelSkinOrCVar)
				mi.skin = atoi(MN_GetReferenceString(menu, node->dataModelSkinOrCVar));
			else
				mi.skin = 0;

			/* do animations */
			if (node->dataAnimOrFont && *(char *) node->dataAnimOrFont) {
				ref = MN_GetReferenceString(menu, node->dataAnimOrFont);
				if (updateModel) {
					/* model has changed but mem is already reserved in pool */
					if (node->u.model.animationState) {
						Mem_Free(node->u.model.animationState);
						node->u.model.animationState = NULL;
					}
				}
				if (!node->u.model.animationState) {
					as = (animState_t *) Mem_PoolAlloc(sizeof(*as), cl_genericPool, CL_TAG_NONE);
					if (!as)
						Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", mi.name, ref);
					R_AnimChange(as, mi.model, ref);
					node->u.model.animationState = as;
				} else {
					/* change anim if needed */
					as = node->u.model.animationState;
					if (!as)
						Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", mi.name, ref);
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
			if (node->u.model.tag) {
				menuNode_t *search;
				char parent[MAX_VAR];
				char *tag;

				Q_strncpyz(parent, MN_GetReferenceString(menu, node->u.model.tag), MAX_VAR);
				tag = parent;
				/* tag "menuNodeName modelTag" */
				while (*tag && *tag != ' ')
					tag++;
				/* split node name and tag */
				*tag++ = 0;

				for (search = menu->firstNode; search != node && search; search = search->next)
					if (search->type == MN_MODEL && !Q_strncmp(search->name, parent, sizeof(search->name))) {
						char modelName[MAX_VAR];
						Q_strncpyz(modelName, MN_GetReferenceString(menu, search->dataImageOrModel), sizeof(modelName));

						pmi.model = R_RegisterModelShort(modelName);
						if (!pmi.model)
							break;

						pmi.name = modelName;
						VectorCopy(search->u.model.origin, pmiorigin);
						pmi.origin = pmiorigin;
						pmi.origin[0] += mi.origin[0];
						pmi.origin[1] += mi.origin[1];
						/* don't count menuoffset twice for tagged models */
						mi.origin[0] -= node->menu->pos[0];
						mi.origin[1] -= node->menu->pos[1];
						pmi.angles = search->u.model.angles;
						pmi.scale = search->scale;
						pmi.center = search->u.model.center;
						pmi.color = search->color;

						/* autoscale? */
						if (!node->scale[0]) {
							mi.scale = NULL;
							mi.center = node->size;
						}

						as = search->u.model.animationState;
						if (!as)
							Com_Error(ERR_DROP, "Model %s should have animState_t for animation %s - but doesn't\n", modelName, (char*)search->dataAnimOrFont);
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

/**
 * @brief Activates the model rotation
 * @note set the mouse space to MS_ROTATE
 * @sa rotateAngles
 */
static void MN_ModelClick (menuNode_t * node, int x, int y)
{
	mouseSpace = MS_ROTATE;
	/* modify node->model.angles (vec3_t) if you rotate the model */
	rotateAngles = node->u.model.angles;
}

void MN_RegisterNodeModel (nodeBehaviour_t *behaviour)
{
	behaviour->name = "model";
	behaviour->draw = MN_DrawModelNode2;
	behaviour->leftClick = MN_ModelClick;
}
