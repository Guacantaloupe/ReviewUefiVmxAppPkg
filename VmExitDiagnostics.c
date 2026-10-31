/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/IntelVmx.c, lines 1614-1637; function HvAdvanceGuestRip. */
STATIC
BOOLEAN
HvAdvanceGuestRip (
  VOID
  )
{
  UINTN  GuestRip;
  UINTN  InstructionLength;

  if (!HvVmReadValue (VMCS_GUEST_RIP, &GuestRip)) {
    return FALSE;
  }

  if (!HvVmReadValue (VMCS_VM_EXIT_INSTRUCTION_LEN, &InstructionLength)) {
    return FALSE;
  }

  return (BOOLEAN)(
           HvAsmVmWrite (
             VMCS_GUEST_RIP,
             GuestRip + InstructionLength
             ) == HvVmxSuccess
           );
}

/* Original: Application/VmxBringUp/IntelVmx.c, lines 1639-1739; function HvCaptureVmExitSnapshot. */
STATIC
VOID
HvCaptureVmExitSnapshot (
  IN OUT HV_VMX_CONTEXT       *Context,
  IN CONST HV_GUEST_REGS      *Registers,
  IN UINTN                    RawExitReason
  )
{
  UINTN  Value;

  Context->LastExitReasonRaw = (UINT32)RawExitReason;
  Context->LastExitReason    = (UINT32)RawExitReason & VMX_EXIT_REASON_BASIC_MASK;

  if (Registers != NULL) {
    Context->LastGuestRax = Registers->Rax;
    Context->LastGuestRbx = Registers->Rbx;
    Context->LastGuestRcx = Registers->Rcx;
    Context->LastGuestRdx = Registers->Rdx;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_EXIT_QUALIFICATION, &Value)) {
    Context->LastExitQualification = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_GUEST_RIP, &Value)) {
    Context->LastGuestRip = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_GUEST_RSP, &Value)) {
    Context->LastGuestRsp = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_GUEST_RFLAGS, &Value)) {
    Context->LastGuestRflags = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_GUEST_CR0, &Value)) {
    Context->LastGuestCr0 = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_GUEST_CR3, &Value)) {
    Context->LastGuestCr3 = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_GUEST_CR4, &Value)) {
    Context->LastGuestCr4 = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_GUEST_PHYSICAL_ADDRESS, &Value)) {
    Context->LastGuestPhysicalAddress = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_GUEST_LINEAR_ADDRESS, &Value)) {
    Context->LastGuestLinearAddress = Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_VM_EXIT_INSTRUCTION_LEN, &Value)) {
    Context->LastExitInstructionLength = (UINT32)Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_VM_EXIT_INSTRUCTION_INFO, &Value)) {
    Context->LastExitInstructionInfo = (UINT32)Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_VM_EXIT_INTERRUPTION_INFO, &Value)) {
    Context->LastExitInterruptionInfo = (UINT32)Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_VM_EXIT_INTERRUPTION_ERROR, &Value)) {
    Context->LastExitInterruptionError = (UINT32)Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_IDT_VECTORING_INFO, &Value)) {
    Context->LastIdtVectoringInfo = (UINT32)Value;
  }

  Value = 0;
  if (HvVmReadValue (VMCS_IDT_VECTORING_ERROR, &Value)) {
    Context->LastIdtVectoringError = (UINT32)Value;
  }

  if (!Context->FirstExitCaptured) {
    Context->FirstExitCaptured = TRUE;
    Context->FirstExitReason   = Context->LastExitReason;
    Context->FirstExitGuestRip = Context->LastGuestRip;
  }
}
