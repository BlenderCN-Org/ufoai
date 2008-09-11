/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "r_local.h"

void R_UseProgram  (r_program_t *prog)
{
	if (!qglUseProgram || r_state.active_program == prog)
		return;

	r_state.active_program = prog;

	if (prog) {
		qglUseProgram(prog->id);

		if (prog->use)  /* invoke use function */
			prog->use();
	} else {
		qglUseProgram(0);
	}
}

static r_progvar_t *R_ProgramVariable (int type, const char *name)
{
	r_progvar_t *v;
	int i;

	if (!r_state.active_program) {
		Com_Printf("R_ProgramVariable: No program bound.\n");
		return NULL;
	}

	/* find the variable */
	for (i = 0; i < MAX_PROGRAM_VARS; i++) {
		v = &r_state.active_program->vars[i];

		if (!v->location)
			break;

		if (v->type == type && !strcmp(v->name, name))
			return v;
	}

	if (i == MAX_PROGRAM_VARS) {
		Com_Printf("R_ProgramVariable: MAX_PROGRAM_VARS reached.\n");
		return NULL;
	}

	/* or query for it */
	if (type == GL_UNIFORM)
		v->location = qglGetUniformLocation(r_state.active_program->id, name);
	else
		v->location = qglGetAttribLocation(r_state.active_program->id, name);

	if (v->location == -1) {
		Com_Printf("R_ProgramVariable: Could not find %s in shader\n", name);
		return NULL;
	}

	v->type = type;
	Q_strncpyz(v->name, name, sizeof(v->name));

	return v;
}

static void R_ProgramParameter1i (const char *name, GLint value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform1i(v->location, value);
}

#if 0
static void R_ProgramParameter1f (const char *name, GLfloat value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform1f(v->location, value);
}

static void R_ProgramParameter3fv (const char *name, GLfloat *value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform3fv(v->location, 1, value);
}
#endif

static void R_ProgramParameter4fv (const char *name, GLfloat *value)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_UNIFORM, name)))
		return;

	qglUniform4fv(v->location, 1, value);
}

void R_AttributePointer (const char *name, GLuint size, GLvoid *array)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_ATTRIBUTE, name)))
		return;

	qglVertexAttribPointer(v->location, size, GL_FLOAT, GL_FALSE, 0, array);
}

void R_EnableAttribute (const char *name)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_ATTRIBUTE, name)))
		return;

	qglEnableVertexAttribArray(v->location);
}

void R_DisableAttribute (const char *name)
{
	r_progvar_t *v;

	if (!(v = R_ProgramVariable(GL_ATTRIBUTE, name)))
		return;

	qglDisableVertexAttribArray(v->location);
}

static void R_ShutdownShader (r_shader_t *sh)
{
	qglDeleteShader(sh->id);
	memset(sh, 0, sizeof(r_shader_t));
}

static void R_ShutdownProgram (r_program_t *prog)
{
	if (prog->v)
		R_ShutdownShader(prog->v);
	if (prog->f)
		R_ShutdownShader(prog->f);

	qglDeleteProgram(prog->id);

	memset(prog, 0, sizeof(r_program_t));
}

void R_ShutdownPrograms (void)
{
	int i;

	if (!qglDeleteProgram)
		return;

	for (i = 0; i < MAX_PROGRAMS; i++) {
		if (!r_state.programs[i].id)
			continue;

		R_ShutdownProgram(&r_state.programs[i]);
	}
}

static size_t R_ShaderIncludes (const char *name, const char *in, char *out, size_t len)
{
	char path[MAX_QPATH];
	byte *buf;
	int i;
	const char *hwHack;

	switch (r_config.hardwareType) {
	case GLHW_ATI:
		hwHack = "#ifndef ATI\n#define ATI\n#endif\n";
		break;
	case GLHW_INTEL:
		hwHack = "#ifndef INTEL\n#define INTEL\n#endif\n";
		break;
	case GLHW_NVIDIA:
		hwHack = "#ifndef NVIDIA\n#define NVIDIA\n#endif\n";
		break;
	case GLHW_GENERIC:
		hwHack = NULL;
		break;
	default:
		Sys_Error("R_ShaderIncludes: Unknown hardwaretype");
	}

	i = 0;

	if (hwHack) {
		size_t hwHackLength = strlen(hwHack);
		strcpy(out, hwHack);
		out += hwHackLength;
		len -= hwHackLength;
		if (len < 0)
			Sys_Error("overflow in shader loading '%s'", name);
		i += hwHackLength;
	}

	while (*in) {
		if (!strncmp(in, "#include", 8)) {
			size_t inc_len;
			in += 8;
			Com_sprintf(path, sizeof(path), "shaders/%s", COM_Parse(&in));

			if (FS_LoadFile(path, &buf) == -1) {
				Com_Printf("Failed to resolve #include: %s.\n", path);
				continue;
			}

			inc_len = R_ShaderIncludes(name,  (const char *)buf, out, len);
			len -= inc_len;
			out += inc_len;
			FS_FreeFile(buf);
		}
		len--;
		if (len < 0)
			Sys_Error("overflow in shader loading '%s'", name);
		*out++ = *in++;
		i++;
	}
	return i;
}

#define SHADER_BUF_SIZE 16384

static r_shader_t *R_LoadShader (GLenum type, const char *name)
{
	r_shader_t *sh;
	char path[MAX_QPATH], *src[1];
	unsigned e, len, length[1];
	char *source;
	byte *buf;
	int i;

	snprintf(path, sizeof(path), "shaders/%s", name);

	if ((len = FS_LoadFile(path, &buf)) == -1) {
		Com_DPrintf(DEBUG_RENDERER, "R_LoadShader: Failed to load %s.\n", name);
		return NULL;
	}

	source = Mem_PoolAlloc(SHADER_BUF_SIZE, vid_imagePool, 0);

	R_ShaderIncludes(name, (const char *)buf, source, SHADER_BUF_SIZE);
	FS_FreeFile(buf);

	src[0] = source;
	length[0] = strlen(source);

	for (i = 0; i < MAX_SHADERS; i++) {
		sh = &r_state.shaders[i];

		if (!sh->id)
			break;
	}

	if (i == MAX_SHADERS) {
		Com_Printf("R_LoadShader: MAX_SHADERS reached.\n");
		Mem_Free(source);
		return NULL;
	}

	strncpy(sh->name, name, sizeof(sh->name));

	sh->type = type;

	sh->id = qglCreateShader(sh->type);
	qglShaderSource(sh->id, 1, src, length);

	/* compile it and check for errors */
	qglCompileShader(sh->id);

	qglGetShaderiv(sh->id, GL_COMPILE_STATUS, &e);
	if (!e) {
		char log[MAX_STRING_CHARS];
		qglGetShaderInfoLog(sh->id, sizeof(log) - 1, NULL, log);
		Com_Printf("R_LoadShader: %s: %s\n", sh->name, log);

		qglDeleteShader(sh->id);
		memset(sh, 0, sizeof(r_shader_t));

		Mem_Free(source);
		return NULL;
	}

	Mem_Free(source);
	return sh;
}

static r_program_t *R_LoadProgram (const char *name, void *init, void *use, void *think)
{
	r_program_t *prog;
	char log[MAX_STRING_CHARS];
	unsigned e;
	int i;

	for (i = 0; i < MAX_PROGRAMS; i++) {
		prog = &r_state.programs[i];

		if (!prog->id)
			break;
	}

	if (i == MAX_PROGRAMS) {
		Com_Printf("R_LoadProgram: MAX_PROGRAMS reached.\n");
		return NULL;
	}

	Q_strncpyz(prog->name, name, sizeof(prog->name));

	prog->id = qglCreateProgram();

	prog->v = R_LoadShader(GL_VERTEX_SHADER, va("%s.vs", name));
	prog->f = R_LoadShader(GL_FRAGMENT_SHADER, va("%s.fs", name));

	if (prog->v)
		qglAttachShader(prog->id, prog->v->id);
	if (prog->f)
		qglAttachShader(prog->id, prog->f->id);

	qglLinkProgram(prog->id);

	qglGetProgramiv(prog->id, GL_LINK_STATUS, &e);
	if (!e) {
		qglGetProgramInfoLog(prog->id, sizeof(log) - 1, NULL, log);
		Com_Printf("R_LoadProgram: %s: %s\n", prog->name, log);

		R_ShutdownProgram(prog);
		return NULL;
	}

	prog->init = init;

	if (prog->init) {  /* invoke initialization function */
		R_UseProgram(prog);

		prog->init();

		R_UseProgram(NULL);
	}

	prog->use = use;
	prog->think = think;

	Com_Printf("R_LoadProgram: '%s' loaded.\n", name);

	return prog;
}

static void R_InitDefaultProgram (void)
{
	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1i("SAMPLER1", 1);
	R_ProgramParameter1i("SAMPLER2", 2);
	R_ProgramParameter1i("SAMPLER3", 3);

	R_ProgramParameter1i("LIGHTMAP", 0);
	R_ProgramParameter1i("BUMPMAP", 0);
}

static void R_UseDefaultProgram (void)
{
	if (texunit_lightmap.enabled)
		R_ProgramParameter1i("LIGHTMAP", 1);
	else
		R_ProgramParameter1i("LIGHTMAP", 0);
}

static void R_ThinkDefaultProgram (void)
{
	if (r_state.bumpmap_enabled) {
		R_EnableAttribute("TANGENT");
		R_ProgramParameter1i("BUMPMAP", 1);
	} else {
		R_DisableAttribute("TANGENT");
		R_ProgramParameter1i("BUMPMAP", 0);
	}
}

static void R_InitWarpProgram (void)
{
	R_ProgramParameter1i("SAMPLER0", 0);
	R_ProgramParameter1i("SAMPLER1", 1);
}

static void R_UseWarpProgram (void)
{
	static vec4_t offset;

	offset[0] = offset[1] = refdef.time / 8.0;
	R_ProgramParameter4fv("OFFSET", offset);
}

void R_InitPrograms (void)
{
	if (!qglCreateProgram) {
		Com_Printf("R_InitPrograms: glCreateProgram not found\n");
		return;
	}

	memset(r_state.shaders, 0, sizeof(r_state.shaders));

	memset(r_state.programs, 0, sizeof(r_state.programs));

	r_state.default_program = R_LoadProgram("default", R_InitDefaultProgram,
		R_UseDefaultProgram, R_ThinkDefaultProgram);

	r_state.warp_program = R_LoadProgram("warp", R_InitWarpProgram,
			R_UseWarpProgram, NULL);
}

/**
 * @brief Reloads the glsl shaders
 */
void R_RestartPrograms_f (void)
{
	R_ShutdownPrograms();
	R_InitPrograms();
}
