/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1236-1246; function HvNestedInjectInvalidOpcode. */
STATIC
BOOLEAN
HvNestedInjectInvalidOpcode (
  VOID
  )
{
  return HvNestedVmWrite (
           VMCS_VM_ENTRY_INTR_INFO_FIELD,
           VMX_INTR_INFO_VALID | VMX_INTR_TYPE_HARDWARE_EXCEPTION | X86_VECTOR_UD
           );
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1248-1264; function HvNestedInjectGeneralProtection. */
STATIC
BOOLEAN
HvNestedInjectGeneralProtection (
  VOID
  )
{
  return (BOOLEAN)(
           HvNestedVmWrite (VMCS_VM_ENTRY_EXCEPTION_ERROR_CODE, 0) &&
           HvNestedVmWrite (
             VMCS_VM_ENTRY_INTR_INFO_FIELD,
             VMX_INTR_INFO_VALID |
             VMX_INTR_TYPE_HARDWARE_EXCEPTION |
             VMX_INTR_DELIVER_ERROR_CODE |
             X86_VECTOR_GP
             )
           );
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1266-1278; function HvNestedGuestAtCpl0. */
STATIC
BOOLEAN
HvNestedGuestAtCpl0 (
  VOID
  )
{
  UINTN  Selector;

  return (BOOLEAN)(
           HvNestedVmRead (VMCS_GUEST_CS_SELECTOR, &Selector) &&
           ((Selector & 3U) == 0)
           );
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1280-1295; function HvNestedAdvanceRipOnly. */
STATIC
BOOLEAN
HvNestedAdvanceRipOnly (
  VOID
  )
{
  UINTN  GuestRip;
  UINTN  InstructionLength;

  if (!HvNestedVmRead (VMCS_GUEST_RIP, &GuestRip) ||
      !HvNestedVmRead (VMCS_VM_EXIT_INSTRUCTION_LEN, &InstructionLength)) {
    return FALSE;
  }

  return HvNestedVmWrite (VMCS_GUEST_RIP, GuestRip + InstructionLength);
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1297-1338; function HvNestedAdvanceInstruction. */
STATIC
BOOLEAN
HvNestedAdvanceInstruction (
  IN OUT HV_NESTED_STATE  *Nested,
  IN BOOLEAN              FailInvalid,
  IN BOOLEAN              FailValid,
  IN UINT32               InstructionError
  )
{
  UINTN   GuestRip;
  UINTN   GuestRflags;
  UINTN   InstructionLength;
  UINT64  NewFlags;

  if (!HvNestedVmRead (VMCS_GUEST_RIP, &GuestRip) ||
      !HvNestedVmRead (VMCS_GUEST_RFLAGS, &GuestRflags) ||
      !HvNestedVmRead (VMCS_VM_EXIT_INSTRUCTION_LEN, &InstructionLength)) {
    return FALSE;
  }

  NewFlags = (UINT64)GuestRflags & ~((UINT64)RFLAGS_VMX_STATUS_MASK);
  if (FailInvalid) {
    NewFlags |= RFLAGS_CF;
  } else if (FailValid) {
    NewFlags |= RFLAGS_ZF;
    if ((Nested != NULL) && Nested->VmcsLoaded) {
      Nested->LastEntryError = InstructionError;
      if (!HvNestedFieldWrite (
             Nested,
             VMCS_VM_INSTRUCTION_ERROR,
             InstructionError
             )) {
        return FALSE;
      }
    }
  }

  return (BOOLEAN)(
           HvNestedVmWrite (VMCS_GUEST_RFLAGS, (UINTN)NewFlags) &&
           HvNestedVmWrite (VMCS_GUEST_RIP, GuestRip + InstructionLength)
           );
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1340-1349; function HvNestedSucceed. */
STATIC
UINTN
HvNestedSucceed (
  IN OUT HV_NESTED_STATE  *Nested
  )
{
  return HvNestedAdvanceInstruction (Nested, FALSE, FALSE, 0)
           ? HvVmExitResume
           : HvVmExitShutdownFailure;
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1351-1360; function HvNestedFailInvalid. */
STATIC
UINTN
HvNestedFailInvalid (
  IN OUT HV_NESTED_STATE  *Nested
  )
{
  return HvNestedAdvanceInstruction (Nested, TRUE, FALSE, 0)
           ? HvVmExitResume
           : HvVmExitShutdownFailure;
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1362-1376; function HvNestedFailValid. */
STATIC
UINTN
HvNestedFailValid (
  IN OUT HV_NESTED_STATE  *Nested,
  IN UINT32               Error
  )
{
  if ((Nested == NULL) || !Nested->VmcsLoaded) {
    return HvNestedFailInvalid (Nested);
  }

  return HvNestedAdvanceInstruction (Nested, FALSE, TRUE, Error)
           ? HvVmExitResume
           : HvVmExitShutdownFailure;
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1378-1390; function HvNestedVirtualVmxeSet. */
STATIC
BOOLEAN
HvNestedVirtualVmxeSet (
  VOID
  )
{
  UINTN  Shadow;

  return (BOOLEAN)(
           HvNestedVmRead (VMCS_CR4_READ_SHADOW, &Shadow) &&
           ((Shadow & CR4_VMXE) != 0)
           );
}
