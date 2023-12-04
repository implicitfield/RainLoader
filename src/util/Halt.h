#pragma once

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/CpuLib.h>

inline void NORETURN Halt() {
    while (TRUE) {
        DisableInterrupts();
        CpuSleep();
    }
}
