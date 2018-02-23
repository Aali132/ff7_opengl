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
 * gl/shader.c - support functions for loading/using OpenGL shaders
 */

#include <gl/glew.h>
#include <sys/stat.h>

#include "../types.h"
#include "../log.h"

uint main_program = 0;
uint post_program = 0;
uint yuv_program = 0;

uint current_program = 0;

// read plaintext shader source file
char *read_source(const char *file)
{
	uint size, read = 0;
	FILE *f;
	struct stat s;
	char *buffer;
	char filename[BASEDIR_LENGTH + 1024];

	_snprintf(filename, sizeof(filename), "%s/%s", basedir, file);

	if(stat(filename, &s))
	{
		error("failed to stat file %s\n", filename);
		return 0;
	}

	size = s.st_size;
	buffer = driver_malloc(size + 1);

	f = fopen(filename, "r");

	if(!f)
	{
		error("couldn't open file %s for reading: %s", filename, _strerror(NULL));
		driver_free(buffer);
		return 0;
	}

	while(read < size)
	{
		uint ret = fread(&buffer[read], 1, size - read, f);
		
		if(ret == 0 && feof(f)) break;

		read += ret;
	}

	buffer[read] = 0;

	return buffer;
}

void printShaderInfoLog(GLuint obj, char *name)
{
	int infologLength = 0;
	char *infoLog;
	
	glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
	
	if(infologLength > 1)
	{
		infoLog = (char *)driver_malloc(infologLength);
		glGetShaderInfoLog(obj, infologLength, 0, infoLog);
		info("%s shader compile log:\n%s\n", name, infoLog);
		driver_free(infoLog);
	}
}

void printProgramInfoLog(GLuint obj, char *name)
{
	int infologLength = 0;
	char *infoLog;
	
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
	
	if(infologLength > 1)
	{
		infoLog = (char *)driver_malloc(infologLength);
		glGetProgramInfoLog(obj, infologLength, 0, infoLog);
		info("%s program link log:\n%s\n", name, infoLog);
		driver_free(infoLog);
	}
}

// create shader program from vertex and fragment components, either one is
// optional
GLuint gl_create_program(char *vertex_file, char *fragment_file, char *name)
{
	GLint status = GL_FALSE;
	GLuint program = glCreateProgram();
	GLuint vshader, fshader;

	if(vertex_file)
	{
		char *vs = read_source(vertex_file);

		if(vs == 0)
		{
			glDeleteProgram(program);
			return 0;
		}

		vshader = glCreateShader(GL_VERTEX_SHADER);

		glShaderSource(vshader, 1, &vs, NULL);

		driver_free(vs);

		glCompileShader(vshader);
		printShaderInfoLog(vshader, "vertex");
		glAttachShader(program, vshader);
	}

	if(fragment_file)
	{
		char *fs = read_source(fragment_file);

		if(fs == 0)
		{
			if(vertex_file) glDeleteShader(vshader);
			glDeleteProgram(program);
			return 0;
		}

		fshader = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(fshader, 1, &fs, NULL);

		driver_free(fs);

		glCompileShader(fshader);
		printShaderInfoLog(fshader, "fragment");
		glAttachShader(program, fshader);
	}

	glLinkProgram(program);
	printProgramInfoLog(program, name);

	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if(status != GL_TRUE)
	{
		if(vertex_file) glDeleteShader(vshader);
		if(fragment_file) glDeleteShader(fshader);
		glDeleteProgram(program);
		return 0;
	}

	return program;
}

// enable postprocessing shader
void gl_use_post_program()
{
	glUseProgram(post_program);
	current_program = post_program;

	if(post_program != 0)
	{
		glUniform1i(glGetUniformLocation(current_program, "tex"), 0);
		glUniform1f(glGetUniformLocation(current_program, "width"), (float)internal_size_x);
		glUniform1f(glGetUniformLocation(current_program, "height"), (float)internal_size_y);
	}
}

// enable main rendering shader
void gl_use_main_program()
{
	glUseProgram(main_program);
	current_program = main_program;

	if(main_program != 0)
	{
		glUniform1i(glGetUniformLocation(current_program, "tex"), 0);
	}
}

// enable YUV texture shader
void gl_use_yuv_program()
{
	glUseProgram(yuv_program);
	current_program = yuv_program;

	if(yuv_program != 0)
	{
		glUniform1i(glGetUniformLocation(current_program, "y_tex"), 0);
		glUniform1i(glGetUniformLocation(current_program, "u_tex"), 1);
		glUniform1i(glGetUniformLocation(current_program, "v_tex"), 2);
	}
}
