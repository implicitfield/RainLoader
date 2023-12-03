#include "Loaders.h"
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <util/FileUtils.h>
#include <util/MemUtils.h>

EFI_STATUS LoadBootModule(BOOT_MODULE* Module, UINTN* Base, UINTN* Size) {
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* root = NULL;
    EFI_FILE_PROTOCOL* moduleImage = NULL;

    CHECK(Module != NULL);
    CHECK(Module->Fs != NULL);
    CHECK(Module->Path != NULL);

    EFI_CHECK(Module->Fs->OpenVolume(Module->Fs, &root));
    EFI_CHECK(root->Open(root, &moduleImage, Module->Path, EFI_FILE_MODE_READ, 0));

    *Base = BASE_4GB;
    EFI_CHECK(FileHandleGetSize(moduleImage, Size));
    EFI_CHECK(gBS->AllocatePages(AllocateMaxAddress, gKernelAndModulesMemoryType, EFI_SIZE_TO_PAGES(*Size), Base));
    CHECK_AND_RETHROW(FileRead(moduleImage, (void*)*Base, *Size, 0));

cleanup:
    if (root != NULL) {
        FileHandleClose(root);
    }

    if (moduleImage != NULL) {
        FileHandleClose(moduleImage);
    }

    if (EFI_ERROR(Status) && Base != NULL && Size != NULL) {
        gBS->FreePages(*Base, EFI_SIZE_TO_PAGES(*Size));
    }

    return Status;
}

EFI_STATUS LoadKernel(BOOT_KERNEL_ENTRY* Entry) {
    EFI_STATUS Status = EFI_SUCCESS;

    CHECK(Entry != NULL);

    switch (Entry->Protocol) {
        case BOOT_LINUX:
            CHECK_AND_RETHROW(LoadLinuxKernel(Entry));
            break;

        case BOOT_MB2:
            CHECK_AND_RETHROW(LoadMB2Kernel(Entry));
            break;

        default:
            CHECK_FAIL_TRACE("Unknown boot protocol");
    }

cleanup:
    return Status;
}
