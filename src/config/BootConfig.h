#ifndef __CONFIG_BOOT_CONFIG_H__
#define __CONFIG_BOOT_CONFIG_H__

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

#endif //__CONFIG_BOOT_CONFIG_H__
