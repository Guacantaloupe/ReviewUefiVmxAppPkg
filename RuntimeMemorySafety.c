/*
 * CODE REVIEW EXCERPT — not a standalone translation unit.
 * Extracted verbatim from the supplied UefiVmxAppPkg archive.
 * Original project uses EDK II types, macros, structures, and helpers
 * not bundled in this review-only package.
 * SPDX-License-Identifier: MIT
 */

/* BEGIN original: Application/VmxBringUp/Resident.c, lines 37-60 */
STATIC
BOOLEAN
HvRangesOverlap (
  IN UINT64  StartA,
  IN UINT64  SizeA,
  IN UINT64  StartB,
  IN UINT64  SizeB
  )
{
  UINT64  EndA;
  UINT64  EndB;

  if ((SizeA == 0) || (SizeB == 0)) {
    return FALSE;
  }

  EndA = StartA + SizeA;
  EndB = StartB + SizeB;
  if ((EndA < StartA) || (EndB < StartB)) {
    return TRUE;
  }

  return (BOOLEAN)((StartA < EndB) && (StartB < EndA));
}
/* END original: Application/VmxBringUp/Resident.c */

/* BEGIN original: Application/VmxBringUp/Resident.c, lines 197-212 */
STATIC
EFI_STATUS
HvAllocateRuntimePagesBelow512G (
  IN EFI_SYSTEM_TABLE       *SystemTable,
  IN UINTN                  Pages,
  OUT EFI_PHYSICAL_ADDRESS  *Address
  )
{
  *Address = HV_HOST_IDENTITY_BYTES - 1U;
  return SystemTable->BootServices->AllocatePages (
                                      AllocateMaxAddress,
                                      EfiRuntimeServicesData,
                                      Pages,
                                      Address
                                      );
}
/* END original: Application/VmxBringUp/Resident.c */

/* BEGIN original: Application/VmxBringUp/Resident.c, lines 214-241 */
STATIC
BOOLEAN
HvAccumulateRangeEnd (
  IN UINT64   Start,
  IN UINT64   Size,
  IN OUT UINT64  *HighestEnd
  )
{
  UINT64  End;

  if (HighestEnd == NULL) {
    return FALSE;
  }
  if (Size == 0) {
    return TRUE;
  }

  End = Start + Size;
  if (End < Start) {
    return FALSE;
  }

  if (End > *HighestEnd) {
    *HighestEnd = End;
  }

  return TRUE;
}
/* END original: Application/VmxBringUp/Resident.c */

