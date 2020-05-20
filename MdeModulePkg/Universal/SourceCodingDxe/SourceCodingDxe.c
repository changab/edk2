/** @file

  The implementation of EFI Source Coding (compress/decompress) Protocol.

  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SourceCodingInternal.h"

LIST_ENTRY mSourceCodingeList;
EFI_HANDLE mProtocolHandle;

/**
  This function provides source coding algorithm registration.

  @param[in]    This        This is the EFI_SOURCE_CODING_PROTOCOL instance.
  @param[in]    Identifier  The name of this source coding algorithm.
  @param[in]    Compress    Compress function provided by this source coding instance.
                            NULL means the compression is not supported.
  @param[in]    Decompress  Decompress function provided by this source coding instance.
                            NULL means the compression is not supported.

  @retval EFI_SUCCESS       Register successfully.
  @retval Others            Fail to register.

--*/
EFI_STATUS
EFIAPI
SourceCodingRegister (
  IN EFI_SOURCE_CODING_PROTOCOL       *This,
  IN EFI_SOURCE_CODING_IDENTIFIER      Identifier,
  IN EFI_SOURCE_CODING_COMPRESS        Compress,
  IN EFI_SOURCE_CODING_DECOMPRESS      Decompress
)
{
  SOURCE_CODING_INSTANCE *Instance;

  if (This == NULL ||
      Identifier == NULL ||
      StrLen (Identifier) == 0 ||
      (Compress == NULL && Decompress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check how many name space interpreter can interpret.
  //
  Instance = (SOURCE_CODING_INSTANCE *)AllocateZeroPool (sizeof (SOURCE_CODING_INSTANCE));
  if (Instance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  Instance->MethodIdentifer = AllocatePool (StrSize (Identifier));
  if (Instance->MethodIdentifer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  InitializeListHead (&Instance->NextSourceCodingInstance);
  StrCpyS((CHAR16 *)Instance->MethodIdentifer, StrSize(Identifier), Identifier);
  Instance->CompressMethod = Compress;
  Instance->DecompressMethod = Decompress;
  InsertTailList (&mSourceCodingeList, &Instance->NextSourceCodingInstance);
  return EFI_SUCCESS;
}
/**
  This functions looks for the proper source coding function.

  @param[in]    Identifier      Identifier of source coding.
  @param[in]    DecompressFunc  Pointer to receive decompress function pointer.
  @param[in]    CompressFunc    Pointer to receive compress function pointer.

  @retval EFI_SUCCESS           Register successfully.
  @retval EFI_UNSUPPORTED       No source coding found.
  @retval Others                Other erros.

--*/
EFI_STATUS
FindSourceCodingInstance (
  IN EFI_SOURCE_CODING_IDENTIFIER Identifier,
  IN EFI_SOURCE_CODING_DECOMPRESS *DecompressFunc,
  IN EFI_SOURCE_CODING_COMPRESS   *CompressFunc
  )
{
  SOURCE_CODING_INSTANCE *Instance;
  BOOLEAN NoMoreInstance;

  if (IsListEmpty (&mSourceCodingeList)) {
    return EFI_UNSUPPORTED;
  }
  Instance = (SOURCE_CODING_INSTANCE *)GetFirstNode (&mSourceCodingeList);
  do {
    if (StrCmp ((CONST CHAR16*)Instance->MethodIdentifer, (CONST CHAR16*)Identifier) == 0) {
      if (DecompressFunc != NULL) {
        *DecompressFunc = Instance->DecompressMethod;
      }
      if (CompressFunc != NULL) {
        *CompressFunc = Instance->CompressMethod;
      }
      return EFI_SUCCESS;
    }
    NoMoreInstance = IsNodeAtEnd (&mSourceCodingeList, &Instance->NextSourceCodingInstance);
    if (NoMoreInstance != TRUE) {
      Instance = (SOURCE_CODING_INSTANCE *)GetNextNode (&mSourceCodingeList, &Instance->NextSourceCodingInstance);
    }
  } while (NoMoreInstance != TRUE);

  return EFI_SUCCESS;
}

/**
  This function provides source coding supported on system.

  @param[in]    This                    This is the EFI_SOURCE_CODING_PROTOCOL instance.
  @param[in]    SourceCodingSupported   An array of EFI_SOURCE_CODING_IDENTIFIER of supported
                                        source coding.
  @retval EFI_SUCCESS             Register successfully.
  @retval Others                  Fail to register.

--*/
EFI_STATUS
SourceCodingSupported (
  IN EFI_SOURCE_CODING_PROTOCOL       *This,
  IN EFI_SOURCE_CODING_IDENTIFIER      *Identifer
)
{
  return EFI_SUCCESS;
}

/**
  Deompress function provided by this source coding instance

  @param[in]    This                 This is the EFI_SOURCE_CODING_PROTOCOL instance.
  @param[in]    Identifier           identifier of source coding.
  @param[in]    CompressedData       Pointer to compressed data.
  @param[in]    CompressedDataLen    Length of decompressed data.
  @param[in]    DecompressedData     Pointer to receive the pointer to decompressed data.
                                     Caller has to free the memory allocated for this.
  @param[in]    DecompressedDataLen  Pointer to receive decompressed data length.

  @retval EFI_SUCCESS           Register successfully.
  @retval EFI_UNSUPPORTED       No source coding instance to decompress the source data.
  @retval EFI_INVALID_PARAMETER The given parameter is invalid.
  @retval Others                Other erros.

--*/
EFI_STATUS
SourceCodingDecompress (
  IN EFI_SOURCE_CODING_PROTOCOL   *This,
  IN EFI_SOURCE_CODING_IDENTIFIER Identifier,
  IN VOID    *CompressedData,
  IN UINTN   CompressedDataLen,
  OUT VOID   **DecompressedData,
  OUT UINTN  *DecompressedDataLen
)
{
  EFI_STATUS Status;
  EFI_SOURCE_CODING_DECOMPRESS TargetDecompressFunction;

  if (CompressedData == NULL ||
      DecompressedData == NULL ||
      DecompressedDataLen == NULL
      ) {
    return EFI_INVALID_PARAMETER;
  }
  Status = FindSourceCodingInstance (Identifier, &TargetDecompressFunction, NULL);
  if (Status == EFI_NOT_FOUND || TargetDecompressFunction == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No decompress function for %s source coding", __FUNCTION__, Identifier));
    return EFI_UNSUPPORTED;
  }
  Status = TargetDecompressFunction (
              This,
              Identifier,
              CompressedData,
              CompressedDataLen,
              DecompressedData,
              DecompressedDataLen
           );
  return Status;
}

/**
  Compress function provided by this source coding instance

  @param[in]    This                 This is the EFI_SOURCE_CODING_PROTOCOL instance.
  @param[in]    Identifier           identifier of source coding.
  @param[in]    SourceData           Pointer to the uncompressed data.
  @param[in]    SourceDataLen        Length of uncompressed data.
  @param[in]    CompressedData       Pointer to receive the pointer to compressed data.
                                     Caller has to free the memory allocated for this.
  @param[in]    CompressedDataLen    Pointer to receive compressed data length.

  @retval EFI_SUCCESS           Register successfully.
  @retval EFI_UNSUPPORTED       No source coding instance to decompress the source data.
  @retval EFI_INVALID_PARAMETER The given parameter is invalid.
  @retval Others                Other erros.

--*/
EFI_STATUS
SourceCodingCompress (
  IN EFI_SOURCE_CODING_PROTOCOL   *This,
  IN EFI_SOURCE_CODING_IDENTIFIER Identifier,
  IN VOID    *SourceData,
  IN UINTN   SourceDataLen,
  OUT VOID   **CompressedData,
  OUT UINTN  *CompressedDataLen
)
{
  EFI_STATUS Status;
  EFI_SOURCE_CODING_COMPRESS TargetCompressFunction;

  if (SourceData == NULL ||
      SourceDataLen == 0 ||
      CompressedData == NULL ||
      CompressedDataLen == NULL
      ) {
    return EFI_INVALID_PARAMETER;
  }
  Status = FindSourceCodingInstance (Identifier, NULL, &TargetCompressFunction);
  if (Status == EFI_UNSUPPORTED || TargetCompressFunction == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No compress function for %s source coding", __FUNCTION__, Identifier));
    return EFI_UNSUPPORTED;
  }
  Status = TargetCompressFunction (
              This,
              Identifier,
              SourceData,
              SourceDataLen,
              CompressedData,
              CompressedDataLen
           );
  return Status;
}

EFI_SOURCE_CODING_PROTOCOL mSourceCodingProtocol = {
  SourceCodingRegister,
  SourceCodingSupported,
  SourceCodingCompress,
  SourceCodingDecompress
};

/**
  This is the declaration of an EFI image entry point.

  @param  ImageHandle           The firmware allocated handle for the UEFI image.
  @param  SystemTable           A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.
**/
EFI_STATUS
EFIAPI
SourceCodingEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  InitializeListHead (&mSourceCodingeList);
  //
  // Install the Restful Resource Interpreter Protocol.
  //
  mProtocolHandle = NULL;
  Status = gBS->InstallProtocolInterface (
                  &mProtocolHandle,
                  &gEfiSourceCodingProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  (VOID *)&mSourceCodingProtocol
                  );
  return Status;
}

/**
  This is the unload handle of source codinge instance.

  @param[in] ImageHandle           The drivers' driver image.

  @retval    EFI_SUCCESS           The image is unloaded.
  @retval    Others                Failed to unload the image.

**/
EFI_STATUS
EFIAPI
SourceCodingUnload (
  IN EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS Status;
  SOURCE_CODING_INSTANCE *Instance;
  SOURCE_CODING_INSTANCE *NextInstance;

  if (IsListEmpty (&mSourceCodingeList)) {
    return EFI_SUCCESS;
  }
  //
  // Free memory of SOURCE_CODING_INSTANCE instance.
  //
  Instance = (SOURCE_CODING_INSTANCE *)GetFirstNode (&mSourceCodingeList);
  do {
    NextInstance = NULL;
    if (!IsNodeAtEnd(&mSourceCodingeList, &Instance->NextSourceCodingInstance)) {
      NextInstance = (SOURCE_CODING_INSTANCE *)GetNextNode (&mSourceCodingeList, &Instance->NextSourceCodingInstance);
    }
    FreePool ((VOID *)Instance->MethodIdentifer);
    FreePool ((VOID *)Instance);
    Instance = NextInstance;
  } while (Instance != NULL);

  Status = gBS->UninstallProtocolInterface (
                  mProtocolHandle,
                  &gEfiSourceCodingProtocolGuid,
                  (VOID *)&mSourceCodingProtocol
                  );
  return EFI_SUCCESS;
}
