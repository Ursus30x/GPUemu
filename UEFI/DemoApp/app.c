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

    // Draw the test pattern
    DrawTestPattern();

    WAIT_FOR_KEYPRESS()
    
    Print(L"Test pattern complete!\n");
    
    WAIT_FOR_KEYPRESS()

    Print(L"Test 3D capabilities\n\nPress any key to test 3D\n");
    
    WAIT_FOR_KEYPRESS()

    mGOP3D->SetGpuMode(mGOP3D,1);

    WAIT_FOR_KEYPRESS()

    Vec3 cube_vertices[] = {
        { -1, -1, -1, 0xFFFF0000 },
        {  1, -1, -1, 0xFF00FF00 },
        {  1,  1, -1, 0xFF0000FF },
        { -1,  1, -1, 0xFFFFFF00 }, 
        { -1, -1,  1, 0xFFFF00FF },
        {  1, -1,  1, 0xFF00FFFF },
        {  1,  1,  1, 0xFFFFFFFF }, 
        { -1,  1,  1, 0xFF808080 }  
    };
    Edge cube_edges[] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7},{5,3}
    };

    mGOP3D->TransferDataBuffer(mGOP3D,VERTEX_BUFFER,cube_vertices,sizeof(cube_vertices));
    mGOP3D->TransferDataBuffer(mGOP3D,EDGE_BUFFER,cube_edges,sizeof(cube_edges));

    WAIT_FOR_KEYPRESS()

    Vec3 piramid_vertices[] = {
        {  1,  0, 1, 0xFF00FF00 }, 
        {  1,  0,-1, 0xFF0000FF }, 
        { -1,  0,-1, 0xFFFF00FF },
        { -1,  0, 1, 0xFFFFFFFF },
        {  0, -2.5, 0, 0xFFFF0000 }
    };

    Edge piramid_edges[] = {
        {0,1},{1,2},{2,3},{3,0},
        {0,4},{1,4},{2,4},{3,4}
    };

    mGOP3D->TransferDataBuffer(mGOP3D,VERTEX_BUFFER,piramid_vertices,sizeof(piramid_vertices));
    mGOP3D->TransferDataBuffer(mGOP3D,EDGE_BUFFER,piramid_edges,sizeof(piramid_edges));


    WAIT_FOR_KEYPRESS()

    Vec3 star_vertices[] = {
        {  0.000,  2.000,  0.000, 0xFFFFFF00 },  // 0: top point
        {  0.500,  0.500,  0.000, 0xFFFFFFFF },  // 1
        {  2.000,  0.500,  0.000, 0xFFFF0000 },  // 2: right point
        {  0.700, -0.300,  0.000, 0xFFFFFFFF },  // 3
        {  1.200, -1.500,  0.000, 0xFF00FF00 },  // 4: bottom-right
        {  0.000, -0.700,  0.000, 0xFFFFFFFF },  // 5: center
        { -1.200, -1.500,  0.000, 0xFF0000FF },  // 6: bottom-left
        { -0.700, -0.300,  0.000, 0xFFFFFFFF },  // 7
        { -2.000,  0.500,  0.000, 0xFFFF00FF },  // 8: left point
        { -0.500,  0.500,  0.000, 0xFFFFFFFF }   // 9
    };

    Edge star_edges[] = {
        {0,1}, {1,2}, {2,3}, {3,4}, {4,5},
        {5,6}, {6,7}, {7,8}, {8,9}, {9,0}
    };

    mGOP3D->TransferDataBuffer(mGOP3D,VERTEX_BUFFER,star_vertices,sizeof(star_vertices));
    mGOP3D->TransferDataBuffer(mGOP3D,EDGE_BUFFER,star_edges,sizeof(star_edges));

    WAIT_FOR_KEYPRESS()

    mGOP3D->SetGpuMode(mGOP3D,0);

    DEBUG((EFI_D_INFO, "TestGop end\n"));
    return EFI_SUCCESS;
}

/*==========================================================================*/
/*                                  3D TEST                                 */
/*==========================================================================*/

// STATIC EFI_PCI_IO_PROTOCOL *PciIo = NULL;

// EFI_STATUS EFIAPI Test3D(){
//     EFI_STATUS Status;

//     Status = gBS->OpenProtocol(
//         DeviceHandle,                           // Handle to open protocol on
//         &gEfiPciIoProtocolGuid,                // Protocol GUID
//         (VOID **)PciIo,                        // Interface pointer
//         AgentHandle,                            // Agent handle (your driver/app)
//         NULL,                                   // Controller handle (NULL for apps)
//         EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL   // Open mode
//     );
// }


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