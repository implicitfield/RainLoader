#include "DPUtils.h"
#include "Except.h"

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

BOOLEAN InsideDevicePath(EFI_DEVICE_PATH* All, EFI_DEVICE_PATH* One) {
    EFI_DEVICE_PATH* Path;
    for (
        Path = One;
        DevicePathNodeLength(Path) == DevicePathNodeLength(All) && !IsDevicePathEndType(Path) && CompareMem(Path, All, DevicePathNodeLength(All)) == 0;
        Path = NextDevicePathNode(Path), All = NextDevicePathNode(All)) {
        TRACE("MATCH");
    }

    return IsDevicePathEndType(Path);
}

EFI_DEVICE_PATH* LastDevicePathNode(EFI_DEVICE_PATH* Dp) {
    if (Dp == NULL) {
        return NULL;
    }

    EFI_DEVICE_PATH* LastDp = NULL;
    for (; !IsDevicePathEndType(Dp); Dp = NextDevicePathNode(Dp)) {
        LastDp = Dp;
    }

    return LastDp;
}

EFI_DEVICE_PATH* RemoveLastDevicePathNode(EFI_DEVICE_PATH* Dp) {
    if (Dp == NULL) {
        return NULL;
    }

    // Get the last node and calculate the new node size
    EFI_DEVICE_PATH* LastNode = LastDevicePathNode(Dp);
    UINTN Len = (UINTN)LastNode - (UINTN)Dp;

    // Allocate the new node with another one for the path end
    EFI_DEVICE_PATH* NewNode = AllocatePool(Len + sizeof(EFI_DEVICE_PATH));

    CopyMem(NewNode, Dp, Len);

    // Set the last one to be empty
    LastNode = (EFI_DEVICE_PATH*)((UINTN)NewNode + Len);
    SetDevicePathEndNode(LastNode);

    return NewNode;
}
