#pragma once

#include <Uefi.h>
#include <ProcessorBind.h>

extern EFI_MEMORY_TYPE gKernelAndModulesMemoryType;

// Don't use this function unless you *really* need to. This is extremely expensive compared to the standard AllocatePages function.
EFI_STATUS AllocateAlignedPagesInRange(EFI_MEMORY_TYPE MemoryType, UINTN PageCount, EFI_PHYSICAL_ADDRESS MinAddress, EFI_PHYSICAL_ADDRESS MaxAddress, UINTN Alignment, EFI_PHYSICAL_ADDRESS* Base);
