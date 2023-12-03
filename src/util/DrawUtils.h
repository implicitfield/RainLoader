#pragma once

#include <Uefi.h>

#include <Protocol/GraphicsOutput.h>

extern EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
extern UINT32 ActiveBackgroundColor;
extern UINT32 BackgroundColor;

UINT32 GetColumns();
UINT32 GetRows();
void PutChar(unsigned x_offset, unsigned y_offset, unsigned char c);
void WriteAt(unsigned x_offset, unsigned y_offset, const CHAR8* fmt, ...);
void ClearScreen(UINT32);
void FillBox(int _x, int _y, int width, int height, UINT32 color);
