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
 * globals.h - global variables
 */

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <windows.h>

#include "types.h"
#include "ff7.h"
#include "ff8.h"

extern uint version;

#define BASEDIR_LENGTH 512
extern char basedir[BASEDIR_LENGTH];

extern uint width;
extern uint height;
extern uint x_offset;
extern uint y_offset;
extern uint output_size_x;
extern uint output_size_y;
extern bool indirect_rendering;

extern struct texture_format *texture_format;

extern struct ff7_externals ff7_externals;
extern struct ff8_externals ff8_externals;
extern struct common_externals common_externals;
extern struct driver_stats stats;

extern char popup_msg[];
extern uint popup_ttl;
extern uint popup_color;

extern HWND hwnd;

extern unsigned char font_map[];
extern struct game_mode modes[];
extern uint num_modes;

extern uint text_colors[];

extern bool ff8;

extern uint frame_counter;

#endif
