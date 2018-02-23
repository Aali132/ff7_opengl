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
 * ff7/file.c - replacement routines for FF7's file system functions
 */

#include <sys/stat.h>

#include "../types.h"
#include "../common.h"
#include "../ff7.h"
#include "../log.h"
#include "../globals.h"

FILE *open_lgp_file(char *filename, uint mode)
{
	if(trace_files) trace("opening lgp file %s\n", filename);

	return fopen(filename, "rb");
}

void close_lgp_file(FILE *fd)
{
	if(!fd) return;

	if(trace_files) trace("closing lgp file\n");

	fclose(fd);
}

// LGP names used for modpath lookup
char lgp_names[18][256] = {
	"char",
	"flevel",
	"battle",
	"magic",
	"menu",
	"world",
	"condor",
	"chocobo",
	"high",
	"coaster",
	"snowboard",
	"midi",
	"",
	"",
	"moviecam",
	"cr",
	"disc",
	"sub",
};

struct lgp_file
{
	bool is_lgp_offset;
	union
	{
		uint offset;
		FILE *fd;
	};
	bool resolved_conflict;
};

#define NUM_LGP_FILES 64

struct lgp_file *lgp_files[NUM_LGP_FILES];
uint lgp_files_index = 0;

struct lgp_file *last;

char lgp_current_dir[256];

bool use_files_array = true;

int lgp_lookup_value(unsigned char c)
{
	c = tolower(c);
	
	if(c == '.') return -1;
	
	if(c < 'a' && c >= '0' && c <= '9') c += 'a' - '0';
	
	if(c == '_') c = 'k';
	if(c == '-') c = 'l';
	
	return c - 'a';
}

bool lgp_chdir(char *path)
{
	uint len = strlen(path);

	while(path[0] == '/' || path[0] == '\\') path++;
	
	memcpy(lgp_current_dir, path, len + 1);

	while(lgp_current_dir[len - 1] == '/' || lgp_current_dir[len - 1] == '\\') len--;
	lgp_current_dir[len] = 0;

	return true;
}

// original LGP open file logic, unchanged except for the LGP Tools safety net
bool original_lgp_open_file(char *filename, uint lgp_num, struct lgp_file *ret)
{
	uint lookup_value1 = lgp_lookup_value(filename[0]);
	uint lookup_value2 = lgp_lookup_value(filename[1]) + 1;
	struct lookup_table_entry *lookup_table = ff7_externals.lgp_lookup_tables[lgp_num];
	uint toc_offset = lookup_table[lookup_value1 * 30 + lookup_value2].toc_offset;
	uint i;

	// did we find anything in the lookup table?
	if(toc_offset)
	{
		uint num_files = lookup_table[lookup_value1 * 30 + lookup_value2].num_files;

		// look for our file
		for(i = 0; i < num_files; i++)
		{
			struct lgp_toc_entry *toc_entry = &ff7_externals.lgp_tocs[lgp_num * 2][toc_offset + i - 1];

			if(!_stricmp(toc_entry->name, filename))
			{
				if(!toc_entry->conflict)
				{
					// this is the only file with this name, we're done here
					ret->is_lgp_offset = true;
					ret->offset = toc_entry->offset;
					return true;
				}
				else
				{
					struct conflict_list *conflict = &ff7_externals.lgp_folders[lgp_num].conflicts[toc_entry->conflict - 1];
					struct conflict_entry *conflict_entries = conflict->conflict_entries;
					uint num_conflicts = conflict->num_conflicts;
					
					// there are multiple files with this name, look for our
					// current directory in the conflict table
					for(i = 0; i < num_conflicts; i++)
					{
						if(!_stricmp(conflict_entries[i].name, lgp_current_dir))
						{
							struct lgp_toc_entry *toc_entry = &ff7_externals.lgp_tocs[lgp_num * 2][conflict_entries[i].toc_index];

							// file name and directory matches, this is our file
							ret->is_lgp_offset = true;
							ret->offset = toc_entry->offset;
							ret->resolved_conflict = true;
							return true;
						}
					}

					break;
				}
			}
		}
	}

	// one last chance, the lookup table might have been broken by LGP Tools,
	// search through the entire archive
	for(i = 0; i < ((uint *)ff7_externals.lgp_tocs)[lgp_num * 2 + 1]; i++)
	{
		struct lgp_toc_entry *toc_entry = &ff7_externals.lgp_tocs[lgp_num * 2][i];

		if(!_stricmp(toc_entry->name, filename))
		{
			glitch("broken LGP file (%s), don't use LGP Tools!\n", lgp_names[lgp_num]);

			if(!toc_entry->conflict)
			{
				ret->is_lgp_offset = true;
				ret->offset = toc_entry->offset;
				return true;
			}
		}
	}

	return false;
}

// new LGP open file logic with modpath and direct mode support
struct lgp_file *lgp_open_file(char *filename, uint lgp_num)
{
	struct lgp_file *ret = driver_calloc(sizeof(*ret), 1);
	char tmp[512 + sizeof(basedir)];
	char _fname[_MAX_FNAME];
	char *fname = _fname;
	char ext[_MAX_EXT];
	char name[_MAX_FNAME + _MAX_EXT];

	_splitpath(filename, 0, 0, fname, ext);

	if(direct_mode)
	{
		_snprintf(tmp, sizeof(tmp), "%s/direct/%s/%s%s", basedir, lgp_names[lgp_num], fname, ext);
		ret->fd = fopen(tmp, "rb");

		if(!ret->fd)
		{
			_snprintf(tmp, sizeof(tmp), "%s/direct/%s/%s/%s%s", basedir, lgp_names[lgp_num], lgp_current_dir, fname, ext);
			ret->fd = fopen(tmp, "rb");
			if(ret->fd) ret->resolved_conflict = true;
		}

		if(trace_direct) trace("lgp_open_file: %i, %s (%s) = 0x%x\n", lgp_num, filename, lgp_current_dir, ret);
	}

	if(!ret->fd)
	{
		sprintf(name, "%s%s", fname, ext);

		if(!original_lgp_open_file(name, lgp_num, ret))
		{
			if(direct_mode) error("failed to find file %s; tried direct/%s/%s, direct/%s/%s/%s, %s/%s (LGP) (path: %s)\n", filename, lgp_names[lgp_num], name, lgp_names[lgp_num], lgp_current_dir, name, lgp_names[lgp_num], name, lgp_current_dir);
			else error("failed to find file %s/%s (LGP) (path: %s)\n", lgp_names[lgp_num], name, lgp_current_dir);
			driver_free(ret);
			return 0;
		}
	}

	last = ret;

	if(use_files_array && !ret->is_lgp_offset)
	{
		if(lgp_files[lgp_files_index])
		{
			fclose(lgp_files[lgp_files_index]->fd);
			driver_free(lgp_files[lgp_files_index]);
		}

		lgp_files[lgp_files_index] = ret;
		lgp_files_index = (lgp_files_index + 1) % NUM_LGP_FILES;
	}

	return ret;
}

/* 
 * Direct LGP file access routines are used all over the place despite the nice
 * generic file interface found below in this file. Therefore we must implement
 * these in a way that works with the original code.
 */

// seek to given offset in LGP file
bool lgp_seek_file(uint offset, uint lgp_num)
{
	if(!ff7_externals.lgp_fds[lgp_num]) return false;

	fseek(ff7_externals.lgp_fds[lgp_num], offset, SEEK_SET);

	return true;
}

// read straight from LGP file
uint lgp_read(uint lgp_num, char *dest, uint size)
{
	if(!ff7_externals.lgp_fds[lgp_num]) return 0;

	if(last->is_lgp_offset) return fread(dest, 1, size, ff7_externals.lgp_fds[lgp_num]);

	return fread(dest, 1, size, last->fd);
}

// read from LGP file by LGP file descriptor
uint lgp_read_file(struct lgp_file *file, uint lgp_num, char *dest, uint size)
{
	if(!ff7_externals.lgp_fds[lgp_num]) return 0;

	if(file->is_lgp_offset)
	{
		lgp_seek_file(file->offset + 24, lgp_num);
		return fread(dest, 1, size, ff7_externals.lgp_fds[lgp_num]);
	}

	return fread(dest, 1, size, file->fd);
}

// retrieve the size of a file within the LGP archive
uint lgp_get_filesize(struct lgp_file *file, uint lgp_num)
{
	if(file->is_lgp_offset)
	{
		uint size;

		lgp_seek_file(file->offset + 20, lgp_num);
		fread(&size, 4, 1, ff7_externals.lgp_fds[lgp_num]);
		return size;
	}
	else
	{
		struct stat s;

		fstat(file->fd->_file, &s);

		return s.st_size;
	}
}

// close a file handle
void close_file(struct ff7_file *file)
{
	if(!file) return;

	if(file->fd)
	{
		if(!file->fd->is_lgp_offset && file->fd->fd) fclose(file->fd->fd);
		driver_free(file->fd);
	}

	driver_free(file->name);
	driver_free(file);
}

// open file handle, target could be a file within an LGP archive or a regular
// file on disk
struct ff7_file *open_file(struct file_context *file_context, char *filename)
{
	char mangled_name[200];
	struct ff7_file *ret = driver_calloc(sizeof(*ret), 1);
	char *_filename = filename;

	if(trace_files)
	{
		if(file_context->use_lgp) trace("open %s (LGP:%s)\n", filename, lgp_names[file_context->lgp_num]);
		else trace("open %s (mode %i)\n", filename, file_context->mode);
	}

	if(!ret) return 0;

	ret->name = driver_malloc(strlen(filename) + 1);
	strcpy(ret->name, filename);
	memcpy(&ret->context, file_context, sizeof(*file_context));

	// file name mangler used mainly by battle module to convert PSX file names
	// to LGP-friendly PC names
	if(file_context->name_mangler)
	{
		file_context->name_mangler(filename, mangled_name);
		_filename = mangled_name;

		if(trace_files) trace("mangled name: %s\n", mangled_name);
	}

	if(file_context->use_lgp)
	{
		use_files_array = false;
		ret->fd = lgp_open_file(_filename, ret->context.lgp_num);
		use_files_array = true;
		if(!ret->fd)
		{
			if(file_context->name_mangler) error("offset error: %s %s\n", filename, _filename);
			else error("offset error: %s\n", filename);
			goto error;
		}

		if(!lgp_seek_file(ret->fd->offset + 24, ret->context.lgp_num))
		{
			error("seek error: %s\n", filename);
			goto error;
		}
	}
	else
	{
		ret->fd = driver_calloc(sizeof(*ret->fd), 1);

		if(ret->context.mode == FF7_FMODE_READ) ret->fd->fd = fopen(_filename, "rb");
		else if(ret->context.mode == FF7_FMODE_READ_TEXT) ret->fd->fd = fopen(_filename, "r");
		else if(ret->context.mode == FF7_FMODE_WRITE) ret->fd->fd = fopen(_filename, "wb");
		else if(ret->context.mode == FF7_FMODE_CREATE) ret->fd->fd = fopen(_filename, "w+b");
		else ret->fd->fd = fopen(_filename, "r+b");

		if(!ret->fd->fd) goto error;
	}

	return ret;

error:
	// it's normal for save files to be missing, anything else is probably
	// going to cause trouble
	if(file_context->use_lgp || _stricmp(&_filename[strlen(_filename) - 4], ".ff7")) error("could not open file %s\n", filename);
	close_file(ret);
	return 0;
}

// read from file handle, returns how many bytes were actually read
uint __read_file(uint count, void *buffer, struct ff7_file *file)
{
	uint ret = 0;

	if(!file || !count) return false;

	if(trace_files) trace("reading %i bytes from %s (ALT)\n", count, file->name);

	if(file->context.use_lgp) return lgp_read(file->context.lgp_num, buffer, count);

	ret = fread(buffer, 1, count, file->fd->fd);

	if(ferror(file->fd->fd))
	{
		error("could not read from file %s (%i)\n", file->name, ret);
		return -1;
	}

	return ret;
}

// read from file handle, returns true if the read succeeds
bool read_file(uint count, void *buffer, struct ff7_file *file)
{
	uint ret = 0;

	if(!file || !count) return false;

	if(trace_files) trace("reading %i bytes from %s\n", count, file->name);

	if(file->context.use_lgp) return lgp_read(file->context.lgp_num, buffer, count);

	ret = fread(buffer, 1, count, file->fd->fd);

	if(ret != count)
	{
		error("could not read from file %s (%i)\n", file->name, ret);
		return false;
	}

	return true;
}

// read directly from a file descriptor returned by the open_file function
uint __read(FILE *file, char *buffer, uint count)
{
	return fread(buffer, 1, count, file);
}

// write to file handle, returns true if the write succeeds
bool write_file(uint count, void *buffer, struct ff7_file *file)
{
	uint ret = 0;
	void *tmp = 0;

	if(!file || !count) return false;

	if(file->context.use_lgp) return false;

	if(trace_files) trace("writing %i bytes to %s\n", count, file->name);

	// hack to emulate win95 style writes, a NULL buffer means we should write
	// all zeroes
	if(!buffer)
	{
		tmp = driver_calloc(count, 1);
		buffer = tmp;
	}

	ret = fwrite(buffer, 1, count, file->fd->fd);

	if(tmp) driver_free(tmp);

	if(ret != count)
	{
		error("could not write to file %s\n", file->name);
		return false;
	}

	return true;
}

// retrieve the size of a file from file handle
uint get_filesize(struct ff7_file *file)
{
	if(!file) return 0;

	if(trace_files) trace("get_filesize %s\n", file->name);

	if(file->context.use_lgp) return lgp_get_filesize(file->fd, file->context.lgp_num);
	else
	{
		struct stat s;
		fstat(file->fd->fd->_file, &s);

		return s.st_size;
	}
}

// retrieve the current seek position from file handle
uint tell_file(struct ff7_file *file)
{
	if(!file) return 0;

	if(trace_files) trace("tell %s\n", file->name);

	if(file->context.use_lgp) return 0;

	return ftell(file->fd->fd);
}

// seek to position in file
void seek_file(struct ff7_file *file, uint offset)
{
	if(!file) return;

	if(trace_files) trace("seek %s to %i\n", file->name, offset);

	// it's not possible to seek within LGP archives
	if(file->context.use_lgp) return;

	if(fseek(file->fd->fd, offset, SEEK_SET)) error("could not seek file %s\n", file->name);
}

// construct modpath name from file context, file handle and filename
char *make_pc_name(struct file_context *file_context, struct ff7_file *file, char *filename)
{
	uint i, len;
	char *backslash;
	char *ret = external_malloc(1024);

	if(file_context->use_lgp)
	{
		if(file->fd->resolved_conflict) len = _snprintf(ret, 1024, "%s/%s/%s", lgp_names[file_context->lgp_num], lgp_current_dir, filename);
		else len = _snprintf(ret, 1024, "%s/%s", lgp_names[file_context->lgp_num], filename);
	}
	else len = _snprintf(ret, 1024, "%s", filename);

	for(i = 0; i < len; i++)
	{
		if(ret[i] == '.')
		{
			if(!stricmp(&ret[i], ".tex")) ret[i] = 0;
			else if(!stricmp(&ret[i], ".p")) ret[i] = 0;
			else ret[i] = '_';
		}
	}

	while(backslash = strchr(ret, '\\')) *backslash = '/';

	return ret;
}
