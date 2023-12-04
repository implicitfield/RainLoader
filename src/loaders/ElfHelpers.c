#include "ElfHelpers.h"

#include <Library/FileHandleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <util/Except.h>
#include <util/FileUtils.h>
#include <util/MemUtils.h>

#include <ElfLib/ElfLibInternal.h>

STATIC BOOLEAN IsStrtabShdr(UINT8* ImageBase, Elf64_Shdr* Shdr) {
    Elf64_Ehdr* Ehdr = (Elf64_Ehdr*)ImageBase;
    Elf64_Shdr* Namedr = GetElf64SectionByIndex(ImageBase, Ehdr->e_shstrndx);

    return (BOOLEAN)(AsciiStrCmp((CHAR8*)Ehdr + Namedr->sh_offset + Shdr->sh_name, ".strtab") == 0);
}

STATIC Elf64_Shdr* FindStrtabShdr(UINT8* ImageBase) {
    Elf64_Ehdr* Ehdr = (Elf64_Ehdr*)ImageBase;
    for (UINT32 i = 0; i < Ehdr->e_shnum; i++) {
        Elf64_Shdr* Shdr = GetElf64SectionByIndex(ImageBase, i);
        if (IsStrtabShdr(ImageBase, Shdr)) {
            return Shdr;
        }
    }
    return NULL;
}

STATIC const UINT8* GetSymName(UINT8* ImageBase, Elf64_Sym* Sym) {
    Elf64_Ehdr* Ehdr = (Elf64_Ehdr*)ImageBase;

    if (Sym->st_name == 0) {
        return NULL;
    }

    Elf64_Shdr* StrtabShdr = FindStrtabShdr(ImageBase);

    if (StrtabShdr == NULL) {
        return NULL;
    }

    ASSERT(Sym->st_name < StrtabShdr->sh_size);

    UINT8* StrtabContents = (UINT8*)Ehdr + StrtabShdr->sh_offset;

    BOOLEAN foundEnd = FALSE;

    for (UINT32 i = Sym->st_name; (i < StrtabShdr->sh_size) && !foundEnd; i++) {
        foundEnd = (BOOLEAN)(StrtabContents[i] == 0);
    }

    ASSERT(foundEnd);

    return StrtabContents + Sym->st_name;
}

EFI_STATUS ElfLookupSymbol(UINT8* ImageBase, CHAR8* TargetSymbolName, CHAR8 SymbolType, Elf64_Sym** Symbol) {
    EFI_STATUS Status = EFI_NOT_FOUND;

    CHECK(ImageBase != NULL && TargetSymbolName != NULL);

    Elf64_Ehdr* Ehdr = (Elf64_Ehdr*)ImageBase;
    for (UINT64 i = 0; i < Ehdr->e_shnum; i++) {
        // Determine if this is a symbol section.
        Elf64_Shdr* Shdr = GetElf64SectionByIndex(ImageBase, i);
        Elf64_Shdr* Namehdr = GetElf64SectionByIndex(ImageBase, Ehdr->e_shstrndx);

        if (AsciiStrCmp((CHAR8*)Ehdr + Namehdr->sh_offset + Shdr->sh_name, ".symtab") != 0)
            continue;

        UINT8* Symtab = (UINT8*)Ehdr + Shdr->sh_offset;
        UINT64 SymNum = (Shdr->sh_size) / (Shdr->sh_entsize);

        for (UINT32 SymIndex = 0; SymIndex < SymNum; SymIndex++) {
            Elf64_Sym* Sym = (Elf64_Sym*)(Symtab + SymIndex * Shdr->sh_entsize);

            const UINT8* SymName = GetSymName(ImageBase, Sym);
            if (SymName == NULL) {
                continue;
            }

            if (AsciiStrCmp((CHAR8*)SymName, TargetSymbolName) == 0) {
                if (Sym->st_shndx != SHN_UNDEF && Sym->st_value != 0 && ELF64_ST_TYPE(Sym->st_info) == SymbolType) {
                    *Symbol = Sym;
                    return EFI_SUCCESS;
                }
                return EFI_NOT_FOUND;
            }
        }
    }

cleanup:
    return Status;
}

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
