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
 * externals_102_de.h - memory addresses for the german 1.02 version of FF7
 */

common_externals.directsound =        (void *)0xDC2680;
common_externals.debug_print =                0x664E00;
common_externals.debug_print2 =               0x414EE0;
common_externals.create_tex_header =  (void *)0x688C16;
common_externals.get_time =                   0x660340;
common_externals.midi_init =                  0x6DE060;
common_externals.get_midi_name =      (void *)0x6E69B0;
common_externals.play_midi =                  0x6DE935;
common_externals.stop_midi =                  0x6DF70B;
common_externals.cross_fade_midi =            0x6DF4CE;
common_externals.pause_midi =                 0x6DF65B;
common_externals.restart_midi =               0x6DF6B3;
common_externals.midi_status =                0x6DF793;
common_externals.set_master_midi_volume =     0x6DF7BA;
common_externals.set_midi_volume =            0x6DF817;
common_externals.set_midi_volume_trans =      0x6DF92C;
common_externals.set_midi_tempo =             0x6DFA9D;
common_externals.draw_graphics_object =       0x66E611;
common_externals.font_info =          (void *)0x99EB68;
common_externals.build_dialog_window =        0x7743B0;
common_externals.load_tex_file =              0x688C66;

ff7_externals.chocobo_fix =                   0x70B512;
ff7_externals.midi_fix =                      0x6E0422;
ff7_externals.snowboard_fix =         (void *)0x94BA30;
ff7_externals.cdcheck =                       0x408FF3;
ff7_externals.sub_665D9A =            (void *)0x665D6A;
ff7_externals.sub_671742 =            (void *)0x671712;
ff7_externals.sub_6B27A9 =            (void *)0x6B2779;
ff7_externals.sub_68D2B8 =            (void *)0x68D288;
ff7_externals.sub_665793 =            (void *)0x665763;
ff7_externals.matrix3x4 =             (void *)0x67BC2B;
ff7_externals.matrix4x3_multiply =            0x66CC0B;
ff7_externals.sub_6B26C0 =                    0x6B2690;
ff7_externals.sub_6B2720 =                    0x6B26F0;
ff7_externals.sub_673F5C =                    0x673F2C;
ff7_externals.savemap =               (void *)0xF38A68;
ff7_externals.menu_objects =          (void *)0xF39CF0;
ff7_externals.magic_thread_start =            0x427928;
ff7_externals.destroy_magic_effects = (void *)0x429322;
ff7_externals.init_stuff =			     	  0x40A091;
