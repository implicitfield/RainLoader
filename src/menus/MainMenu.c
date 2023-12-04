#include "Menus.h"

#include <config/BootConfig.h>
#include <config/BootEntries.h>
#include <util/Colors.h>
#include <util/DrawUtils.h>

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
    EFI_STATUS Status = EFI_SUCCESS;
    BOOT_CONFIG config;
    LoadBootConfig(&config);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
    UINTN sizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    Status = gop->QueryMode(gop, config.GfxMode, &sizeOfInfo, &info);
    ASSERT_EFI_ERROR(Status);

    ClearScreen(WHITE);

    WriteAt(3, 1, "RainLoader v1");
    WriteAt(3, 2, "Copyright (c) 2020, TomatOrg");
    WriteAt(3, 3, "Copyright (c) 2023, implicitfield");

    EFI_TIME time;
    Status = gRT->GetTime(&time, NULL);
    ASSERT_EFI_ERROR(Status);
    WriteAt(3, 5, "Current time: %02d/%02d/%d %02d:%02d", time.Day, time.Month, time.Year, time.Hour, time.Minute);
    WriteAt(3, 6, "Graphics mode: %dx%d", info->HorizontalResolution, info->VerticalResolution);
    if (gDefaultEntry != NULL) {
        WriteAt(3, 7, "Current OS: %s (%s)", gDefaultEntry->Name, gDefaultEntry->Path);
    } else {
        WriteAt(3, 7, "No config file found!");
    }
    WriteAt(3, 9, "Firmware: %s (%08x)", gST->FirmwareVendor, gST->FirmwareRevision);
    WriteAt(3, 10, "UEFI Version: %d.%d", (gST->Hdr.Revision >> 16u) & 0xFFFFu, gST->Hdr.Revision & 0xFFFFu);

    WriteAt(3, 13, "Press B for BOOTMENU");
    WriteAt(3, 14, "Press S for SETUP");
    WriteAt(3, 15, "Press TAB for SHUTDOWN");
}

MENU EnterMainMenu(BOOLEAN first) {
    EFI_STATUS Status = EFI_SUCCESS;
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
    Status = gBS->CreateEvent(EVT_TIMER, TPL_CALLBACK, NULL, NULL, &events[1]);
    ASSERT_EFI_ERROR(Status);

    if (first && ContainsKernel()) {
        Status = gBS->SetTimer(events[1], TimerRelative, TIMER_INTERVAL);
        ASSERT_EFI_ERROR(Status);
    }

    UINTN count = 2;
    do {
        // Wait for a key press
        UINTN which = 0;
        EFI_INPUT_KEY key = {};
        Status = gBS->WaitForEvent(count, events, &which);
        ASSERT_EFI_ERROR(Status);

        if (which == 0) {
            Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
            if (Status == EFI_NOT_READY) {
                continue;
            }
            ASSERT_EFI_ERROR(Status);

            // Cancel the timer and destroy it
            if (count == 2) {
                Status = gBS->SetTimer(events[1], TimerCancel, 0);
                ASSERT_EFI_ERROR(Status);
                Status = gBS->CloseEvent(events[1]);
                ASSERT_EFI_ERROR(Status);
                count = 1;

                // Clear the progress bar
                for (UINTN i = 0; i < BAR_WIDTH; ++i) {
                    WriteAt(i, 18, " ");
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
                Status = gBS->CloseEvent(events[1]);
                ASSERT_EFI_ERROR(Status);

                LoadKernel(config.DefaultOS > 0 ? GetKernelEntryAt(config.DefaultOS) : gDefaultEntry);
            } else {
                // Write a new chunk of the bar
                int start = ((INITIAL_TIMEOUT_COUNTER - timeout_counter - 1) * BAR_WIDTH) / INITIAL_TIMEOUT_COUNTER;
                int end = ((INITIAL_TIMEOUT_COUNTER - timeout_counter) * BAR_WIDTH) / INITIAL_TIMEOUT_COUNTER;
                ActiveBackgroundColor = LIGHTGREY;
                for (int i = start; i <= end; ++i) {
                    WriteAt(i, 18, " ");
                }
                ActiveBackgroundColor = BackgroundColor;

                // Restart the timer
                Status = gBS->SetTimer(events[1], TimerRelative, TIMER_INTERVAL);
                ASSERT_EFI_ERROR(Status);
            }
        }
    } while (TRUE);
}
