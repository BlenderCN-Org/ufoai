/**
 * @file g_stats.c
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

#include "g_local.h"

/**
 * @brief Send stats to network buffer
 * @sa CL_ActorStats
 */
void G_SendStats (edict_t * ent)
{
	/* extra sanity checks */
	ent->TU = max(ent->TU, 0);
	ent->HP = max(ent->HP, 0);
	ent->STUN = min(ent->STUN, 255);
	ent->morale = max(ent->morale, 0);

	gi.AddEvent(G_TeamToPM(ent->team), EV_ACTOR_STATS);
	gi.WriteShort(ent->number);
	gi.WriteByte(ent->TU);
	gi.WriteShort(ent->HP);
	gi.WriteByte(ent->STUN);
	gi.WriteByte(ent->morale);
}

/**
 * @brief Write player stats to network buffer
 * @sa G_SendStats
 */
void G_SendPlayerStats (const player_t *player)
{
	edict_t *ent;
	int i;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsActor(ent) && ent->team == player->pers.team)
			G_SendStats(ent);
}
