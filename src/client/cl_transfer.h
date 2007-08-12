/**
 * @file cl_transfer.h
 * @brief Header file for Transfer stuff.
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

#ifndef CLIENT_CL_TRANSFER_H
#define CLIENT_CL_TRANSFER_H

#define MAX_TRANSFERS	16

/** @brief Transfer informations (they are being stored in gd.alltransfers[MAX_TRANSFERS]. */
typedef struct transfer_s {
	int itemAmount[MAX_OBJDEFS];			/**< Amount of given item [csi.ods[idx]]. */
	int alienAmount[MAX_TEAMDEFS][2];		/**< Alien cargo, [0] alive, [1] dead. */
	int employeesArray[MAX_EMPL][MAX_EMPLOYEES];	/**< Array of indexes of personel transfering. */
	int aircraftsArray[MAX_AIRCRAFT];		/**< Aircrafts being transferred. */
	int destBase;					/**< Index of destination base. */
	int srcBase;					/**< Intex of source base. */
	date_t event;					/**< When the transfer finish process should start. */
	qboolean active;				/**< True if this transfer is under processing. */
	qboolean hasItems;				/**< Transfer of items. */
	qboolean hasEmployees;				/**< Transfer of employees. */
	qboolean hasAliens;				/**< Transfer of Aliens. */
	qboolean hasAircrafts;				/**< Transfer of aircrafts. */
} transfer_t;

/** @brief Array of current cargo onboard. */
typedef struct transferCargo_s {
	int type;			/**< Type of cargo (1 - items, 2 - employees, 3 - alien bodies, 4 - alive aliens). */
	int itemidx;			/**< Index of item in cargo. */
} transferCargo_t;

void TR_TransferAircraftMenu(aircraft_t* aircraft);
void TR_TransferEnd(transfer_t *transfer);
void TR_EmptyTransferCargo(transfer_t *transfer, qboolean success);
void TR_TransferCheck(void);

void TR_Reset(void);

#endif /* CLIENT_CL_TRANSFER_H */

