#include "BootConfig.h"

#include <util/Except.h>
#include <util/GfxUtils.h>

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

static BOOT_CONFIG ProtectedConfig = {
    .BootDelay = 4,
    .DefaultOS = 0,
    .GfxMode = -1,
    .OverrideGfx = FALSE,
    .DisableTimer = FALSE,
};

void LoadBootConfig(BOOT_CONFIG* config) {
    if (ProtectedConfig.GfxMode == -1)
        ProtectedConfig.GfxMode = GetFirstGfxMode();

    CopyMem(config, &ProtectedConfig, sizeof(ProtectedConfig));
}

void SaveBootConfig(BOOT_CONFIG* config) {
    CopyMem(&ProtectedConfig, config, sizeof(ProtectedConfig));
}
