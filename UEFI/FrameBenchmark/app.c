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
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Pci.h>
#include <Library/UefiBootServicesTableLib.h> 
#include <Library/DevicePathLib.h>
#include <Library/TimerLib.h> // Required for high-resolution timing

#include "math3d.h"

#define WAIT_FOR_KEYPRESS() Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL); \
    if (!EFI_ERROR(Status)) { \
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); \
    }

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGraphicsOutput = NULL;
STATIC GOP_3D_PROTOCOL              *mGOP3D          = NULL;

// --- Benchmark Helpers ---

typedef struct {
    UINT64 StartTick;
    UINT64 EndTick;
    UINT64 Freq;
    CONST CHAR16* Name;
} BENCH_TIMER;

/**
 * Initialize and start the benchmark timer.
 * Captures the current tick count and timer frequency.
 */
static void BenchStart(BENCH_TIMER* Timer, CONST CHAR16* Name) {
    Timer->Name = Name;
    Timer->Freq = GetPerformanceCounterProperties(NULL, NULL);
    Timer->StartTick = GetPerformanceCounter();
}

/**
 * Stop the benchmark timer by capturing the current tick count.
 */
static void BenchStop(BENCH_TIMER* Timer) {
    Timer->EndTick = GetPerformanceCounter();
}

/**
 * Calculate the elapsed time in microseconds.
 */
static UINT64 BenchGetDurationMicroseconds(BENCH_TIMER* Timer) {
    UINT64 Delta = 0;
    
    // Determine the difference based on timer direction
    if (Timer->StartTick > Timer->EndTick) {
         Delta = Timer->StartTick - Timer->EndTick;
    } else {
         Delta = Timer->EndTick - Timer->StartTick;
    }
    
    return DivU64x64Remainder(MultU64x64(Delta, 1000000), Timer->Freq, NULL);
}

/**
 * Print the benchmark result to the console.
 */
static void BenchPrint(BENCH_TIMER* Timer) {
    UINT64 us = BenchGetDurationMicroseconds(Timer);
    Print(L" %-25s : %ld us\n", Timer->Name, us);
}


VOID TestFrame(){
    // Shader Binaries
    UINT64 bin_vertex_shader[]   = { 0x80000916, 0x0, 0xC5010901, 0x8, 0x80010906, 0x0, 0x907, 0x0 };
    UINT64 bin_fragment_shader[] = { 0xFF000A0900, 0x0, 0xB0900, 0x0, 0xC0900, 0x0, 0x801000408, 0x12C, 0x64000C0800, 0x0, 0xA0800, 0x0, 0x907, 0x0 };

    // Cube Data
    Vec3 cube_vertices[] = {
        { -1.0f, -1.0f, -1.0f, 0xFFFF0000 }, {  1.0f, -1.0f, -1.0f, 0xFF00FF00 },
        {  1.0f,  1.0f, -1.0f, 0xFF0000FF }, { -1.0f,  1.0f, -1.0f, 0xFFFFFF00 }, 
        { -1.0f, -1.0f,  1.0f, 0xFFFF00FF }, {  1.0f, -1.0f,  1.0f, 0xFF00FFFF },
        {  1.0f,  1.0f,  1.0f, 0xFFFFFFFF }, { -1.0f,  1.0f,  1.0f, 0xFF808080 }  
    };
    
    Edge cube_edges[] = { 
        {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, 
        {0,4}, {1,5}, {2,6}, {3,7}, {5,3} 
    };
    
    UINT32 IndexCount = (sizeof(cube_edges) / sizeof(Edge)) * 2; 
    
    // Define Timers
    BENCH_TIMER TotalTimer, StaticDataTimer, MvpTimer, CmdRecTimer, RenderTimer;

    // --- BENCHMARK START ---
    // TOTAL TIMER START
    BenchStart(&TotalTimer, L"Total Frame Time");

    mGOP3D->GpuSetMode(mGOP3D, 1); 

    // STATIC DATA TIMER START
    BenchStart(&StaticDataTimer, L"Static Data Transfer");
    VRAMADDR hVBO, hIBO, hVS, hFS;
    VRAMADDR hMVP1 = 0; 

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, cube_vertices, sizeof(cube_vertices), &hVBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  cube_edges,    sizeof(cube_edges),    &hIBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_vertex_shader, sizeof(bin_vertex_shader), &hVS);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, bin_fragment_shader, sizeof(bin_fragment_shader), &hFS);
    BenchStop(&StaticDataTimer);
    // STATIC DATA TIMER END

    // MVP DATA TIMER START
    BenchStart(&MvpTimer, L"Matrix Calc & Upload");
    Mat4 ry, rx, scale, trans, proj;
    Mat4 model1, mvp1;
    
    Mat4_RotateY(0.0f, &ry);
    Mat4_RotateX(0.2f, &rx);
    Mat4_Scale(0.5f, &scale);
    Mat4_Translate(0.0f, 0.0f, 5.0f, &trans);
    Mat4_Perspective(PI/3.0f, 640.0f/480.0f, 1.0f, 10.0f, &proj);

    Mat4_Mul(&ry, &rx, &model1);
    Mat4_Mul(&scale, &model1, &model1); 
    Mat4_Mul(&trans, &model1, &model1);
    Mat4_Mul(&proj, &model1, &mvp1);

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &mvp1, sizeof(Mat4), &hMVP1);
    BenchStop(&MvpTimer);
    // MVP DATA TIMER END

    // COMMAND BUFFER RECORDING TIMER START
    BenchStart(&CmdRecTimer, L"Command Recording");
    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000); 
    
    mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
    mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
    mGOP3D->GpuBindVBO(mGOP3D, hVBO, 8);
    mGOP3D->GpuBindIBO(mGOP3D, hIBO, 13);

    mGOP3D->GpuBindUBO(mGOP3D, hMVP1, sizeof(Mat4)); 
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLines, IndexCount); 
    
    mGOP3D->GpuCmdEnd(mGOP3D);
    BenchStop(&CmdRecTimer);
    // COMMAND BUFFER RECORDING TIMER END

    // SUBMISSION AND RENDER TIMER START
    BenchStart(&RenderTimer, L"Submit & Present");
    mGOP3D->GpuPresent(mGOP3D);        
    BenchStop(&RenderTimer);
    // SUBMISSION AND RENDER TIMER END

    mGOP3D->GpuSetMode(mGOP3D, 0); 
    BenchStop(&TotalTimer);
    // TOTAL TIMER END

    // --- Show Stats ---
    Print(L"\n========================================\n");
    Print(L"         FRAME BREAKDOWN (Single)       \n");
    Print(L"========================================\n");
    BenchPrint(&StaticDataTimer);
    BenchPrint(&MvpTimer);
    BenchPrint(&CmdRecTimer);
    BenchPrint(&RenderTimer);
    Print(L"----------------------------------------\n");
    BenchPrint(&TotalTimer);
    Print(L"========================================\n");
}

EFI_STATUS EFIAPI Test() {
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    DEBUG((EFI_D_INFO, "Frame Benchmark Start\n"));

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

    TestFrame();

    Print(L"\nPress any key to exit...\n");
    WAIT_FOR_KEYPRESS()

    DEBUG((EFI_D_INFO, "Frame Benchmark End\n"));
    return EFI_SUCCESS;
}

/*==========================================================================*/
/*                              MAIN APP ENTRY                              */
/*==========================================================================*/

EFI_STATUS EFIAPI FrameBenchmarkEntry(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable) {
        
    EFI_STATUS Status;

    Status = Test();

    ASSERT_EFI_ERROR(Status);
    
    return Status;
}

EFI_STATUS EFIAPI FrameBenchmarkEntryUnload (IN  EFI_HANDLE  ImageHandle) {
    return EFI_SUCCESS;
}