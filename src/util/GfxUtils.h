#pragma once

#include <Uefi.h>

INT32 GetFirstGfxMode();
INT32 GetNextGfxMode(INT32 Current);
INT32 GetPrevGfxMode(INT32 Current);

INT32 GetBestGfxMode(UINT32 Width, UINT32 Height);
