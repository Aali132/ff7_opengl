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
 * log.c - logging routines for writing to app.log
 */

#include <stdio.h>
#include <windows.h>
#include <gl/gl.h>

#include "types.h"
#include "globals.h"
#include "log.h"

FILE *app_log;

void open_applog(char *path)
{
	app_log = fopen(path, "w");

	if(!app_log) MessageBoxA(hwnd, "Failed to open log file", "Error", 0);
}

void plugin_trace(const char *fmt, ...)
{
	va_list args;
	char tmp_str[1024];

	va_start(args, fmt);

	vsnprintf(tmp_str, sizeof(tmp_str), fmt, args);

	trace("%s", tmp_str);
}

void plugin_info(const char *fmt, ...)
{
	va_list args;
	char tmp_str[1024];

	va_start(args, fmt);

	vsnprintf(tmp_str, sizeof(tmp_str), fmt, args);

	info("%s", tmp_str);
}

void plugin_glitch(const char *fmt, ...)
{
	va_list args;
	char tmp_str[1024];

	va_start(args, fmt);

	vsnprintf(tmp_str, sizeof(tmp_str), fmt, args);

	glitch("%s", tmp_str);
}

void plugin_error(const char *fmt, ...)
{
	va_list args;
	char tmp_str[1024];

	va_start(args, fmt);

	vsnprintf(tmp_str, sizeof(tmp_str), fmt, args);

	error("%s", tmp_str);
}

void debug_print(const char *str)
{
	char tmp_str[1024];

	sprintf(tmp_str, "[%08i] %s", frame_counter, str);

	fwrite(tmp_str, 1, strlen(tmp_str), app_log);
	fflush(app_log);
}

// filter out some less useful spammy messages
const char ff7_filter[] = "SET VOLUME ";
const char ff8_filter[] = "Patch ";

void external_debug_print(const char *str)
{
	if(!ff8 && !strncmp(str, ff7_filter, sizeof(ff7_filter) - 1)) return;
	if(ff8 && !strncmp(str, ff8_filter, sizeof(ff8_filter) - 1)) return;

	if(show_applog) debug_print(str);

	if(ff7_popup)
	{
		strcpy(popup_msg, str);
		popup_ttl = POPUP_TTL_MAX;
		popup_color = text_colors[TEXTCOLOR_GRAY];
	}
}

void external_debug_print2(const char *fmt, ...)
{
	va_list args;
	char tmp_str[1024];

	va_start(args, fmt);

	vsnprintf(tmp_str, sizeof(tmp_str), fmt, args);
	external_debug_print(tmp_str);
}

#define POPUP_LOG_LENGTH 128

void debug_printf(const char *prefix, bool popup, uint color, const char *fmt, ...)
{
	va_list args;
	char tmp_str[1024];
	char tmp_str2[1024];

	va_start(args, fmt);

	vsnprintf(tmp_str, sizeof(tmp_str), fmt, args);
	_snprintf(tmp_str2, sizeof(tmp_str2), "%s: %s", prefix, tmp_str);
	debug_print(tmp_str2);

	if(popup)
	{
#ifdef RELEASE
		static char *popup_log[POPUP_LOG_LENGTH];
		static uint popup_log_index = 0;
		uint i;

		for(i = 0; i < POPUP_LOG_LENGTH; i++)
		{
			if(popup_log[i] && !strcmp(popup_log[i], tmp_str2)) return;
		}

		if(popup_log[popup_log_index]) free(popup_log[popup_log_index]);

		popup_log[popup_log_index] = strdup(tmp_str2);

		popup_log_index = (popup_log_index + 1) % POPUP_LOG_LENGTH;
#endif

		strcpy(popup_msg, tmp_str2);
		popup_ttl = POPUP_TTL_MAX;
		popup_color = color;
	}
}

void windows_error(uint error)
{
	char tmp_str[200];

	if(FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, error == 0 ? GetLastError() : error, 0, tmp_str, sizeof(tmp_str), 0)) debug_print(tmp_str);
}

void gl_error()
{
	GLenum ret = glGetError();

	while(ret != GL_NO_ERROR)
	{
		switch(ret)
		{
			case GL_INVALID_ENUM:
				error("GL_INVALID_ENUM\n");
				break;
			case GL_INVALID_VALUE:
				error("GL_INVALID_VALUE\n");
				break;
			case GL_INVALID_OPERATION:
				error("GL_INVALID_OPERATION\n");
				break;
			case GL_STACK_OVERFLOW:
				error("GL_STACK_OVERFLOW\n");
				break;
			case GL_STACK_UNDERFLOW:
				error("GL_STACK_UNDERFLOW\n");
				break;
			case GL_OUT_OF_MEMORY:
				error("GL_OUT_OF_MEMORY\n");
				break;
		}

		ret = glGetError();
	}
}
