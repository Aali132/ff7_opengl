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
 * cfg.h - configuration variable definitions
 */

#ifndef _CFG_H_
#define _CFG_H_

#include "types.h"

extern char *mod_path;
extern char *movie_plugin;
extern char *music_plugin;
extern bool save_textures;
extern char *traced_texture;
extern char *vert_source;
extern char *frag_source;
extern char *yuv_source;
extern char *post_source;
extern bool enable_postprocessing;
extern bool trace_all;
extern bool trace_movies;
extern bool trace_fake_dx;
extern bool trace_direct;
extern bool trace_files;
extern bool trace_loaders;
extern bool trace_lights;
extern bool vertex_log;
extern bool show_fps;
extern bool show_stats;
extern uint window_size_x;
extern uint window_size_y;
extern int window_pos_x;
extern int window_pos_y;
extern bool preserve_aspect;
extern bool fullscreen;
extern uint refresh_rate;
extern bool prevent_rounding_errors;
extern uint internal_size_x;
extern uint internal_size_y;
extern bool enable_vsync;
extern uint field_framerate;
extern uint battle_framerate;
extern uint worldmap_framerate;
extern uint menu_framerate;
extern uint chocobo_framerate;
extern uint condor_framerate;
extern uint submarine_framerate;
extern uint gameover_framerate;
extern uint credits_framerate;
extern uint snowboard_framerate;
extern uint highway_framerate;
extern uint coaster_framerate;
extern uint battleswirl_framerate;
extern bool use_new_timer;
extern bool linear_filter;
extern bool transparent_dialogs;
extern bool mdef_fix;
extern bool fancy_transparency;
extern bool compress_textures;
extern uint texture_cache_size;
extern bool use_pbo;
extern bool use_mipmaps;
extern bool skip_frames;
extern bool more_ff7_debug;
extern bool show_applog;
extern bool direct_mode;
extern bool show_missing_textures;
extern bool ff7_popup;
extern bool info_popup;
extern char *load_library;
extern bool opengl_debug;
extern bool movie_sync_debug;

void read_cfg();

#endif
