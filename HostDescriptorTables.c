/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/Resident.c, lines 371-386; function HvSetIdtGate. */
STATIC
VOID
HvSetIdtGate (
  OUT HV_IDT_GATE  *Gate,
  IN UINT16        Selector,
  IN UINT64        Handler
  )
{
  Gate->OffsetLow     = (UINT16)Handler;
  Gate->Selector      = Selector;
  Gate->Ist           = 0;
  Gate->TypeAttributes = HV_IDT_GATE_PRESENT;
  Gate->OffsetMiddle  = (UINT16)(Handler >> 16);
  Gate->OffsetHigh    = (UINT32)(Handler >> 32);
  Gate->Reserved      = 0;
}

/* Original: Application/VmxBringUp/Resident.c, lines 388-524; function HvBuildHostDescriptorTables. */
STATIC
EFI_STATUS
HvBuildHostDescriptorTables (
  IN EFI_SYSTEM_TABLE    *SystemTable,
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  GdtRegion;
  EFI_PHYSICAL_ADDRESS  IdtRegion;
  EFI_PHYSICAL_ADDRESS  TssRegion;
  EFI_PHYSICAL_ADDRESS  GuestGdtRegion;
  EFI_PHYSICAL_ADDRESS  GuestTssRegion;
  IA32_DESCRIPTOR       Gdtr;
  IA32_DESCRIPTOR       Idtr;
  UINTN                 GdtBytes;
  UINTN                 TssOffset;
  UINTN                 Index;
  HV_IDT_GATE           *Idt;

  AsmReadGdtr (&Gdtr);
  AsmReadIdtr (&Idtr);
  Context->OriginalGdtrBase  = Gdtr.Base;
  Context->OriginalGdtrLimit = Gdtr.Limit;
  Context->OriginalIdtrBase  = Idtr.Base;
  Context->OriginalIdtrLimit = Idtr.Limit;
  Context->OriginalTrSelector = AsmReadTr ();
  Context->HostCsSelector = AsmReadCs () & 0xFFF8U;

  GdtBytes = (UINTN)Gdtr.Limit + 1U;
  if ((Gdtr.Base == 0) || (GdtBytes == 0) || (GdtBytes > HV_PAGE_SIZE) ||
      (Context->HostCsSelector == 0) ||
      (((UINTN)Context->HostCsSelector + 7U) >= GdtBytes)) {
    return EFI_UNSUPPORTED;
  }

  // Do not depend on the firmware having loaded a usable task register.
  // Append a private 64-bit TSS descriptor to the copied GDT instead.
  TssOffset = (GdtBytes + 7U) & ~((UINTN)7U);
  if ((TssOffset == 0) || ((TssOffset + 16U) > HV_PAGE_SIZE) ||
      (TssOffset > 0xFFF8U)) {
    return EFI_UNSUPPORTED;
  }

  Context->HostTrSelector = (UINT16)TssOffset;

  GdtRegion = 0;
  IdtRegion = 0;
  TssRegion      = 0;
  GuestGdtRegion = 0;
  GuestTssRegion = 0;
  Status = HvAllocateRuntimePagesBelow512G (SystemTable, HV_HOST_GDT_PAGES, &GdtRegion);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Context->HostGdtRegion = GdtRegion;

  Status = HvAllocateRuntimePagesBelow512G (SystemTable, HV_HOST_IDT_PAGES, &IdtRegion);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Context->HostIdtRegion = IdtRegion;

  Status = HvAllocateRuntimePagesBelow512G (SystemTable, HV_HOST_TSS_PAGES, &TssRegion);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Context->HostTssRegion = TssRegion;

  Status = HvAllocateRuntimePagesBelow512G (SystemTable, HV_GUEST_GDT_PAGES, &GuestGdtRegion);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Context->GuestGdtRegion = GuestGdtRegion;

  Status = HvAllocateRuntimePagesBelow512G (SystemTable, HV_GUEST_TSS_PAGES, &GuestTssRegion);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Context->GuestTssRegion = GuestTssRegion;

  ZeroMem ((VOID *)(UINTN)GdtRegion, HV_PAGE_SIZE);
  ZeroMem ((VOID *)(UINTN)IdtRegion, HV_PAGE_SIZE);
  ZeroMem ((VOID *)(UINTN)TssRegion, HV_PAGE_SIZE);
  ZeroMem ((VOID *)(UINTN)GuestGdtRegion, HV_PAGE_SIZE);
  ZeroMem ((VOID *)(UINTN)GuestTssRegion, HV_PAGE_SIZE);
  CopyMem ((VOID *)(UINTN)GdtRegion, (VOID *)(UINTN)Gdtr.Base, GdtBytes);
  CopyMem ((VOID *)(UINTN)GuestGdtRegion, (VOID *)(UINTN)Gdtr.Base, GdtBytes);

  // The I/O-map base at the end of the architectural 64-bit TSS means that
  // this private TSS has no I/O permission bitmap.
  WriteUnaligned16 (
    (UINT16 *)(VOID *)((UINT8 *)(UINTN)TssRegion + HV_X64_TSS_IO_MAP_OFFSET),
    (UINT16)(HV_X64_TSS_LIMIT + 1U)
    );
  WriteUnaligned16 (
    (UINT16 *)(VOID *)((UINT8 *)(UINTN)GuestTssRegion + HV_X64_TSS_IO_MAP_OFFSET),
    (UINT16)(HV_X64_TSS_LIMIT + 1U)
    );

  // The host descriptor may remain available because VM-exit loads TR from
  // VMCS host state.  The guest descriptor must be a present busy 64-bit TSS
  // (type 11), which is an explicit VM-entry requirement in IA-32e mode.
  HvWritePrivateTssDescriptor (
    (UINT8 *)(UINTN)GdtRegion,
    Context->HostTrSelector,
    TssRegion,
    HV_X64_TSS_LIMIT,
    9U
    );

  Context->GuestTrSelector = (UINT16)TssOffset;
  Context->GuestGdtLimit   = (UINT16)(TssOffset + 16U - 1U);
  HvWritePrivateTssDescriptor (
    (UINT8 *)(UINTN)GuestGdtRegion,
    Context->GuestTrSelector,
    GuestTssRegion,
    HV_X64_TSS_LIMIT,
    11U
    );

  Idt = (HV_IDT_GATE *)(UINTN)IdtRegion;
  for (Index = 0; Index < 256U; ++Index) {
    VOID  *Handler;

    Handler = (Index < 32U)
                ? HvHostExceptionStubTable[Index]
                : (VOID *)HvHostExceptionEntry;
    HvSetIdtGate (
      &Idt[Index],
      Context->HostCsSelector,
      (UINT64)(UINTN)Handler
      );
  }

  return EFI_SUCCESS;
}

/* Original: Application/VmxBringUp/Resident.c, lines 526-601; function HvRefreshHostDescriptorTablesForCurrentProcessor. */
EFI_STATUS
HvRefreshHostDescriptorTablesForCurrentProcessor (
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  IA32_DESCRIPTOR  Gdtr;
  IA32_DESCRIPTOR  Idtr;
  UINTN            GdtBytes;
  UINTN            TssOffset;
  UINTN            Index;
  HV_IDT_GATE      *Idt;

  if ((Context == NULL) || (Context->HostGdtRegion == 0) ||
      (Context->HostIdtRegion == 0) || (Context->HostTssRegion == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  AsmReadGdtr (&Gdtr);
  AsmReadIdtr (&Idtr);
  Context->OriginalGdtrBase   = Gdtr.Base;
  Context->OriginalGdtrLimit  = Gdtr.Limit;
  Context->OriginalIdtrBase   = Idtr.Base;
  Context->OriginalIdtrLimit  = Idtr.Limit;
  Context->OriginalTrSelector = AsmReadTr ();
  Context->HostCsSelector     = AsmReadCs () & 0xFFF8U;

  GdtBytes = (UINTN)Gdtr.Limit + 1U;
  TssOffset = (GdtBytes + 7U) & ~((UINTN)7U);
  if ((Gdtr.Base == 0) || (GdtBytes == 0) || (GdtBytes > HV_PAGE_SIZE) ||
      (Context->HostCsSelector == 0) ||
      (((UINTN)Context->HostCsSelector + 7U) >= GdtBytes) ||
      (TssOffset == 0) || ((TssOffset + 16U) > HV_PAGE_SIZE) ||
      (TssOffset > 0xFFF8U)) {
    return EFI_UNSUPPORTED;
  }

  ZeroMem ((VOID *)(UINTN)Context->HostGdtRegion, HV_PAGE_SIZE);
  ZeroMem ((VOID *)(UINTN)Context->HostIdtRegion, HV_PAGE_SIZE);
  ZeroMem ((VOID *)(UINTN)Context->HostTssRegion, HV_PAGE_SIZE);
  CopyMem (
    (VOID *)(UINTN)Context->HostGdtRegion,
    (CONST VOID *)(UINTN)Gdtr.Base,
    GdtBytes
    );

  WriteUnaligned16 (
    (UINT16 *)(VOID *)((UINT8 *)(UINTN)Context->HostTssRegion +
                       HV_X64_TSS_IO_MAP_OFFSET),
    (UINT16)(HV_X64_TSS_LIMIT + 1U)
    );

  Context->HostTrSelector = (UINT16)TssOffset;
  HvWritePrivateTssDescriptor (
    (UINT8 *)(UINTN)Context->HostGdtRegion,
    Context->HostTrSelector,
    Context->HostTssRegion,
    HV_X64_TSS_LIMIT,
    9U
    );

  Idt = (HV_IDT_GATE *)(UINTN)Context->HostIdtRegion;
  for (Index = 0; Index < 256U; ++Index) {
    VOID  *Handler;

    Handler = (Index < 32U)
                ? HvHostExceptionStubTable[Index]
                : (VOID *)HvHostExceptionEntry;
    HvSetIdtGate (
      &Idt[Index],
      Context->HostCsSelector,
      (UINT64)(UINTN)Handler
      );
  }

  return EFI_SUCCESS;
}
