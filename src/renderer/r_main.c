/**
 * @file r_main.c
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

#include "r_local.h"
#include "r_program.h"
#include "r_sphere.h"
#include "r_font.h"
#include "r_light.h"
#include "r_lightmap.h"
#include "r_main.h"
#include "r_error.h"
#include "../common/tracing.h"

refdef_t refdef;

rconfig_t r_config;
rstate_t r_state;
rlocals_t r_locals;

image_t *r_noTexture;			/* use for bad textures */
image_t *r_warpTexture;

static cvar_t *r_maxtexres;

cvar_t *r_brightness;
cvar_t *r_contrast;
cvar_t *r_invert;
cvar_t *r_monochrome;
cvar_t *r_drawentities;
cvar_t *r_drawworld;
cvar_t *r_drawspecialbrushes;
cvar_t *r_nocull;
cvar_t *r_isometric;
cvar_t *r_anisotropic;
cvar_t *r_texture_lod;			/* lod_bias */
cvar_t *r_screenshot_format;
cvar_t *r_screenshot_jpeg_quality;
cvar_t *r_lightmap;
cvar_t *r_ext_texture_compression;
cvar_t *r_ext_s3tc_compression;
cvar_t *r_intel_hack;
cvar_t *r_materials;
cvar_t *r_checkerror;
cvar_t *r_drawbuffer;
cvar_t *r_driver;
cvar_t *r_shadows;
cvar_t *r_soften;
cvar_t *r_modulate;
cvar_t *r_swapinterval;
cvar_t *r_multisample;
cvar_t *r_texturemode;
cvar_t *r_texturealphamode;
cvar_t *r_texturesolidmode;
cvar_t *r_wire;
cvar_t *r_showbox;
cvar_t *r_threads;
cvar_t *r_vertexbuffers;
cvar_t *r_warp;
cvar_t *r_lighting;
cvar_t *r_programs;
cvar_t *r_maxlightmap;
cvar_t *r_geoscape_overlay;
cvar_t *r_bumpmap;
cvar_t *r_specular;
cvar_t *r_shownormals;
cvar_t *r_parallax;

/**
 * @brief Prints some OpenGL strings
 */
static void R_Strings_f (void)
{
	Com_Printf("GL_VENDOR: %s\n", r_config.vendorString);
	Com_Printf("GL_RENDERER: %s\n", r_config.rendererString);
	Com_Printf("GL_VERSION: %s\n", r_config.versionString);
	Com_Printf("MODE: %i, %d x %d FULLSCREEN: %i\n", vid_mode->integer, viddef.width, viddef.height, vid_fullscreen->integer);
	Com_Printf("GL_EXTENSIONS: %s\n", r_config.extensionsString);
	Com_Printf("GL_MAX_TEXTURE_SIZE: %d\n", r_config.maxTextureSize);
}

/**
 * @brief Returns true if the box is completely outside the frustum
 */
qboolean R_CullBox (const vec3_t mins, const vec3_t maxs)
{
	int i;

	for (i = 0; i < 4; i++)
		if (TR_BoxOnPlaneSide(mins, maxs, &r_locals.frustum[i]) == PSIDE_BACK)
			return qtrue;
	return qfalse;
}

/**
 * @sa R_ModLoadPlanes
 */
static inline int SignbitsForPlane (const cBspPlane_t * out)
{
	int bits, j;

	/* for fast box on planeside test */
	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

void R_SetupFrustum (void)
{
	int i;

	/* build the transformation matrix for the given view angles */
	AngleVectors(refdef.viewangles, r_locals.forward, r_locals.right, r_locals.up);

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (refdef.rdflags & RDF_NOWORLDMODEL) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(refdef.x, viddef.height - refdef.height - refdef.y, refdef.width, refdef.height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		R_CheckError();
		glDisable(GL_SCISSOR_TEST);
	}

	if (r_isometric->integer) {
		/* 4 planes of a cube */
		VectorScale(r_locals.right, +1, r_locals.frustum[0].normal);
		VectorScale(r_locals.right, -1, r_locals.frustum[1].normal);
		VectorScale(r_locals.up, +1, r_locals.frustum[2].normal);
		VectorScale(r_locals.up, -1, r_locals.frustum[3].normal);

		for (i = 0; i < 4; i++) {
			r_locals.frustum[i].type = PLANE_ANYZ;
			r_locals.frustum[i].dist = DotProduct(refdef.vieworg, r_locals.frustum[i].normal);
			r_locals.frustum[i].signbits = SignbitsForPlane(&r_locals.frustum[i]);
		}
		r_locals.frustum[0].dist -= 10 * refdef.fov_x;
		r_locals.frustum[1].dist -= 10 * refdef.fov_x;
		r_locals.frustum[2].dist -= 10 * refdef.fov_x * ((float) refdef.height / refdef.width);
		r_locals.frustum[3].dist -= 10 * refdef.fov_x * ((float) refdef.height / refdef.width);
	} else {
		/* rotate VPN right by FOV_X/2 degrees */
		RotatePointAroundVector(r_locals.frustum[0].normal, r_locals.up, r_locals.forward, -(90 - refdef.fov_x / 2));
		/* rotate VPN left by FOV_X/2 degrees */
		RotatePointAroundVector(r_locals.frustum[1].normal, r_locals.up, r_locals.forward, 90 - refdef.fov_x / 2);
		/* rotate VPN up by FOV_X/2 degrees */
		RotatePointAroundVector(r_locals.frustum[2].normal, r_locals.right, r_locals.forward, 90 - refdef.fov_y / 2);
		/* rotate VPN down by FOV_X/2 degrees */
		RotatePointAroundVector(r_locals.frustum[3].normal, r_locals.right, r_locals.forward, -(90 - refdef.fov_y / 2));

		for (i = 0; i < 4; i++) {
			r_locals.frustum[i].type = PLANE_ANYZ;
			r_locals.frustum[i].dist = DotProduct(refdef.vieworg, r_locals.frustum[i].normal);
			r_locals.frustum[i].signbits = SignbitsForPlane(&r_locals.frustum[i]);
		}
	}
}

static inline void R_Clear (void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	R_CheckError();
	glDepthFunc(GL_LEQUAL);
	R_CheckError();

	glDepthRange(0.0f, 1.0f);
	R_CheckError();
}

/**
 * @sa CL_ClearState
 */
static inline void R_ClearScene (void)
{
	r_numEntities = r_numLights = 0;
}

/**
 * @sa R_RenderFrame
 * @sa R_EndFrame
 */
void R_BeginFrame (void)
{
	/* change modes if necessary */
	if (vid_mode->modified || vid_fullscreen->modified) {
		R_SetMode();
#if defined(_WIN32) || defined(__APPLE__)
		VID_Restart_f();
#endif
	}
	/* we definitly need a restart here */
	if (r_ext_texture_compression->modified)
		VID_Restart_f();

	if (r_anisotropic->modified) {
		if (r_anisotropic->integer > r_config.maxAnisotropic) {
			Com_Printf("...max GL_EXT_texture_filter_anisotropic value is %i\n", r_config.maxAnisotropic);
			Cvar_SetValue("r_anisotropic", r_config.maxAnisotropic);
		}
		/*R_UpdateAnisotropy();*/
		r_anisotropic->modified = qfalse;
	}

	/* draw buffer stuff */
	if (r_drawbuffer->modified) {
		r_drawbuffer->modified = qfalse;

		if (Q_strcasecmp(r_drawbuffer->string, "GL_FRONT") == 0)
			glDrawBuffer(GL_FRONT);
		else
			glDrawBuffer(GL_BACK);
		R_CheckError();
	}

	/* texturemode stuff */
	/* Realtime set level of anisotropy filtering and change texture lod bias */
	if (r_texturemode->modified) {
		R_TextureMode(r_texturemode->string);
		r_texturemode->modified = qfalse;
	}

	if (r_texturealphamode->modified) {
		R_TextureAlphaMode(r_texturealphamode->string);
		r_texturealphamode->modified = qfalse;
	}

	if (r_texturesolidmode->modified) {
		R_TextureSolidMode(r_texturesolidmode->string);
		r_texturesolidmode->modified = qfalse;
	}

	/* threads */
	if (r_threads->modified) {
		if (r_threads->integer)
			R_InitThreads();
		else
			R_ShutdownThreads();
		r_threads->modified = qfalse;
	}

	R_SetupGL2D();

	/* clear screen if desired */
	R_Clear();
}

/**
 * @sa R_BeginFrame
 * @sa R_EndFrame
 */
void R_RenderFrame (void)
{
	int tile;

	R_SetupGL3D();

	if (!(refdef.rdflags & RDF_NOWORLDMODEL)) {
		if (r_threads->integer) {
			while (r_threadstate.state != THREAD_RENDERER)
				Sys_Sleep(0);

			r_threadstate.state = THREAD_CLIENT;
		} else {
			R_SetupFrustum();

			/* draw brushes on current worldlevel */
			R_GetLevelSurfaceLists();
		}
		/* activate wire mode */
		if (r_wire->integer)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		R_EnableLights();

		R_CheckError();

		for (tile = 0; tile < r_numMapTiles; tile++) {
			R_DrawOpaqueSurfaces(r_mapTiles[tile]->bsp.opaque_surfaces);
			R_DrawOpaqueWarpSurfaces(r_mapTiles[tile]->bsp.opaque_warp_surfaces);
			R_DrawAlphaTestSurfaces(r_mapTiles[tile]->bsp.alpha_test_surfaces);

			R_EnableBlend(qtrue);

			R_DrawMaterialSurfaces(r_mapTiles[tile]->bsp.material_surfaces);
			R_DrawBlendSurfaces(r_mapTiles[tile]->bsp.blend_surfaces);
			R_DrawBlendWarpSurfaces(r_mapTiles[tile]->bsp.blend_warp_surfaces);

			R_EnableBlend(qfalse);

			R_DrawBspNormals(tile);
		}
	}

	R_DrawEntities();

	R_CheckError();

	R_EnableBlend(qtrue);

	R_DrawParticles();

	R_CheckError();

	R_EnableBlend(qfalse);

	R_CheckError();

	/* leave wire mode again */
	if (r_wire->integer)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	/* go back into 2D mode for hud and the like */
	R_SetupGL2D();

	R_CheckError();
}

/**
 * @sa R_RenderFrame
 * @sa R_BeginFrame
 */
void R_EndFrame (void)
{
	if (vid_gamma->modified) {
		const float g = vid_gamma->value;
		SDL_SetGamma(g, g, g);
	}

	R_ClearScene();

	SDL_GL_SwapBuffers();
}

static const cmdList_t r_commands[] = {
	{"r_listimages", R_ImageList_f, "Show all loaded images on game console"},
	{"r_listfontcache", R_FontListCache_f, "Show information about font cache"},
	{"r_screenshot", R_ScreenShot_f, "Take a screenshot"},
	{"r_listmodels", R_ModModellist_f, "Show all loaded models on game console"},
	{"r_strings", R_Strings_f, "Print openGL vendor and other strings"},
	{"r_state", R_StatePrint, "Print the gl state to game console"},
	{"r_restartprograms", R_RestartPrograms_f, "Reloads the shaders"},

	{NULL, NULL, NULL}
};

static qboolean R_CvarCheckMaxLightmap (cvar_t *cvar)
{
	if (r_config.maxTextureSize && cvar->integer > r_config.maxTextureSize) {
		Com_Printf("%s exceeded max supported texture size\n", cvar->name);
		Cvar_SetValue(cvar->name, r_config.maxTextureSize);
		return qfalse;
	}

	if (!Q_IsPowerOfTwo(cvar->integer)) {
		Com_Printf("%s must be power of two\n", cvar->name);
		Cvar_SetValue(cvar->name, LIGHTMAP_BLOCK_WIDTH);
		return qfalse;
	}

	return Cvar_AssertValue(cvar, 128, LIGHTMAP_BLOCK_WIDTH, qtrue);
}

static void R_RegisterSystemVars (void)
{
	const cmdList_t *commands;

	r_driver = Cvar_Get("r_driver", "", CVAR_ARCHIVE | CVAR_CONTEXT, "You can define the opengl driver you want to use - empty if you want to use the system default");
	r_drawentities = Cvar_Get("r_drawentities", "1", 0, "Draw the local entities");
	r_drawworld = Cvar_Get("r_drawworld", "1", 0, "Draw the world brushes");
	r_drawspecialbrushes = Cvar_Get("r_drawspecialbrushes", "0", 0, "Draw stuff like actorclip");
	r_isometric = Cvar_Get("r_isometric", "0", CVAR_ARCHIVE, "Draw the world in isometric mode");
	r_nocull = Cvar_Get("r_nocull", "0", 0, "Don't perform culling for brushes and entities");
	r_anisotropic = Cvar_Get("r_anisotropic", "1", CVAR_ARCHIVE, NULL);
	r_texture_lod = Cvar_Get("r_texture_lod", "0", CVAR_ARCHIVE, NULL);
	r_screenshot_format = Cvar_Get("r_screenshot_format", "jpg", CVAR_ARCHIVE, "png, jpg or tga are valid screenshot formats");
	r_screenshot_jpeg_quality = Cvar_Get("r_screenshot_jpeg_quality", "75", CVAR_ARCHIVE, "jpeg quality in percent for jpeg screenshots");
	r_threads = Cvar_Get("r_threads", "0", CVAR_ARCHIVE, "Activate threads for the renderer");

	r_geoscape_overlay = Cvar_Get("r_geoscape_overlay", "0", 0, "Geoscape overlays - Bitmask");
	r_materials = Cvar_Get("r_materials", "1", CVAR_ARCHIVE, "Activate material subsystem");
	r_checkerror = Cvar_Get("r_checkerror", "0", CVAR_ARCHIVE, "Check for opengl errors");
	r_shadows = Cvar_Get("r_shadows", "1", CVAR_ARCHIVE, "Activate or deactivate shadows");
	r_maxtexres = Cvar_Get("r_maxtexres", "2048", CVAR_ARCHIVE | CVAR_IMAGES, "The maximum texture resolution UFO should use");
	r_texturemode = Cvar_Get("r_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE, NULL);
	r_texturealphamode = Cvar_Get("r_texturealphamode", "default", CVAR_ARCHIVE, NULL);
	r_texturesolidmode = Cvar_Get("r_texturesolidmode", "default", CVAR_ARCHIVE, NULL);
	r_wire = Cvar_Get("r_wire", "0", 0, "Draw the scene in wireframe mode");
	r_showbox = Cvar_Get("r_showbox", "0", CVAR_ARCHIVE, "Shows model bounding box");
	r_lightmap = Cvar_Get("r_lightmap", "0", 0, "Draw only the lightmap");
	r_ext_texture_compression = Cvar_Get("r_ext_texture_compression", "0", CVAR_ARCHIVE, NULL);
	r_ext_s3tc_compression = Cvar_Get("r_ext_s3tc_compression", "1", CVAR_ARCHIVE, "Also see r_ext_texture_compression");
	r_intel_hack = Cvar_Get("r_intel_hack", "1", CVAR_ARCHIVE, "Intel cards have activated texture compression until this is set to 0");
	r_vertexbuffers = Cvar_Get("r_vertexbuffers", "0", CVAR_ARCHIVE | CVAR_CONTEXT, "Controls usage of OpenGL Vertex Buffer Objects (VBO) versus legacy vertex arrays.");
	r_maxlightmap = Cvar_Get("r_maxlightmap", "2048", CVAR_ARCHIVE | CVAR_LATCH, "Reduce this value on older hardware");
	Cvar_SetCheckFunction("r_maxlightmap", R_CvarCheckMaxLightmap);

	r_drawbuffer = Cvar_Get("r_drawbuffer", "GL_BACK", 0, NULL);
	r_swapinterval = Cvar_Get("r_swapinterval", "0", CVAR_ARCHIVE | CVAR_CONTEXT, "Controls swap interval synchronization (V-Sync). Values between 0 and 2");
	r_multisample = Cvar_Get("r_multisample", "0", CVAR_ARCHIVE | CVAR_CONTEXT, "Controls multisampling (anti-aliasing). Values between 0 and 4");
	r_lighting = Cvar_Get("r_lighting", "1", CVAR_ARCHIVE, "Activates or deactivates hardware lighting");
	r_warp = Cvar_Get("r_warp", "1", CVAR_ARCHIVE, "Activates or deactivates warping surface rendering");
	r_programs = Cvar_Get("r_programs", "1", CVAR_ARCHIVE, "Use GLSL shaders");
	r_shownormals = Cvar_Get("r_shownormals", "0", CVAR_ARCHIVE, "Show normals on bsp surfaces");
	r_bumpmap = Cvar_Get("r_bumpmap", "1.0", CVAR_ARCHIVE, "Activate bump mapping");
	r_specular = Cvar_Get("r_specular", "1.0", CVAR_ARCHIVE, "Controls specular parameters");
	r_parallax = Cvar_Get("r_parallax", "1.0", CVAR_ARCHIVE, "Controls parallax parameters");

	for (commands = r_commands; commands->name; commands++)
		Cmd_AddCommand(commands->name, commands->function, commands->description);
}

/**
 * @note image cvars
 * @sa R_FilterTexture
 */
static void R_RegisterImageVars (void)
{
	r_brightness = Cvar_Get("r_brightness", "1.5", CVAR_ARCHIVE | CVAR_IMAGES, "Brightness for images");
	r_contrast = Cvar_Get("r_contrast", "1.5", CVAR_ARCHIVE | CVAR_IMAGES, "Contrast for images");
	r_monochrome = Cvar_Get("r_monochrome", "0", CVAR_ARCHIVE | CVAR_IMAGES, "Monochrome world - Bitmask - 1, 2");
	r_invert = Cvar_Get("r_invert", "0", CVAR_ARCHIVE | CVAR_IMAGES, "Invert images - Bitmask - 1, 2");
	if (r_config.hardwareType == GLHW_NVIDIA)
		r_modulate = Cvar_Get("r_modulate", "1.0", CVAR_ARCHIVE | CVAR_IMAGES, "Scale lightmap values");
	else
		r_modulate = Cvar_Get("r_modulate", "2.0", CVAR_ARCHIVE | CVAR_IMAGES, "Scale lightmap values");
	r_soften = Cvar_Get("r_soften", "0", CVAR_ARCHIVE | CVAR_IMAGES, "Apply blur to lightmap");
}

qboolean R_SetMode (void)
{
	Com_Printf("I: setting mode %d:", vid_mode->integer);

	/* store old values if new ones will fail */
	viddef.prev_width = viddef.width;
	viddef.prev_height = viddef.height;
	viddef.prev_fullscreen = viddef.fullscreen;
	viddef.prev_mode = viddef.mode;

	/* new values */
	viddef.mode = vid_mode->integer;
	viddef.fullscreen = vid_fullscreen->integer;
	if (!VID_GetModeInfo()) {
		Com_Printf(" invalid mode\n");
		return qfalse;
	}
	viddef.rx = (float)viddef.width / VID_NORM_WIDTH;
	viddef.ry = (float)viddef.height / VID_NORM_HEIGHT;
	Com_Printf(" %dx%d (fullscreen: %s)\n", viddef.width, viddef.height, viddef.fullscreen ? "yes" : "no");

	if (R_InitGraphics())
		return qtrue;

	Com_Printf("Failed to set video mode %dx%d %s.\n",
			viddef.width, viddef.height,
			(vid_fullscreen->integer ? "fullscreen" : "windowed"));

	Cvar_SetValue("vid_width", viddef.prev_width);  /* failed, revert */
	Cvar_SetValue("vid_height", viddef.prev_height);
	Cvar_SetValue("vid_mode", viddef.prev_mode);
	Cvar_SetValue("vid_fullscreen", viddef.prev_fullscreen);

	viddef.mode = vid_mode->integer;
	viddef.fullscreen = vid_fullscreen->integer;
	if (!VID_GetModeInfo())
		return qfalse;
	viddef.rx = (float)viddef.width / VID_NORM_WIDTH;
	viddef.ry = (float)viddef.height / VID_NORM_HEIGHT;

	return R_InitGraphics();
}

/**
 * @brief Check and load all needed and supported opengl extensions
 * @sa R_Init
 */
static qboolean R_InitExtensions (void)
{
	GLenum err;

	/* multitexture */
	qglActiveTexture = NULL;
	qglClientActiveTexture = NULL;

	/* vertex buffer */
	qglGenBuffers = NULL;
	qglDeleteBuffers = NULL;
	qglBindBuffer = NULL;
	qglBufferData = NULL;

	/* glsl */
	qglCreateShader = NULL;
	qglDeleteShader = NULL;
	qglShaderSource = NULL;
	qglCompileShader = NULL;
	qglGetShaderiv = NULL;
	qglGetShaderInfoLog = NULL;
	qglCreateProgram = NULL;
	qglDeleteProgram = NULL;
	qglAttachShader = NULL;
	qglDetachShader = NULL;
	qglLinkProgram = NULL;
	qglUseProgram = NULL;
	qglGetProgramiv = NULL;
	qglGetProgramInfoLog = NULL;
	qglGetUniformLocation = NULL;
	qglUniform1i = NULL;
	qglUniform1f = NULL;
	qglUniform3fv = NULL;
	qglUniform4fv = NULL;

	/* multitexture */
	if (strstr(r_config.extensionsString, "GL_ARB_multitexture")) {
		qglActiveTexture = SDL_GL_GetProcAddress("glActiveTexture");
		qglClientActiveTexture = SDL_GL_GetProcAddress("glClientActiveTexture");
	}

	if (strstr(r_config.extensionsString, "GL_ARB_texture_compression")) {
		if (r_ext_texture_compression->integer) {
			Com_Printf("using GL_ARB_texture_compression\n");
			if (r_ext_s3tc_compression->integer && strstr(r_config.extensionsString, "GL_EXT_texture_compression_s3tc")) {
				r_config.gl_compressed_solid_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				r_config.gl_compressed_alpha_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			} else {
				r_config.gl_compressed_solid_format = GL_COMPRESSED_RGB_ARB;
				r_config.gl_compressed_alpha_format = GL_COMPRESSED_RGBA_ARB;
			}
		}
	}

	/* anisotropy */
	if (strstr(r_config.extensionsString, "GL_EXT_texture_filter_anisotropic")) {
		if (r_anisotropic->integer) {
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r_config.maxAnisotropic);
			R_CheckError();
			if (r_anisotropic->integer > r_config.maxAnisotropic) {
				Com_Printf("max GL_EXT_texture_filter_anisotropic value is %i\n", r_config.maxAnisotropic);
				Cvar_SetValue("r_anisotropic", r_config.maxAnisotropic);
			}

			if (r_config.maxAnisotropic)
				r_config.anisotropic = qtrue;
		}
	}

	if (strstr(r_config.extensionsString, "GL_EXT_texture_lod_bias"))
		r_config.lod_bias = qtrue;

	/* vertex buffer objects */
	if (strstr(r_config.extensionsString, "GL_ARB_vertex_buffer_object")) {
		qglGenBuffers = SDL_GL_GetProcAddress("glGenBuffers");
		qglDeleteBuffers = SDL_GL_GetProcAddress("glDeleteBuffers");
		qglBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
		qglBufferData = SDL_GL_GetProcAddress("glBufferData");
	}

	/* glsl vertex and fragment shaders and programs */
	if (strstr(r_config.extensionsString, "GL_ARB_fragment_shader")) {
		qglCreateShader = SDL_GL_GetProcAddress("glCreateShader");
		qglDeleteShader = SDL_GL_GetProcAddress("glDeleteShader");
		qglShaderSource = SDL_GL_GetProcAddress("glShaderSource");
		qglCompileShader = SDL_GL_GetProcAddress("glCompileShader");
		qglGetShaderiv = SDL_GL_GetProcAddress("glGetShaderiv");
		qglGetShaderInfoLog = SDL_GL_GetProcAddress("glGetShaderInfoLog");
		qglCreateProgram = SDL_GL_GetProcAddress("glCreateProgram");
		qglDeleteProgram = SDL_GL_GetProcAddress("glDeleteProgram");
		qglAttachShader = SDL_GL_GetProcAddress("glAttachShader");
		qglDetachShader = SDL_GL_GetProcAddress("glDetachShader");
		qglLinkProgram = SDL_GL_GetProcAddress("glLinkProgram");
		qglUseProgram = SDL_GL_GetProcAddress("glUseProgram");
		qglGetProgramiv = SDL_GL_GetProcAddress("glGetProgramiv");
		qglGetProgramInfoLog = SDL_GL_GetProcAddress("glGetProgramInfoLog");
		qglGetUniformLocation = SDL_GL_GetProcAddress("glGetUniformLocation");
		qglUniform1i = SDL_GL_GetProcAddress("glUniform1i");
		qglUniform1f = SDL_GL_GetProcAddress("glUniform1f");
		qglUniform3fv = SDL_GL_GetProcAddress("glUniform3fv");
		qglUniform4fv = SDL_GL_GetProcAddress("glUniform4fv");
		qglGetAttribLocation = SDL_GL_GetProcAddress("glGetAttribLocation");

		/* vertex attribute arrays */
		qglEnableVertexAttribArray = SDL_GL_GetProcAddress("glEnableVertexAttribArray");
		qglDisableVertexAttribArray = SDL_GL_GetProcAddress("glDisableVertexAttribArray");
		qglVertexAttribPointer = SDL_GL_GetProcAddress("glVertexAttribPointer");
	}

	/* reset gl error state */
	R_CheckError();

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &r_config.maxTextureUnits);
	Com_Printf("max texture units: %i\n", r_config.maxTextureUnits);
	if (r_config.maxTextureUnits < 2)
		Sys_Error("You need at least 2 texture units to run "GAME_TITLE);

	/* reset gl error state */
	R_CheckError();

	/* check max texture size */
	Com_Printf("max texture size: ");
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &r_config.maxTextureSize);
	/* stubbed or broken drivers may have reported 0 */
	if (r_config.maxTextureSize <= 0)
		r_config.maxTextureSize = 256;

	if ((err = glGetError()) != GL_NO_ERROR) {
		Com_Printf("cannot detect - using %i! (%s)\n", r_config.maxTextureSize, R_TranslateError(err));
		Cvar_SetValue("r_maxtexres", r_config.maxTextureSize);
	} else {
		Com_Printf("detected %d\n", r_config.maxTextureSize);
		if (r_maxtexres->integer > r_config.maxTextureSize) {
			Com_Printf("downgrading from %i\n", r_maxtexres->integer);
			Cvar_SetValue("r_maxtexres", r_config.maxTextureSize);
		/* check for a minimum */
		} else if (r_maxtexres->integer >= 128 && r_maxtexres->integer < r_config.maxTextureSize) {
			Com_Printf("but using %i as requested\n", r_maxtexres->integer);
			r_config.maxTextureSize = r_maxtexres->integer;
		}
	}

	/* multitexture is the only one we absolutely need */
	if (!qglActiveTexture || !qglClientActiveTexture)
		return qfalse;

	return qtrue;
}

/**
 * @brief We need at least opengl version 1.2.1
 */
static inline void R_EnforceVersion (void)
{
	int maj, min, rel;

	sscanf(r_config.versionString, "%d.%d.%d ", &maj, &min, &rel);

	if (maj > 1)
		return;

	if (maj < 1)
		Com_Error(ERR_FATAL, "OpenGL version %s is less than 1.2.1", r_config.versionString);

	if (min > 2)
		return;

	if (min < 2)
		Com_Error(ERR_FATAL, "OpenGL Version %s is less than 1.2.1", r_config.versionString);

	if (rel > 1)
		return;

	if (rel < 1)
		Com_Error(ERR_FATAL, "OpenGL version %s is less than 1.2.1", r_config.versionString);
}

/**
 * @brief Searches vendor and renderer GL strings for the given vendor id
 */
static inline qboolean R_SearchForVendor (const char *vendor)
{
	return Q_stristr(r_config.vendorString, vendor)
		|| Q_stristr(r_config.rendererString, vendor);
}

/**
 * @brief Checks whether we have hardware acceleration
 */
static inline void R_VerifyDriver (void)
{
#ifdef _WIN32
	if (!Q_strcasecmp((const char*)glGetString(GL_RENDERER), "gdi generic"))
		Com_Error(ERR_FATAL, "No hardware acceleration detected.\n"
			"Update your graphic card drivers.");
#endif
	if (r_intel_hack->integer && R_SearchForVendor("Intel")) {
		/* HACK: */
		Com_Printf("Activate texture compression for Intel chips - see cvar r_intel_hack\n");
		Cvar_Set("r_ext_texture_compression", "1");
		r_ext_texture_compression->modified = qfalse;
#define INTEL_TEXTURE_RESOLUTION 1024
		if (r_maxtexres->integer > INTEL_TEXTURE_RESOLUTION) {
			Com_Printf("Set max. texture resolution to %i - see cvar r_intel_hack\n", INTEL_TEXTURE_RESOLUTION);
			Cvar_SetValue("r_maxtexres", INTEL_TEXTURE_RESOLUTION);
		}
		r_ext_texture_compression->modified = qfalse;
		r_config.hardwareType = GLHW_INTEL;
	} else if (R_SearchForVendor("NVIDIA")) {
		r_config.hardwareType = GLHW_NVIDIA;
	} else if (R_SearchForVendor("ATI")) {
		r_config.hardwareType = GLHW_ATI;
	} else {
		r_config.hardwareType = GLHW_GENERIC;
	}
}

qboolean R_Init (void)
{
	R_RegisterSystemVars();

	memset(&r_state, 0, sizeof(r_state));
	memset(&r_locals, 0, sizeof(r_locals));
	memset(&r_config, 0, sizeof(r_config));

	/* some config default values */
	r_config.gl_solid_format = GL_RGB;
	r_config.gl_alpha_format = GL_RGBA;
	r_config.gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
	r_config.gl_filter_max = GL_LINEAR;
	r_config.maxTextureSize = 256;

	/* set our "safe" modes */
	viddef.prev_mode = 6;

	/* initialize OS-specific parts of OpenGL */
	if (!Rimp_Init())
		return qfalse;

	/* get our various GL strings */
	r_config.vendorString = (const char *)glGetString(GL_VENDOR);
	Com_Printf("GL_VENDOR: %s\n", r_config.vendorString);
	r_config.rendererString = (const char *)glGetString(GL_RENDERER);
	Com_Printf("GL_RENDERER: %s\n", r_config.rendererString);
	r_config.versionString = (const char *)glGetString(GL_VERSION);
	Com_Printf("GL_VERSION: %s\n", r_config.versionString);
	r_config.extensionsString = (const char *)glGetString(GL_EXTENSIONS);
	Com_Printf("GL_EXTENSIONS: %s\n", r_config.extensionsString);

	/* sanity checks and card specific hacks */
	R_VerifyDriver();
	R_EnforceVersion();

	R_RegisterImageVars();

	R_InitExtensions();
	R_SetDefaultState();
	R_InitPrograms();
	R_InitImages();
	R_InitMiscTexture();
	R_DrawInitLocal();
	R_SphereInit();
	R_FontInit();

	R_CheckError();

	return qtrue;
}

/**
 * @sa R_Init
 */
void R_Shutdown (void)
{
	const cmdList_t *commands;

	for (commands = r_commands; commands->name; commands++)
		Cmd_RemoveCommand(commands->name);

	R_ShutdownModels();
	R_ShutdownImages();

	R_ShutdownPrograms();
	R_SphereShutdown();
	R_FontShutdown();

	/* shut down OS specific OpenGL stuff like contexts, etc. */
	Rimp_Shutdown();

	if (developer->integer & DEBUG_RENDERER)
		R_StatePrint();

	R_ShutdownThreads();
}
