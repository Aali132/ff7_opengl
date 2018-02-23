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
 * gl/texture.c - support functions for loading/using OpenGL textures
 */

#include <gl/glew.h>

#include "../types.h"
#include "../log.h"
#include "../gl.h"
#include "../common.h"
#include "../macro.h"
#include "../saveload.h"

// check to make sure we can actually load a given texture
void gl_check_texture_dimensions(uint width, uint height, char *source)
{
	if(width > max_texture_size || height > max_texture_size) error("texture dimensions exceed max texture size, will not be able to load %s\n", source);
}

// create a blank OpenGL texture
GLuint gl_create_empty_texture()
{
	GLuint texture;

	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);

	return texture;
}

// create a simple texture from pixel data, if the size parameter is used it
// will be treated as a compressed texture
GLuint gl_create_texture(void *data, uint width, uint height, uint format, uint internalformat, uint size, bool generate_mipmaps)
{
	GLuint texture = gl_create_empty_texture();

	if(size) glCompressedTexImage2DARB(GL_TEXTURE_2D, 0, format, width, height, 0, size, data);
	else glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

	if(generate_mipmaps) glGenerateMipmapEXT(GL_TEXTURE_2D);

	return texture;
}

/*
 * Pixel Buffer Object (PBO) support
 * Only one pixel buffer can be outstanding at a time, normal workflow is to
 * request a new pixel buffer, fill it with data and commit it ASAP.
 * Uses a circular buffer of PBOs to encourage lazy uploading of data
 */

#define PBO_RING_SIZE 32

uint pbo_ring[PBO_RING_SIZE];
uint pbo_index;
bool commited;

void gl_init_pbo_ring()
{
	glGenBuffers(PBO_RING_SIZE, pbo_ring);
	pbo_index = 0;
	commited = true;
}

void *gl_get_pixel_buffer(uint size)
{
	void *ret;

	if(!use_pbo) return driver_malloc(size);

	if(!commited) error("PBO not commited\n");

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ring[pbo_index]);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_STREAM_DRAW);
	ret = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

	commited = false;

	pbo_index = (pbo_index + 1) % PBO_RING_SIZE;

	return ret;
}

GLuint gl_commit_pixel_buffer_generic(void *data, uint width, uint height, uint format, uint internalformat, uint size, bool generate_mipmaps)
{
	uint ret;

	if(!use_pbo)
	{
		ret = gl_create_texture(data, width, height, format, internalformat, size, generate_mipmaps);
		driver_free(data);
		return ret;
	}

	if(commited) error("PBO already commited\n");

	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

	ret = gl_create_texture(0, width, height, format, internalformat, size, generate_mipmaps);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	commited = true;

	return ret;
}

GLuint gl_commit_pixel_buffer(void *data, uint width, uint height, uint format, bool generate_mipmaps)
{
	return gl_commit_pixel_buffer_generic(data, width, height, format, GL_RGBA8, 0, generate_mipmaps);
}

GLuint gl_compress_pixel_buffer(void *data, uint width, uint height, uint format)
{
	return gl_commit_pixel_buffer_generic(data, width, height, format, GL_COMPRESSED_RGBA, 0, true);
}

GLuint gl_commit_compressed_buffer(void *data, uint width, uint height, uint format, uint size)
{
	return gl_commit_pixel_buffer_generic(data, width, height, format, 0, size, true);
}

// apply OpenGL texture for a certain palette in a texture set, possibly
// replacing an existing texture which will then be unloaded
void gl_replace_texture(struct texture_set *texture_set, uint palette_index, uint new_texture)
{
	VOBJ(texture_set, texture_set, texture_set);

	if(VREF(texture_set, texturehandle[palette_index]) != 0)
	{
		if(VREF(texture_set, ogl.external)) glitch("oops, may have messed up an external texture\n");
		glDeleteTextures(1, VREFP(texture_set, texturehandle[palette_index]));
	}

	VRASS(texture_set, texturehandle[palette_index], new_texture);
}

// upload texture for a texture set from raw pixel data
void gl_upload_texture(struct texture_set *texture_set, uint palette_index, void *image_data, uint format)
{
	GLuint texture;
	uint w, h;
	VOBJ(texture_set, texture_set, texture_set);
	VOBJ(tex_header, tex_header, VREF(texture_set, tex_header));

	if(VREF(texture_set, ogl.external))
	{
		w = VREF(texture_set, ogl.width);
		h = VREF(texture_set, ogl.height);
	}
	else
	{
		w = VREF(tex_header, version) == FB_TEX_VERSION ? VREF(tex_header, fb_tex.w) : VREF(tex_header, tex_format.width);
		h = VREF(tex_header, version) == FB_TEX_VERSION ? VREF(tex_header, fb_tex.h) : VREF(tex_header, tex_format.height);
	}

	gl_check_texture_dimensions(w, h, "unknown");

	texture = gl_commit_pixel_buffer(image_data, w, h, format, false);

	gl_replace_texture(texture_set, palette_index, texture);
}

// prepare texture set for rendering
void gl_bind_texture_set(struct texture_set *_texture_set)
{
	VOBJ(texture_set, texture_set, _texture_set);

	if(VPTR(texture_set))
	{
		VOBJ(tex_header, tex_header, VREF(texture_set, tex_header));

		gl_set_texture(VREF(texture_set, texturehandle[VREF(tex_header, palette_index)]));

		if(VREF(tex_header, version) == FB_TEX_VERSION) current_state.fb_texture = true;
		else current_state.fb_texture = false;

		ext_cache_access(VPTR(texture_set));
	}
	else gl_set_texture(0);

	current_state.texture_set = VPTR(texture_set);
}

// prepare an OpenGL texture for rendering, passing zero to this function will
// disable texturing entirely
void gl_set_texture(GLuint texture)
{
	if(trace_all) trace("set texture %i\n", texture);

	if(texture)
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		glUniform1i(glGetUniformLocation(current_program, "texture"), 1);
	}
	else
	{
		glUniform1i(glGetUniformLocation(current_program, "texture"), 0);
	}

	current_state.texture_handle = texture;
	current_state.texture_set = 0;
}
