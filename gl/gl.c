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
 * gl/gl.c - main rendering code
 */

#include <windows.h>
#include <gl/glew.h>
#include <stdio.h>
#include <math.h>

#include "../types.h"
#include "../cfg.h"
#include "../common.h"
#include "../gl.h"
#include "../globals.h"
#include "../macro.h"
#include "../log.h"
#include "../matrix.h"

struct matrix d3dviewport_matrix = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

struct driver_state current_state;

GLuint indirect_texture;
GLuint indirect_fbo;
GLuint depthbuffer;

bool yuv_shader_loaded = false;

uint max_texture_size;

extern bool nodefer;

extern uint main_program;
extern uint post_program;
extern uint yuv_program;

extern uint current_program;

// draw a fullscreen quad, respect aspect ratio of source image
void gl_draw_movie_quad_common(int movie_width, int movie_height)
{
	float x1 = 0.0f;
	float x2 = (float)width;
	float y1 = 0.0f;
	float y2 = min((float)width * (float)movie_height / (float)movie_width, (float)height);
	float u1 = 0.0f;
	float u2 = 1.0f;
	float v1 = 0.0f;
	float v2 = 1.0f;
	float z = 1.0f;
	struct nvertex vertices[] = {
		{x2, y1, z, 1.0f, 0xffffffff, 0, u2, v1},
		{x2, y2, z, 1.0f, 0xffffffff, 0, u2, v2},
		{x1, y2, z, 1.0f, 0xffffffff, 0, u1, v2},
		{x1, y1, z, 1.0f, 0xffffffff, 0, u1, v1},
	};
	word indices[] = {0, 1, 2, 3};
	struct game_obj *game_object = common_externals.get_game_object();

	current_state.texture_filter = true;
	internal_set_renderstate(V_DEPTHTEST, 0, game_object);

	gl_draw_indexed_primitive(GL_QUADS, TLVERTEX, vertices, 4, indices, 4, 0, true, false);
}

// draw movie frame from BGRA data, called from movie plugin
void gl_draw_movie_quad_bgra(GLuint movie_texture, int movie_width, int movie_height)
{
	struct driver_state saved_state;

	gl_save_state(&saved_state);

	gl_set_texture(movie_texture);

	gl_draw_movie_quad_common(movie_width, movie_height);

	gl_load_state(&saved_state);
}

// draw movie frame from YUV data, called from movie plugin
void gl_draw_movie_quad_yuv(GLuint *yuv_textures, int movie_width, int movie_height, bool full_range)
{
	struct driver_state saved_state;

	gl_save_state(&saved_state);

	gl_set_texture(yuv_textures[0]);

	if(!yuv_shader_loaded)
	{
		yuv_program = gl_create_program(vert_source, yuv_source, "yuv");
		yuv_shader_loaded = true;
		if(!yuv_program)
		{
			error("failed to load yuv shader\n");
			return;
		}
	}

	if(yuv_program)
	{
		gl_use_yuv_program();
		glUniform1i(glGetUniformLocation(current_program, "full_range"), full_range);
		gl_draw_movie_quad_common(movie_width, movie_height);
		gl_use_main_program();
	}

	gl_load_state(&saved_state);
}

// save complete rendering state to memory
void gl_save_state(struct driver_state *dest)
{
	memcpy(dest, &current_state, sizeof(current_state));
}

// restore complete rendering state from memory
void gl_load_state(struct driver_state *src)
{
	memcpy(&current_state, src, sizeof(current_state));

	gl_bind_texture_set(src->texture_set);
	gl_set_texture(src->texture_handle);
	current_state.texture_set = src->texture_set;
	common_setviewport(src->viewport[0], src->viewport[1], src->viewport[2], src->viewport[3], 0);
	gl_set_blend_func(src->blend_mode);
	internal_set_renderstate(V_WIREFRAME, src->wireframe, 0);
	// setting V_LINEARFILTER has no side effects
	internal_set_renderstate(V_CULLFACE, src->cullface, 0);
	internal_set_renderstate(V_NOCULL, src->nocull, 0);
	internal_set_renderstate(V_DEPTHTEST, src->depthtest, 0);
	internal_set_renderstate(V_DEPTHMASK, src->depthmask, 0);
	internal_set_renderstate(V_ALPHATEST, src->alphatest, 0);
	internal_set_renderstate(V_ALPHAFUNC, src->alphafunc, 0);
	internal_set_renderstate(V_ALPHAREF, src->alpharef, 0);
	glShadeModel(src->shademode ? GL_SMOOTH : GL_FLAT);
	gl_set_world_matrix(&src->world_matrix);
	gl_set_d3dprojection_matrix(&src->d3dprojection_matrix);
}

// draw a set of primitives with a known model->world transformation
// interesting for real-time lighting, maps to normal rendering routine for now
void gl_draw_with_lighting(struct indexed_primitive *ip, bool clip, struct matrix *model_matrix)
{
	gl_draw_indexed_primitive(ip->primitivetype, ip->vertextype, ip->vertices, ip->vertexcount, ip->indices, ip->indexcount, 0, clip, true);
}

// main rendering routine, draws a set of primitives according to the current render state
void gl_draw_indexed_primitive(GLenum primitivetype, uint vertextype, struct nvertex *vertices, uint vertexcount, word *indices, uint count, struct graphics_object *graphics_object, bool clip, bool mipmap)
{
	FILE *log;
	uint i;
	uint mode = getmode_cached()->driver_mode;
	// filter setting can change inside this function, we don't want that to
	// affect the global rendering state so save & restore it
	bool saved_texture_filter = current_state.texture_filter;

	// should never happen, broken 3rd-party models cause this
	if(!count) return;

	// scissor test is used to emulate D3D viewports
	if(clip) glEnable(GL_SCISSOR_TEST);
	else glDisable(GL_SCISSOR_TEST);

	if(vertextype > TLVERTEX)
	{
		unexpected_once("vertextype > TLVERTEX\n");
		return;
	}

	// handle some special cases, see special_case.c
	if(gl_special_case(primitivetype, vertextype, vertices, vertexcount, indices, count, graphics_object, clip, mipmap))
	{
		// special cases can signal back to this function that the draw call has
		// been handled in some other manner
		current_state.texture_filter = saved_texture_filter;
		return;
	}

	// use mipmaps if available
	if(current_state.texture_filter && use_mipmaps && current_state.texture_set)
	{
		VOBJ(texture_set, texture_set, current_state.texture_set);

		if(VREF(texture_set, ogl.external)) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}

	// OpenGL treats texture filtering as a per-texture parameter, we need it
	// to be consistent with our global render state
	if(current_state.texture_filter)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	if(vertex_log)
	{
		log = fopen("vert.log", "ab");

		fprintf(log, "Vertextype: %i\n", vertextype);

		for(i = 0; i < vertexcount; i++)
		{
			fprintf(log, "%f\t\t%f\t\t%f\t\t%f\t\t%f\t\t%f\n", vertices[i]._.x, vertices[i]._.y, vertices[i]._.z, vertices[i].color.w, vertices[i].u, vertices[i].v);
		}

		fclose(log);
	}

	// upload shader uniforms
	if(current_program != 0)
	{
		if(vertextype != TLVERTEX)
		{
			glUniformMatrix4fv(glGetUniformLocation(current_program, "d3dprojection_matrix"), 1, false, &current_state.d3dprojection_matrix.m[0][0]);
			glUniformMatrix4fv(glGetUniformLocation(current_program, "d3dviewport_matrix"), 1, false, &d3dviewport_matrix.m[0][0]);
		}

		glUniform1i(glGetUniformLocation(current_program, "vertextype"), vertextype);
		glUniform1i(glGetUniformLocation(current_program, "fb_texture"), current_state.fb_texture);

		glUniform1i(glGetUniformLocation(current_program, "modulate_alpha"), true);

		if(ff8 && current_state.fb_texture) glUniform1i(glGetUniformLocation(current_program, "modulate_alpha"), false);
	}

	// upload vertex data
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(*vertices), &vertices[0].color.color);
	glVertexPointer(vertextype == TLVERTEX ? 4 : 3, GL_FLOAT, sizeof(*vertices), &vertices[0]._);
	glTexCoordPointer(2, GL_FLOAT, sizeof(*vertices), &vertices[0].u);
	glDrawElements(primitivetype, count, GL_UNSIGNED_SHORT, indices);

#ifdef SINGLE_STEP
	glFinish();
#endif

	stats.vertex_count += count;

	current_state.texture_filter = saved_texture_filter;
}

void gl_set_world_matrix(struct matrix *matrix)
{
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(&matrix->m[0][0]);
	memcpy(&current_state.world_matrix, matrix, sizeof(struct matrix));
}

void gl_set_d3dprojection_matrix(struct matrix *matrix)
{
	memcpy(&current_state.d3dprojection_matrix, matrix, sizeof(struct matrix));
}

// apply blend mode to OpenGL state
void gl_set_blend_func(uint blend_mode)
{
	if(trace_all) trace("set blend mode %i\n", blend_mode);

	glUniform1i(glGetUniformLocation(current_program, "blend_mode"), blend_mode);

	current_state.blend_mode = blend_mode;

	glBlendEquation(GL_FUNC_ADD);

	switch(blend_mode)
	{
		case BLEND_AVG:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case BLEND_ADD:
			glBlendFunc(GL_ONE, GL_ONE);
			break;
		case BLEND_SUB:
			glBlendFunc(GL_ONE, GL_ONE);
			glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
			break;
		case BLEND_25P:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;
		case BLEND_NONE:
			if(fancy_transparency) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			else glBlendFunc(GL_ONE, GL_ZERO);
			break;

		default:
			unexpected("Unknown blend mode %i\n", blend_mode);
	}
}

// draw text on screen using the game font
bool gl_draw_text(uint x, uint y, uint color, uint alpha, char *fmt, ...)
{
	uint tile_width = 24;
	uint tile_height = 24;
	uint i;
	uint len;
	uint x_origin = x;
	struct nvertex *vertices_a;
	word *indices_a;
	struct nvertex *vertices_b;
	word *indices_b;
	uint vert_a = 0;
	uint vert_b = 0;
	VOBJ(texture_set, font_a, 0);
	VOBJ(texture_set, font_b, 0);
	va_list args;
	VOBJ(graphics_object, object_a, 0);
	VOBJ(graphics_object, object_b, 0);
	char text[4096];
	struct driver_state saved_state;

	va_start(args, fmt);

	vsnprintf(text, sizeof(text), fmt, args);

	len = strlen(text);

	if(!ff8)
	{
		ff7_object_a = ff7_externals.menu_objects->font_a;
		ff7_object_b = ff7_externals.menu_objects->font_b;
	}
	else
	{
		if(!*ff8_externals.fonts) return false;
		ff8_object_a = (*ff8_externals.fonts)->font_a;
		ff8_object_b = (*ff8_externals.fonts)->font_b;
	}

	if(!VPTR(object_a)) return false;
	if(!VREF(object_a, hundred_data)) return false;
	VASS(font_a, VREF(object_a, hundred_data->texture_set));

	if(!VPTR(object_b)) return false;
	if(!VREF(object_b, hundred_data)) return false;
	VASS(font_b, VREF(object_b, hundred_data->texture_set));

	if(!VPTR(font_a) || !VPTR(font_b)) return false;

	if(ff8 && !ff8_externals.get_character_width(50)) return false;

	gl_save_state(&saved_state);

	vertices_a = driver_malloc(len * sizeof(struct nvertex) * 4);
	indices_a = driver_malloc(len * 2 * 4);
	vertices_b = driver_malloc(len * sizeof(struct nvertex) * 4);
	indices_b = driver_malloc(len * 2 * 4);

	for(i = 0; i < len; i++)
	{
		uint c = font_map[text[i]];
		uint font = c % 21 > 10 ? 1 : 0;
		uint row = c / 21;
		uint col = c % 21 - font * 11;
		uint x_offset = font == 0 ? 0 : 8;
		uint char_width;
		float x1 = (float)(x);
		float x2 = (float)(x + (col < 10 ? tile_width : tile_width - 8));
		float y1 = (float)(y);
		float y2 = (float)(y + tile_height);
		float z = 0.0f;
		float u1 = (1.0f / 256) * (col * tile_width + x_offset);
		float u2 = (1.0f / 256) * (col < 10 ? tile_width : tile_width - 8) + u1;
		float v1 = (1.0f / 256) * row * tile_height;
		float v2 = (1.0f / 256) * tile_height + v1;
		struct nvertex vertices[] = {
			{x2, y1, z, 1.0f, 0xffffff | alpha << 24, 0, u2, v1},
			{x2, y2, z, 1.0f, 0xffffff | alpha << 24, 0, u2, v2},
			{x1, y2, z, 1.0f, 0xffffff | alpha << 24, 0, u1, v2},
			{x1, y1, z, 1.0f, 0xffffff | alpha << 24, 0, u1, v1},
		};

		if(!ff8) char_width = (uint)((common_externals.font_info[c] & 0x1F) * (5.0 / 3.0));
		else char_width = ff8_externals.get_character_width(c) * 2;

		if(text[i] == '\n')
		{
			x = x_origin;
			y += tile_height;
			continue;
		}

		if(x + char_width > width)
		{
			x = x_origin;
			y += tile_height;
			i--;
			continue;
		}

		if(font == 0)
		{
			memcpy(&vertices_a[vert_a], vertices, sizeof(struct nvertex) * 4);
			indices_a[vert_a] = vert_a++;
			indices_a[vert_a] = vert_a++;
			indices_a[vert_a] = vert_a++;
			indices_a[vert_a] = vert_a++;
		}

		if(font == 1)
		{
			memcpy(&vertices_b[vert_b], vertices, sizeof(struct nvertex) * 4);
			indices_b[vert_b] = vert_b++;
			indices_b[vert_b] = vert_b++;
			indices_b[vert_b] = vert_b++;
			indices_b[vert_b] = vert_b++;
		}

		x += char_width;
	}

	gl_set_blend_func(BLEND_AVG);

	nodefer = true;

	current_state.texture_filter = false;

	if(vert_a > 0)
	{
		VRASS(font_a, palette_index, color);
		common_palette_changed(0, 0, 0, VREF(font_a, palette), VPTR(font_a));

		gl_draw_indexed_primitive(GL_QUADS, TLVERTEX, vertices_a, vert_a, indices_a, vert_a, VPTR(object_a), false, true);
	}

	if(vert_b > 0)
	{
		VRASS(font_b, palette_index, color);
		common_palette_changed(0, 0, 0, VREF(font_b, palette), VPTR(font_b));

		gl_draw_indexed_primitive(GL_QUADS, TLVERTEX, vertices_b, vert_b, indices_b, vert_b, VPTR(object_b), false, true);
	}

	nodefer = false;

	driver_free(vertices_a);
	driver_free(indices_a);
	driver_free(vertices_b);
	driver_free(indices_b);

	gl_load_state(&saved_state);

	return true;
}

// prepare for buffer swap, flush game rendering to back buffer
void gl_prepare_flip()
{
	float x1 = -1.0f;
	float x2 = 1.0f;
	float y1 = 1.0f;
	float y2 = -1.0f;
	float z = 1.0f;
	struct nvertex vertices[] = {
		{x2, y1, z, 1.0f, 0xffffffff, 0, 1.0f, 1.0f},
		{x2, y2, z, 1.0f, 0xffffffff, 0, 1.0f, 0.0f},
		{x1, y2, z, 1.0f, 0xffffffff, 0, 0.0f, 0.0f},
		{x1, y1, z, 1.0f, 0xffffffff, 0, 0.0f, 1.0f},
	};
	word indices[] = {0, 1, 2, 3};

	glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_COLOR_BUFFER_BIT);

	gl_set_texture(indirect_texture);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	glDisable(GL_SCISSOR_TEST);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);

	gl_use_post_program();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	current_state.texture_filter = true;
	current_state.fb_texture = false;

	glViewport(x_offset, y_offset, output_size_x, output_size_y);

	nodefer = true;

	gl_draw_indexed_primitive(GL_QUADS, TLVERTEX, vertices, 4, indices, 4, 0, false, false);

	nodefer = false;

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	gl_use_main_program();

	glPopAttrib();
}

// prepare for game rendering
void gl_prepare_render()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, indirect_fbo);

	glViewport(0, 0, internal_size_x, internal_size_y);
}

bool gl_load_shaders()
{
	uint max_interpolators;
	uint max_vert_uniforms;
	uint max_frag_uniforms;

	glGetIntegerv(GL_MAX_VARYING_FLOATS, &max_interpolators);
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_vert_uniforms);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max_frag_uniforms);

	info("Shader limits: varying %i, vert uniform %i, frag uniform %i\n", max_interpolators, max_vert_uniforms, max_frag_uniforms);

	main_program = gl_create_program(vert_source, frag_source, "main");

	if(!main_program) return false;

	gl_use_main_program();

	glUniform1i(glGetUniformLocation(current_program, "vertexcolor"), 1);

	return true;
}

bool gl_init_indirect()
{
	uint fbo_width = internal_size_x, fbo_height = internal_size_y;

	if(!GLEW_EXT_framebuffer_object)
	{
		error("No FBO support, cannot do indirect rendering\n");
		return false;
	}

	indirect_texture = gl_create_empty_texture();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffersEXT(1, &indirect_fbo);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, indirect_fbo);

	glGenRenderbuffersEXT(1, &depthbuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthbuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, fbo_width, fbo_height);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depthbuffer);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbo_width, fbo_height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, 0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, indirect_texture, 0);

	if(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
		error("Driver didn't accept our FBO attachments, cannot do indirect rendering\n");
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &indirect_fbo);

		return false;
	}

	return true;
}

bool gl_init_postprocessing()
{
	return (post_program = gl_create_program(0, post_source, "postprocessing")) != 0;
}
