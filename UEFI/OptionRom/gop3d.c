#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h> // gBS

#include <Protocol/Gop3D.h>

#include "ringbuffer.h"
#include "gpu_memory.h"
#include "vram.h"
#include "sync.h"


#define RING_BUFFER_SIZE (1 << 16) // 64KB

/* -------------------------------------------------------------------------
 * Initialization & Teardown
 * ------------------------------------------------------------------------- */

EFI_STATUS EFIAPI GpuInit(
  IN GOP_3D_PROTOCOL *This
  )
{
    // Reset Ring Buffer to clean state
    GpuRingBufferClearCmdBuffer();

    // Enable GPU interrupts
    GpuRingBufferEnableInterrupts(TRUE);

    DEBUG((EFI_D_INFO, "GOP3D: Initialized.\n"));
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuDestroy(
  IN GOP_3D_PROTOCOL *This
  )
{
    // Wait for idle before destroying resources
    while (!GpuRingBufferIsIdle()) {
        gBS->Stall(100);
    }

    GpuRingBufferClearCmdBuffer();

    DEBUG((EFI_D_INFO, "GOP3D: Destroyed.\n"));
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuSetMode(
  IN GOP_3D_PROTOCOL *This,
  IN UINT32 Mode
  )
{
    GpuMmioWrite32(REG_GPU_MODE_ADDR, Mode);
    DEBUG((EFI_D_INFO, "GOP3D: SetMode(%d) (Stub)\n", Mode));
    return EFI_SUCCESS;
}

/* -------------------------------------------------------------------------
 * Command Recording (Batch Building)
 * ------------------------------------------------------------------------- */

EFI_STATUS EFIAPI GpuCmdBegin(
  IN GOP_3D_PROTOCOL *This
  )
{
    return GpuRingBufferClearCmdBuffer();
}

EFI_STATUS EFIAPI GpuCmdEnd(
  IN GOP_3D_PROTOCOL *This
  )
{
    return EFI_SUCCESS;
}

/* -------------------------------------------------------------------------
 * Resource Binding
 * ------------------------------------------------------------------------- */

// Internal helper function
EFI_STATUS EFIAPI GpuBindResourceGeneric(
  IN StateID StateId,
  IN VRAMADDR Address,
  IN UINT32 Size,
  IN DataType ElementType
  )
{
    Command cmd;
    cmd.opcode = CMD_SET_STATE;
    cmd.payload.state.state_id = StateId;

    GenericBufferConfig conf;
    conf.addr = Address;
    conf.size = Size;
    conf.element_type = ElementType;

    cmd.payload.state.value.buffer_config = conf;

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuBindVBO(
  IN GOP_3D_PROTOCOL *This,
  IN VRAMADDR GpuAddress,
  IN UINT32 Size
  )
{
    return GpuBindResourceGeneric(STATE_ID_VBO_CONFIG, GpuAddress, Size,D_TYPE_VEC3);
}

EFI_STATUS EFIAPI GpuBindIBO(
  IN GOP_3D_PROTOCOL *This,
  IN VRAMADDR GpuAddress,
  IN UINT32 Size
  )
{
    return GpuBindResourceGeneric(STATE_ID_EDGE_CONFIG, GpuAddress, Size, D_TYPE_VEC2);
}

EFI_STATUS EFIAPI GpuBindUBO(
  IN GOP_3D_PROTOCOL *This,
  IN VRAMADDR GpuAddress,
  IN UINT32 Size
  )
{
    return GpuBindResourceGeneric(STATE_ID_UNIFORM_CONFIG, GpuAddress, Size, D_TYPE_MAT4);
}

EFI_STATUS EFIAPI GpuBindVertShader(
  IN GOP_3D_PROTOCOL *This,
  IN VRAMADDR GpuAddress,
  IN UINT32 Size
  )
{
    Command cmd;
    cmd.opcode = CMD_SET_STATE;
    cmd.payload.state.state_id = STATE_ID_VERTEX_SHADER_PTR;
    cmd.payload.state.value.shader_ptrs.vs_addr = GpuAddress;

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuBindFragShader(
  IN GOP_3D_PROTOCOL *This,
  IN VRAMADDR GpuAddress,
  IN UINT32 Size
  )
{
    Command cmd;
    cmd.opcode = CMD_SET_STATE;
    cmd.payload.state.state_id = STATE_ID_FRAGMENT_SHADER_PTR;
    cmd.payload.state.value.shader_ptrs.fs_addr = GpuAddress;

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuBindCompShader(
  IN GOP_3D_PROTOCOL *This,
  IN VRAMADDR GpuAddress,
  IN UINT32 Size
  )
{
    Command cmd;
    cmd.opcode = CMD_SET_STATE;
    cmd.payload.state.state_id = STATE_ID_COMPUTE_SHADER_PTR;
    cmd.payload.state.value.shader_ptrs.cs_addr = GpuAddress;

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuBindSSBO(
  IN GOP_3D_PROTOCOL *This,
  IN UINT32 BindingSlot,
  IN VRAMADDR GpuAddress,
  IN UINT32 Size
  )
{
    Command cmd;
    cmd.opcode = CMD_SET_STATE;
    cmd.payload.state.state_id = STATE_ID_SSBO_CONFIG;
    cmd.payload.state.value.ssbo_config.binding = BindingSlot;
    cmd.payload.state.value.ssbo_config.addr = GpuAddress;
    cmd.payload.state.value.ssbo_config.size = Size;

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuBindTexture(
  IN GOP_3D_PROTOCOL *This,
  IN UINT32 BindingSlot,
  IN VRAMADDR DescAddress
  )
{
    Command cmd;
    cmd.opcode = CMD_SET_STATE;
    cmd.payload.state.state_id = STATE_ID_TEXTURE_CONFIG;
    cmd.payload.state.value.texture_config.binding_slot = BindingSlot;
    cmd.payload.state.value.texture_config.desc_vram_addr = DescAddress;

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}


EFI_STATUS EFIAPI GpuSetBlendState(
  IN GOP_3D_PROTOCOL      *This,
  IN BOOLEAN              EnableBlend,
  IN GOP_3D_BLEND_FACTOR  SrcFactor,
  IN GOP_3D_BLEND_FACTOR  DstFactor
)
{
  Command cmd;
  cmd.opcode = CMD_SET_STATE;
  cmd.payload.state.state_id = STATE_ID_BLEND_CONFIG;
  cmd.payload.state.value.blend_config.enable = EnableBlend;
  cmd.payload.state.value.blend_config.dst_factor = DstFactor;
  cmd.payload.state.value.blend_config.src_factor = SrcFactor;

  return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}
EFI_STATUS EFIAPI GpuSetDepthWrite(
  IN GOP_3D_PROTOCOL      *This,
  IN BOOLEAN              EnableDepthWrite
)
{
  Command cmd;
  cmd.opcode = CMD_SET_STATE;
  cmd.payload.state.state_id = STATE_ID_DEPTH_CONFIG;
  cmd.payload.state.value.depth_config.depth_write_enable = EnableDepthWrite;

  return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}


/* -------------------------------------------------------------------------
 * Data Transfer
 * ------------------------------------------------------------------------- */

EFI_STATUS EFIAPI GpuTransferBuffer(
  IN  GOP_3D_PROTOCOL     *This,
  IN  GOP_3D_BUFFER_TYPE  Type,
  IN  VOID                *HostData,
  IN  UINT32              Size,
  OUT VRAMADDR            *GpuAddress
  )
{
    if (HostData == NULL || Size == 0 || GpuAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    CHAR8 *Tag = "GENERIC";
    if (Type == Gop3dBufferTypeVertex) Tag = "VBO";
    else if (Type == Gop3dBufferTypeIndex) Tag = "IBO";
    else if (Type == Gop3dBufferTypeUniform) Tag = "UBO";
    else if (Type == Gop3dBufferTypeShaderCode) Tag = "SHADER";
    else if (Type == Gop3dBufferTypeTexture) Tag = "TEX";
    else if (Type == Gop3dBufferTypeTextureDesc) Tag = "TEXDESC";

    (VOID)Tag; // Suppress "unused variable" error if debug is disabled

    // Allocate VRAM
    VRAMADDR Addr = GpuAllocateMem(Size, Tag);
    if (Addr == 0) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Transfer Data (CPU -> VRAM)
    EFI_STATUS Status = GpuDmaWrite(Addr, HostData, Size);
    if (EFI_ERROR(Status)) {
        GpuFreeMem(Addr);
        return Status;
    }

    *GpuAddress = Addr;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuUpdateBuffer(
  IN     GOP_3D_PROTOCOL     *This,
  IN     GOP_3D_BUFFER_TYPE  Type,
  IN     VOID                *HostData,
  IN     UINT32              Size,
  IN OUT VRAMADDR            *GpuAddress
)
{
    if (HostData == NULL || Size == 0 || GpuAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    VRAMADDR OldAddr = *GpuAddress;
    UINT32 OldSize = GpuGetAllocatedSize(OldAddr);

    // If existing buffer is large enough, just overwrite it
    if (OldSize >= Size) {
        return GpuDmaWrite(OldAddr, HostData, Size);
    }

    // If updated size is bigger, try to allocate new buffer first
    VRAMADDR NewAddr = 0;
    EFI_STATUS Status = GpuTransferBuffer(This, Type, HostData, Size, &NewAddr);

    if (EFI_ERROR(Status) || NewAddr == 0) {
        return EFI_OUT_OF_RESOURCES;
    }

    // TODO: Implement proper realloc mechanizm in memory allocator
    if (OldAddr != 0) {
        GpuFreeMem(OldAddr);
    }

    *GpuAddress = NewAddr;

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuFreeBuffer(
  IN  GOP_3D_PROTOCOL     *This,
  IN  VRAMADDR            *GpuAddress
)
{
    if (GpuAddress == NULL) {
      return EFI_INVALID_PARAMETER;
    }

    if (*GpuAddress != 0) {
        GpuFreeMem(*GpuAddress);
        *GpuAddress = 0;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuReadBuffer(
  IN  GOP_3D_PROTOCOL     *This,
  IN  VRAMADDR            GpuAddress,
  OUT VOID                *HostData,
  IN  UINT32              Size
)
{
    if (HostData == NULL || Size == 0 || GpuAddress == 0) {
      return EFI_INVALID_PARAMETER;
    }

    GpuCmdSync();

    return GpuVramRead(HostData, GpuAddress, Size);
}

/* -------------------------------------------------------------------------
 * Drawing & Execution
 * ------------------------------------------------------------------------- */

EFI_STATUS EFIAPI GpuClearFrame(
  IN GOP_3D_PROTOCOL *This,
  IN UINT32 Color
  )
{
    Command cmd;
    cmd.opcode = CMD_CLEAR_FRAMEBUFFER;
    cmd.payload.clear.options = 0b11; // Clear Color + Depth

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuDraw(
  IN GOP_3D_PROTOCOL      *This,
  IN GOP_3D_TOPOLOGY      Topology,
  IN UINT32               VertexCount
  )
{
    PrimitiveType primType = PRIMITIVE_TYPE_TRIANGLES;

    if (Topology == Gop3dTopologyLines) {
        primType = PRIMITIVE_TYPE_LINES;
    } else if (Topology == Gop3dTopologyPoints) {
        primType = PRIMITIVE_TYPE_POINTS;
    } else if (Topology == Gop3dTopologyLineStrip) {
        primType = PRIMITIVE_TYPE_LINE_STRIP;
    } else if (Topology == Gop3dTopologyTriangleStrip) {
        primType = PRIMITIVE_TYPE_TRIANGLE_STRIP;
    } else if (Topology == Gop3dTopologyTriangleFan) {
        primType = PRIMITIVE_TYPE_TRIANGLE_FAN;
    } else if (Topology == Gop3dTopologyQuads) {
        primType = PRIMITIVE_TYPE_QUADS;
    }
    Command cmd;
    cmd.opcode = CMD_DRAW_PRIMITIVE;
    cmd.payload.draw.type = primType;

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuDispatchCompute(
  IN GOP_3D_PROTOCOL      *This,
  IN UINT32               GroupCountX,
  IN UINT32               GroupCountY,
  IN UINT32               GroupCountZ
  )
{
    Command cmd;
    cmd.opcode = CMD_DISPATCH;
    cmd.payload.dispatch.group_count_x = GroupCountX;
    cmd.payload.dispatch.group_count_y = GroupCountY;
    cmd.payload.dispatch.group_count_z = GroupCountZ;

    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuSubmitCmd(
  IN GOP_3D_PROTOCOL *This
  )
{
    // Ensure previous batch is done before kicking new one
    GpuCmdSync();

    EFI_STATUS Status = GpuRingBufferFlush();
    if (!EFI_ERROR(Status)) {
        GPU_CONTEXT *Private = MY_GPU_PRIVATE_DATA_FROM_GOP3D(This);
        Private->DmaFence.CmdBusy = TRUE;
    }

#ifdef MEM_DEBUG
    DEBUG((DEBUG_INFO, "Mem Dump after command submition\n"));
    GpuDebugDumpMemoryMap();
    GpuDebugDumpMmio();
#endif


    return Status;
}

EFI_STATUS EFIAPI GpuPresent(
  IN GOP_3D_PROTOCOL *This
  )
{
    // If there's recorded commands, submit them
    GpuSubmitCmd(This);

    // Later there will be handling of multi-buffering, right now its a frontend for command submition

    return EFI_SUCCESS;
}

/* -------------------------------------------------------------------------
 * Protocol Setup
 * ------------------------------------------------------------------------- */

EFI_STATUS EFIAPI Gop3DSetup(IN OUT GPU_CONTEXT *Private)
{
  DEBUG((DEBUG_INFO, "GOP3D: Setting up GOP3D protocol\n"));

  GpuRingBufferInit(RING_BUFFER_SIZE);

  // Link Implementation to Protocol Pointers
  Private->Gop3dProtocol.GpuInit           = GpuInit;
  Private->Gop3dProtocol.GpuDestroy        = GpuDestroy;
  Private->Gop3dProtocol.GpuSetMode        = GpuSetMode;

  Private->Gop3dProtocol.GpuCmdBegin       = GpuCmdBegin;
  Private->Gop3dProtocol.GpuCmdEnd         = GpuCmdEnd;

  Private->Gop3dProtocol.GpuBindVBO        = GpuBindVBO;
  Private->Gop3dProtocol.GpuBindIBO        = GpuBindIBO;
  Private->Gop3dProtocol.GpuBindUBO        = GpuBindUBO;
  Private->Gop3dProtocol.GpuBindVertShader = GpuBindVertShader;
  Private->Gop3dProtocol.GpuBindFragShader = GpuBindFragShader;
  Private->Gop3dProtocol.GpuBindCompShader = GpuBindCompShader;
  Private->Gop3dProtocol.GpuBindSSBO       = GpuBindSSBO;
  Private->Gop3dProtocol.GpuBindTexture    = GpuBindTexture;
  Private->Gop3dProtocol.GpuSetBlendState  = GpuSetBlendState;
  Private->Gop3dProtocol.GpuSetDepthWrite  = GpuSetDepthWrite;


  Private->Gop3dProtocol.GpuTransferBuffer = GpuTransferBuffer;
  Private->Gop3dProtocol.GpuUpdateBuffer   = GpuUpdateBuffer;
  Private->Gop3dProtocol.GpuFreeBuffer     = GpuFreeBuffer;
  Private->Gop3dProtocol.GpuReadBuffer     = GpuReadBuffer;

  Private->Gop3dProtocol.GpuClearFrame     = GpuClearFrame;
  Private->Gop3dProtocol.GpuDraw           = GpuDraw;
  Private->Gop3dProtocol.GpuDispatchCompute = GpuDispatchCompute;

  Private->Gop3dProtocol.GpuSubmitCmd      = GpuSubmitCmd;
  Private->Gop3dProtocol.GpuPresent        = GpuPresent;

  return EFI_SUCCESS;
}