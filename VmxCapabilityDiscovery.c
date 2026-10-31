/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/IntelVmx.c, lines 273-339; function HvReadVmxCapabilities. */
STATIC
EFI_STATUS
HvReadVmxCapabilities (
  IN EFI_SYSTEM_TABLE   *SystemTable,
  IN OUT HV_VMX_CONTEXT *Context
  )
{
  UINT64  Basic;

  Basic                                   = AsmReadMsr64 (MSR_IA32_VMX_BASIC);
  Context->VmxBasic                       = Basic;
  Context->RevisionId                     = (UINT32)(Basic & 0x7FFFFFFFULL);
  Context->RegionSize                     = (UINT32)((Basic >> 32) & 0x1FFFULL);
  Context->RegionMemoryType               = (UINT8)((Basic >> 50) & 0x0FULL);
  Context->PhysicalAddressLimitedTo32Bits = (BOOLEAN)(((Basic >> 48) & 1ULL) != 0);
  Context->TrueControlsSupported          = (BOOLEAN)((Basic & VMX_BASIC_TRUE_CONTROLS) != 0);

  {
    UINT32  PrimaryMsr;
    UINT64  PrimaryCapability;
    UINT64  SecondaryCapability;

    PrimaryMsr = Context->TrueControlsSupported
                   ? MSR_IA32_VMX_TRUE_PROCBASED_CTLS
                   : MSR_IA32_VMX_PROCBASED_CTLS;
    PrimaryCapability = AsmReadMsr64 (PrimaryMsr);

    if ((((UINT32)(PrimaryCapability >> 32)) & CPU_BASED_ACTIVATE_SECONDARY) == 0) {
      HvOutput (SystemTable, L"ERROR: Secondary VM-execution controls are unavailable.\r\n");
      return EFI_UNSUPPORTED;
    }

    SecondaryCapability = AsmReadMsr64 (MSR_IA32_VMX_PROCBASED_CTLS2);
    if ((((UINT32)(SecondaryCapability >> 32)) & SECONDARY_EXEC_ENABLE_EPT) == 0) {
      HvOutput (SystemTable, L"ERROR: Secondary controls do not permit EPT.\r\n");
      return EFI_UNSUPPORTED;
    }

    Context->EptVpidCapabilities = AsmReadMsr64 (MSR_IA32_VMX_EPT_VPID_CAP);
  }

  HvOutputHex64 (SystemTable, L"IA32_VMX_BASIC       = ", Basic);
  HvOutputHex64 (SystemTable, L"VMCS revision ID     = ", Context->RevisionId);
  HvOutputHex64 (SystemTable, L"VMX region size      = ", Context->RegionSize);
  HvOutputHex64 (SystemTable, L"VMX memory type      = ", Context->RegionMemoryType);
  HvOutput (
    SystemTable,
    Context->TrueControlsSupported
      ? L"VMX control capability = true control MSRs\r\n"
      : L"VMX control capability = legacy control MSRs\r\n"
    );

  if ((Context->RegionSize == 0) || (Context->RegionSize > HV_PAGE_SIZE)) {
    HvOutput (SystemTable, L"ERROR: VMX region is larger than one 4-KiB page.\r\n");
    return EFI_UNSUPPORTED;
  }

  if (Context->RegionMemoryType != VMX_MEMORY_TYPE_WRITE_BACK) {
    HvOutput (
      SystemTable,
      L"ERROR: This sample supports only write-back VMXON/VMCS memory.\r\n"
      );
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/* Original: Application/VmxBringUp/IntelVmx.c, lines 2218-2269; function HvValidateCpuidExecutionCompatibility. */
STATIC
EFI_STATUS
HvValidateCpuidExecutionCompatibility (
  IN EFI_SYSTEM_TABLE  *SystemTable,
  IN HV_VMX_CONTEXT    *Context
  )
{
#if HV_CPUID_AP_SYMMETRY_MODE
  UINT32  Eax;
  UINT32  Ebx;
  UINT32  Ecx;
  UINT32  Edx;
  UINT32  MaxExtendedLeaf;
#endif

  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  (VOID)SystemTable;

#if HV_CPUID_AP_SYMMETRY_MODE
  // In mixed BSP/AP compatibility mode the BSP returns the physical CPUID
  // feature set. Verify that optional instructions which need secondary VMX
  // controls are actually executable in non-root mode before Windows starts.
  AsmCpuidEx (CPUID_STRUCTURED_EXTENDED, 0, &Eax, &Ebx, &Ecx, &Edx);
  if (((Ebx & CPUID_INVPCID_BIT) != 0) &&
      ((Context->SecondaryControls & SECONDARY_EXEC_ENABLE_INVPCID) == 0)) {
    HvOutput (SystemTable, L"ERROR: Physical CPUID exposes INVPCID but VMX controls rejected it.\r\n");
    return EFI_UNSUPPORTED;
  }

  AsmCpuid (CPUID_EXTENDED_MAX, &MaxExtendedLeaf, &Ebx, &Ecx, &Edx);
  if (MaxExtendedLeaf >= CPUID_EXTENDED_FEATURES) {
    AsmCpuid (CPUID_EXTENDED_FEATURES, &Eax, &Ebx, &Ecx, &Edx);
    if (((Edx & CPUID_RDTSCP_BIT) != 0) &&
        ((Context->SecondaryControls & SECONDARY_EXEC_ENABLE_RDTSCP) == 0)) {
      HvOutput (SystemTable, L"ERROR: Physical CPUID exposes RDTSCP but VMX controls rejected it.\r\n");
      return EFI_UNSUPPORTED;
    }
  }

  AsmCpuidEx (CPUID_EXTENDED_STATE, 1, &Eax, &Ebx, &Ecx, &Edx);
  if (((Eax & CPUID_XSAVES_BIT) != 0) &&
      ((Context->SecondaryControls & SECONDARY_EXEC_ENABLE_XSAVES) == 0)) {
    HvOutput (SystemTable, L"ERROR: Physical CPUID exposes XSAVES but VMX controls rejected it.\r\n");
    return EFI_UNSUPPORTED;
  }
#endif

  return EFI_SUCCESS;
}

/* Original: Application/VmxBringUp/IntelVmx.c, lines 2271-2291; function HvCaptureApFirmwareDescriptorState. */
STATIC
VOID
HvCaptureApFirmwareDescriptorState (
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  IA32_DESCRIPTOR  Gdtr;
  IA32_DESCRIPTOR  Idtr;

  if (Context == NULL) {
    return;
  }

  AsmReadGdtr (&Gdtr);
  AsmReadIdtr (&Idtr);
  Context->OriginalGdtrBase   = Gdtr.Base;
  Context->OriginalGdtrLimit  = Gdtr.Limit;
  Context->OriginalIdtrBase   = Idtr.Base;
  Context->OriginalIdtrLimit  = Idtr.Limit;
  Context->OriginalTrSelector = AsmReadTr ();
}
