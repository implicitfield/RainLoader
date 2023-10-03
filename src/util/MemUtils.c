#include "MemUtils.h"

#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <util/Except.h>

EFI_STATUS AllocateAlignedPagesInRange(EFI_MEMORY_TYPE MemoryType, UINTN PageCount, EFI_PHYSICAL_ADDRESS MinAddress, EFI_PHYSICAL_ADDRESS MaxAddress, UINTN Alignment, EFI_PHYSICAL_ADDRESS* Base) {
    EFI_STATUS Status = EFI_NOT_FOUND;
    UINT8 TmpMemoryMap[1];
    EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
    UINTN MemoryMapSize = sizeof(TmpMemoryMap);
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    *Base = 0;

    CHECK(gBS->GetMemoryMap(&MemoryMapSize, (EFI_MEMORY_DESCRIPTOR*)TmpMemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion) == EFI_BUFFER_TOO_SMALL);

    MemoryMapSize += EFI_PAGE_SIZE;
    MemoryMap = AllocatePool(MemoryMapSize);

    EFI_CHECK(gBS->GetMemoryMap(&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion));

    UINTN EntryCount = (MemoryMapSize / DescriptorSize);
    for (UINTN i = 0; i < EntryCount; ++i) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((UINTN)MemoryMap + DescriptorSize * i);

        if (desc->Type != EfiConventionalMemory || desc->NumberOfPages < PageCount)
            continue;

        EFI_PHYSICAL_ADDRESS RegionMax = desc->PhysicalStart + EFI_PAGES_TO_SIZE(desc->NumberOfPages);
        EFI_PHYSICAL_ADDRESS AlignedBase = ALIGN_VALUE(desc->PhysicalStart, Alignment);

        // FIXME: This isn't efficient.
        while (AlignedBase + EFI_PAGES_TO_SIZE(PageCount) < RegionMax - Alignment)
            AlignedBase += Alignment;

        *Base = AlignedBase;

        if (*Base > RegionMax || *Base > MaxAddress || *Base < desc->PhysicalStart || *Base < MinAddress)
            continue;

        EFI_CHECK(gBS->AllocatePages(AllocateAddress, gKernelAndModulesMemoryType, PageCount, Base));

        break;
    }

cleanup:
    if (MemoryMap != NULL)
        FreePool(MemoryMap);

    return Status;
}
