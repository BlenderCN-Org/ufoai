/**
 * @file g_combat.c
 * @brief All parts of the main game logic that are combat related
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#define MAX_WALL_THICKNESS_FOR_SHOOTING_THROUGH 8

typedef enum {
	ML_WOUND,
	ML_DEATH
} morale_modifiers;

/**
 * @brief test if point is "visible" from team
 * @param[in] team
 * @param[in] point
 */
static qboolean G_TeamPointVis (int team, vec3_t point)
{
	edict_t *from;
	vec3_t eye;
	int i;

	/* test if point is visible from team */
	for (i = 0, from = g_edicts; i < globals.num_edicts; i++, from++)
		if (from->inuse && (from->type == ET_ACTOR || from->type == ET_ACTOR2x2) && !(from->state & STATE_DEAD) && from->team == team && G_FrustumVis(from, point)) {
			/* get viewers eye height */
			VectorCopy(from->origin, eye);
			if (from->state & (STATE_CROUCHED | STATE_PANIC))
				eye[2] += EYE_CROUCH;
			else
				eye[2] += EYE_STAND;

			/* line of sight */
			if (!gi.TestLine(eye, point, TL_FLAG_NONE))
				return qtrue;
		}

	/* not visible */
	return qfalse;
}

/**
 * @brief Applies morale changes to actors around a wounded or killed actor
 * @note only called when mor_panic is not zero
 * @param[in] type
 * @param[in] victim
 * @param[in] attacker
 * @param[in] param
 */
static void G_Morale (int type, edict_t * victim, edict_t * attacker, int param)
{
	edict_t *ent;
	int i, newMorale;
	float mod;

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		/* this only applies to ET_ACTOR but not ET_ACTOR2x2 */
		if (ent->inuse && ent->type == ET_ACTOR && !(ent->state & STATE_DEAD) && ent->team != TEAM_CIVILIAN) {
			switch (type) {
			case ML_WOUND:
			case ML_DEATH:
				/* morale damage is damage dependant */
				mod = mob_wound->value * param;
				/* death hurts morale even more than just damage */
				if (type == ML_DEATH)
					mod += mob_death->value;
				/* seeing how someone gets shot increases the morale change */
				if (ent == victim || (G_ActorVis(ent->origin, victim, qfalse) && G_FrustumVis(ent, victim->origin)))
					mod *= mof_watching->value;
				if (ent->team == attacker->team) {
					/* teamkills are considered to be bad form, but won't cause an increased morale boost for the enemy */
					/* morale boost isn't equal to morale loss (it's lower, but morale gets regenerated) */
					if (victim->team == attacker->team)
						mod *= mof_teamkill->value;
					else
						mod *= mof_enemy->value;
				}
				/* seeing a civi die is more "acceptable" */
				if (victim->team == TEAM_CIVILIAN)
					mod *= mof_civilian->value;
				/* if an ally (or in singleplayermode, as human, a civilian) got shot, lower the morale, don't heighten it. */
				if (victim->team == ent->team || (victim->team == TEAM_CIVILIAN && ent->team != TEAM_ALIEN && sv_maxclients->integer == 1))
					mod *= -1;
				/* if you stand near to the attacker or the victim, the morale change is higher. */
				mod *= mor_default->value + pow(0.5, VectorDist(ent->origin, victim->origin) / mor_distance->value)
					* mor_victim->value + pow(0.5, VectorDist(ent->origin, attacker->origin) / mor_distance->value)
					* mor_attacker->value;
				/* morale damage is dependant on the number of living allies */
				mod *= (1 - mon_teamfactor->value)
					+ mon_teamfactor->value * (level.num_spawned[victim->team] + 1)
					/ (level.num_alive[victim->team] + 1);
				/* being hit isn't fun */
				if (ent == victim)
					mod *= mor_pain->value;
				break;
			default:
				Com_Printf("Undefined morale modifier type %i\n", type);
				mod = 0;
			}
			/* clamp new morale */
			/*+0.9 to allow weapons like flamethrowers to inflict panic (typecast rounding) */
			newMorale = ent->morale + (int) (MORALE_RANDOM(mod) + 0.9);
			if (newMorale > GET_MORALE(ent->chr.score.skills[ABILITY_MIND]))
				ent->morale = GET_MORALE(ent->chr.score.skills[ABILITY_MIND]);
			else if (newMorale < 0)
				ent->morale = 0;
			else
				ent->morale = newMorale;

			/* send phys data */
			G_SendStats(ent);
		}
}

/**
 * @param[in] mock pseudo action - only for calculating mock values - NULL for real action
 * @sa G_Damage
 */
static void G_UpdateShotMock (shot_mock_t *mock, edict_t *shooter, edict_t *struck, int damage)
{
	assert(struck->number != shooter->number || mock->allow_self);

	if (damage > 0) {
		if (!struck || !struck->inuse || struck->state & STATE_DEAD)
			return;
		else if (!(struck->visflags & (1 << shooter->team)))
			return;
		else if (struck->team == TEAM_CIVILIAN)
			mock->civilian += 1;
		else if (struck->team == shooter->team)
			mock->friend += 1;
		else if (struck->type == ET_ACTOR || struck->type == ET_ACTOR2x2)
			mock->enemy += 1;
		else
			return;

		mock->damage += damage;
	}
}

/**
 * @brief Update character stats for this mission after successful shoot.
 * @note Mind you that this code is always from the view of PHALANX soldiers right now, not anybody else!
 * @param[in] attacker Pointer to attacker.
 * @param[in] fd Pointer to fireDef_t used in shoot.
 * @param[in] target Pointer to target.
 * @sa G_UpdateCharacterSkills
 * @todo Generally rename "KILLED_ALIENS" to "KILLED_ENEMIES" and adapt all checks to check for (attacker->team == target->team)?
 */
static void G_UpdateCharacterBodycount (edict_t *attacker, const fireDef_t *fd, const edict_t *target)
{
	if (!attacker || !fd || !target)
		return;

	switch (target->team) {
	case TEAM_ALIEN:	/**< Aliens. */
		if (target->HP <= 0) {
			if (attacker->chr.scoreMission)
				attacker->chr.scoreMission->kills[KILLED_ALIENS]++;
			attacker->chr.score.kills[KILLED_ALIENS]++;
		} else {
			if (attacker->chr.scoreMission)
				attacker->chr.scoreMission->stuns[KILLED_ALIENS]++;
			attacker->chr.score.stuns[KILLED_ALIENS]++;
		}

		/**@todo Add check for valid values of fd->weaponSkill */
		if (attacker->chr.scoreMission)
			attacker->chr.scoreMission->skillKills[fd->weaponSkill]++;
		break;
	case TEAM_CIVILIAN:	/**< Civilians. */
		if (target->HP <= 0) {
			if (attacker->chr.scoreMission)
				attacker->chr.scoreMission->kills[KILLED_CIVILIANS]++;
			attacker->chr.score.kills[KILLED_CIVILIANS]++;
		} else {
			attacker->chr.scoreMission->stuns[KILLED_CIVILIANS]++;
			attacker->chr.score.stuns[KILLED_CIVILIANS]++;
		}
		break;
	case TEAM_PHALANX:	/* PHALANX soldiers. */
		if (target->HP <= 0) {
			if (attacker->chr.scoreMission)
				attacker->chr.scoreMission->kills[KILLED_TEAM]++;
			attacker->chr.score.kills[KILLED_TEAM]++;
		} else {
			if (attacker->chr.scoreMission)
				attacker->chr.scoreMission->stuns[KILLED_TEAM]++;
			attacker->chr.score.stuns[KILLED_TEAM]++;
		}
		break;
	default:
		break;
	}
}

/**
 * @brief Increases the 'hit' score by one for all affected teams/skills by one (except splash damage, read below).
 * @param[in,out] attacker The soldier to update (he/she dealt the damage)
 * @param[in] target The hit target.
 * @param[in] fd the used fire definition.
 * @param[in] splashDamage Do we count it as splashdamage? If this value is not zero the stats well be counted as splashdamage and the value will be added to the overall splash-damage count.
 */
static void G_UpdateHitScore (edict_t * attacker, const edict_t * target, const fireDef_t * fd, const int splashDamage)
{
	if (!attacker || !target || !fd)
		return;

	/* Abort if no player team. */
	if (!attacker->chr.scoreMission)
		return;

	if (!splashDamage) {
		if ((attacker->team == target->team)
		&& (!attacker->chr.scoreMission->firedHit[KILLED_TEAM])) {
			/* Increase friendly fire counter. */
			attacker->chr.scoreMission->hits[fd->weaponSkill][KILLED_TEAM]++;
			attacker->chr.scoreMission->firedHit[KILLED_TEAM] = qtrue;
		}

		switch (target->team) {
			case TEAM_CIVILIAN:
				if (!attacker->chr.scoreMission->firedHit[KILLED_CIVILIANS]) {
					attacker->chr.scoreMission->hits[fd->weaponSkill][KILLED_CIVILIANS]++;
					attacker->chr.scoreMission->firedHit[KILLED_CIVILIANS] = qtrue;
				}
				break;
			case TEAM_ALIEN:
				if (!attacker->chr.scoreMission->firedHit[KILLED_ALIENS]) {
					attacker->chr.scoreMission->hits[fd->weaponSkill][KILLED_ALIENS]++;
					attacker->chr.scoreMission->firedHit[KILLED_ALIENS] = qtrue;
				}
				break;
			default:
				return;
		}
	} else {
		if (attacker->team == target->team) {
			/* Increase friendly fire counter. */
			attacker->chr.scoreMission->hitsSplashDamage[fd->weaponSkill][KILLED_TEAM] += splashDamage;
			if (!attacker->chr.scoreMission->firedSplashHit[KILLED_TEAM]) {
				attacker->chr.scoreMission->hitsSplash[fd->weaponSkill][KILLED_TEAM]++;
				attacker->chr.scoreMission->firedSplashHit[KILLED_TEAM] = qtrue;
			}
		}

		switch (target->team) {
			case TEAM_CIVILIAN:
				attacker->chr.scoreMission->hitsSplashDamage[fd->weaponSkill][KILLED_CIVILIANS] += splashDamage;
				if (!attacker->chr.scoreMission->firedSplashHit[KILLED_CIVILIANS]) {
					attacker->chr.scoreMission->hitsSplash[fd->weaponSkill][KILLED_CIVILIANS]++;
					attacker->chr.scoreMission->firedSplashHit[KILLED_CIVILIANS] = qtrue;
				}
				break;
			case TEAM_ALIEN:
				attacker->chr.scoreMission->hitsSplashDamage[fd->weaponSkill][KILLED_ALIENS] += splashDamage;
				if (!attacker->chr.scoreMission->firedSplashHit[KILLED_ALIENS]) {
					attacker->chr.scoreMission->hitsSplash[fd->weaponSkill][KILLED_ALIENS]++;
					attacker->chr.scoreMission->firedSplashHit[KILLED_ALIENS] = qtrue;
				}
				break;
			default:
				return;
		}
	}
}

/**
 * @brief Deals damage of a give type and amount to a target.
 * @param[in] target What we want to damage.
 * @param[in] fd The fire definition that defines what type of damage is dealt.
 * @param[in] damage The value of the damage.
 * @param[in] attacker The attacker.
 * @param[in] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @sa G_SplashDamage
 * @sa G_PrintActorStats
 */
static void G_Damage (edict_t *target, const fireDef_t *fd, int damage, edict_t *attacker, shot_mock_t *mock)
{
	qboolean stunEl = (fd->obj->dmgtype == gi.csi->damStunElectro);
	qboolean stunGas = (fd->obj->dmgtype == gi.csi->damStunGas);
	qboolean shock = (fd->obj->dmgtype == gi.csi->damShock);
	qboolean isRobot;

	assert(target);
	assert(target->type == ET_ACTOR
			|| target->type == ET_ACTOR2x2
			|| target->type == ET_BREAKABLE
			|| target->type == ET_DOOR);

	/* Breakables */
	if (target->type == ET_BREAKABLE || target->type == ET_DOOR) {
		/* Breakables are immune to stun & shock damage. */
		if (stunEl || stunGas || shock || mock)
 			return;

		if (damage >= target->HP) {
			vec3_t origin;

			VectorCenterFromMinsMaxs(target->absmin, target->absmax, origin);

			gi.AddEvent(PM_ALL, EV_MODEL_EXPLODE);
			gi.WriteShort(target->number);
			if (target->particle && Q_strcmp(target->particle, "null")) {
				gi.AddEvent(PM_ALL, EV_SPAWN_PARTICLE);
				gi.WriteShort(target->spawnflags);
				gi.WriteByte(1);
				gi.WritePos(origin);
				gi.WriteString(target->particle);
				gi.EndEvents();
			}
			switch (target->material) {
			case MAT_GLASS:
				gi.PositionedSound(PM_ALL, origin, target, "misc/breakglass", CHAN_AUTO, 1);
				break;
			case MAT_METAL:
				gi.PositionedSound(PM_ALL, origin, target, "misc/breakmetal", CHAN_AUTO, 1);
				break;
			case MAT_ELECTRICAL:
				gi.PositionedSound(PM_ALL, origin, target, "misc/breakelectric", CHAN_AUTO, 1);
				break;
			case MAT_WOOD:
				gi.PositionedSound(PM_ALL, origin, target, "misc/breakwood", CHAN_AUTO, 1);
				break;
			case MAT_MAX:
				break;
			}
			/* unlink to update the routing */
			gi.UnlinkEdict(target);
			target->inuse = qfalse;
			target->HP = 0;
			G_RecalcRouting(target);
			/* now we can destroy the edict completely */
			G_FreeEdict(target);
		} else {
			target->HP = max(target->HP - damage, 0);
		}
		return;
	}

	/* Actors don't die again. */
	if (target->state & STATE_DEAD)
		return;

	/* only actors after this point - and they must have a teamdef */
	assert(target->chr.teamDef);
	isRobot = target->chr.teamDef->robot;

	/* Apply armour effects. */
	if (damage > 0) {
		if (target->i.c[gi.csi->idArmour]) {
			const objDef_t *ad = target->i.c[gi.csi->idArmour]->item.t;
			Com_DPrintf(DEBUG_GAME, "G_Damage: damage for '%s': %i, dmgweight (%i) protection: %i",
				target->chr.name, damage, fd->dmgweight, ad->protection[fd->dmgweight]);
			damage = max(1, damage - ad->protection[fd->dmgweight]);
		} else {
			Com_DPrintf(DEBUG_GAME, "G_Damage: damage for '%s': %i, dmgweight (%i) protection: 0",
				target->chr.name, damage, fd->dmgweight);
		}
	} else if (damage < 0) {
		/* Robots can't be healed. */
		/** @todo We can't 'repair' robots yet :) The least we need for this would be a seperation of medikit vs. a repair-kit. */
		if (isRobot)
			return;
	}
	Com_DPrintf(DEBUG_GAME, " Total damage: %d\n", damage);

	/* Apply difficulty settings. */
	if (sv_maxclients->integer == 1) {
		if (attacker->team == TEAM_ALIEN && target->team < TEAM_ALIEN)
			damage *= pow(1.18, difficulty->integer);
		else if (attacker->team < TEAM_ALIEN && target->team == TEAM_ALIEN)
			damage *= pow(1.18, -difficulty->integer);
	}

	assert((attacker->team >= 0) && (attacker->team < MAX_TEAMS));
	assert((target->team >= 0) && (target->team < MAX_TEAMS));

	if (g_nodamage != NULL && !g_nodamage->integer) {
		/* hit */
		if (mock) {
			G_UpdateShotMock(mock, attacker, target, damage);
		} else if (stunEl) {
			target->STUN += damage;
		} else if (stunGas) {
			if (!isRobot)
				target->STUN += damage;
		} else if (shock) {
			/* Only do this if it's not one from our own team ... they should known that there is a flashbang coming. */
			if (!isRobot && target->team != attacker->team) {
				const player_t *player = game.players + target->pnum;
				/* FIXME: there should be a possible protection, too */
				target->TU = 0; /* flashbangs kill TUs */
				target->state |= STATE_DAZED; /* entity is dazed */
				gi.cprintf(player, PRINT_HUD, _("Soldier is dazed!\nEnemy used flashbang!\n"));
				return;
			}
		} else {
			target->HP = max(target->HP - damage, 0);
			if (damage < 0) {
				/* The 'attacker' is healing the target. */
				/* Update stats here to get info on how many TUs the target received. */
				if (target->chr.scoreMission)
					target->chr.scoreMission->heal += -damage;

				/** @todo Do the same for "attacker" but as "applied" healing
					e.g. attacker->chr->scoreMission.healOthers += -damage; ? */

				/** @todo Also increase the morale a little bit when
					soldier gets healing and morale is lower than max possible? */
			} else {
				/* Real damage was dealt. */

				/* Update overall splash damage for stats/score. */
				if (!mock && (damage > 0) && (fd->splrad)) /**< Check for >0 and splrad to not count this as direct hit. */
					G_UpdateHitScore(attacker, target, fd, damage);
			}
		}
	}

	if (mock)
		return;

	/* HP shouldn't become negative.
	 * Note: This check needs to be done for every assignment to HP above anyway since a "return" could pop up in between.
	 * I'll leave this one in here just in case. */
	target->HP = max(target->HP, 0);

	/* Check death/knockout. */
	if (target->HP == 0 || target->HP <= target->STUN) {
		G_SendStats(target);
		/* prints stats for multiplayer to game console */
		if (sv_maxclients->integer > 1) {
			G_PrintActorStats(target, attacker, fd);
		}

		G_ActorDie(target, target->HP == 0 ? STATE_DEAD : STATE_STUN, attacker);

		/* apply morale changes */
		if (mor_panic->integer)
			G_Morale(ML_DEATH, target, attacker, damage);

		/* count kills */
		if (target->HP == 0)
			level.num_kills[attacker->team][target->team]++;
		/* count stuns */
		else
			level.num_stuns[attacker->team][target->team]++;

		/* Update number of killed/stunned actors for this attacker. */
		G_UpdateCharacterBodycount(attacker, fd, target);

	} else {
		target->chr.minHP = min(target->chr.minHP, target->HP);
		if (damage > 0) {
			if (mor_panic->integer)
				G_Morale(ML_WOUND, target, attacker, damage);
		} else { /* medikit, etc. */
			if (target->HP > GET_HP(target->chr.score.skills[ABILITY_POWER]))
				target->HP = max(GET_HP(target->chr.score.skills[ABILITY_POWER]), 0);
		}
		G_SendStats(target);
	}
}

#ifdef DEBUG
/**
 * @brief Stun all members of a giben team.
 */
void G_StunTeam (void)
{
	/* default is to kill all teams */
	int teamToKill = -1, i;
	edict_t *ent;

	/* with a parameter we will be able to kill a specific team */
	if (gi.Cmd_Argc() == 2)
		teamToKill = atoi(gi.Cmd_Argv(1));

	Com_DPrintf(DEBUG_GAME, "G_StunTeam: stun team %i\n", teamToKill);

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent)) {
			if (teamToKill >= 0 && ent->team != teamToKill)
				continue;

			/* die */
			G_ActorDie(ent, STATE_STUN, NULL);

			if (teamToKill == TEAM_ALIEN)
				level.num_stuns[TEAM_PHALANX][TEAM_ALIEN]++;
			else
				level.num_stuns[TEAM_ALIEN][teamToKill]++;
		}

	/* check for win conditions */
	G_CheckEndGame();
}
#endif

/**
 * @returns True if the surface has the fireaffected flag set and the firedef
 * might produce fire (e.g. flamer)
 * @param[in] surface The collision surface to check the surface flag for
 * @param[in] fd The firedef to check the @c dmgtype for
 */
static inline qboolean G_FireAffectedSurface (const cBspSurface_t *surface, const fireDef_t *fd)
{
	if (!surface)
		return qfalse;

	if (!(surface->surfaceFlags & SURF_BURN))
		return qfalse;

	if (fd->obj->dmgtype == gi.csi->damFire || fd->obj->dmgtype == gi.csi->damBlast)
		return qtrue;

	return qfalse;
}

/**
 * @brief Deals splash damage to a target and its surroundings.
 * @param[in] ent The shooting actor
 * @param[in] fd The fire definition that defines what type of damage is dealt and how big the splash radius is.
 * @param[in] impact The impact vector where the grenade is exploding
 * @param[in,out] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @param[in] tr The trace where the grenade hits something (or not)
 */
static void G_SplashDamage (edict_t *ent, const fireDef_t *fd, vec3_t impact, shot_mock_t *mock, trace_t* tr)
{
	edict_t *check;
	vec3_t center;
	float dist;
	int damage;
	int i;

	const qboolean shock = (fd->obj->dmgtype == gi.csi->damShock);

	assert(fd->splrad);

	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++) {
		/* check basic info */
		if (!check->inuse)
			continue;

		/* If we use a blinding weapon we skip the target if it's looking
		 * away from the impact location. */
		if (shock && !G_FrustumVis(check, impact))
			continue;

		if (check->type == ET_ACTOR || check->type == ET_ACTOR2x2)
			VectorCopy(check->origin, center);
		else if (check->type == ET_BREAKABLE || check->type == ET_DOOR)
			VectorCenterFromMinsMaxs(check->absmin, check->absmax, center);
		else
			continue;

		/* check for distance */
		dist = VectorDist(impact, center);
		dist = dist > UNIT_SIZE / 2 ? dist - UNIT_SIZE / 2 : 0;
		if (dist > fd->splrad)
			continue;

		if (fd->irgoggles && (check->type == ET_ACTOR || check->type == ET_ACTOR2x2)) {
			/* check whether this actor (check) is in the field of view of the 'shooter' (ent) */
			if (G_FrustumVis(ent, check->origin)) {
				if (!mock) {
					G_AppearPerishEvent(~G_VisToPM(check->visflags), 1, check);
					check->visflags |= ~check->visflags;
				}
				continue;
			}
		}

		/* check for walls */
		if ((check->type == ET_ACTOR || check->type == ET_ACTOR2x2) && !G_ActorVis(impact, check, qfalse))
			continue;

		/* do damage */
		if (shock)
			damage = 0;
		else
			damage = fd->spldmg[0] * (1.0 - dist / fd->splrad);

		if (mock)
			mock->allow_self = qtrue;
		G_Damage(check, fd, damage, ent, mock);
		if (mock)
			mock->allow_self = qfalse;
	}

	/* FIXME: splash might also hit other surfaces and the trace doesn't handle that */
	if (tr && G_FireAffectedSurface(tr->surface, fd)) {
		/* sent particle to all players */
		gi.AddEvent(PM_ALL, EV_SPAWN_PARTICLE);
		gi.WriteShort(tr->contentFlags >> 8);
		gi.WriteByte(0);
		/* move a little away from the impact vector */
		VectorMA(impact, 1, tr->plane.normal, impact);
		gi.WritePos(impact);
		gi.WriteString("burning");
		gi.EndEvents();
	}
}

#define GRENADE_DT			0.1
#define GRENADE_STOPSPEED	60.0
/**
 * @sa G_ShootSingle
 * @sa Com_GrenadeTarget
 * @param[in] player The shooting player
 * @param[in] ent The shooting actor
 * @param[in] fd The firedefinition the actor is shooting with
 * @param[in] from The position the actor is shooting from
 * @param[in] at The grid position the actor is shooting to (or trying to shoot to)
 * @param[in] mask The team visibility mask (who's seeing the event)
 * @param[in] weapon The weapon to shoot with
 * @param[in] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @param[in] z_align This value may change the target z height
 */
static void G_ShootGrenade (player_t *player, edict_t *ent, const fireDef_t *fd,
	vec3_t from, pos3_t at, int mask, const item_t *weapon, shot_mock_t *mock, int z_align)
{
	vec3_t last, target, temp;
	vec3_t startV, curV, oldPos, newPos;
	vec3_t angles;
	float dt, time, speed;
	float acc;
	trace_t tr;
	int bounce;
/*	int i; */
	byte flags;

	/* Check if the shooter is still alive (me may fire with area-damage ammo and have just hit the near ground). */
	if (ent->state & STATE_DEAD) {
		Com_DPrintf(DEBUG_GAME, "G_ShootGrenade: Shooter is dead, shot not possible.\n");
		return;
	}

	/* get positional data */
	VectorCopy(from, last);
	gi.GridPosToVec(gi.routingMap, at, target);
	/* first apply z_align value */
	target[2] -= z_align;

	/* prefer to aim grenades at the ground */
	target[2] -= GROUND_DELTA;

	/* calculate parabola */
	dt = gi.GrenadeTarget(last, target, fd->range, fd->launched, fd->rolled, startV);
	if (!dt) {
		if (!mock)
			gi.cprintf(player, PRINT_CONSOLE, _("Can't perform action - impossible throw!\n"));
		return;
	}

	/* cap start speed */
	speed = VectorLength(startV);
	if (speed > fd->range)
		speed = fd->range;

	/* add random effects and get new dir */
	acc = GET_ACC(ent->chr.score.skills[ABILITY_ACCURACY], fd->weaponSkill ? ent->chr.score.skills[fd->weaponSkill] : 0);

	VecToAngles(startV, angles);
	/* @todo Remove the 2.0f and use gaussian random number instead of crand() */
	angles[PITCH] += crand() * 2.0f * (fd->spread[0] * (WEAPON_BALANCE + SKILL_BALANCE * acc));
	angles[YAW] += crand() * 2.0f * (fd->spread[1] * (WEAPON_BALANCE + SKILL_BALANCE * acc));
	AngleVectors(angles, startV, NULL, NULL);
	VectorScale(startV, speed, startV);

	/* move */
	VectorCopy(last, oldPos);
	VectorCopy(startV, curV);
	time = 0;
	dt = 0;
	bounce = 0;
	flags = SF_BOUNCING;
	for (;;) {
		/* kinematics */
		VectorMA(oldPos, GRENADE_DT, curV, newPos);
		newPos[2] -= 0.5 * GRAVITY * GRENADE_DT * GRENADE_DT;
		curV[2] -= GRAVITY * GRENADE_DT;

		/* trace */
		tr = gi.trace(oldPos, NULL, NULL, newPos, ent, MASK_SHOT);
		if (tr.fraction < 1.0 || time + dt > 4.0) {
			/* advance time */
			dt += tr.fraction * GRENADE_DT;
			time += dt;
			bounce++;

			if (tr.fraction < 1.0)
				VectorCopy(tr.endpos, newPos);

#if 0
			/* please debug, currently it causes double sounds */
			/* calculate additional visibility */
			for (i = 0; i < MAX_TEAMS; i++)
				if (G_TeamPointVis(i, newPos))
					mask |= 1 << i;
#endif

			if
				/* enough bouncing around */
				(VectorLength(curV) < GRENADE_STOPSPEED || time > 4.0 || bounce > fd->bounce
				 /* or we have sensors that tell us enemy is near */
				 || (!fd->delay && tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_ACTOR2x2))) {

				if (!mock) {
					/* explode */
					gi.AddEvent(G_VisToPM(mask), EV_ACTOR_THROW);
					gi.WriteShort(dt * 1000);
					gi.WriteShort(fd->obj->idx);
					gi.WriteByte(fd->weapFdsIdx);
					gi.WriteByte(fd->fdIdx);
					if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_ACTOR2x2))
						gi.WriteByte(flags | SF_BODY);
					else
						gi.WriteByte(flags | SF_IMPACT);
					gi.WritePos(last);
					gi.WritePos(startV);
				}

				tr.endpos[2] += 10;

				/* check if this is a stone, ammor clip or grenade */
				if (fd->splrad) {
					G_SplashDamage(ent, fd, tr.endpos, mock, &tr);
				} else if (!mock) {
					/* spawn the stone on the floor */
					if (fd->ammo && !fd->splrad && weapon->t->thrown) {
						pos3_t drop;
						edict_t *floor, *actor;

						VecToPos(tr.endpos, drop);

						for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
							if (floor->inuse
								&& floor->type == ET_ITEM
								&& VectorCompare(drop, floor->pos))
								break;
						}

						if (floor == &g_edicts[globals.num_edicts]) {
							floor = G_SpawnFloor(drop);

							for (actor = g_edicts; actor < &g_edicts[globals.num_edicts]; actor++)
								if (actor->inuse
									 && (actor->type == ET_ACTOR || actor->type == ET_ACTOR2x2)
									 && VectorCompare(drop, actor->pos) )
									FLOOR(actor) = FLOOR(floor);
						} else {
							gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
							gi.WriteShort(floor->number);
							floor->visflags = 0;
						}
						Com_TryAddToInventory(&floor->i, *weapon, &gi.csi->ids[gi.csi->idFloor]);

						/* send item info to the clients */
						G_CheckVis(floor, qtrue);
					}
				}
				return;
			}

			if (!mock) {
				/* send */
				gi.AddEvent(G_VisToPM(mask), EV_ACTOR_THROW);
				gi.WriteShort(dt * 1000);
				gi.WriteShort(fd->obj->idx);
				gi.WriteByte(fd->weapFdsIdx);
				gi.WriteByte(fd->fdIdx);
				gi.WriteByte(flags);
				gi.WritePos(last);
				gi.WritePos(startV);
			}
			flags |= SF_BOUNCED;

			/* bounce */
			VectorScale(curV, fd->bounceFac, curV);
			VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, curV), temp);
			VectorAdd(temp, curV, startV);
			VectorAdd(temp, startV, curV);

			/* prepare next move */
			VectorCopy(tr.endpos, last);
			VectorCopy(tr.endpos, oldPos);
			VectorCopy(curV, startV);
			dt = 0;
		} else {
			dt += GRENADE_DT;
			VectorCopy(newPos, oldPos);
		}
	}
}

/**
 * @brief Displays the results of a trace. Used to see if a bullet hit something.
 * @param[in] start The starting loaction of the trace.
 * @param[in] tr The trace data.
 */
static void DumpTrace (vec3_t start, trace_t tr)
{
	Com_DPrintf(DEBUG_GAME, "start (%i, %i, %i) end (%i, %i, %i)\n",
		(int)start[0], (int)start[1], (int)start[2],
		(int)tr.endpos[0], (int)tr.endpos[1], (int)tr.endpos[2]);
	Com_DPrintf(DEBUG_GAME, "allsolid:%s startsolid:%s fraction:%f contentFlags:%X\n",
		tr.allsolid ? "true" : "false",
		tr.startsolid ? "true" : "false",
		tr.fraction, tr.contentFlags);
	Com_DPrintf(DEBUG_GAME, "is entity:%s %s %i\n",
		tr.ent ? "yes" : "no",
		tr.ent ? tr.ent->classname : "",
		tr.ent ? tr.ent->HP : 0);
}

/**
 * @brief Displays data about all server entities.
 */
static void DumpAllEntities (void)
{
	int i;
	edict_t *check;

	for (i = 0, check = g_edicts; i < globals.num_edicts; i++, check++) {
		Com_DPrintf(DEBUG_GAME, "%i %s %s %s (%i, %i, %i) (%i, %i, %i) [%i, %i, %i] [%i, %i, %i]\n", i,
			check->inuse ? "in use" : "unused",
			check->classname,
			check->model,
			(int) check->absmin[0], (int) check->absmin[1], (int) check->absmin[2],
			(int) check->absmax[0], (int) check->absmax[1], (int) check->absmax[2],
			(int) check->mins[0], (int) check->mins[1], (int) check->mins[2],
			(int) check->maxs[0], (int) check->maxs[1], (int) check->maxs[2]);
	}
}

/**
 * @brief Fires straight shots.
 * @param[in] ent The attacker.
 * @param[in] fd The fire definition that is used for the shot.
 * @param[in] from Location of the gun muzzle.
 * @param[in] at Grid coordinate of the target.
 * @param[in] mask Visibility bit-mask (who's seeing the event)
 * @param[in] weapon The weapon the actor is shooting with
 * @param[in] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @param[in] z_align This value may change the target z height
 * @param[in] i The ith shot
 * @param[in] type The firemode (ST_NUM_SHOOT_TYPES)
 */
static void G_ShootSingle (edict_t *ent, const fireDef_t *fd, const vec3_t from, pos3_t at,
	int mask, const item_t *weapon, shot_mock_t *mock, int z_align, int i, int type)
{
	vec3_t dir;	/* Direction from the location of the gun muzzle ("from") to the target ("at") */
	vec3_t angles;	/* ?? @todo The random dir-modifier ?? */
	vec3_t cur_loc;	/* The current location of the projectile. */
	vec3_t impact;	/* The location of the target (-center?) */
	vec3_t temp;
	vec3_t tracefrom;	/* sum */
	trace_t tr;	/* the traceing */
	float acc;	/* Accuracy modifier for the angle of the shot. */
	float range;	/* ?? @todo */
	float gauss1;
	float gauss2;   /* For storing 2 gaussian distributed random values. */
	float commonfactor; /* common to pitch and yaw spread, avoid extra multiplications */
	float injurymultiplier;
	int bounce;	/* count the bouncing */
	int damage;	/* The damage to be dealt to the target. */
	byte flags;	/* ?? @todo */
	int throughWall; /* shoot through x walls */
	int clientType;	/* shoot type for ai controlled actors */

	/* Check if the shooter is still alive (me may fire with area-damage ammo and have just hit the near ground). */
	if (ent->state & STATE_DEAD) {
		Com_DPrintf(DEBUG_GAME, "G_ShootSingle: Shooter is dead, shot not possible.\n");
		return;
	}

	/* Calc direction of the shot. */
	gi.GridPosToVec(gi.routingMap, at, impact);	/* Get the position of the targetted grid-cell. ('impact' is used only temporary here)*/
	impact[2] -= z_align;
	VectorCopy(from, cur_loc);		/* Set current location of the projectile to the starting (muzzle) location. */
	VectorSubtract(impact, cur_loc, dir);	/* Calculate the vector from current location to the target. */
	VectorNormalize(dir);			/* Normalize the vector i.e. make length 1.0 */

	/* ?? @todo: Probably places the starting-location a bit away (cur_loc+8*dir) from the attacker-model/grid.
	 * Might need some change to reflect 2x2 units.
	 * Also might need a check if the distance is bigger than the one to the impact location. */
	VectorMA(cur_loc, sv_shot_origin->value, dir, cur_loc);
	VecToAngles(dir, angles);		/* Get the angles of the direction vector. */

	/* Get accuracy value for this attacker. */
	acc = GET_ACC(ent->chr.score.skills[ABILITY_ACCURACY], fd->weaponSkill ? ent->chr.score.skills[fd->weaponSkill] : 0);

	/* Get 2 gaussian distributed random values */
	gaussrand(&gauss1, &gauss2);

	/* Calculate spread multiplier to give worse precision when HPs are not at max */
	injurymultiplier = GET_INJURY_MULT(ent->chr.score.skills[ABILITY_MIND], ent->HP, ent->chr.maxHP == 0 ? 100 : ent->chr.maxHP);
	Com_DPrintf(DEBUG_GAME, "G_ShootSingle: injury spread multiplier = %5.3f (mind %d, HP %d, maxHP %d)\n", injurymultiplier,
		ent->chr.score.skills[ABILITY_MIND], ent->HP, ent->chr.maxHP == 0 ? 100 : ent->chr.maxHP);

	/* Modify the angles with the accuracy modifier as a randomizer-range. If the attacker is crouched this modifier is included as well.  */
	/* Base spread multiplier comes from the firedef's spread values. Soldier skills further modify the spread.
	 * A good soldier will tighten the spread, a bad one will widen it, for skillBalanceMinimum values between 0 and 1.*/
	commonfactor = (WEAPON_BALANCE + SKILL_BALANCE * acc) * injurymultiplier;
	if ((ent->state & STATE_CROUCHED) && fd->crouch) {
		angles[PITCH] += gauss1 * (fd->spread[0] * commonfactor) * fd->crouch;
		angles[YAW] += gauss2 * (fd->spread[1] * commonfactor) * fd->crouch;
	} else {
		angles[PITCH] += gauss1 * (fd->spread[0] * commonfactor);
		angles[YAW] += gauss2 * (fd->spread[1] * commonfactor);
	}
	/* Convert changed angles into new direction. */
	AngleVectors(angles, dir, NULL, NULL);

	/* shoot and bounce */
	throughWall = fd->throughWall;
	range = fd->range;
	bounce = 0;
	flags = 0;

	/* Are we healing? */
	if (fd->damage[0] < 0)
		damage = fd->damage[0] + (fd->damage[1] * crand());
	else
		damage = max(0, fd->damage[0] + (fd->damage[1] * crand()));
	VectorCopy(cur_loc, tracefrom);
	for (;;) {
		/* Calc 'impact' vector that is located at the end of the range
		 * defined by the fireDef_t. This is not really the impact location,
		 * but rather the 'endofrange' location, see below for another use.*/
		VectorMA(cur_loc, range, dir, impact);

		DumpAllEntities();

		/* Do the trace from current position of the projectile
		 * to the end_of_range location.*/
		/* mins and maxs should be set via localModel_t don't they? */
		tr = gi.trace(tracefrom, NULL, NULL, impact, ent, MASK_SHOT);

		DumpTrace(tracefrom, tr);

		/* maybe we start the trace from within a brush (e.g. in case of throughWall) */
		if (tr.startsolid)
			break;

		/* _Now_ we copy the correct impact location. */
		VectorCopy(tr.endpos, impact);

		/* set flags when trace hit something */
		if (tr.fraction < 1.0) {
			if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_ACTOR2x2)
				/* check if we differenciate between body and wall */
				&& !fd->delay)
				flags |= SF_BODY;
			else if (bounce < fd->bounce)
				flags |= SF_BOUNCING;
			else
				flags |= SF_IMPACT;
		}

#if 0
		/* @todo: please debug, currently it causes double sounds */
		/* calculate additional visibility */
		for (k = 0; k < MAX_TEAMS; k++)
			if (G_TeamPointVis(k, impact))
				mask |= 1 << k;
#endif

		/* victims see shots */
		if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_ACTOR2x2))
			mask |= 1 << tr.ent->team;

		if (!mock) {
			if (IS_SHOT_RIGHT(type))
				clientType = M_FIRE_R;
			if (IS_SHOT_LEFT(type))
				clientType = M_FIRE_L;
			else if (IS_SHOT_HEADGEAR(type))
				clientType = M_FIRE_HEADGEAR;
			else
				clientType = 0xFF; /* human controlled player */

			/* send shot */
			gi.AddEvent(G_VisToPM(mask), EV_ACTOR_SHOOT);
			gi.WriteShort(ent->number);
			gi.WriteShort(fd->obj->idx);
			gi.WriteByte(fd->weapFdsIdx);
			gi.WriteByte(fd->fdIdx);
			gi.WriteByte(clientType);
			gi.WriteByte(flags);
			gi.WriteByte(tr.contentFlags);
			gi.WritePos(tracefrom);
			gi.WritePos(impact);
			gi.WriteDir(tr.plane.normal);

			/* send shot sound to the others */
			gi.AddEvent(~G_VisToPM(mask), EV_ACTOR_SHOOT_HIDDEN);
			gi.WriteByte(qfalse);
			gi.WriteShort(fd->obj->idx);
			gi.WriteByte(fd->weapFdsIdx);
			gi.WriteByte(fd->fdIdx);

			if (i == 0 && (fd->obj->dmgtype == gi.csi->damFire
			 || fd->obj->dmgtype == gi.csi->damBlast)
			 && tr.surface && tr.surface->surfaceFlags & SURF_BURN) {
				vec3_t origin;

				/* sent particle to all players */
				gi.AddEvent(PM_ALL, EV_SPAWN_PARTICLE);
				gi.WriteShort(tr.contentFlags >> 8);
				gi.WriteByte(1);
				VectorMA(impact, 1, tr.plane.normal, origin);
				gi.WritePos(origin);
				gi.WriteString("fire");
				gi.EndEvents();
			}
		}

		if (tr.fraction < 1.0 && !fd->bounce) {
			/* check for shooting through wall */
			if (throughWall && tr.contentFlags & CONTENTS_SOLID) {
				throughWall--;
				Com_DPrintf(DEBUG_GAME, "Shot through wall, %i walls left.\n", throughWall);
				/* reduce damage */
				/* TODO: reduce even more if the wall was hit far away and
				 * not close by the shooting actor */
				damage /= sqrt(fd->throughWall - throughWall + 1);
				VectorMA(tr.endpos, MAX_WALL_THICKNESS_FOR_SHOOTING_THROUGH, dir, tracefrom);
				continue;
			}

			/* do splash damage */
			if (fd->splrad) {
				VectorMA(impact, sv_shot_origin->value, tr.plane.normal, impact);
				G_SplashDamage(ent, fd, impact, mock, &tr);
			}
		}

		/* do damage if the trace hit an entity */
		if (tr.ent && (tr.ent->type == ET_ACTOR || tr.ent->type == ET_ACTOR2x2 || (tr.ent->flags & FL_DESTROYABLE))) {
			G_Damage(tr.ent, fd, damage, ent, mock);

			if (!mock) { /* check for firedHit is done in G_UpdateHitScore */
				/* Count this as a hit of this firemode. */
				G_UpdateHitScore(ent, tr.ent, fd, 0);
			}
			break;
		}

		/* bounce is checked here to see if the rubber rocket hit walls enough times to wear out*/
		bounce++;
		if (bounce > fd->bounce || tr.fraction >= 1.0)
			break;

		range -= tr.fraction * range;
		VectorCopy(impact, cur_loc);
		VectorScale(tr.plane.normal, -DotProduct(tr.plane.normal, dir), temp);
		VectorAdd(temp, dir, dir);
		VectorAdd(temp, dir, dir);
		flags |= SF_BOUNCED;
	}

	if (!mock) {
		/* spawn the throwable item on the floor but only if it is not depletable */
		if (fd->ammo && !fd->splrad && weapon->t->thrown && !weapon->t->deplete) {
			pos3_t drop;
			edict_t *floor, *actor;

			if (VectorCompare(ent->pos, at)) { /* throw under his own feet */
				VectorCopy(at, drop);
			} else {
				impact[2] -= 20; /* a hack: no-gravity items are flying high */
				VecToPos(impact, drop);
			}

			for (floor = g_edicts; floor < &g_edicts[globals.num_edicts]; floor++) {
				if (floor->inuse
					&& floor->type == ET_ITEM
					&& VectorCompare(drop, floor->pos))
					break;
			}

			if (floor == &g_edicts[globals.num_edicts]) {
				floor = G_SpawnFloor(drop);

				for (actor = g_edicts; actor < &g_edicts[globals.num_edicts]; actor++)
					if (actor->inuse
						 && (actor->type == ET_ACTOR || actor->type == ET_ACTOR2x2)
						 && VectorCompare(drop, actor->pos) )
						FLOOR(actor) = FLOOR(floor);
			} else {
				gi.AddEvent(G_VisToPM(floor->visflags), EV_ENT_PERISH);
				gi.WriteShort(floor->number);
				floor->visflags = 0;
			}
			Com_TryAddToInventory(&floor->i, *weapon, &gi.csi->ids[gi.csi->idFloor]);

			/* send item info to the clients */
			G_CheckVis(floor, qtrue);
		}
	}
}

static void G_GetShotOrigin (edict_t *shooter, fireDef_t *fd, vec3_t dir, vec3_t shotOrigin)
{
	/* get weapon position */
	gi.GridPosToVec(gi.routingMap, shooter->pos, shotOrigin);
	/* adjust height: */
	shotOrigin[2] += fd->shotOrg[1];
	/* adjust horizontal: */
	if (fd->shotOrg[0] != 0) {
		float x, y, length;

		/* get "right" and "left" of a unit(rotate dir 90 on the x-y plane): */
		x = dir[1];
		y = -dir[0];
		length = sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
		/* assign adjustments: */
		shotOrigin[0] += x * fd->shotOrg[0] / length;
		shotOrigin[1] += y * fd->shotOrg[0] / length;
	}
}

/**
 * @sa G_ClientShoot
 */
static qboolean G_GetShotFromType (edict_t *ent, int type, int firemode, item_t **weapon, int *container, fireDef_t **fd)
{
	int weaponFdIdx;

	if (type >= ST_NUM_SHOOT_TYPES)
		gi.error("G_GetShotFromType: unknown shoot type %i.\n", type);

	if (IS_SHOT_HEADGEAR(type)) {
		if (!HEADGEAR(ent))
			return qfalse;
		*weapon = &HEADGEAR(ent)->item;
		*container = gi.csi->idHeadgear;
	} else if (IS_SHOT_RIGHT(type)) {
		if (!RIGHT(ent))
			return qfalse;
		*weapon = &RIGHT(ent)->item;
		*container = gi.csi->idRight;
	} else {
		if (!LEFT(ent))
			return qfalse;
		*weapon = &LEFT(ent)->item;
		*container = gi.csi->idLeft;
	}

	if (!(*weapon)->m) {
		/* This weapon does not use ammo, check for existing firedefs in the weapon. */
		if ((*weapon)->t->numWeapons > 0) {
			/* Get firedef from the weapon entry instead */
			Com_DPrintf(DEBUG_GAME, "od->numWeapons: %i\n", (*weapon)->t->numWeapons);
			weaponFdIdx = FIRESH_FiredefsIDXForWeapon((*weapon)->t, (*weapon)->t);
			Com_DPrintf(DEBUG_GAME, "weaponFdIdx: %i (%s), firemode: %i\n", weaponFdIdx, (*weapon)->t->name, firemode);
			assert(weaponFdIdx >= 0);
			assert(firemode >= 0);
			/* fd = od[weaponFdIdx][firemodeidx] */
			*fd = &(*weapon)->t->fd[weaponFdIdx][firemode];
		} else {
			*weapon = NULL;
			return qfalse;
		}
	} else {
		/* Get firedef from the ammo entry. */
		weaponFdIdx = FIRESH_FiredefsIDXForWeapon((*weapon)->m, (*weapon)->t);
		assert(weaponFdIdx >= 0);
		assert(firemode >= 0);
		/* fd = od[weaponFdIdx][firemodeidx] */
		*fd = &(*weapon)->m->fd[weaponFdIdx][firemode];
	}

	return qtrue;
}

/**
 * @brief Setup for shooting, either real or mock
 * @param[in] player @todo: The player this action belongs to (i.e. either the ai or the player)
 * @param[in] num @todo: The index number of the 'inventory' that is used for the shot (i.e. left or right hand)
 * @param[in] at Position to fire on.
 * @param[in] type What type of shot this is (left, right reaction-left etc...).
 * @param[in] firemode The firemode index of the ammo for the used weapon (objDef.fd[][x])  .
 * @param[in] mock pseudo shooting - only for calculating mock values - NULL for real shots
 * @param[in] allowReaction Set to qtrue to check whether this has forced any reaction fire, otherwise qfalse.
 * @return qtrue if everthing went ok (i.e. the shot(s) where fired ok), otherwise qfalse.
 * @param[in] z_align This value may change the target z height
 */
qboolean G_ClientShoot (player_t * player, int num, pos3_t at, int type,
	int firemode, shot_mock_t *mock, qboolean allowReaction, int z_align)
{
	fireDef_t *fd;
	edict_t *ent;
	item_t *weapon;
	vec3_t dir, center, target, shotOrigin;
	int i, ammo, prev_dir, reaction_leftover, shots;
	int container, mask;
	qboolean quiet;
	int clientType;

	ent = g_edicts + num;
	quiet = (mock != NULL);

	weapon = NULL;
	fd = NULL;
	container = 0;
	if (!G_GetShotFromType(ent, type, firemode, &weapon, &container, &fd)) {
		if (!weapon && !quiet)
			gi.cprintf(player, PRINT_CONSOLE, _("Can't perform action - object not activateable!\n"));
		return qfalse;
	}

	ammo = weapon->a;
	reaction_leftover = IS_SHOT_REACTION(type) ? sv_reaction_leftover->integer : 0;

	/* check if action is possible */
	if (!G_ActionCheck(player, ent, fd->time + reaction_leftover, quiet))
		return qfalse;

	/* Don't allow to shoot yourself */
	if (VectorCompare(ent->pos, at))
		return qfalse;

	/* check that we're not firing a twohanded weapon with one hand! */
	if (weapon->t->fireTwoHanded &&	LEFT(ent)) {
		if (!quiet)
			gi.cprintf(player, PRINT_CONSOLE, _("Can't perform action - weapon cannot be fired one handed!\n"));
		return qfalse;
	}

	/* check we're not out of ammo */
	if (!ammo && fd->ammo && !weapon->t->thrown) {
		if (!quiet)
			gi.cprintf(player, PRINT_CONSOLE, _("Can't perform action - no ammo!\n"));
		return qfalse;
	}

	/* check target is not out of range */
	gi.GridPosToVec(gi.routingMap, at, target);
	if (fd->range < VectorDist(ent->origin, target)) {
		if (!quiet)
			gi.cprintf(player, PRINT_HUD, _("Can't perform action - target out of range!\n"));
		return qfalse;
	}

	/* Count for stats if it's no mock-shot and it's a Phalanx soldier (aliens do not have this info yet). */
	if (!mock && ent->chr.scoreMission) {
		/* Count this start of the shooting for stats/score. */
		/** @todo check for direct shot / splash damage shot? */
		if (fd->splrad) {
			/* Splash damage */
			ent->chr.scoreMission->firedSplashTUs[fd->weaponSkill] += fd->time;
			ent->chr.scoreMission->firedSplash[fd->weaponSkill]++;
			for (i = 0; i < KILLED_NUM_TYPES; i++) {
				/** Reset status. @see G_UpdateHitScore for the check. */
				ent->chr.scoreMission->firedSplashHit[i] = qfalse;
			}
		} else {
			/* Direkt hits */
			ent->chr.scoreMission->firedTUs[fd->weaponSkill] += fd->time;
			ent->chr.scoreMission->fired[fd->weaponSkill]++;
			for (i = 0; i < KILLED_NUM_TYPES; i++) {
				/** Reset status. @see G_UpdateHitScore for the check. */
				ent->chr.scoreMission->firedHit[i] = qfalse;
			}
		}
	}

	/* fire shots */
	shots = fd->shots;
	if (fd->ammo && !weapon->t->thrown) {
		/**
		 * If loaded ammo is less than needed ammo from firedef
		 * then reduce shot-number relative to the difference.
		 * @todo This really needs an overhaul.And it might get dumped completely when
		 * Feature Request "[1814158] Extended shot-definitions in firedef"
		 * https://sourceforge.net/tracker/?func=detail&atid=805245&aid=1814158&group_id=157793
		 * gets implemented.
		 */
		if (ammo < fd->ammo) {
			shots = fd->shots * ammo / fd->ammo;
			ammo = 0;
		} else {
			ammo -= fd->ammo;
		}
		if (shots < 1) {
			if (!quiet)
				gi.cprintf(player, PRINT_HUD, _("Can't perform action - not enough ammo!\n"));
			return qfalse;
		}
	}

	/* rotate the player */
	if (mock)
		prev_dir = ent->dir;
	else
		prev_dir = 0;

	VectorSubtract(at, ent->pos, dir);
	ent->dir = AngleToDV((int) (atan2(dir[1], dir[0]) * todeg));

	if (!mock) {
		G_CheckVisTeam(ent->team, NULL, qfalse);

		gi.AddEvent(G_VisToPM(ent->visflags), EV_ACTOR_TURN);
		gi.WriteShort(num);
		gi.WriteByte(ent->dir);
	}

	/* calculate visibility */
	target[2] -= z_align;
	VectorSubtract(target, ent->origin, dir);
	VectorMA(ent->origin, 0.5, dir, center);
	mask = 0;
	for (i = 0; i < MAX_TEAMS; i++)
		if (ent->visflags & (1 << i) || G_TeamPointVis(i, target) || G_TeamPointVis(i, center))
			mask |= 1 << i;

	if (!mock) {
		/* check whether this has forced any reaction fire */
		if (allowReaction) {
			G_ReactToPreFire(ent);
			if (ent->state & STATE_DEAD)
				/* dead men can't shoot */
				return qfalse;
		}

		if (IS_SHOT_RIGHT(type))
			clientType = M_FIRE_R;
		if (IS_SHOT_LEFT(type))
			clientType = M_FIRE_L;
		else if (IS_SHOT_HEADGEAR(type))
			clientType = M_FIRE_HEADGEAR;
		else
			clientType = 0xFF; /* human controlled player */

		/* start shoot */
		gi.AddEvent(G_VisToPM(mask), EV_ACTOR_START_SHOOT);
		gi.WriteShort(ent->number);
		gi.WriteShort(fd->obj->idx);
		gi.WriteByte(fd->weapFdsIdx);
		gi.WriteByte(fd->fdIdx);
		gi.WriteByte(clientType);
		gi.WriteGPos(ent->pos);
		gi.WriteGPos(at);

		/* send shot sound to the others */
		gi.AddEvent(~G_VisToPM(mask), EV_ACTOR_SHOOT_HIDDEN);
		gi.WriteByte(qtrue);
		gi.WriteShort(fd->obj->idx);
		gi.WriteByte(fd->weapFdsIdx);
		gi.WriteByte(fd->fdIdx);

		/* ammo... */
		if (fd->ammo) {
			if (ammo > 0 || !weapon->t->thrown) {
				gi.AddEvent(G_VisToPM(ent->visflags), EV_INV_AMMO);
				gi.WriteShort(num);
				gi.WriteByte(ammo);
				gi.WriteByte(weapon->m->idx);
				weapon->a = ammo;
				if (IS_SHOT_RIGHT(type))
					gi.WriteByte(gi.csi->idRight);
				else
					gi.WriteByte(gi.csi->idLeft);
			} else { /* delete the knife or the rifle without ammo */
				gi.AddEvent(G_VisToPM(ent->visflags), EV_INV_DEL);
				gi.WriteShort(num);
				gi.WriteByte(container);
				assert(gi.csi->ids[container].single);
				INVSH_EmptyContainer(&ent->i, &gi.csi->ids[container]);
			}
			/* x and y value */
			gi.WriteByte(0);
			gi.WriteByte(0);
		}

		/* remove throwable oneshot && deplete weapon from inventory */
		if (weapon->t->thrown && weapon->t->oneshot && weapon->t->deplete) {
			gi.AddEvent(G_VisToPM(ent->visflags), EV_INV_DEL);
			gi.WriteShort(num);
			gi.WriteByte(container);
			assert(gi.csi->ids[container].single);
			INVSH_EmptyContainer(&ent->i, &gi.csi->ids[container]);
			/* x and y value */
			gi.WriteByte(0);
			gi.WriteByte(0);
		}
	}

	G_GetShotOrigin(ent, fd, dir, shotOrigin);

	/* Fire all shots. */
	for (i = 0; i < shots; i++)
		if (fd->gravity)
			G_ShootGrenade(player, ent, fd, shotOrigin, at, mask, weapon, mock, z_align);
		else
			G_ShootSingle(ent, fd, shotOrigin, at, mask, weapon, mock, z_align, i, type);

	if (!mock) {
		/* send TUs if ent still alive */
		if (ent->inuse && !(ent->state & STATE_DEAD)) {
			ent->TU = max(ent->TU - fd->time, 0);
			G_SendStats(ent);
		}

		/* end events */
		gi.EndEvents();

		/* check for win/draw conditions */
		G_CheckEndGame();

		/* check for Reaction fire against the shooter */
		if (allowReaction)
			G_ReactToPostFire(ent);
	} else {
		ent->dir = prev_dir;
	}
	return qtrue;
}
