/**
 * @file cp_airfight.h
 * @brief Header file for airfights
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

#ifndef CLIENT_CL_AIRFIGHT_H
#define CLIENT_CL_AIRFIGHT_H

#define BULLETS_PER_SHOT	1
#define MAX_MULTIPLE_PROJECTILES	10

/* Remove attack of base by ufo for 2.2: doesn't match storyline. */
/* #define UFO_ATTACK_BASES */

/**
 * @note 0 if the weapon can shoot
 * @note -1 if it can't shoot atm
 * @note -2 if it will never be able to shoot
 */
#define AIRFIGHT_WEAPON_CAN_SHOOT 0
#define AIRFIGHT_WEAPON_CAN_NOT_SHOOT_AT_THE_MOMENT -1
#define AIRFIGHT_WEAPON_CAN_NEVER_SHOOT -2

#define AIRFIGHT_BASE_CAN_T_FIRE -1

/** @brief projectile used during fight between two or more aircraft */
typedef struct aircraftProjectile_s {
	const objDef_t *aircraftItem;		/**< Corresponding ammo in the array csi.ods[] */
	int idx;				/**< self link of the idx in ccs.projectiles[] */
	vec3_t pos[MAX_MULTIPLE_PROJECTILES];	/**< array of positions of the projectile (latitude and longitude) */
	vec3_t projectedPos[MAX_MULTIPLE_PROJECTILES];	/**< Array if projected positions of the projectile (latitude and longitude). */
	vec3_t oldDrawPos[MAX_MULTIPLE_PROJECTILES];	/**< Array if old draw positions of the projectile (latitude and longitude). */
	int numProjectiles;      /**< Number of projectile positions used for this projectile. */
	qboolean hasMoved;       /**< Has the projectile been moved by the CampaignRunProjectiles function */
	int numInterpolationPoints;	/**< Number of points drawn so far during interpolation. */
	vec3_t idleTarget;		/**< target of the projectile
							 ** used only if the projectile will miss its target (that is if aimedAircraft is NULL) */
	aircraft_t *attackingAircraft;	/**< Aircraft which shooted the projectile. NULL if aircraft is destroyed or base is shooting */

	struct base_s* aimedBase;		/**< aimed base - NULL if the target is an aircraft */
	struct installation_s* aimedInstallation;	/**< aimed installation - NULL if the target is an aircraft */
	aircraft_t *aimedAircraft;	/**< target of the projectile/
								 ** used only if the projectile will touch its target (otherwise it's NULL)
									and if aimedBase != NULL */
	int time;				/**< time since the projectile has been launched */
	float angle;			/**< angle of the missile on the geoscape */
	qboolean bullets;		/**< projectile has active bullets on geoscape */
	qboolean laser;			/**< projectile has an active laser on geoscape */
} aircraftProjectile_t;

void AIRFIGHT_ExecuteActions(aircraft_t* air, aircraft_t* ufo);
void AIRFIGHT_ActionsAfterAirfight(aircraft_t* shooter, aircraft_t* aircraft, qboolean phalanxWon);
void AIRFIGHT_CampaignRunProjectiles(int dt);
void AIRFIGHT_CampaignRunBaseDefense(int dt);
int AIRFIGHT_CheckWeapon(const aircraftSlot_t *slot, float distance);
int AIRFIGHT_ChooseWeapon(const aircraftSlot_t const *slot, int maxSlot, const vec3_t pos, const vec3_t targetPos);
qboolean AIRFIGHT_BaseCanTargetUFO(const struct base_s *base, const aircraft_t *ufo);
void AIRFIGHT_InitStartup(void);

#endif /* CLIENT_CL_AIRFIGHT_H */
