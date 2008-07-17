/**
 * @file cl_le.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#ifndef CLIENT_CL_LE_H
#define CLIENT_CL_LE_H

#define MAX_LE_PATHLENGTH 32

/** @brief a local entity */
typedef struct le_s {
	qboolean inuse;
	qboolean invis;
	qboolean autohide;
	qboolean selected;
	int hearTime;		/**< draw a marker over the entity if its an actor and he heard something */
	int type;				/**< the local entity type */
	int entnum;				/**< the server side edict num this le belongs to */

	vec3_t origin, oldOrigin;	/**< position given via world coordinates */
	pos3_t pos, oldPos;		/**< position on the grid */
	int dir;				/**< the current dir the le is facing into */

	int TU, maxTU;				/**< time units */
	int morale, maxMorale;		/**< morale value - used for soldier panic and the like */
	int HP, maxHP;				/**< health points */
	int STUN;					/**< if stunned - state STATE_STUN */
	int state;					/**< rf states, dead, crouched and so on */
	int reaction_minhit;

	float angles[3];
	float alpha;

	int team;		/**< the team number this local entity belongs to */
	int pnum;		/**< the player number this local entity belongs to */

	int client_action;		/**< entnum from server that is currently triggered */

	int contents;			/**< content flags for this LE - used for tracing */
	vec3_t mins, maxs;

	char inlineModelName[8];
	int modelnum1;	/**< the number of the body model in the cl.model_draw array */
	int modelnum2;	/**< the number of the head model in the cl.model_draw array */
	int skinnum;	/**< the skin number of the body and head model */
	struct model_s *model1, *model2;	/**< pointers to the cl.model_draw array
					 * that holds the models for body and head - model1 is body,
					 * model2 is head */

/* 	character_t	*chr; */

	/** is called every frame */
	void (*think) (struct le_s * le);
	/** number of frames to skip the think function for */
	int thinkDelay;

	/** various think function vars */
	byte path[MAX_LE_PATHLENGTH];
	int pathContents[MAX_LE_PATHLENGTH];	/**< content flags of the brushes the actor is walking in */
	int positionContents;					/**< content flags for the current brush the actor is standing in */
	int pathLength, pathPos;
	int startTime, endTime;
	int speed;			/**< the speed the le is moving with */
	float rotationSpeed;

	/** sound effects */
	struct sfx_s* sfx;
	float volume;

	/** gfx */
	animState_t as;	/**< holds things like the current active frame and so on */
	const char *particleID;
	int levelflags;	/**< the levels this particle should be visible at */
	ptl_t *ptl;				/**< particle pointer to display */
	const char *ref1, *ref2;
	inventory_t i;
	int left, right, extension, headgear;
	int fieldSize;				/**< ACTOR_SIZE_* */
	teamDef_t* teamDef;
	int gender;
	const fireDef_t *fd;	/**< in case this is a projectile */

	/** is called before adding a le to scene */
	qboolean(*addFunc) (struct le_s * le, entity_t * ent);
} le_t;							/* local entity */

#define MAX_LOCALMODELS		512

/** @brief local models */
typedef struct lm_s {
	char name[MAX_VAR];
	char particle[MAX_VAR];

	vec3_t origin;
	vec3_t angles;

	int entnum;
	int skin;
	int renderFlags;	/**< effect flags */
	int frame;	/**< which frame to show */
	char animname[MAX_QPATH];	/**< is this an animated model */
	int levelflags;
	animState_t as;

	struct model_s *model;
} localModel_t;							/* local models */

extern localModel_t LMs[MAX_LOCALMODELS];
extern int numLMs;

extern le_t LEs[MAX_EDICTS];
extern int numLEs;

static const vec3_t player_mins = { -PLAYER_WIDTH, -PLAYER_WIDTH, PLAYER_MIN };
static const vec3_t player_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_STAND };
static const vec3_t player_dead_maxs = { PLAYER_WIDTH, PLAYER_WIDTH, PLAYER_DEAD };

qboolean CL_OutsideMap(vec3_t impact);
const char *LE_GetAnim(const char *anim, int right, int left, int state);
void LE_AddProjectile(const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t impact, int normal, qboolean autohide);
void LE_AddGrenade(const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t v0, int dt);
void LE_AddAmbientSound(const char *sound, const vec3_t origin, float volume, int levelflags);
le_t *LE_GetClosestActor(const vec3_t origin);

void LE_Think(void);
/* think functions */
void LET_StartIdle(le_t *le);
void LET_Appear(le_t *le);
void LET_StartPathMove(le_t *le);
void LET_ProjectileAutoHide(le_t *le);
void LET_PlayAmbientSound(le_t *le);
void LET_BrushModel(le_t *le);

/* local model functions */
localModel_t *LM_AddModel(const char *model, const char *particle, const vec3_t origin, const vec3_t angles, int entnum, int levelflags, int flags);
void LM_Perish(struct dbuffer *msg);
void LM_AddToScene(void);

qboolean LE_BrushModelAction(le_t *le, entity_t *ent);
void CL_RecalcRouting(const le_t *le);
void CL_CompleteRecalcRouting(void);

qboolean LE_IsLivingActor(const le_t *le);
void LE_Explode(struct dbuffer *msg);
void LE_DoorOpen(struct dbuffer *msg);
void LE_DoorClose(struct dbuffer *msg);
le_t *LE_Add(int entnum);
le_t *LE_Get(int entnum);
le_t *LE_Find(int type, pos3_t pos);
void LE_Cleanup(void);
void LE_AddToScene(void);

trace_t CL_Trace(vec3_t start, vec3_t end, const vec3_t mins, const vec3_t maxs, le_t * passle, le_t * passle2, int contentmask);

void LM_Register(void);

#endif
