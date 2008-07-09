#ifndef FS_H
#define FS_H

#include "str.h"

void fs_init( void );

typedef struct string_t
{
	u32 offset;
	u32 length;
} string_t;

#include "pack1.h"
typedef struct file_entry
{
	string_t filename;
	string_t content_type;
	string_t content;
	string_t digest;
	u08 attribs;
} PACKED_STRUCT file_entry;
#include "packdefault.h"

#define ATTRIB_AWESOME	0x80
#define ATTRIB_GZIP		0x01

file_entry const * fs_find_file ( char const * filename );
u08 fs_is_gzipped( file_entry const * entry );

str_t fs_get_str( struct string_t const * fs_str );

#endif
