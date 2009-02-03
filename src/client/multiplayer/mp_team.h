/**
 * @file mp_team.h
 * @brief Multiplayer team management headers.
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

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CL_TEAM_MULTIPLAYER_H
#define CL_TEAM_MULTIPLAYER_H

#define MAX_MULTIPLAYER_CHARACTERS 32

extern character_t multiplayerCharacters[MAX_MULTIPLAYER_CHARACTERS];

void MP_SaveTeamMultiplayerSlot_f(void);
void MP_LoadTeamMultiplayerSlot_f(void);
void MP_MultiplayerTeamSlotComments_f(void);
void MP_UpdateMenuParameters_f(void);

#endif
