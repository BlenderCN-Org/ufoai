/**
 * @file cl_sequence.c
 * @brief Non-interactive sequence rendering and AVI recording.
 * @note Sequences are rendered on top of a menu node - the default menu
 * is stored in mn_sequence cvar
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

#include "client.h"

#define MAX_DATA_LENGTH 2048

typedef struct seqCmd_s {
	int (*handler) (const char *name, char *data);
	char name[MAX_VAR];
	char data[MAX_DATA_LENGTH];
} seqCmd_t;

typedef struct sequence_s {
	char name[MAX_VAR];
	int start;
	int length;
} sequence_t;

typedef enum {
	SEQ_END,
	SEQ_WAIT,
	SEQ_CLICK,
	SEQ_PRECACHE,
	SEQ_CAMERA,
	SEQ_MODEL,
	SEQ_2DOBJ,
	SEQ_REM,
	SEQ_CMD,

	SEQ_NUMCMDS
} seqCmdEnum_t;

static const char *seqCmdName[SEQ_NUMCMDS] = {
	"end",
	"wait",
	"click",
	"precache",
	"camera",
	"model",
	"2dobj",
	"rem",
	"cmd"
};

typedef struct seqCamera_s {
	vec3_t origin, speed;
	vec3_t angles, omega;
	float dist, ddist;
	float zoom, dzoom;
} seqCamera_t;

typedef struct seqEnt_s {
	qboolean inuse;
	char name[MAX_VAR];
	struct model_s *model;
	int skin;
	vec3_t origin, speed;
	vec3_t angles, omega;
	float alpha;
	char parent[MAX_VAR];
	char tag[MAX_VAR];
	animState_t as;
	entity_t *ep;
} seqEnt_t;

typedef struct seq2D_s {
	qboolean inuse;
	char name[MAX_VAR];
	char text[MAX_VAR];			/* a placeholder for gettext (V_TRANSLATION2_STRING) */
	char font[MAX_VAR];
	char image[MAX_VAR];
	vec2_t pos, speed;
	vec2_t size, enlarge;
	vec4_t color, fade, bgcolor;
	byte align;
	qboolean relativePos;		/* useful for translations when sentence length may differ */
} seq2D_t;

int SEQ_Click(const char *name, char *data);
int SEQ_Wait(const char *name, char *data);
int SEQ_Precache(const char *name, char *data);
int SEQ_Camera(const char *name, char *data);
int SEQ_Model(const char *name, char *data);
int SEQ_2Dobj(const char *name, char *data);
int SEQ_Remove(const char *name, char *data);
int SEQ_Command(const char *name, char *data);

static int (*seqCmdFunc[SEQ_NUMCMDS]) (const char *name, char *data) = {
	NULL,
	SEQ_Wait,
	SEQ_Click,
	SEQ_Precache,
	SEQ_Camera,
	SEQ_Model,
	SEQ_2Dobj,
	SEQ_Remove,
	SEQ_Command
};

#define MAX_SEQCMDS		8192
#define MAX_SEQUENCES	32
#define MAX_SEQENTS		128
#define MAX_SEQ2DS		128

static seqCmd_t seqCmds[MAX_SEQCMDS];
static int numSeqCmds;

static sequence_t sequences[MAX_SEQUENCES];
static int numSequences;

static int seqTime;	/**< miliseconds the sequence is already running */
static qboolean seqLocked = qfalse; /**< if a click event is triggered this is true */
static qboolean seqEndClickLoop = qfalse; /**< if the menu node the sequence is rendered in fetches a click this is true */
static int seqCmd, seqEndCmd;

static seqCamera_t seqCamera;

static seqEnt_t seqEnts[MAX_SEQENTS];
static int numSeqEnts;

static seq2D_t seq2Ds[MAX_SEQ2DS];
static int numSeq2Ds;

static cvar_t *seq_animspeed;


/**
 * @brief Sets the client state to ca_disconnected
 * @sa CL_SequenceStart_f
 */
void CL_SequenceEnd_f (void)
{
	CL_SetClientState(ca_disconnected);
}


/**
 * @brief Set the camera values for a sequence
 * @sa CL_SequenceRender
 */
static void CL_SequenceCamera (void)
{
	if (!scr_vrect.width || !scr_vrect.height)
		return;

	/* advance time */
	VectorMA(seqCamera.origin, cls.frametime, seqCamera.speed, seqCamera.origin);
	VectorMA(seqCamera.angles, cls.frametime, seqCamera.omega, seqCamera.angles);
	seqCamera.zoom += cls.frametime * seqCamera.dzoom;
	seqCamera.dist += cls.frametime * seqCamera.ddist;

	/* set camera */
	VectorCopy(seqCamera.origin, cl.cam.reforg);
	VectorCopy(seqCamera.angles, cl.cam.angles);

	AngleVectors(cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);
	VectorMA(cl.cam.reforg, -seqCamera.dist, cl.cam.axis[0], cl.cam.camorg);
	cl.cam.zoom = max(seqCamera.zoom, MIN_ZOOM);
	/* fudge to get isometric and perspective modes looking similar */
	if (cl_isometric->integer)
		cl.cam.zoom /= 1.35;
	V_CalcFovX();
}


/**
 * @brief Finds a given entity in all sequence entities
 * @sa CL_SequenceFind2D
 */
static seqEnt_t *CL_SequenceFindEnt (const char *name)
{
	seqEnt_t *se;
	int i;

	for (i = 0, se = seqEnts; i < numSeqEnts; i++, se++)
		if (se->inuse && !Q_strncmp(se->name, name, MAX_VAR))
			break;
	if (i < numSeqEnts)
		return se;
	else
		return NULL;
}


/**
 * @brief Finds a given 2d object in the current sequence data
 * @sa CL_SequenceFindEnt
 */
static seq2D_t *CL_SequenceFind2D (const char *name)
{
	seq2D_t *s2d;
	int i;

	for (i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++)
		if (s2d->inuse && !Q_strncmp(s2d->name, name, MAX_VAR))
			break;
	if (i < numSeq2Ds)
		return s2d;
	else
		return NULL;
}


/**
 * @brief
 * @sa CL_Sequence2D
 * @sa V_RenderView
 * @sa CL_SequenceEnd_f
 * @sa MN_PopMenu
 * @sa CL_SequenceFindEnt
 */
void CL_SequenceRender (void)
{
	entity_t ent;
	seqCmd_t *sc;
	seqEnt_t *se;
	float sunfrac;
	int i;

	/* run script */
	while (seqTime <= cl.time) {
		/* test for finish */
		if (seqCmd >= seqEndCmd) {
			CL_SequenceEnd_f();
			MN_PopMenu(qfalse);
			return;
		}

		/* call handler */
		sc = &seqCmds[seqCmd];
		seqCmd += sc->handler(sc->name, sc->data);
	}

	/* set camera */
	CL_SequenceCamera();

	/* render sequence */
	for (i = 0, se = seqEnts; i < numSeqEnts; i++, se++)
		if (se->inuse) {
			/* advance in time */
			VectorMA(se->origin, cls.frametime, se->speed, se->origin);
			VectorMA(se->angles, cls.frametime, se->omega, se->angles);
			re.AnimRun(&se->as, se->model, seq_animspeed->value * cls.frametime);

			/* add to scene */
			memset(&ent, 0, sizeof(ent));
			ent.model = se->model;
			ent.skinnum = se->skin;
			ent.as = se->as;
			ent.alpha = se->alpha;

			sunfrac = 1.0;
			ent.lightparam = &sunfrac;

			VectorCopy(se->origin, ent.origin);
			VectorCopy(se->origin, ent.oldorigin);
			VectorCopy(se->angles, ent.angles);

			if (se->parent && se->tag) {
				seqEnt_t *parent;

				parent = CL_SequenceFindEnt(se->parent);
				if (parent)
					ent.tagent = parent->ep;
				ent.tagname = se->tag;
			}

			/* add to render list */
			se->ep = V_GetEntity();
			V_AddEntity(&ent);
		}
}


/**
 * @brief Renders text and images
 * @sa CL_ResetSequences
 */
void CL_Sequence2D (void)
{
	seq2D_t *s2d;
	int i, j;
	int height = 0;

	/* add texts */
	for (i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++)
		if (s2d->inuse) {
			if (s2d->relativePos && height > 0) {
				s2d->pos[1] += height;
				s2d->relativePos = qfalse;
			}
			/* advance in time */
			for (j = 0; j < 4; j++) {
				s2d->color[j] += cls.frametime * s2d->fade[j];
				if (s2d->color[j] < 0.0)
					s2d->color[j] = 0.0;
				else if (s2d->color[j] > 1.0)
					s2d->color[j] = 1.0;
			}
			for (j = 0; j < 2; j++) {
				s2d->pos[j] += cls.frametime * s2d->speed[j];
				s2d->size[j] += cls.frametime * s2d->enlarge[j];
			}

			/* outside the screen? */
			/* FIXME: We need this check - but this does not work */
			/*if (s2d->pos[1] >= VID_NORM_HEIGHT || s2d->pos[0] >= VID_NORM_WIDTH)
				continue;*/

			/* render */
			re.DrawColor(s2d->color);

			/* image can be background */
			if (*s2d->image)
				re.DrawNormPic(s2d->pos[0], s2d->pos[1], s2d->size[0], s2d->size[1], 0, 0, 0, 0, s2d->align, qtrue, s2d->image);

			/* bgcolor can be overlay */
			if (s2d->bgcolor[3] > 0.0)
				re.DrawFill(s2d->pos[0], s2d->pos[1], s2d->size[0], s2d->size[1], s2d->align, s2d->bgcolor);

			/* render */
			re.DrawColor(s2d->color);

			/* gettext placeholder */
			if (*s2d->text)
				height += re.FontDrawString(s2d->font, s2d->align, s2d->pos[0], s2d->pos[1], s2d->pos[0], s2d->pos[1], (int) s2d->size[0], (int) s2d->size[1], -1 /* @todo: use this for some nice line spacing */, _(s2d->text), 0, 0, NULL, qfalse);
		}
	re.DrawColor(NULL);
}

/**
 * @brief Unlock a click event for the current sequence or ends the current sequence if not locked
 * @note Script binding for seq_click
 * @sa menu sequence in menu_main.ufo
 */
void CL_SequenceClick_f (void)
{
	if (seqLocked) {
		seqEndClickLoop = qtrue;
		seqLocked = qfalse;
	} else
		MN_PopMenu(qfalse);
}

/**
 * @brief Start a sequence
 * @sa CL_SequenceEnd_f
 */
void CL_SequenceStart_f (void)
{
	sequence_t *sp;
	const char *name, *menuName;
	int i;
	menu_t* menu;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: seq_start <name> [<menu>]\n");
		return;
	}
	name = Cmd_Argv(1);

	/* find sequence */
	for (i = 0, sp = sequences; i < numSequences; i++, sp++)
		if (!Q_strncmp(name, sp->name, MAX_VAR))
			break;
	if (i >= numSequences) {
		Com_Printf("Couldn't find sequence '%s'\n", name);
		return;
	}

	/* display the sequence menu */
	/* the default is in menu_main.ufo - menu sequence */
	menuName = Cmd_Argc() < 3 ? mn_sequence->string : Cmd_Argv(2);
	menu = MN_PushMenu(menuName);
	if (! menu) {
		Com_Printf("CL_SequenceStart_f: can't display menu '%s'\n", menuName);
		return;
	}

	/* init script parsing */
	numSeqEnts = 0;
	numSeq2Ds = 0;
	memset(&seqCamera, 0, sizeof(seqCamera_t));
	seqTime = cl.time;
	seqCmd = sp->start;
	seqEndCmd = sp->start + sp->length;

	/* init sequence state */
	CL_SetClientState(ca_sequence);

	/* init sun */
	VectorSet(map_sun.dir, 2, 2, 3);
	VectorSet(map_sun.ambient, 1.6, 1.6, 1.6);
	map_sun.ambient[3] = 5.4;
	VectorSet(map_sun.color, 1.2, 1.2, 1.2);
	map_sun.color[3] = 1.0;
}


/**
 * @brief
 */
void CL_ResetSequences (void)
{
	/* reset counters */
	seq_animspeed = Cvar_Get("seq_animspeed", "1000", 0, NULL);
	numSequences = 0;
	numSeqCmds = 0;
	numSeqEnts = 0;
	numSeq2Ds = 0;
	seqLocked = qfalse;
}


/* =========================================================== */

/** @brief valid id names for camera */
static const value_t seqCamera_vals[] = {
	{"origin", V_VECTOR, offsetof(seqCamera_t, origin), MEMBER_SIZEOF(seqCamera_t, origin)},
	{"speed", V_VECTOR, offsetof(seqCamera_t, speed), MEMBER_SIZEOF(seqCamera_t, speed)},
	{"angles", V_VECTOR, offsetof(seqCamera_t, angles), MEMBER_SIZEOF(seqCamera_t, angles)},
	{"omega", V_VECTOR, offsetof(seqCamera_t, omega), MEMBER_SIZEOF(seqCamera_t, omega)},
	{"dist", V_FLOAT, offsetof(seqCamera_t, dist), MEMBER_SIZEOF(seqCamera_t, dist)},
	{"ddist", V_FLOAT, offsetof(seqCamera_t, dist), MEMBER_SIZEOF(seqCamera_t, dist)},
	{"zoom", V_FLOAT, offsetof(seqCamera_t, zoom), MEMBER_SIZEOF(seqCamera_t, zoom)},
	{"dzoom", V_FLOAT, offsetof(seqCamera_t, dist), MEMBER_SIZEOF(seqCamera_t, dist)},
	{NULL, 0, 0, 0}
};

/** @brief valid entity names for a sequence */
static const value_t seqEnt_vals[] = {
	{"name", V_STRING, offsetof(seqEnt_t, name), 0},
	{"skin", V_INT, offsetof(seqEnt_t, skin), MEMBER_SIZEOF(seqEnt_t, skin)},
	{"alpha", V_FLOAT, offsetof(seqEnt_t, alpha), MEMBER_SIZEOF(seqEnt_t, alpha)},
	{"origin", V_VECTOR, offsetof(seqEnt_t, origin), MEMBER_SIZEOF(seqEnt_t, origin)},
	{"speed", V_VECTOR, offsetof(seqEnt_t, speed), MEMBER_SIZEOF(seqEnt_t, speed)},
	{"angles", V_VECTOR, offsetof(seqEnt_t, angles), MEMBER_SIZEOF(seqEnt_t, angles)},
	{"omega", V_VECTOR, offsetof(seqEnt_t, omega), MEMBER_SIZEOF(seqEnt_t, omega)},
	{"parent", V_STRING, offsetof(seqEnt_t, parent), 0},
	{"tag", V_STRING, offsetof(seqEnt_t, tag), 0},
	{NULL, 0, 0, 0}
};

/** @brief valid id names for 2d entity */
static const value_t seq2D_vals[] = {
	{"name", V_STRING, offsetof(seq2D_t, name), 0},
	{"text", V_TRANSLATION2_STRING, offsetof(seq2D_t, text), 0},
	{"font", V_STRING, offsetof(seq2D_t, font), 0},
	{"image", V_STRING, offsetof(seq2D_t, image), 0},
	{"pos", V_POS, offsetof(seq2D_t, pos), MEMBER_SIZEOF(seq2D_t, pos)},
	{"speed", V_POS, offsetof(seq2D_t, speed), MEMBER_SIZEOF(seq2D_t, speed)},
	{"size", V_POS, offsetof(seq2D_t, size), MEMBER_SIZEOF(seq2D_t, size)},
	{"enlarge", V_POS, offsetof(seq2D_t, enlarge), MEMBER_SIZEOF(seq2D_t, enlarge)},
	{"bgcolor", V_COLOR, offsetof(seq2D_t, bgcolor), MEMBER_SIZEOF(seq2D_t, bgcolor)},
	{"color", V_COLOR, offsetof(seq2D_t, color), MEMBER_SIZEOF(seq2D_t, color)},
	{"fade", V_COLOR, offsetof(seq2D_t, fade), MEMBER_SIZEOF(seq2D_t, fade)},
	{"align", V_ALIGN, offsetof(seq2D_t, align), MEMBER_SIZEOF(seq2D_t, align)},
	{"relative", V_BOOL, offsetof(seq2D_t, relativePos), MEMBER_SIZEOF(seq2D_t, relativePos)},
	{NULL, 0, 0, 0},
};

/**
 * @brief Wait until someone clicks with the mouse
 * @return 0 if you wait for the click
 * @return 1 if the click occured
 */
int SEQ_Click (const char *name, char *data)
{
	/* if a CL_SequenceClick_f event was called */
	if (seqEndClickLoop) {
		seqEndClickLoop = qfalse;
		seqLocked = qfalse;
		/* increase the command counter by 1 */
		return 1;
	}
	seqTime += 1000;
	seqLocked = qtrue;
	/* don't increase the command counter - stay at click command */
	return 0;
}

/**
 * @brief Increase the sequence time
 * @return 1 - increase the command position of the sequence by one
 */
int SEQ_Wait (const char *name, char *data)
{
	seqTime += 1000 * atof(name);
	return 1;
}

/**
 * @brief Precaches the models and images for a sequence
 * @return 1 - increase the command position of the sequence by one
 * @sa R_RegisterModel
 * @sa Draw_FindPic
 */
int SEQ_Precache (const char *name, char *data)
{
	if (!Q_strncmp(name, "models", 6)) {
		while (*data) {
			Com_DPrintf(DEBUG_CLIENT, "Precaching model: %s\n", data);
			re.RegisterModel(data);
			data += strlen(data) + 1;
		}
	} else if (!Q_strncmp(name, "pics", 4)) {
		while (*data) {
			Com_DPrintf(DEBUG_CLIENT, "Precaching image: %s\n", data);
			re.RegisterPic(data);
			data += strlen(data) + 1;
		}
	} else
		Com_Printf("SEQ_Precache: unknown format '%s'\n", name);
	return 1;
}

/**
 * @brief Parse the values for the camera like given in seqCamera
 */
int SEQ_Camera (const char *name, char *data)
{
	const value_t *vp;

	/* get values */
	while (*data) {
		for (vp = seqCamera_vals; vp->string; vp++)
			if (!Q_strcmp(data, vp->string)) {
				data += strlen(data) + 1;
				Com_ParseValue(&seqCamera, data, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string)
			Com_Printf("SEQ_Camera: unknown token '%s'\n", data);

		data += strlen(data) + 1;
	}
	return 1;
}

/**
 * @brief Parse values for a sequence model
 * @return 1 - increase the command position of the sequence by one
 * @sa seqEnt_vals
 * @sa CL_SequenceFindEnt
 */
int SEQ_Model (const char *name, char *data)
{
	seqEnt_t *se;
	const value_t *vp;
	int i;

	/* get sequence entity */
	se = CL_SequenceFindEnt(name);
	if (!se) {
		/* create new sequence entity */
		for (i = 0, se = seqEnts; i < numSeqEnts; i++, se++)
			if (!se->inuse)
				break;
		if (i >= numSeqEnts) {
			if (numSeqEnts >= MAX_SEQENTS)
				Com_Error(ERR_FATAL, "Too many sequence entities\n");
			se = &seqEnts[numSeqEnts++];
		}
		/* allocate */
		memset(se, 0, sizeof(seqEnt_t));
		se->inuse = qtrue;
		Com_sprintf(se->name, MAX_VAR, name);
	}

	/* get values */
	while (*data) {
		for (vp = seqEnt_vals; vp->string; vp++)
			if (!Q_strcmp(data, vp->string)) {
				data += strlen(data) + 1;
				Com_ParseValue(se, data, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string) {
			if (!Q_strncmp(data, "model", 5)) {
				data += strlen(data) + 1;
				Com_DPrintf(DEBUG_CLIENT, "Registering model: %s\n", data);
				se->model = re.RegisterModel(data);
			} else if (!Q_strncmp(data, "anim", 4)) {
				data += strlen(data) + 1;
				Com_DPrintf(DEBUG_CLIENT, "Change anim to: %s\n", data);
				re.AnimChange(&se->as, se->model, data);
			} else
				Com_Printf("SEQ_Model: unknown token '%s'\n", data);
		}

		data += strlen(data) + 1;
	}
	return 1;
}

/**
 * @brief Parse 2D objects like text and images
 * @return 1 - increase the command position of the sequence by one
 * @sa seq2D_vals
 * @sa CL_SequenceFind2D
 */
int SEQ_2Dobj (const char *name, char *data)
{
	seq2D_t *s2d;
	const value_t *vp;
	int i;

	/* get sequence text */
	s2d = CL_SequenceFind2D(name);
	if (!s2d) {
		/* create new sequence text */
		for (i = 0, s2d = seq2Ds; i < numSeq2Ds; i++, s2d++)
			if (!s2d->inuse)
				break;
		if (i >= numSeq2Ds) {
			if (numSeq2Ds >= MAX_SEQ2DS)
				Com_Error(ERR_FATAL, "Too many sequence 2d objects\n");
			s2d = &seq2Ds[numSeq2Ds++];
		}
		/* allocate */
		memset(s2d, 0, sizeof(seq2D_t));
		for (i = 0; i < 4; i++)
			s2d->color[i] = 1.0f;
		s2d->inuse = qtrue;
		Q_strncpyz(s2d->font, "f_big", sizeof(s2d->font));	/* default font */
		Q_strncpyz(s2d->name, name, sizeof(s2d->name));
	}

	/* get values */
	while (*data) {
		for (vp = seq2D_vals; vp->string; vp++)
			if (!Q_strcmp(data, vp->string)) {
				data += strlen(data) + 1;
				Com_ParseValue(s2d, data, vp->type, vp->ofs, vp->size);
				break;
			}
		if (!vp->string)
			Com_Printf("SEQ_Text: unknown token '%s'\n", data);

		data += strlen(data) + 1;
	}
	return 1;
}

/**
 * @brief Removed a sequence entity from the current sequence
 * @return 1 - increase the command position of the sequence by one
 * @sa CL_SequenceFind2D
 * @sa CL_SequenceFindEnt
 */
int SEQ_Remove (const char *name, char *data)
{
	seqEnt_t *se;
	seq2D_t *s2d;

	se = CL_SequenceFindEnt(name);
	if (se)
		se->inuse = qfalse;

	s2d = CL_SequenceFind2D(name);
	if (s2d)
		s2d->inuse = qfalse;

	if (!se && !s2d)
		Com_Printf("SEQ_Remove: couldn't find '%s'\n", name);
	return 1;
}

/**
 * @brief Executes a sequence command
 * @return 1 - increase the command position of the sequence by one
 * @sa Cbuf_AddText
 */
int SEQ_Command (const char *name, char *data)
{
	/* add the command */
	Cbuf_AddText(name);
	return 1;
}

/**
 * @brief Reads the sequence values from given text-pointer
 * @sa CL_ParseClientData
 */
void CL_ParseSequence (const char *name, const char **text)
{
	const char *errhead = "CL_ParseSequence: unexpected end of file (sequence ";
	sequence_t *sp;
	seqCmd_t *sc;
	const char *token;
	char *data;
	int i, depth, maxLength;

	/* search for sequences with same name */
	for (i = 0; i < numSequences; i++)
		if (!Q_strncmp(name, sequences[i].name, MAX_VAR))
			break;

	if (i < numSequences) {
		Com_Printf("CL_ParseSequence: sequence def \"%s\" with same name found, second ignored\n", name);
		return;
	}

	/* initialize the sequence */
	if (numSequences >= MAX_SEQUENCES)
		Com_Error(ERR_FATAL, "Too many sequences\n");

	sp = &sequences[numSequences++];
	memset(sp, 0, sizeof(sequence_t));
	Com_sprintf(sp->name, MAX_VAR, name);
	sp->start = numSeqCmds;

	/* get it's body */
	token = COM_Parse(text);

	if (!*text || *token != '{') {
		Com_Printf("CL_ParseSequence: sequence def \"%s\" without body ignored\n", name);
		numSequences--;
		return;
	}

	do {
		token = COM_EParse(text, errhead, name);
		if (!*text)
			break;
	next_cmd:
		if (*token == '}')
			break;

		/* check for commands */
		for (i = 0; i < SEQ_NUMCMDS; i++)
			if (!Q_strcmp(token, seqCmdName[i])) {
				maxLength = MAX_DATA_LENGTH;
				/* found a command */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;

				if (numSeqCmds >= MAX_SEQCMDS)
					Com_Error(ERR_FATAL, "Too many sequence commands\n");

				/* init seqCmd */
				sc = &seqCmds[numSeqCmds++];
				memset(sc, 0, sizeof(seqCmd_t));
				sc->handler = seqCmdFunc[i];
				sp->length++;

				/* copy name */
				Com_sprintf(sc->name, MAX_VAR, token);

				/* read data */
				token = COM_EParse(text, errhead, name);
				if (!*text)
					return;
				if (*token != '{')
					goto next_cmd;

				depth = 1;
				data = &sc->data[0];
				while (depth) {
					if (maxLength <= 0) {
						Com_Printf("Too much data for sequence %s", sc->name);
						break;
					}
					token = COM_EParse(text, errhead, name);
					if (!*text)
						return;

					if (*token == '{')
						depth++;
					else if (*token == '}')
						depth--;
					if (depth) {
						Com_sprintf(data, maxLength, token);
						data += strlen(token) + 1;
						maxLength -= (strlen(token) + 1);
					}
				}
				break;
			}

		if (i == SEQ_NUMCMDS) {
			Com_Printf("CL_ParseSequence: unknown command \"%s\" ignored (sequence %s)\n", token, name);
			COM_EParse(text, errhead, name);
		}
	} while (*text);
}

/* ===================== AVI FUNCTIONS ==================================== */

#include "snd_loc.h"

#define INDEX_FILE_EXTENSION ".index.dat"

#define PAD(x,y) (((x)+(y)-1) & ~((y)-1))

#define MAX_RIFF_CHUNKS 16

typedef struct audioFormat_s {
	int rate;
	int format;
	int channels;
	int bits;

	int sampleSize;
	int totalBytes;
} audioFormat_t;

typedef struct aviFileData_s {
	qboolean fileOpen;
	qFILE f;
	char fileName[MAX_QPATH];
	int fileSize;
	int moviOffset;
	int moviSize;

	qFILE idxF;
	int numIndices;

	int frameRate;
	int framePeriod;
	int width, height;
	int numVideoFrames;
	int maxRecordSize;
	qboolean motionJpeg;
	qboolean audio;
	audioFormat_t a;
	int numAudioFrames;

	int chunkStack[MAX_RIFF_CHUNKS];
	int chunkStackTop;

	byte *cBuffer, *eBuffer;
} aviFileData_t;

static aviFileData_t afd;

#define MAX_AVI_BUFFER 2048

static byte buffer[MAX_AVI_BUFFER];
static int bufIndex;

/**
 * @brief
 *
 * video
 * video [filename]
 */
void CL_Video_f (void)
{
	char filename[MAX_OSPATH];
	int i, last;

	if (Cmd_Argc() == 2) {
		/* explicit filename */
		Com_sprintf(filename, MAX_OSPATH, "videos/%s.avi", Cmd_Argv(1));
	} else {
		/* scan for a free filename */
		for (i = 0; i <= 9999; i++) {
			int a, b, c, d;

			last = i;

			a = last / 1000;
			last -= a * 1000;
			b = last / 100;
			last -= b * 100;
			c = last / 10;
			last -= c * 10;
			d = last;

			Com_sprintf(filename, MAX_OSPATH, "videos/ufo%d%d%d%d.avi", a, b, c, d);

			if (FS_CheckFile(filename) <= 0)
				break;			/* file doesn't exist */
		}

		if (i > 9999) {
			Com_Printf("ERROR: no free file names to create video\n");
			return;
		}
	}
	/* create path if it does not exists */
	FS_CreatePath(va("%s/%s", FS_Gamedir(), filename));
	CL_OpenAVIForWriting(filename);
}

/**
 * @brief
 */
void CL_StopVideo_f (void)
{
	CL_CloseAVI();
}

/**
 * @brief
 */
static inline void SafeFS_Write (const void *buffer, int len, qFILE * f)
{
	int write = FS_Write(buffer, len, f);

	if (write < len)
		Com_Printf("Failed to write avi file %p - %i:%i\n", (void*)f->f, write, len);
}

/**
 * @brief
 */
static inline void WRITE_STRING (const char *s)
{
	memcpy(&buffer[bufIndex], s, strlen(s));
	bufIndex += strlen(s);
}

/**
 * @brief
 */
static inline void WRITE_4BYTES (int x)
{
	buffer[bufIndex + 0] = (byte) ((x >> 0) & 0xFF);
	buffer[bufIndex + 1] = (byte) ((x >> 8) & 0xFF);
	buffer[bufIndex + 2] = (byte) ((x >> 16) & 0xFF);
	buffer[bufIndex + 3] = (byte) ((x >> 24) & 0xFF);
	bufIndex += 4;
}

/**
 * @brief
 */
static inline void WRITE_2BYTES (int x)
{
	buffer[bufIndex + 0] = (byte) ((x >> 0) & 0xFF);
	buffer[bufIndex + 1] = (byte) ((x >> 8) & 0xFF);
	bufIndex += 2;
}

#if 0
/**
 * @brief
 */
static inline void WRITE_1BYTES (int x)
{
	buffer[bufIndex] = x;
	bufIndex += 1;
}
#endif

/**
 * @brief
 */
static inline void START_CHUNK (const char *s)
{
	if (afd.chunkStackTop == MAX_RIFF_CHUNKS)
		Sys_Error("ERROR: Top of chunkstack breached\n");

	afd.chunkStack[afd.chunkStackTop] = bufIndex;
	afd.chunkStackTop++;
	WRITE_STRING(s);
	WRITE_4BYTES(0);
}

/**
 * @brief
 */
static inline void END_CHUNK (void)
{
	int endIndex = bufIndex;

	if (afd.chunkStackTop <= 0)
		Sys_Error("ERROR: Bottom of chunkstack breached\n");

	afd.chunkStackTop--;
	bufIndex = afd.chunkStack[afd.chunkStackTop];
	bufIndex += 4;
	WRITE_4BYTES(endIndex - bufIndex - 1);
	bufIndex = endIndex;
	bufIndex = PAD(bufIndex, 2);
}

/**
 * @brief
 */
static void CL_WriteAVIHeader (void)
{
	bufIndex = 0;
	afd.chunkStackTop = 0;

	START_CHUNK("RIFF");
	{
		WRITE_STRING("AVI ");
		{
			START_CHUNK("LIST");
			{
				WRITE_STRING("hdrl");
				WRITE_STRING("avih");
				WRITE_4BYTES(56);	/*"avih" "chunk" size */
				WRITE_4BYTES(afd.framePeriod);	/*dwMicroSecPerFrame */
				WRITE_4BYTES(afd.maxRecordSize * afd.frameRate);	/*dwMaxBytesPerSec */
				WRITE_4BYTES(0);	/*dwReserved1 */
				WRITE_4BYTES(0x110);	/*dwFlags bits HAS_INDEX and IS_INTERLEAVED */
				WRITE_4BYTES(afd.numVideoFrames);	/*dwTotalFrames */
				WRITE_4BYTES(0);	/*dwInitialFrame */

				if (afd.audio)	/*dwStreams */
					WRITE_4BYTES(2);
				else
					WRITE_4BYTES(1);

				WRITE_4BYTES(afd.maxRecordSize);	/*dwSuggestedBufferSize */
				WRITE_4BYTES(afd.width);	/*dwWidth */
				WRITE_4BYTES(afd.height);	/*dwHeight */
				WRITE_4BYTES(0);	/*dwReserved[ 0 ] */
				WRITE_4BYTES(0);	/*dwReserved[ 1 ] */
				WRITE_4BYTES(0);	/*dwReserved[ 2 ] */
				WRITE_4BYTES(0);	/*dwReserved[ 3 ] */

				START_CHUNK("LIST");
				{
					WRITE_STRING("strl");
					WRITE_STRING("strh");
					WRITE_4BYTES(56);	/*"strh" "chunk" size */
					WRITE_STRING("vids");

					if (afd.motionJpeg)
						WRITE_STRING("MJPG");
					else
						WRITE_STRING(" BGR");

					WRITE_4BYTES(0);	/*dwFlags */
					WRITE_4BYTES(0);	/*dwPriority */
					WRITE_4BYTES(0);	/*dwInitialFrame */

					WRITE_4BYTES(1);	/*dwTimescale */
					WRITE_4BYTES(afd.frameRate);	/*dwDataRate */
					WRITE_4BYTES(0);	/*dwStartTime */
					WRITE_4BYTES(afd.numVideoFrames);	/*dwDataLength */

					WRITE_4BYTES(afd.maxRecordSize);	/*dwSuggestedBufferSize */
					WRITE_4BYTES(-1);	/*dwQuality */
					WRITE_4BYTES(0);	/*dwSampleSize */
					WRITE_2BYTES(0);	/*rcFrame */
					WRITE_2BYTES(0);	/*rcFrame */
					WRITE_2BYTES(afd.width);	/*rcFrame */
					WRITE_2BYTES(afd.height);	/*rcFrame */

					WRITE_STRING("strf");
					WRITE_4BYTES(40);	/*"strf" "chunk" size */
					WRITE_4BYTES(40);	/*biSize */
					WRITE_4BYTES(afd.width);	/*biWidth */
					WRITE_4BYTES(afd.height);	/*biHeight */
					WRITE_2BYTES(1);	/*biPlanes */
					WRITE_2BYTES(24);	/*biBitCount */

					if (afd.motionJpeg)	/*biCompression */
						WRITE_STRING("MJPG");
					else
						WRITE_STRING(" BGR");

					WRITE_4BYTES(afd.width * afd.height);	/*biSizeImage */
					WRITE_4BYTES(0);	/*biXPelsPetMeter */
					WRITE_4BYTES(0);	/*biYPelsPetMeter */
					WRITE_4BYTES(0);	/*biClrUsed */
					WRITE_4BYTES(0);	/*biClrImportant */
				}
				END_CHUNK();

				if (afd.audio) {
					START_CHUNK("LIST");
					{
						WRITE_STRING("strl");
						WRITE_STRING("strh");
						WRITE_4BYTES(56);	/*"strh" "chunk" size */
						WRITE_STRING("auds");
						WRITE_4BYTES(0);	/*FCC */
						WRITE_4BYTES(0);	/*dwFlags */
						WRITE_4BYTES(0);	/*dwPriority */
						WRITE_4BYTES(0);	/*dwInitialFrame */

						WRITE_4BYTES(afd.a.sampleSize);	/*dwTimescale */
						WRITE_4BYTES(afd.a.sampleSize * afd.a.rate);	/*dwDataRate */
						WRITE_4BYTES(0);	/*dwStartTime */
						WRITE_4BYTES(afd.a.totalBytes / afd.a.sampleSize);	/*dwDataLength */

						WRITE_4BYTES(0);	/*dwSuggestedBufferSize */
						WRITE_4BYTES(-1);	/*dwQuality */
						WRITE_4BYTES(afd.a.sampleSize);	/*dwSampleSize */
						WRITE_2BYTES(0);	/*rcFrame */
						WRITE_2BYTES(0);	/*rcFrame */
						WRITE_2BYTES(0);	/*rcFrame */
						WRITE_2BYTES(0);	/*rcFrame */

						WRITE_STRING("strf");
						WRITE_4BYTES(18);	/*"strf" "chunk" size */
						WRITE_2BYTES(afd.a.format);	/*wFormatTag */
						WRITE_2BYTES(afd.a.channels);	/*nChannels */
						WRITE_4BYTES(afd.a.rate);	/*nSamplesPerSec */
						WRITE_4BYTES(afd.a.sampleSize * afd.a.rate);	/*nAvgBytesPerSec */
						WRITE_2BYTES(afd.a.sampleSize);	/*nBlockAlign */
						WRITE_2BYTES(afd.a.bits);	/*wBitsPerSample */
						WRITE_2BYTES(0);	/*cbSize */
					}
					END_CHUNK();
				}
			}
			END_CHUNK();

			afd.moviOffset = bufIndex;

			START_CHUNK("LIST");
			{
				WRITE_STRING("movi");
			}
		}
	}
}

/**
 * @brief Creates an AVI file and gets it into a state where
 * writing the actual data can begin
 */
qboolean CL_OpenAVIForWriting (const char *fileName)
{
	if (afd.fileOpen)
		return qfalse;

	memset(&afd, 0, sizeof(aviFileData_t));

	/* Don't start if a framerate has not been chosen */
	if (cl_avifreq->integer <= 0) {
		Com_Printf("cl_avifreq must be >= 1\n");
		return qfalse;
	}

	FS_FOpenFileWrite(va("%s/%s", FS_Gamedir(), fileName), &afd.f);
	if (afd.f.f == NULL) {
		Com_Printf("Could not open %s for writing\n", fileName);
		return qfalse;
	}

	FS_FOpenFileWrite(va("%s/%s" INDEX_FILE_EXTENSION, FS_Gamedir(), fileName), &afd.idxF);
	if (afd.idxF.f == NULL) {
		Com_Printf("Could not open index file for writing\n");
		FS_FCloseFile(&afd.f);
		return qfalse;
	}

	Com_sprintf(afd.fileName, MAX_QPATH, fileName);

	afd.frameRate = cl_avifreq->integer;
	afd.framePeriod = (int) (1000000.0f / afd.frameRate);
	afd.width = viddef.width;
	afd.height = viddef.height;

	Com_Printf("Capturing avi with resolution %i:%i\n", afd.width, afd.height);

	if (cl_aviMotionJpeg->integer) {
		Com_Printf("...MotionJPEG codec\n");
		afd.motionJpeg = qtrue;
	} else {
		Com_Printf("...no MotionJPEG\n");
		afd.motionJpeg = qfalse;
	}

	afd.cBuffer = Mem_Alloc(afd.width * afd.height * 4);
	afd.eBuffer = Mem_Alloc(afd.width * afd.height * 4);

	afd.a.rate = dma.speed;
	afd.a.format = 1;
	afd.a.channels = dma.channels;
	afd.a.bits = dma.samplebits;
	afd.a.sampleSize = (afd.a.bits / 8) * afd.a.channels;

	if (afd.a.rate % afd.frameRate) {
		int suggestRate = afd.frameRate;

		while ((afd.a.rate % suggestRate) && suggestRate >= 1)
			suggestRate--;

		Com_Printf("WARNING: cl_avifreq is not a divisor of the audio rate, suggest %d\n", suggestRate);
	}

	if (!Cvar_VariableInteger("snd_init")) {
		afd.audio = qfalse;
		Com_Printf("No audio for video capturing\n");
	} else {
		if (afd.a.bits == 16 && afd.a.channels == 2)
			afd.audio = qtrue;
		else {
			Com_Printf("No audio for video capturing\n");
			afd.audio = qfalse;	/*FIXME: audio not implemented for this case */
		}
	}

	Com_Printf("video frame rate: %i\naudio frame rate: %i\n", afd.frameRate, afd.a.rate);
	/* This doesn't write a real header, but allocates the */
	/* correct amount of space at the beginning of the file */
	CL_WriteAVIHeader();

	SafeFS_Write(buffer, bufIndex, &afd.f);
	afd.fileSize = bufIndex;

	bufIndex = 0;
	START_CHUNK("idx1");
	SafeFS_Write(buffer, bufIndex, &afd.idxF);

	afd.moviSize = 4;			/* For the "movi" */
	afd.fileOpen = qtrue;

	Com_Printf("Hint: Use a lower resolution for avi capturing will increase the speed\n");

	return qtrue;
}

/**
 * @brief
 */
static qboolean CL_CheckFileSize (int bytesToAdd)
{
	unsigned int newFileSize;

	newFileSize = afd.fileSize +	/* Current file size */
		bytesToAdd +			/* What we want to add */
		(afd.numIndices * 16) +	/* The index */
		4;						/* The index size */

	/* I assume all the operating systems */
	/* we target can handle a 2Gb file */
	if (newFileSize > INT_MAX) {
		/* Close the current file... */
		CL_CloseAVI();

		/* ...And open a new one */
		CL_OpenAVIForWriting(va("%s_", afd.fileName));

		return qtrue;
	}

	return qfalse;
}

/**
 * @brief
 * @sa R_TakeVideoFrame
 */
void CL_WriteAVIVideoFrame (const byte * imageBuffer, size_t size)
{
	int chunkOffset = afd.fileSize - afd.moviOffset - 8;
	int chunkSize = 8 + size;
	int paddingSize = PAD(size, 2) - size;
	byte padding[4] = { 0 };

	if (!afd.fileOpen)
		return;

	/* Chunk header + contents + padding */
	if (CL_CheckFileSize(8 + size + 2))
		return;

	bufIndex = 0;
	WRITE_STRING("00dc");
	WRITE_4BYTES(size);

	SafeFS_Write(buffer, 8, &afd.f);
	SafeFS_Write(imageBuffer, size, &afd.f);
	SafeFS_Write(padding, paddingSize, &afd.f);
	afd.fileSize += (chunkSize + paddingSize);

	afd.numVideoFrames++;
	afd.moviSize += (chunkSize + paddingSize);

	if (size > afd.maxRecordSize)
		afd.maxRecordSize = size;

	/* Index */
	bufIndex = 0;
	WRITE_STRING("00dc");		/*dwIdentifier */
	WRITE_4BYTES(0);			/*dwFlags */
	WRITE_4BYTES(chunkOffset);	/*dwOffset */
	WRITE_4BYTES(size);			/*dwLength */
	SafeFS_Write(buffer, 16, &afd.idxF);

	afd.numIndices++;
}

#define PCM_BUFFER_SIZE 44100

/**
 * @brief
 */
void CL_WriteAVIAudioFrame (const byte * pcmBuffer, size_t size)
{
	static byte pcmCaptureBuffer[PCM_BUFFER_SIZE] = { 0 };
	static int bytesInBuffer = 0;

	if (!afd.audio || !afd.fileOpen)
		return;

	/* Chunk header + contents + padding */
	if (CL_CheckFileSize(8 + bytesInBuffer + size + 2))
		return;

	if (bytesInBuffer + size > PCM_BUFFER_SIZE) {
		Com_Printf("WARNING: Audio capture buffer overflow -- truncating\n");
		size = PCM_BUFFER_SIZE - bytesInBuffer;
	}

	memcpy(&pcmCaptureBuffer[bytesInBuffer], pcmBuffer, size);
	bytesInBuffer += size;

	/* Only write if we have a frame's worth of audio */
	if (bytesInBuffer >= (int) ceil((float) afd.a.rate / (float) afd.frameRate) * afd.a.sampleSize) {
		int chunkOffset = afd.fileSize - afd.moviOffset - 8;
		int chunkSize = 8 + bytesInBuffer;
		int paddingSize = PAD(bytesInBuffer, 2) - bytesInBuffer;
		byte padding[4] = { 0 };

		bufIndex = 0;
		WRITE_STRING("01wb");
		WRITE_4BYTES(bytesInBuffer);

		SafeFS_Write(buffer, 8, &afd.f);
		SafeFS_Write(pcmBuffer, bytesInBuffer, &afd.f);
		SafeFS_Write(padding, paddingSize, &afd.f);
		afd.fileSize += (chunkSize + paddingSize);

		afd.numAudioFrames++;
		afd.moviSize += (chunkSize + paddingSize);
		afd.a.totalBytes += bytesInBuffer;

		/* Index */
		bufIndex = 0;
		WRITE_STRING("01wb");	/*dwIdentifier */
		WRITE_4BYTES(0);		/*dwFlags */
		WRITE_4BYTES(chunkOffset);	/*dwOffset */
		WRITE_4BYTES(bytesInBuffer);	/*dwLength */
		SafeFS_Write(buffer, 16, &afd.idxF);

		afd.numIndices++;

		bytesInBuffer = 0;
	}
}

/**
 * @brief Calls the renderer function to capture the frame
 */
void CL_TakeVideoFrame (void)
{
	/* AVI file isn't open */
	if (!afd.fileOpen)
		return;

	re.TakeVideoFrame(afd.width, afd.height, afd.cBuffer, afd.eBuffer, afd.motionJpeg);
}

/**
 * @brief Closes the AVI file and writes an index chunk
 */
qboolean CL_CloseAVI (void)
{
	int indexRemainder;
	int indexSize = afd.numIndices * 16;
	char *idxFileName = va("%s" INDEX_FILE_EXTENSION, afd.fileName);

	/* AVI file isn't open */
	if (!afd.fileOpen)
		return qfalse;

	afd.fileOpen = qfalse;

	FS_Seek(&afd.idxF, 4, FS_SEEK_SET);
	bufIndex = 0;
	WRITE_4BYTES(indexSize);
	SafeFS_Write(buffer, bufIndex, &afd.idxF);
	FS_FCloseFile(&afd.idxF);

	/* Write index */

	/* Open the temp index file */
	if ((indexSize = FS_FOpenFile(idxFileName, &afd.idxF)) <= 0) {
		FS_FCloseFile(&afd.f);
		return qfalse;
	}

	indexRemainder = indexSize;

	/* Append index to end of avi file */
	while (indexRemainder > MAX_AVI_BUFFER) {
		FS_Read(buffer, MAX_AVI_BUFFER, &afd.idxF);
		SafeFS_Write(buffer, MAX_AVI_BUFFER, &afd.f);
		afd.fileSize += MAX_AVI_BUFFER;
		indexRemainder -= MAX_AVI_BUFFER;
	}
	FS_Read(buffer, indexRemainder, &afd.idxF);
	SafeFS_Write(buffer, indexRemainder, &afd.f);
	afd.fileSize += indexRemainder;
	FS_FCloseFile(&afd.idxF);

	/* Remove temp index file */
#if 0
	/* FIXME */
	Com_Printf("remove temp index file '%s/%s'\n", FS_Gamedir(), idxFileName);
	/*FS_Remove(va("%s/%s", FS_Gamedir(), idxFileName);*/
#endif

	/* Write the real header */
	FS_Seek(&afd.f, 0, FS_SEEK_SET);
	CL_WriteAVIHeader();

	bufIndex = 4;
	WRITE_4BYTES(afd.fileSize - 8);	/* "RIFF" size */

	bufIndex = afd.moviOffset + 4;	/* Skip "LIST" */
	WRITE_4BYTES(afd.moviSize);

	SafeFS_Write(buffer, bufIndex, &afd.f);

	Mem_Free(afd.cBuffer);
	Mem_Free(afd.eBuffer);
	FS_FCloseFile(&afd.f);

	Com_Printf("Wrote %d:%d frames to %s\n", afd.numVideoFrames, afd.numAudioFrames, afd.fileName);

	return qtrue;
}

/**
 * @brief Status of video recording
 * @return true if video recording is active
 */
qboolean CL_VideoRecording (void)
{
	return afd.fileOpen;
}
