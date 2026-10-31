/*
 * CODE REVIEW EXCERPT — not a standalone translation unit.
 * Extracted verbatim from the supplied UefiVmxAppPkg archive.
 * Original project uses EDK II types, macros, structures, and helpers
 * not bundled in this review-only package.
 * SPDX-License-Identifier: MIT
 */

/* BEGIN original: Application/VmxBringUp/WindowsLoader.c, lines 52-97 */
STATIC
EFI_STATUS
LoaderFileExists (
  IN EFI_SYSTEM_TABLE  *SystemTable,
  IN EFI_HANDLE        FileSystemHandle,
  IN CONST CHAR16      *Path
  )
{
  EFI_STATUS                       Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem;
  EFI_FILE_PROTOCOL                *Root;
  EFI_FILE_PROTOCOL                *File;

  FileSystem = NULL;
  Root       = NULL;
  File       = NULL;

  Status = SystemTable->BootServices->HandleProtocol (
                                      FileSystemHandle,
                                      &gEfiSimpleFileSystemProtocolGuid,
                                      (VOID **)&FileSystem
                                      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FileSystem->OpenVolume (FileSystem, &Root);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Root->Open (
                   Root,
                   &File,
                   (CHAR16 *)Path,
                   EFI_FILE_MODE_READ,
                   0
                   );

  if (File != NULL) {
    File->Close (File);
  }

  Root->Close (Root);
  return Status;
}
/* END original: Application/VmxBringUp/WindowsLoader.c */

/* BEGIN original: Application/VmxBringUp/WindowsLoader.c, lines 99-223 */
EFI_STATUS
HvChainloadWindowsBootManager (
  IN EFI_HANDLE        ParentImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_STATUS                LastStatus;
  EFI_HANDLE                *Handles;
  EFI_HANDLE                WindowsImageHandle;
  EFI_DEVICE_PATH_PROTOCOL  *WindowsDevicePath;
  CHAR16                    *ExitData;
  UINTN                     ExitDataSize;
  UINTN                     HandleCount;
  UINTN                     Index;

  if ((ParentImageHandle == NULL) || (SystemTable == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Handles            = NULL;
  HandleCount        = 0;
  WindowsImageHandle = NULL;
  WindowsDevicePath  = NULL;
  ExitData           = NULL;
  ExitDataSize       = 0;
  LastStatus         = EFI_NOT_FOUND;

  LoaderOutput (
    SystemTable,
    L"Searching EFI file systems for " WINDOWS_BOOT_MANAGER_PATH L" ...\r\n"
    );

  Status = SystemTable->BootServices->LocateHandleBuffer (
                                      ByProtocol,
                                      &gEfiSimpleFileSystemProtocolGuid,
                                      NULL,
                                      &HandleCount,
                                      &Handles
                                      );
  if (EFI_ERROR (Status)) {
    LoaderOutputHex64 (SystemTable, L"LocateHandleBuffer status = ", Status);
    return Status;
  }

  LoaderOutputHex64 (SystemTable, L"Simple file-system handles = ", HandleCount);

  for (Index = 0; Index < HandleCount; ++Index) {
    Status = LoaderFileExists (
               SystemTable,
               Handles[Index],
               WINDOWS_BOOT_MANAGER_PATH
               );
    if (EFI_ERROR (Status)) {
      LastStatus = Status;
      continue;
    }

    LoaderOutput (SystemTable, L"Found Windows Boot Manager on an EFI file system.\r\n");

    WindowsDevicePath = FileDevicePath (
                          Handles[Index],
                          WINDOWS_BOOT_MANAGER_PATH
                          );
    if (WindowsDevicePath == NULL) {
      LastStatus = EFI_OUT_OF_RESOURCES;
      continue;
    }

    WindowsImageHandle = NULL;
    Status = SystemTable->BootServices->LoadImage (
                                        FALSE,
                                        ParentImageHandle,
                                        WindowsDevicePath,
                                        NULL,
                                        0,
                                        &WindowsImageHandle
                                        );

    SystemTable->BootServices->FreePool (WindowsDevicePath);
    WindowsDevicePath = NULL;

    if (EFI_ERROR (Status)) {
      LoaderOutputHex64 (SystemTable, L"LoadImage status = ", Status);
      LastStatus = Status;
      continue;
    }

    LoaderOutput (
      SystemTable,
      L"LoadImage succeeded. Starting Microsoft Windows Boot Manager...\r\n"
      );

    SystemTable->BootServices->SetWatchdogTimer (0, 0, 0, NULL);

    ExitData     = NULL;
    ExitDataSize = 0;
    Status = SystemTable->BootServices->StartImage (
                                        WindowsImageHandle,
                                        &ExitDataSize,
                                        &ExitData
                                        );

    LoaderOutputHex64 (SystemTable, L"StartImage returned status = ", Status);

    if ((ExitData != NULL) && (ExitDataSize != 0)) {
      LoaderOutput (SystemTable, L"Windows Boot Manager exit data: ");
      LoaderOutput (SystemTable, ExitData);
      LoaderOutput (SystemTable, L"\r\n");
      SystemTable->BootServices->FreePool (ExitData);
      ExitData = NULL;
    }

    SystemTable->BootServices->UnloadImage (WindowsImageHandle);
    SystemTable->BootServices->FreePool (Handles);
    return Status;
  }

  SystemTable->BootServices->FreePool (Handles);
  LoaderOutput (
    SystemTable,
    L"ERROR: Microsoft Windows Boot Manager was not found on any UEFI file system.\r\n"
    );
  return LastStatus;
}
/* END original: Application/VmxBringUp/WindowsLoader.c */

