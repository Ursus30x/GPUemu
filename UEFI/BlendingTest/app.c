#include <Uefi.h>
#include <stddef.h>
#include <Protocol/PciIo.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/Gop3D.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h> // gBS
#include <Library/DevicePathLib.h>
#include <Library/TimerLib.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Pci.h>


#include "math3d.h"

#define WAIT_FOR_KEYPRESS() Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL); \
    if (!EFI_ERROR(Status)) { \
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); \
    }

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGraphicsOutput = NULL;
STATIC GOP_3D_PROTOCOL              *mGOP3D          = NULL;
// ============================================================================
// SIMT UBO Conversion Helper
// ============================================================================
// Converts a scalar Mat4 into SIMT-vectorized format (1024 bytes, 16 lanes)
typedef struct {
    // Each SimtFloat contains 16 lanes worth of data (16 floats = 64 bytes each)
    float data[16 * 16];  // 4 rows x 4 cols x 16 lanes = 256 floats = 1024 bytes
} SimtMat4UBO;

void Mat4_ToSimtUBO(Mat4 *scalar_mat4, SimtMat4UBO *simt_ubo) {
    // Layout: [Col 0: [Row 0 (16 lanes), Row 1 (16 lanes), ...], Col 1: [...], ...]
    int base_offset = 0;
    
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float value = scalar_mat4->m[row][col];
            
            // Write to all 16 lanes for this specific row in this specific column
            for (int lane = 0; lane < 16; lane++) {
                simt_ubo->data[base_offset + lane] = value;
            }
            // Advance by 16 floats (64 bytes) to the next row
            base_offset += 16;
        }
    }
}

VOID TestBlendingSimt(GOP_3D_BLEND_FACTOR srcFactor, GOP_3D_BLEND_FACTOR dstFactor, GOP_3D_BLEND_OP blendOp) {
    EFI_INPUT_KEY Key;

    // --- Data Definitions ---
    #include "mvp.h"

    #include "simple_fs.h"


    Vec3 vertices_opaque[] = {
        {-0.55, -0.85,  0.5, 0xFFFF0000},
        { 0.85, -0.85,  0.5, 0xFFFF0000},
        { 0.85,  0.55,  0.5, 0xFFFF0000},
        {-0.55,  0.55,  0.5, 0xFFFF0000},
    };

    Vec3 vertices_transparent[] = {
        {-0.85, -0.55,  0.4, 0x8000FF00},
        { 0.55, -0.55,  0.4, 0x8000FF00},
        { 0.55,  0.85,  0.4, 0x8000FF00},
        {-0.85,  0.85,  0.4, 0x8000FF00},
    };
    Triangle indices[] = {
        {0, 1, 2}, {3, 2, 0}
    };

    UINT32 IndexCount = (sizeof(indices) / sizeof(Triangle)) * 2;

    // --- Static Asset Transfer ---
    mGOP3D->GpuSetMode(mGOP3D, 1);

    VRAMADDR hVBO_Opaque = GPU_NULL_ADDR, hVBO_Transparent = GPU_NULL_ADDR, hIBO = GPU_NULL_ADDR, hVS = GPU_NULL_ADDR, hFS = GPU_NULL_ADDR;
    VRAMADDR hMVP1 = GPU_NULL_ADDR;
    VRAMADDR hMVP2 = GPU_NULL_ADDR;

    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices_opaque,      sizeof(vertices_opaque),      &hVBO_Opaque);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices_transparent, sizeof(vertices_transparent), &hVBO_Transparent);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  indices,             sizeof(indices),             &hIBO);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader,   sizeof(bin_vertex_shader),   &hVS);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);
    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuSubmitCmd(mGOP3D);

    Print(L"Testing Alpha Blending & Depth Masking... Press Key to Exit.\n");


    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
        Mat4 mvp1, mvp2;

        Mat4_Identity(&mvp1);
        Mat4_Identity(&mvp2);

        

        SimtMat4UBO simt_mvp1, simt_mvp2;
        Mat4_ToSimtUBO(&mvp1, &simt_mvp1);
        Mat4_ToSimtUBO(&mvp2, &simt_mvp2);

        // --- RENDER PASS ---
        mGOP3D->GpuCmdBegin(mGOP3D);

        if (hMVP1 == GPU_NULL_ADDR) {
            mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        } else {
            mGOP3D->GpuCmdUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        }

        if (hMVP2 == GPU_NULL_ADDR) {
            mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp2, sizeof(SimtMat4UBO), &hMVP2);
        } else {
            mGOP3D->GpuCmdUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp2, sizeof(SimtMat4UBO), &hMVP2);
        }

        mGOP3D->GpuCmdClearFrame(mGOP3D, 0xFF101010); // Clear to dark gray
        
        mGOP3D->GpuCmdBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
        mGOP3D->GpuCmdBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
        mGOP3D->GpuCmdBindIBO(mGOP3D, hIBO, 2);

        // Configure vertex attributes: Location 0 (Position), Location 2 (Color)
        mGOP3D->GpuCmdVertexAttribPointer(mGOP3D, 0, 3, Gop3dAttribFloat, FALSE, sizeof(Vec3), (UINT32)OFFSET_OF(Vec3, x));
        mGOP3D->GpuCmdEnableVertexAttribArray(mGOP3D, 0);
        mGOP3D->GpuCmdVertexAttribPointer(mGOP3D, 2, 4, Gop3dAttribUbyteNorm, TRUE, sizeof(Vec3), (UINT32)OFFSET_OF(Vec3, color));
        mGOP3D->GpuCmdEnableVertexAttribArray(mGOP3D, 2);
        mGOP3D->GpuCmdDisableVertexAttribArray(mGOP3D, 1);
        mGOP3D->GpuCmdDisableVertexAttribArray(mGOP3D, 3);


        mGOP3D->GpuCmdSetBlendState(mGOP3D, FALSE, Gop3dBlendFactorOne, Gop3dBlendFactorZero);
        mGOP3D->GpuCmdSetBlendOp(mGOP3D, Gop3dBlendOpAdd);
        mGOP3D->GpuCmdSetDepthWrite(mGOP3D, TRUE);

        mGOP3D->GpuCmdBindVBO(mGOP3D, hVBO_Opaque, 4);
        mGOP3D->GpuCmdBindUBO(mGOP3D, hMVP1, sizeof(SimtMat4UBO));
        mGOP3D->GpuCmdDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);

        mGOP3D->GpuCmdSetBlendState(mGOP3D, TRUE, srcFactor, dstFactor);
        mGOP3D->GpuCmdSetBlendOp(mGOP3D, blendOp);
        mGOP3D->GpuCmdSetDepthWrite(mGOP3D, FALSE); 

        mGOP3D->GpuCmdBindVBO(mGOP3D, hVBO_Transparent, 4);
        mGOP3D->GpuCmdBindUBO(mGOP3D, hMVP2, sizeof(SimtMat4UBO));
        mGOP3D->GpuCmdDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);

        mGOP3D->GpuCmdEnd(mGOP3D);
        mGOP3D->GpuPresent(mGOP3D);

    }

    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_Opaque);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_Transparent);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hIBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP1);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP2);

    mGOP3D->GpuSetMode(mGOP3D, 0);


}




EFI_STATUS EFIAPI Test() {
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    DEBUG((EFI_D_INFO, "TestGop start\n"));

    Status = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID **)&mGraphicsOutput
    );

    Status = gBS->LocateProtocol(
        &gGop3dProtocolGuid,
        NULL,
        (VOID **)&mGOP3D
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate GOP: %r\n", Status);
        return Status;
    }
    Print(L"====================================================\n");
    Print(L"   GPUemu UEFI BLENDING TEST Application   \n");
    Print(L"====================================================\n");
    // Print GOP information
    Print(L"GOP Located Successfully!\n");
    Print(L"Current Mode: %d\n", mGraphicsOutput->Mode->Mode);
    Print(L"Resolution: %dx%d\n",
          mGraphicsOutput->Mode->Info->HorizontalResolution,
          mGraphicsOutput->Mode->Info->VerticalResolution);
    Print(L"Pixel Format: %d\n", mGraphicsOutput->Mode->Info->PixelFormat);
    
    Print(L"1. Replace / None Test: One, Zero (Add)\n");
    WAIT_FOR_KEYPRESS()
    TestBlendingSimt(Gop3dBlendFactorOne, Gop3dBlendFactorZero, Gop3dBlendOpAdd);

    Print(L"2. Standard Alpha Blending Test: SrcAlpha, OneMinusSrcAlpha (Add)\n");
    WAIT_FOR_KEYPRESS()
    TestBlendingSimt(Gop3dBlendFactorSrcAlpha, Gop3dBlendFactorOneMinusSrcAlpha, Gop3dBlendOpAdd);

    Print(L"3. Additive Blending Test: One, One (Add)\n");
    WAIT_FOR_KEYPRESS()
    TestBlendingSimt(Gop3dBlendFactorOne, Gop3dBlendFactorOne, Gop3dBlendOpAdd);

    Print(L"4. Premultiplied Blending Test: One, OneMinusSrcAlpha (Add)\n");
    WAIT_FOR_KEYPRESS()
    TestBlendingSimt(Gop3dBlendFactorOne, Gop3dBlendFactorOneMinusSrcAlpha, Gop3dBlendOpAdd);

    Print(L"5. Multiplicative Blending Test: Zero, SrcColor (Add)\n");
    WAIT_FOR_KEYPRESS()
    TestBlendingSimt(Gop3dBlendFactorZero, Gop3dBlendFactorSrcColor, Gop3dBlendOpAdd);

    Print(L"6. Reverse Subtract Blending Test: One, One (ReverseSubtract)\n");
    WAIT_FOR_KEYPRESS()
    TestBlendingSimt(Gop3dBlendFactorOne, Gop3dBlendFactorOne, Gop3dBlendOpReverseSubtract);

    DEBUG((EFI_D_INFO, "Test end\n"));
    return EFI_SUCCESS;
}


/*==========================================================================*/
/*                               MAIN APP ENTRY                             */
/*==========================================================================*/


EFI_STATUS EFIAPI SpirvAppEntry(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable) {

    EFI_STATUS Status;

    Status = Test();

    ASSERT_EFI_ERROR(Status);

    return Status;
}

EFI_STATUS EFIAPI SpirvAppEntryUnload (IN  EFI_HANDLE  ImageHandle) {
    return EFI_SUCCESS;
}