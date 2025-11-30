#ifndef __GOP_3D_H__
#define __GOP_3D_H__

#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FrameBufferBltLib.h>
#include <stddef.h>

#define GOP_3D_PROTOCOL_GUID \
  { 0x12345678, 0x1234, 0x5678, \
    { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 } }

typedef struct GOP_3D_PROTOCOL GOP_3D_PROTOCOL;

// TODO: Shouldnt this enum, structs and defines be shared with QEMU gpu emulation code?
enum GPU_MODE{
  MODE_GOP,
  MODE_3D
};

enum DATA_TYPE{
  VERTEX_BUFFER,
  EDGE_BUFFER
};

enum SHADER_TYPE{
  VERTEX_SHADER,
  FRAGMENT_SHADER
};

/* ------------------------- Function declarations ------------------------- */

// Direct write to command buffer memory
typedef EFI_STATUS (EFIAPI *GOP_3D_SET_CMD_BUFF)(
  IN GOP_3D_PROTOCOL  *This,
  IN VOID             *CmdBufferConfig
);

// Setting GPU mode
typedef EFI_STATUS (EFIAPI *GOP_3D_SET_GPU_MODE)(
  IN GOP_3D_PROTOCOL  *This,
  IN GPU_MODE          Mode
);

// Transfer POD data to GPU memory
typedef EFI_STATUS (EFIAPI *GOP_3D_TRANSFER_DATA_BUFFER)(
  IN GOP_3D_PROTOCOL  *This,
  IN DATA_TYPE         DataType,
  IN VOID             *Data,
  IN UINT32            Size
);

// Transfer shader code to GPU memory
typedef EFI_STATUS (EFIAPI *GOP_3D_TRANSFER_SHADER_BUFFER)(
  IN GOP_3D_PROTOCOL  *This,
  IN SHADER_TYPE       ShaderType,
  IN VOID             *Data,
  IN UINT32            Size
);

// Draw frame
typedef EFI_STATUS (EFIAPI *GOP_3D_DRAW_FRAME)(
  IN GOP_3D_PROTOCOL *This
);

// Present frame
typedef EFI_STATUS (EFIAPI *GOP_3D_PRESENT_FRAME)(
  IN GOP_3D_PROTOCOL *This
);

/* -------------------------- Protocol structure -------------------------- */

struct GOP_3D_PROTOCOL {
  // Command buffer and pipeline preperation functions
  GOP_3D_SET_CMD_BUFF           SetCmdBuffer;
  GOP_3D_SET_GPU_MODE           SetGpuMode;
  
  // Data transfer functions
  GOP_3D_TRANSFER_DATA_BUFFER   TransferDataBuffer;
  GOP_3D_TRANSFER_SHADER_BUFFER TransferShaderBuffer;

  // Draw calls 
  GOP_3D_DRAW_FRAME             DrawFrame;
  GOP_3D_PRESENT_FRAME          PresentFrame;
};

/* ------------------------------------------------------------------------ */

extern EFI_GUID gGop3dProtocolGuid;

#endif