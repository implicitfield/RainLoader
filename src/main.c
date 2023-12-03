#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/CpuLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <config/BootConfig.h>
#include <config/BootEntries.h>
#include <menus/Menus.h>
#include <util/Except.h>

// Define all constructors
extern EFI_STATUS EFIAPI UefiBootServicesTableLibConstructor(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable);
extern EFI_STATUS EFIAPI DxeDebugLibConstructor(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable);
extern EFI_STATUS EFIAPI UefiRuntimeServicesTableLibConstructor(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable);
extern RETURN_STATUS EFIAPI AcpiTimerLibConstructor(VOID);

EFI_MEMORY_TYPE gKernelAndModulesMemoryType = EfiLoaderData;

EFI_STATUS EFIAPI EfiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    EFI_STATUS Status = EFI_SUCCESS;

    // Call constructors
    EFI_CHECK(DxeDebugLibConstructor(ImageHandle, SystemTable));
    EFI_CHECK(UefiBootServicesTableLibConstructor(ImageHandle, SystemTable));
    EFI_CHECK(UefiRuntimeServicesTableLibConstructor(ImageHandle, SystemTable));

    CHECK(gST != NULL);
    CHECK(gBS != NULL);
    CHECK(gRT != NULL);

    AcpiTimerLibConstructor();

    // Disable the watchdog timer
    EFI_CHECK(gST->BootServices->SetWatchdogTimer(0, 0, 0, NULL));

    EFI_CHECK(gST->ConOut->ClearScreen(gST->ConOut));

    // Workaround for old AMI firmware
    if (StrCmp(gST->FirmwareVendor, L"American Megatrends") == 0 && gST->FirmwareRevision <= 0x0005000C) {
        gKernelAndModulesMemoryType = EfiMemoryMappedIOPortSpace;
    }

    // Load boot configs and set the default one
    BOOT_CONFIG config;
    LoadBootConfig(&config);
    CHECK_AND_RETHROW(GetBootEntries(&gBootEntries));
    gDefaultEntry = GetKernelEntryAt(config.DefaultOS);

    StartMenus();

cleanup:
    if (EFI_ERROR(Status)) {
        ASSERT_EFI_ERROR(Status);
    }

    while (1)
        CpuSleep();

    return EFI_SUCCESS;
}
