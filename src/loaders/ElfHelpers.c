#include "ElfHelpers.h"

#include <Library/FileHandleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <util/Except.h>
#include <util/FileUtils.h>
#include <util/MemUtils.h>

EFI_STATUS LoadElf(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs, CHAR16* Path, UINTN* Base, UINTN* Size) {
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* root = NULL;
    EFI_FILE_PROTOCOL* moduleImage = NULL;

    CHECK(Fs != NULL);
    CHECK(Path != NULL);

    EFI_CHECK(Fs->OpenVolume(Fs, &root));
    EFI_CHECK(root->Open(root, &moduleImage, Path, EFI_FILE_MODE_READ, 0));

    *Base = BASE_4GB - *Size - 1;
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
