/** @file
  Utility Functions for Redfish JSON file and HII configuration converting.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "HiiRedfishJson.h"

HII_REDFISH_PRIVATE_DATA    mHiiRedfishPrivateData;
LIST_ENTRY                  mAttributeNameNodeList;
LIST_ENTRY                  mMenuNameList;

EFI_STATUS
RedfishPlatformGetAttributeValue (
  IN     CONST CHAR8  *AttributeName,
  OUT    VOID         **AttributeValue,
  OUT    UINTN        *Length
  );

/**
  The function is used to tell if an expression is a constant expression.

  An expression can be constant (Equals to a static value), or dynamic (Depends on the result
  of other expressions). Constant expression includes: EFI_IFR_FALSE_OP, EFI_IFR_ONE_OP,
  EFI_IFR_ONES_OP, EFI_IFR_TRUE_OP, EFI_IFR_UINT8_OP, EFI_IFR_UINT16_OP, EFI_IFR_UINT32_OP,
  EFI_IFR_UINT64_OP, EFI_IFR_VERSION_OP, EFI_IFR_ZERO_OP.

  @param[in]          ExpDes                  The expression to evaluate

  @retval  TRUE       This expression is a constant expression.
  @retval  FALSE      This expression is not a constant expression.

**/
BOOLEAN
IsConstantExpression (
  IN HII_DEPENDENCY_EXPRESSION    *ExpDes
  )
{
  switch (*((UINT8*) ExpDes)) {
    case EFI_IFR_FALSE_OP:
    case EFI_IFR_ONE_OP:
    case EFI_IFR_ONES_OP:
    case EFI_IFR_TRUE_OP:
    case EFI_IFR_UINT8_OP:
    case EFI_IFR_UINT16_OP:
    case EFI_IFR_UINT32_OP:
    case EFI_IFR_UINT64_OP:
    case EFI_IFR_VERSION_OP:
    case EFI_IFR_ZERO_OP:
      return TRUE;

    default:
      return FALSE;
  }
}

/**
  Get the next supported language in the given language list. The language list must be an Ascii
  string with NULL terminator, each different language is seperated by ';', like "language1; language2".

  @param[in]          SupportedLanguages          The supported language list
  @param[in,out]      EndPointer                  As the input, indicates the current language
                                                  to search from. As the output, indicates the
                                                  language to search from next time.

  @return the next supported language or NULL when it is not found.

**/
CHAR8*
GetNextSupportedLanguage (
  IN     CHAR8    *SupportedLanguages,
  IN OUT CHAR8    **EndPointer    OPTIONAL
  )
{
  CHAR8    *Ptr;
  CHAR8    *Buf;
  UINTN    BufLen;

  if (EndPointer == NULL) {
    return NULL;
  }

  while (**EndPointer == ';') {
    (*EndPointer) ++;
  }

  Ptr = *EndPointer;
  if (*Ptr == '\0') {
    return NULL;
  }

  // Find the end of the language
  while (*Ptr != ';' && *Ptr != '\0') {
    Ptr ++;
  }

  BufLen = Ptr - *EndPointer + 1;
  Buf = AllocateCopyPool (BufLen, *EndPointer);
  if (Buf == NULL) {
    return NULL;
  }
  Buf[BufLen - 1] = '\0';
  *EndPointer = Ptr;

  return Buf;
}

/**
  The function is used to tell if the given character is valid for attribute name.

  Only characters: '0'-'9', 'A'-'Z', 'a'-'z' and '_' are allowed to use in attribute name.

  @param[in]          Char                The character to evaluate

  @retval  TRUE       This character is valid for attribute name.
  @retval  FALSE      This character is not valid for attribute name.

**/
BOOLEAN
IsCharValidInAttributeName (
  IN CHAR8    Char
  )
{
  if ((Char >= '0' && Char <= '9') ||
      (Char >= 'A' && Char <= 'Z') ||
      (Char >= 'a' && Char <= 'z') ||
      Char == '_') {
    return FALSE;
  }

  return TRUE;
}

/**
  Query the element size for a certain buffer type for a HII_STATEMENT_VALUE.

  This function only supports 4 kinds of buffer type: EFI_IFR_TYPE_NUM_SIZE_8, EFI_IFR_TYPE_NUM_SIZE_16,
  EFI_IFR_TYPE_NUM_SIZE_32 and EFI_IFR_TYPE_NUM_SIZE_64.

  Other kinds of buffer type will return 0.

  @param[in]       BufferValueType                The buffer type for this element to query.

  @retval  Return the needed element size for the given buffer type.

**/
UINT8
GetElementSizeByBufferType (
  IN UINT8    BufferValueType
  )
{
  switch (BufferValueType) {
  case EFI_IFR_TYPE_NUM_SIZE_8:
    return 1;

  case EFI_IFR_TYPE_NUM_SIZE_16:
    return 2;

  case EFI_IFR_TYPE_NUM_SIZE_32:
    return 3;

  case EFI_IFR_TYPE_NUM_SIZE_64:
    return 4;

  default:
    return 0;
  }
}

/**
  The function is used to formalize an attribute name and remove invalid characters.

  Only characters: '0'-'9', 'A'-'Z', 'a'-'z' and '_' are allowed to use in attribute name.

  @param[in]          String                The attribute name string to formalize

**/
VOID
FomalizeAttributeName (
  IN   CHAR8      *String
  )
{
  UINT32      Index;
  UINT32      Offset;

  Index = 0;
  Offset = 0;

  while (String[Index] != '\0') {
    if (!IsCharValidInAttributeName (String[Index])) {
      String[Offset] = String[Index];
      Offset++;
    }
    Index++;
  }

  String[Offset] = '\0';
}

/**
  Get the date structure from a date represented string. Date represented string is a NULL terminator
  Ascii string, and format as: "yy/mm/dd".

  @param[in]       DateStr                  The input date represented string
  @param[out]      Date                     The retrieved date structure

  @retval  EFI_SUCCESS                      Get the date successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.

**/
EFI_STATUS
GetDateByString  (
  IN   CHAR8           *DateStr,
  OUT  EFI_HII_DATE    *Date
  )
{
  CHAR8           *StartPtr;
  CHAR8           *EndPtr;
  CHAR8           TempStr[MAX_SIZE_FOR_REDFISH_DATE];
  EFI_HII_DATE    TempDate;

  if (DateStr == NULL || Date == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  StartPtr = DateStr;
  EndPtr   = StartPtr;
  while (*EndPtr != '/') {
    if (*EndPtr == '\0') {
      return EFI_INVALID_PARAMETER;
    }
    EndPtr ++;
  }
  AsciiStrnCpyS (TempStr, MAX_SIZE_FOR_REDFISH_DATE, StartPtr, EndPtr - StartPtr);
  TempStr[EndPtr - StartPtr] = '\0';
  TempDate.Month = (UINT8) AsciiStrDecimalToUintn (TempStr);

  StartPtr = EndPtr + 1;
  EndPtr   = StartPtr;
  while (*EndPtr != '/') {
    if (*EndPtr == '\0') {
      return EFI_INVALID_PARAMETER;
    }
    EndPtr ++;
  }
  AsciiStrnCpyS (TempStr, MAX_SIZE_FOR_REDFISH_DATE, StartPtr, EndPtr - StartPtr);
  TempStr[EndPtr - StartPtr] = '\0';
  TempDate.Day = (UINT8) AsciiStrDecimalToUintn (TempStr);

  StartPtr = EndPtr + 1;
  EndPtr   = StartPtr;
  while (*EndPtr != '\0') {
    EndPtr ++;
  }
  AsciiStrnCpyS (TempStr, MAX_SIZE_FOR_REDFISH_DATE, StartPtr, EndPtr - StartPtr);
  TempStr[EndPtr - StartPtr] = '\0';
  TempDate.Year = (UINT16) AsciiStrDecimalToUintn (TempStr);

  CopyMem (Date, &TempDate, sizeof (EFI_HII_DATE));
  return EFI_SUCCESS;
}


/**
  Get the time structure from a time represented string. Time represented string is a NULL terminator
  Ascii string, and format as: "hh:mm:ss".

  @param[in]       TimeStr                  The input time represented string
  @param[out]      Time                     The retrieved time structure

  @retval  EFI_SUCCESS                      Get the time successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.

**/
EFI_STATUS
GetTimeByString  (
  IN        CHAR8           *TimeStr,
     OUT    EFI_HII_TIME    *Time
  )
{
  CHAR8           *StartPtr;
  CHAR8           *EndPtr;
  CHAR8           TempStr[MAX_SIZE_FOR_REDFISH_TIME];
  EFI_HII_TIME    TempTime;

  if (TimeStr == NULL || Time == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  StartPtr = TimeStr;
  EndPtr   = StartPtr;
  while (*EndPtr != ':') {
    if (*EndPtr == '\0') {
      return EFI_INVALID_PARAMETER;
    }
    EndPtr ++;
  }
  AsciiStrnCpyS (TempStr, MAX_SIZE_FOR_REDFISH_TIME, StartPtr, EndPtr - StartPtr);
  TempStr[EndPtr - StartPtr] = '\0';
  TempTime.Hour= (UINT8) AsciiStrDecimalToUintn (TempStr);

  StartPtr = EndPtr + 1;
  EndPtr   = StartPtr;
  while (*EndPtr != ':') {
    if (*EndPtr == '\0') {
      return EFI_INVALID_PARAMETER;
    }
    EndPtr ++;
  }
  AsciiStrnCpyS (TempStr, MAX_SIZE_FOR_REDFISH_TIME, StartPtr, EndPtr - StartPtr);
  TempStr[EndPtr - StartPtr] = '\0';
  TempTime.Minute= (UINT8) AsciiStrDecimalToUintn (TempStr);

  StartPtr = EndPtr + 1;
  EndPtr   = StartPtr;
  while (*EndPtr != '\0') {
    EndPtr ++;
  }
  AsciiStrnCpyS (TempStr, MAX_SIZE_FOR_REDFISH_TIME, StartPtr, EndPtr - StartPtr);
  TempStr[EndPtr - StartPtr] = '\0';
  TempTime.Second= (UINT8) AsciiStrDecimalToUintn (TempStr);

  CopyMem (Time, &TempTime, sizeof (EFI_HII_TIME));
  return EFI_SUCCESS;
}

/**
  Get the order list buffer from a order list represented string. Order list represented string is a NULL
  terminator Ascii string, and format as: "{xx,xx,xx}".

  Only number data type is allowed for order list buffer, such as: EFI_IFR_TYPE_NUM_SIZE_8,
  EFI_IFR_TYPE_NUM_SIZE_16, EFI_IFR_TYPE_NUM_SIZE_32, EFI_IFR_TYPE_NUM_SIZE_64.

  @param[in]       OrderListStr             The input order list represented string
  @param[in]       BufferValueType          The data type of order list element
  @param[in]       BufferMaxSize            The maximum buffer size for order list
  @param[out]      BufferValue              The retrieved order list buffer
  @param[out]      BufferLen                The retrieved order list buffer length

  @retval  EFI_SUCCESS                      Get the order list successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.

**/
EFI_STATUS
GetOrderListByString  (
  IN      CHAR8     *OrderListStr,
  IN      UINT8     BufferValueType,
  IN      UINT16    BufferMaxSize,
     OUT  UINT8     *BufferValue,
     OUT  UINT16    *BufferLen
  )
{
  CHAR8      *StartPtr;
  CHAR8      *EndPtr;
  CHAR8      TempStr[MAX_SIZE_FOR_REDFISH_ORDERED_LIST];
  UINT8      TempValueUint8;
  UINT16     TempValueUint16;
  UINT32     TempValueUint32;
  UINT64     TempValueUint64;
  UINT8      BufferIndex;
  UINT8      *TempBuffer;
  UINT16     TempCount;
  BOOLEAN    IsEnd;

  if (OrderListStr == NULL || OrderListStr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  StartPtr    = OrderListStr;
  BufferIndex = 0;

  if (*StartPtr != '{') {
    return EFI_INVALID_PARAMETER;
  }

  StartPtr ++;
  EndPtr     = StartPtr;

  IsEnd      = FALSE;
  TempBuffer = AllocateZeroPool (MAX_SIZE_FOR_REDFISH_ORDERED_LIST * 4);
  if (TempBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TempCount  = *BufferLen;
  *BufferLen = 0;
  while (*EndPtr != '\0' && !IsEnd) {

    if (*EndPtr == ',' || *EndPtr == '}') {
      AsciiStrnCpyS (TempStr, MAX_SIZE_FOR_REDFISH_ORDERED_LIST, StartPtr, EndPtr - StartPtr);
      TempStr[EndPtr - StartPtr] = '\0';
      TempValueUint64 = AsciiStrDecimalToUint64 (TempStr);

      switch (BufferValueType) {
      case EFI_IFR_TYPE_NUM_SIZE_8:

        TempValueUint8 = (UINT8) TempValueUint64;
        CopyMem (TempBuffer + BufferIndex, &TempValueUint8, sizeof (UINT8));
        *BufferLen += 1;

        break;

      case EFI_IFR_TYPE_NUM_SIZE_16:

        TempValueUint16 = (UINT16) TempValueUint64;
        CopyMem (((UINT16 *) TempBuffer) + BufferIndex, &TempValueUint16, sizeof (UINT16));
        *BufferLen += 2;

        break;

      case EFI_IFR_TYPE_NUM_SIZE_32:

        TempValueUint32 = (UINT32) TempValueUint64;
        CopyMem (((UINT32 *) TempBuffer) + BufferIndex, &TempValueUint32, sizeof (UINT32));
        *BufferLen += 4;

        break;

      case EFI_IFR_TYPE_NUM_SIZE_64:

        CopyMem (((UINT64 *) TempBuffer) + BufferIndex, &TempValueUint64, sizeof (UINT64));
        *BufferLen += 8;

        break;
      }

      BufferIndex ++;
      StartPtr = EndPtr + 1;
    }

    if (*EndPtr == '}') {
      IsEnd = TRUE;
    }

    EndPtr ++;
  }

  if (IsEnd && *BufferLen <= BufferMaxSize) {

    CopyMem (BufferValue, TempBuffer, *BufferLen);
    FreePool (TempBuffer);
    return EFI_SUCCESS;
  }

  FreePool (TempBuffer);
  *BufferLen = TempCount;
  return EFI_INVALID_PARAMETER;
}

/**
  Get the name node by question id and formset guid from system name node list.

  Name node list is the mapping relationship between an attribute and its' question, and is recorded in a
  global list.

  @param[in]       FormSet                  The formset of this question
  @param[in]       NameNodeListHead         The name node list to query
  @param[in]       QuestionId               The question id of this question

  @return the retrieved name node or NULL if it is not found.

**/
ATTRIBUTE_NAME_NODE*
GetNameNodeByQuestionId (
  IN  HII_FORMSET        *FormSet,
  IN  LIST_ENTRY         *NameNodeListHead,
  IN  EFI_QUESTION_ID    QuestionId
  )
{
  ATTRIBUTE_NAME_NODE    *CurrentNameNode;
  LIST_ENTRY             *NameNodeListLink;

  NameNodeListLink = GetFirstNode (NameNodeListHead);
  while (!IsNull (NameNodeListHead, NameNodeListLink)) {

    CurrentNameNode  = ATTRIBUTE_NAME_NODE_FROM_LINK (NameNodeListLink);
    NameNodeListLink = GetNextNode (NameNodeListHead, NameNodeListLink);

    if (CompareGuid (&FormSet->Guid, &CurrentNameNode->FormSetGuid) == TRUE &&
      QuestionId == CurrentNameNode->QuestionId) {

      return CurrentNameNode;
    }
  }

  return NULL;
}

/**
  The mapping relationship between an attribute and its' question needs to be recorded in a global
  list. This function is used to add a node for a new pair of attribute name and question.

  A question can be uniquely identified by formset guid and question id.

  @param[in]       FormSetGuid              The formset guid for this question's formset
  @param[in]       QuestionId               The question id for this question
  @param[in]      AttributeName            The attribute name to be recorded

  @retval  EFI_SUCCESS                      The attribute name node has been added to global list.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES             System has no other memory to allocate.

**/
EFI_STATUS
AppendNameNodeList (
  IN EFI_GUID           *FormSetGuid,
  IN EFI_QUESTION_ID    QuestionId,
  IN CHAR8              *AttributeName
  )
{
  ATTRIBUTE_NAME_NODE    *AttributeNameNode;

  if (AttributeName == NULL || QuestionId == 0 || IsZeroGuid (FormSetGuid)) {
    return EFI_INVALID_PARAMETER;
  }

  AttributeNameNode = AllocateZeroPool (sizeof (ATTRIBUTE_NAME_NODE));
  if (AttributeNameNode == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  AttributeNameNode->Signature     = ATTRIBUTE_NAME_NODE_SIGNATURE;
  AttributeNameNode->QuestionId    = QuestionId;
  AttributeNameNode->AttributeName = AllocateCopyPool (AsciiStrLen (AttributeName) + 1, AttributeName);
  if (AttributeNameNode->AttributeName == NULL) {

    FreePool (AttributeNameNode);
    return EFI_OUT_OF_RESOURCES;
  }
  CopyGuid (&AttributeNameNode->FormSetGuid, FormSetGuid);

  InsertTailList (&mAttributeNameNodeList, &AttributeNameNode->Link);

  return EFI_SUCCESS;
}

/**
  Add a name space id to the formset name space list. The formset keeps a list for normal language name
  spaces and a list for keyword name spaces.

  The keyword name space starts with the prefix: "x-UEFI-".

  @param[in]       FormSet                  The formset to add this name space
  @param[in]       NamespaceId              The name space to be added

  @retval  EFI_SUCCESS                      The name space id has been added to formset successfully.
  @retval  EFI_OUT_OF_RESOURCES             System has no other memory to allocate.

**/
EFI_STATUS
AddNamespaceIdToFormSet (
  IN REDFISH_FORMSET    *FormSet,
  IN CHAR8              *NamespaceId
  )
{
  NAMESPACE_DATA                *NamespaceData;
  CHAR8                         *KeywordPre;

  NamespaceData = AllocateZeroPool (sizeof (NAMESPACE_DATA));
  if (NamespaceData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  NamespaceData->Signature = NAMESPACE_DATA_SIGNATURE;
  NamespaceData->NamespaceId = AllocateCopyPool (AsciiStrSize (NamespaceId), NamespaceId);
  if (NamespaceData->NamespaceId == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  KeywordPre = AsciiStrStr (NamespaceId, "x-UEFI-");

  if (KeywordPre != NULL) {
    InsertTailList (&FormSet->KeywordNamespaceList, &NamespaceData->Link);
  } else {
    InsertTailList (&FormSet->NormalNamespaceList, &NamespaceData->Link);
  }

  return EFI_SUCCESS;
}

/**
  Generate attribute name string for an attribute.

  Attribute name is unique for each question and consists by <NameSpace + Device Alias + Question Id>
  if it is a non-keyword support attribute or <NameSpace + Device Alias + Keyword> if it has keyword
  support.

  @param[in]       NamespaceId              The input name space id
  @param[in]       DevicePathAlias          The input device path alias
  @param[in]       Identifier               The input question id or keyword as an identifier

  @return the generated attribute name or NULL when error occurs. Caller is responsible to free this string.

**/
CHAR8 *
GenerateAttributeName (
  IN CHAR8     *NamespaceId,
  IN CHAR8     *DevicePathAlias,
  IN CHAR16    *Identifier
  )
{
  UINTN    BufLen;
  CHAR8    *Buf;
  UINTN    Index;

  if (NamespaceId == NULL || DevicePathAlias == NULL || Identifier == NULL) {
    return NULL;
  }

  BufLen = AsciiStrLen (NamespaceId) + AsciiStrLen (DevicePathAlias) + StrLen (Identifier) + 3;
  Buf = AllocatePool (BufLen);
  if (Buf == NULL) {
    return NULL;
  }

  AsciiStrCpyS (Buf, BufLen, NamespaceId);
  Index = AsciiStrLen (NamespaceId);

  Buf[Index] = '_';
  Index++;

  AsciiStrCpyS (Buf + Index, BufLen - Index, DevicePathAlias);
  Index += AsciiStrLen (DevicePathAlias);

  Buf[Index] = '_';
  Index++;

  UnicodeStrToAsciiStrS (Identifier, Buf + Index, BufLen - Index);
  Index += StrLen (Identifier);
  Buf[Index] = '\0';

  FomalizeAttributeName (Buf);
  return Buf;
}

/**
  Generate Device Path Alias for a Device Path string. Each Device Path has an unique Alias as
  the format "Dev####". "####" represents the 4 hexadecimal digits device index in system, like
  "0001" or "001F". The length for this alias is fixed as 8 characters.

  @param[in]        DevicePathStr       The Device Path string to generate Alias.
  @param[out]       AliasBuf            The buffer to contain Alias.
  @param[in]        AliasBufSize        The buffer size to contain Alias.

  @retval  EFI_SUCCESS                  This Bool Statement has been converted successfully.
  @retval  EFI_INVALID_PARAMETER        One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES         There are no enough Memory.
  @retval  Others                       Another unexpected error occured.

**/
EFI_STATUS
GenerateDevAlias (
  IN     CHAR16    *DevicePathStr,
     OUT CHAR8     *AliasBuf,
  IN     UINT32    AliasBufSize
  )
{
  EFI_STATUS    Status;
  UINTN         Index;
  CHAR16        **NewAliasTable;
  CHAR16        VarName[DEVICE_PATH_ALIAS_SIZE];

  if (DevicePathStr == NULL || AliasBuf == NULL || AliasBufSize != DEVICE_PATH_ALIAS_SIZE) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check if it's a known device
  //
  for (Index = 0; Index < mHiiRedfishPrivateData.AliasNum; Index++) {
    if (StrCmp (DevicePathStr, mHiiRedfishPrivateData.AliasTable[Index]) == 0) {
      AsciiSPrint (AliasBuf, AliasBufSize, "Dev%04x", Index);
      return EFI_SUCCESS;
    }
  }

  //
  // Not found, assign a new alias and save it
  //
  if (mHiiRedfishPrivateData.AliasNum == 0xFFFF) {
    return EFI_OUT_OF_RESOURCES;
  }
  AsciiSPrint (AliasBuf, AliasBufSize, "Dev%04x", mHiiRedfishPrivateData.AliasNum);
  NewAliasTable = AllocateZeroPool ((mHiiRedfishPrivateData.AliasNum + 1) * sizeof (CHAR16**));
  if (NewAliasTable == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem (NewAliasTable, mHiiRedfishPrivateData.AliasTable, (mHiiRedfishPrivateData.AliasNum * sizeof (CHAR16**)));
  NewAliasTable[mHiiRedfishPrivateData.AliasNum] = AllocateCopyPool (StrSize (DevicePathStr), DevicePathStr);
  if (NewAliasTable[mHiiRedfishPrivateData.AliasNum] == NULL) {
    FreePool (NewAliasTable);
    return EFI_OUT_OF_RESOURCES;
  }
  if (mHiiRedfishPrivateData.AliasTable != NULL) {
    FreePool (mHiiRedfishPrivateData.AliasTable);
  }
  mHiiRedfishPrivateData.AliasNum ++;
  mHiiRedfishPrivateData.AliasTable = NewAliasTable;

  Status = gRT->SetVariable (
                DEVICE_PATH_ALIAS_NUM_VAR_NAME,
                &gEfiCallerIdGuid,
                EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                sizeof (UINT32),
                &mHiiRedfishPrivateData.AliasNum
                );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  AsciiStrToUnicodeStrS (AliasBuf, VarName, DEVICE_PATH_ALIAS_SIZE);
  Status = gRT->SetVariable (
                  VarName,
                  &gEfiCallerIdGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  StrSize (DevicePathStr),
                  DevicePathStr
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}


/**
  This function is used to retrieve device alias for a non-keyword support attribute name.

  Attribute name is unique for each question and consists by <NameSpace + Device Alias + Question Id>
  if it is a non-keyword support attribute or <NameSpace + Device Alias + Keyword> if it has keyword
  support.

  Device alias is fixed format string as: Dev####, "####" represents the 4 hexadecimal digits device
  index in system, like "0001" or "001F". The length for this alias is fixed as 8 characters.

  @param[in]       AttributeName                    The attribute name contains device alias information.
  @param[out]      DevAlias                         The retrieved device alias information.

  @retval  EFI_SUCCESS                              This device alias has been retrieved successfully.
  @retval  EFI_INVALID_PARAMETER                    One or more parameters are invalid.

**/
EFI_STATUS
GetDevAliasByAttributeName (
  IN     CHAR8    *AttributeName,
  OUT    CHAR8    *DevAlias
  )
{
  CHAR8    *AsciiStrPtr;

  if (DevAlias == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  AsciiStrPtr = AsciiStrStr (AttributeName, "Dev");
  if (AsciiStrPtr == NULL) {
    return EFI_NOT_FOUND;
  }

  CopyMem (DevAlias, AsciiStrPtr, DEVICE_PATH_ALIAS_SIZE);
  *(DevAlias + (DEVICE_PATH_ALIAS_SIZE - 1)) = '\0';

  return EFI_SUCCESS;
}

/**
  This function is used to retrieve question id for a non-keyword support attribute name.

  Attribute name is unique for each question and consists by <NameSpace + Device Alias + Question Id>
  if it is a non-keyword support attribute or <NameSpace + Device Alias + Keyword> if it has keyword
  support.

  @param[in]       AttributeName                    The attribute name contains question id information.
  @param[out]      QuestionId                       The retrieved question id information.

  @retval  EFI_SUCCESS                              This question id has been retrieved successfully.
  @retval  EFI_INVALID_PARAMETER                    One or more parameters are invalid.

**/
EFI_STATUS
GetQuestionIdByAttributeName (
  IN     CHAR8              *AttributeName,
  OUT    EFI_QUESTION_ID    *QuestionId
  )
{
  CHAR8              *AsciiStrPtr;
  EFI_QUESTION_ID    TempId;

  AsciiStrPtr = AsciiStrStr (AttributeName, "QuenstionId");
  if (AsciiStrPtr == NULL || QuestionId == NULL) {
    return EFI_NOT_FOUND;
  }

  AsciiStrPtr = AsciiStrPtr + AsciiStrLen ("QuenstionId");
  TempId = (EFI_QUESTION_ID) AsciiStrDecimalToUintn (AsciiStrPtr);
  if (TempId == 0) {
    return EFI_INVALID_PARAMETER;
  }

  *QuestionId = TempId;
  return EFI_SUCCESS;
}

/**
  Set attribute name for a given atttibute.

  Attribute name is unique for each question and consists by <NameSpace + Device Alias + Question Id>
  if it is a non-keyword support attribute or <NameSpace + Device Alias + Keyword> if it has keyword
  support.

  Three related properties UefiNamespaceId, UefiDevicePath and UefiKeywordName are also set.

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Statement                The question of this attribute.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This attribute name has been set successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  EFI_NOT_FOUND                    Some information for this attribute name are not found.

**/
EFI_STATUS
SetAttributeNameToAttribute (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  EFI_STATUS           Status;
  LIST_ENTRY           *NamespaceLink;
  NAMESPACE_DATA       *Namespace;
  CHAR8                *AttributeName;
  CHAR16               *KeywordName;
  CHAR16               ItemName[MAX_SIZE_QUENSTIONID_STR_LENGTH];
  EDKII_JSON_VALUE     JsonTemp;

  Status        = EFI_SUCCESS;
  KeywordName   = NULL;
  AttributeName = NULL;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (Attribute, EFI_INVALID_PARAMETER);

  if (Statement->IsKeywordSupported) {

    if (IsListEmpty (&FormSet->KeywordNamespaceList)) {
      return EFI_NOT_FOUND;
    }
    NamespaceLink = GetFirstNode (&FormSet->KeywordNamespaceList);
    Namespace     = NAMESPACE_DATA_FROM_LINK(NamespaceLink);

    if (Statement->HiiStatement->Prompt == 0) {
      return EFI_NOT_FOUND;
    }
    KeywordName = HiiGetStringEx (
                    FormSet->HiiFormSet->HiiHandle,
                    Statement->HiiStatement->Prompt,
                    Namespace->NamespaceId,
                    FALSE
                    );
    if (KeywordName == NULL) {
      return EFI_NOT_FOUND;
    }
    AttributeName = GenerateAttributeName (
                      Namespace->NamespaceId,
                      FormSet->DevicePathAlias,
                      KeywordName
                      );
    if (AttributeName == NULL) {

      Status = EFI_NOT_FOUND;
      goto ON_EXIT;
    }
  } else {

    if (IsListEmpty (&FormSet->NormalNamespaceList)) {
      return EFI_NOT_FOUND;
    }
    NamespaceLink = GetFirstNode (&FormSet->NormalNamespaceList);
    Namespace     = NAMESPACE_DATA_FROM_LINK(NamespaceLink);

    UnicodeSPrint (ItemName, sizeof (ItemName), L"QuenstionId%d", Statement->HiiStatement->QuestionId);
    AttributeName = GenerateAttributeName (
                      Namespace->NamespaceId,
                      FormSet->DevicePathAlias,
                      ItemName
                      );
    if (AttributeName == NULL) {
      Status = EFI_NOT_FOUND;
      goto ON_EXIT;
    }
  }

  JsonTemp = JsonValueInitAsciiString (AttributeName);
  Status = JsonObjectSetValue (
             JsonValueGetObject (Attribute),
             "AttributeName",
             JsonTemp
             );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  //
  // Add this attribute name node to global list
  //
  Status = AppendNameNodeList (
             &FormSet->HiiFormSet->Guid,
             Statement->HiiStatement->QuestionId,
             AttributeName
             );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  //
  // Set three related properties UefiNamespaceId, UefiDevicePath and UefiKeywordName
  //
  JsonTemp = JsonValueInitAsciiString (Namespace->NamespaceId);
  JsonObjectSetValue (
    JsonValueGetObject (Attribute),
    "UefiNamespaceId",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonTemp = JsonValueInitUnicodeString (FormSet->DevicePathStr);
  JsonObjectSetValue (
    JsonValueGetObject (Attribute),
    "UefiDevicePath",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  if (KeywordName != NULL) {

    JsonTemp = JsonValueInitUnicodeString (KeywordName);
    JsonObjectSetValue (
      JsonValueGetObject (Attribute),
      "UefiKeywordName",
      JsonValueInitUnicodeString (KeywordName)
      );
    JsonValueFree (JsonTemp);
  }

ON_EXIT:

  if (KeywordName != NULL) {
    FreePool (KeywordName);
  }
  if (AttributeName != NULL) {
    FreePool (AttributeName);
  }

  return Status;
}

/**
  Set a string value to an attribute for the property identified by property name.

  @param[in]       PropertyName             The name of this propery to be set.
  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       StringId                 String id for this string to be set
  @param[in]       Language                 To indicate which language this string belongs to.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      The value of this property has been set successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  EFI_NOT_FOUND                    String can't be found through this string id.

**/
EFI_STATUS
SetStringPropertyToAttribute (
  IN     CHAR8               *PropertyName,
  IN     REDFISH_FORMSET     *FormSet,
  IN     EFI_STRING_ID       StringId,
  IN     CHAR8               *Language    OPTIONAL,
     OUT EDKII_JSON_VALUE    Attribute
  )
{
  EFI_STATUS          Status;
  CHAR16              *String;
  EDKII_JSON_VALUE    JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (Attribute, EFI_INVALID_PARAMETER);

  if (PropertyName == NULL || StringId == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  String = HiiGetStringEx (
             FormSet->HiiFormSet->HiiHandle,
             StringId,
             Language,
             TRUE
             );
  if (String == NULL) {
    return EFI_NOT_FOUND;
  }

  JsonTemp = JsonValueInitUnicodeString (String);
  Status   = JsonObjectSetValue (
               JsonValueGetObject (Attribute),
               PropertyName,
               JsonTemp
               );
  JsonValueFree (JsonTemp);

  FreePool (String);
  return Status;
}

/**
  Set the value to "DisplayName" property for the given attribute JSON value. This value will
  be set to the string of question prompt message in the given name space. If prompt message
  is not found, then set this value to "None".

  @param[in]         FormSet                  The formset for this attribute.
  @param[in]         Statement                The question for this attribute.
  @param[in]         NormalNamespace          The name space for this prompt message.
  @param[out]        Attribute                The attribute JSON value, should be a JSON object.

**/
VOID
SetDisplayNameToAttribute (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_STATEMENT    *Statement,
  IN     NAMESPACE_DATA       *NormalNamespace,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  EFI_STATUS        Status;
  EDKII_JSON_VALUE  JsonTemp;

  if (Statement->HiiStatement->Prompt != 0) {

    Status = SetStringPropertyToAttribute (
               "DisplayName",
               FormSet,
               Statement->HiiStatement->Prompt,
               NormalNamespace->NamespaceId,
               Attribute
               );
    if (!EFI_ERROR (Status)) {
      return;
    }
  }

  JsonTemp = JsonValueInitUnicodeString (L"");
  JsonObjectSetValue (
    JsonValueGetObject (Attribute),
    "DisplayName",
    JsonTemp
    );
  JsonValueFree (JsonTemp);
}

/**
  Set the value to "HelpText" property for the given attribute JSON value. This value will
  be set to the string of question help message in the given name space. If help message
  is not found, then set this value to "None".

  @param[in]       FormSet                  The formset for this attribute.
  @param[in]       Statement                The question for this attribute.
  @param[in]       NormalNamespace          The name space for this help message.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

**/
VOID
SetHelpTextToAttribute (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_STATEMENT    *Statement,
  IN     NAMESPACE_DATA       *NormalNamespace,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  EFI_STATUS        Status;
  EDKII_JSON_VALUE  JsonTemp;

  if (Statement->HiiStatement->Help != 0) {

    Status = SetStringPropertyToAttribute (
               "HelpText",
               FormSet,
               Statement->HiiStatement->Help,
               NormalNamespace->NamespaceId,
               Attribute
               );
    if (!EFI_ERROR (Status)) {
      return;
    }
  }

  JsonTemp = JsonValueInitUnicodeString (L"");
  JsonObjectSetValue (
    JsonValueGetObject (Attribute),
    "HelpText",
    JsonTemp
    );
  JsonValueFree (JsonTemp);
}

/**
  Set the value to "ResetRequired" property for the given attribute JSON value. This value
  will be set to ture if question flag EFI_IFR_FLAG_RESET_REQUIRED is set.

  @param[in]       Statement                The question of this attribute.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

**/
VOID
SetResetRequiredToAttribute (
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  BOOLEAN       IsResetRequired;

  if ((Statement->HiiStatement->QuestionFlags & EFI_IFR_FLAG_RESET_REQUIRED) == EFI_IFR_FLAG_RESET_REQUIRED) {
    IsResetRequired = TRUE;
  } else {
    IsResetRequired = FALSE;
  }

  JsonObjectSetValue (
    JsonValueGetObject (Attribute),
    "ResetRequired",
    JsonValueInitBoolean(IsResetRequired)
    );
}

/**
  Set the value to "ReadOnly" property for the given attribute JSON value. This value
  will be set to ture if question flag EFI_IFR_FLAG_READ_ONLY is set.

  @param[in]       Statement                The question of this attribute.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

**/
VOID
SetReadOnlyToAttribute (
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  BOOLEAN       IsReadOnly;

  if ((Statement->HiiStatement->QuestionFlags & EFI_IFR_FLAG_READ_ONLY) == EFI_IFR_FLAG_READ_ONLY) {
    IsReadOnly = TRUE;
  } else {
    IsReadOnly = FALSE;
  }

  JsonObjectSetValue (
    JsonValueGetObject (Attribute),
    "ReadOnly",
    JsonValueInitBoolean(IsReadOnly)
    );
}

/**
  Set the value to "MenuPath" property for the given attribute JSON value. This value will
  be set to the name of the given form. If it is not set in form, set this value to "None".

  This menum path will also be added into the global menu name list.

  @param[in]       Form                     The form for this Attribute.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

**/
VOID
SetMenuPathToAttribute (
  IN     REDFISH_FORM         *Form,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  MENU_NAME_NODE*     MenuNameNode;
  EDKII_JSON_VALUE    JsonTemp;

  if (Form->RedfishMenuName != NULL &&
    StrLen (Form->RedfishMenuName) != 0 &&
    StrCmp (Form->RedfishMenuName, L"_") != 0) {

    JsonTemp = JsonValueInitUnicodeString (Form->RedfishMenuName);
    JsonObjectSetValue (
      JsonValueGetObject (Attribute),
      "MenuPath",
      JsonTemp
      );
    JsonValueFree (JsonTemp);

    MenuNameNode = AllocateZeroPool (sizeof (MENU_NAME_NODE));
    if (MenuNameNode == NULL) {
      return;
    }
    MenuNameNode->Signature = MENU_NAME_NODE_SIGNATURE;
    StrCpyS (MenuNameNode->MenuName, MAX_SIZE_REDFISH_MENU_LENGTH, Form->RedfishMenuName);

    InsertTailList (&mMenuNameList, &MenuNameNode->Link);

  } else {

    JsonTemp = JsonValueInitUnicodeString (L"");
    JsonObjectSetValue (
      JsonValueGetObject (Attribute),
      "MenuPath",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  }
}

/**
  Set the "Value" property for a order list or an one of JSON value. This property is set by the question
  option list information, and formatted as the pair list <ValueName, ValueDisplayName>, "ValueName"
  is set to the current value of this option, and "ValueDisplayName" is set to the option text message or
  "None".

  @param[in]       FormSet                  The formset for this attribute.
  @param[in]       Statement                The question to set "Value".
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

**/
VOID
SetOptionListToAttribute (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  LIST_ENTRY              *OptionListEntry;
  HII_QUESTION_OPTION     *CurrentOption;
  CHAR8                   ValueNameAsciiStr[MAX_SIZE_U64_NUMBER_STR];
  CHAR16                  *TempStr;
  EDKII_JSON_VALUE        ListOptionArrayValue;
  EDKII_JSON_VALUE        ListOptionObjValue;
  EDKII_JSON_VALUE        JsonTemp;

  if (IsListEmpty (&Statement->HiiStatement->OptionListHead)) {
    return;
  }

  ListOptionArrayValue = JsonValueInitArray ();
  OptionListEntry      = GetFirstNode (&Statement->HiiStatement->OptionListHead);

  while (!IsNull (&Statement->HiiStatement->OptionListHead, OptionListEntry)) {

    CurrentOption   = HII_QUESTION_OPTION_FROM_LINK (OptionListEntry);
    OptionListEntry = GetNextNode (&Statement->HiiStatement->OptionListHead, OptionListEntry);

    ListOptionObjValue = JsonValueInitObject ();

    AsciiSPrint (ValueNameAsciiStr, MAX_SIZE_U64_NUMBER_STR, "%d", CurrentOption->Value.Value.u64);

    JsonTemp = JsonValueInitAsciiString (ValueNameAsciiStr);
    JsonObjectSetValue (
      JsonValueGetObject (ListOptionObjValue),
      "ValueName",
      JsonTemp
      );
    JsonValueFree (JsonTemp);

    TempStr = NULL;
    if (CurrentOption->Text != 0) {
      TempStr = HiiGetStringEx (
                  FormSet->HiiFormSet->HiiHandle,
                  CurrentOption->Text,
                  NULL,
                  TRUE
                  );
    }

    JsonTemp = JsonValueInitUnicodeString (TempStr);
    if (TempStr != NULL) {
      FreePool (TempStr);
    }

    JsonObjectSetValue (
      JsonValueGetObject (ListOptionObjValue),
      "ValueDisplayName",
      JsonTemp
      );
    JsonValueFree (JsonTemp);

    JsonArrayAppendValue (JsonValueGetArray (ListOptionArrayValue), ListOptionObjValue);
    JsonValueFree (ListOptionObjValue);
  }

  JsonObjectSetValue (
    JsonValueGetObject (Attribute),
    "Value",
    ListOptionArrayValue
    );
  JsonValueFree (ListOptionArrayValue);
}


/**
  Initialize an attribute JSON value and set some common properties to this attribute.

  Common Properties are shared between different kinds of properties. Include:
  AttributeName (Mandatory Field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly, ResetRequired and MenuPath.

  @param[in]       FormSet                  The formset for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The question of this attribute.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This attribute has been initialized successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured when settring
                                            attribute name.

**/
EFI_STATUS
SetCommonPropertiesToAttribute (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_FORM         *Form,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  EFI_STATUS           Status;
  LIST_ENTRY           *NamespaceLink;
  NAMESPACE_DATA       *NormalNamespace;

  if (!IsListEmpty (&FormSet->NormalNamespaceList)) {

    NamespaceLink   = GetFirstNode (&FormSet->NormalNamespaceList);
    NormalNamespace = NAMESPACE_DATA_FROM_LINK(NamespaceLink);
  }
  if (NormalNamespace == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;

  //
  // Set AttributeName, it is required for an attribute
  //
  Status = SetAttributeNameToAttribute (FormSet, Statement, Attribute);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Set Properties for DisplayName, HelpText, ReadOnly, ResetRequired and MenuPath
  //
  SetDisplayNameToAttribute (FormSet, Statement, NormalNamespace, Attribute);
  SetHelpTextToAttribute (FormSet, Statement, NormalNamespace, Attribute);
  SetReadOnlyToAttribute (Statement, Attribute);
  SetResetRequiredToAttribute (Statement, Attribute);
  SetMenuPathToAttribute (Form, Attribute);

  return EFI_SUCCESS;
}

/**
  Set the "DefaultValue" property for order list attribute JSON value. This property is set by the order
  list default list information, and formatted as the string "{xx,xx,xx}".

  @param[in]       Statement                The order list question to set "Value".
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

**/
VOID
SetDefaultToAttributeForOrderList (
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  UINT16                  Index;
  UINT16                  Count;
  UINT8                   ElementSize;
  UINT16                  DefaultBufferLen;
  UINT8                   *DefaultBuffer;
  CHAR8                   DefaultBufferStr[MAX_SIZE_FOR_REDFISH_ORDERED_LIST];
  LIST_ENTRY              *DefaultListEntry;
  HII_QUESTION_DEFAULT    *CurrentDefault;
  EDKII_JSON_VALUE        JsonTemp;

  if (IsListEmpty (&Statement->HiiStatement->DefaultListHead)) {
    return;
  }
  DefaultListEntry = GetFirstNode (&Statement->HiiStatement->DefaultListHead);
  CurrentDefault   = HII_QUESTION_DEFAULT_FROM_LINK (DefaultListEntry);

  DefaultBufferLen = CurrentDefault->Value.BufferLen;
  DefaultBuffer    = CurrentDefault->Value.Buffer;
  ElementSize      = GetElementSizeByBufferType (CurrentDefault->Value.BufferValueType);
  Count            = DefaultBufferLen / ElementSize;

  for (Index = 0; Index < Count; Index ++) {

    if (Index == 0) {
      AsciiStrCpyS (DefaultBufferStr, MAX_SIZE_FOR_REDFISH_ORDERED_LIST, "{");
    }
    AsciiSPrint (DefaultBufferStr, MAX_SIZE_FOR_REDFISH_ORDERED_LIST - AsciiStrLen (DefaultBufferStr) - 1,
      "%a%d", DefaultBufferStr, * (DefaultBuffer + Index));

    if (Index == Count - 1) {
      AsciiStrCpyS (DefaultBufferStr + AsciiStrLen (DefaultBufferStr),
        MAX_SIZE_FOR_REDFISH_ORDERED_LIST - AsciiStrLen (DefaultBufferStr) - 1, "}");
    } else {
      AsciiStrCpyS (DefaultBufferStr + AsciiStrLen (DefaultBufferStr),
        MAX_SIZE_FOR_REDFISH_ORDERED_LIST - AsciiStrLen (DefaultBufferStr) - 1, ",");
    }
  }

  JsonTemp = JsonValueInitAsciiString (DefaultBufferStr);
  JsonObjectSetValue (
    JsonValueGetObject (Attribute),
    "DefaultValue",
    JsonTemp
    );
  JsonValueFree (JsonTemp);
}


/**
  Convert an order list question to an attribute JSON value.

  Common properties to be set include:
  AttributeName (Mandatory field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly, ResetRequired and MenuPath.

  Order list question specific properties to be set include:
  Type (String only), DefaultValue (Format as: "{xx,xx,xx}"), Value (Pair list <ValueName, ValueDisplayName>)

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The order list question to convert.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This order list question has been converted successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
OrderListToJson (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_FORM         *Form,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  EFI_STATUS        Status;
  EDKII_JSON_VALUE  JsonTemp;

  if (!JsonValueIsObject (Attribute)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = SetCommonPropertiesToAttribute (FormSet, Form, Statement, Attribute);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString ("String");
  Status   = JsonObjectSetValue (
               JsonValueGetObject (Attribute),
               "Type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SetDefaultToAttributeForOrderList (Statement, Attribute);
  SetOptionListToAttribute (FormSet, Statement, Attribute);

  return EFI_SUCCESS;
}

/**
  Convert an one of question to an attribute JSON value.

  Common properties to be set include:
  AttributeName (Mandatory field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly, ResetRequired and MenuPath.

  One of question specific properties to be set include:
  Type (Enumeration only), DefaultValue (Number), Value (Pair list <ValueName, ValueDisplayName>)

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The one of question to convert.
  @param[out]      Attribute                The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This one of question has been converted successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
OneofToJson (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_FORM         *Form,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     Attribute
  )
{
  EFI_STATUS              Status;
  LIST_ENTRY              *DefaultListEntry;
  HII_QUESTION_DEFAULT    *CurrentDefault;
  EDKII_JSON_VALUE        JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (Attribute, EFI_INVALID_PARAMETER);

  Status = SetCommonPropertiesToAttribute (FormSet, Form, Statement, Attribute);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString ("Enumeration");
  Status   = JsonObjectSetValue (
               JsonValueGetObject (Attribute),
               "Type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!IsListEmpty (&Statement->HiiStatement->DefaultListHead)) {

    DefaultListEntry = GetFirstNode (&Statement->HiiStatement->DefaultListHead);
    CurrentDefault   = HII_QUESTION_DEFAULT_FROM_LINK (DefaultListEntry);

    JsonTemp = JsonValueInitNumber (CurrentDefault->Value.Value.u64);
    JsonObjectSetValue (
      JsonValueGetObject (Attribute),
      "DefaultValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  }

  SetOptionListToAttribute (FormSet, Statement, Attribute);

  return EFI_SUCCESS;
}


/**
  Convert a numeric question to an attribute JSON value.

  Common properties to be set include:
  AttributeName (Mandatory field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly, ResetRequired and MenuPath.

  Numeric question specific properties to be set include:
  Type (Integer only), DefaultValue (Number), UpperBound, LowerBound

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The numeric question to convert.
  @param[out]      AttributeValue           The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This numeric question has been converted successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
NumericToJson (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_FORM         *Form,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     AttributeValue
  )
{
  EFI_STATUS              Status;
  LIST_ENTRY              *DefaultListEntry;
  HII_QUESTION_DEFAULT    *CurrentDefault;
  EDKII_JSON_VALUE        JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (AttributeValue, EFI_INVALID_PARAMETER);

  Status = SetCommonPropertiesToAttribute (FormSet, Form, Statement, AttributeValue);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString ("Integer");
  Status   = JsonObjectSetValue (
               JsonValueGetObject (AttributeValue),
               "Type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitNumber (Statement->HiiStatement->ExtraData.NumData.Maximum);
  JsonObjectSetValue (
    JsonValueGetObject (AttributeValue),
    "UpperBound",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonTemp = JsonValueInitNumber (Statement->HiiStatement->ExtraData.NumData.Minimum);
  JsonObjectSetValue (
    JsonValueGetObject (AttributeValue),
    "LowerBound",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  if (!IsListEmpty (& Statement->HiiStatement->DefaultListHead)) {

    DefaultListEntry = GetFirstNode (&Statement->HiiStatement->DefaultListHead);
    CurrentDefault = HII_QUESTION_DEFAULT_FROM_LINK (DefaultListEntry);

    JsonTemp = JsonValueInitNumber (CurrentDefault->Value.Value.u64);
    JsonObjectSetValue (
      JsonValueGetObject (AttributeValue),
      "DefaultValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  }

  return EFI_SUCCESS;
}


/**
  Convert a date question to an attribute JSON value.

  Common properties to be set include:
  AttributeName (Mandatory field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly, ResetRequired and MenuPath.

  Date question specific properties to be set include:
  Type (String only), DefaultValue (Formatted as "yy/mm/dd")

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The date question to convert.
  @param[out]      AttributeValue           The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This date question has been converted successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
DateToJson (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_FORM         *Form,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     AttributeValue
  )
{
  EFI_STATUS              Status;
  LIST_ENTRY              *DefaultListEntry;
  HII_QUESTION_DEFAULT    *CurrentDefault;
  EFI_HII_DATE            Date;
  CHAR16                  DateStr[MAX_SIZE_FOR_REDFISH_DATE];
  EDKII_JSON_VALUE        JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (AttributeValue, EFI_INVALID_PARAMETER);

  Status = SetCommonPropertiesToAttribute (FormSet, Form, Statement, AttributeValue);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString ("String");
  Status =   JsonObjectSetValue (
               JsonValueGetObject (AttributeValue),
               "Type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!IsListEmpty (&Statement->HiiStatement->DefaultListHead)) {

    DefaultListEntry = GetFirstNode (&Statement->HiiStatement->DefaultListHead);
    CurrentDefault = HII_QUESTION_DEFAULT_FROM_LINK (DefaultListEntry);
    CopyMem (&Date, &CurrentDefault->Value.Value, sizeof (Date));
    UnicodeSPrint (
      DateStr,
      sizeof (CHAR16) * MAX_SIZE_FOR_REDFISH_DATE,
      L"%d/%d/%d",
      Date.Month,
      Date.Day,
      Date.Year
      );

    JsonTemp = JsonValueInitUnicodeString (DateStr);
    JsonObjectSetValue (
      JsonValueGetObject (AttributeValue),
      "DefaultValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  }

  return EFI_SUCCESS;
}

/**
  Convert a time question to an attribute JSON value.

  Common properties to be set include:
  AttributeName (Mandatory field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly, ResetRequired and MenuPath.

  Time question specific properties to be set include:
  Type (String only), DefaultValue (Formatted as "hh:mm:ss")

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The time question to convert.
  @param[out]      AttributeValue           The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This time question has been converted successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
TimeToJson (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_FORM         *Form,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     AttributeValue
  )
{
  EFI_STATUS              Status;
  LIST_ENTRY              *DefaultListEntry;
  HII_QUESTION_DEFAULT    *CurrentDefault;
  EFI_HII_TIME            Time;
  CHAR16                  TimeStr[MAX_SIZE_FOR_REDFISH_TIME];
  EDKII_JSON_VALUE        JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (AttributeValue, EFI_INVALID_PARAMETER);

  Status = SetCommonPropertiesToAttribute (FormSet, Form, Statement, AttributeValue);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString ("String");
  Status   = JsonObjectSetValue (
               JsonValueGetObject (AttributeValue),
               "Type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!IsListEmpty (&Statement->HiiStatement->DefaultListHead)) {

    DefaultListEntry = GetFirstNode (&Statement->HiiStatement->DefaultListHead);
    CurrentDefault = HII_QUESTION_DEFAULT_FROM_LINK (DefaultListEntry);
    CopyMem (&Time, &CurrentDefault->Value.Value, sizeof (Time));
    UnicodeSPrint (
      TimeStr,
      sizeof (CHAR16) * MAX_SIZE_FOR_REDFISH_TIME,
      L"%d:%d:%d",
      Time.Hour,
      Time.Minute,
      Time.Second
      );

    JsonTemp = JsonValueInitUnicodeString (TimeStr);
    JsonObjectSetValue (
      JsonValueGetObject (AttributeValue),
      "DefaultValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  }

  return EFI_SUCCESS;
}

/**
  Convert a string question to an attribute JSON value.

  Common properties to be set include:
  AttributeName (Mandatory field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly, ResetRequired and MenuPath.

  String question specific properties to be set include:
  Type (String only), DefaultValue, MaxLength and MinLength

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The string question to convert.
  @param[out]      AttributeValue           The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This string question has been converted successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
StringToJson (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_FORM         *Form,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     AttributeValue
  )
{
  EFI_STATUS              Status;
  LIST_ENTRY              *DefaultListEntry;
  HII_QUESTION_DEFAULT    *CurrentDefault;
  CHAR16                  *DefaultString;
  EDKII_JSON_VALUE        JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (AttributeValue, EFI_INVALID_PARAMETER);

  Status = SetCommonPropertiesToAttribute (FormSet, Form, Statement, AttributeValue);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString ("String");
  Status   = JsonObjectSetValue (
               JsonValueGetObject (AttributeValue),
               "Type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitNumber (Statement->HiiStatement->ExtraData.StrData.MaxSize);
  JsonObjectSetValue (
    JsonValueGetObject (AttributeValue),
    "MaxLength",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonTemp = JsonValueInitNumber (Statement->HiiStatement->ExtraData.StrData.MinSize);
  JsonObjectSetValue (
    JsonValueGetObject (AttributeValue),
    "MinLength",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  if (!IsListEmpty (&Statement->HiiStatement->DefaultListHead)) {

    DefaultListEntry = GetFirstNode (&Statement->HiiStatement->DefaultListHead);
    CurrentDefault   = HII_QUESTION_DEFAULT_FROM_LINK (DefaultListEntry);

    DefaultString = L"";
    if (CurrentDefault->Value.Value.string != 0) {
      DefaultString = HiiGetString (
                        FormSet->HiiFormSet->HiiHandle,
                        CurrentDefault->Value.Value.string,
                        NULL
                        );
    }

    JsonTemp = JsonValueInitUnicodeString (DefaultString);
    JsonObjectSetValue (
      JsonValueGetObject (AttributeValue),
      "DefaultValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  }

  return EFI_SUCCESS;
}

/**
  Convert a password question to an attribute JSON value.

  Common properties to be set include:
  AttributeName (Mandatory field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly (Always to be TRUE), ResetRequired and MenuPath.

  Password question specific properties to be set include:
  Type (String only), MaxLength and MinLength

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The password question to convert.
  @param[out]      AttributeValue           The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This password question has been converted successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
PasswordToJson (
  IN     REDFISH_FORMSET      *FormSet,
  IN     REDFISH_FORM         *Form,
  IN     REDFISH_STATEMENT    *Statement,
     OUT EDKII_JSON_VALUE     AttributeValue
  )
{
  EFI_STATUS          Status;
  EDKII_JSON_VALUE    JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (AttributeValue, EFI_INVALID_PARAMETER);

  Status = SetCommonPropertiesToAttribute (FormSet, Form, Statement, AttributeValue);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString ("Password");
  Status   = JsonObjectSetValue (
               JsonValueGetObject (AttributeValue),
               "Type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitNumber (Statement->HiiStatement->ExtraData.PwdData.MaxSize);
  JsonObjectSetValue (
    JsonValueGetObject (AttributeValue),
    "MaxLength",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonTemp = JsonValueInitNumber (Statement->HiiStatement->ExtraData.PwdData.MinSize);
  JsonObjectSetValue (
    JsonValueGetObject (AttributeValue),
    "MinLength",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonObjectSetValue (
    JsonValueGetObject (AttributeValue),
    "ReadOnly",
    JsonValueInitBoolean (TRUE)
    );

  return EFI_SUCCESS;
}

/**
  Convert a check box question to an attribute JSON value.

  Common properties to be set include:
  AttributeName (Mandatory field), UefiNamespaceId, UefiDevicePath, UefiKeywordName, DisplayName,
  HelpText, ReadOnly, ResetRequired and MenuPath.

  Check box question specific properties to be set include:
  Type (Boolean only), DefaultValue

  @param[in]       FormSet                  The formSet for this attribute.
  @param[in]       Form                     The form for this attribute.
  @param[in]       Statement                The check box question to convert.
  @param[out]      AttributeValue           The attribute JSON value, should be a JSON object.

  @retval  EFI_SUCCESS                      This check box question has been converted successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
BooleanToJson (
  IN      REDFISH_FORMSET       *FormSet,
  IN      REDFISH_FORM          *Form,
  IN      REDFISH_STATEMENT     *Statement,
     OUT  EDKII_JSON_VALUE      AttributeValue
  )
{
  EFI_STATUS              Status;
  LIST_ENTRY              *DefaultListEntry;
  HII_QUESTION_DEFAULT    *CurrentDefault;
  EDKII_JSON_VALUE        JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (AttributeValue, EFI_INVALID_PARAMETER);

  Status = SetCommonPropertiesToAttribute (FormSet, Form, Statement, AttributeValue);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString ("Boolean");
  Status   = JsonObjectSetValue (
               JsonValueGetObject (AttributeValue),
               "Type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!IsListEmpty (& Statement->HiiStatement->DefaultListHead)) {

    DefaultListEntry = GetFirstNode (&Statement->HiiStatement->DefaultListHead);
    CurrentDefault = HII_QUESTION_DEFAULT_FROM_LINK (DefaultListEntry);

    JsonObjectSetValue (
      JsonValueGetObject (AttributeValue),
      "DefaultValue",
      JsonValueInitBoolean ((BOOLEAN) CurrentDefault->Value.Value.u64)
      );
  }

  return EFI_SUCCESS;
}

/**
  Get the current value from HII database for an attribute represented question through keyword.

  @param[in]       UefiNamespaceIdStr       The string of x-UEFI namespace.
  @param[in]       DevPathStr               The device path string for this question
  @param[in]       KeywordNameStr           The keyword name for this question
  @param[out]      KeywordValue             The question value retrieved from HII database
  @param[out]      Length                   The size needed for this value

  @retval  EFI_SUCCESS                      This value has been retrieved successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES             There are no enough Memory.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
GetAttributeValueFromKeywordConfig (
  IN     CONST CHAR16       *UefiNamespaceIdStr,
  IN     CONST CHAR16       *DevPathStr,
  IN     CONST CHAR16       *KeywordNameStr,
  OUT    VOID               **KeywordValue,
  OUT    UINTN              *Length
  )
{
  EFI_DEVICE_PATH_PROTOCOL *DevicePath;
  EFI_STATUS               Status;
  UINT8                    *Value;
  UINTN                    ValueLen;

  if (DevPathStr == NULL || KeywordNameStr == NULL || KeywordValue == NULL || Length == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Value      = NULL;
  ValueLen   = 0;
  DevicePath = NULL;

  DevicePath = ConvertTextToDevicePath (DevPathStr);
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = KeywordConfigGetValue (
             UefiNamespaceIdStr,
             DevicePath,
             KeywordNameStr,
             &Value,
             &ValueLen
             );

  *KeywordValue = Value;
  *Length = ValueLen;

  FreePool (DevicePath);
  return Status;
}

/**
  Get the current value from HII database for an attribute represented question through attribute
  name.

  Attribute name is unique for each question and consists by <NameSpace + Device Alias + Question Id>
  if it is a non-keyword support attribute.

  @param[in]       AttributeName            The attribute name for this attribute
  @param[out]      AttributeValue           The question value retrieved from HII database
  @param[out]      Length                   The size needed for this value

  @retval  EFI_SUCCESS                      This value has been retrieved successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  EFI_NOT_FOUND                    This question is not found.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
GetAttributeValueFromAttributeName (
  IN     CONST CHAR8    *AttributeName,
  OUT    VOID     **AttributeValue,
  OUT    UINTN    *Length
  )
{
  LIST_ENTRY                 *FormsetListLinkHead;
  LIST_ENTRY                 *FormsetListLink;
  REDFISH_FORMSET            *FormSet;
  LIST_ENTRY                 *FormListLink;
  REDFISH_FORM               *Form;
  LIST_ENTRY                 *StatementLink;
  REDFISH_STATEMENT          *Statement;

  EFI_STRING_ID              AttributeQuestionId;
  CHAR8                      *QuestionIdStr;
  BOOLEAN                    IsValueFound;
  CHAR8                      *AsciiPointer;
  CHAR8                      *DevAlias;
  UINTN                      DevAliasLen;

  UINTN                      ElementCount;
  UINTN                      ElementWidth;
  UINT8                      *Buffer;
  CHAR16                     BufferStr[MAX_SIZE_FOR_REDFISH_ORDERED_LIST];
  UINT16                     Counter;

  EFI_HII_TIME               Time;
  CHAR16                     TimeStr[MAX_SIZE_FOR_REDFISH_TIME];
  EFI_HII_DATE               Date;
  CHAR16                     DateStr[MAX_SIZE_FOR_REDFISH_DATE];

  // get dev alias

  if (AttributeName == NULL || AttributeValue == NULL || Length == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DevAliasLen = 0;
  AsciiPointer = AsciiStrStr (AttributeName, "Dev");
  while (AsciiPointer != NULL && *AsciiPointer != '_' && *AsciiPointer != '\0') {
    AsciiPointer ++;
    DevAliasLen ++;
  }

  if (DevAliasLen == 0) {
    return EFI_NOT_FOUND;
  }

  DevAlias = (CHAR8 *) AllocateZeroPool (DevAliasLen + 1);
  AsciiStrnCpyS (DevAlias, DevAliasLen + 1, AsciiStrStr (AttributeName, "Dev"), DevAliasLen);
  *(DevAlias + DevAliasLen) = '\0';

  QuestionIdStr = AsciiStrStr (AttributeName, "QuenstionId");
  if (QuestionIdStr == NULL) {
    FreePool (DevAlias);
    return EFI_INVALID_PARAMETER;
  }
  AttributeQuestionId = (UINT16) AsciiStrDecimalToUint64(QuestionIdStr + AsciiStrLen("QuenstionId"));

  IsValueFound = FALSE;
  FormsetListLinkHead = &mHiiRedfishPrivateData.RedfishFormSetList;
  FormsetListLink = GetFirstNode (FormsetListLinkHead);
  while (!IsNull (FormsetListLinkHead, FormsetListLink) && !IsValueFound) {

    FormSet = REDFISH_FORMSET_FROM_LINK (FormsetListLink);
    DevAliasLen = AsciiStrLen (DevAlias);
    if (AsciiStrnCmp (DevAlias, FormSet->DevicePathAlias, DevAliasLen) != 0) {
      FormsetListLink = GetNextNode (FormsetListLinkHead, FormsetListLink);
      continue;
    }

    FormListLink = GetFirstNode (&FormSet->RedfishFormList);
    while (!IsNull (&FormSet->RedfishFormList, FormListLink) && !IsValueFound) {

      Form = REDFISH_FORM_FROM_LINK (FormListLink);
      StatementLink = GetFirstNode (&Form->RedfishStatementList);
      while (!IsNull (&Form->RedfishStatementList, StatementLink)) {

        Statement = REDFISH_STATEMENT_FROM_LINK (StatementLink);
        if (Statement->HiiStatement->QuestionId == AttributeQuestionId) {

          IsValueFound = TRUE;
          switch (Statement->HiiStatement->Operand) {

          case EFI_IFR_ONE_OF_OP:
          case EFI_IFR_NUMERIC_OP:

            *Length = (UINTN) Statement->HiiStatement->StorageWidth;
            *AttributeValue = AllocateZeroPool (*Length);
            CopyMem (*AttributeValue, &Statement->HiiStatement->Value.Value, *Length);

            break;

          case EFI_IFR_CHECKBOX_OP:

            *Length = sizeof (UINTN);
            *AttributeValue = AllocateZeroPool (*Length);
            CopyMem (*AttributeValue, &Statement->HiiStatement->Value.Value, *Length);

            break;

          case EFI_IFR_STRING_OP:

            *Length = Statement->HiiStatement->Value.BufferLen + sizeof (CHAR16);
            *AttributeValue = AllocateZeroPool (*Length + sizeof (CHAR16));
            CopyMem (*AttributeValue, Statement->HiiStatement->Value.Buffer, *Length);
            CopyMem (((UINT8*) *AttributeValue) + Statement->HiiStatement->Value.BufferLen, L"\0", sizeof (CHAR16));

            break;

          case EFI_IFR_PASSWORD_OP:

            //
            // For security consideration, there is no value saved in Password statement!
            //
            break;

          case EFI_IFR_DATE_OP:

            CopyMem (&Date, &Statement->HiiStatement->Value.Value, sizeof (Date));
            UnicodeSPrint (
              DateStr,
              sizeof (CHAR16) * MAX_SIZE_FOR_REDFISH_DATE,
              L"%d/%d/%d",
              Date.Month,
              Date.Day,
              Date.Year
              );
            *Length = (StrLen(DateStr) + 1) * sizeof (CHAR16);
            *AttributeValue = AllocateZeroPool (*Length);
            CopyMem (*AttributeValue, DateStr, *Length);

            break;

          case EFI_IFR_TIME_OP:

            CopyMem (&Time, &Statement->HiiStatement->Value.Value, sizeof (Time));
            UnicodeSPrint (
              TimeStr,
              sizeof (CHAR16) * MAX_SIZE_FOR_REDFISH_TIME,
              L"%d:%d:%d",
              Time.Hour,
              Time.Minute,
              Time.Second
              );
            *Length = (StrLen(TimeStr) + 1) * sizeof (CHAR16);
            *AttributeValue = AllocateZeroPool (*Length);
            CopyMem (*AttributeValue, TimeStr, *Length);

            break;

          case EFI_IFR_ORDERED_LIST_OP:

            Buffer = Statement->HiiStatement->Value.Buffer;
            switch (Statement->HiiStatement->Value.BufferValueType) {
              case EFI_IFR_TYPE_NUM_SIZE_8:

                ElementCount = Statement->HiiStatement->Value.BufferLen;
                ElementWidth = 1;
                break;

              case EFI_IFR_TYPE_NUM_SIZE_16:

                ElementCount = Statement->HiiStatement->Value.BufferLen / 2;
                ElementWidth = 2;
                break;

              case EFI_IFR_TYPE_NUM_SIZE_32:

                ElementCount = Statement->HiiStatement->Value.BufferLen / 4;
                ElementWidth = 4;
                break;

              case EFI_IFR_TYPE_NUM_SIZE_64:

                ElementCount = Statement->HiiStatement->Value.BufferLen / 8;
                ElementWidth = 8;
                break;

              default:
                ElementCount = Statement->HiiStatement->Value.BufferLen;
                ElementWidth = 1;
                break;
            }

            StrCpyS (BufferStr, MAX_SIZE_FOR_REDFISH_ORDERED_LIST, L"{");
            if (Buffer != NULL) {
              for (Counter = 0; Counter < ElementCount; Counter ++) {
                switch (ElementWidth) {

                case 1:

                  UnicodeSPrint (BufferStr, MAX_SIZE_FOR_REDFISH_ORDERED_LIST - StrLen (BufferStr) - 1,
                    L"%s%d", BufferStr, *(Buffer + Counter));
                  break;

                case 2:
                  UnicodeSPrint (BufferStr, MAX_SIZE_FOR_REDFISH_ORDERED_LIST - StrLen (BufferStr) - 1,
                    L"%s%d", BufferStr, *(((UINT16 *) Buffer) + Counter));
                  break;

                case 4:
                  UnicodeSPrint (BufferStr, MAX_SIZE_FOR_REDFISH_ORDERED_LIST - StrLen (BufferStr) - 1,
                    L"%s%d", BufferStr, *(((UINT32 *) Buffer) + Counter));
                  break;

                case 8:
                  UnicodeSPrint (BufferStr, MAX_SIZE_FOR_REDFISH_ORDERED_LIST - StrLen (BufferStr) - 1,
                    L"%s%d", BufferStr, *(((UINT64 *) Buffer) + Counter));
                  break;

                default:
                  break;
                }

                if (Counter != ElementCount - 1) {

                  StrCpyS (BufferStr + StrLen (BufferStr), MAX_SIZE_FOR_REDFISH_ORDERED_LIST - StrLen (BufferStr) - 1, L",");
                }
              }
            }
            StrCpyS (BufferStr + StrLen (BufferStr), MAX_SIZE_FOR_REDFISH_ORDERED_LIST - StrLen (BufferStr) - 1, L"}");

            *Length = (StrLen (BufferStr) + 1) * sizeof (CHAR16);
            *AttributeValue = AllocateZeroPool (*Length);
            CopyMem (*AttributeValue, BufferStr, *Length);
            break;

          default:
            break;
          }
        }

        StatementLink = GetNextNode (&Form->RedfishStatementList, StatementLink);
      }
      FormListLink = GetNextNode (&FormSet->RedfishFormList, FormListLink);
    }
    FormsetListLink = GetNextNode (FormsetListLinkHead, FormsetListLink);
  }

  FreePool (DevAlias);
  if (*AttributeValue == NULL) {
    return EFI_NOT_FOUND;
  } else {
    return EFI_SUCCESS;
  }
}

/**
  Each question in HII Database may be converted to an attribute for Redfish, this function is used to save
  value into a question without keyword support from an attribute value.

  Not all questions are supported to save value, supported types include: Number, OneOf, CheckBox, String,
  OrderList, Time and Date. The value for each question can only be mapped to a certain type of JSON value.
  This mapping is as below:

  EFI_IFR_NUMERIC_OP:       Number JSON value
  EFI_IFR_ONE_OF_OP:        Number JSON value
  EFI_IFR_STRING_OP:        String JSON value
  EFI_IFR_CHECKBOX_OP:      Boolean JSON value
  EFI_IFR_ORDERED_LIST_OP:  String JSON value, format as "{xx,xx,xx}"
  EFI_IFR_DATE_OP:          String JSON value, format as "yy/mm/dd"
  EFI_IFR_TIME_OP:          String JSON value, format as "hh:mm:ss"

  @param[in]      FormSet                          The formset for the target question.
  @param[in]      Form                             The form for the target question.
  @param[in]      Statement                        The target question to save value to.
  @param[in]      AttributeValue                   The value of this attribute for a certain question.

  @retval  EFI_SUCCESS                             The value has been saved to this question.
  @retval  EFI_INVALID_PARAMETER                   One or more parameters are invalid.
  @retval  EFI_ABORTED                             This operation has been aborted.
  @retval  Others                                  Another unexpected error occured.

**/
EFI_STATUS
SaveAttributeValueByQuestion  (
  IN REDFISH_FORMSET      *FormSet,
  IN REDFISH_FORM         *Form,
  IN REDFISH_STATEMENT    *Statement,
  IN EDKII_JSON_VALUE     AttributeValue
  )
{
  EFI_STATUS    Status;
  CHAR8         *ValueAsciiStr;
  CHAR16        *ValueUcs2StrPtr;
  UINTN         ValueNum;

  Status = EFI_SUCCESS;

  if (FormSet == NULL || Form == NULL || Statement == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (Statement->HiiStatement == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  switch (Statement->HiiStatement->Operand) {

  case EFI_IFR_NUMERIC_OP:
  case EFI_IFR_ONE_OF_OP:

    if (!JsonValueIsNumber (AttributeValue)) {
      return EFI_INVALID_PARAMETER;
    }

    ValueNum = (UINTN) JsonValueGetNumber (AttributeValue);
    CopyMem (&Statement->HiiStatement->Value.Value, &ValueNum, sizeof (UINTN));
    break;

  case EFI_IFR_STRING_OP:

    if (!JsonValueIsString (AttributeValue)) {
      return EFI_INVALID_PARAMETER;
    }

    ValueUcs2StrPtr = JsonValueGetUnicodeString (AttributeValue);
    if (ValueUcs2StrPtr == NULL) {
      return EFI_ABORTED;
    }

    if (Statement->HiiStatement->StorageWidth < StrSize (ValueUcs2StrPtr)) {
      FreePool (ValueUcs2StrPtr);
      return EFI_ABORTED;
    }

    Statement->HiiStatement->Value.Value.string = HiiSetString (FormSet->HiiFormSet->HiiHandle, 0, ValueUcs2StrPtr, NULL);
    CopyMem (Statement->HiiStatement->Value.Buffer, ValueUcs2StrPtr, Statement->HiiStatement->Value.BufferLen);
    Statement->HiiStatement->Value.BufferLen = (UINT16) StrSize (ValueUcs2StrPtr);
    FreePool (ValueUcs2StrPtr);
    break;

  case EFI_IFR_CHECKBOX_OP:

    if (!JsonValueIsBoolean (AttributeValue)) {
      return EFI_INVALID_PARAMETER;
    }

    ValueNum = (UINTN) JsonValueGetBoolean (AttributeValue);
    CopyMem (&Statement->HiiStatement->Value.Value, &ValueNum, sizeof (UINTN));
    break;

  case EFI_IFR_ORDERED_LIST_OP:

    if (!JsonValueIsString (AttributeValue)) {
      return EFI_INVALID_PARAMETER;
    }

    ValueAsciiStr = JsonValueGetAsciiString (AttributeValue);
    if (ValueAsciiStr == NULL) {
      return EFI_ABORTED;
    }

    Status = GetOrderListByString (
               ValueAsciiStr,
               Statement->HiiStatement->Value.BufferValueType,
               Statement->HiiStatement->StorageWidth,
               Statement->HiiStatement->Value.Buffer,
               &Statement->HiiStatement->Value.BufferLen
               );
    break;

  case EFI_IFR_DATE_OP:

    if (!JsonValueIsString (AttributeValue)) {
      return EFI_INVALID_PARAMETER;
    }

    ValueAsciiStr = JsonValueGetAsciiString (AttributeValue);
    if (ValueAsciiStr == NULL) {
      return EFI_ABORTED;
    }

    Status = GetDateByString (
               ValueAsciiStr,
               (EFI_HII_DATE *) &Statement->HiiStatement->Value.Value
               );
    break;

  case EFI_IFR_TIME_OP:

    if (!JsonValueIsString (AttributeValue)) {
      return EFI_INVALID_PARAMETER;
    }

    ValueAsciiStr = JsonValueGetAsciiString (AttributeValue);
    if (ValueAsciiStr == NULL) {
      return EFI_ABORTED;
    }

    Status = GetTimeByString (
               ValueAsciiStr,
               (EFI_HII_TIME *) &Statement->HiiStatement->Value.Value
               );
    break;

  default:
    return EFI_INVALID_PARAMETER;
  }
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SetQuestionValue (
             FormSet->HiiFormSet,
             Form->HiiForm,
             Statement->HiiStatement,
             &Statement->HiiStatement->Value
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Statement->ValueChanged = TRUE;

  Status = SubmitForm (
             FormSet->HiiFormSet,
             Form->HiiForm
             );
  return Status;
}

/**
  This function is used to save a string value into a question through keyword.

  @param[in]      NamespaceStr                     X-UEFI Namespace this Attribute associated with.
  @param[in]      DevicePath                       The device path for this keyword.
  @param[in]      Keyword                          The keyword name for this question.
  @param[in]      StringValue                      The string value of this attribute for a certain question.

  @retval  EFI_SUCCESS                             The value has been saved to this question.
  @retval  EFI_INVALID_PARAMETER                   One or more parameters are invalid.
  @retval  EFI_ABORTED                             Save action has been aborted.
  @retval  Others                                  Another unexpected error occured.

**/
EFI_STATUS
SaveStringByKeyword  (
  IN CHAR16                      *NamespaceStr,
  IN EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
  IN CHAR16                      *Keyword,
  IN EDKII_JSON_VALUE            StringValue
  )
{
  EFI_STATUS    Status;
  CHAR16        *ValueUcs2Str;

  RETURN_STATUS_IF_JSON_VALUE_NOT_STRING (StringValue, EFI_INVALID_PARAMETER);

  ValueUcs2Str = JsonValueGetUnicodeString (StringValue);
  if (ValueUcs2Str == NULL) {
    return EFI_ABORTED;
  }

  Status = KeywordConfigSetValue (
             NamespaceStr,
             DevicePath,
             Keyword,
             ValueUcs2Str,
             KeywordTypeBuffer
             );

  return Status;
}

/**
  This function is used to save a number value into a question through keyword.

  @param[in]      NamespaceStr                     X-UEFI Namespace this Attribute associated with.
  @param[in]      DevicePath                       The device path for this keyword.
  @param[in]      Keyword                          The keyword name for this question.
  @param[in]      NumberValue                      The number value of this attribute for a certain question.
  @param[in]      NumberValueType                  The number type of this attribute. Supported number types include:
                                                   EFI_IFR_TYPE_NUM_SIZE_8, EFI_IFR_TYPE_NUM_SIZE_16,
                                                   EFI_IFR_TYPE_NUM_SIZE_32, EFI_IFR_TYPE_NUM_SIZE_64

  @retval  EFI_SUCCESS                             The value has been saved to this question.
  @retval  EFI_INVALID_PARAMETER                   One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES                    There is no enough memory.
  @retval  Others                                  Another unexpected error occured.

**/
EFI_STATUS
SaveNumberByKeyword  (
  IN CHAR16                      *NamespaceStr,
  IN EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
  IN CHAR16                      *Keyword,
  IN EDKII_JSON_VALUE            NumberValue,
  IN UINT8                       NumberValueType
  )
{
  EFI_STATUS      Status;
  CHAR16          *ValueUcs2Str;
  UINT64          ValueNum;
  KEYWORD_TYPE    KeywordType;

  RETURN_STATUS_IF_JSON_VALUE_NOT_NUMBER (NumberValue, EFI_INVALID_PARAMETER);

  ValueUcs2Str = AllocateZeroPool (TEM_STR_LEN * sizeof (CHAR16));
  if (ValueUcs2Str == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  ValueNum = JsonValueGetNumber (NumberValue);
  UnicodeSPrint (ValueUcs2Str, TEM_STR_LEN * sizeof (CHAR16), L"%ld", ValueNum);

  switch (NumberValueType) {

    case EFI_IFR_TYPE_NUM_SIZE_8:

      KeywordType = KeywordTypeNumeric1;
      break;

    case EFI_IFR_TYPE_NUM_SIZE_16:

      KeywordType = KeywordTypeNumeric2;
      break;

    case EFI_IFR_TYPE_NUM_SIZE_32:

      KeywordType = KeywordTypeNumeric4;
      break;

    case EFI_IFR_TYPE_NUM_SIZE_64:

      KeywordType = KeywordTypeNumeric8;
      break;

    default:
      break;
  }

  Status = KeywordConfigSetValue (
             NamespaceStr,
             DevicePath,
             Keyword,
             ValueUcs2Str,
             KeywordType
             );

  return Status;
}

/**
  This function is used to save a boolean value into a question through keyword.

  @param[in]      NamespaceStr                     X-UEFI Namespace this Attribute associated with.
  @param[in]      DevicePath                       The device path for this keyword.
  @param[in]      Keyword                          The keyword name for this question.
  @param[in]      BooleanValue                     The boolean value of this attribute for a certain question.

  @retval  EFI_SUCCESS                             The value has been saved to this question.
  @retval  EFI_INVALID_PARAMETER                   One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES                    There is no enough memory.
  @retval  Others                                  Another unexpected error occured.

**/
EFI_STATUS
SaveBooleanByKeyword  (
  IN CHAR16                      *NamespaceStr,
  IN EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
  IN CHAR16                      *Keyword,
  IN EDKII_JSON_VALUE            BooleanValue
  )
{
  EFI_STATUS      Status;
  CHAR16          *ValueUcs2Str;
  BOOLEAN         ValueBool;

  RETURN_STATUS_IF_JSON_VALUE_NOT_BOOLEAN (BooleanValue, EFI_INVALID_PARAMETER);

  ValueUcs2Str = AllocateZeroPool (TEM_STR_LEN * sizeof (CHAR16));
  if (ValueUcs2Str == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  ValueBool = JsonValueGetBoolean (BooleanValue);
  if (ValueBool == TRUE) {
    ValueUcs2Str = L"1";
  } else {
    ValueUcs2Str = L"0";
  }

  Status = KeywordConfigSetValue (
             NamespaceStr,
             DevicePath,
             Keyword,
             ValueUcs2Str,
             KeywordTypeNumeric1
             );

  return Status;
}

/**
  Each question in HII Database may be converted to an attribute for Redfish, this function is used to save
  value into a question through keyword.

  OrderList, Date and Time questions are not supported now.

  @param[in]      Statement                        The target question identified by the attribute name.
  @param[in]      NamespaceStr                     X-UEFI Namespace this Attribute associated with.
  @param[in]      DevicePathStr                    The device path string for this keyword.
  @param[in]      Keyword                          The keyword name for this question.
  @param[in]      AttributeValue                   The value of this attribute for a certain question.

  @retval  EFI_SUCCESS                             The value has been saved to this question.
  @retval  EFI_INVALID_PARAMETER                   One or more parameters are invalid.
  @retval  EFI_ABORTED                             This operation has been aborted.
  @retval  EFI_UNSUPPORTED                         This kind of value can't be set through keyword.
  @retval  Others                                  Another unexpected error occured.

**/
EFI_STATUS
SaveAttributeValueByKeyword (
  IN REDFISH_STATEMENT    *Statement,
  IN CHAR16               *NamespaceStr,
  IN CHAR16               *DevicePathStr,
  IN CHAR16               *Keyword,
  IN EDKII_JSON_VALUE     AttributeValue
  )
{
  EFI_STATUS                  Status;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;

  if (Statement == NULL || Keyword == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DevicePath = ConvertTextToDevicePath (DevicePathStr);
  if (DevicePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  switch (Statement->HiiStatement->Operand) {
  case EFI_IFR_NUMERIC_OP:

    Status = SaveNumberByKeyword (NamespaceStr, DevicePath, Keyword, AttributeValue, Statement->HiiStatement->Value.Type);
    break;

  case EFI_IFR_STRING_OP:

    Status = SaveStringByKeyword (NamespaceStr, DevicePath, Keyword, AttributeValue);
    break;

  case EFI_IFR_CHECKBOX_OP:

    Status = SaveBooleanByKeyword (NamespaceStr, DevicePath, Keyword, AttributeValue);
    break;

  case EFI_IFR_ONE_OF_OP:

    Status = SaveNumberByKeyword (NamespaceStr, DevicePath, Keyword, AttributeValue, KeywordTypeNumeric1);
    break;

  default:

    return EFI_UNSUPPORTED;
  }

  FreePool (DevicePath);
  return Status;
}

/**
  Publish Redfish required MapFrom Dependency for an id value equal expression dependency.
  This expression compares the value of a question and a static number value.

  @param[in]          ExpDes                  The id value equal expression.
  @param[in]          AttributeName           The attribute name for MapFrom dependency.
  @param[in, out]     MapFromArray            MapFrom dependency array.

  @retval  EFI_SUCCESS                        This dependency has been published successfully.
  @retval  EFI_OUT_OF_RESOURCES               There are no enough Memory.
  @retval  EFI_INVALID_PARAMETER              One or more parameters are invalid.

**/
EFI_STATUS
GetDependencyForEqIdVal (
  IN     HII_DEPENDENCY_EXPRESSION    *ExpDes,
  IN     CHAR8                        *AttributeName,
  IN OUT EDKII_JSON_VALUE             MapFromArray
  )
{
  EFI_STATUS          Status;
  UINT64              NumValue;
  EDKII_JSON_VALUE    MapFromValue;
  EDKII_JSON_VALUE    JsonTemp;

  if (ExpDes->EqIdValExp.Value.Type == EFI_IFR_TYPE_NUM_SIZE_8) {
    NumValue = ExpDes->EqIdValExp.Value.Value.u8;
  } else if (ExpDes->EqIdValExp.Value.Type == EFI_IFR_TYPE_NUM_SIZE_16) {
    NumValue = ExpDes->EqIdValExp.Value.Value.u16;
  } else if (ExpDes->EqIdValExp.Value.Type == EFI_IFR_TYPE_NUM_SIZE_32) {
    NumValue = ExpDes->EqIdValExp.Value.Value.u32;
  } else if (ExpDes->EqIdValExp.Value.Type == EFI_IFR_TYPE_NUM_SIZE_64) {
    NumValue = ExpDes->EqIdValExp.Value.Value.u64;
  } else {
    return EFI_INVALID_PARAMETER;
  }

  MapFromValue = JsonValueInitObject ();
  if (MapFromValue == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = JsonArrayAppendValue (JsonValueGetArray (MapFromArray), MapFromValue);
  if (EFI_ERROR (Status)) {

    JsonValueFree (MapFromValue);
    return Status;
  }

  JsonTemp = JsonValueInitAsciiString (AttributeName);
  JsonObjectSetValue (
    JsonValueGetObject (MapFromValue),
    "MapFromAttribute",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonTemp = JsonValueInitAsciiString ("CurrentValue");
  JsonObjectSetValue (
    JsonValueGetObject (MapFromValue),
    "MapFromProperty",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonTemp = JsonValueInitNumber (NumValue);
  JsonObjectSetValue (
    JsonValueGetObject (MapFromValue),
    "MapFromValue",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonTemp = JsonValueInitAsciiString ("EQU");
  JsonObjectSetValue (
    JsonValueGetObject (MapFromValue),
    "MapFromCondition",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonValueFree (MapFromValue);
  return EFI_SUCCESS;
}

/**
  Update an existing MapFrom Dependency for a not expression. Change "MapFromCondition" to the
  opposite side for this dependency, as: EQU->NEQ, NEQ->EQU, GEQ->LSS, GTR->LEQ, LEQ->GTR, LSS->GEQ.

  @param[in, out]     MapFromArray            MapFrom dependency array.

  @retval  EFI_SUCCESS                        This dependency has been updated successfully.
  @retval  EFI_INVALID_PARAMETER              One or more parameters are invalid.

**/
EFI_STATUS
GetDependencyForNot (
  IN OUT EDKII_JSON_VALUE    MapFromArray
  )
{
  UINTN               ArrayCount;
  CHAR8               *ConditionStr;
  EDKII_JSON_VALUE    MapFromValue;
  EDKII_JSON_VALUE    ConditionValue;
  EDKII_JSON_VALUE    JsonTemp;

  ArrayCount = JsonArrayCount (JsonValueGetArray (MapFromArray));
  if (ArrayCount == 0) {
    return EFI_INVALID_PARAMETER;
  }

  MapFromValue   = JsonArrayGetValue (JsonValueGetArray (MapFromArray), ArrayCount - 1);
  ConditionValue = JsonObjectGetValue (JsonValueGetObject (MapFromValue), "MapFromCondition");
  if (!JsonValueIsString (ConditionValue)) {
    return EFI_INVALID_PARAMETER;
  }

  ConditionStr = JsonValueGetAsciiString (ConditionValue);
  if (ConditionStr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (AsciiStrCmp (ConditionStr, "EQU") == 0) {

    JsonTemp = JsonValueInitAsciiString ("NEQ");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else if (AsciiStrCmp (ConditionStr, "NEQ") == 0) {

    JsonTemp = JsonValueInitAsciiString ("EQU");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else if (AsciiStrCmp (ConditionStr, "GEQ") == 0) {

    JsonTemp = JsonValueInitAsciiString ("LSS");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else if (AsciiStrCmp (ConditionStr, "GTR") == 0) {

    JsonTemp = JsonValueInitAsciiString ("LEQ");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else if (AsciiStrCmp (ConditionStr, "LEQ") == 0) {

    JsonTemp = JsonValueInitAsciiString ("GTR");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else {  //LSS

    JsonTemp = JsonValueInitAsciiString ("GEQ");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  }
  JsonValueFree (JsonTemp);

  return EFI_SUCCESS;
}

/**
  Formalize the comparing expressions in a condition expression to a left expression and a right
  expression.

  Condition expression dependencies in UEFI include: EFI_IFR_EQUAL_OP, EFI_IFR_NOT_EQUAL_OP,
  EFI_IFR_GREATER_EQUAL_OP, EFI_IFR_GREATER_THAN_OP, EFI_IFR_LESS_EQUAL_OP and EFI_IFR_LESS_THAN_OP.

  @param[in]          Oprand                  To indicate what kind of condition it is.
  @param[in]          ExpDes                  The condition expression.
  @param[out]         SubExpDes1              The retrieved left expression to be compaired.
  @param[out]         SubExpDes2              The retrieved right expression to be compaired.

**/
VOID
GetExpressionsForCondition (
  IN     UINT8                        Oprand,
  IN     HII_DEPENDENCY_EXPRESSION    *ExpDes,
     OUT HII_DEPENDENCY_EXPRESSION    **SubExpDes1,
     OUT HII_DEPENDENCY_EXPRESSION    **SubExpDes2
  )
{
  if (Oprand == EFI_IFR_EQUAL_OP) {

    *SubExpDes1 = ExpDes->EqualExp.SubExpression2;
    *SubExpDes2 = ExpDes->EqualExp.SubExpression1;
  } else if (Oprand == EFI_IFR_NOT_EQUAL_OP) {

    *SubExpDes1 = ExpDes->NotEqualExp.SubExpression2;
    *SubExpDes2 = ExpDes->NotEqualExp.SubExpression1;
  } else if (Oprand == EFI_IFR_GREATER_EQUAL_OP) {

    *SubExpDes1 = ExpDes->GreaterEqualExp.RightHandExp;
    *SubExpDes2 = ExpDes->GreaterEqualExp.LeftHandExp;
  } else if (Oprand == EFI_IFR_GREATER_THAN_OP) {

    *SubExpDes1 = ExpDes->GreaterThanExp.RightHandExp;
    *SubExpDes2 = ExpDes->GreaterThanExp.LeftHandExp;

  } else if (Oprand == EFI_IFR_LESS_EQUAL_OP) {

    *SubExpDes1 = ExpDes->LessEqualExp.RightHandExp;
    *SubExpDes2 = ExpDes->LessEqualExp.LeftHandExp;
  } else {

    *SubExpDes1 = ExpDes->LessThanExp.RightHandExp;
    *SubExpDes2 = ExpDes->LessThanExp.LeftHandExp;
  }
}

/**
  Publish Redfish required MapFrom Dependency for a condition expression dependency. The condition
  expression has to be a comparison between two consistent expressions.

  Condition expression dependencies in UEFI include: EFI_IFR_EQUAL_OP, EFI_IFR_NOT_EQUAL_OP,
  EFI_IFR_GREATER_EQUAL_OP, EFI_IFR_GREATER_THAN_OP, EFI_IFR_LESS_EQUAL_OP and EFI_IFR_LESS_THAN_OP.

  @param[in]          Oprand                  To indicate what kind of condition it is.
  @param[in]          AttributeName           The attribute name for MapFrom dependency.
  @param[in, out]     MapFromArray            MapFrom dependency array.
  @param[in]          SubExpDes1              The left expression to be compaired.
  @param[in]          SubExpDes2              The right expression to be compaired.

  @retval  EFI_SUCCESS                        This dependency has been published successfully.
  @retval  EFI_OUT_OF_RESOURCES               There are no enough Memory.
  @retval  EFI_UNSUPPORTED                    This dependency was not supported by Redfish or UEFI.

**/
EFI_STATUS
GetDependencyForCondition (
  IN     UINT8                        Oprand,
  IN     CHAR8                        *AttributeName,
  IN OUT EDKII_JSON_VALUE             MapFromArray,
  IN     HII_DEPENDENCY_EXPRESSION    *SubExpDes1,
  IN     HII_DEPENDENCY_EXPRESSION    *SubExpDes2
  )
{
  EDKII_JSON_VALUE    MapFromValue;
  EDKII_JSON_VALUE    JsonTemp;

  MapFromValue = JsonValueInitObject ();
  if (MapFromValue == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  JsonTemp = JsonValueInitAsciiString (AttributeName);
  JsonObjectSetValue (
    JsonValueGetObject (MapFromValue),
    "MapFromAttribute",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  JsonTemp = JsonValueInitAsciiString ("CurrentValue");
  JsonObjectSetValue (
    JsonValueGetObject (MapFromValue),
    "MapFromProperty",
    JsonTemp
    );
  JsonValueFree (JsonTemp);

  if (Oprand == EFI_IFR_EQUAL_OP) {

    JsonTemp = JsonValueInitAsciiString ("EQU");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else if (Oprand == EFI_IFR_NOT_EQUAL_OP) {

    JsonTemp = JsonValueInitAsciiString ("NEQ");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else if (Oprand == EFI_IFR_GREATER_EQUAL_OP) {

    JsonTemp = JsonValueInitAsciiString ("GEQ");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else if (Oprand == EFI_IFR_GREATER_THAN_OP) {

    JsonTemp = JsonValueInitAsciiString ("GTR");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else if (Oprand == EFI_IFR_LESS_EQUAL_OP) {

    JsonTemp = JsonValueInitAsciiString ("LEQ");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  } else {

    JsonTemp = JsonValueInitAsciiString ("LSS");
    JsonObjectSetValue (JsonValueGetObject (MapFromValue), "MapFromCondition", JsonTemp);
  }
  JsonValueFree (JsonTemp);

  if (*((UINT8*) SubExpDes2) == EFI_IFR_TRUE_OP ||
    *((UINT8*) SubExpDes2) == EFI_IFR_FALSE_OP) {

    JsonObjectSetValue (
      JsonValueGetObject (MapFromValue),
      "MapFromValue",
      JsonValueInitBoolean(SubExpDes2->ContantExp.Value.Value.b)
      );
  } else if (*((UINT8*) SubExpDes2) == EFI_IFR_ONE_OP ||
    *((UINT8*) SubExpDes2) == EFI_IFR_UINT8_OP ||
    *((UINT8*) SubExpDes2) == EFI_IFR_ZERO_OP) {

    JsonTemp = JsonValueInitNumber (SubExpDes2->ContantExp.Value.Value.u8);
    JsonObjectSetValue (
      JsonValueGetObject (MapFromValue),
      "MapFromValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  } else if (*((UINT8*) SubExpDes2) == EFI_IFR_UINT16_OP ||
    *((UINT8*) SubExpDes2) == EFI_IFR_VERSION_OP) {

    JsonTemp = JsonValueInitNumber (SubExpDes2->ContantExp.Value.Value.u16);
    JsonObjectSetValue (
      JsonValueGetObject (MapFromValue),
      "MapFromValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  } else if (*((UINT8*) SubExpDes2) == EFI_IFR_UINT32_OP) {

    JsonTemp = JsonValueInitNumber (SubExpDes2->ContantExp.Value.Value.u32);
    JsonObjectSetValue (
      JsonValueGetObject (MapFromValue),
      "MapFromValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  } else if (*((UINT8*) SubExpDes2) == EFI_IFR_ONES_OP ||
    *((UINT8*) SubExpDes2) == EFI_IFR_UINT64_OP) {

    JsonTemp = JsonValueInitNumber ((UINTN) SubExpDes2->ContantExp.Value.Value.u64);
    JsonObjectSetValue (
      JsonValueGetObject (MapFromValue),
      "MapFromValue",
      JsonTemp
      );
    JsonValueFree (JsonTemp);
  } else {

    JsonValueFree (MapFromValue);
    return EFI_UNSUPPORTED;
  }

  return JsonArrayAppendValue (JsonValueGetArray (MapFromArray), MapFromValue);
}

/**
  Publish Redfish required MapFrom Dependency for a given expression dependency from HII database.

  Properties for MapFrom Dependency include: MapFromAttribute, MapFromProperty, MapFromValue,
  MapFromCondition and MapTerms. The map from attribute has to be existing in global attribute list.

  Supported expression dependencies in UEFI include: EFI_IFR_EQ_ID_VAL_OP, EFI_IFR_NOT_OP,
  EFI_IFR_AND_OP, EFI_IFR_OR_OP, EFI_IFR_EQUAL_OP, EFI_IFR_NOT_EQUAL_OP, EFI_IFR_GREATER_EQUAL_OP,
  EFI_IFR_GREATER_THAN_OP, EFI_IFR_LESS_EQUAL_OP and EFI_IFR_LESS_THAN_OP.

  @param[in]          FormSet                 The formset for this expression.
  @param[in]          Form                    The form for this expression.
  @param[in]          ExpDes                  The expression dependency to be published.
  @param[in, out]     MapFromArray            MapFrom dependency array.

  @retval  EFI_SUCCESS                        This dependency has been published successfully.
  @retval  EFI_INVALID_PARAMETER              One or more parameters are invalid.
  @retval  EFI_UNSUPPORTED                    This dependency was not supported by Redfish or UEFI.

**/
EFI_STATUS
RedfishGetDependencyForExpression (
  IN     HII_FORMSET                  *FormSet,
  IN     HII_FORM                     *Form,
  IN     HII_DEPENDENCY_EXPRESSION    *ExpDes,
  IN OUT EDKII_JSON_VALUE             MapFromArray
  )
{
  EFI_STATUS                    Status;
  EDKII_JSON_VALUE              MapFromValue;
  ATTRIBUTE_NAME_NODE           *MapFromNameNode;
  EFI_QUESTION_ID               CurrentQuestionId;
  UINT8                         Oprand;
  UINTN                         ArrayCount;
  HII_DEPENDENCY_EXPRESSION*    SubExpDes1;
  HII_DEPENDENCY_EXPRESSION*    SubExpDes2;
  EDKII_JSON_VALUE              JsonTemp;

  if (ExpDes == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (!JsonValueIsArray (MapFromArray)) {
    return EFI_INVALID_PARAMETER;
  }

  Oprand = *((UINT8*) ExpDes);
  switch (Oprand) {

    case EFI_IFR_EQ_ID_VAL_OP:

      MapFromNameNode = GetNameNodeByQuestionId (
                          FormSet,
                          &mAttributeNameNodeList,
                          ExpDes->EqIdValExp.QuestionId
                          );

      if (MapFromNameNode != NULL) {
        Status = GetDependencyForEqIdVal (
                   ExpDes,
                   MapFromNameNode->AttributeName,
                   MapFromArray
                   );
      } else {
        Status = EFI_UNSUPPORTED;
      }
      break;

    case EFI_IFR_NOT_OP:

      Status = RedfishGetDependencyForExpression (
                 FormSet,
                 Form,
                 ExpDes->NotExp.SubExpression,
                 MapFromArray);
      if (EFI_ERROR (Status)) {
        Status = EFI_UNSUPPORTED;
        break;
      }

      Status = GetDependencyForNot (MapFromArray);
      break;

    case EFI_IFR_AND_OP:
    case EFI_IFR_OR_OP:

      ArrayCount = JsonArrayCount (MapFromArray);

      Status = RedfishGetDependencyForExpression (
                 FormSet,
                 Form,
                 ExpDes->AndExp.SubExpression2,
                 MapFromArray
                 );
      if (EFI_ERROR (Status)) {
        Status = EFI_UNSUPPORTED;
        break;
      }

      Status = RedfishGetDependencyForExpression (
                 FormSet,
                 Form,
                 ExpDes->AndExp.SubExpression1,
                 MapFromArray
                 );
      if (EFI_ERROR (Status)) {

        JsonArrayRemoveValue (MapFromArray, ArrayCount);
        Status = EFI_UNSUPPORTED;
        break;
      }

      MapFromValue = JsonArrayGetValue (MapFromArray, ArrayCount + 1);
      if (Oprand == EFI_IFR_AND_OP) {

        JsonTemp = JsonValueInitAsciiString ("AND");
        JsonObjectSetValue (
          JsonValueGetObject (MapFromValue),
          "MapTerms",
          JsonTemp
          );
      } else {

        JsonTemp = JsonValueInitAsciiString ("OR");
        JsonObjectSetValue (
          JsonValueGetObject (MapFromValue),
          "MapTerms",
          JsonTemp
          );
      }
      JsonValueFree (JsonTemp);
      break;

    case EFI_IFR_EQUAL_OP:
    case EFI_IFR_NOT_EQUAL_OP:
    case EFI_IFR_GREATER_EQUAL_OP:
    case EFI_IFR_GREATER_THAN_OP:
    case EFI_IFR_LESS_EQUAL_OP:
    case EFI_IFR_LESS_THAN_OP:

      GetExpressionsForCondition (Oprand, ExpDes, &SubExpDes1, &SubExpDes2);
      if (!IsConstantExpression (SubExpDes2)) {

        Status = EFI_UNSUPPORTED;
        break;
      }

      if (*((UINT8*) SubExpDes1) == EFI_IFR_THIS_OP) {
        CurrentQuestionId = SubExpDes1->QuestionRef1Exp.QuestionId;
      } else if (*((UINT8*) SubExpDes1) == EFI_IFR_QUESTION_REF1_OP) {
        CurrentQuestionId = SubExpDes1->ThisExp.QuestionId;
      } else {
        Status = EFI_UNSUPPORTED;
        break;
      }

      MapFromNameNode = GetNameNodeByQuestionId (
                          FormSet,
                          &mAttributeNameNodeList,
                          CurrentQuestionId
                          );
      if (MapFromNameNode != NULL) {
        Status = GetDependencyForCondition (
                   Oprand,
                   MapFromNameNode->AttributeName,
                   MapFromArray,
                   SubExpDes1,
                   SubExpDes2
                   );
      } else {
        Status = EFI_UNSUPPORTED;
      }
      break;

    default:

      Status = EFI_UNSUPPORTED;
      break;
  }

  return Status;
}

/**
  Initialize the Redfish specific formset, and add some Redfish required information for a formset,
  such information mainly includes: formset device path and supported name space list.

  @param[in]          HiiFormSet                 The original formset retrieved from HII database

  @return the initialized Redfish formset or NULL when error occurs.

**/
REDFISH_FORMSET *
InitRedfishFormSet (
  IN HII_FORMSET    *HiiFormSet
  )
{
  EFI_STATUS         Status;
  REDFISH_FORMSET    *RedfishFormSet;
  CHAR8              *EndPointer;
  CHAR8              *NameSpaceId;

  RedfishFormSet = AllocateZeroPool (sizeof (REDFISH_FORMSET));
  if (RedfishFormSet == NULL) {
    return NULL;
  }

  RedfishFormSet->Signature  = REDFISH_FORMSET_SIGNATURE;
  RedfishFormSet->HiiFormSet = HiiFormSet;
  InitializeListHead (&RedfishFormSet->RedfishFormList);

  RedfishFormSet->DevicePathStr = ConvertDevicePathToText (HiiFormSet->DevicePath, FALSE, FALSE);
  Status = GenerateDevAlias (
             RedfishFormSet->DevicePathStr,
             RedfishFormSet->DevicePathAlias,
             sizeof (RedfishFormSet->DevicePathAlias)
             );

  RedfishFormSet->SupportedLanguages = HiiGetSupportedLanguages (HiiFormSet->HiiHandle);

  InitializeListHead (&RedfishFormSet->KeywordNamespaceList);
  InitializeListHead (&RedfishFormSet->NormalNamespaceList);

  EndPointer  = RedfishFormSet->SupportedLanguages;
  NameSpaceId = GetNextSupportedLanguage (RedfishFormSet->SupportedLanguages, &EndPointer);
  while (NameSpaceId != NULL) {

    AddNamespaceIdToFormSet (RedfishFormSet, NameSpaceId);
    FreePool (NameSpaceId);
    NameSpaceId = GetNextSupportedLanguage (EndPointer, &EndPointer);
  }

  return RedfishFormSet;
}

/**
  Initialize the Redfish specific form, and add some Redfish required information for a form,
  such information mainly includes: form menu name and form menu path.

  @param[in]          RedfishFormSet          The Redfish formset this form belongs to
  @param[in]          HiiForm                 The original form retrieved from HII database

  @return the initialized Redfish form or NULL when error occurs.

**/
REDFISH_FORM *
InitRedfishForm (
  IN REDFISH_FORMSET    *RedfishFormSet,
  IN HII_FORM           *HiiForm
  )
{
  REDFISH_FORM    *RedfishForm;
  CHAR16          *FormTitleStr;
  CHAR16          *Ptr;
  UINTN           StrIndex;

  RedfishForm = AllocateZeroPool (sizeof (REDFISH_FORM));
  if (RedfishForm == NULL) {
    return NULL;
  }

  RedfishForm->Signature = REDFISH_FORM_SIGNATURE;
  RedfishForm->HiiForm   = HiiForm;
  InitializeListHead (&RedfishForm->RedfishStatementList);

  //
  // Initialize Redfish Specific Form fields
  //
  FormTitleStr = NULL;
  if (HiiForm->FormTitle != 0) {
    FormTitleStr = HiiGetString (RedfishFormSet->HiiFormSet->HiiHandle, HiiForm->FormTitle, NULL);
  }

  if (FormTitleStr == NULL) {
    RedfishForm->RedfishMenuName = L"";
  } else {

    RedfishForm->RedfishMenuName = AllocateZeroPool (StrSize (FormTitleStr));
    if (RedfishForm->RedfishMenuName == NULL) {

      FreePool (RedfishForm);
      return NULL;
    }

    //
    // Set form menu name, and replace internal ' ' with '_'
    //
    Ptr      = FormTitleStr;
    StrIndex = 0;
    while (*Ptr != L'\0') {
      while (*Ptr != L' ' && *Ptr != L'\0') {

        RedfishForm->RedfishMenuName[StrIndex ++] = *Ptr;
        Ptr ++;
      }

      if (*Ptr == L' ') {

        RedfishForm->RedfishMenuName[StrIndex ++] = L'_';
        while (*Ptr == L' ') {
          Ptr ++;
        }
      } else {
        RedfishForm->RedfishMenuName[StrIndex] = L'\0';
      }
    }
  }

  return RedfishForm;
}

/**
  Initialize the Redfish specific question, and add some Redfish required information for a question,
  such information mainly includes: keyword supported tag and keyword name.

  @param[in]          RedfishFormSet          The Redfish formset this question belongs to
  @param[in]          HiiStatement            The original question retrieved from HII database

  @return the initialized Redfish question or NULL when error occurs.

**/
REDFISH_STATEMENT*
InitRedfishStatement (
  IN REDFISH_FORMSET    *RedfishFormSet,
  IN HII_STATEMENT      *HiiStatement
  )
{
  REDFISH_STATEMENT    *RedfishStatement;
  NAMESPACE_DATA       *NameSpace;
  LIST_ENTRY           *NameSpaceLink;
  CHAR16               *KeywordName;

  RedfishStatement = AllocateZeroPool (sizeof (REDFISH_STATEMENT));
  if (RedfishStatement == NULL) {
    return NULL;
  }

  RedfishStatement->Signature = REDFISH_STATEMENT_SIGNATURE;
  RedfishStatement->HiiStatement = HiiStatement;

  KeywordName = NULL;
  if (IsListEmpty (&RedfishFormSet->KeywordNamespaceList)) {
    RedfishStatement->IsKeywordSupported = FALSE;
  } else {

    NameSpaceLink = GetFirstNode (&RedfishFormSet->KeywordNamespaceList);
    NameSpace     = NAMESPACE_DATA_FROM_LINK(NameSpaceLink);
    if (NameSpace == NULL) {
      return NULL;
    }

    if (HiiStatement->Prompt != 0) {
      KeywordName = HiiGetStringEx (
                      RedfishFormSet->HiiFormSet->HiiHandle,
                      HiiStatement->Prompt,
                      NameSpace->NamespaceId,
                      FALSE
                      );

    }

    if (KeywordName != NULL) {

      RedfishStatement->IsKeywordSupported = TRUE;
      RedfishStatement->Keyword            = KeywordName;
    } else {

      RedfishStatement->IsKeywordSupported = FALSE;
      RedfishStatement->Keyword            = NULL;
    }
  }

  return RedfishStatement;
}

/**
  Initialize the system device alias table.

  Device alias is fixed format string as: Dev####, "####" represents the 4 hexadecimal digits device
  index in system, like "0001" or "001F". The length for this alias is fixed as 8 characters.

  The total count of device alias is saved in the variable: "RedfishDevAliasNum", and each device
  alias is saved in the variable named by "Dev####".

  @retval  EFI_SUCCESS                      Device alias table has been initialized successfully.
  @retval  EFI_DEVICE_ERROR                 Device storage meets some errors.
  @retval  EFI_OUT_OF_RESOURCES             System has no other memory to allocate.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
InitDevAliasTable (
  VOID
  )
{
  EFI_STATUS    Status;
  VOID          *AliasNumBuf;
  UINTN         AliasNumSize;
  CHAR16        AliasName[DEVICE_PATH_ALIAS_SIZE];
  UINTN         Index;

  //
  // Read Device path alias table
  //
  Status = GetVariable2 (
             DEVICE_PATH_ALIAS_NUM_VAR_NAME,
             &gEfiCallerIdGuid,
             &AliasNumBuf,
             &AliasNumSize
             );
  if (Status == EFI_NOT_FOUND) {

    AliasNumSize = sizeof (UINT32);
    Status = gRT->SetVariable (
                    DEVICE_PATH_ALIAS_NUM_VAR_NAME,
                    &gEfiCallerIdGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    sizeof (UINT32),
                    &mHiiRedfishPrivateData.AliasNum
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  } else if (Status == EFI_SUCCESS) {

    if (AliasNumSize != sizeof (UINT32)) {

      FreePool (AliasNumBuf);
      return EFI_DEVICE_ERROR;
    }

    mHiiRedfishPrivateData.AliasNum = *((UINT32*) AliasNumBuf);
    FreePool (AliasNumBuf);

    if (mHiiRedfishPrivateData.AliasNum > 0xFFFF) {
      return EFI_DEVICE_ERROR;
    } else if (mHiiRedfishPrivateData.AliasNum != 0) {

      mHiiRedfishPrivateData.AliasTable = AllocateZeroPool (mHiiRedfishPrivateData.AliasNum * sizeof (CHAR16*));
      if (mHiiRedfishPrivateData.AliasTable == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }
      for (Index = 0; Index < mHiiRedfishPrivateData.AliasNum; Index++) {

        UnicodeSPrint (AliasName, sizeof (AliasName), L"Dev%04x", Index);
        Status = GetVariable2 (
                   AliasName,
                   &gEfiCallerIdGuid,
                   &mHiiRedfishPrivateData.AliasTable[Index],
                   NULL
                   );
      }
    }
  } else {
    return Status;
  }

  return Status;
}

/**
  Parse a question to an attribute JSON value, and add this attribute to the end of the given
  attribute array.

  Question only support following types: EFI_IFR_NUMERIC_OP, EFI_IFR_STRING_OP, EFI_IFR_PASSWORD_OP,
  EFI_IFR_CHECKBOX_OP, EFI_IFR_ONE_OF_OP, EFI_IFR_ORDERED_LIST_OP, EFI_IFR_DATE_OP and EFI_IFR_TIME_OP.

  @param[in]          FormSet             The formset this question belongs to
  @param[in]          Form                The form this question belongs to
  @param[in]          Statement           The question to be parsed to attribute JSON value
  @param[in]          AttributesArray     The attribute array to be appended

  @retval  EFI_SUCCESS                    Device alias table has been initialized successfully.
  @retval  EFI_UNSUPPORTED                This attribute is not supported.
  @retval  EFI_OUT_OF_RESOURCES           System has no other memory to allocate.
  @retval  Others                         Another unexpected error occured.

**/
EFI_STATUS
AttributesArrayAppendValue (
  IN REDFISH_FORMSET      *FormSet,
  IN REDFISH_FORM         *Form,
  IN REDFISH_STATEMENT    *Statement,
  IN EDKII_JSON_VALUE     AttributesArray
  )
{
  EFI_STATUS          Status;
  EDKII_JSON_VALUE    Attribute;

  Attribute = JsonValueInitObject ();
  if (Attribute == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = EFI_UNSUPPORTED;

  switch (Statement->HiiStatement->Operand) {
  case EFI_IFR_NUMERIC_OP:

    Status = NumericToJson (FormSet, Form, Statement, Attribute);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (AttributesArray),
               Attribute
               );
    break;

  case EFI_IFR_STRING_OP:

    Status = StringToJson (FormSet, Form, Statement, Attribute);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (AttributesArray),
               Attribute
               );
    break;

  case EFI_IFR_PASSWORD_OP:

    Status = PasswordToJson (FormSet, Form, Statement, Attribute);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (AttributesArray),
               Attribute
               );
    break;

  case EFI_IFR_CHECKBOX_OP:

    Status = BooleanToJson (FormSet, Form, Statement, Attribute);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (AttributesArray),
               Attribute
               );
    break;

  case EFI_IFR_ONE_OF_OP:

    Status = OneofToJson (FormSet, Form, Statement, Attribute);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (AttributesArray),
               Attribute
               );
    break;

  case EFI_IFR_ORDERED_LIST_OP:

    Status = OrderListToJson(FormSet, Form, Statement, Attribute);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (AttributesArray),
               Attribute
               );
    break;

  case EFI_IFR_DATE_OP:

    Status = DateToJson (FormSet, Form, Statement, Attribute);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (AttributesArray),
               Attribute
               );
    break;

  case EFI_IFR_TIME_OP:

    Status = TimeToJson (FormSet, Form, Statement, Attribute);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (AttributesArray),
               Attribute
               );
    break;

  default:

    break;
  }

  JsonValueFree (Attribute);
  return Status;
}

/**
  Get the attribute JSON value from registry entries by attribute name.

  @param[in]       RegistryEntries                   The JSON wrapper value for Registry Entries.
  @param[in]       AttributeName                     Name of the Attribute to search.
  @param[out]      Attribute                         The retrieved Attribute.

  @retval  EFI_SUCCESS                               This Attribute has been retrieved successfully.
  @retval  EFI_INVALID_PARAMETER                     One or more parameters are invalid.
  @retval  EFI_NOT_FOUND                             This Attribute was not found.

**/
EFI_STATUS
GetAttributeByName (
  IN        EDKII_JSON_VALUE     RegistryEntries,
  IN        CONST CHAR8          *AttributeName,
  OUT       EDKII_JSON_VALUE     *Attribute
  )
{
  EDKII_JSON_VALUE     AttributesArray;
  EDKII_JSON_VALUE     AttributeEntry;
  EDKII_JSON_VALUE     StrValue;
  UINT32               Index;
  CHAR8                *AsciiStr;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (RegistryEntries, EFI_INVALID_PARAMETER);
  if (AttributeName == NULL || Attribute == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  AttributesArray = JsonObjectGetValue (RegistryEntries, "Attributes");
  RETURN_STATUS_IF_JSON_VALUE_NOT_ARRAY (AttributesArray, EFI_INVALID_PARAMETER);

  for (Index = 0; Index < JsonArrayCount (JsonValueGetArray (AttributesArray)); Index++) {

    AttributeEntry = JsonArrayGetValue (
                       JsonValueGetArray (AttributesArray),
                       Index
                       );

    StrValue = JsonObjectGetValue (JsonValueGetObject (AttributeEntry), "AttributeName");
    CONTINUE_IF_JSON_VALUE_NOT_STRING (StrValue);

    AsciiStr = JsonValueGetAsciiString (StrValue);
    if (AsciiStr == NULL) {
      continue;
    }

    if (AsciiStrCmp (AttributeName, AsciiStr) == 0) {

      *Attribute = AttributeEntry;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

/**
  Append a dependency JSON value to the end of the dependency array.

  Supported dependencies in UEFI include: SuppressIf, DisableIf, GrayoutIf and WarningTextIf.

  @param[in]       FormSet                          The .
  @param[in]       Form                             The current form menu to set menu path string.
  @param[in]       Statement                          The current form menu to set menu path string.
  @param[in]       Expression                          The current form menu to set menu path string.
  @param[in]       IsWarningExpression                          The current form menu to set menu path string.
  @param[in]       DependenciesArray                          The current form menu to set menu path string.

  @retval  EFI_SUCCESS                               The menu path has been set successfully.
  @retval  EFI_NOT_FOUND                             This form menu is not found in system menu list.

**/
EFI_STATUS
AppendDependencyArray(
  IN REDFISH_FORMSET      *FormSet,
  IN REDFISH_FORM         *Form,
  IN REDFISH_STATEMENT    *Statement,
  IN HII_EXPRESSION       *Expression,
  IN BOOLEAN              IsWarningExpression,
  IN EDKII_JSON_VALUE     DependenciesArray
  )
{
  EFI_STATUS               Status;
  EDKII_JSON_VALUE         MapFromArray;
  EDKII_JSON_VALUE         DependencyValue;
  EDKII_JSON_VALUE         DependenciesItem;
  ATTRIBUTE_NAME_NODE      *MapToNameNode;
  EDKII_JSON_VALUE         JsonTemp;
  LIST_ENTRY               *Link;
  HII_EXPRESSION_OPCODE    *ExpressionOpCode;

  MapToNameNode = GetNameNodeByQuestionId (
                    FormSet->HiiFormSet,
                    &mAttributeNameNodeList,
                    Statement->HiiStatement->QuestionId
                    );

  Link = GetFirstNode (&Expression->OpCodeListHead);
  while (!IsNull (&Expression->OpCodeListHead, Link)) {
    ExpressionOpCode = HII_EXPRESSION_OPCODE_FROM_LINK (Link);

    Link = GetNextNode (&Expression->OpCodeListHead, Link);
  }

  if (MapToNameNode != NULL) {

    DependencyValue = JsonValueInitObject ();
    if (DependencyValue == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    JsonObjectSetValue (
      JsonValueGetObject (DependencyValue),
      "MapToValue",
      JsonValueInitBoolean (TRUE)
      );

    JsonTemp = JsonValueInitAsciiString (MapToNameNode->AttributeName);
    JsonObjectSetValue (
      JsonValueGetObject (DependencyValue),
      "MapToAttribute",
      JsonTemp
      );
    JsonValueFree (JsonTemp);

    if (IsWarningExpression) {

      JsonTemp = JsonValueInitAsciiString ("WarningText");
      JsonObjectSetValue (
        JsonValueGetObject (DependencyValue),
        "MapToProperty",
        JsonTemp
        );
      JsonValueFree (JsonTemp);
    } else {

      if (Expression->Type == EFI_HII_EXPRESSION_SUPPRESS_IF || Expression->Type == EFI_HII_EXPRESSION_DISABLE_IF) {

        JsonTemp = JsonValueInitAsciiString ("Hidden");
        JsonObjectSetValue (
          JsonValueGetObject (DependencyValue),
          "MapToProperty",
          JsonTemp
          );
        JsonValueFree (JsonTemp);
      } else if (Expression->Type == EFI_HII_EXPRESSION_GRAY_OUT_IF) {

        JsonTemp = JsonValueInitAsciiString ("GrayOut");
        JsonObjectSetValue (
          JsonValueGetObject (DependencyValue),
          "MapToProperty",
          JsonTemp
          );
        JsonValueFree (JsonTemp);
      } else {

        JsonValueFree (DependencyValue);
        return EFI_UNSUPPORTED;
      }
    }

    Status = GetHiiExpressionDependency (Expression);
    if (EFI_ERROR (Status)) {

      JsonValueFree (DependencyValue);
      return Status;
    }

    if (Expression->RootDepdencyExp != NULL) {

      MapFromArray = JsonValueInitArray ();
      if (MapFromArray == NULL) {

        JsonValueFree (DependencyValue);
        return EFI_OUT_OF_RESOURCES;
      }

      Status = RedfishGetDependencyForExpression (
                 FormSet->HiiFormSet,
                 Form->HiiForm,
                 Expression->RootDepdencyExp,
                 MapFromArray
                 );

      if (EFI_ERROR (Status)) {

        JsonValueFree (MapFromArray);
        JsonValueFree (DependencyValue);
        return Status;
      }

      JsonObjectSetValue (
        JsonValueGetObject (DependencyValue),
        "MapFrom",
        MapFromArray
        );
      JsonValueFree (MapFromArray);

      DependenciesItem = JsonValueInitObject ();
      if (DependenciesItem == NULL) {

        JsonValueFree (DependencyValue);
        return EFI_OUT_OF_RESOURCES;
      }

      Status = JsonObjectSetValue (
                 JsonValueGetObject (DependenciesItem),
                 "Dependency",
                 DependencyValue
                 );
      JsonValueFree (DependencyValue);
      if (EFI_ERROR (Status)) {

        JsonValueFree (DependenciesItem);
        return Status;
      }

      JsonTemp = JsonValueInitAsciiString ("");
      JsonObjectSetValue (
        JsonValueGetObject (DependenciesItem),
        "DependencyFor",
        JsonTemp
        );
      JsonValueFree (JsonTemp);

      JsonTemp = JsonValueInitAsciiString ("Map");
      JsonObjectSetValue (
        JsonValueGetObject (DependenciesItem),
        "Type",
        JsonTemp
        );
      JsonValueFree (JsonTemp);

      Status = JsonArrayAppendValue (
                 JsonValueGetArray (DependenciesArray),
                 DependenciesItem
                 );
      JsonValueFree (DependenciesItem);
      return Status;
    } else {

      JsonValueFree (DependencyValue);
      return EFI_NOT_FOUND;
    }
  } else {

    return EFI_NOT_FOUND;
  }
}

/**
  This function is used to find a question identified by the attribute name for a non-keyword support
  attribute.

  Attribute name is unique for each question and consists by <NameSpace + Device Alias + Question Id>
  if it is a non-keyword support attribute or <NameSpace + Device Alias + Keyword> if it has keyword
  support.

  @param[in]       FormsetListLinkHead              List of the formsets to search for the target question.
  @param[in]       AttributeName                    The attribute name for the target question.
  @param[out]      FormSet                          The located formset for target question.
  @param[out]      Form                             The located form for target question.
  @param[out]      Statement                        The target question identified by the attribute name.

  @retval  EFI_SUCCESS                              This question is found.
  @retval  EFI_NOT_FOUND                            This question is not found in the given formset list.
  @retval  Others                                   Another unexpected error occured.

**/
EFI_STATUS
FindQuestion  (
  IN    LIST_ENTRY           *FormsetListLinkHead,
  IN    CHAR8                *AttributeName,
  IN    CHAR16               *Keyword,
  OUT   REDFISH_FORMSET      **FormSet,
  OUT   REDFISH_FORM         **Form,
  OUT   REDFISH_STATEMENT    **Statement
  )
{
  EFI_STATUS                Status;
  CHAR8                     DevAlias[DEVICE_PATH_ALIAS_SIZE];
  EFI_QUESTION_ID           QuestionId;
  LIST_ENTRY                *FormsetListLink;
  LIST_ENTRY                *FormListLink;
  LIST_ENTRY                *StatementListLink;

  Status = GetDevAliasByAttributeName (AttributeName, DevAlias);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Keyword == NULL) {
    Status = GetQuestionIdByAttributeName (AttributeName, &QuestionId);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  FormsetListLink = GetFirstNode (FormsetListLinkHead);
  while (!IsNull (FormsetListLinkHead, FormsetListLink)) {

    *FormSet = REDFISH_FORMSET_FROM_LINK (FormsetListLink);
    if (AsciiStrCmp ((*FormSet)->DevicePathAlias, DevAlias) == 0) {

      FormListLink = GetFirstNode (&(*FormSet)->RedfishFormList);
      while (!IsNull (&(*FormSet)->RedfishFormList, FormListLink)) {

        *Form = REDFISH_FORM_FROM_LINK (FormListLink);
        StatementListLink = GetFirstNode (&(*Form)->RedfishStatementList);
        while (!IsNull (&(*Form)->RedfishStatementList, StatementListLink)) {

          *Statement = REDFISH_STATEMENT_FROM_LINK (StatementListLink);

          if ((*Statement)->IsKeywordSupported && Keyword != NULL) {
            if (StrCmp (Keyword, (*Statement)->Keyword) == 0) {
              return EFI_SUCCESS;
            }
          } else {
            if ((*Statement)->HiiStatement->QuestionId == QuestionId) {
              return EFI_SUCCESS;
            }
          }

          StatementListLink = GetNextNode (&(*Form)->RedfishStatementList, StatementListLink);
        }
        FormListLink = GetNextNode (&(*FormSet)->RedfishFormList, FormListLink);
      }
    }
    FormsetListLink = GetNextNode (FormsetListLinkHead, FormsetListLink);
  }

  return EFI_NOT_FOUND;
}

/**
  Save the result for last route, the failed attribute name will be recorded. The route result
  can be leveraged to Redfish driver.

  @param[in]          RouteStatus                 The latest route status.
  @param[in]          EtagStr                     The Etag string
  @param[in]          AttributeName               To indicate which attribute error occurs on.

  @retval  EFI_SUCCESS                            The result has been saved successfully.
  @retval  EFI_INVALID_PARAMETER                  One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES                   There are no enough Memory.
  @retval  Others                                 Another unexpected error occured.

**/
EFI_STATUS
RedfishSaveRouteResult (
  IN       EFI_STATUS    RouteStatus,
  IN CONST CHAR8         *EtagStr,   OPTIONAL
  IN CONST CHAR8         *AttributeName
  )
{
  EFI_STATUS                    Status;
  UINTN                         DataLength;
  HII_REDFISH_LIB_NV_STORAGE    *Storage;

  if (EFI_ERROR (RouteStatus) && AttributeName == NULL) {

    DEBUG ((EFI_D_ERROR, "[Redfish] Must provide an Attribute Name for error status.\n"));
    return EFI_INVALID_PARAMETER;
  }

  DataLength = sizeof (HII_REDFISH_LIB_NV_STORAGE);
  if (EtagStr != NULL) {
    DataLength += AsciiStrSize (EtagStr);
  }
  if (AttributeName != NULL) {
    DataLength += sizeof ("#/Attributes/") + AsciiStrSize (AttributeName);
  }

  Storage = AllocateZeroPool (DataLength);
  if (Storage == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gRT->GetTime (&Storage->TimeOfLastRoute, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Storage->StatusOfLastRoute= RouteStatus;

  if (EtagStr != NULL) {

    Storage->EtagStrSize = AsciiStrSize (EtagStr);
    AsciiStrCpyS (
      (UINT8*)Storage + sizeof(HII_REDFISH_LIB_NV_STORAGE),
      Storage->EtagStrSize,
      EtagStr
      );
  }

  if (EFI_ERROR (RouteStatus)) {

    Storage->FailedAttributeNameSize = AsciiStrSize (AttributeName);
    CopyMem (
      (UINT8*) Storage + sizeof(HII_REDFISH_LIB_NV_STORAGE) + Storage->EtagStrSize,
      "#/Attributes/",
      sizeof ("#/Attributes/") - 1
      );
    AsciiStrCpyS (
      (UINT8*) Storage + sizeof(HII_REDFISH_LIB_NV_STORAGE) + Storage->EtagStrSize + sizeof ("#/Attributes/") - 1,
      AsciiStrSize (AttributeName),
      AttributeName
      );
  }

  Status = gRT->SetVariable (
                  HII_REDFISH_ROUTE_RESULT_VAR_NAME,
                  &gEfiCallerIdGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                  DataLength,
                  Storage
                  );
  FreePool (Storage);

  return Status;
}

/**
  Set some message information according to last route status to RedfishSettings JSON value.

  If the status of last route is EFI_SUCCESS, required message include: Message Id, Message
  and Resolution; otherwise required message include: Message Id and RelatedProperties.

  @param[in]         RedfishSettings              RedfishSetting JSON value
  @param[in]         Storage                      The storage which contains last route status

  @retval  EFI_SUCCESS                            Message information has been set successfully
  @retval  EFI_INVALID_PARAMETER                  One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES                   There are no enough Memory.
  @retval  Others                                 Another unexpected error occured.

**/
EFI_STATUS
SetMessageArrayToRedfishSetting (
  IN EDKII_JSON_VALUE              RedfishSettings,
  IN HII_REDFISH_LIB_NV_STORAGE    *Storage
  )
{
  EFI_STATUS          Status;
  EDKII_JSON_VALUE    MessagesArray;
  EDKII_JSON_VALUE    Message;
  EDKII_JSON_VALUE    RelatedProperties;
  EDKII_JSON_VALUE    FailedAttributeName;
  EDKII_JSON_VALUE    JsonTemp;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (RedfishSettings, EFI_INVALID_PARAMETER);

  //
  // Message
  //
  MessagesArray = JsonValueInitArray ();
  if (MessagesArray == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Message = JsonValueInitObject ();
  if (Message == NULL) {

    JsonValueFree (MessagesArray);
    return EFI_OUT_OF_RESOURCES;
  }

  if (EFI_ERROR (Storage->StatusOfLastRoute)) {

    JsonTemp = JsonValueInitAsciiString ("Base.1.0.SettingsFailed");
    Status   = JsonObjectSetValue (
                 JsonValueGetObject (Message),
                 "MessageId",
                 JsonTemp
                 );
    JsonValueFree (JsonTemp);
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }

    if (Storage->FailedAttributeNameSize != 0) {

      RelatedProperties = JsonValueInitArray ();
      if (RelatedProperties == NULL) {
        goto ON_EXIT;
      }

      FailedAttributeName = JsonValueInitAsciiString (
                              (CHAR8*) Storage + sizeof (HII_REDFISH_LIB_NV_STORAGE) + Storage->EtagStrSize
                              );
      JsonArrayAppendValue (JsonValueGetArray (RelatedProperties), FailedAttributeName);
      JsonValueFree (FailedAttributeName);

      Status = JsonObjectSetValue (
                 JsonValueGetObject (Message),
                 "RelatedProperties",
                 RelatedProperties
                 );
      JsonValueFree (RelatedProperties);
      if (EFI_ERROR (Status)) {
        goto ON_EXIT;
      }
    }
  } else {

    JsonTemp = JsonValueInitAsciiString ("Base.1.0.Success");
    Status   = JsonObjectSetValue (
                 JsonValueGetObject (Message),
                 "MessageId",
                 JsonTemp
                 );
    JsonValueFree (JsonTemp);
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }

    if (mHiiRedfishPrivateData.ResetRequired == TRUE) {

      JsonTemp = JsonValueInitAsciiString ("Configuration Changed, Reset system to Take Effect!");
      Status   = JsonObjectSetValue (
                   JsonValueGetObject (Message),
                   "Message",
                   JsonTemp
                   );
      JsonValueFree (JsonTemp);
      if (EFI_ERROR (Status)) {
        goto ON_EXIT;
      }

      JsonTemp = JsonValueInitAsciiString ("Link to perform a system reset: \\xxx\\xxx\\xxx\\xxx");
      Status = JsonObjectSetValue (
                 JsonValueGetObject (Message),
                 "Resolution",
                 JsonTemp
                 );
      JsonValueFree (JsonTemp);
      if (EFI_ERROR (Status)) {
        goto ON_EXIT;
      }
    }
  }

  Status = JsonArrayAppendValue (
             JsonValueGetArray (MessagesArray),
             Message
             );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  Status = JsonObjectSetValue (
             JsonValueGetObject (RedfishSettings),
             "Messages",
             MessagesArray
             );

ON_EXIT:

  JsonValueFree (MessagesArray);
  JsonValueFree (Message);

  return Status;
}

/**
  Initialize the form related information required by Redfish menu.

  @param[in]          RedfishFormSet     The Redfish formset this form belongs to
  @param[in]          RedfishForm        The Redfish form which contains the form needed
  @param[in, out]     RedfishMenu        The Refish menu to initiallize

**/
VOID
InitRedfishMenu (
  IN     REDFISH_FORMSET      *RedfishFormSet,
  IN     REDFISH_FORM         *RedfishForm,
  IN OUT REDFISH_MENU         *RedfishMenu
  )
{
  CopyGuid (&RedfishMenu->FormSetGuid, &RedfishFormSet->HiiFormSet->Guid);
  RedfishMenu->FormId     = RedfishForm->HiiForm->FormId;
  RedfishMenu->IsHidden   = FALSE;
  RedfishMenu->IsReadOnly = FALSE;
  RedfishMenu->IsRootMenu = RedfishForm->IsRootForm;
  RedfishMenu->IsRestMenu = RedfishForm->IsRest;

  RedfishMenu->DisplayName = NULL;
  if (RedfishForm->HiiForm->FormTitle != 0) {
    RedfishMenu->DisplayName = HiiGetString (
                                 RedfishFormSet->HiiFormSet->HiiHandle,
                                 RedfishForm->HiiForm->FormTitle,
                                 NULL
                                 );
    if (RedfishMenu->DisplayName == NULL) {
      RedfishMenu->DisplayName = L"";
    }
  } else {
    RedfishMenu->DisplayName = L"";
  }

  if (StrLen (RedfishForm->RedfishMenuName) != 0 &&
    StrCmp (RedfishForm->RedfishMenuName, L"_") != 0) {

    RedfishMenu->MenuName = AllocateCopyPool (
                              StrSize (RedfishForm->RedfishMenuName),
                              RedfishForm->RedfishMenuName
                              );
    if (RedfishMenu->MenuName == NULL) {
      RedfishMenu->MenuName = L"";
    }

  } else {
    RedfishMenu->MenuName = L"";
  }

  RedfishMenu->ChildCount = 0;
}

/**
  Get the count of Redfish Menus in the whole system.

  Redfish Menu is a structure contains some Redfish required information over HII form.

  @return the count of Redfish forms.

**/
UINTN
GetSystemRedfishMenuCount (
  VOID
  )
{
  UINT16                     MenuListCount;
  LIST_ENTRY                 *FormsetListLinkHead;
  LIST_ENTRY                 *FormsetListLink;
  REDFISH_FORMSET            *FormSet;
  LIST_ENTRY                 *FormListLink;
  REDFISH_FORM               *Form;

  MenuListCount = 0;

  FormsetListLinkHead = &mHiiRedfishPrivateData.RedfishFormSetList;
  FormsetListLink = GetFirstNode (FormsetListLinkHead);
  while (!IsNull (FormsetListLinkHead, FormsetListLink)) {

    FormSet = REDFISH_FORMSET_FROM_LINK (FormsetListLink);
    FormListLink = GetFirstNode (&FormSet->RedfishFormList);
    while (!IsNull (&FormSet->RedfishFormList, FormListLink)) {

      Form = REDFISH_FORM_FROM_LINK (FormListLink);
      FormListLink = GetNextNode (&FormSet->RedfishFormList, FormListLink);

      MenuListCount ++;
    }
    FormsetListLink = GetNextNode (FormsetListLinkHead, FormsetListLink);
  }

  return MenuListCount;
}

/**
  Link all Redfish form menus in the whole system, and try to find the parent form menu for
  each of them.

  For each form in HII databse, there is a path form root form to it. This function is to link
  all forms to their parent forms in system.

  @param[in]          MenuList                The list of Redfish form menus
  @param[in]          MenuListCount           The count of Redfish form menus in system

**/
VOID
LinkSystemRedfishMenus (
  IN REDFISH_MENU    *MenuList,
  IN UINTN           MenuListCount
  )
{
  REDFISH_MENU            *TempMenu;
  REDFISH_MENU            *TempChildMenu;
  REDFISH_MENU            *CurrentMenu;
  REDFISH_MENU            *ChildMenu;
  UINTN                   MenuListIndex;

  EFI_IFR_TYPE_VALUE *    Value;
  LIST_ENTRY              *FormsetListLinkHead;
  LIST_ENTRY              *FormsetListLink;
  REDFISH_FORMSET         *FormSet;
  LIST_ENTRY              *FormListLink;
  REDFISH_FORM            *Form;
  LIST_ENTRY              *StatementListLink;
  REDFISH_STATEMENT       *Statement;
  UINTN                   ChildIndex;
  UINTN                   ChangeCount;
  UINTN                   CurrentDepth;

  //
  // Link all Menus through EFI_IFR_REF_OP
  //
  FormsetListLinkHead = &mHiiRedfishPrivateData.RedfishFormSetList;
  FormsetListLink = GetFirstNode (FormsetListLinkHead);
  while (!IsNull (FormsetListLinkHead, FormsetListLink)) {

    FormSet         = REDFISH_FORMSET_FROM_LINK (FormsetListLink);
    FormsetListLink = GetNextNode (FormsetListLinkHead, FormsetListLink);

    FormListLink = GetFirstNode (&FormSet->RedfishFormList);
    while (!IsNull (&FormSet->RedfishFormList, FormListLink)) {

      Form         = REDFISH_FORM_FROM_LINK (FormListLink);
      FormListLink = GetNextNode (&FormSet->RedfishFormList, FormListLink);

      StatementListLink = GetFirstNode (&Form->RedfishStatementList);
      while (!IsNull (&Form->RedfishStatementList, StatementListLink)) {

        Statement         = REDFISH_STATEMENT_FROM_LINK (StatementListLink);
        StatementListLink = GetNextNode (&Form->RedfishStatementList, StatementListLink);
        if (Statement->HiiStatement->Operand != EFI_IFR_REF_OP) {
          continue;
        }

        Value = &Statement->HiiStatement->Value.Value;
        if (!IsZeroGuid (&Value->ref.FormSetGuid)) {
          continue;
        }

        CurrentMenu = NULL;
        ChildMenu   = NULL;

        for (MenuListIndex = 0; MenuListIndex < MenuListCount; MenuListIndex ++) {

          TempMenu = MenuList + MenuListIndex;

          if (CompareGuid (&TempMenu->FormSetGuid, &FormSet->HiiFormSet->Guid) == TRUE &&
            TempMenu->FormId == Form->HiiForm->FormId && CurrentMenu == NULL) {

            CurrentMenu = TempMenu;
          }
          if (CompareGuid (&TempMenu->FormSetGuid, &FormSet->HiiFormSet->Guid) == TRUE &&
            TempMenu->FormId == Value->ref.FormId && ChildMenu == NULL) {

            if (TempMenu->IsRootMenu == TRUE) {

              //
              // FormSet Root Menu Shouldn't be a child
              //
              break;
            } else {
              ChildMenu = TempMenu;
            }
          }

          if (CurrentMenu != NULL && ChildMenu != NULL &&
            CurrentMenu->ChildCount < MAX_COUNT_REDIFHS_MENU_CHILD) {

            for (ChildIndex = 0; ChildIndex < CurrentMenu->ChildCount; ChildIndex ++) {
              if (CurrentMenu->ChildList[ChildIndex] == ChildMenu) {
                break;
              }
            }

            if (ChildIndex == CurrentMenu->ChildCount) {

              CurrentMenu->ChildList[CurrentMenu->ChildCount] = ChildMenu;
              CurrentMenu->ChildCount ++;
            }

            break;
          }
        }
      }
    }
  }

  //
  // Generate Menu Path for all linked Menus
  //
  CurrentDepth = 1;
  do {

    ChangeCount = 0;
    for (MenuListIndex = 0; MenuListIndex < MenuListCount; MenuListIndex ++) {

      TempMenu = MenuList + MenuListIndex;

      if (CurrentDepth == 1) {

        //
        // Find All Root Menus
        //
        if (TempMenu->IsRootMenu) {

          TempMenu->MenuDepth = 1;
          UnicodeSPrint (
            TempMenu->MenuPath,
            MAX_SIZE_REDFISH_MENU_LENGTH,
            L"./.../%s",
            TempMenu->MenuName
            );
          ChangeCount ++;
        }
      } else {

        if (TempMenu->MenuDepth == CurrentDepth - 1) {

          for (ChildIndex = 0; ChildIndex < TempMenu->ChildCount; ChildIndex ++) {

            TempChildMenu = TempMenu->ChildList[ChildIndex];

            if (TempChildMenu->MenuDepth == 0) {

              TempChildMenu->MenuDepth = CurrentDepth;
              UnicodeSPrint (
                TempChildMenu->MenuPath,
                MAX_SIZE_REDFISH_MENU_LENGTH,
                L"%s/%s",
                TempMenu->MenuPath,
                TempChildMenu->MenuName
                );
              ChangeCount ++;
            }
          }
        }
      }
    }

    CurrentDepth ++;
  } while (ChangeCount != 0);

  //
  // Generate Menu Path for all independent Menus
  //
  for (MenuListIndex = 0; MenuListIndex < MenuListCount; MenuListIndex ++) {

    TempMenu = MenuList + MenuListIndex;
    if (TempMenu->IsRootMenu) {

      for (ChildIndex = 0; ChildIndex < MenuListCount; ChildIndex ++) {

        TempChildMenu = MenuList + ChildIndex;
        if (TempChildMenu->MenuDepth == 0 &&
          CompareGuid (&TempMenu->FormSetGuid, &TempChildMenu->FormSetGuid)) {

          UnicodeSPrint (
            TempChildMenu->MenuPath,
            MAX_SIZE_REDFISH_MENU_LENGTH,
            L"%s/.../%s",
            TempMenu->MenuPath,
            TempChildMenu->MenuName
            );
        }
      }
    }
  }
}

/**
  Retrieve the value of an attribute from HII database.

  @param[in]         Attribute                    The attribute to retrieve value
  @param[out]        AttributeValue               The value retrieved from HII database
  @param[out]        Length                       The size of the value

  @retval  EFI_SUCCESS                            The value has been retrieved successfully
  @retval  Others                                 Another unexpected error occured.

**/
EFI_STATUS
GetAttributeValue (
  IN  EDKII_JSON_VALUE    Attribute,
  OUT VOID                **AttributeValue,
  OUT UINTN               *Length
  )
{
  EFI_STATUS          Status;
  EDKII_JSON_VALUE    AttributeName;
  EDKII_JSON_VALUE    UefiDevicePath;
  EDKII_JSON_VALUE    UefiKeywordName;
  EDKII_JSON_VALUE    UefiNamespaceId;
  CHAR16              *DevStr;
  CHAR16              *KeywordStr;
  CHAR16              *UefiNamespaceIdStr;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (Attribute, EFI_INVALID_PARAMETER);

  Status = EFI_SUCCESS;
  if (JsonObjectGetValue (JsonValueGetObject (Attribute), "UefiKeywordName") == NULL) {

    AttributeName = JsonObjectGetValue (JsonValueGetObject (Attribute), "AttributeName");
    RETURN_STATUS_IF_JSON_VALUE_NOT_STRING (AttributeName, EFI_INVALID_PARAMETER);
    //
    // Check if "Dev" and "QuestionId" pattern in
    // AttributeName.
    // It is EDK2 generated non-keyword Attribute if both of "Dev" and "QuestionId"
    // in AttributeName.
    // It is EDK2 generated keyword Attribute if both of "Dev" and Keyword
    // exist.
    // Otherwise, return EFI_NOT_FOUND for the further processing.
    //
    Status = GetAttributeValueFromAttributeName (
               JsonValueGetAsciiString (AttributeName),
               AttributeValue,
               Length
               );

    if (Status == EFI_NOT_FOUND) {
      Status = RedfishPlatformGetAttributeValue (
                  AttributeName,
                  AttributeValue,
                  Length
                  );
      if (Status == EFI_UNSUPPORTED) {
        Status = EFI_INVALID_PARAMETER;
      }
    }
  } else {
    UefiDevicePath = JsonObjectGetValue (JsonValueGetObject (Attribute), "UefiDevicePath");
    RETURN_STATUS_IF_JSON_VALUE_NOT_STRING (UefiDevicePath, EFI_INVALID_PARAMETER);
    DevStr = JsonValueGetUnicodeString (UefiDevicePath);

    UefiKeywordName = JsonObjectGetValue (JsonValueGetObject (Attribute), "UefiKeywordName");
    RETURN_STATUS_IF_JSON_VALUE_NOT_STRING (UefiKeywordName, EFI_INVALID_PARAMETER);
    KeywordStr = JsonValueGetUnicodeString (UefiKeywordName);

    UefiNamespaceId = JsonObjectGetValue (JsonValueGetObject (Attribute), "UefiNamespaceId");
    RETURN_STATUS_IF_JSON_VALUE_NOT_STRING (UefiNamespaceId, EFI_INVALID_PARAMETER);
    UefiNamespaceIdStr = JsonValueGetUnicodeString (UefiNamespaceId);

    Status = GetAttributeValueFromKeywordConfig (
               UefiNamespaceIdStr,
               DevStr,
               KeywordStr,
               AttributeValue,
               Length
               );

    if (DevStr != NULL) {
      FreePool (DevStr);
    }

    if (KeywordStr != NULL) {
      FreePool (KeywordStr);
    }
  }

  return Status;
}

/**
  Set the string type attribute key-value pair to an attribute set.

  "Key" is the atrribute name and "value" is the attribute value.

  @param[in]        AttributeSet                  The attribute set contains all key-value pairs
  @param[in]        Attribute                     The attribute contains attribute name
  @param[in]        AttributeValue                The string type attribute value
  @param[in]        Length                        The size of the attribute value

**/
VOID
SetStringAttributeValue (
  IN EDKII_JSON_VALUE    AttributeSet,
  IN EDKII_JSON_VALUE    Attribute,
  IN VOID                *AttributeValue,
  IN UINTN               Length
  )
{
  CHAR16              *TempStr;
  EDKII_JSON_VALUE    StrValue;
  EDKII_JSON_VALUE    AttributeName;

  RETURN_IF_JSON_VALUE_NOT_OBJECT (AttributeSet);
  RETURN_IF_JSON_VALUE_NOT_OBJECT (Attribute);

  TempStr = AllocateZeroPool (Length + sizeof (CHAR16));
  if (TempStr == NULL) {

    FreePool (AttributeValue);
    return;
  }
  CopyMem (TempStr, AttributeValue, Length);

  StrValue = JsonValueInitUnicodeString (TempStr);
  AttributeName = JsonObjectGetValue (JsonValueGetObject (Attribute), "AttributeName");
  RETURN_IF_JSON_VALUE_NOT_STRING (AttributeName);

  JsonObjectSetValue (
    AttributeSet,
    JsonValueGetAsciiString (AttributeName),
    StrValue
    );
  JsonValueFree (StrValue);

  FreePool (TempStr);
}

/**
  Set the number type attribute key-value pair to an attribute set.

  "Key" is the atrribute name and "value" is the attribute value.

  @param[in]        AttributeSet                  The attribute set contains all key-value pairs
  @param[in]        Attribute                     The attribute contains attribute name
  @param[in]        AttributeValue                The number type attribute value
  @param[in]        Length                        The size of the attribute value

**/
VOID
SetNumberAttributeValue (
  IN EDKII_JSON_VALUE    AttributeSet,
  IN EDKII_JSON_VALUE    Attribute,
  IN VOID                *AttributeValue,
  IN UINTN               Length
  )
{
  EDKII_JSON_VALUE    NumberValue;
  EDKII_JSON_VALUE    AttributeName;
  UINT64              TempNumber;

  RETURN_IF_JSON_VALUE_NOT_OBJECT (AttributeSet);
  RETURN_IF_JSON_VALUE_NOT_OBJECT (Attribute);


  switch (Length) {

  case 1:
    TempNumber = *((UINT8 *) AttributeValue);
    break;

  case 2:
    TempNumber = *((UINT16 *) AttributeValue);
    break;

  case 4:
    TempNumber = *((UINT32 *) AttributeValue);
    break;

  case 8:
    TempNumber = *((UINT64 *) AttributeValue);
    break;

  default:
    FreePool (AttributeValue);
    return;
  }

  NumberValue = JsonValueInitNumber (TempNumber);
  if (NumberValue == NULL) {
    return;
  }

  AttributeName = JsonObjectGetValue (JsonValueGetObject (Attribute), "AttributeName");
  RETURN_IF_JSON_VALUE_NOT_STRING (AttributeName);

  JsonObjectSetValue (
    AttributeSet,
    JsonValueGetAsciiString (AttributeName),
    NumberValue
    );
  JsonValueFree (NumberValue);

  return;
}

/**
  Set the boolean type attribute key-value pair to an attribute set.

  "Key" is the atrribute name and "value" is the attribute value.

  @param[in]        AttributeSet                  The attribute set contains all key-value pairs
  @param[in]        Attribute                     The attribute contains attribute name
  @param[in]        AttributeValue                The boolean type attribute value
  @param[in]        Length                        The size of the attribute value

**/
VOID
SetBooleanAttributeValue (
  IN EDKII_JSON_VALUE    AttributeSet,
  IN EDKII_JSON_VALUE    Attribute,
  IN VOID                *AttributeValue,
  IN UINTN               Length
  )
{
  EDKII_JSON_VALUE    AttributeName;

  RETURN_IF_JSON_VALUE_NOT_OBJECT (AttributeSet);
  RETURN_IF_JSON_VALUE_NOT_OBJECT (Attribute);

  AttributeName = JsonObjectGetValue (JsonValueGetObject (Attribute), "AttributeName");
  RETURN_IF_JSON_VALUE_NOT_STRING (AttributeName);

  JsonObjectSetValue (
    AttributeSet,
    JsonValueGetAsciiString (AttributeName),
    JsonValueInitBoolean (*((BOOLEAN *) AttributeValue))
    );

  return;
}

/**
  Set time information for last route to Redfish settings.

  "Key" is "Time" and value format is "yyyy-mm-ddThh:mm:ss+00:00+00:00".

  @param[in]        RedfishSettings             The Redfish settings to be set
  @param[in]        Storage                     The storage which contains last route time

**/
EFI_STATUS
SetTimeToRedfishSettings (
  IN EDKII_JSON_VALUE              RedfishSettings,
  IN HII_REDFISH_LIB_NV_STORAGE    *Storage
  )
{
  EFI_STATUS        Status;
  CHAR8             TimeStr[sizeof("yyyy-mm-ddThh:mm:ss+00:00")];
  CHAR8             TimeZoneStr[sizeof("+00:00")];
  EDKII_JSON_VALUE  JsonTemp;

  if (Storage->TimeOfLastRoute.TimeZone > 0) {
    AsciiSPrint (
      TimeZoneStr,
      sizeof (TimeZoneStr),
      "%a%02d:%02d",
      "-",
      Storage->TimeOfLastRoute.TimeZone / 60,
      Storage->TimeOfLastRoute.TimeZone % 60
      );
  } else {
    AsciiSPrint (
      TimeZoneStr,
      sizeof (TimeZoneStr),
      "%a%02d:%02d", "+",
      (0 - Storage->TimeOfLastRoute.TimeZone) / 60,
      (0 - Storage->TimeOfLastRoute.TimeZone) % 60
      );
  }

  AsciiSPrint (
    TimeStr,
    sizeof (TimeStr),
    "%04d-%02d-%02dT%02d:%02d:%02d%a",
    Storage->TimeOfLastRoute.Year,
    Storage->TimeOfLastRoute.Month,
    Storage->TimeOfLastRoute.Day,
    Storage->TimeOfLastRoute.Hour,
    Storage->TimeOfLastRoute.Minute,
    Storage->TimeOfLastRoute.Second,
    TimeZoneStr
    );

  JsonTemp = JsonValueInitAsciiString (TimeStr);
  Status   = JsonObjectSetValue (
               JsonValueGetObject (RedfishSettings),
               "Time",
               JsonTemp
               );
  JsonValueFree (JsonTemp);

  return Status;
}


/**
  Save value to a question in HII Database, this question can be identified by keyword if is supported,
  or attribute name if is not.

  Attribute name is unique for each question and consists by <NameSpace + Device Alias + Question Id>
  if it is a non-keyword support attribute or <NameSpace + Device Alias + Keyword> if it has keyword
  support.

  Only "String" or "Integer" value can be set through keyword.

  @param[in]      AttributeName            The name for this attribute to identify question.
  @param[in]      AttributeValue           The value of this attribute to save to a certain question.
  @param[in]      DevPathStr               The device path string to indicate which device this attribute
                                           locates in.
  @param[in]      NamespaceStr             The namespace this keyword assoicated with.
  @param[in]      IsKeywordSupported       To indicate if keyword is supported by this Attribute.
  @param[in]      KeywordType              To indicate what kind of value this keyword needs to save.
  @param[in]      Keyword                  The keyword name for this question.

  @retval  EFI_SUCCESS                     This Attribute has been saved successfully.
  @retval  Others                          Another unexpected error occured.

**/
EFI_STATUS
SaveAttributeValue  (
  IN CHAR8               *AttributeName,
  IN EDKII_JSON_VALUE    AttributeValue,
  IN CHAR16              *DevPathStr,
  IN CHAR16              *NamespaceStr,
  IN BOOLEAN             IsKeywordSupported,
  IN CHAR8               *KeywordType,    OPTIONAL
  IN CHAR16              *Keyword         OPTIONAL
  )
{
  EFI_STATUS           Status;
  REDFISH_FORMSET      *FormSet;
  REDFISH_FORM         *Form;
  REDFISH_STATEMENT    *Statement;

  if (IsKeywordSupported && DevPathStr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  FormSet   = NULL;
  Form      = NULL;
  Statement = NULL;

  //
  // Check if "Dev" and "QuestionId" pattern in
  // AttributeName.
  // It is EDK2 generated non-keyword Attribute if both of "Dev" and "QuestionId"
  // in AttributeName.
  // It is EDK2 generated keyword Attribute if both of "Dev" and Keyword
  // exist.
  // Otherwise, return EFI_NOT_FOUND for the further processing.
  //
  Status = FindQuestion (
             &mHiiRedfishPrivateData.RedfishFormSetList,
             AttributeName,
             Keyword,
             &FormSet,
             &Form,
             &Statement
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!IsKeywordSupported) {
    Status = SaveAttributeValueByQuestion (
               FormSet,
               Form,
               Statement,
               AttributeValue
               );
  } else {
    Status = SaveAttributeValueByKeyword (
               Statement,
               NamespaceStr,
               DevPathStr,
               Keyword,
               AttributeValue
               );
  }

  return Status;
}
