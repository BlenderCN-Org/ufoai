/**
 * @file inv_shared.h
 * @brief common object-, inventory-, container- and firemode-related functions headers.
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

#ifndef GAME_INV_SHARED_H
#define GAME_INV_SHARED_H

#include "q_shared.h"

/* this is the absolute max for now */
#define MAX_OBJDEFS		128		/* Remember to adapt the "NONE" define (and similar) if this gets changed. */
#define MAX_MAPDEFS		128
#define MAX_WEAPONS_PER_OBJDEF 4
#define MAX_AMMOS_PER_OBJDEF 4
#define MAX_FIREDEFS_PER_WEAPON 8
#define MAX_DAMAGETYPES 64
#define WEAPON_BALANCE 0.5f
#define SKILL_BALANCE 1.0f
#define INJURY_BALANCE 0.2f
#define INJURY_THRESHOLD 0.5f /* HP / maxHP > INJURY_THRESHOLD no penalty is incurred */

/** @brief Possible inventory actions for moving items between containers */
typedef enum {
	IA_NONE,			/**< no move possible */

	IA_MOVE,			/**< normal inventory item move */
	IA_ARMOUR,			/**< move or swap armour */
	IA_RELOAD,			/**< reload weapon */
	IA_RELOAD_SWAP,		/**< switch loaded ammo */

	IA_NOTIME,			/**< not enough TUs to make this inv move */
	IA_NORELOAD			/**< not loadable or already fully loaded */
} inventory_action_t;

/** @brief this is a fire definition for our weapons/ammo */
typedef struct fireDef_s {
	char name[MAX_VAR];			/**< script id */
	char projectile[MAX_VAR];	/**< projectile particle */
	char impact[MAX_VAR];		/**< impact particle */
	char hitBody[MAX_VAR];		/**< hit the body particles */
	char fireSound[MAX_VAR];	/**< the sound when a recruits fires */
	char impactSound[MAX_VAR];	/**< the sound that is played on impact */
	char hitBodySound[MAX_VAR];	/**< the sound that is played on hitting a body */
	float relFireVolume;
	float relImpactVolume;
	char bounceSound[MAX_VAR];	/**< bouncing sound */

	/* These values are created in Com_ParseItem and Com_AddObjectLinks.
	 * They are used for self-referencing the firedef. */
	struct objDef_s *obj;		/**< The weapon/ammo item this fd is located in. */
	int weapFdsIdx;	/**< The index of the "weapon_mod" entry (objDef_t->fd[weapFdsIdx]) this fd is located in.
						 ** Depending on this value you can find out via objDef_t->weapIdx[weapFdsIdx] what weapon this firemode is used for.
						 ** This does _NOT_ equal the index of the weapon object in ods.
						 */
	int fdIdx;		/**< Self link of the fd in the objDef_t->fd[][fdIdx] array. */

	qboolean soundOnce;
	qboolean gravity;			/**< Does gravity has any influence on this item? */
	qboolean launched;
	qboolean rolled;			/**< Can it be rolled - e.g. grenades */
	qboolean reaction;			/**< This firemode can be used/selected for reaction fire.*/
	int throughWall;		/**< allow the shooting through a wall */
	byte dmgweight;
	float speed;
	vec2_t shotOrg;
	vec2_t spread;
	int delay;
	int bounce;				/**< Is this item bouncing? e.g. grenades */
	float bounceFac;
	float crouch;
	float range;			/**< range of the weapon ammunition */
	int shots;
	int ammo;
	/** the delay that the weapon needs to play sounds and particles
	 * The higher the value, the less the delay (1000/delay) */
	float delayBetweenShots;
	int time;
	vec2_t damage, spldmg;
	float splrad;			/**< splash damage radius */
	int weaponSkill;		/**< What weapon skill is needed to fire this weapon. */
	int irgoggles;			/**< Is this an irgoogle? */
} fireDef_t;

/**
 * @brief The max width and height of an item-shape
 * @note these values depend on the the usage of an uint32_t that has 32 bits and a width of 8bit => 4 rows
 * @sa SHAPE_BIG_MAX_HEIGHT
 * @sa SHAPE_BIG_MAX_WIDTH
 * @note This is also used for bit shifting, so please don't change this until
 * you REALLY know what you are doing
 */
#define SHAPE_SMALL_MAX_WIDTH 8
#define SHAPE_SMALL_MAX_HEIGHT 4

/**
 * @brief defines the max height of an inventory container
 * @note the max width is 32 - because uint32_t has 32 bits and we are
 * using a bitmask for the x values
 * @sa SHAPE_SMALL_MAX_WIDTH
 * @sa SHAPE_SMALL_MAX_HEIGHT
 * @note This is also used for bit shifting, so please don't change this until
 * you REALLY know what you are doing
 */
#define SHAPE_BIG_MAX_HEIGHT 16
/** @brief 32 bit mask */
#define SHAPE_BIG_MAX_WIDTH 32

/**
 * @brief All different types of craft items.
 * @note must begin with weapons and end with ammos
 */
typedef enum {
	/* weapons */
	AC_ITEM_BASE_MISSILE,	/**< base weapon */
	AC_ITEM_BASE_LASER,		/**< base weapon */
	AC_ITEM_WEAPON,			/**< aircraft weapon */

	/* misc */
	AC_ITEM_SHIELD,			/**< aircraft shield */
	AC_ITEM_ELECTRONICS,	/**< aircraft electronics */

	/* ammos */
	AC_ITEM_AMMO,			/**< aircraft ammos */
	AC_ITEM_AMMO_MISSILE,	/**< base ammos */
	AC_ITEM_AMMO_LASER,		/**< base ammos */

	MAX_ACITEMS
} aircraftItemType_t;

/**
 * @brief Aircraft parameters.
 * @note This is a list of all aircraft parameters that depends on aircraft items.
 * those values doesn't change with shield or weapon assigned to aircraft
 * @note AIR_STATS_WRANGE must be the last parameter (see AII_UpdateAircraftStats)
 */
typedef enum {
	AIR_STATS_SPEED,	/**< Aircraft crusing speed. */
	AIR_STATS_MAXSPEED,	/**< Aircraft max speed. */
	AIR_STATS_SHIELD,	/**< Aircraft shield. */
	AIR_STATS_ECM,		/**< Aircraft electronic warfare level. */
	AIR_STATS_DAMAGE,	/**< Aircraft damage points (= hit points of the aircraft). */
	AIR_STATS_ACCURACY,	/**< Aircraft accuracy - base accuracy (without weapon). */
	AIR_STATS_FUELSIZE,	/**< Aircraft fuel capacity. */
	AIR_STATS_WRANGE,	/**< Aircraft weapon range - the maximum distance aircraft can open fire. @note: Should be the last one */

	AIR_STATS_MAX,
	AIR_STATS_OP_RANGE	/**< Operationnal range of the aircraft (after AIR_STATS_MAX because not needed in stats[AIR_STATS_MAX], only in CL_AircraftMenuStatsValues */
} aircraftParams_t;

/**
 * @brief Aircraft items.
 * @note This is a part of objDef, only filled for aircraft items (weapons, shield, electronics).
 * @sa objDef_t
 */
typedef struct craftitem_s {
	aircraftItemType_t type;		/**< The type of the aircraft item. */
	float stats[AIR_STATS_MAX];	/**< All coefficient that can affect aircraft->stats */
	float weaponDamage;			/**< The base damage inflicted by an ammo */
	float weaponSpeed;			/**< The speed of the projectile on geoscape */
	float weaponDelay;			/**< The minimum delay between 2 shots */
	int installationTime;		/**< The time needed to install/remove the item on an aircraft */
	qboolean bullets;			/**< create bullets for the projectiles */
} craftitem_t;

/** @brief Buytype categories in the various equipment screens (buy/sell, equip, etc...)
 ** Do not mess with the order (especially BUY_AIRCRAFT and BUY_MULTI_AMMO is/will be used for max-check in normal equipment screens)
 ** @sa scripts.c:buytypeNames
 ** @note Be sure to also update all usages of the buy_type" console function (defined in cl_market.c and mostly used there and in menu_buy.ufo) when changing this.
 **/
typedef enum {
	BUY_WEAP_PRI,	/**< All 'Primary' weapons and their ammo for soldiers. */
	BUY_WEAP_SEC,	/**< All 'Secondary' weapons and their ammo for soldiers. */
	BUY_MISC,		/**< Misc soldier equipment. */
	BUY_ARMOUR,		/**< Armour for soldiers. */
	BUY_MULTI_AMMO, /**< Ammo (and other stuff) that is used in both Pri/Sec weapons. */
	/* MAX_SOLDIER_EQU_BUYTYPES */
	BUY_AIRCRAFT,	/**< Aircraft and craft-equipment. */
	BUY_DUMMY,		/**< Everything that is not equipment for soldiers except craftitems. */
	BUY_CRAFTITEM,	/**< Craftitem. */
	BUY_HEAVY,	/**< Heavy equipment like tanks (i.e. these are actually employees). */
	MAX_BUYTYPES,

	ENSURE_32BIT = 0xFFFFFFFF
} equipmentBuytypes_t;

#define MAX_SOLDIER_EQU_BUYTYPES BUY_MULTI_AMMO

#define BUY_PRI(type)	( (((type) == BUY_WEAP_PRI) || ((type) == BUY_MULTI_AMMO)) ) /** < Checks if "type" is displayable/usable in the primary category. */
#define BUY_SEC(type)	( (((type) == BUY_WEAP_SEC) || ((type) == BUY_MULTI_AMMO)) ) /** < Checks if "type" is displayable/usable in the secondary category. */
#define BUYTYPE_MATCH(type1,type2) (\
	(  ((((type1) == BUY_WEAP_PRI) || ((type1) == BUY_WEAP_SEC)) && ((type2) == BUY_MULTI_AMMO)) \
	|| ((((type2) == BUY_WEAP_PRI) || ((type2) == BUY_WEAP_SEC)) && ((type1) == BUY_MULTI_AMMO)) \
	|| ((type1) == (type2)) ) \
	) /**< Check if the 2 buytypes (type1 and type2) are compatible) */

/**
 * @brief Defines all attributes of obejcts used in the inventory.
 * @todo Document the various (and mostly not obvious) varables here. The documentation in the .ufo file(s) might be a good starting point.
 * @note See also http://ufoai.ninex.info/wiki/index.php/UFO-Scripts/weapon_%2A.ufo
 */
typedef struct objDef_s {
	/* Common */
	int idx;	/**< Index of the objDef in the global item list (ods). */
	char name[MAX_VAR];		/**< Item name taken from scriptfile. */
	char id[MAX_VAR];		/**< Identifier of the item being item definition in scriptfile. */
	char model[MAX_VAR];		/**< Model name - relative to game dir. */
	char image[MAX_VAR];		/**< Object image file - relative to game dir. */
	char type[MAX_VAR];		/**< melee, rifle, ammo, armour. e.g. used in the ufopedia */
	char extends_item[MAX_VAR];
	uint32_t shape;			/**< The shape in inventory. */

	byte sx, sy;			/**< Size in x and y direction. */
	float scale;			/**< scale value for images? and models @todo: fixme - array of scales. */
	vec3_t center;			/**< origin for models @todo: fixme - array of scales. */
	char animationIndex;	/**< The animation index for the character with the weapon. */
	qboolean weapon;		/**< This item is a weapon or ammo. */
	qboolean holdTwoHanded;		/**< The soldier needs both hands to hold this object. */
	qboolean fireTwoHanded;		/**< The soldier needs both hands to fire using object. */
	qboolean extension;		/**< This is an extension. (may not be headgear, too). */
	qboolean headgear;		/**< This is a headgear. (may not be extension, too). */
	qboolean thrown;		/**< This item can be thrown. */

	int price;			/**< Price for this item. */
	int size;			/**< Size of an item, used in storage capacities. */
	equipmentBuytypes_t buytype;			/**< Category of the item - used in menus (see equipment_buytypes_t). */
	qboolean notOnMarket;		/**< True if this item should not be available on market. */

	/* Weapon specific. */
	int ammo;			/**< How much can we load into this weapon at once. @todo: what is this? isn't it ammo-only specific which defines amount of bullets in clip? */
	int reload;			/**< Time units (TUs) for reloading the weapon. */
	qboolean oneshot;	/**< This weapon contains its own ammo (it is loaded in the base).
						 * "int ammo" of objDef_s defines the amount of ammo used in oneshoot weapons. */
	qboolean deplete;	/**< This weapon useless after all ("oneshot") ammo is used up.
						 * If true this item will not be collected on mission-end. (see INV_CollectinItems). */

	int useable;		/**< Defines which team can use this item: 0 - human, 1 - alien.
						 * Used in checking the right team when filling the containers with available armours. */

	struct objDef_s *ammos[MAX_AMMOS_PER_OBJDEF];		/**< List of ammo-object pointers that can be used in this one. */
	int numAmmos;			/**< Number of ammos this weapon can be used with, which is <= MAX_AMMOS_PER_OBJDEF. */

	/* Firemodes (per weapon). */
	struct objDef_s *weapons[MAX_WEAPONS_PER_OBJDEF];		/**< List of weapon-object pointers where thsi item can be used in.
															 * Correct index for this array can be get from fireDef_t.weapFdsIdx. or
															 * FIRESH_FiredefsIDXForWeapon. */
	fireDef_t fd[MAX_WEAPONS_PER_OBJDEF][MAX_FIREDEFS_PER_WEAPON];	/**< List of firemodes per weapon (the ammo can be used in). */
	int numFiredefs[MAX_WEAPONS_PER_OBJDEF];	/**< Number of firemodes per weapon.
												 * Maximum value for fireDef_t.fdIdx <= MAX_FIREDEFS_PER_WEAPON. */
	int numWeapons;		/**< Number of weapons this ammo can be used in.
						 * Maximum value for fireDef_t.weapFdsIdx <= MAX_WEAPONS_PER_OBJDEF. */

	struct technology_s *tech;	/**< Technology link to item. */
	struct technology_s *extension_tech;	/**< Technology link to item to use this extension for (if this is an extension).
											 * @todo Is this used anywhere? */

	/* Armour specific */
	short protection[MAX_DAMAGETYPES];	/**< Protection values for each armour and every damage type. */
	short ratings[MAX_DAMAGETYPES];		/**< Rating values for each armour and every damage type to display in the menus. */

	/* Aircraft specific */
	byte dmgtype;
	craftitem_t craftitem;
} objDef_t;

/**
 * @brief Return values for Com_CheckToInventory.
 * @sa Com_CheckToInventory
 */
enum {
	INV_DOES_NOT_FIT		= 0,	/**< Item does not fit. */
	INV_FITS 				= 1,	/**< The item fits without rotation (only) */
	INV_FITS_ONLY_ROTATED	= 2,	/**< The item fits only when rotated (90! to theleft) */
	INV_FITS_BOTH			= 3		/**< The item fits either rotated or not. */
};

#define MAX_INVDEFS	16

/** @brief inventory definition for our menus */
typedef struct invDef_s {
	char name[MAX_VAR];	/**< id from script files. */
	int id;				/**< Special container id. See csi_t for the values to compare it with. */
	/** Type of this container or inventory. */
	qboolean single;	/**< Just a single item can be stored in this container. */
	qboolean armour;	/**< Only armour can be stored in this container. */
	qboolean extension;	/**< Only extension items can be stored in this container. */
	qboolean headgear;	/**< Only headgear items can be stored in this container. */
	qboolean all;		/**< Every item type can be stored in this container. */
	qboolean temp;		/**< This is only a pointer to another inventory definitions. */
	uint32_t shape[SHAPE_BIG_MAX_HEIGHT];	/**< The inventory form/shape. */
	int in, out;	/**< TU costs for moving items in and out. */
} invDef_t;

#define MAX_CONTAINERS	MAX_INVDEFS
#define MAX_INVLIST		1024

/**
 * @brief item definition
 * @note m and t are transfered as shorts over the net - a value of NONE means
 * that there is no item - e.g. a value of NONE for m means, that there is no
 * ammo loaded or assigned to this weapon
 */
typedef struct item_s {
	int a;			/**< Number of ammo rounds left - see NONE_AMMO */
	objDef_t *m;	/**< Pointer to ammo type. */
	objDef_t *t;	/**< Pointer to weapon. */
	int amount;		/**< The amount of items of this type on the same x and y location in the container */
	int rotated;	/**< If the item is currently displayed rotated (qtrue or 1) or not (qfalse or 0) */
} item_t;

/** @brief container/inventory list (linked list) with items */
typedef struct invList_s {
	item_t item;	/**< which item */
	int x, y;		/**< position of the item */
	struct invList_s *next;		/**< next entry in this list */
} invList_t;

/** @brief inventory defintion with all its containers */
typedef struct inventory_s {
	invList_t *c[MAX_CONTAINERS];
} inventory_t;

#define MAX_EQUIPDEFS   64

typedef struct equipDef_s {
	char name[MAX_VAR];
	int num[MAX_OBJDEFS];
	byte numLoose[MAX_OBJDEFS];
} equipDef_t;

#define MAX_TEAMS_PER_MISSION 4
#define MAX_TERRAINS 8
#define MAX_CULTURES 8
#define MAX_POPULATIONS 8

typedef struct mapDef_s {
	/* general */
	char *id;				/**< script file id */
	char *map;				/**< bsp or ump base filename (without extension and day or night char) */
	char *param;			/**< in case of ump file, the assembly to use */
	char *description;		/**< the description to show in the menus */
	char *size;				/**< small, medium, big */

	/* multiplayer */
	qboolean multiplayer;	/**< is this map multiplayer ready at all */
	int teams;				/**< multiplayer teams */
	linkedList_t *gameTypes;	/**< gametype strings this map is useable for */

	/* singleplayer */
	int maxAliens;				/**< Number of spawning points on the map */

	/* @todo: Make use of these values */
	linkedList_t *terrains;		/**< terrain strings this map is useable for */
	linkedList_t *populations;	/**< population strings this map is useable for */
	linkedList_t *cultures;		/**< culture strings this map is useable for */
	qboolean storyRelated;		/**< Is this a mission story related? */
	int timesAlreadyUsed;		/**< Number of times the map has already been used */
	linkedList_t *ufos;			/**< Type of allowed UFOs on the map */
} mapDef_t;

typedef struct damageType_s {
	char id[MAX_VAR];
	qboolean showInMenu;
} damageType_t;

/**
 * @brief The csi structure is the client-server-information structure
 * which contains all the UFO info needed by the server and the client.
 * @sa ccs_t
 * @note Most of this comes from the script files
 */
typedef struct csi_s {
	/** Object definitions */
	objDef_t ods[MAX_OBJDEFS];
	int numODs;

	/** Inventory definitions */
	invDef_t ids[MAX_INVDEFS];
	int numIDs;

	/** Map definitions */
	mapDef_t mds[MAX_MAPDEFS];
	int numMDs;
	mapDef_t *currentMD;	/**< currently selected mapdef */

	/** Special container ids */
	int idRight, idLeft, idExtension;
	int idHeadgear, idBackpack, idBelt, idHolster;
	int idArmour, idFloor, idEquip;

	/** Damage type ids */
	int damNormal, damBlast, damFire;
	int damShock;	/**< Flashbang-type 'damage' (i.e. Blinding). */

	/** Damage type ids */
	int damLaser, damPlasma, damParticle;
	int damStunGas;		/**< Stun gas attack (only effective against organic targets).
						 * @todo Maybe even make a differentiation between aliens/humans here? */
	int damStunElectro;	/**< Electro-Shock attack (effective against organic and robotic targets). */

	/** Equipment definitions */
	equipDef_t eds[MAX_EQUIPDEFS];
	int numEDs;

	/** Damage types */
	damageType_t dts[MAX_DAMAGETYPES];
	int numDTs;

	/** team definitions */
	teamDef_t teamDef[MAX_TEAMDEFS];
	int numTeamDefs;

	/** the current assigned teams for this mission */
	teamDef_t* alienTeams[MAX_TEAMS_PER_MISSION];
	int numAlienTeams;
} csi_t;


/** @todo Medals. Still subject to (major) changes. */

#define MAX_SKILL	100

#define GET_HP_HEALING( ab ) (1 + (ab) * 15/MAX_SKILL)
#define GET_HP( ab )        (min((80 + (ab) * 90/MAX_SKILL), 255))
#define GET_INJURY_MULT( mind, hp, hpmax )  ((float)(hp) / (float)(hpmax) > 0.5f ? 1.0f : 1.0f + INJURY_BALANCE * ((1.0f / ((float)(hp) / (float)(hpmax) + INJURY_THRESHOLD)) -1.0f)* (float)MAX_SKILL / (float)(mind))
#define GET_ACC( ab, sk )   ((1 - ((float)(ab)/MAX_SKILL + (float)(sk)/MAX_SKILL) / 2)) /**@todo Skill-influence needs some balancing. */
#define GET_TU( ab )        (min((27 + (ab) * 20/MAX_SKILL), 255))
#define GET_MORALE( ab )        (min((100 + (ab) * 150/MAX_SKILL), 255))

/**
 * @todo Generally rename "KILLED_ALIENS" to "KILLED_ENEMIES" and adapt all checks to check for (attacker->team == target->team)?
 * For this see also g_combat.c:G_UpdateCharacterScore
*/
typedef enum {
	KILLED_ALIENS,		/**< Killed aliens @todo maybe generally "enemies" in the future? */
	KILLED_CIVILIANS,	/**< Civilians, animals @todo maybe also scientists and workers int he future? */
	KILLED_TEAM,	/**< Friendly fire, own team, partner-teams. */

	KILLED_NUM_TYPES
} killtypes_t;

typedef enum { /** @note Changing order/entries also changes network-transmission and savegames! */
	ABILITY_POWER,
	ABILITY_SPEED,
	ABILITY_ACCURACY,
	ABILITY_MIND,

	SKILL_CLOSE,
	SKILL_HEAVY,
	SKILL_ASSAULT,
	SKILL_SNIPER,
	SKILL_EXPLOSIVE,
	SKILL_NUM_TYPES
} abilityskills_t;

#define ABILITY_NUM_TYPES SKILL_CLOSE


#define MAX_UGV	8
/** @brief Defines a type of UGV/Robot */
typedef struct ugv_s {
	char *id;
	char weapon[MAX_VAR];
	char armour[MAX_VAR];
	int tu;
	char actors[MAX_VAR];
	int price;
} ugv_t;

#define MAX_RANKS	32

/** @brief Describes a rank that a recruit can gain */
typedef struct rank_s {
	char *id;		/**< Unique identifier as parsed from the ufo files. */
	char name[MAX_VAR];	/**< Rank name (Captain, Squad Leader) */
	char shortname[8];	/**< Rank shortname (Cpt, Sqd Ldr) */
	char *image;		/**< Image to show in menu */
	int type;			/**< employeeType_t */
	int mind;			/**< character mind attribute needed */
	int killed_enemies;		/**< needed amount of enemies killed */
	int killed_others;		/**< needed amount of other actors killed */
	float factor;		/**< a factor that is used to e.g. increase the win
						 * probability for auto missions */
} rank_t;

extern rank_t ranks[MAX_RANKS];	/**< Global list of all ranks defined in medals.ufo. */
extern int numRanks;			/**< The number of entries in the list above. */

/**
 * @brief Structure of all stats collected in a mission.
 * @todo Will replace chrScoreOLD_s at some point.
 * @note More general Info: http://ufoai.ninex.info/wiki/index.php/Proposals/Attribute_Increase
 * @note Mostly collected in g_client.c and not used anywhere else (at least that's the plan ;)).
 * The result is parsed into chrScoreGlobal_t which is stored in savegames.
 * @note
BTAxis about "hit" count:
"But yeah, what we want is a counter per skill. This counter should start at 0 every battle, and then be intreased by 1 everytime
- a direct fire weapon hits (or deals damage, same thing) the actor the weapon was fired at. If it wasn't fired at an actor, nothing should happen.
- a splash weapon deals damage to any enemy actor. If multiple actors are hit, increase the counter multiple times."
 */
typedef struct chrScoreMission_s {

	/* Movement counts. */
	int movedNormal;
	int movedCrouched;
#if 0
	int movedPowered;

	int weight; /**< Weight of equipment (or only armour?) */
#endif
	/** Kills & stuns @todo use existing code */
	int kills[KILLED_NUM_TYPES];	/**< Count of kills (aliens, civilians, teammates) */
	int stuns[KILLED_NUM_TYPES];	/**< Count of stuns(aliens, civilians, teammates) */

	/**
	 * Hits/Misses @sa g_combat.c:G_UpdateHitScore. */
	int fired[SKILL_NUM_TYPES];				/**< Count of fired "firemodes" (i.e. the count of how many times the soldeir started shooting) */
	int firedTUs[SKILL_NUM_TYPES];				/**< Count of TUs used for the fired "firemodes". (direct hits only)*/
	qboolean firedHit[KILLED_NUM_TYPES];	/** Temporarily used for shot-stats calculations and status-tracking. Not used in stats.*/
	int hits[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of hits (aliens, civilians or, teammates) per skill.
													 * It is a sub-count of "fired".
													 * It's planned to be increased by 1 for each series fo shots that dealt _some_ damage. */
	int firedSplash[SKILL_NUM_TYPES];	/**< Count of fired splash "firemodes". */
	int firedSplashTUs[SKILL_NUM_TYPES];				/**< Count of TUs used for the fired "firemodes" (splash damage only). */
	qboolean firedSplashHit[KILLED_NUM_TYPES];	/** Same as firedHit but for Splash damage. */
	int hitsSplash[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of splash hits. */
	int hitsSplashDamage[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of dealt splash damage (aliens, civilians or, teammates).
														 		 * This is counted in overall damage (healthpoint).*/
	/** @todo Check HEALING of others. */

	int skillKills[SKILL_NUM_TYPES];	/**< Number of kills related to each skill. */

	int heal;	/**< How many hitpoints has this soldier received trough healing in battlescape. */
#if 0
	int reactionFire;	/**< Count number of usage of RF (or TUs?) */
#endif
} chrScoreMission_t;

/**
 * @brief Structure of all stats collected for an actor over time.
 * @todo Will replace chrScoreOLD_s at some point.
 * @note More general Info: http://ufoai.ninex.info/wiki/index.php/Proposals/Attribute_Increase
 * @note This information is stored in savegames (in contract to chrScoreMission_t).
 * @note WARNING: if you change something here you'll have to make sure all the network and savegame stuff is updated as well!
 * Additionally you have to check the size of the network-transfer in g_main.c:G_SendCharacterData and cl_team.c:CL_ParseCharacterData (updateCharacter_t)
 */
typedef struct chrScoreGlobal_s {
	int experience[SKILL_NUM_TYPES + 1]; /**< Array of experience values for all skills, and health. @todo What are the mins and maxs for these values */

	int skills[SKILL_NUM_TYPES];		/**< Array of skills and abilities. This is the total value. */
	int initialSkills[SKILL_NUM_TYPES + 1];		/**< Array of initial skills and abilities. This is the value generated at character generation time. */

	/* Kills & Stuns */
	int kills[KILLED_NUM_TYPES];	/**< Count of kills (aliens, civilians, teammates) */
	int stuns[KILLED_NUM_TYPES];	/**< Count of stuns(aliens, civilians, teammates) */

	int assignedMissions;		/**< Number of missions this soldeir was assigned to. */

	int rank;					/**< Index of rank (in gd.ranks). */
} chrScoreGlobal_t;

typedef struct chrFiremodeSettings_s {
	int hand;	/**< Stores the used hand (0=right, 1=left, -1=undef) */
	int fmIdx;	/**< Stores the used firemode index. Max. number is MAX_FIREDEFS_PER_WEAPON -1=undef*/
	int wpIdx;	/**< Stores the weapon idx in ods. (for faster access and checks) -1=undef */
} chrFiremodeSettings_t;

/**
 * @brief How many TUs (and of what type) did a player reserve for a unit?
 * @sa CL_UsableTUs
 * @sa CL_ReservedTUs
 * @sa CL_ReserveTUs
 * @todo Would a list be better here? See the enum reservation_types_t
 */
typedef struct chrReservations_s {
	/* Reaction fire reservation (for current round and next enemy round) */
	int reserveReaction; /**< Stores if the player has activated or disabled reservation for RF. states can be 0, STATE_REACTION_ONCE or STATE_REACTION_MANY See also le_t->state. This is only for remembering over missions. @sa: g_client:G_ClientSpawn*/
	int reaction;	/**< Did the player activate RF with a usable firemode? (And at the same time storing the TU-costs of this firemode) */

	/* Crouch reservation (for current round)	*/
	qboolean reserveCrouch; /**< Stores if the player has activated or disabled reservation for crouching/standing up. @sa cl_parse:CL_StartingGameDone */
	int crouch;	/**< Did the player reserve TUs for crouching (or standing up)? Depends exclusively on TU_CROUCH.
			 * @sa cl_actor:CL_ActorStandCrouch_f
			 * @sa cl_parse:CL_ActorStateChange
			 */

	/** Shot reservation (for current round)
	 * @sa cl_actor.c:CL_PopupFiremodeReservation_f (sel_shotreservation) */
	int shot;	/**< If non-zero we reserved a shot in this turn. */
	chrFiremodeSettings_t shotSettings;	/**< Stores what type of firemode & weapon (and hand) was used for "shot" reservation. */

/*
	int reserveCustom;	**< Did the player activate reservation for the custom value?
	int custom;	**< How many TUs the player has reserved by manual input. @todo: My suggestion is to provide a numerical input-field.
*/
} chrReservations_t;

typedef enum {
	RES_REACTION,
	RES_CROUCH,
	RES_SHOT,
	RES_ALL,
	RES_ALL_ACTIVE,
	RES_TYPES /**< Max. */
} reservation_types_t;

/**
 * @brief The types of employees.
 * @note If you will change order, make sure personel transfering still works.
 */
typedef enum {
	EMPL_SOLDIER,
	EMPL_SCIENTIST,
	EMPL_WORKER,
	EMPL_MEDIC,
	EMPL_ROBOT,
	MAX_EMPL		/**< For counting over all available enums. */
} employeeType_t;

/** @brief Describes a character with all its attributes */
typedef struct character_s {
	int ucn;
	char name[MAX_VAR];			/**< Character name (as in: soldier name). */
	char path[MAX_VAR];
	char body[MAX_VAR];
	char head[MAX_VAR];
	int skin;				/**< Index of skin. */

	int HP;						/**< Health points (current ones). */
	int minHP;					/**< Minimum hp during combat */
	int maxHP;					/**< Maximum health points (as in: 100% == fully healed). */
	int STUN;
	int morale;

	chrScoreGlobal_t score;		/**< Array of scores/stats the soldier/unit collected over time. */
	chrScoreMission_t *scoreMission;		/**< Array of scores/stats the soldier/unit collected in a mission - only used in battlescape (server side). Otherwise it's NULL. */

	/** @sa memcpy in Grid_CheckForbidden */
	int fieldSize;				/**< @sa ACTOR_SIZE_**** */

	inventory_t *inv;			/**< Inventory definition. */

	/** @note These unfortunately need to be indices 'cause in the battlescape there is no employee-info anywhere (AFAIK). */
	int emplIdx;				/**< Backlink to employee-struct - global employee index (gd.employees[][emplIdx]). */
	employeeType_t emplType;				/**< Employee type.  (gd.employees[emplType][]). */

	qboolean armour;			/**< Able to use armour. */
	qboolean weapons;			/**< Able to use weapons. */

	teamDef_t *teamDef;			/**< Pointer to team definition. */
	int gender;				/**< Gender index. */
	chrReservations_t reservedTus;	/** < Stores the reserved TUs for actions. @sa See chrReserveSettings_t for more. */
	chrFiremodeSettings_t RFmode;	/** < Stores the firemode to be used for reaction fire (if the fireDef allows that) See also reaction_firemode_type_t */
} character_t;

#define THIS_FIREMODE(fm, HAND, fdIdx)	((fm)->hand == HAND && (fm)->fmIdx == fdIdx)
#define SANE_FIREMODE(fm)	((((fm)->hand >= 0) && ((fm)->fmIdx >=0 && (fm)->fmIdx < MAX_FIREDEFS_PER_WEAPON) && ((fm)->fmIdx >= 0)))

#define MAX_CAMPAIGNS	16

/* ================================ */
/*  CHARACTER GENERATING FUNCTIONS  */
/* ================================ */

int Com_StringToTeamNum(const char* teamString) __attribute__((nonnull));
int CHRSH_CharGetMaxExperiencePerMission(abilityskills_t skill);
void CHRSH_CharGenAbilitySkills(character_t * chr, int team, employeeType_t type, qboolean multiplayer) __attribute__((nonnull));
char *CHRSH_CharGetBody(character_t* const chr) __attribute__((nonnull));
char *CHRSH_CharGetHead(character_t* const chr) __attribute__((nonnull));

/* ================================ */
/*  INVENTORY MANAGEMENT FUNCTIONS  */
/* ================================ */

void INVSH_InitCSI(csi_t * import) __attribute__((nonnull));
void INVSH_InitInventory(invList_t * invChain) __attribute__((nonnull));
int Com_CheckToInventory(const inventory_t* const i, objDef_t *ob, const invDef_t * container, int x, int y);
invList_t *Com_SearchInInventory(const inventory_t* const i, const invDef_t * container, int x, int y) __attribute__((nonnull(1)));
invList_t *Com_AddToInventory(inventory_t* const i, item_t item, const invDef_t * container, int x, int y, int amount) __attribute__((nonnull(1)));
qboolean Com_RemoveFromInventory(inventory_t* const i, const invDef_t * container, int x, int y) __attribute__((nonnull(1)));
qboolean Com_RemoveFromInventoryIgnore(inventory_t* const i, const invDef_t * container, int x, int y, qboolean ignore_type) __attribute__((nonnull(1)));
int Com_MoveInInventory(inventory_t* const i, const invDef_t * from, int fx, int fy, const invDef_t * to, int tx, int ty, int *TU, invList_t ** icp) __attribute__((nonnull(1)));
int Com_MoveInInventoryIgnore(inventory_t* const i, const invDef_t * from, int fx, int fy, const invDef_t * to, int tx, int ty, int *TU, invList_t ** icp, qboolean ignore_type) __attribute__((nonnull(1)));
void INVSH_EmptyContainer(inventory_t* const i, const invDef_t * container) __attribute__((nonnull(1)));
void INVSH_DestroyInventory(inventory_t* const i) __attribute__((nonnull(1)));
void Com_FindSpace(const inventory_t* const inv, item_t *item, const invDef_t * container, int * const px, int * const py) __attribute__((nonnull(1)));
int Com_TryAddToInventory(inventory_t* const inv, item_t item, const invDef_t * container) __attribute__((nonnull(1)));
int Com_TryAddToBuyType(inventory_t* const inv, item_t item, int buytypeContainer, int amount) __attribute__((nonnull(1)));
void INVSH_EquipActorMelee(inventory_t* const inv, character_t* chr) __attribute__((nonnull(1)));
void INVSH_EquipActorRobot(inventory_t* const inv, character_t* chr, objDef_t* weapon) __attribute__((nonnull(1)));
void INVSH_EquipActor(inventory_t* const inv, const int *equip, int anzEquip, const char *name, character_t* chr) __attribute__((nonnull(1)));
void INVSH_PrintContainerToConsole(inventory_t* const i);

void INVSH_PrintItemDescription(const objDef_t *od);
objDef_t *INVSH_GetItemByID(const char *id);
qboolean INVSH_LoadableInWeapon(const objDef_t *od, const objDef_t *weapon);

/* =============================== */
/*  FIREMODE MANAGEMENT FUNCTIONS  */
/* =============================== */

const fireDef_t* FIRESH_GetFiredef(const objDef_t *obj, const int weapFdsIdx, const int fdIdx);
int FIRESH_FiredefsIDXForWeapon(const objDef_t *od, const objDef_t *weapon);
int FIRESH_GetDefaultReactionFire(const objDef_t *ammo, int weapFdsIdx);

void Com_MergeShapes(uint32_t *shape, uint32_t itemshape, int x, int y);
qboolean Com_CheckShape(const uint32_t *shape, int x, int y);
int Com_ShapeUsage(uint32_t shape);
uint32_t Com_ShapeRotate(uint32_t shape);
#ifdef DEBUG
void Com_ShapePrint(uint32_t shape);
#endif

/** @brief Number of bytes that is read and written via inventory transfer functions */
#define INV_INVENTORY_BYTES 9

#endif /* GAME_INV_SHARED_H */
