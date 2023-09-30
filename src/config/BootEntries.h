#ifndef __CONFIG_CONFIG_H__
#define __CONFIG_CONFIG_H__

#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>

typedef enum {
    BOOT_ENTRY_KERNEL,
    BOOT_ENTRY_ACTION,
} BOOT_ENTRY_TYPE;

typedef enum {
    BOOT_ACTION_SHUTDOWN,
    BOOT_ACTION_REBOOT,
} BOOT_ACTION_TYPE;

typedef enum {
    BOOT_INVALID,
    BOOT_LINUX,
    BOOT_MB2,
} BOOT_PROTOCOL;

typedef struct {
    LIST_ENTRY Link;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs;
    CHAR16* Path;
    CHAR16* Tag;
} BOOT_MODULE;

typedef struct {
    BOOT_ACTION_TYPE Action;
    CHAR16* Name;
} BOOT_ACTION_ENTRY;

typedef struct {
    BOOT_PROTOCOL Protocol;
    CHAR16* Name;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs;
    CHAR16* Path;
    CHAR16* Cmdline;
    LIST_ENTRY BootModules;
} BOOT_KERNEL_ENTRY;

typedef struct {
    BOOT_ENTRY_TYPE EntryType;
    VOID* Entry;
    LIST_ENTRY Link;
} BOOT_ENTRY;

extern BOOT_KERNEL_ENTRY* gDefaultEntry;
extern LIST_ENTRY gBootEntries;

BOOT_KERNEL_ENTRY* GetKernelEntryAt(int i);
BOOLEAN ContainsKernel(VOID);

// Provides a linked list all the boot entries found in the configuration
// files across all detected filesystems
EFI_STATUS GetBootEntries(LIST_ENTRY* Head);

#endif //__CONFIG_CONFIG_H__
