/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// snd_dma.c -- main control for any streaming sound output device

#include "quakedef.h"
#include <SDL.h>

void S_Play(void);
void S_PlayVol(void);
void S_SoundList(void);
void S_Update_();
void S_StopAllSounds(qboolean clear);
void S_StopAllSoundsC(void);

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t   channels[MAX_CHANNELS];
int			total_channels;

int				snd_blocked = 0;
static qboolean	snd_ambient = 1;
qboolean		snd_initialized = false;

// pointer should go away
volatile dma_t* shm = 0;
volatile dma_t sn;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
vec_t		sound_nominal_clip_dist = 1000.0;

int			soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS


#define	MAX_SFX		512
sfx_t* known_sfx;		// hunk allocated [MAX_SFX]
int			num_sfx;

sfx_t* ambient_sfx[NUM_AMBIENTS];

int 		desired_speed = 11025;
int 		desired_bits = 16;

int sound_started = 0;

#define WAV_BUFFERS       64
#define WAV_BUFFER_SIZE   1024
#define WAV_MASK          (WAV_BUFFERS - 1)
#define WAV_BUFFER_BYTES  (WAV_BUFFERS * WAV_BUFFER_SIZE)

static unsigned char* wav_buffer = NULL;

static SDL_AudioStream* sdl_audio_stream = NULL;

/* separate audio stream for CD / background music */
static SDL_AudioStream* cd_audio_stream = NULL;

static Uint64 wav_submitted = 0;

static qboolean wav_initialized = false;


cvar_t bgmvolume = { "bgmvolume", "1", true };
cvar_t volume = { "volume", "0.7", true };

cvar_t nosound = { "nosound", "0" };
cvar_t precache = { "precache", "1" };
cvar_t loadas8bit = { "loadas8bit", "0" };
cvar_t bgmbuffer = { "bgmbuffer", "4096" };
cvar_t ambient_level = { "ambient_level", "0.3" };
cvar_t ambient_fade = { "ambient_fade", "100" };
cvar_t snd_noextraupdate = { "snd_noextraupdate", "0" };
cvar_t snd_show = { "snd_show", "0" };
cvar_t _snd_mixahead = { "_snd_mixahead", "0.25", true };


// ====================================================================
// User-setable variables
// ====================================================================


//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

qboolean fakedma = false;
int fakedma_updates = 15;

extern int desired_speed;
extern int desired_bits;

qboolean SNDDMA_Init(void)
{
	SDL_Init(SDL_INIT_AUDIO);

	SDL_AudioSpec spec;

	memset(&sn, 0, sizeof(sn));

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = desired_speed;

	shm->samples = WAV_BUFFER_BYTES / (shm->samplebits / 8);

	shm->submission_chunk = WAV_BUFFER_SIZE / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->soundalive = false;
	shm->gamealive = true;

	/*
	 * SDL3's audio stream API takes an application-side PCM format.
	 * The stream handles conversion to whatever format the audio
	 * device actually uses.
	 */
	memset(&spec, 0, sizeof(spec));

	spec.format = SDL_AUDIO_S16;
	spec.channels = shm->channels;
	spec.freq = shm->speed;

	sdl_audio_stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&spec,
		NULL,
		NULL);

	if (!sdl_audio_stream)
	{
		Con_Printf(
			"SNDDMA_Init: SDL_OpenAudioDeviceStream failed: %s\n",
			SDL_GetError());

		shm = NULL;
		return false;
	}

	wav_buffer = (unsigned char*)malloc(WAV_BUFFER_BYTES);

	if (!wav_buffer)
	{
		Con_Printf("SNDDMA_Init: out of memory for sound buffer\n");

		SDL_DestroyAudioStream(sdl_audio_stream);
		sdl_audio_stream = NULL;

		shm = NULL;
		return false;
	}

	memset(wav_buffer, 0, WAV_BUFFER_BYTES);

	shm->buffer = wav_buffer;

	wav_submitted = 0;

	shm->samplepos = 0;
	shm->soundalive = true;
	shm->gamealive = true;

	wav_initialized = true;

	/*
	 * SDL_OpenAudioDeviceStream starts its device paused.
	 * Resume it after the DMA buffer has been initialized.
	 */
	if (!SDL_ResumeAudioStreamDevice(sdl_audio_stream))
	{
		Con_Printf(
			"SNDDMA_Init: SDL_ResumeAudioStreamDevice failed: %s\n",
			SDL_GetError());

		wav_initialized = false;

		free(wav_buffer);
		wav_buffer = NULL;

		SDL_DestroyAudioStream(sdl_audio_stream);
		sdl_audio_stream = NULL;

		shm = NULL;
		return false;
	}

	Con_Printf(
		"SDL3 sound initialized: %d Hz, %d-bit stereo\n",
		shm->speed,
		shm->samplebits);

	return true;
}



void SNDDMA_Submit(void)
{
	Uint64 submitted;
	Uint64 target;
	int queued;
	int bytes_to_submit;
	int frames_to_submit;
	int buffer_pos;
	int first_bytes;

	if (!wav_initialized || !sdl_audio_stream || !shm)
		return;

	/*
	 * paintedtime is the absolute stereo sample-frame position that
	 * Quake has mixed.
	 */
	target = (Uint64)paintedtime;
	submitted = wav_submitted;

	/*
	 * Don't allow the SDL stream to get arbitrarily far ahead.
	 *
	 * Keep the same roughly-four-block queueing behavior as the
	 * previous waveOut backend.
	 */
	queued = SDL_GetAudioStreamQueued(sdl_audio_stream);

	if (queued < 0)
	{
		Con_Printf(
			"SNDDMA_Submit: SDL_GetAudioStreamQueued failed: %s\n",
			SDL_GetError());
		return;
	}

	if (queued >= WAV_BUFFER_SIZE * 4)
		return;

	/*
	 * Only submit audio that Quake has actually painted.
	 */
	if (submitted >= target)
		return;

	/*
	 * Limit this call to the amount of free queue space.
	 */
	bytes_to_submit =
		(WAV_BUFFER_SIZE * 4) - queued;

	/*
	 * Convert bytes to stereo sample frames.
	 */
	frames_to_submit =
		bytes_to_submit /
		(shm->channels * (shm->samplebits / 8));

	if (frames_to_submit <= 0)
		return;

	if (submitted + (Uint64)frames_to_submit > target)
		frames_to_submit =
		(int)(target - submitted);

	bytes_to_submit =
		frames_to_submit *
		shm->channels *
		(shm->samplebits / 8);

	/*
	 * shm->buffer is a circular DMA buffer. SDL copies the data when
	 * SDL_PutAudioStreamData() is called, so it is safe to reuse the
	 * DMA buffer afterward.
	 */
	buffer_pos =
		(int)((submitted * shm->channels *
			(shm->samplebits / 8)) %
			(shm->samples * (shm->samplebits / 8)));

	/*
	 * Handle wrapping around the DMA buffer.
	 */
	first_bytes =
		(shm->samples * (shm->samplebits / 8)) - buffer_pos;

	if (first_bytes > bytes_to_submit)
		first_bytes = bytes_to_submit;

	if (!SDL_PutAudioStreamData(
		sdl_audio_stream,
		(unsigned char*)shm->buffer + buffer_pos,
		first_bytes))
	{
		Con_Printf(
			"SNDDMA_Submit: SDL_PutAudioStreamData failed: %s\n",
			SDL_GetError());
		return;
	}

	if (first_bytes < bytes_to_submit)
	{
		if (!SDL_PutAudioStreamData(
			sdl_audio_stream,
			(unsigned char*)shm->buffer,
			bytes_to_submit - first_bytes))
		{
			Con_Printf(
				"SNDDMA_Submit: SDL_PutAudioStreamData failed: %s\n",
				SDL_GetError());
			return;
		}
	}

	wav_submitted += frames_to_submit;
}



int SNDDMA_GetDMAPos(void)
{
	int queued;
	Uint64 played;
	int samplepos;

	if (!wav_initialized || !sdl_audio_stream || !shm)
		return 0;

	queued = SDL_GetAudioStreamQueued(sdl_audio_stream);

	if (queued < 0)
		return shm->samplepos;

	played =
		wav_submitted -
		((Uint64)queued /
			(shm->channels * (shm->samplebits / 8)));

	samplepos =
		(int)((played * shm->channels) %
			(Uint64)shm->samples);

	shm->samplepos = samplepos;

	return samplepos;
}

void SNDDMA_Shutdown(void)
{
	if (!wav_initialized)
		return;

	wav_initialized = false;

	if (sdl_audio_stream)
	{
		SDL_ClearAudioStream(sdl_audio_stream);
		SDL_DestroyAudioStream(sdl_audio_stream);
		sdl_audio_stream = NULL;
	}

	if (wav_buffer)
	{
		free(wav_buffer);
		wav_buffer = NULL;
	}

	wav_submitted = 0;

	shm = NULL;
}

void S_AmbientOff(void)
{
	snd_ambient = false;
}


void S_AmbientOn(void)
{
	snd_ambient = true;
}


void S_SoundInfo_f(void)
{
	if (!sound_started || !shm)
	{
		Con_Printf("sound system not started\n");
		return;
	}

	Con_Printf("%5d stereo\n", shm->channels - 1);
	Con_Printf("%5d samples\n", shm->samples);
	Con_Printf("%5d samplepos\n", shm->samplepos);
	Con_Printf("%5d samplebits\n", shm->samplebits);
	Con_Printf("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf("%5d speed\n", shm->speed);
	Con_Printf("0x%x dma buffer\n", shm->buffer);
	Con_Printf("%5d total_channels\n", total_channels);
}


/*
================
S_Startup
================
*/

void S_Startup(void)
{
	int		rc;

	if (!snd_initialized)
		return;

	if (!fakedma)
	{
		rc = SNDDMA_Init();

		if (!rc)
		{
			Con_Printf("S_Startup: SNDDMA_Init failed.\n");
			sound_started = 0;
			return;
		}
	}

	sound_started = 1;
}


/*
================
S_Init
================
*/
void S_Init(void)
{

	Con_Printf("\nSound Initialization\n");

	if (COM_CheckParm("-nosound"))
		return;

	if (COM_CheckParm("-simsound"))
		fakedma = true;

	Cmd_AddCommand("play", S_Play);
	Cmd_AddCommand("playvol", S_PlayVol);
	Cmd_AddCommand("stopsound", S_StopAllSoundsC);
	Cmd_AddCommand("soundlist", S_SoundList);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&precache);
	Cvar_RegisterVariable(&loadas8bit);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&bgmbuffer);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&_snd_mixahead);

	if (host_parms.memsize < 0x800000)
	{
		Cvar_Set("loadas8bit", "1");
		Con_Printf("loading all sounds as 8bit\n");
	}



	snd_initialized = true;

	S_Startup();

	SND_InitScaletable();

	known_sfx = Hunk_AllocName(MAX_SFX * sizeof(sfx_t), "sfx_t");
	num_sfx = 0;

	// create a piece of DMA memory

	if (fakedma)
	{
		shm = (void*)Hunk_AllocName(sizeof(*shm), "shm");
		shm->splitbuffer = 0;
		shm->samplebits = 16;
		shm->speed = 22050;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;
		shm->submission_chunk = 1;
		shm->buffer = Hunk_AllocName(1 << 16, "shmbuf");
	}

	Con_Printf("Sound sampling rate: %i\n", shm->speed);

	// provides a tick sound until washed clean

//	if (shm->buffer)
//		shm->buffer[4] = shm->buffer[5] = 0x7f;	// force a pop for debugging

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound("ambience/wind2.wav");

	S_StopAllSounds(true);
}

void CDHelpMe(void* audio, int size) {
	Uint8* data = (Uint8*)audio;
	int data_offset = 0, channels = 0, freq = 0, bits = 0;
	Uint32 data_len = 0;

	int pos = 12;
	while (pos + 8 <= size) {
		char chunk[5];
		Uint32 chunk_size;

		memcpy(chunk, data + pos, 4);
		chunk[4] = '\0';
		chunk_size = (Uint32)data[pos + 4] | ((Uint32)data[pos + 5] << 8) | ((Uint32)data[pos + 6] << 16) | ((Uint32)data[pos + 7] << 24);

		if (pos + 8 + (int)chunk_size > size)
			break;

		if (memcmp(chunk, "fmt ", 4) == 0) {
			if (chunk_size < 16)
				break;
			Uint16 audio_format = (Uint16)(data[pos + 8] | (data[pos + 9] << 8));
			channels = data[pos + 10] | (data[pos + 11] << 8);
			freq = data[pos + 12] | (data[pos + 13] << 8) | (data[pos + 14] << 16) | (data[pos + 15] << 24);
			bits = data[pos + 22] | (data[pos + 23] << 8);
		}
		else if (memcmp(chunk, "data", 4) == 0) {
			data_offset = pos + 8;
			data_len = chunk_size;
			break;
		}
		pos += 8 + ((chunk_size + 1) & ~1);
	}

	if (cd_audio_stream) {
		SDL_ClearAudioStream(cd_audio_stream);
		SDL_DestroyAudioStream(cd_audio_stream);
		cd_audio_stream = NULL;
	}

	SDL_AudioSpec spec;
	memset(&spec, 0, sizeof(spec));
	spec.format = (bits == 8) ? SDL_AUDIO_U8 : SDL_AUDIO_S16;
	spec.channels = channels;
	spec.freq = freq;

	cd_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	SDL_PutAudioStreamData(cd_audio_stream, (unsigned char*)data + data_offset, data_len);
	SDL_ResumeAudioStreamDevice(cd_audio_stream);
	free(audio);
}

// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown(void)
{
	if (!sound_started)
		return;

	if (shm)
		shm->gamealive = 0;

	if (!fakedma)
	{
		SNDDMA_Shutdown();
	}
	else
	{
		shm = 0;
	}

	sound_started = 0;
}



// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t* S_FindName(char* name)
{
	int		i;
	sfx_t* sfx;

	if (!name)
		Sys_Error("S_FindName: NULL\n");

	if (Q_strlen(name) >= MAX_QPATH)
		Sys_Error("Sound name too long: %s", name);

	// see if already loaded
	for (i = 0; i < num_sfx; i++)
		if (!Q_strcmp(known_sfx[i].name, name))
		{
			return &known_sfx[i];
		}

	if (num_sfx == MAX_SFX)
		Sys_Error("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strcpy(sfx->name, name);

	num_sfx++;

	return sfx;
}


/*
==================
S_TouchSound

==================
*/
void S_TouchSound(char* name)
{
	sfx_t* sfx;

	if (!sound_started)
		return;

	sfx = S_FindName(name);
	Cache_Check(&sfx->cache);
}

/*
==================
S_PrecacheSound

==================
*/
sfx_t* S_PrecacheSound(char* name)
{
	sfx_t* sfx;

	if (!sound_started || nosound.value)
		return NULL;

	sfx = S_FindName(name);

	// cache it in
	if (precache.value)
		S_LoadSound(sfx);

	return sfx;
}


//=============================================================================

/*
=================
SND_PickChannel
=================
*/
channel_t* SND_PickChannel(int entnum, int entchannel)
{
	int ch_idx;
	int first_to_die;
	int life_left;

	// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++)
	{
		if (entchannel != 0		// channel 0 never overrides
			&& channels[ch_idx].entnum == entnum
			&& (channels[ch_idx].entchannel == entchannel || entchannel == -1))
		{	// allways override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.viewentity && entnum != cl.viewentity && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
		channels[first_to_die].sfx = NULL;

	return &channels[first_to_die];
}

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize(channel_t* ch)
{
	vec_t dot;
	vec_t ldist, rdist, dist;
	vec_t lscale, rscale, scale;
	vec3_t source_vec;
	sfx_t* snd;

	// anything coming from the view entity will allways be full volume
	if (ch->entnum == cl.viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	// calculate stereo seperation and distance attenuation

	snd = ch->sfx;
	VectorSubtract(ch->origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec) * ch->dist_mult;

	dot = DotProduct(listener_right, source_vec);

	if (shm->channels == 1)
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int)(ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int)(ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}


// =======================================================================
// Start a sound effect
// =======================================================================

void S_StartSound(int entnum, int entchannel, sfx_t* sfx, vec3_t origin, float fvol, float attenuation)
{
	channel_t* target_chan, * check;
	sfxcache_t* sc;
	int		vol;
	int		ch_idx;
	int		skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (nosound.value)
		return;

	vol = fvol * 255;

	// pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

	// spatialize
	memset(target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;		// not audible at all

	// new channel
	sc = S_LoadSound(sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + sc->length;

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = rand() % (int)(0.1 * shm->speed);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}

	}
}

void S_StopSound(int entnum, int entchannel)
{
	int i;

	for (i = NUM_AMBIENTS;
		i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS;
		i++)
	{
		if (channels[i].entnum == entnum &&
			channels[i].entchannel == entchannel)
		{
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}


void S_StopAllSounds(qboolean clear)
{
	int		i;

	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i = 0; i < MAX_CHANNELS; i++)
		if (channels[i].sfx)
			channels[i].sfx = NULL;

	Q_memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (clear)
		S_ClearBuffer();
}

void S_StopAllSoundsC(void)
{
	S_StopAllSounds(true);
}

void S_ClearBuffer(void)
{
	int		clear;

	if (!sound_started || !shm || !shm->buffer)
		return;

	if (shm->samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	Q_memset(shm->buffer, clear, shm->samples * shm->samplebits / 8);
}


/*
=================
S_StaticSound
=================
*/
void S_StaticSound(sfx_t* sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t* ss;
	sfxcache_t* sc;

	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Con_Printf("total_channels == MAX_CHANNELS\n");
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	sc = S_LoadSound(sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Con_Printf("Sound %s not looped\n", sfx->name);
		return;
	}

	ss->sfx = sfx;
	VectorCopy(origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation / 64) / sound_nominal_clip_dist;
	ss->end = paintedtime + sc->length;

	SND_Spatialize(ss);
}


//=============================================================================

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds(void)
{
	mleaf_t* l;
	float		vol;
	int			ambient_channel;
	channel_t* chan;

	if (!snd_ambient)
		return;

	// calc ambient sound levels
	if (!cl.worldmodel)
		return;

	l = Mod_PointInLeaf(listener_origin, cl.worldmodel);
	if (!l || !ambient_level.value)
	{
		for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
	{
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = ambient_level.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += host_frametime * ambient_fade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= host_frametime * ambient_fade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}


/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i, j;
	int			total;
	channel_t* ch;
	channel_t* combine;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds();

	combine = NULL;

	// update spatialization for static and dynamic sounds	
	ch = channels + NUM_AMBIENTS;
	for (i = NUM_AMBIENTS; i < total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame

		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
			// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
			// search for one
			combine = channels + MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; j < i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;

			if (j == total_channels)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}


	}

	//
	// debugging output
	//
	if (snd_show.value)
	{
		total = 0;
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol))
			{
				//Con_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}

		Con_Printf("----(%i)----\n", total);
	}

	// mix some sound
	S_Update_();
}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	fullsamples = shm->samples / shm->channels;

	samplepos = SNDDMA_GetDMAPos();


	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped

		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds(true);
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / shm->channels;
}

void S_ExtraUpdate(void)
{
	if (snd_noextraupdate.value)
		return;		// don't pollute timings
	S_Update_();
}

void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

	if (!sound_started || (snd_blocked > 0))
		return;

	// Updates DMA time
	GetSoundtime();

	// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		//Con_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	// mix ahead of current position
	endtime = soundtime + _snd_mixahead.value * shm->speed;
	samps = shm->samples >> (shm->channels - 1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	S_PaintChannels(endtime);

	SNDDMA_Submit();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play(void)
{
	static int hash = 345;
	int 	i;
	char name[256];
	sfx_t* sfx;

	i = 1;
	while (i < Cmd_Argc())
	{
		if (!Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strcpy(name, Cmd_Argv(i));
			Q_strcat(name, ".wav");
		}
		else
			Q_strcpy(name, Cmd_Argv(i));
		sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
}

void S_PlayVol(void)
{
	static int hash = 543;
	int i;
	float vol;
	char name[256];
	sfx_t* sfx;

	i = 1;
	while (i < Cmd_Argc())
	{
		if (!Q_strrchr(Cmd_Argv(i), '.'))
		{
			Q_strcpy(name, Cmd_Argv(i));
			Q_strcat(name, ".wav");
		}
		else
			Q_strcpy(name, Cmd_Argv(i));
		sfx = S_PrecacheSound(name);
		vol = Q_atof(Cmd_Argv(i + 1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 1.0);
		i += 2;
	}
}

void S_SoundList(void)
{
	int		i;
	sfx_t* sfx;
	sfxcache_t* sc;
	int		size, total;

	total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++)
	{
		sc = Cache_Check(&sfx->cache);
		if (!sc)
			continue;
		size = sc->length * sc->width * (sc->stereo + 1);
		total += size;
		if (sc->loopstart >= 0)
			Con_Printf("L");
		else
			Con_Printf(" ");
		Con_Printf("(%2db) %6i : %s\n", sc->width * 8, size, sfx->name);
	}
	Con_Printf("Total resident: %i\n", total);
}


void S_LocalSound(char* sound)
{
	sfx_t* sfx;

	if (nosound.value)
		return;
	if (!sound_started)
		return;

	sfx = S_PrecacheSound(sound);
	if (!sfx)
	{
		Con_Printf("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound(cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}


void S_ClearPrecache(void)
{
}


void S_BeginPrecaching(void)
{
}


void S_EndPrecaching(void)
{
}

