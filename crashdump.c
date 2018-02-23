/* 
 * ff7_opengl - Complete OpenGL replacement of the Direct3D renderer used in 
 * the original ports of Final Fantasy VII and Final Fantasy VIII for the PC.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * crashdump.c - crash dump & emergency save functionality
 */

#include <windows.h>
#include <stdio.h>
#include <dbghelp.h>

#include "globals.h"
#include "types.h"
#include "log.h"

// FF7 save file checksum, original by dziugo
int ff7_checksum(void* qw)
{
	int i = 0, t, d;
	long r = 0xFFFF, len = 4336;
	long pbit = 0x8000;
	char* b = (char*)qw;

	while(len--)
	{
		t = b[i++];
		r ^= t << 8;

		for(d = 0; d < 8; d++)
		{
			if(r & pbit) r = (r << 1) ^ 0x1021;
			else r <<= 1;
		}

		r &= (1 << 16) - 1;
	}

	return (r ^ 0xFFFF) & 0xFFFF;
}

static const char crash_dmp[] = "crash.dmp";

static const char save_name[] = "\x25" "MERGENCY" "\x00\x33" "AVE" "\xFF";

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS *ep)
{
	static bool had_exception = false;
	HMODULE dbghelp;
	char filename[4096];
	bool save;

	// give up if we crash again inside the exception handler (this function)
	if(had_exception)
	{
		SetUnhandledExceptionFilter(0);
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	had_exception = true;

	// show cursor in case it was hidden
	ShowCursor(true);

	if(!ff8)
	{
		save = MessageBoxA(0, "Oops! Something very bad happened\nWrote crash.dmp to FF7 install dir.\n"
			"Please provide a copy of it along with APP.LOG when reporting this error.\n"
			"Write emergency save to save/crash.ff7?", "Error", MB_YESNO) == IDYES;
	}
	else
	{
		MessageBoxA(0, "Oops! Something very bad happened\nWrote crash.dmp to FF8 install dir.\n"
			"Please provide a copy of it along with APP.LOG when reporting this error.\n", "Error", MB_OK);

		save = false;
	}

	// save crash dump to game directory
	sprintf(filename, "%s/%s", basedir, crash_dmp);
	dbghelp = LoadLibrary("dbghelp.dll");
	if (dbghelp != NULL)
	{
		typedef BOOL (WINAPI *MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE,
				MINIDUMP_TYPE,
				CONST PMINIDUMP_EXCEPTION_INFORMATION,
				CONST PMINIDUMP_USER_STREAM_INFORMATION,
				CONST PMINIDUMP_CALLBACK_INFORMATION);
		MiniDumpWriteDump_t funcMiniDumpWriteDump = (MiniDumpWriteDump_t)GetProcAddress(dbghelp, "MiniDumpWriteDump");
		if (funcMiniDumpWriteDump != NULL) {
			HANDLE file  = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
			HANDLE proc  = GetCurrentProcess();
			DWORD procid = GetCurrentProcessId();
			MINIDUMP_EXCEPTION_INFORMATION mdei;

			mdei.ThreadId = GetCurrentThreadId();
			mdei.ExceptionPointers  = ep;
			mdei.ClientPointers     = false;

			funcMiniDumpWriteDump(proc, procid, file, MiniDumpWithDataSegs | MiniDumpWithPrivateReadWriteMemory, &mdei, NULL, NULL);
		}
		FreeLibrary(dbghelp);
	}

	if(!ff8)
	{
		sprintf(filename, "%s/%s", basedir, "save/crash.ff7");

		// try to dump the current savemap from memory
		// the savemap could be old, inconsistent or corrupted at this point
		// avoid playing from an emergency save if at all possible!
		if(save)
		{
			FILE *f = fopen(filename, "wb");
			uint magic = 0x6277371;
			uint bitmask = 1;
			struct savemap dummy[14];

			memset(dummy, 0, sizeof(dummy));

			memcpy(ff7_externals.savemap->preview_location, save_name, sizeof(save_name));

			ff7_externals.savemap->checksum = ff7_checksum(&(ff7_externals.savemap->preview_level));

			fwrite(&magic, 4, 1, f);
			fwrite("", 1, 1, f);
			fwrite(&bitmask, 4, 1, f);
			fwrite(ff7_externals.savemap, sizeof(*ff7_externals.savemap), 1, f);
			fwrite(dummy, sizeof(dummy), 1, f);
			fclose(f);
		}
	}

	error("unhandled exception\n");

	// let OS handle the crash
	SetUnhandledExceptionFilter(0);
	return EXCEPTION_CONTINUE_EXECUTION;
}
