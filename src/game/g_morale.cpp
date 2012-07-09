/**
 * @file
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
 * @sa G_MoraleStopPanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoralePanic (edict_t * ent, bool sanity)
{
	G_ClientPrintf(G_PLAYER_FROM_ENT(ent), PRINT_HUD, _("%s panics!"), ent->chr.name);
	G_PrintStats("%s panics (entnum %i).", ent->chr.name, ent->number);
	/* drop items in hands */
	if (!sanity && ent->chr.teamDef->weapons) {
		if (RIGHT(ent))
			G_ActorInvMove(ent, INVDEF(gi.csi->idRight), RIGHT(ent),
					INVDEF(gi.csi->idFloor), NONE, NONE, true);
		if (LEFT(ent))
			G_ActorInvMove(ent, INVDEF(gi.csi->idLeft), LEFT(ent),
					INVDEF(gi.csi->idFloor), NONE, NONE, true);
	}

	/* get up */
	G_RemoveCrouched(ent);
	G_ActorSetMaxs(ent);

	/* send panic */
	G_SetPanic(ent);
	G_EventSendState(G_VisToPM(ent->visflags), ent);

	/* center view */
	G_EventCenterView(ent);

	/* move around a bit, try to avoid opponents */
	AI_ActorThink(G_PLAYER_FROM_ENT(ent), ent);

	/* kill TUs */
	G_ActorSetTU(ent, 0);
}

/**
 * @brief Stops the panic state of an actor
 * @note This is only called when cvar mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoraleStopPanic (edict_t * ent)
{
	if (ent->morale / mor_panic->value > m_panic_stop->value * frand()) {
		G_RemovePanic(ent);
		G_PrintStats("%s is no longer paniced (entnum %i).", ent->chr.name, ent->number);
	} else {
		G_MoralePanic(ent, true);
	}
}

/**
 * @sa G_MoralePanic
 * @sa G_MoraleStopPanic
 * @sa G_MoraleStopRage
 * @sa G_MoraleBehaviour
 */
static void G_MoraleRage (edict_t * ent, bool sanity)
{
	if (sanity) {
		G_SetRage(ent);
		gi.BroadcastPrintf(PRINT_HUD, _("%s is on a rampage!"), ent->chr.name);
		G_PrintStats("%s is on a rampage (entnum %i).", ent->chr.name, ent->number);
	} else {
		G_SetInsane(ent);
		gi.BroadcastPrintf(PRINT_HUD, _("%s is consumed by mad rage!"), ent->chr.name);
		G_PrintStats("%s is consumed by mad rage (entnum %i).", ent->chr.name, ent->number);
	}
	G_EventSendState(G_VisToPM(ent->visflags), ent);

	AI_ActorThink(G_PLAYER_FROM_ENT(ent), ent);
}

/**
 * @brief Stops the rage state of an actor
 * @note This is only called when cvar mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopPanic
 * @sa G_MoraleBehaviour
 */
static void G_MoraleStopRage (edict_t * ent)
{
	if (ent->morale / mor_panic->value > m_rage_stop->value * frand()) {
		G_RemoveInsane(ent);
		G_EventSendState(G_VisToPM(ent->visflags), ent);
		G_PrintStats("%s is no longer insane (entnum %i).", ent->chr.name, ent->number);
	} else {
		G_MoralePanic(ent, true); /* regains sanity */
	}
}

/**
 * @brief Checks whether the morale handling is activated for this game. This is always
 * the case in singleplayer matches, and might be disabled for multiplayer matches.
 * @return @c true if the morale is activated for this game.
 */
static bool G_IsMoraleEnabled (int team)
{
	if (sv_maxclients->integer == 1)
		return true;

	/* multiplayer */
	if (team == TEAM_CIVILIAN || sv_enablemorale->integer == 1)
		return true;

	return false;
}

/**
 * @brief Applies morale behaviour on actors
 * @note only called when mor_panic is not zero
 * @sa G_MoralePanic
 * @sa G_MoraleRage
 * @sa G_MoraleStopRage
 * @sa G_MoraleStopPanic
 */
void G_MoraleBehaviour (int team)
{
	bool enabled = G_IsMoraleEnabled(team);
	if (!enabled)
		return;

	edict_t *ent = NULL;
	while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, team)) != NULL) {
		/* this only applies to ET_ACTOR but not to ET_ACTOR2x2 */
		if (ent->type != ET_ACTOR)
			continue;

		/* if panic, determine what kind of panic happens: */
		if (!G_IsPaniced(ent) && !G_IsRaged(ent)) {
			if (ent->morale <= mor_panic->integer) {
				const float ratio = (float) ent->morale / mor_panic->value;
				const bool sanity = ratio > (m_sanity->value * frand());
				if (ratio > (m_rage->value * frand()))
					G_MoralePanic(ent, sanity);
				else
					G_MoraleRage(ent, sanity);
				/* if shaken, well .. be shaken; */
			} else if (ent->morale <= mor_shaken->integer) {
				/* shaken is later reset along with reaction fire */
				G_SetShaken(ent);
				G_ClientStateChange(G_PLAYER_FROM_ENT(ent), ent, STATE_REACTION, false);
				G_EventSendState(G_VisToPM(ent->visflags), ent);
				G_ClientPrintf(G_PLAYER_FROM_ENT(ent), PRINT_HUD, _("%s is currently shaken."),
						ent->chr.name);
				G_PrintStats("%s is shaken (entnum %i).", ent->chr.name, ent->number);
			}
		} else {
			if (G_IsPaniced(ent))
				G_MoraleStopPanic(ent);
			else if (G_IsRaged(ent))
				G_MoraleStopRage(ent);
		}

		G_ActorSetMaxs(ent);

		/* morale-regeneration, capped at max: */
		int newMorale = ent->morale + MORALE_RANDOM(mor_regeneration->value);
		const int maxMorale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);
		if (newMorale > maxMorale)
			ent->morale = maxMorale;
		else
			ent->morale = newMorale;

		/* send phys data and state: */
		G_SendStats(ent);
	}
}
