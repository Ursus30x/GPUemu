#include "oprom.h"
#include <Protocol/Gop3D.h>
#include <Library/UefiBootServicesTableLib.h> // gBS
#include "gpu_hw.h"
EFI_STATUS
EFIAPI
Gop3DSetGpuMode (
  IN GOP_3D_PROTOCOL  *This,
  IN GPU_MODE          Mode
  )
{
  MY_GPU_PRIVATE_DATA *Private;
  EFI_STATUS           Status;
  UINT32               ModeValue;
  
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  
  if (Mode != MODE_GOP && Mode != MODE_3D) {
    return EFI_INVALID_PARAMETER;
  }
  
  Private = MY_GPU_PRIVATE_DATA_FROM_GOP3D(This);

  DEBUG((DEBUG_INFO, "GOP3D: Setting GPU mode to %d\n", Mode));
  
  ModeValue = (UINT32)Mode;
  
  // Write mode to GPU register
  Status = Private->PciIo->Mem.Write(
    Private->PciIo,
    EfiPciIoWidthUint32,
    GPU_MMIO_BAR,
    REG_GPU_MODE_ADDR,
    1,
    &ModeValue
  );
  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_ERROR, "GOP3D: Failed to set GPU mode: %r\n", Status));
    return EFI_DEVICE_ERROR;
  }
  
  Private->CurrentMode = Mode;
  
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Gop3DTransferDataBuffer (
  IN GOP_3D_PROTOCOL  *This,
  IN DATA_TYPE         DataType,
  IN VOID             *Data,
  IN UINT32            Size
  )
{
  MY_GPU_PRIVATE_DATA *Private;
  EFI_STATUS           Status;
  UINT64               BaseAddress;
  UINT32               i;
  UINT32               Count;

  if (This == NULL || Data == NULL || Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = MY_GPU_PRIVATE_DATA_FROM_GOP3D(This);

  if (DataType == VERTEX_BUFFER) {
    BaseAddress = 0;
    
    // Write vertices via PCI
    for (i = 0; i < Size; i += 4) {
      Status = Private->PciIo->Mem.Write(
        Private->PciIo,
        EfiPciIoWidthUint32,
        GPU_VRAM_BAR,
        BaseAddress + i,
        1,
        (UINT8*)Data + i
      );
      if (EFI_ERROR(Status)) {
        return Status;
      }
    }
    
    // Set vertex count (Size / sizeof(Vec3))
    Count = Size / sizeof(Vec3);
    Status = Private->PciIo->Mem.Write(
      Private->PciIo,
      EfiPciIoWidthUint32,
      GPU_MMIO_BAR,
      0,//REG_VERTEX_SIZE_ADDR
      1,
      &Count
    );
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }
  else if (DataType == EDGE_BUFFER) {
    BaseAddress = 0;//GPU_VRAM_EDGES_SEGMENT_ADDR;
    
    // Write edges via PCI
    for (i = 0; i < Size; i += 4) {
      Status = Private->PciIo->Mem.Write(
        Private->PciIo,
        EfiPciIoWidthUint32,
        GPU_VRAM_BAR,
        BaseAddress + i,
        1,
        (UINT8*)Data + i
      );
      if (EFI_ERROR(Status)) {
        return Status;
      }
    }
    
    // Set edge count (Size / sizeof(Edge))
    Count = Size / sizeof(Edge);
    Status = Private->PciIo->Mem.Write(
      Private->PciIo,
      EfiPciIoWidthUint32,
      GPU_MMIO_BAR,
      0,//REG_EDGE_SIZE_ADDR
      1,
      &Count
    );
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }
  else {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI Gop3DSetup(IN OUT MY_GPU_PRIVATE_DATA *Private) {
  DEBUG((DEBUG_INFO, "GOP3D: Setting up GOP3D protocol\n"));
  
  // Initialize the GOP3D protocol function pointers
  Private->Gop3dProtocol.SetGpuMode = Gop3DSetGpuMode;
  Private->Gop3dProtocol.TransferDataBuffer = Gop3DTransferDataBuffer;
  // TODO: Add other functions when implemented
  // Private->Gop3dProtocol.SetCmdBuffer = Gop3DSetCmdBuffer;
  // Private->Gop3dProtocol.TransferDataBuffer = Gop3DTransferDataBuffer;
  // Private->Gop3dProtocol.TransferShaderBuffer = Gop3DTransferShaderBuffer;
  // Private->Gop3dProtocol.DrawFrame = Gop3DDrawFrame;
  // Private->Gop3dProtocol.PresentFrame = Gop3DPresentFrame;
  
  return EFI_SUCCESS;
}