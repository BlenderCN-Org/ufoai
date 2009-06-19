/**
 * @file e_event_entperish.c
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

#include "../../../client.h"
#include "../../../cl_le.h"
#include "../../../cl_particle.h"
#include "e_event_entperish.h"

/**
 * @brief Called whenever an entity disappears from view
 * @sa CL_EntAppear
 */
void CL_EntPerish (const eventRegister_t *self, struct dbuffer *msg)
{
	int		entnum, i;
	le_t	*le, *actor;

	NET_ReadFormat(msg, self->formatString, &entnum);

	le = LE_Get(entnum);

	if (!le)
		LE_NotFoundError(entnum);

	/* decrease the count of spotted aliens */
	if (LE_IsLivingAndVisibleActor(le) && le->team != cls.team && le->team != TEAM_CIVILIAN)
		cl.numAliensSpotted--;

	switch (le->type) {
	case ET_ITEM:
		INVSH_EmptyContainer(&le->i, &csi.ids[csi.idFloor]);

		/* search owners (there can be many, some of them dead) */
		for (i = 0, actor = LEs; i < cl.numLEs; i++, actor++)
			if (actor->inuse && (actor->type == ET_ACTOR || actor->type == ET_ACTOR2x2)
			 && VectorCompare(actor->pos, le->pos)) {
				Com_DPrintf(DEBUG_CLIENT, "CL_EntPerish: le of type ET_ITEM hidden\n");
				FLOOR(actor) = NULL;
			}
		break;
	case ET_ACTOR:
	case ET_ACTOR2x2:
		INVSH_DestroyInventory(&le->i);
		break;
#ifdef DEBUG
	case ET_ACTORHIDDEN:
		Com_DPrintf(DEBUG_CLIENT, "CL_EntPerish: It should not happen that we perish an hidden actor\n");
		return;
#endif
	case ET_PARTICLE:
		CL_ParticleFree(le->ptl);
		le->ptl = NULL;
		break;
	case ET_BREAKABLE:
	case ET_DOOR:
		break;
	default:
		break;
	}

	le->invis = qtrue;
}

