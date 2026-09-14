#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/Gop3D.h>

EFI_STATUS
EFIAPI
TestHarnessEntry(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    Print(L"========================================\n");
    Print(L"       GPUemu Test Harness v1.0         \n");
    Print(L"========================================\n");
    Print(L"Test harness entry point initialized.\n");

    return EFI_SUCCESS;
}
