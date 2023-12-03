#pragma once

#include <config/BootEntries.h>
#include <util/Except.h>

#include <Uefi.h>

#include <Protocol/SimpleFileSystem.h>

EFI_STATUS LoadBootModule(BOOT_MODULE* Module, UINTN* Base, UINTN* Size);

EFI_STATUS LoadLinuxKernel(BOOT_KERNEL_ENTRY* Entry);
EFI_STATUS LoadMB2Kernel(BOOT_KERNEL_ENTRY* Entry);

EFI_STATUS LoadKernel(BOOT_KERNEL_ENTRY* Entry);
