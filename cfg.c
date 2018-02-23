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
 * cfg.c - configuration file parser based on libconfuse
 */

#include <stdio.h>
#include <string.h>

#include "types.h"
#include "3rdparty/confuse.h"
#include "log.h"
#include "globals.h"
#include "compile_cfg.h"

// configuration variables with their default values
char *mod_path;
char *movie_plugin;
char *music_plugin;
bool save_textures = false;
char *traced_texture;
char *vert_source;
char *frag_source;
char *yuv_source;
char *post_source;
bool enable_postprocessing = false;
bool trace_all = false;
bool trace_movies = false;
bool trace_fake_dx = false;
bool trace_direct = false;
bool trace_files = false;
bool trace_loaders = false;
bool trace_lights = false;
bool vertex_log = false;
bool show_fps = false;
bool show_stats = false;
uint window_size_x = 0;
uint window_size_y = 0;
int window_pos_x = 0;
int window_pos_y = 0;
bool preserve_aspect = true;
bool fullscreen = true;
uint refresh_rate = 0;
bool prevent_rounding_errors = true;
uint internal_size_x = 0;
uint internal_size_y = 0;
bool enable_vsync = true;
uint field_framerate = 30;
uint battle_framerate = 15;
uint worldmap_framerate = 30;
uint menu_framerate = 30;
uint chocobo_framerate = 30;
uint condor_framerate = 30;
uint submarine_framerate = 30;
uint gameover_framerate = 30;
uint credits_framerate = 30;
uint snowboard_framerate = 60;
uint highway_framerate = 30;
uint coaster_framerate = 60;
uint battleswirl_framerate = 30;
bool use_new_timer = true;
bool linear_filter = false;
bool transparent_dialogs = false;
bool mdef_fix = true;
bool fancy_transparency = true;
bool compress_textures = false;
uint texture_cache_size = 256;
bool use_pbo = true;
bool use_mipmaps = true;
bool skip_frames = false;
bool more_ff7_debug = false;
bool show_applog = true;
bool direct_mode = false;
bool show_missing_textures = false;
bool ff7_popup = false;
bool info_popup = false;
char *load_library;
bool opengl_debug = false;
bool movie_sync_debug = false;

cfg_opt_t opts[] = {
		CFG_SIMPLE_STR("mod_path", &mod_path),
		CFG_SIMPLE_STR("movie_plugin", &movie_plugin),
		CFG_SIMPLE_STR("music_plugin", &music_plugin),
		CFG_SIMPLE_BOOL("save_textures", &save_textures),
		CFG_SIMPLE_STR("traced_texture", &traced_texture),
		CFG_SIMPLE_STR("vert_source", &vert_source),
		CFG_SIMPLE_STR("frag_source", &frag_source),
		CFG_SIMPLE_STR("yuv_source", &yuv_source),
		CFG_SIMPLE_STR("post_source", &post_source),
		CFG_SIMPLE_BOOL("enable_postprocessing", &enable_postprocessing),
		CFG_SIMPLE_BOOL("trace_all", &trace_all),
		CFG_SIMPLE_BOOL("trace_movies", &trace_movies),
		CFG_SIMPLE_BOOL("trace_fake_dx", &trace_fake_dx),
		CFG_SIMPLE_BOOL("trace_direct", &trace_direct),
		CFG_SIMPLE_BOOL("trace_files", &trace_files),
		CFG_SIMPLE_BOOL("trace_loaders", &trace_loaders),
		CFG_SIMPLE_BOOL("trace_lights", &trace_lights),
		CFG_SIMPLE_BOOL("vertex_log", &vertex_log),
		CFG_SIMPLE_BOOL("show_fps", &show_fps),
		CFG_SIMPLE_BOOL("show_stats", &show_stats),
		CFG_SIMPLE_INT("window_size_x", &window_size_x),
		CFG_SIMPLE_INT("window_size_y", &window_size_y),
		CFG_SIMPLE_INT("window_pos_x", &window_pos_x),
		CFG_SIMPLE_INT("window_pos_y", &window_pos_y),
		CFG_SIMPLE_BOOL("preserve_aspect", &preserve_aspect),
		CFG_SIMPLE_BOOL("fullscreen", &fullscreen),
		CFG_SIMPLE_INT("refresh_rate", &refresh_rate),
		CFG_SIMPLE_BOOL("prevent_rounding_errors", &prevent_rounding_errors),
		CFG_SIMPLE_INT("internal_size_x", &internal_size_x),
		CFG_SIMPLE_INT("internal_size_y", &internal_size_y),
		CFG_SIMPLE_BOOL("enable_vsync", &enable_vsync),
		CFG_SIMPLE_INT("field_framerate", &field_framerate),
		CFG_SIMPLE_INT("battle_framerate", &battle_framerate),
		CFG_SIMPLE_INT("worldmap_framerate", &worldmap_framerate),
		CFG_SIMPLE_INT("menu_framerate", &menu_framerate),
		CFG_SIMPLE_INT("chocobo_framerate", &chocobo_framerate),
		CFG_SIMPLE_INT("condor_framerate", &condor_framerate),
		CFG_SIMPLE_INT("submarine_framerate", &submarine_framerate),
		CFG_SIMPLE_INT("gameover_framerate", &gameover_framerate),
		CFG_SIMPLE_INT("credits_framerate", &credits_framerate),
		CFG_SIMPLE_INT("snowboard_framerate", &snowboard_framerate),
		CFG_SIMPLE_INT("highway_framerate", &highway_framerate),
		CFG_SIMPLE_INT("coaster_framerate", &coaster_framerate),
		CFG_SIMPLE_INT("battleswirl_framerate", &battleswirl_framerate),
		CFG_SIMPLE_BOOL("use_new_timer", &use_new_timer),
		CFG_SIMPLE_BOOL("linear_filter", &linear_filter),
		CFG_SIMPLE_BOOL("transparent_dialogs", &transparent_dialogs),
		CFG_SIMPLE_BOOL("mdef_fix", &mdef_fix),
		CFG_SIMPLE_BOOL("fancy_transparency", &fancy_transparency),
		CFG_SIMPLE_BOOL("compress_textures", &compress_textures),
		CFG_SIMPLE_INT("texture_cache_size", &texture_cache_size),
		CFG_SIMPLE_BOOL("use_pbo", &use_pbo),
		CFG_SIMPLE_BOOL("use_mipmaps", &use_mipmaps),
		CFG_SIMPLE_BOOL("skip_frames", &skip_frames),
		CFG_SIMPLE_BOOL("more_ff7_debug", &more_ff7_debug),
		CFG_SIMPLE_BOOL("show_applog", &show_applog),
		CFG_SIMPLE_BOOL("direct_mode", &direct_mode),
		CFG_SIMPLE_BOOL("show_missing_textures", &show_missing_textures),
		CFG_SIMPLE_BOOL("ff7_popup", &ff7_popup),
		CFG_SIMPLE_BOOL("info_popup", &info_popup),
		CFG_SIMPLE_STR("load_library", &load_library),
		CFG_SIMPLE_BOOL("opengl_debug", &opengl_debug),
		CFG_SIMPLE_BOOL("movie_sync_debug", &movie_sync_debug),

		CFG_END()
};

void error_callback(cfg_t *cfg, const char *fmt, va_list ap)
{
	char config_error_string[4096];
	char display_string[4096];

	vsnprintf(config_error_string, sizeof(config_error_string), fmt, ap);

	error("parse error in config file\n");
	error("%s\n", config_error_string);
	sprintf(display_string, "You have an error in your config file, some options may not have been parsed.\n(%s)", config_error_string);
	MessageBoxA(hwnd, display_string, "Warning", 0);
}

void read_cfg()
{
	char filename[BASEDIR_LENGTH + 1024];

	cfg_t *cfg;

	mod_path = strdup("");
	if(!ff8) movie_plugin = strdup("plugins/ffmpeg_movies.fgp");
	else movie_plugin = strdup("");
	music_plugin = strdup("");
	vert_source = strdup("shaders/main.vert");
	frag_source = strdup("shaders/main.frag");
	yuv_source = strdup("shaders/yuv.frag");
	post_source = strdup("");

	traced_texture = strdup("");

	load_library = strdup("");

	if(!ff8) _snprintf(filename, sizeof(filename), "%s/ff7_opengl.cfg", basedir);
	else _snprintf(filename, sizeof(filename), "%s/ff8_opengl.cfg", basedir);
	
	cfg = cfg_init(opts, 0);

	cfg_set_error_function(cfg, error_callback);

	cfg_parse(cfg, filename);

	cfg_free(cfg);

#ifdef SINGLE_STEP
	window_size_x = 0;
	window_size_y = 0;
	fullscreen = false;
#endif
}
