/** @file
  Functions for Redfish BIOS platform resource.

  (C) Copyright 2019 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishBiosDxe.h"

extern EFI_REDFISH_PLATFORM_BIOS_RESOURCE_PROTOCOL *mRedfishPlatformBiosResourceProtocol;

/**
  This function gets the URI to platform BIOS resource.

  @param[in]   ResourceType     BIOS Resource type
  @param[in]   Uri              Pointer to retrive platform BIOS resource URI

  @retval EFI_SUCCESS           URI to platform BIOS resource returned.
  @retval Other                 Failed to retirve URI of platform BIOS resource

**/
EFI_STATUS
GetRedfishPlatformBiosResourceUri (
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_TYPE ResourceType,
  OUT CHAR8 **Uri
  )
{
  EFI_STATUS Status;

  Status = EFI_UNSUPPORTED;
  if (mRedfishPlatformBiosResourceProtocol != NULL) {
    Status = mRedfishPlatformBiosResourceProtocol->GetRedfishBiosResource (
                                                     mRedfishPlatformBiosResourceProtocol,
                                                     ResourceType,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     Uri
                                                     );
  }
  return Status;
}

/**
  This get the URI to platform BIOS resource in Redpath format.

  @param[in]   ValueLength      Length of bytes of BIOS Attribute value.

  @retval NULL                  No URI to platform BIOS resource.
  @retval Other                 URI to platform BIOS resource.

**/
CHAR8 *
GetRedfishPlatformBiosResourceUriRedpath (
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_TYPE ResourceType
  )
{
  EFI_STATUS Status;
  CHAR8 *Uri;
  CHAR8 *InstanceStr;
  CHAR8 *RedPath;
  UINTN BufSize;
  UINTN LeftSize;
  CHAR8 TempBuff;

  Status = GetRedfishPlatformBiosResourceUri (ResourceType, &Uri);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "__FUNCTION__: No URI returned from Platform BIOS Resource Protocol.\n"));
    return NULL;
  }

  if (ResourceType == EfiRedfishPlatformBiosResourceTypeComputerSystemId) {
    return Uri; // This is just a string of computer system instance ID.
  }

  RedPath = Uri;
  if (ResourceType == EfiRedfishPlatformBiosResourceTypeBiosUri ||
      ResourceType == EfiRedfishPlatformBiosResourceTypeComputerSystemUri
      ) {
    if (AsciiStrStr (Uri, "/redfish/v1/Systems/") == NULL) {
      DEBUG ((DEBUG_ERROR, "__FUNCTIOON__: \"redfish/v1/Systems/\" is not the partial string in URI returned from Platform BIOS Resource Protocol\n"));
      DEBUG ((DEBUG_ERROR, "               The returned URI is %s\n", Uri));
      return NULL;
    }
  }

  if (ResourceType == EfiRedfishPlatformBiosResourceTypeBiosUri) {
    //
    // Construct redpath for BIOS resource
    //
    InstanceStr = Uri + AsciiStrLen ("/redfish/v1/Systems/");
    if (AsciiStrStr (InstanceStr, "/Bios") == NULL) {
      return NULL;
    }
    BufSize = 0;
    while (*(InstanceStr + BufSize) != '/' && *(InstanceStr + BufSize) != 0) {
      BufSize ++;
    }
    *(InstanceStr + BufSize) = 0;
    BufSize = AsciiStrSize ("/v1/Systems[Id=]/Bios") + BufSize;
    RedPath = AllocateZeroPool (BufSize);
    if (RedPath == NULL) {
      return NULL;
    }
    AsciiSPrint (RedPath, BufSize, "/v1/Systems[Id=%a]/Bios", InstanceStr);
    FreePool (Uri);
  } else if (ResourceType == EfiRedfishPlatformBiosResourceTypeComputerSystemUri) {
    InstanceStr = Uri + AsciiStrLen ("/redfish/v1/Systems/");
    BufSize = 0;
    while (*(InstanceStr + BufSize) != '/' && *(InstanceStr + BufSize) != 0) {
      BufSize ++;
    }
    *(InstanceStr + BufSize) = 0;
    BufSize = AsciiStrSize ("/v1/Systems[Id=]") + BufSize;
    RedPath = AllocateZeroPool (BufSize);
    if (RedPath == NULL) {
      return NULL;
    }
    AsciiSPrint (RedPath, BufSize, "/v1/Systems[Id=%a]", InstanceStr);
    FreePool (Uri);
    return RedPath;
  } else if (ResourceType == EfiRedfishPlatformBiosResourceTypeAttributeRegistryUri) {
    if (AsciiStrStr (Uri, "/redfish/v1/Registries/") == NULL) {
      //
      // Attribute registry is not in the ServiceRoot/Registries.
      // Just return URI as Redpath.
      //
      return Uri;
    }
    //
    // Convert URI to Redpath.
    //
    InstanceStr = Uri + AsciiStrLen ("/redfish/v1/Registries/");
    BufSize = 0;
    while (*(InstanceStr + BufSize) != '/' && *(InstanceStr + BufSize) != 0) {
      BufSize ++;
    }
    LeftSize = BufSize;
    if (*(InstanceStr + LeftSize) != 0) {
      do {
        LeftSize ++;
      } while (*(InstanceStr + LeftSize) != 0);
    }
    LeftSize -= BufSize; // Left size of string.
    TempBuff = *(InstanceStr + BufSize);
    *(InstanceStr + BufSize) = 0;
    RedPath = AllocateZeroPool (AsciiStrSize ("/v1/Registries[Id=]") + BufSize + LeftSize);
    if (RedPath == NULL) {
      return NULL;
    }
    AsciiSPrint (RedPath, AsciiStrSize ("/v1/Registries[Id=]") + BufSize + LeftSize, "/v1/Registries[Id=%a]", InstanceStr);
    *(InstanceStr + BufSize) = TempBuff;
    if (LeftSize != 0) {
      AsciiStrCatS (RedPath, AsciiStrSize ("/v1/Registries[Id=]") + BufSize + LeftSize, InstanceStr + BufSize);
    }
    FreePool (Uri);
    return RedPath;
  } else {
    return NULL;
  }
  return RedPath;
}

/**
  This functions gets platform BIOS resource.

  @param[in]   PlatformBiosResourceType      Type of platform BIOS resource.
  @param[in]   PlatformBsioResourceMergeType Merge type.
  @param[in]   PlatformBiosResourceJson      Pointer to platform BIOS resource.
                                             Must be freed by caller.
  @param[in]   PlatformBiosResourceSize      Size of platform BIOS resource.

  @retval EFI_STATUS           Platform BIOS resource information returned.
  @retval EFI_UNSUPPORTED      No BIOS resource information of type indicated in ResourceType.
  @retval Other                Errors happened.

**/
EFI_STATUS
GetRedfishPlatformBiosResource (
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_TYPE PlatformBiosResourceType,
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_MERGE_TYPE *PlatformBiosResourceMergeType OPTIONAL,
  IN CHAR8  **PlatformBiosResourceJson OPTIONAL,
  IN UINTN  *PlatformBiosResourceSize OPTIONAL
  )
{
  EFI_STATUS Status;

  if (mRedfishPlatformBiosResourceProtocol == NULL) {
    return EFI_UNSUPPORTED;
  }
  Status = mRedfishPlatformBiosResourceProtocol->GetRedfishBiosResource (
                                                   mRedfishPlatformBiosResourceProtocol,
                                                   PlatformBiosResourceType,
                                                   PlatformBiosResourceMergeType,
                                                   PlatformBiosResourceJson,
                                                   PlatformBiosResourceSize,
                                                   NULL
                                                   );
  return Status;
}

/**
  This functions build up platform BIOS Attribute Registry properties oon the given
  JSON object. Platform code could add, delete, modify the property in JSON object. Or replace
  JSON object if it needs.

  @param[in]   AttrRegObject    Pointer to JSON object on which platform properties can be added.


  @retval EFI_SUCCESS           Platform BIOS resource information returned.
  @retval Other                 Errors happened.

**/
EFI_STATUS
BuildPlatformAttributeRegistryProperties (
  IN EDKII_JSON_OBJECT *AttrRegObject
  )
{
  EFI_STATUS Status;

  if (mRedfishPlatformBiosResourceProtocol == NULL) {
    return EFI_UNSUPPORTED;
  }
  Status = mRedfishPlatformBiosResourceProtocol->AddAttributeRegistryProperties (
                                                   mRedfishPlatformBiosResourceProtocol,
                                                   AttrRegObject
                                                   );
  return Status;
}

/**
  This functions build up platform BIOS properties oon the given
  JSON object. Platform code could add, delete, modify the property in JSON object. Or replace
  JSON object if it needs.

  @param[in]   AttrRegObject    Pointer to JSON object on which platform properties can be added.


  @retval EFI_SUCCESS           Platform BIOS resource information returned.
  @retval Other                 Errors happened.

**/
EFI_STATUS
BuildPlatformBiosProperties (
  IN EDKII_JSON_OBJECT *BiosObject
  )
{
  EFI_STATUS Status;

  if (mRedfishPlatformBiosResourceProtocol == NULL) {
    return EFI_UNSUPPORTED;
  }
  Status = mRedfishPlatformBiosResourceProtocol->AddBiosProperties (
                                                   mRedfishPlatformBiosResourceProtocol,
                                                   BiosObject
                                                   );
  return Status;
}

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
  )
{
  EFI_REDFISH_PLATFORM_BIOS_VALUE_TYPE ValueType;
  INT64 ValueNumber;
  BOOLEAN ValueBoolen;
  CHAR8 *ValueString;
  VOID  *ValuePtr;
  UINTN ValueLen;

  if (mRedfishPlatformBiosResourceProtocol == NULL) {
    return EFI_UNSUPPORTED;
  }
  if (AsciiStrCmp (AttributeTypeStr, "String") == 0 ||
      AsciiStrCmp (AttributeTypeStr, "Enumeration") == 0
      ) {
    //
    // This is string value.
    //
    ValueType = EfiRedfishPlatformBiosValueTypeString;
    ValueString = JsonValueGetAsciiString (AttributeValue);
    ValuePtr = (VOID *)ValueString;
    ValueLen = AsciiStrSize (ValueString);
  } else if (AsciiStrCmp (AttributeTypeStr, "Integer") == 0 ||
             AsciiStrCmp (AttributeTypeStr, "number") == 0
             ) {
    //
    // This is integer value.
    //
    ValueType = EfiRedfishPlatformBiosValueTypeNumber;
    ValueNumber = JsonValueGetNumber (AttributeValue);
    ValuePtr = (VOID *)&ValueNumber;
    ValueLen = sizeof (INT64);
  } else if (AsciiStrCmp (AttributeTypeStr, "boolean") == 0) {
    //
    // This is Boolean value.
    //
    ValueType = EfiRedfishPlatformBiosValueTypeBoolean;
    ValueBoolen = JsonValueGetBoolean (AttributeValue);
    ValuePtr = (VOID *)&ValueBoolen;
    ValueLen = sizeof (BOOLEAN);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Unsupported JSON vale type", __FUNCTION__));
    ASSERT (FALSE);
  }

  return mRedfishPlatformBiosResourceProtocol->ApplyRedfishBiosAttributeValue (
                                                   mRedfishPlatformBiosResourceProtocol,
                                                   JsonValueGetAsciiString (AttributeName),
                                                   ValueType,
                                                   ValuePtr,
                                                   ValueLen
                                                   );
}

/**
  Get the current value of platform defined BIOS Attribute.
  name.

  @param[in]       AttributeName            The attribute name for this attribute
  @param[out]      AttributeValue           The question value retrieved from HII database
  @param[out]      Length                   The size needed for this value

  @retval  EFI_SUCCESS                      This value has been retrieved successfully.
  @retval  EFI_INVALID_PARAMETER            One or more parameters are invalid.
  @retval  EFI_NOT_FOUND                    This question is not found.
  @retval  Others                           Another unexpected error occured.

**/
EFI_STATUS
RedfishPlatformGetAttributeValue (
  IN     CONST CHAR8 *AttributeName,
  OUT    VOID        **AttributeValue,
  OUT    UINTN       *Length
  )
{
  return mRedfishPlatformBiosResourceProtocol->GetRedfishBiosAttributeValue (
                                                   mRedfishPlatformBiosResourceProtocol,
                                                   AttributeName,
                                                   AttributeValue,
                                                   Length
                                                   );
}
