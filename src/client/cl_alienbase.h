/**
 * @file cl_alienbase.h
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

#ifndef CLIENT_CL_ALIENBASE_H
#define CLIENT_CL_ALIENBASE_H

#define MAX_ALIEN_BASES		8

/** @brief Alien Base */
typedef struct alienBase_s {
	int idx;				/**< idx of base in alienBases[] */
	vec2_t pos;			/**< position of the base (longitude, latitude) */
	int supply;			/**< Number of supply missions this base was already involved in */
	float stealth;		/**< How much PHALANX know this base. Decreases depending on PHALANX observation
						 * and base is known if stealth < 0 */
} alienBase_t;

void AB_ResetAlienBases(void);
void AB_SetAlienBasePosition(vec2_t position);
alienBase_t* AB_BuildBase(vec2_t pos);
void AB_DestroyBase(alienBase_t *base);
alienBase_t* AB_GetBase(int baseIDX, qboolean checkIdx);
void AB_UpdateStealthForAllBase(const aircraft_t *aircraft, int dt);
void AB_BaseSearchedByNations(void);
qboolean AB_CheckSupplyMissionPossible(void);
alienBase_t* AB_ChooseBaseToSupply(vec2_t pos);
void AB_SupplyBase(alienBase_t *base, qboolean decreaseStealth);
int AB_GetAlienBaseNumber(void);
void AB_InitStartup(void);

#endif	/* CLIENT_CL_ALIENBASE_H */
