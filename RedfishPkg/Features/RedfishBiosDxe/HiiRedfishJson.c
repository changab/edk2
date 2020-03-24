/** @file
  Internal Functions for Redfish JSON file and HII configuration converting.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "HiiRedfishJson.h"
#include "HiiRedfishMisc.h"

HII_REDFISH_PRIVATE_DATA    mHiiRedfishPrivateData;
extern LIST_ENTRY           mAttributeNameNodeList;
extern LIST_ENTRY           mMenuNameList;

EFI_STATUS
RedfishPlatformSetAttributeValue (
  IN EDKII_JSON_VALUE  AttributeName,
  IN EDKII_JSON_VALUE  AttributeValue,
  IN CHAR8 *AttributeTypeStr
  );

/**
  Initialize all HII data required by Redfish, and add some additional information to
  each formset, form and statement.

  Initialize the global device path alias table, and save it into storage.

  If the initialization work successfully done, set the global data mHiiRedfishPrivateData.Initialized
  to TRUE.

  @retval  EFI_SUCCESS                  Redfish required HII data has been initialized successfully.
  @retval  EFI_NOT_FOUND                There are no HII handles in the system.
  @retval  EFI_OUT_OF_RESOURCES         There are no enough Memory.
  @retval  Others                       Another unexpected error occured.

**/
EFI_STATUS
HiiRedfishInit (
  VOID
  )
{
  EFI_STATUS           Status;
  UINT32               Index;
  EFI_HII_HANDLE       *HiiHandles;
  EFI_GUID             ZeroGuid;
  REDFISH_FORMSET      *RedfishFormSet;
  HII_FORMSET          *HiiFormSet;
  BOOLEAN              IsFormSetRoot;
  REDFISH_FORM         *RedfishForm;
  HII_FORM             *HiiForm;
  LIST_ENTRY           *HiiFormLink;
  REDFISH_STATEMENT    *RedfishStatement;
  HII_STATEMENT        *HiiStatement;
  LIST_ENTRY           *HiiStatementLink;

  ZeroMem (&mHiiRedfishPrivateData, sizeof (mHiiRedfishPrivateData));
  InitializeListHead (&mHiiRedfishPrivateData.RedfishFormSetList);
  InitializeListHead (&mHiiRedfishPrivateData.KeywordNamespaceList);
  InitializeListHead (&mAttributeNameNodeList);
  InitializeListHead (&mMenuNameList);

  Status = gBS->LocateProtocol (
                  &gEfiDevicePathFromTextProtocolGuid,
                  NULL,
                  (VOID **) &mHiiRedfishPrivateData.PathFromText
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InitDevAliasTable ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  HiiHandles = HiiGetHiiHandles (NULL);
  if (HiiHandles == NULL) {
    return EFI_NOT_FOUND;
  }

  for (Index = 0; HiiHandles[Index] != NULL; Index++) {

    HiiFormSet = AllocateZeroPool (sizeof (HII_FORMSET));
    if (HiiFormSet == NULL) {

      FreePool (HiiHandles);
      return EFI_OUT_OF_RESOURCES;
    }

    ZeroMem (&ZeroGuid, sizeof (ZeroGuid));
    Status = CreateFormSetFromHiiHandle (HiiHandles[Index], &ZeroGuid, HiiFormSet);
    if (EFI_ERROR (Status) || IsListEmpty (&HiiFormSet->FormListHead)) {

      DestroyFormSet (HiiFormSet);
      continue;
    }

    InitializeFormSet (HiiFormSet);
    RedfishFormSet = InitRedfishFormSet (HiiFormSet);
    if (RedfishFormSet == NULL) {

      DestroyFormSet (HiiFormSet);
      continue;
    }

    //
    // The first Form in each FormSet is a root Form
    //
    IsFormSetRoot = TRUE;

    HiiFormLink = GetFirstNode (&HiiFormSet->FormListHead);
    while (!IsNull (&HiiFormSet->FormListHead, HiiFormLink)) {

      HiiForm     = HII_FORM_FROM_LINK (HiiFormLink);
      HiiFormLink = GetNextNode (&HiiFormSet->FormListHead, HiiFormLink);

      RedfishForm = InitRedfishForm (RedfishFormSet, HiiForm);
      if (RedfishForm == NULL) {
        continue;
      }

      HiiStatementLink = GetFirstNode (&HiiForm->StatementListHead);
      while (!IsNull (&HiiForm->StatementListHead, HiiStatementLink)) {

        HiiStatement     = HII_STATEMENT_FROM_LINK (HiiStatementLink);
        HiiStatementLink = GetNextNode (&HiiForm->StatementListHead, HiiStatementLink);

        if (HiiStatement->QuestionId == 0) {
          continue;
        }

        RedfishStatement = InitRedfishStatement (RedfishFormSet, HiiStatement);
        if (RedfishStatement == NULL) {
          continue;
        }

        InsertTailList (&RedfishForm->RedfishStatementList, &RedfishStatement->Link);
      }

      if (IsFormSetRoot) {

        RedfishForm->IsRootForm = TRUE;
        IsFormSetRoot = FALSE;
      }
      InsertTailList (&RedfishFormSet->RedfishFormList, &RedfishForm->Link);
    }

    InsertTailList (&mHiiRedfishPrivateData.RedfishFormSetList, &RedfishFormSet->Link);
  }

  DEBUG ((EFI_D_INFO, "\n [Redfish] ********** Redfish Initialization Done **********\n\n"));
  mHiiRedfishPrivateData.Initialized = TRUE;

  FreePool (HiiHandles);
  return EFI_SUCCESS;
}

/**
  Generate the attribute set JSON value required by Bios JSON schema.

  The attribute set is an object JSON value to contain all attribtue name-value pairs in the system.
  Only string, Integer, Enumeration and boolean types will be cared.

  @param[in]     AttributeRegistry        The JSON value for attribute registry which defines
                                          all attributes in system.
  @param[out]    AttributeSet             The attribute set JSON value to be generated

**/
VOID
GenerateAttributeSetForBios (
  IN     EDKII_JSON_VALUE    AttributeRegistry,
  OUT    EDKII_JSON_VALUE    AttributeSet
  )
{
  EFI_STATUS                 Status;
  EDKII_JSON_VALUE           RegistryEntries;
  EDKII_JSON_VALUE           AttributeArray;
  EDKII_JSON_VALUE           Attribute;
  EDKII_JSON_VALUE           Type;
  UINTN                      Count;
  UINT32                     Index;

  UINT8                      *AttributeValue;
  UINTN                      AttributeValueLen;

  RETURN_IF_JSON_VALUE_NOT_OBJECT (AttributeRegistry);
  RegistryEntries = JsonObjectGetValue (JsonValueGetObject (AttributeRegistry), "RegistryEntries");

  RETURN_IF_JSON_VALUE_NOT_OBJECT (RegistryEntries);
  AttributeArray  = JsonObjectGetValue (JsonValueGetObject (RegistryEntries), "Attributes");

  RETURN_IF_JSON_VALUE_NOT_ARRAY (AttributeArray);
  Count = JsonArrayCount (JsonValueGetArray (AttributeArray));
  if (Count == 0) {
    return;
  }

  for (Index = 0; Index < Count; Index++) {

    Attribute = JsonArrayGetValue (JsonValueGetArray (AttributeArray), Index);
    CONTINUE_IF_JSON_VALUE_NOT_OBJECT (Attribute);

    Status = GetAttributeValue (Attribute, &AttributeValue, &AttributeValueLen);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Type = JsonObjectGetValue (JsonValueGetObject (Attribute), "Type");
    CONTINUE_IF_JSON_VALUE_NOT_STRING (Type);

    if (AsciiStrCmp (JsonValueGetAsciiString (Type), "String") == 0) {

      SetStringAttributeValue (AttributeSet, Attribute, AttributeValue, AttributeValueLen);

    } else if (AsciiStrCmp (JsonValueGetAsciiString (Type), "Integer") == 0 ||
               AsciiStrCmp (JsonValueGetAsciiString (Type), "Enumeration") == 0) {

      SetNumberAttributeValue (AttributeSet, Attribute, AttributeValue, AttributeValueLen);

    } else if (AsciiStrCmp (JsonValueGetAsciiString (Type), "Boolean") == 0) {

      SetBooleanAttributeValue (AttributeSet, Attribute, AttributeValue, AttributeValueLen);

    } else {

      continue;
    }

    FreePool (AttributeValue);
  }
}

/**
  Following Redfish Bios JSON schema to generate Bios JSON value.

  Bios JSON value should be Object type. The properties should contain: Attributes, Redfish.Settings,
  odata.id, odata.type, SettingsObject, Time and Etag.

  Time and Etag information can be retrieved from the variable "HiiRedfishRouteResult".

  @param[in, out]     RedfishJson                 A structure to contain Redfish reuqired system JSON values

  @retval  EFI_SUCCESS                       The Bios JSON value has generated successfully.
  @retval  EFI_INVALID_PARAMETER             One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES              There are no enough Memory.
  @retval  Others                            Other error occurs.

**/
EFI_STATUS
GenerateBiosJsonObj (
  IN OUT REDFISH_JSON_DATA    *RedfishJson
  )
{
  EFI_STATUS                    Status;
  EDKII_JSON_VALUE              Bios;
  EDKII_JSON_VALUE              AttributeSet;
  EDKII_JSON_VALUE              RedfishSettings;
  EDKII_JSON_VALUE              SettingsObject;
  HII_REDFISH_LIB_NV_STORAGE    *Storage;
  EDKII_JSON_VALUE              JsonTemp;
  CHAR8                         *PlatformBiosSettingUri;

  if (RedfishJson == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!mHiiRedfishPrivateData.Initialized) {

    Status = HiiRedfishInit();
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (!JsonValueIsObject (RedfishJson->AttributeRegistry)) {

    Status = GenerateAttributeRegistryJsonObj (RedfishJson);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Bios = JsonValueInitObject ();
  if (Bios == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Extract current value for attributes.
  //
  AttributeSet = JsonValueInitObject ();
  if (AttributeSet == NULL) {

    JsonValueFree (Bios);
    return EFI_OUT_OF_RESOURCES;
  }

  GenerateAttributeSetForBios (RedfishJson->AttributeRegistry, AttributeSet);

  Status = JsonObjectSetValue (
             JsonValueGetObject (Bios),
             "Attributes",
             AttributeSet);
  JsonValueFree (AttributeSet);
  if (EFI_ERROR (Status)) {

    JsonValueFree (Bios);
    return Status;
  }

  RedfishJson->Bios = Bios;

  Storage = NULL;
  //
  // Create "Settings" property.
  //
  RedfishSettings = JsonValueInitObject ();
  if (RedfishSettings == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  JsonTemp = JsonValueInitAsciiString ("#Settings.v1_0_4.Settings");
  Status   = JsonObjectSetValue (
               JsonValueGetObject (RedfishSettings),
               "@odata.type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  SettingsObject = JsonValueInitObject ();
  if (SettingsObject == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  if (mHiiRedfishPrivateData.JsonData.BiosSettingObject != NULL) {
    PlatformBiosSettingUri = JsonValueGetAsciiString (mHiiRedfishPrivateData.JsonData.BiosSettingObject);
    Status = EFI_SUCCESS;
  } else {
    Status = GetRedfishPlatformBiosResourceUri (EfiRedfishPlatformBiosSettingUri, &PlatformBiosSettingUri);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: No URI for BIOS Setting Object\n"));
    }
  }
  if (!EFI_ERROR (Status)) {
    JsonTemp = JsonValueInitAsciiString(PlatformBiosSettingUri);
    Status   = JsonObjectSetValue (
                 JsonValueGetObject (SettingsObject),
                 "@odata.id",
                 JsonTemp
                 );
    JsonValueFree (JsonTemp);
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }

  Status = JsonObjectSetValue (
             JsonValueGetObject (RedfishSettings),
             "SettingsObject",
             SettingsObject
             );
  JsonValueFree (SettingsObject);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  Status = GetVariable2 (
             HII_REDFISH_ROUTE_RESULT_VAR_NAME,
             &gEfiCallerIdGuid,
             &Storage,
             NULL
             );
  if (!EFI_ERROR (Status)) {

    //
    // Time
    //
    Status = SetTimeToRedfishSettings (RedfishSettings, Storage);
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }

    //
    // ETag
    //
    if (Storage->EtagStrSize != 0) {

      JsonTemp = JsonValueInitAsciiString ((CHAR8*) Storage + sizeof (HII_REDFISH_LIB_NV_STORAGE));
      Status   = JsonObjectSetValue (
                   JsonValueGetObject (RedfishSettings),
                   "ETag",
                   JsonTemp
                   );
      JsonValueFree (JsonTemp);
      if (EFI_ERROR (Status)) {
        goto ON_EXIT;
      }
    }

    Status = SetMessageArrayToRedfishSetting (RedfishSettings, Storage);
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
  }

  Status = JsonObjectSetValue (
             JsonValueGetObject (Bios),
             "@Redfish.Settings",
             RedfishSettings
             );

ON_EXIT:

  if (Storage != NULL) {
    FreePool (Storage);
  }

  return Status;
}

/**
  Following Redfish Attribute Registry JSON schema to generate Menus JSON value.

  A Menu JSON value should be a JSON obejct which contains all Redfish required information about a form
  retrieved from HII database. The properties of a Menu should contain: DisplayName, MenuPath, MenuName,
  Hidden and ReadOnly.

  @param[in,out]     RegistryEntries          The Attribute Registry which which contains Menus JSON value.

**/
VOID
GenerateMenuObjForRegistry (
  IN OUT EDKII_JSON_VALUE    RegistryEntries
  )
{
  EFI_STATUS           Status;
  EDKII_JSON_VALUE     MenuArray;
  EDKII_JSON_VALUE     MenuValue;
  REDFISH_MENU         *MenuList;
  REDFISH_MENU         *TempMenu;
  UINTN                MenuListCount;
  UINT16               MenuListIndex;
  LIST_ENTRY           *FormsetListLink;
  REDFISH_FORMSET      *FormSet;
  LIST_ENTRY           *FormListLink;
  REDFISH_FORM         *Form;
  EDKII_JSON_VALUE     JsonTemp;

  MenuListCount = GetSystemRedfishMenuCount ();
  if (MenuListCount == 0) {
    return;
  }

  MenuList = AllocateZeroPool (sizeof (REDFISH_MENU) * MenuListCount);
  if (MenuList == NULL) {
    return;
  }

  MenuArray = JsonValueInitArray ();
  if (MenuArray == NULL) {

    FreePool (MenuList);
    return;
  }
  Status = JsonObjectSetValue (
             JsonValueGetObject (RegistryEntries),
             "Menus",
             MenuArray
             );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  MenuListIndex     = 0;
  FormsetListLink   = GetFirstNode (&mHiiRedfishPrivateData.RedfishFormSetList);
  while (!IsNull (&mHiiRedfishPrivateData.RedfishFormSetList, FormsetListLink)) {

    FormSet         = REDFISH_FORMSET_FROM_LINK (FormsetListLink);
    FormsetListLink = GetNextNode (&mHiiRedfishPrivateData.RedfishFormSetList, FormsetListLink);

    FormListLink = GetFirstNode (&FormSet->RedfishFormList);
    while (!IsNull (&FormSet->RedfishFormList, FormListLink)) {

      Form         = REDFISH_FORM_FROM_LINK (FormListLink);
      FormListLink = GetNextNode (&FormSet->RedfishFormList, FormListLink);
      InitRedfishMenu (FormSet, Form, MenuList + MenuListIndex);
      MenuListIndex ++;
    }
  }

  LinkSystemRedfishMenus (MenuList, MenuListCount);

  for (MenuListIndex = 0; MenuListIndex < MenuListCount; MenuListIndex ++) {

    TempMenu = MenuList + MenuListIndex;
    if (!TempMenu->IsRestMenu) {
      continue;
    }

    MenuValue = JsonValueInitObject ();
    if (MenuValue == NULL) {
      goto ON_EXIT;
    }

    Status = JsonArrayAppendValue (
               JsonValueGetArray (MenuArray),
               MenuValue
               );
    if (EFI_ERROR (Status)) {

      JsonValueFree (MenuValue);
      continue;
    }

    JsonTemp = JsonValueInitUnicodeString (TempMenu->DisplayName);
    JsonObjectSetValue (
      JsonValueGetObject (MenuValue),
      "DisplayName",
      JsonTemp
      );
    JsonValueFree (JsonTemp);

    JsonTemp = JsonValueInitUnicodeString (TempMenu->MenuPath);
    JsonObjectSetValue (
      JsonValueGetObject (MenuValue),
      "MenuPath",
      JsonTemp
      );
    JsonValueFree (JsonTemp);

    JsonTemp = JsonValueInitUnicodeString (TempMenu->MenuName);
    JsonObjectSetValue (
      JsonValueGetObject (MenuValue),
      "MenuName",
      JsonTemp
      );
    JsonValueFree (JsonTemp);

    JsonObjectSetValue (
      JsonValueGetObject (MenuValue),
      "Hidden",
      JsonValueInitBoolean (TempMenu->IsHidden)
      );

    JsonObjectSetValue (
      JsonValueGetObject (MenuValue),
      "ReadOnly",
      JsonValueInitBoolean (TempMenu->IsReadOnly)
      );

    JsonValueFree (MenuValue);
  }

ON_EXIT:

  JsonValueFree (MenuArray);
  FreePool (MenuList);
}

/**
  Following Redfish Attribute Registry JSON schema to generate Dependencies JSON value.

  A Dependency JSON value should be a JSON obejct which contains all Redfish required information about the
  relationship between one question to another. Supported dependencies in UEFI include: SuppressIf, DisableIf,
  GrayoutIf and WarningTextIf.

  @param[in,out]     RegistryEntries          The Attribute Registry which which contains Dependencies JSON value.

**/
VOID
GenerateDependencyObjForRegistry  (
  IN OUT EDKII_JSON_VALUE    RegistryEntries
  )
{
  EFI_STATUS                 Status;
  EDKII_JSON_VALUE           DependenciesArray;
  LIST_ENTRY                 *FormsetListLinkHead;
  LIST_ENTRY                 *FormsetListLink;
  REDFISH_FORMSET            *FormSet;
  LIST_ENTRY                 *FormListLink;
  REDFISH_FORM               *Form;
  LIST_ENTRY                 *StatementListLink;
  REDFISH_STATEMENT          *Statement;
  LIST_ENTRY                 *Link;
  HII_EXPRESSION_LIST        *ExpressionList;
  HII_EXPRESSION             *Expression;
  UINTN                      ExpressionIndex;

  RETURN_IF_JSON_VALUE_NOT_OBJECT (RegistryEntries);

  DependenciesArray = JsonValueInitArray ();
  if (DependenciesArray == NULL) {
    return;
  }

  Status = JsonObjectSetValue (
             JsonValueGetObject (RegistryEntries),
             "Dependencies",
             DependenciesArray
             );
  if (EFI_ERROR (Status)) {

    JsonValueFree (DependenciesArray);
    return;
  }

  FormsetListLinkHead = &mHiiRedfishPrivateData.RedfishFormSetList;
  FormsetListLink = GetFirstNode (FormsetListLinkHead);
  while (!IsNull (FormsetListLinkHead, FormsetListLink)) {

    FormSet = REDFISH_FORMSET_FROM_LINK (FormsetListLink);
    FormsetListLink = GetNextNode (FormsetListLinkHead, FormsetListLink);

    FormListLink = GetFirstNode (&FormSet->RedfishFormList);
    while (!IsNull (&FormSet->RedfishFormList, FormListLink)) {

      Form = REDFISH_FORM_FROM_LINK (FormListLink);
      FormListLink = GetNextNode (&FormSet->RedfishFormList, FormListLink);

      StatementListLink = GetFirstNode (&Form->RedfishStatementList);
      while (!IsNull (&Form->RedfishStatementList, StatementListLink)) {

       Statement = REDFISH_STATEMENT_FROM_LINK (StatementListLink);
       StatementListLink = GetNextNode (&Form->RedfishStatementList, StatementListLink);

       ExpressionList = Statement->HiiStatement->ExpressionList;
       if (ExpressionList != NULL) {

          for (ExpressionIndex = 0; ExpressionIndex < ExpressionList->Count; ExpressionIndex ++) {

            Expression = ExpressionList->Expression[ExpressionIndex];
            Status = AppendDependencyArray (
                       FormSet,
                       Form,
                       Statement,
                       Expression,
                       FALSE,
                       DependenciesArray
                       );
            if (EFI_ERROR (Status)) {
              continue;
            }
          }
        }

        Link = GetFirstNode (&Statement->HiiStatement->WarningListHead);
        while (!IsNull (&Statement->HiiStatement->WarningListHead, Link)) {

          Expression = HII_EXPRESSION_FROM_LINK (Link);
          Link = GetNextNode (&Statement->HiiStatement->WarningListHead, Link);

          Status = AppendDependencyArray (
                     FormSet,
                     Form,
                     Statement,
                     Expression,
                     TRUE,
                     DependenciesArray
                     );
          if (EFI_ERROR (Status)) {
            continue;
          }
        }
      }
    }
  }

  JsonValueFree (DependenciesArray);
}

/**
  Following Redfish Attribute Registry JSON schema to generate Attributes JSON value.

  An Attribute JSON value should be a JSON obejct which contains all Redfish required information about a question
  retrieved from HII database. The Rest stype question will be parsed to an attribute JSON value, and be added to
  the attributes array.

  Question only support following types: EFI_IFR_NUMERIC_OP, EFI_IFR_STRING_OP, EFI_IFR_PASSWORD_OP,
  EFI_IFR_CHECKBOX_OP, EFI_IFR_ONE_OF_OP, EFI_IFR_ORDERED_LIST_OP, EFI_IFR_DATE_OP and EFI_IFR_TIME_OP.

  @param[in,out]     RegistryEntries          The Attribute Registry which which contains Attributes JSON value.

**/
EFI_STATUS
GenerateAttributesObjForRegistry  (
  IN OUT EDKII_JSON_VALUE    RegistryEntries
 )
{
  EFI_STATUS                 Status;
  EDKII_JSON_VALUE           AttributesArray;
  LIST_ENTRY                 *FormsetListLinkHead;
  LIST_ENTRY                 *FormsetListLink;
  REDFISH_FORMSET            *FormSet;
  LIST_ENTRY                 *FormListLink;
  REDFISH_FORM               *Form;
  LIST_ENTRY                 *StatementListLink;
  REDFISH_STATEMENT          *Statement;
  UINT8                      ClassGuidIndex;
  BOOLEAN                    IsRestFormSet;

  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (RegistryEntries, EFI_INVALID_PARAMETER);

  AttributesArray = JsonValueInitArray ();
  if (AttributesArray == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = JsonObjectSetValue (
             JsonValueGetObject (RegistryEntries),
             "Attributes",
             AttributesArray
             );
  if (EFI_ERROR (Status)) {

    JsonValueFree (AttributesArray);
    return Status;
  }

  FormsetListLinkHead = &mHiiRedfishPrivateData.RedfishFormSetList;
  FormsetListLink = GetFirstNode (FormsetListLinkHead);
  while (!IsNull (FormsetListLinkHead, FormsetListLink)) {

    FormSet = REDFISH_FORMSET_FROM_LINK (FormsetListLink);
    FormsetListLink = GetNextNode (FormsetListLinkHead, FormsetListLink);

    IsRestFormSet = FALSE;
    for (ClassGuidIndex = 0; ClassGuidIndex < 3; ClassGuidIndex ++) {
      if (CompareGuid (&FormSet->HiiFormSet->ClassGuid[ClassGuidIndex], &gEfiHiiRestStyleFormsetGuid) == TRUE) {
        IsRestFormSet = TRUE;
      }
    }

    FormListLink = GetFirstNode (&FormSet->RedfishFormList);
    while (!IsNull (&FormSet->RedfishFormList, FormListLink)) {

      Form = REDFISH_FORM_FROM_LINK (FormListLink);
      FormListLink = GetNextNode (&FormSet->RedfishFormList, FormListLink);

      StatementListLink = GetFirstNode (&Form->RedfishStatementList);
      while (!IsNull (&Form->RedfishStatementList, StatementListLink)) {

        Statement = REDFISH_STATEMENT_FROM_LINK (StatementListLink);
        StatementListLink = GetNextNode (&Form->RedfishStatementList, StatementListLink);

        if (((Statement->HiiStatement->QuestionFlags & EFI_IFR_FLAG_REST_STYLE) != EFI_IFR_FLAG_REST_STYLE) && !IsRestFormSet) {
          continue;
        }

        Status = AttributesArrayAppendValue (
                   FormSet,
                   Form,
                   Statement,
                   AttributesArray
                   );

        if (!EFI_ERROR (Status) && !Form->IsRest) {
          Form->IsRest = TRUE;
        }
      }
    }
  }

  JsonValueFree (AttributesArray);
  return Status;
}

/**
  Following Redfish Attribute Registry JSON schema to generate Attribute Registry JSON value.

  Attribute Registry JSON value should be a JSON obejct which contains some Redfish required information
  about Attributes, Menus and Dependencies.

  @param[in,out]     RedfishJson             A structure to contain Redfish reuqired system JSON values

  @retval  EFI_SUCCESS                       The Attribute Registry Json Object has generated successfully.
  @retval  EFI_INVALID_PARAMETER             One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES              There are no enough Memory.

**/
EFI_STATUS
GenerateAttributeRegistryJsonObj (
  IN OUT REDFISH_JSON_DATA    *RedfishJson
  )
{
  EFI_STATUS           Status;
  EDKII_JSON_VALUE     AttributeRegistry;
  EDKII_JSON_VALUE     RegistryEntries;

  if (!mHiiRedfishPrivateData.Initialized) {

    Status = HiiRedfishInit();
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if (RedfishJson == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  AttributeRegistry = JsonValueInitObject ();
  if (AttributeRegistry == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RegistryEntries = JsonValueInitObject ();
  if (AttributeRegistry == NULL) {

    JsonValueFree (AttributeRegistry);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = JsonObjectSetValue (
             JsonValueGetObject (AttributeRegistry),
             "RegistryEntries",
             RegistryEntries
             );
  if (EFI_ERROR (Status)) {

    JsonValueFree (RegistryEntries);
    JsonValueFree (AttributeRegistry);
    return Status;
  }

  RedfishJson->AttributeRegistry = AttributeRegistry;
  RedfishJson->RegistryEntries   = RegistryEntries;

  GenerateAttributesObjForRegistry (RegistryEntries);
  GenerateMenuObjForRegistry (RegistryEntries);
  GenerateDependencyObjForRegistry (RegistryEntries);

  return EFI_SUCCESS;
}

/**
  This function allows the caller to extract the current BIOS configuration to Json represent object
  according to the Redfish schema (http://redfish.dmtf.org/schemas/Bios.v1_0_2.json).

  @param[in, out]  DataSize         On input, the size in bytes of the return Data buffer.
                                    On output the size of data returned in Data.
  @param[out]      Data             The buffer to return the contents of Json object.

  @retval  EFI_SUCCESS              Json object is extracted correctly.
  @retval  EFI_INVALID_PARAMETER    One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES     There are no enough Memory.
  @retval  EFI_BUFFER_TOO_SMALL     DataSize is too small for the result. DataSize has been updated with the required size.
  @return  Other                    Failed to extract the Json object.

**/
EFI_STATUS
HiiRedfishExtractBiosJson (
  IN  OUT UINTN  *DataSize,
  OUT VOID      *Data        OPTIONAL
 )
{
  EFI_STATUS     Status;
  CONST CHAR8    *Text;

  if (DataSize == NULL || (*DataSize != 0 && Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (mHiiRedfishPrivateData.JsonData.Bios == NULL) {
    Status = GenerateBiosJsonObj (&mHiiRedfishPrivateData.JsonData);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Text = JsonToText (mHiiRedfishPrivateData.JsonData.Bios);
  if (Text == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (*DataSize < AsciiStrSize (Text)) {
    *DataSize = AsciiStrSize (Text);
    FreePool (Text);
    return EFI_BUFFER_TOO_SMALL;
  }

  *DataSize = AsciiStrSize (Text);
  CopyMem (Data, Text, *DataSize);
  FreePool (Text);

  return EFI_SUCCESS;
}


/**
  This function allows the caller to extract the current Attribute Registry to Json represent object
  according to the Redfish schema (http://redfish.dmtf.org/schemas/AttributeRegistry.v1_1_0.json).

  @param[in, out]  DataSize         On input, the size in bytes of the return Data buffer.
                                    On output the size of data returned in Data.
  @param[out]      Data             The buffer to return the contents of Json object.

  @retval  EFI_SUCCESS              Json object is extracted correctly.
  @retval  EFI_INVALID_PARAMETER    One or more parameters are invalid.
  @retval  EFI_OUT_OF_RESOURCES     There are no enough Memory.
  @retval  EFI_BUFFER_TOO_SMALL     DataSize is too small for the result. DataSize has been updated with the required size.
  @return  Other                    Failed to extract the Json object.

**/
EFI_STATUS
HiiRedfishExtractAttributeRegistryJson (
  IN OUT UINTN    *DataSize,
  OUT    VOID     *Data  OPTIONAL
 )
{
  EFI_STATUS     Status;
  CONST CHAR8    *Text;

  if (DataSize == NULL || (*DataSize != 0 && Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (mHiiRedfishPrivateData.JsonData.AttributeRegistry == NULL) {
    Status = GenerateAttributeRegistryJsonObj (&mHiiRedfishPrivateData.JsonData);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Text = JsonToText (mHiiRedfishPrivateData.JsonData.AttributeRegistry);
  if (Text == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (*DataSize < AsciiStrSize (Text)) {

    FreePool (Text);
    *DataSize = AsciiStrSize (Text);
    return EFI_BUFFER_TOO_SMALL;
  }

  *DataSize = AsciiStrSize (Text);
  CopyMem (Data, Text, *DataSize);
  FreePool (Text);

  return EFI_SUCCESS;
}

/**
  This function applies changes in the input BiosSettings Json Object to the system. If any error occurs
  when an attribute is routing, the applying process will end and this attribute name will be recored in
  the give result.

  If reset is required after successful apply, the global data mHiiRedfishPrivateData.ResetRequired will
  be set to TRUE.

  Read only attribtue can't be applied to the system.

  @param[in]       BiosSettings       The Bios Json Object to apply.
  @param[in]       EtagStr            Etag of the input Json from Redfish Server.
  @param[out]      Result             Point to the failed attribute name. Caller need to free this buffer.

  @retval  EFI_SUCCESS                Configuration is applied correctly.
  @return  Other                      Failed to apply the configuration.

**/
EFI_STATUS
HiiRedfishRouteBiosSettingsJson (
  IN     EDKII_JSON_VALUE    BiosSettings,
  IN     CONST CHAR8         *EtagStr,   OPTIONAL
  OUT    CHAR8               **Result
  )
{
  EFI_STATUS           Status;
  EDKII_JSON_VALUE     Attributes;
  EDKII_JSON_VALUE     AttributeEntry;
  EDKII_JSON_VALUE     ReadOnlyValue;
  EDKII_JSON_VALUE     ResetRequiredValue;
  EDKII_JSON_VALUE     KeywordValue;
  EDKII_JSON_VALUE     DevicePathValue;
  EDKII_JSON_VALUE     NamespaceValue;
  EDKII_JSON_VALUE     TypeValue;
  UINT32               Index;
  CONST CHAR8          *AttributeName;
  CHAR16               *KeywordNameStr;
  CHAR16               *DevicePathStr;
  CHAR16               *NamespaceStr;
  CONST CHAR8          **KeyArray;
  UINTN                KeyArrayCount;

  if (!JsonValueIsObject (BiosSettings) || Result == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (mHiiRedfishPrivateData.JsonData.AttributeRegistry == NULL) {
    Status = GenerateAttributeRegistryJsonObj (&mHiiRedfishPrivateData.JsonData);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Attributes = JsonObjectGetValue (JsonValueGetObject (BiosSettings), "Attributes");
  RETURN_STATUS_IF_JSON_VALUE_NOT_OBJECT (Attributes, EFI_INVALID_PARAMETER);

  KeyArrayCount = 0;
  KeyArray = JsonObjectGetKeys (Attributes, &KeyArrayCount);
  if (KeyArrayCount == 0) {
    return EFI_NOT_FOUND;
  }

  Status = EFI_SUCCESS;
  for (Index = 0; Index < KeyArrayCount; Index ++) {

    AttributeName = KeyArray[Index];

    Status = GetAttributeByName (
               mHiiRedfishPrivateData.JsonData.RegistryEntries,
               KeyArray[Index],
               &AttributeEntry
               );
    if (EFI_ERROR (Status)) {

      Status = EFI_UNSUPPORTED;
      DEBUG ((EFI_D_ERROR, "[Redfish] Failed to route unsupported attribute: %a !\n", AttributeName));
      *Result = AllocateCopyPool (AsciiStrSize (AttributeName), AttributeName);
      if (*Result == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
      }
      break;
    }

    //
    // Skip the setting for "ReadOnly" properties.
    //
    ReadOnlyValue = JsonObjectGetValue (JsonValueGetObject (AttributeEntry), "ReadOnly");
    if (JsonValueIsBoolean (ReadOnlyValue)) {
      if (JsonValueGetBoolean (ReadOnlyValue) == TRUE) {
        continue;
      }
    }
    KeywordNameStr = NULL;
    DevicePathStr = NULL;
    NamespaceStr = NULL;
    KeywordValue    = JsonObjectGetValue (JsonValueGetObject (AttributeEntry), "UefiKeywordName");
    if (KeywordValue != NULL) {
      KeywordNameStr  = JsonValueGetUnicodeString(KeywordValue);
    }
    DevicePathValue = JsonObjectGetValue (JsonValueGetObject (AttributeEntry), "UefiDevicePath");
    if (DevicePathValue != NULL) {
     DevicePathStr   = JsonValueGetUnicodeString (DevicePathValue);
    }
    NamespaceValue = JsonObjectGetValue (JsonValueGetObject (AttributeEntry), "UefiNamespaceId");
    if (NamespaceValue != NULL) {
     NamespaceStr   = JsonValueGetUnicodeString (NamespaceValue);
    }

    TypeValue = JsonObjectGetValue (JsonValueGetObject (AttributeEntry), "Type");
    Status = SaveAttributeValue (
               AttributeName,
               JsonObjectGetValue (Attributes, KeyArray[Index]),
               DevicePathStr,
               NamespaceStr,
               (KeywordNameStr != NULL) && (NamespaceStr != NULL),
               JsonValueGetAsciiString (TypeValue),
               KeywordNameStr
               );
    if (KeywordNameStr != NULL) {
      FreePool (KeywordNameStr);
    }
    if (DevicePathStr != NULL) {
      FreePool (DevicePathStr);
    }
    if (NamespaceStr != NULL) {
      FreePool (NamespaceStr);
    }
    if (Status == EFI_NOT_FOUND) {
      Status = RedfishPlatformSetAttributeValue (
                  AttributeName,
                  JsonObjectGetValue (Attributes, KeyArray[Index]),
                  JsonValueGetAsciiString (TypeValue)
                  );
      if (Status == EFI_UNSUPPORTED) {
        Status = EFI_INVALID_PARAMETER;
      }
    }
    if (EFI_ERROR (Status)) {
      //
      // Stop processing more attributes, and return the failed item to the caller
      //
      DEBUG ((EFI_D_ERROR, "[Redfish] Failed to save value: %a. Status: %r\n", AttributeName, Status));
      *Result = AllocateCopyPool (AsciiStrSize (AttributeName), AttributeName);
      if (*Result == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
      }
      break;
    } else {

      ResetRequiredValue = JsonObjectGetValue (AttributeEntry, "ResetRequired");
      if (JsonValueIsBoolean (ResetRequiredValue)) {
        if (JsonValueGetBoolean (ResetRequiredValue)) {
          mHiiRedfishPrivateData.ResetRequired = TRUE;
        }
      }
    }
  }
  FreePool (KeyArray);

  Status = RedfishSaveRouteResult (Status, EtagStr, AttributeName);
  return Status;
}

/**
  This function applies changes in the input Buffer represent BiosSettings JSON value to the system.
  The BiosSettings JSON value will be saved in a data buffer with UTF-8 encoding.

  @param[in]       DataSize       The size in bytes of the Data buffer.
  @param[in]       Data           The buffer of the Json object.
  @param[in]       EtagStr        Etag of the input Json from Redfish Server.
  @param[out]      Result         Point to the failed attribute name. Caller need to free this buffer.

  @retval  EFI_SUCCESS            Configuration is applied correctly.
  @return  Other                  Failed to apply the configuration.

**/
EFI_STATUS
HiiRedfishRouteBiosSettingsJsonByBuffer (
  IN           UINTN    DataSize,
  IN           VOID     *Data,
  IN     CONST CHAR8    *EtagStr,   OPTIONAL
  OUT          CHAR8    **Result
  )
{
  EFI_STATUS           Status;
  EDKII_JSON_VALUE     BiosSettings;

  if (DataSize == 0 || Data == NULL || Result == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  BiosSettings = TextToJson ((CHAR8 *) Data);
  if (BiosSettings == NULL) {
    return EFI_ABORTED;
  }

  Status = HiiRedfishRouteBiosSettingsJson (BiosSettings, EtagStr, Result);
  JsonValueFree (BiosSettings);
  return Status;
}
