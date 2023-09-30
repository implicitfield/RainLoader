#include "BootEntries.h"
#include "BootConfig.h"

#include <util/Except.h>
#include <util/FileUtils.h>

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <util/DPUtils.h>

#define CHECK_OPTION(x) (StrnCmp(Line, x L"=", ARRAY_SIZE(x)) == 0)

BOOT_KERNEL_ENTRY* gDefaultEntry = NULL;
LIST_ENTRY gBootEntries = INITIALIZE_LIST_HEAD_VARIABLE(gBootEntries);

static CHAR16* ConfigPaths[] = {
    L"boot\\rainloader.cfg",
    L"rainloader.cfg",
    L"boot\\tomatboot.cfg",
    L"tomatboot.cfg",
};

static BOOLEAN ValidateKernelEntry(const BOOT_KERNEL_ENTRY* const Entry) {
    if (Entry == NULL) {
        return FALSE;
    } else if (Entry->Protocol == BOOT_INVALID) {
        return FALSE;
    } else if (Entry->Name == NULL) {
        return FALSE;
    } else if (Entry->Path == NULL) {
        return FALSE;
    }
    return TRUE;
}

BOOT_KERNEL_ENTRY* GetKernelEntryAt(int index) {
    int i = 0;
    for (LIST_ENTRY* Link = gBootEntries.ForwardLink; Link != &gBootEntries; Link = Link->ForwardLink, ++i) {
        if (index == i) {
            BOOT_ENTRY* TargetEntry = BASE_CR(Link, BOOT_ENTRY, Link);
            if (TargetEntry->EntryType == BOOT_ENTRY_KERNEL) {
                return TargetEntry->Entry;
            }
            return NULL;
        }
    }
    return NULL;
}

static CHAR16* CopyString(CHAR16* String) {
    return AllocateCopyPool((1 + StrLen(String)) * sizeof(CHAR16), String);
}

static EFI_STATUS ParseUri(CHAR16* Uri, EFI_SIMPLE_FILE_SYSTEM_PROTOCOL** OutFs, CHAR16** OutPath) {
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
    EFI_DEVICE_PATH* BootDevicePath = NULL;
    EFI_HANDLE* Handles = NULL;
    UINTN HandleCount = 0;

    CHECK(Uri != NULL);
    CHECK(OutFs != NULL);
    CHECK(OutPath != NULL);

    // separate the domain from the uri type
    CHAR16* Root = StrStr(Uri, L":");
    *Root = L'\0';
    Root += 3; // skip ://

    // create the path itself
    CHAR16* Path = StrStr(Root, L"/");
    *Path = L'\0';
    Path++; // skip the /
    *OutPath = CopyString(Path);

    // convert `/` to `\` for uefi
    for (CHAR16* C = *OutPath; *C != CHAR_NULL; C++) {
        if (*C == '/') {
            *C = '\\';
        }
    }

    // check the uri
    if (StrCmp(Uri, L"boot") == 0) {
        // boot://[<partition number>]/

        if (*Root == L'\0') {
            // The partition number is missing, meaning that the requested
            // path resides on the EFI partition.
        } else {
            UINTN PartNum = StrDecimalToUintn(Root);

            // we need the boot drive device path so we can check the filesystems
            // are on the same drive
            EFI_CHECK(gBS->HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage));
            EFI_CHECK(gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiDevicePathProtocolGuid, (void**)&BootDevicePath));

            // remove the last part which should be the partition itself
            BootDevicePath = RemoveLastDevicePathNode(BootDevicePath);
            CHECK(BootDevicePath != NULL);

            CHAR16* Text = ConvertDevicePathToText(BootDevicePath, TRUE, TRUE);
            FreePool(Text);

            // Iterate through all protocols
            EFI_CHECK(gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleCount, &Handles));
            int index = -1;
            for (UINTN i = 0; i < HandleCount; ++i) {
                EFI_DEVICE_PATH* DevicePath = NULL;
                EFI_CHECK(gBS->HandleProtocol(Handles[i], &gEfiDevicePathProtocolGuid, (void**)&DevicePath));

                Text = ConvertDevicePathToText(DevicePath, TRUE, TRUE);
                FreePool(Text);

                // Check this is part of the same drive
                if (!InsideDevicePath(DevicePath, BootDevicePath)) {
                    WARN("This is not on the same drive, next");
                    continue;
                }

                // Get the last one, and make sure it is a partition.
                DevicePath = LastDevicePathNode(DevicePath);
                if (DevicePathType(DevicePath) != MEDIA_DEVICE_PATH || DevicePathSubType(DevicePath) != MEDIA_HARDDRIVE_DP) {
                    // skip it
                    WARN("The last node is not a hard drive");
                    continue;
                }

                HARDDRIVE_DEVICE_PATH* Hd = (HARDDRIVE_DEVICE_PATH*)DevicePath;
                if (Hd->PartitionNumber == PartNum) {
                    index = i;
                    break;
                }
            }

            // Make sure we found it
            CHECK_TRACE(index != -1, "Could not find partition number `%d`", PartNum);

            // Get the filesystem and set it
            EFI_CHECK(gBS->HandleProtocol(Handles[index], &gEfiSimpleFileSystemProtocolGuid, (void**)OutFs));
        }
    } else if (StrCmp(Uri, L"guid") == 0 || StrCmp(Uri, L"uuid") == 0) {
        // guid://<guid>/

        // Parse the guid
        EFI_GUID Guid;
        EFI_CHECK(StrToGuid(Root, &Guid));

        // Iterate through all protocols
        EFI_CHECK(gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleCount, &Handles));
        int index = -1;
        for (UINTN i = 0; i < HandleCount; ++i) {
            EFI_DEVICE_PATH* DevicePath = NULL;
            EFI_CHECK(gBS->HandleProtocol(Handles[i], &gEfiDevicePathProtocolGuid, (void**)&DevicePath));

            // Get the last one, and make sure it is a partition.
            DevicePath = LastDevicePathNode(DevicePath);
            if (DevicePathType(DevicePath) != MEDIA_DEVICE_PATH || DevicePathSubType(DevicePath) != MEDIA_HARDDRIVE_DP) {
                continue;
            }

            HARDDRIVE_DEVICE_PATH* Hd = (HARDDRIVE_DEVICE_PATH*)DevicePath;
            if (Hd->SignatureType == SIGNATURE_TYPE_GUID && CompareGuid(&Guid, (EFI_GUID*)Hd->Signature)) {
                index = i;
                break;
            }
        }

        CHECK_TRACE(index != -1, "Could not find partition or fs with guid of `%s`", Root);

        // Get the filesystem from the guid
        EFI_CHECK(gBS->HandleProtocol(Handles[index], &gEfiSimpleFileSystemProtocolGuid, (void**)OutFs));
    } else {
        CHECK_FAIL_TRACE("Unsupported resource type `%s`", Uri);
    }

cleanup:
    if (Handles != NULL) {
        FreePool(Handles);
    }

    if (BootDevicePath != NULL) {
        FreePool(BootDevicePath);
    }

    return Status;
}

static EFI_STATUS LoadBootEntries(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FS, LIST_ENTRY* Head) {
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* root = NULL;
    EFI_FILE_PROTOCOL* file = NULL;

    // Open the configuration
    CHECK(FS != NULL);
    EFI_CHECK(FS->OpenVolume(FS, &root));

    for (UINTN i = 0; i < ARRAY_SIZE(ConfigPaths); ++i) {
        if (!EFI_ERROR(root->Open(root, &file, ConfigPaths[i], EFI_FILE_MODE_READ, 0))) {
            break;
        }

        file = NULL;
    }

    // Continue to next filesystem if no config was found
    if (file == NULL) {
        goto cleanup;
    }

    // Start from a clean state
    *Head = (LIST_ENTRY)INITIALIZE_LIST_HEAD_VARIABLE(*Head);
    BOOT_ENTRY* CurrentWrapperEntry = NULL;
    BOOT_MODULE* CurrentModuleString = NULL;

    while (TRUE) {
        CHAR16 Line[255] = {0};
        UINTN LineSize = sizeof(Line);
        BOOLEAN Ascii = TRUE;

        if (FileHandleEof(file)) {
            break;
        }
        EFI_CHECK(FileHandleReadLine(file, Line, &LineSize, FALSE, &Ascii));

        // New entry
        if (Line[0] == L':') {
            // Got a new entry
            BOOT_KERNEL_ENTRY* KernelEntry = NULL;
            KernelEntry = AllocateZeroPool(sizeof(BOOT_KERNEL_ENTRY));
            KernelEntry->Fs = FS;
            KernelEntry->Name = CopyString(Line + 1);
            KernelEntry->Protocol = BOOT_INVALID;
            KernelEntry->Cmdline = L"";
            KernelEntry->BootModules = (LIST_ENTRY)INITIALIZE_LIST_HEAD_VARIABLE(KernelEntry->BootModules);

            CurrentWrapperEntry = AllocateZeroPool(sizeof(BOOT_ENTRY));
            CurrentWrapperEntry->EntryType = BOOT_ENTRY_KERNEL;
            CurrentWrapperEntry->Entry = KernelEntry;
            InsertTailList(Head, &CurrentWrapperEntry->Link);
            CurrentModuleString = NULL;

            // Global keys
        } else if (CurrentWrapperEntry == NULL) {
            if (CHECK_OPTION(L"TIMEOUT")) {
                if (StrCmp(StrStr(Line, L"=") + 1, L"Disabled") == 0) {
                    gBootConfigOverride.DisableTimer = TRUE;
                } else {
                    gBootConfigOverride.DisableTimer = FALSE;
                    gBootConfigOverride.BootDelay = (INT32)StrDecimalToUintn(StrStr(Line, L"=") + 1);
                    if (gBootConfigOverride.BootDelay < 0)
                        gBootConfigOverride.BootDelay = 0;
                }
            } else if (CHECK_OPTION(L"DEFAULT_ENTRY")) {
                gBootConfigOverride.DefaultOS = (INT32)StrDecimalToUintn(StrStr(Line, L"=") + 1);
            }
        } else {
            // Local keys
            BOOT_KERNEL_ENTRY* CurrentEntry = CurrentWrapperEntry->Entry;
            if (CHECK_OPTION(L"PATH") || CHECK_OPTION(L"KERNEL_PATH")) {
                CHAR16* Path = StrStr(Line, L"=") + 1;
                CHECK_AND_RETHROW(ParseUri(Path, &CurrentEntry->Fs, &CurrentEntry->Path));
            } else if (CHECK_OPTION(L"CMDLINE") || CHECK_OPTION(L"KERNEL_CMDLINE")) {
                CurrentEntry->Cmdline = CopyString(StrStr(Line, L"=") + 1);
            } else if (CHECK_OPTION(L"PROTOCOL") || CHECK_OPTION(L"KERNEL_PROTO") || CHECK_OPTION(L"KERNEL_PROTOCOL")) {
                CHAR16* Protocol = StrStr(Line, L"=") + 1;

                if (StrCmp(Protocol, L"linux") == 0) {
                    CurrentEntry->Protocol = BOOT_LINUX;
                } else if (StrCmp(Protocol, L"mb2") == 0) {
                    CurrentEntry->Protocol = BOOT_MB2;
                } else {
                    CHECK_FAIL_TRACE("Unknown protocol `%s` for option `%s`", Protocol, CurrentEntry->Name);
                }
            } else if (CHECK_OPTION(L"MODULE_PATH")) {
                CHAR16* Path = StrStr(Line, L"=") + 1;

                BOOT_MODULE* Module = AllocateZeroPool(sizeof(BOOT_MODULE));
                Module->Fs = FS;
                Module->Tag = L"";
                CHECK_AND_RETHROW(ParseUri(Path, &Module->Fs, &Module->Path));
                InsertTailList(&CurrentEntry->BootModules, &Module->Link);

                // This is the next one which will need a string
                if (CurrentModuleString == NULL) {
                    CurrentModuleString = Module;
                }
            } else if (CHECK_OPTION(L"MODULE_STRING")) {
                CHECK_TRACE(
                    CurrentEntry->Protocol == BOOT_MB2,
                    "`MODULE_STRING` is only available for Multiboot2 (%d)", CurrentEntry->Protocol);
                CHECK_TRACE(CurrentModuleString != NULL, "MODULE_PATH must be provided before MODULE_STRING");

                CurrentModuleString->Tag = CopyString(StrStr(Line, L"=") + 1);

                if (IsNodeAtEnd(&CurrentEntry->BootModules, &CurrentModuleString->Link)) {
                    CurrentModuleString = NULL;
                } else {
                    CurrentModuleString = BASE_CR(GetNextNode(&CurrentEntry->BootModules, &CurrentModuleString->Link), BOOT_MODULE, Link);
                }
            }
        }
    }

cleanup:
    if (file != NULL) {
        FileHandleClose(file);
    }

    if (root != NULL) {
        FileHandleClose(root);
    }

    return Status;
}

static VOID AppendActionEntry(const BOOT_ACTION_TYPE Action, CHAR16* Name, LIST_ENTRY* Head) {
    BOOT_ACTION_ENTRY* Entry = NULL;
    Entry = AllocateZeroPool(sizeof(BOOT_ACTION_ENTRY));
    Entry->Action = Action;
    Entry->Name = Name;

    BOOT_ENTRY* CurrentWrapperEntry = NULL;
    CurrentWrapperEntry = AllocateZeroPool(sizeof(BOOT_ENTRY));
    CurrentWrapperEntry->EntryType = BOOT_ENTRY_ACTION;
    CurrentWrapperEntry->Entry = Entry;
    InsertTailList(Head, &CurrentWrapperEntry->Link);
}

BOOLEAN ContainsKernel(VOID) {
    for (LIST_ENTRY* Link = gBootEntries.ForwardLink; Link != &gBootEntries; Link = Link->ForwardLink) {
        if (BASE_CR(Link, BOOT_ENTRY, Link)->EntryType == BOOT_ENTRY_KERNEL) {
            return TRUE;
        }
    }
    return FALSE;
}

VOID FreeModules(LIST_ENTRY* Head) {
    LIST_ENTRY *Entry;
    LIST_ENTRY *Next;
    BASE_LIST_FOR_EACH_SAFE(Entry, Next, Head) {
        BOOT_MODULE* BootModule = BASE_CR(Entry, BOOT_MODULE, Link);
        FreePool(BootModule);
    }
}

EFI_STATUS GetBootEntries(LIST_ENTRY* Head) {
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
    EFI_DEVICE_PATH_PROTOCOL* BootDevicePath = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* BootFs = NULL;
    EFI_HANDLE FsHandle = NULL;

    // Get the boot image device path
    EFI_CHECK(gBS->HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage));
    EFI_CHECK(gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiDevicePathProtocolGuid, (void**)&BootDevicePath));

    // Locate the file system
    EFI_CHECK(gBS->LocateDevicePath(&gEfiSimpleFileSystemProtocolGuid, &BootDevicePath, &FsHandle));
    EFI_CHECK(gBS->HandleProtocol(FsHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&BootFs));

    // Try to load a config from it
    CHECK_AND_RETHROW(LoadBootEntries(BootFs, Head));

    // Validate all entries and drop invalid ones
    LIST_ENTRY *Entry;
    LIST_ENTRY *Next;
    BASE_LIST_FOR_EACH_SAFE(Entry, Next, Head) {
        BOOT_ENTRY* EntryWrapper = BASE_CR(Entry, BOOT_ENTRY, Link);
        if (!ValidateKernelEntry(EntryWrapper->Entry)) {
            RemoveEntryList(Entry);
            BOOT_KERNEL_ENTRY* KernelEntry = EntryWrapper->Entry;
            FreeModules(&KernelEntry->BootModules);
            FreePool(KernelEntry);
            FreePool(EntryWrapper);
        }
    }

    AppendActionEntry(BOOT_ACTION_SHUTDOWN, L"Shutdown", Head);
    AppendActionEntry(BOOT_ACTION_REBOOT, L"Reboot", Head);

cleanup:
    return Status;
}
