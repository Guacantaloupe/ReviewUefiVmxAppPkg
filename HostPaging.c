/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/Resident.c, lines 243-339; function HvBuildHostPaging. */
STATIC
EFI_STATUS
HvBuildHostPaging (
  IN EFI_SYSTEM_TABLE    *SystemTable,
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  Region;
  UINT64                *Pml4;
  UINT64                *Pdpt;
  UINT64                *Pd;
  UINT64                PdAddress;
  UINT64                PhysicalAddress;
  UINT64                HighestEnd;
  UINTN                 RequiredPdPages;
  UINTN                 PdptIndex;
  UINTN                 PdIndex;

  HighestEnd = 0;
  if (!HvAccumulateRangeEnd (Context->ImageBase, Context->ImageSize, &HighestEnd) ||
      !HvAccumulateRangeEnd ((UINT64)(UINTN)Context, sizeof (*Context), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->VmxonRegion, HV_PAGE_SIZE, &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->VmcsRegion, HV_PAGE_SIZE, &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->NestedVmcsRegion, HV_PAGE_SIZE, &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->NestedStateRegion, EFI_PAGES_TO_SIZE (HV_NESTED_STATE_PAGES), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->NestedAuxRegion, EFI_PAGES_TO_SIZE (HV_NESTED_AUX_PAGES), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->HostStackRegion, EFI_PAGES_TO_SIZE (HV_HOST_STACK_PAGES), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->MsrBitmapRegion, EFI_PAGES_TO_SIZE (HV_MSR_BITMAP_PAGES), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->HostGdtRegion, EFI_PAGES_TO_SIZE (HV_HOST_GDT_PAGES), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->HostIdtRegion, EFI_PAGES_TO_SIZE (HV_HOST_IDT_PAGES), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->HostTssRegion, EFI_PAGES_TO_SIZE (HV_HOST_TSS_PAGES), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->GuestGdtRegion, EFI_PAGES_TO_SIZE (HV_GUEST_GDT_PAGES), &HighestEnd) ||
      !HvAccumulateRangeEnd (Context->GuestTssRegion, EFI_PAGES_TO_SIZE (HV_GUEST_TSS_PAGES), &HighestEnd)) {
    return EFI_COMPROMISED_DATA;
  }

  // Nested VMX reads L1-owned VMXON/VMCS/EPT structures through the private
  // VMX-root CR3. Cover the same physical span as the identity EPT so those
  // guest-physical pointers remain directly addressable in root operation.
  if (Context->NestedVmxEnabled &&
      (Context->EptMappedBytes > HighestEnd)) {
    HighestEnd = Context->EptMappedBytes;
  }

  RequiredPdPages = (UINTN)((HighestEnd + HV_HOST_GIGABYTE_BYTES - 1ULL) /
                            HV_HOST_GIGABYTE_BYTES);
  // Keep the diagnostic-return path usable on firmware that places console or
  // Boot Services code elsewhere below 4 GiB. Four PD pages map the full low
  // 4-GiB physical range with 2-MiB leaves and cost only 16 KiB.
  if (RequiredPdPages < 4U) {
    RequiredPdPages = 4U;
  }

  if (RequiredPdPages > HV_HOST_PD_PAGES_MAX) {
    return EFI_UNSUPPORTED;
  }

  Context->HostPagingPageCount = HV_HOST_PML4_PAGES +
                                 HV_HOST_PDPT_PAGES +
                                 RequiredPdPages;
  Region = 0;
  Status = HvAllocateRuntimePagesBelow512G (
             SystemTable,
             Context->HostPagingPageCount,
             &Region
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Context->HostPagingRegion = Region;
  ZeroMem ((VOID *)(UINTN)Region, EFI_PAGES_TO_SIZE (Context->HostPagingPageCount));

  Pml4 = (UINT64 *)(UINTN)Region;
  Pdpt = (UINT64 *)(UINTN)(Region + HV_PAGE_SIZE);
  Pml4[0] = ((Region + HV_PAGE_SIZE) & X64_PAGE_ADDRESS_MASK) |
            X64_PTE_PRESENT | X64_PTE_READ_WRITE;

  for (PdptIndex = 0; PdptIndex < RequiredPdPages; ++PdptIndex) {
    PdAddress = Region + EFI_PAGES_TO_SIZE (2U + PdptIndex);
    Pdpt[PdptIndex] = (PdAddress & X64_PAGE_ADDRESS_MASK) |
                      X64_PTE_PRESENT | X64_PTE_READ_WRITE;
    Pd = (UINT64 *)(UINTN)PdAddress;

    for (PdIndex = 0; PdIndex < 512U; ++PdIndex) {
      PhysicalAddress = LShiftU64 ((UINT64)PdptIndex, 30) |
                        LShiftU64 ((UINT64)PdIndex, 21);
      Pd[PdIndex] = (PhysicalAddress & X64_PAGE_ADDRESS_MASK) |
                    X64_PTE_PRESENT | X64_PTE_READ_WRITE |
                    X64_PTE_LARGE_PAGE;
    }
  }

  Context->HostCr3 = Region;
  return EFI_SUCCESS;
}
