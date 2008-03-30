#include <windows.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

#include "../common.h"
#include "../hal_file_system.h"

static char const * start;
static file_entry const * files;

char const * find_start_of_content( void )
{
	file_entry const * f = files;
	while( f->filename_pair.offset || (f == files) )
		++f;

	return ((char const *)f) + 4;
}

void load_content( void )
{
	u32 bytesRead;
	HANDLE h = CreateFile(L"content.blob", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	u32 size = GetFileSize(h, 0);

	files = malloc( size );
	ReadFile(h, (LPVOID)files, size, &bytesRead, 0);
	CloseHandle(h);
	start = find_start_of_content();
}

file_entry const * find_file ( char const * filename )
{
	file_entry const * f = files;
	while( f->filename_pair.offset || (f == files) )
	{
		if ( 0 == strncmp( start + f->filename_pair.offset, filename, f->filename_pair.length ) )
			return f;
		++f;
	}

	return 0;
}

char const * get_mime_type ( file_entry const * entry )
{
	assert(entry);

	return start + entry->mime_pair.offset;
}

char const * get_content ( file_entry const * entry )
{
	assert(entry);

	return start + entry->content_pair.offset;
}