/**
 * @file levels.c
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

#include "bsp.h"


const vec3_t v_epsilon = { 1, 1, 1 };
int brush_start, brush_end;

vec3_t worldMins, worldMaxs;

static int oldleafs, oldleafbrushes, oldplanes, oldvertexes, oldnodes, oldtexinfo, oldfaces, oldedges, oldsurfedges;

void PushInfo (void)
{
	oldleafs = numleafs;
	oldleafbrushes = numleafbrushes;
	oldplanes = numplanes;
	oldvertexes = numvertexes;
	oldnodes = numnodes;
	oldtexinfo = numtexinfo;
	oldfaces = numfaces;
	oldedges = numedges;
	oldsurfedges = numsurfedges;
}

void PopInfo (void)
{
	numleafs = oldleafs;
	numleafbrushes = oldleafbrushes;
	numplanes = oldplanes;
	numvertexes = oldvertexes;
	numnodes = oldnodes;
	numtexinfo = oldtexinfo;
	numfaces = oldfaces;
	numedges = oldedges;
	numsurfedges = oldsurfedges;
}


static int BuildNodeChildren (vec3_t mins, vec3_t maxs, int n[3])
{
	int node = -1, i;

	for (i = 0; i < 3; i++) {
		dBspNode_t	 *newnode;
		vec3_t newmins, newmaxs, addvec;

		if (n[i] == -1)
			continue;

		if (node == -1) {
			/* store the valid node */
			node = n[i];

			ClearBounds(newmins, newmaxs);
			VectorCopy(dnodes[node].mins, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(dnodes[node].maxs, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
		} else {
			/* add a new "special" dnode and store it */
			newnode = &dnodes[numnodes];
			numnodes++;

			newnode->planenum = PLANENUM_LEAF;
			newnode->children[0] = node;
			newnode->children[1] = n[i];
			newnode->firstface = 0;
			newnode->numfaces = 0;

			ClearBounds(newmins, newmaxs);
			VectorCopy(dnodes[node].mins, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(dnodes[node].maxs, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(dnodes[n[i]].mins, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(dnodes[n[i]].maxs, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(newmins, newnode->mins);
			VectorCopy(newmaxs, newnode->maxs);

			node = numnodes - 1;
		}

		AddPointToBounds(newmins, worldMins, worldMaxs);
		AddPointToBounds(newmaxs, worldMins, worldMaxs);
	}

	/* return the last stored node */
	return node;
}

#define SPLIT_BRUSH_SIZE 256

/**
 * @sa ProcessLevel
 */
static int ConstructLevelNodes_r (int levelnum, vec3_t cmins, vec3_t cmaxs)
{
	bspbrush_t *list;
	tree_t *tree;
	vec3_t diff, bmins, bmaxs;
	int nn[3];
	node_t *n;

	/* calculate bounds, stop if no brushes are available */
	if (!MapBrushesBounds(brush_start, brush_end, levelnum, cmins, cmaxs, bmins, bmaxs))
		return -1;

/*	VectorCopy(cmins, bmins); */
/*	VectorCopy(cmaxs, bmaxs); */

	VectorSubtract(bmaxs, bmins, diff);

/*	Com_Printf("(%i): %i %i: (%i %i) (%i %i) -> (%i %i) (%i %i)\n", levelnum, (int)diff[0], (int)diff[1],
		(int)cmins[0], (int)cmins[1], (int)cmaxs[0], (int)cmaxs[1],
		(int)bmins[0], (int)bmins[1], (int)bmaxs[0], (int)bmaxs[1]); */

	if (diff[0] > SPLIT_BRUSH_SIZE || diff[1] > SPLIT_BRUSH_SIZE) {
		/* continue subdivision */
		/* split the remaining hull at the middle of the longer axis */
		vec3_t nmins, nmaxs;
		int n;

		if (diff[1] > diff[0])
			n = 1;
		else
			n = 0;

		VectorCopy(bmins, nmins);
		VectorCopy(bmaxs, nmaxs);

		nmaxs[n] -= diff[n] / 2;
/*		Com_Printf("  (%i %i) (%i %i)\n", (int)nmins[0], (int)nmins[1], (int)nmaxs[0], (int)nmaxs[1]); */
		nn[0] = ConstructLevelNodes_r(levelnum, nmins, nmaxs);

		nmins[n] += diff[n] / 2;
		nmaxs[n] += diff[n] / 2;
/*		Com_Printf("    (%i %i) (%i %i)\n", (int)nmins[0], (int)nmins[1], (int)nmaxs[0], (int)nmaxs[1]); */
		nn[1] = ConstructLevelNodes_r(levelnum, nmins, nmaxs);
	} else {
		/* no children */
		nn[0] = -1;
		nn[1] = -1;
	}

	BeginModel(entity_num);

	/* add v_epsilon to avoid clipping errors */
	VectorSubtract(bmins, v_epsilon, bmins);
	VectorAdd(bmaxs, v_epsilon, bmaxs);

	list = MakeBspBrushList(brush_start, brush_end, levelnum, bmins, bmaxs);
	if (!list) {
		nn[2] = -1;
		return BuildNodeChildren(bmins, bmaxs, nn);
	}

	if (!config.nocsg)
		list = ChopBrushes(list);

	/* begin model creation now */
	tree = BrushBSP(list, bmins, bmaxs);
	MakeTreePortals(tree);
	MarkVisibleSides(tree, brush_start, brush_end);
	MakeFaces(tree->headnode);
	FixTjuncs(tree->headnode);

	if (!config.noprune)
		PruneNodes(tree->headnode);

	/* correct bounds */
	n = tree->headnode;
	VectorAdd(bmins, v_epsilon, n->mins);
	VectorSubtract(bmaxs, v_epsilon, n->maxs);

	/* finish model */
	WriteBSP(tree->headnode);
	FreeTree(tree);

/*	EndModel(); */

/*	Com_Printf("  headnode: %i\n", dmodels[nummodels].headnode); */

	nn[2] = dmodels[nummodels].headnode;
	return BuildNodeChildren(bmins, bmaxs, nn);
}


/**
 * @brief process brushes with that level mask
 * @param[in] levelnum is the level mask
 * @note levelnum
 * 256: weaponclip-level
 * 257: actorclip-level
 * 258: stepon-level
 * 259: tracing structure
 * @sa ProcessWorldModel
 * @sa ConstructLevelNodes_r
 */
void ProcessLevel (unsigned int levelnum)
{
	vec3_t mins, maxs;
	dBspModel_t *dm;

	/* oversizing the blocks guarantees that all the boundaries will also get nodes. */
	/* get maxs */
	mins[0] = (config.block_xl) * 512.0 + 1.0;
	mins[1] = (config.block_yl) * 512.0 + 1.0;
	mins[2] = -4096.0 + 1.0;

	maxs[0] = (config.block_xh + 1.0) * 512.0 - 1.0;
	maxs[1] = (config.block_yh + 1.0) * 512.0 - 1.0f;
	maxs[2] = 4096.0 - 1.0;

	Sys_FPrintf(SYS_VRB, "Process levelnum %i (nummodels: %i)\n", levelnum, nummodels);

	dm = &dmodels[levelnum];
	memset(dm, 0, sizeof(*dm));

	/* back copy backup brush sides structure */
	/* to reset all the changed values (especialy "finished") */
	memcpy(mapbrushes, mapbrushes + nummapbrushes, sizeof(mapbrush_t) * nummapbrushes);

	/* store face number for later use */
	dm->firstface = numfaces;
	dm->headnode = ConstructLevelNodes_r(levelnum, mins, maxs);
	dm->numfaces = numfaces - dm->firstface;

/*	if (!dm->numfaces)
		Com_Printf("level: %i -> %i : f %i\n", levelnum, dm->headnode, dm->numfaces);
*/
}
