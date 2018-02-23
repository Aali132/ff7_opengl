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
 * ff7/defs.h - definitions & prototypes for FF7 code replacements
 */

#ifndef _DEFS_H_
#define _DEFS_H_

// battle
void magic_thread_start(void (*func)());

// misc
uint get_equipment_stats(uint party_index, uint type);
void kernel2_reset_counters();
char *kernel2_add_section(uint size);
char *kernel2_get_text(uint section_base, uint string_id, uint section_offset);

// field
void field_load_textures(struct ff7_game_obj *game_object, struct struc_3 *struc_3);
void field_layer2_pick_tiles(short x_offset, short y_offset);
void char_addhp(uint party_index, word amount);
void char_addmp(uint party_index, word amount);

// file
FILE *open_lgp_file(char *filename, uint mode);
void close_lgp_file(FILE *fd);
extern char lgp_names[18][256];
bool lgp_chdir(char *path);
struct lgp_file *lgp_open_file(char *filename, uint lgp_num);
bool lgp_seek_file(uint offset, uint lgp_num);
uint lgp_read(uint lgp_num, char *dest, uint size);
uint lgp_read_file(struct lgp_file *file, uint lgp_num, char *dest, uint size);
uint lgp_get_filesize(struct lgp_file *file, uint lgp_num);
void close_file(struct ff7_file *file);
struct ff7_file *open_file(struct file_context *file_context, char *filename);
uint __read_file(uint count, void *buffer, struct ff7_file *file);
bool read_file(uint count, void *buffer, struct ff7_file *file);
uint __read(FILE *file, char *buffer, uint count);
bool write_file(uint count, void *buffer, struct ff7_file *file);
uint get_filesize(struct ff7_file *file);
uint tell_file(struct ff7_file *file);
void seek_file(struct ff7_file *file, uint offset);
char *make_pc_name(struct file_context *file_context, struct ff7_file *file, char *filename);

// graphics
void destroy_d3d2_indexed_primitive(struct indexed_primitive *ip);
bool ff7gl_load_group(uint group_num, struct matrix_set *matrix_set, struct p_hundred *_hundred_data, struct p_group *_group_data, struct polygon_data *polygon_data, struct ff7_polygon_set *polygon_set, struct ff7_game_obj *game_object);
struct tex_header *sub_673F5C(struct struc_91 *struc91);
void draw_single_triangle(struct nvertex *vertices);
void sub_6B2720(struct indexed_primitive *ip);
void draw_3d_model(uint current_frame, struct anim_header *anim_header, struct struc_110 *struc_110, struct hrc_data *hrc_data, struct ff7_game_obj *game_object);

// loaders
struct anim_header *load_animation(struct file_context *file_context, char *filename);
struct battle_hrc_header *read_battle_hrc(bool use_file_context, struct file_context *file_context, char *filename);
struct polygon_data *load_p_file(struct file_context *file_context, bool create_lists, char *filename);
void destroy_tex_header(struct ff7_tex_header *tex_header);
struct ff7_tex_header *load_tex_file(struct file_context *file_context, char *filename);

#endif
