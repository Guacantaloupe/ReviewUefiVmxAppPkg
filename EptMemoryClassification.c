/*
 * CODE REVIEW EXCERPT — not a standalone translation unit.
 * Extracted verbatim from the supplied UefiVmxAppPkg archive.
 * Original project uses EDK II types, macros, structures, and helpers
 * not bundled in this review-only package.
 * SPDX-License-Identifier: MIT
 */

/* BEGIN original: Application/VmxBringUp/Ept.c, lines 57-81 */
STATIC
BOOLEAN
HvEptRamType (
  IN UINT32  Type
  )
{
  switch ((EFI_MEMORY_TYPE)Type) {
    // The GetMemoryMap hook re-labels the loaded hypervisor image as reserved
    // while preserving its WB attribute. Treat such RAM as WB, but only when
    // the descriptor itself advertises EFI_MEMORY_WB.
    case EfiReservedMemoryType:
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:
    case EfiRuntimeServicesCode:
    case EfiRuntimeServicesData:
    case EfiConventionalMemory:
    case EfiACPIReclaimMemory:
    case EfiACPIMemoryNVS:
      return TRUE;
    default:
      return FALSE;
  }
}
/* END original: Application/VmxBringUp/Ept.c */

/* BEGIN original: Application/VmxBringUp/Ept.c, lines 83-136 */
STATIC
EFI_STATUS
HvGetMemoryMapCopy (
  IN EFI_SYSTEM_TABLE        *SystemTable,
  OUT EFI_MEMORY_DESCRIPTOR  **MemoryMap,
  OUT UINTN                  *MemoryMapSize,
  OUT UINTN                  *DescriptorSize
  )
{
  EFI_STATUS  Status;
  UINTN       MapKey;
  UINT32      DescriptorVersion;
  UINTN       Size;
  VOID        *Buffer;

  *MemoryMap      = NULL;
  *MemoryMapSize  = 0;
  *DescriptorSize = 0;
  Size = 0;

  Status = SystemTable->BootServices->GetMemoryMap (
                                      &Size,
                                      NULL,
                                      &MapKey,
                                      DescriptorSize,
                                      &DescriptorVersion
                                      );
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  Size += (*DescriptorSize * 8U);
  Buffer = NULL;
  Status = SystemTable->BootServices->AllocatePool (EfiLoaderData, Size, &Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SystemTable->BootServices->GetMemoryMap (
                                      &Size,
                                      Buffer,
                                      &MapKey,
                                      DescriptorSize,
                                      &DescriptorVersion
                                      );
  if (EFI_ERROR (Status)) {
    SystemTable->BootServices->FreePool (Buffer);
    return Status;
  }

  *MemoryMap     = (EFI_MEMORY_DESCRIPTOR *)Buffer;
  *MemoryMapSize = Size;
  return EFI_SUCCESS;
}
/* END original: Application/VmxBringUp/Ept.c */

/* BEGIN original: Application/VmxBringUp/Ept.c, lines 138-175 */
STATIC
BOOLEAN
HvLeafCanUseWriteBack (
  IN EFI_MEMORY_DESCRIPTOR  *MemoryMap,
  IN UINTN                  MemoryMapSize,
  IN UINTN                  DescriptorSize,
  IN OUT UINTN              *DescriptorOffset,
  IN UINT64                 LeafStart
  )
{
  EFI_MEMORY_DESCRIPTOR  *Descriptor;
  UINT64                 DescriptorStart;
  UINT64                 DescriptorEnd;
  UINT64                 LeafEnd;

  LeafEnd = LeafStart + HV_2MB_SIZE;
  while (*DescriptorOffset + DescriptorSize <= MemoryMapSize) {
    Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + *DescriptorOffset);
    DescriptorStart = Descriptor->PhysicalStart;
    DescriptorEnd   = DescriptorStart + EFI_PAGES_TO_SIZE (Descriptor->NumberOfPages);

    if (DescriptorEnd <= LeafStart) {
      *DescriptorOffset += DescriptorSize;
      continue;
    }

    if ((DescriptorStart <= LeafStart) &&
        (DescriptorEnd >= LeafEnd) &&
        HvEptRamType (Descriptor->Type) &&
        ((Descriptor->Attribute & EFI_MEMORY_WB) != 0)) {
      return TRUE;
    }

    return FALSE;
  }

  return FALSE;
}
/* END original: Application/VmxBringUp/Ept.c */

