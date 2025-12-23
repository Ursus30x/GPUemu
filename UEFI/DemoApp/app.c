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
#include <Library/UefiBootServicesTableLib.h> // gBS
#include <Library/DevicePathLib.h>

#define WAIT_FOR_KEYPRESS() Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL); \
    if (!EFI_ERROR(Status)) { \
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key); \
    }

typedef struct { double x, y, z; UINT32 rgba; } Vec3;
typedef struct { UINT32 a, b; } Edge;

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGraphicsOutput = NULL;
STATIC GOP_3D_PROTOCOL              *mGOP3D          = NULL;

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

EFI_STATUS EFIAPI TestGop() {
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    // --- 1. Data Definitions ---
    // Assuming Vec3 structure matches your hardware expectation (floats + color)
    typedef struct {
        float x, y, z;
        UINT32 color;
    } Vec3;

    // Cube Data
    Vec3 cube_vertices[] = {
        { -1.0f, -1.0f, -1.0f, 0xFFFF0000 },
        {  1.0f, -1.0f, -1.0f, 0xFF00FF00 },
        {  1.0f,  1.0f, -1.0f, 0xFF0000FF },
        { -1.0f,  1.0f, -1.0f, 0xFFFFFF00 }, 
        { -1.0f, -1.0f,  1.0f, 0xFFFF00FF },
        {  1.0f, -1.0f,  1.0f, 0xFF00FFFF },
        {  1.0f,  1.0f,  1.0f, 0xFFFFFFFF }, 
        { -1.0f,  1.0f,  1.0f, 0xFF808080 }  
    };
    // Note: Converted Edge struct {{0,1},...} to flat Index Array for Index Buffer
    UINT32 cube_indices[] = { 
        0,1, 1,2, 2,3, 3,0, 4,5, 5,6, 6,7, 7,4, 0,4, 1,5, 2,6, 3,7, 5,3
    };

    // Pyramid Data
    Vec3 piramid_vertices[] = {
        {  1.0f,  0.0f,  1.0f, 0xFF00FF00 }, 
        {  1.0f,  0.0f, -1.0f, 0xFF0000FF }, 
        { -1.0f,  0.0f, -1.0f, 0xFFFF00FF },
        { -1.0f,  0.0f,  1.0f, 0xFFFFFFFF },
        {  0.0f, -2.5f,  0.0f, 0xFFFF0000 }
    };
    UINT32 piramid_indices[] = {
        0,1, 1,2, 2,3, 3,0, 0,4, 1,4, 2,4, 3,4
    };

    // Star Data
    Vec3 star_vertices[] = {
        {  0.000f,  2.000f,  0.000f, 0xFFFFFF00 }, 
        {  0.500f,  0.500f,  0.000f, 0xFFFFFFFF }, 
        {  2.000f,  0.500f,  0.000f, 0xFFFF0000 }, 
        {  0.700f, -0.300f,  0.000f, 0xFFFFFFFF }, 
        {  1.200f, -1.500f,  0.000f, 0xFF00FF00 }, 
        {  0.000f, -0.700f,  0.000f, 0xFFFFFFFF }, 
        { -1.200f, -1.500f,  0.000f, 0xFF0000FF }, 
        { -0.700f, -0.300f,  0.000f, 0xFFFFFFFF }, 
        { -2.000f,  0.500f,  0.000f, 0xFFFF00FF }, 
        { -0.500f,  0.500f,  0.000f, 0xFFFFFFFF }  
    };
    UINT32 star_indices[] = {
        0,1, 1,2, 2,3, 3,4, 4,5, 5,6, 6,7, 7,8, 8,9, 9,0
    };

    // Dummy Identity Matrix for MVP (Safety)
    float identity_matrix[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };


    DEBUG((EFI_D_INFO, "TestGop start\n"));

    // --- 2. Locate Protocols ---
    Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&mGraphicsOutput);
    Status = gBS->LocateProtocol(&gGop3dProtocolGuid, NULL, (VOID **)&mGOP3D);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to locate Protocols: %r\n", Status);
        return Status;
    }
    
    Print(L"GOP Located! Mode: %dx%d\n", 
          mGraphicsOutput->Mode->Info->HorizontalResolution,
          mGraphicsOutput->Mode->Info->VerticalResolution);
    
    WAIT_FOR_KEYPRESS();
    
    DrawTestPattern(); // 2D Pattern
    Print(L"2D Test Complete. Press Key for 3D...\n");
    WAIT_FOR_KEYPRESS();


    // --- 3. 3D Setup & Asset Transfer ---
    
    Print(L"Switching to 3D Mode...\n");
    mGOP3D->GpuSetMode(mGOP3D, 1); 

    VRAMADDR hCubeVBO, hCubeIBO;
    VRAMADDR hPyrVBO,  hPyrIBO;
    VRAMADDR hStarVBO, hStarIBO;
    VRAMADDR hMVP;

    // Upload Assets to VRAM
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, cube_vertices, sizeof(cube_vertices), &hCubeVBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  cube_indices,  sizeof(cube_indices),  &hCubeIBO);

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, piramid_vertices, sizeof(piramid_vertices), &hPyrVBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  piramid_indices,  sizeof(piramid_indices),  &hPyrIBO);

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, star_vertices, sizeof(star_vertices), &hStarVBO);
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex,  star_indices,  sizeof(star_indices),  &hStarIBO);

    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, identity_matrix, sizeof(identity_matrix), &hMVP);


    // --- 4. Render Loop Simulation ---

    // === FRAME 1: Cube ===
    Print(L"Drawing Cube... (Press Key)\n");
    WAIT_FOR_KEYPRESS();

    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000); // Clear Black
    
    mGOP3D->GpuBindUBO(mGOP3D, hMVP);     // Bind Matrix
    mGOP3D->GpuBindVBO(mGOP3D, hCubeVBO); // Bind Cube Vertices
    mGOP3D->GpuBindIBO(mGOP3D, hCubeIBO); // Bind Cube Indices
    
    // Draw Lines, count = number of indices
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLines, sizeof(cube_indices)/sizeof(UINT32));
    
    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuPresent(mGOP3D); // Submit and Wait

    
    // === FRAME 2: Pyramid ===
    Print(L"Drawing Pyramid... (Press Key)\n");
    WAIT_FOR_KEYPRESS();

    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000);

    mGOP3D->GpuBindUBO(mGOP3D, hMVP);
    mGOP3D->GpuBindVBO(mGOP3D, hPyrVBO);
    mGOP3D->GpuBindIBO(mGOP3D, hPyrIBO);

    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLines, sizeof(piramid_indices)/sizeof(UINT32));

    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuPresent(mGOP3D);


    // === FRAME 3: Star ===
    Print(L"Drawing Star... (Press Key)\n");
    WAIT_FOR_KEYPRESS();

    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000);

    mGOP3D->GpuBindUBO(mGOP3D, hMVP);
    mGOP3D->GpuBindVBO(mGOP3D, hStarVBO);
    mGOP3D->GpuBindIBO(mGOP3D, hStarIBO);

    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLines, sizeof(star_indices)/sizeof(UINT32));

    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuPresent(mGOP3D);
    
    WAIT_FOR_KEYPRESS();

    // --- Cleanup ---
    mGOP3D->GpuSetMode(mGOP3D, 0); // Back to GOP 2D Mode

    DEBUG((EFI_D_INFO, "TestGop end\n"));
    return EFI_SUCCESS;
}

/*==========================================================================*/
/*                                  3D TEST                                 */
/*==========================================================================*/



/*==========================================================================*/
/*                               MAIN APP ENTRY                             */
/*==========================================================================*/


EFI_STATUS EFIAPI DemoAppEntry(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable) {
        
    EFI_STATUS Status;

    Status = TestGop();

    ASSERT_EFI_ERROR(Status);
    
    return Status;
}

EFI_STATUS EFIAPI DemoAppEntryUnload (IN  EFI_HANDLE  ImageHandle) {
    return EFI_SUCCESS;
}