#include "../common.h"
#include "../hal_time.h"

#include <windows.h>

u32 ticks(void) { return GetTickCount(); }