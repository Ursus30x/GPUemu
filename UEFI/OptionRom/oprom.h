#ifndef OPROM_H
#define OPROM_H


#include "isa.h"
#include "vram.h"
#include "gpu_hw.h"
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

  UINT32                          MainFrameBufferWidth;
  UINT32                          MainFrameBufferHeight;

  FRAME_BUFFER_CONFIGURE          *FrameBufferBltConfigure;
  UINTN                           FrameBufferBltConfigureSize;

  EFI_PHYSICAL_ADDRESS            VRAMBaseAddr;

  EFI_GRAPHICS_OUTPUT_PROTOCOL Gop;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;

  GOP_3D_PROTOCOL           Gop3dProtocol;
} GPU_CONTEXT;

// Macro for accessing from GOP protocol
#define MY_GPU_PRIVATE_DATA_FROM_THIS(a) BASE_CR(a, GPU_CONTEXT, Gop)

// Macro for accessing from GOP3D protocol
#define MY_GPU_PRIVATE_DATA_FROM_GOP3D(a) BASE_CR(a, GPU_CONTEXT, Gop3dProtocol)

// Setup functions
EFI_STATUS EFIAPI GopSetup(IN OUT GPU_CONTEXT *Private);
EFI_STATUS EFIAPI Gop3DSetup(IN OUT GPU_CONTEXT *Private);

EFI_STATUS EFIAPI DoBusMasterWrite (
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN UINT8                *HostAddress,
  IN UINTN                 Length
);

#endif