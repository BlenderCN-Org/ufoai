/**
 * @file cl_game_campaign.h
 * @brief Singleplayer campaign game type headers
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

#ifndef CL_GAME_CAMPAIGN_H
#define CL_GAME_CAMPAIGN_H

qboolean GAME_CP_IsRunning(void);

const mapDef_t* GAME_CP_MapInfo(int step);
void GAME_CP_InitStartup(void);
void GAME_CP_Shutdown(void);
qboolean GAME_CP_ItemIsUseable(const objDef_t *od);
void GAME_CP_Results(struct dbuffer *msg, int winner, int *numSpawned, int *numAlive, int numKilled[][MAX_TEAMS], int numStunned[][MAX_TEAMS]);
qboolean GAME_CP_Spawn(void);
int GAME_CP_GetTeam(void);

#endif
