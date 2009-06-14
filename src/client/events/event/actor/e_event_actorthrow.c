/**
 * @file e_event_actorthrow.c
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
#include "e_event_actorthrow.h"

static qboolean firstShot;

int CL_ActorDoThrowTime (const eventRegister_t *self, struct dbuffer *msg, const int dt)
{
	const int t = NET_ReadShort(msg);
	return cl.time + t;
}

/**
 * @brief Throw item with actor.
 * @param[in] msg The netchannel message
 */
void CL_ActorDoThrow (const eventRegister_t *self, struct dbuffer *msg)
{
	const fireDef_t *fd;
	vec3_t muzzle, v0;
	int flags;
	int dtime;
	int objIdx;
	const objDef_t *obj;
	int weapFdsIdx, fdIdx;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &dtime, &objIdx, &weapFdsIdx, &fdIdx, &flags, &muzzle, &v0);

	/* get the fire def */
	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* add effect le (local entity) */
	LE_AddGrenade(fd, flags, muzzle, v0, dtime);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0] && !(flags & SF_BOUNCED)) {
		s_sample_t *sample = S_LoadSample(fd->fireSound);
		S_PlaySample(muzzle, sample, SOUND_ATTN_IDLE, 1.0f);
	}

	firstShot = qfalse;
}
