#include "oprom.h"
#include <Protocol/Gop3D.h>
#include <Library/UefiBootServicesTableLib.h> // gBS

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

EFI_STATUS EFIAPI Gop3DSetup(IN OUT MY_GPU_PRIVATE_DATA *Private) {
  DEBUG((DEBUG_INFO, "GOP3D: Setting up GOP3D protocol\n"));
  
  // Initialize the GOP3D protocol function pointers
  Private->Gop3dProtocol.SetGpuMode = Gop3DSetGpuMode;
  
  // TODO: Add other functions when implemented
  // Private->Gop3dProtocol.SetCmdBuffer = Gop3DSetCmdBuffer;
  // Private->Gop3dProtocol.TransferDataBuffer = Gop3DTransferDataBuffer;
  // Private->Gop3dProtocol.TransferShaderBuffer = Gop3DTransferShaderBuffer;
  // Private->Gop3dProtocol.DrawFrame = Gop3DDrawFrame;
  // Private->Gop3dProtocol.PresentFrame = Gop3DPresentFrame;
  
  return EFI_SUCCESS;
}