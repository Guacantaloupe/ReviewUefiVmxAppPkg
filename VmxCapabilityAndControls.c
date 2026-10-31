/*
 * CODE REVIEW EXCERPT — not a standalone translation unit.
 * Extracted verbatim from the supplied UefiVmxAppPkg archive.
 * Original project uses EDK II types, macros, structures, and helpers
 * not bundled in this review-only package.
 * SPDX-License-Identifier: MIT
 */

/* BEGIN original: Application/VmxBringUp/IntelVmx.c, lines 187-220 */
STATIC
BOOLEAN
HvIsIntelCpu (
  VOID
  )
{
  UINT32  Eax;
  UINT32  Ebx;
  UINT32  Ecx;
  UINT32  Edx;

  AsmCpuid (0, &Eax, &Ebx, &Ecx, &Edx);

  return (BOOLEAN)(
           (Ebx == 0x756E6547U) &&
           (Edx == 0x49656E69U) &&
           (Ecx == 0x6C65746EU)
           );
}

STATIC
BOOLEAN
HvCpuSupportsVmx (
  VOID
  )
{
  UINT32  Eax;
  UINT32  Ebx;
  UINT32  Ecx;
  UINT32  Edx;

  AsmCpuid (CPUID_VERSION_INFO, &Eax, &Ebx, &Ecx, &Edx);
  return (BOOLEAN)((Ecx & CPUID_VMX_BIT) != 0);
}
/* END original: Application/VmxBringUp/IntelVmx.c */

/* BEGIN original: Application/VmxBringUp/IntelVmx.c, lines 222-271 */
STATIC
EFI_STATUS
HvCheckFeatureControl (
  IN EFI_SYSTEM_TABLE   *SystemTable,
  IN OUT HV_VMX_CONTEXT *Context
  )
{
  UINT64  FeatureControl;

  FeatureControl          = AsmReadMsr64 (MSR_IA32_FEATURE_CONTROL);
  Context->FeatureControl = FeatureControl;

  HvOutputHex64 (SystemTable, L"IA32_FEATURE_CONTROL = ", FeatureControl);

  if ((FeatureControl & IA32_FEATURE_CONTROL_LOCK) != 0) {
    if ((FeatureControl & IA32_FEATURE_CONTROL_VMX_OUTSIDE) == 0) {
      HvOutput (
        SystemTable,
        L"ERROR: Firmware locked VMX outside SMX as disabled. Enable Intel VT-x in BIOS/UEFI.\r\n"
        );
      return EFI_UNSUPPORTED;
    }

    return EFI_SUCCESS;
  }

#if HV_ALLOW_LOCKING_FEATURE_CONTROL
  FeatureControl |= IA32_FEATURE_CONTROL_LOCK |
                    IA32_FEATURE_CONTROL_VMX_OUTSIDE;
  AsmWriteMsr64 (MSR_IA32_FEATURE_CONTROL, FeatureControl);
  Context->FeatureControlChanged = TRUE;
  Context->FeatureControl        = FeatureControl;

  HvOutput (
    SystemTable,
    L"WARNING: IA32_FEATURE_CONTROL was unlocked; this app enabled VMX and locked it until reset.\r\n"
    );
  return EFI_SUCCESS;
#else
  HvOutput (
    SystemTable,
    L"ERROR: IA32_FEATURE_CONTROL is unlocked. The safe default refuses to lock it.\r\n"
    );
  HvOutput (
    SystemTable,
    L"       Set HV_ALLOW_LOCKING_FEATURE_CONTROL to 1 only on a development machine if required.\r\n"
    );
  return EFI_ACCESS_DENIED;
#endif
}
/* END original: Application/VmxBringUp/IntelVmx.c */

/* BEGIN original: Application/VmxBringUp/IntelVmx.c, lines 626-642 */
STATIC
UINT32
HvAdjustControls (
  IN UINT32  Desired,
  IN UINT32  CapabilityMsr
  )
{
  UINT64  Capability;
  UINT32  MustBeOne;
  UINT32  MayBeOne;

  Capability = AsmReadMsr64 (CapabilityMsr);
  MustBeOne  = (UINT32)Capability;
  MayBeOne   = (UINT32)(Capability >> 32);

  return (Desired | MustBeOne) & MayBeOne;
}
/* END original: Application/VmxBringUp/IntelVmx.c */

/* BEGIN original: Application/VmxBringUp/IntelVmx.c, lines 644-672 */
STATIC
EFI_STATUS
HvVmWriteChecked (
  IN EFI_SYSTEM_TABLE  *SystemTable,
  IN UINTN             Field,
  IN UINTN             Value
  )
{
  HV_VMX_INSTRUCTION_STATUS  VmxStatus;

  VmxStatus = HvAsmVmWrite (Field, Value);
  if (VmxStatus == HvVmxSuccess) {
    return EFI_SUCCESS;
  }

  HvOutput (SystemTable, L"ERROR: VMWRITE failed for field ");
  HvOutputHex64 (SystemTable, L"", Field);
  return EFI_DEVICE_ERROR;
}

STATIC
BOOLEAN
HvVmReadValue (
  IN UINTN   Field,
  OUT UINTN  *Value
  )
{
  return (BOOLEAN)(HvAsmVmRead (Field, Value) == HvVmxSuccess);
}
/* END original: Application/VmxBringUp/IntelVmx.c */

