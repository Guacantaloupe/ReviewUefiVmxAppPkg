/*
 * CODE REVIEW EXCERPT — not a standalone translation unit.
 * Extracted verbatim from the supplied UefiVmxAppPkg archive.
 * Original project uses EDK II types, macros, structures, and helpers
 * not bundled in this review-only package.
 * SPDX-License-Identifier: MIT
 */

/* BEGIN original: Application/VmxBringUp/Main.c, lines 43-70 */
EFI_STATUS
EFIAPI
HvGuestWindowsMain (
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  EFI_STATUS  Status;

  if ((Context == NULL) || (Context->SystemTable == NULL) ||
      (Context->ImageHandle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Context->SystemTable->ConOut->OutputString (
                                  Context->SystemTable->ConOut,
                                  L"VMX non-root guest is active. Starting Microsoft Windows Boot Manager...\r\n"
                                  );
  Context->SystemTable->BootServices->SetWatchdogTimer (0, 0, 0, NULL);

  Status = HvChainloadWindowsBootManager (
             Context->ImageHandle,
             Context->SystemTable
             );

  Context->WindowsBootStatus   = Status;
  Context->WindowsBootReturned = TRUE;
  return Status;
}
/* END original: Application/VmxBringUp/Main.c */

/* BEGIN original: Application/VmxBringUp/Main.c, lines 72-131 */
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  ContextRegion;
  HV_VMX_CONTEXT        *Context;

  SystemTable->ConOut->ClearScreen (SystemTable->ConOut);
  SystemTable->ConOut->OutputString (
                         SystemTable->ConOut,
                         L"Intel VMX UEFI Application v0.8.0 - enhanced one-level nested VMX build\r\n"
                         L"Every enabled CPU enters VMX non-root before Windows Boot Manager starts.\r\n"
                         L"L1 receives virtual VMX instructions and a software-backed VMCS.\r\n"
                         L"A private hardware VMCS executes one L2 vCPU per logical processor.\r\n"
                         L"This is a research prototype; Hyper-V/VBS compatibility is not guaranteed.\r\n"
                         L"----------------------------------------------------------------------------\r\n"
                         );

  ContextRegion = (1ULL << 39) - 1U;
  Status = SystemTable->BootServices->AllocatePages (
                                      AllocateMaxAddress,
                                      EfiRuntimeServicesData,
                                      HV_CONTEXT_PAGES,
                                      &ContextRegion
                                      );
  if (EFI_ERROR (Status)) {
    SystemTable->ConOut->OutputString (
                           SystemTable->ConOut,
                           L"ERROR: Could not allocate resident hypervisor context.\r\n"
                           );
    WaitForKey (SystemTable);
    return Status;
  }

  Context = (HV_VMX_CONTEXT *)(UINTN)ContextRegion;
  Status = HvVmxStartWindows (ImageHandle, SystemTable, Context);

  // A successful Windows boot does not return here. Reaching this point means
  // VMLAUNCH failed, a VM exit was not handled, or Windows Boot Manager returned.
  SystemTable->ConOut->OutputString (
                         SystemTable->ConOut,
                         L"The resident Windows guest stopped or failed to start.\r\n"
                         );

  if (Context->ApGuestActive || (Context->ApContexts != NULL)) {
    SystemTable->ConOut->OutputString (
                           SystemTable->ConOut,
                           L"FATAL: AP VMX teardown did not complete; resident memory cannot be released. Reset the machine.\r\n"
                           );
    CpuDeadLoop ();
  }

  SystemTable->BootServices->FreePages (ContextRegion, HV_CONTEXT_PAGES);
  WaitForKey (SystemTable);
  return Status;
}
/* END original: Application/VmxBringUp/Main.c */

