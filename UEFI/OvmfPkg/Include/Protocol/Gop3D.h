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

#define GOP_3D_PROTOCOL_GUID { 0x12345678, 0x1234, 0x5678, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 } }

typedef struct GOP_3D_PROTOCOL GOP_3D_PROTOCOL;

/* ---------------------------- Data structures --------------------------- */

typedef UINT32 VRAMADDR;

// Global DMA fence
typedef struct{
  BOOLEAN               DmaBusy;
  BOOLEAN               CmdBusy;
  VOID                  *MapPtr;
  EFI_PHYSICAL_ADDRESS  DeviceAdress;
} GPU_DMA_FENCE;

// Needed to distinguish buffer types during Transfer
typedef enum {
  Gop3dBufferTypeVertex,
  Gop3dBufferTypeIndex,
  Gop3dBufferTypeUniform,
  Gop3dBufferTypeShaderCode
} GOP_3D_BUFFER_TYPE;

// Needed for Draw command
typedef enum {
  Gop3dTopologyPoints,
  Gop3dTopologyLines,
  Gop3dTopologyTriangles
} GOP_3D_TOPOLOGY;

/* ------------------------- Function declarations ------------------------ */

/**
 * Initializes the GPU driver state and PCI I/O.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_INIT)(
  IN GOP_3D_PROTOCOL      *This
  );

/**
 * Cleans up resources and shuts down the GPU driver.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_DESTROY)(
  IN GOP_3D_PROTOCOL      *This
  );

/**
 * Sets GPU mode (GOP/3D).
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_SET_MODE)(
  IN GOP_3D_PROTOCOL      *This,
  IN UINT32 Mode
  );

/**
 * Resets the command buffer head/cursor, preparing for new commands.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_CMD_BEGIN)(
  IN GOP_3D_PROTOCOL      *This
  );

/**
 * Finalizes the recording phase (adds debug markers/padding if needed).
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_CMD_END)(
  IN GOP_3D_PROTOCOL      *This
  );

/**
 * Generic Bind function signature used for VBO, IBO, UBO, and Shaders.
 * Binds a VRAM address to a specific pipeline slot.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_BIND_RESOURCE)(
  GOP_3D_PROTOCOL *This,
  VRAMADDR         Addr,
  UINT32           Size
  );

/**
 * Allocates VRAM and transfers data from Host to Device.
 * @param Type        The type of buffer (Vertex, Index, Uniform, Shader).
 * @param HostData    Pointer to the source data in System Memory.
 * @param Size        Size in bytes to allocate and copy.
 * @param GpuAddress  [OUT] The resulting VRAM address of the uploaded buffer.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_TRANSFER_BUFFER)(
  IN  GOP_3D_PROTOCOL     *This,
  IN  GOP_3D_BUFFER_TYPE  Type,
  IN  VOID                *HostData,
  IN  UINT32              Size,
  OUT VRAMADDR            *GpuAddress
  );

/**
 * Reallocates VRAM and transfers new data from Host to Device.
 * @param Type        The type of buffer (Vertex, Index, Uniform, Shader).
 * @param HostData    Pointer to the source data in System Memory.
 * @param Size        Size in bytes to allocate and copy.
 * @param GpuAddress  [OUT] The resulting VRAM address of the uploaded buffer.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_UPDATE_BUFFER)(
  IN  GOP_3D_PROTOCOL     *This,
  IN  GOP_3D_BUFFER_TYPE  Type,
  IN  VOID                *HostData,
  IN  UINT32              Size,
  OUT VRAMADDR            *GpuAddress
  );

/**
 * Frees VRAM allocated resource.
 * @param GpuAddress  Adress under which resource is located.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_FREE_BUFFER)(
  IN  GOP_3D_PROTOCOL     *This,
  IN VRAMADDR            *GpuAddress
  );

/**
 * Issues a draw call.
 * @param Topology    Primitive type (Points, Lines, Triangles).
 * @param VertexCount Number of vertices (or indices if IBO is bound) to draw.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_DRAW)(
  IN GOP_3D_PROTOCOL      *This,
  IN GOP_3D_TOPOLOGY      Topology,
  IN UINT32               VertexCount
  );

/**
 * Clears the framebuffer.
 * @param Color       32-bit color value (0xAARRGGBB).
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_CLEAR_FRAME)(
  IN GOP_3D_PROTOCOL      *This,
  IN UINT32               Color
  );

/**
 * Submits the recorded command buffer to the GPU (updates RING_HEAD).
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_SUBMIT_CMD)(
  IN GOP_3D_PROTOCOL      *This
  );

/**
 * Swaps buffers or flushes execution (Present).
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_PRESENT)(
  IN GOP_3D_PROTOCOL      *This
  );


/* -------------------------- Protocol structure -------------------------- */

struct GOP_3D_PROTOCOL {
  GOP_3D_INIT               GpuInit;
  GOP_3D_DESTROY            GpuDestroy;

  GOP_3D_SET_MODE           GpuSetMode;

  GOP_3D_CMD_BEGIN          GpuCmdBegin;
  GOP_3D_CMD_END            GpuCmdEnd;

  GOP_3D_BIND_RESOURCE      GpuBindUBO;
  GOP_3D_BIND_RESOURCE      GpuBindVBO;
  GOP_3D_BIND_RESOURCE      GpuBindIBO;
  GOP_3D_BIND_RESOURCE      GpuBindFragShader;
  GOP_3D_BIND_RESOURCE      GpuBindVertShader;

  GOP_3D_TRANSFER_BUFFER    GpuTransferBuffer;
  GOP_3D_UPDATE_BUFFER      GpuUpdateBuffer;
  GOP_3D_FREE_BUFFER        GpuFreeBuffer;

  GOP_3D_DRAW               GpuDraw;
  GOP_3D_CLEAR_FRAME        GpuClearFrame;

  GOP_3D_SUBMIT_CMD         GpuSubmitCmd;
  GOP_3D_PRESENT            GpuPresent;
};

/* ----------------------------------------------------------------------- */

extern EFI_GUID gGop3dProtocolGuid;

#endif