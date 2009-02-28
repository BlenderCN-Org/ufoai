/**
 * @file trace.c
 * @brief The major lighting operation is a point to point visibility test, performed
 * by recursive subdivision of the line by the BSP tree.
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


#include "shared.h"
#include "cmdlib.h"
#include "../bsp.h"
#include "bspfile.h"
#include <stddef.h>


/**
 * @brief Use the bsp node structure to reconstruct efficient tracing structures
 * that are used for fast visibility and pathfinding checks
 * @note curTile->tnodes is expected to have enough memory malloc'ed for the function to work.
 * @sa BuildTracingNode_r
 */
void MakeTracingNodes (int levels)
{
	size_t size;
	int i;

	/* Release any memory we have for existing tnodes, just in case. */
	if (curTile->tnodes)
		CloseTracingNodes();

	size = (curTile->numnodes + 1) * sizeof(tnode_t);
	/* 32 byte align the structs */
	size = (size + 31) &~ 31;
	/* allocate memory for the tnodes structure */
	curTile->tnodes = Mem_Alloc(size);
	tnode_p = curTile->tnodes;
	curTile->numtheads = 0;

	for (i = 0; i < levels; i++) {
		if (!curTile->models[i].numfaces)
			continue;

		curTile->thead[curTile->numtheads] = tnode_p - curTile->tnodes;
		curTile->theadlevel[curTile->numtheads] = i;
		curTile->numtheads++;
		assert(curTile->numtheads < LEVEL_MAX);
		TR_BuildTracingNode_r(curTile->models[i].headnode, i);
	}
}

/**
 * @brief
 * @sa MakeTnodes
 */
void CloseTracingNodes (void)
{
	if (curTile->tnodes)
		Mem_Free(curTile->tnodes);
	curTile->tnodes = NULL;
}
