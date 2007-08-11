/**
 * @file cl_map.h
 * @brief Header for Geoscape/Map management
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

#ifndef CLIENT_CL_MAP_H
#define CLIENT_CL_MAP_H

#define GLOBE_ROTATE -90
#define MAX_PROJECTILESONGEOSCAPE 16
nation_t* MAP_GetNation(const vec2_t pos);
qboolean MAP_AllMapToScreen(const menuNode_t* node, const vec2_t pos, int *x, int *y, int *z);
qboolean MAP_Draw3DMarkerIfVisible(const menuNode_t* node, const vec2_t pos, float angle, const char *model, qboolean globe);
void MAP_MapDrawEquidistantPoints(const menuNode_t* node, vec2_t center, const float angle, const vec4_t color, qboolean globe);
float MAP_AngleOfPath(const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector, qboolean qtrue);
void MAP_MapCalcLine(const vec2_t start, const vec2_t end, mapline_t* line);
void MAP_DrawMap(const menuNode_t* node, qboolean map3D);
void MAP_CenterOnPoint(void);
void MAP_MapClick(const menuNode_t * node, int x, int y, qboolean globe);
void MAP_ResetAction(void);
void MAP_SelectAircraft(aircraft_t* aircraft);
void MAP_SelectMission(actMis_t* mission);
void MAP_NotifyMissionRemoved(const actMis_t* mission);
void MAP_NotifyUfoRemoved(const aircraft_t* ufo);
void MAP_NotifyUfoDisappear(const aircraft_t* ufo);
void MAP_GameInit(void);
const char* MAP_GetZoneType(byte* color);
float MAP_GetDistance(const vec2_t pos1, const vec2_t pos2);
qboolean MAP_IsNight(vec2_t pos);
qboolean MAP_MaskFind(byte * color, vec2_t polar);
byte *MAP_GetColor(const vec2_t pos, mapType_t type);
void MAP_Init(void);

#endif /* CLIENT_CL_MAP_H */
