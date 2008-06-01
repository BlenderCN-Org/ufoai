/**
 * @file g_main.c
 * @brief Main game functions.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/game/g_main.c
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

game_locals_t game;
level_locals_t level;
game_import_t gi;
game_export_t globals;

edict_t *g_edicts;

cvar_t *password;

cvar_t *sv_needpass;
cvar_t *sv_maxplayersperteam;
cvar_t *sv_maxsoldiersperteam;
cvar_t *sv_maxsoldiersperplayer;
cvar_t *sv_enablemorale;
cvar_t *sv_roundtimelimit;
cvar_t *sv_maxentities;
cvar_t *sv_dedicated;
cvar_t *developer;

cvar_t *logstats;
FILE *logstatsfile;

cvar_t *sv_filterban;

cvar_t *sv_cheats;

cvar_t *sv_maxteams;

cvar_t *sv_ai;
cvar_t *sv_teamplay;
cvar_t *sv_maxclients;
cvar_t *sv_reaction_leftover;
cvar_t *sv_shot_origin;
cvar_t *sv_send_edicts;

cvar_t *ai_alien;
cvar_t *ai_civilian;
cvar_t *ai_equipment;
cvar_t *ai_numaliens;
cvar_t *ai_numcivilians;
cvar_t *ai_numactors;
cvar_t *ai_autojoin;

/* morale cvars */
cvar_t *mob_death;
cvar_t *mob_wound;
cvar_t *mof_watching;
cvar_t *mof_teamkill;
cvar_t *mof_civilian;
cvar_t *mof_enemy;
cvar_t *mor_pain;

/*everyone gets this times morale damage */
cvar_t *mor_default;

/*at this distance the following two get halfed (exponential scale) */
cvar_t *mor_distance;

/*at this distance the following two get halfed (exponential scale) */
cvar_t *mor_victim;

/*at this distance the following two get halfed (exponential scale) */
cvar_t *mor_attacker;

/* how much the morale depends on the size of the damaged team */
cvar_t *mon_teamfactor;

cvar_t *mor_regeneration;
cvar_t *mor_shaken;
cvar_t *mor_panic;

cvar_t *m_sanity;
cvar_t *m_rage;
cvar_t *m_rage_stop;
cvar_t *m_panic_stop;

cvar_t *g_aidebug;
cvar_t *g_nodamage;
cvar_t *g_notu;
cvar_t *flood_msgs;
cvar_t *flood_persecond;
cvar_t *flood_waitdelay;

cvar_t *difficulty;

invList_t invChain[MAX_INVLIST];


/*=================================================================== */


/**
 * @brief This will be called when the dll is first loaded
 * @note only happens when a new game is started or a save game is loaded.
 */
static void G_Init (void)
{
	Com_Printf("==== InitGame ====\n");

	/* noset vars */
	sv_dedicated = gi.Cvar_Get("sv_dedicated", "0", CVAR_SERVERINFO | CVAR_NOSET, "Is this a dedicated server?");

	/* latched vars */
	sv_cheats = gi.Cvar_Get("sv_cheats", "0", CVAR_SERVERINFO | CVAR_LATCH, "Activate cheats");
	gi.Cvar_Get("gamename", GAMEVERSION, CVAR_SERVERINFO | CVAR_LATCH, NULL);
	gi.Cvar_Get("gamedate", __DATE__, CVAR_SERVERINFO | CVAR_LATCH, NULL);
	developer = gi.Cvar_Get("developer", "0", 0, "Print out a lot of developer debug messages - useful to track down bugs");
	logstats = gi.Cvar_Get("logstats", "1", CVAR_ARCHIVE, "Server logfile output for kills");

	/* max. players per team (original quake) */
	sv_maxplayersperteam = gi.Cvar_Get("sv_maxplayersperteam", "8", CVAR_SERVERINFO | CVAR_LATCH, "How many players (humans) may a team have");
	/* max. soldiers per team */
	sv_maxsoldiersperteam = gi.Cvar_Get("sv_maxsoldiersperteam", "4", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, "How many soldiers may one team have");
	/* max soldiers per player */
	sv_maxsoldiersperplayer = gi.Cvar_Get("sv_maxsoldiersperplayer", "8", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, "How many soldiers one player is able to control in a given team");
	/* enable moralestates in multiplayer */
	sv_enablemorale = gi.Cvar_Get("sv_enablemorale", "1", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, "Enable morale behaviour for actors");
	sv_roundtimelimit = gi.Cvar_Get("sv_roundtimelimit", "0", CVAR_SERVERINFO, "Timelimit for multiplayer rounds");
	sv_roundtimelimit->modified = qfalse;
	sv_maxentities = gi.Cvar_Get("sv_maxentities", "1024", CVAR_LATCH, NULL);

	sv_maxteams = gi.Cvar_Get("sv_maxteams", "2", CVAR_SERVERINFO, "How many teams for current running map");
	sv_maxteams->modified = qfalse;

	/* change anytime vars */
	password = gi.Cvar_Get("password", "", CVAR_USERINFO, NULL);
	sv_needpass = gi.Cvar_Get("sv_needpass", "0", CVAR_SERVERINFO, NULL);
	sv_filterban = gi.Cvar_Get("sv_filterban", "1", 0, NULL);
	sv_ai = gi.Cvar_Get("sv_ai", "1", 0, NULL);
	sv_teamplay = gi.Cvar_Get("sv_teamplay", "0", CVAR_ARCHIVE | CVAR_LATCH | CVAR_SERVERINFO, "Is teamplay activated? see sv_maxclients, sv_maxplayersperteam, sv_maxsoldiersperteam and sv_maxsoldiersperplayer");
	/* how many connected clients */
	sv_maxclients = gi.Cvar_Get("sv_maxclients", "1", CVAR_SERVERINFO, "If sv_maxclients is 1 we are in singleplayer - otherwise we are mutliplayer mode (see sv_teamplay)");
	/* reaction leftover is 0 for acceptance testing; should default to 13 */
	sv_reaction_leftover = gi.Cvar_Get("sv_reaction_leftover", "0", CVAR_LATCH, "Minimum TU left over by reaction fire");
	sv_shot_origin = gi.Cvar_Get("sv_shot_origin", "8", 0, "Assumed distance of muzzle from model");

	sv_send_edicts = gi.Cvar_Get("sv_send_edicts", "0", CVAR_ARCHIVE | CVAR_LATCH, "Send server side edicts for client display like triggers");

	ai_alien = gi.Cvar_Get("ai_alien", "ortnok", 0, "Alien team");
	ai_civilian = gi.Cvar_Get("ai_civilian", "europe", 0, "Civilian team");
	/* this cvar is set in singleplayer via campaign definition */
	ai_equipment = gi.Cvar_Get("ai_equipment", "multiplayer_alien", 0, "Initial equipment definition for aliens");
	/* aliens in singleplayer (can differ each mission) */
	ai_numaliens = gi.Cvar_Get("ai_numaliens", "8", 0, "How many aliens in this battle (singleplayer)");
	/* civilians for singleplayer */
	ai_numcivilians = gi.Cvar_Get("ai_numcivilians", "8", 0, "How many civilians in this battle");
	/* aliens in multiplayer */
	ai_numactors = gi.Cvar_Get("ai_numactors", "8", CVAR_ARCHIVE, "How many (ai controlled) actors in this battle (multiplayer)");
	/* autojoin aliens */
	ai_autojoin = gi.Cvar_Get("ai_autojoin", "0", 0, "Auto join ai players if no human player was found for a team");

	/* FIXME: Apply CVAR_NOSET after balancing */
	mob_death = gi.Cvar_Get("mob_death", "10", CVAR_LATCH, NULL);
	mob_wound = gi.Cvar_Get("mob_wound", "0.1", CVAR_LATCH, NULL);
	mof_watching = gi.Cvar_Get("mof_watching", "1.7", CVAR_LATCH, NULL);
	mof_teamkill = gi.Cvar_Get("mof_teamkill", "2.0", CVAR_LATCH, NULL);
	mof_civilian = gi.Cvar_Get("mof_civilian", "0.3", CVAR_LATCH, NULL);
	mof_enemy = gi.Cvar_Get("mof_ememy", "0.5", CVAR_LATCH, NULL);
	mor_pain = gi.Cvar_Get("mof_pain", "3.6", CVAR_LATCH, NULL);
	/*everyone gets this times morale damage */
	mor_default = gi.Cvar_Get("mor_default", "0.3", CVAR_LATCH, "Everyone gets this times morale damage");
	/*at this distance the following two get halfed (exponential scale) */
	mor_distance = gi.Cvar_Get("mor_distance", "120", CVAR_LATCH, "At this distance the following two get halfed (exponential scale)");
	/*at this distance the following two get halfed (exponential scale) */
	mor_victim = gi.Cvar_Get("mor_victim", "0.7", CVAR_LATCH, "At this distance the following two get halfed (exponential scale)");
	/*at this distance the following two get halfed (exponential scale) */
	mor_attacker = gi.Cvar_Get("mor_attacker", "0.3", CVAR_LATCH, "At this distance the following two get halfed (exponential scale)");
	/* how much the morale depends on the size of the damaged team */
	mon_teamfactor = gi.Cvar_Get("mon_teamfactor", "0.6", CVAR_LATCH, "How much the morale depends on the size of the damaged team");

	mor_regeneration = gi.Cvar_Get("mor_regeneration", "15", CVAR_LATCH, NULL);
	mor_shaken = gi.Cvar_Get("mor_shaken", "50", CVAR_LATCH, NULL);
	mor_panic = gi.Cvar_Get("mor_panic", "30", CVAR_LATCH, NULL);

	m_sanity = gi.Cvar_Get("m_sanity", "1.0", CVAR_LATCH, NULL);
	m_rage = gi.Cvar_Get("m_rage", "0.6", CVAR_LATCH, NULL);
	m_rage_stop = gi.Cvar_Get("m_rage_stop", "2.0", CVAR_LATCH, NULL);
	m_panic_stop = gi.Cvar_Get("m_panic_stop", "1.0", CVAR_LATCH, NULL);

	g_aidebug = gi.Cvar_Get("g_aidebug", "0", CVAR_DEVELOPER, "All AI actors are visible");
	g_nodamage = gi.Cvar_Get("g_nodamage", "0", CVAR_DEVELOPER, "No damage in developer mode");
	g_notu = gi.Cvar_Get("g_notu", "0", CVAR_DEVELOPER, "No TU costs while moving around (e.g. for map testing)");

	/* flood control */
	flood_msgs = gi.Cvar_Get("flood_msgs", "4", 0, NULL);
	flood_persecond = gi.Cvar_Get("flood_persecond", "4", 0, NULL);
	flood_waitdelay = gi.Cvar_Get("flood_waitdelay", "10", 0, "Delay until someone is unlocked from talking again");

	difficulty = gi.Cvar_Get("difficulty", "0", CVAR_NOSET, "Difficulty level");

	game.sv_maxentities = sv_maxentities->integer;
	/* FIXME: */
	game.sv_maxplayersperteam = sv_maxplayersperteam->integer;

	/* initialize all entities for this game */
	g_edicts = gi.TagMalloc(game.sv_maxentities * sizeof(g_edicts[0]), TAG_GAME);
	globals.edicts = g_edicts;
	globals.max_edicts = game.sv_maxentities;
	globals.num_edicts = game.sv_maxplayersperteam;

	/* initialize all players for this game */
	/* game.sv_maxplayersperteam for human controlled players
	 * + game.sv_maxplayer for ai */
	game.players = gi.TagMalloc(game.sv_maxplayersperteam * 2 * sizeof(game.players[0]), TAG_GAME);
	globals.players = game.players;
	globals.maxplayersperteam = game.sv_maxplayersperteam;

	/* init csi and inventory */
	INVSH_InitCSI(gi.csi);
	INVSH_InitInventory(invChain);

	logstatsfile = NULL;
	if (logstats->integer)
		logstatsfile = fopen(va("%s/stats.log", gi.FS_Gamedir()), "a");
}


/*=================================================================== */


/**
 * @brief Free the tags TAG_LEVEL and TAG_GAME
 * @sa Mem_FreeTags
 */
static void G_Shutdown (void)
{
	Com_Printf("==== ShutdownGame ====\n");

	if (logstatsfile)
		fclose(logstatsfile);
	logstatsfile = NULL;

	gi.FreeTags(TAG_LEVEL);
	gi.FreeTags(TAG_GAME);
}


/**
 * @brief Returns a pointer to the structure with all entry points and global variables
 */
game_export_t *GetGameAPI (game_import_t * import)
{
	gi = *import;
	srand(gi.seed);

	globals.apiversion = GAME_API_VERSION;
	globals.Init = G_Init;
	globals.Shutdown = G_Shutdown;
	globals.SpawnEntities = G_SpawnEntities;

	globals.ClientConnect = G_ClientConnect;
	globals.ClientUserinfoChanged = G_ClientUserinfoChanged;
	globals.ClientDisconnect = G_ClientDisconnect;
	globals.ClientBegin = G_ClientBegin;
	globals.ClientSpawn = G_ClientSpawn;
	globals.ClientCommand = G_ClientCommand;
	globals.ClientAction = G_ClientAction;
	globals.ClientEndRound = G_ClientEndRound;
	globals.ClientTeamInfo = G_ClientTeamInfo;
	globals.ClientGetTeamNum = G_ClientGetTeamNum;
	globals.ClientGetTeamNumPref = G_ClientGetTeamNumPref;
	globals.ClientGetName = G_GetPlayerName;
	globals.ClientGetActiveTeam = G_GetActiveTeam;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

	globals.edict_size = sizeof(edict_t);
	globals.player_size = sizeof(player_t);

	return &globals;
}

#ifndef GAME_HARD_LINKED
/* this is only here so the functions in q_shared.c and q_shwin.c can link */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	text[sizeof(text) - 1] = 0;

	gi.error("%s", text);
}

void Com_Printf (const char *msg, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	text[sizeof(text) - 1] = 0;

	gi.dprintf("%s", text);
}

void Com_DPrintf (int level, const char *msg, ...)
{
	va_list argptr;
	char text[1024];

	/* don't confuse non-developers with techie stuff... */
	if (!developer || developer->integer == 0)
		return;

	if (developer->integer != DEBUG_ALL && developer->integer & ~level)
		return;

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	text[sizeof(text) - 1] = 0;

	gi.dprintf("%s", text);
}
#endif

/*====================================================================== */


/**
 * @brief If password has changed, update sv_needpass cvar as needed
 */
static void CheckNeedPass (void)
{
	if (password->modified) {
		char *need = "0";
		password->modified = qfalse;

		if (*password->string && Q_stricmp(password->string, "none"))
			need = "1";

		gi.Cvar_Set("sv_needpass", need);
	}
}

/**
 * @brief Sends character stats like assigned missions and kills back to client
 *
 * @note first short is the ucn to allow the client to identify the character
 * @note parsed in CL_ParseCharacterData
 * @sa CL_ParseCharacterData
 * @sa G_EndGame
 * @sa CL_SendCurTeamInfo
 * @note you also have to update the pascal string size in G_EndGame if you change the buffer here
 */
static void G_SendCharacterData (const edict_t* ent)
{
	int k;

	assert(ent);

	/* write character number */
	gi.WriteShort(ent->chr.ucn);

	gi.WriteShort(ent->HP);
	gi.WriteByte(ent->STUN);
	gi.WriteByte(ent->morale);

	/** Scores @sa inv_shared.h:chrScoreGlobal_t */
	for (k = 0; k < SKILL_NUM_TYPES + 1; k++)
		gi.WriteLong(ent->chr.score.experience[k]);
	for (k = 0; k < SKILL_NUM_TYPES; k++)
		gi.WriteByte(ent->chr.score.skills[k]);
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.WriteShort(ent->chr.score.kills[k]);
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.WriteShort(ent->chr.score.stuns[k]);
	gi.WriteShort(ent->chr.score.assignedMissions);
	gi.WriteByte(ent->chr.score.rank);
}

/**
 * @brief Determines the amount of XP earned by a given soldier for a given skill, based on the soldier's performance in the last mission.
 * @param[in] skill The skill for which to fetch the maximum amount of XP.
 * @param[in] chr Pointer to the character you want to get the earned experience for
 * @sa G_UpdateCharacterSkills
 * @sa G_GetMaxExperiencePerMission
 */
static int G_GetEarnedExperience (abilityskills_t skill, character_t *chr)
{
	int exp = 0;
	abilityskills_t i;

	switch (skill) {
	case ABILITY_POWER:
		exp = 46; /** @todo Make a formula for this once strength is used in combat. */
		break;
	case ABILITY_SPEED:
		exp = chr->scoreMission->movedNormal / 2 + chr->scoreMission->movedCrouched + (chr->scoreMission->firedTUs[skill] + chr->scoreMission->firedSplashTUs[skill]) / 10;
		break;
	case ABILITY_ACCURACY:
		for (i = 0; i < SKILL_NUM_TYPES; i++) {
			if (i == SKILL_SNIPER)
				exp = 30 * (chr->scoreMission->hits[i][KILLED_ALIENS] + chr->scoreMission->hitsSplash[i][KILLED_ALIENS]);
			else
				exp = 20 * (chr->scoreMission->hits[i][KILLED_ALIENS] + chr->scoreMission->hitsSplash[i][KILLED_ALIENS]);
		}
		break;
	case ABILITY_MIND:
		exp = 100 * chr->scoreMission->kills[KILLED_ALIENS];
		break;
	case SKILL_CLOSE:
		exp = 150 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	case SKILL_HEAVY:
		exp = 200 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	case SKILL_ASSAULT:
		exp = 100 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	case SKILL_SNIPER:
		exp = 200 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	case SKILL_EXPLOSIVE:
		exp = 200 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	default:
		Com_DPrintf(DEBUG_GAME, "G_GetEarnedExperience: invalid skill type\n");
		break;
	}

	return exp;
}

/**
 * @brief Updates character skills after a mission.
 * @param[in] chr Pointer to a character_t.
 * @sa CL_UpdateCharacterStats
 * @sa G_UpdateCharacterScore
 * @sa G_UpdateHitScore
 * @todo Update skill calculation with implant data once implants are implemented
 */
static void G_UpdateCharacterSkills (character_t *chr)
{
	abilityskills_t i = 0;
	unsigned int maxXP, gainedXP, totalGainedXP;

	if (!chr) {
		Com_DPrintf(DEBUG_GAME, "G_UpdateCharacterSkills: Bad character_t pointer given.");
		return;
	}

	/* Mostly for debugging */
	if (chr->emplType >= MAX_EMPL)
		Com_DPrintf(DEBUG_GAME, "G_UpdateCharacterSkills Soldier %s has employee-type of %i - please check if this is ok.\n", chr->name, chr->emplType);

	/* Robots/UGVs do not get skill-upgrades. */
	if (chr->emplType == EMPL_ROBOT)
		return;

	totalGainedXP = 0;
	for (i = 0; i < SKILL_NUM_TYPES; i++) {
		maxXP = CHRSH_CharGetMaxExperiencePerMission(i);
		gainedXP = G_GetEarnedExperience(i, chr);

		gainedXP = min(gainedXP, maxXP);
		chr->score.experience[i] += gainedXP;
		totalGainedXP += gainedXP;
		chr->score.skills[i] = chr->score.initialSkills[i] + (int) (pow((float) (chr->score.experience[i])/100, 0.6f));
		Com_DPrintf(DEBUG_GAME, "Soldier %s earned %d experience points in skill #%d (total experience: %d). It is now %d higher.\n", chr->name, gainedXP, i, chr->score.experience[i], chr->score.skills[i] - chr->score.initialSkills[i]);
	}

	/* Health isn't part of abilityskills_t, so it needs to be handled separately. */
	assert(i == SKILL_NUM_TYPES);	/**< We need to get sure that index for health-experience is correct. */
	maxXP = CHRSH_CharGetMaxExperiencePerMission(i);
	gainedXP = min(maxXP, totalGainedXP / 2);

	chr->score.experience[i] += gainedXP;
	chr->maxHP = chr->score.initialSkills[i] + (int) (pow((float) (chr->score.experience[i]) / 100, 0.6f));
	Com_DPrintf(DEBUG_GAME, "Soldier %s earned %d experience points in skill #%d (total experience: %d). It is now %d higher.\n", chr->name, gainedXP, i, chr->score.experience[i], chr->maxHP - chr->score.initialSkills[i]);
}

/**
 * @brief Handles the end of a game
 * @note Called by game_abort command (or sv win [team])
 * @sa G_RunFrame
 * @sa CL_ParseResults
 * @sa G_SendInventory
 */
void G_EndGame (int team)
{
	edict_t *ent;
	int i, j = 0;
	int	number_of_teams;

	G_PrintStats(va("End of game - Team %i is the winner", team));

	/** Calculate new scores/skills for the soldiers.
	 * @note In theory we do not have to check for ET_ACTOR2x2 actors (since phalanx has only robots that are 2x2),
	 * but we never know, maybe we have Hulk int hge future ;). */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++) {
		if (ent->inuse && G_IsLivingActor(ent) && ent->team == TEAM_PHALANX) {
			G_UpdateCharacterSkills(&ent->chr);
		}
	}

	/* if aliens won, make sure every soldier dies */
	if (team == TEAM_ALIEN) {
		level.num_alive[TEAM_PHALANX] = 0;
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
			if (ent->inuse && G_IsLivingActor(ent) && ent->team == TEAM_PHALANX) {
				ent->state = STATE_DEAD;
				ent->HP = 0;
				gi.AddEvent(PM_ALL, EV_ACTOR_STATECHANGE);
				gi.WriteShort(ent->number);
				gi.WriteShort(STATE_DEAD);
				level.num_kills[team][ent->team]++;
			}
		/* also kill all civilians */
		level.num_kills[team][TEAM_CIVILIAN] += level.num_alive[TEAM_CIVILIAN];
		level.num_alive[TEAM_CIVILIAN] = 0;
	}

	/* Make everything visible to anyone who can't already see it */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
		if (ent->inuse) {
			G_AppearPerishEvent(~G_VisToPM(ent->visflags), 1, ent);
			if (ent->type == ET_ACTOR || ent->type == ET_ACTOR2x2)
				G_SendInventory(~G_TeamToPM(ent->team), ent);
		}

	/* send results */
	Com_DPrintf(DEBUG_GAME, "Sending results for game won by team %i.\n", team);
	gi.AddEvent(PM_ALL, EV_RESULTS);
	number_of_teams = MAX_TEAMS;
	gi.WriteByte(number_of_teams);
	gi.WriteByte(team);

	for (i = 0; i < number_of_teams; i++) {
		gi.WriteByte(level.num_spawned[i]);
		gi.WriteByte(level.num_alive[i]);
	}

	for (i = 0; i < number_of_teams; i++)
		for (j = 0; j < number_of_teams; j++) {
			gi.WriteByte(level.num_kills[i][j]);
		}

	for (i = 0; i < number_of_teams; i++)
		for (j = 0; j < number_of_teams; j++) {
			gi.WriteByte(level.num_stuns[i][j]);
		}

	/* how many actors */
	for (j = 0, i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_ACTOR2x2)
			 && ent->team == TEAM_PHALANX)
			j++;

	Com_DPrintf(DEBUG_GAME, "Sending results with %i actors.\n", j);

	/* number of soldiers */
	gi.WriteByte(j);

	if (j) {
		for (i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
			if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_ACTOR2x2)
				 && ent->team == TEAM_PHALANX) {
				Com_DPrintf(DEBUG_GAME, "Sending results for actor %i.\n", i);
				G_SendCharacterData(ent);
			}
	}

	gi.EndEvents();
}


#if 0
/**
 * @brief checks for a mission objective
 * @param[in] activeTeams The number of teams with living actors
 * @return 1 if objective successful
 * @return 0 if objective unsuccessful
 * @return -1 if objective state unclear
 */
int G_MissionObjective (int activeTeams, int* winningTeam)
{
	/* @todo: put objective flag to level */
	switch (level.objective) {
	/* @todo: enum for objectives */
	case OBJ_RESCUE_CIVILIANS:
		if (!level.num_alive[TEAM_CIVILIAN])
			return 0;
		if (!level.num_alive[TEAM_ALIEN])
			return 1;
	/* @todo: More objectives */
	default:
		return -1;
	}
}
#endif

/**
 * @brief Checks whether there are still actors to fight with left
 * @sa G_EndGame
 */
void G_CheckEndGame (void)
{
	int activeTeams;
	int i, last;

	if (level.intermissionTime) /* already decided */
		return;

	/* FIXME: count from 0 to get the civilians for objectives */
	for (i = 1, activeTeams = 0, last = 0; i < MAX_TEAMS; i++)
		if (level.num_alive[i]) {
			last = i;
			activeTeams++;
		}

	/* @todo: < 2 does not work when we count civilians */
	/* prepare for sending results */
	if (activeTeams < 2 /* || G_MissionObjective(activeTeams, &level.winningTeam) != -1*/ ) {
		if (activeTeams == 0)
			level.winningTeam = 0;
		else if (activeTeams == 1)
			level.winningTeam = last;
		level.intermissionTime = level.time + (last == TEAM_ALIEN ? 10.0 : 3.0);
	}
}

/**
 * @brief Checks whether the game is running (active team)
 * @returns true if there is an active team for the current round
 */
qboolean G_GameRunning (void)
{
	return (level.activeTeam != NO_ACTIVE_TEAM);
}

/**
 * @sa SV_RunGameFrame
 * @sa G_EndGame
 * @sa AI_Run
 * @return true if game reaches its end - false otherwise
 */
qboolean G_RunFrame (void)
{
	level.framenum++;
	/* server is running at 10 fps */
	level.time = level.framenum * SERVER_FRAME_SECONDS;
/*	Com_Printf("frame: %i   time: %f\n", level.framenum, level.time); */

	/* still waiting for other players */
	if (!G_GameRunning()) {
		if (sv_maxteams->modified) {
			/* inform the client */
			gi.ConfigString(CS_MAXTEAMS, va("%i", sv_maxteams->integer));
			sv_maxteams->modified = qfalse;
		}
	}

	if (sv_maxclients->integer > 1) {
		if (sv_roundtimelimit->modified) {
			/* some played around here - restart the count down */
			level.roundstartTime = level.time;
			/* don't allow smaller values here */
			if (sv_roundtimelimit->integer < 30 && sv_roundtimelimit->integer > 0) {
				Com_Printf("The minimum value for sv_roundtimelimit is 30\n");
				gi.Cvar_Set("sv_roundtimelimit", "30");
			}
			sv_roundtimelimit->modified = qfalse;
		}
		G_ForceEndRound();
	}

	/* check for intermission */
	if (level.intermissionTime && level.time > level.intermissionTime) {
		G_EndGame(level.winningTeam);
		G_PrintStats(va("End of game - Team %i is the winner", level.winningTeam));
#if 0
		/* It still happens that game results are send twice --- dangerous */

		/* if the message gets lost, the game will not end
		 * until you kill someone else, so we'll try again later,
		 * but much later, so that the intermission animations can end */
		level.intermissionTime = level.time + 10.0;
#endif
		level.intermissionTime = 0.0;
		/* end this game */
		return qtrue;
	}

	CheckNeedPass();

	/* run ai */
	AI_Run();
	G_PhysicsRun();

	return qfalse;
}
