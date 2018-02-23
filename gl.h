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
 * gl.h - definitions used by OpenGL renderer
 */

#ifndef _DRIVER_GL_H_
#define _DRIVER_GL_H_

#include <gl/glew.h>

#include "types.h"
#include "common.h"
#include "matrix.h"

#define VERTEX 1
#define LVERTEX 2
#define TLVERTEX 3

#define BLEND_AVG 0
#define BLEND_ADD 1
#define BLEND_SUB 2
#define BLEND_25P 3
#define BLEND_NONE 4

struct driver_state
{
	struct texture_set *texture_set;
	uint texture_handle;
	uint blend_mode;
	uint viewport[4];
	bool fb_texture;
	bool wireframe;
	bool texture_filter;
	bool cullface;
	bool nocull;
	bool depthtest;
	bool depthmask;
	bool shademode;
	bool alphatest;
	uint alphafunc;
	uint alpharef;
	struct matrix world_matrix;
	struct matrix d3dprojection_matrix;
};

struct deferred_draw
{
	GLenum primitivetype;
	uint vertextype;
	uint vertexcount;
	uint count;
	struct nvertex *vertices;
	word *indices;
	bool clip;
	bool mipmap;
	struct driver_state state;
	double z;
	bool drawn;
};

struct gl_texture_set
{
	uint textures;
	bool force_filter;
	bool force_zsort;
};

extern struct matrix d3dviewport_matrix;

extern struct driver_state current_state;

extern GLuint current_program;

extern uint max_texture_size;

void gl_draw_movie_quad_bgra(GLuint, int, int);
void gl_draw_movie_quad_yuv(GLuint *, int, int, bool);
GLuint gl_create_program(char *vertex_file, char *fragment_file, char *name);
void gl_use_post_program();
void gl_use_main_program();
void gl_use_yuv_program();
void gl_save_state(struct driver_state *dest);
void gl_load_state(struct driver_state *src);
bool gl_defer_draw(GLenum primitivetype, uint vertextype, struct nvertex *vertices, uint vertexcount, word *indices, uint count, bool clip, bool mipmap);
void gl_draw_deferred();
void gl_check_deferred(struct texture_set *texture_set);
bool gl_special_case(GLenum primitivetype, uint vertextype, struct nvertex *vertices, uint vertexcount, word *indices, uint count, struct graphics_object *graphics_object, bool clip, bool mipmap);
void gl_draw_with_lighting(struct indexed_primitive *ip, bool clip, struct matrix *model_matrix);
void gl_draw_indexed_primitive(GLenum, uint, struct nvertex *, uint, word *, uint, struct graphics_object *, bool clip, bool mipmap);
void gl_set_world_matrix(struct matrix *matrix);
void gl_set_d3dprojection_matrix(struct matrix *matrix);
void gl_set_blend_func(uint);
void gl_check_texture_dimensions(uint width, uint height, char *source);
GLuint gl_create_empty_texture();
GLuint gl_create_texture(void *data, uint width, uint height, uint format, uint internalformat, uint size, bool generate_mipmaps);
void gl_init_pbo_ring();
void *gl_get_pixel_buffer(uint size);
GLuint gl_commit_pixel_buffer(void *data, uint width, uint height, uint format, bool generate_mipmaps);
GLuint gl_compress_pixel_buffer(void *data, uint width, uint height, uint format);
GLuint gl_commit_compressed_buffer(void *data, uint width, uint height, uint format, uint size);
void gl_replace_texture(struct texture_set *texture_set, uint palette_index, uint new_texture);
void gl_upload_texture(struct texture_set *texture_set, uint palette_index, void *image_data, uint format);
void gl_bind_texture_set(struct texture_set *);
void gl_set_texture(GLuint);
bool gl_init_indirect();
bool gl_init_postprocessing();
void gl_prepare_flip();
void gl_prepare_render();
bool gl_load_shaders();
bool gl_draw_text(uint x, uint y, uint color, uint alpha, char *fmt, ...);

#endif
