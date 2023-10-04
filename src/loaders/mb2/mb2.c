#include "gdt.h"
#include "multiboot2.h"

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleFileSystem.h>

#include <Guid/Acpi.h>
#include <Library/CpuLib.h>
#include <config/BootConfig.h>
#include <config/BootEntries.h>
#include <loaders/Loaders.h>
#include <util/DrawUtils.h>
#include <util/Except.h>
#include <util/FileUtils.h>
#include <util/GfxUtils.h>
#include <util/MemUtils.h>

#include <ElfLib.h>
#include <ElfLib/ElfCommon.h>
#include <ElfLib/Elf32.h>
#include <ElfLib/Elf64.h>

#include <loaders/ElfHelpers.h>

static UINT8* mBootParamsBuffer = NULL;
static UINTN mBootParamsSize = 0;

extern void JumpToMB2Kernel(void* KernelStart, void* KernelParams);
extern void JumpToAMD64MB2Kernel(void* KernelStart, void* KernelParams);

// TODO: Make this force allocations below 4GB
static void* PushBootParams(void* data, UINTN size) {
    UINTN AllocationSize = ALIGN_VALUE(size, MULTIBOOT_TAG_ALIGN);

    if (mBootParamsBuffer == NULL) {
        mBootParamsBuffer = AllocatePool(AllocationSize);
    } else {
        UINT8* old = mBootParamsBuffer;
        mBootParamsBuffer = AllocateCopyPool(mBootParamsSize + AllocationSize, mBootParamsBuffer);
        FreePool(old);
    }

    if (data != NULL) {
        CopyMem(mBootParamsBuffer + mBootParamsSize, data, size);
    }

    UINT8* base = mBootParamsBuffer + mBootParamsSize;

    mBootParamsSize += AllocationSize;

    return base;
}

static multiboot_uint32_t EfiTypeToMB2Type[] = {
    [EfiReservedMemoryType] = MULTIBOOT_MEMORY_RESERVED,
    [EfiRuntimeServicesCode] = MULTIBOOT_MEMORY_RESERVED,
    [EfiRuntimeServicesData] = MULTIBOOT_MEMORY_RESERVED,
    [EfiMemoryMappedIO] = MULTIBOOT_MEMORY_RESERVED,
    [EfiMemoryMappedIOPortSpace] = MULTIBOOT_MEMORY_RESERVED,
    [EfiPalCode] = MULTIBOOT_MEMORY_RESERVED,
    [EfiUnusableMemory] = MULTIBOOT_MEMORY_BADRAM,
    [EfiACPIReclaimMemory] = MULTIBOOT_MEMORY_ACPI_RECLAIMABLE,
    [EfiLoaderCode] = MULTIBOOT_MEMORY_AVAILABLE,
    [EfiLoaderData] = MULTIBOOT_MEMORY_AVAILABLE,
    [EfiBootServicesCode] = MULTIBOOT_MEMORY_AVAILABLE,
    [EfiBootServicesData] = MULTIBOOT_MEMORY_AVAILABLE,
    [EfiConventionalMemory] = MULTIBOOT_MEMORY_AVAILABLE,
    [EfiACPIMemoryNVS] = MULTIBOOT_MEMORY_NVS};

static struct multiboot_header* LoadMB2Header(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs, CHAR16* file, UINTN* headerOff) {
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* root = NULL;
    EFI_FILE_PROTOCOL* mb2image = NULL;
    struct multiboot_header* ptr = NULL;

    Print(L"Loading image `%s`\n", file);
    EFI_CHECK(fs->OpenVolume(fs, &root));
    EFI_CHECK(root->Open(root, &mb2image, file, EFI_FILE_MODE_READ, 0));

    Print(L"Searching for mb2 header\n");
    struct multiboot_header header;
    for (int i = 0; i < MULTIBOOT_SEARCH; i += MULTIBOOT_HEADER_ALIGN) {
        CHECK_AND_RETHROW(FileRead(mb2image, &header, sizeof(header), i));

        // Check if this is a valid header
        if (header.magic == MULTIBOOT2_HEADER_MAGIC && header.architecture == MULTIBOOT_ARCHITECTURE_I386 && (header.checksum + header.magic + header.architecture + header.header_length) == 0) {
            *headerOff = i;

            ptr = AllocatePool(header.header_length);
            CHECK_AND_RETHROW(FileRead(mb2image, ptr, header.header_length, i));
            goto cleanup;
        }
    }

cleanup:
    if (root != NULL) {
        FileHandleClose(root);
    }

    if (mb2image != NULL) {
        FileHandleClose(mb2image);
    }

    return ptr;
}

static void GetBasicMemoryInfo(struct multiboot_tag_mmap* mmap, multiboot_uint32_t* lower, multiboot_uint32_t* upper) {
    *lower = 0;
    *upper = 0;

    for (UINTN i = 0; i < mmap->size; i++) {
        if (mmap->entries[i].type == MULTIBOOT_MEMORY_AVAILABLE) {
            if (mmap->entries[i].addr < 0x100000) {
                if (mmap->entries[i].addr + mmap->entries[i].len > 0x100000) {
                    UINTN low_len = 0x100000 - mmap->entries[i].addr;

                    *lower += low_len;
                    *upper += mmap->entries[i].len - low_len;
                } else {
                    *lower += mmap->entries[i].len;
                }
            } else {
                *upper += mmap->entries[i].len;
            }
        }
    }

    *lower /= 1024;
    *upper /= 1024;
}

EFI_STATUS LoadMB2Kernel(BOOT_KERNEL_ENTRY* Entry) {
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN HeaderOffset = 0;

    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

    BOOT_CONFIG config;
    LoadBootConfig(&config);

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    ASSERT_EFI_ERROR(gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop));
    ASSERT_EFI_ERROR(gop->SetMode(gop, (UINT32)config.GfxMode));

    struct multiboot_header* header = LoadMB2Header(Entry->Fs, Entry->Path, &HeaderOffset);
    CHECK_ERROR_TRACE(header != NULL, EFI_NOT_FOUND, "Could not find a valid multiboot2 header!");
    TRACE("Found header at offset %d", HeaderOffset);

    UINTN EntryAddressOverride = 0;
    UINTN EFIEntryAddressOverride = 0; // Ignored without PassBootServices.
    BOOLEAN MustHaveOldAcpi = FALSE;
    BOOLEAN MustHaveNewAcpi = FALSE;
    BOOLEAN NotElf = FALSE;
    BOOLEAN IsRelocatable = FALSE;
    BOOLEAN PassBootServices = FALSE; // Ignored without EFIEntryAddressOverride.

    mBootParamsSize = 8;
    mBootParamsBuffer = AllocatePool(8);

    for (struct multiboot_header_tag* tag = (struct multiboot_header_tag*)(header + 1);
         tag < (struct multiboot_header_tag*)((UINTN)header + header->header_length) && tag->type != MULTIBOOT_HEADER_TAG_END;
         tag = (struct multiboot_header_tag*)((UINTN)tag + ALIGN_VALUE(tag->size, MULTIBOOT_TAG_ALIGN))) {

        switch (tag->type) {
            case MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST:
                struct multiboot_header_tag_information_request* request = (void*)tag;
                for (multiboot_uint32_t i = 0;
                     i < (request->size - OFFSET_OF(struct multiboot_header_tag_information_request, requests)) / sizeof(multiboot_uint32_t);
                     ++i) {
                    multiboot_uint32_t r = request->requests[i];
                    switch (r) {
                        // These are all supported
                        case MULTIBOOT_TAG_TYPE_CMDLINE:
                        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                        case MULTIBOOT_TAG_TYPE_MODULE:
                        case MULTIBOOT_TAG_TYPE_MMAP:
                        case MULTIBOOT_TAG_TYPE_EFI_MMAP:
                        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                        case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
                        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                        case MULTIBOOT_TAG_TYPE_EFI_BS:
                        case MULTIBOOT_TAG_TYPE_EFI64:
                        case MULTIBOOT_TAG_TYPE_EFI64_IH:
                            break;

                        // These may not always be available
                        case MULTIBOOT_TAG_TYPE_ACPI_OLD:
                            MustHaveOldAcpi = !(tag->flags & MULTIBOOT_HEADER_TAG_OPTIONAL);
                            break;
                        case MULTIBOOT_TAG_TYPE_ACPI_NEW:
                            MustHaveNewAcpi = !(tag->flags & MULTIBOOT_HEADER_TAG_OPTIONAL);
                            break;

                        // Make sure any unknown requests are optional
                        default:
                            CHECK_ERROR_TRACE(tag->flags & MULTIBOOT_HEADER_TAG_OPTIONAL, EFI_UNSUPPORTED, "Requested tag %d which is unsupported!", r);
                    }
                }
                break;

            case MULTIBOOT_HEADER_TAG_ADDRESS: {
                NotElf = TRUE;
            } break;

            case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS: {
                struct multiboot_header_tag_entry_address* entry_address = (void*)tag;
                EntryAddressOverride = entry_address->entry_addr;
            } break;

            case MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS: {
                struct multiboot_header_tag_console_flags* console_flags = (void*)tag;
                if (console_flags->console_flags & MULTIBOOT_CONSOLE_FLAGS_CONSOLE_REQUIRED) {
                    CHECK_FAIL_TRACE("We do not support text mode");
                }
            } break;

            case MULTIBOOT_HEADER_TAG_FRAMEBUFFER: {
                struct multiboot_header_tag_framebuffer* framebuffer = (void*)tag;

                INT32 GfxMode = config.GfxMode;
                if (!config.OverrideGfx && framebuffer->width != 0 && framebuffer->height != 0) {
                    GfxMode = GetBestGfxMode(framebuffer->width, framebuffer->height);
                }

                ASSERT_EFI_ERROR(gop->SetMode(gop, (UINT32)GfxMode));
            } break;

            case MULTIBOOT_HEADER_TAG_MODULE_ALIGN: {
                // We always align modules
            } break;

            case MULTIBOOT_HEADER_TAG_EFI_BS: {
                PassBootServices = TRUE;
            } break;

            case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI32: {
                if (!(tag->flags & MULTIBOOT_HEADER_TAG_OPTIONAL)) {
                    CHECK_FAIL_TRACE("We do not support EFI boot for i386");
                }
            } break;

            case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64: {
                struct multiboot_header_tag_entry_address* entry_address = (void*)tag;
                EFIEntryAddressOverride = entry_address->entry_addr;
            } break;

            case MULTIBOOT_HEADER_TAG_RELOCATABLE: {
                IsRelocatable = TRUE;
            } break;

            default:
                CHECK_FAIL_TRACE("Unsupported tag type %d", tag->type);
        }
    }

    {
        TRACE("Pushing cmdline");
        UINTN size = StrLen(Entry->Cmdline) + 1 + OFFSET_OF(struct multiboot_tag_string, string);
        struct multiboot_tag_string* string = PushBootParams(NULL, size);
        string->type = MULTIBOOT_TAG_TYPE_CMDLINE;
        string->size = size;
        UINTN length = StrLen(Entry->Cmdline);
        if (length == 0) {
            AsciiStrCpyS(string->string, 1, "");
        } else {
            UnicodeStrToAsciiStrS(Entry->Cmdline, string->string, StrLen(Entry->Cmdline) + 1);
        }
    }

    {
        TRACE("Pushing bootloader name");
        UINTN size = sizeof("RainLoader-v1") + OFFSET_OF(struct multiboot_tag_string, string);
        struct multiboot_tag_string* string = PushBootParams(NULL, size);
        string->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
        string->size = size;
        AsciiStrCpyS(string->string, sizeof("RainLoader-v1"), "RainLoader-v1");
    }

    TRACE("Pushing modules");
    for (LIST_ENTRY* Link = Entry->BootModules.ForwardLink; Link != &Entry->BootModules; Link = Link->ForwardLink) {
        BOOT_MODULE* Module = BASE_CR(Link, BOOT_MODULE, Link);
        UINTN Start = 0;
        UINTN Size = 0;
        CHECK_AND_RETHROW(LoadBootModule(Module, &Start, &Size));

        UINTN TotalTagSize = OFFSET_OF(struct multiboot_tag_module, cmdline) + StrLen(Module->Tag) + 1;
        struct multiboot_tag_module* mod = PushBootParams(NULL, TotalTagSize);
        mod->size = TotalTagSize;
        mod->type = MULTIBOOT_TAG_TYPE_MODULE;
        mod->mod_start = Start;
        mod->mod_end = Start + Size;
        UnicodeStrToAsciiStrS(Module->Tag, mod->cmdline, StrLen(Module->Tag));

        TRACE("    Added %s (%s) -> %p - %p", Module->Tag, Module->Path, mod->mod_start, mod->mod_end);
    }

    TRACE("Pushing framebuffer info");
    struct multiboot_tag_framebuffer framebuffer = {
        .common = {
            .type = MULTIBOOT_TAG_TYPE_FRAMEBUFFER,
            .size = sizeof(struct multiboot_tag_framebuffer),
            .framebuffer_addr = gop->Mode->FrameBufferBase,
            .framebuffer_pitch = gop->Mode->Info->PixelsPerScanLine * 4,
            .framebuffer_width = gop->Mode->Info->HorizontalResolution,
            .framebuffer_height = gop->Mode->Info->VerticalResolution,
            .framebuffer_bpp = 32,
            .framebuffer_type = MULTIBOOT_FRAMEBUFFER_TYPE_RGB},
        .framebuffer_red_field_position = 16,
        .framebuffer_red_mask_size = 8,
        .framebuffer_green_field_position = 8,
        .framebuffer_green_mask_size = 8,
        .framebuffer_blue_field_position = 0,
        .framebuffer_blue_mask_size = 8};
    PushBootParams(&framebuffer, sizeof(framebuffer));

    void* acpi10table;
    if (!EFI_ERROR(EfiGetSystemConfigurationTable(&gEfiAcpi10TableGuid, &acpi10table))) {
        // RSDP is 20 bytes long
        TRACE("Pushing old ACPI info");
        struct multiboot_tag_old_acpi* old_acpi = PushBootParams(NULL, 20 + OFFSET_OF(struct multiboot_tag_old_acpi, rsdp));
        old_acpi->size = 20 + OFFSET_OF(struct multiboot_tag_old_acpi, rsdp);
        old_acpi->type = MULTIBOOT_TAG_TYPE_ACPI_OLD;
        CopyMem(old_acpi->rsdp, acpi10table, 20);
    } else if (MustHaveOldAcpi) {
        CHECK_FAIL_TRACE("Old ACPI Table is not present");
    }

    void* acpi20table;
    if (!EFI_ERROR(EfiGetSystemConfigurationTable(&gEfiAcpi20TableGuid, &acpi20table))) {
        // XSDP is 36 bytes long
        TRACE("Pushing new ACPI info");
        struct multiboot_tag_new_acpi* new_acpi = PushBootParams(NULL, 36 + OFFSET_OF(struct multiboot_tag_new_acpi, rsdp));
        new_acpi->size = 36 + OFFSET_OF(struct multiboot_tag_new_acpi, rsdp);
        new_acpi->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
        CopyMem(new_acpi->rsdp, acpi20table, 36);
    } else if (MustHaveNewAcpi) {
        CHECK_FAIL_TRACE("New ACPI Table is not present");
    }

    if (NotElf) {
        CHECK_FAIL_TRACE("Raw images are not supported");
    }

    UINTN KernelSize = 0;
    ELF_IMAGE_CONTEXT Context;
    VOID* Elf = NULL;
    ZeroMem(&Context, sizeof(Context));

    CHECK_AND_RETHROW(LoadElf(Entry->Fs, Entry->Path, (UINTN*)&Elf, &KernelSize));
    CHECK_AND_RETHROW(ParseElfImage(Elf, &Context));

    EFI_PHYSICAL_ADDRESS Base = (EFI_PHYSICAL_ADDRESS)Context.PreferredImageAddress;
    EFI_CHECK(gBS->AllocatePages(AllocateAddress, gKernelAndModulesMemoryType, EFI_SIZE_TO_PAGES(Context.ImageSize), &Base));
    Context.ImageAddress = Context.PreferredImageAddress;

    CHECK_AND_RETHROW(LoadElfImage(&Context));
    TRACE("Loaded ELF image into memory");

    if (PassBootServices && EFIEntryAddressOverride != 0) {
        TRACE("Pushing boot services");
        UINTN size = sizeof(struct multiboot_tag);
        struct multiboot_tag* bs = PushBootParams(NULL, size);
        bs->type = MULTIBOOT_TAG_TYPE_EFI_BS;
        bs->size = size;
    } else {
        EntryAddressOverride = Context.EntryPoint;
    }

    {
        TRACE("Pushing EFI system table");
        UINTN size = sizeof(struct multiboot_tag_efi64);
        struct multiboot_tag_efi64* system_table = PushBootParams(NULL, size);
        system_table->type = MULTIBOOT_TAG_TYPE_EFI64;
        system_table->size = size;
        system_table->pointer = (multiboot_uint64_t)gST;
    }

    {
        TRACE("Pushing EFI image handle");
        UINTN size = sizeof(struct multiboot_tag_efi64_ih);
        struct multiboot_tag_efi64_ih* image_handle = PushBootParams(NULL, size);
        image_handle->type = MULTIBOOT_TAG_TYPE_EFI64_IH;
        image_handle->size = size;
        image_handle->pointer = (multiboot_uint64_t)gImageHandle;
    }

#define PushELF(ELFTYPE)                                                                                     \
    ELFTYPE* Ehdr = (ELFTYPE*)Context.ImageAddress;                                                          \
    UINTN Size = OFFSET_OF(struct multiboot_tag_elf_sections, sections) + Ehdr->e_shnum * Ehdr->e_shentsize; \
    struct multiboot_tag_elf_sections* sections = PushBootParams(NULL, Size);                                \
    sections->size = Size;                                                                                   \
    sections->type = MULTIBOOT_TAG_TYPE_ELF_SECTIONS;                                                        \
    sections->entsize = Ehdr->e_shentsize;                                                                   \
    sections->num = Ehdr->e_shnum;                                                                           \
    sections->shndx = Ehdr->e_shstrndx;                                                                      \
    CopyMem(sections->sections, Context.ImageAddress + Ehdr->e_shoff, Ehdr->e_shnum * Ehdr->e_shentsize);

    TRACE("Pushing ELF info");
    if (Context.EiClass == ELFCLASS32) {
        PushELF(Elf32_Ehdr);
    } else {
        CHECK(Context.EiClass == ELFCLASS64);
        PushELF(Elf64_Ehdr);
    }

#undef PushELF

    if (IsRelocatable) {
        TRACE("Pushing load base address");
        struct multiboot_tag_load_base_addr* load_base_addr = PushBootParams(NULL, sizeof(struct multiboot_tag_load_base_addr));
        load_base_addr->type = MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR;
        load_base_addr->size = sizeof(struct multiboot_tag_load_base_addr);
        load_base_addr->load_base_addr = (multiboot_uint32_t)(UINTN)Context.ImageAddress;
    }

    TRACE("Allocating area for GDT");
    InitLinuxDescriptorTables();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // No prints from here
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    UINT8 TmpMemoryMap[1];
    UINTN MemoryMapSize = sizeof(TmpMemoryMap);
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    CHECK(gBS->GetMemoryMap(&MemoryMapSize, (EFI_MEMORY_DESCRIPTOR*)TmpMemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion) == EFI_BUFFER_TOO_SMALL);

    // Allocate space for the efi memory map and take
    // into account that there will be changes
    MemoryMapSize += EFI_PAGE_SIZE;
    EFI_MEMORY_DESCRIPTOR* MemoryMap = AllocatePool(MemoryMapSize);

    // Allocate all the space we will need
    UINT8* start_from = PushBootParams(NULL, OFFSET_OF(struct multiboot_tag_efi_mmap, efi_mmap) + MemoryMapSize + OFFSET_OF(struct multiboot_tag_mmap, entries) + MemoryMapSize + sizeof(struct multiboot_tag_basic_meminfo));

    EFI_CHECK(gBS->GetMemoryMap(&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion));
    UINTN EntryCount = (MemoryMapSize / DescriptorSize);

    if (!PassBootServices || EFIEntryAddressOverride == 0) {
        EFI_CHECK(gBS->ExitBootServices(gImageHandle, MapKey));
    }

    struct multiboot_tag_mmap* mmap = (void*)start_from;
    mmap->type = MULTIBOOT_TAG_TYPE_MMAP;
    mmap->entry_size = sizeof(struct multiboot_mmap_entry);
    mmap->entry_version = 0;
    mmap->size = OFFSET_OF(struct multiboot_tag_mmap, entries) + EntryCount * sizeof(struct multiboot_mmap_entry);
    for (UINTN i = 0; i < EntryCount; ++i) {
        struct multiboot_mmap_entry* entry = &mmap->entries[i];
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((UINTN)MemoryMap + DescriptorSize * i);
        entry->type = EfiTypeToMB2Type[desc->Type];
        entry->addr = desc->PhysicalStart;
        entry->len = EFI_PAGES_TO_SIZE(desc->NumberOfPages);
        entry->zero = 0;
    }

    struct multiboot_tag_efi_mmap* efi_mmap = (struct multiboot_tag_efi_mmap*)ALIGN_VALUE((UINTN)mmap + mmap->size, MULTIBOOT_TAG_ALIGN);
    efi_mmap->size = MemoryMapSize + OFFSET_OF(struct multiboot_tag_efi_mmap, efi_mmap);
    efi_mmap->type = MULTIBOOT_TAG_TYPE_EFI_MMAP;
    efi_mmap->descr_size = DescriptorSize;
    efi_mmap->descr_vers = DescriptorVersion;
    CopyMem(efi_mmap->efi_mmap, MemoryMap, MemoryMapSize);

    struct multiboot_tag_basic_meminfo* basic_meminfo = (struct multiboot_tag_basic_meminfo*)ALIGN_VALUE((UINTN)efi_mmap + efi_mmap->size, MULTIBOOT_TAG_ALIGN);
    basic_meminfo->type = MULTIBOOT_TAG_TYPE_BASIC_MEMINFO;
    basic_meminfo->size = sizeof(struct multiboot_tag_basic_meminfo);
    GetBasicMemoryInfo(mmap, &basic_meminfo->mem_lower, &basic_meminfo->mem_upper);

    struct multiboot_tag* end_tag = (struct multiboot_tag*)ALIGN_VALUE((UINTN)basic_meminfo + basic_meminfo->size, MULTIBOOT_TAG_ALIGN);
    end_tag->type = MULTIBOOT_TAG_TYPE_END;
    end_tag->size = sizeof(struct multiboot_tag);

    if (PassBootServices && EFIEntryAddressOverride != 0) {
        JumpToAMD64MB2Kernel((void*)(EFIEntryAddressOverride), mBootParamsBuffer);
    } else {
        DisableInterrupts();

        // Setup GDT and IDT
        SetLinuxDescriptorTables();

        JumpToMB2Kernel((void*)EntryAddressOverride, mBootParamsBuffer);
    }

    // Sleep if we ever return
    while (1)
        CpuSleep();

cleanup:
    if (header != NULL) {
        FreePool(header);
    }

    return Status;
}
