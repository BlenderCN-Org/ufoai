/**
 * @file s_sample.c
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
#include "s_sample.h"

#define SAMPLE_HASH_SIZE 64
static s_sample_t *sampleHash[SAMPLE_HASH_SIZE];

static s_sample_t *S_FindName (const char *name)
{
	s_sample_t *sample;
	const unsigned hash = Com_HashKey(name, SAMPLE_HASH_SIZE);

	for (sample = sampleHash[hash]; sample; sample = sample->hashNext)
		if (!strcmp(name, sample->name))
			return sample;

	return NULL;
}

static Mix_Chunk* S_LoadSampleChunk (const char *sound)
{
	size_t len;
	byte *buf;
	const char *soundExtensions[] = SAMPLE_TYPES;
	const char **extension = soundExtensions;
	SDL_RWops *rw;
	Mix_Chunk *chunk;

	if (!sound || sound[0] == '*')
		return NULL;

	len = strlen(sound);
	if (len + 4 >= MAX_QPATH) {
		Com_Printf("S_LoadSound: MAX_QPATH exceeded for: '%s'\n", sound);
		return NULL;
	}

	while (*extension) {
		if ((len = FS_LoadFile(va("sound/%s.%s", sound, *extension++), &buf)) == -1)
			continue;

		if (!(rw = SDL_RWFromMem(buf, len))){
			FS_FreeFile(buf);
			continue;
		}

		if (!(chunk = Mix_LoadWAV_RW(rw, qfalse)))
			Com_Printf("S_LoadSound: %s.\n", Mix_GetError());

		FS_FreeFile(buf);

		SDL_FreeRW(rw);

		if (chunk)
			return chunk;
	}

	Com_Printf("S_LoadSound: Could not find sound file: '%s'\n", sound);
	return NULL;
}

/**
 * @brief Loads and registers a sound file for later use
 * @param[in] name The name of the soundfile, relative to the sounds dir
 * @sa S_LoadSound
 */
s_sample_t *S_LoadSample (const char *soundFile)
{
	Mix_Chunk *chunk;
	s_sample_t *sample;
	char name[MAX_QPATH];
	unsigned hash;

	if (!s_env.initialized)
		return NULL;

	Com_StripExtension(soundFile, name, sizeof(name));

	sample = S_FindName(name);
	if (sample)
		return sample;

	/* make sure the sound is loaded */
	chunk = S_LoadSampleChunk(name);
	if (!chunk)
		return NULL;		/* couldn't load the sound's data */

	hash = Com_HashKey(name, SAMPLE_HASH_SIZE);
	sample = Mem_PoolAlloc(sizeof(*sample), cl_soundSysPool, 0);
	sample->name = Mem_PoolStrDup(name, cl_soundSysPool, 0);
	sample->chunk = chunk;
	Mix_VolumeChunk(sample->chunk, snd_volume->value * MIX_MAX_VOLUME);
	sample->hashNext = sampleHash[hash];
	sampleHash[hash] = sample;
	return sample;
}

void S_FreeSamples (void)
{
	int i;
	s_sample_t* sample;

	for (i = 0; i < SAMPLE_HASH_SIZE; i++)
		for (sample = sampleHash[i]; sample; sample = sample->hashNext)
			Mix_FreeChunk(sample->chunk);

	for (i = 0; i < SAMPLE_HASH_SIZE; i++)
		for (sample = sampleHash[i]; sample; sample = sample->hashNext)
			Mem_Free(sample);

	memset(sampleHash, 0, sizeof(sampleHash));
}

/**
 * @note Called at precache phase - only load these soundfiles once at startup or on sound restart
 * @sa S_Restart_f
 */
void S_LoadSamples (void)
{
	int i, j, k;

	/* load weapon sounds */
	for (i = 0; i < csi.numODs; i++) { /* i = obj */
		for (j = 0; j < csi.ods[i].numWeapons; j++) {	/* j = weapon-entry per obj */
			for (k = 0; k < csi.ods[i].numFiredefs[j]; j++) { /* k = firedef per wepaon */
				if (csi.ods[i].fd[j][k].fireSound[0] != '\0')
					S_LoadSample(csi.ods[i].fd[j][k].fireSound);
				if (csi.ods[i].fd[j][k].impactSound[0] != '\0')
					S_LoadSample(csi.ods[i].fd[j][k].impactSound);
				if (csi.ods[i].fd[j][k].hitBodySound[0] != '\0')
					S_LoadSample(csi.ods[i].fd[j][k].hitBodySound);
				if (csi.ods[i].fd[j][k].bounceSound[0] != '\0')
					S_LoadSample(csi.ods[i].fd[j][k].bounceSound);
			}
		}
	}

	/* precache the sound pool */
	cls.sound_pool[SOUND_WATER_IN] = S_LoadSample("footsteps/water_in");
	cls.sound_pool[SOUND_WATER_OUT] = S_LoadSample("footsteps/water_out");
	cls.sound_pool[SOUND_WATER_MOVE] = S_LoadSample("footsteps/water_under");
}
