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
 * ctx.c - save/load functionality for the compressed texture cache
 */

#include <stdio.h>
#include <gl/glew.h>
#include <direct.h>

#include "types.h"
#include "gl.h"
#include "cfg.h"
#include "log.h"
#include "globals.h"

// compressed texture file header
// 'format' field is OpenGL implementation-specific and not well defined in any way
// sharing .ctx files between different setups is not recommended
struct ctx_header
{
	uint size;
	uint format;
	uint width;
	uint height;
};

// save a compressed texture to disk
bool write_ctx(char *filename, uint width, uint height, uint texture)
{
	FILE *f;
	struct ctx_header header;
	char *data;
	GLint tmp;
	char *next = filename;

	glBindTexture(GL_TEXTURE_2D, texture);

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &tmp);

	if(!tmp)
	{
		error("Texture could not be compressed\n");
		return false;
	}
	
	while((next = strchr(next, '/')))
	{
		char tmp[sizeof(basedir) + 1024];
		
		while(next[0] == '/') next++;
		
		strncpy(tmp, filename, next - filename);
		tmp[next - filename] = 0;
		
		if(trace_all) trace("Creating directory %s\n", tmp);
		
		mkdir(tmp);
	}

	if(fopen_s(&f, filename, "wb"))
	{
		error("couldn't open file %s for writing: %s", filename, _strerror(NULL));
		return false;
	}

	header.width = width;
	header.height = height;

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &tmp);
	header.format = tmp;

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &tmp);
	header.size = tmp;

	fwrite(&header, sizeof(header), 1, f);

	data = driver_malloc(header.size);

	glGetCompressedTexImageARB(GL_TEXTURE_2D, 0, data);

	fwrite(data, header.size, 1, f);

	driver_free(data);
	fclose(f);

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &tmp);

	if(trace_all) trace("Texture compression ratio: %i:1\n", (width * height * 4) / tmp);

	stats.ext_cache_size += tmp;

	return true;
}

// load a compressed texture from disk
uint read_ctx(char *filename, uint *width, uint *height)
{
	FILE *f;
	struct ctx_header header;
	char *data;
	GLuint texture;
	GLint tmp;

	if(fopen_s(&f, filename, "rb")) return 0;

	fread(&header, sizeof(header), 1, f);

	data = gl_get_pixel_buffer(header.size);

	fread(data, header.size, 1, f);

	*width = header.width;
	*height = header.height;

	gl_check_texture_dimensions(*width, *height, filename);

	texture = gl_commit_compressed_buffer(data, header.width, header.height, header.format, header.size);

	fclose(f);

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &tmp);

	if(trace_all) trace("Texture compression ratio: %i:1\n", ((*width) * (*height) * 4) / tmp);

	stats.ext_cache_size += tmp;

	return texture;
}
