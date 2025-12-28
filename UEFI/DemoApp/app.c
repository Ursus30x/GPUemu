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

/*==========================================================================*/
/*                                  GOP TEST                                */
/*==========================================================================*/
VOID DrawPixel(UINT32 X, UINT32 Y, EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Color) {
    if (X >= mGraphicsOutput->Mode->Info->HorizontalResolution ||
        Y >= mGraphicsOutput->Mode->Info->VerticalResolution) {
        return;
    }
    
    mGraphicsOutput->Blt(
        mGraphicsOutput,
        Color,
        EfiBltVideoFill,
        0, 0,
        X, Y,
        1, 1,
        0
    );
}

VOID DrawRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, 
              EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Color) {
    mGraphicsOutput->Blt(
        mGraphicsOutput,
        Color,
        EfiBltVideoFill,
        0, 0,
        X, Y,
        Width, Height,
        0
    );
}

VOID DrawTestPattern() {
    UINT32 Width = mGraphicsOutput->Mode->Info->HorizontalResolution;
    UINT32 Height = mGraphicsOutput->Mode->Info->VerticalResolution;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color;
    UINT32 i, j;
    
    // Clear screen to black
    Color.Blue = 0;
    Color.Green = 0;
    Color.Red = 0;
    Color.Reserved = 0;
    DrawRect(0, 0, Width, Height, &Color);
    
    // Draw color bars in the top third
    UINT32 BarHeight = Height / 3;
    UINT32 BarWidth = Width / 7;
    
    // Red
    Color.Red = 255; Color.Green = 0; Color.Blue = 0;
    DrawRect(0, 0, BarWidth, BarHeight, &Color);
    
    // Green
    Color.Red = 0; Color.Green = 255; Color.Blue = 0;
    DrawRect(BarWidth, 0, BarWidth, BarHeight, &Color);
    
    // Blue
    Color.Red = 0; Color.Green = 0; Color.Blue = 255;
    DrawRect(BarWidth * 2, 0, BarWidth, BarHeight, &Color);
    
    // Yellow
    Color.Red = 255; Color.Green = 255; Color.Blue = 0;
    DrawRect(BarWidth * 3, 0, BarWidth, BarHeight, &Color);
    
    // Cyan
    Color.Red = 0; Color.Green = 255; Color.Blue = 255;
    DrawRect(BarWidth * 4, 0, BarWidth, BarHeight, &Color);
    
    // Magenta
    Color.Red = 255; Color.Green = 0; Color.Blue = 255;
    DrawRect(BarWidth * 5, 0, BarWidth, BarHeight, &Color);
    
    // White
    Color.Red = 255; Color.Green = 255; Color.Blue = 255;
    DrawRect(BarWidth * 6, 0, Width - (BarWidth * 6), BarHeight, &Color);
    
    // Draw horizontal gradient in the middle third
    UINT32 GradientStart = BarHeight;
    UINT32 GradientHeight = Height / 3;
    
    for (i = 0; i < Width; i++) {
        UINT8 Value = (UINT8)((i * 255) / Width);
        Color.Red = Value;
        Color.Green = Value;
        Color.Blue = Value;
        DrawRect(i, GradientStart, 1, GradientHeight, &Color);
    }
    
    // Draw checkerboard pattern in the bottom third
    UINT32 CheckerStart = GradientStart + GradientHeight;
    UINT32 CheckerSize = 40;
    
    for (i = 0; i < Width; i += CheckerSize) {
        for (j = CheckerStart; j < Height; j += CheckerSize) {
            if (((i / CheckerSize) + (j / CheckerSize)) % 2 == 0) {
                Color.Red = 255; Color.Green = 255; Color.Blue = 255;
            } else {
                Color.Red = 100; Color.Green = 100; Color.Blue = 100;
            }
            DrawRect(i, j, CheckerSize, CheckerSize, &Color);
        }
    }
    
    // Draw a border around the entire screen
    Color.Red = 255; Color.Green = 0; Color.Blue = 0;
    UINT32 BorderWidth = 5;
    DrawRect(0, 0, Width, BorderWidth, &Color); // Top
    DrawRect(0, Height - BorderWidth, Width, BorderWidth, &Color); // Bottom
    DrawRect(0, 0, BorderWidth, Height, &Color); // Left
    DrawRect(Width - BorderWidth, 0, BorderWidth, Height, &Color); // Right
}

VOID Test3D(){
    EFI_INPUT_KEY Key;

    // --- Data Definitions ---
    UINT64 bin_vertex_shader[]   = { 0x80000916, 0x0, 0xC5010901, 0x8, 0x80010906, 0x0, 0x907, 0x0 };
    UINT64 bin_fragment_shader[] = { 0xFF000A0900, 0x0, 0xB0900, 0x0, 0xC0900, 0x0, 0x801000408, 0x12C, 0x64000C0800, 0x0, 0xA0800, 0x0, 0x907, 0x0 };

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

    // --- Static Asset Transfer ---
    mGOP3D->GpuSetMode(mGOP3D, 1); 

    VRAMADDR hVBO, hIBO, hVS, hFS;
    VRAMADDR hMVP1 = 0, hMVP2 = 0; 

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, cube_vertices, sizeof(cube_vertices), &hVBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  cube_edges,    sizeof(cube_edges),    &hIBO);
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

        Mat4 ry, rx, scale, trans, proj;
        Mat4 model1, mvp1;
        
        Mat4_RotateY(angle, &ry);
        Mat4_RotateX(0.2f, &rx);
        Mat4_Scale(0.5f, &scale);
        Mat4_Translate(0.0f, 0.0f, 5.0f, &trans);
        Mat4_Perspective(PI/3.0f, 640.0f/480.0f, 1.0f, 10.0f, &proj);

        Mat4_Mul(&ry, &rx, &model1);
        Mat4_Mul(&scale, &model1, &model1); 
        Mat4_Mul(&trans, &model1, &model1);
        Mat4_Mul(&proj, &model1, &mvp1);

        // --- Object 2 Math ---
        Mat4 ry2, rx2, scale2, trans2, model2, mvp2;
        Mat4_RotateY(angle * 2.0f, &ry2);
        Mat4_RotateX(0.0f, &rx2);
        Mat4_Scale(0.25f, &scale2);
        Mat4_Translate(0.0f, 1.0f, 5.0f, &trans2);

        Mat4_Mul(&ry2, &rx2, &model2);
        Mat4_Mul(&scale2, &model2, &model2);
        Mat4_Mul(&trans2, &model2, &model2);
        Mat4_Mul(&proj, &model2, &mvp2);

        if(hMVP1 == 0 || hMVP2 == 0){
            mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &mvp1, sizeof(Mat4), &hMVP1);
            mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &mvp2, sizeof(Mat4), &hMVP2);
        }
        else{
            mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &mvp1, sizeof(Mat4), &hMVP1);
            mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &mvp2, sizeof(Mat4), &hMVP2);
        }
        
        // --- RENDER ---
        mGOP3D->GpuCmdBegin(mGOP3D);
        mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000); 
        
        mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
        mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
        mGOP3D->GpuBindVBO(mGOP3D, hVBO, 8);
        mGOP3D->GpuBindIBO(mGOP3D, hIBO, 13);

        mGOP3D->GpuBindUBO(mGOP3D, hMVP1, sizeof(Mat4)); 
        mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLines, IndexCount); 

        mGOP3D->GpuBindUBO(mGOP3D, hMVP2, sizeof(Mat4)); 
        mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLines, IndexCount); 
        
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
    
    // Print GOP information
    Print(L"GOP Located Successfully!\n");
    Print(L"Current Mode: %d\n", mGraphicsOutput->Mode->Mode);
    Print(L"Resolution: %dx%d\n", 
          mGraphicsOutput->Mode->Info->HorizontalResolution,
          mGraphicsOutput->Mode->Info->VerticalResolution);
    Print(L"Pixel Format: %d\n", mGraphicsOutput->Mode->Info->PixelFormat);
    Print(L"\nPress any key to draw test pattern.\n");
    
    WAIT_FOR_KEYPRESS()

    DrawTestPattern();

    WAIT_FOR_KEYPRESS()
    
    Print(L"Test pattern complete!\n");
    
    WAIT_FOR_KEYPRESS()

    Print(L"Test 3D capabilities\n\nPress any key to test 3D\n");
    
    WAIT_FOR_KEYPRESS()

    Test3D();

    DEBUG((EFI_D_INFO, "Test end\n"));
    return EFI_SUCCESS;
}


/*==========================================================================*/
/*                               MAIN APP ENTRY                             */
/*==========================================================================*/


EFI_STATUS EFIAPI DemoAppEntry(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable) {
        
    EFI_STATUS Status;

    Status = Test();

    ASSERT_EFI_ERROR(Status);
    
    return Status;
}

EFI_STATUS EFIAPI DemoAppEntryUnload (IN  EFI_HANDLE  ImageHandle) {
    return EFI_SUCCESS;
}