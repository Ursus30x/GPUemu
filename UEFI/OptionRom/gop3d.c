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

    // Allocate VRAM
    VRAMADDR Addr = GpuAllocateMem(Size, Tag);
    if (Addr == 0) {
        return EFI_OUT_OF_RESOURCES;
    }

    // Transfer Data (CPU -> VRAM)
    EFI_STATUS Status = GpuVramWrite(Addr, HostData, Size);
    if (EFI_ERROR(Status)) {
        GpuFreeMem(Addr);
        return Status;
    }

    *GpuAddress = Addr;
    return EFI_SUCCESS;
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
    }

    Command cmd;
    cmd.opcode = CMD_DRAW_PRIMITIVE;
    cmd.payload.draw.type = primType;
    
    return GpuRingBufferAddCmd(&cmd, sizeof(Command));
}

EFI_STATUS EFIAPI GpuSubmitCmd(
  IN GOP_3D_PROTOCOL *This
  )
{
    DEBUG((DEBUG_INFO, "Mem Dump after command submition\n"));
    GpuDebugDumpMemoryMap();

    return GpuRingBufferFlush();
}

EFI_STATUS EFIAPI GpuPresent(
  IN GOP_3D_PROTOCOL *This
  )
{
    GpuSubmitCmd(This);
    GpuRingBufferWaitSpace(0); // Wait for idle (Sync)
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
  
  Private->Gop3dProtocol.GpuTransferBuffer = GpuTransferBuffer;
  Private->Gop3dProtocol.GpuClearFrame     = GpuClearFrame;
  Private->Gop3dProtocol.GpuDraw           = GpuDraw;
  
  Private->Gop3dProtocol.GpuSubmitCmd      = GpuSubmitCmd;
  Private->Gop3dProtocol.GpuPresent        = GpuPresent;

  return EFI_SUCCESS;
}