/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/NestedVmx.c, lines 4044-4090; function HvNestedInveptDescriptorValid. */
STATIC
BOOLEAN
HvNestedInveptDescriptorValid (
  IN CONST HV_VMX_CONTEXT      *Context,
  IN UINT64                    Type,
  IN CONST HV_INVEPT_DESCRIPTOR *Descriptor
  )
{
  UINT64  Capability;
  UINT64  PhysicalMask;
  UINT64  RootAddress;

  if ((Context == NULL) || (Descriptor == NULL)) {
    return FALSE;
  }

  Capability = Context->EptVpidCapabilities;
  if ((Capability & VMX_EPT_INVEPT_CAP) == 0) {
    return FALSE;
  }

  if (Type == 1U) {
    if ((Capability & VMX_EPT_INVEPT_SINGLE_CONTEXT) == 0) {
      return FALSE;
    }

    PhysicalMask = HvNestedPhysicalMask (Context);
    RootAddress  = Descriptor->EptPointer & PhysicalMask;
    return (BOOLEAN)(
             (Descriptor->GuestPhysicalAddress == 0) &&
             ((Descriptor->EptPointer & 7ULL) == EPT_POINTER_MEMORY_TYPE_WB) &&
             (((Descriptor->EptPointer >> 3) & 7ULL) == 3ULL) &&
             ((Descriptor->EptPointer & 0xF80ULL) == 0) &&
             ((Descriptor->EptPointer & ~(PhysicalMask | 0x7FULL)) == 0) &&
             ((RootAddress & (HV_PAGE_SIZE - 1ULL)) == 0) &&
             HvNestedPhysicalRangeValid (Context, RootAddress, HV_PAGE_SIZE) &&
             !HvNestedPointerConflictsWithL0 (Context, RootAddress)
             );
  }

  return (BOOLEAN)(
           (Type == 2U) &&
           ((Capability & VMX_EPT_INVEPT_ALL_CONTEXTS) != 0) &&
           (Descriptor->EptPointer == 0) &&
           (Descriptor->GuestPhysicalAddress == 0)
           );
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 4092-4140; function HvNestedInvvpidDescriptorValid. */
STATIC
BOOLEAN
HvNestedInvvpidDescriptorValid (
  IN CONST HV_VMX_CONTEXT       *Context,
  IN UINT64                     Type,
  IN CONST HV_INVVPID_DESCRIPTOR *Descriptor
  )
{
  UINT64  Capability;

  if ((Context == NULL) || (Descriptor == NULL) ||
      (Descriptor->Reserved1 != 0) || (Descriptor->Reserved2 != 0)) {
    return FALSE;
  }

  Capability = Context->EptVpidCapabilities;
  if ((Capability & VMX_VPID_INVVPID_CAP) == 0) {
    return FALSE;
  }

  switch (Type) {
    case 0U:
      return (BOOLEAN)(
               ((Capability & VMX_VPID_INVVPID_INDIVIDUAL_ADDR) != 0) &&
               (Descriptor->Vpid != 0) &&
               HvNestedCanonicalAddress (Descriptor->GuestLinearAddress)
               );
    case 1U:
      return (BOOLEAN)(
               ((Capability & VMX_VPID_INVVPID_SINGLE_CONTEXT) != 0) &&
               (Descriptor->Vpid != 0) &&
               (Descriptor->GuestLinearAddress == 0)
               );
    case 2U:
      return (BOOLEAN)(
               ((Capability & VMX_VPID_INVVPID_ALL_CONTEXTS) != 0) &&
               (Descriptor->Vpid == 0) &&
               (Descriptor->GuestLinearAddress == 0)
               );
    case 3U:
      return (BOOLEAN)(
               ((Capability & VMX_VPID_INVVPID_SINGLE_CONTEXT_GLOBALS) != 0) &&
               (Descriptor->Vpid != 0) &&
               (Descriptor->GuestLinearAddress == 0)
               );
    default:
      return FALSE;
  }
}
