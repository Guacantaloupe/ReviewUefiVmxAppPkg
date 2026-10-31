/*
 * UefiVmxAppPkg — selected REAL C source excerpt for technical review.
 * Original file(s) are marked per function below.
 * Dependencies, types, constants and additional core implementations are omitted.
 * NOT a standalone translation unit or buildable sample.
 * Source header: SPDX-License-Identifier: MIT (see original project).
 */

/* Original: Application/VmxBringUp/IntelVmx.c, lines 2423-2444; function HvCopyProcessorCapabilities. */
STATIC
VOID
HvCopyProcessorCapabilities (
  IN CONST HV_VMX_CONTEXT  *Source,
  OUT HV_VMX_CONTEXT       *Destination
  )
{
  Destination->SystemTable                     = Source->SystemTable;
  Destination->ImageHandle                     = Source->ImageHandle;
  Destination->ImageBase                       = Source->ImageBase;
  Destination->ImageSize                       = Source->ImageSize;
  Destination->FeatureControl                  = Source->FeatureControl;
  Destination->VmxBasic                        = Source->VmxBasic;
  Destination->RevisionId                      = Source->RevisionId;
  Destination->RegionSize                      = Source->RegionSize;
  Destination->RegionMemoryType                = Source->RegionMemoryType;
  Destination->PhysicalAddressLimitedTo32Bits = Source->PhysicalAddressLimitedTo32Bits;
  Destination->TrueControlsSupported           = Source->TrueControlsSupported;
  Destination->EptVpidCapabilities             = Source->EptVpidCapabilities;
  Destination->NestedVmxEnabled                = Source->NestedVmxEnabled;
  Destination->IsApplicationProcessor          = TRUE;
}

/* Original: Application/VmxBringUp/IntelVmx.c, lines 2446-2479; function HvPrepareApResources. */
STATIC
EFI_STATUS
HvPrepareApResources (
  IN EFI_SYSTEM_TABLE    *SystemTable,
  IN CONST HV_VMX_CONTEXT *BspContext,
  IN OUT HV_VMX_CONTEXT  *ApContext
  )
{
  EFI_STATUS  Status;

  HvCopyProcessorCapabilities (BspContext, ApContext);

  Status = HvAllocateVmxResources (SystemTable, ApContext);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // The identity-map EPT is immutable after construction and can be shared by
  // all VMCS instances. Copy its span before building the AP private CR3 so
  // nested-VMX root code can directly access L1-owned VMX data structures.
  ApContext->EptPointer          = BspContext->EptPointer;
  ApContext->EptMappedBytes      = BspContext->EptMappedBytes;
  ApContext->PhysicalAddressBits = BspContext->PhysicalAddressBits;
  if (ApContext->EptPointer == 0) {
    return EFI_NOT_READY;
  }

  Status = HvHostEnvironmentInitialize (SystemTable, ApContext);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/* Original: Application/VmxBringUp/IntelVmx.c, lines 2481-2526; function HvStopApplicationProcessors. */
STATIC
BOOLEAN
HvStopApplicationProcessors (
  IN EFI_SYSTEM_TABLE    *SystemTable,
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  EFI_STATUS      Status;
  HV_VMX_CONTEXT  *ApContext;
  UINTN           Index;
  BOOLEAN         AllStopped;

  if ((Context == NULL) || (Context->ApContexts == NULL)) {
    return TRUE;
  }

  AllStopped = TRUE;
  for (Index = 0; Index < Context->ApContextCount; ++Index) {
    ApContext = &Context->ApContexts[Index];
    if (!ApContext->IsApplicationProcessor || !ApContext->ApGuestActive) {
      continue;
    }

    Status = Context->MpServices->StartupThisAP (
                                    Context->MpServices,
                                    HvApLeaveVmxGuest,
                                    ApContext->ProcessorNumber,
                                    NULL,
                                    5000000U,
                                    ApContext,
                                    NULL
                                    );
    if (EFI_ERROR (Status) || ApContext->ApGuestActive) {
      AllStopped = FALSE;
    }
  }

  Context->ApGuestActive = (BOOLEAN)!AllStopped;
  if (!AllStopped) {
    HvOutput (
      SystemTable,
      L"ERROR: One or more APs could not leave VMX; resident memory is being preserved.\r\n"
      );
  }
  return AllStopped;
}

/* Original: Application/VmxBringUp/IntelVmx.c, lines 2528-2562; function HvReleaseApResources. */
STATIC
VOID
HvReleaseApResources (
  IN EFI_SYSTEM_TABLE    *SystemTable,
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
  HV_VMX_CONTEXT  *ApContext;
  UINTN           Index;

  if ((Context == NULL) || (Context->ApContexts == NULL)) {
    return;
  }

  for (Index = 0; Index < Context->ApContextCount; ++Index) {
    ApContext = &Context->ApContexts[Index];
    if (ApContext->IsApplicationProcessor && !ApContext->ApGuestActive) {
      HvVmxCleanup (SystemTable, ApContext);
    }
  }

  if ((Context->ApContextsRegion != 0) && (Context->ApContextsPages != 0)) {
    SystemTable->BootServices->FreePages (
                                 Context->ApContextsRegion,
                                 Context->ApContextsPages
                                 );
  }

  Context->ApContextsRegion = 0;
  Context->ApContextsPages  = 0;
  Context->ApContexts       = NULL;
  Context->ApContextCount   = 0;
  Context->MpServices       = NULL;
  Context->ApGuestActive    = FALSE;
}

/* Original: Application/VmxBringUp/IntelVmx.c, lines 2564-2692; function HvStartApplicationProcessors. */
STATIC
EFI_STATUS
HvStartApplicationProcessors (
  IN EFI_SYSTEM_TABLE    *SystemTable,
  IN OUT HV_VMX_CONTEXT  *Context
  )
{
#if HV_VIRTUALIZE_ALL_ENABLED_APS
  EFI_STATUS                 Status;
  EFI_PROCESSOR_INFORMATION  ProcessorInfo;
  EFI_PHYSICAL_ADDRESS       Region;
  HV_VMX_CONTEXT             *ApContext;
  UINTN                      TotalProcessors;
  UINTN                      EnabledProcessors;
  UINTN                      Index;
  UINTN                      LaunchedCount;

  Context->MpServices = NULL;
  Status = SystemTable->BootServices->LocateProtocol (
                                      &gEfiMpServiceProtocolGuid,
                                      NULL,
                                      (VOID **)&Context->MpServices
                                      );
  if (EFI_ERROR (Status)) {
    HvOutput (SystemTable, L"ERROR: EFI MP Services protocol is unavailable.\r\n");
    return Status;
  }

  Status = Context->MpServices->GetNumberOfProcessors (
                                  Context->MpServices,
                                  &TotalProcessors,
                                  &EnabledProcessors
                                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Context->NumberOfProcessors        = TotalProcessors;
  Context->NumberOfEnabledProcessors = EnabledProcessors;
  if (EnabledProcessors <= 1U) {
    HvOutput (SystemTable, L"No enabled APs were reported by firmware.\r\n");
    return EFI_SUCCESS;
  }

  Context->ApContextsPages = EFI_SIZE_TO_PAGES (
                               TotalProcessors * sizeof (HV_VMX_CONTEXT)
                               );
  Region = (1ULL << 39) - 1U;
  Status = SystemTable->BootServices->AllocatePages (
                                      AllocateMaxAddress,
                                      EfiRuntimeServicesData,
                                      Context->ApContextsPages,
                                      &Region
                                      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Context->ApContextsRegion = Region;
  Context->ApContexts       = (HV_VMX_CONTEXT *)(UINTN)Region;
  Context->ApContextCount   = TotalProcessors;
  ZeroMem (
    Context->ApContexts,
    EFI_PAGES_TO_SIZE (Context->ApContextsPages)
    );

  LaunchedCount = 0;
  for (Index = 0; Index < TotalProcessors; ++Index) {
    ZeroMem (&ProcessorInfo, sizeof (ProcessorInfo));
    Status = Context->MpServices->GetProcessorInfo (
                                    Context->MpServices,
                                    Index,
                                    &ProcessorInfo
                                    );
    if (EFI_ERROR (Status)) {
      break;
    }

    if (((ProcessorInfo.StatusFlag & PROCESSOR_ENABLED_BIT) == 0) ||
        ((ProcessorInfo.StatusFlag & PROCESSOR_AS_BSP_BIT) != 0)) {
      continue;
    }

    ApContext = &Context->ApContexts[Index];
    ApContext->ProcessorNumber = Index;
    Status = HvPrepareApResources (SystemTable, Context, ApContext);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = Context->MpServices->StartupThisAP (
                                    Context->MpServices,
                                    HvApEnterVmxGuest,
                                    Index,
                                    NULL,
                                    5000000U,
                                    ApContext,
                                    NULL
                                    );
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = ApContext->ApBringUpStatus;
    if (EFI_ERROR (Status) || !ApContext->ApGuestActive) {
      if (!EFI_ERROR (Status)) {
        Status = EFI_DEVICE_ERROR;
      }
      break;
    }

    ++LaunchedCount;
  }

  if (EFI_ERROR (Status)) {
    HvStopApplicationProcessors (SystemTable, Context);
    if (!Context->ApGuestActive) {
      HvReleaseApResources (SystemTable, Context);
    }
    return Status;
  }

  Context->ApGuestActive = (BOOLEAN)(LaunchedCount != 0);
  HvOutputHex64 (SystemTable, L"APs running firmware wait code in VMX non-root = ", LaunchedCount);
  return EFI_SUCCESS;
#else
  return EFI_SUCCESS;
#endif
}
