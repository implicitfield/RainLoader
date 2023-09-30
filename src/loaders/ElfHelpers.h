#pragma once

#include "Loaders.h"

#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>

EFI_STATUS LoadElf(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs, CHAR16* Path, UINTN* Base, UINTN* Size, UINTN MaxAddress);
