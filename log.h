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
 * log.h - logging routines
 */

#ifndef _LOG_H_
#define _LOG_H_

#include "types.h"
#include "cfg.h"
#include "common.h"
#include "globals.h"

#define glitch_once(x, ...) { static bool glitch_ ## __LINE__ = false; if(!glitch_ ## __LINE__) { glitch(x, __VA_ARGS__); glitch_ ## __LINE__ = true; } }
#define unexpected_once(x, ...) { static bool unexpected_ ## __LINE__ = false; if(!unexpected_ ## __LINE__) { unexpected(x, __VA_ARGS__); unexpected_ ## __LINE__ = true; } }

#define error(x, ...) debug_printf("ERROR", true, text_colors[TEXTCOLOR_RED], (x), __VA_ARGS__)
#define info(x, ...) debug_printf("INFO", info_popup, text_colors[TEXTCOLOR_WHITE], (x), __VA_ARGS__)
#define dump(x, ...) debug_printf("DUMP", false, text_colors[TEXTCOLOR_PINK], (x), __VA_ARGS__)
#ifndef RELEASE
#define trace(x, ...) debug_printf("TRACE", true, text_colors[TEXTCOLOR_GREEN], (x), __VA_ARGS__)
#else
#define trace(x, ...)
#endif
#define glitch(x, ...) debug_printf("GLITCH", true, text_colors[TEXTCOLOR_YELLOW], (x), __VA_ARGS__)
#define unexpected(x, ...) debug_printf("UNEXPECTED", true, text_colors[TEXTCOLOR_LIGHT_BLUE], (x), __VA_ARGS__)

void open_applog(char *path);

void plugin_trace(const char *fmt, ...);
void plugin_info(const char *fmt, ...);
void plugin_glitch(const char *fmt, ...);
void plugin_error(const char *fmt, ...);

void external_debug_print(const char *str);
void external_debug_print2(const char *fmt, ...);

void debug_printf(const char *, bool, uint, const char *, ...);

void windows_error(uint error);
void gl_error();

#endif
