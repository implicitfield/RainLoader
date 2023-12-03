#pragma once

typedef enum {
    MENU_BOOT_MENU,
    MENU_MAIN_MENU,
    MENU_REBOOT,
    MENU_SETUP,
    MENU_SHUTDOWN,
} MENU;

void StartMenus();
