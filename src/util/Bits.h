#pragma once

#include <Uefi.h>

inline BOOLEAN GetNthBit(unsigned char c, unsigned char n) {
    return (c & (1 << n)) >> n;
}
