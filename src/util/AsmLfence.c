#include <Uefi.h>

VOID
EFIAPI
AsmLfence (
  VOID
  )
{
  __asm__ __volatile__ ("lfence");
}
