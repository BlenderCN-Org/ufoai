/**
 * @file r_main.c
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

#include "r_local.h"
#include "r_particle.h"

ptlArt_t r_particlesArt[MAX_PTL_ART];
int r_numParticlesArt;

ptl_t r_particles[MAX_PTLS];
int r_numParticles;

/*
=============================================================
PARTICLE DRAWING
=============================================================
*/

/**
 * @brief Get Sprite Vectors
 * @param[in] p the particle to give the dimensions for
 * @param[out] right the output right vector for the particle
 * @param[out] up the output up vector for the particle
 * @sa R_DrawSprite
 */
static void R_GetSpriteVectors (const ptl_t *p, vec3_t right, vec3_t up)
{
	/* get transformation */
	switch (p->style) {
	case STYLE_FACING:
		VectorScale(vright, p->size[0], right);
		VectorScale(vup, p->size[1], up);
		break;

	case STYLE_ROTATED:
		AngleVectors(p->angles, NULL, right, up);
		VectorScale(right, p->size[0], right);
		VectorScale(up, p->size[1], up);
		break;

	case STYLE_BEAM:
	case STYLE_AXIS:
		AngleVectors(p->angles, right, NULL, NULL);
		CrossProduct(right, vpn, up);
		VectorNormalize(up);
		VectorScale(right, p->size[0], right);
		VectorScale(up, p->size[1], up);
		break;

	default:
		/* shouldn't happen */
		abort();
		return;
	}
}

/**
 * @sa R_DrawParticles
 */
static void R_DrawSprite (const ptl_t * p)
{
	const ptl_t *q;
	vec3_t up, right;
	vec3_t nup, nright;
	vec3_t pos;

	/* load texture set up coordinates */
	assert(p->pic);
	R_BindTexture(((image_t *) (p->pic->art))->texnum);

	/* calculate main position and normalised up and right vectors */
	q = p->parent ? p->parent : p;
	R_GetSpriteVectors(q, right, up);

	/* Calculate normalised */
	VectorCopy(up, nup);
	VectorCopy(right, nright);
	VectorNormalize(nup);
	VectorNormalize(nright);

	/* offset */
	VectorCopy(q->s, pos);
	VectorMA(pos, q->offset[0], nup, pos);
	VectorMA(pos, q->offset[1], nright, pos);

	if (p->parent) {
		/* if this is a child then calculate our own up and right vectors and offsets */
		R_GetSpriteVectors(p, right, up);

		/* but offset by our parent's nup and nright */
		VectorMA(pos, p->offset[0], nup, pos);
		VectorMA(pos, p->offset[1], nright, pos);
	}

	/* center image */
	VectorMA(pos, -0.5, up, pos);
	VectorMA(pos, -0.5, right, pos);

	R_Color(p->color);
	/* draw it */
	qglBegin(GL_TRIANGLE_FAN);

	qglTexCoord2f(0, 0);
	qglVertex3fv(pos);

	VectorAdd(pos, up, pos);
	qglTexCoord2f(0, 1);
	qglVertex3fv(pos);

	VectorAdd(pos, right, pos);
	qglTexCoord2f(1, 1);
	qglVertex3fv(pos);

	VectorSubtract(pos, up, pos);
	qglTexCoord2f(1, 0);
	qglVertex3fv(pos);

	qglEnd();
}


/**
 * @sa R_DrawParticles
 */
static void R_DrawPtlModel (ptl_t * p)
{
	modelInfo_t mi;

	/* initialize minfo */
	memset(&mi, 0, sizeof(modelInfo_t));
	mi.color = p->color;
	mi.origin = p->s;
	mi.angles = p->angles;
	assert(p->model);
	mi.model = (model_t *) p->model->art;
	mi.skin = p->skin;

	/* draw it */
	R_DrawModelParticle(&mi);
}

/**
 * @brief Draws a circle out of lines
 * @param[in] p The particle definition with origin, radius and color
 * @sa Draw_Circle
 */
static void R_DrawPtlCircle (const ptl_t* p)
{
	float radius = p->size[0];
	int thickness = (int)p->size[1];
	float theta;
	const float accuracy = 5.0f;

	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_LINE_SMOOTH);

	R_Color(p->color);

	if (p->stipplePattern) {
		qglLineStipple(1, p->stipplePattern);
		qglEnable(GL_LINE_STIPPLE);
	}

	assert(radius > thickness);
	if (thickness <= 1) {
		qglBegin(GL_LINE_LOOP);
		for (theta = 0.0f; theta < 2.0f * M_PI; theta += M_PI / (radius * accuracy)) {
			qglVertex3f(p->s[0] + radius * cos(theta), p->s[1] + radius * sin(theta), p->s[2]);
		}
		qglEnd();
	} else {
		qglBegin(GL_TRIANGLE_STRIP);
		for (theta = 0; theta <= 2 * M_PI; theta += M_PI / (radius * accuracy)) {
			qglVertex3f(p->s[0] + radius * cos(theta), p->s[1] + radius * sin(theta), p->s[2]);
			qglVertex3f(p->s[0] + radius * cos(theta - M_PI / (radius * accuracy)), p->s[1] + radius * sin(theta - M_PI / (radius * accuracy)), p->s[2]);
			qglVertex3f(p->s[0] + (radius - thickness) * cos(theta - M_PI / (radius * accuracy)), p->s[1] + (radius - thickness) * sin(theta - M_PI / (radius * accuracy)), p->s[2]);
			qglVertex3f(p->s[0] + (radius - thickness) * cos(theta), p->s[1] + (radius - thickness) * sin(theta), p->s[2]);
		}
		qglEnd();
	}

	if (p->stipplePattern)
		qglDisable(GL_LINE_STIPPLE);

	qglDisable(GL_LINE_SMOOTH);
	qglEnable(GL_TEXTURE_2D);
}

/**
 * @sa R_DrawParticles
 */
static void R_DrawPtlLine (const ptl_t * p)
{
	qglDisable(GL_TEXTURE_2D);
	qglEnable(GL_LINE_SMOOTH);

	R_Color(p->color);

	if (p->stipplePattern) {
		qglLineStipple(1, p->stipplePattern);
		qglEnable(GL_LINE_STIPPLE);
	}

	/* draw line from s to v */
	qglBegin(GL_LINE_STRIP);
	qglVertex3fv(p->s);
	qglVertex3fv(p->v);
	qglEnd();

	if (p->stipplePattern)
		qglDisable(GL_LINE_STIPPLE);

	qglDisable(GL_LINE_SMOOTH);
	qglEnable(GL_TEXTURE_2D);
}


/*
=============================================================
GENERIC PARTICLE FUNCTIONS
=============================================================
*/

/**
 * @sa R_DrawParticles
 */
static void R_SetBlendMode (int mode)
{
	switch (mode) {
	case BLEND_REPLACE:
		R_TexEnv(GL_REPLACE);
		break;
	case BLEND_BLEND:
		R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case BLEND_ADD:
		R_BlendFunc(GL_ONE, GL_ONE);
		break;
	case BLEND_FILTER:
		R_BlendFunc(GL_ZERO, GL_SRC_COLOR);
		break;
	case BLEND_INVFILTER:
		R_BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		break;
	default:
		Com_Error(ERR_DROP, "unknown blend mode");
		break;
	}
}

/**
 * @brief
 * @note No need to reset the blend mode - R_SetupGL2D will do this
 * @sa R_SetupGL2D
 */
void R_DrawParticles (void)
{
	ptl_t *p;
	int i;

	for (i = 0, p = r_particles; i < r_numParticles; i++, p++)
		if (p->inuse && !p->invis) {
			/* test for visibility */
			if (p->levelFlags && !((1 << refdef.worldlevel) & p->levelFlags))
				continue;

			/* set blend mode and draw gfx */
			R_SetBlendMode(p->blend);
			switch (p->style) {
			case STYLE_LINE:
				R_DrawPtlLine(p);
				break;
			case STYLE_CIRCLE:
				R_DrawPtlCircle(p);
				break;
			}
			if (p->pic)
				R_DrawSprite(p);
			if (p->model)
				R_DrawPtlModel(p);
			R_TexEnv(GL_MODULATE);
		}
}
