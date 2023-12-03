#pragma once

#include <Uefi.h>

typedef struct {
    INT32 BootDelay;
    INT32 DefaultOS;
    INT32 GfxMode;
    BOOLEAN OverrideGfx;
    BOOLEAN DisableTimer;
} BOOT_CONFIG;

void LoadBootConfig(BOOT_CONFIG* config);

void SaveBootConfig(BOOT_CONFIG* config);
