/**
 * @file cl_aircraft.h
 * @brief Header file for aircraft stuff
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
#ifndef CLIENT_CL_AIRCRAFT_H
#define CLIENT_CL_AIRCRAFT_H

#define MAX_AIRCRAFT	64
#define LINE_MAXSEG 64
#define LINE_MAXPTS (LINE_MAXSEG + 2)
#define LINE_DPHI	(M_PI / LINE_MAXSEG)

/** Invalid aircraft index (global index). */
#define AIRCRAFT_INVALID -1
/** Invalid aircraft index in base-list of aircraft. */
#define AIRCRAFT_INBASE_INVALID -1

/** factor to speed up refuelling */
#define AIRCRAFT_REFUEL_FACTOR 16

/** @brief A path on the map described by 2D points */
typedef struct mapline_s {
	int numPoints;		/**< number of points that make up this path */
	float distance;		/**< the distance between two points of the path - total
						 * distance is then (distance * (numPoints - 1)) */
	vec2_t point[LINE_MAXPTS]; /**< array of 2D points that make up this path */
} mapline_t;

/** @brief All different types of aircraft. */
typedef enum {
	AIRCRAFT_TRANSPORTER,
	AIRCRAFT_INTERCEPTOR,
	AIRCRAFT_UFO
} aircraftType_t;

#define MAX_HUMAN_AIRCRAFT_TYPE AIRCRAFT_INTERCEPTOR

/** @brief All different size of aircraft. */
typedef enum {
	AIRCRAFT_SMALL = 1,
	AIRCRAFT_LARGE = 2
} aircraftSize_t;

/** @brief All different Hangar size.
 * @note for Phalanx aircraft and UFO.
 */
typedef enum {
	AIRCRAFT_HANGAR_NONE = 0,
	AIRCRAFT_HANGAR_SMALL = 1,
	AIRCRAFT_HANGAR_BIG = 2,

	AIRCRAFT_HANGAR_ERROR
} aircraftHangarType_t;

/** @brief different weight for aircraft items
 * @note values must go from the lightest to the heaviest item */
typedef enum {
	ITEM_LIGHT,
	ITEM_MEDIUM,
	ITEM_HEAVY
} itemWeight_t;

#define MAX_AIRCRAFTITEMS 64

/** @brief different positions for aircraft items */
typedef enum {
	AIR_NOSE_LEFT,
	AIR_NOSE_CENTER,
	AIR_NOSE_RIGHT,
	AIR_WING_LEFT,
	AIR_WING_RIGHT,
	AIR_REAR_LEFT,
	AIR_REAR_CENTER,
	AIR_REAR_RIGHT,

	AIR_POSITIONS_MAX
} itemPos_t;

typedef enum {
	COMBAT_ZOOM_FULL,	/**< Zoomed in at max weapons range */
	COMBAT_ZOOM_HALF	/**< Zoomed out, but still tracking the combat zoomed ufo */
} combatZoomLevel_t;


#define MAX_AIRCRAFTSLOT 4

/** @brief slot of aircraft */
typedef struct aircraftSlot_s {
	int idx;					/**< self link */
	struct base_s *base;		/**< A link to the base. (if defined by aircraftItemType_t) */
	struct installation_s *installation;	/**< A link to the installation. (if defined by aircraftItemType_t) */
	struct aircraft_s *aircraft;	/**< A link to the aircraft (if defined by aircraftItemType_t). */
	aircraftItemType_t type;	/**< The type of item that can fit in this slot. */

	const objDef_t *item;		/**< Item that is currently in the slot. NULL if empty. */
	const objDef_t *ammo;		/**< Ammo that is currently in the slot. NULL if empty. */
	itemWeight_t size;			/**< The maximum size (weight) of item that can fit in this slot. */
	int ammoLeft;				/**< The number of ammo left in this slot */
	int delayNextShot;			/**< The delay before the next projectile can be shot */
	int installationTime;		/**< The time (in hours) left before the item is finished to be installed or removed in/from slot
								  *	This is > 0 if the item is being installed, < 0 if the item is being removed, 0 if the item is in place */
	const objDef_t *nextItem;	/**< Next item to install when the current item in slot will be removed
								  *	(Should be used only if installationTime is different of 0) */
	const objDef_t *nextAmmo;	/**< Next ammo to install when the nextItem will be installed */
	itemPos_t pos;				/**< Position of the slot on the aircraft */
} aircraftSlot_t;


/** @brief A cargo of items collected after mission. */
typedef struct itemsTmp_s {
	const objDef_t *item;		/**< Collected item. */
	int amount;					/**< Amount of collected items. */
} itemsTmp_t;

/**
 * @brief All different types of UFOs.
 * @note If you change the order, you have to change the ids in the scriptfiles, too
 * @sa cp_uforecovery <id>
 */
typedef enum {
	UFO_SCOUT,
	UFO_FIGHTER,
	UFO_HARVESTER,
	UFO_CORRUPTER,
	UFO_BOMBER,
	UFO_CARRIER,
	UFO_SUPPLY,

	UFO_MAX
} ufoType_t;

/** possible aircraft states */
typedef enum aircraftStatus_s {
	AIR_NONE,
	AIR_REFUEL,			/**< refill fuel */
	AIR_HOME,			/**< in homebase */
	AIR_IDLE,			/**< just sit there on geoscape */
	AIR_TRANSIT,		/**< moving */
	AIR_MISSION,		/**< moving to a mission */
	AIR_UFO,			/**< pursuing a UFO - also used for ufos that are pursuing an aircraft */
	AIR_DROP,			/**< ready to drop down */
	AIR_INTERCEPT,		/**< ready to intercept */
	AIR_TRANSFER,		/**< being transfered */
	AIR_RETURNING		/**< returning to homebase */
} aircraftStatus_t;

/** @brief An aircraft with all it's data */
typedef struct aircraft_s {
	int idx;			/**< Global index of this aircraft. See also gd.numAircraft and AIRCRAFT_INVALID
						 * this index is also updated when AIR_DeleteAircraft was called
						 * for all the other aircraft.
						 * For aircraftTemplates[] aicrafts this is the index in that array.
						 * this should be references only with the variable name aircraftIdx
						 * to let us find references all over the code easier @sa AIR_DeleteAircraft  */
	struct aircraft_s *tpl;	/**< Self-link in aircraft_sample list (i.e. templates). */
	char *id;			/**< Internal id from script file. */
	char *name;			/**< Translateable name. */
	char *shortname;	/**< Translateable shortname (being used in small popups). */
	char *image;		/**< Image on geoscape. */
	char *model;		/**< Model used on geoscape */
	aircraftType_t type;/**< Type of aircraft, see aircraftType_t. */
	ufoType_t ufotype;	/**< Type of UFO, see ufoType_t (UFO_MAX if craft is not a UFO). */
	int status;			/**< Status of this aircraft, see aircraftStatus_t. */

	int price;			/**< Price of this aircraft type. */
	int fuel;			/**< Current fuel amount. */
	int damage;			/**< Current Hit Point of the aircraft */
	int maxTeamSize;	/**< Max amount of soldiers onboard. @todo How do we handle 2x2 units here? @note Limited to MAX_ACTIVETEAM */
	int size;			/**< Size of the aircraft used in capacity calculations. */
	vec3_t pos;			/**< Current position on the geoscape. */
	vec3_t direction;	/**< Direction in which the aircraft is going on 3D geoscape (used for smoothed rotation). */
	vec3_t projectedPos;	/**< Projected position of the aircraft (latitude and longitude). */
	vec3_t oldDrawPos;	/**< The old draw position of the aircraft ( (latitude and longitude). */
	qboolean hasMoved;       /**< Has the aircraft been moved. */
	int numInterpolationPoints;	/**< Number of points drawn so far during interpolation. */
	int point;			/**< Number of route points that has already been done when aircraft is moving */
	int time;			/**< Elapsed seconds since aircraft started it's new route */
	int hangar;			/**< This is the baseCapacities_t enum value which says in which hangar this aircraft
						 * is being parked in (CAP_AIRCRAFTS_SMALL/CAP_AIRCRAFTS_BIG). */

	int teamSize;				/**< How many soldiers/units are on board (i.e. in the craft-team). */
	struct employee_s *acTeam[MAX_ACTIVETEAM];			/**< List of employees. i.e. current team for this aircraft. @todo Make this a linked list? */

	struct employee_s *pilot;			/**< Current Pilot assigned to the aircraft. */

	aircraftSlot_t weapons[MAX_AIRCRAFTSLOT];	/**< Weapons assigned to aircraft */
	int maxWeapons;					/**< Total number of weapon slots aboard this aircraft (empty or not) */
	aircraftSlot_t shield;			/**< Armour assigned to aircraft (1 maximum) */
	aircraftSlot_t electronics[MAX_AIRCRAFTSLOT];		/**< Electronics assigned to aircraft */
	int maxElectronics;				/**< Total number of electronics slots aboard this aircraft  (empty or not) */

	mapline_t route;
	struct base_s *homebase;			/**< Pointer to homebase for faster access. */
	aliensTmp_t aliencargo[MAX_CARGO];	/**< Cargo of aliens. */
	int alientypes;						/**< How many types of aliens we collected. */
	itemsTmp_t itemcargo[MAX_CARGO];	/**< Cargo of items. */
	int itemtypes;						/**< How many types of items we collected. */

	char *building;		/**< id of the building needed as hangar */

	int numUpgrades;	/**< @todo Implement or remove */

	struct mission_s* mission;	/**< The mission the aircraft is moving to if this is a PHALANX aircraft
								 * The mission the UFO is involved if this is a UFO */
	char *missionID;			/**< if this is a crashsite, we need the string here
								 * this is needed because we won't find the ufocrash mission
								 * in the parsed missions in csi.missions until we loaded the campaign */
	struct base_s *baseTarget;	/**< Target of the aircraft. NULL if the target is an aircraft */
	struct installation_s *installationTarget;		/**< Target of the aircraft. NULL if the target is an aircraft */
	struct aircraft_s *aircraftTarget;		/**< Target of the aircraft (ufo or phalanx) */
	radar_t	radar;				/**< Radar to track ufos */
	int stats[AIR_STATS_MAX];	/**< aircraft parameters for speed, damage and so on
								 * @note As this is an int, wrange is multiplied by 1000 */

	technology_t* tech;		/**< link to the aircraft tech */

	qboolean detected;		/**< Is the ufo detected by a radar? (note that a detected landed ufo has @c detected set to qtrue
							 * and @c visible set to qfalse: we can't see it on geoscape) */
	qboolean landed;		/**< Is ufo landed for a mission? This is used when a UFO lands (a UFO must have both
							 * @c detected and @c visible set to true to be actually seen on geoscape) */
	qboolean notOnGeoscape;	/**< don't let this aircraft appear ever on geoscape (e.g. ufo_carrier) */
} aircraft_t;

extern aircraft_t aircraftTemplates[MAX_AIRCRAFT]; /**< available aircraft types */
extern int numAircraftTemplates;

/** @todo Is this really needed - we have base_t->aircraftCurrent - isn't that the same? */
extern aircraft_t *menuAircraft; /**< current selected aircraft for menu texts*/

/* script functions */

#ifdef DEBUG
void AIR_ListAircraft_f(void);
void AIR_ListAircraftSamples_f(void);
#endif

void AIM_AircraftStart_f(void);
void AIR_NewAircraft_f(void);
void AIM_ResetAircraftCvars_f (void);
void AIM_NextAircraft_f(void);
void AIM_PrevAircraft_f(void);
void AIR_AircraftReturnToBase_f(void);

int AIR_GetAircraftIdxInBase(const aircraft_t* aircraft);
const char *AIR_AircraftStatusToName(const aircraft_t *aircraft);
qboolean AIR_IsAircraftInBase(const aircraft_t *aircraft);
qboolean AIR_IsAircraftOnGeoscape(const aircraft_t *aircraft);
void AIR_AircraftSelect(aircraft_t *aircraft);
void AIR_AircraftSelect_f(void);

void AIR_DeleteAircraft(struct base_s *base, aircraft_t *aircraft);
void AIR_DestroyAircraft(aircraft_t *aircraft);
qboolean AIR_MoveAircraftIntoNewHomebase(aircraft_t *aircraft, struct base_s *base);

void AIR_ResetAircraftTeam(aircraft_t *aircraft);
qboolean AIR_AddToAircraftTeam(aircraft_t *aircraft, struct employee_s* employee);
qboolean AIR_RemoveFromAircraftTeam(aircraft_t *aircraft, const struct employee_s* employee);
qboolean AIR_IsInAircraftTeam(const aircraft_t *aircraft, const struct employee_s* employee);

void CL_CampaignRunAircraft(int dt, qboolean updateRadarOverlay);
aircraft_t *AIR_GetAircraft(const char *name);
aircraft_t* AIR_AircraftGetFromIdx(int idx);
objDef_t *AII_GetAircraftItemByID(const char *id);
qboolean AIR_AircraftMakeMove(int dt, aircraft_t* aircraft);
void AIR_ParseAircraft(const char *name, const char **text, qboolean assignAircraftItems);
void AII_ReloadWeapon(aircraft_t *aircraft);
qboolean AIR_AircraftHasEnoughFuel(const aircraft_t *aircraft, const vec2_t destination);
qboolean AIR_AircraftHasEnoughFuelOneWay(const aircraft_t *aircraft, const vec2_t destination);
void AIR_AircraftReturnToBase(aircraft_t *aircraft);
qboolean AIR_SendAircraftToMission(aircraft_t* aircraft, struct mission_s* mission);
void AIR_GetDestinationWhilePursuing(const aircraft_t const *shooter, const aircraft_t const *target, vec2_t *dest);
qboolean AIR_SendAircraftPursuingUFO(aircraft_t* aircraft, aircraft_t* ufo);
void AIR_AircraftsNotifyUFORemoved(const aircraft_t *const ufo, qboolean destroyed);
void AIR_AircraftsUFODisappear(const aircraft_t *const ufo);
void AIR_UpdateHangarCapForAll(struct base_s *base);
qboolean AIR_ScriptSanityCheck(void);
int AIR_CalculateHangarStorage(const aircraft_t *aircraft, const struct base_s *base, int used);
int CL_AircraftMenuStatsValues(const int value, const int stat);
int AIR_CountTypeInBase(const struct base_s *base, aircraftType_t aircraftType);
const char *AIR_GetAircraftString(aircraftType_t aircraftType);
void AIR_AutoAddPilotToAircraft(struct base_s* base, struct employee_s* pilot);
void AIR_RemovePilotFromAssignedAircraft(struct base_s* base, const struct employee_s* pilot);
float AIR_GetMaxAircraftWeaponRange(const aircraftSlot_t *slot, int maxSlot);
int AIR_GetAircraftWeaponRanges(const aircraftSlot_t *slot, int maxSlot, float *weaponRanges);
int AIR_GetCapacityByAircraftWeight(const aircraft_t *aircraft);
const char *AIR_CheckMoveIntoNewHomebase(const aircraft_t *aircraft, const struct base_s* base, const int capacity);
#endif
