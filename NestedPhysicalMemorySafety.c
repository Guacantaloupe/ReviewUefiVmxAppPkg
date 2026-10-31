/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/NestedVmx.c, lines 516-533; function HvNestedPhysicalMask. */
STATIC
UINT64
HvNestedPhysicalMask (
  IN CONST HV_VMX_CONTEXT  *Context
  )
{
  UINT8  Bits;

  Bits = (Context != NULL) ? Context->PhysicalAddressBits : 36U;
  if (Bits < 32U) {
    Bits = 32U;
  }
  if (Bits > 52U) {
    Bits = 52U;
  }

  return (LShiftU64 (1ULL, Bits) - 1ULL) & X64_ENTRY_ADDRESS_MASK;
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 535-558; function HvNestedPhysicalRangeValid. */
STATIC
BOOLEAN
HvNestedPhysicalRangeValid (
  IN CONST HV_VMX_CONTEXT  *Context,
  IN UINT64                Address,
  IN UINTN                 Size
  )
{
  UINT64  End;

  if ((Context == NULL) || (Size == 0)) {
    return FALSE;
  }

  End = Address + Size;
  if (End < Address) {
    return FALSE;
  }

  return (BOOLEAN)(
           (Address < Context->EptMappedBytes) &&
           (End <= Context->EptMappedBytes)
           );
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 560-575; function HvNestedReadPhysical. */
STATIC
BOOLEAN
HvNestedReadPhysical (
  IN CONST HV_VMX_CONTEXT  *Context,
  IN UINT64                Address,
  OUT VOID                 *Buffer,
  IN UINTN                 Size
  )
{
  if ((Buffer == NULL) || !HvNestedPhysicalRangeValid (Context, Address, Size)) {
    return FALSE;
  }

  CopyMem (Buffer, (CONST VOID *)(UINTN)Address, Size);
  return TRUE;
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 577-592; function HvNestedWritePhysical. */
STATIC
BOOLEAN
HvNestedWritePhysical (
  IN CONST HV_VMX_CONTEXT  *Context,
  IN UINT64                Address,
  IN CONST VOID            *Buffer,
  IN UINTN                 Size
  )
{
  if ((Buffer == NULL) || !HvNestedPhysicalRangeValid (Context, Address, Size)) {
    return FALSE;
  }

  CopyMem ((VOID *)(UINTN)Address, Buffer, Size);
  return TRUE;
}
