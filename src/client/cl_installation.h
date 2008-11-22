/**
 * @file cl_installationmanagement.h
 * @brief Header for installation management related stuff.
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

#ifndef CLIENT_CL_INSTALLATION_H
#define CLIENT_CL_INSTALLATION_H

#include "cl_basemanagement.h"

#define MAX_INSTALLATIONS	16
#define MAX_INSTALLATION_TEMPLATES	6

#define MAX_INSTALLATION_DAMAGE	100
#define MAX_INSTALLATION_BATTERIES	5
#define MAX_INSTALLATION_SLOT	4

/**
 * @brief Possible installation states
 * @note: Don't change the order or you have to change the installationmenu scriptfiles, too
 */
typedef enum {
	INSTALLATION_NOT_USED,
	INSTALLATION_UNDER_CONSTRUCTION,
	INSTALLATION_WORKING		/**< nothing special */
} installationStatus_t;

typedef struct installationTemplate_s {
	char *id;
	char *name;

	int cost;
	float radarRange; /* The range of the installation's radar.  Units is the angle of the two points from center of earth. */
	int maxBatteries; /* The maximum number of battery slots that can be used in an installation. */
	int maxUfoStored; /* The maximum number of ufos that can be stored in an installation. */
	int maxDamage; /* The maximum amount of damage an installation can sustain before it is destroyed. */
	int buildTime;
} installationTemplate_t;

typedef struct installationUfos_s {
	aircraft_t *aircraftTemplate;
	int amount;
} installationUfos_t;

typedef struct installationWeapon_s {
	/* int idx; */
	aircraftSlot_t slot;	/**< Weapon. */
	aircraft_t *target;		/**< Aimed target for the weapon. */
} installationWeapon_t;

typedef enum {
	INSTALLATION_RADAR,
	INSTALLATION_DEFENCE,
	INSTALLATION_UFOYARD,

	INSTALLATION_TYPE_MAX
} installationType_t;

/** @brief A installation with all it's data */
typedef struct installation_s {
	int idx;					/**< Self link. Index in the global installation-list. */
	char name[MAX_VAR];			/**< Name of the installation */

	installationTemplate_t *installationTemplate; /** The template used for the installation. **/

	qboolean founded;	/**< already founded? */
	vec3_t pos;		/**< pos on geoscape */

	installationStatus_t installationStatus; /**< the current installation status */

	float alienInterest;	/**< How much aliens know this installation (and may attack it) */

	radar_t	radar;

	baseWeapon_t batteries[MAX_INSTALLATION_BATTERIES];	/**< Missile/Laser batteries assigned to this installation. For Sam Sites only. */
	int numBatteries;		/**< how many batteries are installed? */

	equipDef_t storage;	/**< weapons, etc. stored in base */

	/** All ufo aircraft in this installation. This is used for UFO Yards. **/
	installationUfos_t installationUfos[MAX_AIRCRAFT];
	int numUfosInInstallation;	/**< How many ufos are in this installation. */

	capacities_t aircraftCapacitiy;		/**< Capacity of UFO Yard. */

	int installationDamage;			/**< Hit points of installation */
	int buildStart;
} installation_t;

/** Currently displayed/accessed base. */
extern installation_t *installationCurrent;

installation_t* INS_GetInstallationByIDX(int instIdx);
void INS_SetUpInstallation(installation_t* installation, installationTemplate_t *installationTemplate);
void INS_ResetInstallation(void);

installationType_t INS_GetType(const installation_t *installation);

/** Coordinates to place the new installation at (long, lat) */
extern vec3_t newInstallationPos;

int INS_GetFoundedInstallationCount(void);
installation_t* INS_GetFoundedInstallationByIDX(int installationIdx);

void INS_NewInstallations(void);
void INS_SelectInstallation(installation_t *installation);

aircraft_t *INS_GetAircraftFromInstallationByIndex(installation_t* installation, int index);

void INS_DestroyInstallation(installation_t *installation);

void INS_InitStartup(void);
void INS_ParseInstallationNames(const char *name, const char **text);
void INS_ParseInstallations(const char *name, const char **text);
void INS_UpdateInstallationData(void);

qboolean INS_Load(sizebuf_t* sb, void* data);
qboolean INS_Save(sizebuf_t* sb, void* data);

#endif /* CLIENT_CL_INSTALLATION_H */
