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

#ifndef UNIX_INPUT_H
#define UNIX_INPUT_H

#include <dlfcn.h> /* ELF dl loader */

typedef void (*Key_Event_fp_t)(int key, qboolean down);

extern void (*KBD_Update_fp)(void);
extern void (*KBD_Init_fp)(Key_Event_fp_t fp);
extern void (*KBD_Close_fp)(void);

#define MOUSE_MAX 3000
#define MOUSE_MIN 40

typedef struct in_state {
	/* Pointers to functions back in client, set by vid_so */
	void (*IN_CenterView_fp)(void);
	Key_Event_fp_t Key_Event_fp;
	vec_t *viewangles;
} in_state_t;

void InitSig(void);

void RW_IN_Init(in_state_t *in_state_p);
void RW_IN_Shutdown(void);
void RW_IN_GetMousePos(int *x, int *y);
void RW_IN_Activate(qboolean active);
void RW_IN_Frame(void);

void Real_IN_Init(void *lib);

void KBD_Update(void);
void KBD_Init(Key_Event_fp_t fp);
void KBD_Close(void);

#endif /* UNIX_INPUT_H */
