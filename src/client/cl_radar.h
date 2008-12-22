/**
 * @file cl_radar.h
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_RADAR_H
#define CLIENT_CL_RADAR_H

#include "menu/m_nodes.h"

#define MAX_UFOONGEOSCAPE	8

extern qboolean radarOverlayWasSet;

extern const float RADAR_BASERANGE;
extern const float RADAR_BASERTRACKINGANGE;
extern const float RADAR_AIRCRAFTRANGE;
extern const float RADAR_AIRCRAFTTRACKINGRANGE;

/**
 * @brief Different status for numAmmo.
 */
enum {
	UFO_NOT_SENSORED = -1,			/**< Radar has no UFO Sensored. */

	/* positive or null values are used for UFO idx */
};

typedef struct radar_s {
	int range;						/**< Range of radar */
	int trackingRange;				/**< Tracking range of radar */
	int ufos[MAX_UFOONGEOSCAPE];	/**< UFOs id sensored by radar (gd.ufos[id]). UFO_NOT_SENSORED if no UFO sensored. */
	int numUFOs;					/**< Num UFOs sensored by radar */
} radar_t;

void RADAR_UpdateStaticRadarCoverage(void);
void RADAR_UpdateWholeRadarOverlay(void);
void RADAR_DrawInMap(const struct menuNode_s* node, const radar_t* radar, const vec2_t pos);
void RADAR_DeactivateRadarOverlay(void);
void RADAR_NotifyUFORemoved(const struct aircraft_s* ufo, qboolean destroyed);
void RADAR_Initialise(radar_t* radar, float range, float trackingRange, float level, qboolean updateSourceRadarMap);
void RADAR_InitialiseUFOs(radar_t *radar);
void RADAR_UpdateBaseRadarCoverage_f(void);
void RADAR_UpdateInstallationRadarCoverage(struct installation_s *installation, const float radarRange, const float trackingRadarRange);
void RADAR_AddDetectedUFOToEveryRadar(const struct aircraft_s const *ufo);
qboolean RADAR_CheckRadarSensored(const vec2_t pos);
qboolean RADAR_CheckUFOSensored(radar_t* radar, vec2_t posRadar, const struct aircraft_s* ufo, qboolean detected);
void RADAR_SetRadarAfterLoading(void);

#endif
