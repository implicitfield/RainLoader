#include "Menus.h"

#include <config/BootConfig.h>
#include <config/BootEntries.h>
#include <util/Colors.h>
#include <util/DrawUtils.h>
#include <util/GfxUtils.h>

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

static void draw() {
    UINTN width = GetColumns();
    UINTN height = GetRows();

    ClearScreen(WHITE);

    WriteAt(width / 2 - AsciiStrLen("BOOT SETUP") / 2, 0, "BOOT SETUP");

    // Draw the controls
    UINTN controls_start = width - 18;

    WriteAt(controls_start, 2, "Press [-] to");
    WriteAt(controls_start, 3, "decrease value");

    WriteAt(controls_start, 5, "Press [+] to");
    WriteAt(controls_start, 6, "increase value");

    WriteAt(controls_start, 8, "Press ENTER");
    WriteAt(controls_start, 9, "to save and exit");

    WriteAt(controls_start, 11, "Press ESC to");
    WriteAt(controls_start, 12, "quit");

    FillBox(2, 1, width - 22, height - 2, LIGHTGREY);
}

#define IF_SELECTED(...)                                        \
    do {                                                        \
        if (selected == control_line) {                         \
            ActiveBackgroundColor = BackgroundColor;            \
            FillBox(2, control_line, width - 22, 1, LIGHTGREY); \
            __VA_ARGS__;                                        \
        } else {                                                \
            ActiveBackgroundColor = LIGHTGREY;                  \
        }                                                       \
    } while (0)

// Selectable operations
#define NO_OP (0)
#define OP_INC (1)
#define OP_DEC (2)

MENU EnterSetupMenu() {
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN width = GetColumns();
    draw();

    BOOT_CONFIG config = {};
    LoadBootConfig(&config);
    UINTN op = NO_OP;

    UINTN controls_start = 3;
    UINTN control_line_start = 2;
    UINTN selected = 2;
    do {
        UINTN control_line = control_line_start;

        ////////////////////////////////////////////////////////////////////////
        // Setup entries
        ////////////////////////////////////////////////////////////////////////

        /**
         * Boot delay: changes the amount of time (in seconds) it takes to boot by default
         * min: 1
         * max: 30
         */
        IF_SELECTED({
            // Check last button press
            if (op == OP_INC) {
                config.BootDelay++;
                if (config.BootDelay > 30) {
                    config.BootDelay = 30;
                }
            } else if (op == OP_DEC) {
                config.BootDelay--;
                if (config.BootDelay < 0) {
                    config.BootDelay = 0;
                }
            }
        });
        WriteAt(controls_start, control_line++, "Boot Delay: %d", config.BootDelay);

        /*
         * Graphics Mode: changes the resolution and format of the graphics buffer
         */
        IF_SELECTED({
            if (op == OP_INC) {
                config.GfxMode = GetNextGfxMode(config.GfxMode);
            } else if (op == OP_DEC) {
                config.GfxMode = GetPrevGfxMode(config.GfxMode);
            }
        });
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
        UINTN sizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
        Status = gop->QueryMode(gop, config.GfxMode, &sizeOfInfo, &info);
        ASSERT_EFI_ERROR(Status);
        WriteAt(controls_start, control_line++, "Graphics Mode: %dx%d (BGRA8)", info->HorizontalResolution, info->VerticalResolution);

        /**
         * Override Resolution: if false then we will only fall back to this resolution,
         * otherwise we will completely override the requested resolution
         */
        IF_SELECTED({
            // Check last button press
            if (op == OP_INC) {
                config.OverrideGfx = TRUE;
            } else if (op == OP_DEC) {
                config.OverrideGfx = FALSE;
            }
        });
        WriteAt(controls_start, control_line++, "Override Graphics: %a", config.OverrideGfx ? "True" : "False");

        /*
         * Default OS to load
         */
        IF_SELECTED({
            // Check last button press
            if (op == OP_INC) {
                config.DefaultOS++;
            } else if (op == OP_DEC) {
                config.DefaultOS--;
                if (config.DefaultOS < 0) {
                    config.DefaultOS = 0;
                }
            }
        });
        BOOT_KERNEL_ENTRY* entry = GetKernelEntryAt(config.DefaultOS);
        if (entry == NULL) {
            config.DefaultOS--;
            entry = GetKernelEntryAt(config.DefaultOS);
        }
        if (entry != NULL) {
            WriteAt(controls_start, control_line++, "Default OS: %s (%s)", entry->Name, entry->Path);
        } else {
            WriteAt(controls_start, control_line++, "Default OS: None");
        }

        ////////////////////////////////////////////////////////////////////////
        // Input handling
        ////////////////////////////////////////////////////////////////////////

        op = NO_OP;

        UINTN which = 0;
        EFI_INPUT_KEY key = {};
        Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &which);
        ASSERT_EFI_ERROR(Status);
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            continue;
        }

        ASSERT_EFI_ERROR(Status);

        if (key.UnicodeChar == L'-') {
            op = OP_DEC;
        } else if (key.UnicodeChar == L'+') {
            op = OP_INC;
        } else if (key.ScanCode == SCAN_DOWN) {
            selected++;
            if (selected >= control_line) {
                selected = control_line_start;
            }
        } else if (key.ScanCode == SCAN_UP) {
            selected--;
            if (selected < control_line_start) {
                selected = control_line - 1;
            }
        } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            SaveBootConfig(&config);
            ActiveBackgroundColor = BackgroundColor;
            return MENU_MAIN_MENU;
        } else if (key.ScanCode == SCAN_ESC) {
            // Quit without saving
            ActiveBackgroundColor = BackgroundColor;
            return MENU_MAIN_MENU;
        }
    } while (TRUE);
}
