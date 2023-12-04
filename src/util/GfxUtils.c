#include "DrawUtils.h"
#include "Except.h"
#include "GfxUtils.h"

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

static INT32 GetModeCommon(INT32 start, INT32 dir) {
    EFI_STATUS Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop);
    ASSERT_EFI_ERROR(Status);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
    UINTN sizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    INTN started = start;
    BOOLEAN looped_once = FALSE;
    do {
        start += dir;

        // Don't overflow
        if (dir == -1) {
            if (start < 0) {
                start = gop->Mode->MaxMode - 1;
            }
        } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            if (start >= gop->Mode->MaxMode) {
#pragma GCC diagnostic pop
                start = 0;
            }
        }

        // Handle a full loop
        if (started == start) {
            // If we get to this assert then it means there is no compatible mode
            ASSERT(!looped_once);
            looped_once = TRUE;
            break;
        }

        // Make sure this is a supported format
        Status = gop->QueryMode(gop, start, &sizeOfInfo, &info);
        ASSERT_EFI_ERROR(Status);
        if (info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            break;
        }
    } while (TRUE);

    return start;
}

INT32 GetFirstGfxMode() {
    return GetNextGfxMode(-1);
}

INT32 GetNextGfxMode(INT32 Current) {
    return GetModeCommon(Current, 1);
}

INT32 GetPrevGfxMode(INT32 Current) {
    return GetModeCommon(Current, -1);
}

INT32 GetBestGfxMode(UINT32 Width, UINT32 Height) {
    EFI_STATUS Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gop);
    ASSERT_EFI_ERROR(Status);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info = NULL;
    UINTN sizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    INT32 goodOption = 0;
    UINT32 BestWidth = 0;
    UINT32 BestHeight = 0;
    for (INTN i = 0; i < gop->Mode->MaxMode; ++i) {
        // Make sure that this is a supported format
        Status = gop->QueryMode(gop, i, &sizeOfInfo, &info);
        ASSERT_EFI_ERROR(Status);
        if (info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) {
            continue;
        }

        if (info->VerticalResolution == Height && info->HorizontalResolution == Width) {
            return i;
        }

        if (
            (info->VerticalResolution > Height && info->HorizontalResolution > Width) || (info->VerticalResolution < BestHeight && info->HorizontalResolution < BestWidth)) {
            // This is bigger than what we want or smaller than the best we got so far
            continue;
        }

        BestWidth = info->HorizontalResolution;
        BestHeight = info->VerticalResolution;
        goodOption = i;
    }

    return goodOption;
}
