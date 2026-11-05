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

STATIC
VOID
ResetGop3dDisplayState(
    VOID
    )
{
    if (mGOP3D != NULL) {
    mGOP3D->GpuSetMode(mGOP3D, 1);
        mGOP3D->GpuCmdBegin(mGOP3D);
        mGOP3D->GpuCmdClearFrame(mGOP3D, 0xFF000000);
        mGOP3D->GpuCmdEnd(mGOP3D);
        mGOP3D->GpuPresent(mGOP3D);
        mGOP3D->GpuSetMode(mGOP3D, 0);
    }

    if (gST != NULL && gST->ConOut != NULL) {
        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
        gST->ConOut->ClearScreen(gST->ConOut);
    }
}
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

VOID TestSampling(GOP_3D_FILTER_MODE filter, GOP_3D_WRAP_MODE wrap) {
    EFI_INPUT_KEY Key;

    // --- Data Definitions ---

    #include "sampler_vs.h"

    #include "sampler_fs.h"


    Vec3 vertices[] = {
        {-0.5,  -0.5,  0.5, 0xFFFF0000, -1.0f, -1.0f},
        { 0.5,  -0.5,  0.5, 0xFFFF0000,  3.0f, -1.0f},
        { 0.5,  0.5,  0.5, 0xFFFF0000,  3.0f,  3.0f},
        {-0.5,  0.5,  0.5, 0xFFFF0000, -1.0f,  3.0f},
    };

    Triangle indices[] = {
        {0, 1, 2}, {3, 2, 0}
    };
    #define IMG_WIDTH  50
    #define IMG_HEIGHT 50
    UINT8 image[IMG_WIDTH * IMG_HEIGHT * 3];
    for (UINTN y = 0; y < IMG_HEIGHT; y++) {
        for (UINTN x = 0; x < IMG_WIDTH; x++) {
            UINTN idx = (y * IMG_WIDTH + x) * 3;
            UINT8 red = 0;
            UINT8 green = 0;
            UINT8 blue = 0;

            // Strong stripe pattern: easy to distinguish Repeat / Clamp / Mirror
            if ((x / 5) % 2 == 0) {
                red = 255;
                green = 0;
                blue = 0;
            } else {
                red = 0;
                green = 255;
                blue = 0;
            }

            if ((y / 5) % 2 == 0) {
                blue = 255;
            }

            // Add alternating white/black vertical stripes to make UV wrapping obvious
            if ((x % 10) < 5) {
                red = 255;
                green = 255;
                blue = 255;
            }

            image[idx + 0] = red;
            image[idx + 1] = green;
            image[idx + 2] = blue;
        }
    }
    GOP_3D_TEXTURE_DESC tex_desc;
    SetMem(&tex_desc, sizeof(tex_desc), 0);
    tex_desc.DataAddr = GPU_NULL_ADDR;
    tex_desc.Width = 50;
    tex_desc.Height = 50;
    tex_desc.Depth = 1;
    tex_desc.Channels = 3;
    tex_desc.Dimention = Gop3dTexture2D;
    tex_desc.Filter = filter;
    tex_desc.Wrap = wrap;
    tex_desc.WrapU = wrap;
    tex_desc.WrapV = wrap;
    tex_desc.WrapW = wrap;

    UINT32 IndexCount = (sizeof(indices) / sizeof(Triangle)) * 2;

    // --- Static Asset Transfer ---
    mGOP3D->GpuSetMode(mGOP3D, 1);

    VRAMADDR hVBO = GPU_NULL_ADDR, hIBO = GPU_NULL_ADDR, hVS = GPU_NULL_ADDR, hFS = GPU_NULL_ADDR;
    VRAMADDR hMVP1 = GPU_NULL_ADDR;
    VRAMADDR hTexData = GPU_NULL_ADDR, hTexDesc = GPU_NULL_ADDR;

    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices,      sizeof(vertices),      &hVBO);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  indices,             sizeof(indices),             &hIBO);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader,   sizeof(bin_vertex_shader),   &hVS);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);

    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeTexture, image, sizeof(image), &hTexData);
    tex_desc.DataAddr = hTexData;
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeTextureDesc, &tex_desc, sizeof(tex_desc), &hTexDesc);
    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuSubmitCmd(mGOP3D);

    Print(L"Testing Alpha Blending & Depth Masking... Press Key to Exit.\n");


    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
        Mat4 mvp1;

        Mat4_Identity(&mvp1);

        

        SimtMat4UBO simt_mvp1;
        Mat4_ToSimtUBO(&mvp1, &simt_mvp1);

        // --- RENDER PASS ---
        mGOP3D->GpuCmdBegin(mGOP3D);

        if (hMVP1 == GPU_NULL_ADDR) {
            mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        } else {
            mGOP3D->GpuCmdUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        }


        mGOP3D->GpuCmdClearFrame(mGOP3D, 0xFF101010); // Clear to dark gray
        
        mGOP3D->GpuCmdBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
        mGOP3D->GpuCmdBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
        mGOP3D->GpuCmdBindIBO(mGOP3D, hIBO, 2);

        mGOP3D->GpuCmdBindTexture(mGOP3D, 1, hTexDesc);

        // Configure vertex attributes: Location 0 (Position), Location 1 (TexCoord)
        mGOP3D->GpuCmdVertexAttribPointer(mGOP3D, 0, 3, Gop3dAttribFloat, FALSE, sizeof(Vec3), (UINT32)OFFSET_OF(Vec3, x));
        mGOP3D->GpuCmdEnableVertexAttribArray(mGOP3D, 0);
        mGOP3D->GpuCmdVertexAttribPointer(mGOP3D, 1, 2, Gop3dAttribFloat, FALSE, sizeof(Vec3), (UINT32)OFFSET_OF(Vec3, u));
        mGOP3D->GpuCmdEnableVertexAttribArray(mGOP3D, 1);
        mGOP3D->GpuCmdDisableVertexAttribArray(mGOP3D, 2);
        mGOP3D->GpuCmdDisableVertexAttribArray(mGOP3D, 3);
       

        mGOP3D->GpuCmdBindVBO(mGOP3D, hVBO, 4);
        mGOP3D->GpuCmdBindUBO(mGOP3D, hMVP1, sizeof(SimtMat4UBO));
        mGOP3D->GpuCmdDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);


        mGOP3D->GpuCmdEnd(mGOP3D);
        mGOP3D->GpuPresent(mGOP3D);

    }

    mGOP3D->GpuSetMode(mGOP3D, 0);

    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hIBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP1);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hTexData);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hTexDesc);


}

VOID TestVolume3D(GOP_3D_FILTER_MODE filter, GOP_3D_WRAP_MODE wrap) {
    EFI_INPUT_KEY Key;

    #include "volume3d_vs.h"
    #include "volume3d_fs.h"

    typedef struct {
        float x, y, z;
        float u, v, w;
    } VOLUME_VERTEX;

    #define VOLUME_SIZE 16
    UINT8 volume[VOLUME_SIZE * VOLUME_SIZE * VOLUME_SIZE * 4];
    for (UINT32 z = 0; z < VOLUME_SIZE; z++) {
        for (UINT32 y = 0; y < VOLUME_SIZE; y++) {
            for (UINT32 x = 0; x < VOLUME_SIZE; x++) {
                UINT32 index = ((z * VOLUME_SIZE + y) * VOLUME_SIZE + x) * 4;
                volume[index + 0] = (UINT8)(x * 255 / (VOLUME_SIZE - 1));
                volume[index + 1] = (UINT8)(y * 255 / (VOLUME_SIZE - 1));
                volume[index + 2] = (UINT8)(z * 255 / (VOLUME_SIZE - 1));
                volume[index + 3] = 255;
            }
        }
    }

    static CONST INT8 corner[8][3] = {
        {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
    };
    VOLUME_VERTEX vertices[8];
    for (UINT32 i = 0; i < 8; i++) {
        float x = (float)corner[i][0] * 0.58f;
        float y = (float)corner[i][1] * 0.58f;
        float z = (float)corner[i][2] * 0.58f;
        float rotatedX = (x + z) * 0.70710678f;
        float rotatedZ = (z - x) * 0.70710678f;
        float rotatedY = y * 0.81915204f - rotatedZ * 0.57357644f;
        float depth = y * 0.57357644f + rotatedZ * 0.81915204f;

        vertices[i].x = rotatedX * 0.68f;
        vertices[i].y = rotatedY * 0.68f;
        vertices[i].z = 0.5f + depth * 0.25f;
        vertices[i].u = (float)(corner[i][0] + 1) * 2.0f - 1.0f;
        vertices[i].v = (float)(corner[i][1] + 1) * 2.0f - 1.0f;
        vertices[i].w = (float)(corner[i][2] + 1) * 2.0f - 1.0f;
    }

    Triangle indices[] = {
        {4, 5, 6}, {4, 6, 7},
        {0, 2, 1}, {0, 3, 2},
        {0, 4, 7}, {0, 7, 3},
        {1, 2, 6}, {1, 6, 5},
        {3, 7, 6}, {3, 6, 2},
        {0, 1, 5}, {0, 5, 4}
    };
    UINT32 triangleCount = sizeof(indices) / sizeof(indices[0]);

    GOP_3D_TEXTURE_DESC textureDesc;
    SetMem(&textureDesc, sizeof(textureDesc), 0);
    textureDesc.DataAddr = GPU_NULL_ADDR;
    textureDesc.Width = VOLUME_SIZE;
    textureDesc.Height = VOLUME_SIZE;
    textureDesc.Depth = VOLUME_SIZE;
    textureDesc.Channels = 4;
    textureDesc.Dimention = Gop3dTexture3D;
    textureDesc.Filter = filter;
    textureDesc.Wrap = wrap;
    textureDesc.WrapU = wrap;
    textureDesc.WrapV = wrap;
    textureDesc.WrapW = wrap;
    textureDesc.NumMipLevels = 1;
    textureDesc.MaxAnisotropy = 1.0f;
    Gop3dCalculateMipMapOffsets(&textureDesc);

    Mat4 mvp;
    SimtMat4UBO simtMvp;
    Mat4_Identity(&mvp);
    Mat4_ToSimtUBO(&mvp, &simtMvp);

    VRAMADDR hVBO = GPU_NULL_ADDR;
    VRAMADDR hIBO = GPU_NULL_ADDR;
    VRAMADDR hVS = GPU_NULL_ADDR;
    VRAMADDR hFS = GPU_NULL_ADDR;
    VRAMADDR hMVP = GPU_NULL_ADDR;
    VRAMADDR hTextureData = GPU_NULL_ADDR;
    VRAMADDR hTextureDesc = GPU_NULL_ADDR;

    mGOP3D->GpuSetMode(mGOP3D, 1);
    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices, sizeof(vertices), &hVBO);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeIndex, indices, sizeof(indices), &hIBO);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader, sizeof(bin_vertex_shader), &hVS);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simtMvp, sizeof(simtMvp), &hMVP);
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeTexture, volume, sizeof(volume), &hTextureData);
    textureDesc.DataAddr = hTextureData;
    mGOP3D->GpuCmdTransferBuffer(mGOP3D, Gop3dBufferTypeTextureDesc, &textureDesc, sizeof(textureDesc), &hTextureDesc);
    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuSubmitCmd(mGOP3D);

    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuCmdClearFrame(mGOP3D, 0xFF101820);
    mGOP3D->GpuCmdBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
    mGOP3D->GpuCmdBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
    mGOP3D->GpuCmdBindVBO(mGOP3D, hVBO, 8);
    mGOP3D->GpuCmdBindIBO(mGOP3D, hIBO, triangleCount);
    mGOP3D->GpuCmdBindTexture(mGOP3D, 1, hTextureDesc);
    mGOP3D->GpuCmdVertexAttribPointer(mGOP3D, 0, 3, Gop3dAttribFloat, FALSE, sizeof(VOLUME_VERTEX), OFFSET_OF(VOLUME_VERTEX, x));
    mGOP3D->GpuCmdEnableVertexAttribArray(mGOP3D, 0);
    mGOP3D->GpuCmdVertexAttribPointer(mGOP3D, 1, 3, Gop3dAttribFloat, FALSE, sizeof(VOLUME_VERTEX), OFFSET_OF(VOLUME_VERTEX, u));
    mGOP3D->GpuCmdEnableVertexAttribArray(mGOP3D, 1);
    mGOP3D->GpuCmdBindUBO(mGOP3D, hMVP, sizeof(simtMvp));
    mGOP3D->GpuCmdDraw(mGOP3D, Gop3dTopologyTriangles, triangleCount);
    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuPresent(mGOP3D);

        CONST CHAR16 *filterName = filter == Gop3dFilterNearest ? L"Nearest" : L"Linear";
        CONST CHAR16 *wrapName = wrap == Gop3dWrapRepeat ? L"Repeat" :
                         wrap == Gop3dWrapClamp ? L"Clamp" : L"Mirror";
        Print(L"3D volume sampler: %s / %s. RGB shows U, V and W. Press a key to continue.\n",
            filterName, wrapName);
    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
        gBS->Stall(10000);
    }

    mGOP3D->GpuSetMode(mGOP3D, 0);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hIBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hTextureData);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hTextureDesc);
}
#undef VOLUME_SIZE

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
    Print(L"   GPUemu UEFI SAMPLING TEST Application   \n");
    Print(L"====================================================\n");
    // Print GOP information
    Print(L"GOP Located Successfully!\n");
    Print(L"Current Mode: %d\n", mGraphicsOutput->Mode->Mode);
    Print(L"Resolution: %dx%d\n",
          mGraphicsOutput->Mode->Info->HorizontalResolution,
          mGraphicsOutput->Mode->Info->VerticalResolution);
    Print(L"Pixel Format: %d\n", mGraphicsOutput->Mode->Info->PixelFormat);
    
    Print(L"1. Gop3dFilterNearest Gop3dWrapRepeat\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestSampling(Gop3dFilterNearest, Gop3dWrapRepeat);

    Print(L"2. Gop3dFilterNearest Gop3dWrapClamp\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestSampling(Gop3dFilterNearest, Gop3dWrapClamp);

    Print(L"3. Gop3dFilterNearest Gop3dWrapMirror\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestSampling(Gop3dFilterNearest, Gop3dWrapMirror);

    Print(L"4. Gop3dFilterLinear Gop3dWrapRepeat\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestSampling(Gop3dFilterLinear, Gop3dWrapRepeat);

    Print(L"5. Gop3dFilterLinear Gop3dWrapClamp\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestSampling(Gop3dFilterLinear, Gop3dWrapClamp);

    Print(L"6. Gop3dFilterLinear Gop3dWrapMirror\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestSampling(Gop3dFilterLinear, Gop3dWrapMirror);

    Print(L"7. 3D Nearest Repeat\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestVolume3D(Gop3dFilterNearest, Gop3dWrapRepeat);

    Print(L"8. 3D Nearest Clamp\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestVolume3D(Gop3dFilterNearest, Gop3dWrapClamp);

    Print(L"9. 3D Nearest Mirror\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestVolume3D(Gop3dFilterNearest, Gop3dWrapMirror);

    Print(L"10. 3D Linear Repeat\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestVolume3D(Gop3dFilterLinear, Gop3dWrapRepeat);

    Print(L"11. 3D Linear Clamp\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestVolume3D(Gop3dFilterLinear, Gop3dWrapClamp);

    Print(L"12. 3D Linear Mirror\n");
    WAIT_FOR_KEYPRESS()
    ResetGop3dDisplayState();
    TestVolume3D(Gop3dFilterLinear, Gop3dWrapMirror);

    ResetGop3dDisplayState();



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