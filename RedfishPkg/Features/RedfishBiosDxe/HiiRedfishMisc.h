/** @file
  Utility Functions for Redfish JSON file and HII configuration converting.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef HII_REDFISH_MISC_H_
#define HII_REDFISH_MISC_H_

/**
  Initialize the Redfish specific formset, and add some Redfish required information for a formset,
  such information mainly includes: formset device path and supported name space list.

  @param[in]          HiiFormSet                 The original formset retrieved from HII database

  @return the initialized Redfish formset or NULL when error occurs.

**/
REDFISH_FORMSET *
InitRedfishFormSet (
  IN HII_FORMSET    *HiiFormSet
  );

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
  );

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
  );

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
  );

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
  );

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
  IN           EDKII_JSON_VALUE     RegistryEntries,
  IN     CONST CHAR8                *AttributeName,
     OUT       EDKII_JSON_VALUE     *Attribute
  );

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
  );

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
FindQuestionByAttributeName  (
  IN     LIST_ENTRY           *FormsetListLinkHead,
  IN     CHAR8                *AttributeName,
     OUT REDFISH_FORMSET      **FormSet,
     OUT REDFISH_FORM         **Form,
     OUT REDFISH_STATEMENT    **Statement
  );

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
  );

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
  );

/**
  Initialize the form related information required by Redfish menu.

  @param[in]          RedfishFormSet     The Redfish formset this form belongs to
  @param[in]          RedfishForm        The Redfish form which contains the form needed
  @param[in,out]      RedfishMenu        The Refish menu to initiallize

**/
VOID
InitRedfishMenu (
  IN     REDFISH_FORMSET      *RedfishFormSet,
  IN     REDFISH_FORM         *RedfishForm,
  IN OUT REDFISH_MENU         *RedfishMenu
  );

/**
  Get the count of Redfish forms in the whole system.

  Redfish form is a structure contains some Redfish required information over HII form.

  @return the count of Redfish forms.

**/
UINTN
GetSystemRedfishMenuCount (
  VOID
  );

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
  );

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
  IN     EDKII_JSON_VALUE    Attribute,
     OUT VOID                **AttributeValue,
     OUT UINTN               *Length
  );

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
  );

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
  );

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
  );

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
  );

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
  );

#endif
