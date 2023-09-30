#include "Menus.h"

#include <util/DrawUtils.h>

#include <config/BootConfig.h>
#include <config/BootEntries.h>

#include <Uefi.h>
#include <Library/CpuLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <loaders/Loaders.h>

// (14x14) (28x14)
#define b EFI_LIGHTBLUE
#define B EFI_BLUE
#define W EFI_WHITE
// clang-format off
__attribute__((unused)) static CHAR8 DropletImage[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, b, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, b, b, B, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, b, b, b, 0, 0, 0, 0, 0,
        0, 0, 0, 0, b, b, b, b, B, 0, 0, 0, 0,
        0, 0, 0, 0, b, W, b, b, B, 0, 0, 0, 0,
        0, 0, 0, b, W, b, b, b, b, B, 0, 0, 0,
        0, 0, 0, b, W, b, b, b, B, B, 0, 0, 0,
        0, 0, 0, b, b, W, b, B, B, B, 0, 0, 0,
        0, 0, 0, 0, b, b, B, B, B, 0, 0, 0, 0,
        0, 0, 0, 0, 0, B, B, B, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
// clang-format on
#undef b
#undef B
#undef W

static void draw() {
    ClearScreen(EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));

    WriteAt(0, 1, "RainLoader v1");
    WriteAt(0, 2, "Copyright (c) 2020, TomatOrg");
    WriteAt(0, 3, "Copyright (c) 2023, implicitfield");

    UINTN width = 0;
    UINTN height = 0;
    ASSERT_EFI_ERROR(gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &width, &height));

    BOOT_CONFIG config;
    LoadBootConfig(&config);

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    ASSERT_EFI_ERROR(gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop));
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
    UINTN sizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    ASSERT_EFI_ERROR(gop->QueryMode(gop, config.GfxMode, &sizeOfInfo, &info));

    EFI_TIME time;
    ASSERT_EFI_ERROR(gRT->GetTime(&time, NULL));
    WriteAt(0, 5, "Current time: %d/%d/%d %d:%d", time.Day, time.Month, time.Year, time.Hour, time.Minute);
    WriteAt(0, 6, "Graphics mode: %dx%d", info->HorizontalResolution, info->VerticalResolution);
    if (gDefaultEntry != NULL) {
        WriteAt(0, 7, "Current OS: %s (%s)", gDefaultEntry->Name, gDefaultEntry->Path);
    } else {
        WriteAt(0, 8, "No config file found!");
    }
    WriteAt(0, 9, "Firmware: %s (%08x)", gST->FirmwareVendor, gST->FirmwareRevision);
    WriteAt(0, 10, "UEFI Version: %d.%d", (gST->Hdr.Revision >> 16u) & 0xFFFFu, gST->Hdr.Revision & 0xFFFFu);

    WriteAt(0, 13, "Press B for BOOTMENU");
    WriteAt(0, 14, "Press S for SETUP");
    WriteAt(0, 15, "Press TAB for SHUTDOWN");

    DrawImage(30 + ((width - 30) / 2) - 14, 1, DropletImage, 13, 14);
}

MENU EnterMainMenu(BOOLEAN first) {
    BOOT_CONFIG config;
    LoadBootConfig(&config);

    if ((config.BootDelay <= 0 || gBootConfigOverride.BootDelay == 0) && gDefaultEntry != NULL) {
        ASSERT_EFI_ERROR(gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK)));
        LoadKernel(gDefaultEntry);
        while (1)
            CpuSleep();
    }

    draw();
    ASSERT_EFI_ERROR(gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_RED, EFI_BLACK)));

    const UINTN TIMER_INTERVAL = 250000; // 1/40 sec
    const UINTN INITIAL_TIMEOUT_COUNTER = ((gBootConfigOverride.BootDelay >= 0 ? gBootConfigOverride.BootDelay : config.BootDelay) * 10000000) / TIMER_INTERVAL;
    const UINTN BAR_WIDTH = 80;

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
                ASSERT_EFI_ERROR(gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK)));
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

        } else if (!gBootConfigOverride.DisableTimer) {
            // Timeout reached
            timeout_counter--;
            if (timeout_counter <= 0) {
                ASSERT_EFI_ERROR(gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK)));

                ASSERT_EFI_ERROR(gBS->CloseEvent(events[1]));

                LoadKernel(gBootConfigOverride.DefaultOS > 0 ? GetKernelEntryAt(gBootConfigOverride.DefaultOS) : gDefaultEntry);
            } else {
                // Set the bar color
                ASSERT_EFI_ERROR(gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY)));

                // Write a new chunk of the bar
                int start = ((INITIAL_TIMEOUT_COUNTER - timeout_counter - 1) * BAR_WIDTH) / INITIAL_TIMEOUT_COUNTER;
                int end = ((INITIAL_TIMEOUT_COUNTER - timeout_counter) * BAR_WIDTH) / INITIAL_TIMEOUT_COUNTER;
                for (int i = start; i <= end; ++i) {
                    WriteAt(i, 22, " ");
                }

                // Restart the timer
                ASSERT_EFI_ERROR(gBS->SetTimer(events[1], TimerRelative, TIMER_INTERVAL));
            }
        }
    } while (TRUE);
}
