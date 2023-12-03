#include "Menus.h"

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

MENU EnterMainMenu(BOOLEAN first);
MENU EnterSetupMenu();
MENU EnterBootMenu();

void StartMenus() {
    MENU current_menu = MENU_MAIN_MENU;
    BOOLEAN first = TRUE;

    while (TRUE) {
        switch (current_menu) {
            case MENU_MAIN_MENU:
                current_menu = EnterMainMenu(first);
                break;

            case MENU_BOOT_MENU:
                current_menu = EnterBootMenu();
                break;

            case MENU_SETUP:
                current_menu = EnterSetupMenu();
                break;

            case MENU_SHUTDOWN:
                gRT->ResetSystem(EfiResetShutdown, 0, 0, L"Shutdown");
                CpuDeadLoop();
                break;

            case MENU_REBOOT:
                gRT->ResetSystem(EfiResetCold, 0, 0, L"Reboot");
                CpuDeadLoop();
                break;
        }

        first = FALSE;
    }
}
