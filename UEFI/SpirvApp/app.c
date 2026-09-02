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
#include "fps_counter.h"

#define WAIT_FOR_KEYPRESS() Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL); \
    if (!EFI_ERROR(Status)) { \
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); \
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

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGraphicsOutput = NULL;
STATIC GOP_3D_PROTOCOL              *mGOP3D          = NULL;


STATIC UINT64  mTimerFreq      = 0;
STATIC UINT64  mLastTick       = 0;
STATIC UINT64  mTotalTicks     = 0;
STATIC UINT64  mTimerMask      = 0;
STATIC BOOLEAN mTimerCountsUp  = TRUE;
STATIC BOOLEAN mTimerInit      = FALSE;

VOID GetTimeSeconds(OUT float *TimeOut) {
    if (!mTimerInit) {
        UINT64 StartVal = 0;
        UINT64 EndVal   = 0;

        mTimerFreq = GetPerformanceCounterProperties(&StartVal, &EndVal);

        // Safety Fallback
        if (mTimerFreq == 0) mTimerFreq = 1000000;

        // Determine Direction and Mask
        if (EndVal > StartVal) {
            mTimerCountsUp = TRUE;
            mTimerMask = EndVal;
        } else {
            mTimerCountsUp = FALSE;
            mTimerMask = StartVal;
        }

        mLastTick = GetPerformanceCounter();
        mTimerInit = TRUE;
    }

    UINT64 CurrentTick = GetPerformanceCounter();
    UINT64 Delta = 0;

    if (mTimerCountsUp) {
        // Handle UP counter wrap (Current < Last)
        // Using bitwise & with Mask handles the overflow math automatically
        // IF the counter is a power-of-two size (standard).
        Delta = (CurrentTick - mLastTick) & mTimerMask;
    } else {
        // Handle DOWN counter wrap
        Delta = (mLastTick - CurrentTick) & mTimerMask;
    }

    // Accumulate the small delta
    mTotalTicks += Delta;
    mLastTick    = CurrentTick;

    // Convert Total Ticks to Seconds
    *TimeOut = (float)mTotalTicks / (float)mTimerFreq;
}




VOID Test3DTrianglesSimt(){
    EFI_INPUT_KEY Key;

    // --- Data Definitions ---
    #include "mvp.h"

    #include "simple_fs.h"

    Vec3 vertices[] = {
        {-0.5, -0.5,  0.5, 0xFF0000}, { 0.5, -0.5,  0.5, 0xFF0000}, { 0.5,  0.5,  0.5, 0xFF0000}, {-0.5,  0.5,  0.5, 0x0000FF},
        {-0.5, -0.5, -0.5,0x0000FF}, { 0.5, -0.5, -0.5, 0x0000FF}, { 0.5,  0.5, -0.5,  0x00FFFF}, {-0.5,  0.5, -0.5,  0x00FFFF}
    };
    Triangle indices[] = {
        {0, 1, 2}, {0, 2, 3}, {1, 5, 6}, {1, 6, 2},
        {5, 4, 7}, {5, 7, 6}, {4, 0, 3}, {4, 3, 7},
        {3, 2, 6}, {3, 6, 7}, {4, 5, 1}, {4, 1, 0}
    };

    UINT32 IndexCount = (sizeof(indices) / sizeof(Triangle)) * 2;

    // --- Static Asset Transfer ---
    mGOP3D->GpuSetMode(mGOP3D, 1);

    VRAMADDR hVBO, hIBO, hVS, hFS;
    VRAMADDR hMVP1 = 0;
    VRAMADDR hMVP2 = 0;

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices, sizeof(vertices), &hVBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  indices,    sizeof(indices),    &hIBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader, sizeof(bin_vertex_shader), &hVS);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);

    float angle = 0.0f;
    Print(L"Animating... Press Key to Exit.\n");

    // --- START BENCHMARK ---
    FpsCounterStart();

    // Reset timer base for this run
    mTimerInit = FALSE;

    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
        float time;

        GetTimeSeconds(&time);

        INT32 Seconds = (INT32)time;
        INT32 Millis  = (INT32)((time - Seconds) * 1000); // 3 decimal places

        DEBUG((EFI_D_INFO, "Time: %d.%03d s\n", Seconds, Millis));

        float rotation_period = 2.0;
        angle = (2.0 * PI * time) / rotation_period;

        Mat4 ry1, ry2, rx, trans1, trans2, proj;
        Mat4 model1, mvp1, model2, mvp2;

        // Model 1 transformations
        Mat4_RotateY(angle, &ry1);
        Mat4_Translate(-0.5, 0.0f, 5.0f, &trans1); // Shift Left

        // Model 2 transformations
        Mat4_RotateY(-angle * 1.5f, &ry2); // Rotate opposite and slightly faster
        Mat4_Translate(0.5f, 0.0f, 5.0f, &trans2); // Shift Right
        
        // Shared transformations
        Mat4_RotateX(0.2f, &rx);
        Mat4_Perspective(PI/3.0f, 640.0f/480.0f, 1.0f, 10.0f, &proj);

        // Build Model 1 Matrix
        Mat4_Mul(&ry1, &rx, &model1);
        Mat4_Mul(&trans1, &model1, &model1);
        Mat4_Mul(&proj, &model1, &mvp1);

        // Build Model 2 Matrix
        Mat4_Mul(&ry2, &rx, &model2);
        Mat4_Mul(&trans2, &model2, &model2);
        Mat4_Mul(&proj, &model2, &mvp2);

        // Convert scalar Mat4 to SIMT-vectorized format
        SimtMat4UBO simt_mvp1, simt_mvp2;
        Mat4_ToSimtUBO(&mvp1, &simt_mvp1);
        Mat4_ToSimtUBO(&mvp2, &simt_mvp2);

        // Upload/Update Model 1 UBO
        if(hMVP1 == 0){
            mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        }
        else{
            mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        }

        // Upload/Update Model 2 UBO
        if(hMVP2 == 0){
            mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp2, sizeof(SimtMat4UBO), &hMVP2);
        }
        else{
            mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp2, sizeof(SimtMat4UBO), &hMVP2);
        }

        // --- RENDER ---
        mGOP3D->GpuCmdBegin(mGOP3D);
        mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000);
        
        mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
        mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
        mGOP3D->GpuBindVBO(mGOP3D, hVBO, 8);
        mGOP3D->GpuBindIBO(mGOP3D, hIBO, 12);

        // Draw Model 1
        mGOP3D->GpuBindUBO(mGOP3D, hMVP1, sizeof(SimtMat4UBO));
        mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);

        // Draw Model 2
        mGOP3D->GpuBindUBO(mGOP3D, hMVP2, sizeof(SimtMat4UBO));
        mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);

        mGOP3D->GpuCmdEnd(mGOP3D);
        mGOP3D->GpuPresent(mGOP3D);

        // --- TICK ---
        FpsCounterTick();
    }

    // --- STOP & SHOW STATS ---
    FpsCounterStop();

    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hIBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP1);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP2);

    mGOP3D->GpuSetMode(mGOP3D, 0);

    FpsCounterShowStats();
}

VOID TestComputeShader() {

    #include "vec_add.h"
    VRAMADDR hCS = 0;
    VRAMADDR hSSBO_A = 0;
    VRAMADDR hSSBO_B = 0;
    VRAMADDR hSSBO_C = 0;

    float input_a[16];
    float input_b[16];
    float output_c[16];

    Print(L"=== GOP3D Compute Shader Test (Vector Add) ===\n");

    for (UINT32 i = 0; i < 16; i++) {
        input_a[i] = (float)(i + 1);
        input_b[i] = (float)((i + 1) * 10);
        output_c[i] = 0.0f;
    }

    // Print Inputs
    Print(L"[INPUT DATA]\n");
    for (UINT32 i = 0; i < 16; i++) {
        Print(L"  [%02d] A = %d, B = %d  ", i, (INT32)input_a[i], (INT32)input_b[i]);
    }

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, (VOID*)bin_compute_shader, sizeof(bin_compute_shader), &hCS);
    Print(L"\n[VRAM] Shader uploaded. Handle: 0x%LX (Size: %u bytes)\n", (UINT64)hCS, (UINT32)sizeof(bin_compute_shader));

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeSSBO, input_a, sizeof(input_a), &hSSBO_A);
    Print(L"[VRAM] SSBO A Handle: 0x%LX\n", (UINT64)hSSBO_A);

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeSSBO, input_b, sizeof(input_b), &hSSBO_B);
    Print(L"[VRAM] SSBO B Handle: 0x%LX\n", (UINT64)hSSBO_B);

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeSSBO, output_c, sizeof(output_c), &hSSBO_C);
    Print(L"[VRAM] SSBO C Handle: 0x%LX\n", (UINT64)hSSBO_C);

    Print(L"[CMD] Recording Compute Command Batch (1, 1, 1)...\n");
    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuBindCompShader(mGOP3D, hCS, sizeof(bin_compute_shader));
    mGOP3D->GpuBindSSBO(mGOP3D, 0, hSSBO_A, sizeof(input_a));
    mGOP3D->GpuBindSSBO(mGOP3D, 1, hSSBO_B, sizeof(input_b));
    mGOP3D->GpuBindSSBO(mGOP3D, 2, hSSBO_C, sizeof(output_c));
    mGOP3D->GpuDispatchCompute(mGOP3D, 1, 1, 1);
    mGOP3D->GpuCmdEnd(mGOP3D);

    Print(L"[CMD] Submitting GPU commands (GpuPresent)...\n");
    mGOP3D->GpuPresent(mGOP3D);

    Print(L"[VRAM] Reading back compute shader results from SSBO C...\n");
    mGOP3D->GpuReadBuffer(mGOP3D, hSSBO_C, output_c, sizeof(output_c));

    Print(L"[OUTPUT DATA]\n");
    UINT32 pass_count = 0;
    for (UINT32 i = 0; i < 16; i++) {
        Print(L"  [%02d] C = %d  ", i, (INT32)output_c[i]);
        if ((INT32)output_c[i] == (INT32)(input_a[i] + input_b[i])) {
            pass_count++;
        }
    }
    Print(L"\n");
    if (pass_count == 16) {
        Print(L"==> Compute Shader Test PASSED (%u/16 matches) <==\n", pass_count);
    } else {
        Print(L"==> Compute Shader Test FAILED (%u/16 matches) <==\n", pass_count);
    }

    Print(L"[VRAM] Freeing allocated GPU buffers...\n");
    mGOP3D->GpuFreeBuffer(mGOP3D, &hCS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hSSBO_A);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hSSBO_B);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hSSBO_C);

    Print(L"GOP3D: Compute Shader Dispatch Complete.\n");
}

VOID TestBarrierComputeShader(VOID) {
    #include "barrier_reduction.h"
    VRAMADDR hCS = 0;
    VRAMADDR hSSBO_In = 0;
    VRAMADDR hSSBO_Out = 0;

    float input_data[16];
    float output_data[1];

    Print(L"\n=== GOP3D Compute Shader Test (Barrier Parallel Reduction) ===\n");

    for (UINT32 i = 0; i < 16; i++) {
        input_data[i] = (float)(i + 1);
    }
    output_data[0] = 0.0f;

    Print(L"[INPUT DATA] 16 elements: 1..16 (Expected Sum = 136)\n");

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, (VOID*)bin_barrier_reduction_shader, sizeof(bin_barrier_reduction_shader), &hCS);
    Print(L"[VRAM] Barrier Shader uploaded. Handle: 0x%LX (Size: %u bytes)\n", (UINT64)hCS, (UINT32)sizeof(bin_barrier_reduction_shader));

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeSSBO, input_data, sizeof(input_data), &hSSBO_In);
    Print(L"[VRAM] SSBO In Handle: 0x%LX\n", (UINT64)hSSBO_In);

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeSSBO, output_data, sizeof(output_data), &hSSBO_Out);
    Print(L"[VRAM] SSBO Out Handle: 0x%LX\n", (UINT64)hSSBO_Out);

    Print(L"[CMD] Recording Barrier Compute Command Batch (1, 1, 1)...\n");
    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuBindCompShader(mGOP3D, hCS, sizeof(bin_barrier_reduction_shader));
    mGOP3D->GpuBindSSBO(mGOP3D, 0, hSSBO_In, sizeof(input_data));
    mGOP3D->GpuBindSSBO(mGOP3D, 1, hSSBO_Out, sizeof(output_data));
    mGOP3D->GpuDispatchCompute(mGOP3D, 1, 1, 1);
    mGOP3D->GpuCmdEnd(mGOP3D);

    Print(L"[CMD] Submitting GPU commands (GpuPresent)...\n");
    mGOP3D->GpuPresent(mGOP3D);

    Print(L"[VRAM] Reading back reduction result from SSBO Out...\n");
    mGOP3D->GpuReadBuffer(mGOP3D, hSSBO_Out, output_data, sizeof(output_data));

    Print(L"[OUTPUT DATA] Reduced Sum = %d\n", (INT32)output_data[0]);

    if ((INT32)output_data[0] == 136) {
        Print(L"==> Barrier Parallel Reduction Test PASSED (Sum = 136) <==\n");
    } else {
        Print(L"==> Barrier Parallel Reduction Test FAILED (Sum = %d, Expected 136) <==\n", (INT32)output_data[0]);
    }

    Print(L"[VRAM] Freeing allocated GPU buffers...\n");
    mGOP3D->GpuFreeBuffer(mGOP3D, &hCS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hSSBO_In);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hSSBO_Out);

    Print(L"GOP3D: Barrier Compute Shader Test Complete.\n");
}

VOID TestShaderArt() {
    EFI_INPUT_KEY Key;

    Vec3 vertices[] = {
        {-1.0f, -1.0f, 0.0f, 0xFFFF00},
        { 1.0f, -1.0f, 0.0f, 0xFFFF00},
        {-1.0f,  1.0f, 0.0f, 0xFFFF00},
        { 1.0f,  1.0f, 0.0f, 0xFFFF00}
    };

    Triangle indices[] = {
        {0, 1, 2},
        {1, 3, 2}
    };

    UINT32 IndexCount = 6;

    // --- Data Definitions ---
    #include "simple_vs.h"        
    #include "art.h"

    mGOP3D->GpuSetMode(mGOP3D, 1);

    VRAMADDR hVBO, hIBO, hVS, hFS, hMVP1 = 0;

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices, sizeof(vertices), &hVBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  indices,    sizeof(indices),    &hIBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader, sizeof(bin_vertex_shader), &hVS);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);

    Print(L"Rendering Full Screen Quad... Press Key to Exit.\n");
    FpsCounterStart();
    mTimerInit = FALSE;


    struct  UniformBuffer {
        float iTime[16];
    } uniform;

    float time = 0.0f;
    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
        GetTimeSeconds(&time);
        for(UINT32 i = 0; i<16; i++)
        {
            uniform.iTime[i] = time;
        }
        if(hMVP1 == 0){
            mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &uniform, sizeof(struct UniformBuffer), &hMVP1);
        } else {
            mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &uniform, sizeof(struct UniformBuffer), &hMVP1);
        }

        mGOP3D->GpuCmdBegin(mGOP3D);
        mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000);

        mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
        mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
        mGOP3D->GpuBindVBO(mGOP3D, hVBO, 4);
        mGOP3D->GpuBindIBO(mGOP3D, hIBO, 2);

        mGOP3D->GpuBindUBO(mGOP3D, hMVP1, sizeof(struct UniformBuffer));
        mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);

        mGOP3D->GpuCmdEnd(mGOP3D);
        mGOP3D->GpuPresent(mGOP3D);

        FpsCounterTick();
    }

    // --- Cleanup ---
    FpsCounterStop();
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hIBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP1);
    mGOP3D->GpuSetMode(mGOP3D, 0);
    FpsCounterShowStats();
}
VOID Test3DObszar()
{
    EFI_INPUT_KEY Key;
    #include "sampler_vs.h"

    #include "sampler_fs.h"

    #include "image.h"

  
    Vec3 vertices[] = {
        // --- Front Face (z=+0.7) ---
        {-0.7f, -0.7f,  0.7f, 0x0D00D0, 0.0f, 0.0f},
        { 0.7f, -0.7f,  0.7f, 0xFD0000, 1.0f, 0.0f},
        { 0.7f,  0.7f,  0.7f, 0xFFDF00, 1.0f, 1.0f},
        {-0.7f,  0.7f,  0.7f, 0x00FF00, 0.0f, 1.0f},

        // --- Back Face (z=-0.7) ---
        { 0.7f, -0.7f, -0.7f, 0x00D000, 0.0f, 0.0f},
        {-0.7f, -0.7f, -0.7f, 0xFFD000, 1.0f, 0.0f},
        {-0.7f,  0.7f, -0.7f, 0xDFFF00, 1.0f, 1.0f},
        { 0.7f,  0.7f, -0.7f, 0xD0FF00, 0.0f, 1.0f},

        // --- Left Face (x=-0.7) ---
        {-0.7f, -0.7f, -0.7f, 0x000000, 0.0f, 0.0f},
        {-0.7f, -0.7f,  0.7f, 0xFD0000, 1.0f, 0.0f},
        {-0.7f,  0.7f,  0.7f, 0xFFFD00, 1.0f, 1.0f},
        {-0.7f,  0.7f, -0.7f, 0x00FF00, 0.0f, 1.0f},

        // --- Right Face (x=+0.7) ---
        { 0.7f, -0.7f,  0.7f, 0x000000, 0.0f, 0.0f},
        { 0.7f, -0.7f, -0.7f, 0xFF0D00, 1.0f, 0.0f},
        { 0.7f,  0.7f, -0.7f, 0xFFFF00, 1.0f, 1.0f},
        { 0.7f,  0.7f,  0.7f, 0x00FF00, 0.0f, 1.0f},

        // --- Top Face (y=+0.7) ---
        {-0.7f,  0.7f,  0.7f, 0x000000, 0.0f, 0.0f},
        { 0.7f,  0.7f,  0.7f, 0xFF0000, 1.0f, 0.0f},
        { 0.7f,  0.7f, -0.7f, 0xFFFF00, 1.0f, 1.0f},
        {-0.7f,  0.7f, -0.7f, 0x00FF00, 0.0f, 1.0f},

        // --- Bottom Face (y=-0.7) ---
        {-0.7f, -0.7f, -0.7f, 0x000000, 0.0f, 0.0f},
        { 0.7f, -0.7f, -0.7f, 0xFF0000, 1.0f, 0.0f},
        { 0.7f, -0.7f,  0.7f, 0xFFFF00, 1.0f, 1.0f},
        {-0.7f, -0.7f,  0.7f, 0x00FF00, 0.0f, 1.0f}
    };

    // Indices adjusted for the 24-vertex layout (4 vertices per face offset)
    Triangle indices[] = {
        // Front
        {0, 1, 2}, {0, 2, 3},
        // Back
        {4, 5, 6}, {4, 6, 7},
        // Left
        {8, 9, 10}, {8, 10, 11},
        // Right
        {12, 13, 14}, {12, 14, 15},
        // Top
        {16, 17, 18}, {16, 18, 19},
        // Bottom
        {20, 21, 22}, {20, 22, 23}
    };
    UINT32 IndexCount = (sizeof(indices) / sizeof(Triangle)) * 2;

    // --- Static Asset Transfer ---
    mGOP3D->GpuSetMode(mGOP3D, 1);

    VRAMADDR hVBO, hIBO, hVS, hFS, hTexData, hTexDesc;
    VRAMADDR hMVP1 = 0;

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices, sizeof(vertices), &hVBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  indices,    sizeof(indices),    &hIBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader, sizeof(bin_vertex_shader), &hVS);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeTexture, image, sizeof(image), &hTexData);

    GOP_3D_TEXTURE_DESC tex_desc = {
        .DataAddr = hTexData,
        .Width = 200,
        .Height = 200,
        .Channels = 3,
        .Filter = Gop3dFilterLinear,
        .Wrap = Gop3dWrapRepeat
    };
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeTextureDesc, &tex_desc, sizeof(tex_desc), &hTexDesc);
    float angle = 0.0f;
    Print(L"Animating... Press Key to Exit.\n");

    // --- START BENCHMARK ---
    FpsCounterStart();

    // Reset timer base for this run
    mTimerInit = FALSE;

    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
        float time;

        GetTimeSeconds(&time);

        INT32 Seconds = (INT32)time;
        INT32 Millis  = (INT32)((time - Seconds) * 1000); // 3 decimal places

        DEBUG((EFI_D_INFO, "Time: %d.%03d s\n", Seconds, Millis));

        float rotation_period = 2.0;
        angle = (2.0 * PI * time) / rotation_period;

        Mat4 ry, rx, trans, proj;
        Mat4 model1, mvp1;

        Mat4_RotateY(angle, &ry);
        Mat4_RotateX(0.2f, &rx);
        Mat4_Translate(0.0f, 0.0f, 5.0f, &trans);
        Mat4_Perspective(PI/3.0f, 640.0f/480.0f, 1.0f, 10.0f, &proj);

        Mat4_Mul(&ry, &rx, &model1);
        Mat4_Mul(&trans, &model1, &model1);
        Mat4_Mul(&proj, &model1, &mvp1);

        // Convert scalar Mat4 to SIMT-vectorized format
        SimtMat4UBO simt_mvp1;
        Mat4_ToSimtUBO(&mvp1, &simt_mvp1);

        if(hMVP1 == 0){
            mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        }
        else{
            mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        }

        // --- RENDER ---
        mGOP3D->GpuCmdBegin(mGOP3D);
        mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000);
        
        mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
        mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
        mGOP3D->GpuBindVBO(mGOP3D, hVBO, 24);
        mGOP3D->GpuBindIBO(mGOP3D, hIBO, 12);
        mGOP3D->GpuBindTexture(mGOP3D, 1, hTexDesc);

        mGOP3D->GpuBindUBO(mGOP3D, hMVP1, sizeof(SimtMat4UBO));
        mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);


        mGOP3D->GpuCmdEnd(mGOP3D);
        mGOP3D->GpuPresent(mGOP3D);

        // --- TICK ---
        FpsCounterTick();
    }

    // --- STOP & SHOW STATS ---
    FpsCounterStop();

    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hIBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP1);

    mGOP3D->GpuSetMode(mGOP3D, 0);

    FpsCounterShowStats();
}

VOID TestBlendingSimt() {
    EFI_INPUT_KEY Key;

    // --- Data Definitions ---
    #include "mvp.h"

    #include "simple_fs.h"

    // --- Mesh 1: Solid Opaque Background Cube (Alpha = 0xFF) ---
    Vec3 vertices_opaque[] = {
        {-0.5, -0.5,  0.5, 0xFFFF0000}, { 0.5, -0.5,  0.5, 0xFFFF0000}, { 0.5,  0.5,  0.5, 0xFFFF0000}, {-0.5,  0.5,  0.5, 0xFF0000FF},
        {-0.5, -0.5, -0.5, 0xFF0000FF}, { 0.5, -0.5, -0.5, 0xFF0000FF}, { 0.5,  0.5, -0.5, 0xFF00FFFF}, {-0.5,  0.5, -0.5, 0xFF00FFFF}
    };

    // --- Mesh 2: Semi-Transparent Foreground Cube (Alpha = 0x80 -> ~50% Opacity) ---
    Vec3 vertices_transparent[] = {
        {-0.5, -0.5,  0.5, 0x8000FF00}, { 0.5, -0.5,  0.5, 0x8000FF00}, { 0.5,  0.5,  0.5, 0x8000FF00}, {-0.5,  0.5,  0.5, 0x80FFFF00},
        {-0.5, -0.5, -0.5, 0x80FFFF00}, { 0.5, -0.5, -0.5, 0x80FFFF00}, { 0.5,  0.5, -0.5, 0x8000FFFF}, {-0.5,  0.5, -0.5, 0x8000FFFF}
    };

    Triangle indices[] = {
        {0, 1, 2}, {0, 2, 3}, {1, 5, 6}, {1, 6, 2},
        {5, 4, 7}, {5, 7, 6}, {4, 0, 3}, {4, 3, 7},
        {3, 2, 6}, {3, 6, 7}, {4, 5, 1}, {4, 1, 0}
    };

    UINT32 IndexCount = (sizeof(indices) / sizeof(Triangle)) * 2;

    // --- Static Asset Transfer ---
    mGOP3D->GpuSetMode(mGOP3D, 1);

    VRAMADDR hVBO_Opaque, hVBO_Transparent, hIBO, hVS, hFS;
    VRAMADDR hMVP1 = 0;
    VRAMADDR hMVP2 = 0;

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices_opaque,      sizeof(vertices_opaque),      &hVBO_Opaque);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, vertices_transparent, sizeof(vertices_transparent), &hVBO_Transparent);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  indices,             sizeof(indices),             &hIBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader,   sizeof(bin_vertex_shader),   &hVS);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);

    float angle = 0.0f;
    Print(L"Testing Alpha Blending & Depth Masking... Press Key to Exit.\n");

    FpsCounterStart();
    mTimerInit = FALSE;

    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
        float time;
        GetTimeSeconds(&time);

        float rotation_period = 2.0f;
        angle = (2.0f * PI * time) / rotation_period;

        Mat4 ry1, ry2, rx, trans1, trans2, proj;
        Mat4 model1, mvp1, model2, mvp2;

        // Model 1 (Opaque Background): Slightly behind at z = 5.0f
        Mat4_RotateY(angle, &ry1);
        Mat4_Translate(-0.2f, 0.0f, 5.0f, &trans1);

        // Model 2 (Transparent Foreground): Positioned in front at z = 4.3f to overlap Model 1
        Mat4_RotateY(-angle * 1.5f, &ry2);
        Mat4_Translate(0.2f, 0.0f, 4.3f, &trans2);
        
        Mat4_RotateX(0.2f, &rx);
        Mat4_Perspective(PI / 3.0f, 640.0f / 480.0f, 1.0f, 10.0f, &proj);

        // Build Matrices
        Mat4_Mul(&ry1, &rx, &model1);
        Mat4_Mul(&trans1, &model1, &model1);
        Mat4_Mul(&proj, &model1, &mvp1);

        Mat4_Mul(&ry2, &rx, &model2);
        Mat4_Mul(&trans2, &model2, &model2);
        Mat4_Mul(&proj, &model2, &mvp2);

        SimtMat4UBO simt_mvp1, simt_mvp2;
        Mat4_ToSimtUBO(&mvp1, &simt_mvp1);
        Mat4_ToSimtUBO(&mvp2, &simt_mvp2);

        if (hMVP1 == 0) {
            mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        } else {
            mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp1, sizeof(SimtMat4UBO), &hMVP1);
        }

        if (hMVP2 == 0) {
            mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp2, sizeof(SimtMat4UBO), &hMVP2);
        } else {
            mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp2, sizeof(SimtMat4UBO), &hMVP2);
        }

        // --- RENDER PASS ---
        mGOP3D->GpuCmdBegin(mGOP3D);
        mGOP3D->GpuClearFrame(mGOP3D, 0xFF101010); // Clear to dark gray
        
        mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
        mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
        mGOP3D->GpuBindIBO(mGOP3D, hIBO, 12);

        // ---------------------------------------------------------------------
        // PASS 1: Render Opaque Object (Solid Red/Blue Cube)
        // ---------------------------------------------------------------------
        mGOP3D->GpuSetBlendState(mGOP3D, FALSE, Gop3dBlendFactorOne, Gop3dBlendFactorZero);
        mGOP3D->GpuSetDepthWrite(mGOP3D, TRUE); // Enable Z-buffer updates

        mGOP3D->GpuBindVBO(mGOP3D, hVBO_Opaque, 8);
        mGOP3D->GpuBindUBO(mGOP3D, hMVP1, sizeof(SimtMat4UBO));
        mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);

        // ---------------------------------------------------------------------
        // PASS 2: Render Transparent Object with Alpha Blending
        // ---------------------------------------------------------------------
        mGOP3D->GpuSetBlendState(mGOP3D, TRUE, Gop3dBlendFactorSrcAlpha, Gop3dBlendFactorOneMinusSrcAlpha);
        mGOP3D->GpuSetDepthWrite(mGOP3D, FALSE); // Disable Z-writes to avoid blocking subsequent geometry

        mGOP3D->GpuBindVBO(mGOP3D, hVBO_Transparent, 8);
        mGOP3D->GpuBindUBO(mGOP3D, hMVP2, sizeof(SimtMat4UBO));
        mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, IndexCount);

        mGOP3D->GpuCmdEnd(mGOP3D);
        mGOP3D->GpuPresent(mGOP3D);

        FpsCounterTick();
    }

    // --- CLEANUP ---
    FpsCounterStop();

    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_Opaque);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_Transparent);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hIBO);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP1);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP2);

    mGOP3D->GpuSetMode(mGOP3D, 0);

    FpsCounterShowStats();
}

VOID TestPrimitivesSimt() {
    EFI_INPUT_KEY Key;
    EFI_STATUS Status;

    #include "mvp.h"
    #include "simple_fs.h"

    mGOP3D->GpuSetMode(mGOP3D, 1);

    VRAMADDR hVS, hFS, hMVP;
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader, sizeof(bin_vertex_shader), &hVS);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);

    Mat4 identity;
    Mat4_Identity(&identity);
    SimtMat4UBO simt_mvp;
    Mat4_ToSimtUBO(&identity, &simt_mvp);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &simt_mvp, sizeof(SimtMat4UBO), &hMVP);

    Print(L"Testing New Primitives (Points, Lines, LineStrip, TriStrip, TriFan, Quads)...\n");

    // 1. Points
    Vec3 point_verts[] = {
        {-0.6f,  0.5f, 0.5f, 0xFF0000},
        {-0.2f,  0.5f, 0.5f, 0x00FF00},
        { 0.2f,  0.5f, 0.5f, 0x0000FF},
        { 0.6f,  0.5f, 0.5f, 0xFFFF00}
    };
    VRAMADDR hVBO_Points;
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, point_verts, sizeof(point_verts), &hVBO_Points);

    // 2. Lines
    Vec3 line_verts[] = {
        {-0.8f,  0.2f, 0.5f, 0xFF0000}, {-0.4f,  0.2f, 0.5f, 0x00FF00},
        { 0.4f,  0.2f, 0.5f, 0x0000FF}, { 0.8f,  0.2f, 0.5f, 0xFFFF00}
    };
    VRAMADDR hVBO_Lines;
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, line_verts, sizeof(line_verts), &hVBO_Lines);

    // 3. Line Strip
    Vec3 line_strip_verts[] = {
        {-0.8f, -0.1f, 0.5f, 0xFF0000},
        {-0.4f,  0.1f, 0.5f, 0x00FF00},
        { 0.0f, -0.1f, 0.5f, 0x0000FF},
        { 0.4f,  0.1f, 0.5f, 0xFFFF00},
        { 0.8f, -0.1f, 0.5f, 0x00FFFF}
    };
    VRAMADDR hVBO_LineStrip;
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, line_strip_verts, sizeof(line_strip_verts), &hVBO_LineStrip);

    // 4. Triangle Strip
    Vec3 tri_strip_verts[] = {
        {-0.8f, -0.5f, 0.5f, 0xFF0000},
        {-0.8f, -0.2f, 0.5f, 0x00FF00},
        {-0.5f, -0.5f, 0.5f, 0x0000FF},
        {-0.5f, -0.2f, 0.5f, 0xFFFF00}
    };
    VRAMADDR hVBO_TriStrip;
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, tri_strip_verts, sizeof(tri_strip_verts), &hVBO_TriStrip);

    // 5. Triangle Fan
    Vec3 tri_fan_verts[] = {
        { 0.0f, -0.35f, 0.5f, 0xFF0000},
        {-0.2f, -0.5f,  0.5f, 0xFF0000},
        { 0.0f, -0.6f,  0.5f, 0x00FF00},
        { 0.2f, -0.5f,  0.5f, 0x0000FF},
        { 0.1f, -0.2f,  0.5f, 0xFFFF00}
    };
    VRAMADDR hVBO_TriFan;
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, tri_fan_verts, sizeof(tri_fan_verts), &hVBO_TriFan);

    // 6. Quads
    Vec3 quad_verts[] = {
        { 0.4f, -0.6f, 0.5f, 0xFF0000},
        { 0.8f, -0.6f, 0.5f, 0x00FF00},
        { 0.8f, -0.2f, 0.5f, 0x0000FF},
        { 0.4f, -0.2f, 0.5f, 0xFFFF00}
    };
    VRAMADDR hVBO_Quads;
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, quad_verts, sizeof(quad_verts), &hVBO_Quads);

    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuClearFrame(mGOP3D, 0xFF101010);
    mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
    mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));

    // Render Points
    mGOP3D->GpuBindVBO(mGOP3D, hVBO_Points, 4);
    mGOP3D->GpuBindUBO(mGOP3D, hMVP, sizeof(SimtMat4UBO));
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyPoints, 4);

    // Render Lines
    mGOP3D->GpuBindVBO(mGOP3D, hVBO_Lines, 4);
    mGOP3D->GpuBindUBO(mGOP3D, hMVP, sizeof(SimtMat4UBO));
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLines, 4);

    // Render Line Strip
    mGOP3D->GpuBindVBO(mGOP3D, hVBO_LineStrip, 5);
    mGOP3D->GpuBindUBO(mGOP3D, hMVP, sizeof(SimtMat4UBO));
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLineStrip, 5);

    // Render Triangle Strip
    mGOP3D->GpuBindVBO(mGOP3D, hVBO_TriStrip, 4);
    mGOP3D->GpuBindUBO(mGOP3D, hMVP, sizeof(SimtMat4UBO));
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangleStrip, 4);

    // Render Triangle Fan
    mGOP3D->GpuBindVBO(mGOP3D, hVBO_TriFan, 5);
    mGOP3D->GpuBindUBO(mGOP3D, hMVP, sizeof(SimtMat4UBO));
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangleFan, 5);

    // Render Quads
    mGOP3D->GpuBindVBO(mGOP3D, hVBO_Quads, 4);
    mGOP3D->GpuBindUBO(mGOP3D, hMVP, sizeof(SimtMat4UBO));
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyQuads, 4);

    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuPresent(mGOP3D);

    Print(L"Rendered Points, Lines, LineStrip, TriStrip, TriFan, Quads! Press key...\n");
    WAIT_FOR_KEYPRESS();

    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_Points);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_Lines);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_LineStrip);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_TriStrip);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_TriFan);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO_Quads);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
    mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP);
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
    Print(L"   GPUemu UEFI SPIR-V SIMT JIT Shader Application   \n");
    Print(L"====================================================\n");
    // Print GOP information
    Print(L"GOP Located Successfully!\n");
    Print(L"Current Mode: %d\n", mGraphicsOutput->Mode->Mode);
    Print(L"Resolution: %dx%d\n",
          mGraphicsOutput->Mode->Info->HorizontalResolution,
          mGraphicsOutput->Mode->Info->VerticalResolution);
    Print(L"Pixel Format: %d\n", mGraphicsOutput->Mode->Info->PixelFormat);
    WAIT_FOR_KEYPRESS()

    Print(L"Press key for SPIR-V Compute Shader Test (Vector Add)...\n");
    TestComputeShader();
    Print(L"Press key for SPIR-V Barrier Compute Shader Test (Parallel Reduction)...\n");
    WAIT_FOR_KEYPRESS()
    TestBarrierComputeShader();
    Print(L"Press key for SPIR-V Primitives Demo (Points, Lines, LineStrip, TriStrip, TriFan, Quads)...\n");

    WAIT_FOR_KEYPRESS()

    TestPrimitivesSimt();

    Print(L"Press key for SPIR-V 3D SIMT Triangles Demo...\n");

    WAIT_FOR_KEYPRESS()

    Test3DTrianglesSimt();

    WAIT_FOR_KEYPRESS()

    TestShaderArt();

    WAIT_FOR_KEYPRESS()

    Test3DObszar();

    WAIT_FOR_KEYPRESS()

    TestBlendingSimt();
    


    WAIT_FOR_KEYPRESS()

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