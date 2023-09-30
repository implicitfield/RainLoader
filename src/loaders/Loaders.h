#ifndef __LOADERS_LOADERS_H__
#define __LOADERS_LOADERS_H__

#include <config/BootEntries.h>
#include <util/Except.h>

#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>

extern EFI_MEMORY_TYPE gKernelAndModulesMemoryType;

EFI_STATUS LoadBootModule(BOOT_MODULE* Module, UINTN* Base, UINTN* Size);

EFI_STATUS LoadLinuxKernel(BOOT_KERNEL_ENTRY* Entry);
EFI_STATUS LoadMB2Kernel(BOOT_KERNEL_ENTRY* Entry);

EFI_STATUS LoadKernel(BOOT_KERNEL_ENTRY* Entry);

#endif //__LOADERS_LOADERS_H__
