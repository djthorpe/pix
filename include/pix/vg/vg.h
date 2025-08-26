#pragma once
#include <stdlib.h>

#ifndef VG_MALLOC
#define VG_MALLOC malloc
#endif
#ifndef VG_FREE
#define VG_FREE free
#endif

#define VG_COLOR_NONE 0xDEADBEEFu
