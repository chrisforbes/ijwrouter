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
	while( f->filename.offset || (f == fs) )
		++f;

	return ((char const *)f) + 4;	//skip 4 byte sentinel!
}

file_entry const * fs_find_file( char const * filename )
{
	file_entry const * f = fs;
	assert( f );

	while( f->filename.offset || (f == fs) )
	{
		if ( 0 == strncmp( start + f->filename.offset, filename, f->filename.length ) )
			return f;
		++f;
	}

	return 0;
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

str_t fs_get_str( struct string_t const * fs_str )
{
	assert( fs_str );
	return __make_string( (char *)start + fs_str->offset, fs_str->length );
}