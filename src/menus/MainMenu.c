#include "Menus.h"

#include <util/DrawUtils.h>

#include <config/BootConfig.h>
#include <config/BootEntries.h>
#include <util/Colors.h>

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/CpuLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/LoadedImage.h>
#include <loaders/Loaders.h>

static void draw() {
    BOOT_CONFIG config;
    LoadBootConfig(&config);

    ASSERT_EFI_ERROR(gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop));
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
    UINTN sizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    ASSERT_EFI_ERROR(gop->QueryMode(gop, config.GfxMode, &sizeOfInfo, &info));

    ClearScreen(WHITE);

    WriteAt(3, 1, "RainLoader v1");
    WriteAt(3, 2, "Copyright (c) 2020, TomatOrg");
    WriteAt(3, 3, "Copyright (c) 2023, implicitfield");

    EFI_TIME time;
    ASSERT_EFI_ERROR(gRT->GetTime(&time, NULL));
    WriteAt(3, 5, "Current time: %02d/%02d/%d %02d:%02d", time.Day, time.Month, time.Year, time.Hour, time.Minute);
    WriteAt(3, 6, "Graphics mode: %dx%d", info->HorizontalResolution, info->VerticalResolution);
    if (gDefaultEntry != NULL) {
        WriteAt(3, 7, "Current OS: %s (%s)", gDefaultEntry->Name, gDefaultEntry->Path);
    } else {
        WriteAt(3, 8, "No config file found!");
    }
    WriteAt(3, 9, "Firmware: %s (%08x)", gST->FirmwareVendor, gST->FirmwareRevision);
    WriteAt(3, 10, "UEFI Version: %d.%d", (gST->Hdr.Revision >> 16u) & 0xFFFFu, gST->Hdr.Revision & 0xFFFFu);

    WriteAt(3, 13, "Press B for BOOTMENU");
    WriteAt(3, 14, "Press S for SETUP");
    WriteAt(3, 15, "Press TAB for SHUTDOWN");
}

MENU EnterMainMenu(BOOLEAN first) {
    BOOT_CONFIG config;
    LoadBootConfig(&config);

    if (config.BootDelay <= 0 && gDefaultEntry != NULL) {
        LoadKernel(gDefaultEntry);
        while (1)
            CpuSleep();
    }

    draw();

    const UINTN TIMER_INTERVAL = 250000; // 1/40 sec
    const UINTN INITIAL_TIMEOUT_COUNTER = (config.BootDelay * 10000000) / TIMER_INTERVAL;
    const UINTN BAR_WIDTH = GetColumns();

    INTN timeout_counter = INITIAL_TIMEOUT_COUNTER;
    EFI_EVENT events[2] = {gST->ConIn->WaitForKey};
    ASSERT_EFI_ERROR(gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &events[1]));

    if (first && ContainsKernel()) {
        ASSERT_EFI_ERROR(gBS->SetTimer(events[1], TimerRelative, TIMER_INTERVAL));
    }

    UINTN count = 2;
    do {
        // Wait for a key press
        UINTN which = 0;
        EFI_INPUT_KEY key = {};
        ASSERT_EFI_ERROR(gBS->WaitForEvent(count, events, &which));

        if (which == 0) {
            EFI_STATUS status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
            if (status == EFI_NOT_READY) {
                continue;
            }
            ASSERT_EFI_ERROR(status);

            // Cancel the timer and destroy it
            if (count == 2) {
                ASSERT_EFI_ERROR(gBS->SetTimer(events[1], TimerCancel, 0));
                ASSERT_EFI_ERROR(gBS->CloseEvent(events[1]));
                count = 1;

                // Clear the progress bar
                for (UINTN i = 0; i < BAR_WIDTH; ++i) {
                    WriteAt(i, 22, " ");
                }
            }

            if (key.UnicodeChar == L'b' || key.UnicodeChar == L'B') {
                return MENU_BOOT_MENU;
            } else if (key.UnicodeChar == L's' || key.UnicodeChar == L'S') {
                return MENU_SETUP;
            } else if (key.UnicodeChar == CHAR_TAB) {
                return MENU_SHUTDOWN;
            }

        } else if (!config.DisableTimer) {
            // Timeout reached
            timeout_counter--;
            if (timeout_counter <= 0) {
                ASSERT_EFI_ERROR(gBS->CloseEvent(events[1]));

                LoadKernel(config.DefaultOS > 0 ? GetKernelEntryAt(config.DefaultOS) : gDefaultEntry);
            } else {
                // Write a new chunk of the bar
                int start = ((INITIAL_TIMEOUT_COUNTER - timeout_counter - 1) * BAR_WIDTH) / INITIAL_TIMEOUT_COUNTER;
                int end = ((INITIAL_TIMEOUT_COUNTER - timeout_counter) * BAR_WIDTH) / INITIAL_TIMEOUT_COUNTER;
                ActiveBackgroundColor = LIGHTGREY;
                for (int i = start; i <= end; ++i) {
                    WriteAt(i, 22, " ");
                }
                ActiveBackgroundColor = BackgroundColor;

                // Restart the timer
                ASSERT_EFI_ERROR(gBS->SetTimer(events[1], TimerRelative, TIMER_INTERVAL));
            }
        }
    } while (TRUE);
}
