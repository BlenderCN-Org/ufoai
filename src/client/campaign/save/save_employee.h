/**
 * @file save_employee.h
 * @brief XML tag constants for savegame.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#define SAVE_EMPLOYEE_EMPLOYEES "employees"
#define SAVE_EMPLOYEE_TYPE "type"
#define SAVE_EMPLOYEE_EMPLOYEE "employee"
#define SAVE_EMPLOYEE_IDX "IDX"
#define SAVE_EMPLOYEE_BASEHIRED "baseHired"
#define SAVE_EMPLOYEE_BUILDING "building"
#define SAVE_EMPLOYEE_NATION "nation"
#define SAVE_EMPLOYEE_UGV "UGV"
#define SAVE_EMPLOYEE_CHR "character"

const constListEntry_t saveEmployeeConstants[] = {
	{"saveEmployeeType::soldier", EMPL_SOLDIER},
	{"saveEmployeeType::scientist", EMPL_SCIENTIST},
	{"saveEmployeeType::worker", EMPL_WORKER},
	{"saveEmployeeType::pilot", EMPL_PILOT},
	{"saveEmployeeType::robot", EMPL_ROBOT},
	{NULL, -1}
};

/*
DTD:

<!ELEMENT employees employee*>
<!ATTLIST employees
	type		soldier|
				scientist|
				worker|
				pilot|robot	#REQUIRED
>

<!ELEMENT employee character>
<!ATTLIST employee
	IDX			#NMTOKEN	#REQUIRED
	baseHired	#NMTOKEN	#IMPLIED
	building	#NMTOKEN	#IMPLIED
	nation		#CDATA		#REQUIRED
	UGV			#CDATA		#IMPLIED
>

** for <character> check save_character.h

*/

