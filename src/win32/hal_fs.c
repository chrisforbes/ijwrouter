#include "../common.h"
#include "../hal_fs.h"
#include "../hal_debug.h"

#include <windows.h>
#include <memory.h>

void const * fs_getimage( u32 * size )
{
	u32 bytesRead;
	void * fs;
	HANDLE h = CreateFile(L"..\\content.blob", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

	if (INVALID_HANDLE_VALUE == h)
	{
		log_printf( "fs: cannot open content.blob\n" );
		return 0;
	}
	*size = GetFileSize(h, 0);

	fs = malloc( *size );
	ReadFile(h, fs, *size, &bytesRead, 0);
	CloseHandle(h);
	return fs;
}