/**
 * @file check.c
 * @brief Some checks during compile, warning on -check and changes .map on -fix
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "common/shared.h"
#include "common/bspfile.h"
#include "common/scriplib.h"
#include "check.h"
#include "bsp.h"
#include "ufo2map.h"

#define MANDATORY_KEY 1
#define NON_MANDATORY_KEY 0

/** how close faces have to be in order for one to be hidden and set to SURF_NODRAW. Also
 *  the margin for abutting brushes to be considered not intersecting */
#define CH_DIST_EPSILON 0.001f
#define CH_DIST_EPSILON_SQR 0.000001

#define CH_DIST_EPSILON_COLLINEAR_POINTS 0.02f /**< this epsilon does need to be this big for the odd difficult case */

/** if the cosine of an angle is greater than this, then the angle is negligibly different from zero */
#define COS_EPSILON 0.9999f

/** if the sine of an angle is less than this, then the angle is negligibly different from zero */
#define SIN_EPSILON 0.0001f

static int numToMoveToWorldspawn = 0;

/**
 * @brief wether the surface of a brush is included when testing if a point is in a brush
 * determines how epsilon is applied.
 * @sa Check_IsPointInsideBrush
 */
typedef enum {
	PIB_EXCL_SURF, 				/**< surface is excluded */
	PIB_INCL_SURF_EXCL_EDGE,	/**< surface is included, but edges of brush are excluded */
	PIB_INCL_SURF,				/**< surface is included */
	PIB_ON_SURFACE_ONLY			/**< point on the surface, and the inside of the brush is excluded */
} pointInBrush_t;

#define NUM_NONE -1
#define NUM_DIFF -2
#define NUM_SAME -3

static int checkWorld (entity_t *e, int entnum);
static int checkLight (entity_t *e, int entnum);
static int checkFuncBreakable (entity_t *e, int entnum);
static int checkFuncDoor (entity_t *e, int entnum);
static int checkFuncRotating (entity_t *e, int entnum);
static int checkFuncGroup (entity_t *e, int entnum);
static int checkMiscItem (entity_t *e, int entnum);
static int checkMiscModel (entity_t *e, int entnum);
static int checkMiscParticle (entity_t *e, int entnum);
static int checkMiscSound (entity_t *e, int entnum);
static int checkMiscMission (entity_t *e, int entnum);
static int checkInfoPlayerStart (entity_t *e, int entnum);
static int checkStartPosition (entity_t *e, int entnum);
static int checkInfoNull (entity_t *e, int entnum);
static int checkInfoCivilianTarget (entity_t *e, int entnum);
static int checkTriggerHurt (entity_t *e, int entnum);
static int checkTriggerTouch (entity_t *e, int entnum);

typedef struct entityCheck_s {
	const char *name;	/**< The entity name */
	int (*checkCallback)(entity_t* e, int entnum);		/**< @return 0 if successfully fixed it
						 * everything else in case of an error that can't be fixed automatically */
} entityCheck_t;

static const entityCheck_t checkArray[] = {
	{"worldspawn", checkWorld},
	{"light", checkLight},
	{"func_breakable", checkFuncBreakable},
	{"func_door", checkFuncDoor},
	{"func_rotating", checkFuncRotating},
	{"func_group", checkFuncGroup},
	{"misc_item", checkMiscItem},
	{"misc_model", checkMiscModel},
	{"misc_particle", checkMiscParticle},
	{"misc_sound", checkMiscSound},
	{"misc_mission", checkMiscMission},
	{"misc_mission_aliens", checkMiscMission},
	{"info_player_start", checkInfoPlayerStart},
	{"info_human_start", checkStartPosition},
	{"info_alien_start", checkStartPosition},
	{"info_2x2_start", checkStartPosition},
	{"info_civilian_start", checkStartPosition},
	{"info_null", checkInfoNull},
	{"info_civilian_target", checkInfoCivilianTarget},
	{"trigger_hurt", checkTriggerHurt},
	{"trigger_touch", checkTriggerTouch},

	{NULL, NULL}
};

static void Check_Printf(const verbosityLevel_t msgVerbLevel, qboolean change, int entnum, int brushnum, const char *format, ...) __attribute__((format(printf, 5, 6)));

/**
 * @brief decides wether to proceed with output based on verbosity and ufo2map's mode: check/fix/compile
 * @param change true if there will an automatic change on -fix
 * @param brushnum the brush that the report is about. send NUM_NONE, if the report only regards an entity
 * @param entnum the entity the brush is from.send NUM_NONE if the report is a summary, not regarding a specific entity or brush
 * @note for brushnum and entnum send NUM_SAME in multi-call messages to indicate that the message still regards the same brush or entity
 * @note the start of a report on a particular item (eg brush) is specially prefixed. The function notes newlines in the previous
 * call, and prefixes this message with a string depending on wether the message indicates a change and the entity and brush number.
 * @note if entnum is set to NUM_SAME, then msgVerbLevel is carried over from the previous call.
 * @sa Com_Printf, Verb_Printf, DisplayContentFlags
 */
static void Check_Printf (verbosityLevel_t msgVerbLevel, qboolean change,
							int entnum, int brushnum, const char *format, ...)
{
	static int skippingCheckLine = 0;
	static int lastMsgVerbLevel = VERB_NORMAL;
	static qboolean firstSuccessfulPrint = qtrue;
	static qboolean startOfLine = qtrue;
	const qboolean containsNewline = strchr(format, '\n') != NULL;

	/* some checking/fix functions are called when ufo2map is compiling
	 * then the check/fix functions should be quiet */
	if (!(config.performMapCheck || config.fixMap))
		return;

	if (entnum == NUM_SAME)
		msgVerbLevel = lastMsgVerbLevel;

	lastMsgVerbLevel = msgVerbLevel;

	if (AbortPrint(msgVerbLevel))
		return;

	/* output prefixed with "  " is only a warning, should not be
	 * be displayed in fix mode. may be sent here in several function calls.
	 * skip everything from start of line "  " to \n */
	if (config.fixMap) {
		/* skip warning output sent in single call */
		if (!skippingCheckLine && startOfLine && !change && containsNewline)
			return;

		/* enter multi-call skip mode */
		if (!skippingCheckLine && startOfLine && !change) {
			skippingCheckLine = 1;
			return;
		}

		/* leave multi-call skip mode */
		if (skippingCheckLine && containsNewline) {
			skippingCheckLine = 0;
			return;
		}

		/* middle of multi-call skip mode */
		if (skippingCheckLine)
			return;
	}

	if (firstSuccessfulPrint && config.verbosity == VERB_MAPNAME) {
		PrintName();
		firstSuccessfulPrint = qfalse;
	}

	{
		char out_buffer1[4096];
		{
			va_list argptr;

			va_start(argptr, format);
			Q_vsnprintf(out_buffer1, sizeof(out_buffer1), format, argptr);
			va_end(argptr);
		}

		if (startOfLine) {
			char *prefix;
			prefix = change ? "* " : "  ";
			prefix = (brushnum == NUM_NONE && entnum == NUM_NONE) ? "//" : prefix;

			printf("%sent:%i brush:%i - %s", prefix, entnum, brushnum, out_buffer1);

		} else {
			printf("%s", out_buffer1);
		}
	}

	/* ensure next call gets brushnum and entnum printed if this is the end of the previous*/
	startOfLine = containsNewline ? qtrue : qfalse;
}

/** needs to be done here, on map brushes as worldMins and worldMaxs from levels.c
 * are only calculated on BSPing
 * @param[out] mapSize the returned size in map units
 */
static void Check_MapSize (vec3_t mapSize) {
	int i, bi, vi;
	vec3_t mins, maxs;

	VectorSet(mins, 0, 0, 0);
	VectorSet(maxs, 0, 0, 0);

	for (i = 0; i < nummapbrushes; i++) {
		const mapbrush_t *brush = &mapbrushes[i];

		for (bi = 0; bi < brush->numsides; bi++) {
			const winding_t *winding = brush->original_sides[bi].winding;

			for (vi = 0; vi < winding->numpoints; vi++) {

				AddPointToBounds(winding->p[vi], mins, maxs);
			}
		}
	}

	VectorSubtract(maxs, mins, mapSize);
}

#define MIN_TILE_SIZE 256 /**< @todo take this datum from the correct place */
#define NUM_ENT_TYPES 32

/**
 * @brief print map stats on -stats
 */
void Check_Stats() {
	vec3_t worldSize;
	int i, j;
	int entNums[NUM_ENT_TYPES];

	memset(entNums, '\0', NUM_ENT_TYPES * sizeof(int));

	Check_MapSize(worldSize);
	Verb_Printf(VERB_NORMAL, "        Number of brushes: %i\n",nummapbrushes);
	Verb_Printf(VERB_NORMAL, "         Number of planes: %i\n",nummapplanes);
	Verb_Printf(VERB_NORMAL, "    Number of brush sides: %i\n",nummapbrushsides);
	Verb_Printf(VERB_NORMAL, "         Map size (units): %.0f %.0f %.0f\n", worldSize[0], worldSize[1], worldSize[2]);
	Verb_Printf(VERB_NORMAL, "        Map size (fields): %.0f %.0f %.0f\n", worldSize[0] / UNIT_SIZE, worldSize[1] / UNIT_SIZE, worldSize[2] / UNIT_HEIGHT);
	Verb_Printf(VERB_NORMAL, "         Map size (tiles): %.0f %.0f %.0f\n", worldSize[0] / (MIN_TILE_SIZE), worldSize[1] / (MIN_TILE_SIZE), worldSize[2] / UNIT_HEIGHT);
	Verb_Printf(VERB_NORMAL, "       Number of entities: %i\n", num_entities);

	/* count number of each type of entity */
	for (i = 0; i < num_entities; i++) {
		entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");
		const entityCheck_t *v;

		for (v = checkArray, j = 0; v->name; v++, j++)
			if (!strncmp(name, v->name, strlen(v->name))) {
				entNums[j]++;
#ifdef DEBUG
			if (j >= NUM_ENT_TYPES)
				Com_Printf("Check_Stats: buffer overflow");
#endif
				break;
			}
		if (!v->name) {
			Com_Printf("Check_Stats: entity '%s' not recognised\n", name);
		}
	}

	{
		const entityCheck_t *v;
		/* print number of each type of entity */
		for (v = checkArray, j = 0; v->name; v++, j++)
			if (entNums[j]) {
				Com_Printf("%27s: %i\n", v->name, entNums[j]);
			}
	}
}

/**
 * @param[in] mandatory if this key is missing the entity will be deleted, else just a warning
 * @sa checkEntityNotSet
 */
static int checkEntityKey (entity_t *e, const int entnum, const char* key, const qboolean mandatory)
{
	const char *val = ValueForKey(e, key);
	if (!*val) {
		const char *name = ValueForKey(e, "classname");
		if (mandatory == MANDATORY_KEY) {
			Check_Printf(VERB_CHECK, qtrue, entnum, -1, "%s with no %s given - will be deleted\n", name, key);
			return 1;
		} else {
			Check_Printf(VERB_CHECK, qfalse, entnum, -1, "%s with no %s given\n", name, key);
			return 0;
		}
	}

	return 0;
}

static void checkEntityLevelFlags (entity_t *e, const int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		const char *name = ValueForKey(e, "classname");
		char buf[16];
		Check_Printf(VERB_CHECK, qtrue, entnum, -1, "%s with no levelflags given - setting all\n", name);
		snprintf(buf, sizeof(buf) - 1, "%i", (CONTENTS_LEVEL_ALL >> 8));
		SetKeyValue(e, "spawnflags", buf);
	}
}

/**
 * @sa checkEntityKey
 */
static int checkEntityNotSet (const entity_t *e, int entnum, const char *var)
{
	const char *key = ValueForKey(e, var);
	if (key[0] != '\0') {
		const char *name = ValueForKey(e, "classname");
		Check_Printf(VERB_CHECK, qfalse, entnum, -1, "%s has %s set (%s) - remove it!\n", name, var, key);
		return 1;
	}
	return 0;
}

static int checkEntityZeroBrushes (const entity_t *e, int entnum)
{
	if (!e->numbrushes) {
		const char *name = ValueForKey(e, "classname");
		Check_Printf(VERB_CHECK, qtrue, entnum, -1, "%s with no brushes given - will be deleted\n", name);
		return 1;
	}
	return 0;
}

/** @brief a slightly pointless test, but nice to have one, as it
 * stops a compiler warning */
static int checkWorld (entity_t *e, int entnum)
{
	if (!e->numbrushes)
		Check_Printf(VERB_CHECK, qfalse, entnum, -1, "worldspawn with no brushes given - unusual, but may be OK if there are func_groups\n");
	return 0;
}

static int checkLight (entity_t *e, int entnum)
{
	return checkEntityKey(e, entnum, "origin", qtrue);
}

static int checkFuncRotating (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);
	checkEntityNotSet(e, entnum, "angles");

	if (checkEntityZeroBrushes(e, entnum))
		return 1;

	return 0;
}

static int checkFuncDoor (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);
	checkEntityNotSet(e, entnum, "angles");

	if (checkEntityZeroBrushes(e, entnum))
		return 1;

	return 0;
}

static int checkFuncBreakable (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);
	checkEntityNotSet(e, entnum, "angles");
	checkEntityNotSet(e, entnum, "angle");

	if (checkEntityZeroBrushes(e, entnum)) {
		return 1;
	} else if (e->numbrushes > 1) {
		Check_Printf(VERB_CHECK, qfalse, entnum, -1, "func_breakable with more than one brush given (might break pathfinding)\n");
	}

	return 0;
}

static int checkMiscItem (entity_t *e, int entnum)
{
	if (checkEntityKey(e, entnum, "item", MANDATORY_KEY))
		return 1;

	return 0;
}

static int checkMiscModel (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);

	if (checkEntityKey(e, entnum, "model", MANDATORY_KEY))
		return 1;

	return 0;
}

static int checkMiscParticle (entity_t *e, int entnum)
{
	checkEntityLevelFlags(e, entnum);

	if (checkEntityKey(e, entnum, "particle", MANDATORY_KEY))
		return 1;

	return 0;
}

static int checkMiscMission (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "health");
	if (!*val)
		val = ValueForKey(e, "time");
	if (!*val) {
		val = ValueForKey(e, "target");
		if (*val && !FindTargetEntity(val))
			Check_Printf(VERB_CHECK, qfalse, entnum, -1, "misc_mission could not find specified target: '%s'\n", val);
	}
	if (!*val)
		Check_Printf(VERB_CHECK, qfalse, entnum, -1, "misc_mission with no objectives given\n");
	return 0;
}

#define FUNC_GROUP_NO_PROBLEM 0
#define FUNC_GROUP_MOVE_TO_WORLD 1
#define FUNC_GROUP_EMPTY_DELETE 2

/**
 * @return one of FUNC_GROUP_NO_PROBLEM, FUNC_GROUP_MOVE_TO_WORLD, FUNC_GROUP_EMPTY_DELETE
 */
static int checkFuncGroup (entity_t *e, int entnum)
{
	const char *name = ValueForKey(e, "classname");
	if (e->numbrushes == 1) {
		Check_Printf(VERB_CHECK, qtrue, entnum, -1, "%s with one brush only - will be moved to worldspawn\n", name);
		numToMoveToWorldspawn++;
		/* the  map writer will check and tack them onto the end of the worldspawn */
		return FUNC_GROUP_MOVE_TO_WORLD;
	}
	if (checkEntityZeroBrushes(e, entnum))
		return FUNC_GROUP_EMPTY_DELETE;
	return FUNC_GROUP_NO_PROBLEM;
}

/**
 * @brief single brushes in func_groups are moved to worldspawn. this function allocates space for
 * pointers to those brushes.
 * @return a pointer to the array of pointers
 * @param[out] the number of brushes
 */
mapbrush_t **Check_ExtraBrushesForWorldspawn (int *numBrushes)
{
	int i, j, tmpVerb = config.verbosity;
	mapbrush_t **brushesToMove = (mapbrush_t **)malloc(numToMoveToWorldspawn * sizeof(mapbrush_t *));

	if (!brushesToMove)
		Sys_Error("Check_ExtraBrushesForWorldspawn: out of memory");

	*numBrushes = numToMoveToWorldspawn;

	if (!numToMoveToWorldspawn)
		return brushesToMove;

	/* temporarily drop verbosity as checkFuncGroup should not repeat messages */
	config.verbosity = VERB_SILENT_EXCEPT_ERROR;

	/* 0 is the world - start at 1 */
	for (i = 1, j = 0; i < num_entities; i++) {
		entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");

		if (!strncmp(name, "func_group", 10)) {
			if (checkFuncGroup(e, i) == FUNC_GROUP_MOVE_TO_WORLD)
				brushesToMove[j++] = &mapbrushes[e->firstbrush];
		}
	}

	/* restore */
	config.verbosity = tmpVerb;

	return brushesToMove;
}

static int checkStartPosition (entity_t *e, int entnum)
{
	int align = 16;
	const char *val = ValueForKey(e, "classname");

	if (!strcmp(val, "info_2x2_start"))
		align = 32;

	if (((int)e->origin[0] - align) % UNIT_SIZE || ((int)e->origin[1] - align) % UNIT_SIZE) {
		Check_Printf(VERB_CHECK, qtrue, entnum, -1, "misaligned starting position - (%i: %i). The %s will be deleted\n", (int)e->origin[0], (int)e->origin[1], val);
		return 1; /** @todo auto-align entity and check for intersection with brush */
	}
	return 0;
}

static int checkInfoPlayerStart (entity_t *e, int entnum)
{
	if (checkEntityKey(e, entnum, "team", MANDATORY_KEY))
		return 1;

	return checkStartPosition(e, entnum);
}

static int checkInfoNull (entity_t *e, int entnum)
{
	if (checkEntityKey(e, entnum, "targetname", MANDATORY_KEY))
		return 1;

	return 0;
}

static int checkInfoCivilianTarget (entity_t *e, int entnum)
{
	checkEntityKey(e, entnum, "count", NON_MANDATORY_KEY);
	return 0;
}

static int checkMiscSound (entity_t *e, int entnum)
{
	if (checkEntityKey(e, entnum, "noise", MANDATORY_KEY))
		return 1;
	return 0;
}

static int checkTriggerHurt (entity_t *e, int entnum)
{
	checkEntityKey(e, entnum, "dmg", NON_MANDATORY_KEY);
	return 0;
}

static int checkTriggerTouch (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "target");
	if (!*val)
		Check_Printf(VERB_CHECK, qfalse, entnum, -1, "trigger_touch with no target given\n");
	else if (!FindTargetEntity(val))
		Check_Printf(VERB_CHECK, qfalse, entnum, -1, "trigger_touch could not find specified target: '%s'\n", val);
	return 0;
}

/** faces close to pointing down may be set to nodraw.
 * this is the cosine of the angle of how close it has to be. around 10 degrees */
#define NEARDOWN_COS 0.985

/**
 * @brief faces that are near pointing down may be set nodraw,
 * as views are always slightly down
 */
static qboolean Check_SidePointsDown (const side_t *s)
{
	const vec3_t down = {0.0f, 0.0f, -1.0f};
	const plane_t *plane = &mapplanes[s->planenum];
	const float dihedralCos = DotProduct(plane->normal, down);
	return dihedralCos >= NEARDOWN_COS;
}

/**
 * @brief distance from a point to a plane.
 * @note the sign of the result depends on which side of the plane the point is
 * @return a negative distance if the point is on the inside of the plane
 */
static inline float Check_PointPlaneDistance (const vec3_t point, const plane_t *plane)
{
	/* normal should have a magnitude of one */
	assert(fabs(VectorLengthSqr(plane->normal) - 1.0f) < CH_DIST_EPSILON);

	return DotProduct(point, plane->normal) - plane->dist;
}

/**
 * @brief calculates whether side1 faces side2 and touches.
 * @details The surface unit normals
 * must be antiparallel (i.e. they face each other), and the distance
 * to the origin must be such that they occupy the same region of
 * space, to within a distance of epsilon. These are based on consideration of
 * the planes of the faces only - they could be offset by a long way.
 * @note i did some experiments that show that the plane indices alone cannot
 * cannot be relied on to test for 2 planes facing each other. blondandy
 * @return true if the planes of the sides face and touch
 * @sa CheckNodraws
 */
static qboolean FacingAndCoincidentTo (const side_t *side1, const side_t *side2)
{
	const plane_t *plane1 = &mapplanes[side1->planenum];
	const plane_t *plane2 = &mapplanes[side2->planenum];
	float distance;

	const float dihedralCos = DotProduct(plane1->normal, plane2->normal);
	if (dihedralCos >= -COS_EPSILON)
		return qfalse; /* not facing each other */

	/* calculate the distance of point from plane2. as we have established that the
	 * plane's normals are antiparallel, and plane1->planeVector[0] is a point on plane1
	 * (that was supplied in the map file), this is the distance
	 * between the planes */
	distance = Check_PointPlaneDistance(plane1->planeVector[0], plane2);

	return fabs(distance) < CH_DIST_EPSILON;
}

/**
 * @brief calculates whether side1 and side2 are on a common plane
 * @details normals must be parallel, planes must touch
 * @return true if the side1 and side2 are on a common plane
 * @sa CheckZFighting, FacingAndCoincidentTo
 */
static qboolean ParallelAndCoincidentTo (const side_t *side1, const side_t *side2)
{
	float distance;
	const plane_t *plane1 = &mapplanes[side1->planenum];
	const plane_t *plane2 = &mapplanes[side2->planenum];
	const float dihedralCos = DotProduct(plane1->normal, plane2->normal);
	if (dihedralCos <= COS_EPSILON)
		return qfalse; /* not parallel */

	distance = Check_PointPlaneDistance(plane1->planeVector[0], plane2);

	return fabs(distance) < CH_DIST_EPSILON;
}

/**
 * @brief tests if a point is in a map brush.
 * @param[in] point The point to check whether it's inside the brush boundaries or not
 * @param[in] mode determines how epsilons are applied
 * @return qtrue if the supplied point is inside the brush
 */
static inline qboolean Check_IsPointInsideBrush (const vec3_t point, const mapbrush_t *brush, const pointInBrush_t mode)
{
	int i;
	int numPlanes = 0; /* how many of the sides the point is on. on 2 sides, means on an edge. on 3 a vertex */
	/* PIB_INCL_SURF is the default */
	/* apply epsilon the other way if the surface is excluded */
	const float epsilon = CH_DIST_EPSILON * (mode == PIB_EXCL_SURF ? -1.0f : 1.0f);

	for (i = 0; i < brush->numsides; i++) {
		const plane_t *plane = &mapplanes[brush->original_sides[i].planenum];

		/* if the point is on the wrong side of any face, then it is outside */
		/* distance to one of the planes of the sides, negative implies the point is inside this plane */
		const float dist = Check_PointPlaneDistance(point, plane);
		if (dist > epsilon)
			return qfalse;

		numPlanes += fabs(dist) < CH_DIST_EPSILON ? 1 : 0;
	}

	if (mode == PIB_ON_SURFACE_ONLY && numPlanes == 0)
		return qfalse; /* must be on at least one surface */

	if (mode == PIB_INCL_SURF_EXCL_EDGE && numPlanes > 1)
		return qfalse; /* must not be on more than one side, that would be an edge */

	/* inside all planes, therefore inside the brush */
	return qtrue;
}

/**
 * @brief Perform an entity check
 */
void CheckEntities (void)
{
	int i;

	/* include worldspawn, at entities[0] */
	for (i = 0; i < num_entities; i++) {
		entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");
		const entityCheck_t *v;

		for (v = checkArray; v->name; v++)
			if (!strncmp(name, v->name, strlen(v->name))) {
				if (v->checkCallback(e, i) != 0) {
					e->skip = qtrue; /* skip: the entity will not be saved back on -fix */
				}
				break;
			}
		if (!v->name) {
			Check_Printf(VERB_CHECK, qfalse, i, -1, "No check for '%s' implemented\n", name);
		}
	}
}

/**
 * @brief textures take priority over flags. checks if a tex marks a side as having a
 * special property.
 * @param (surface or content) flag the property to check for. should only have one bit set.
 * @param s the side to check the texture of
 * @return qtrue if the tex indicates the side has the property. Also returns qfalse if
 * the property is not one of those covered by this function.
 * @sa Check_SurfProps to check for one of several properties with one call
 */
static qboolean Check_SurfProp (const int flag, const side_t *s)
{
	const ptrdiff_t index = s - brushsides;
	brush_texture_t *tex = &side_brushtextures[index];
	switch (flag) {
		case SURF_NODRAW:
			return !strcmp(tex->name, "tex_common/nodraw") ? qtrue : qfalse;
		case CONTENTS_WEAPONCLIP:
			return !strcmp(tex->name, "tex_common/weaponclip") ? qtrue : qfalse;
		case CONTENTS_ACTORCLIP:
			return !strcmp(tex->name, "tex_common/actorclip") ? qtrue : qfalse;
		case CONTENTS_ORIGIN:
			return !strcmp(tex->name, "tex_common/origin") ? qtrue : qfalse;
		default:
			return qfalse;
	}
}

/**
 * @brief textures take priority over flags. checks if a tex marks a side as having a
 * special property.
 * @param flags the properties to check for. may have several bits set
 * @param s the side to check the texture of
 * @return qtrue if the tex indicates the side has one of the properties in flags
 * Also returns qfalse if
 * the property is not one of those covered by this function.
 * @sa Check_SurfProp to check for one property with a call
 */
static qboolean Check_SurfProps (const int flags, const side_t *s)
{
	const ptrdiff_t index = s - brushsides;
	brush_texture_t *tex = &side_brushtextures[index];
	char *texname = tex->name;
	if (flags & SURF_NODRAW) {
		if (!strcmp(texname, "tex_common/nodraw"))
			return qtrue;
	} else if (flags & CONTENTS_WEAPONCLIP) {
		if (!strcmp(texname, "tex_common/weaponclip"))
			return qtrue;
	} else if (flags & CONTENTS_ACTORCLIP) {
		if (!strcmp(texname, "tex_common/actorclip"))
			return qtrue;
	} else if (flags & CONTENTS_ORIGIN) {
		if (!strcmp(texname, "tex_common/origin"))
			return qtrue;
	}
	return qfalse;
}

/**
 * @return qtrue for brushes that do not move, are breakable, are seethrough, etc
 */
static qboolean Check_IsOptimisable (const mapbrush_t *b) {
	const entity_t *e = &entities[b->entitynum];
	const char *name = ValueForKey(e, "classname");
	int i, numNodraws = 0;

	if (strcmp(name, "func_group") && strcmp(name, "worldspawn"))
		return qfalse;/* other entities, eg func_breakable are no use */

	/* content flags should be the same on all faces, but we shall be suspicious */
	for (i = 0; i < b->numsides; i++) {
		const side_t *side = &b->original_sides[i];
		if (Check_SurfProps(CONTENTS_ORIGIN | MASK_CLIP, side))
			return qfalse;
		if (side->contentFlags & CONTENTS_TRANSLUCENT)
			return qfalse;
		numNodraws += Check_SurfProp(SURF_NODRAW, side) ? 1 : 0;
	}

	/* all nodraw brushes are special too */
	return numNodraws == b->numsides ? qfalse : qtrue;
}

/**
 * @return qtrue if the bounding boxes intersect or are within CH_DIST_EPSILON of intersecting
 */
static qboolean Check_BoundingBoxIntersects (const mapbrush_t *a, const mapbrush_t *b)
{
	int i;

	for (i = 0; i < 3; i++)
		if (a->mins[i] - CH_DIST_EPSILON >= b->maxs[i] || a->maxs[i] <= b->mins[i] - CH_DIST_EPSILON)
			return qfalse;

	return qtrue;
}

/**
 * @brief add a list of near brushes to each mapbrush. near meaning that the bounding boxes
 * are intersecting or within CH_DIST_EPSILON of touching.
 * @warning includes changeable brushes: mostly non-optimisable brushes will need to be excluded.
 * @sa Check_IsOptimisable
 */
static void Check_NearList (void)
{
	/* this function may be called more than once, but we only want this done once */
	static qboolean done = qfalse;
	mapbrush_t *bbuf[MAX_MAP_BRUSHES];/*< store pointers to brushes here and then malloc them when we know how many */
	int i, j, numNear;

	if (done)
		return;

	/* make a list for iBrush*/
	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];

		/* test all brushes for nearness to iBrush */
		for (j = 0, numNear = 0 ; j < nummapbrushes; j++) {
			mapbrush_t *jBrush = &mapbrushes[j];

			if (i == j) /* do not list a brush as being near itself - not useful!*/
				continue;

			if (!Check_BoundingBoxIntersects(iBrush, jBrush))
				continue;

			/* near, therefore add to temp list for iBrush */
			assert(numNear < nummapbrushes);
			bbuf[numNear++] = jBrush;
		}

		iBrush->numNear = numNear;
		if (!numNear)
			continue;

		/* now we know how many, we can malloc. then copy the pointers */
		iBrush->nearBrushes = (mapbrush_t **)malloc(numNear * sizeof(mapbrush_t *));

		if (!iBrush->nearBrushes)
			Sys_Error("Check_Nearlist: out of memory");

		for (j = 0; j < numNear; j++)
			iBrush->nearBrushes[j] = bbuf[j];
	}

	done = qtrue;
}

/**
 * @brief tests the vertices in the winding of side s.
 * @param[in] mode determines how epsilon is applied
 * @return qtrue if they are all in or on (within epsilon) brush b
 * @sa Check_IsPointInsideBrush
 */
static qboolean Check_SideIsInBrush (const side_t *side, const mapbrush_t *brush, pointInBrush_t mode)
{
	int i;
	const winding_t *w = side->winding;

	assert(w->numpoints > 0);

	for (i = 0; i < w->numpoints ; i++)
		if (!Check_IsPointInsideBrush(w->p[i], brush, mode))
			return qfalse;

	return qtrue;
}

#if 0
/**
 * @brief for debugging, use this to see which face is the problem
 * as there is no non-debugging use (yet) usually #ifed out
 */
static void Check_SetError (side_t *s)
{
	const ptrdiff_t index = s - brushsides;
	brush_texture_t *tex = &side_brushtextures[index];

	Q_strncpyz(tex->name, "tex_common/error", sizeof(tex->name));
}
#endif

/**
 * @brief test if sides abut or intersect
 * @note return qtrue if they do
 * @note assumes the sides are parallel and coincident
 * @note tests for either side having a vertex in the other's brush, this will miss some odd types of intersection
 * @sa ParallelAndCoincident
 */
static qboolean Check_SidesTouch (side_t *a, side_t *b)
{
	side_t *s[2];
	int i, j;

	s[0] = a;
	s[1] = b;

	for (i = 0; i < 2; i++) {
		const winding_t *w = s[i]->winding; /* winding from one of the sides */
		const mapbrush_t *b = s[i ^ 1]->brush; /* the brush that the other side belongs to */

		for (j = 0; j < w->numpoints ; j++) {
			if (Check_IsPointInsideBrush(w->p[j], b, PIB_INCL_SURF))
				return qtrue;
		}
	}
	return qfalse;
}

#if 0
/** @brief returns qtrue if the side has the texture */
static qboolean Check_HasTex(const side_t *s, const char *tex)
{
	const ptrdiff_t index = s - brushsides;
	brush_texture_t *stex = &side_brushtextures[index];

	return strcmp(stex->name, tex) ? qfalse : qtrue;
}
#endif

/**
 * @brief a composite side is a side made of sides from neighbouring brushes. the sides abut.
 * these sides can cooperate to hide a face, this is used for nodraw setting. composite sides may be
 * used for other things in the future.
 */
static void Check_FindCompositeSides (void)
{
	static qboolean done = qfalse;
	int i, is, j, k, l, m, numMembers, numDone = 0, numTodo;

	/* store pointers to sides here and then malloc them when we know how many.
	 * divide by 4 becuase, the minimum number of sides for a brush is 4, so if
	 * all brushes were lined up, and had one side as a member, that would be their number */
	side_t *sbuf[MAX_MAP_SIDES / 4];

	mapbrush_t *bDone[MAX_MAP_SIDES]; /*< an array of brushes to check if the composite propagates across */
	mapbrush_t *bTodo[MAX_MAP_SIDES]; /*< an array of brushes that have been checked, without this it would never stop */

	/* this function may be called more than once, but we only want this done once */
	if (done)
		return;

	Check_NearList();

	/* check each brush, iBrush */
	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];

		if (!Check_IsOptimisable(iBrush))
			continue; /* skip clips etc */

		/* check each side, iSide, of iBrush for being the seed of a composite face */
		for (is = 0; is < iBrush->numsides; is++) {
			side_t *iSide = &iBrush->original_sides[is];

			if (iSide->isCompositeMember || Check_SurfProp(SURF_NODRAW, iSide))
				continue; /* do not find the same composite again. no nodraws */

			/* start making the list of brushes in the composite,
			 * we will only keep it if the composite has more than member */
			sbuf[0] = iSide; /* set iSide->isCompositeMember = true later, if we keep the composite */
			numMembers = 1;

			/* add neighbouring brushes to the list to check for composite propagation */
			numTodo = iBrush->numNear;
			for (j = 0; j < iBrush->numNear; j++)
				if (Check_IsOptimisable(iBrush->nearBrushes[j]))
					bTodo[j] = iBrush->nearBrushes[j];

			/* this brush's nearlist is listed for checking, so it is done */
			bDone[numDone++] = iBrush;

			while (numTodo > 0) {
				mapbrush_t *bChecking = bTodo[--numTodo];
				if (bChecking == NULL)
					continue;
				bDone[numDone++] = bChecking; /* remember so it is not added to the todo list again */

				for (j = 0; j < bChecking->numsides; j++) {
					side_t *sChecking = &bChecking->original_sides[j];

					if (Check_SurfProp(SURF_NODRAW, sChecking))
						continue; /* no nodraws in composites */

					if (ParallelAndCoincidentTo(iSide, sChecking)) {

						/* test if sChecking intersects or touches any of sides that are already in the composite*/
						for (k = 0; k < numMembers; k++) {
							if (Check_SidesTouch(sChecking, sbuf[k])) {
								const mapbrush_t *newMembersBrush = sChecking->brush;
								sbuf[numMembers++] = sChecking; /* add to the array of members */
								sChecking->isCompositeMember = qtrue;

								/* add this brush's nearList to the todo list, as the compostite may propagate through it */
								for (l = 0; l < newMembersBrush->numNear;l++) {
									mapbrush_t *nearListBrush = newMembersBrush->nearBrushes[l];

									if (!Check_IsOptimisable(nearListBrush))
										continue; /* do not propogate across clips etc */

									/* only add them to the todo list if they are not on the done list
									 * as a brush cannot have parallel sides, this also ensures the same side
									 * is not added to a composite more than once */
									for (m = 0; m < numDone; m++) {
										if (nearListBrush == bDone[m])
											goto skip_add_brush_to_todo_list;
									}
									bTodo[numTodo++] = nearListBrush;

									skip_add_brush_to_todo_list:
									; /* there must be a statement after the label, ";" will do */
								}
								goto next_brush_todo; /* need not test any more sides of this brush, if a member is found */
							}
						}
					}
				}
				next_brush_todo:
				;
			}

			if (numMembers > 1) { /* composite found */
				side_t **sidesInNewComposite = (side_t **)malloc(numMembers * sizeof(side_t *));

				if (!sidesInNewComposite)
					Sys_Error("Check_FindCompositeSides: out of memory");

				/* this was not done before for the first side, as we did not know it would have at least 2 members */
				iSide->isCompositeMember = qtrue;

				compositeSides[numCompositeSides].numMembers = numMembers;
				compositeSides[numCompositeSides].memberSides = sidesInNewComposite;

				for (j = 0; j < numMembers; j++) {
					compositeSides[numCompositeSides].memberSides[j] = sbuf[j];
				}
				numCompositeSides++;
			}
		}
	}

	Check_Printf(VERB_EXTRA, qfalse, -1, -1, "%i composite sides found\n", numCompositeSides);

	done = qtrue;
}

/**
 * @brief free the mapbrush_t::nearBrushes and compositeSides
 */
void Check_Free (void)
{
	int i;
	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];
		if (iBrush->numNear) {
			assert(iBrush->nearBrushes);
			free(iBrush->nearBrushes);
			iBrush->numNear = 0;
			iBrush->nearBrushes = NULL;
		}
	}

	for (i = 0; i < numCompositeSides; i++) {
		compositeSide_t *cs = &compositeSides[i];
		if (cs->numMembers) {
			assert(cs->memberSides);
			free(cs->memberSides);
			cs->numMembers = 0;
			cs->memberSides = NULL;
		}
	}
}

/**
 * @brief 	calculate where an edge (defined by the vertices) intersects a plane.
 *			http://local.wasp.uwa.edu.au/~pbourke/geometry/planeline/
 * @param[out] the position of the intersection, if the edge is not too close to parallel.
 * @return 	zero if the edge is within an epsilon angle of parallel to the plane, or the edge is near zero length.
 * @note	an epsilon is used to exclude the actual vertices from passing the test.
 */
static int Check_EdgePlaneIntersection (const vec3_t vert1, const vec3_t vert2, const plane_t *plane, vec3_t intersection)
{
	vec3_t direction; /* a vector in the direction of the line */
	vec3_t lineToPlane; /* a line from vert1 on the line to a point on the plane */
	float sin; /* sine of angle to plane, cosine of angle to normal */
	float param; /* param in line equation  line = vert1 + param * (vert2 - vert1) */
	float length; /* length of the edge */

	VectorSubtract(vert2, vert1, direction);/*< direction points from vert1 to vert2 */
	length = VectorLength(direction);
	if (length < DIST_EPSILON)
		return qfalse;
	sin = DotProduct(direction, plane->normal) / length;
	if (fabs(sin) < SIN_EPSILON)
		return qfalse;
	VectorSubtract(plane->planeVector[0], vert1, lineToPlane);
	param = DotProduct(plane->normal, lineToPlane) / DotProduct(plane->normal, direction);
	VectorMul(param, direction, direction);/*< now direction points from vert1 to intersection */
	VectorAdd(vert1, direction, intersection);
	param = param * length;/*< param is now the distance along the edge from vert1 */
	return (param > CH_DIST_EPSILON) && (param < (length - CH_DIST_EPSILON));
}

/**
 * @brief tests the lines joining the vertices in the winding
 * @return qtrue if the any lines intersect the brush
 */
static qboolean Check_WindingIntersects (const winding_t *winding, const mapbrush_t *brush)
{
	vec3_t intersection;
	int vi, bi;

	for (bi = 0; bi < brush->numsides; bi++) {
		for (vi = 0; vi < winding->numpoints; vi++) {
			const int val = vi + 1;
			const int vj = (winding->numpoints == val) ? 0 : val;
			if (Check_EdgePlaneIntersection(winding->p[vi], winding->p[vj], &mapplanes[brush->original_sides[bi].planenum], intersection))
				if (Check_IsPointInsideBrush(intersection, brush, PIB_INCL_SURF_EXCL_EDGE))
					return qtrue;
		}
	}
	return qfalse;
}

/**
 * @brief reports intersection between optimisable map brushes
 */
void Check_BrushIntersection (void)
{
	int i, j, is;

	/* initialise mapbrush_t.nearBrushes */
	Check_NearList();

	for (i = 0; i < nummapbrushes; i++) {
		const mapbrush_t *iBrush = &mapbrushes[i];

		if (!Check_IsOptimisable(iBrush))
			continue;

		for (j = 0; j < iBrush->numNear; j++) {
			const mapbrush_t *jBrush = iBrush->nearBrushes[j];

			if (!Check_IsOptimisable(jBrush))
				continue;

			/* check each side of i for intersection with brush j */
			for (is = 0; is < iBrush->numsides; is++) {
				const winding_t *winding = (iBrush->original_sides[is].winding);
				if (Check_WindingIntersects(winding, jBrush)) {
					Check_Printf(VERB_CHECK, qfalse, iBrush->entitynum, iBrush->brushnum, "intersects with brush %i (entity %i)\n", jBrush->brushnum, jBrush->entitynum);
					break;
				}
			}
		}
	}
}

/**
 * @brief finds point of intersection of two finite lines, if one exists
 * @param[in] e1p1 first point defining line 1
 * @param[in] e1p2 second point defining line 1
 * @param[out] intersection will be set to the point of intersection, if one exists
 * @return qtrue if the lines intersect between the given points
 * @note http://mathworld.wolfram.com/Line-LineDistance.html
 */
static qboolean Check_EdgeEdgeIntersection (const vec3_t e1p1, const vec3_t e1p2,
					const vec3_t e2p1, const vec3_t e2p2, vec3_t intersection)
{
	vec3_t dir1, dir2, unitDir1, unitDir2;
	vec3_t dirClosestApproach, from1To2, e1p1ToIntersection, e2p1ToIntersection;
	vec3_t cross1, cross2;
	float cosAngle, length1, length2, dist, magCross2, param1;
	float e1p1Dist, e2p1Dist;

	VectorSubtract(e1p2, e1p1, dir1);
	VectorSubtract(e2p2, e2p1, dir2);
	length1 = VectorLength(dir1);
	length2 = VectorLength(dir2);

	if (length1 < CH_DIST_EPSILON || length2 < CH_DIST_EPSILON)
		return qfalse; /* edges with no length cannot intersect */

	VectorScale(dir1, 1.0f / length1, unitDir1);
	VectorScale(dir2, 1.0f / length2, unitDir2);

	cosAngle = fabs(DotProduct(unitDir1, unitDir2));

	if (cosAngle >= COS_EPSILON)
		return qfalse; /* parallel lines either do not intersect, or are coincident */

	CrossProduct(unitDir1, unitDir2, dirClosestApproach);
	VectorNormalize(dirClosestApproach);

	VectorSubtract(e2p1, e1p1, from1To2);
	dist = fabs(DotProduct(dirClosestApproach, from1To2));

	if (dist > CH_DIST_EPSILON)
		return qfalse; /* closest approach of skew lines is nonzero: no intersection */

	CrossProduct(from1To2, dir2, cross1);
	CrossProduct(dir1, dir2, cross2);
	magCross2 = VectorLength(cross2);
	param1 = DotProduct(cross1, cross2) / (magCross2 * magCross2);
	VectorScale(dir1, param1, e1p1ToIntersection);
	VectorAdd(e1p1, e1p1ToIntersection, intersection);
	e1p1Dist = DotProduct(e1p1ToIntersection, unitDir1);

	if (e1p1Dist < CH_DIST_EPSILON || e1p1Dist > (length1 - CH_DIST_EPSILON))
		return qfalse; /* intersection is not between vertices of edge 1 */

	VectorSubtract(intersection, e2p1, e2p1ToIntersection);
	e2p1Dist = DotProduct(e2p1ToIntersection, unitDir2);
	if (e2p1Dist < CH_DIST_EPSILON || e2p1Dist > (length2 - CH_DIST_EPSILON))
		return qfalse; /* intersection is not between vertices of edge 2 */

	return qtrue;
}

/**
 * @brief test if three points are in a straight line in a robust way
 * @note if 2 points are very close, then there are essentially only 2 points, which must be in a straight line
 * @note this function should return the same result regardless of the order the points are sent
 * @note calculates how far off the line one of the points is and uses an epsilon to test.
 * @return qtrue if the 3 points are in a line
 */
static qboolean Check_PointsAreCollinear (const vec3_t a, const vec3_t b, const vec3_t c)
{
	vec3_t d1, d2, d3, cross;
	float d1d, d2d, d3d, offLineDist;

	VectorSubtract(a, b, d1);
	VectorSubtract(a, c, d2);
	VectorSubtract(b, c, d3);

	d1d = VectorLength(d1);
	d2d = VectorLength(d2);
	d3d = VectorLength(d3);

	/* if 2 points are in the same place, we only have 2 points, which must be in a line */
	if (d1d < CH_DIST_EPSILON || d2d < CH_DIST_EPSILON || d3d < CH_DIST_EPSILON)
		return qtrue;

	if (d1d >= d2d && d1d >= d3d) {
		CrossProduct(d2, d3, cross);
		offLineDist = VectorLength(cross) / d1d;
	} else if (d2d >= d1d && d2d >= d3d) {
		CrossProduct(d1, d3, cross);
		offLineDist = VectorLength(cross) / d2d;
	} else { /* d3d must be the largest */
		CrossProduct(d1, d2, cross);
		offLineDist = VectorLength(cross) / d3d;
	}

	return offLineDist < CH_DIST_EPSILON_COLLINEAR_POINTS;
}

#define VERT_BUF_SIZE_DISJOINT_SIDES 21

/**
 * @brief tests if sides overlap, for z-fighting check
 * @note the sides must be on a common plane. if they are not, the result is unspecified
 * @note http://mathworld.wolfram.com/Collinear.html
 */
static qboolean Check_SidesOverlap (const side_t *s1, const side_t *s2)
{
	vec3_t vertbuf[VERT_BUF_SIZE_DISJOINT_SIDES];/* vertices of intersection of sides. arbitrary choice of size: more than 4 is unusual */
	int numVert = 0, i, j, k;
	winding_t *w[2];
	mapbrush_t *b[2];

	w[0] = s1->winding; w[1] = s2->winding;
	b[0] = s1->brush; b[1] = s2->brush;

	/* test if points from first winding are in (or on) brush that is parent of second winding
	 * and vice - versa. i ^ 1 toggles */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < w[i]->numpoints ; j++) {
			if (Check_IsPointInsideBrush(w[i]->p[j], b[i ^ 1], PIB_INCL_SURF)) {
				if (numVert == VERT_BUF_SIZE_DISJOINT_SIDES) {
					Check_Printf(VERB_NORMAL, qfalse, b[i]->entitynum, b[i]->brushnum, "warning: Check_SidesAreDisjoint buffer too small");
					return qfalse;
				}
				VectorCopy(w[i]->p[j], vertbuf[numVert]);
				numVert++;
			}
		}
	}

	/* test for intersections between windings*/
	for (i = 0; i < w[0]->numpoints; i++) {
		const int pointIndex = (i + 1) % w[0]->numpoints;
		for (k = 0; k < w[1]->numpoints; k++) {
			const int pointIndex2 = (k + 1) % w[1]->numpoints;
			if (Check_EdgeEdgeIntersection(w[0]->p[i], w[0]->p[pointIndex], w[1]->p[k], w[1]->p[pointIndex2], vertbuf[numVert])) {
				numVert++; /* if intersection, keep it */
				if (numVert == VERT_BUF_SIZE_DISJOINT_SIDES) {
					Check_Printf(VERB_NORMAL, qfalse, b[i]->entitynum, b[i]->brushnum, "warning: Check_SidesAreDisjoint buffer too small");
					return qfalse;
				}
			}
		}
	}

	if (numVert < 3)
		return qfalse; /* must be at least 3 points to be not in a line */

	{
		vec3_t from0to1, one, zero;

		/* skip past elements 0, 1, ... if they are coincident - to avoid division by zero */
		i = 0;
		do {
			i++;
			if ((i + 1) >= numVert)
				return qfalse; /* not enough separated points - they must be in a line */
			VectorSubtract(vertbuf[i], vertbuf[i - 1], from0to1);
			VectorCopy(vertbuf[i - 1], zero);
			VectorCopy(vertbuf[i], one);
		} while (VectorLength(from0to1) < CH_DIST_EPSILON);

		for (i++; i < numVert; i++) {
			if (!Check_PointsAreCollinear(zero, one, vertbuf[i])) {
#if 0
				Com_Printf("\non-collinear points\n");
				Print3Vector(zero);
				Print3Vector(one);
				Print3Vector(vertbuf[i]);
#endif
				return qtrue; /* 3 points not in a line, there is overlap */
			}
		}
	}

	return qfalse; /* all points are collinear */
}

/**
 * @brief check all brushes for overlapping shared faces
 *  @todo maybe too fussy. perhaps should ignore small overlaps.
 */
void CheckZFighting (void)
{
	int i, j, is, js;

	/* initialise mapbrush_t.nearBrushes */
	Check_NearList();

	/* loop through all pairs of near brushes */
	for (i = 0; i < nummapbrushes; i++) {
		const mapbrush_t *iBrush = &mapbrushes[i];

		if (!Check_IsOptimisable(iBrush))
			continue; /* skip moving brushes, clips etc */

		for (j = 0; j < iBrush->numNear; j++) {
			const mapbrush_t *jBrush = iBrush->nearBrushes[j];

			if ((iBrush->contentFlags & CONTENTS_LEVEL_ALL) != (jBrush->contentFlags & CONTENTS_LEVEL_ALL))
				continue; /* must be on the same level */

			if (!Check_IsOptimisable(jBrush))
				continue; /* skip moving brushes, clips etc */

			for (is = 0; is < iBrush->numsides; is++) {
				const side_t *iSide = &iBrush->original_sides[is];

				if (Check_SurfProp(SURF_NODRAW, iSide))
					continue; /* skip nodraws */

				if (Check_SidePointsDown(iSide))
					continue; /* can't see these, view is always from above */

				/* check each side of brush j for doing the hiding */
				for (js = 0; js < jBrush->numsides; js++) {
					const side_t *jSide = &jBrush->original_sides[js];

					/* skip nodraws */
					if (Check_SurfProp(SURF_NODRAW, jSide))
						continue;

#if 0
					/** running this chunk on a large map proves that plane indices
					  * cannot be relied on to test sides are from a common plane
					  * @todo this test needs repeating*/
					if (ParallelAndCoincidentTo(iSide, jSide))
						if (jSide->planenum != jSide->planenum)
							Com_Printf("CheckZFighting: plane indices %i %i \n",
								iSide->planenum, jSide->planenum);
#endif

					if (ParallelAndCoincidentTo(iSide, jSide) ) {
						if (Check_SidesOverlap(iSide, jSide)) {
							Check_Printf(VERB_CHECK, qfalse, iBrush->entitynum, iBrush->brushnum,
								"z-fighting with brush %i (entity %i)\n", jBrush->brushnum, jBrush->entitynum);
#if 0
							Check_SetError(iSide);
							Check_SetError(jSide);
#endif
						}
					}
				}
			}
		}
	}
}

/**
 * @brief find duplicated brushes and brushes contained inside brushes
 */
void Check_ContainedBrushes (void)
{
	int i, j, js;

	/* initialise mapbrush_t.nearBrushes */
	Check_NearList();

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];

		/* do not check for brushes inside special (clip etc) brushes */
		if (!Check_IsOptimisable(iBrush))
			continue;

		for (j = 0; j < iBrush->numNear; j++) {
			mapbrush_t *jBrush = iBrush->nearBrushes[j];
			int numSidesInside = 0;

			for (js = 0; js < jBrush->numsides; js++) {
				const side_t *jSide = &jBrush->original_sides[js];

				if (Check_SideIsInBrush(jSide, iBrush, PIB_INCL_SURF))
					numSidesInside++;
			}

			if (numSidesInside == jBrush->numsides) {
				Check_Printf(VERB_CHECK, qfalse, jBrush->entitynum, jBrush->brushnum, "inside brush %i (entity %i)\n",
							iBrush->brushnum, iBrush->entitynum);
			}
		}
	}
}

/**
 * @return nonzero if for any level selection the coveree will only be hidden when the coverer is too.
 * so the coveree may safely be set to nodraw, as far as levelflags are concerned.
 */
static int Check_LevelForNodraws (const side_t *coverer, const side_t *coveree)
{
	return !(CONTENTS_LEVEL_ALL & ~coverer->contentFlags & coveree->contentFlags);
}

static void Check_SetNodraw (side_t *s)
{
	const ptrdiff_t index = s - brushsides;
	brush_texture_t *tex = &side_brushtextures[index];

	Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));

	/* do not actually set the flag that will be written back on -fix
	 * the texture is set, this should trigger the flag to be set
	 * in compile mode. check should behave the same as fix.
	 * The flag must be set in compile mode, as SetImpliedFlags calls are before the
	 * CheckNodraws call */
	if (!(config.fixMap || config.performMapCheck))
		tex->surfaceFlags |= SURF_NODRAW;

	s->surfaceFlags &= ~SURF_PHONG;
	tex->surfaceFlags &= ~SURF_PHONG;
	s->surfaceFlags |= SURF_NODRAW;
}

#define CH_COMP_NDR_EDGE_INTSCT_BUF 21

/**
 * @brief check for
 * faces which can safely be set to SURF_NODRAW because they are pressed against
 * the faces of other brushes. Also set faces pointing near straight down nodraw.
 * @todo test for sides hidden by composite faces
 * @note probably cannot warn about faces which are nodraw, but might be visible, as there will
 * always be planty of optimisations beyond faces being hidden by one brush, or composite faces.
 */
void CheckNodraws (void)
{
	int i, j, k, l, m, n, is, js;
	int numSetFromSingleSide = 0, numSetPointingDown = 0, numSetFromCompositeSide = 0, iBrushNumSet = 0;

	/* Initialise compositeSides[].. Note that this function
	 * calls Check_NearList to initialise mapbrush_t.nearBrushes */
	Check_FindCompositeSides();

	/* check each brush, i, for downward sides */
	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];
		iBrushNumSet = 0;

		/* skip moving brushes, clips etc */
		if (!Check_IsOptimisable(iBrush))
			continue;

		/* check each side of i for pointing down */
		for (is = 0; is < iBrush->numsides; is++) {
			side_t *iSide = &iBrush->original_sides[is];

			/* skip those that are already nodraw */
			if (Check_SurfProp(SURF_NODRAW, iSide))
				continue;
			/* surface lights may point downwards */
			else if (iSide->surfaceFlags & SURF_LIGHT)
				continue;

			if (Check_SidePointsDown(iSide)) {
				Check_SetNodraw(iSide);
				numSetPointingDown++;
				iBrushNumSet++;
			}

		}
		if (iBrushNumSet)
			Check_Printf(VERB_EXTRA, qtrue, iBrush->entitynum, iBrush->brushnum, "set nodraw on %i sides (point down, or are close to pointing down).\n", iBrushNumSet);
	} /* next iBrush for downward faces that can be nodraw */
	if (numSetPointingDown)
		Check_Printf(VERB_CHECK, qtrue, -1, -1, "total of %i nodraws set (point down, or are close to pointing down)\n", numSetPointingDown);

	/* check each brush, i, for hidden sides */
	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *iBrush = &mapbrushes[i];
		iBrushNumSet = 0;

		/* skip moving brushes, clips etc */
		if (!Check_IsOptimisable(iBrush))
			continue;

		/* check each brush, j, for having a side that hides one of i's faces */
		for (j = 0; j < iBrush->numNear; j++) {
			mapbrush_t *jBrush = iBrush->nearBrushes[j];

			/* skip moving brushes, clips etc */
			if (!Check_IsOptimisable(jBrush))
				continue;

			/* check each side of i for being hidden */
			for (is = 0; is < iBrush->numsides; is++) {
				side_t *iSide = &iBrush->original_sides[is];

				/* skip those that are already nodraw */
				if (Check_SurfProp(SURF_NODRAW, iSide))
					continue;
				/* surface lights may point downwards */
				else if (iSide->surfaceFlags & SURF_LIGHT)
					continue;

				/* check each side of brush j for doing the hiding */
				for (js = 0; js < jBrush->numsides; js++) {
					const side_t *jSide = &jBrush->original_sides[js];

#if 0
					/* run on a largish map, this section proves that the plane indices alone cannot
					 * cannot be relied on to test for 2 planes facing each other. */
					if (FacingAndCoincidentTo(iSide, jSide)) {
						const int minIndex = min(iSide->planenum, jSide->planenum);
						const int maxIndex = max(iSide->planenum, jSide->planenum);
						const int diff = maxIndex - minIndex, minOdd = (minIndex & 1);
						if ((diff != 1) || minOdd) {
							Com_Printf("CheckNodraws: facing and coincident plane indices %i %i diff:%i minOdd:%i\n",
								iSide->planenum, jSide->planenum, diff, minOdd);
						}
					}
#endif

					if (Check_LevelForNodraws(jSide, iSide) &&
						FacingAndCoincidentTo(iSide, jSide) &&
						Check_SideIsInBrush(iSide, jBrush, PIB_INCL_SURF)) {
						Check_SetNodraw(iSide);
						iBrushNumSet++;
						numSetFromSingleSide++;
					}
				}
			}
		} /* next jBrush */
		if (iBrushNumSet)
			Check_Printf(VERB_EXTRA, qtrue, iBrush->entitynum, iBrush->brushnum, "set nodraw on %i sides (covered by another brush).\n", iBrushNumSet);

		iBrushNumSet = 0; /* reset to count composite side coverings */

		/* check each composite side for hiding one of iBrush's sides */
		for (j = 0; j < numCompositeSides; j++) {
			const compositeSide_t *composite = &compositeSides[j];
			assert(composite);
			assert(composite->memberSides[0]);

			/* check each side for being hidden */
			for (is = 0; is < iBrush->numsides; is++) {
				side_t *iSide = &iBrush->original_sides[is];
				winding_t *iWinding;
				vec3_t lastIntersection = {0, 0, 0}; /* used in innner loop, here to avoid repeated memset calls */

				if (!FacingAndCoincidentTo(iSide, composite->memberSides[0]))
					continue; /* all sides in the composite are parallel, and iSide must face them to be hidden */

				/* skip those that are already nodraw. note: this includes sides hidden by single sides
				 * set above - this prevents duplicate nodraw reports */
				if (Check_SurfProp(SURF_NODRAW, iSide))
					continue;

				iWinding = iSide->winding;

				/* to be covered each vertex of iSide must be on one of the composite side's members */
				for (k = 0; k < iWinding->numpoints; k++) {
					qboolean pointOnComposite = qfalse;
					for (l = 0; l < composite->numMembers; l++) {
						if (Check_IsPointInsideBrush(iWinding->p[k], composite->memberSides[l]->brush, PIB_INCL_SURF)) {

							/* levelflags mean this member cannot cover iSide
							 * might be wrong to assume the composite will not cover iSide (if the members intersect)
							  * it is _safe_ in that it will not result in an exposed nodraw */
							if (!Check_LevelForNodraws(composite->memberSides[l], iSide))
								goto next_iSide;

							pointOnComposite = qtrue;
							break;
						}
					}
					if (!pointOnComposite)
						goto next_iSide;
				}

				/* search for intersections between composite and iSide */
				for (k = 0; k < iWinding->numpoints; k++) {
					vec3_t intersection;
					int lastIntersectionMembInd = -1;
					vec3_t intersections[CH_COMP_NDR_EDGE_INTSCT_BUF];
					qboolean paired[CH_COMP_NDR_EDGE_INTSCT_BUF];
					int numIntsct = 0;

					memset(paired, '\0', CH_COMP_NDR_EDGE_INTSCT_BUF * sizeof(qboolean));

					for (l = 0; l < composite->numMembers; l++) {
						const winding_t *mWinding = composite->memberSides[l]->winding;

						for (m = 0; m < mWinding->numpoints; m++) {
							qboolean intersects = Check_EdgeEdgeIntersection(
								iWinding->p[k], iWinding->p[(k + 1) % iWinding->numpoints],
								mWinding->p[m], mWinding->p[(m + 1) % mWinding->numpoints],
								intersection);

							if (intersects) {
								qboolean coincident = qfalse;
								/* check for coincident intersections */
								for (n = 0; n < numIntsct; n++) {
									float distSq = VectorDistSqr(intersection, intersections[n]);
									if (CH_DIST_EPSILON_SQR > distSq) {
										paired[n] = qtrue;
										coincident = qtrue;
									}
								}

								/* if it is not coincident, then add it to the list */
								if (!coincident) {
									VectorCopy(intersection, intersections[numIntsct]);
									numIntsct++;
									if (numIntsct >= CH_COMP_NDR_EDGE_INTSCT_BUF) {
										Check_Printf(VERB_LESS, qfalse, -1, -1, "warning: CheckNodraws: buffer too small");
										return;
									}
								}

								/* if edge k of iSide crosses side l of composite then check levelflags */
								/* note that as each member side is convex any line can intersect its edges a maximum of twice,
								 * as the member sides of the composite are the inner loop, these two (if they exist) will
								 * be found consecutively */
								if ((lastIntersectionMembInd == l) /* same composite as last intersection found */
								 && (VectorDistSqr(intersection, lastIntersection) > CH_DIST_EPSILON_SQR) /* dist between this and last intersection is nonzero, indicating they are different intersections */
								 && !Check_LevelForNodraws(composite->memberSides[l], iSide)) /* check nodraws */
									goto next_iSide;

								lastIntersectionMembInd = l;
								VectorCopy(intersection, lastIntersection);
							}
						}
					}

					/* make sure all intersections are paired. an unpaired intersection indicates
					 * that iSide's boundary crosses out of the composite side, so iSide is not hidden */
					for (l = 0; l < numIntsct; l++) {
						if (!paired[l])
							goto next_iSide;
					}

				}

				/* set nodraw for iSide (covered by composite) */
				Check_SetNodraw(iSide);
				iBrushNumSet++;
				numSetFromCompositeSide++;

				next_iSide:
				;
			}
		} /* next composite */
		if (iBrushNumSet)
			Check_Printf(VERB_EXTRA, qtrue, iBrush->entitynum, iBrush->brushnum, "set nodraw on %i sides (covered by a composite side).\n", iBrushNumSet);
	} /* next iBrush */

	if (numSetFromSingleSide)
		Check_Printf(VERB_CHECK, qtrue, -1, -1, "%i nodraws set (covered by another brush).\n", numSetFromSingleSide);

	if (numSetFromCompositeSide)
		Check_Printf(VERB_CHECK, qtrue, -1, -1, "%i nodraws set (covered by a composite side).\n", numSetFromCompositeSide);

}

/**
 * @returns false if the brush has a mirrored set of planes,
 * meaning it encloses no volume.
 * also checks for planes without any normal
 */
static qboolean Check_DuplicateBrushPlanes (const mapbrush_t *b)
{
	int i, j;
	const side_t *sides = b->original_sides;

	for (i = 1; i < b->numsides; i++) {
		/* check for a degenerate plane */
		if (sides[i].planenum == -1) {
			Check_Printf(VERB_CHECK, qfalse, b->entitynum, b->brushnum, "degenerate plane\n");
			continue;
		}

		/* check for duplication and mirroring */
		for (j = 0; j < i; j++) {
			if (sides[i].planenum == sides[j].planenum) {
				/* remove the second duplicate */
				Check_Printf(VERB_CHECK, qfalse, b->entitynum, b->brushnum, "mirrored or duplicated\n");
				break;
			}

			if (sides[i].planenum == (sides[j].planenum ^ 1)) {
				Check_Printf(VERB_CHECK, qfalse, b->entitynum, b->brushnum, "mirror plane - brush is invalid\n");
				return qfalse;
			}
		}
	}
	return qtrue;
}

/**
 * @sa BrushVolume
 */
static vec_t Check_MapBrushVolume (const mapbrush_t *brush)
{
	int i;
	const winding_t *w;
	vec3_t corner;
	vec_t d, area, volume;
	const plane_t *plane;

	if (!brush)
		return 0;

	/* grab the first valid point as the corner */
	w = NULL;
	for (i = 0; i < brush->numsides; i++) {
		w = brush->original_sides[i].winding;
		if (w)
			break;
	}
	if (!w)
		return 0;
	VectorCopy(w->p[0], corner);

	/* make tetrahedrons to all other faces */
	volume = 0;
	for (; i < brush->numsides; i++) {
		w = brush->original_sides[i].winding;
		if (!w)
			continue;
		plane = &mapplanes[brush->original_sides[i].planenum];
		d = -(DotProduct(corner, plane->normal) - plane->dist);
		area = WindingArea(w);
		volume += d * area;
	}

	return volume / 3;
}

/**
 * @brief report brushes from the map below 1 unit^3
 */
void CheckMapMicro (void)
{
	int i;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];
		const float vol = Check_MapBrushVolume(brush);
		if (vol < config.mapMicrovol) {
			Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "microbrush volume %f - will be deleted\n", vol);
			brush->skipWriteBack = qtrue;
		}
	}
}

/**
 * @brief prints a list of the names of the set content flags or "no contentflags" if all bits are 0
 * @todo make this use names like CONTENTS_STEPON, so it is found when the code is searched.
 * @todo check list of content flags is up to date
 */
void DisplayContentFlags (const int flags)
{
	if (!flags) {
		Check_Printf(VERB_CHECK, qfalse, NUM_SAME, NUM_SAME, " no contentflags");
		return;
	}
#define M(x) if (flags & CONTENTS_##x) Check_Printf(VERB_CHECK, qfalse, NUM_SAME, NUM_SAME, " " #x)
	M(SOLID);
	M(WINDOW);
	M(WATER);
	M(LEVEL_1);
	M(LEVEL_2);
	M(LEVEL_3);
	M(LEVEL_4);
	M(LEVEL_5);
	M(LEVEL_6);
	M(LEVEL_7);
	M(LEVEL_8);
	M(ACTORCLIP);
	M(PASSABLE);
	M(ACTOR);
	M(ORIGIN);
	M(WEAPONCLIP);
	M(DEADACTOR);
	M(DETAIL);
	M(TRANSLUCENT);
#undef x
}

/**
 * @brief calculate the bits that have to be set to fill levelflags such that they are contiguous
 */
static int Check_CalculateLevelFlagFill (int contentFlags)
{
	int firstSetLevel = 0, lastSetLevel = 0;
	int scanLevel, flagFill = 0;

	for (scanLevel = CONTENTS_LEVEL_1; scanLevel <= CONTENTS_LEVEL_8; scanLevel <<= 1) {
		if (scanLevel & contentFlags) {
			if (!firstSetLevel) {
				firstSetLevel = scanLevel;
			} else {
				lastSetLevel = scanLevel;
			}
		}
	}
	for (scanLevel = firstSetLevel << 1 ; scanLevel < lastSetLevel; scanLevel <<= 1)
		flagFill |= scanLevel & ~contentFlags;
	return flagFill;
}

/**
 * @brief ensures set levelflags are in one contiguous block
 */
void CheckFillLevelFlags (void)
{
	int i, j, flagFill;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		/* CheckLevelFlags should be done first, so we will boldly
		 * assume that levelflags are the same on each face */
		flagFill = Check_CalculateLevelFlagFill(brush->original_sides[0].contentFlags);
		if (flagFill) {
			Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "making set levelflags continuous by setting");
			DisplayContentFlags(flagFill);
			Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "\n");
			for (j = 0; j < brush->numsides; j++)
				brush->original_sides[j].contentFlags |= flagFill;
		}
	}
}

/**
 * @brief sets all levelflags, if none are set.
 */
void CheckLevelFlags (void)
{
	int i, j;
	qboolean allNodraw, setFlags;
	int allLevelFlagsForBrush;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		/* test if all faces are nodraw */
		allNodraw = qtrue;
		for (j = 0; j < brush->numsides; j++) {
			const side_t *side = &brush->original_sides[j];
			assert(side);

			if (!(Check_SurfProp(SURF_NODRAW, side))) {
				allNodraw = qfalse;
				break;
			}
		}

		/* proceed if some or all faces are not nodraw */
		if (!allNodraw) {
			allLevelFlagsForBrush = 0;

			setFlags = qfalse;
			/* test if some faces do not have levelflags and remember
			 * all levelflags which are set. */
			for (j = 0; j < brush->numsides; j++) {
				const side_t *side = &brush->original_sides[j];

				allLevelFlagsForBrush |= (side->contentFlags & CONTENTS_LEVEL_ALL);

				if (!(side->contentFlags & (CONTENTS_ORIGIN | MASK_CLIP))) {
					/* check level 1 - level 8 */
					if (!(side->contentFlags & CONTENTS_LEVEL_ALL)) {
						setFlags = qtrue;
						break;
					}
				}
			}

			/* set the same flags for each face */
			if (setFlags) {
				const int flagsToSet = allLevelFlagsForBrush ? allLevelFlagsForBrush : CONTENTS_LEVEL_ALL;
				Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "at least one face has no levelflags, setting %i on all faces\n", flagsToSet);
				for (j = 0; j < brush->numsides; j++) {
					side_t *side = &brush->original_sides[j];
					side->contentFlags |= flagsToSet;
				}
			}
		}
	}
}

/**
 * @brief Sets surface flags dependent on assigned texture
 * @sa ParseBrush
 * @sa CheckFlags
 * @note surfaceFlags are set in side_t for map compiling and in brush_texture_t
 * because this is saved back on -fix.
 * @note also removes phongs from nodraws. also removes legacy flags.
 */
void SetImpliedFlags (side_t *side, brush_texture_t *tex, const mapbrush_t *brush)
{
	const char *texname = tex->name;
	const int initSurf = tex->surfaceFlags;
	const int initCont = side->contentFlags;
	const char *flagsDescription = NULL;

	/* see discussion at Check_SetNodraw */
	if (config.fixMap || config.performMapCheck)
		goto skipflagsetting;

	if (!strcmp(texname, "tex_common/actorclip")) {
		side->contentFlags |= CONTENTS_ACTORCLIP;
		flagsDescription = "CONTENTS_ACTORCLIP";
	} else if (!strcmp(texname, "tex_common/caulk")) {
		side->surfaceFlags |= SURF_NODRAW;
		tex->surfaceFlags |= SURF_NODRAW;
		flagsDescription = "SURF_NODRAW";
	} else if (!strcmp(texname, "tex_common/hint")) {
		side->surfaceFlags |= SURF_HINT;
		tex->surfaceFlags |= SURF_HINT;
		flagsDescription = "SURF_HINT";
	} else if (!strcmp(texname, "tex_common/ladder")) {
		side->contentFlags |= CONTENTS_LADDER;
		flagsDescription = "CONTENTS_LADDER";
	} else if (!strcmp(texname, "tex_common/nodraw")) {
		/*side->contentFlags |= CONTENTS_SOLID;*/
		side->surfaceFlags |= SURF_NODRAW;
		tex->surfaceFlags |= SURF_NODRAW;
		flagsDescription = "SURF_NODRAW";
	} else if (!strcmp(texname, "tex_common/trigger")) {
		side->surfaceFlags |= SURF_NODRAW;
		tex->surfaceFlags |= SURF_NODRAW;
		flagsDescription = "SURF_NODRAW";
	} else if (!strcmp(texname, "tex_common/origin")) {
		side->contentFlags |= CONTENTS_ORIGIN;
		flagsDescription = "CONTENTS_ORIGIN";
	} else if (!strcmp(texname, "tex_common/slick")) {
		side->contentFlags |= SURF_SLICK;
		flagsDescription = "SURF_SLICK";
	} else if (!strcmp(texname, "tex_common/weaponclip")) {
		side->contentFlags |= CONTENTS_WEAPONCLIP;
		flagsDescription = "CONTENTS_WEAPONCLIP";
	}

	if (strstr(texname, "water")) {
#if 0
		side->surfaceFlags |= SURF_WARP;
		tex->surfaceFlags |= SURF_WARP;
#endif
		side->contentFlags |= CONTENTS_WATER;
		side->contentFlags |= CONTENTS_PASSABLE;
		flagsDescription = "CONTENTS_WATER and CONTENTS_PASSABLE";
	}

	/* If in check/fix mode and we have made a change, give output. */
	if ((side->contentFlags != initCont) || (tex->surfaceFlags != initSurf)) {
		Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum,
			"%s implied by %s texture has been set\n", flagsDescription ? flagsDescription : "-", texname);
	}

	skipflagsetting:

	/* additional test, which does not directly depend on tex. */
	if (Check_SurfProp(SURF_NODRAW, side) && (tex->surfaceFlags & SURF_PHONG)) {
		/* nodraw never has phong set */
		side->surfaceFlags &= ~SURF_PHONG;
		tex->surfaceFlags &= ~SURF_PHONG;
		Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum,
			"SURF_PHONG unset, as it has SURF_NODRAW set\n");
	}

	if (side->surfaceFlags & SURF_SKIP) {
		side->surfaceFlags &= ~SURF_SKIP;
		Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum,
			"removing legacy flag, SURF_SKIP\n");
	}
}

/**
 * @brief sets content flags based on textures
 */
void CheckFlagsBasedOnTextures (void)
{
	int i, j;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			const ptrdiff_t index = side - brushsides;
			brush_texture_t *tex = &side_brushtextures[index];

			assert(side);
			assert(tex);

			/* set surface and content flags based on texture. */
			SetImpliedFlags(side, tex, brush);
		}
	}
}

/**
 * @brief check that sides have textures and that where content/surface flags are set the texture
 * is correct.
 */
void CheckTexturesBasedOnFlags (void)
{
	int i, j;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			const ptrdiff_t index = side - brushsides;
			brush_texture_t *tex = &side_brushtextures[index];

			assert(side);
			assert(tex);

			/* set textures based on flags */
			if (tex->name[0] == '\0') {
				Check_Printf(VERB_CHECK, qfalse, brush->entitynum, brush->brushnum, " no texture assigned\n");
			}

			if (!Q_strcmp(tex->name, "tex_common/error")) {
				Check_Printf(VERB_CHECK, qfalse, brush->entitynum, brush->brushnum, "error texture assigned - check this brush\n");
			}

			if (!Q_strcmp(tex->name, "NULL")) {
				Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "replaced NULL with nodraw texture\n");
				Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
				tex->surfaceFlags |= SURF_NODRAW;
			}
			if (tex->surfaceFlags & SURF_NODRAW && Q_strcmp(tex->name, "tex_common/nodraw")) {
				Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "set nodraw texture for SURF_NODRAW\n");
				Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
			}
			if (tex->surfaceFlags & SURF_HINT && Q_strcmp(tex->name, "tex_common/hint")) {
				Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum,  "set hint texture for SURF_HINT\n");
				Q_strncpyz(tex->name, "tex_common/hint", sizeof(tex->name));
			}

			if (side->contentFlags & CONTENTS_WEAPONCLIP && Q_strcmp(tex->name, "tex_common/weaponclip")) {
				Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum,  "set weaponclip texture for CONTENTS_WEAPONCLIP\n");
				Q_strncpyz(tex->name, "tex_common/weaponclip", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ACTORCLIP && Q_strcmp(tex->name, "tex_common/actorclip")) {
				Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum,  "*set actorclip texture for CONTENTS_ACTORCLIP\n");
				Q_strncpyz(tex->name, "tex_common/actorclip", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ORIGIN && Q_strcmp(tex->name, "tex_common/origin")) {
				Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "set origin texture for CONTENTS_ORIGIN\n");
				Q_strncpyz(tex->name, "tex_common/origin", sizeof(tex->name));
			}
		}
	}
}

/**
 * @brief some contentlflags are set as a result of some surface flag. For example,
 * if one face is TRANS* then the brush is TRANSLUCENT. this is required by the .map parser
 * as well as th check/fix code.
 * @sa ParseBrush
 */
void CheckPropagateParserContentFlags(mapbrush_t *b)
{
	int notInformedMixedFace = 1;
	int m, contentFlagDiff;
	int transferFlags = (CONTENTS_DETAIL | CONTENTS_TRANSLUCENT);

	for (m = 0; m < b->numsides; m++) {
		contentFlagDiff = (b->original_sides[m].contentFlags ^ b->contentFlags) & transferFlags;
		if (contentFlagDiff) {
			/* only tell them once per brush */
			if (notInformedMixedFace) {
				Check_Printf(VERB_CHECK, qtrue, b->entitynum , b->brushnum, "transferring contentflags to all faces:");
				DisplayContentFlags(contentFlagDiff);
				Check_Printf(VERB_CHECK, qtrue, b->entitynum , b->brushnum, "\n");
				notInformedMixedFace = 0;
			}
			b->original_sides[m].contentFlags |= b->contentFlags ;
		}
	}
}

/**
 * @brief contentflags should be the same on each face of a brush. print warnings
 * if they are not. remove contentflags that are set on less than half of the faces.
 * some content flags are transferred to all faces on parsing, ParseBrush().
 * @todo at the moment only actorclip is removed if only set on less than half of the
 * faces. there may be other contentflags that would benefit from this treatment
 * @sa ParseBrush
 */
void CheckMixedFaceContents (void)
{
	int i, j;
	int nfActorclip; /* number of faces with actorclip contentflag set */

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];
		side_t *side0;

		/* if the origin flag is set in the mapbrush_t struct, then the brushes
		 * work is done, and we can skip the mixed face contents check for this brush*/
		if (brush->contentFlags & CONTENTS_ORIGIN)
			continue;

		side0 = &brush->original_sides[0];
		nfActorclip = 0;

		CheckPropagateParserContentFlags(brush);

		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			assert(side);

			nfActorclip += (side->contentFlags & CONTENTS_ACTORCLIP) ? 1 : 0;

			if (side0->contentFlags != side->contentFlags) {
				const int jNotZero = side->contentFlags & ~side0->contentFlags;
				const int zeroNotJ = side0->contentFlags & ~side->contentFlags;
				Check_Printf(VERB_CHECK, qfalse, brush->entitynum, brush->brushnum, "mixed face contents (");
				if (jNotZero) {
					Check_Printf(VERB_CHECK, qfalse, NUM_SAME, NUM_SAME, "face %i has and face 0 has not", j);
					DisplayContentFlags(jNotZero);
					if (zeroNotJ)
						Check_Printf(VERB_CHECK, qfalse, NUM_SAME, NUM_SAME, ", ");
				}
				if (zeroNotJ) {
					Check_Printf(VERB_CHECK, qfalse, NUM_SAME, NUM_SAME, "face 0 has and face %i has not", j);
					DisplayContentFlags(zeroNotJ);
				}
				Check_Printf(VERB_CHECK, qfalse, NUM_SAME, NUM_SAME, ")\n");
			}
		}

		if (nfActorclip && nfActorclip <  brush->numsides / 2) {
			Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "ACTORCLIP set on less than half of the faces: removing.\n");
			for (j = 0; j < brush->numsides; j++) {
				side_t *side = &brush->original_sides[j];
				const ptrdiff_t index = side - brushsides;
				brush_texture_t *tex = &side_brushtextures[index];

				if (side->contentFlags & CONTENTS_ACTORCLIP && !Q_strcmp(tex->name, "tex_common/actorclip")) {
					Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "removing tex_common/actorclip, setting tex_common/error\n");
					Q_strncpyz(tex->name, "tex_common/error", sizeof(tex->name));
				}

				side->contentFlags &= ~CONTENTS_ACTORCLIP;
			}
		}
	}
}

void CheckBrushes (void)
{
	int i, j;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];

		Check_DuplicateBrushPlanes(brush);

		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];

			assert(side);

			if (side->contentFlags & CONTENTS_ORIGIN && brush->entitynum == 0) {
				Check_Printf(VERB_CHECK, qtrue, brush->entitynum, brush->brushnum, "origin brush inside worldspawn - removed CONTENTS_ORIGIN\n");
				side->contentFlags &= ~CONTENTS_ORIGIN;
			}
		}
	}
}
