/** @file
  Header file for RedfishBiosDxe driver private data.

  (C) Copyright 2019 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_REDFISH_BIOSPLATFORM_LIB_H_
#define EFI_REDFISH_BIOSPLATFORM_LIB_H_

/**
  This function gets the URI to platform OEM BIOS resource.

  @param[in]   ResourceType     BIOS Resource type
  @param[in]   Uri              Pointer to retrive platform OEM BIOS resource URI

  @retval EFI_SUCCESS           URI to platform OEM BIOS resource returned.
  @retval Other                 Fail to retirve URI of platform OEM BIOS resource

**/
EFI_STATUS
GetRedfishPlatformBiosResourceUri (
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_TYPE ResourceType,
  OUT CHAR8 **Uri
  );

/**
  This function gets the URI to platform OEM BIOS resource and
  returns redpath of this resource.

  @param[in]   ResourceType     BIOS Resource type

  @retval NULL                  No URI to platform OEM BIOS resource.
  @retval Other                 URI to platform OEM BIOS resource.

**/
CHAR8 *
GetRedfishPlatformBiosResourceUriRedpath (
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_TYPE ResourceType
  );

/**
  This functions gets platform OEM BIOS resource.

  @param[in]   PlatformBiosResourceType      Type of platform BIOS resource.
  @param[in]   PlatformBsioResourceMergeType Merge type.
  @param[in]   PlatformBiosResourceJson      Pointer to platform BIOS resource.
                                             Must be freed by caller.
  @param[in]   PlatformBiosResourceSize      Size of platform BIOS resource.

  @retval EFI_SUCCESS         Platform OEM BIOS resource information returned.
  @retval EFI_UNSUPPPORTED     No platform OEM BIOS resource information
  @retval EFI_ALREADY_STARTED  No platform OEM BIOS resource information
  @retval Other                Errors happened.

**/
EFI_STATUS
GetRedfishPlatformBiosResource (
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_TYPE PlatformBiosResourceType,
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_MERGE_TYPE *PlatformBsioResourceMergeType OPTIONAL,
  IN CHAR8  **PlatformBiosResourceJson OPTIONAL,
  IN UINTN  *PlatformBiosResourceSize OPTIONAL
  );
/**
  This functions build up platform OEM BIOS Attribute Registry properties,

  @param[in]   AttrRegObject    JSON object on which platform OEM properties can be added.


  @retval EFI_SUCCESS           Platform OEM BIOS resource information returned.
  @retval Other                 Errors happened.

**/
EFI_STATUS
BuildPlatformAttributeRegistryProperties (
  IN EDKII_JSON_OBJECT *AttrRegObject
  );

/**
  This functions build up platform OEM BIOS properties oon the given
  JSON object. Platform code could add, delete, modify the property in JSON object. Or replace
  JSON object if it needs.

  @param[in]   AttrRegObject    Pointer to JSON object on which platform OEM properties can be added.


  @retval EFI_SUCCESS           Platform OEM BIOS resource information returned.
  @retval Other                 Errors happened.

**/
EFI_STATUS
BuildPlatformBiosProperties (
  IN EDKII_JSON_OBJECT *BiosObject
  );

/**
  This functions invoke EFI_REDFISH_PLATFORM_BIOS_RESOURCE_PROTOCOL to set
  platform defined BIOS Attribute.

  @param[in]   AttributeName     JSON value of Attribute name
  @param[in]   AttributeValue    JSON value of Attribute value
  @param[in]   AttributeTypeStr  String of JSON type

  @retval EFI_SUCCESS           Platform defined Attribute us set successfully.
  @retval Other                 Errors happened.

**/
EFI_STATUS
RedfishPlatformSetAttributeValue (
  IN EDKII_JSON_VALUE  AttributeName,
  IN EDKII_JSON_VALUE  AttributeValue,
  IN CHAR8 *AttributeTypeStr
  );
#endif
