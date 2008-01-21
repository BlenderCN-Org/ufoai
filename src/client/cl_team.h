/**
 * @file cl_team.h
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

#ifndef CLIENT_CL_TEAM_H
#define CLIENT_CL_TEAM_H

#define MAX_WHOLETEAM	32

#define NUM_TEAMSKINS	6
#define NUM_TEAMSKINS_SINGLEPLAYER 4

qboolean CL_SoldierAwayFromBase(employee_t *soldier);
void CL_UpdateHireVar(aircraft_t *aircraft, employeeType_t employeeType);
void CL_ReloadAndRemoveCarried(aircraft_t *aircraft, equipDef_t * equip);
void CL_CleanTempInventory(base_t* base);

void CL_ResetCharacters(base_t* const base);
void CL_ResetTeamInBase(void);
void CL_GenerateCharacter(employee_t *employee, const char *team, employeeType_t employeeType);
const char* CL_GetTeamSkinName(int id);

qboolean CL_SoldierInAircraft(int employee_idx, employeeType_t employeeType, int aircraft_idx);
void CL_RemoveSoldierFromAircraft(int employee_idx, employeeType_t employeeType, int aircraft_idx, struct base_s *base);
void CL_RemoveSoldiersFromAircraft(int aircraft_idx, struct base_s *base);

void CL_SaveInventory(sizebuf_t * buf, inventory_t * i);
void CL_NetReceiveItem(struct dbuffer * buf, item_t * item, int * container, int * x, int * y);
void CL_LoadInventory(sizebuf_t * buf, inventory_t * i);
void CL_ResetTeams(void);
void CL_ParseResults(struct dbuffer *msg);
void CL_SendCurTeamInfo(struct dbuffer * buf, chrList_t *team);
void CL_AddCarriedToEq(struct aircraft_s *aircraft, equipDef_t * equip);
void CL_ParseCharacterData(struct dbuffer *msg, qboolean updateCharacter);

void CL_UpdateCharacterSkills(character_t *chr);

#endif
