#pragma once

#include <Uefi.h>

#include <Protocol/SimpleFileSystem.h>

EFI_STATUS FileRead(EFI_FILE_HANDLE Handle, void* Buffer, UINTN Size, UINTN Offset);
