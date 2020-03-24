/** @file
  Basic Functions for HII operations.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "HiiInternal.h"

CHAR16    *mUnknownString = L"!";

/**
  Allocate new memory and then copy the Unicode string Source to Destination.

  @param  Dest                   Location to copy string
  @param  Src                    String to copy

**/
VOID
NewStringCpy (
  IN OUT CHAR16    **Dest,
  IN     CHAR16    *Src
  )
{
  if (*Dest != NULL) {
    FreePool (*Dest);
  }
  *Dest = AllocateCopyPool (StrSize (Src), Src);
}

/**
  Set Value of given Name in a NameValue Storage.

  @param  Storage                The NameValue Storage.
  @param  Name                   The Name.
  @param  Value                  The Value to set.
  @param  ReturnNode             The node use the input name.

  @retval EFI_SUCCESS            Value found for given Name.
  @retval EFI_NOT_FOUND          No such Name found in NameValue storage.

**/
EFI_STATUS
SetValueByName (
  IN     HII_FORMSET_STORAGE    *Storage,
  IN     CHAR16                 *Name,
  IN     CHAR16                 *Value,
     OUT HII_NAME_VALUE_NODE    **ReturnNode
  )
{
  LIST_ENTRY              *Link;
  HII_NAME_VALUE_NODE     *Node;
  CHAR16                  *Buffer;

  Link = GetFirstNode (&Storage->NameValueList);
  while (!IsNull (&Storage->NameValueList, Link)) {
    Node = HII_NAME_VALUE_NODE_FROM_LINK (Link);

    if (StrCmp (Name, Node->Name) == 0) {

      Buffer = Node->Value;
      if (Buffer != NULL) {
        FreePool (Buffer);
      }
      Buffer = AllocateCopyPool (StrSize (Value), Value);
      if (Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }
      Node->Value = Buffer;

      if (ReturnNode != NULL) {
        *ReturnNode = Node;
      }

      return EFI_SUCCESS;
    }

    Link = GetNextNode (&Storage->NameValueList, Link);
  }

  return EFI_NOT_FOUND;
}


/**
  Get bit field value from the buffer and then set the value for the question.
  Note: Data type UINT32 can cover all the bit field value.

  @param  Question        The question refer to bit field.
  @param  Buffer          Point to the buffer which the question value get from.
  @param  QuestionValue   The Question Value retrieved from Bits.

**/
VOID
GetBitsQuestionValue (
  IN     HII_STATEMENT          *Question,
  IN     UINT8                  *Buffer,
     OUT HII_STATEMENT_VALUE    *QuestionValue
  )
{
  UINTN    StartBit;
  UINTN    EndBit;
  UINT32   RetVal;
  UINT32   BufferValue;

  StartBit = Question->BitVarOffset % 8;
  EndBit = StartBit + Question->BitStorageWidth - 1;

  CopyMem ((UINT8 *) &BufferValue, Buffer, Question->StorageWidth);

  RetVal = BitFieldRead32 (BufferValue, StartBit, EndBit);

  //
  // Set question value.
  // Note: Since Question with BufferValue (orderedlist, password, string)are not supported to refer bit field.
  // Only oneof/checkbox/oneof can support bit field.So we can copy the value to the Hiivalue of Question directly.
  //
  CopyMem ((UINT8 *) &QuestionValue->Value, (UINT8 *) &RetVal, Question->StorageWidth);
}

/**
  Set bit field value to the buffer.
  Note: Data type UINT32 can cover all the bit field value.

  @param  Question        The question refer to bit field.
  @param  Buffer          Point to the buffer which the question value set to.
  @param  Value           The bit field value need to set.

**/
VOID
SetBitsQuestionValue (
  IN     HII_STATEMENT    *Question,
  IN OUT UINT8            *Buffer,
  IN     UINT32           Value
  )
{
  UINT32   Operand;
  UINTN    StartBit;
  UINTN    EndBit;
  UINT32   RetVal;

  StartBit = Question->BitVarOffset % 8;
  EndBit = StartBit + Question->BitStorageWidth - 1;

  CopyMem ((UINT8*) &Operand, Buffer, Question->StorageWidth);
  RetVal = BitFieldWrite32 (Operand, StartBit, EndBit, Value);
  CopyMem (Buffer, (UINT8*) &RetVal, Question->StorageWidth);
}

/**
  Convert the buffer value to HiiValue.

  @param  Question              The question.
  @param  Value                 Unicode buffer save the question value.
  @param  QuestionValue         The Question Value retrieved from Buffer.

  @retval  Status whether convert the value success.

**/
EFI_STATUS
BufferToValue (
  IN     HII_STATEMENT          *Question,
  IN     CHAR16                 *Value,
     OUT HII_STATEMENT_VALUE    *QuestionValue
  )
{
  CHAR16                *StringPtr;
  BOOLEAN               IsBufferStorage;
  CHAR16                *DstBuf;
  CHAR16                TempChar;
  UINTN                 LengthStr;
  UINT8                 *Dst;
  CHAR16                TemStr[5];
  UINTN                 Index;
  UINT8                 DigitUint8;
  BOOLEAN               IsString;
  UINTN                 Length;
  EFI_STATUS            Status;
  UINT8                 *Buffer;

  Buffer = NULL;

  IsString = (BOOLEAN) ((QuestionValue->Type == EFI_IFR_TYPE_STRING) ?  TRUE : FALSE);
  if (Question->Storage->Type == EFI_HII_VARSTORE_BUFFER ||
      Question->Storage->Type == EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER) {
    IsBufferStorage = TRUE;
  } else {
    IsBufferStorage = FALSE;
  }

  //
  // Question Value is provided by Buffer Storage or NameValue Storage
  //
  if (QuestionValue->Type == EFI_IFR_TYPE_STRING || QuestionValue->Type == EFI_IFR_TYPE_BUFFER) {
    //
    // This Question is password or orderedlist
    //
    Dst = QuestionValue->Buffer;
  } else {
    //
    // Other type of Questions
    //
    if (Question->QuestionReferToBitField) {
      Buffer = (UINT8 *)AllocateZeroPool (Question->StorageWidth);
      if (Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }
      Dst = Buffer;
    } else {
      Dst = (UINT8 *) &QuestionValue->Value;
    }
  }

  //
  // Temp cut at the end of this section, end with '\0' or '&'.
  //
  StringPtr = Value;
  while (*StringPtr != L'\0' && *StringPtr != L'&') {
    StringPtr++;
  }
  TempChar = *StringPtr;
  *StringPtr = L'\0';

  LengthStr = StrLen (Value);

  //
  // Value points to a Unicode hexadecimal string, we need to convert the string to the value with CHAR16/UINT8...type.
  // When generating the Value string, we follow this rule: 1 byte -> 2 Unicode characters (for string: 2 byte(CHAR16) ->4 Unicode characters).
  // So the maximum value string length of a question is : Question->StorageWidth * 2.
  // If the value string length > Question->StorageWidth * 2, only set the string length as Question->StorageWidth * 2, then convert.
  //
  if (LengthStr > (UINTN) Question->StorageWidth * 2) {
    Length = (UINTN) Question->StorageWidth * 2;
  } else {
    Length = LengthStr;
  }

  Status    = EFI_SUCCESS;
  if (!IsBufferStorage && IsString) {
    //
    // Convert Config String to Unicode String, e.g "0041004200430044" => "ABCD"
    // Add string tail char L'\0' into Length
    //
    DstBuf = (CHAR16 *) Dst;
    ZeroMem (TemStr, sizeof (TemStr));
    for (Index = 0; Index < Length; Index += 4) {
      StrnCpyS (TemStr, sizeof (TemStr) / sizeof (CHAR16), Value + Index, 4);
      DstBuf[Index/4] = (CHAR16) StrHexToUint64 (TemStr);
    }
    //
    // Add tailing L'\0' character
    //
    DstBuf[Index/4] = L'\0';
  } else {
    ZeroMem (TemStr, sizeof (TemStr));
    for (Index = 0; Index < Length; Index ++) {
      TemStr[0] = Value[LengthStr - Index - 1];
      DigitUint8 = (UINT8) StrHexToUint64 (TemStr);
      if ((Index & 1) == 0) {
        Dst [Index/2] = DigitUint8;
      } else {
        Dst [Index/2] = (UINT8) ((DigitUint8 << 4) + Dst [Index/2]);
      }
    }
  }

  *StringPtr = TempChar;

  if (Buffer != NULL && Question->QuestionReferToBitField) {
    GetBitsQuestionValue (Question, Buffer, QuestionValue);
    FreePool (Buffer);
  }

  return Status;
}

/**
  Get the string based on the StringId and HII Package List Handle.

  @param  Token                  The String's ID.
  @param  HiiHandle              The package list in the HII database to search for
                                 the specified string.

  @return The output string.

**/
CHAR16*
GetToken (
  IN EFI_STRING_ID     Token,
  IN EFI_HII_HANDLE    HiiHandle
  )
{
  EFI_STRING  String;

  if (HiiHandle == NULL) {
    return NULL;
  }

  String = HiiGetString (HiiHandle, Token, NULL);
  if (String == NULL) {
    String = AllocateCopyPool (StrSize (mUnknownString), mUnknownString);
    if (String == NULL) {
      return NULL;
    }

  }
  return (CHAR16 *) String;
}

/**
  Converts the unicode character of the string from uppercase to lowercase.
  This is a internal function.

  @param ConfigString  String to be converted

**/
VOID
EFIAPI
HiiToLower (
  IN EFI_STRING    ConfigString
  )
{
  EFI_STRING  String;
  BOOLEAN     Lower;

  //
  // Convert all hex digits in range [A-F] in the configuration header to [a-f]
  //
  for (String = ConfigString, Lower = FALSE; *String != L'\0'; String++) {
    if (*String == L'=') {
      Lower = TRUE;
    } else if (*String == L'&') {
      Lower = FALSE;
    } else if (Lower && *String >= L'A' && *String <= L'F') {
      *String = (CHAR16) (*String - L'A' + L'a');
    }
  }
}

/**
  Evaluate if the result is a non-zero value.

  @param  Result           The result to be evaluated.

  @retval TRUE             It is a non-zero value.
  @retval FALSE            It is a zero value.

**/
BOOLEAN
IsTrue (
  IN EFI_HII_VALUE    *Result
  )
{
  switch (Result->Type) {
  case EFI_IFR_TYPE_BOOLEAN:
    return Result->Value.b;

  case EFI_IFR_TYPE_NUM_SIZE_8:
    return (BOOLEAN)(Result->Value.u8 != 0);

  case EFI_IFR_TYPE_NUM_SIZE_16:
    return (BOOLEAN)(Result->Value.u16 != 0);

  case EFI_IFR_TYPE_NUM_SIZE_32:
    return (BOOLEAN)(Result->Value.u32 != 0);

  case EFI_IFR_TYPE_NUM_SIZE_64:
    return (BOOLEAN)(Result->Value.u64 != 0);

  default:
    return FALSE;
  }
}

/**
  Set a new string to string package.

  @param[in]  String              A pointer to the Null-terminated Unicode string
                                  to add or update in the String Package associated
                                  with HiiHandle.
  @param[in]  HiiHandle           A handle that was previously registered in the
                                  HII Database.

  @return the Id for this new string.

**/
EFI_STRING_ID
NewString (
  IN CHAR16            *String,
  IN EFI_HII_HANDLE    HiiHandle
  )
{
  EFI_STRING_ID  StringId;

  StringId = HiiSetString (HiiHandle, 0, String, NULL);
  return StringId;
}

/**
  Perform nosubmitif check for a Form.

  @param  FormSet                FormSet data structure.
  @param  Form                   Form data structure.
  @param  Question               The Question to be validated.

  @retval EFI_SUCCESS            Form validation pass.
  @retval other                  Form validation failed.

**/
EFI_STATUS
ValidateNoSubmit (
  IN HII_FORMSET      *FormSet,
  IN HII_FORM         *Form,
  IN HII_STATEMENT    *Question
  )
{
  EFI_STATUS              Status;
  LIST_ENTRY              *Link;
  LIST_ENTRY              *ListHead;
  HII_EXPRESSION          *Expression;

  ListHead = &Question->NoSubmitListHead;
  Link = GetFirstNode (ListHead);
  while (!IsNull (ListHead, Link)) {
    Expression = HII_EXPRESSION_FROM_LINK (Link);

    //
    // Evaluate the expression
    //
    Status = EvaluateHiiExpression (FormSet, Form, Expression);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (IsTrue (&Expression->Result)) {
      return EFI_NOT_READY;
    }

    Link = GetNextNode (ListHead, Link);
  }

  return EFI_SUCCESS;
}

/**
  Perform NoSubmit check for each Form in FormSet.

  @param  FormSet                FormSet data structure.
  @param  CurrentForm            Current input form data structure.
  @param  Statement              The statement for this check.

  @retval EFI_SUCCESS            Form validation pass.
  @retval other                  Form validation failed.

**/
EFI_STATUS
NoSubmitCheck (
  IN     HII_FORMSET      *FormSet,
  IN OUT HII_FORM         **CurrentForm,
     OUT HII_STATEMENT    **Statement
  )
{
  EFI_STATUS              Status;
  LIST_ENTRY              *Link;
  HII_STATEMENT           *Question;
  HII_FORM                *Form;
  LIST_ENTRY              *LinkForm;

  LinkForm = GetFirstNode (&FormSet->FormListHead);
  while (!IsNull (&FormSet->FormListHead, LinkForm)) {
    Form = HII_FORM_FROM_LINK (LinkForm);
    LinkForm = GetNextNode (&FormSet->FormListHead, LinkForm);

    if (*CurrentForm != NULL && *CurrentForm != Form) {
      continue;
    }

    Link = GetFirstNode (&Form->StatementListHead);
    while (!IsNull (&Form->StatementListHead, Link)) {
      Question = HII_STATEMENT_FROM_LINK (Link);
      Status = ValidateNoSubmit (FormSet, Form, Question);
      if (EFI_ERROR (Status)) {
        if (*CurrentForm == NULL) {
          *CurrentForm = Form;
        }
        if (Statement != NULL) {
          *Statement = Question;
        }
        return Status;
      }

      Link = GetNextNode (&Form->StatementListHead, Link);
    }
  }

  return EFI_SUCCESS;
}

/**
  Allocate new memory and concatinate Source on the end of Destination.

  @param  Dest                   String to added to the end of.
  @param  Src                    String to concatinate.

**/
VOID
NewStringCat (
  IN OUT CHAR16    **Dest,
  IN     CHAR16    *Src
  )
{
  CHAR16  *NewString;
  UINTN   MaxLen;

  if (*Dest == NULL) {

    NewStringCpy (Dest, Src);
    return;
  }

  MaxLen    = ( StrSize (*Dest) + StrSize (Src) - 1) / sizeof (CHAR16);
  NewString = AllocateZeroPool (MaxLen * sizeof (CHAR16));
  if (NewString == NULL) {
    return;
  }

  StrCpyS (NewString, MaxLen, *Dest);
  StrCatS (NewString, MaxLen, Src);

  FreePool (*Dest);
  *Dest = NewString;
}


/**
  Convert setting of Buffer Storage or NameValue Storage to <ConfigResp>.

  @param  Storage                The Storage to be conveted.
  @param  ConfigResp             The returned <ConfigResp>.
  @param  ConfigRequest          The ConfigRequest string.

  @retval EFI_SUCCESS            Convert success.
  @retval EFI_INVALID_PARAMETER  Incorrect storage type.

**/
EFI_STATUS
StorageToConfigResp (
  IN HII_FORMSET_STORAGE    *Storage,
  IN CHAR16                 **ConfigResp,
  IN CHAR16                 *ConfigRequest
  )
{
  EFI_STATUS                       Status;
  EFI_STRING                       Progress;
  LIST_ENTRY                       *Link;
  HII_NAME_VALUE_NODE              *Node;
  UINT8                            *SourceBuf;
  EFI_HII_CONFIG_ROUTING_PROTOCOL  *HiiConfigRouting;
  EFI_STRING                       TempConfigRequest;
  UINTN                            RequestStrSize;

  Status = EFI_SUCCESS;

  if (Storage->ConfigHdr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (ConfigRequest != NULL) {

    TempConfigRequest = AllocateCopyPool (StrSize (ConfigRequest), ConfigRequest);
    if (TempConfigRequest == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  } else {

    RequestStrSize = (StrLen (Storage->ConfigHdr) + StrLen (Storage->ConfigRequest) + 1) * sizeof (CHAR16);
    TempConfigRequest = AllocateZeroPool (RequestStrSize);
    if (TempConfigRequest == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    UnicodeSPrint (
      TempConfigRequest,
      RequestStrSize,
      L"%s%s",
      Storage->ConfigHdr,
      Storage->ConfigRequest
      );
  }

  switch (Storage->Type) {
  case EFI_HII_VARSTORE_BUFFER:
  case EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER:

    Status = gBS->LocateProtocol (
                &gEfiHiiConfigRoutingProtocolGuid,
                NULL,
                (VOID **) &HiiConfigRouting
                );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    SourceBuf = Storage->Buffer;
    Status = HiiConfigRouting->BlockToConfig (
                                 HiiConfigRouting,
                                 TempConfigRequest,
                                 SourceBuf,
                                 Storage->Size,
                                 ConfigResp,
                                 &Progress
                                 );
    break;

  case EFI_HII_VARSTORE_NAME_VALUE:

    *ConfigResp = NULL;
    NewStringCat (ConfigResp, Storage->ConfigHdr);

    Link = GetFirstNode (&Storage->NameValueList);
    while (!IsNull (&Storage->NameValueList, Link)) {
      Node = HII_NAME_VALUE_NODE_FROM_LINK (Link);

      if (StrStr (TempConfigRequest, Node->Name) != NULL) {
        NewStringCat (ConfigResp, L"&");
        NewStringCat (ConfigResp, Node->Name);
        NewStringCat (ConfigResp, L"=");
        NewStringCat (ConfigResp, Node->Value);
      }
      Link = GetNextNode (&Storage->NameValueList, Link);
    }
    break;

  case EFI_HII_VARSTORE_EFI_VARIABLE:
  default:
    Status = EFI_INVALID_PARAMETER;
    break;
  }

  return Status;
}


/**
  Convert <ConfigResp> to settings in Buffer Storage or NameValue Storage.

  @param  Storage                The Storage to receive the settings.
  @param  ConfigResp             The <ConfigResp> to be converted.

  @retval EFI_SUCCESS            Convert success.
  @retval EFI_INVALID_PARAMETER  Incorrect storage type.

**/
EFI_STATUS
ConfigRespToStorage (
  IN HII_FORMSET_STORAGE    *Storage,
  IN CHAR16                 *ConfigResp
  )
{
  EFI_STATUS  Status;
  EFI_STRING  Progress;
  UINTN       BufferSize;
  CHAR16      *StrPtr;
  CHAR16      *Name;
  CHAR16      *Value;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;

  Status = EFI_SUCCESS;

  switch (Storage->Type) {
  case EFI_HII_VARSTORE_BUFFER:
  case EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER:

    Status = gBS->LocateProtocol (
                    &gEfiHiiConfigRoutingProtocolGuid,
                    NULL,
                    (VOID **) &HiiConfigRouting
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    BufferSize = Storage->Size;
    Status = HiiConfigRouting->ConfigToBlock (
                                  HiiConfigRouting,
                                  ConfigResp,
                                  Storage->Buffer,
                                  &BufferSize,
                                  &Progress
                                  );
    break;

  case EFI_HII_VARSTORE_NAME_VALUE:
    StrPtr = StrStr (ConfigResp, L"PATH");
    if (StrPtr == NULL) {
      break;
    }
    StrPtr = StrStr (ConfigResp, L"&");
    while (StrPtr != NULL) {
      //
      // Skip '&'
      //
      StrPtr = StrPtr + 1;
      Name = StrPtr;
      StrPtr = StrStr (StrPtr, L"=");
      if (StrPtr == NULL) {
        break;
      }
      *StrPtr = 0;

      //
      // Skip '='
      //
      StrPtr = StrPtr + 1;
      Value = StrPtr;
      StrPtr = StrStr (StrPtr, L"&");
      if (StrPtr != NULL) {
        *StrPtr = 0;
      }
      SetValueByName (Storage, Name, Value, NULL);
    }
    break;

  case EFI_HII_VARSTORE_EFI_VARIABLE:
  default:
    Status = EFI_INVALID_PARAMETER;
    break;
  }

  return Status;
}

/**
  Fetch the Ifr binary data of a FormSet.

  @param  Handle                 PackageList Handle
  @param  FormSetGuid            On input, GUID or class GUID of a formset. If not
                                 specified (NULL or zero GUID), take the first
                                 FormSet with class GUID EFI_HII_PLATFORM_SETUP_FORMSET_GUID
                                 found in package list.
                                 On output, GUID of the formset found(if not NULL).
  @param  BinaryLength           The length of the FormSet IFR binary.
  @param  BinaryData             The buffer designed to receive the FormSet.

  @retval EFI_SUCCESS            Buffer filled with the requested FormSet.
                                 BufferLength was updated.
  @retval EFI_INVALID_PARAMETER  The handle is unknown.
  @retval EFI_NOT_FOUND          A form or FormSet on the requested handle cannot
                                 be found with the requested FormId.

**/
EFI_STATUS
GetIfrBinaryData (
  IN     EFI_HII_HANDLE    Handle,
  IN OUT EFI_GUID          *FormSetGuid,
     OUT UINTN             *BinaryLength,
     OUT UINT8             **BinaryData
  )
{
  EFI_STATUS                   Status;
  EFI_HII_PACKAGE_LIST_HEADER  *HiiPackageList;
  UINTN                        BufferSize;
  UINT8                        *Package;
  UINT8                        *OpCodeData;
  UINT32                       Offset;
  UINT32                       Offset2;
  UINT32                       PackageListLength;
  EFI_HII_PACKAGE_HEADER       PackageHeader;
  UINT8                        Index;
  UINT8                        NumberOfClassGuid;
  BOOLEAN                      ClassGuidMatch;
  EFI_GUID                     *ClassGuid;
  EFI_GUID                     *ComparingGuid;
  EFI_HII_DATABASE_PROTOCOL    *HiiDatabase;

  OpCodeData = NULL;
  Package = NULL;
  ZeroMem (&PackageHeader, sizeof (EFI_HII_PACKAGE_HEADER));

  //
  // if FormSetGuid is NULL or zero GUID, return first Setup FormSet in the package list
  //
  if (FormSetGuid == NULL) {
    ComparingGuid = &gZeroGuid;
  } else {
    ComparingGuid = FormSetGuid;
  }

  //
  // Get HII PackageList
  //
  Status = gBS->LocateProtocol (
                  &gEfiHiiDatabaseProtocolGuid,
                  NULL,
                  (VOID **) &HiiDatabase
                  );
  if (EFI_ERROR (Status)) {
    *BinaryData = NULL;
    return Status;
  }

  BufferSize = 0;
  HiiPackageList = NULL;
  Status = HiiDatabase->ExportPackageLists (HiiDatabase, Handle, &BufferSize, HiiPackageList);
  if (Status == EFI_BUFFER_TOO_SMALL) {

    HiiPackageList = AllocatePool (BufferSize);
    if (HiiPackageList == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    Status = HiiDatabase->ExportPackageLists (HiiDatabase, Handle, &BufferSize, HiiPackageList);
  }
  if (EFI_ERROR (Status)) {

    FreePool (HiiPackageList);
    *BinaryData = NULL;
    return Status;
  }

  //
  // Get Form package from this HII package List
  //
  Offset = sizeof (EFI_HII_PACKAGE_LIST_HEADER);
  Offset2 = 0;
  CopyMem (&PackageListLength, &HiiPackageList->PackageLength, sizeof (UINT32));

  ClassGuidMatch = FALSE;
  while (Offset < PackageListLength) {

    Package = ((UINT8 *) HiiPackageList) + Offset;
    CopyMem (&PackageHeader, Package, sizeof (EFI_HII_PACKAGE_HEADER));

    if (PackageHeader.Type == EFI_HII_PACKAGE_FORMS) {
      //
      // Search FormSet in this Form Package
      //

      Offset2 = sizeof (EFI_HII_PACKAGE_HEADER);
      while (Offset2 < PackageHeader.Length) {
        OpCodeData = Package + Offset2;

        if (((EFI_IFR_OP_HEADER *) OpCodeData)->OpCode == EFI_IFR_FORM_SET_OP) {
          //
          // Try to compare against formset GUID
          //

          if (IsZeroGuid (FormSetGuid) ||
              CompareGuid (ComparingGuid, (EFI_GUID *) (OpCodeData + sizeof (EFI_IFR_OP_HEADER)))) {
            break;
          }

          if (((EFI_IFR_OP_HEADER *) OpCodeData)->Length > OFFSET_OF (EFI_IFR_FORM_SET, Flags)) {
            //
            // Try to compare against formset class GUID
            //
            NumberOfClassGuid = (UINT8) (((EFI_IFR_FORM_SET *) OpCodeData)->Flags & 0x3);
            ClassGuid         = (EFI_GUID *) (OpCodeData + sizeof (EFI_IFR_FORM_SET));
            for (Index = 0; Index < NumberOfClassGuid; Index++) {
              if (CompareGuid (ComparingGuid, ClassGuid + Index)) {
                ClassGuidMatch = TRUE;
                break;
              }
            }
            if (ClassGuidMatch) {
              break;
            }
          } else if (ComparingGuid == &gEfiHiiPlatformSetupFormsetGuid) {
            ClassGuidMatch = TRUE;
            break;
          }
        }

        Offset2 += ((EFI_IFR_OP_HEADER *) OpCodeData)->Length;
      }

      if (Offset2 < PackageHeader.Length) {
        //
        // Target formset found
        //
        break;
      }
    }

    Offset += PackageHeader.Length;
  }

  if (Offset >= PackageListLength) {
    //
    // Form package not found in this Package List
    //
    FreePool (HiiPackageList);
    *BinaryData = NULL;
    return EFI_NOT_FOUND;
  }

  if (FormSetGuid != NULL) {
    //
    // Return the FormSet GUID
    //
    CopyMem (FormSetGuid, &((EFI_IFR_FORM_SET *) OpCodeData)->Guid, sizeof (EFI_GUID));
  }

  //
  // To determine the length of a whole FormSet IFR binary, one have to parse all the Opcodes
  // in this FormSet; So, here just simply copy the data from start of a FormSet to the end
  // of the Form Package.
  //

  *BinaryLength = PackageHeader.Length - Offset2;
  *BinaryData = AllocateCopyPool (*BinaryLength, OpCodeData);
  FreePool (HiiPackageList);
  if (*BinaryData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}

/**
  Check if the requested element is in storage.

  @param  Storage                   The storage contains elements.
  @param  RequestElement            The element to be searched.

**/
BOOLEAN
ElementValidation (
  IN HII_FORMSET_STORAGE    *Storage,
  IN CHAR16                 *RequestElement
  )
{
  return StrStr (Storage->ConfigRequest, RequestElement) != NULL ? TRUE : FALSE;
}

/**
  Append the Request element to the Config Request.

  @param  ConfigRequest          Current ConfigRequest info.
  @param  SpareStrLen            Current remain free buffer for config reqeust.
  @param  RequestElement         New Request element.

**/
VOID
AppendConfigRequest (
  IN OUT CHAR16               **ConfigRequest,
  IN OUT UINTN                *SpareStrLen,
  IN     CHAR16               *RequestElement
  )
{
  CHAR16   *NewStr;
  UINTN    StringSize;
  UINTN    StrLength;
  UINTN    MaxLen;

  StrLength = StrLen (RequestElement);
  StringSize = (*ConfigRequest != NULL) ? StrSize (*ConfigRequest) : sizeof (CHAR16);
  MaxLen = StringSize / sizeof (CHAR16) + *SpareStrLen;

  //
  // Append <RequestElement> to <ConfigRequest>
  //
  if (StrLength > *SpareStrLen) {
    //
    // Old String buffer is not sufficient for RequestElement, allocate a new one
    //
    MaxLen = StringSize / sizeof (CHAR16) + CONFIG_REQUEST_STRING_INCREMENTAL;
    NewStr = AllocateZeroPool (MaxLen * sizeof (CHAR16));
    if (NewStr == NULL) {
      return;
    }

    if (*ConfigRequest != NULL) {
      CopyMem (NewStr, *ConfigRequest, StringSize);
      FreePool (*ConfigRequest);
    }
    *ConfigRequest = NewStr;
    *SpareStrLen   = CONFIG_REQUEST_STRING_INCREMENTAL;
  }

  StrCatS (*ConfigRequest, MaxLen, RequestElement);
  *SpareStrLen -= StrLength;
}


/**
  Adjust the config request info, remove the request elements which already in AllConfigRequest string.

  @param  Storage                Form set Storage.
  @param  Request                The input request string.
  @param  RespString             Whether the input is ConfigRequest or ConfigResp format.

  @retval TRUE                   Has element not covered by current used elements, need to continue to call ExtractConfig
  @retval FALSE                  All elements covered by current used elements.

**/
BOOLEAN
ConfigRequestAdjust (
  IN HII_FORMSET_STORAGE    *Storage,
  IN CHAR16                 *Request,
  IN BOOLEAN                RespString
  )
{
  CHAR16       *RequestElement;
  CHAR16       *NextRequestElement;
  CHAR16       *NextElementBakup;
  CHAR16       *SearchKey;
  CHAR16       *ValueKey;
  BOOLEAN      RetVal;
  CHAR16       *ConfigRequest;

  RetVal           = FALSE;
  NextElementBakup = NULL;
  ValueKey         = NULL;

  if (Request != NULL) {
    ConfigRequest = Request;
  } else {
    ConfigRequest = Storage->ConfigRequest;
  }

  if (Storage->ConfigRequest == NULL) {

    Storage->ConfigRequest = AllocateCopyPool (StrSize (ConfigRequest), ConfigRequest);
    if (Storage->ConfigRequest == NULL) {
      return FALSE;
    }

    return TRUE;
  }

  if (Storage->Type == EFI_HII_VARSTORE_NAME_VALUE) {
    //
    // "&Name1&Name2" section for EFI_HII_VARSTORE_NAME_VALUE storage
    //
    SearchKey = L"&";
  } else {
    //
    // "&OFFSET=####&WIDTH=####" section for EFI_HII_VARSTORE_BUFFER storage
    //
    SearchKey = L"&OFFSET";
    ValueKey  = L"&VALUE";
  }

  //
  // Find SearchKey storage
  //
  if (Storage->Type == EFI_HII_VARSTORE_NAME_VALUE) {

    RequestElement = StrStr (ConfigRequest, L"PATH");
    if (RequestElement == NULL) {
      return FALSE;
    }
    RequestElement = StrStr (RequestElement, SearchKey);
  } else {
    RequestElement = StrStr (ConfigRequest, SearchKey);
  }

  while (RequestElement != NULL) {

    //
    // +1 to avoid find header itself.
    //
    NextRequestElement = StrStr (RequestElement + 1, SearchKey);

    //
    // The last Request element in configRequest string.
    //
    if (NextRequestElement != NULL) {
      if (RespString && (Storage->Type == EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER)) {

        NextElementBakup   = NextRequestElement;
        NextRequestElement = StrStr (RequestElement, ValueKey);
        if (NextRequestElement == NULL) {
          return FALSE;
        }
      }
      //
      // Replace "&" with '\0'.
      //
      *NextRequestElement = L'\0';
    } else {
      if (RespString && (Storage->Type == EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER)) {

        NextElementBakup = NextRequestElement;
        NextRequestElement = StrStr (RequestElement, ValueKey);
        if (NextRequestElement == NULL) {
          return FALSE;
        }

        //
        // Replace "&" with '\0'.
        //
        *NextRequestElement = L'\0';
      }
    }

    if (!ElementValidation (Storage, RequestElement)) {
      //
      // Add this element to the Storage->BrowserStorage->AllRequestElement.
      //
      AppendConfigRequest (&Storage->ConfigRequest, &Storage->SpareStrLen, RequestElement);
      RetVal = TRUE;
    }

    if (NextRequestElement != NULL) {
      //
      // Restore '&' with '\0' for later used.
      //
      *NextRequestElement = L'&';
    }

    if (RespString && (Storage->Type == EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER)) {
      RequestElement = NextElementBakup;
    } else {
      RequestElement = NextRequestElement;
    }
  }

  return RetVal;
}


/**
  Fill storage with settings requested from Configuration Driver.

  @param  FormSet                FormSet data structure.
  @param  Storage                Buffer Storage.

**/
VOID
LoadStorage (
  IN HII_FORMSET            *FormSet,
  IN HII_FORMSET_STORAGE    *Storage
  )
{
  EFI_STATUS                      Status;
  EFI_STRING                      Progress;
  EFI_STRING                      Result;
  CHAR16                          *StrPtr;
  EFI_STRING                      ConfigRequest;
  UINTN                           RequestStrSize;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;

  ConfigRequest = NULL;

  switch (Storage->Type) {
    case EFI_HII_VARSTORE_EFI_VARIABLE:
      return;

    case EFI_HII_VARSTORE_EFI_VARIABLE_BUFFER:

      if (Storage->ConfigRequest != NULL) {
        ConfigRequestAdjust (Storage, Storage->ConfigRequest, FALSE);
        return;
      }
      break;

    case EFI_HII_VARSTORE_BUFFER:
    case EFI_HII_VARSTORE_NAME_VALUE:
      //
      // Skip if there is no RequestElement.
      //
      if (Storage->ElementCount == 0) {
        return;
      }
      break;

    default:
      return;
  }

  if (Storage->Type != EFI_HII_VARSTORE_NAME_VALUE) {
    //
    // Create the config request string to get all fields for this storage.
    // Allocate and fill a buffer large enough to hold the <ConfigHdr> template
    // followed by "&OFFSET=0&WIDTH=WWWW"followed by a Null-terminator
    //
    RequestStrSize = StrSize (Storage->ConfigHdr) + 20 * sizeof (CHAR16);
    ConfigRequest = AllocateZeroPool (RequestStrSize);
    if (ConfigRequest == NULL) {
      return;
    }

    UnicodeSPrint (
      ConfigRequest,
      RequestStrSize,
      L"%s&OFFSET=0&WIDTH=%04x",
      Storage->ConfigHdr,
      Storage->Size);
  } else {

    RequestStrSize = (StrLen (Storage->ConfigHdr) + StrLen (Storage->ConfigRequest) + 1) * sizeof (CHAR16);
    ConfigRequest = AllocateZeroPool (RequestStrSize);
    if (ConfigRequest == NULL) {
      return;
    }

    UnicodeSPrint (
      ConfigRequest,
      RequestStrSize,
      L"%s%s",
      Storage->ConfigHdr,
      Storage->ConfigRequest);
  }

  //
  // Request current settings from Configuration Driver
  //
  Status = gBS->LocateProtocol (
                  &gEfiHiiConfigRoutingProtocolGuid,
                  NULL,
                  (VOID **) &HiiConfigRouting
                  );
  if (EFI_ERROR (Status)) {
    return;
  }
  Status = HiiConfigRouting->ExtractConfig (
                               HiiConfigRouting,
                               ConfigRequest,
                               &Progress,
                               &Result
                               );
  if (!EFI_ERROR (Status)) {

    //
    // Convert Result from <ConfigAltResp> to <ConfigResp>
    //
    StrPtr = StrStr (Result, L"&GUID=");
    if (StrPtr != NULL) {
      *StrPtr = L'\0';
    }

    Status = ConfigRespToStorage (Storage, Result);
    FreePool (Result);
  }

  Storage->ConfigRequest = AllocateCopyPool (StrSize (ConfigRequest), ConfigRequest);
  if (Storage->ConfigRequest == NULL) {

    if (ConfigRequest != NULL) {
      FreePool (ConfigRequest);
    }
    return;
  }

  if (Storage->Type != EFI_HII_VARSTORE_NAME_VALUE) {
    if (ConfigRequest != NULL) {
      FreePool (ConfigRequest);
    }
  }
}

/**
  Zero extend integer/boolean/date/time to UINT64 for comparing.

  @param  Value                  HII Value to be converted.

**/
VOID
ExtendValueToU64 (
  IN HII_STATEMENT_VALUE    *Value
  )
{
  UINT64  Temp;

  Temp = 0;
  switch (Value->Type) {
  case EFI_IFR_TYPE_NUM_SIZE_8:
    Temp = Value->Value.u8;
    break;

  case EFI_IFR_TYPE_NUM_SIZE_16:
    Temp = Value->Value.u16;
    break;

  case EFI_IFR_TYPE_NUM_SIZE_32:
    Temp = Value->Value.u32;
    break;

  case EFI_IFR_TYPE_BOOLEAN:
    Temp = Value->Value.b;
    break;

  case EFI_IFR_TYPE_TIME:
    Temp = Value->Value.u32 & 0xffffff;
    break;

  case EFI_IFR_TYPE_DATE:
    Temp = Value->Value.u32;
    break;

  default:
    return;
  }

  Value->Value.u64 = Temp;
}
