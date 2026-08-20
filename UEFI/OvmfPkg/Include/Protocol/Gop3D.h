#ifndef __GOP_3D_H__
#define __GOP_3D_H__

#define MAX_MIP_LEVELS 14

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
#define MAX_MIP_LEVELS 14
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
  Gop3dBufferTypeShaderCode,
  Gop3dBufferTypeTexture,
  Gop3dBufferTypeTextureDesc
} GOP_3D_BUFFER_TYPE;

typedef enum {
  Gop3dTexture2D   = 0,
  Gop3dTexture3D   = 1,
  Gop3dTextureCube = 2
} GOP_3D_TEXTURE_DEMENSION;

typedef enum {
  Gop3dFilterNearest               = 0,
  Gop3dFilterLinear                = 1,
  Gop3dFilterNearestMipmapNearest  = 2,
  Gop3dFilterLinearMipmapNearest   = 3,
  Gop3dFilterNearestMipmapLinear   = 4,
  Gop3dFilterLinearMipmapLinear    = 5  // Trilinear filtering
} GOP_3D_FILTER_MODE;

typedef enum {
  Gop3dWrapRepeat = 0,
  Gop3dWrapClamp  = 1,
  Gop3dWrapMirror = 2
} GOP_3D_WRAP_MODE;

typedef struct __attribute__((packed))  {
  VRAMADDR                  DataAddr;    // VRAM offset where pixel bytes start
  VRAMADDR                  MipData[MAX_MIP_LEVELS]; 
  UINT32                    Width;
  UINT32                    Height;
  UINT32                    Depth;
  UINT32                    Channels;    // 1, 2, 3, 4
  GOP_3D_TEXTURE_DEMENSION  Dimention;
  GOP_3D_FILTER_MODE        Filter;      
  GOP_3D_WRAP_MODE          Wrap;       
  GOP_3D_WRAP_MODE          WrapU;
  GOP_3D_WRAP_MODE          WrapV;
  GOP_3D_WRAP_MODE          WrapW;
  UINT32                    NumMipLevels;
  float                     MaxAnisotropy;
  float                     MinLod;
  float                     MaxLod;
  float                     LodBias;
} GOP_3D_TEXTURE_DESC;

// Needed for Draw command
typedef enum {
  Gop3dTopologyPoints,
  Gop3dTopologyLines,
  Gop3dTopologyLineStrip,
  Gop3dTopologyTriangles,
  Gop3dTopologyTriangleStrip,
  Gop3dTopologyTriangleFan,
  Gop3dTopologyQuads
} GOP_3D_TOPOLOGY;

typedef enum {
  Gop3dBlendFactorZero,
  Gop3dBlendFactorOne,
  Gop3dBlendFactorSrcAlpha,
  Gop3dBlendFactorOneMinusSrcAlpha,
  Gop3dBlendFactorDstAlpha,
  Gop3dBlendFactorOneMinusDstAlpha
} GOP_3D_BLEND_FACTOR;

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
 * Binds a texture descriptor in VRAM to a specific shader sampler binding slot.
 * @param BindingSlot   The shader binding slot (e.g. layout(binding = X)).
 * @param DescAddress   VRAM address of the texture descriptor.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_BIND_TEXTURE)(
  IN GOP_3D_PROTOCOL      *This,
  IN UINT32               BindingSlot,
  IN VRAMADDR             DescAddress
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


typedef
EFI_STATUS
(EFIAPI *GOP_3D_SET_BLEND_STATE)(
  IN GOP_3D_PROTOCOL      *This,
  IN BOOLEAN              EnableBlend,
  IN GOP_3D_BLEND_FACTOR  SrcFactor,
  IN GOP_3D_BLEND_FACTOR  DstFactor
);

typedef
EFI_STATUS
(EFIAPI *GOP_3D_SET_DEPTH_WRITE)(
  IN GOP_3D_PROTOCOL      *This,
  IN BOOLEAN              EnableDepthWrite
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
  
  GOP_3D_BIND_TEXTURE       GpuBindTexture;
  GOP_3D_SET_BLEND_STATE    GpuSetBlendState;
  GOP_3D_SET_DEPTH_WRITE    GpuSetDepthWrite;

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