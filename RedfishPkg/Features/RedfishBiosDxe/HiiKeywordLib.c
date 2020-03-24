/** @file
  Internal Functions for x-uefi keyword operations.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "HiiKeywordLib.h"

/**
  The function is used to construct the KeywordRequest String.

  @param[in]   DevicePath         The Device path related to specified controller.
  @param[in]   Keyword            The Keyword name.
  @param[out]  KeywordString      A null-terminated string in <MultiKeywordRequest> format.
                                  Caller takes the responsibility to free memory.

  @retval EFI_SUCCESS             Construct the KeywordRequest String successfully.
  @retval EFI_INVALID_PARAMETER   Any input or configured parameter is invalid.
  @retval EFI_OUT_OF_RESOURCES    Could not allocate needed resources.

**/
EFI_STATUS
KeywordConstructRequest (
  IN     EFI_DEVICE_PATH_PROTOCOL      *DevicePath, OPTIONAL
  IN     CHAR16                        *Keyword,
  OUT    CHAR16                        **KeywordString
)
{
  CHAR16                        *Str;
  CHAR16                        *ReturnString;
  UINT8                         *Buffer;
  UINTN                         DevicePathSize;
  UINTN                         Index;
  UINTN                         Len;

  if (Keyword == NULL || KeywordString == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DevicePathSize = 0;
  ReturnString   = NULL;

  if (DevicePath != NULL) {

    //
    // Compute the size of the device path in bytes
    //
    DevicePathSize = GetDevicePathSize (DevicePath);
    Len = sizeof ("PATH=") + DevicePathSize * 2 + sizeof ("&KEYWORD=") + StrLen (Keyword) - 1;
  } else {
    Len = sizeof ("&KEYWORD=") + StrLen (Keyword);
  }
  Str = AllocateZeroPool (Len * sizeof (CHAR16));
  if (Str == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  *KeywordString = Str;

  if (DevicePath != NULL) {

    StrCpyS (Str, Len, L"PATH=");
    ReturnString = Str;
    Str = Str + StrLen (Str);

    for (Index = 0, Buffer = (UINT8 *)DevicePath; Index < DevicePathSize; Index++) {

      UnicodeValueToStringS (
        Str,
        Len * sizeof (CHAR16) - ((UINTN)Str - (UINTN)ReturnString),
        PREFIX_ZERO | RADIX_HEX,
        *(Buffer++),
        2
        );

      Str += StrnLenS (Str, Len - ((UINTN)Str - (UINTN)ReturnString) / sizeof (CHAR16));
    }
  }

  StrCatS (Str, Len, L"&KEYWORD=");
  Str = Str + sizeof ("&KEYWORD=") - 1;
  CopyMem (Str, Keyword, StrLen (Keyword) * sizeof (CHAR16));
  Str += StrLen (Keyword);

  *Str = L'\0';

  return EFI_SUCCESS;
}

/**
  Get value from the input KeywordResp string.

  @param  StringPtr              String in <MultiKeywordResp> format.
  @param  Value                  The output value. Caller takes the responsibility
                                 to free memory.
  @param  Len                    Length of the <Value>, in characters.

  @retval EFI_SUCCESS            Success to get the keyword value.
  @retval EFI_OUT_OF_RESOURCES   Insufficient resources to store neccessary
                                 structures.
  @retval EFI_INVALID_PARAMETETR Input parameters combination is invalid.

**/
EFI_STATUS
ExtractValue (
  IN EFI_STRING                    StringPtr,
  OUT VOID                         **Value,
  OUT UINTN                        *Len
  )
{
  EFI_STRING               TmpPtr;
  UINTN                    Length;
  EFI_STRING               Str;
  UINT8                    *Buf;
  EFI_STATUS               Status;
  UINT8                    DigitUint8;
  UINTN                    Index;
  CHAR16                   TemStr[2];
  UINTN                    CharNum;

  if (StringPtr == NULL || *StringPtr == L'\0' || Value == NULL || Len == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Buf = NULL;

  TmpPtr = StringPtr;
  while (*StringPtr != L'\0' && *StringPtr != L'&') {
    StringPtr++;
  }
  CharNum = StringPtr - TmpPtr;

  Str = (EFI_STRING) AllocateZeroPool ((CharNum + 1) * sizeof (CHAR16));
  if (Str == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem (Str, TmpPtr, CharNum * sizeof (CHAR16));

  Length = CharNum / 2;

  Buf = (UINT8 *) AllocateZeroPool (Length);
  if (Buf == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  ZeroMem (TemStr, sizeof (TemStr));
  for (Index = 0; Index < CharNum; Index ++) {

    TemStr[0] = Str[CharNum - Index - 1];
    DigitUint8 = (UINT8) StrHexToUint64 (TemStr);

    if ((Index & 1) == 0) {
      Buf [Index/2] = DigitUint8;
    } else {
      Buf [Index/2] = (UINT8) ((DigitUint8 << 4) + Buf [Index/2]);
    }
  }

  *Value = Buf;
  *Len = Length;
  Status  = EFI_SUCCESS;

Exit:
  if (Str != NULL) {
    FreePool (Str);
  }

  return Status;
}


/**
  Get the original Keyword Value.

  This function Get value by the specified keyword. ASSERT() when result format from keyword handler
  protocol GetData is not correct.

  @param[in]   NamespaceStr       The string of x-UEFI namespace.
  @param[in]   DevicePath         The Device path related to specified Keyword instance.
                                  If NULL, will retrive the first instance match to the keyword.
  @param[in]   Keyword            Pointer of the Keyword String.
  @param[out]  Value              Pointer of the Original Value String specified by keyword.Caller
                                  takes the responsibility to free memory.
  @param[out]  Length             Length of the Original Value in Bytes.

  @retval EFI_SUCCESS             The configuration processed successfully.
  @retval EFI_INVALID_PARAMETER   Any input or configured parameter is invalid.
  @retval EFI_NOT_FOUND           The KeywordString or NamespaceId was not found.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
  @retval EFI_ACCESS_DENIED       The action violated system policy.
  @retval EFI_DEVICE_ERROR        An unexpected system error occurred.

**/
EFI_STATUS
KeywordConfigGetValue (
  IN     CHAR16                     *NamespaceStr,
  IN     EFI_DEVICE_PATH_PROTOCOL   *DevicePath, OPTIONAL
  IN     CHAR16                     *Keyword,
  OUT    VOID                       **Value,
  OUT    UINTN                      *Length
  )
{
  EFI_STATUS                      Status;
  EFI_STRING                      Progress;
  EFI_STRING                      Results;
  UINT32                          ProgressErr;
  CHAR16                          *ResultStr;
  CHAR16                          *KeywordString;
  CHAR16                          *KeywordNamespaceStr;

  EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL *HiiKeywordHandler;

  Status = EFI_SUCCESS;
  KeywordString = NULL;

  if (Keyword == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // Locate keyword handler protocol
  //
  Status = gBS->LocateProtocol (&gEfiConfigKeywordHandlerProtocolGuid, NULL, (VOID **) &HiiKeywordHandler);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = KeywordConstructRequest (DevicePath, Keyword, &KeywordString);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  //
  // Build up x-UEFI namespace string
  //
  KeywordNamespaceStr = AllocatePool (
                          StrSize (L"NAMESPACE=") +
                          StrLen (NamespaceStr) * 2
                          );
  if (KeywordNamespaceStr == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  UnicodeSPrint (
    KeywordNamespaceStr,
    StrSize (L"NAMESPACE=") + StrLen (NamespaceStr) * 2,
    L"NAMESPACE=%s",
    NamespaceStr
    );

  Status = HiiKeywordHandler->GetData (
                                HiiKeywordHandler,
                                KeywordNamespaceStr,
                                KeywordString,
                                &Progress,
                                &ProgressErr,
                                &Results
                                );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  ResultStr = StrStr (Results, L"&VALUE=");
  ASSERT (ResultStr != NULL);
  ResultStr += StrLen (L"&VALUE=");

  Status = ExtractValue (ResultStr, Value, Length);

  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

ON_EXIT:

  if (KeywordString != NULL) {
    FreePool (KeywordString);
  }
  return Status;
}

/**
  Update the Keyword value.

  This function update value by the specified keyword. ASSERT() when result format from keyword handler
  protocol GetData is not correct.

  @param[in]   NamespaceStr       X-UEFI Namespace this Attribute associated with.
  @param[in]   DevicePath         The Device path related to specified Keyword instance.
                                  If NULL, will retrive the first instance match to the keyword.
  @param[in]   Keyword            Pointer of the Keyword String.
  @param[in]   Value              Pointer of the New Value String which will be Setted.
  @param[in]   Type               If not NULL, the Keyword value will be processed by the type.
                                  If NULL, will be decided by predefined type.

  @retval EFI_SUCCESS             The configuration processed successfully.
  @retval EFI_INVALID_PARAMETER   Any input or configured parameter is invalid.
  @retval EFI_NOT_FOUND           The KeywordString or NamespaceId was not found.
  @retval EFI_OUT_OF_RESOURCES    Required system resources could not be allocated.
  @retval EFI_ACCESS_DENIED       The action violated system policy.
  @retval EFI_DEVICE_ERROR        An unexpected system error occurred.

**/
EFI_STATUS
KeywordConfigSetValue (
  IN    CHAR16                       *NamespaceStr,
  IN   EFI_DEVICE_PATH_PROTOCOL      *DevicePath, OPTIONAL
  IN   CHAR16                        *Keyword,
  IN   CHAR16                        *Value,
  IN   KEYWORD_TYPE                   Type
  )
{
  EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL  *HiiKeywordHandler;
  EFI_STATUS                           Status;
  EFI_STRING                           Progress;
  EFI_STRING                           Results;
  UINT32                               ProgressErr;
  CHAR16                               *ResultStr;
  CHAR16                               *KeywordString;
  CHAR16                               *TmpPtr;
  UINT8                                *ValuePtr;
  CHAR16                               *KeywordValueResp;
  UINTN                                ResultLen;
  UINTN                                ValueLen;
  UINTN                                Index;
  UINTN                                MaxLen;
  UINT64                               TmpValue;
  CHAR16                               *KeywordNamespaceStr;

  ValuePtr         = NULL;
  KeywordString    = NULL;
  KeywordValueResp = NULL;

  if ((Keyword == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Locate keyword handler protocol
  //
  Status = gBS->LocateProtocol (&gEfiConfigKeywordHandlerProtocolGuid, NULL, (VOID **) &HiiKeywordHandler);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = KeywordConstructRequest (DevicePath, Keyword, &KeywordString);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  //
  // Build up x-UEFI namespace string
  //
  KeywordNamespaceStr = AllocatePool (
                          StrSize (L"NAMESPACE=") +
                          StrLen (NamespaceStr) * 2
                          );
  if (KeywordNamespaceStr == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  UnicodeSPrint (
    KeywordNamespaceStr,
    StrSize (L"NAMESPACE=") + StrLen (NamespaceStr) * 2,
    L"NAMESPACE=%s",
    NamespaceStr
    );
  //
  // 1. Check to see whether system support keyword.
  //
  Status = HiiKeywordHandler->GetData (
                                HiiKeywordHandler,
                                KeywordNamespaceStr,
                                KeywordString,
                                &Progress,
                                &ProgressErr,
                                &Results
                                );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }


  //
  // 2. If system support the keyword, just try to change value.
  //
  ResultStr = StrStr (Results, L"&VALUE=");
  ASSERT (ResultStr != NULL);
  ResultStr += StrLen (L"&VALUE=");

  //
  // Get the Value length in characters.
  //
  TmpPtr = ResultStr;
  while ((*TmpPtr != L'\0') && (*TmpPtr != L'&')) {
    //
    // Reset the value.
    //
    *TmpPtr = L'0';
    TmpPtr++;
  }
  ResultLen = TmpPtr - ResultStr;

  if (Type == KeywordTypeBuffer) {

    ValueLen = StrLen (Value) * sizeof (CHAR16);
    if (ResultLen < ValueLen * 2) {
      DEBUG ((EFI_D_ERROR, "The length of keyword value is invalid.\n"));
      goto ON_EXIT;
    }
    ResultStr += (ResultLen - ValueLen * 2);
    ValuePtr = (UINT8 *)(Value + StrLen (Value)) - 1 ;
  } else {
    ValueLen = ResultLen / 2;
    TmpValue = StrDecimalToUint64 (Value);
    ValuePtr = (UINT8 *)(&TmpValue) + ValueLen - 1;
  }

  MaxLen = ValueLen * 2 + 1;
  KeywordValueResp = AllocateZeroPool (MaxLen * sizeof (CHAR16));
  if (KeywordValueResp == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  TmpPtr = KeywordValueResp;

  for (Index = 0; Index < ValueLen; Index ++, ValuePtr --) {

    UnicodeValueToStringS (
      TmpPtr,
      MaxLen * sizeof (CHAR16) - ((UINTN)TmpPtr - (UINTN)KeywordValueResp),
      PREFIX_ZERO | RADIX_HEX,
      *ValuePtr,
      2
      );

    TmpPtr += StrnLenS (TmpPtr, MaxLen - ((UINTN)TmpPtr - (UINTN)KeywordValueResp) / sizeof (CHAR16));

  }

  CopyMem (ResultStr, KeywordValueResp, ValueLen * sizeof (CHAR16) * 2);

  //
  // 3. Call the keyword handler protocol to change the value.
  //
  Status = HiiKeywordHandler->SetData (
                                HiiKeywordHandler,
                                Results,
                                &Progress,
                                &ProgressErr
                                );

ON_EXIT:

  if (KeywordString != NULL) {
    FreePool (KeywordString);
  }

  if (KeywordValueResp != NULL) {
    FreePool (KeywordValueResp);
  }

  return Status;
}

