/** @file
  Internal Structures for Redfish JSON file and HII configuration converting.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef HII_REDFISH_JSON_H_
#define HII_REDFISH_JSON_H_

#include <Uefi.h>

#include <Protocol/DevicePath.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/RedfishPlatformBiosResource.h>

#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseJsonLib.h>

#include <Guid/MdeModuleHii.h>
#include <Guid/GlobalVariable.h>
#include <Guid/ImageAuthentication.h>

#include "HiiUtilityLib.h"
#include "HiiKeywordLib.h"
#include "HiiRedfishLib.h"
#include "RedfishBiosPlatformLib.h"

#define DEVICE_PATH_ALIAS_SIZE            8    // L"Dev####"
#define DEVICE_PATH_ALIAS_NUM_VAR_NAME    L"RedfishDevAliasNum"
#define TEM_STR_LEN                       32

#define CONTINUE_IF_JSON_VALUE_NOT_ARRAY(Value)       if (!JsonValueIsArray (Value))   {continue;}
#define CONTINUE_IF_JSON_VALUE_NOT_OBJECT(Value)      if (!JsonValueIsObject (Value))  {continue;}
#define CONTINUE_IF_JSON_VALUE_NOT_STRING(Value)      if (!JsonValueIsString (Value))  {continue;}
#define CONTINUE_IF_JSON_VALUE_NOT_NUMBER(Value)      if (!JsonValueIsNumber (Value))  {continue;}
#define CONTINUE_IF_JSON_VALUE_NOT_BOOLEAN(Value)     if (!JsonValueIsBoolean (Value)) {continue;}
#define CONTINUE_IF_JSON_VALUE_NOT_NULL(Value)        if (!JsonValueIsNull (Value))    {continue;}

#define RETURN_IF_JSON_VALUE_NOT_ARRAY(Value)         if (!JsonValueIsArray (Value))   {return;}
#define RETURN_IF_JSON_VALUE_NOT_OBJECT(Value)        if (!JsonValueIsObject (Value))  {return;}
#define RETURN_IF_JSON_VALUE_NOT_STRING(Value)        if (!JsonValueIsString (Value))  {return;}
#define RETURN_IF_JSON_VALUE_NOT_NUMBER(Value)        if (!JsonValueIsNumber (Value))  {return;}
#define RETURN_IF_JSON_VALUE_NOT_BOOLEAN(Value)       if (!JsonValueIsBoolean (Value)) {return;}
#define RETURN_IF_JSON_VALUE_NOT_NULL(Value)          if (!JsonValueIsNull (Value))    {return;}

#define RETURN_STATUS_IF_JSON_VALUE_NOT_ARRAY(Value, Status)      if (!JsonValueIsArray (Value))   {return Status;}
#define RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT(Value, Status)     if (!JsonValueIsObject (Value))  {return Status;}
#define RETURN_STATUS_IF_JSON_VALUE_NOT_STRING(Value, Status)     if (!JsonValueIsString (Value))  {return Status;}
#define RETURN_STATUS_IF_JSON_VALUE_NOT_NUMBER(Value, Status)     if (!JsonValueIsNumber (Value))  {return Status;}
#define RETURN_STATUS_IF_JSON_VALUE_NOT_BOOLEAN(Value, Status)    if (!JsonValueIsBoolean (Value)) {return Status;}
#define RETURN_STATUS_IF_JSON_VALUE_NOT_NULL(Value, Status)       if (!JsonValueIsNull (Value))    {return Status;}

//
// For Redfish
// Max size for redfish strings
//
#define MAX_SIZE_FOR_REDFISH_ORDERED_LIST    255
#define MAX_SIZE_FOR_REDFISH_TIME            11
#define MAX_SIZE_FOR_REDFISH_DATE            11
#define MAX_SIZE_REDFISH_MENU_LENGTH         255
#define MAX_SIZE_QUENSTIONID_STR_LENGTH      32
#define MAX_SIZE_U64_NUMBER_STR              65
#define MAX_COUNT_REDIFHS_MENU_CHILD         32

//
// Structures for Redfish
//
typedef struct _ATTRIBUTE_NAME_NODE    ATTRIBUTE_NAME_NODE;
typedef struct _MENU_NAME_NODE         MENU_NAME_NODE;
typedef struct _NAMESPACE_DATA         NAMESPACE_DATA;
typedef struct _REDFISH_MENU           REDFISH_MENU;

//
// ***** ATTRIBUTE_NAME_NODE Begin
//
#define ATTRIBUTE_NAME_NODE_SIGNATURE  SIGNATURE_32 ('A', 'N', 'N', 'S')

struct _ATTRIBUTE_NAME_NODE {
  UINT32                 Signature;
  LIST_ENTRY             Link;
  EFI_GUID               FormSetGuid;
  EFI_QUESTION_ID        QuestionId;
  CHAR8                  *AttributeName;
};

#define ATTRIBUTE_NAME_NODE_FROM_LINK(a)  CR (a, ATTRIBUTE_NAME_NODE, Link, ATTRIBUTE_NAME_NODE_SIGNATURE)

//
// ***** ATTRIBUTE_NAME_NODE End
//

//
// ***** MENU_NAME_NODE Begin
//
#define MENU_NAME_NODE_SIGNATURE    SIGNATURE_32 ('M','N','N','S')

struct _MENU_NAME_NODE {
  UINT32        Signature;
  CHAR16        MenuName[MAX_SIZE_REDFISH_MENU_LENGTH];
  LIST_ENTRY    Link;
};

#define MENU_NAME_NODE_FROM_LINK(a)  CR (a, MENU_NAME_NODE, Link, MENU_NAME_NODE_SIGNATURE)

//
// ***** MENU_NAME_NODE End
//

//
// ***** NAMESPACE_DATA Begin
//
#define NAMESPACE_DATA_SIGNATURE  SIGNATURE_32 ('K', 'N', 'D', 'S')

struct _NAMESPACE_DATA {
  UINTN                           Signature;
  CHAR8                           *NamespaceId;
  LIST_ENTRY                      Link;
};

#define NAMESPACE_DATA_FROM_LINK(a)  CR (a, NAMESPACE_DATA, Link, NAMESPACE_DATA_SIGNATURE)

//
// ***** NAMESPACE_DATA End
//

//
// Redfish Menu
//
struct _REDFISH_MENU{
  EFI_GUID             FormSetGuid;
  EFI_FORM_ID          FormId;
  EFI_STRING           MenuName;
  CHAR16               MenuPath[MAX_SIZE_REDFISH_MENU_LENGTH];
  EFI_STRING           DisplayName;
  BOOLEAN              IsHidden;
  BOOLEAN              IsReadOnly;

  BOOLEAN              IsRootMenu;
  BOOLEAN              IsRestMenu;
  UINTN                MenuDepth;
  UINTN                ChildCount;
  REDFISH_MENU         *ChildList[MAX_COUNT_REDIFHS_MENU_CHILD];
};

//
// Name Value Node
//
#define NAME_VALUE_NODE_SIGNATURE  SIGNATURE_32 ('N', 'V', 'S', 'T')

typedef struct {
  UINTN         Signature;
  LIST_ENTRY    Link;
  CHAR16        *Name;
  CHAR16        *Value;
  CHAR16        *EditValue;
} NAME_VALUE_NODE;

#define NAME_VALUE_NODE_FROM_LINK(a)  CR (a, NAME_VALUE_NODE, Link, NAME_VALUE_NODE_SIGNATURE)

//
// Redfish Statement
//
#define REDFISH_STATEMENT_SIGNATURE  SIGNATURE_32 ('R', 'F', 'S', 'M')

typedef struct {
  UINTN            Signature;
  LIST_ENTRY       Link;
  HII_STATEMENT    *HiiStatement;

  BOOLEAN          ValueChanged;

  //
  // Keyword Support
  //
  BOOLEAN          IsKeywordSupported;
  EFI_STRING       Keyword;
} REDFISH_STATEMENT;

#define REDFISH_STATEMENT_FROM_LINK(a)  CR (a, REDFISH_STATEMENT, Link, REDFISH_STATEMENT_SIGNATURE)

//
// Redfish Form
//
#define REDFISH_FORM_SIGNATURE  SIGNATURE_32 ('R', 'F', 'F', 'M')

typedef struct {
  UINTN         Signature;
  LIST_ENTRY    Link;
  HII_FORM      *HiiForm;
  BOOLEAN       IsRootForm;
  BOOLEAN       IsRest;

  LIST_ENTRY    RedfishStatementList;

  EFI_STRING    RedfishMenuName;
  EFI_STRING    RedfishMenuPath;
} REDFISH_FORM;

#define REDFISH_FORM_FROM_LINK(a)  CR (a, REDFISH_FORM, Link, REDFISH_FORM_SIGNATURE)

//
// Formset
//
#define REDFISH_FORMSET_SIGNATURE  SIGNATURE_32 ('R', 'F', 'F', 'S')

typedef struct {
  UINTN          Signature;
  LIST_ENTRY     Link;
  HII_FORMSET    *HiiFormSet;

  LIST_ENTRY     RedfishFormList;

  CHAR16         *DevicePathStr;
  CHAR8          *SupportedLanguages;
  LIST_ENTRY     KeywordNamespaceList;
  LIST_ENTRY     NormalNamespaceList;
  CHAR8          DevicePathAlias[DEVICE_PATH_ALIAS_SIZE];
} REDFISH_FORMSET;

#define REDFISH_FORMSET_FROM_LINK(a)  CR (a, REDFISH_FORMSET, Link, REDFISH_FORMSET_SIGNATURE)

//
// Redfish Json Data
//
typedef struct {
  // For Bios Json object
  EDKII_JSON_VALUE    Bios;
  EDKII_JSON_VALUE    BiosSettingObject;
  EDKII_JSON_VALUE    MessagesArray;
  EDKII_JSON_VALUE    Message;

  // For AttributeRegistry Json object
  EDKII_JSON_VALUE    AttributeRegistry;
  EDKII_JSON_VALUE    RegistryEntries;

  // For SecureBoot Json object
  EDKII_JSON_VALUE    SecureBoot;

  // For BootOrder Json object
  EDKII_JSON_VALUE    BootOrder;
} REDFISH_JSON_DATA;

//
// Redfish Private Data
//
typedef struct {
  BOOLEAN                               Initialized;

  //
  // The ResetRequired field tells if system requires another reset to fit some data's changes
  //
  BOOLEAN                               ResetRequired;

  EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL    *PathFromText;

  LIST_ENTRY                            RedfishFormSetList;
  LIST_ENTRY                            KeywordNamespaceList;

  UINT32                                AliasNum;
  CHAR16                                **AliasTable;

  REDFISH_JSON_DATA                     JsonData;
} HII_REDFISH_PRIVATE_DATA;

extern HII_REDFISH_PRIVATE_DATA    mHiiRedfishPrivateData;

#define HII_REDFISH_ROUTE_RESULT_VAR_NAME    L"HiiRedfishRouteResult"
typedef struct {
  EFI_TIME        TimeOfLastRoute;
  EFI_STATUS      StatusOfLastRoute;
  UINTN           EtagStrSize;
  UINTN           FailedAttributeNameSize;
  //CHAR8         EtagStr[];
  //CHAR8         FailedAttributeName[];
} HII_REDFISH_LIB_NV_STORAGE;

/**
  Following Redfish Attribute Registry Json Schema to generate Json Objects.
  This is an internal function.

  @param[in, out]     RedfishJson                 A structure to contain Redfish Driver reuqired Json Objects

  @retval  EFI_SUCCESS                       The Attribute Registry Json Object has generated successfully.
  @retval  Others                            Other error occurs.

**/
EFI_STATUS
GenerateAttributeRegistryJsonObj (
  IN OUT REDFISH_JSON_DATA    *RedfishJson
  );

#include "HiiRedfishMisc.h"

#endif
