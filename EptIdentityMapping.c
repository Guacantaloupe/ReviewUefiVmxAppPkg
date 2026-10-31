/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/Ept.c, lines 177-191; function HvKnownAddressesFit. */
STATIC
BOOLEAN
HvKnownAddressesFit (
  IN CONST HV_VMX_CONTEXT  *Context
  )
{
  UINT64  Highest;

  Highest = Context->ImageBase + Context->ImageSize;
  if ((Context->GuestStackRegion + EFI_PAGES_TO_SIZE (HV_GUEST_STACK_PAGES)) > Highest) {
    Highest = Context->GuestStackRegion + EFI_PAGES_TO_SIZE (HV_GUEST_STACK_PAGES);
  }

  return (BOOLEAN)(Highest <= Context->EptMappedBytes);
}

/* Original: Application/VmxBringUp/Ept.c, lines 193-326; function HvEptInitialize. */
EFI_STATUS
HvEptInitialize (
  IN EFI_SYSTEM_TABLE    *SystemTable,
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  EFI_STATUS             Status;
  EFI_PHYSICAL_ADDRESS   Region;
  EFI_MEMORY_DESCRIPTOR  *MemoryMap;
  UINTN                  MemoryMapSize;
  UINTN                  DescriptorSize;
  UINTN                  DescriptorOffset;
  UINT64                 *Pml4;
  UINT64                 *Pdpt;
  UINT64                 *Pd;
  UINT64                 PdAddress;
  UINT64                 PhysicalAddress;
  UINT64                 LeafMemoryType;
  UINTN                  PdptIndex;
  UINTN                  PdIndex;
  UINT32                 MaxExtendedLeaf;
  UINT32                 Eax;
  UINT32                 Ebx;
  UINT32                 Ecx;
  UINT32                 Edx;
  UINT8                  EffectivePhysicalBits;

  if ((SystemTable == NULL) || (Context == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (((Context->EptVpidCapabilities & VMX_EPT_PAGE_WALK_4_CAP) == 0) ||
      ((Context->EptVpidCapabilities & VMX_EPTP_WB_CAP) == 0) ||
      ((Context->EptVpidCapabilities & VMX_EPT_2MB_PAGE_CAP) == 0)) {
    HvEptOutput (SystemTable, L"ERROR: Required EPT capabilities are unavailable.\r\n");
    return EFI_UNSUPPORTED;
  }

  AsmCpuid (CPUID_EXTENDED_MAX, &MaxExtendedLeaf, &Ebx, &Ecx, &Edx);
  Context->PhysicalAddressBits = 36U;
  if (MaxExtendedLeaf >= CPUID_ADDRESS_SIZE) {
    AsmCpuid (CPUID_ADDRESS_SIZE, &Eax, &Ebx, &Ecx, &Edx);
    Context->PhysicalAddressBits = (UINT8)(Eax & 0xFFU);
  }

  EffectivePhysicalBits = Context->PhysicalAddressBits;
  if (EffectivePhysicalBits > 39U) {
    EffectivePhysicalBits = 39U;
  }
  if (EffectivePhysicalBits < 30U) {
    return EFI_UNSUPPORTED;
  }

  Context->EptMappedBytes = LShiftU64 (1ULL, EffectivePhysicalBits);
  if (Context->EptMappedBytes > HV_EPT_IDENTITY_BYTES) {
    Context->EptMappedBytes = HV_EPT_IDENTITY_BYTES;
  }

  Context->EptPdPageCount = (UINTN)RShiftU64 (Context->EptMappedBytes, 30);
  Context->EptPageCount   = 2U + Context->EptPdPageCount;
  if ((Context->EptPdPageCount == 0) ||
      (Context->EptPdPageCount > HV_EPT_PD_PAGES_MAX)) {
    return EFI_UNSUPPORTED;
  }

  if (!HvKnownAddressesFit (Context)) {
    HvEptOutput (SystemTable, L"ERROR: Hypervisor image/stack lies above the 512-GiB prototype EPT limit.\r\n");
    return EFI_UNSUPPORTED;
  }

  MemoryMap = NULL;
  Status = HvGetMemoryMapCopy (
             SystemTable,
             &MemoryMap,
             &MemoryMapSize,
             &DescriptorSize
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Region = Context->EptMappedBytes - 1U;
  Status = SystemTable->BootServices->AllocatePages (
                                      AllocateMaxAddress,
                                      EfiRuntimeServicesData,
                                      Context->EptPageCount,
                                      &Region
                                      );
  if (EFI_ERROR (Status)) {
    SystemTable->BootServices->FreePool (MemoryMap);
    return Status;
  }

  Context->EptRegion = Region;
  ZeroMem ((VOID *)(UINTN)Region, EFI_PAGES_TO_SIZE (Context->EptPageCount));
  Pml4 = (UINT64 *)(UINTN)Region;
  Pdpt = (UINT64 *)(UINTN)(Region + HV_PAGE_SIZE);
  Pml4[0] = ((Region + HV_PAGE_SIZE) & EPT_PHYSICAL_ADDRESS_MASK) | EPT_ENTRY_RWX;

  DescriptorOffset = 0;
  for (PdptIndex = 0; PdptIndex < Context->EptPdPageCount; ++PdptIndex) {
    PdAddress = Region + EFI_PAGES_TO_SIZE (2U + PdptIndex);
    Pdpt[PdptIndex] = (PdAddress & EPT_PHYSICAL_ADDRESS_MASK) | EPT_ENTRY_RWX;
    Pd = (UINT64 *)(UINTN)PdAddress;

    for (PdIndex = 0; PdIndex < 512U; ++PdIndex) {
      PhysicalAddress = LShiftU64 ((UINT64)PdptIndex, 30) |
                        LShiftU64 ((UINT64)PdIndex, 21);
      LeafMemoryType = HvLeafCanUseWriteBack (
                         MemoryMap,
                         MemoryMapSize,
                         DescriptorSize,
                         &DescriptorOffset,
                         PhysicalAddress
                         ) ? EPT_ENTRY_MEMORY_TYPE_WB : EPT_ENTRY_MEMORY_TYPE_UC;

      Pd[PdIndex] = (PhysicalAddress & EPT_PHYSICAL_ADDRESS_MASK) |
                    EPT_ENTRY_RWX |
                    LeafMemoryType |
                    EPT_ENTRY_LARGE_PAGE;
    }
  }

  SystemTable->BootServices->FreePool (MemoryMap);
  Context->EptPointer = (Region & EPT_PHYSICAL_ADDRESS_MASK) |
                        EPT_POINTER_MEMORY_TYPE_WB |
                        EPT_POINTER_PAGE_WALK_LENGTH_4;

  if (!Context->IsApplicationProcessor) {
    HvEptOutputHex64 (SystemTable, L"EPT pointer              = ", Context->EptPointer);
    HvEptOutputHex64 (SystemTable, L"EPT identity-map bytes   = ", Context->EptMappedBytes);
  }
  return EFI_SUCCESS;
}

/* Original: Application/VmxBringUp/Ept.c, lines 328-346; function HvEptDestroy. */
VOID
HvEptDestroy (
  IN EFI_SYSTEM_TABLE    *SystemTable,
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  if ((SystemTable == NULL) || (Context == NULL)) {
    return;
  }

  if ((Context->EptRegion != 0) && (Context->EptPageCount != 0)) {
    SystemTable->BootServices->FreePages (Context->EptRegion, Context->EptPageCount);
    Context->EptRegion      = 0;
    Context->EptPageCount   = 0;
    Context->EptPdPageCount = 0;
    Context->EptPointer     = 0;
    Context->EptEnabled     = FALSE;
  }
}
