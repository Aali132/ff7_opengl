/*
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU Lesser General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU Lesser General Public License for more details.
 *
 *	You should have received a copy of the GNU Lesser General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <dsound.h>

#include "types.h"

// logging functions, printf-style, trace output is not visible in a release build of the driver
void (*trace)(char *, ...);
void (*info)(char *, ...);
void (*glitch)(char *, ...);
void (*error)(char *, ...);

// directsound interface, not useful for the FF7Music plugin
IDirectSound **directsound;

uint current_id = 0;

// send a message to be intercepted by FF7Music
void send_ff7music(char *fmt, ...)
{
	va_list args;
	char msg[0x100];

	va_start(args, fmt);

	vsnprintf(msg, sizeof(msg), fmt, args);

	OutputDebugStringA(msg);
}

BOOL APIENTRY DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}

// called once just after the plugin has been loaded, <plugin_directsound> is a pointer to FF7s own directsound pointer
__declspec(dllexport) void music_init(void *plugin_trace, void *plugin_info, void *plugin_glitch, void *plugin_error, IDirectSound **plugin_directsound, const char *plugin_basedir)
{
	trace = plugin_trace;
	info = plugin_info;
	glitch = plugin_glitch;
	error = plugin_error;
	directsound = plugin_directsound;

	info("FF7Music helper plugin loaded\n");
}

// start playing some music, <midi> is the name of the MIDI file without the .mid extension
__declspec(dllexport) void play_music(char *midi, uint id)
{
	if(id == current_id) return;

	send_ff7music("reading midi file: %s.mid\n", midi);

	current_id = id;
}

__declspec(dllexport) void stop_music()
{
	send_ff7music("MIDI stop\n");
}

// cross fade to a new song
__declspec(dllexport) void cross_fade_music(char *midi, uint id, uint time)
{
	if(id == current_id) return;

	stop_music();
	play_music(midi, id);
}

__declspec(dllexport) void pause_music()
{
	
}

__declspec(dllexport) void resume_music()
{
	
}

// return true if music is playing, false if it isn't
// it's important for some field scripts that this function returns true atleast once when a song has been requested
// 
// even if there's nothing to play because of errors/missing files you cannot return false every time
__declspec(dllexport) bool music_status()
{
	static bool dummy = true;

	dummy = !dummy;

	return dummy;
}

__declspec(dllexport) void set_master_music_volume(uint volume)
{
	
}

__declspec(dllexport) void set_music_volume(uint volume)
{
	
}

// make a volume transition
__declspec(dllexport) void set_music_volume_trans(uint volume, uint step)
{
	uint from = volume ? 0 : 127;

	send_ff7music("MIDI set volume trans: %d->%d; step=%d\n", from, volume, step);
}

__declspec(dllexport) void set_music_tempo(unsigned char tempo)
{
	
}
