#ifndef HAL_FILE_SYSTEM_H
#define HAL_FILE_SYSTEM_H

typedef struct offset_length_pair
{
	u32 offset;
	u32 length;
} offset_length_pair;

typedef struct file_entry
{
	offset_length_pair filename_pair;
	offset_length_pair mime_pair;
	offset_length_pair content_pair;
} file_entry;

void load_content( void );

file_entry const * find_file ( char const * filename );

char const * get_mime_type ( file_entry const * entry );

char const * get_content ( file_entry const * entry );

#endif