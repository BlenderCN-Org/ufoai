/**
 * @file g_phys.c
 * @brief Misc physic functions
 * @note G_PhysicsRun is called every frame to handle physics stuff
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

#include "g_local.h"

/**
 * @brief Runs thinking code for this frame if necessary
 */
static qboolean G_PhysicsThink (edict_t *ent)
{
	float	thinktime;

	thinktime = ent->nextthink;
	if (thinktime <= 0)
		return qtrue;
	if (thinktime > level.time + 0.001f)
		return qtrue;

	ent->nextthink = level.time + FRAMETIME;
	if (!ent->think)
		gi.error("G_PhysicsThink ent->think is NULL");
	ent->think(ent);

	return qfalse;
}

/**
 * @brief Handles door and other objects
 * @sa G_RunFrame
 */
void G_PhysicsRun (void)
{
	int i;
	edict_t *ent;

	/* not all teams are spawned */
	if (level.activeTeam == -1)
		return;

	/* don't run this too often to prevent overflows */
	if (level.framenum % 10)
		return;

	/* treat each object in turn */
	/* even the world gets a chance to think */
	ent = &g_edicts[0];
	for (i = 0; i < globals.num_edicts; i++, ent++) {
		if (!ent->inuse)
			continue;
		if (ent->think)
			G_PhysicsThink(ent);
	}
}
