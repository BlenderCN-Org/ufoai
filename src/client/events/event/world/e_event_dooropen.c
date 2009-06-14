/**
 * @file e_event_dooropen.c
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
#include "e_event_dooropen.h"

/**
 * @brief Callback for EV_DOOR_OPEN event - rotates the inline model and recalc routing
 * @sa Touch_DoorTrigger
 */
void CL_DoorOpen (const eventRegister_t *self, struct dbuffer *msg)
{
	/* get local entity */
	const int entnum = NET_ReadShort(msg);
	le_t *le = LE_Get(entnum);
	if (!le)
		LE_NotFoundError(entnum);

	/** @todo YAW should be the orientation of the door */
	le->angles[YAW] += DOOR_ROTATION_ANGLE;

	Com_DPrintf(DEBUG_CLIENT, "Client processed door movement.\n");

	CM_SetInlineModelOrientation(le->inlineModelName, le->origin, le->angles);
	CL_RecalcRouting(le);
}
