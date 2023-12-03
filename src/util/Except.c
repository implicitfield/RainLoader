#include "DrawUtils.h"
#include "Except.h"

static UINTN Row = 1;
static UINTN Column = 3;

void LinePrint(const CHAR8* fmt, ...) {
    VA_LIST marker;
    VA_START(marker, fmt);
    WriteAt(Column, Row++, fmt, marker);
    VA_END(marker);
}
