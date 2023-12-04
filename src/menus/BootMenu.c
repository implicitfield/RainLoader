#include "Menus.h"

#include <config/BootEntries.h>
#include <util/Colors.h>
#include <util/DrawUtils.h>

#include <Uefi.h>

#include <Library/CpuLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <loaders/Loaders.h>

static void draw() {
    UINTN width = GetColumns();
    UINTN height = GetRows();

    ClearScreen(WHITE);

    FillBox(0, 0, (int)width, 1, LIGHTGREY);
    ActiveBackgroundColor = LIGHTGREY;
    WriteAt((int)(width / 2 - 6), 0, "RainLoader v1");
    ActiveBackgroundColor = BackgroundColor;

    FillBox(0, (int)(height - 2), (int)width, 1, LIGHTGREY);
}

static const char* loader_names[] = {
    [BOOT_LINUX] = "Linux Boot",
    [BOOT_MB2] = "Multiboot2",
};

MENU EnterBootMenu() {
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN width = GetColumns();

    draw();

    INTN selected = 0;
    BOOT_ENTRY* selectedEntry = NULL;

    while (TRUE) {
        // Draw the entries
        // TODO: Add a way to edit the command line
        INTN i = 0;
        for (LIST_ENTRY* link = gBootEntries.ForwardLink; link != &gBootEntries; link = link->ForwardLink, ++i) {
            BOOT_ENTRY* entry = BASE_CR(link, BOOT_ENTRY, Link);
            UINTN offset = 2;
            if (entry->EntryType == BOOT_ENTRY_ACTION) {
                ++offset;
            }

            // Draw the correct background
            if (i == selected) {
                selectedEntry = entry;
                FillBox(4, offset + i, (int)width - 8, 1, LIGHTGREY);
                ActiveBackgroundColor = LIGHTGREY;
            } else {
                FillBox(4, offset + i, (int)width - 8, 1, BackgroundColor);
            }

            // Write the option
            switch (entry->EntryType) {
                case BOOT_ENTRY_KERNEL:
                    BOOT_KERNEL_ENTRY* KernelEntry = entry->Entry;
                    WriteAt(6, offset + i, "%s (%s) - %a", KernelEntry->Name, KernelEntry->Path, loader_names[KernelEntry->Protocol]);
                    break;
                case BOOT_ENTRY_ACTION:
                    BOOT_ACTION_ENTRY* ActionEntry = entry->Entry;
                    WriteAt(6, offset + i, "%s", ActionEntry->Name);
                    break;
            }
            ActiveBackgroundColor = BackgroundColor;
        }

        UINTN which = 0;
        EFI_INPUT_KEY key = {};
        Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &which);
        ASSERT_EFI_ERROR(Status);
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            continue;
        }
        ASSERT_EFI_ERROR(Status);

        if (key.ScanCode == SCAN_DOWN) {
            ++selected;
            if (selected == i) {
                selected = 0;
            }

        } else if (key.ScanCode == SCAN_UP) {
            --selected;
            if (selected < 0) {
                selected = i - 1;
            }

        } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            switch (selectedEntry->EntryType) {
                case BOOT_ENTRY_KERNEL:
                    LoadKernel(selectedEntry->Entry);
                    while (1)
                        CpuSleep();
                    break;
                case BOOT_ENTRY_ACTION:
                    BOOT_ACTION_ENTRY* ActionEntry = selectedEntry->Entry;
                    switch (ActionEntry->Action) {
                        case BOOT_ACTION_SHUTDOWN:
                            return MENU_SHUTDOWN;
                        case BOOT_ACTION_REBOOT:
                            return MENU_REBOOT;
                    }
            }
        } else if (key.ScanCode == SCAN_ESC) {
            return MENU_MAIN_MENU;
        }
    }
}
