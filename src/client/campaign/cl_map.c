/**
 * @file cl_map.c
 * @brief Geoscape/Map management
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
#include "../cl_game.h"
#include "../cl_screen.h"
#include "../renderer/r_draw.h"
#include "../cl_menu.h"
#include "../menu/m_popup.h"
#include "../menu/m_font.h"
#include "../menu/m_nodes.h"
#include "../menu/node/m_node_abstractnode.h"
#include "cl_campaign.h"
#include "cl_popup.h"
#include "cl_mapfightequip.h"
#include "cl_map.h"
#include "cp_missions.h"
#include "cl_ufo.h"
#include "cp_time.h"
#include "cp_xvi.h"

cvar_t *cl_3dmap;				/**< 3D geoscape or flat geoscape */
cvar_t *cl_mapzoommax;
cvar_t *cl_mapzoommin;

void R_UploadRadarCoverage(qboolean smooth);
void R_InitializeRadarOverlay(qboolean source);

/*
==============================================================
MULTI SELECTION DEFINITION
==============================================================
*/

#define MULTISELECT_MAXSELECT 6	/**< Maximal count of elements that can be selected at once */

/**
 * @brief Types of elements that can be selected in map
 */
typedef enum {
	MULTISELECT_TYPE_BASE,
	MULTISELECT_TYPE_INSTALLATION,
	MULTISELECT_TYPE_MISSION,
	MULTISELECT_TYPE_AIRCRAFT,
	MULTISELECT_TYPE_UFO,
	MULTISELECT_TYPE_NONE
} multiSelectType_t;


/**
 * @brief Structure to manage the multi selection
 */
typedef struct multiSelect_s {
	int nbSelect;						/**< Count of currently selected elements */
	multiSelectType_t selectType[MULTISELECT_MAXSELECT];	/**< Type of currently selected elements */
	int selectId[MULTISELECT_MAXSELECT];	/**< Identifier of currently selected element */
	char popupText[2048];				/**< Text to display in popup_multi_selection menu */
} multiSelect_t;

static multiSelect_t multiSelect;	/**< Data to manage the multi selection */


/*
==============================================================
STATIC DEFINITION
==============================================================
*/

/* Functions */
static qboolean MAP_IsMapPositionSelected(const menuNode_t* node, const vec2_t pos, int x, int y);
static void MAP3D_ScreenToMap(const menuNode_t* node, int x, int y, vec2_t pos);
static void MAP_ScreenToMap(const menuNode_t* node, int x, int y, vec2_t pos);

/* static variables */
aircraft_t *selectedAircraft;		/**< Currently selected aircraft */
aircraft_t *selectedUFO;			/**< Currently selected UFO */
static char text_standard[2048];		/**< Buffer to display standard text in geoscape */
static int centerOnEventIdx = 0;		/**< Current Event centered on 3D geoscape */
static const vec2_t northPole = {0.0f, 90.0f};	/**< Position of the north pole (used to know where is the 'up' side */

/* Colors */
static const vec4_t yellow = {1.0f, 0.874f, 0.294f, 1.0f};
static const vec4_t red = {1.0f, 0.0f, 0.0f, 0.8f};

/* Smoothing variables */
static qboolean smoothRotation = qfalse;	/**< qtrue if the rotation of 3D geoscape must me smooth */
static vec3_t smoothFinalGlobeAngle = {0, GLOBE_ROTATE, 0};	/**< value of finale ccs.angles for a smooth change of angle (see MAP_CenterOnPoint)*/
static vec2_t smoothFinal2DGeoscapeCenter = {0.5, 0.5};		/**< value of ccs.center for a smooth change of position (see MAP_CenterOnPoint) */
static float smoothDeltaLength = 0.0f;		/**< angle/position difference that we need to change when smoothing */
static float smoothFinalZoom = 0.0f;		/**< value of finale ccs.zoom for a smooth change of angle (see MAP_CenterOnPoint)*/
static float smoothDeltaZoom = 0.0f;		/**< zoom difference that we need to change when smoothing */
static float smoothAcceleration = 0.0f;		/**< the acceleration to use during a smooth motion (This affects the speed of the smooth motion) */
static qboolean smoothNewClick = qfalse;		/**< New click on control panel to make geoscape rotate */

static const float safeAcceleration = 0.06f;

static byte *terrainPic = NULL;			/**< this is the terrain mask for separating the clima
											zone and water by different color values */
static int terrainWidth, terrainHeight;		/**< the width and height for the terrain pic. */

static byte *culturePic = NULL;			/**< this is the mask for separating the culture
											zone and water by different color values */
static int cultureWidth, cultureHeight;		/**< the width and height for the culture pic. */

static byte *populationPic = NULL;			/**< this is the mask for separating the population rate
											zone and water by different color values */
static int populationWidth, populationHeight;		/**< the width and height for the population pic. */

static byte *nationsPic = NULL;			/**< this is the nation mask - separated
											by colors given in nations.ufo. */
static int nationsWidth, nationsHeight;	/**< the width and height for the nation pic. */


/*
==============================================================
CLICK ON MAP and MULTI SELECTION FUNCTIONS
==============================================================
*/

/**
 * @brief Add an element in the multiselection list
 */
static void MAP_MultiSelectListAddItem (multiSelectType_t item_type, int item_id,
	const char* item_description, const char* item_name)
{
	Q_strcat(multiSelect.popupText, va("%s\t%s\n", item_description, item_name), sizeof(multiSelect.popupText));
	multiSelect.selectType[multiSelect.nbSelect] = item_type;
	multiSelect.selectId[multiSelect.nbSelect] = item_id;
	multiSelect.nbSelect++;
}

/**
 * @brief Execute action for 1 element of the multi selection
 * Param Cmd_Argv(1) is the element selected in the popup_multi_selection menu
 */
static void MAP_MultiSelectExecuteAction_f (void)
{
	int selected, id;
	aircraft_t* aircraft;
	qboolean multiSelection = qfalse;

	if (Cmd_Argc() < 2) {
		/* Direct call from code, not from a popup menu */
		selected = 0;
	} else {
		/* Call from a geoscape popup menu (popup_multi_selection) */
		MN_PopMenu(qfalse);
		selected = atoi(Cmd_Argv(1));
		multiSelection = qtrue;
	}

	if (selected < 0 || selected >= multiSelect.nbSelect)
		return;
	id = multiSelect.selectId[selected];

	/* Execute action on element */
	switch (multiSelect.selectType[selected]) {
	case MULTISELECT_TYPE_BASE:	/* Select a base */
		if (id >= ccs.numBases)
			break;
		MAP_ResetAction();
		B_SelectBase(B_GetFoundedBaseByIDX(id));
		break;
	case MULTISELECT_TYPE_INSTALLATION:	/* Select a installation */
		if (id >= ccs.numInstallations)
			break;
		MAP_ResetAction();
		INS_SelectInstallation(INS_GetFoundedInstallationByIDX(id));
		break;
	case MULTISELECT_TYPE_MISSION: /* Select a mission */
		if (ccs.mapAction == MA_INTERCEPT && ccs.selectedMission && ccs.selectedMission == MAP_GetMissionByIdx(id)) {
			CL_DisplayPopupIntercept(ccs.selectedMission, NULL);
			return;
		}

		MAP_ResetAction();
		ccs.selectedMission = MAP_GetMissionByIdx(id);

		Com_DPrintf(DEBUG_CLIENT, "Select mission: %s at %.0f:%.0f\n", ccs.selectedMission->id, ccs.selectedMission->pos[0], ccs.selectedMission->pos[1]);
		ccs.mapAction = MA_INTERCEPT;
		if (multiSelection) {
			/* if we come from a multiSelection menu, no need to open twice this popup to go to mission */
			CL_DisplayPopupIntercept(ccs.selectedMission, NULL);
			return;
		}
		break;

	case MULTISELECT_TYPE_AIRCRAFT: /* Selection of an aircraft */
		aircraft = AIR_AircraftGetFromIDX(id);
		if (aircraft == NULL) {
			Com_DPrintf(DEBUG_CLIENT, "MAP_MultiSelectExecuteAction: selection of an unknow aircraft idx %i\n", id);
			return;
		}

		if (aircraft == selectedAircraft) {
			/* Selection of an already selected aircraft */
			CL_DisplayPopupAircraft(aircraft);	/* Display popup_aircraft */
		} else {
			/* Selection of an unselected aircraft */
			MAP_ResetAction();
			selectedAircraft = aircraft;
			if (multiSelection)
				/* if we come from a multiSelection menu, no need to open twice this popup to choose an action */
				CL_DisplayPopupAircraft(aircraft);
		}
		break;

	case MULTISELECT_TYPE_UFO : /* Selection of a UFO */
		/* Get the ufo selected */
		if (id < 0 || id >= ccs.numUFOs)
			return;
		aircraft = ccs.ufos + id;

		if (aircraft == selectedUFO) {
			/* Selection of an already selected ufo */
			CL_DisplayPopupIntercept(NULL, selectedUFO);
		} else {
			/* Selection of an unselected ufo */
			MAP_ResetAction();
			selectedUFO = aircraft;
			if (multiSelection)
				/* if we come from a multiSelection menu, no need to open twice this popup to choose an action */
				CL_DisplayPopupIntercept(NULL, selectedUFO);
		}
		break;

	case MULTISELECT_TYPE_NONE :	/* Selection of an element that has been removed */
		break;
	default:
		Com_DPrintf(DEBUG_CLIENT, "MAP_MultiSelectExecuteAction: selection of an unknow element type %i\n", multiSelect.selectType[selected]);
	}
}

/**
 * @brief Click on the map/geoscape
 */
void MAP_MapClick (menuNode_t* node, int x, int y)
{
	aircraft_t *aircraft;
	int i;
	vec2_t pos;
	const linkedList_t *list = ccs.missions;

	/* get map position */
	if (cl_3dmap->integer) {
		MAP3D_ScreenToMap(node, x, y, pos);
	} else {
		MAP_ScreenToMap(node, x, y, pos);
	}

	/* new base construction */
	switch (ccs.mapAction) {
	case MA_NEWBASE:
		/** @todo make this a function in cp_base.c - B_BuildBaseAtPos and make newBasePos static */
		if (!MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN))) {
			const nation_t* nation = MAP_GetNation(pos);
			if (nation)
				Com_DPrintf(DEBUG_CLIENT, "MAP_MapClick: Build base in nation '%s'\n", nation->id);

			Vector2Copy(pos, newBasePos);
			Com_DPrintf(DEBUG_CLIENT, "MAP_MapClick: Build base at: %.0f:%.0f\n", pos[0], pos[1]);

			CL_GameTimeStop();

			if (ccs.numBases < MAX_BASES) {
				Cvar_Set("mn_base_title", ccs.bases[ccs.numBases].name);
				MN_PushMenu("popup_newbase", NULL);
			} else {
				MS_AddNewMessage(_("Notice"), _("You've reached the base limit."), qfalse, MSG_STANDARD, NULL);
			}
			return;
		} else {
			MS_AddNewMessage(_("Notice"), _("Could not set up your base at this location"), qfalse, MSG_INFO, NULL);
			if (r_geoscape_overlay->integer & OVERLAY_RADAR)
				MAP_SetOverlay("radar");
		}
		break;
	case MA_NEWINSTALLATION:
		if (!MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN))) {
			const nation_t* nation = MAP_GetNation(pos);
			if (nation)
				Com_DPrintf(DEBUG_CLIENT, "MAP_MapClick: Build installation in nation '%s'\n", nation->id);

			Vector2Copy(pos, newInstallationPos);
			Com_DPrintf(DEBUG_CLIENT, "MAP_MapClick: Build installation at: %.0f:%.0f\n", pos[0], pos[1]);

			CL_GameTimeStop();

			if (ccs.numInstallations < MAX_INSTALLATIONS) {
				Cvar_Set("mn_installation_title", ccs.installations[ccs.numInstallations].name);
				MN_PushMenu("popup_newinstallation", NULL);
			} else {
				MS_AddNewMessage(_("Notice"), _("You've reached the installation limit."), qfalse, MSG_STANDARD, NULL);
			}
			return;
		} else {
			MS_AddNewMessage(_("Notice"), _("Could not set up your installation at this location"), qfalse, MSG_INFO, NULL);
			if (r_geoscape_overlay->integer & OVERLAY_RADAR)
				MAP_SetOverlay("radar");
		}
		break;
	case MA_UFORADAR:
		MN_PushMenu("popup_intercept_ufo", NULL);
		/* if shoot down - we have a new crashsite mission if color != water */
		break;
	default:
		break;
	}

	/* Init datas for multi selection */
	multiSelect.nbSelect = 0;
	memset(multiSelect.popupText, 0, sizeof(multiSelect.popupText));

	/* Get selected missions */
	for (; list; list = list->next) {
		const mission_t *tempMission = (mission_t *)list->data;
		if (multiSelect.nbSelect >= MULTISELECT_MAXSELECT)
			break;
		if ((tempMission->stage == STAGE_NOT_ACTIVE) || !tempMission->onGeoscape)
			continue;
		if (tempMission->pos && MAP_IsMapPositionSelected(node, tempMission->pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_MISSION, MAP_GetIdxByMission(tempMission),
				CP_MissionToTypeString(tempMission), _(tempMission->location));
	}

	/* Get selected bases */
	for (i = 0; i < MAX_BASES && multiSelect.nbSelect < MULTISELECT_MAXSELECT; i++) {
		const base_t *base = B_GetFoundedBaseByIDX(i);
		if (!base)
			continue;
		if (MAP_IsMapPositionSelected(node, ccs.bases[i].pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_BASE, i, _("Base"), base->name);

		/* Get selected aircraft wich belong to the base */
		aircraft = ccs.bases[i].aircraft + base->numAircraftInBase - 1;
		for (; aircraft >= base->aircraft; aircraft--)
			if (AIR_IsAircraftOnGeoscape(aircraft) && aircraft->fuel > 0 && MAP_IsMapPositionSelected(node, aircraft->pos, x, y))
				MAP_MultiSelectListAddItem(MULTISELECT_TYPE_AIRCRAFT, aircraft->idx, _("Aircraft"), _(aircraft->name));
	}

	/* Get selected installations */
	for (i = 0; i < MAX_INSTALLATIONS && multiSelect.nbSelect < MULTISELECT_MAXSELECT; i++) {
		const installation_t *installation = INS_GetFoundedInstallationByIDX(i);
		if (!installation)
			continue;
		if (MAP_IsMapPositionSelected(node, ccs.installations[i].pos, x, y))
			MAP_MultiSelectListAddItem(MULTISELECT_TYPE_INSTALLATION, i, _("Installation"), installation->name);
	}

	/* Get selected ufos */
	for (aircraft = ccs.ufos + ccs.numUFOs - 1; aircraft >= ccs.ufos; aircraft--)
		if (UFO_IsUFOSeenOnGeoscape(aircraft)
#ifdef DEBUG
		|| Cvar_VariableInteger("debug_showufos")
#endif
		)
			if (AIR_IsAircraftOnGeoscape(aircraft) && MAP_IsMapPositionSelected(node, aircraft->pos, x, y))
				MAP_MultiSelectListAddItem(MULTISELECT_TYPE_UFO, aircraft - ccs.ufos, _("UFO Sighting"), (RS_IsResearched_ptr(aircraft->tech)) ? _(aircraft->name) : _("Unknown"));

	if (multiSelect.nbSelect == 1) {
		/* Execute directly action for the only one element selected */
		Cmd_ExecuteString("multi_select_click");
	} else if (multiSelect.nbSelect > 1) {
		/* Display popup for multi selection */
		MN_RegisterText(TEXT_MULTISELECTION, multiSelect.popupText);
		CL_GameTimeStop();
		MN_PushMenu("popup_multi_selection", NULL);
	} else {
		/* Nothing selected */
		if (!selectedAircraft)
			MAP_ResetAction();
		else if (AIR_IsAircraftOnGeoscape(selectedAircraft) && AIR_AircraftHasEnoughFuel(selectedAircraft, pos)) {
			/* Move the selected aircraft to the position clicked */
			MAP_MapCalcLine(selectedAircraft->pos, pos, &selectedAircraft->route);
			selectedAircraft->status = AIR_TRANSIT;
			selectedAircraft->time = aircraft->point = 0;
		}
	}
}


/*
==============================================================
GEOSCAPE DRAWING AND COORDINATES
==============================================================
*/
/**
 * @brief maximum distance (in pixel) to get a valid mouse click
 * @note this is for a 1024 * 768 screen
 */
#define MN_MAP_DIST_SELECTION 15
/**
 * @brief Tell if the specified position is considered clicked
 */
static qboolean MAP_IsMapPositionSelected (const menuNode_t* node, const vec2_t pos, int x, int y)
{
	int msx, msy;

	if (MAP_AllMapToScreen(node, pos, &msx, &msy, NULL))
		if (x >= msx - MN_MAP_DIST_SELECTION && x <= msx + MN_MAP_DIST_SELECTION
		 && y >= msy - MN_MAP_DIST_SELECTION && y <= msy + MN_MAP_DIST_SELECTION)
			return qtrue;

	return qfalse;
}

/**
 * @brief Typical zoom to use in 3D geoscape to use same zoom values for both 2D and 3D geoscape
 * @note Used to convert openGL coordinates of the sphere into screen coordinates
 * @sa GLOBE_RADIUS */
const float STANDARD_3D_ZOOM = 40.0f;

/** @brief radius of the globe in screen coordinates */
#define GLOBE_RADIUS EARTH_RADIUS * (ccs.zoom / STANDARD_3D_ZOOM)

#if 0
/**
 * @brief Return needed zoom to show a given distance on screen, for 3D geoscape.
 * @param[in] node Pointer to the node where 3D geoscape is displayed.
 * @param[in] distance Half distance that we want to display on the full height of the screen.
 * @return zoom that should be applied to see given distance, if within boundaries.
 */
static float MAP_GetZoomFromDistance (const menuNode_t* node, float distance)
{
	float zoom;
	/* maximum zoom reachable with automatic zoom
	 * we can zoom closer than manually, so don't use cl_mapzoomax */
	const float MAX_AUTO_ZOOM = 40.0f;

	if (distance >= 90.0f) {
		/* We want to see the whole world */
		distance = EARTH_RADIUS;
	} else {
		distance = sin(torad * distance) * EARTH_RADIUS;
	}

	/* smaller size is height of the screen */
	zoom = (ccs.mapSize[1] / 2.0f) * STANDARD_3D_ZOOM / distance;

	if (zoom > MAX_AUTO_ZOOM)
		zoom = MAX_AUTO_ZOOM;
	else if (zoom < cl_mapzoommin->value)
		zoom = cl_mapzoommin->value;

	return zoom;
}
#endif

/**
 * @brief Transform a 2D position on the map to screen coordinates.
 * @param[in] pos vector that holds longitude and latitude
 * @param[out] x normalized (rotated and scaled) x value of mouseclick
 * @param[out] y normalized (rotated and scaled) y value of mouseclick
 * @param[out] z z value of the given latitude and longitude - might also be NULL if not needed
 * @sa MAP_MapToScreen
 * @sa MAP3D_ScreenToMap
 * @return qtrue if the point is visible, qfalse else (if it's outside the node or on the wrong side of earth).
 * @note In the function, we do the opposite of MAP3D_ScreenToMap
 */
static qboolean MAP_3DMapToScreen (const menuNode_t* node, const vec2_t pos, int *x, int *y, int *z)
{
	vec2_t mid;
	vec3_t v, v1, rotationAxis;
	const float radius = GLOBE_RADIUS;

	PolarToVec(pos, v);

	/* rotate the vector to switch of reference frame.
	 * We switch from the static frame of earth to the local frame of the player (opposite rotation of MAP3D_ScreenToMap) */
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v1, rotationAxis, v, - ccs.angles[PITCH]);

	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v, rotationAxis, v1, - ccs.angles[YAW]);

	/* set mid to the coordinates of the center of the globe */
	Vector2Set(mid, ccs.mapPos[0] + ccs.mapSize[0] / 2.0f, ccs.mapPos[1] + ccs.mapSize[1] / 2.0f);

	/* We now convert those coordinates relative to the center of the globe to coordinates of the screen
	 * (which are relative to the upper left side of the screen) */
	*x = (int) (mid[0] - radius * v[1]);
	*y = (int) (mid[1] - radius * v[0]);

	if (z) {
		*z = (int) (radius * v[2]);
	}

	/* if the point is on the wrong side of earth, the player cannot see it */
	if (v[2] > 0)
		return qfalse;

	/* if the point is outside the screen, the player cannot see it */
	if (*x < ccs.mapPos[0] && *y < ccs.mapPos[1] && *x > ccs.mapPos[0] + ccs.mapSize[0] && *y > ccs.mapPos[1] + ccs.mapSize[1])
		return qfalse;

	return qtrue;
}

/**
 * @brief Transform a 2D position on the map to screen coordinates.
 * @param[in] node Menu node
 * @param[in] pos Position on the map described by longitude and latitude
 * @param[out] x X coordinate on the screen
 * @param[out] y Y coordinate on the screen
 * @returns qtrue if the screen position is within the boundaries of the menu
 * node. Otherwise returns qfalse.
 * @sa MAP_3DMapToScreen
 */
qboolean MAP_MapToScreen (const menuNode_t* node, const vec2_t pos,
		int *x, int *y)
{
	float sx;

	/* get "raw" position */
	sx = pos[0] / 360 + ccs.center[0] - 0.5;

	/* shift it on screen */
	if (sx < -0.5)
		sx += 1.0;
	else if (sx > +0.5)
		sx -= 1.0;

	*x = ccs.mapPos[0] + 0.5 * ccs.mapSize[0] - sx * ccs.mapSize[0] * ccs.zoom;
	*y = ccs.mapPos[1] + 0.5 * ccs.mapSize[1] -
		(pos[1] / 180 + ccs.center[1] - 0.5) * ccs.mapSize[1] * ccs.zoom;

	if (*x < ccs.mapPos[0] && *y < ccs.mapPos[1] &&
		*x > ccs.mapPos[0] + ccs.mapSize[0] && *y > ccs.mapPos[1] + ccs.mapSize[1])
		return qfalse;
	return qtrue;
}

/**
 * @brief Call either MAP_MapToScreen or MAP_3DMapToScreen depending on the geoscape you're using.
 * @param[in] node Menu node
 * @param[in] pos Position on the map described by longitude and latitude
 * @param[out] x Pointer to the X coordinate on the screen
 * @param[out] y Pointer to the Y coordinate on the screen
 * @param[out] z Pointer to the Z coordinate on the screen (may be NULL if not needed)
 * @returns qtrue if pos corresponds to a point which is visible on the screen. Otherwise returns qfalse.
 * @sa MAP_MapToScreen
 * @sa MAP_3DMapToScreen
 */
qboolean MAP_AllMapToScreen (const menuNode_t* node, const vec2_t pos, int *x, int *y, int *z)
{
	if (cl_3dmap->integer)
		return MAP_3DMapToScreen(node, pos, x, y, z);
	else {
		if (z)
			*z = -10;
		return MAP_MapToScreen(node, pos, x, y);
	}
}

/**
 * @brief Draws a 3D marker on geoscape if the player can see it.
 * @param[in] node Menu node.
 * @param[in] pos Longitude and latitude of the marker to draw.
 * @param[in] theta Angle (degree) of the model to the horizontal.
 * @param[in] model The name of the model of the marker.
 */
qboolean MAP_Draw3DMarkerIfVisible (const menuNode_t* node, const vec2_t pos, float theta, const char *model, int skin)
{
	int x, y, z;
	vec3_t screenPos, angles, v;
	float zoom;
	float costheta, sintheta;
	const float radius = GLOBE_RADIUS;
	qboolean test;

	test = MAP_AllMapToScreen(node, pos, &x, &y, &z);

	if (test) {
		/* Set position of the model on the screen */
		VectorSet(screenPos, x, y, z);

		if (cl_3dmap->integer) {
			/* Set angles of the model */
			VectorCopy(screenPos, v);
			v[0] -= ccs.mapPos[0] + ccs.mapSize[0] / 2.0f;
			v[1] -= ccs.mapPos[1] + ccs.mapSize[1] / 2.0f;

			angles[0] = theta;
			costheta = cos(angles[0] * torad);
			sintheta = sin(angles[0] * torad);

			angles[1] = 180.0f - asin((v[0] * costheta + v[1] * sintheta) / radius) * todeg;
			angles[2] = asin((v[0] * sintheta - v[1] * costheta) / radius) * todeg;
		} else {
			VectorSet(angles, theta, 180, 0);
		}
		/* Set zoomed scale of markers. */
		zoom = 0.4f;

		/* Draw */
		R_Draw3DMapMarkers(angles, zoom, screenPos, model, skin);
		return qtrue;
	}
	return qfalse;
}


/**
 * @brief Return longitude and latitude of a point of the screen for 2D geoscape
 * @return pos vec2_t was filled with longitude and latitude
 * @param[in] x X coordinate on the screen that was clicked to
 * @param[in] y Y coordinate on the screen that was clicked to
 * @param[in] node The current menuNode we was clicking into (3dmap or map)
 */
static void MAP_ScreenToMap (const menuNode_t* node, int x, int y, vec2_t pos)
{
	pos[0] = (((ccs.mapPos[0] - x) / ccs.mapSize[0] + 0.5) / ccs.zoom - (ccs.center[0] - 0.5)) * 360.0;
	pos[1] = (((ccs.mapPos[1] - y) / ccs.mapSize[1] + 0.5) / ccs.zoom - (ccs.center[1] - 0.5)) * 180.0;

	while (pos[0] > 180.0)
		pos[0] -= 360.0;
	while (pos[0] < -180.0)
		pos[0] += 360.0;
}

/**
 * @brief Return longitude and latitude of a point of the screen for 3D geoscape (globe)
 * @return pos vec2_t was filled with longitude and latitude
 * @param[in] x X coordinate on the screen that was clicked to
 * @param[in] y Y coordinate on the screen that was clicked to
 * @param[in] node The current menuNode we was clicking into (3dmap or map)
 * @sa MAP_3DMapToScreen
 */
static void MAP3D_ScreenToMap (const menuNode_t* node, int x, int y, vec2_t pos)
{
	vec2_t mid;
	vec3_t v, v1, rotationAxis;
	float dist;
	const float radius = GLOBE_RADIUS;

	/* set mid to the coordinates of the center of the globe */
	Vector2Set(mid, ccs.mapPos[0] + ccs.mapSize[0] / 2.0f, ccs.mapPos[1] + ccs.mapSize[1] / 2.0f);

	/* stop if we click outside the globe (distance is the distance of the point to the center of the globe) */
	dist = sqrt((x - mid[0]) * (x - mid[0]) + (y - mid[1]) * (y - mid[1]));
	if (dist > radius) {
		Vector2Set(pos, -1.0, -1.0);
		return;
	}

	/* calculate the coordinates in the local frame
	 * this frame is the frame of the screen.
	 * v[0] is the vertical axis of the screen
	 * v[1] is the horizontal axis of the screen
	 * v[2] is the axis perpendicular to the screen - we get its value knowing that norm of v is egal to radius
	 *  (because the point is on the globe) */
	v[0] = - (y - mid[1]);
	v[1] = - (x - mid[0]);
	v[2] = - sqrt(radius * radius - (x - mid[0]) * (x - mid[0]) - (y - mid[1]) * (y - mid[1]));
	VectorNormalize(v);

	/* rotate the vector to switch of reference frame */
	/* note the ccs.angles[ROLL] is always 0, so there is only 2 rotations and not 3 */
	/*	and that GLOBE_ROTATE is already included in ccs.angles[YAW] */
	/* first rotation is along the horizontal axis of the screen, to put north-south axis of the earth
	 *	perpendicular to the screen */
	VectorSet(rotationAxis, 0, 1, 0);
	RotatePointAroundVector(v1, rotationAxis, v, ccs.angles[YAW]);

	/* second rotation is to rotate the earth around its north-south axis
	 *	so that Greenwich meridian is along the vertical axis of the screen */
	VectorSet(rotationAxis, 0, 0, 1);
	RotatePointAroundVector(v, rotationAxis, v1, ccs.angles[PITCH]);

	/* we therefore got in v the coordinates of the point in the static frame of the earth
	 *	that we can convert in polar coordinates to get its latitude and longitude */
	VecToPolar(v, pos);
}

/**
 * @brief Calculate the shortest way to go from start to end on a sphere
 * @param[in] start The point you start from
 * @param[in] end The point you go to
 * @param[out] line Contains the shortest path to go from start to end
 * @sa MAP_MapDrawLine
 */
void MAP_MapCalcLine (const vec2_t start, const vec2_t end, mapline_t* line)
{
	vec3_t s, e, v;
	vec3_t normal;
	vec2_t trafo, sa, ea;
	float cosTrafo, sinTrafo;
	float phiStart, phiEnd, dPhi, phi;
	float *p, *last;
	int i, n;

	/* get plane normal */
	PolarToVec(start, s);
	PolarToVec(end, e);
	/* Procedur below won't work if start is the same than end */
	if (VectorCompareEps(s, e, UFO_EPSILON)) {
		line->distance = 0;
		line->numPoints = 2;
		Vector2Set(line->point[0], end[0], end[1]);
		Vector2Set(line->point[1], end[0], end[1]);
		return;
	}

	CrossProduct(s, e, normal);
	VectorNormalize(normal);

	/* get transformation */
	VecToPolar(normal, trafo);
	cosTrafo = cos(trafo[1] * torad);
	sinTrafo = sin(trafo[1] * torad);

	sa[0] = start[0] - trafo[0];
	sa[1] = start[1];
	PolarToVec(sa, s);
	ea[0] = end[0] - trafo[0];
	ea[1] = end[1];
	PolarToVec(ea, e);

	phiStart = atan2(s[1], cosTrafo * s[2] - sinTrafo * s[0]);
	phiEnd = atan2(e[1], cosTrafo * e[2] - sinTrafo * e[0]);

	/* get waypoints */
	if (phiEnd < phiStart - M_PI)
		phiEnd += 2 * M_PI;
	if (phiEnd > phiStart + M_PI)
		phiEnd -= 2 * M_PI;

	n = (phiEnd - phiStart) / M_PI * LINE_MAXSEG;
	if (n > 0)
		n = n + 1;
	else
		n = -n + 1;

#if 0
	Com_DPrintf(DEBUG_CLIENT, "MAP_MapCalcLine: #(%3.1f %3.1f) -> (%3.1f %3.1f)\n", start[0], start[1], end[0], end[1]);
#endif

	line->distance = fabs(phiEnd - phiStart) / n * todeg;
	line->numPoints = n + 1;
	/* make sure we do not exceed route array size */
	assert(line->numPoints <= LINE_MAXPTS);
	dPhi = (phiEnd - phiStart) / n;
	p = NULL;
	for (phi = phiStart, i = 0; i <= n; phi += dPhi, i++) {
		last = p;
		p = line->point[i];
		VectorSet(v, -sinTrafo * cos(phi), sin(phi), cosTrafo * cos(phi));
		VecToPolar(v, p);
		p[0] += trafo[0];

		if (!last) {
			while (p[0] < -180.0)
				p[0] += 360.0;
			while (p[0] > +180.0)
				p[0] -= 360.0;
		} else {
			while (p[0] - last[0] > +180.0)
				p[0] -= 360.0;
			while (p[0] - last[0] < -180.0)
				p[0] += 360.0;
		}
	}
}

/**
 * @brief Draw a path on a menu node (usually the 2D geoscape map)
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the geoscape menu node.
 * @param[in] line The path which is to be drawn
 * @sa MAP_MapCalcLine
 */
static void MAP_MapDrawLine (const menuNode_t* node, const mapline_t* line)
{
	const vec4_t color = {1, 0.5, 0.5, 1};
	screenPoint_t pts[LINE_MAXPTS];
	screenPoint_t *p;
	int i, start, old;

	/* draw */
	R_Color(color);
	start = 0;
	old = ccs.mapSize[0] / 2;
	for (i = 0, p = pts; i < line->numPoints; i++, p++) {
		MAP_MapToScreen(node, line->point[i], &p->x, &p->y);

		/* If we cross longitude 180 degree (right/left edge of the screen), draw the first part of the path */
		if (i > start && abs(p->x - old) > ccs.mapSize[0] / 2) {
			/* shift last point */
			int diff;

			if (p->x - old > ccs.mapSize[0] / 2)
				diff = -ccs.mapSize[0] * ccs.zoom;
			else
				diff = ccs.mapSize[0] * ccs.zoom;
			p->x += diff;

			/* wrap around screen border */
			R_DrawLineStrip(i - start, (int*)(&pts));

			/* first path of the path is drawn, now we begin the second part of the path */
			/* shift first point, continue drawing */
			start = i;
			pts[0].x = p[-1].x - diff;
			pts[0].y = p[-1].y;
			p = pts;
		}
		old = p->x;
	}

	R_DrawLineStrip(i - start, (int*)(&pts));
	R_Color(NULL);
}

/**
 * @brief Draw a path on a menu node (usually the 3Dgeoscape map)
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the 3Dgeoscape menu node.
 * @param[in] line The path which is to be drawn
 * @sa MAP_MapCalcLine
 */
static void MAP_3DMapDrawLine (const menuNode_t* node, const mapline_t* line)
{
	const vec4_t color = {1, 0.5, 0.5, 1};
	screenPoint_t pts[LINE_MAXPTS];
	int i, numPoints, start;

	start = 0;

	/* draw only when the point of the path is visible */
	R_Color(color);
	for (i = 0, numPoints = 0; i < line->numPoints; i++) {
		if (MAP_3DMapToScreen(node, line->point[i], &pts[i].x, &pts[i].y, NULL)) {
			numPoints++;
		} else if (!numPoints)
			/* the point which is not drawn is at the begining of the path */
			start++;
	}

	R_DrawLineStrip(numPoints, (int*)(&pts[start]));
	R_Color(NULL);
}

#define CIRCLE_DRAW_POINTS	60
/**
 * @brief Draw equidistant points from a given point on a menu node
 * @param[in] node The menu node which will be used for drawing dimensions.
 * This is usually the geoscape menu node.
 * @param[in] center The latitude and longitude of center point
 * @param[in] angle The angle defining the distance of the equidistant points to center
 * @param[in] color The color for drawing
 * @sa RADAR_DrawCoverage
 */
void MAP_MapDrawEquidistantPoints (const menuNode_t* node, const vec2_t center, const float angle, const vec4_t color)
{
	int i, xCircle, yCircle;
	screenPoint_t pts[CIRCLE_DRAW_POINTS + 1];
	vec2_t posCircle;
	qboolean oldDraw = qfalse;
	int numPoints = 0;
	vec3_t initialVector, rotationAxis, currentPoint, centerPos;

	R_ColorBlend(color);

	/* Set centerPos corresponding to cartesian coordinates of the center point */
	PolarToVec(center, centerPos);

	/* Find a perpendicular vector to centerPos, and rotate centerPos around him to obtain one point distant of angle from centerPos */
	PerpendicularVector(rotationAxis, centerPos);
	RotatePointAroundVector(initialVector, rotationAxis, centerPos, angle);

	/* Now, each equidistant point is given by a rotation around centerPos */
	for (i = 0; i <= CIRCLE_DRAW_POINTS; i++) {
		qboolean draw = qfalse;
		const float degrees = i * 360.0f / (float)CIRCLE_DRAW_POINTS;
		RotatePointAroundVector(currentPoint, centerPos, initialVector, degrees);
		VecToPolar(currentPoint, posCircle);
		if (MAP_AllMapToScreen(node, posCircle, &xCircle, &yCircle, NULL)) {
			draw = qtrue;
			if (!cl_3dmap->integer && numPoints != 0 && abs(pts[numPoints - 1].x - xCircle) > 512)
				oldDraw = qfalse;
		}

		/* if moving from a point of the screen to a distant one, draw the path we already calculated, and begin a new path
		 * (to avoid unwanted lines) */
		if (draw != oldDraw && i != 0) {
			R_DrawLineStrip(numPoints, (int*)(&pts));
			numPoints = 0;
		}
		/* if the current point is to be drawn, add it to the path */
		if (draw) {
			pts[numPoints].x = xCircle;
			pts[numPoints].y = yCircle;
			numPoints++;
		}
		/* update value of oldDraw */
		oldDraw = draw;
	}

	/* Draw the last path */
	R_DrawLineStrip(numPoints, (int*)(&pts));
	R_ColorBlend(NULL);
}

/**
 * @brief Return the angle of a model given its position and destination.
 * @param[in] start Latitude and longitude of the position of the model.
 * @param[in] end Latitude and longitude of aimed point.
 * @param[in] direction vec3_t giving current direction of the model (NULL if the model is idle).
 * @param[out] ortVector If not NULL, this will be filled with the normalized vector around which rotation allows to go toward @c direction.
 * @return Angle (degrees) of rotation around the axis perpendicular to the screen for a model in @c start going toward @c end.
 */
float MAP_AngleOfPath (const vec3_t start, const vec2_t end, vec3_t direction, vec3_t ortVector)
{
	float angle = 0.0f;
	vec3_t start3D, end3D, tangentVector, v, rotationAxis;
	float dist;

	/* calculate the vector tangent to movement */
	PolarToVec(start, start3D);
	PolarToVec(end, end3D);
	if (ortVector) {
		CrossProduct(start3D, end3D, ortVector);
		VectorNormalize(ortVector);
		CrossProduct(ortVector, start3D, tangentVector);
	} else {
		CrossProduct(start3D, end3D, v);
		CrossProduct(v, start3D, tangentVector);
	}
	VectorNormalize(tangentVector);

	/* smooth change of direction if the model is not idle */
	if (direction) {
		VectorSubtract(tangentVector, direction, v);
		dist = VectorLength(v);
		if (dist > 0.01) {
			CrossProduct(direction, tangentVector, rotationAxis);
			VectorNormalize(rotationAxis);
			RotatePointAroundVector(v, rotationAxis, direction, 5.0);
			VectorCopy(v, direction);
			VectorSubtract(tangentVector, direction, v);
			if (VectorLength(v) < dist) {
				VectorCopy(direction, tangentVector);
			} else {
				VectorCopy(tangentVector, direction);
			}
		}
	}

	if (cl_3dmap->integer) {
		/* rotate vector tangent to movement in the frame of the screen */
		VectorSet(rotationAxis, 0, 0, 1);
		RotatePointAroundVector(v, rotationAxis, tangentVector, - ccs.angles[PITCH]);
		VectorSet(rotationAxis, 0, 1, 0);
		RotatePointAroundVector(tangentVector, rotationAxis, v, - ccs.angles[YAW]);
	} else {
		VectorSet(rotationAxis, 0, 0, 1);
		RotatePointAroundVector(v, rotationAxis, tangentVector, - start[0]);
		VectorSet(rotationAxis, 0, 1, 0);
		RotatePointAroundVector(tangentVector, rotationAxis, v, start[1] + 90.0f);
	}

	/* calculate the orientation angle of the model around axis perpendicular to the screen */
	angle = todeg * atan(tangentVector[0] / tangentVector[1]);
	if (tangentVector[1] > 0)
		angle += 180.0f;

	return angle;
}

/**
 * @brief Returns position of the model corresponding to centerOnEventIdx.
 * @param[out] vector Latitude and longitude of the model (finalAngle[2] is always 0).
 * @note Vector is a vec3_t if cl_3dmap is true, and a vec2_t if cl_3dmap is false.
 * @sa MAP_CenterOnPoint
 */
static void MAP_GetGeoscapeAngle (float *vector)
{
	int i;
	int baseIdx;
	int counter = 0;
	int maxEventIdx;
	const int numMissions = CP_CountMissionOnGeoscape();
	aircraft_t *aircraft;

	/* If the value of maxEventIdx is too big or to low, restart from begining */
	maxEventIdx = numMissions + ccs.numBases + ccs.numInstallations - 1;
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;
		for (i = 0, aircraft = base->aircraft; i < base->numAircraftInBase; i++, aircraft++) {
			if (AIR_IsAircraftOnGeoscape(aircraft))
				maxEventIdx++;
		}
	}
	for (aircraft = ccs.ufos + ccs.numUFOs - 1; aircraft >= ccs.ufos; aircraft --) {
		if (UFO_IsUFOSeenOnGeoscape(aircraft)) {
			maxEventIdx++;
		}
	}

	/* if there's nothing to center the view on, just go to 0,0 pos */
	if (maxEventIdx < 0) {
		if (cl_3dmap->integer)
			VectorSet(vector, 0, 0, 0);
		else
			Vector2Set(vector, 0, 0);
		return;
	}

	/* check centerOnEventIdx is within the bounds */
	if (centerOnEventIdx < 0)
		centerOnEventIdx = maxEventIdx;
	if (centerOnEventIdx > maxEventIdx)
		centerOnEventIdx = 0;

	/* Cycle through missions */
	if (centerOnEventIdx < numMissions) {
		const linkedList_t *list = ccs.missions;
		mission_t *mission = NULL;
		for (;list && (centerOnEventIdx != counter - 1); list = list->next) {
			mission = (mission_t *)list->data;
			if (mission->stage != STAGE_NOT_ACTIVE && mission->stage != STAGE_OVER && mission->onGeoscape) {
				counter++;
			}
		}
		assert(mission);

		if (cl_3dmap->integer)
			VectorSet(vector, mission->pos[0], -mission->pos[1], 0);
		else
			Vector2Set(vector, mission->pos[0], mission->pos[1]);
		MAP_ResetAction();
		ccs.selectedMission = mission;
		return;
	}
	counter += numMissions;

	/* Cycle through bases */
	if (centerOnEventIdx < ccs.numBases + counter) {
		for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
			const base_t *base = B_GetFoundedBaseByIDX(baseIdx);
			if (!base)
				continue;

			if (counter == centerOnEventIdx) {
				if (cl_3dmap->integer)
					VectorSet(vector, ccs.bases[baseIdx].pos[0], -ccs.bases[baseIdx].pos[1], 0);
				else
					Vector2Set(vector, ccs.bases[baseIdx].pos[0], ccs.bases[baseIdx].pos[1]);
				return;
			}
			counter++;
		}
	}
	counter += ccs.numBases;

	/* Cycle through installations */
	if (centerOnEventIdx < ccs.numInstallations + counter) {
		int instIdx;
		for (instIdx = 0; instIdx < MAX_INSTALLATIONS; instIdx++) {
			const installation_t *inst = INS_GetFoundedInstallationByIDX(instIdx);
			if (!inst)
				continue;

			if (counter == centerOnEventIdx) {
				if (cl_3dmap->integer)
					VectorSet(vector, ccs.installations[instIdx].pos[0], -ccs.installations[instIdx].pos[1], 0);
				else
					Vector2Set(vector, ccs.installations[instIdx].pos[0], ccs.installations[instIdx].pos[1]);
				return;
			}
			counter++;
		}
	}
	counter += ccs.numInstallations;

	/* Cycle through aircraft (only those present on geoscape) */
	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		for (i = 0, aircraft = base->aircraft; i < base->numAircraftInBase; i++, aircraft++) {
			if (AIR_IsAircraftOnGeoscape(aircraft)) {
				if (centerOnEventIdx == counter) {
					if (cl_3dmap->integer)
						VectorSet(vector, aircraft->pos[0], -aircraft->pos[1], 0);
					else
						Vector2Set(vector, aircraft->pos[0], aircraft->pos[1]);
					MAP_ResetAction();
					selectedAircraft = aircraft;
					return;
				}
				counter++;
			}
		}
	}

	/* Cycle through UFO (only those visible on geoscape) */
	for (aircraft = ccs.ufos + ccs.numUFOs - 1; aircraft >= ccs.ufos; aircraft --) {
		if (UFO_IsUFOSeenOnGeoscape(aircraft)) {
			if (centerOnEventIdx == counter) {
				if (cl_3dmap->integer)
					VectorSet(vector, aircraft->pos[0], -aircraft->pos[1], 0);
				else
					Vector2Set(vector, aircraft->pos[0], aircraft->pos[1]);
				MAP_ResetAction();
				selectedUFO = aircraft;
				return;
			}
			counter++;
		}
	}
}

#define ZOOM_LIMIT	2.5f
/**
 * @brief Switch to next model on 2D and 3D geoscape.
 * @note Set @c smoothRotation to @c qtrue to allow a smooth rotation in MAP_DrawMap.
 * @note This function sets the value of smoothFinalGlobeAngle (for 3D) or smoothFinal2DGeoscapeCenter (for 2D),
 *  which contains the finale value that ccs.angles or ccs.centre must respectively take.
 * @sa MAP_GetGeoscapeAngle
 * @sa MAP_DrawMap
 * @sa MAP3D_SmoothRotate
 * @sa MAP_SmoothTranslate
 */
void MAP_CenterOnPoint_f (void)
{
	if (Q_strncmp(MN_GetActiveMenuName(), "map", 3))
		return;

	centerOnEventIdx++;

	if (cl_3dmap->integer) {
		/* case 3D geoscape */
		vec3_t diff;

		MAP_GetGeoscapeAngle(smoothFinalGlobeAngle);
		smoothFinalGlobeAngle[1] += GLOBE_ROTATE;
		VectorSubtract(smoothFinalGlobeAngle, ccs.angles, diff);
		smoothDeltaLength = VectorLength(diff);
	} else {
		/* case 2D geoscape */
		vec2_t diff;

		MAP_GetGeoscapeAngle(smoothFinal2DGeoscapeCenter);
		Vector2Set(smoothFinal2DGeoscapeCenter, 0.5f - smoothFinal2DGeoscapeCenter[0] / 360.0f, 0.5f - smoothFinal2DGeoscapeCenter[1] / 180.0f);
		if (smoothFinal2DGeoscapeCenter[1] < 0.5 / ZOOM_LIMIT)
			smoothFinal2DGeoscapeCenter[1] = 0.5 / ZOOM_LIMIT;
		if (smoothFinal2DGeoscapeCenter[1] > 1.0 - 0.5 / ZOOM_LIMIT)
			smoothFinal2DGeoscapeCenter[1] = 1.0 - 0.5 / ZOOM_LIMIT;
		diff[0] = smoothFinal2DGeoscapeCenter[0] - ccs.center[0];
		diff[1] = smoothFinal2DGeoscapeCenter[1] - ccs.center[1];
		smoothDeltaLength = sqrt(diff[0] * diff[0] + diff[1] * diff[1]);
	}
	smoothFinalZoom = ZOOM_LIMIT;

	smoothDeltaZoom = fabs(smoothFinalZoom - ccs.zoom);
	smoothAcceleration = safeAcceleration;

	smoothRotation = qtrue;
}

/**
 * @brief Smoothly moves the map center position to the secified location on the geoscape.
 * @param[in] pointOnGeoscape  The position to center on.
 * @param[in] zoom  The level at which to zoom.
 * @param[in] acceleration  How fast the smooth movement should go.
 */
static void MAP_SmoothlyMoveToGeoscapePoint (const vec3_t pointOnGeoscape, const float zoomLevel, float acceleration)
{
	/* this function only concerns maps */
	if (Q_strncmp(MN_GetActiveMenuName(), "map", 3))
		return;

	if (cl_3dmap->integer) {
		/* case 3D geoscape */
		vec3_t diff;

		if (pointOnGeoscape == ccs.angles) {
			VectorCopy(pointOnGeoscape, smoothFinalGlobeAngle);
			smoothDeltaLength = 0.0f;
		} else {
			VectorSet(smoothFinalGlobeAngle, pointOnGeoscape[0], -pointOnGeoscape[1], 0);
			smoothFinalGlobeAngle[1] += GLOBE_ROTATE;
			VectorSubtract(smoothFinalGlobeAngle, ccs.angles, diff);
			smoothDeltaLength = VectorLength(diff);
		}
	} else {
		/* case 2D geoscape */
		vec2_t diff;

		/** @todo check why disabling combatzoom moves the viewpoint to the southpole
		 *	  then this can be re-enabled maybe for map menu too
		 */
		if (Q_strncmp(MN_GetActiveMenuName(), "map_combatzoom", 13))
			return;

		Vector2Set(smoothFinal2DGeoscapeCenter, pointOnGeoscape[0], pointOnGeoscape[1]);
		Vector2Set(smoothFinal2DGeoscapeCenter, 0.5f - smoothFinal2DGeoscapeCenter[0] / 360.0f, 0.5f - smoothFinal2DGeoscapeCenter[1] / 180.0f);
/*		if (smoothFinal2DGeoscapeCenter[1] < 0.5 / ZOOM_LIMIT)
			smoothFinal2DGeoscapeCenter[1] = 0.5 / ZOOM_LIMIT;
		if (smoothFinal2DGeoscapeCenter[1] > 1.0 - 0.5 / ZOOM_LIMIT)
			smoothFinal2DGeoscapeCenter[1] = 1.0 - 0.5 / ZOOM_LIMIT; */
		diff[0] = smoothFinal2DGeoscapeCenter[0] - ccs.center[0];
		diff[1] = smoothFinal2DGeoscapeCenter[1] - ccs.center[1];
		smoothDeltaLength = sqrt(diff[0] * diff[0] + diff[1] * diff[1]);
	}

	smoothFinalZoom = zoomLevel;
	smoothDeltaZoom = fabs(smoothFinalZoom - ccs.zoom);
	smoothAcceleration = acceleration;

	smoothRotation = qtrue;
}


/**
 * @brief Activates the "combat zoom" interception framework.
 */
void MAP_TurnCombatZoomOn (void)
{
	ccs.combatZoomOn = qtrue;
}

/**
 * @brief Sets the UFO that "combat zoom" should focus on.
 * @param[in] combatZoomedUFO  A pointer to the ufo for "combat zoom" should focus on.
 */
void MAP_SetCombatZoomedUFO (aircraft_t *combatZoomedUFO)
{
	ccs.combatZoomedUFO = combatZoomedUFO;
	ccs.combatZoomLevel = COMBAT_ZOOM_FULL;
	CL_EnsureValidGameLapseForCombatZoom();
	MAP_SmoothlyMoveToGeoscapePoint(combatZoomedUFO->pos, 40.0f, 0.06f);
}

/**
 * @brief Deactivates the "combat zoom" interception framework.
 * @sa MAP_ResetAction
 */
void MAP_TurnCombatZoomOff (void)
{
	if (ccs.combatZoomedUFO)
		MN_PopMenu(qfalse);
}

/**
 * @brief Performs the steps necessary to exit the "combat zoom" interception
 * framework once it has been deactivated.
 */
void MAP_CombatZoomExit_f (void)
{
	if (ccs.combatZoomedUFO) {
		MAP_SetSmoothZoom(cl_mapzoommax->value - 0.5f, qtrue);
		ccs.combatZoomOn = qfalse;
		ccs.combatZoomedUFO = NULL;
		CL_EnsureValidGameLapseForGeoscape();
	}
}

/**
 * @brief Toggles the level of "combat zoom" in the interception framework.  This will cause the zoom to
 * move between close up combat (if in combat range) and a half zoom.
 */
static void MAP_ToggleCombatZoomLevel_f (void)
{
	if (ccs.combatZoomLevel == COMBAT_ZOOM_FULL)
		ccs.combatZoomLevel = COMBAT_ZOOM_HALF;
	else
		ccs.combatZoomLevel = COMBAT_ZOOM_FULL;
}


/**
 * @brief smooth rotation of the 3D geoscape
 * @note updates slowly values of ccs.angles and ccs.zoom so that its value goes to smoothFinalGlobeAngle
 * @sa MAP_DrawMap
 * @sa MAP_CenterOnPoint
 */
static void MAP3D_SmoothRotate (void)
{
	vec3_t diff;
	float diff_angle, diff_zoom;
	const float epsilon = 0.1f;
	const float epsilonZoom = 0.01f;

	static float speedOffset = 0.0f;
	static float rotationSpeed = 0.0f;

	diff_zoom = smoothFinalZoom - ccs.zoom;

	VectorSubtract(smoothFinalGlobeAngle, ccs.angles, diff);
	diff_angle = VectorLength(diff);

	if (smoothNewClick) {
		if (ccs.combatZoomOn)
			speedOffset = 0.0f;
		else
			speedOffset = rotationSpeed;
		smoothNewClick = qfalse;
	}

	if (smoothDeltaLength > smoothDeltaZoom) {
		if (diff_angle > epsilon) {
			rotationSpeed = smoothDeltaLength * sin(3.05f * diff_angle / smoothDeltaLength) + speedOffset * diff_angle / smoothDeltaLength;
			VectorScale(diff, smoothAcceleration / diff_angle * rotationSpeed, diff);
			VectorAdd(ccs.angles, diff, ccs.angles);
			ccs.zoom = ccs.zoom + smoothAcceleration * diff_zoom / diff_angle * rotationSpeed;
			return;
		}
	} else {
		if (fabs(diff_zoom) > epsilonZoom) {
			rotationSpeed = smoothDeltaZoom * sin(3.05f * (diff_zoom / smoothDeltaZoom)) + fabs(speedOffset) * diff_zoom / smoothDeltaZoom;
			VectorScale(diff, smoothAcceleration * diff_angle / fabs(diff_zoom) * rotationSpeed, diff);
			VectorAdd(ccs.angles, diff, ccs.angles);
			ccs.zoom = ccs.zoom + smoothAcceleration * rotationSpeed;
			return;
		}
	}

	/* if we reach this point, that means that movement is over */
	VectorCopy(smoothFinalGlobeAngle, ccs.angles);
	smoothRotation = qfalse;
	speedOffset = 0.0f;
	ccs.zoom = smoothFinalZoom;
}

/**
 * @brief stop smooth translation on geoscape
 * @sa MN_RightClick
 * @sa MN_MouseWheel
 */
void MAP_StopSmoothMovement (void)
{
	smoothRotation = qfalse;
}

/**
 * @brief Sets up the variables required to do a smooth zoom.
 * @param[in] finalZoomLevel  The final zoom level once smooth zooming has completed.
 * @param[in] useSafeAcceleration  Boolean flag indicating if safe acceleration should be used.
 * There are some general bugs with using very fast acceleration.  This is the level at which it generally works.
 */
void MAP_SetSmoothZoom (float finalZoomLevel, qboolean useSafeAcceleration)
{
	if (useSafeAcceleration)
		MAP_SmoothlyMoveToGeoscapePoint(ccs.angles, finalZoomLevel, safeAcceleration);
	else
		MAP_SmoothlyMoveToGeoscapePoint(ccs.angles, finalZoomLevel, 0.2f);

}


#define SMOOTHING_STEP_2D	0.02f
/**
 * @brief smooth translation of the 2D geoscape
 * @note updates slowly values of ccs.center so that its value goes to smoothFinal2DGeoscapeCenter
 * @note updates slowly values of ccs.zoom so that its value goes to ZOOM_LIMIT
 * @sa MAP_DrawMap
 * @sa MAP_CenterOnPoint
 */
void MAP_SmoothTranslate (void)
{
	const float dist1 = smoothFinal2DGeoscapeCenter[0] - ccs.center[0];
	const float dist2 = smoothFinal2DGeoscapeCenter[1] - ccs.center[1];
	const float length = sqrt(dist1 * dist1 + dist2 * dist2);

	if (length < SMOOTHING_STEP_2D) {
		ccs.center[0] = smoothFinal2DGeoscapeCenter[0];
		ccs.center[1] = smoothFinal2DGeoscapeCenter[1];
		ccs.zoom = smoothFinalZoom;
		smoothRotation = qfalse;
	} else {
		const float diff_zoom = smoothFinalZoom - ccs.zoom;
		ccs.center[0] = ccs.center[0] + SMOOTHING_STEP_2D * dist1 / length;
		ccs.center[1] = ccs.center[1] + SMOOTHING_STEP_2D * dist2 / length;
		ccs.zoom = ccs.zoom + SMOOTHING_STEP_2D * diff_zoom;
	}
}

qboolean checkSmoothRotation(void)
{
	if (smoothRotation)
		return qtrue;
	else
		return qfalse;
}

#define BULLET_SIZE	1
/**
 * @brief Draws on bunch of bullets on the geoscape map
 * @param[in] node Pointer to the node in which you want to draw the bullets.
 * @param[in] pos
 * @sa MAP_DrawMap
 */
static void MAP_DrawBullets (const menuNode_t* node, const vec3_t pos)
{
	int x, y;
	const vec4_t yellow = {1.0f, 0.874f, 0.294f, 1.0f};

	if (MAP_AllMapToScreen(node, pos, &x, &y, NULL))
		R_DrawFill(x, y, BULLET_SIZE, BULLET_SIZE, ALIGN_CC, yellow);
}

/**
 * @brief Draws on bunch of bullets on the geoscape map
 * @param[in] node Pointer to the node in which you want to draw the laser.
 * @param[in] start Start position of the laser shot (on geoscape)
 * @param[in] end End position of the laser shot (on geoscape)
 * @sa MAP_DrawMap
 */
static void MAP_DrawLaser (const menuNode_t* node, const vec3_t start, const vec3_t end)
{
}

#define SELECT_CIRCLE_RADIUS	1.5f + 3.0f / ccs.zoom

/**
 * @brief Draws one mission on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] ms Pointer to the mission to draw.
 */
static void MAP_DrawMapOneMission (const menuNode_t* node, const mission_t *ms)
{
	int x, y;

	if (!MAP_AllMapToScreen(node, ms->pos, &x, &y, NULL))
		return;

	if (ms == ccs.selectedMission) {
		Cvar_Set("mn_mapdaytime", MAP_IsNight(ms->pos) ? _("Night") : _("Day"));

		/* Draw circle around the mission */
		if (cl_3dmap->integer) {
			if (!ccs.selectedMission->active)
				MAP_MapDrawEquidistantPoints(node, ms->pos, SELECT_CIRCLE_RADIUS, yellow);
		} else
			R_DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, ccs.selectedMission->active ? "geoscape/circleactive" : "geoscape/circle");
	}

	/* Draw mission model (this must be after drawing 'selected circle' so that the model looks above it)*/
	if (cl_3dmap->integer) {
		const float angle = MAP_AngleOfPath(ms->pos, northPole, NULL, NULL) + 90.0f;
		MAP_Draw3DMarkerIfVisible(node, ms->pos, angle, MAP_GetMissionModel(ms), 0);
	} else
		R_DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "geoscape/mission");

	R_FontDrawString("f_verysmall", ALIGN_UL, x + 10, y, ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1], ccs.mapSize[1],  _(ms->location), 0, 0, NULL, qfalse, 0);
}

/**
 * @brief Draws one installation on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] installation Pointer to the installation to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 * @param[in] font Default font.
 * @pre installation is not NULL.
 */
static void MAP_DrawMapOneInstallation (const menuNode_t* node, const installation_t *installation,
	qboolean oneUFOVisible, const char* font)
{
	const installationTemplate_t *tpl = installation->installationTemplate;
	int i, x, y;

	/* Draw weapon range if at least one UFO is visible */
	if (oneUFOVisible && AII_InstallationCanShoot(installation)) {
		/** @todo When there will be different possible installation weapon, range should change */
		for (i = 0; i < tpl->maxBatteries; i++) {
			const aircraftSlot_t const *slot = &installation->batteries[i].slot;
			if (slot->item
				&& (slot->ammoLeft > 0 || slot->ammoLeft)
				&& slot->installationTime == 0) {
				MAP_MapDrawEquidistantPoints(node, installation->pos,
					slot->ammo->craftitem.stats[AIR_STATS_WRANGE], red);
			}
		}
	}

	/* Draw installation radar (only the "wire" style part) */
	if (r_geoscape_overlay->integer & OVERLAY_RADAR)
		RADAR_DrawInMap(node, &installation->radar, installation->pos);

	/* Draw installation */
	if (cl_3dmap->integer) {
		const float angle = MAP_AngleOfPath(installation->pos, northPole, NULL, NULL) + 90.0f;
		MAP_Draw3DMarkerIfVisible(node, installation->pos, angle, tpl->model, 0);
	} else if (MAP_MapToScreen(node, installation->pos, &x, &y)) {
		R_DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, tpl->model);
	}

	/* Draw installation names */
	if (MAP_AllMapToScreen(node, installation->pos, &x, &y, NULL))
		R_FontDrawString(font, ALIGN_UL, x, y + 10, ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1], ccs.mapSize[1], installation->name, 0, 0, NULL, qfalse, 0);
}

/**
 * @brief Draws one base on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] base Pointer to the base to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 * @param[in] font Default font.
 */
static void MAP_DrawMapOneBase (const menuNode_t* node, const base_t *base,
	qboolean oneUFOVisible, const char* font)
{
	int i, x, y;

	/* Draw weapon range if at least one UFO is visible */
	if (oneUFOVisible && AII_BaseCanShoot(base)) {
		/** @todo When there will be different possible base weapon, range should change */
		for (i = 0; i < base->numBatteries; i++) {
			const aircraftSlot_t const *slot = &base->batteries[i].slot;
			if (slot->item
				&& (slot->ammoLeft > 0 || slot->ammoLeft)
				&& slot->installationTime == 0) {
				MAP_MapDrawEquidistantPoints(node, base->pos,
					slot->ammo->craftitem.stats[AIR_STATS_WRANGE], red);
			}
		}
	}

	/* Draw base radar (only the "wire" style part) */
	if (r_geoscape_overlay->integer & OVERLAY_RADAR)
		RADAR_DrawInMap(node, &base->radar, base->pos);

	/* Draw base */
	if (cl_3dmap->integer) {
		const float angle = MAP_AngleOfPath(base->pos, northPole, NULL, NULL) + 90.0f;
		if (base->baseStatus == BASE_UNDER_ATTACK)
			/* two skins - second skin is for baseattack */
			MAP_Draw3DMarkerIfVisible(node, base->pos, angle, "geoscape/base", 1);
		else
			MAP_Draw3DMarkerIfVisible(node, base->pos, angle, "geoscape/base", 0);
	} else if (MAP_MapToScreen(node, base->pos, &x, &y)) {
		if (base->baseStatus == BASE_UNDER_ATTACK)
			R_DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "geoscape/baseattack");
		else
			R_DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "geoscape/base");
	}

	/* Draw base names */
	if (MAP_AllMapToScreen(node, base->pos, &x, &y, NULL))
		R_FontDrawString(font, ALIGN_UL, x, y + 10, ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1], ccs.mapSize[1], base->name, 0, 0, NULL, qfalse, 0);
}

/**
 * @brief Draws one Phalanx aircraft on the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @param[in] aircraft Pointer to the aircraft to draw.
 * @param[in] oneUFOVisible Is there at least one UFO visible on the geoscape?
 */
static void MAP_DrawMapOnePhalanxAircraft (const menuNode_t* node, aircraft_t *aircraft, qboolean oneUFOVisible)
{
	int x, y;
	float angle;

	/* Draw aircraft radar (only the "wire" style part) */
	if (r_geoscape_overlay->integer & OVERLAY_RADAR)
		RADAR_DrawInMap(node, &aircraft->radar, aircraft->pos);

	/* Draw only the bigger weapon range on geoscape: more detail will be given on airfight map */
	if (oneUFOVisible)
		MAP_MapDrawEquidistantPoints(node, aircraft->pos, aircraft->stats[AIR_STATS_WRANGE] / 1000.0f, red);

	/* Draw aircraft route */
	if (aircraft->status >= AIR_TRANSIT) {
		/* aircraft is moving */
		mapline_t path;

		path.numPoints = aircraft->route.numPoints - aircraft->point;
		/** @todo : check why path.numPoints can be sometime equal to -1 */
		if (path.numPoints > 1) {
			memcpy(path.point, aircraft->pos, sizeof(vec2_t));
			memcpy(path.point + 1, aircraft->route.point + aircraft->point + 1, (path.numPoints - 1) * sizeof(vec2_t));
			if (cl_3dmap->integer)
				MAP_3DMapDrawLine(node, &path);
			else
				MAP_MapDrawLine(node, &path);
		}
		angle = MAP_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, NULL);
	} else {
		/* aircraft is idle */
		angle = MAP_AngleOfPath(aircraft->pos, northPole, aircraft->direction, NULL);
	}

	/* Draw a circle around selected aircraft */
	if (aircraft == selectedAircraft) {
		if (cl_3dmap->integer)
			MAP_MapDrawEquidistantPoints(node, aircraft->pos, SELECT_CIRCLE_RADIUS, yellow);
		else
			R_DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "geoscape/circle");

		/* Draw a circle around ufo purchased by selected aircraft */
		if (aircraft->status == AIR_UFO && MAP_AllMapToScreen(node, aircraft->aircraftTarget->pos, &x, &y, NULL)) {
			if (cl_3dmap->integer)
				MAP_MapDrawEquidistantPoints(node, aircraft->pos, SELECT_CIRCLE_RADIUS, yellow);
			else
				R_DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qtrue, "geoscape/circle");
		}
	}

	/* Draw aircraft (this must be after drawing 'selected circle' so that the aircraft looks above it)*/
	MAP_Draw3DMarkerIfVisible(node, aircraft->pos, angle, aircraft->model, 0);
	VectorCopy(aircraft->pos, aircraft->oldDrawPos);
}

/**
 * @brief Draws all ufos, aircraft, bases and so on to the geoscape map (2D and 3D)
 * @param[in] node The menu node which will be used for drawing markers.
 * @note This is a drawing function only, called each time a frame is drawn. Therefore
 * you should not use this function to calculate eg. distance between 2 items on geoscape
 * (you should instead calculate it just after one of the item moved -- distance is not
 * going to change when you rotate earth around itself and time is stopped eg.).
 * @sa MAP_DrawMap
 */
void MAP_DrawMapMarkers (const menuNode_t* node)
{
	const linkedList_t *list = ccs.missions;
	int x, y, i, baseIdx, installationIdx, aircraftIdx, idx;
	const char* font;

	const vec4_t white = {1.f, 1.f, 1.f, 0.7f};
	qboolean showXVI = qfalse;
	qboolean oneUFOVisible = qfalse;
	static char buffer[512] = "";
	float closestUFODistance = -1.0f;
	vec3_t *closestInterceptorPos = NULL;
	int closestInterceptorStatus = -1;
	float closestInterceptorDistance = -1.0f;
	float weaponZoomRange = 0;
	static qboolean aircraftInWeaponsRange = qfalse;
	aircraft_t *closestUFO = NULL;
	int maxInterpolationPoints;
	vec3_t combatZoomAttackingAircraftPos[MAX_AIRCRAFT];
	int combatZoomNumAttackingAircraft = 0;
	vec3_t combatZoomAircraftInCombatPos[MAX_AIRCRAFT];
	int combatZoomNumCombatAircraft = 0;

	assert(node);

	if (ccs.zoom < 35.0f && ccs.combatZoomedUFO)
		MN_PushMenu("airfight", NULL);

	/* font color on geoscape */
	R_Color(node->color);
	/* default font */
	font = MN_GetFont(node);

	/* check if at least 1 UFO is visible */
	for (aircraftIdx = 0; aircraftIdx < ccs.numUFOs; aircraftIdx++) {
		const aircraft_t const *aircraft = &ccs.ufos[aircraftIdx];
		if (UFO_IsUFOSeenOnGeoscape(aircraft)) {
			oneUFOVisible = qtrue;
			break;
		}
	}

	/* draw mission pics */
	Cvar_Set("mn_mapdaytime", "");
	for (; list; list = list->next) {
		const mission_t *ms = (mission_t *)list->data;
		if (!ms->onGeoscape)
			continue;
		MAP_DrawMapOneMission(node, ms);
	}

	/* draw installations */
	for (installationIdx = 0; installationIdx < MAX_INSTALLATIONS; installationIdx++) {
		const installation_t *installation = INS_GetFoundedInstallationByIDX(installationIdx);
		if (!installation)
			continue;
		MAP_DrawMapOneInstallation(node, installation, oneUFOVisible, font);
	}

	closestInterceptorDistance = -1.0f;

	/* draw bases */
 	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		MAP_DrawMapOneBase(node, base, oneUFOVisible, font);

		/* draw all aircraft of base */
		for (aircraftIdx = 0; aircraftIdx < base->numAircraftInBase; aircraftIdx++) {
			aircraft_t *aircraft = &base->aircraft[aircraftIdx];
			if (AIR_IsAircraftOnGeoscape(aircraft)) {
				float maxRange = AIR_GetMaxAircraftWeaponRange(aircraft->weapons, aircraft->maxWeapons);

				MAP_DrawMapOnePhalanxAircraft(node, aircraft, oneUFOVisible);

				/** @todo Remove me: this shouldn't be in this function */
				if (ccs.combatZoomedUFO && aircraft->aircraftTarget == ccs.combatZoomedUFO) {
					float distance = MAP_GetDistance(aircraft->pos, ccs.combatZoomedUFO->pos);
					VectorCopy(aircraft->pos, combatZoomAttackingAircraftPos[combatZoomNumAttackingAircraft]);
					combatZoomNumAttackingAircraft++;
					if (distance < maxRange && weaponZoomRange < maxRange)
						weaponZoomRange = maxRange;
					if ((distance < closestInterceptorDistance || !closestInterceptorPos) && distance > maxRange) {
						closestInterceptorPos = &aircraft->pos;
						closestInterceptorDistance = distance;
						closestInterceptorStatus = aircraft->status;
					}
				}
			}
		}
	}

	/* draws ufos */
	for (aircraftIdx = 0; aircraftIdx < ccs.numUFOs; aircraftIdx++) {
		aircraft_t *aircraft = &ccs.ufos[aircraftIdx];
#ifdef DEBUG
		/* in debug mode you execute set showufos 1 to see the ufos on geoscape */
		if (Cvar_VariableInteger("debug_showufos")) {
			/* Draw ufo route */
			if (cl_3dmap->integer)
				MAP_3DMapDrawLine(node, &aircraft->route);
			else
				MAP_MapDrawLine(node, &aircraft->route);
		} else
#endif
		if (!oneUFOVisible || !UFO_IsUFOSeenOnGeoscape(aircraft) || !MAP_AllMapToScreen(node, aircraft->pos, &x, &y, NULL))
			continue;

		{
			const float angle = MAP_AngleOfPath(aircraft->pos, aircraft->route.point[aircraft->route.numPoints - 1], aircraft->direction, NULL);

			if (ccs.combatZoomOn && !ccs.combatZoomedUFO) {
				if (closestUFODistance > MAP_GetDistance(ccs.mapPos, aircraft->pos) || closestUFODistance == -1.0f) {
					closestUFODistance = MAP_GetDistance(ccs.mapPos, aircraft->pos);
					closestUFO = aircraft;
				}
			}

			if (ccs.combatZoomedUFO && ccs.combatZoomOn) {
				if (ccs.combatZoomedUFO == aircraft) {
					float maxRange = AIR_GetMaxAircraftWeaponRange(aircraft->weapons, aircraft->maxWeapons);
					if (weaponZoomRange != 0) {
						int attAirIdx;

						if (!aircraftInWeaponsRange) {
							CL_SetGameTime(1);
							aircraftInWeaponsRange = qtrue;
						}

						if (weaponZoomRange < maxRange)
							weaponZoomRange = maxRange;

						for (attAirIdx = 0; attAirIdx < combatZoomNumAttackingAircraft; attAirIdx++) {
							float distance = MAP_GetDistance(combatZoomAttackingAircraftPos[attAirIdx], ccs.combatZoomedUFO->pos);
							if (distance <= weaponZoomRange) {
								VectorCopy(combatZoomAttackingAircraftPos[attAirIdx], combatZoomAircraftInCombatPos[combatZoomNumCombatAircraft]);
								combatZoomNumCombatAircraft++;
							}
						}

					} else {
						aircraftInWeaponsRange = qfalse;
					}
#if 0
					if (aircraftInWeaponsRange && ccs.combatZoomLevel == COMBAT_ZOOM_FULL) {
						vec3_t centroid = {0,0,0};
						int combatAirIdx;

						VectorCopy(ccs.combatZoomedUFO->pos, combatZoomAircraftInCombatPos[combatZoomNumCombatAircraft]);
						combatZoomNumCombatAircraft++;

						/* finds the centroid of all aircraft in combat range who are targeting the zoomed ufo
						 * and uses this as the point on which to center. */
						for (combatAirIdx = 0; combatAirIdx < combatZoomNumCombatAircraft; combatAirIdx++) {
							VectorAdd(combatZoomAircraftInCombatPos[combatAirIdx], centroid, centroid);
						}
						if (combatAirIdx > 1) {
							VectorScale(centroid, 1.0/(float)combatAirIdx, centroid);
							MAP_SmoothlyMoveToGeoscapePoint(centroid, MAP_GetZoomFromDistance(node, weaponZoomRange * 0.75), 0.15);
						} else {
							MAP_SmoothlyMoveToGeoscapePoint(ccs.combatZoomedUFO->pos, MAP_GetZoomFromDistance(node, weaponZoomRange * 0.75), 0.15);
						}
					}
					else {
						if (closestInterceptorStatus != AIR_RETURNING && closestInterceptorPos && (ccs.combatZoomLevel == COMBAT_ZOOM_FULL || (ccs.combatZoomLevel == COMBAT_ZOOM_HALF && closestInterceptorDistance >= weaponZoomRange + 2))) {
							vec3_t midpoint = {0,0,0};
							VectorMidpoint(ccs.combatZoomedUFO->pos, *closestInterceptorPos, midpoint);
							MAP_SmoothlyMoveToGeoscapePoint(midpoint, MAP_GetZoomFromDistance(node, closestInterceptorDistance), 0.4);
						} else {
							MAP_SmoothlyMoveToGeoscapePoint(ccs.combatZoomedUFO->pos, cl_mapzoommax->value + 1, 0.15);
						}
					}
#endif
				}
			}

			if (cl_3dmap->integer && !ccs.combatZoomedUFO)
				MAP_MapDrawEquidistantPoints(node, aircraft->pos, SELECT_CIRCLE_RADIUS, white);
			if (aircraft == selectedUFO && !ccs.combatZoomedUFO) {
				if (cl_3dmap->integer)
					MAP_MapDrawEquidistantPoints(node, aircraft->pos, SELECT_CIRCLE_RADIUS, yellow);
				else
					R_DrawNormPic(x, y, 0, 0, 0, 0, 0, 0, ALIGN_CC, qfalse, "geoscape/circle");
			}
			MAP_Draw3DMarkerIfVisible(node, aircraft->pos, angle, aircraft->model, 0);
			VectorCopy(aircraft->pos, aircraft->oldDrawPos);
		}
	}

	if (ccs.gameTimeScale > 0)
		maxInterpolationPoints = floor(1.0f / (cls.frametime * (float)ccs.gameTimeScale));
	else
		maxInterpolationPoints = 0;

	/** draws projectiles
	 * @todo you should only draw projectile comming from base here: projectiles fired by missiles
	 * are drawn only in airfight menu */
	for (idx = 0; idx < ccs.numProjectiles; idx++) {
		aircraftProjectile_t *projectile = &ccs.projectiles[idx];
		vec3_t drawPos = {0, 0, 0};

		if (projectile->attackingAircraft && !ccs.combatZoomedUFO)
			continue;

		if (projectile->hasMoved) {
			projectile->hasMoved = qfalse;
			VectorCopy(projectile->pos[0], drawPos);
		} else {
			if (maxInterpolationPoints > 2 && projectile->numInterpolationPoints < maxInterpolationPoints) {
				/* If a new point hasn't been given and there is at least 3 points need to be filled in then
				 * use linear interpolation to draw the points until a new projectile point is provided.
				 * The reason you need at least 3 points is that acceptable results can be achieved with 2 or less
				 * gaps in points so dont add the overhead of interpolation. */
				const float xInterpolStep = (projectile->projectedPos[0][0] - projectile->pos[0][0]) / (float)maxInterpolationPoints;
				projectile->numInterpolationPoints += 1;
				drawPos[0] = projectile->pos[0][0] + (xInterpolStep * projectile->numInterpolationPoints);
				LinearInterpolation(projectile->pos[0], projectile->projectedPos[0], drawPos[0], drawPos[1]);
			} else {
				VectorCopy(projectile->pos[0], drawPos);
			}
		}

		if (projectile->bullets)
			MAP_DrawBullets(node, drawPos);
		else if (projectile->laser)
			/** @todo Implement rendering of laser shot */
			MAP_DrawLaser(node, vec3_origin, vec3_origin);
		else
			MAP_Draw3DMarkerIfVisible(node, drawPos, projectile->angle, projectile->aircraftItem->model, 0);
	}

	if (ccs.combatZoomOn && !ccs.combatZoomedUFO && closestUFO) {
		MAP_SetCombatZoomedUFO(closestUFO);
	} else if (ccs.combatZoomOn && !ccs.combatZoomedUFO && !closestUFO) {
		ccs.combatZoomOn = qfalse;
	}

	showXVI = CP_IsXVIResearched() ? qtrue : qfalse;

	/* Draw nation names */
	for (i = 0; i < ccs.numNations; i++) {
		if (MAP_AllMapToScreen(node, ccs.nations[i].pos, &x, &y, NULL))
			R_FontDrawString("f_verysmall", ALIGN_UC, x , y, ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1], ccs.mapSize[1], _(ccs.nations[i].name), 0, 0, NULL, qfalse, 0);
		if (showXVI) {
			Q_strcat(buffer, va(_("%s\t%i%%\n"), _(ccs.nations[i].name), ccs.nations[i].stats[0].xviInfection), sizeof(buffer));
		}
	}
	if (showXVI)
		MN_RegisterText(TEXT_XVI, buffer);
	else
		MN_ResetData(TEXT_XVI);
}

/**
 * @brief Draw the geoscape
 * @param[in] node The map menu node
 * @sa MAP_DrawMapMarkers
 */
void MAP_DrawMap (const menuNode_t* node)
{
	float distance;
	vec2_t pos;
	qboolean disableSolarRender = qfalse;

	/* store these values in ccs struct to be able to handle this even in the input code */
	MN_GetNodeAbsPos(node, pos);
	Vector2Copy(pos, ccs.mapPos);
	Vector2Copy(node->size, ccs.mapSize);

	/* Draw the map and markers */
	if (cl_3dmap->integer) {
		if (ccs.zoom > cl_mapzoommax->value)
			disableSolarRender = qtrue;
		if (smoothRotation)
			MAP3D_SmoothRotate();
		R_Draw3DGlobe(ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1],
			ccs.date.day, ccs.date.sec, ccs.angles, ccs.zoom, curCampaign->map, disableSolarRender);
	} else {
		const float q = (ccs.date.day % DAYS_PER_YEAR + (float)(ccs.date.sec / (SECONDS_PER_HOUR * 6)) / 4) * 2 * M_PI / DAYS_PER_YEAR - M_PI;
		if (smoothRotation)
			MAP_SmoothTranslate();
		R_DrawFlatGeoscape(ccs.mapPos[0], ccs.mapPos[1], ccs.mapSize[0], ccs.mapSize[1], (float) ccs.date.sec / SECONDS_PER_DAY, q,
			ccs.center[0], ccs.center[1], 0.5 / ccs.zoom, curCampaign->map);
	}
	MAP_DrawMapMarkers(node);

	/* display text */
	MN_ResetData(TEXT_STANDARD);
	switch (ccs.mapAction) {
	case MA_NEWBASE:
		MN_RegisterText(TEXT_STANDARD, _("Select the desired location of the new base on the map.\n"));
		return;
	case MA_NEWINSTALLATION:
		MN_RegisterText(TEXT_STANDARD, _("Select the desired location of the new installation on the map.\n"));
		return;
	case MA_BASEATTACK:
		if (ccs.selectedMission)
			break;
		MN_RegisterText(TEXT_STANDARD, _("Aliens are attacking our base at this very moment.\n"));
		return;
	case MA_INTERCEPT:
		if (ccs.selectedMission)
			break;
		MN_RegisterText(TEXT_STANDARD, _("Select ufo or mission on map\n"));
		return;
	case MA_UFORADAR:
		if (ccs.selectedMission)
			break;
		MN_RegisterText(TEXT_STANDARD, _("UFO in radar range\n"));
		return;
	case MA_NONE:
		break;
	}

	/* Nothing is displayed yet */
	if (ccs.selectedMission) {
		const char *t = va(_("Location: %s\nType: %s\nObjective: %s"), ccs.selectedMission->location,
			CP_MissionToTypeString(ccs.selectedMission), _(ccs.selectedMission->mapDef->description));
		MN_RegisterText(TEXT_STANDARD, t);
	} else if (selectedAircraft) {
		switch (selectedAircraft->status) {
		case AIR_HOME:
		case AIR_REFUEL:
			MAP_ResetAction();
			break;
		case AIR_UFO:
			assert(selectedAircraft->aircraftTarget);
			distance = MAP_GetDistance(selectedAircraft->pos, selectedAircraft->aircraftTarget->pos);
			Com_sprintf(text_standard, sizeof(text_standard), _("Name:\t%s (%i/%i)\n"), _(selectedAircraft->name), selectedAircraft->teamSize, selectedAircraft->maxTeamSize);
			Q_strcat(text_standard, va(_("Status:\t%s\n"), AIR_AircraftStatusToName(selectedAircraft)), sizeof(text_standard));
			Q_strcat(text_standard, va(_("Distance to target:\t\t%.0f\n"), distance), sizeof(text_standard));
			Q_strcat(text_standard, va(_("Speed:\t%i km/h\n"), CL_AircraftMenuStatsValues(selectedAircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED)), sizeof(text_standard));
			Q_strcat(text_standard, va(_("Fuel:\t%i/%i\n"), CL_AircraftMenuStatsValues(selectedAircraft->fuel, AIR_STATS_FUELSIZE),
				CL_AircraftMenuStatsValues(selectedAircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE)), sizeof(text_standard));
			Q_strcat(text_standard, va(_("ETA:\t%sh\n"), CL_SecondConvert((float)SECONDS_PER_HOUR * distance / selectedAircraft->stats[AIR_STATS_SPEED])), sizeof(text_standard));
			MN_RegisterText(TEXT_STANDARD, text_standard);
			break;
		default:
			Com_sprintf(text_standard, sizeof(text_standard), _("Name:\t%s (%i/%i)\n"), _(selectedAircraft->name), selectedAircraft->teamSize, selectedAircraft->maxTeamSize);
			Q_strcat(text_standard, va(_("Status:\t%s\n"), AIR_AircraftStatusToName(selectedAircraft)), sizeof(text_standard));
			Q_strcat(text_standard, va(_("Speed:\t%i km/h\n"), CL_AircraftMenuStatsValues(selectedAircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED)), sizeof(text_standard));
			Q_strcat(text_standard, va(_("Fuel:\t%i/%i\n"), CL_AircraftMenuStatsValues(selectedAircraft->fuel, AIR_STATS_FUELSIZE),
				CL_AircraftMenuStatsValues(selectedAircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE)), sizeof(text_standard));
			if (selectedAircraft->status != AIR_IDLE) {
				distance = MAP_GetDistance(selectedAircraft->pos,
					selectedAircraft->route.point[selectedAircraft->route.numPoints - 1]);
				Q_strcat(text_standard, va(_("ETA:\t%sh\n"), CL_SecondConvert((float)SECONDS_PER_HOUR * distance / selectedAircraft->stats[AIR_STATS_SPEED])), sizeof(text_standard));
			}
			MN_RegisterText(TEXT_STANDARD, text_standard);
			break;
		}
	} else if (selectedUFO) {
		Com_sprintf(text_standard, sizeof(text_standard), "%s\n", UFO_AircraftToIDOnGeoscape(selectedUFO));
		Q_strcat(text_standard, va(_("Speed:\t%i km/h\n"), CL_AircraftMenuStatsValues(selectedUFO->stats[AIR_STATS_SPEED], AIR_STATS_SPEED)), sizeof(text_standard));
		MN_RegisterText(TEXT_STANDARD, text_standard);
	} else {
		MN_RegisterText(TEXT_STANDARD, "");
	}
}

/**
 * @brief No more special action in geoscape
 * @sa MAP_TurnCombatZoomOff
 */
void MAP_ResetAction (void)
{
	/* only if there is a running single player campaign */
	if (!GAME_CP_IsRunning())
		return;

	/* don't allow a reset when no base is set up */
	if (ccs.numBases)
		ccs.mapAction = MA_NONE;

	ccs.interceptAircraft = NULL;
	ccs.selectedMission = NULL;
	selectedAircraft = NULL;
	selectedUFO = NULL;
}

/**
 * @brief Select the specified aircraft in geoscape
 */
void MAP_SelectAircraft (aircraft_t* aircraft)
{
	MAP_ResetAction();
	selectedAircraft = aircraft;
}

/**
 * @brief Selected the specified mission
 */
void MAP_SelectMission (mission_t* mission)
{
	if (!mission || mission == ccs.selectedMission)
		return;
	MAP_ResetAction();
	ccs.mapAction = MA_INTERCEPT;
	ccs.selectedMission = mission;
}

/**
 * @brief Notify that a mission has been removed
 */
void MAP_NotifyMissionRemoved (const mission_t* mission)
{
	/* Unselect the current selected mission if it's the same */
	if (ccs.selectedMission == mission)
		MAP_ResetAction();
}

/**
 * @brief Notify that a UFO has been removed
 * @param[in] destroyed True if the UFO has been destroyed, false if it's been only set invisible (landed)
 */
void MAP_NotifyUFORemoved (const aircraft_t* ufo, qboolean destroyed)
{
	if (!selectedUFO)
		return;

	if (ccs.combatZoomedUFO == ufo)
		MAP_TurnCombatZoomOff();

	/* Unselect the current selected ufo if its the same */
	if (selectedUFO == ufo)
		MAP_ResetAction();
	else if (destroyed && selectedUFO > ufo)
		selectedUFO--;
}

/**
 * @brief Notify that an aircraft has been removed from game
 * @param[in] destroyed True if the UFO has been destroyed, false if it's been only set invisible (landed)
 */
void MAP_NotifyAircraftRemoved (const aircraft_t* aircraft, qboolean destroyed)
{
	if (!selectedAircraft)
		return;

	/* Unselect the current selected ufo if its the same */
	if (selectedAircraft == aircraft || ccs.interceptAircraft == aircraft)
		MAP_ResetAction();
	else if (destroyed && (selectedAircraft->homebase == aircraft->homebase) && selectedAircraft > aircraft)
		selectedAircraft--;
}

/**
 * @brief Translate nation map color to nation
 * @sa MAP_GetColor
 * @param[in] pos Map Coordinates to get the nation from
 * @return returns the nation pointer with the given color on nationPic at given pos
 * @return NULL if no nation with the given color value was found
 * @note The coodinates already have to be transfored to map coordinates via MAP_ScreenToMap
 */
nation_t* MAP_GetNation (const vec2_t pos)
{
	int i;
	const byte* color = MAP_GetColor(pos, MAPTYPE_NATIONS);
#ifdef PARANOID
	Com_DPrintf(DEBUG_CLIENT, "MAP_GetNation: color value for %.0f:%.0f is r:%i, g:%i, b: %i\n", pos[0], pos[1], color[0], color[1], color[2]);
#endif
	for (i = 0; i < ccs.numNations; i++) {
		nation_t *nation = &ccs.nations[i];
		/* compare the first three color values with color value at pos */
		if (VectorCompare(nation->color, color))
			return nation;
	}
	Com_DPrintf(DEBUG_CLIENT, "MAP_GetNation: No nation found at %.0f:%.0f - color: %i:%i:%i\n", pos[0], pos[1], color[0], color[1], color[2]);
	return NULL;
}


/**
 * @brief Translate color value to terrain type
 * @sa MAP_GetColor
 * @param[in] color the color value from the terrain mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 * @note Make sure, that there are textures with the same name in base/textures/tex_terrain
 */
const char* MAP_GetTerrainType (const byte* const color)
{
	if (MapIsDesert(color))
		return "desert";
	else if (MapIsArctic(color))
		return "arctic";
	else if (MapIsWater(color))
		return "water";
	else if (MapIsMountain(color))
		return "mountain";
	else if (MapIsTropical(color))
		return "tropical";
	else if (MapIsCold(color))
		return "cold";
	else if (MapIsWasted(color))
		return "wasted";
	else
		return "grass";
}

/**
 * @brief Translate color value to culture type
 * @sa MAP_GetColor
 * @param[in] color the color value from the culture mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 */
static const char* MAP_GetCultureType (const byte* color)
{
	if (MapIsWater(color))
		return "water";
	else if (MapIsEastern(color))
		return "eastern";
	else if (MapIsWestern(color))
		return "western";
	else if (MapIsOriental(color))
		return "oriental";
	else if (MapIsAfrican(color))
		return "african";
	else
		return "western";
}

/**
 * @brief Translate color value to population type
 * @sa MAP_GetColor
 * @param[in] color the color value from the population mask
 * @return returns the zone name
 * @note never may return a null pointer or an empty string
 */
static const char* MAP_GetPopulationType (const byte* color)
{
	if (MapIsWater(color))
		return "water";
	else if (MapIsUrban(color))
		return "urban";
	else if (MapIsSuburban(color))
		return "suburban";
	else if (MapIsVillage(color))
		return "village";
	else if (MapIsRural(color))
		return "rural";
	else if (MapIsNopopulation(color))
		return "nopopulation";
	else
		return "nopopulation";
}

/**
 * @brief Determine the terrain type under a given position
 * @sa MAP_GetColor
 * @param[in] pos Map Coordinates to get the terrain type from
 * @return returns the zone name
 */
static inline const char* MAP_GetTerrainTypeByPos (const vec2_t pos)
{
	const byte* color = MAP_GetColor(pos, MAPTYPE_TERRAIN);
	return MAP_GetTerrainType(color);
}

/**
 * @brief Determine the culture type under a given position
 * @sa MAP_GetColor
 * @param[in] pos Map Coordinates to get the culture type from
 * @return returns the zone name
 */
static inline const char* MAP_GetCultureTypeByPos (const vec2_t pos)
{
	const byte* color = MAP_GetColor(pos, MAPTYPE_CULTURE);
	return MAP_GetCultureType(color);
}

/**
 * @brief Determine the population type under a given position
 * @sa MAP_GetColor
 * @param[in] pos Map Coordinates to get the population type from
 * @return returns the zone name
 */
static inline const char* MAP_GetPopulationTypeByPos (const vec2_t pos)
{
	const byte* color = MAP_GetColor(pos, MAPTYPE_POPULATION);
	return MAP_GetPopulationType(color);
}

/**
 * @brief Get number of civilian on a map at given position.
 * @param[in] pos Position where the mission takes place.
 * @return Number of civilian.
 * @sa CP_CreateCivilianTeam
 */
int MAP_GetCivilianNumberByPosition (const vec2_t pos)
{
	const byte* color = MAP_GetColor(pos, MAPTYPE_POPULATION);

	if (MapIsWater(color))
		Sys_Error("MAP_GetPopulationType: Trying to get number of civilian in a position on water");
	else if (MapIsUrban(color))
		return 10;
	else if (MapIsSuburban(color))
		return 8;
	else if (MapIsVillage(color))
		return 6;
	else if (MapIsRural(color))
		return 4;
	else if (MapIsNopopulation(color))
		return 2;
	else
		return 0;
}

/**
 * @brief Prints positions parameter in console.
 * @param[in] pos Location (latitude, longitude) where you want to check parameters.
 * @note Used for printing in console, do not translate.
 * @sa NAT_ScriptSanityCheck
 */
void MAP_PrintParameterStringByPos (const vec2_t pos)
{
	const char *terrainType = MAP_GetTerrainTypeByPos(pos);
	const char *cultureType = MAP_GetCultureTypeByPos(pos);
	const char *populationType = MAP_GetPopulationTypeByPos(pos);

	Com_Printf ("      (Terrain: %s, Culture: %s, Population: %s)\n", terrainType, cultureType, populationType);
}

/**
 * @brief Calculate distance on the geoscape.
 * @param[in] pos1 Position at the start.
 * @param[in] pos2 Position at the end.
 * @return Distance from pos1 to pos2.
 * @note distance is an angle! This is the angle (in degrees) between the 2 vectors starting at earth's center and ending at pos1 or pos2. (if you prefer distance, this is also the distance on a globe of radius 180 / pi = 57)
 */
float MAP_GetDistance (const vec2_t pos1, const vec2_t pos2)
{
	/* convert into rad */
	const float latitude1 = pos1[1] * torad;
	const float latitude2 = pos2[1] * torad;
	const float deltaLongitude = (pos1[0] - pos2[0]) * torad;
	float distance;

	distance = cos(latitude1) * cos(latitude2) * cos(deltaLongitude) + sin(latitude1) * sin(latitude2);
	distance = acos(distance) * todeg;

	return distance;
}

/**
 * @brief Check that a position (in latitude / longitude) is within boundaries.
 * @param[in,out] pos Pointer to the 2 elements vector giving the position.
 */
void MAP_CheckPositionBoundaries (float *pos)
{
	while (pos[0] > 180.0)
		pos[0] -= 360.0;
	while (pos[0] < -180.0)
		pos[0] += 360.0;
	while (pos[1] > 90.0)
		pos[1] -= 180.0;
	while (pos[1] < -90.0)
		pos[1] += 180.0;
}

/**
 * @brief Check whether given position is Day or Night.
 * @param[in] pos Given position.
 * @return True if given position is Night.
 */
qboolean MAP_IsNight (const vec2_t pos)
{
	float p, q, a, root, x;

	/* set p to hours (we don't use ccs.day here because we need a float value) */
	p = (float) ccs.date.sec / SECONDS_PER_DAY;
	/* convert current day to angle (-pi on 1st january, pi on 31 december) */
	q = (ccs.date.day + p) * 2 * M_PI / DAYS_PER_YEAR_AVG - M_PI;
	p = (0.5 + pos[0] / 360 - p) * 2 * M_PI - q;
	a = -sin(pos[1] * torad);
	root = sqrt(1.0 - a * a);
	x = sin(p) * root * sin(q) - (a * SIN_ALPHA + cos(p) * root * COS_ALPHA) * cos(q);
	return (x > 0);
}

/**
 * @brief Searches the terrain mask for a given color
 * @param[in] color The color to search the terrain picture for
 * @param[out] polar The polar coordinates we found the color at
 * @todo There should only be one loop to search the color and decide
 * whether to use this location
 * @note Function is unused
 */
qboolean MAP_MaskFind (byte * color, vec2_t polar)
{
	byte *c;
	int res, i, num;

	/* check color */
	if (!VectorNotEmpty(color))
		return qfalse;

	/* find possible positions in the terrain pic pixel mask */
	res = terrainWidth * terrainHeight;
	num = 0;
	for (i = 0, c = terrainPic; i < res; i++, c += 4)
		if (VectorCompare(c, color))
			num++;

	/* nothing found? */
	if (!num)
		return qfalse;

	/* get position */
	num = rand() % num;
	for (i = 0, c = terrainPic; i < num; c += 4)
		if (VectorCompare(c, color))
			i++;

	/* transform to polar coords */
	res = (c - terrainPic) / 4;
	polar[0] = 180.0 - 360.0 * ((float) (res % terrainWidth) + 0.5) / terrainWidth;
	polar[1] = 90.0 - 180.0 * ((float) (res / terrainWidth) + 0.5) / terrainHeight;
	Com_DPrintf(DEBUG_CLIENT, "Set new coords for mission to %.0f:%.0f\n", polar[0], polar[1]);
	return qtrue;
}


/**
 * @brief Returns the color value from geoscape of a certain mask (terrain, culture or population) at a given position.
 * @param[in] pos vec2_t Value of position on map to get the color value from.
 * pos is longitude and latitude
 * @param[in] type determine the map to get the color from (there are different masks)
 * one for the climazone (bases made use of this - there are grass, ice and desert
 * base tiles available) and one for the nations
 * @return Returns the color value at given position.
 * @note terrainPic, culturePic and populationPic are pointers to an rgba image in memory
 * @sa MAP_GetTerrainType
 * @sa MAP_GetCultureType
 * @sa MAP_GetPopulationType
 */
byte *MAP_GetColor (const vec2_t pos, mapType_t type)
{
	int x, y;
	int width, height;
	byte *mask;

	switch (type) {
	case MAPTYPE_TERRAIN:
		mask = terrainPic;
		width = terrainWidth;
		height = terrainHeight;
		break;
	case MAPTYPE_CULTURE:
		mask = culturePic;
		width = cultureWidth;
		height = cultureHeight;
		break;
	case MAPTYPE_POPULATION:
		mask = populationPic;
		width = populationWidth;
		height = populationHeight;
		break;
	case MAPTYPE_NATIONS:
		mask = nationsPic;
		width = nationsWidth;
		height = nationsHeight;
		break;
	default:
		Sys_Error("Unknown maptype %i\n", type);
	}

	assert(pos[0] >= -180);
	assert(pos[0] <= 180);
	assert(pos[1] >= -90);
	assert(pos[1] <= 90);

	/* get coordinates */
	x = (180 - pos[0]) / 360 * width;
	x--; /* we start from 0 */
	y = (90 - pos[1]) / 180 * height;
	y--; /* we start from 0 */
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	/* 4 => RGBA */
	/* terrainWidth is the width of the image */
	/* this calulation returns the pixel in col x and in row y */
	assert(4 * (x + y * width) < width * height * 4);
	return mask + 4 * (x + y * width);
}

/**
 * @brief Minimum distance between a new mission and an existing base.
 */
static const float MIN_DIST_BASE = 4.0f;

/**
 * @brief Check if given pos is close to an existing base.
 * @return Pointer to the base if one base is closer than MIN_DIST_BASE from pos, NULL else
 */
base_t* MAP_PositionCloseToBase (const vec2_t pos)
{
	int baseIdx;

	for (baseIdx = 0; baseIdx < MAX_BASES; baseIdx++) {
		base_t *base = B_GetFoundedBaseByIDX(baseIdx);
		if (!base)
			continue;

		if (MAP_GetDistance(pos, base->pos) < MIN_DIST_BASE) {
			return base;
		}
	}

	return NULL;
}

/**
 * @brief Checks for a given location, if it fulfills all criteria given via parameters (terrain, culture, population, nation type)
 * @param[in] pos Location to be tested
 * @param[in] terrainTypes A linkedList_t containing a list of strings determining the terrain types to be tested for (e.g. "grass") may be NULL
 * @param[in] cultureTypes A linkedList_t containing a list of strings determining the culture types to be tested for (e.g. "western") may be NULL
 * @param[in] populationTypes A linkedList_t containing a list of strings determining the population types to be tested for (e.g. "suburban") may be NULL
 * @param[in] nations A linkedList_t containing a list of strings determining the nations to be tested for (e.g. "asia") may be NULL
 * @return true if a location was found, otherwise false. If map is water, return false
 * @note The name TCPNTypes comes from terrain, culture, population, nation types
 */
qboolean MAP_PositionFitsTCPNTypes (const vec2_t pos, const linkedList_t* terrainTypes, const linkedList_t* cultureTypes, const linkedList_t* populationTypes, const linkedList_t* nations)
{
	const char *terrainType = MAP_GetTerrainTypeByPos(pos);
	const char *cultureType = MAP_GetCultureTypeByPos(pos);
	const char *populationType = MAP_GetPopulationTypeByPos(pos);

	if (MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)))
		return qfalse;

	if (!terrainTypes || LIST_ContainsString(terrainTypes, terrainType)) {
		if (!cultureTypes || LIST_ContainsString(cultureTypes, cultureType)) {
			if (!populationTypes || LIST_ContainsString(populationTypes, populationType)) {
				const nation_t *nationAtPos = MAP_GetNation(pos);
				if (!nations)
					return qtrue;
				if (nationAtPos && (!nations || LIST_ContainsString(nations, nationAtPos->id))) {
					return qtrue;
				}
			}
		}
	}

	return qfalse;
}

void MAP_Init (void)
{
	/* load terrain mask */
	if (terrainPic) {
		Mem_Free(terrainPic);
		terrainPic = NULL;
	}
	R_LoadImage(va("pics/geoscape/%s_terrain", curCampaign->map), &terrainPic, &terrainWidth, &terrainHeight);
	if (!terrainPic || !terrainWidth || !terrainHeight)
		Sys_Error("Couldn't load map mask %s_terrain in pics/geoscape", curCampaign->map);

	/* load culture mask */
	if (culturePic) {
		Mem_Free(culturePic);
		culturePic = NULL;
	}
	R_LoadImage(va("pics/geoscape/%s_culture", curCampaign->map), &culturePic, &cultureWidth, &cultureHeight);
	if (!culturePic || !cultureWidth || !cultureHeight)
		Sys_Error("Couldn't load map mask %s_culture in pics/geoscape", curCampaign->map);

	/* load population mask */
	if (populationPic) {
		Mem_Free(populationPic);
		populationPic = NULL;
	}
	R_LoadImage(va("pics/geoscape/%s_population", curCampaign->map), &populationPic, &populationWidth, &populationHeight);
	if (!populationPic || !populationWidth || !populationHeight)
		Sys_Error("Couldn't load map mask %s_population in pics/geoscape", curCampaign->map);

	/* load nations mask */
	if (nationsPic) {
		Mem_Free(nationsPic);
		nationsPic = NULL;
	}
	R_LoadImage(va("pics/geoscape/%s_nations", curCampaign->map), &nationsPic, &nationsWidth, &nationsHeight);
	if (!nationsPic || !nationsWidth || !nationsHeight)
		Sys_Error("Couldn't load map mask %s_nations in pics/geoscape", curCampaign->map);

	MAP_ResetAction();
}

/**
 * @brief Notify that a UFO disappears on radars
 */
void MAP_NotifyUFODisappear (const aircraft_t* ufo)
{
	/* Unselect the current selected ufo if its the same */
	if (selectedUFO == ufo)
		MAP_ResetAction();

	if (ccs.combatZoomedUFO == ufo)
		MAP_TurnCombatZoomOff();
}

/**
 * @brief Command binding for map zooming
 */
void MAP_Zoom_f (void)
{
	const char *cmd;
	const float zoomAmount = 50.0f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <in|out>\n", Cmd_Argv(0));
		return;
	}

	if (ccs.combatZoomOn && ccs.combatZoomedUFO)
		return;

	cmd = Cmd_Argv(1);
	switch (cmd[0]) {
	case 'i':
		smoothFinalZoom = ccs.zoom * pow(0.995, -zoomAmount);
		break;
	case 'o':
		smoothFinalZoom = ccs.zoom * pow(0.995, zoomAmount);
		break;
	default:
		Com_Printf("MAP_Zoom_f: Invalid parameter: %s\n", cmd);
		return;
	}

	smoothAcceleration = safeAcceleration;

	if (smoothFinalZoom < cl_mapzoommin->value)
		smoothFinalZoom = cl_mapzoommin->value;
	else if (smoothFinalZoom > cl_mapzoommax->value) {
		smoothFinalZoom = cl_mapzoommax->value;
		if (cmd[0] == 'i')
			MAP_TurnCombatZoomOn();
	}

	if (!cl_3dmap->integer) {
		ccs.zoom = smoothFinalZoom;
		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
	} else {
		VectorCopy(ccs.angles, smoothFinalGlobeAngle);
		smoothDeltaLength = 0;
		if (smoothRotation)
			/* geoscape is already smooth rotating */
			smoothNewClick = qtrue;
		smoothRotation = qtrue;
		smoothDeltaZoom = fabs(smoothFinalZoom - ccs.zoom);
	}
}

/**
 * @brief Command binding for map scrolling
 */
void MAP_Scroll_f (void)
{
	const char *cmd;
	float scrollX = 0.0f, scrollY = 0.0f;
	const float scrollAmount = 80.0f;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <up|down|left|right>\n", Cmd_Argv(0));
		return;
	}

	cmd = Cmd_Argv(1);
	switch (cmd[0]) {
	case 'l':
		scrollX = scrollAmount;
		break;
	case 'r':
		scrollX = -scrollAmount;
		break;
	case 'u':
		scrollY = scrollAmount;
		break;
	case 'd':
		scrollY = -scrollAmount;
		break;
	default:
		Com_Printf("MAP_Scroll_f: Invalid parameter\n");
		return;
	}
	if (cl_3dmap->integer) {
		/* case 3D geoscape */
		vec3_t diff;

		VectorCopy(ccs.angles, smoothFinalGlobeAngle);

		/* rotate a model */
		smoothFinalGlobeAngle[PITCH] += ROTATE_SPEED * (scrollX) / ccs.zoom;
		smoothFinalGlobeAngle[YAW] -= ROTATE_SPEED * (scrollY) / ccs.zoom;

		while (smoothFinalGlobeAngle[YAW] > 180.0) {
			smoothFinalGlobeAngle[YAW] -= 360.0;
			ccs.angles[YAW] -= 360.0;
		}
		while (smoothFinalGlobeAngle[YAW] < -180.0) {
			smoothFinalGlobeAngle[YAW] += 360.0;
			ccs.angles[YAW] += 360.0;
		}

		while (smoothFinalGlobeAngle[PITCH] > 180.0) {
			smoothFinalGlobeAngle[PITCH] -= 360.0;
			ccs.angles[PITCH] -= 360.0;
		}
		while (smoothFinalGlobeAngle[PITCH] < -180.0) {
			smoothFinalGlobeAngle[PITCH] += 360.0;
			ccs.angles[PITCH] += 360.0;
		}
		VectorSubtract(smoothFinalGlobeAngle, ccs.angles, diff);
		smoothDeltaLength = VectorLength(diff);

		smoothFinalZoom = ccs.zoom;
		smoothDeltaZoom = 0.0f;
		smoothAcceleration = safeAcceleration;
		if (smoothRotation)
			/* geoscape is already smooth rotating */
			smoothNewClick = qtrue;
		smoothRotation = qtrue;
	} else {
		int i;
		/* shift the map */
		ccs.center[0] -= (float) (scrollX) / (ccs.mapSize[0] * ccs.zoom);
		ccs.center[1] -= (float) (scrollY) / (ccs.mapSize[1] * ccs.zoom);
		for (i = 0; i < 2; i++) {
			while (ccs.center[i] < 0.0)
				ccs.center[i] += 1.0;
			while (ccs.center[i] > 1.0)
				ccs.center[i] -= 1.0;
		}
		if (ccs.center[1] < 0.5 / ccs.zoom)
			ccs.center[1] = 0.5 / ccs.zoom;
		if (ccs.center[1] > 1.0 - 0.5 / ccs.zoom)
			ccs.center[1] = 1.0 - 0.5 / ccs.zoom;
	}
}

/**
 * @brief Switch overlay (turn on / off)
 * @param[in] overlayID Name of the overlay you want to switch.
 */
void MAP_SetOverlay (const char *overlayID)
{
	if (!Q_strcmp(overlayID, "nations")) {
		if (r_geoscape_overlay->integer & OVERLAY_NATION)
			r_geoscape_overlay->integer ^= OVERLAY_NATION;
		else
			r_geoscape_overlay->integer |= OVERLAY_NATION;
	}

	/* do nothing while the first base/installation is not build */
	if (ccs.numBases + ccs.numInstallations == 0)
		return;

	if (!Q_strcmp(overlayID, "xvi")) {
		if (r_geoscape_overlay->integer & OVERLAY_XVI)
			r_geoscape_overlay->integer ^= OVERLAY_XVI;
		else
			r_geoscape_overlay->integer |= OVERLAY_XVI;
	} else if (!Q_strcmp(overlayID, "radar")) {
		if (r_geoscape_overlay->integer & OVERLAY_RADAR)
			r_geoscape_overlay->integer ^= OVERLAY_RADAR;
		else {
			r_geoscape_overlay->integer |= OVERLAY_RADAR;
			RADAR_UpdateWholeRadarOverlay();
		}
	}
}

/**
 * @brief Console command to call MAP_SetOverlay.
 */
static void MAP_SetOverlay_f (void)
{
	const char *arg;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nations|xvi|radar>\n", Cmd_Argv(0));
		return;
	}

	arg = Cmd_Argv(1);
	MAP_SetOverlay(arg);

	/* save last decision player took on radar display, in order to be able to restore it later */
	if (!Q_strcmp(arg, "radar"))
		radarOverlayWasSet = (r_geoscape_overlay->integer & OVERLAY_RADAR);
}

/**
 * @brief Remove overlay.
 * @param[in] overlayID Name of the overlay you want to turn off.
 */
void MAP_DeactivateOverlay (const char *overlayID)
{
	if (!Q_strcmp(overlayID, "nations")) {
		if (r_geoscape_overlay->integer & OVERLAY_NATION)
			MAP_SetOverlay("nations");
		else
			return;
	}

	if (!Q_strcmp(overlayID, "xvi")) {
		if (r_geoscape_overlay->integer & OVERLAY_XVI)
			MAP_SetOverlay("xvi");
		else
			return;
	} else if (!Q_strcmp(overlayID, "radar")) {
		if (r_geoscape_overlay->integer & OVERLAY_RADAR)
			MAP_SetOverlay("radar");
		else
			return;
	}
}

/**
 * @brief Console command to call MAP_DeactivateOverlay.
 */
static void MAP_DeactivateOverlay_f (void)
{
	const char *arg;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <nations|xvi|radar>\n", Cmd_Argv(0));
		return;
	}

	arg = Cmd_Argv(1);
	MAP_DeactivateOverlay(arg);
}

/**
 * @brief Initialise MAP/Geoscape
 */
void MAP_InitStartup (void)
{
	Cmd_AddCommand("multi_select_click", MAP_MultiSelectExecuteAction_f, NULL);
	Cmd_AddCommand("map_overlay", MAP_SetOverlay_f, "Set the geoscape overlay");
	Cmd_AddCommand("map_deactivateoverlay", MAP_DeactivateOverlay_f, "Deactivate overlay");
	Cmd_AddCommand("togglecombatzoomlevel", MAP_ToggleCombatZoomLevel_f, _("Toggle the combat zoom level (full zoom or half zoom)."));

	cl_3dmap = Cvar_Get("cl_3dmap", "1", CVAR_ARCHIVE, "3D geoscape or flat geoscape");
	cl_mapzoommax = Cvar_Get("cl_mapzoommax", "6.0", CVAR_ARCHIVE, "Maximum geoscape zooming value");
	cl_mapzoommin = Cvar_Get("cl_mapzoommin", "1.0", CVAR_ARCHIVE, "Minimum geoscape zooming value");
}
