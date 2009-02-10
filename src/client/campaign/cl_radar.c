/**
 * @file cl_radar.c
 * @brief Radars / sensor stuff, to detect and track ufos
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

#include "../client.h"
#include "../cl_global.h"
#include "cl_map.h"
#include "cl_ufo.h"
#include "../renderer/r_draw.h"
#include "../menu/m_nodes.h"

void R_AddRadarCoverage(const vec2_t pos, float innerRadius, float outerRadius, qboolean source);
void R_InitializeRadarOverlay(qboolean source);
void R_UploadRadarCoverage(qboolean smooth);

/**
 * used to store the previous configuration of overlay before radar
 * is automatically turned on (e.g when creating base or when UFO appears)
 */
qboolean radarOverlayWasSet;

/* Define base radar range (can be modified by level of the radar) */
const float RADAR_BASERANGE = 24.0f;
const float RADAR_BASETRACKINGRANGE = 34.0f;
const float RADAR_AIRCRAFTRANGE = 10.0f;
const float RADAR_AIRCRAFTTRACKINGRANGE = 14.0f;
const float RADAR_INSTALLATIONLEVEL = 1.0f;
/** @brief this is the multiplier applied to the radar range when the radar levels up */
static const float RADAR_UPGRADE_MULTIPLIER = 0.4f;

/**
 * @brief Update every static radar drawing (radar that don't move: base and installation radar).
 * @note This is only called when radar range of bases change.
 */
void RADAR_UpdateStaticRadarCoverage (void)
{
	int baseIdx, installationIdx;

	/* Initialise radar range (will be filled below) */
	R_InitializeRadarOverlay(qtrue);

	/* Add base radar coverage */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(baseIdx);
		if (base && base->radar.range) {
			R_AddRadarCoverage(base->pos, base->radar.range, base->radar.trackingRange, qtrue);
		}
	}

	/* Add installation coverage */
	for (installationIdx = 0; installationIdx < MAX_INSTALLATIONS; installationIdx++) {
		const installation_t const *installation = INS_GetFoundedInstallationByIDX(installationIdx);
		if (installation && installation->founded &&
			installation->installationStatus == INSTALLATION_WORKING && installation->radar.range) {
			R_AddRadarCoverage(installation->pos, installation->radar.range, installation->radar.trackingRange, qtrue);
		}
	}

	/* Smooth and bind radar overlay without aircraft (in case no aircraft is on geoscape:
	 * RADAR_UpdateWholeRadarOverlay won't be called) */
	R_InitializeRadarOverlay(qfalse);
	R_UploadRadarCoverage(qtrue);
}

/**
 * @brief Update map radar coverage with moving radar
 * @sa RADAR_UpdateWholeRadarOverlay
 */
static inline void RADAR_DrawCoverage (const radar_t* radar, const vec2_t pos)
{
	R_AddRadarCoverage(pos, radar->range, radar->trackingRange, qfalse);
}

/**
 * @brief Update radar overlay of base, installation and aircraft range.
 */
void RADAR_UpdateWholeRadarOverlay (void)
{
	int baseIdx;

	/* Copy Base and installation radar overlay*/
	R_InitializeRadarOverlay(qfalse);

	/* Add aircraft radar coverage */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(baseIdx);
		int aircraftIdx;
		if (!base)
			continue;

		for (aircraftIdx = 0; aircraftIdx < base->numAircraftInBase; aircraftIdx++) {
			if (AIR_IsAircraftOnGeoscape(&base->aircraft[aircraftIdx]))
				RADAR_DrawCoverage(&base->aircraft[aircraftIdx].radar, base->aircraft[aircraftIdx].pos);
		}
	}

	/* Smooth Radar Coverage and bind it */
	R_UploadRadarCoverage(qtrue);
}

/**
 * @brief Draw only the "wire" Radar coverage.
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 * @sa MAP_MapDrawEquidistantPoints
 */
static void RADAR_DrawLineCoverage (const menuNode_t* node, const radar_t* radar, const vec2_t pos)
{
	const vec4_t color = {1., 1., 1., .4};

	/* Set color */
	R_Color(color);

	MAP_MapDrawEquidistantPoints(node, pos, radar->range, color);
	MAP_MapDrawEquidistantPoints(node, pos, radar->trackingRange, color);

	R_Color(NULL);
}

/**
 * @brief Draw only the "wire" part of the radar coverage in geoscape
 * @param[in] node The menu node where radar coverage will be drawn.
 * @param[in] radar Pointer to the radar that will be drawn.
 * @param[in] pos Position of the radar.
 */
void RADAR_DrawInMap (const menuNode_t *node, const radar_t *radar, const vec2_t pos)
{
	int x, y;
	int i;
	const vec4_t color = {1., 1., 1., .3};
	screenPoint_t pts[2];
	qboolean display;

	/* Show radar range zones */
	RADAR_DrawLineCoverage(node, radar, pos);

	/* everything below is drawn only if there is at least one detected UFO */
	if (!radar->numUFOs)
		return;

	/* Set color */
	R_ColorBlend(color);

	/* Draw lines from radar to ufos sensored */
	display = MAP_AllMapToScreen(node, pos, &x, &y, NULL);
	pts[0].x = x;
	pts[0].y = y;
	for (i = 0; i < radar->numUFOs; i++) {
		/* we really want to avoid buffer overflow: numUFO is supposed to be UFO idx */
		assert(radar->ufos[i] < MAX_UFOONGEOSCAPE && radar->ufos[i] >= 0);
		if (UFO_IsUFOSeenOnGeoscape(&ccs.ufos[radar->ufos[i]])						/* Is UFO landed? */
			&& MAP_AllMapToScreen(node, ccs.ufos[radar->ufos[i]].pos, &x, &y, NULL)	/* UFO is on the screen */
			&& display) {															/* radar is on the screen */
			pts[1].x = x;
			pts[1].y = y;
			R_DrawLineStrip(2, (int*)pts); /** @todo */
		}
	}

	R_ColorBlend(NULL);
}

/**
 * @brief Deactivate Radar overlay if there is no more UFO on geoscape
 */
void RADAR_DeactivateRadarOverlay (void)
{
	int idx, aircraftIdx;

	/* never deactivate radar if player wants it to be alwayse turned on */
	if (radarOverlayWasSet)
		return;

	for (idx = 0; idx < MAX_BASES; idx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(idx);
		if (!base)
			continue;

		if (base->radar.numUFOs)
			return;

		for (aircraftIdx = 0; aircraftIdx < base->numAircraftInBase; aircraftIdx++) {
			const aircraft_t const *aircraft = &base->aircraft[aircraftIdx];
			if (aircraft->radar.numUFOs)
				return;
		}
	}

	for (idx = 0; idx < MAX_INSTALLATIONS; idx++) {
		const installation_t const *installation = INS_GetFoundedInstallationByIDX(idx);
		if (!installation)
			continue;

		if (installation->radar.numUFOs)
			return;
	}

	if (r_geoscape_overlay->integer & OVERLAY_RADAR)
		MAP_SetOverlay("radar");
}

/**
 * @brief Check if UFO is sensored list, and return its position in list (UFO_NOT_SENSORED if not)
 */
static int RADAR_IsUFOSensored (const radar_t* radar, int numUFO)
{
	int i;

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == numUFO)
			return i;

	return UFO_NOT_SENSORED;
}

/**
 * @brief Add a UFO in the list of sensored UFOs
 */
static qboolean RADAR_AddUFO (radar_t* radar, int numUFO)
{
#ifdef DEBUG
	const int num = RADAR_IsUFOSensored(radar, numUFO);
	if (num != UFO_NOT_SENSORED) {
		Com_Printf("RADAR_AddUFO: Aircraft already in radar range\n");
		return qfalse;
	}
#endif

	if (radar->numUFOs >= MAX_UFOONGEOSCAPE)
		return qfalse;

	/* we really want to avoid buffer overflow: numUFO is supposed to be UFO idx */
	assert(numUFO < MAX_UFOONGEOSCAPE && numUFO >= 0);

	radar->ufos[radar->numUFOs] = numUFO;
	radar->numUFOs++;

	return qtrue;
}

/**
 * @brief UFO will no more be referenced by radar
 */
static void RADAR_RemoveUFO (radar_t* radar, const aircraft_t* ufo)
{
	int i, numUFO = ufo - ccs.ufos;

	assert(radar->numUFOs < MAX_UFOONGEOSCAPE && radar->numUFOs > 0);

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == numUFO) {
			break;
		}
	REMOVE_ELEM(radar->ufos, i, radar->numUFOs);
	/* default value is not 0 but UFO_NOT_SENSORED */
	radar->ufos[radar->numUFOs] = UFO_NOT_SENSORED;

	RADAR_DeactivateRadarOverlay();
}

/**
 * @brief Notify that the specified ufo has been removed from geoscape to one radar.
 * @param[in] radar Pointer to the radar where ufo should be removed.
 * @param[in] ufo Pointer to UFO to remove.
 * @param[in] destroyed True if the UFO has been destroyed, false if it's been only set invisible (landed)
 **/
static void RADAR_NotifyUFORemovedFromOneRadar (radar_t* radar, const aircraft_t* ufo, qboolean destroyed)
{
	int i, numUFO = ufo - ccs.ufos;

	for (i = 0; i < radar->numUFOs; i++)
		if (radar->ufos[i] == numUFO) {
			radar->numUFOs--;
			radar->ufos[i] = radar->ufos[radar->numUFOs];
			i--;	/* Allow the moved value to be checked */
		} else if (destroyed && (radar->ufos[i] > numUFO))
			radar->ufos[i]--;

	RADAR_DeactivateRadarOverlay();
}

/**
 * @brief Notify to every radar that the specified ufo has been removed from geoscape
 * @param[in] ufo Pointer to UFO to remove.
 * @param[in] destroyed True if the UFO has been destroyed, false if it's only landed.
 **/
void RADAR_NotifyUFORemoved (const aircraft_t* ufo, qboolean destroyed)
{
	int baseIdx;
	int installationIdx;
	aircraft_t *aircraft;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		RADAR_NotifyUFORemovedFromOneRadar(&base->radar, ufo, destroyed);

		for (aircraft = base->aircraft; aircraft < base->aircraft + base->numAircraftInBase; aircraft++)
			RADAR_NotifyUFORemovedFromOneRadar(&aircraft->radar, ufo, destroyed);
	}

	for (installationIdx = 0; installationIdx < MAX_INSTALLATIONS; installationIdx++) {
		installation_t *installation = INS_GetFoundedInstallationByIDX(installationIdx);
		if (installation && installation->founded && installation->installationStatus == INSTALLATION_WORKING) {
			RADAR_NotifyUFORemovedFromOneRadar(&installation->radar, ufo, destroyed);
		}
	}
}

/**
 * @brief Set radar range to new value
 * @param[in,out] radar The radar to update/initialize
 * @param[in] range New range of the radar
 * @param[in] level The tech level of the radar
 * @param[in] updateSourceRadarMap
 */
void RADAR_Initialise (radar_t *radar, float range, float trackingRange, float level, qboolean updateSourceRadarMap)
{
	const int oldrange = radar->range;

	if (!level) {
		radar->range = 0.0f;
		radar->trackingRange = 0.0f;
	} else {
		radar->range = range * (1 + (level - 1) * RADAR_UPGRADE_MULTIPLIER);
		radar->trackingRange = trackingRange * (1 + (level - 1) * RADAR_UPGRADE_MULTIPLIER);
	}

	assert(radar->numUFOs >= 0);

	if (updateSourceRadarMap && !equal(radar->range,oldrange)) {
		RADAR_UpdateStaticRadarCoverage();
		RADAR_UpdateWholeRadarOverlay();
	}
}

/**
 * @brief Reset UFO sensored on radar.
 * @param[out] radar The radar to initialize.
 */
void RADAR_InitialiseUFOs (radar_t *radar)
{
	int i;

	radar->numUFOs = 0;

	for (i = 0; i < MAX_UFOONGEOSCAPE; i++)
		radar->ufos[i] = UFO_NOT_SENSORED;
}

/**
 * @brief Update radar coverage when building/destroying new radar
 * @note This must be called on each radar build/destruction because radar facilities may have different level.
 * @note This must also be called when radar installation become inactive or active (due to dependencies)
 * @note called with update_base_radar_coverage
 */
void RADAR_UpdateBaseRadarCoverage_f (void)
{
	int baseIdx;
	base_t *base;
	float level;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <baseIdx> <buildingType>\n", Cmd_Argv(0));
		return;
	}

	baseIdx = atoi(Cmd_Argv(1));

	if (baseIdx < 0 || baseIdx >= MAX_BASES) {
		Com_Printf("RADAR_UpdateBaseRadarCoverage_f: %i is outside bounds\n", baseIdx);
		return;
	}

	base = B_GetFoundedBaseByIDX(baseIdx);

	if (!base)
		return;

	level = B_GetMaxBuildingLevel(base, B_RADAR);
	RADAR_Initialise(&base->radar, RADAR_BASERANGE, RADAR_BASETRACKINGRANGE, level, qtrue);
	if (!loading)
		CP_UpdateMissionVisibleOnGeoscape();
}

/**
 * @brief Update radar coverage when building/destroying new radar
 */
void RADAR_UpdateInstallationRadarCoverage (installation_t *installation, const float radarRange, const float trackingRadarRange)
{
	if (installation && installation->founded && (installation->installationStatus == INSTALLATION_WORKING)) {
		RADAR_Initialise(&installation->radar, radarRange, trackingRadarRange, RADAR_INSTALLATIONLEVEL, qtrue);
		if (!loading)
			CP_UpdateMissionVisibleOnGeoscape();
	}
}

/**
 * @brief Adds detected UFO to any radar in range (if not already detected).
 * @param[in] ufo Pointer to the UFO to check.
 */
void RADAR_AddDetectedUFOToEveryRadar (const aircraft_t const *ufo)
{
	int idx;

	for (idx = 0; idx < MAX_BASES; idx++) {
		base_t *base = B_GetFoundedBaseByIDX(idx);
		int aircraftIdx;

		if (!base)
			continue;

		if (RADAR_IsUFOSensored(&base->radar, ufo - ccs.ufos) == UFO_NOT_SENSORED) {
			const float dist = MAP_GetDistance(ufo->pos, base->pos);	/* Distance from radar to UFO */
			if (dist <= base->radar.trackingRange)
				RADAR_AddUFO(&base->radar, ufo - ccs.ufos);
		}

		for (aircraftIdx = 0; aircraftIdx < base->numAircraftInBase; aircraftIdx++) {
			aircraft_t *aircraft = &base->aircraft[aircraftIdx];

			if (!AIR_IsAircraftOnGeoscape(aircraft))
				continue;

			if (RADAR_IsUFOSensored(&aircraft->radar, ufo - ccs.ufos) == UFO_NOT_SENSORED) {
				const float dist = MAP_GetDistance(ufo->pos, aircraft->pos);	/* Distance from radar to UFO */
				if (dist <= aircraft->radar.trackingRange)
					RADAR_AddUFO(&aircraft->radar, ufo - ccs.ufos);
			}
		}
	}

	for (idx = 0; idx < MAX_INSTALLATIONS; idx++) {
		installation_t *installation = INS_GetFoundedInstallationByIDX(idx);

		if (!installation)
			continue;

		/* No need to check installations without radar */
		if (!installation->radar.trackingRange)
			continue;

		if (RADAR_IsUFOSensored(&installation->radar, ufo - ccs.ufos) == UFO_NOT_SENSORED) {
			const float dist = MAP_GetDistance(ufo->pos, installation->pos);	/* Distance from radar to UFO */
			if (dist <= ufo->radar.trackingRange)
				RADAR_AddUFO(&installation->radar, ufo - ccs.ufos);
		}
	}
}

/**
 * @brief Check if the specified position is within base radar range
 * @note aircraft radars are not checked (and this is intended)
 * @return true if the position is inside one of the base radar range
 */
qboolean RADAR_CheckRadarSensored (const vec2_t pos)
{
	int idx;

	for (idx = 0; idx < MAX_BASES; idx++) {
		const base_t const *base = B_GetFoundedBaseByIDX(idx);
		float dist;
		if (!base)
			continue;

		dist = MAP_GetDistance(pos, base->pos);		/* Distance from base to position */
		if (dist <= base->radar.range)
			return qtrue;
	}

	for (idx = 0; idx < MAX_INSTALLATIONS; idx++) {
		const installation_t const *installation = INS_GetFoundedInstallationByIDX(idx);
		float dist;
		if (!installation)
			continue;

		dist = MAP_GetDistance(pos, installation->pos);		/* Distance from base to position */
		if (dist <= installation->radar.range)
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Check if the specified UFO is inside the sensor range of the given radar
 * @param[in] radar radar that may detect the UFO.
 * @param[in] posRadar Position of @c radar
 * @param[in] ufo aircraft that should be checked.
 * @param[in] detected Is the UFO already detected by another radar? (Beware: this is not the same as ufo->detected)
 * @return true if the aircraft is inside sensor and was sensored
 * @sa UFO_CampaignCheckEvents
 * @sa CP_CheckNewMissionDetectedOnGeoscape
 */
qboolean RADAR_CheckUFOSensored (radar_t *radar, vec2_t posRadar,
	const aircraft_t *ufo, qboolean detected)
{
	/** @brief Probability to detect UFO each @c DETECTION_INTERVAL
	 * @note This correspond to 40 percents each 30 minutes (coded this way to be able to
	 * change @c DETECTION_INTERVAL without changing the way radar works)
	 * @todo There is a hardcoded detection probability here
	 * - this should be scripted. Probability should be a
	 * function of UFO type and maybe radar type too. */
	const float ufoDetectionProbability = 0.000125f * DETECTION_INTERVAL;

	int dist;
	int num;
	int numAircraftSensored;

	/* Get num of ufo in ccs.ufos */
	/** @todo why not ufo->idx? Is this only valid for aircraft in base? */
	num = ufo - ccs.ufos;
	if (num < 0 || num >= ccs.numUFOs)
		return qfalse;

	numAircraftSensored = RADAR_IsUFOSensored(radar, num);	/* indice of ufo in radar list */
	dist = MAP_GetDistance(posRadar, ufo->pos);	/* Distance from radar to ufo */

	if ((detected ? radar->trackingRange : radar->range) > dist) {
		if (detected) {
			if (numAircraftSensored == UFO_NOT_SENSORED) {
				/* UFO was not sensored by this radar, but by another one (it just entered this radar zone) */
				RADAR_AddUFO(radar, num);
			}
			return qtrue;
		} else {
			/* UFO is sensored by no radar, so it shouldn't be sensored by this radar */
			assert(numAircraftSensored == UFO_NOT_SENSORED);
			/* Check if UFO is detected */
			if (frand() <= ufoDetectionProbability) {
				RADAR_AddDetectedUFOToEveryRadar(ufo);
				return qtrue;
			}
			return qfalse;
		}
	}

	/* UFO is not in this sensor range any more (but maybe in the range of another radar) */
	if (numAircraftSensored != UFO_NOT_SENSORED) {
		RADAR_RemoveUFO(radar, ufo);
	}
	return qfalse;
}

/**
 * @brief Set radar to proper values after loading
 * @note numUFOs is not saved, so we must calculate it.
 * @note should be called after loading.
 */
void RADAR_SetRadarAfterLoading (void)
{
	int ufoIdx;

	for (ufoIdx = 0; ufoIdx < ccs.numUFOs; ufoIdx++) {
		const aircraft_t const *ufo = &ccs.ufos[ufoIdx];

		if (!ufo->detected)
			continue;

		RADAR_AddDetectedUFOToEveryRadar(ufo);
	}
}
