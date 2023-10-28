#pragma once

#include "Loaders.h"

#include <Uefi.h>

#include <Protocol/SimpleFileSystem.h>

#include <ElfLib.h>
#include <ElfLib/ElfCommon.h>

#include <ElfLib/Elf32.h>
#include <ElfLib/Elf64.h>

EFI_STATUS ElfLookupSymbol(UINT8* ImageBase, CHAR8* TargetSymbolName, CHAR8 SymbolType, Elf64_Sym** Symbol);
EFI_STATUS LoadElf(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs, CHAR16* Path, UINTN* Base, UINTN* Size);
