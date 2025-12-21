#include "oprom.h"
#include "memAlloc.h"
#include <Library/BaseMemoryLib.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Pci.h>
#include <Library/UefiBootServicesTableLib.h> // gBS
#include <Library/DevicePathLib.h>

/**
  Check if this device is supported.
  Yoinked straight out of QemuVideoDxe/Driver

  @param  This                   The driver binding protocol.
  @param  Controller             The controller handle to check.
  @param  RemainingDevicePath    The remaining device path.

  @retval EFI_SUCCESS            The bus supports this controller.
  @retval EFI_UNSUPPORTED        This device isn't supported.

 **/
EFI_STATUS
  EFIAPI
GpuVideoControllerDriverSupported (
    IN EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                   Controller,
    IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
    )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  PCI_TYPE00           Pci;

  //
  // Open the PCI I/O Protocol
  //
  Status = gBS->OpenProtocol (
      Controller,
      &gEfiPciIoProtocolGuid,
      (VOID **)&PciIo,
      This->DriverBindingHandle,
      Controller,
      EFI_OPEN_PROTOCOL_BY_DRIVER
      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Read the PCI Configuration Header from the PCI Device
  //
  Status = PciIo->Pci.Read (
      PciIo,
      EfiPciIoWidthUint32,
      0,
      sizeof (Pci) / sizeof (UINT32),
      &Pci
      );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = EFI_UNSUPPORTED;

  DEBUG ((DEBUG_INFO, "GpuVideo: Class: %x Vendor %x Device %x\n", Pci.Hdr.ClassCode[1], Pci.Hdr.VendorId, Pci.Hdr.DeviceId));
  if (Pci.Hdr.VendorId == 0x6969 && Pci.Hdr.DeviceId == 0x2137) {
    Status = EFI_SUCCESS;
  }

Done:
  //
  // Close the PCI I/O Protocol
  //
  gBS->CloseProtocol (
      Controller,
      &gEfiPciIoProtocolGuid,
      This->DriverBindingHandle,
      Controller
      );

  return Status;
}



EFI_STATUS EFIAPI GpuVideoControllerDriverStart (
    IN EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                   Controller,
    IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
    ) {
  EFI_STATUS Status;
  MY_GPU_PRIVATE_DATA *Private;
  EFI_TPL OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  DEBUG ((EFI_D_INFO, "UEFI GPU Driver start\n"));

  Private = AllocateZeroPool(sizeof(MY_GPU_PRIVATE_DATA));
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  Private->Handle = NULL;

  // Hardcode FB size
  Private->MainFrameBufferWidth   = 640;
  Private->MainFrameBufferHeight  = 480;

  // Open PCI protocol
  Status = gBS->OpenProtocol (
      Controller,
      &gEfiPciIoProtocolGuid,
      (VOID **)&Private->PciIo,
      This->DriverBindingHandle,
      Controller,
      EFI_OPEN_PROTOCOL_BY_DRIVER
      );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to open PCI protocol\n"));
    return Status;
  }

  // Read supported attribiutes
  UINT64                    SupportedAttrs;
  Status = Private->PciIo->Attributes (
      Private->PciIo,
      EfiPciIoAttributeOperationSupported,
      0,
      &SupportedAttrs
      );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to check attrs\n"));
    return Status;
  }
  DEBUG ((EFI_D_INFO, "Supported attrs: %x\n", SupportedAttrs));

  //
  // Set new PCI attributes
  //
  Status = Private->PciIo->Attributes (
      Private->PciIo,
      EfiPciIoAttributeOperationEnable,
      EFI_PCI_DEVICE_ENABLE | EFI_PCI_IO_ATTRIBUTE_BUS_MASTER | SupportedAttrs,
      NULL
      );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to enable device\n"));
    return Status;
  }

  // Read BAR atribs
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Resources;

  // BAR #0 = MMIO
  Status = Private->PciIo->GetBarAttributes (Private->PciIo, 0, NULL, (VOID **)&Resources);
  DEBUG (( EFI_D_INFO, "MMIO is at %x and is %x long\n", Resources->AddrRangeMin, Resources->AddrLen));

  // BAR #1 = VRAM
  Status = Private->PciIo->GetBarAttributes (Private->PciIo, 1, NULL, (VOID **)&Resources);
  DEBUG (( EFI_D_INFO, "VRAM is at %x and is %x long\n", Resources->AddrRangeMin, Resources->AddrLen));

  // Initalize memory allocator
  InitMemoryAllocator(Private->PciIo, Resources->AddrLen, Resources->AddrRangeMin);

  // Allocate framebuffer
  AllocateMemAt(Private->MainFrameBufferHeight * Private->MainFrameBufferWidth * sizeof(UINT32),0x000000);

  DebugPrintAllocatorStats();
  DebugDumpMemoryMap();


  Private->VRAMBaseAddr = Resources->AddrRangeMin;
  FreePool(Resources);

  //
  // Get ParentDevicePath
  //
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  Status = gBS->HandleProtocol (
      Controller,
      &gEfiDevicePathProtocolGuid,
      (VOID **)&ParentDevicePath
      );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to get ParentDevicePath\n"));
    return Status;
  }

  // what even is this ACPI & why is it required? installing the proto fails otherwise
  ACPI_ADR_DEVICE_PATH      AcpiDeviceNode;
  ZeroMem (&AcpiDeviceNode, sizeof (ACPI_ADR_DEVICE_PATH));
  AcpiDeviceNode.Header.Type    = ACPI_DEVICE_PATH;
  AcpiDeviceNode.Header.SubType = ACPI_ADR_DP;
  AcpiDeviceNode.ADR            = ACPI_DISPLAY_ADR (1, 0, 0, 1, 0, ACPI_ADR_DISPLAY_TYPE_VGA, 0, 0);
  SetDevicePathNodeLength (&AcpiDeviceNode.Header, sizeof (ACPI_ADR_DEVICE_PATH));
  // what even is this
  Private->GopDevicePath = AppendDevicePathNode (ParentDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&AcpiDeviceNode);

  if (Private->GopDevicePath == NULL) {
    DEBUG ((EFI_D_ERROR, "Failed to AppendDevice\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Install PathProtocol
  Status = gBS->InstallMultipleProtocolInterfaces (
      &Private->Handle,
      &gEfiDevicePathProtocolGuid,
      Private->GopDevicePath,
      NULL
      );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to install PathProtocol\n"));
    return Status;
  }

  // Setup GOP protocol
  Status = GopSetup(Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to GopSetup\n"));
    return Status;
  }

  // Setup GOP3D protocol
  Status = Gop3DSetup(Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Gop3DSetup\n"));
    return Status;
  }

  // Install the GOP protocol
  Status = gBS->InstallMultipleProtocolInterfaces(
      &Private->Handle,
      &gEfiGraphicsOutputProtocolGuid,
      &Private->Gop,
      NULL
      );

  DEBUG ((EFI_D_INFO, "Installed GOP protocol\n"));
  if (EFI_ERROR(Status)) {
    FreePool(Private->Gop.Mode);
    FreePool(Private);
    DEBUG ((EFI_D_INFO, "Failed to install GOP protocol\n"));
  }

  // Install the GOP3D protocol
  Status = gBS->InstallMultipleProtocolInterfaces(
      &Private->Handle,
      &gGop3dProtocolGuid,
      &Private->Gop3dProtocol,
      NULL
      );
  DEBUG ((EFI_D_INFO,  "Installed GOP3D protocol\n"));
  if (EFI_ERROR(Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to install GOP3D protocol\n"));
    // TODO: cleanup
  }

  //
  // Reference parent handle from child handle.
  //
  // i assume, some refcounting shit?
  // this succeeds but then machine hangs - not just this driver
  DEBUG ((EFI_D_INFO, "Opening child PCI IO protocol\n"));
  EFI_PCI_IO_PROTOCOL       *ChildPciIo;
  Status = gBS->OpenProtocol (
      Controller,
      &gEfiPciIoProtocolGuid,
      (VOID **)&ChildPciIo,
      This->DriverBindingHandle,
      Private->Handle,
      EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
      );
    
  DEBUG ((EFI_D_INFO, "done1, status=%d\n", Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to reference parent from child\n"));
    return Status;
  }

  gBS->RestoreTPL(OldTpl); 


  // TODO DELTE THIS LATER 
  // VRAM alloc test
  DEBUG ((EFI_D_INFO, "------------- VRAM ALLOCATION TEST ------------- \n\n"));

  VRAMADDR ptr1, ptr2, ptr3;
  
  DEBUG ((EFI_D_INFO, "Allocating mem for ptr 1\n"));
  ptr1 = AllocateMem(200*sizeof(UINT32));
  DEBUG ((EFI_D_INFO, "AllocateMem returned address %x\n\n", ptr1));

  // DebugPrintAllocatorStats();
  // DebugDumpMemoryMap();

  DEBUG ((EFI_D_INFO, "Allocating mem for ptr 2\n"));
  ptr2 = AllocateMem(200*sizeof(UINT32));
  DEBUG ((EFI_D_INFO, "AllocateMem returned address %x\n\n", ptr2));



  // DebugPrintAllocatorStats();
  // DebugDumpMemoryMap();

  DEBUG ((EFI_D_INFO, "Allocating mem for ptr 3\n"));
  ptr3 = AllocateMem(200*sizeof(UINT32));
  DEBUG ((EFI_D_INFO, "AllocateMem returned address %x\n\n", ptr3));

  // DebugPrintAllocatorStats();
  // DebugDumpMemoryMap();

  DEBUG ((EFI_D_INFO, "Freeing ptr2\n"));

  FreeMem(ptr2);

  DebugPrintAllocatorStats();
  DebugDumpMemoryMap();

  DEBUG ((EFI_D_INFO, "Allocating ptr2 again\n"));

  ptr2 = AllocateMem(200*sizeof(UINT32));


  DebugPrintAllocatorStats();
  DebugDumpMemoryMap();


  DEBUG ((EFI_D_INFO, "Driver installation done, status=%d\n", Status));
  return Status;
}

EFI_STATUS EFIAPI GpuVideoControllerDriverStop (
    IN EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                   Controller,
    IN UINTN                        NumberOfChildren,
    IN EFI_HANDLE                   *ChildHandleBuffer
    ) {
  DEBUG ((EFI_D_INFO, "Called STOP\n"));
  // TODO implement
  return EFI_SUCCESS;
}

EFI_DRIVER_BINDING_PROTOCOL gGpuVideoDriverBinding = {
  GpuVideoControllerDriverSupported,
  GpuVideoControllerDriverStart,
  GpuVideoControllerDriverStop,
  0x10, // version
  NULL,
  NULL
};

EFI_STATUS EFIAPI OptionRomEntry(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
    ) {
  EFI_STATUS Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
      ImageHandle,
      SystemTable,
      &gGpuVideoDriverBinding,
      ImageHandle,
      NULL, // name1, optional
      NULL  // name2, optional
      );

  ASSERT_EFI_ERROR (Status);
  
  return Status;
}