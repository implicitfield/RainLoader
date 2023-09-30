#include "BootConfig.h"

#include <util/Except.h>
#include <util/GfxUtils.h>

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>

static EFI_GUID gRainLoaderConfigGuid = {0xF3B9476E, 0x4DD5, 0x4825, {0xBE, 0xE7, 0x90, 0xA8, 0xB7, 0x7C, 0x05, 0x1E}};

static CHAR16* gRainLoaderConfigName = L"RainLoader";

BOOT_CONFIG gBootConfigOverride = {
    .BootDelay = -1,
    .DefaultOS = -1,
    .GfxMode = -1,
    .OverrideGfx = FALSE,
    .DisableTimer = FALSE,
};

void LoadBootConfig(BOOT_CONFIG* config) {
    UINT32 Attributes = 0;
    UINTN Size = sizeof(BOOT_CONFIG);

    EFI_STATUS Status = gRT->GetVariable(gRainLoaderConfigName, &gRainLoaderConfigGuid, &Attributes, &Size, config);
    if (Status == EFI_NOT_FOUND) {
        config->BootDelay = 4;
        config->DefaultOS = 0;
        config->GfxMode = GetFirstGfxMode();
        config->OverrideGfx = FALSE;
        config->DisableTimer = FALSE;

        SaveBootConfig(config);
    } else {
        ASSERT_EFI_ERROR(Status);
        // TODO: Validate the boot config
    }
}

void SaveBootConfig(BOOT_CONFIG* config) {
    ASSERT_EFI_ERROR(gRT->SetVariable(gRainLoaderConfigName, &gRainLoaderConfigGuid,
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS, sizeof(BOOT_CONFIG), config));
}
