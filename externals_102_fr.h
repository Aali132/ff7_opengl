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
 * externals_102_fr.h - memory addresses for the french 1.02 version of FF7
 */

common_externals.directsound =        (void *)0xDC36B0;
common_externals.debug_print =                0x664DD0;
common_externals.debug_print2 =               0x414EF0;
common_externals.create_tex_header =  (void *)0x688BE6;
common_externals.get_time =                   0x660310;
common_externals.midi_init =                  0x6DE000;
common_externals.get_midi_name =      (void *)0x6E6950;
common_externals.play_midi =                  0x6DE8D5;
common_externals.stop_midi =                  0x6DF6AB;
common_externals.cross_fade_midi =            0x6DF46E;
common_externals.pause_midi =                 0x6DF5FB;
common_externals.restart_midi =               0x6DF653;
common_externals.midi_status =                0x6DF733;
common_externals.set_master_midi_volume =     0x6DF75A;
common_externals.set_midi_volume =            0x6DF7B7;
common_externals.set_midi_volume_trans =      0x6DF8CC;
common_externals.set_midi_tempo =             0x6DFA3D;
common_externals.draw_graphics_object =       0x66E5E1;
common_externals.font_info =          (void *)0x99FB98;
common_externals.build_dialog_window =        0x774690;
common_externals.load_tex_file =              0x688C36;

ff7_externals.chocobo_fix =                   0x70B4B2;
ff7_externals.midi_fix =                      0x6E03C2;
ff7_externals.snowboard_fix =         (void *)0x94BA48;
ff7_externals.cdcheck =                       0x409003;
ff7_externals.sub_665D9A =            (void *)0x665D3A;
ff7_externals.sub_671742 =            (void *)0x6716E2;
ff7_externals.sub_6B27A9 =            (void *)0x6B2749;
ff7_externals.sub_68D2B8 =            (void *)0x68D258;
ff7_externals.sub_665793 =            (void *)0x665733;
ff7_externals.matrix3x4 =             (void *)0x67BBFB;
ff7_externals.matrix4x3_multiply =            0x66CBDB;
ff7_externals.sub_6B26C0 =                    0x6B2660;
ff7_externals.sub_6B2720 =                    0x6B26C0;
ff7_externals.sub_673F5C =                    0x673EFC;
ff7_externals.savemap =               (void *)0xF39A78;
ff7_externals.menu_objects =          (void *)0xF3AD00;
ff7_externals.magic_thread_start =            0x427938;
ff7_externals.destroy_magic_effects = (void *)0x429332;
ff7_externals.init_stuff =				      0x40A0A1;
