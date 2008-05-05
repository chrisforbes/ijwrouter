#include <assert.h>
#include <string.h>

#include "common.h"
#include "fs.h"
#include "hal_fs.h"
#include "hal_debug.h"

static u32 image_size = 0;
static char const * start = 0;
static file_entry const * fs = 0;

static char const * find_start_of_content( void )
{
	file_entry const * f = fs;
	while( f->filename_pair.offset || (f == fs) )
		++f;

	return ((char const *)f) + 4;	//skip 4 byte sentinel!
}

file_entry const * fs_find_file( char const * filename )
{
	file_entry const * f = fs;
	assert( f );

	while( f->filename_pair.offset || (f == fs) )
	{
		if ( 0 == strncmp( start + f->filename_pair.offset, filename, f->filename_pair.length ) )
			return f;
		++f;
	}

	return 0;
}

char const * fs_get_mimetype( file_entry const * entry )
{
	assert(entry);

	return start + entry->mime_pair.offset;
}

char const * fs_get_content( file_entry const * entry )
{
	assert(entry);

	return start + entry->content_pair.offset;
}

u08 fs_is_gzipped( file_entry const * entry )
{
	assert(entry);
	return entry->attribs & ATTRIB_GZIP;
}

void fs_init( void )
{
	fs = fs_getimage(&image_size);
	if ( fs )
		start = find_start_of_content();
}

u08 fs_is_static_buf( void * p )
{
	if ((u32)p >= (u32)start && (u32)p < (u32)(start + image_size))
		return 1;
	return 0;
}