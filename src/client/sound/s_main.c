/**
 * @file s_main.c
 * @brief Main control for any streaming sound output device.
 */

/*
All original material Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#include "s_local.h"
#include "s_main.h"
#include "s_music.h"
#include "s_sample.h"
#include "s_mix.h"

s_env_t s_env;

cvar_t *snd_volume;
static cvar_t *snd_init;
static cvar_t *snd_rate;

static void S_Restart_f(void);

int audioRate;
int audioChannels;
uint16_t audioFormat;

/**
 * @brief Stop all channels
 */
void S_StopAllSounds (void)
{
	if (!s_env.initialized)
		return;
	Mix_HaltChannel(-1);

	memset(s_env.channels, 0, sizeof(s_env.channels));
}

/**
 * @sa CL_Frame
 */
void S_Frame (void)
{
	int i;
	s_channel_t *ch;

	if (snd_init && snd_init->modified) {
		S_Restart_f();
		snd_init->modified = qfalse;
	}

	if (!s_env.initialized)
		return;

	M_Frame();

	/* update right angle for stereo panning */
	AngleVectors(cl.cam.angles, NULL, s_env.right, NULL);

	/* update spatialization for current sounds */
	ch = s_env.channels;

	for (i = 0; i < MAX_CHANNELS; i++, ch++) {
		if (!ch->sample)
			continue;

		/* reset channel's count for loop samples */
		ch->count = 0;

		S_SpatializeChannel(ch);
	}
}

/**
 * @brief Plays sound fx files via console
 */
static void S_Play_f (void)
{
	int i;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <filename> [<filename> ...]\n", Cmd_Argv(0));
		return;
	}

	i = 1;
	while(i < Cmd_Argc()){
		S_StartLocalSample(Cmd_Argv(i));
		i++;
	}
}

/**
 * @brief Restart the sound subsystem so it can pick up new parameters and flush all sounds
 * @sa S_Shutdown
 * @sa S_Init
 */
static void S_Restart_f (void)
{
	Com_Printf("Restarting sound\n");
	S_Shutdown();
	S_Init();
}

/** @todo This should be removed and read from the dir tree instead */
static const char *soundFileSubDirs[] = {
	"aliens", "ambience", "civilians", "doors", "footsteps", "geoscape", "misc", "soldiers", "weapons", NULL
};


static int S_CompleteSounds (const char *partial, const char **match)
{
	const char *filename;
	int matches = 0;
	char *localMatch[MAX_COMPLETE];
	size_t len = strlen(partial);
	const char **dirList = soundFileSubDirs;
	const char *soundExtensions[] = SAMPLE_TYPES;
	const char **extension = soundExtensions;
	int returnValue;

	/* check for partial matches */
	while (*dirList) {
		while (*extension) {
			char pattern[MAX_OSPATH];
			Com_sprintf(pattern, sizeof(pattern), "sound/%s/*.%s", *dirList, *extension);
			FS_BuildFileList(pattern);
			while ((filename = FS_NextFileFromFileList(pattern)) != NULL) {
				char fileWithPath[MAX_OSPATH];
				Com_sprintf(fileWithPath, sizeof(fileWithPath), "%s/%s", *dirList, filename);
				if (!len) {
					Com_Printf("%s\n", fileWithPath);
				} else if (!strncmp(partial, fileWithPath, len)) {
					Com_Printf("%s\n", fileWithPath);
					localMatch[matches++] = strdup(fileWithPath);
					if (matches >= MAX_COMPLETE)
						break;
				}
			}
			FS_NextFileFromFileList(NULL);
			extension++;
		}
		dirList++;
	}

	returnValue = Cmd_GenericCompleteFunction(len, match, matches, (const char **)localMatch);
	while (--matches >= 0)
		free(localMatch[matches]);
	return returnValue;
}

/**
 * @sa S_Shutdown
 * @sa S_Restart_f
 */
void S_Init (void)
{
	SDL_version version;
	char drivername[MAX_VAR];

	Com_Printf("\n------- sound initialization -------\n");

	memset(&s_env, 0, sizeof(s_env));

	snd_init = Cvar_Get("snd_init", "1", CVAR_ARCHIVE, "Should the sound renderer get initialized");
	snd_init->modified = qfalse; /* don't restart right away */
	Cmd_AddCommand("snd_restart", S_Restart_f, "Restart the sound renderer");

	if (!snd_init->integer) {
		Com_Printf("not initializing.\n");
		return;
	}

	snd_volume = Cvar_Get("snd_volume", "0.7", CVAR_ARCHIVE, "Sound volume - default is 0.7");
	snd_rate = Cvar_Get("snd_rate", "44100", CVAR_ARCHIVE, "Hz value for sound renderer - default is 44100");
	/* set volumes to be changed so they are applied again for next sound/music playing */
	snd_volume->modified = qtrue;

	Cmd_AddCommand("snd_play", S_Play_f, "Plays a sound fx file. Pass path relative to base/sound without file extension");
	Cmd_AddParamCompleteFunction("snd_play", S_CompleteSounds);

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		if (SDL_Init(SDL_INIT_AUDIO) < 0) {
			Com_Printf("S_Init: %s.\n", SDL_GetError());
			return;
		}
	} else if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
			Com_Printf("S_Init: %s.\n", SDL_GetError());
			return;
		}
	}

	MIX_VERSION(&version)
	Com_Printf("SDL_mixer version: %d.%d.%d\n", version.major, version.minor, version.patch);

	if (Mix_OpenAudio((int)snd_rate->value, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		Com_Printf("S_Init: %s\n", Mix_GetError());
		return;
	}

	if (Mix_QuerySpec(&audioRate, &audioFormat, &audioChannels) == 0) {
		Com_Printf("S_Init: %s\n", Mix_GetError());
		return;
	}

	if (SDL_AudioDriverName(drivername, sizeof(drivername)) == NULL)
		strncpy(drivername, "(UNKNOWN)", sizeof(drivername) - 1);
	Com_Printf("... driver: '%s'\n", drivername);

	if (Mix_AllocateChannels(MAX_CHANNELS) != MAX_CHANNELS) {
		Com_Printf("S_Init: %s\n", Mix_GetError());
		return;
	}

	Mix_ChannelFinished(S_FreeChannel);

	Com_Printf("... audio rate: %i\n... audio channels: %i\n", audioRate, audioChannels);

	s_env.initialized = qtrue;

	S_LoadSamples();

	M_Init();
}

/**
 * @sa S_Init
 * @sa S_Restart_f
 */
void S_Shutdown (void)
{
	if (!s_env.initialized)
		return;

	M_Shutdown();

	S_StopAllSounds();

	Mix_AllocateChannels(0);

	S_FreeSamples();

	Mix_CloseAudio();

	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	Mem_FreeTag(cl_soundSysPool, 0);

	Cmd_RemoveCommand("snd_play");
	Cmd_RemoveCommand("snd_restart");

	s_env.initialized = qfalse;
}
