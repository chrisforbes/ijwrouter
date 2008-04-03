#ifndef FS_H
#define FS_H

void fs_init( void );

typedef struct string_t
{
	u32 offset;
	u32 length;
} string_t;

typedef struct file_entry
{
	string_t filename_pair;
	string_t mime_pair;
	string_t content_pair;
} file_entry;

file_entry const * fs_find_file ( char const * filename );
char const * fs_get_mimetype ( struct file_entry const * entry );
char const * fs_get_content ( struct file_entry const * entry );
u08 fs_is_static_buf( void * p );

#endif