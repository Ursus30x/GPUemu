
#include "oprom.h"
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h> // gBS
#define MY_GPU_PRIVATE_DATA_FROM_THIS(a) BASE_CR(a, MY_GPU_PRIVATE_DATA, Gop)

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
    MY_GPU_PRIVATE_DATA *Private = MY_GPU_PRIVATE_DATA_FROM_THIS(This);
    
    // 1. Calculate the Buffer Stride (Width in Pixels of the source buffer)
    // If Delta is 0, Stride is just the Width. Delta is in Bytes.
    UINTN BufferStrideInPixels = (Delta == 0) ? Width : (Delta / sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    
    // 2. Variables for loops
    UINTN  Row, Col;
    UINT32 ScreenWidth = Private->Info.HorizontalResolution;
    UINT32 HardwarePixel;
    UINT64 VramOffset;
    // 3. Switch based on the requested operation
    switch (BltOperation) {
        
    case EfiBltVideoFill:
        // Case: Fill a rectangle on screen with a single color (BltBuffer[0])
        {
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL FillColor = *BltBuffer;
        
        // FIX: Change to BGR Format (Blue << 16, Red << 0)
        HardwarePixel = ((UINT32)FillColor.Blue  << 16) |  // Blue -> byte 2
                        ((UINT32)FillColor.Green << 8)  |  // Green -> byte 1
                         (UINT32)FillColor.Red;           // Red -> byte 0

        for (Row = 0; Row < Height; Row++) {
            for (Col = 0; Col < Width; Col++) {
                VramOffset = ((DestinationY + Row) * ScreenWidth + (DestinationX + Col)) * 4;
                Private->PciIo->Mem.Write (
                    Private->PciIo, EfiPciIoWidthUint32, 1, 
                    VramOffset, 1, &HardwarePixel
                );
            }
        }
    }
        break;

  case EfiBltBufferToVideo:
  {
      UINTN PixelSize = sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
      // Allocate a buffer for one scanline (required for batch writing)
      UINT32 *LineBuffer = AllocatePool(Width * PixelSize);
      if (LineBuffer == NULL) return EFI_OUT_OF_RESOURCES;

      for (Row = 0; Row < Height; Row++) {
          // 1. Get pointer to the source row
          EFI_GRAPHICS_OUTPUT_BLT_PIXEL *SrcRow = 
              &BltBuffer[(SourceY + Row) * BufferStrideInPixels + SourceX];
          
          // 2. Convert and copy the entire row to LineBuffer (Inner loop is necessary here)
          for (UINTN Col = 0; Col < Width; Col++) {
              // Use the BGR conversion that may have fixed the color:
              LineBuffer[Col] = ((UINT32)SrcRow[Col].Red  << 16) | 
                                ((UINT32)SrcRow[Col].Green << 8)  | 
                                  (UINT32)SrcRow[Col].Blue;
          }

          // 3. Write the entire converted row to VRAM in one batch
          VramOffset = ((DestinationY + Row) * ScreenWidth + DestinationX) * 4;
          
          Private->PciIo->Mem.Write (
              Private->PciIo, EfiPciIoWidthUint32, 1, 
              VramOffset, Width, LineBuffer // Batch write of 'Width' pixels
          );
      }
      FreePool(LineBuffer);
  }
break;

    case EfiBltVideoToVideo:
        // Copy VRAM to VRAM (Scrolling). 
        // NOTE: This is complex with PciIo because you must Read then Write.
        // For now, returning unsupported is safer than doing it wrong, 
        // or you must implement a Read loop here.
        // DEBUG((EFI_D_ERROR, "VideoToVideo Not Implemented yet\n"));
        return EFI_UNSUPPORTED; 

    default:
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI MyGpuSetMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber
    ) {
  DEBUG ((EFI_D_INFO, "setmode to %d\n", ModeNumber));
  MY_GPU_PRIVATE_DATA        *Private;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Black;
  EFI_STATUS Status;
  Private =  MY_GPU_PRIVATE_DATA_FROM_THIS(This);
  DEBUG((EFI_D_INFO, "hr %d vr %d\n FrameBufferBltConfigureSize: %d\n", This->Mode->Info->HorizontalResolution, This->Mode->Info->VerticalResolution,Private->FrameBufferBltConfigureSize));

  Status = FrameBufferBltConfigure (
      (VOID *)(UINTN)This->Mode->FrameBufferBase,
      This->Mode->Info,
      Private->FrameBufferBltConfigure,
      &Private->FrameBufferBltConfigureSize
      );
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    DEBUG((EFI_D_ERROR, "ERROR: was2small\n"));
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem (&Black, sizeof (Black));
  Status = FrameBufferBlt (
      Private->FrameBufferBltConfigure,
      &Black,
      EfiBltVideoFill,
      0,
      0,
      0,
      0,
      This->Mode->Info->HorizontalResolution,
      This->Mode->Info->VerticalResolution,
      0
      );
  ASSERT_RETURN_ERROR (Status);
  return EFI_SUCCESS;
}

//#define MY_GPU_PRIVATE_DATA_FROM_THIS(a) CR(a, MY_GPU_PRIVATE_DATA, Gop, SIGNATURE_32('g','o','p','d'))
EFI_STATUS EFIAPI MyGpuQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    ) {
  MY_GPU_PRIVATE_DATA *Private = MY_GPU_PRIVATE_DATA_FROM_THIS(This);
  DEBUG ((EFI_D_INFO, "in querymode for mode=%d\n", ModeNumber));

  if (ModeNumber >= This->Mode->MaxMode) {
    DEBUG ((EFI_D_INFO, "badparam\n"));
    return EFI_INVALID_PARAMETER;
  }
  // Info must be a newly allocated pool

  *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  *Info = AllocateCopyPool (*SizeOfInfo, &Private->Info);

  //*Info = &Private->Info;
  DEBUG ((EFI_D_INFO, "donequery hr %d vr %d\n", (*Info)->HorizontalResolution, (*Info)->VerticalResolution));
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GopSetup(IN OUT MY_GPU_PRIVATE_DATA *Private) {
  EFI_STATUS Status;

  // Initialize the GOP protocol
  Private->Gop.QueryMode = MyGpuQueryMode;
  Private->Gop.SetMode = MyGpuSetMode;
  Private->Gop.Blt = MyGpuBlt;
 DEBUG ((EFI_D_INFO, "Blsdsdsdadsadsadsdasdit\n"));
  // Fill in the mode information
  Private->Info.Version = 0;
  Private->Info.HorizontalResolution = 640; // hardcoded on the adapter
  Private->Info.VerticalResolution = 480;
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
  Private->Gop.Mode->FrameBufferBase = Private->PciFbMemBase;
  Private->Gop.Mode->FrameBufferSize = FbSize;

  Private->FrameBufferBltConfigureSize = SIZE_8KB;  // 4KB should be plenty
  Private->FrameBufferBltConfigure = AllocateZeroPool(SIZE_8KB);

  Status = Private->Gop.SetMode(&Private->Gop, 0);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "failed to setmode\n"));
    return Status;
  }

  DEBUG ((EFI_D_INFO, "installing handle, with private at %p\n", Private));
  return Status;
}