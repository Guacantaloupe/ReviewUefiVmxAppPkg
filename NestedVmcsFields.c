/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/NestedVmx.c, lines 965-983; function HvNestedFindField. */
HvNestedFindField (
  IN OUT HV_NESTED_STATE  *Nested,
  IN UINT32               Field
  )
{
  UINTN  Index;

  if (Nested == NULL) {
    return NULL;
  }

  for (Index = 0; Index < Nested->FieldCount; ++Index) {
    if (Nested->Fields[Index].Field == Field) {
      return &Nested->Fields[Index];
    }
  }

  return NULL;
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 985-1002; function HvNestedFieldEncodingValid. */
STATIC
BOOLEAN
HvNestedFieldEncodingValid (
  IN UINT64  Field
  )
{
  UINT32  Encoding;
  UINT32  Width;

  if ((Field > MAX_UINT16) || ((Field & BIT12) != 0)) {
    return FALSE;
  }

  Encoding = (UINT32)Field;
  Width = (Encoding >> VMX_FIELD_WIDTH_SHIFT) & VMX_FIELD_WIDTH_MASK;

  return (BOOLEAN)(((Encoding & 1U) == 0) || (Width == VMX_FIELD_WIDTH_64));
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1004-1018; function HvNestedFieldSupported. */
STATIC
BOOLEAN
HvNestedFieldSupported (
  IN UINT32  Field
  )
{
  UINTN   Value;
  UINT32  CanonicalField;

  CanonicalField = Field & ~1U;
  Value = 0;
  return (BOOLEAN)(
           HvAsmVmRead ((UINTN)CanonicalField, &Value) == HvVmxSuccess
           );
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1020-1038; function HvNestedNormalizeFieldValue. */
STATIC
UINT64
HvNestedNormalizeFieldValue (
  IN UINT32  Field,
  IN UINT64  Value
  )
{
  UINT32  Width;

  Width = (Field >> VMX_FIELD_WIDTH_SHIFT) & VMX_FIELD_WIDTH_MASK;
  if (Width == VMX_FIELD_WIDTH_16) {
    return (UINT16)Value;
  }
  if (Width == VMX_FIELD_WIDTH_32) {
    return (UINT32)Value;
  }

  return Value;
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1040-1063; function HvNestedFieldRead. */
STATIC
BOOLEAN
HvNestedFieldRead (
  IN OUT HV_NESTED_STATE  *Nested,
  IN UINT32               Field,
  OUT UINT64              *Value
  )
{
  HV_NESTED_VMCS_FIELD  *Entry;

  if ((Nested == NULL) || (Value == NULL)) {
    return FALSE;
  }

  if ((Field & 1U) != 0) {
    Entry = HvNestedFindField (Nested, Field - 1U);
    *Value = (Entry != NULL) ? RShiftU64 (Entry->Value, 32) : 0;
    return TRUE;
  }

  Entry = HvNestedFindField (Nested, Field);
  *Value = (Entry != NULL) ? Entry->Value : 0;
  return TRUE;
}

/* Original: Application/VmxBringUp/NestedVmx.c, lines 1065-1106; function HvNestedFieldWrite. */
STATIC
BOOLEAN
HvNestedFieldWrite (
  IN OUT HV_NESTED_STATE  *Nested,
  IN UINT32               Field,
  IN UINT64               Value
  )
{
  HV_NESTED_VMCS_FIELD  *Entry;
  UINT32                CanonicalField;

  if (Nested == NULL) {
    return FALSE;
  }

  CanonicalField = Field & ~1U;
  Entry = HvNestedFindField (Nested, CanonicalField);
  if (Entry == NULL) {
    if (Nested->FieldCount >= HV_NESTED_VMCS_FIELD_CAPACITY) {
      return FALSE;
    }

    Entry = &Nested->Fields[Nested->FieldCount++];
    Entry->Field    = CanonicalField;
    Entry->Reserved = 0;
    Entry->Value    = 0;
  }

  if ((Field & 1U) != 0) {
    Entry->Value = (Entry->Value & MAX_UINT32) |
                   LShiftU64 ((UINT64)(UINT32)Value, 32);
    return TRUE;
  }

  Value = HvNestedNormalizeFieldValue (Field, Value);
  if ((Field >= VMCS_GUEST_ES_AR_BYTES) && (Field <= VMCS_GUEST_TR_AR_BYTES)) {
    Value &= 0x0001F0FFULL;
  }

  Entry->Value = Value;
  return TRUE;
}
