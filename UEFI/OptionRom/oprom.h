#include "isa.h"
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/Gop3D.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FrameBufferBltLib.h>
#include <stddef.h>

typedef struct {
  EFI_HANDLE Handle;
  EFI_PCI_IO_PROTOCOL             *PciIo;
  EFI_DEVICE_PATH_PROTOCOL        *GopDevicePath;

  FRAME_BUFFER_CONFIGURE          *FrameBufferBltConfigure;
  UINTN                           FrameBufferBltConfigureSize;

  EFI_PHYSICAL_ADDRESS            PciFbMemBase;

  EFI_GRAPHICS_OUTPUT_PROTOCOL Gop;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;

  GOP_3D_PROTOCOL           Gop3dProtocol;
  GPU_MODE                  CurrentMode;
} MY_GPU_PRIVATE_DATA;

// Macro for accessing from GOP protocol
#define MY_GPU_PRIVATE_DATA_FROM_THIS(a) BASE_CR(a, MY_GPU_PRIVATE_DATA, Gop)

// Macro for accessing from GOP3D protocol
#define MY_GPU_PRIVATE_DATA_FROM_GOP3D(a) BASE_CR(a, MY_GPU_PRIVATE_DATA, Gop3dProtocol)

// Setup functions
EFI_STATUS EFIAPI GopSetup(IN OUT MY_GPU_PRIVATE_DATA *Private);
EFI_STATUS EFIAPI Gop3DSetup(IN OUT MY_GPU_PRIVATE_DATA *Private);  // Add this line

EFI_STATUS EFIAPI DoBusMasterWrite (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT8                *HostAddress,
  IN UINTN                 Length
);