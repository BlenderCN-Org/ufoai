/**
 * @file cl_global.h
 * @brief Defines global data structure.
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

#ifndef CLIENT_CL_GLOBAL_H
#define CLIENT_CL_GLOBAL_H

#include "cl_research.h"
#include "cl_basemanagement.h"

#define MAX_PROJECTILESONGEOSCAPE 32

#define MAX_ALIEN_TEAM_LEVEL	4

/**
 * @brief This struct/variable holds the global data for a game.
 * @note This struct will be saved directly to the savegame-file.
 * Everything that should be saved needs to be in here.
 * No pointers are allowed inside this struct. Use an index of an array instead.
 * @note If you need pointers you have to change CL_UpdatePointersInGlobalData, too
 * @sa CL_UpdatePointersInGlobalData
 * @sa CL_ReadSinglePlayerData
 */
typedef struct globalData_s
{
	/* == technolgies == */
	/* A list of all research-topics resp. the research-tree. */
	technology_t technologies[MAX_TECHNOLOGIES];
	/* Total nubmer of technologies. */
	int numTechnologies;

	/* == pedia == */
	/* A list of all UFOpaedia chapters. */
	pediaChapter_t upChapters[MAX_PEDIACHAPTERS];
	/* Total number uf UFOpaedia chapters */
	int numChapters;
	int numUnreadMails; /**< only for faster access (don't cycle all techs every frame) */

	eventMail_t eventMails[MAX_EVENTMAILS];	/**< holds all event mails (cl_event.c) */
	int numEventMails;	/**< how many eventmails (script-id: mail) parsed */

	/* == employees == */
	/* A list of all phalanx employees (soldiers, scies, workers, etc...) */
	employee_t employees[MAX_EMPL][MAX_EMPLOYEES];
	/* Total number of employees. */
	int numEmployees[MAX_EMPL];

	/* == bases == */
	/* A list of _all_ bases ... even unbuilt ones. */
	base_t bases[MAX_BASES];
	/* used for unique aircraft ids */
	int numAircraft;
	/* Total number of parsed base-names. */
	int numBaseNames;
	/* Total number of built bases (how many are enabled). */
	int numBases;

	/* == buildings in bases == */
	/* A list of all possible unique buldings. */
	building_t buildingTemplates[MAX_BUILDINGS];
	int numBuildingTemplates;
	/*  A list of the building-list per base. (new buildings in a base get copied from buildingTypes) */
	building_t buildings[MAX_BASES][MAX_BUILDINGS];
	/* Total number of buildings per base. */
	int numBuildings[MAX_BASES];

	/* == misc == */
	/* MA_NEWBASE, MA_INTERCEPT, MA_BASEATTACK, ... */
	mapAction_t mapAction;
	/* BA_NEWBUILDING ... */
	baseAction_t baseAction;
	/* how fast the game is running */
	int gameTimeScale;
	/* selected aircraft for interceptions */
	aircraft_t *interceptAircraft;
	/* already paid in this month? */
	qboolean fund;
	/* missions last seen on geoscape */
	char oldMis1[MAX_VAR];
	char oldMis2[MAX_VAR];
	char oldMis3[MAX_VAR];
	qboolean autosell[MAX_OBJDEFS];		/**< True if this objDef_t has autosell enabled. */

	/* == production == */
	/* we will allow only one active production at the same time for each base */
	/* NOTE The facility to produce equipment should have the once-flag set */
	production_queue_t productions[MAX_BASES];

	/* == Ranks == */
	/* Global list of all ranks defined in medals.ufo. */
	rank_t ranks[MAX_RANKS];
	/* The number of entries in the list above. */
	int numRanks;

	/* == Nations == */
	nation_t nations[MAX_NATIONS];
	int numNations;

	/* == UGVs == */
	ugv_t ugvs[MAX_UGV];
	int numUGV;

	/* unified character id */
	int nextUCN;

	/* == Aircraft == */
	/* UFOs on geoscape: @todo update their inner pointers if needed */
	aircraft_t ufos[MAX_UFOONGEOSCAPE];
	int numUFOs;	/**< The current amount of UFOS on the geoscape. */

	/* Projectiles on geoscape (during fights) */
	aircraftProjectile_t projectiles[MAX_PROJECTILESONGEOSCAPE];
	int numProjectiles;

	/* All transfers. */
	transfer_t alltransfers[MAX_TRANSFERS];

	/* UFO recoveries. */
	ufoRecoveries_t recoveries[MAX_RECOVERIES];

	/* UFO components. */
	int numComponents;
	components_t components[MAX_ASSEMBLIES];

	/* Alien Team Definitions. */
	int numAliensTD;

	/* Alien Team Package used during battle */
	teamDef_t *alienTeams[ALIENTEAM_MAX][MAX_ALIEN_TEAM_LEVEL][MAX_TEAMDEFS];	/**< different alien team available
																	 * that will be used in mission */
	int numAlienTeams[ALIENTEAM_MAX];		/** number of alienTeams defined */
} globalData_t;


extern globalData_t gd;

globalData_t gd;


#endif /* CLIENT_CL_GLOBAL_H */
