#pragma once

#include <Uefi.h>

#include <Library/DevicePathLib.h>
#include <Protocol/DevicePath.h>

BOOLEAN InsideDevicePath(EFI_DEVICE_PATH* All, EFI_DEVICE_PATH* One);

EFI_DEVICE_PATH* LastDevicePathNode(EFI_DEVICE_PATH* Dp);

EFI_DEVICE_PATH* RemoveLastDevicePathNode(EFI_DEVICE_PATH* Dp);
