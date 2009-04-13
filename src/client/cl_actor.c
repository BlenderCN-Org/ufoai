/**
 * @file cl_actor.c
 * @brief Actor related routines.
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

#include "client.h"
#include "cl_le.h"
#include "cl_team.h"
#include "cl_sound.h"
#include "cl_particle.h"
#include "cl_actor.h"
#include "cl_hud.h"
#include "cl_view.h"
#include "cl_parse.h"
#include "cl_menu.h"
#include "renderer/r_entity.h"
#include "renderer/r_mesh_anim.h"
#include "menu/m_popup.h"
#include "../common/routing.h"
#include "cl_ugv.h"
#include "menu/node/m_node_container.h"
#include "cl_game.h"

/** @brief Confirm actions in tactical mode - valid values are 0, 1 and 2 */
static cvar_t *confirm_actions;
/** @brief Player preference: should the server make guys stand for long walks, to save TU. */
static cvar_t *cl_autostand;

/* public */
le_t *selActor;
const fireDef_t *selFD;
character_t *selChr;
pos3_t truePos; /**< The cell at the current worldlevel under the mouse cursor. */
pos3_t mousePos; /**< The cell that an actor will move to when directed to move. */

/**
 * @brief If you want to change the z level of targeting and shooting,
 * use this value. Negative and positive offsets are possible
 * @sa CL_ActorTargetAlign_f
 * @sa G_ClientShoot
 * @sa G_ShootGrenade
 * @sa G_ShootSingle
 */
int mousePosTargettingAlign = 0;

/**
 * @brief The TUs that the current selected actor needs to walk to the
 * current grid position marked by the mouse cursor (mousePos)
 * @sa CL_MoveLength
 */
int actorMoveLength;

static le_t *mouseActor;
static pos3_t mouseLastPos;
/** for double-click movement and confirmations ... */
pos3_t mousePendPos;

/**
 * @brief Writes player action with its data
 * @param[in] player_action
 * @param[in] entnum The server side edict number of the actor
 */
void MSG_Write_PA (player_action_t player_action, int entnum, ...)
{
	va_list ap;
	struct dbuffer *msg = new_dbuffer();

	va_start(ap, entnum);
	NET_WriteFormat(msg, "bbs", clc_action, player_action, entnum);
	NET_vWriteFormat(msg, pa_format[player_action], ap);
	va_end(ap);
	NET_WriteMsg(cls.netStream, msg);
}

/*
==============================================================
ACTOR MENU UPDATING
==============================================================
*/

/**
 * @brief Return the skill string for the given skill level
 * @return skill string
 * @param[in] skill a skill value between 0 and MAX_SKILL (@todo 0? never reached?)
 */
const char *CL_GetSkillString (const int skill)
{
	const int skillLevel = skill * 10 / MAX_SKILL;
#ifdef DEBUG
	if (skill > MAX_SKILL) {
		Com_Printf("CL_GetSkillString: Skill is bigger than max allowed skill value (%i/%i)\n", skill, MAX_SKILL);
	}
#endif
	switch (skillLevel) {
	case 0:
		return _("Poor");
	case 1:
		return _("Mediocre");
	case 2:
		return _("Average");
	case 3:
		return _("Competent");
	case 4:
		return _("Proficient");
	case 5:
		return _("Very Good");
	case 6:
		return _("Highly Proficient");
	case 7:
		return _("Excellent");
	case 8:
		return _("Outstanding");
	case 9:
	case 10:
		return _("Superhuman");
	default:
		Com_Printf("CL_GetSkillString: Unknown skill: %i (index: %i)\n", skill, skillLevel);
		return "";
	}
}

/**
 * @brief Decide how the actor will walk, taking into account autostanding.
 * @param[in] length The distance to move: units are TU required assuming actor is standing.
 */
int CL_MoveMode (const le_t *le, int length)
{
	assert(le);
	if (le->state & STATE_CROUCHED) {
		if (cl_autostand->integer) { /* Is the player using autostand? */
			if ((float)(2 * TU_CROUCH) < (float)length * (TU_CROUCH_MOVING_FACTOR - 1.0f)) {
				return WALKTYPE_AUTOSTAND_BEING_USED;
			} else {
				return WALKTYPE_AUTOSTAND_BUT_NOT_FAR_ENOUGH;
			}
		} else {
			return WALKTYPE_CROUCH_WALKING;
		}
	} else {
		return WALKTYPE_WALKING;
	}
}

/**
 * @brief Updates the character cvars for the given character.
 *
 * The models and stats that are displayed in the menu are stored in cvars.
 * These cvars are updated here when you select another character.
 *
 * @param chr Pointer to character_t (may not be null)
 * @sa CL_UGVCvars
 * @sa CL_ActorSelect
 */
void CL_CharacterCvars (const character_t * chr)
{
	invList_t *weapon;
	assert(chr);

	Cvar_ForceSet("mn_name", chr->name);
	Cvar_ForceSet("mn_body", CHRSH_CharGetBody(chr));
	Cvar_ForceSet("mn_head", CHRSH_CharGetHead(chr));
	Cvar_ForceSet("mn_skin", va("%i", chr->skin));
	Cvar_ForceSet("mn_skinname", CL_GetTeamSkinName(chr->skin));

	/* visible equipment */
	weapon = chr->inv.c[csi.idRight];
	if (weapon) {
		assert(weapon->item.t >= &csi.ods[0] && weapon->item.t < &csi.ods[MAX_OBJDEFS]);
		Cvar_Set("mn_rweapon", weapon->item.t->model);
	} else
		Cvar_Set("mn_rweapon", "");
	weapon = chr->inv.c[csi.idLeft];
	if (weapon) {
		assert(weapon->item.t >= &csi.ods[0] && weapon->item.t < &csi.ods[MAX_OBJDEFS]);
		Cvar_Set("mn_lweapon", weapon->item.t->model);
	} else
		Cvar_Set("mn_lweapon", "");

	Cvar_Set("mn_chrmis", va("%i", chr->score.assignedMissions));
	Cvar_Set("mn_chrkillalien", va("%i", chr->score.kills[KILLED_ALIENS]));
	Cvar_Set("mn_chrkillcivilian", va("%i", chr->score.kills[KILLED_CIVILIANS]));
	Cvar_Set("mn_chrkillteam", va("%i", chr->score.kills[KILLED_TEAM]));

	GAME_CharacterCvars(chr);

	Cvar_Set("mn_vpwr", va("%i", chr->score.skills[ABILITY_POWER]));
	Cvar_Set("mn_vspd", va("%i", chr->score.skills[ABILITY_SPEED]));
	Cvar_Set("mn_vacc", va("%i", chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_vmnd", va("%i", chr->score.skills[ABILITY_MIND]));
	Cvar_Set("mn_vcls", va("%i", chr->score.skills[SKILL_CLOSE]));
	Cvar_Set("mn_vhvy", va("%i", chr->score.skills[SKILL_HEAVY]));
	Cvar_Set("mn_vass", va("%i", chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set("mn_vsnp", va("%i", chr->score.skills[SKILL_SNIPER]));
	Cvar_Set("mn_vexp", va("%i", chr->score.skills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_vhp", va("%i", chr->HP));
	Cvar_Set("mn_vhpmax", va("%i", chr->maxHP));

	Cvar_Set("mn_tpwr", va("%s (%i)", CL_GetSkillString(chr->score.skills[ABILITY_POWER]), chr->score.skills[ABILITY_POWER]));
	Cvar_Set("mn_tspd", va("%s (%i)", CL_GetSkillString(chr->score.skills[ABILITY_SPEED]), chr->score.skills[ABILITY_SPEED]));
	Cvar_Set("mn_tacc", va("%s (%i)", CL_GetSkillString(chr->score.skills[ABILITY_ACCURACY]), chr->score.skills[ABILITY_ACCURACY]));
	Cvar_Set("mn_tmnd", va("%s (%i)", CL_GetSkillString(chr->score.skills[ABILITY_MIND]), chr->score.skills[ABILITY_MIND]));
	Cvar_Set("mn_tcls", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_CLOSE]), chr->score.skills[SKILL_CLOSE]));
	Cvar_Set("mn_thvy", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_HEAVY]), chr->score.skills[SKILL_HEAVY]));
	Cvar_Set("mn_tass", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_ASSAULT]), chr->score.skills[SKILL_ASSAULT]));
	Cvar_Set("mn_tsnp", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_SNIPER]), chr->score.skills[SKILL_SNIPER]));
	Cvar_Set("mn_texp", va("%s (%i)", CL_GetSkillString(chr->score.skills[SKILL_EXPLOSIVE]), chr->score.skills[SKILL_EXPLOSIVE]));
	Cvar_Set("mn_thp", va("%i (%i)", chr->HP, chr->maxHP));
}

/**
 * @brief Returns the number of the actor in the teamlist.
 * @param[in] le The actor to search.
 * @return The number of the actor in the teamlist.
 */
int CL_GetActorNumber (const le_t * le)
{
	int actorIdx;

	assert(le);

	for (actorIdx = 0; actorIdx < cl.numTeamList; actorIdx++) {
		if (cl.teamList[actorIdx] == le)
			return actorIdx;
	}
	return -1;
}

/**
 * @brief Returns the character information for an actor in the teamlist.
 * @param[in] le The actor to search.
 * @return A pointer to a character struct.
 * @todo We really needs a better way to sync this up.
 */
character_t *CL_GetActorChr (const le_t * le)
{
	const int actorIdx = CL_GetActorNumber(le);
	if (actorIdx < 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_GetActorChr: BAD ACTOR INDEX!\n");
		return NULL;
	}

	return cl.chrList.chr[actorIdx];
}

/**
 * @brief Returns the weapon its ammo and the firemodes-index inside the ammo for a given hand.
 * @param[in] actor The pointer to the actor we want to get the data from.
 * @param[in] hand Which weapon(-hand) to use [l|r].
 * @param[out] weapFdsIdx weapon_mod index in the ammo for the weapon (objDef.fd[x][])
 * @return the used @c fireDef_t
 */
const fireDef_t *CL_GetWeaponAndAmmo (const le_t * actor, const char hand)
{
	const invList_t *invlistWeapon;

	if (!actor)
		return NULL;

	if (hand == ACTOR_HAND_CHAR_RIGHT)
		invlistWeapon = RIGHT(actor);
	else
		invlistWeapon = LEFT(actor);

	if (!invlistWeapon || !invlistWeapon->item.t)
		return NULL;

	return FIRESH_FiredefForWeapon(&invlistWeapon->item);
}

#ifdef DEBUG
/**
 * @brief Prints all reaction- and reservation-info for the team.
 * @note Console command: debug_listreservations
 */
void CL_ListReactionAndReservations_f (void)
{
	int actorIdx;

	for (actorIdx = 0; actorIdx < cl.numTeamList; actorIdx++) {
		if (cl.teamList[actorIdx]) {
			const character_t *chr = CL_GetActorChr(cl.teamList[actorIdx]);
			Com_Printf("%s\n", chr->name);
			Com_Printf(" - hand: %i | fm: %i | weapon: %s\n",
				chr->RFmode.hand, chr->RFmode.fmIdx, chr->RFmode.weapon->id);
			Com_Printf(" - res... reaction: %i | crouch: %i\n",
				chr->reservedTus.reaction, chr->reservedTus.crouch);
		}
	}
}
#endif

/**
 * @param[in] hand Store the given hand.
 * @param[in] fireModeIndex Store the given firemode for this hand.
 * @param[in] weaponIndex Store the weapon-idx of the object in the hand (for faster access).
 */
void CL_CharacterSetRFMode (character_t *chr, int hand, int fireModeIndex, const objDef_t *weapon)
{
	chr->RFmode.hand = hand;
	chr->RFmode.fmIdx = fireModeIndex;
	chr->RFmode.weapon = weapon;
}

/**
 * @param[in] hand Store the given hand.
 * @param[in] fireModeIndex Store the given firemode for this hand.
 * @param[in] weaponIndex Store the weapon-idx of the object in the hand (for faster access).
 */
void CL_CharacterSetShotSettings (character_t *chr, int hand, int fireModeIndex, const objDef_t *weapon)
{
	chr->reservedTus.shotSettings.hand = hand;
	chr->reservedTus.shotSettings.fmIdx = fireModeIndex;
	chr->reservedTus.shotSettings.weapon = weapon;
}

/**
 * @brief Checks if the currently selected firemode is useable with the defined weapon.
 * @param[in] actor The actor to check the firemode for.
 * @param[in] reaction Use qtrue to check chr->RFmode or qfalse to check chr->reservedTus->shotSettings
 * @return qtrue if nothing has to be done.
 * @return qfalse if settings are outdated.
 */
qboolean CL_WorkingFiremode (const le_t * actor, qboolean reaction)
{
	const character_t *chr;
	const chrFiremodeSettings_t *fmSettings;
	const fireDef_t *fd;

	if (!actor) {
		Com_DPrintf(DEBUG_CLIENT, "CL_WorkingFiremode: No actor given! Abort.\n");
		return qtrue;
	}

	chr = CL_GetActorChr(actor);
	if (!chr) {
		Com_DPrintf(DEBUG_CLIENT, "CL_WorkingFiremode: No character found! Abort.\n");
		return qtrue;
	}

	if (reaction)
		fmSettings = &chr->RFmode;
	else
		fmSettings = &chr->reservedTus.shotSettings;

	if (!SANE_FIREMODE(fmSettings)) {
		/* Settings out of range or otherwise invalid - update needed. */
		return qfalse;
	}

	fd = CL_GetWeaponAndAmmo(actor, ACTOR_GET_HAND_CHAR(fmSettings->hand));
	if (fd == NULL)
		return qfalse;

	if (fd->obj->weapons[fd->weapFdsIdx] == fmSettings->weapon && fmSettings->fmIdx >= 0
	 && fmSettings->fmIdx < fd->obj->numFiredefs[fd->weapFdsIdx]) {
		/* Stored firemode settings up to date - nothing has to be changed */
		return qtrue;
	}

	/* Return "settings unusable" */
	return qfalse;
}

/**
 * @brief Returns the amount of reserved TUs for a certain type.
 * @param[in] le The actor to check.
 * @param[in] type The type to check. Use RES_ALL_ACTIVE to get all reserved TUs that are not "active" (e.g. RF is skipped if disabled). RES_ALL returns ALL of them, no matter what. See reservation_types_t for a list of options.
 * @return The reserved TUs for the given type.
 * @return -1 on error.
 */
int CL_ReservedTUs (const le_t * le, const reservation_types_t type)
{
	character_t *chr;
	int reservedReaction, reservedCrouch, reservedShot;

	if (!le) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ReservedTUs: No le_t given.\n");
		return -1;
	}

	chr = CL_GetActorChr(le);
	if (!chr) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ReservedTUs: No character found for le.\n");
		return -1;
	}

	reservedReaction = max(0, chr->reservedTus.reaction);
	reservedCrouch = max(0, chr->reservedTus.crouch);
	reservedShot = max(0, chr->reservedTus.shot);

	switch (type) {
	case RES_ALL:
		/* A summary of ALL TUs that are reserved. */
		return reservedReaction + reservedCrouch + reservedShot;
	case RES_ALL_ACTIVE: {
		/* A summary of ALL TUs that are reserved depending on their "status". */
		const int crouch = (chr->reservedTus.reserveCrouch) ? reservedCrouch : 0;
		/** @todo reserveReaction is not yet correct on the client side - at least not tested. */
		/* Only use reaction-value if we are have RF activated. */
		if ((le->state & STATE_REACTION))
			return reservedReaction + reservedShot + crouch;
		else
			return reservedShot + crouch;
	}
	case RES_REACTION:
		return reservedReaction;
	case RES_CROUCH:
		return reservedCrouch;
	case RES_SHOT:
		return reservedShot;
	default:
		Com_DPrintf(DEBUG_CLIENT, "CL_ReservedTUs: Bad type given: %i\n", type);
		return -1;
	}
}

/**
 * @brief Returns the amount of usable (overall-reserved) TUs for this actor.
 * @param[in] le The actor to check.
 * @return The remaining/usable TUs for this actor
 * @return -1 on error (this includes bad [very large] numbers stored in the struct).
 */
int CL_UsableTUs (const le_t * le)
{
	if (!le) {
		Com_DPrintf(DEBUG_CLIENT, "CL_UsableTUs: No le_t given.\n");
		return -1;
	}

	return le->TU - CL_ReservedTUs(le, RES_ALL_ACTIVE);
}

/**
 * @brief Returns the amount of usable "reaction fire" TUs for this actor (depends on active/inactive RF)
 * @param[in] le The actor to check.
 * @return The remaining/usable TUs for this actor
 * @return -1 on error (this includes bad [very large] numbers stored in the struct).
 * @todo Maybe only return "reaction" value if reaction-state is active? The value _should_ be 0, but one never knows :)
 */
int CL_UsableReactionTUs (const le_t * le)
{
	/* Get the amount of usable TUs depending on the state (i.e. is RF on or off?) */
	if (le->state & STATE_REACTION)
		/* CL_UsableTUs DOES NOT return the stored value for "reaction" here. */
		return CL_UsableTUs(le) + CL_ReservedTUs(le, RES_REACTION);
	else
		/* CL_UsableTUs DOES return the stored value for "reaction" here. */
		return CL_UsableTUs(le);
}


/**
 * @brief Replace the reserved TUs for a certain type.
 * @param[in] le The actor to change it for.
 * @param[in] type The reservation type to be changed (i.e be replaced).
 * @param[in] tus How many TUs to set.
 * @todo Make the "type" into enum
 */
void CL_ReserveTUs (const le_t * le, const reservation_types_t type, const int tus)
{
	character_t *chr;

	if (!le || tus < 0) {
		return;
	}

	chr = CL_GetActorChr(le);
	if (!chr)
		return;

	Com_DPrintf(DEBUG_CLIENT, "CL_ReserveTUs: Debug: Reservation type=%i, TUs=%i\n", type, tus);

	switch (type) {
	case RES_ALL:
	case RES_ALL_ACTIVE:
		Com_DPrintf(DEBUG_CLIENT, "CL_ReserveTUs: RES_ALL and RES_ALL_ACTIVE are not valid options.\n");
		return;
	case RES_REACTION:
		chr->reservedTus.reaction = tus;
		return;
	case RES_CROUCH:
		chr->reservedTus.crouch = tus;
		return;
	case RES_SHOT:
		chr->reservedTus.shot = tus;
		return;
	default:
		Com_DPrintf(DEBUG_CLIENT, "CL_ReserveTUs: Bad reservation type given: %i\n", type);
		return;
	}
}

/**
 * @brief Stores the given firedef index and object index for reaction fire and sends in over the network as well.
 * @param[in] actor The actor to update the firemode for.
 * @param[in] handidx Index of hand with item, which will be used for reactionfiR_ Possible hand indices: 0=right, 1=right, -1=undef
 * @param[in] fdIdx Index of firedefinition for an item in given hand.
 */
void CL_SetReactionFiremode (le_t * actor, const int handidx, const objDef_t *od, const int fdIdx)
{
	character_t *chr;
	int usableTusForRF = 0;

	if (cls.team != cl.actTeam) {	/**< Not our turn */
		/* This check is just here (additional to the one in HUD_DisplayFiremodes_f) in case a possible situation was missed. */
		Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: Function called on enemy/other turn.\n");
		return;
	}

	if (!actor) {
		Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: No actor given! Abort.\n");
		return;
	}

	usableTusForRF = CL_UsableReactionTUs(actor);

	if (handidx < -1 || handidx > ACTOR_HAND_LEFT) {
		Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: Bad hand index given. Abort.\n");
		return;
	}

	Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: actor:%i entnum:%i hand:%i fd:%i\n",
		CL_GetActorNumber(actor), actor->entnum, handidx, fdIdx);

	chr = CL_GetActorChr(actor);

	/* Store TUs needed by the selected firemode (if reaction-fire is enabled). Otherwise set it to 0. */
	if (od != NULL && fdIdx >= 0) {
		const fireDef_t *fd;

		/* Get 'ammo' (of weapon in defined hand) and index of firedefinitions in 'ammo'. */
		fd = CL_GetWeaponAndAmmo(actor, ACTOR_GET_HAND_CHAR(handidx));

		/* Reserve the TUs needed by the selected firemode (defined in the ammo). */
		if (fd) {
			if (chr->reservedTus.reserveReaction == STATE_REACTION_MANY) {
				Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: Reserving %i x %i = %i TUs for RF.\n",
					usableTusForRF / fd[fdIdx].time, fd[fdIdx].time, fd[fdIdx].time * (usableTusForRF / fd[fdIdx].time));
				CL_ReserveTUs(actor, RES_REACTION, fd[fdIdx].time * (usableTusForRF / fd[fdIdx].time));
			} else {
				Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: Reserving %i TUs for RF.\n", fd[fdIdx].time);
				CL_ReserveTUs(actor, RES_REACTION, fd[fdIdx].time);
			}
		} else {
			Com_DPrintf(DEBUG_CLIENT, "CL_SetReactionFiremode: No firedef found! No TUs will be reserved.\n");
		}
	}

	CL_CharacterSetRFMode(chr, handidx, fdIdx, od);
	/* Send RFmode[] to server-side storage as well. See g_local.h for more. */
	MSG_Write_PA(PA_REACT_SELECT, actor->entnum, handidx, fdIdx, od ? od->idx : NONE);
	/* Update server-side settings */
	MSG_Write_PA(PA_RESERVE_STATE, actor->entnum, RES_REACTION, chr->reservedTus.reserveReaction, chr->reservedTus.reaction);
}

/**
 * @brief Checks if there is a weapon in the hand that can be used for reaction fire.
 * @param[in] actor What actor to check.
 * @param[in] hand Which hand to check: 'l' for left hand, 'r' for right hand.
 */
qboolean CL_WeaponWithReaction (const le_t * actor, const char hand)
{
	int i;
	const fireDef_t *fd;

	/* Get ammo and weapon-index in ammo (if there is a weapon in that hand). */
	fd = CL_GetWeaponAndAmmo(actor, hand);

	if (fd == NULL)
		return qfalse;

	/* Check ammo for reaction-enabled firemodes. */
	for (i = 0; i < fd->obj->numFiredefs[fd->weapFdsIdx]; i++)
		if (fd[i].reaction)
			return qtrue;

	return qfalse;
}

/**
 * @brief Updates the information in RFmode for the selected actor with the given data from the parameters.
 * @param[in] actor The actor we want to update the reaction-fire firemode for.
 * @param[in] hand Which weapon(-hand) to use (l|r).
 * @param[in] firemodeActive Set this to the firemode index you want to activate or set it to -1 if the default one (currently the first one found) should be used.
 */
void CL_UpdateReactionFiremodes (le_t * actor, const char hand, int firemodeActive)
{
	const fireDef_t *fd;
	character_t *chr;
	const int handidx = ACTOR_GET_HAND_INDEX(hand);
	const objDef_t *ammo;

	if (!actor) {
		Com_DPrintf(DEBUG_CLIENT, "CL_UpdateReactionFiremodes: No actor given!\n");
		return;
	}

	fd = CL_GetWeaponAndAmmo(actor, hand);
	if (fd == NULL) {
		CL_DisplayImpossibleReaction(actor);
		return;
	}

	ammo = fd->obj;

	if (!GAME_ItemIsUseable(ammo->weapons[fd->weapFdsIdx])) {
		Com_DPrintf(DEBUG_CLIENT, "CL_UpdateReactionFiremodes: Weapon '%s' not useable in current gamemode, can't use for reaction fire.\n",
			ammo->weapons[fd->weapFdsIdx]->id);
		return;
	}

	if (firemodeActive >= MAX_FIREDEFS_PER_WEAPON) {
		Com_Printf("CL_UpdateReactionFiremodes: Firemode index to big (%i). Highest possible number is %i.\n",
			firemodeActive, MAX_FIREDEFS_PER_WEAPON - 1);
		return;
	}

	if (firemodeActive < 0) {
		/* Set default reaction firemode for this hand (firemodeActive=-1) */
		const int reactionFiremodeIndex = FIRESH_GetDefaultReactionFire(ammo, fd->weapFdsIdx);

		if (reactionFiremodeIndex >= 0) {
			/* Found usable firemode for the weapon in _this_ hand. */
			CL_SetReactionFiremode(actor, handidx, ammo->weapons[fd->weapFdsIdx], reactionFiremodeIndex);

			if (CL_UsableReactionTUs(actor) >= ammo->fd[fd->weapFdsIdx][reactionFiremodeIndex].time) {
				/* Display 'usable" (blue) reaction buttons */
				HUD_DisplayPossibleReaction(actor);
			} else {
				/* Display "impossible" (red) reaction button. */
				CL_DisplayImpossibleReaction(actor);
			}
		} else {
			/* Weapon in _this_ hand not RF-capable. */
			if (CL_WeaponWithReaction(actor, ACTOR_SWAP_HAND(hand))) {
				/* The _other_ hand has usable firemodes for RF, use it instead. */
				CL_UpdateReactionFiremodes(actor, ACTOR_SWAP_HAND(hand), -1);
			} else {
				/* No RF-capable item in either hand. */

				/* Display "impossible" (red) reaction button. */
				CL_DisplayImpossibleReaction(actor);
				/* Set RF-mode info to undef. */
				CL_SetReactionFiremode(actor, -1, NULL, -1);
				CL_ReserveTUs(actor, RES_REACTION, 0);
			}
		}
		/* The rest of this function assumes that firemodeActive is bigger than -1 -> finish. */
		return;
	}

	chr = CL_GetActorChr(actor);
	assert(chr);

	Com_DPrintf(DEBUG_CLIENT, "CL_UpdateReactionFiremodes: act%s handidx%i weapfdidx%i\n",
		chr->name, handidx, fd->weapFdsIdx);

	if (chr->RFmode.weapon == ammo->weapons[fd->weapFdsIdx] && chr->RFmode.hand == handidx) {
		if (ammo->fd[fd->weapFdsIdx][firemodeActive].reaction) {
			if (chr->RFmode.fmIdx == firemodeActive)
				/* Weapon is the same, firemode is already selected and reaction-usable. Nothing to do. */
				return;
		} else {
			/* Weapon is the same and firemode is not reaction-usable*/
			return;
		}
	}

	/* Search for a (reaction) firemode with the given index and store/send it. */
	if (ammo->fd[fd->weapFdsIdx][firemodeActive].reaction) {
		/* Get the amount of usable TUs depending on the state (i.e. is RF on or off?) and abort if no use*/
		if (CL_UsableReactionTUs(actor) >= ammo->fd[fd->weapFdsIdx][firemodeActive].time)
			CL_SetReactionFiremode(actor, handidx, ammo->weapons[fd->weapFdsIdx], firemodeActive);
	}
}

/**
 * @brief Sets the reaction-firemode of an actor/soldier to it's default value on client- and server-side.
 * @param[in] actor The actor to set the firemode for.
 * @param[in] hand Which weapon(-hand) to try first for reaction-firemode (r|l).
 */
void CL_SetDefaultReactionFiremode (le_t *actor, const char hand)
{
	if (!actor) {
		Com_DPrintf(DEBUG_CLIENT, "CL_SetDefaultReactionFiremode: No actor given! Abort.\n");
		return;
	}

	/* Set default firemode */
	CL_UpdateReactionFiremodes(actor, hand, -1);
	if (!CL_WorkingFiremode(actor, qtrue))
		/* If that failed try to set the other hand. */
		CL_UpdateReactionFiremodes(actor, ACTOR_SWAP_HAND(hand), -1);
}

/*
==============================================================
ACTOR SELECTION AND TEAM LIST
==============================================================
*/

/**
 * @brief Adds the actor the the team list.
 * @sa CL_ActorAppear
 * @sa CL_RemoveActorFromTeamList
 * @param le Pointer to local entity struct
 */
void CL_AddActorToTeamList (le_t * le)
{
	int i;

	/* test team */
	if (!le || le->team != cls.team || le->pnum != cl.pnum || LE_IsDead(le))
		return;

	/* check list length */
	if (cl.numTeamList >= MAX_TEAMLIST)
		return;

	/* check list for that actor */
	i = CL_GetActorNumber(le);

	/* add it */
	if (i == -1) {
		i = cl.numTeamList;
		le->pathMap = Mem_PoolAlloc(sizeof(*le->pathMap), cl_genericPool, 0);
		cl.teamList[cl.numTeamList++] = le;
		MN_ExecuteConfunc("numonteam%i", cl.numTeamList); /* althud */
		MN_ExecuteConfunc("huddeselect %i", i);
		if (cl.numTeamList == 1)
			CL_ActorSelectList(0);
	}
}


/**
 * @brief Removes an actor from the team list.
 * @sa CL_ActorStateChange
 * @sa CL_AddActorToTeamList
 * @param le Pointer to local entity struct
 */
void CL_RemoveActorFromTeamList (const le_t * le)
{
	int i;

	if (!le)
		return;

	for (i = 0; i < cl.numTeamList; i++) {
		if (cl.teamList[i] == le) {
			Mem_Free(le->pathMap);

			/* disable hud button */
			MN_ExecuteConfunc("huddisable %i", i);

			/* remove from list */
			cl.teamList[i] = NULL;
			break;
		}
	}

	/* check selection */
	if (selActor == le) {
		CL_ConditionalMoveCalcForCurrentSelectedActor();

		for (i = 0; i < cl.numTeamList; i++) {
			if (cl.teamList[i] && CL_ActorSelect(cl.teamList[i]))
				break;
		}

		if (i == cl.numTeamList) {
			selActor->selected = qfalse;
			CL_ActorSelect(NULL);
		}
	}
}

/**
 * @brief Selects an actor.
 * @param le Pointer to local entity struct. If this is @c NULL the menuInventory that is linked from the actors
 * @sa CL_UGVCvars
 * @sa CL_CharacterCvars
 */
qboolean CL_ActorSelect (le_t * le)
{
	int actorIdx;
	qboolean sameActor = qfalse;

	/* test team */
	if (!le) {
		if (selActor)
			selActor->selected = qfalse;
		selActor = NULL;
		menuInventory = NULL;
		return qfalse;
	}

	if (le->team != cls.team || LE_IsDead(le) || !le->inuse)
		return qfalse;

	/* select him */
	if (selActor)
		selActor->selected = qfalse;
	le->selected = qtrue;

	/* reset the align if we switched the actor */
	if (selActor != le)
		mousePosTargettingAlign = 0;
	else
		sameActor = qtrue;

	selActor = le;
	menuInventory = &selActor->i;

	actorIdx = CL_GetActorNumber(le);
	if (actorIdx < 0)
		return qfalse;

	/* console commands, update cvars */
	Cvar_ForceSet("cl_selected", va("%i", actorIdx));

	selChr = CL_GetActorChr(le);
	assert(selChr);

	/* Right now we can only update this if the selChr is already set. */
	switch (le->fieldSize) {
	case ACTOR_SIZE_NORMAL:
		CL_CharacterCvars(selChr);
		break;
	case ACTOR_SIZE_2x2:
		CL_UGVCvars(selChr);
		break;
	default:
		Com_Error(ERR_DROP, "CL_ActorSelect: Unknown fieldsize");
	}

	/* Forcing the hud-display to refresh its displayed stuff. */
	/** @todo make this a function parameter to HUD_ActorUpdateCvars ? */
	Cvar_SetValue("hud_refresh", 1);
	/** @todo remove this here */
	HUD_UpdateSelectedActorReactionState();
	HUD_ActorUpdateCvars();

	CL_ConditionalMoveCalcForCurrentSelectedActor();

	/* Change to move-mode and hide firemodes.
	 * Only if it's a different actor - if it's the same we keep the current mode etc... */
	if (!sameActor) {
		HUD_HideFiremodes(); /**< @todo remove this here */
		le->actorMode = M_MOVE;
	}

	return qtrue;
}

/**
 * @brief Selects an actor from a list.
 *
 * This function is used to select an actor from the lists that are
 * used in equipment and team assemble screens
 *
 * @param num The index value from the list of actors
 *
 * @sa CL_ActorSelect
 * @return qtrue if selection was possible otherwise qfalse
 */
qboolean CL_ActorSelectList (int num)
{
	le_t *le;

	/* check if actor exists */
	if (num >= cl.numTeamList || num < 0)
		return qfalse;

	/* select actor */
	le = cl.teamList[num];
	if (!le || !CL_ActorSelect(le))
		return qfalse;

	/* center view (if wanted) */
	if (cl_centerview->integer)
		VectorCopy(le->origin, cl.cam.origin);
	/* change to worldlevel were actor is right now */
	Cvar_SetValue("cl_worldlevel", le->pos[2]);

	return qtrue;
}

/**
 * @brief selects the next actor
 */
qboolean CL_ActorSelectNext (void)
{
	int selIndex = -1;
	int num = cl.numTeamList;
	int i;

	/* find index of currently selected actor */
	for (i = 0; i < num; i++) {
		const le_t *le = cl.teamList[i];
		if (le && le->selected && le->inuse && !LE_IsDead(le)) {
			selIndex = i;
			break;
		}
	}
	if (selIndex < 0)
		return qfalse;			/* no one selected? */

	/* cycle round */
	i = selIndex;
	while (qtrue) {
		i = (i + 1) % num;
		if (i == selIndex)
			break;
		if (CL_ActorSelectList(i))
			return qtrue;
	}
	return qfalse;
}


/*
==============================================================
ACTOR MOVEMENT AND SHOOTING
==============================================================
*/

/**
 * @brief A list of locations that cannot be moved to.
 * @note Pointer to le->pos or edict->pos followed by le->fieldSize or edict->fieldSize
 * @see CL_BuildForbiddenList
 */
pos_t *fb_list[MAX_FORBIDDENLIST];
/**
 * @brief Current length of fb_list.
 * @note all byte pointers in the fb_list list (pos + fieldSize)
 * @see fb_list
 */
int fb_length;

/**
 * @brief Builds a list of locations that cannot be moved to (client side).
 * @sa G_MoveCalc
 * @sa G_BuildForbiddenList <- server side
 * @sa Grid_CheckForbidden
 * @note This is used for pathfinding.
 * It is a list of where the selected unit can not move to because others are standing there already.
 */
static void CL_BuildForbiddenList (void)
{
	le_t *le;
	int i;

	fb_length = 0;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse || le->invis)
			continue;
		/* Dead ugv will stop walking, too. */
		if (le->type == ET_ACTOR2x2 || LE_IsLivingAndVisibleActor(le)) {
			fb_list[fb_length++] = le->pos;
			fb_list[fb_length++] = (byte*)&le->fieldSize;
		}
	}

#ifdef PARANOID
	if (fb_length > MAX_FORBIDDENLIST)
		Com_Error(ERR_DROP, "CL_BuildForbiddenList: list too long");
#endif
}

#ifdef DEBUG
/**
 * @brief Draws a marker for all blocked map-positions.
 * @note currently uses basically the same code as CL_BuildForbiddenList
 * @note usage in console: "debug_drawblocked"
 * @todo currently the particles stay a _very_ long time ... so everybody has to stand still in order for the display to be correct.
 * @sa CL_BuildForbiddenList
 */
void CL_DisplayBlockedPaths_f (void)
{
	le_t *le;
	int i, j;
	ptl_t *ptl;
	vec3_t s;

	for (i = 0, le = LEs; i < numLEs; i++, le++) {
		if (!le->inuse)
			continue;

		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			/* draw blocking cursor at le->pos */
			if (!LE_IsDead(le))
				Grid_PosToVec(clMap, le->fieldSize, le->pos, s);
			break;
		case ET_DOOR:
		case ET_BREAKABLE:
		case ET_ROTATING:
			VectorCopy(le->origin, s);
			break;
		default:
			continue;
		}

		ptl = CL_ParticleSpawn("blocked_field", 0, s, NULL, NULL);
		ptl->rounds = 2;
		ptl->roundsCnt = 2;
		ptl->life = 10000;
		ptl->t = 0;
		if (le->fieldSize == ACTOR_SIZE_2x2) {
			/* If this actor blocks 4 fields draw them as well. */
			for (j = 0; j < 3; j++) {
				ptl_t *ptl2 = CL_ParticleSpawn("blocked_field", 0, s, NULL, NULL);
				ptl2->rounds = ptl->rounds;
				ptl2->roundsCnt = ptl->roundsCnt;
				ptl2->life = ptl->life;
				ptl2->t = ptl->t;
			}
		}
	}
}
#endif

/**
 * @brief Recalculate forbidden list, available moves and actor's move length
 * for the current selected actor.
 */
void CL_ConditionalMoveCalcForCurrentSelectedActor (void)
{
	if (selActor) {
		const int crouchingState = (selActor->state & STATE_CROUCHED) ? 1 : 0;
		CL_BuildForbiddenList();
		Grid_MoveCalc(clMap, selActor->fieldSize, selActor->pathMap, selActor->pos, crouchingState, MAX_ROUTE, fb_list, fb_length);
		CL_ResetActorMoveLength();
	}
}

/**
 * @brief Checks that an action is valid.
 */
int CL_CheckAction (void)
{
	if (!selActor)
		return qfalse;

	/* already moving */
	if (selActor->pathLength)
		return qfalse;

	if (cls.team != cl.actTeam) {
		HUD_DisplayMessage(_("This isn't your round\n"));
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Get the real move length (depends on crouch-state of the current actor).
 * @note The part of the line that is not reachable in this turn (i.e. not enough
 * TUs left) will be drawn differently.
 * @param[in] to The position in the map to calculate the move-length for.
 * @return The amount of TUs that are needed to walk to the given grid position
 */
static int CL_MoveLength (const le_t *le, pos3_t to)
{
	const int crouchingState = le->state & STATE_CROUCHED ? 1 : 0;
	const float length = Grid_MoveLength(le->pathMap, to, crouchingState, qfalse);

	switch (CL_MoveMode(le, length)) {
	case WALKTYPE_AUTOSTAND_BEING_USED:
		return length /* + 2 * TU_CROUCH */;
	case WALKTYPE_AUTOSTAND_BUT_NOT_FAR_ENOUGH:
	case WALKTYPE_CROUCH_WALKING:
		return length /* * TU_CROUCH_MOVING_FACTOR */;
	default:
		Com_DPrintf(DEBUG_CLIENT,"CL_MoveLength: MoveMode not recognised.\n");
	case WALKTYPE_WALKING:
		return length;
	}
}

/**
 * @brief Recalculates the currently selected Actor's move length.
 */
void CL_ResetActorMoveLength (void)
{
	assert(selActor);
	actorMoveLength = CL_MoveLength(selActor, mousePos);
}

/**
 * @brief Draws the way to walk when confirm actions is activated.
 * @param[in] to The location we draw the line to (starting with the location of selActor)
 * @return qtrue if everything went ok, otherwise qfalse.
 * @sa CL_MaximumMove (similar algo.)
 */
static qboolean CL_TraceMove (pos3_t to)
{
	int length;
	vec3_t vec, oldVec;
	pos3_t pos;
	int dv;
	int crouching_state;
#ifdef DEBUG
	int counter = 0;
#endif

	if (!selActor)
		return qfalse;

	length = CL_MoveLength(selActor, to);
	if (!length || length >= 0x3F)
		return qfalse;

	crouching_state = selActor->state & STATE_CROUCHED ? 1 : 0;

	Grid_PosToVec(clMap, selActor->fieldSize, to, oldVec);
	VectorCopy(to, pos);

	Com_DPrintf(DEBUG_PATHING, "Starting pos: (%i, %i, %i).\n", pos[0], pos[1], pos[2]);

	while ((dv = Grid_MoveNext(clMap, selActor->fieldSize, selActor->pathMap, pos, crouching_state)) != ROUTING_UNREACHABLE) {
#ifdef DEBUG
		if (++counter > 100) {
			Com_Printf("First pos: (%i, %i, %i, %i).\n", to[0], to[1], to[2], selActor->state & STATE_CROUCHED ? 1 : 0);
			Com_Printf("Last pos: (%i, %i, %i, %i).\n", pos[0], pos[1], pos[2], crouching_state);
			Grid_DumpDVTable(selActor->pathMap);
			Com_Error(ERR_DROP, "CL_TraceMove: DV table loops.");
		}
#endif
		length = CL_MoveLength(selActor, pos);
		PosSubDV(pos, crouching_state, dv); /* We are going backwards to the origin. */
		Com_DPrintf(DEBUG_PATHING, "Next pos: (%i, %i, %i, %i) [%i].\n", pos[0], pos[1], pos[2], crouching_state, dv);
		Grid_PosToVec(clMap, selActor->fieldSize, pos, vec);
		if (length > CL_UsableTUs(selActor))
			CL_ParticleSpawn("longRangeTracer", 0, vec, oldVec, NULL);
		else if (crouching_state)
			CL_ParticleSpawn("crawlTracer", 0, vec, oldVec, NULL);
		else
			CL_ParticleSpawn("moveTracer", 0, vec, oldVec, NULL);
		VectorCopy(vec, oldVec);
	}
	return qtrue;
}

/**
 * @brief Return the last position we can walk to with a defined amount of TUs.
 * @param[in] to The location we want to reach.
 * @param[in] tus How many timeunits we have to move.
 * @param[out] pos The location we can reach with the given amount of TUs.
 * @sa CL_TraceMove (similar algo.)
 */
static void CL_MaximumMove (pos3_t to, const le_t *le, pos3_t pos)
{
	int length;
	int dv;
	int crouchingState = le && (le->state & STATE_CROUCHED) ? 1 : 0;
	const int tus = CL_UsableTUs(le);
	/* vec3_t vec; */

	length = CL_MoveLength(le, to);
	if (!length || length >= 0x3F)
		return;

	VectorCopy(to, pos);

	while ((dv = Grid_MoveNext(clMap, le->fieldSize, le->pathMap, pos, crouchingState)) != ROUTING_UNREACHABLE) {
		length = CL_MoveLength(le, pos);
		if (length <= tus)
			return;
		PosSubDV(pos, crouchingState, dv); /* We are going backwards to the origin. */
		/* Grid_PosToVec(clMap, le->fieldSize, pos, vec); */
	}
}


/**
 * @brief Starts moving actor.
 * @param[in] le
 * @param[in] to
 * @sa CL_ActorActionMouse
 * @sa CL_ActorSelectMouse
 */
void CL_ActorStartMove (le_t * le, pos3_t to)
{
	int length;
	pos3_t toReal;

	if (mouseSpace != MS_WORLD)
		return;

	if (!CL_CheckAction())
		return;

	length = CL_MoveLength(le, to);

	if (!length || length >= ROUTING_NOT_REACHABLE) {
		/* move not valid, don't even care to send */
		return;
	}

	/* Get the last position we can walk to with the usable TUs. */
	CL_MaximumMove(to, le, toReal);

	/* Get the cost of the new position just in case. */
	length = CL_MoveLength(le, toReal);

	if (CL_UsableTUs(le) < length) {
		/* We do not have enough _usable_ TUs to move so don't even try to send. */
		/* This includes a check for reserved TUs (which isn't done on the server!) */
		return;
	}

	/* change mode to move now */
	le->actorMode = M_MOVE;

	/* move seems to be possible; send request to server */
	MSG_Write_PA(PA_MOVE, le->entnum, toReal);
}


/**
 * @brief Shoot with actor.
 * @param[in] le Who is shooting
 * @param[in] at Position you are targeting to
 */
void CL_ActorShoot (const le_t * le, pos3_t at)
{
	int type;

	if (mouseSpace != MS_WORLD)
		return;

	if (!CL_CheckAction())
		return;

	Com_DPrintf(DEBUG_CLIENT, "CL_ActorShoot: cl.firemode %i.\n", le->currentSelectedFiremode);

	/** @todo Is there a better way to do this?
	 * This type value will travel until it is checked in at least g_combat.c:G_GetShotFromType.
	 */
	if (IS_MODE_FIRE_RIGHT(le->actorMode)) {
		type = ST_RIGHT;
	} else if (IS_MODE_FIRE_LEFT(le->actorMode)) {
		type = ST_LEFT;
	} else if (IS_MODE_FIRE_HEADGEAR(le->actorMode)) {
		type = ST_HEADGEAR;
	} else
		return;

	MSG_Write_PA(PA_SHOOT, le->entnum, at, type, le->currentSelectedFiremode, mousePosTargettingAlign);
}


/**
 * @brief Reload weapon with actor.
 * @param[in] hand
 * @sa CL_CheckAction
 */
void CL_ActorReload (int hand)
{
	inventory_t *inv;
	invList_t *ic;
	objDef_t *weapon;
	int x, y, tu;
	int container, bestContainer;

	if (!CL_CheckAction())
		return;

	/* check weapon */
	inv = &selActor->i;

	/* search for clips and select the one that is available easily */
	x = 0;
	y = 0;
	tu = 100;
	bestContainer = NONE;

	if (inv->c[hand]) {
		weapon = inv->c[hand]->item.t;
	} else if (hand == csi.idLeft && inv->c[csi.idRight]->item.t->holdTwoHanded) {
		/* Check for two-handed weapon */
		hand = csi.idRight;
		weapon = inv->c[hand]->item.t;
	} else
		/* otherwise we could use weapon uninitialized */
		return;

	if (!weapon)
		return;

	/* return if the weapon is not reloadable */
	if (!weapon->reload)
		return;

	if (!GAME_ItemIsUseable(weapon)) {
		HUD_DisplayMessage(_("You cannot reload this unknown item.\n"));
		return;
	}

	for (container = 0; container < csi.numIDs; container++) {
		if (csi.ids[container].out < tu) {
			/* Once we've found at least one clip, there's no point
			 * searching other containers if it would take longer
			 * to retrieve the ammo from them than the one
			 * we've already found. */
			for (ic = inv->c[container]; ic; ic = ic->next)
				if (INVSH_LoadableInWeapon(ic->item.t, weapon)
				 && GAME_ItemIsUseable(ic->item.t)) {
					Com_GetFirstShapePosition(ic, &x, &y);
					x += ic->x;
					y += ic->y;
					tu = csi.ids[container].out;
					bestContainer = container;
					break;
				}
		}
	}

	/* send request */
	if (bestContainer != NONE)
		MSG_Write_PA(PA_INVMOVE, selActor->entnum, bestContainer, x, y, hand, 0, 0);
	else
		Com_Printf("No (researched) clip left.\n");
}

/**
 * @brief Opens a door.
 * @sa CL_ActorDoorAction
 * @sa G_ClientUseEdict
 */
void CL_ActorUseDoor (void)
{
	if (!CL_CheckAction())
		return;

	assert(selActor->clientAction);

	MSG_Write_PA(PA_USE_DOOR, selActor->entnum, selActor->clientAction);
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorUseDoor: Use door number: %i (actor %i)\n", selActor->clientAction, selActor->entnum);
}

/**
 * @brief Reads the door entity number for client interaction
 * @sa EV_DOOR_ACTION
 * @sa Touch_DoorTrigger
 * @sa CL_ActorUseDoor
 */
void CL_ActorDoorAction (struct dbuffer *msg)
{
	le_t* le;
	int number, doornumber;

	/* read data */
	NET_ReadFormat(msg, ev_format[EV_DOOR_ACTION], &number, &doornumber);

	/* get actor le */
	le = LE_Get(number);
	if (!le) {
		Com_Printf("CL_ActorDoorAction: Could not get le %i\n", number);
		return;
	}
	/* set door number */
	le->clientAction = doornumber;
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorDoorAction: Set door number: %i (for actor with entnum %i)\n", doornumber, number);
}

/**
 * @brief Hud callback to open/close a door
 */
void CL_ActorDoorAction_f (void)
{
	if (!CL_CheckAction())
		return;

	/* no client action */
	if (selActor->clientAction == 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorDoorAction_f: No client_action set for actor with entnum %i\n", selActor->entnum);
		return;
	}

	/* Check if we should even try to send this command (no TUs left or). */
	if (CL_UsableTUs(selActor) >= TU_DOOR_ACTION)
		CL_ActorUseDoor();
}

/**
 * @brief When no trigger is touched, the client actions are resetted
 * @sa EV_RESET_CLIENT_ACTION
 * @sa G_ClientMove
 */
void CL_ActorResetClientAction (struct dbuffer *msg)
{
	le_t* le;
	int number;

	/* read data */
	NET_ReadFormat(msg, ev_format[EV_RESET_CLIENT_ACTION], &number);

	/* get actor le */
	le = LE_Get(number);
	if (!le) {
		Com_Printf("CL_ActorResetClientAction: Could not get le %i\n", number);
		return;
	}
	/* set door number */
	le->clientAction = 0;
	Com_DPrintf(DEBUG_CLIENT, "CL_ActorResetClientAction: Reset client action for actor with entnum %i\n", number);
}

/**
 * @brief The client changed something in his hand-containers. This function updates the reactionfire info.
 * @param[in] msg The netchannel message
 */
void CL_InvCheckHands (struct dbuffer *msg)
{
	int entnum;
	le_t *le;
	int actorIdx = -1;
	int hand = -1;		/**< 0=right, 1=left -1=undef*/

	NET_ReadFormat(msg, ev_format[EV_INV_HANDS_CHANGED], &entnum, &hand);
	if (entnum < 0 || hand < 0) {
		Com_Printf("CL_InvCheckHands: entnum or hand not sent/received correctly. (number: %i)\n", entnum);
	}

	le = LE_Get(entnum);
	if (!le) {
		Com_Printf("CL_InvCheckHands: LE doesn't exist. (number: %i)\n", entnum);
		return;
	}

	actorIdx = CL_GetActorNumber(le);
	if (actorIdx == -1) {
		Com_DPrintf(DEBUG_CLIENT, "CL_InvCheckHands: Could not get local entity actor id via CL_GetActorNumber\n");
		Com_DPrintf(DEBUG_CLIENT, "CL_InvCheckHands: DEBUG actor info: team=%i(%s) type=%i inuse=%i\n",
			le->team, le->teamDef ? le->teamDef->name : "No team", le->type, le->inuse);
		return;
	}

	/* No need to continue if stored firemode settings are still usable. */
	if (!CL_WorkingFiremode(le, qtrue)) {
		/* Firemode for reaction not sane and/or not usable. */
		/* Update the changed hand with default firemode. */
		if (hand == ACTOR_HAND_RIGHT) {
			Com_DPrintf(DEBUG_CLIENT, "CL_InvCheckHands: DEBUG right\n");
			CL_UpdateReactionFiremodes(le, ACTOR_HAND_CHAR_RIGHT, -1);
		} else {
			Com_DPrintf(DEBUG_CLIENT, "CL_InvCheckHands: DEBUG left\n");
			CL_UpdateReactionFiremodes(le, ACTOR_HAND_CHAR_LEFT, -1);
		}
		HUD_HideFiremodes();
	}
}

/**
 * @brief Moves actor.
 * @param[in] msg The netchannel message
 * @sa LET_PathMove
 * @note EV_ACTOR_MOVE
 */
void CL_ActorDoMove (struct dbuffer *msg)
{
	le_t *le;
	int number, i, pathLength;

	number = NET_ReadShort(msg);
	/* get le */
	le = LE_Get(number);
	if (!le) {
		Com_Printf("CL_ActorDoMove: Could not get LE with id %i\n", number);
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("Can't move, LE doesn't exist or is not an actor (number: %i, type: %i)\n",
			number, le ? le->type : -1);
		return;
	}

	if (LE_IsDead(le)) {
		Com_Printf("Can't move, actor dead\n");
		return;
	}

	pathLength = NET_ReadByte(msg);
	if (le->pathLength + pathLength >= MAX_LE_PATHLENGTH)
		Com_Error(ERR_DROP, "Overflow in pathLength");

	/* Also get the final position */
	le->newPos[0] = NET_ReadByte(msg);
	le->newPos[1] = NET_ReadByte(msg);
	le->newPos[2] = NET_ReadByte(msg);

	for (i = le->pathLength; i < le->pathLength + pathLength; i++) {
		le->path[i] = NET_ReadByte(msg); /** Don't adjust dv values here- the whole thing is needed to move the actor! */
		le->speed = NET_ReadShort(msg);
		le->pathContents[i] = NET_ReadShort(msg);
	}

	/* we are adding to already existing pathLength because a move event can be interrupted by
	 * an perish or appear event (or any other event) on the server side. This results in two events
	 * where the player would just like to walk on path */
	le->pathLength += pathLength;

	/* activate PathMove function */
	FLOOR(le) = NULL;
	le->think = LET_StartPathMove;
	le->pathPos = 0;
	le->startTime = cl.time;
	le->endTime = cl.time;
}


/**
 * @brief Turns the actor around without moving
 */
void CL_ActorTurnMouse (void)
{
	vec3_t div;
	byte dv;

	if (mouseSpace != MS_WORLD)
		return;

	if (!CL_CheckAction())
		return;

	if (CL_UsableTUs(selActor) < TU_TURN) {
		/* Cannot turn because of not enough usable TUs. */
		return;
	}

	/* check for fire-modes, and cancel them */
	switch (selActor->actorMode) {
	case M_FIRE_R:
	case M_FIRE_L:
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		selActor->actorMode = M_MOVE;
		return; /* and return without turning */
	default:
		break;
	}

	/* calculate dv */
	VectorSubtract(mousePos, selActor->pos, div);
	dv = AngleToDV((int) (atan2(div[1], div[0]) * todeg));

	/* send message to server */
	MSG_Write_PA(PA_TURN, selActor->entnum, dv);
}


/**
 * @brief Turns actor.
 * @param[in] msg The netchannel message
 */
void CL_ActorDoTurn (struct dbuffer *msg)
{
	le_t *le;
	int entnum, dir;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_TURN], &entnum, &dir);

	/* get le */
	le = LE_Get(entnum);
	if (!le) {
		Com_Printf("CL_ActorDoTurn: Could not get LE with id %i\n", entnum);
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("Can't turn, LE doesn't exist or is not an actor (number: %i, type: %i)\n", entnum, le ? le->type : -1);
		return;
	}

	if (LE_IsDead(le)) {
		Com_Printf("Can't turn, actor dead\n");
		return;
	}

	le->dir = dir;
	le->angles[YAW] = directionAngles[le->dir];
}


/**
 * @brief Stands or crouches actor.
 * @todo Maybe add a popup that asks if the player _really_ wants to crouch/stand up when only the reserved amount is left?
 */
void CL_ActorStandCrouch_f (void)
{
	if (!CL_CheckAction())
		return;

	if (selActor->fieldSize == ACTOR_SIZE_2x2)
		/** @todo future thoughts: maybe define this in team_*.ufo files instead? */
		return;

	/* Check if we should even try to send this command (no TUs left or). */
	if (CL_UsableTUs(selActor) >= TU_CROUCH || CL_ReservedTUs(selActor, RES_CROUCH) >= TU_CROUCH) {
		/* send a request to toggle crouch to the server */
		MSG_Write_PA(PA_STATE, selActor->entnum, STATE_CROUCHED);
	}
}

/**
 * @brief Toggles the headgear for the current selected player
 */
void CL_ActorUseHeadgear_f (void)
{
	invList_t* headgear;
	const int tmp_mouseSpace = mouseSpace;

	/* this can be executed by a click on a hud button
	 * but we need MS_WORLD mouse space to let the shooting
	 * function work */
	mouseSpace = MS_WORLD;

	if (!CL_CheckAction())
		return;

	headgear = HEADGEAR(selActor);
	if (!headgear)
		return;

	selActor->actorMode = M_FIRE_HEADGEAR;
	selActor->currentSelectedFiremode = 0; /** @todo make this a variable somewhere? */
	CL_ActorShoot(selActor, selActor->pos);
	selActor->actorMode = M_MOVE;

	/* restore old mouse space */
	mouseSpace = tmp_mouseSpace;
}

/**
 * @brief Spawns particle effects for a hit actor.
 * @param[in] le The actor to spawn the particles for.
 * @param[in] impact The impact location (where the particles are spawned).
 * @param[in] normal The index of the normal vector of the particles (think: impact angle).
 * @todo Get real impact location and direction?
 */
static void CL_ActorHit (const le_t * le, vec3_t impact, int normal)
{
	if (!le) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorHit: Can't spawn particles, LE doesn't exist\n");
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("CL_ActorHit: Can't spawn particles, LE is not an actor (type: %i)\n", le->type);
		return;
	}

	if (le->teamDef) {
		/* Spawn "hit_particle" if defined in teamDef. */
		if (le->teamDef->hitParticle[0] != '\0')
			CL_ParticleSpawn(le->teamDef->hitParticle, 0, impact, bytedirs[normal], NULL);
	}
}

/**
 * @brief Records if shot is first shot.
 */
static qboolean firstShot = qfalse;

/**
 * @brief Shoot with weapon.
 * @sa CL_ActorShoot
 * @sa CL_ActorShootHidden
 * @todo Improve detection of left- or right animation.
 * @sa EV_ACTOR_SHOOT
 */
void CL_ActorDoShoot (struct dbuffer *msg)
{
	const fireDef_t *fd;
	le_t *le;
	vec3_t muzzle, impact;
	int flags, normal, entnum;
	int objIdx;
	const objDef_t *obj;
	int weapFdsIdx, fdIdx, surfaceFlags, shootType;

	/* read data */
	NET_ReadFormat(msg, ev_format[EV_ACTOR_SHOOT], &entnum, &objIdx, &weapFdsIdx, &fdIdx, &shootType, &flags, &surfaceFlags, &muzzle, &impact, &normal);

	/* get le */
	le = LE_Get(entnum);

	/* get the fire def */
	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* add effect le */
	LE_AddProjectile(fd, flags, muzzle, impact, normal);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0] && !(flags & SF_BOUNCED))
		S_StartSound(le->origin, S_RegisterSound(fd->fireSound), fd->fireAttenuation);

	firstShot = qfalse;

	if (fd->irgoggles)
		refdef.rdflags |= RDF_IRGOGGLES;

	/* do actor related stuff */
	if (!le)
		return; /* maybe hidden or inuse is false? */

	if (!LE_IsActor(le)) {
		Com_Printf("Can't shoot, LE not an actor (type: %i)\n", le->type);
		return;
	}

	/* no animations for hidden actors */
	if (le->type == ET_ACTORHIDDEN)
		return;

	/** Spawn blood particles (if defined) if actor(-body) was hit. Even if actor is dead :)
	 * Don't do it if it's a stun-attack though.
	 * @todo Special particles for stun attack (mind you that there is electrical and gas/chemical stunning)? */
	if ((flags & SF_BODY) && fd->obj->dmgtype != csi.damStunGas) {	/**< @todo && !(flags & SF_BOUNCED) ? */
		CL_ActorHit(le, impact, normal);
	}

	if (LE_IsDead(le)) {
		Com_Printf("Can't shoot, actor dead or stunned.\n");
		return;
	}

	/* Animate - we have to check if it is right or left weapon usage. */
	if (IS_SHOT_RIGHT(shootType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->right, le->left, le->state));
		R_AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	} else if (IS_SHOT_LEFT(shootType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("shoot", le->left, le->right, le->state));
		R_AnimAppend(&le->as, le->model1, LE_GetAnim("stand", le->left, le->right, le->state));
	} else if (!IS_SHOT_HEADGEAR(shootType)) {
		/* no animation for headgear (yet) */
		Com_Error(ERR_DROP, "CL_ActorDoShoot: Invalid shootType given (entnum: %i, shootType: %i).\n", shootType, entnum);
	}
}


/**
 * @brief Shoot with weapon but don't bother with animations - actor is hidden.
 * @sa CL_ActorShoot
 */
void CL_ActorShootHidden (struct dbuffer *msg)
{
	const fireDef_t *fd;
	int first;
	int objIdx;
	objDef_t *obj;
	int weapFdsIdx, fdIdx;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_SHOOT_HIDDEN], &first, &objIdx, &weapFdsIdx, &fdIdx);

	/* get the fire def */
	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* start the sound */
	if (((first && fd->soundOnce) || (!first && !fd->soundOnce)) && fd->fireSound[0])
		S_StartLocalSound(fd->fireSound);

	/* if the shooting becomes visible, don't repeat sounds! */
	firstShot = qfalse;
}


/**
 * @brief Throw item with actor.
 * @param[in] msg The netchannel message
 */
void CL_ActorDoThrow (struct dbuffer *msg)
{
	const fireDef_t *fd;
	vec3_t muzzle, v0;
	int flags;
	int dtime;
	int objIdx;
	const objDef_t *obj;
	int weapFdsIdx, fdIdx;

	/* read data */
	NET_ReadFormat(msg, ev_format[EV_ACTOR_THROW], &dtime, &objIdx, &weapFdsIdx, &fdIdx, &flags, &muzzle, &v0);

	/* get the fire def */
	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* add effect le (local entity) */
	LE_AddGrenade(fd, flags, muzzle, v0, dtime);

	/* start the sound */
	if ((!fd->soundOnce || firstShot) && fd->fireSound[0] && !(flags & SF_BOUNCED)) {
		sfx_t *sfx = S_RegisterSound(fd->fireSound);
		S_StartSound(muzzle, sfx, DEFAULT_SOUND_ATTENUATION);
	}

	firstShot = qfalse;
}


/**
 * @brief Starts shooting with actor.
 * @param[in] msg The netchannel message
 * @sa CL_ActorShootHidden
 * @sa CL_ActorShoot
 * @sa CL_ActorDoShoot
 * @todo Improve detection of left- or right animation.
 * @sa EV_ACTOR_START_SHOOT
 */
void CL_ActorStartShoot (struct dbuffer *msg)
{
	const fireDef_t *fd;
	le_t *le;
	pos3_t from, target;
	int entnum;
	int objIdx;
	objDef_t *obj;
	int weapFdsIdx, fdIdx, shootType;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_START_SHOOT], &entnum, &objIdx, &weapFdsIdx, &fdIdx, &shootType, &from, &target);

	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* shooting actor */
	le = LE_Get(entnum);

	/* center view (if wanted) */
	if (cl_centerview->integer && cl.actTeam != cls.team)
		CL_CameraRoute(from, target);

	/* first shot */
	firstShot = qtrue;

	/* actor dependent stuff following */
	if (!le)
		/* it's OK, the actor not visible */
		return;

	if (!LE_IsLivingActor(le)) {
		Com_Printf("CL_ActorStartShoot: LE (%i) not a living actor (type: %i)\n", entnum, le->type);
		return;
	}

	/* ET_ACTORHIDDEN actors don't have a model yet */
	if (le->type == ET_ACTORHIDDEN)
		return;

	/* Animate - we have to check if it is right or left weapon usage. */
	if (IS_SHOT_RIGHT(shootType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("move", le->right, le->left, le->state));
	} else if (IS_SHOT_LEFT(shootType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("move", le->left, le->right, le->state));
	} else if (!IS_SHOT_HEADGEAR(shootType)) {
		/* no animation for headgear (yet) */
		Com_Error(ERR_DROP, "CL_ActorStartShoot: Invalid shootType given (entnum: %i, shootType: %i).\n", shootType, entnum);
	}
}

/**
 * @brief Kills actor.
 * @param[in] msg The netchannel message
 */
void CL_ActorDie (struct dbuffer *msg)
{
	le_t *le;
	int number, state;
	int i;

	NET_ReadFormat(msg, ev_format[EV_ACTOR_DIE], &number, &state);

	/* get le */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->entnum == number)
			break;

	if (le->entnum != number) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ActorDie: Can't kill, LE doesn't exist\n");
		return;
	}

	if (!LE_IsActor(le)) {
		Com_Printf("CL_ActorDie: Can't kill, LE is not an actor (type: %i)\n", le->type);
		return;
	}

	if (LE_IsDead(le)) {
		Com_Printf("CL_ActorDie: Can't kill, actor already dead\n");
		return;
	} else if (!le->inuse) {
		/* LE not in use condition normally arises when CL_EntPerish has been
		 * called on the le to hide it from the client's sight.
		 * Probably can return without killing unused LEs, but testing reveals
		 * killing an unused LE here doesn't cause any problems and there is
		 * an outside chance it fixes some subtle bugs. */
	}

	/* count spotted aliens */
	if (le->team != cls.team && le->team != TEAM_CIVILIAN && le->inuse)
		cl.numAliensSpotted--;

	/* set relevant vars */
	FLOOR(le) = NULL;
	le->STUN = 0;	/**< @todo Is there a reason this is reset? We _may_ need that in the future somehow.
					 * @sa g_client.c:G_ActorDie */
	le->state = state;

	/* play animation */
	le->think = NULL;
	R_AnimChange(&le->as, le->model1, va("death%i", LE_GetAnimationIndexForDeath(le)));
	R_AnimAppend(&le->as, le->model1, va("dead%i", LE_GetAnimationIndexForDeath(le)));

	/* Print some info about the death or stun. */
	if (le->team == cls.team) {
		const character_t *chr = CL_GetActorChr(le);
		if (chr) {
			char tmpbuf[128];
			if (LE_IsStunned(le)) {
				Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s was stunned\n"), chr->name);
			} else {
				Com_sprintf(tmpbuf, lengthof(tmpbuf), _("%s was killed\n"), chr->name);
			}
			HUD_DisplayMessage(tmpbuf);
		}
	} else {
		switch (le->team) {
		case (TEAM_CIVILIAN):
			if (LE_IsStunned(le))
				HUD_DisplayMessage(_("A civilian was stunned.\n"));
			else
				HUD_DisplayMessage(_("A civilian was killed.\n"));
			break;
		case (TEAM_ALIEN):
			if (LE_IsStunned(le))
				HUD_DisplayMessage(_("An alien was stunned.\n"));
			else
				HUD_DisplayMessage(_("An alien was killed.\n"));
			break;
		case (TEAM_PHALANX):
			if (LE_IsStunned(le))
				HUD_DisplayMessage(_("A soldier was stunned.\n"));
			else
				HUD_DisplayMessage(_("A soldier was killed.\n"));
			break;
		default:
			if (LE_IsStunned(le))
				HUD_DisplayMessage(va(_("A member of team %i was stunned.\n"), le->team));
			else
				HUD_DisplayMessage(va(_("A member of team %i was killed.\n"), le->team));
			break;
		}
	}

	CL_PlayActorSound(le, SND_DEATH);

	VectorCopy(player_dead_maxs, le->maxs);
	CL_RemoveActorFromTeamList(le);
}


/*
==============================================================
MOUSE INPUT
==============================================================
*/

/**
 * @brief handle select or action clicking in either move mode
 * @sa CL_ActorSelectMouse
 * @sa CL_ActorActionMouse
 */
static void CL_ActorMoveMouse (void)
{
	if (selActor->actorMode == M_PEND_MOVE) {
		if (VectorCompare(mousePos, mousePendPos)) {
			/* Pending move and clicked the same spot (i.e. 2 clicks on the same place) */
			CL_ActorStartMove(selActor, mousePos);
		} else {
			/* Clicked different spot. */
			VectorCopy(mousePos, mousePendPos);
		}
	} else {
		if (confirm_actions->integer) {
			/* Set our mode to pending move. */
			VectorCopy(mousePos, mousePendPos);

			selActor->actorMode = M_PEND_MOVE;
		} else {
			/* Just move there */
			CL_ActorStartMove(selActor, mousePos);
		}
	}
}

/**
 * @brief Selects an actor using the mouse.
 * @todo Comment on the cl.actorMode stuff.
 * @sa CL_ActorStartMove
 */
void CL_ActorSelectMouse (void)
{
	if (mouseSpace != MS_WORLD)
		return;

	switch (selActor->actorMode) {
	case M_MOVE:
	case M_PEND_MOVE:
		/* Try and select another team member */
		if (mouseActor && mouseActor != selActor && CL_ActorSelect(mouseActor)) {
			/* Succeeded so go back into move mode. */
			selActor->actorMode = M_MOVE;
		} else {
			CL_ActorMoveMouse();
		}
		break;
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		if (VectorCompare(mousePos, mousePendPos)) {
			/* Pending shot and clicked the same spot (i.e. 2 clicks on the same place) */
			CL_ActorShoot(selActor, mousePos);

			/* We switch back to aiming mode. */
			if (selActor->actorMode == M_PEND_FIRE_R)
				selActor->actorMode = M_FIRE_R;
			else
				selActor->actorMode = M_FIRE_L;
		} else {
			/* Clicked different spot. */
			VectorCopy(mousePos, mousePendPos);
		}
		break;
	case M_FIRE_R:
		if (mouseActor == selActor)
			break;

		/* We either switch to "pending" fire-mode or fire the gun. */
		if (confirm_actions->integer == 1) {
			selActor->actorMode = M_PEND_FIRE_R;
			VectorCopy(mousePos, mousePendPos);
		} else {
			CL_ActorShoot(selActor, mousePos);
		}
		break;
	case M_FIRE_L:
		if (mouseActor == selActor)
			break;

		/* We either switch to "pending" fire-mode or fire the gun. */
		if (confirm_actions->integer == 1) {
			selActor->actorMode = M_PEND_FIRE_L;
			VectorCopy(mousePos, mousePendPos);
		} else {
			CL_ActorShoot(selActor, mousePos);
		}
		break;
	default:
		break;
	}
}


/**
 * @brief initiates action with mouse.
 * @sa CL_ActionDown
 * @sa CL_ActorStartMove
 */
void CL_ActorActionMouse (void)
{
	if (!selActor || mouseSpace != MS_WORLD)
		return;

	if (selActor->actorMode == M_MOVE || selActor->actorMode == M_PEND_MOVE) {
		CL_ActorMoveMouse();
	} else {
		selActor->actorMode = M_MOVE;
	}
}


/*==============================================================
ROUND MANAGEMENT
==============================================================*/

/**
 * @brief Finishes the current round of the player in battlescape and starts the round for the next team.
 */
void CL_NextRound_f (void)
{
	struct dbuffer *msg;
	/* can't end round if we are not in battlescape */
	if (!CL_OnBattlescape())
		return;

	/* can't end round if we're not active */
	if (cls.team != cl.actTeam)
		return;

	/* send endround */
	msg = new_dbuffer();
	NET_WriteByte(msg, clc_endround);
	NET_WriteMsg(cls.netStream, msg);
}

/**
 * @brief Performs end-of-turn processing.
 * @param[in] msg The netchannel message
 * @sa CL_EndRoundAnnounce
 */
void CL_DoEndRound (struct dbuffer *msg)
{
	/* hud changes */
	if (cls.team == cl.actTeam)
		MN_ExecuteConfunc("endround");

	refdef.rdflags &= ~RDF_IRGOGGLES;

	/* change active player */
	Com_Printf("Team %i ended round", cl.actTeam);
	cl.actTeam = NET_ReadByte(msg);
	Com_Printf(", team %i's round started!\n", cl.actTeam);

	/* hud changes */
	if (cls.team == cl.actTeam) {
		int actorIdx;
		/* check whether a particle has to go */
		CL_ParticleCheckRounds();
		MN_ExecuteConfunc("startround");
		HUD_DisplayMessage(_("Your round started!\n"));
		S_StartLocalSound("misc/roundstart");
		CL_ConditionalMoveCalcForCurrentSelectedActor();

		for (actorIdx = 0; actorIdx < cl.numTeamList; actorIdx++) {
			if (cl.teamList[actorIdx]) {
				/* Check for unusable RF setting - just in case. */
				if (!CL_WorkingFiremode(cl.teamList[actorIdx], qtrue)) {
					Com_DPrintf(DEBUG_CLIENT, "CL_DoEndRound: INFO Updating reaction firemode for actor %i! - We need to check why that happened.\n", actorIdx);
					/* At this point the rest of the code forgot to update RF-settings somewhere. */
					CL_SetDefaultReactionFiremode(cl.teamList[actorIdx], ACTOR_HAND_CHAR_RIGHT);
				}

				/** @todo Reset reservations for shots?
				CL_ReserveTUs(cl.teamList[actorIdx], RES_SHOT, 0);
				MSG_Write_PA(PA_RESERVE_STATE, selActor->entnum, RES_REACTION, 0, selChr->reservedTus.shot); * Update server-side settings *
				*/
			}
		}
	}
}

/*
==============================================================
MOUSE SCANNING
==============================================================
*/

/**
 * @brief Battlescape cursor positioning.
 * @note Sets global var mouseActor to current selected le
 * @sa IN_Parse
 */
void CL_ActorMouseTrace (void)
{
	int i, restingLevel;
	float cur[2], frustumSlope[2], projectionDistance = 2048.0f;
	float nDotP2minusP1;
	vec3_t forward, right, up, stop;
	vec3_t from, end, dir;
	vec3_t mapNormal, P3, P2minusP1, P3minusP1;
	vec3_t pA, pB, pC;
	pos3_t testPos;
	pos3_t actor2x2[3];

	/* Get size of selected actor or fall back to 1x1. */
	const int fieldSize = selActor
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	le_t *le;

	/* get cursor position as a -1 to +1 range for projection */
	cur[0] = (mousePosX * viddef.rx - viddef.viewWidth * 0.5 - viddef.x) / (viddef.viewWidth * 0.5);
	cur[1] = (mousePosY * viddef.ry - viddef.viewHeight * 0.5 - viddef.y) / (viddef.viewHeight * 0.5);

	/* get trace vectors */
	VectorCopy(cl.cam.camorg, from);
	VectorCopy(cl.cam.axis[0], forward);
	VectorCopy(cl.cam.axis[1], right);
	VectorCopy(cl.cam.axis[2], up);

	if (cl_isometric->integer)
		frustumSlope[0] = 10.0 * refdef.fov_x;
	else
		frustumSlope[0] = tan(refdef.fov_x * M_PI / 360.0) * projectionDistance;
	frustumSlope[1] = frustumSlope[0] * ((float)viddef.viewHeight / (float)viddef.viewWidth);

	/* transform cursor position into perspective space */
	VectorMA(from, projectionDistance, forward, stop);
	VectorMA(stop, cur[0] * frustumSlope[0], right, stop);
	VectorMA(stop, cur[1] * -frustumSlope[1], up, stop);

	/* in isometric mode the camera position has to be calculated from the cursor position so that the trace goes in the right direction */
	if (cl_isometric->integer)
		VectorMA(stop, -projectionDistance * 2, forward, from);

	/* set stop point to the intersection of the trace line with the desired plane */
	/* description of maths used:
	 *   The equation for the plane can be written:
	 *     mapNormal dot (end - P3) = 0
	 *     where mapNormal is the vector normal to the plane,
	 *         P3 is any point on the plane and
	 *         end is the point where the line intersects the plane
	 *   All points on the line can be calculated using:
	 *     P1 + u*(P2 - P1)
	 *     where P1 and P2 are points that define the line and
	 *           u is some scalar
	 *   The intersection of the line and plane occurs when:
	 *     mapNormal dot (P1 + u*(P2 - P1)) == mapNormal dot P3
	 *   The intersection therefore occurs when:
	 *     u = (mapNormal dot (P3 - P1))/(mapNormal dot (P2 - P1))
	 * Note: in the code below from & stop represent P1 and P2 respectively
	 */
	VectorSet(P3, 0., 0., cl_worldlevel->integer * UNIT_HEIGHT + CURSOR_OFFSET);
	VectorSet(mapNormal, 0., 0., 1.);
	VectorSubtract(stop, from, P2minusP1);
	nDotP2minusP1 = DotProduct(mapNormal, P2minusP1);

	/* calculate intersection directly if angle is not parallel to the map plane */
	if (nDotP2minusP1 > 0.01 || nDotP2minusP1 < -0.01) {
		float u;
		VectorSubtract(P3, from, P3minusP1);
		u = DotProduct(mapNormal, P3minusP1) / nDotP2minusP1;
		VectorScale(P2minusP1, (vec_t)u, dir);
		VectorAdd(from, dir, end);
	} else { /* otherwise do a full trace */
		TR_TestLineDM(from, stop, end, TL_FLAG_ACTORCLIP);
	}

	VecToPos(end, testPos);
	restingLevel = Grid_Fall(clMap, fieldSize, testPos);

	/* hack to prevent cursor from getting stuck on the top of an invisible
	 * playerclip surface (in most cases anyway) */
	PosToVec(testPos, pA);
	/* ensure that the cursor is in the world, if this is not done, the tracer box is
	 * drawn in the void on the first level and the menu key bindings might get executed
	 * this could result in different problems like the zooming issue (where you can't zoom
	 * in again, because in_zoomout->state is not reseted). */
	if (CL_OutsideMap(pA, MAP_SIZE_OFFSET))
		return;

	VectorCopy(pA, pB);
	pA[2] += UNIT_HEIGHT;
	pB[2] -= UNIT_HEIGHT;
	/** @todo Shouldn't we check the return value of CM_TestLineDM here - maybe
	 * we don't have to do the second Grid_Fall call at all and can safe a lot
	 * of traces */
	TR_TestLineDM(pA, pB, pC, TL_FLAG_ACTORCLIP);
	VecToPos(pC, testPos);
	restingLevel = min(restingLevel, Grid_Fall(clMap, fieldSize, testPos));

	/* if grid below intersection level, start a trace from the intersection */
	if (restingLevel < cl_worldlevel->integer) {
		VectorCopy(end, from);
		from[2] -= CURSOR_OFFSET;
		TR_TestLineDM(from, stop, end, TL_FLAG_ACTORCLIP);
		VecToPos(end, testPos);
		restingLevel = Grid_Fall(clMap, fieldSize, testPos);
	}

	/* test if the selected grid is out of the world */
	if (restingLevel < 0 || restingLevel >= PATHFINDING_HEIGHT)
		return;

	/* Set truePos- test pos is under the cursor. */
	VectorCopy(testPos, truePos);
	truePos[2] = cl_worldlevel->integer;

	/* Set mousePos to the position that the actor will move to. */
	testPos[2] = restingLevel;
	VectorCopy(testPos, mousePos);

	/* search for an actor on this field */
	mouseActor = NULL;
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && LE_IsLivingAndVisibleActor(le))
			switch (le->fieldSize) {
			case ACTOR_SIZE_NORMAL:
				if (VectorCompare(le->pos, mousePos)) {
					mouseActor = le;
				}
				break;
			case ACTOR_SIZE_2x2:
				VectorSet(actor2x2[0], le->pos[0] + 1, le->pos[1],     le->pos[2]);
				VectorSet(actor2x2[1], le->pos[0],     le->pos[1] + 1, le->pos[2]);
				VectorSet(actor2x2[2], le->pos[0] + 1, le->pos[1] + 1, le->pos[2]);
				if (VectorCompare(le->pos, mousePos)
				|| VectorCompare(actor2x2[0], mousePos)
				|| VectorCompare(actor2x2[1], mousePos)
				|| VectorCompare(actor2x2[2], mousePos)) {
					mouseActor = le;
				}
				break;
			default:
				Com_Error(ERR_DROP, "Grid_MoveCalc: unknown actor-size: %i", le->fieldSize);
				break;
		}

	/* calculate move length */
	if (selActor && !VectorCompare(mousePos, mouseLastPos)) {
		VectorCopy(mousePos, mouseLastPos);
		CL_ResetActorMoveLength();
	}

	/* mouse is in the world */
	mouseSpace = MS_WORLD;
}


/*
==============================================================
ACTOR GRAPHICS
==============================================================
*/

/**
 * @brief Checks whether a weapon should be added to the entity's hand
 * @param[in] objID The item id that the actor is holding in his hand (@c le->left or @c le->right)
 * @return true if the weapon is a valid item and false if it's a dummy item or the actor has nothing
 * in the given hand
 */
static inline qboolean CL_AddActorWeapon (int objID)
{
	if (objID != NONE) {
		const objDef_t *od = INVSH_GetItemByIDX(objID);
		if (od->isDummy)
			return qfalse;
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Adds an actor.
 * @param[in] le The local entity to get the values from
 * @param[in] ent The body entity used in the renderer
 * @sa CL_AddUGV
 * @sa LE_AddToScene
 * @sa CL_ActorAppear
 * @note Called via addfunc for each local entity in every frame
 */
qboolean CL_AddActor (le_t * le, entity_t * ent)
{
	entity_t add;

	/* add the weapons to the actor's hands */
	if (!LE_IsDead(le)) {
		const qboolean addLeftHandWeapon = CL_AddActorWeapon(le->left);
		const qboolean addRightHandWeapon = CL_AddActorWeapon(le->right);
		/* add left hand weapon */
		if (addLeftHandWeapon) {
			memset(&add, 0, sizeof(add));

			add.model = cls.model_weapons[le->left];
			if (!add.model)
				Com_Error(ERR_DROP, "Actor model for left hand weapon wasn't found");

			/* point to the body ent which will be added last */
			add.tagent = R_GetFreeEntity() + 2 + addRightHandWeapon;
			add.tagname = "tag_lweapon";
			add.lighting = &le->lighting; /* values from the actor */

			R_AddEntity(&add);
		}

		/* add right hand weapon */
		if (addRightHandWeapon) {
			memset(&add, 0, sizeof(add));

			add.alpha = le->alpha;
			add.model = cls.model_weapons[le->right];
			if (!add.model)
				Com_Error(ERR_DROP, "Actor model for right hand weapon wasn't found");

			/* point to the body ent which will be added last */
			add.tagent = R_GetFreeEntity() + 2;
			add.tagname = "tag_rweapon";
			add.lighting = &le->lighting; /* values from the actor */

			R_AddEntity(&add);
		}
	}

	/* add head */
	memset(&add, 0, sizeof(add));

	add.alpha = le->alpha;
	add.model = le->model2;
	if (!add.model)
		Com_Error(ERR_DROP, "Actor model wasn't found");
	add.skinnum = le->skinnum;

	/* point to the body ent which will be added last */
	add.tagent = R_GetFreeEntity() + 1;
	add.tagname = "tag_head";
	add.lighting = &le->lighting; /* values from the actor */

	R_AddEntity(&add);

	/** Add actor special effects.
	 * Only draw blood if the actor is dead or (if stunned) was damaged more than half its maximum HPs. */
	/** @todo Better value for this?	*/
	if (LE_IsStunned(le) && le->HP <= le->maxHP / 2)
		ent->flags |= RF_BLOOD;
	else if (LE_IsDead(le))
		ent->flags |= RF_BLOOD;
	else
		ent->flags |= RF_SHADOW;

	ent->flags |= RF_ACTOR;

	if (!LE_IsDead(le) && !LE_IsStunned(le)) {
		if (le->selected)
			ent->flags |= RF_SELECTED;
		if (le->team == cls.team) {
			if (le->pnum == cl.pnum)
				ent->flags |= RF_MEMBER;
			if (le->pnum != cl.pnum)
				ent->flags |= RF_ALLIED;
		}
	}

	return qtrue;
}

/*
==============================================================
TARGETING GRAPHICS
==============================================================
*/

#define LOOKUP_EPSILON 0.0001f

/**
 * @brief table for lookup_erf
 * lookup[]= {erf(0), erf(0.1), ...}
 */
static const float lookup[30]= {
	0.0f,    0.1125f, 0.2227f, 0.3286f, 0.4284f, 0.5205f, 0.6039f,
	0.6778f, 0.7421f, 0.7969f, 0.8427f, 0.8802f, 0.9103f, 0.9340f,
	0.9523f, 0.9661f, 0.9763f, 0.9838f, 0.9891f, 0.9928f, 0.9953f,
	0.9970f, 0.9981f, 0.9989f, 0.9993f, 0.9996f, 0.9998f, 0.9999f,
	0.9999f, 1.0000f
};

/**
 * @brief table for lookup_erf
 * lookup[]= {10*(erf(0.1)-erf(0.0)), 10*(erf(0.2)-erf(0.1)), ...}
 */
static const float lookupdiff[30]= {
	1.1246f, 1.1024f, 1.0592f, 0.9977f, 0.9211f, 0.8336f, 0.7395f,
	0.6430f, 0.5481f, 0.4579f, 0.3750f, 0.3011f, 0.2369f, 0.1828f,
	0.1382f, 0.1024f, 0.0744f, 0.0530f, 0.0370f, 0.0253f, 0.0170f,
	0.0112f, 0.0072f, 0.0045f, 0.0028f, 0.0017f, 0.0010f, 0.0006f,
	0.0003f, 0.0002f
};

/**
 * @brief calculate approximate erf, the error function
 * http://en.wikipedia.org/wiki/Error_function
 * uses lookup table and linear interpolation
 * approximation good to around 0.001.
 * easily good enough for the job.
 * @param[in] z the number to calculate the erf of.
 * @return for positive arg, returns approximate erf. for -ve arg returns 0.0f.
 */
static inline float lookup_erf (float z)
{
	float ifloat;
	int iint;

	/* erf(-z)=-erf(z), but erf of -ve number should never be used here
	 * so return 0 here */
	if (z < LOOKUP_EPSILON)
		return 0.0f;
	if (z > 2.8f)
		return 1.0f;
	ifloat = floor(z * 10.0f);
	iint = (int)ifloat;
	assert(iint < 30);
	return lookup[iint] + (z - ifloat / 10.0f) * lookupdiff[iint];
}

/**
 * @brief Calculates chance to hit.
 * @param[in] toPos
 */
static float CL_TargetingToHit (pos3_t toPos)
{
	vec3_t shooter, target;
	float distance, pseudosin, width, height, acc, perpX, perpY, hitchance,
		stdevupdown, stdevleftright, crouch, commonfactor;
	int distx, disty, i, n;
	le_t *le;

	if (!selActor || !selFD)
		return 0.0;

	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && VectorCompare(le->pos, toPos))
			break;

	if (i >= numLEs)
		/* no target there */
		return 0.0;
	/* or suicide attempted */
	if (le == selActor && selFD->damage[0] > 0)
		return 0.0;

	VectorCopy(selActor->origin, shooter);
	VectorCopy(le->origin, target);

	/* Calculate HitZone: */
	distx = fabs(shooter[0] - target[0]);
	disty = fabs(shooter[1] - target[1]);
	distance = sqrt(distx * distx + disty * disty);
	if (distx > disty)
		pseudosin = distance / distx;
	else
		pseudosin = distance / disty;
	width = 2 * PLAYER_WIDTH * pseudosin;
	height = ((le->state & STATE_CROUCHED) ? PLAYER_CROUCHING_HEIGHT : PLAYER_STANDING_HEIGHT);

	acc = GET_ACC(selChr->score.skills[ABILITY_ACCURACY],
			selFD->weaponSkill ? selChr->score.skills[selFD->weaponSkill] : 0);

	crouch = ((selActor->state & STATE_CROUCHED) && selFD->crouch) ? selFD->crouch : 1;

	commonfactor = crouch * torad * distance * GET_INJURY_MULT(selChr->score.skills[ABILITY_MIND], selActor->HP, selActor->maxHP);
	stdevupdown = (selFD->spread[0] * (WEAPON_BALANCE + SKILL_BALANCE * acc)) * commonfactor;
	stdevleftright = (selFD->spread[1] * (WEAPON_BALANCE + SKILL_BALANCE * acc)) * commonfactor;
	hitchance = (stdevupdown > LOOKUP_EPSILON ? lookup_erf(height * 0.3536f / stdevupdown) : 1.0f)
			  * (stdevleftright > LOOKUP_EPSILON ? lookup_erf(width * 0.3536f / stdevleftright) : 1.0f);
	/* 0.3536=sqrt(2)/4 */

	/* Calculate cover: */
	n = 0;
	height = height / 18;
	width = width / 18;
	target[2] -= UNIT_HEIGHT / 2;
	target[2] += height * 9;
	perpX = disty / distance * width;
	perpY = 0 - distx / distance * width;

	target[0] += perpX;
	perpX *= 2;
	target[1] += perpY;
	perpY *= 2;
	target[2] += 6 * height;
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] -= 6 * height;
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] += perpX;
	target[1] += perpY;
	target[2] += 4 * height;
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[2] += 4 * height;
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX * 3;
	target[1] -= perpY * 3;
	target[2] -= 12 * height;
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 6 * height;
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] -= 4 * height;
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;
	target[0] -= perpX;
	target[1] -= perpY;
	target[2] += 10 * height;
	if (!TR_TestLine(shooter, target, TL_FLAG_NONE))
		n++;

	return (hitchance * (0.125) * n);
}

/**
 * @brief Show weapon radius
 * @param[in] center The center of the circle
 */
static void CL_Targeting_Radius (vec3_t center)
{
	const vec4_t color = {0, 1, 0, 0.3};
	ptl_t *particle;

	assert(selFD);

	particle = CL_ParticleSpawn("*circle", 0, center, NULL, NULL);
	particle->size[0] = selFD->splrad; /* misuse size vector as radius */
	particle->size[1] = 1; /* thickness */
	particle->style = STYLE_CIRCLE;
	particle->blend = BLEND_BLEND;
	/* free the particle every frame */
	particle->life = 0.0001;
	Vector4Copy(color, particle->color);
}


/**
 * @brief Draws line to target.
 * @param[in] fromPos The (grid-) position of the aiming actor.
 * @param[in] toPos The (grid-) position of the target.
 * @sa CL_TargetingGrenade
 * @sa CL_AddTargeting
 * @sa CL_Trace
 * @sa G_ShootSingle
 */
static void CL_TargetingStraight (pos3_t fromPos, int from_actor_size, pos3_t toPos)
{
	vec3_t start, end;
	vec3_t dir, mid, temp;
	trace_t tr;
	int oldLevel, i;
	float d;
	qboolean crossNo;
	le_t *le;
	le_t *target = NULL;
	int toActorSize;

	if (!selActor || !selFD)
		return;

	/* search for an actor at target */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && LE_IsLivingAndVisibleActor(le) && VectorCompare(le->pos, toPos)) {
			target = le;
			break;
		}

	/* Determine the target's size. */
	toActorSize = target
		? target->fieldSize
		: ACTOR_SIZE_NORMAL;

	/** @todo Adjust the toPos to the actor in case the actor is 2x2 */

	Grid_PosToVec(clMap, from_actor_size, fromPos, start);
	Grid_PosToVec(clMap, toActorSize, toPos, end);
	if (mousePosTargettingAlign)
		end[2] -= mousePosTargettingAlign;

	/* calculate direction */
	VectorSubtract(end, start, dir);
	VectorNormalize(dir);

	/* calculate 'out of range point' if there is one */
	if (VectorDistSqr(start, end) > selFD->range * selFD->range) {
		VectorMA(start, selFD->range, dir, mid);
		crossNo = qtrue;
	} else {
		VectorCopy(end, mid);
		crossNo = qfalse;
	}

	/* switch up to top level, this is a bit of a hack to make sure our trace doesn't go through ceilings ... */
	oldLevel = cl_worldlevel->integer;
	cl_worldlevel->integer = cl.map_maxlevel - 1;

	VectorMA(start, UNIT_SIZE * 1.4, dir, temp);
	tr = CL_Trace(start, temp, vec3_origin, vec3_origin, selActor, NULL, MASK_SHOT);
	if (tr.le && (tr.le->team == cls.team || tr.le->team == TEAM_CIVILIAN) && (tr.le->state & STATE_CROUCHED))
		VectorMA(start, UNIT_SIZE * 1.4, dir, temp);
	else
		VectorCopy(start, temp);

	tr = CL_Trace(temp, mid, vec3_origin, vec3_origin, selActor, target, MASK_SHOT);

	if (tr.fraction < 1.0) {
		d = VectorDist(temp, mid);
		VectorMA(start, tr.fraction * d, dir, mid);
		crossNo = qtrue;
	}

	/* switch level back to where it was again */
	cl_worldlevel->integer = oldLevel;

	/* spawn particles */
	CL_ParticleSpawn("inRangeTracer", 0, start, mid, NULL);
	if (crossNo) {
		CL_ParticleSpawn("longRangeTracer", 0, mid, end, NULL);
		CL_ParticleSpawn("cross_no", 0, end, NULL, NULL);
	} else {
		CL_ParticleSpawn("cross", 0, end, NULL, NULL);
	}

	hitProbability = 100 * CL_TargetingToHit(toPos);
}


#define GRENADE_PARTITIONS	20

/**
 * @brief Shows targeting for a grenade.
 * @param[in] fromPos The (grid-) position of the aiming actor.
 * @param[in] toPos The (grid-) position of the target (mousePos or mousePendPos).
 * @sa CL_TargetingStraight
 */
static void CL_TargetingGrenade (pos3_t fromPos, int from_actor_size, pos3_t toPos)
{
	vec3_t from, at, cross;
	float vz, dt;
	vec3_t v0, ds, next;
	trace_t tr;
	int oldLevel;
	qboolean obstructed = qfalse;
	int i;
	le_t *le;
	le_t *target = NULL;
	int toActorSize;

	if (!selActor || Vector2Compare(fromPos, toPos))
		return;

	/* search for an actor at target */
	for (i = 0, le = LEs; i < numLEs; i++, le++)
		if (le->inuse && LE_IsLivingAndVisibleActor(le) && VectorCompare(le->pos, toPos)) {
			target = le;
			break;
		}

	/* Determine the target's size. */
	toActorSize = target
		? target->fieldSize
		: ACTOR_SIZE_NORMAL;

	/** @todo Adjust the toPos to the actor in case the actor is 2x2 */

	/* get vectors, paint cross */
	Grid_PosToVec(clMap, from_actor_size, fromPos, from);
	Grid_PosToVec(clMap, toActorSize, toPos, at);
	from[2] += selFD->shotOrg[1];

	/* prefer to aim grenades at the ground */
	at[2] -= GROUND_DELTA;
	if (mousePosTargettingAlign)
		at[2] -= mousePosTargettingAlign;
	VectorCopy(at, cross);

	/* calculate parabola */
	dt = Com_GrenadeTarget(from, at, selFD->range, selFD->launched, selFD->rolled, v0);
	if (!dt) {
		CL_ParticleSpawn("cross_no", 0, cross, NULL, NULL);
		return;
	}

	dt /= GRENADE_PARTITIONS;
	VectorSubtract(at, from, ds);
	VectorScale(ds, 1.0 / GRENADE_PARTITIONS, ds);
	ds[2] = 0;

	/* switch up to top level, this is a bit of a hack to make sure our trace doesn't go through ceilings ... */
	oldLevel = cl_worldlevel->integer;
	cl_worldlevel->integer = cl.map_maxlevel - 1;

	/* paint */
	vz = v0[2];

	for (i = 0; i < GRENADE_PARTITIONS; i++) {
		VectorAdd(from, ds, next);
		next[2] += dt * (vz - 0.5 * GRAVITY * dt);
		vz -= GRAVITY * dt;
		VectorScale(v0, (i + 1.0) / GRENADE_PARTITIONS, at);

		/* trace for obstacles */
		tr = CL_Trace(from, next, vec3_origin, vec3_origin, selActor, target, MASK_SHOT);

		/* something was hit */
		if (tr.fraction < 1.0) {
			obstructed = qtrue;
		}

		/* draw particles */
		/** @todo character strength should be used here, too
		 * the stronger the character, the further the throw */
		if (obstructed || VectorLength(at) > selFD->range)
			CL_ParticleSpawn("longRangeTracer", 0, from, next, NULL);
		else
			CL_ParticleSpawn("inRangeTracer", 0, from, next, NULL);
		VectorCopy(next, from);
	}
	/* draw targeting cross */
	if (obstructed || VectorLength(at) > selFD->range)
		CL_ParticleSpawn("cross_no", 0, cross, NULL, NULL);
	else
		CL_ParticleSpawn("cross", 0, cross, NULL, NULL);

	if (selFD->splrad) {
		Grid_PosToVec(clMap, toActorSize, toPos, at);
		CL_Targeting_Radius(at);
	}

	hitProbability = 100 * CL_TargetingToHit(toPos);

	/* switch level back to where it was again */
	cl_worldlevel->integer = oldLevel;
}


/**
 * @brief field marker box
 * @sa ModelOffset
 */
static const vec3_t boxSize = { BOX_DELTA_WIDTH, BOX_DELTA_LENGTH, BOX_DELTA_HEIGHT };
#define BoxSize(i,source,target) (target[0]=i*source[0]+((i-1)*UNIT_SIZE),target[1]=i*source[1]+((i-1)*UNIT_SIZE),target[2]=source[2])
#define BoxOffset(i, target) (target[0]=(i-1)*(UNIT_SIZE+BOX_DELTA_WIDTH), target[1]=(i-1)*(UNIT_SIZE+BOX_DELTA_LENGTH), target[2]=0)

/**
 * @brief create a targeting box at the given position
 * @sa CL_ParseClientinfo
 */
static void CL_AddTargetingBox (pos3_t pos, qboolean pendBox)
{
	entity_t ent;
	vec3_t realBoxSize;
	vec3_t cursorOffset;

	const int fieldSize = selActor /**< Get size of selected actor or fall back to 1x1. */
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	memset(&ent, 0, sizeof(ent));
	ent.flags = RF_BOX;

	Grid_PosToVec(clMap, fieldSize, pos, ent.origin);

	/* Paint the green box if move is possible ...
	 * OR paint a dark blue one if move is impossible or the
	 * soldier does not have enough TimeUnits left. */
	if (selActor && actorMoveLength < ROUTING_NOT_REACHABLE && actorMoveLength <= CL_UsableTUs(selActor))
		VectorSet(ent.angles, 0, 1, 0); /* Green */
	else
		VectorSet(ent.angles, 0, 0, 0.6); /* Dark Blue */

	VectorAdd(ent.origin, boxSize, ent.oldorigin);

	/* color */
	if (mouseActor && mouseActor != selActor) {
		ent.alpha = 0.4 + 0.2 * sin((float) cl.time / 80);
		/* Paint the box red if the soldiers under the cursor is
		 * not in our team and no civilian either. */
		if (mouseActor->team != cls.team) {
			switch (mouseActor->team) {
			case TEAM_CIVILIAN:
				/* Civilians are yellow */
				VectorSet(ent.angles, 1, 1, 0); /* Yellow */
				break;
			default:
				if (mouseActor->team == TEAM_ALIEN) {
					if (GAME_TeamIsKnown(mouseActor->teamDef))
						MN_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, _(mouseActor->teamDef->name));
					else
						MN_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, _("Unknown alien race"));
				} else {
					/* multiplayer names */
					/* see CL_ParseClientinfo */
					MN_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, cl.configstrings[CS_PLAYERNAMES + mouseActor->pnum]);
				}
				/* Aliens (and players not in our team [multiplayer]) are red */
				VectorSet(ent.angles, 1, 0, 0); /* Red */
				break;
			}
		} else {
			/* coop multiplayer games */
			if (mouseActor->pnum != cl.pnum) {
				MN_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, cl.configstrings[CS_PLAYERNAMES + mouseActor->pnum]);
			} else {
				/* we know the names of our own actors */
				character_t* chr = CL_GetActorChr(mouseActor);
				assert(chr);
				MN_RegisterText(TEXT_MOUSECURSOR_PLAYERNAMES, chr->name);
			}
			/* Paint a light blue box if on our team */
			VectorSet(ent.angles, 0.2, 0.3, 1); /* Light Blue */
		}

		if (selActor) {
			BoxOffset(selActor->fieldSize, cursorOffset);
			VectorAdd(ent.oldorigin, cursorOffset, ent.oldorigin);
			VectorAdd(ent.origin, cursorOffset, ent.origin);
			BoxSize(selActor->fieldSize, boxSize, realBoxSize);
			VectorSubtract(ent.origin, realBoxSize, ent.origin);
		}
	} else {
		if (selActor) {
			BoxOffset(selActor->fieldSize, cursorOffset);
			VectorAdd(ent.oldorigin, cursorOffset, ent.oldorigin);
			VectorAdd(ent.origin, cursorOffset, ent.origin);

			BoxSize(selActor->fieldSize, boxSize, realBoxSize);
			VectorSubtract(ent.origin, realBoxSize, ent.origin);
		} else {
			VectorSubtract(ent.origin, boxSize, ent.origin);
		}
		ent.alpha = 0.3;
	}

	/* if pendBox is true then ignore all the previous color considerations and use cyan */
	if (pendBox) {
		VectorSet(ent.angles, 0, 1, 1); /* Cyan */
		ent.alpha = 0.15;
	}

	/* add it */
	R_AddEntity(&ent);
}

/**
 * @brief Key binding for fast inventory access
 */
void CL_ActorInventoryOpen_f (void)
{
	if (CL_OnBattlescape()) {
		if (!strstr(MN_GetActiveMenuName(), "hudinv")) {
			if (!strcmp(mn_hud->string, "althud"))
				MN_PushMenu("ahudinv", NULL);
			else
				MN_PushMenu("hudinv", NULL);
		} else
			MN_PopMenu(qfalse);
	}
}

/**
 * @brief Targets to the ground when holding the assigned button
 * @sa mousePosTargettingAlign
 */
void CL_ActorTargetAlign_f (void)
{
	int align = GROUND_DELTA;
	static int currentPos = 0;

	/* no firedef selected */
	if (!selFD || !selActor)
		return;
	if (selActor->actorMode != M_FIRE_R && selActor->actorMode != M_FIRE_L
	 && selActor->actorMode != M_PEND_FIRE_R && selActor->actorMode != M_PEND_FIRE_L)
		return;

	/* user defined height align */
	if (Cmd_Argc() == 2) {
		align = atoi(Cmd_Argv(1));
	} else {
		switch (currentPos) {
		case 0:
			if (selFD->gravity)
				align = -align;
			currentPos = 1; /* next level */
			break;
		case 1:
			/* only allow to align to lower z level if the actor is
			 * standing at a higher z-level */
			if (selFD->gravity)
				align = -(2 * align);
			else
				align = -align;
			currentPos = 2;
			break;
		case 2:
			/* the static var is not reseted on weaponswitch or actorswitch */
			if (selFD->gravity) {
				align = 0;
				currentPos = 0; /* next level */
			} else {
				align = -(2 * align);
				currentPos = 3; /* next level */
			}
			break;
		case 3:
			align = 0;
			currentPos = 0; /* back to start */
			break;
		}
	}
	mousePosTargettingAlign = align;
}

/**
 * @brief Adds a target cursor when we render the world.
 * @sa CL_TargetingStraight
 * @sa CL_TargetingGrenade
 * @sa CL_AddTargetingBox
 * @sa CL_TraceMove
 * @sa V_RenderView
 * Draws the tracer (red, yellow, green box) on the grid
 */
void CL_AddTargeting (void)
{
	if (mouseSpace != MS_WORLD || !selActor)
		return;

	switch (selActor->actorMode) {
	case M_MOVE:
	case M_PEND_MOVE:
		/* Display Move-cursor. */
		CL_AddTargetingBox(mousePos, qfalse);

		if (selActor->actorMode == M_PEND_MOVE) {
			/* Also display a box for the pending move if we have one. */
			CL_AddTargetingBox(mousePendPos, qtrue);
			if (!CL_TraceMove(mousePendPos))
				selActor->actorMode = M_MOVE;
		}
		break;
	case M_FIRE_R:
	case M_FIRE_L:
		if (!selFD)
			return;

		if (!selFD->gravity)
			CL_TargetingStraight(selActor->pos, selActor->fieldSize, mousePos);
		else
			CL_TargetingGrenade(selActor->pos, selActor->fieldSize, mousePos);
		break;
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		if (!selFD)
			return;

		/* Draw cursor at mousepointer */
		CL_AddTargetingBox(mousePos, qfalse);

		/* Draw (pending) Cursor at target */
		CL_AddTargetingBox(mousePendPos, qtrue);

		if (!selFD->gravity)
			CL_TargetingStraight(selActor->pos, selActor->fieldSize, mousePendPos);
		else
			CL_TargetingGrenade(selActor->pos, selActor->fieldSize, mousePendPos);
		break;
	default:
		break;
	}
}

static const vec3_t boxShift = { PLAYER_WIDTH, PLAYER_WIDTH, UNIT_HEIGHT / 2 - DIST_EPSILON };

/**
 * @brief create a targeting box at the given position
 * @sa CL_ParseClientinfo
 */
static void CL_AddPathingBox (pos3_t pos)
{
	entity_t ent;
	int height; /* The total opening size */
	int base; /* The floor relative to this cell */

 	/* Get size of selected actor or fall back to 1x1. */
	const int fieldSize = selActor
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;

	const int crouchingState = selActor && (selActor->state & STATE_CROUCHED) ? 1 : 0;
	const int TUneed = Grid_MoveLength(selActor->pathMap, pos, crouchingState, qfalse);
	const int TUhave = CL_UsableTUs(selActor);

	memset(&ent, 0, sizeof(ent));
	ent.flags = RF_PATH;

	Grid_PosToVec(clMap, fieldSize, pos, ent.origin);
	VectorSubtract(ent.origin, boxShift, ent.origin);

	base = Grid_Floor(clMap, fieldSize, pos);

	/* Paint the box green if it is reachable,
	 * yellow if it can be entered but is too far,
	 * or red if it cannot be entered ever. */
	if (base < -PATHFINDING_MAX_FALL * QUANT) {
		VectorSet(ent.angles, 0.0, 0.0, 0.0); /* Can't enter - black */
	} else {
		/* Can reach - green
		 * Passable but unreachable - yellow
		 * Not passable - red */
		VectorSet(ent.angles, (TUneed > TUhave), (TUneed != ROUTING_NOT_REACHABLE), 0);
	}

	/* Set the box height to the ceiling value of the cell. */
	height = 2 + min(TUneed * (UNIT_HEIGHT - 2) / ROUTING_NOT_REACHABLE, 16);
	ent.oldorigin[2] = height;
	ent.oldorigin[0] = TUneed;
	ent.oldorigin[1] = TUhave;

	ent.alpha = 0.25;

	/* add it */
	R_AddEntity(&ent);
}

/**
 * @brief Adds a pathing marker to the current floor when we render the world.
 * @sa V_RenderView
 * Draws the tracer (red, yellow, green box) on the grid
 */
void CL_AddPathing (void)
{
	pos3_t pos;

	pos[2] = cl_worldlevel->integer;
	for (pos[1] = max(mousePos[1] - 8, 0); pos[1] <= min(mousePos[1] + 8, PATHFINDING_WIDTH - 1); pos[1]++)
		for (pos[0] = max(mousePos[0] - 8, 0); pos[0] <= min(mousePos[0] + 8, PATHFINDING_WIDTH - 1); pos[0]++)
			CL_AddPathingBox(pos);
}

/**
 * @brief Plays various sounds on actor action.
 * @param[in] le The actor
 * @param[in] soundType Type of action (among actorSound_t) for which we need a sound.
 */
void CL_PlayActorSound (const le_t *le, actorSound_t soundType)
{
	const char *actorSound = Com_GetActorSound(le->teamDef, le->gender, soundType);
	if (actorSound) {
		sfx_t *sfx = S_RegisterSound(actorSound);
		if (sfx) {
			Com_DPrintf(DEBUG_SOUND|DEBUG_CLIENT, "CL_PlayActorSound: ActorSound: '%s'\n", actorSound);
			S_StartSound(le->origin, sfx, DEFAULT_SOUND_ATTENUATION);
		}
	}
}

/**
 * @brief create an arrow between from and to with the specified color ratios
 */
static void CL_AddArrow (vec3_t from, vec3_t to, float red, float green, float blue)
{
	entity_t ent;

	/* Com_Printf("Adding arrow (%f, %f, %f) to (%f, %f, %f).\n", from[0], from[1], from[2], to[0], to[1], to[2]); */

	memset(&ent, 0, sizeof(ent));
	ent.flags = RF_ARROW;
	VectorCopy(from, ent.origin);
	VectorCopy(to, ent.oldorigin);
	VectorSet(ent.angles, red, green, blue);

	ent.alpha = 0.25;

	/* add it */
	R_AddEntity(&ent);
}

/**
 * @brief Useful for debugging pathfinding
 */
void CL_DisplayFloorArrows (void)
{
	/* Get size of selected actor or fall back to 1x1. */
	const int fieldSize = selActor
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;
	vec3_t base, start;

	Grid_PosToVec(clMap, fieldSize, truePos, base);
	VectorCopy(base, start);
	base[2] -= QUANT;
	start[2] += QUANT;
}

/**
 * @brief Useful for debugging pathfinding
 */
void CL_DisplayObstructionArrows (void)
{
	/* Get size of selected actor or fall back to 1x1. */
	const int fieldSize = selActor
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;
	vec3_t base, start;

	Grid_PosToVec(clMap, fieldSize, truePos, base);
	VectorCopy(base, start);
	CL_AddArrow(base, start, 0.0, 0.0, 0.0);

}

#ifdef DEBUG
/**
 * @brief Triggers @c Grid_MoveMark in every direction at the current truePos.
 */
void CL_DumpMoveMark_f (void)
{
	/* Get size of selected actor or fall back to 1x1. */
	const int fieldSize = selActor
		? selActor->fieldSize
		: ACTOR_SIZE_NORMAL;
	const int crouchingState = selActor
		? (selActor->state & STATE_CROUCHED ? 1 : 0)
		: 0;
	const int temp = developer->integer;

	if (!selActor)
		return;

	developer->integer |= DEBUG_PATHING;

	CL_BuildForbiddenList();
	Grid_MoveCalc(clMap, fieldSize, selActor->pathMap, truePos, crouchingState, MAX_ROUTE, fb_list, fb_length);

	developer->integer ^= DEBUG_PATHING;

	CL_ConditionalMoveCalcForCurrentSelectedActor();
	developer->integer = temp;
}

/**
 * @brief Shows a table of the TUs that would be used by the current actor to move
 * relative to its current location
 */
void CL_DumpTUs_f (void)
{
	int x, y, crouchingState;
	pos3_t pos, loc;

	if (!selActor)
		return;

	crouchingState = selActor->state & STATE_CROUCHED ? 1 : 0;
	VectorCopy(selActor->pos, pos);

	Com_Printf("TUs around (%i, %i, %i)\n", pos[0], pos[1], pos[2]);

	for (y = max(0, pos[1] - 8); y <= min(PATHFINDING_WIDTH, pos[1] + 8); y++) {
		for (x = max(0, pos[0] - 8); x <= min(PATHFINDING_WIDTH, pos[0] + 8); x++) {
			VectorSet(loc, x, y, pos[2]);
			Com_Printf("%3i ", Grid_MoveLength(selActor->pathMap, loc, crouchingState, qfalse));
		}
		Com_Printf("\n");
	}
	Com_Printf("TUs at (%i, %i, %i) = %i\n", pos[0], pos[1], pos[2], Grid_MoveLength(selActor->pathMap, pos, crouchingState, qfalse));
}

/**
 * @brief display pathfinding info to the console. Also useful to
 * directly use the debugger on some vital pathfinding functions.
 * Will probably be removed for the release.
 */
void CL_DebugPath_f (void)
{
	const int actor_size = 1;
	const pos_t x = mousePos[0];
	const pos_t y = mousePos[1];
	const pos_t z = mousePos[2];
	int dir = 1;
	int new_z;

	if (mouseSpace != MS_WORLD)
		return;

	Com_Printf("data at cursor XYZ(%i, %i, %i) Floor(%i) Ceiling(%i)\n", x, y, z,
		RT_FLOOR(clMap, actor_size, x, y, z),
		RT_CEILING(clMap, actor_size, x, y, z) );
	Com_Printf("connections ortho: (PX=%i, NX=%i, PY=%i, NY=%i))\n",
		RT_CONN_PX(clMap, actor_size, x, y, z),		// dir = 0
		RT_CONN_NX(clMap, actor_size, x, y, z),		// 1
		RT_CONN_PY(clMap, actor_size, x, y, z),		// 2
		RT_CONN_NY(clMap, actor_size, x, y, z) );	// 3
	Com_Printf("connections diago: (PX_PY=%i, NX_NY=%i, NX_PY=%i, PX_NY=%i))\n",
		RT_CONN_PX_PY(clMap, actor_size, x, y, z),	// dir = 4
		RT_CONN_NX_NY(clMap, actor_size, x, y, z),	// 5
		RT_CONN_NX_PY(clMap, actor_size, x, y, z),	// 6
		RT_CONN_PX_NY(clMap, actor_size, x, y, z) );// 7
	Com_Printf("stepup ortho: (PX=%i, NX=%i, PY=%i, NY=%i))\n",
		RT_STEPUP_PX(clMap, actor_size, x, y, z),		// dir = 0
		RT_STEPUP_NX(clMap, actor_size, x, y, z),		// 1
		RT_STEPUP_PY(clMap, actor_size, x, y, z),		// 2
		RT_STEPUP_NY(clMap, actor_size, x, y, z) );		// 3

#if 1
	Com_Printf("performing RT_UpdateConnection() in dir: %i\n", dir);
	new_z = RT_UpdateConnection(clMap, actor_size, x, y, z, dir);
	Com_Printf("connections ortho: (PX=%i, NX=%i, PY=%i, NY=%i))\n",
		RT_CONN_PX(clMap, actor_size, x, y, z),
		RT_CONN_NX(clMap, actor_size, x, y, z),
		RT_CONN_PY(clMap, actor_size, x, y, z),
		RT_CONN_NY(clMap, actor_size, x, y, z) );
	Com_Printf("connections diago: (PX_PY=%i, NX_NY=%i, NX_PY=%i, PX_NY=%i))\n",
		RT_CONN_PX_PY(clMap, actor_size, x, y, z),	// dir = 4
		RT_CONN_NX_NY(clMap, actor_size, x, y, z),	// 5
		RT_CONN_NX_PY(clMap, actor_size, x, y, z),	// 6
		RT_CONN_PX_NY(clMap, actor_size, x, y, z) );// 7
#endif
#if 0
	new_z = RT_CheckCell(clMap, actor_size, x, y, z);
	Com_Printf("check returns: Z=%i\n", new_z);
#endif
#if 0
	priorityQueue_t pqueue;
	PQueueInitialise(&pqueue, 1024);
	Grid_MoveMark(clMap, actor_size, le->pathMap, mousePos, 0, 1,&pqueue);
	PQueueFree(&pqueue);
#endif
}
#endif

void ACTOR_InitStartup (void)
{
	cl_autostand = Cvar_Get("cl_autostand","1", CVAR_USERINFO | CVAR_ARCHIVE, "Save accidental TU waste by allowing server to autostand before long walks");
	confirm_actions = Cvar_Get("confirm_actions", "0", CVAR_ARCHIVE, "Confirm all actions in tactical mode");
}
