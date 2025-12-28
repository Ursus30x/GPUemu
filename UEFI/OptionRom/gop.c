
#include "oprom.h"
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h> // gBS
#define MY_GPU_PRIVATE_DATA_FROM_THIS(a) BASE_CR(a, GPU_CONTEXT, Gop)

EFI_STATUS EFIAPI MyGpuBlt(
    IN  EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
    IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer  OPTIONAL,
    IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
    IN  UINTN                              SourceX,
    IN  UINTN                              SourceY,
    IN  UINTN                              DestinationX,
    IN  UINTN                              DestinationY,
    IN  UINTN                              Width,
    IN  UINTN                              Height,
    IN  UINTN                              Delta
    ) 
{
    GPU_CONTEXT *Private = MY_GPU_PRIVATE_DATA_FROM_THIS(This);
    // Performs a simple FrameBufferBlt form FrameBufferBltLib
    return FrameBufferBlt(
            Private->FrameBufferBltConfigure,
            BltBuffer,
            BltOperation,
            SourceX,
            SourceY,
            DestinationX,
            DestinationY,
            Width,
            Height,
            Delta
        );
}

EFI_STATUS EFIAPI MyGpuSetMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber
    ) {
  GPU_CONTEXT        *Private;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Black;
  EFI_STATUS Status;

  Private =  MY_GPU_PRIVATE_DATA_FROM_THIS(This);

  DEBUG((EFI_D_INFO, "hr %d vr %d\n FrameBufferBltConfigureSize: %d\n", 
    This->Mode->Info->HorizontalResolution,
    This->Mode->Info->VerticalResolution,
    Private->FrameBufferBltConfigureSize
  ));

  Status = FrameBufferBltConfigure (
      (VOID *)(UINTN)This->Mode->FrameBufferBase,
      This->Mode->Info,
      Private->FrameBufferBltConfigure,
      &Private->FrameBufferBltConfigureSize
      );


  if (Status == RETURN_BUFFER_TOO_SMALL) {
    DEBUG((EFI_D_ERROR, "ERROR: FrameBufferConfigureSize was too small\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (&Black, sizeof (Black));

  Status = FrameBufferBlt (
      Private->FrameBufferBltConfigure,
      &Black,
      EfiBltVideoFill,
      0, 0,
      0, 0,
      This->Mode->Info->HorizontalResolution,
      This->Mode->Info->VerticalResolution,
      0
      );

  ASSERT_RETURN_ERROR (Status);
  return EFI_SUCCESS;
}

//#define MY_GPU_PRIVATE_DATA_FROM_THIS(a) CR(a, GPU_CONTEXT, Gop, SIGNATURE_32('g','o','p','d'))
EFI_STATUS EFIAPI MyGpuQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    ) {
  GPU_CONTEXT *Private = MY_GPU_PRIVATE_DATA_FROM_THIS(This);


  if (ModeNumber >= This->Mode->MaxMode) {
    DEBUG ((EFI_D_INFO, "Bad parameter in QueryMode\n"));
    return EFI_INVALID_PARAMETER;
  }

  // Info must be a newly allocated pool
  *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  *Info = AllocateCopyPool (*SizeOfInfo, &Private->Info);

  //*Info = &Private->Info;
  DEBUG ((EFI_D_INFO, "Done query hr %d vr %d\n", (*Info)->HorizontalResolution, (*Info)->VerticalResolution));
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GopSetup(IN OUT GPU_CONTEXT *Private) {
  EFI_STATUS Status;

  // Initialize the GOP protocol
  Private->Gop.QueryMode = MyGpuQueryMode;
  Private->Gop.SetMode = MyGpuSetMode;
  Private->Gop.Blt = MyGpuBlt;

  // Fill in the mode information
  Private->Info.Version = 0;
  Private->Info.HorizontalResolution = Private->MainFrameBufferWidth;
  Private->Info.VerticalResolution = Private->MainFrameBufferHeight;
  Private->Info.PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
  Private->Info.PixelsPerScanLine = Private->Info.HorizontalResolution;

  Private->Gop.Mode = AllocateZeroPool(sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE));
  if (Private->Gop.Mode == NULL) {
    FreePool(Private);
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Gop.Mode->MaxMode = 1;
  Private->Gop.Mode->Mode = 0;
  Private->Gop.Mode->Info = &Private->Info;
  Private->Gop.Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  
  UINT32 FbSize = Private->Info.HorizontalResolution * Private->Info.VerticalResolution * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  Private->Gop.Mode->FrameBufferBase = Private->VRAMBaseAddr;
  Private->Gop.Mode->FrameBufferSize = FbSize;

  Private->FrameBufferBltConfigureSize = SIZE_8KB;
  Private->FrameBufferBltConfigure = AllocateZeroPool(SIZE_8KB);

  Status = Private->Gop.SetMode(&Private->Gop, 0);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to SetMode\n"));
    return Status;
  }

  DEBUG ((EFI_D_INFO, "Installing handle, with Private at %p\n", Private));
  return Status;
}