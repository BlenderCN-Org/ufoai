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
 * @todo Use this function to enable footsteps over network
 * @note Only play the water sounds if actor is not in STATE_CROUCHED mode
 * we can assume, that moving slower and more carefully would also not produce
 * the water sounds
 */
void G_PhysicsStep (edict_t *ent)
{
	int contentFlags;
	int visflags;

	if (ent->moveinfo.currentStep < ent->moveinfo.steps) {
		contentFlags = ent->contentFlags;
		visflags = ent->moveinfo.visflags[ent->moveinfo.currentStep];
		/* Send the sound effect to everyone how's not seeing the actor */
		if (ent->state & ~STATE_CROUCHED) {
			if (contentFlags & CONTENTS_WATER) {
				if (ent->moveinfo.contentFlags[ent->moveinfo.currentStep] & CONTENTS_WATER) {
					/* looks like we already are in the water */
					/* send water moving sound */
					gi.PositionedSound(~G_VisToPM(visflags), ent->origin, ent, "footsteps/water_under", CHAN_BODY, 1, 1);
				} else {
					/* send water entering sound */
					gi.PositionedSound(~G_VisToPM(visflags), ent->origin, ent, "footsteps/water_in", CHAN_BODY, 1, 1);
				}
			} else if (ent->contentFlags & CONTENTS_WATER) {
				/* send water leaving sound */
				gi.PositionedSound(~G_VisToPM(visflags), ent->origin, ent, "footsteps/water_out", CHAN_BODY, 1, 1);
			} else {
				trace_t trace;
				vec3_t from, to;

				VectorCopy(ent->origin, to);
				VectorCopy(ent->origin, from);
				/* we should really hit the ground with this */
				to[2] -= UNIT_HEIGHT;

				trace = gi.trace(from, NULL, NULL, to, NULL, MASK_SOLID);
				if (trace.surface) {
					const char *snd = gi.GetFootstepSound(trace.surface->name);
					if (snd)
						gi.PositionedSound(~G_VisToPM(visflags), ent->origin, ent, snd, CHAN_BODY, 1, 1);
				}
			}
		}

		/* and now save the new contents */
		ent->contentFlags = ent->moveinfo.contentFlags[ent->moveinfo.currentStep];
		ent->moveinfo.currentStep++;

		/* immediatly rethink */
		ent->nextthink = (level.framenum + 3) * SERVER_FRAME_SECONDS;
/*		Com_Printf("step: %i/%i\n", ent->moveinfo.currentStep, ent->moveinfo.steps);*/
	} else {
		ent->moveinfo.currentStep = 0;
		ent->moveinfo.steps = 0;
		ent->think = NULL;
	}
}

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

	ent->nextthink = level.time + SERVER_FRAME_SECONDS;
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

#if 0 /* taken out - otherwise footstep sounds are too slow */
	/* don't run this too often to prevent overflows */
	if (level.framenum % 10)
		return;
#endif

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
