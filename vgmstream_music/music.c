#include <windows.h>
#include <dsound.h>
#include <vgmstream.h>
#include <math.h>
#include <process.h>

#include "types.h"

static CRITICAL_SECTION mutex;

#define AUDIO_BUFFER_SIZE 5

static IDirectSound **directsound;

const char *basedir;

// logging functions, printf-style, trace output is not visible in a release build of the driver
void (*trace)(char *, ...);
void (*info)(char *, ...);
void (*glitch)(char *, ...);
void (*error)(char *, ...);

static VGMSTREAM *vgmstream[100];
static uint current_id;
static uint sound_buffer_size;
static IDirectSoundBuffer *sound_buffer;
static uint write_pointer;
static uint bytes_written;
static uint bytespersample;
static uint end_pos;

static bool song_ended = true;

static int trans_step;
static int trans_counter;
static int trans_volume;

static int crossfade_time;
static uint crossfade_id;
static char *crossfade_midi;

static int master_volume;
static int song_volume;

void apply_volume()
{
	if(sound_buffer && *directsound)
	{
		int volume = (((song_volume * 100) / 127) * master_volume) / 100;
		float decibel = 20.0f * log10f(volume / 100.0f);

		IDirectSoundBuffer_SetVolume(sound_buffer, volume ? (int)(decibel * 100.0f) : DSBVOLUME_MIN);
	}
}

void buffer_bytes(uint bytes)
{
	if(sound_buffer && bytes)
	{
		sample *ptr1;
		sample *ptr2;
		uint bytes1;
		uint bytes2;
		sample *buffer = malloc(bytes);

		if(vgmstream[current_id]->loop_flag) render_vgmstream(buffer, bytes / bytespersample, vgmstream[current_id]);
		else
		{
			uint render_bytes = (vgmstream[current_id]->num_samples - vgmstream[current_id]->current_sample) * bytespersample;

			if(render_bytes >= bytes) render_vgmstream(buffer, bytes / bytespersample, vgmstream[current_id]);
			if(render_bytes < bytes)
			{
				render_vgmstream(buffer, render_bytes / bytespersample, vgmstream[current_id]);

				memset(&buffer[render_bytes / sizeof(sample)], 0, bytes - render_bytes);
			}
		}

		if(IDirectSoundBuffer_Lock(sound_buffer, write_pointer, bytes, &ptr1, &bytes1, &ptr2, &bytes2, 0)) error("couldn't lock sound buffer\n");

		memcpy(ptr1, buffer, bytes1);
		memcpy(ptr2, &buffer[bytes1 / sizeof(sample)], bytes2);

		if(IDirectSoundBuffer_Unlock(sound_buffer, ptr1, bytes1, ptr2, bytes2)) error("couldn't unlock sound buffer\n");

		write_pointer = (write_pointer + bytes1 + bytes2) % sound_buffer_size;
		bytes_written += bytes1 + bytes2;

		free(buffer);
	}
}

void cleanup()
{
	if(sound_buffer && *directsound) IDirectSoundBuffer_Release(sound_buffer);

	sound_buffer = 0;
}

void load_song(char *midi, uint id)
{
	char tmp[512];
	WAVEFORMATEX sound_format;
	DSBUFFERDESC1 sbdesc;

	cleanup();

	if(!id)
	{
		current_id = 0;
		return;
	}

	sprintf(tmp, "%s/music/vgmstream/%s.ogg", basedir, midi);

	if(!vgmstream[id])
	{
		vgmstream[id] = init_vgmstream(tmp);

		if(!vgmstream[id])
		{
			error("couldn't open music file: %s\n", tmp);
			return;
		}
	}

	sound_format.cbSize = sizeof(sound_format);
	sound_format.wBitsPerSample = 16;
	sound_format.nChannels = vgmstream[id]->channels;
	sound_format.nSamplesPerSec = vgmstream[id]->sample_rate;
	sound_format.nBlockAlign = sound_format.nChannels * sound_format.wBitsPerSample / 8;
	sound_format.nAvgBytesPerSec = sound_format.nSamplesPerSec * sound_format.nBlockAlign;
	sound_format.wFormatTag = WAVE_FORMAT_PCM;

	sound_buffer_size = sound_format.nAvgBytesPerSec * AUDIO_BUFFER_SIZE;

	sbdesc.dwSize = sizeof(sbdesc);
	sbdesc.lpwfxFormat = &sound_format;
	sbdesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY;
	sbdesc.dwReserved = 0;
	sbdesc.dwBufferBytes = sound_buffer_size;

	if(IDirectSound_CreateSoundBuffer(*directsound, (LPCDSBUFFERDESC)&sbdesc, &sound_buffer, 0))
	{
		error("couldn't create sound buffer (%i, %i)\n", vgmstream[id]->channels, vgmstream[id]->sample_rate);
		sound_buffer = 0;

		return;
	}

	bytespersample = vgmstream[id]->channels * sizeof(sample);
	write_pointer = 0;
	bytes_written = 0;
	song_ended = false;

	if(!vgmstream[id]->loop_flag) end_pos = vgmstream[id]->num_samples * bytespersample;

	current_id = id;

	buffer_bytes(sound_buffer_size);

	apply_volume();

	if(IDirectSoundBuffer_Play(sound_buffer, 0, 0, DSBPLAY_LOOPING)) error("couldn't play sound buffer\n");
}

struct IDirectSoundVtbl vtbl;

ULONG (__stdcall *real_dsound_release)(IDirectSound *);

ULONG __stdcall dsound_release_hook(IDirectSound *this)
{
	trace("directsound release\n");

	EnterCriticalSection(&mutex);

	return real_dsound_release(this);
}

void render_thread(void *parameter)
{
	while(true)
	{
		Sleep(50);

		EnterCriticalSection(&mutex);

		if(*directsound)
		{
			if((*directsound)->lpVtbl->Release != dsound_release_hook)
			{
				real_dsound_release = (*directsound)->lpVtbl->Release;
				memcpy(&vtbl, (*directsound)->lpVtbl, sizeof(*((*directsound)->lpVtbl)));

				(*directsound)->lpVtbl = &vtbl;

				vtbl.Release = dsound_release_hook;
			}

			if(trans_counter > 0)
			{
				song_volume += trans_step;

				apply_volume();

				trans_counter--;

				if(!trans_counter)
				{
					song_volume = trans_volume;

					apply_volume();

					if(crossfade_midi)
					{
						load_song(crossfade_midi, crossfade_id);

						if(crossfade_time)
						{
							trans_volume = 127;
							trans_counter = crossfade_time;
							trans_step = (trans_volume - song_volume) / crossfade_time;
						}
						else
						{
							song_volume = 127;
							apply_volume();
						}

						crossfade_time = 0;
						crossfade_id = 0;
						crossfade_midi = 0;
					}
				}
			}

			if(sound_buffer && *directsound)
			{
				uint play_cursor;
				uint bytes_to_write = 0;

				IDirectSoundBuffer_GetCurrentPosition(sound_buffer, &play_cursor, 0);

				if(!vgmstream[current_id]->loop_flag)
				{
					uint play_pos = ((bytes_written - write_pointer) - sound_buffer_size) + play_cursor;

					if(play_pos > end_pos && !song_ended)
					{
						song_ended = true;

						trace("song ended at %i (%i)\n", play_pos, play_pos / bytespersample);

						IDirectSoundBuffer_Stop(sound_buffer);
					}
				}

				if(write_pointer < play_cursor) bytes_to_write = play_cursor - write_pointer;
				else if(write_pointer > play_cursor) bytes_to_write = (sound_buffer_size - write_pointer) + play_cursor;
				
				buffer_bytes(bytes_to_write);
			}
		}

		LeaveCriticalSection(&mutex);
	}
}

// called once just after the plugin has been loaded, <plugin_directsound> is a pointer to FF7s own directsound pointer
__declspec(dllexport) void music_init(void *plugin_trace, void *plugin_info, void *plugin_glitch, void *plugin_error, IDirectSound **plugin_directsound, const char *plugin_basedir)
{
	trace = plugin_trace;
	info = plugin_info;
	glitch = plugin_glitch;
	error = plugin_error;
	directsound = plugin_directsound;
	basedir = plugin_basedir;

	InitializeCriticalSection(&mutex);

	_beginthread(render_thread, 0, 0);

	info("VGMStream music plugin loaded\n");
}

// start playing some music, <midi> is the name of the MIDI file without the .mid extension
__declspec(dllexport) void play_music(char *midi, uint id)
{
	trace("play music: %s\n", midi);

	EnterCriticalSection(&mutex);
	
	if(id != current_id || song_ended)
	{
		close_vgmstream(vgmstream[id]);
		vgmstream[id] = 0;

		load_song(midi, id);
	}

	LeaveCriticalSection(&mutex);
}

__declspec(dllexport) void stop_music()
{
	EnterCriticalSection(&mutex);

	cleanup();

	song_ended = true;

	LeaveCriticalSection(&mutex);
}

// cross fade to a new song
__declspec(dllexport) void cross_fade_music(char *midi, uint id, int time)
{
	int fade_time = time * 2;

	trace("cross fade music: %s (%i)\n", midi, time);

	EnterCriticalSection(&mutex);

	if(id != current_id || song_ended)
	{
		if(!song_ended && fade_time)
		{
			trans_volume = 0;
			trans_counter = fade_time;
			trans_step = (trans_volume - song_volume) / fade_time;
		}
		else
		{
			trans_volume = 0;
			trans_counter = 1;
			trans_step = 0;
		}

		crossfade_time = fade_time;
		crossfade_id = id;
		crossfade_midi = midi;
	}

	LeaveCriticalSection(&mutex);
}

__declspec(dllexport) void pause_music()
{
	EnterCriticalSection(&mutex);

	if(sound_buffer) IDirectSoundBuffer_Stop(sound_buffer);

	LeaveCriticalSection(&mutex);
}

__declspec(dllexport) void resume_music()
{
	EnterCriticalSection(&mutex);

	if(sound_buffer && !song_ended) IDirectSoundBuffer_Play(sound_buffer, 0, 0, DSBPLAY_LOOPING);

	LeaveCriticalSection(&mutex);
}

// return true if music is playing, false if it isn't
// it's important for some field scripts that this function returns true atleast once when a song has been requested
// 
// even if there's nothing to play because of errors/missing files you cannot return false every time
__declspec(dllexport) bool music_status()
{
	static bool last_status;
	bool status;

	EnterCriticalSection(&mutex);

	status = !song_ended;

	if(!sound_buffer)
	{
		last_status = !last_status;
		return !last_status;
	}

	LeaveCriticalSection(&mutex);

	last_status = status;
	return status;
}

__declspec(dllexport) void set_master_music_volume(int volume)
{
	EnterCriticalSection(&mutex);

	master_volume = volume;

	apply_volume();

	LeaveCriticalSection(&mutex);
}

__declspec(dllexport) void set_music_volume(int volume)
{
	EnterCriticalSection(&mutex);

	song_volume = volume;

	trans_volume = 0;
	trans_counter = 0;
	trans_step = 0;

	apply_volume();

	LeaveCriticalSection(&mutex);
}

// make a volume transition
__declspec(dllexport) void set_music_volume_trans(int volume, int step)
{
	trace("set volume trans: %i (%i)\n", volume, step);

	step /= 4;

	EnterCriticalSection(&mutex);

	if(step < 2)
	{
		trans_volume = 0;
		trans_counter = 0;
		trans_step = 0;
		song_volume = volume;
		apply_volume();
	}
	else
	{
		trans_volume = volume;
		trans_counter = step;
		trans_step = (trans_volume - song_volume) / step;
	}

	LeaveCriticalSection(&mutex);
}

__declspec(dllexport) void set_music_tempo(unsigned char tempo)
{
	uint dstempo;

	EnterCriticalSection(&mutex);

	if(sound_buffer)
	{
		dstempo = (vgmstream[current_id]->sample_rate * (tempo + 480)) / 512;

		IDirectSoundBuffer_SetFrequency(sound_buffer, dstempo);
	}

	LeaveCriticalSection(&mutex);
}
