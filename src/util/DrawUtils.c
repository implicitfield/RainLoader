#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "Bits.h"
#include "Colors.h"
#include "DrawUtils.h"
#include "Font.h"

#define PRINT_BUFFER_SIZE 256

EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
static CHAR8 PrintBuffer[PRINT_BUFFER_SIZE];
UINT32 ActiveBackgroundColor = WHITE;
UINT32 BackgroundColor = WHITE;

static inline void PlotPixel_32bpp(int x, int y, UINT32 pixel) {
    *((UINT32*)(gop->Mode->FrameBufferBase + 4 * gop->Mode->Info->PixelsPerScanLine * y + 4 * x)) = pixel;
}

UINT32 GetColumns() {
    return gop->Mode->Info->HorizontalResolution / 8;
}

UINT32 GetRows() {
    return gop->Mode->Info->VerticalResolution / 16;
}

void PutChar(unsigned x_offset, unsigned y_offset, unsigned char c) {
    if ((x_offset * 8 + 8 > gop->Mode->Info->HorizontalResolution) || (y_offset * 16 + 16 > gop->Mode->Info->VerticalResolution))
        return;

    for (unsigned loop_y = 0; loop_y < 16; ++loop_y) {
        UINTN ascii = (UINTN)c;
        unsigned char bytes = Font[ascii][loop_y];
        for (unsigned loop_x = 0; loop_x < 8; ++loop_x) {
            UINT32 color = ActiveBackgroundColor;
            if (GetNthBit(bytes, 8 - loop_x))
                color = 0;
            PlotPixel_32bpp(loop_x + x_offset * 8, loop_y + y_offset * 16, color);
        }
    }
}

void WriteAt(unsigned x_offset, unsigned y_offset, const CHAR8* fmt, ...) {
    SetMem(&PrintBuffer, PRINT_BUFFER_SIZE, 0);
    VA_LIST marker;
    VA_START(marker, fmt);
    AsciiVSPrint(&PrintBuffer[0], PRINT_BUFFER_SIZE, fmt, marker);
    VA_END(marker);
    for (const CHAR8* c = &PrintBuffer[0]; *c; ++c) {
        PutChar(x_offset, y_offset, *c);
        ++x_offset;
    }
}

void ClearScreen(UINT32 color) {
    SetMem32((VOID*)gop->Mode->FrameBufferBase, gop->Mode->FrameBufferSize, color);
}

void FillBox(int _x, int _y, int width, int height, UINT32 color) {
    _x *= 8;
    _y *= 16;
    width *= 8;
    height *= 16;

    for (int x = _x; x < _x + width; ++x) {
        for (int y = _y; y < _y + height; ++y) {
            PlotPixel_32bpp(x, y, color);
        }
    }
}
