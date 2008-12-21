/**
 * @file brushbsp.c
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
#include "../../common/tracing.h"

int c_nodes;
static int c_nonvis;
static int c_active_brushes;


/**
 * @brief Sets the mins/maxs based on the windings
 */
static void BoundBrush (bspbrush_t *brush)
{
	int i, j;
	winding_t *w;

	ClearBounds(brush->mins, brush->maxs);
	for (i = 0; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (!w)
			continue;
		for (j = 0; j < w->numpoints; j++)
			AddPointToBounds(w->p[j], brush->mins, brush->maxs);
	}
}

#define SNAP_EPSILON	0.01

/**
 * @brief welds two vec3_t's into a third, taking into account nearest-to-integer
 * instead of averaging
 */
static void SnapWeldVector (const vec3_t a, const vec3_t b, vec3_t out)
{
	int i;
	vec_t outi;

	/* dummy check */
	if (a == NULL || b == NULL || out == NULL)
		return;

	/* do each element */
	for (i = 0; i < 3; i++) {
		/* round to integer */
		const double ai = rint(a[i]);
		const double bi = rint(a[i]);

		/* prefer exact integer */
		if (ai == a[i])
			out[i] = a[i];
		else if (bi == b[i])
			out[i] = b[i];

		/* use nearest */
		else if (fabs(ai - a[i]) < fabs(bi < b[i]))
			out[i] = a[i];
		else
			out[i] = b[i];

		/* snap */
		outi = rint(out[i]);
		if (fabs(outi - out[i]) <= SNAP_EPSILON)
			out[i] = outi;
	}
}

/**
 * @brief removes degenerate edges from a winding
 * @returns qtrue if the winding is valid
 */
static qboolean FixWinding (winding_t *w)
{
	qboolean valid;
	int i, k;

	/* dummy check */
	if (!w)
		return qfalse;

	valid = qtrue;

	/* check all verts */
	for (i = 0; i < w->numpoints; i++) {
		/* get second point index */
		const int j = (i + 1) % w->numpoints;
		vec3_t vec;
		float dist;

		/* don't remove points if winding is a triangle */
		if (w->numpoints == 3)
			return valid;

		/* degenerate edge? */
		VectorSubtract(w->p[i], w->p[j], vec);
		dist = VectorLength(vec);
		if (dist < ON_EPSILON) {
			valid = qfalse;

			/* create an average point (ydnar 2002-01-26: using nearest-integer weld preference) */
			SnapWeldVector(w->p[i], w->p[j], vec);
			VectorCopy(vec, w->p[i]);

			/* move the remaining verts */
			for (k = i + 2; k < w->numpoints; k++)
				VectorCopy(w->p[k], w->p[k - 1]);

			w->numpoints--;
		}
	}

	/* one last check and return */
	if (w->numpoints < 3)
		valid = qfalse;
	return valid;
}


/**
 * @brief makes basewindigs for sides and mins/maxs for the brush
 * @returns false if the brush doesn't enclose a valid volume
 */
static void CreateBrushWindings (bspbrush_t *brush)
{
	int i;

	for (i = 0; i < brush->numsides; i++) {
		side_t *side = &brush->sides[i];
		const plane_t *plane = &mapplanes[side->planenum];
		int j;

		/* evidence that winding_t represents a hessian normal plane */
		winding_t *w = BaseWindingForPlane(plane->normal, plane->dist);

		for (j = 0; j < brush->numsides && w; j++) {
			if (i == j)
				continue;
			/* back side clipaway */
			if (brush->sides[j].planenum == (brush->sides[i].planenum ^ 1))
				continue;
			if (brush->sides[j].bevel)
				continue;
			plane = &mapplanes[brush->sides[j].planenum ^ 1];
			ChopWindingInPlace(&w, plane->normal, plane->dist, 0); /*CLIP_EPSILON); */

			/* fix broken windings that would generate trifans */
			if (!FixWinding(w))
				Verb_Printf(VERB_EXTRA, "removed degenerated edge(s) from winding\n");
		}

		side->winding = w;
	}

	BoundBrush(brush);
}

/**
 * @brief Creates a new axial brush
 */
static bspbrush_t *BrushFromBounds (vec3_t mins, vec3_t maxs)
{
	bspbrush_t *b;
	int i;
	vec3_t normal;
	vec_t dist;

	b = AllocBrush(6);
	b->numsides = 6;
	for (i = 0; i < 3; i++) {
		VectorClear(normal);
		normal[i] = 1;
		dist = maxs[i];
		b->sides[i].planenum = FindFloatPlane(normal, dist);

		normal[i] = -1;
		dist = -mins[i];
		b->sides[3 + i].planenum = FindFloatPlane(normal, dist);
	}

	CreateBrushWindings(b);

	return b;
}

/**
 * @brief Returns the volume of the given brush
 */
static vec_t BrushVolume (bspbrush_t *brush)
{
	int i;
	winding_t *w;
	vec3_t corner;
	vec_t volume;

	if (!brush)
		return 0;

	/* grab the first valid point as the corner */
	w = NULL;
	for (i = 0; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (w)
			break;
	}
	if (!w)
		return 0;
	VectorCopy(w->p[0], corner);

	/* make tetrahedrons to all other faces */
	volume = 0;
	for (; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (w) {
			const plane_t *plane = &mapplanes[brush->sides[i].planenum];
			const vec_t d = -(DotProduct(corner, plane->normal) - plane->dist);
			const vec_t area = WindingArea(w);
			volume += d * area;
		}
	}

	volume /= 3;
	return volume;
}

/**
 * @brief Returns the amount of brushes in the the given brushlist
 */
int CountBrushList (bspbrush_t *brushes)
{
	int c;

	c = 0;
	for (; brushes; brushes = brushes->next)
		c++;
	return c;
}

/**
 * @sa AllocBrush
 * @sa AllocNode
 */
static tree_t *AllocTree (void)
{
	tree_t *tree;

	tree = malloc(sizeof(*tree));
	memset(tree, 0, sizeof(*tree));
	ClearBounds(tree->mins, tree->maxs);

	return tree;
}

/**
 * @sa AllocBrush
 * @sa AllocTree
 */
static node_t *AllocNode (void)
{
	node_t *node;

	node = malloc(sizeof(*node));
	memset(node, 0, sizeof(*node));

	return node;
}

/**
 * @sa AllocTree
 * @sa AllocNode
 */
bspbrush_t *AllocBrush (int numsides)
{
	bspbrush_t *bb;
	size_t c = offsetof(bspbrush_t, sides[numsides]);

	bb = malloc(c);
	memset(bb, 0, c);
	if (threadstate.numthreads == 1)
		c_active_brushes++;
	return bb;
}

/**
 * @sa AllocBrush
 */
void FreeBrush (bspbrush_t *brushes)
{
	int i;

	for (i = 0; i < brushes->numsides; i++)
		if (brushes->sides[i].winding)
			FreeWinding(brushes->sides[i].winding);
	free(brushes);
	if (threadstate.numthreads == 1)
		c_active_brushes--;
}

/**
 * @sa AllocBrush
 * @sa CountBrushList
 */
void FreeBrushList (bspbrush_t *brushes)
{
	bspbrush_t *next;

	for (; brushes; brushes = next) {
		next = brushes->next;
		FreeBrush(brushes);
	}
}

/**
 * @brief Duplicates the brush, the sides, and the windings
 * @sa AllocBrush
 */
bspbrush_t *CopyBrush (const bspbrush_t *brush)
{
	bspbrush_t *newbrush;
	int i;
	size_t size = offsetof(bspbrush_t, sides[brush->numsides]);

	newbrush = AllocBrush(brush->numsides);
	memcpy(newbrush, brush, size);

	for (i = 0; i < brush->numsides; i++) {
		if (brush->sides[i].winding)
			newbrush->sides[i].winding = CopyWinding(brush->sides[i].winding);
	}

	return newbrush;
}


static int TestBrushToPlanenum (bspbrush_t *brush, int planenum,
			int *numsplits, qboolean *hintsplit, int *epsilonbrush)
{
	int i, j, s;
	plane_t *plane;
	dBspPlane_t plane2;
	winding_t *w;
	vec_t d_front, d_back;
	int front, back;

	*numsplits = 0;
	*hintsplit = qfalse;

	/* if the brush actually uses the planenum,
	 * we can tell the side for sure */
	for (i = 0; i < brush->numsides; i++) {
		const int num = brush->sides[i].planenum;
		if (num >= 0x10000)
			Sys_Error("bad planenum");
		if (num == planenum)
			return (PSIDE_BACK | PSIDE_FACING);
		if (num == (planenum ^ 1))
			return (PSIDE_FRONT | PSIDE_FACING);
	}

	/* box on plane side */
	plane = &mapplanes[planenum];

	/* Convert to cBspPlane */
	VectorCopy(plane->normal, plane2.normal);
	plane2.dist = plane->dist;
	plane2.type = plane->type;
	s = TR_BoxOnPlaneSide(brush->mins, brush->maxs, &plane2);

	if (s != PSIDE_BOTH)
		return s;

	/* if both sides, count the visible faces split */
	d_front = d_back = 0;

	for (i = 0; i < brush->numsides; i++) {
		if (brush->sides[i].texinfo == TEXINFO_NODE)
			continue;		/* on node, don't worry about splits */
		if (!brush->sides[i].visible)
			continue;		/* we don't care about non-visible */
		w = brush->sides[i].winding;
		if (!w)
			continue;
		front = back = 0;
		for (j = 0; j < w->numpoints; j++) {
			const vec_t d = DotProduct(w->p[j], plane->normal) - plane->dist;
			if (d > d_front)
				d_front = d;
			if (d < d_back)
				d_back = d;

			if (d > 0.1) /* PLANESIDE_EPSILON) */
				front = 1;
			else if (d < -0.1) /* PLANESIDE_EPSILON) */
				back = 1;
		}
		if (front && back) {
			(*numsplits)++;
			if (brush->sides[i].surfaceFlags & SURF_HINT)
				*hintsplit = qtrue;
		}
	}

	if ((d_front > 0.0 && d_front < 1.0) || (d_back < 0.0 && d_back > -1.0))
		(*epsilonbrush)++;

	return s;
}


#define EDGE_LENGTH 0.2
/**
 * @brief Returns true if the winding would be crunched out of existance by the
 * vertex snapping.
 */
qboolean WindingIsTiny (winding_t *w)
{
	int i, edges;
	vec_t len;
	vec3_t delta;

	edges = 0;
	for (i = 0; i < w->numpoints; i++) {
		const int j = ((i == w->numpoints - 1) ? 0 : i) + 1;
		VectorSubtract(w->p[j], w->p[i], delta);
		len = VectorLength(delta);
		if (len > EDGE_LENGTH) {
			if (++edges == 3)
				return qfalse;
		}
	}
	return qtrue;
}

/**
 * @brief Returns true if the winding still has one of the points from
 * basewinding for plane
 */
static qboolean WindingIsHuge (const winding_t *w)
{
	int i, j;

	for (i = 0; i < w->numpoints; i++) {
		for (j = 0; j < 3; j++)
			if (w->p[i][j] < -MAX_WORLD_WIDTH || w->p[i][j] > MAX_WORLD_WIDTH)
				return qtrue;
	}
	return qfalse;
}

static void LeafNode (node_t *node, bspbrush_t *brushes)
{
	bspbrush_t *b;
	int i;

	node->planenum = PLANENUM_LEAF;
	node->contentFlags = 0;

	Verb_Printf(VERB_DUMP, "LeafNode: scanning brushes.\n");

	for (b = brushes; b; b = b->next) {
		Verb_Printf(VERB_DUMP, "LeafNode: scanning brush %i\n", b->original->brushnum);
		/* if the brush is solid and all of its sides are on nodes,
		 * it eats everything */
		if (b->original->contentFlags & CONTENTS_SOLID && !(b->original->contentFlags & CONTENTS_PASSABLE)) {
			for (i = 0; i < b->numsides; i++)
				if (b->sides[i].texinfo != TEXINFO_NODE)
					break;
			if (i == b->numsides) {
				node->contentFlags = CONTENTS_SOLID;
				break;
			}
		}
		node->contentFlags |= b->original->contentFlags;
	}

	node->brushlist = brushes;
}


static void CheckPlaneAgainstParents (int pnum, const node_t *node)
{
	node_t *p;

	for (p = node->parent; p; p = p->parent) {
		if (p->planenum == pnum)
			Sys_Error("Tried parent");
	}
}

static qboolean CheckPlaneAgainstVolume (int pnum, node_t *node)
{
	bspbrush_t *front, *back;
	qboolean good;

	SplitBrush(node->volume, pnum, &front, &back);

	good = (front && back);

	if (front)
		FreeBrush(front);
	if (back)
		FreeBrush(back);

	return good;
}

/**
 * @brief Using a heuristic, choses one of the sides out of the brushlist
 * to partition the brushes with.
 * @return NULL if there are no valid planes to split with..
 */
static side_t *SelectSplitSide (bspbrush_t *brushes, node_t *node)
{
	int value, bestvalue;
	bspbrush_t *brush, *test;
	side_t *bestside;
	int i, j, pass, numpasses, pnum;
	int front, back, both, facing, splits;
	int bsplits, bestsplits, epsilonbrush;
	qboolean hintsplit = qfalse;

	bestside = NULL;
	bestvalue = -99999;
	bestsplits = 0;

	/**
	 * the search order goes: visible-structural, visible-detail,
	 * nonvisible-structural, nonvisible-detail.
	 * If any valid plane is available in a pass, no further
	 * passes will be tried.
	 */
	numpasses = 4;
	for (pass = 0; pass < numpasses; pass++) {
		for (brush = brushes; brush; brush = brush->next) {
			if ((pass & 1) && !(brush->original->contentFlags & CONTENTS_DETAIL))
				continue;
			if (!(pass & 1) && (brush->original->contentFlags & CONTENTS_DETAIL))
				continue;
			for (i = 0; i < brush->numsides; i++) {
				/** @todo This will overflow if numsides is bigger than 6
				 * @sa bspbrush_t */
				side_t *side = brush->sides + i;
				if (side->bevel)
					continue;	/* never use a bevel as a spliter */
				if (!side->winding)
					continue;	/* nothing visible, so it can't split */
				if (side->texinfo == TEXINFO_NODE)
					continue;	/* already a node splitter */
				if (side->tested)
					continue;	/* we already have metrics for this plane */
				if (side->visible ^ (pass < 2))
					continue;	/* only check visible faces on first pass */

				pnum = side->planenum;
				pnum &= ~1;	/* always use positive facing plane */

				CheckPlaneAgainstParents(pnum, node);

				if (!CheckPlaneAgainstVolume(pnum, node))
					continue;	/* would produce a tiny volume */

				front = 0;
				back = 0;
				both = 0;
				facing = 0;
				splits = 0;
				epsilonbrush = 0;

				for (test = brushes; test; test = test->next) {
					const int s = TestBrushToPlanenum(test, pnum, &bsplits, &hintsplit, &epsilonbrush);

					splits += bsplits;
					if (bsplits && (s & PSIDE_FACING))
						Sys_Error("PSIDE_FACING with splits");

					test->testside = s;
					/* if the brush shares this face, don't bother
					 * testing that facenum as a splitter again */
					if (s & PSIDE_FACING) {
						facing++;
						for (j = 0; j < test->numsides; j++) {
							if ((test->sides[j].planenum &~ 1) == pnum)
								test->sides[j].tested = qtrue;
						}
					}
					if (s & PSIDE_FRONT)
						front++;
					if (s & PSIDE_BACK)
						back++;
					if (s == PSIDE_BOTH)
						both++;
				}

				/* give a value estimate for using this plane */

				value = 5 * facing - 5 * splits - abs(front - back);
				if (AXIAL(&mapplanes[pnum]))
					value += 5;		/* axial is better */
				value -= epsilonbrush * 1000;	/* avoid! */

				/* never split a hint side except with another hint */
				if (hintsplit && !(side->surfaceFlags & SURF_HINT))
					value = -9999999;

				/* save off the side test so we don't need
				 * to recalculate it when we actually seperate
				 * the brushes */
				if (value > bestvalue) {
					bestvalue = value;
					bestside = side;
					bestsplits = splits;
					for (test = brushes; test; test = test->next)
						test->side = test->testside;
				}
			}
		}

		/* if we found a good plane, don't bother trying any other passes */
		if (bestside) {
			if (pass > 1) {
				if (threadstate.numthreads == 1)
					c_nonvis++;
			}
			break;
		}
	}

	/* clear all the tested flags we set */
	for (brush = brushes; brush; brush = brush->next) {
		for (i = 0; i < brush->numsides; i++)
			brush->sides[i].tested = qfalse;
	}

	return bestside;
}

/**
 * @brief Checks on which side a of plane the brush is
 * @todo Or vice versa?
 */
static int BrushMostlyOnSide (const bspbrush_t *brush, const plane_t *plane)
{
	int i, j, side;
	vec_t max;

	max = 0;
	side = PSIDE_FRONT;
	for (i = 0; i < brush->numsides; i++) {
		const winding_t *w = brush->sides[i].winding;
		if (!w)
			continue;
		for (j = 0; j < w->numpoints; j++) {
			const vec_t d = DotProduct(w->p[j], plane->normal) - plane->dist;
			if (d > max) {
				max = d;
				side = PSIDE_FRONT;
			}
			if (-d > max) {
				max = -d;
				side = PSIDE_BACK;
			}
		}
	}
	return side;
}

/**
 * @brief Generates two new brushes, leaving the original unchanged
 */
void SplitBrush (const bspbrush_t *brush, int planenum, bspbrush_t **front, bspbrush_t **back)
{
	bspbrush_t *b[2];
	int i, j;
	winding_t *w, *cw[2], *midwinding;
	plane_t *plane, *plane2;
	side_t *cs;
	float d_front, d_back;

	*front = *back = NULL;
	plane = &mapplanes[planenum];

	/* check all points */
	d_front = d_back = 0;
	for (i = 0; i < brush->numsides; i++) {
		w = brush->sides[i].winding;
		if (!w)
			continue;
		for (j = 0; j < w->numpoints; j++) {
			const float d = DotProduct(w->p[j], plane->normal) - plane->dist;
			if (d > 0 && d > d_front)
				d_front = d;
			if (d < 0 && d < d_back)
				d_back = d;
		}
	}
	if (d_front < 0.1) { /* PLANESIDE_EPSILON) */
		/* only on back */
		*back = CopyBrush(brush);
		return;
	}
	if (d_back > -0.1) { /* PLANESIDE_EPSILON) */
		/* only on front */
		*front = CopyBrush(brush);
		return;
	}

	/* create a new winding from the split plane */
	w = BaseWindingForPlane(plane->normal, plane->dist);
	for (i = 0; i < brush->numsides && w; i++) {
		plane2 = &mapplanes[brush->sides[i].planenum ^ 1];
		ChopWindingInPlace(&w, plane2->normal, plane2->dist, 0); /* PLANESIDE_EPSILON); */
	}

	/* the brush isn't really split */
	if (!w || WindingIsTiny(w)) {
		const int side = BrushMostlyOnSide(brush, plane);
		if (side == PSIDE_FRONT)
			*front = CopyBrush(brush);
		else if (side == PSIDE_BACK)
			*back = CopyBrush(brush);
		return;
	}

	if (WindingIsHuge(w)) {
		/** @todo Print brush and entnum either of the brush that was splitted
		 * or the plane that was used as splitplane */
		Com_Printf("WARNING: Large winding\n");
	}

	midwinding = w;

	/* split it for real */
	for (i = 0; i < 2; i++) {
		b[i] = AllocBrush(brush->numsides + 1);
		b[i]->original = brush->original;
	}

	/* split all the current windings */
	for (i = 0; i < brush->numsides; i++) {
		const side_t *s = &brush->sides[i];
		w = s->winding;
		if (!w)
			continue;
		ClipWindingEpsilon(w, plane->normal, plane->dist,
			0 /*PLANESIDE_EPSILON*/, &cw[0], &cw[1]);
		for (j = 0; j < 2; j++) {
			if (!cw[j])
				continue;

			cs = &b[j]->sides[b[j]->numsides];
			b[j]->numsides++;
			*cs = *s;

			cs->winding = cw[j];
			cs->tested = qfalse;
		}
	}

	/* see if we have valid polygons on both sides */
	for (i = 0; i < 2; i++) {
		BoundBrush(b[i]);
		for (j = 0; j < 3; j++) {
			if (b[i]->mins[j] < -MAX_WORLD_WIDTH || b[i]->maxs[j] > MAX_WORLD_WIDTH) {
				/** @todo Print brush and entnum either of the brush that was split
				 * or the plane that was used as splitplane */
				Verb_Printf(VERB_EXTRA, "bogus brush after clip\n");
				break;
			}
		}

		if (b[i]->numsides < 3 || j < 3) {
			FreeBrush(b[i]);
			b[i] = NULL;
		}
	}

	if (!(b[0] && b[1])) {
		/** @todo Print brush and entnum either of the brush that was splitted
		 * or the plane that was used as splitplane */
		if (!b[0] && !b[1])
			Verb_Printf(VERB_EXTRA, "split removed brush\n");
		else
			Verb_Printf(VERB_EXTRA, "split not on both sides\n");
		if (b[0]) {
			FreeBrush(b[0]);
			*front = CopyBrush(brush);
		}
		if (b[1]) {
			FreeBrush(b[1]);
			*back = CopyBrush(brush);
		}
		return;
	}

	/* add the midwinding to both sides */
	for (i = 0; i < 2; i++) {
		cs = &b[i]->sides[b[i]->numsides];
		b[i]->numsides++;

		cs->planenum = planenum ^ i ^ 1;
		cs->texinfo = TEXINFO_NODE;
		cs->visible = qfalse;
		cs->tested = qfalse;
		if (i == 0)
			cs->winding = CopyWinding(midwinding);
		else
			cs->winding = midwinding;
	}

	{
		for (i = 0; i < 2; i++) {
			const vec_t v1 = BrushVolume(b[i]);
			if (v1 < 1.0) {
				FreeBrush(b[i]);
				b[i] = NULL;
				/** @todo Print brush and entnum either of the brush that was splitted
				 * or the plane that was used as splitplane */
				Verb_Printf(VERB_EXTRA, "tiny volume after clip\n");
			}
		}
	}

	*front = b[0];
	*back = b[1];
}

static void SplitBrushList (bspbrush_t *brushes, node_t *node, bspbrush_t **front, bspbrush_t **back)
{
	bspbrush_t *brush, *newbrush, *newbrush2;
	side_t *side;
	int sides, i;

	*front = *back = NULL;

	for (brush = brushes; brush; brush = brush->next) {
		sides = brush->side;

		if (sides == PSIDE_BOTH) {	/* split into two brushes */
			SplitBrush(brush, node->planenum, &newbrush, &newbrush2);
			if (newbrush) {
				newbrush->next = *front;
				Verb_Printf(VERB_DUMP, "SplitBrushList: Adding brush %i to front list.\n", newbrush->original->brushnum);
				*front = newbrush;
			}
			if (newbrush2) {
				newbrush2->next = *back;
				Verb_Printf(VERB_DUMP, "SplitBrushList: Adding brush %i to back list.\n", newbrush2->original->brushnum);
				*back = newbrush2;
			}
			continue;
		}

		newbrush = CopyBrush(brush);

		/* if the planenum is actualy a part of the brush
		 * find the plane and flag it as used so it won't be tried
		 * as a splitter again */
		if (sides & PSIDE_FACING) {
			for (i = 0; i < newbrush->numsides; i++) {
				side = newbrush->sides + i;
				if ((side->planenum & ~1) == node->planenum)
					side->texinfo = TEXINFO_NODE;
			}
		}

		if (sides & PSIDE_FRONT) {
			newbrush->next = *front;
			*front = newbrush;
			Verb_Printf(VERB_DUMP, "SplitBrushList: Adding brush %i to front list.\n", newbrush->original->brushnum);
			continue;
		}
		if (sides & PSIDE_BACK) {
			newbrush->next = *back;
			Verb_Printf(VERB_DUMP, "SplitBrushList: Adding brush %i to back list.\n", newbrush->original->brushnum);
			*back = newbrush;
			continue;
		}
	Verb_Printf(VERB_DUMP, "SplitBrushList: Brush %i fell off the map.\n", newbrush->original->brushnum);
	}
}


static node_t *BuildTree_r (node_t *node, bspbrush_t *brushes)
{
	node_t *newnode;
	side_t *bestside;
	int i;
	bspbrush_t *children[2];

	if (threadstate.numthreads == 1)
		c_nodes++;

	/* find the best plane to use as a splitter */
	bestside = SelectSplitSide(brushes, node);
	if (!bestside) {
		/* leaf node */
		node->side = NULL;
		node->planenum = PLANENUM_LEAF;
		LeafNode(node, brushes);
		Verb_Printf(VERB_DUMP, "BuildTree_r: Created a leaf node.\n");
		return node;
	}

	Verb_Printf(VERB_DUMP, "BuildTree_r: splitting along plane %i\n", bestside->planenum);

	/* this is a splitplane node */
	node->side = bestside;
	assert(bestside->planenum < MAX_MAP_PLANES);
	node->planenum = bestside->planenum & ~1;	/* always use front facing */

	SplitBrushList(brushes, node, &children[0], &children[1]);
	FreeBrushList(brushes);

	/* allocate children before recursing */
	for (i = 0; i < 2; i++) {
		newnode = AllocNode();
		newnode->parent = node;
		node->children[i] = newnode;
	}

	SplitBrush(node->volume, node->planenum, &node->children[0]->volume,
		&node->children[1]->volume);

	/* recursively process children */
	for (i = 0; i < 2; i++) {
		node->children[i] = BuildTree_r(node->children[i], children[i]);
	}

	return node;
}

/**
 * @brief The incoming list will be freed before exiting
 */
tree_t *BrushBSP (bspbrush_t *brushlist, vec3_t mins, vec3_t maxs)
{
	node_t *node;
	bspbrush_t *b;
	int c_faces, c_nonvisfaces, c_brushes;
	tree_t *tree;
	int i;
	vec_t volume;

	Verb_Printf(VERB_EXTRA, "--- BrushBSP ---\n");

	tree = AllocTree();

	c_faces = 0;
	c_nonvisfaces = 0;
	c_brushes = 0;
	for (b = brushlist; b; b = b->next) {
		c_brushes++;

		volume = BrushVolume(b);
		if (volume < config.microvolume) {
			Com_Printf("\nWARNING: entity %i, brush %i: microbrush, volume %.3g\n",
				b->original->entitynum, b->original->brushnum, volume);
		}

		for (i = 0; i < b->numsides; i++) {
			if (b->sides[i].bevel)
				continue;
			if (!b->sides[i].winding)
				continue;
			if (b->sides[i].texinfo == TEXINFO_NODE)
				continue;
			if (b->sides[i].visible)
				c_faces++;
			else
				c_nonvisfaces++;
		}

		AddPointToBounds(b->mins, tree->mins, tree->maxs);
		AddPointToBounds(b->maxs, tree->mins, tree->maxs);
	}

	Verb_Printf(VERB_EXTRA, "%5i brushes\n", c_brushes);
	Verb_Printf(VERB_EXTRA, "%5i visible faces\n", c_faces);
	Verb_Printf(VERB_EXTRA, "%5i nonvisible faces\n", c_nonvisfaces);

	c_nodes = 0;
	c_nonvis = 0;
	node = AllocNode();

	node->volume = BrushFromBounds(mins, maxs);

	tree->headnode = node;

	node = BuildTree_r(node, brushlist);
	Verb_Printf(VERB_EXTRA, "%5i visible nodes\n", c_nodes / 2 - c_nonvis);
	Verb_Printf(VERB_EXTRA, "%5i nonvis nodes\n", c_nonvis);
	Verb_Printf(VERB_EXTRA, "%5i leafs\n", (c_nodes + 1) / 2);
	return tree;
}


/**
 * @brief writes a map with the split bsp brushes
 */
void WriteBSPBrushMap (const char *name, const bspbrush_t *list)
{
	FILE *f;

	/* note it */
	Verb_Printf(VERB_LESS, "Writing %s\n", name);

	/* open the map file */
	f = fopen(name, "wb");
	if (f == NULL)
		Sys_Error("Can't write %s\b", name);

	fprintf(f, "{\n\"classname\" \"worldspawn\"\n");

	for (; list; list = list->next) {
		const side_t *s;
		int i;

		fprintf(f, "{\n");
		for (i = 0, s = list->sides; i < list->numsides; i++, s++) {
			winding_t *w = BaseWindingForPlane(mapplanes[s->planenum].normal, mapplanes[s->planenum].dist);
			const dBspTexinfo_t *t = &curTile->texinfo[s->texinfo];

			fprintf(f, "( %i %i %i ) ", (int)w->p[0][0], (int)w->p[0][1], (int)w->p[0][2]);
			fprintf(f, "( %i %i %i ) ", (int)w->p[1][0], (int)w->p[1][1], (int)w->p[1][2]);
			fprintf(f, "( %i %i %i ) ", (int)w->p[2][0], (int)w->p[2][1], (int)w->p[2][2]);

			fprintf(f, "%s 0 0 0 1 1 0 %i %i\n", t->texture, t->surfaceFlags, t->value);
			FreeWinding(w);
		}
		fprintf(f, "}\n");
	}
	fprintf(f, "}\n");

	fclose (f);
}
