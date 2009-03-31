/**
 * @file cp_time.h
 * @brief Campaign geoscape time header
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

#ifndef CP_TIME_H
#define CP_TIME_H

void CL_UpdateTime(void);
void CL_GameTimeStop(void);
void CL_GameTimeFast(void);
void CL_GameTimeSlow(void);
void CL_SetGameTime(int gameLapseValue);
void CL_SetGameTime_f(void);

qboolean Date_LaterThan(date_t now, date_t compare);
date_t Date_Add(date_t a, date_t b);
date_t Date_Random(date_t minFrame, date_t maxFrame);
const char *Date_GetMonthName(int month);

#endif

