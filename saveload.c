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
 * saveload.c - load/save routines for modpath texture replacements
 */

#include <sys/stat.h>
#include <stdio.h>
#include <direct.h>
#include <gl/glew.h>

#include "types.h"
#include "globals.h"
#include "log.h"
#include "gl.h"
#include "cfg.h"
#include "compile_cfg.h"
#include "png.h"
#include "ctx.h"
#include "macro.h"

void make_path(char *name)
{
	char *next = name;
	
	while((next = strchr(next, '/')))
	{
		char tmp[128];
		
		while(next[0] == '/') next++;
		
		strncpy(tmp, name, next - name);
		tmp[next - name] = 0;
		
		mkdir(tmp);
	}
}

bool save_texture(void *data, uint width, uint height, uint palette_index, char *name)
{
	char filename[sizeof(basedir) + 1024];
	struct stat dummy;

	_snprintf(filename, sizeof(filename), "%s/mods/%s/%s_%02i.png", basedir, mod_path, name, palette_index);

	make_path(filename);

	if(stat(filename, &dummy)) return write_png(filename, width, height, data);
	else return true;
}

struct ext_cache_data
{
	uint texture;
	uint size;
};

struct ext_cache_entry
{
	char *name;
	uint palette_index;
	uint references;
	time_t last_access;
	struct ext_cache_data data;
};

#define EXT_CACHE_ENTRIES 512

// the texture cache can only hold a certain number of textures in memory
// there is also a configurable hard limit on how much memory can be used by the cache
// once it is full the texture with the oldest access time is evicted
struct ext_cache_entry *ext_cache[EXT_CACHE_ENTRIES];

// retrieve a single entry from the texture cache
struct ext_cache_data *ext_cache_get(char *name, uint palette_index, int refcount)
{
	uint i;

	for(i = 0; i < EXT_CACHE_ENTRIES; i++)
	{
		if(!ext_cache[i]) continue;

		if(ext_cache[i]->palette_index != palette_index) continue;

		if(trace_all) trace("Comparing: %s (%s)\n", ext_cache[i]->name, name);

		if(!_stricmp(ext_cache[i]->name, name))
		{
			if(trace_all) trace("Matched: %s\n", ext_cache[i]->name);

			if(refcount >= 0 || ext_cache[i]->references) ext_cache[i]->references += refcount;

			if(refcount >= 0) qpc_get_time(&ext_cache[i]->last_access);

			return &ext_cache[i]->data;
		}
	}

	return 0;
}

// update the access time of a single entry in the texture cache
void ext_cache_access(struct texture_set *texture_set)
{
	VOBJ(texture_set, texture_set, texture_set);
	VOBJ(tex_header, tex_header, VREF(texture_set, tex_header));

	if(!VREF(texture_set, ogl.external)) return;
	if((uint)VREF(tex_header, file.pc_name) > 32) ext_cache_get(VREF(tex_header, file.pc_name), VREF(texture_set, palette_index), 0);
}

// remove a reference to an entry in the texture cache
void ext_cache_release(struct texture_set *texture_set)
{
	uint i;
	VOBJ(texture_set, texture_set, texture_set);
	VOBJ(tex_header, tex_header, VREF(texture_set, tex_header));

	if(!VREF(texture_set, ogl.external)) return;
	if((uint)VREF(tex_header, file.pc_name) > 32)
	{
		for(i = 0; i < VREF(texture_set, ogl.gl_set->textures); i++)
		{
			if(VREF(texture_set, texturehandle[i]))
			{
				ext_cache_get(VREF(tex_header, file.pc_name), i, -1);
			}
		}
	}
}

// add a new entry to the texture cache
struct ext_cache_data *ext_cache_put(char *name, uint palette_index)
{
	uint i;
	time_t oldest_access_time;
	uint oldest_texture;

	if(stats.ext_cache_size < texture_cache_size * 1024 * 1024)
	{
		for(i = 0; i < EXT_CACHE_ENTRIES; i++)
		{
			if(!ext_cache[i])
			{
				ext_cache[i] = driver_calloc(sizeof(*ext_cache[i]), 1);
				ext_cache[i]->name = driver_malloc(strlen(name) + 1);
				strcpy(ext_cache[i]->name, name);
				ext_cache[i]->palette_index = palette_index;

				ext_cache[i]->references = 1;

				qpc_get_time(&ext_cache[i]->last_access);

				return &ext_cache[i]->data;
			}
		}
	}

	// access time can't be higher than current time
	qpc_get_time(&oldest_access_time);
	oldest_texture = EXT_CACHE_ENTRIES;

	for(i = 0; i < EXT_CACHE_ENTRIES; i++)
	{
		if(!ext_cache[i]) continue;

		if(!ext_cache[i]->references && ext_cache[i]->last_access < oldest_access_time)
		{
			oldest_access_time = ext_cache[i]->last_access;
			oldest_texture = i;
		}
	}

	if(oldest_texture == EXT_CACHE_ENTRIES)
	{
		error("texture cache is full and nothing could be evicted!\n");
		return 0;
	}

	glDeleteTextures(1, &ext_cache[oldest_texture]->data.texture);

	stats.ext_cache_size -= ext_cache[oldest_texture]->data.size;

	ext_cache[oldest_texture]->name = driver_realloc(ext_cache[oldest_texture]->name, strlen(name) + 1);
	strcpy(ext_cache[oldest_texture]->name, name);
	ext_cache[oldest_texture]->palette_index = palette_index;
	memset(&ext_cache[oldest_texture]->data, 0, sizeof(ext_cache[oldest_texture]->data));

	qpc_get_time(&ext_cache[oldest_texture]->last_access);

	return &ext_cache[oldest_texture]->data;
}

uint load_texture_helper(char *png_name, char *ctx_name, uint *width, uint *height, bool use_compression)
{
	uint ret;
	uint *data;

	if(!(use_compression && compress_textures) || !(ret = read_ctx(ctx_name, width, height)))
	{
		data = read_png(png_name, width, height);

		if(!data) return 0;

		gl_check_texture_dimensions(*width, *height, png_name);

		if((use_compression && compress_textures))
		{
			ret = gl_compress_pixel_buffer(data, *width, *height, GL_BGRA);
			if(!write_ctx(ctx_name, *width, *height, ret))
			{
				glDeleteTextures(1, &ret);
				data = read_png(png_name, width, height);
				ret = gl_commit_pixel_buffer(data, *width, *height, GL_BGRA, true);
				stats.ext_cache_size += (*width) * (*height) * 4;
			}
		}
		else
		{
			ret = gl_commit_pixel_buffer(data, *width, *height, GL_BGRA, true);
			stats.ext_cache_size += (*width) * (*height) * 4;
		}
	}

	return ret;
}

uint load_texture(char *name, uint palette_index, uint *width, uint *height, bool use_compression)
{
	char png_name[sizeof(basedir) + 1024];
	char ctx_name[sizeof(basedir) + 1024];
	uint ret;
	struct ext_cache_data *cache_data;

	cache_data = ext_cache_get(name, palette_index, 1);

	if(cache_data && cache_data->texture) return cache_data->texture;

	_snprintf(png_name, sizeof(png_name), "%s/mods/%s/%s_%02i.png", basedir, mod_path, name, palette_index);
	_snprintf(ctx_name, sizeof(ctx_name), "%s/mods/%s/cache/%s_%02i.ctx", basedir, mod_path, name, palette_index);

	ret = load_texture_helper(png_name, ctx_name, width, height, use_compression);

	if(!ret)
	{
		if(palette_index != 0)
		{
			if(show_missing_textures) info("tried to load %s, falling back to palette 0\n", png_name, palette_index);
			return load_texture(name, 0, width, height, use_compression);
		}
		else
		{
			if(show_missing_textures) info("tried to load %s, failed\n", png_name);
			return 0;
		}
	}

	if(trace_all) trace("Created texture: %i\n", ret);

	if(!cache_data) cache_data = ext_cache_put(name, palette_index);

	if(cache_data)
	{
		cache_data->texture = ret;
		cache_data->size += (*width) * (*height) * 4;
	}

	return ret;
}
