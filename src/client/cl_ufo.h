/**
 * @file cl_ufo.h
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

#ifndef CLIENT_CL_UFO_H
#define CLIENT_CL_UFO_H

enum {
	UFO_IS_NO_TARGET,
	UFO_IS_TARGET_OF_MISSILE,
	UFO_IS_TARGET_OF_LASER
};

ufoType_t UFO_ShortNameToID(const char *token);
const char* UFO_TypeToShortName(ufoType_t type);
const char* UFO_TypeToName(ufoType_t type);
void UFO_SetRandomDest(aircraft_t* ufo);
void UFO_FleePhalanxAircraft(aircraft_t *ufo, const vec2_t v);
void UFO_CheckShootBack(aircraft_t *ufo, aircraft_t* phalanxAircraft);
void UFO_CampaignRunUFOs(int dt);
void UFO_CampaignCheckEvents(qboolean checkStatusChanged);
void UFO_Reset(void);
aircraft_t *UFO_AddToGeoscape(ufoType_t ufotype, vec2_t pos);
void UFO_RemoveFromGeoscape(aircraft_t* ufo);
void UFO_UpdateUFOHangarCapForAll(int base_idx);
void UFO_PrepareRecovery(base_t *base);
void UFO_Recovery(void);
qboolean UFO_ConditionsForStoring(const base_t *base, const aircraft_t *ufocraft);
qboolean UFO_CanShoot(const aircraft_t *ufo);
void UFO_SendToDestination(aircraft_t* ufo, vec2_t dest);
qboolean UFO_SendPursuingAircraft(aircraft_t* ufo, aircraft_t* aircraft);

#endif
