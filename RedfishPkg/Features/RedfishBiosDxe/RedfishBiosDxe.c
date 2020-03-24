/** @file
  Redfish configure handler driver to manipulate the Redfish BIOS Attributes
  related resources.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishBiosDxe.h"

REDFISH_ODATA_TYPE_MAPPING  mOdataTypeList[] = {
  {"ComputerSystem", "#ComputerSystem.v1_1_0.ComputerSystem"},
  {"ComputerSystem", "#ComputerSystem.v1_4_0.ComputerSystem"},
  {"ComputerSystem", "#ComputerSystem.v1_5_1.ComputerSystem"},
  {"ComputerSystem", "#ComputerSystem.v1_10_0.ComputerSystem"},
  {"Bios", "#Bios.v1_0_2.Bios"},
  {"Bios", "#Bios.v1_0_4.Bios"},
  {"AttributeRegistry", "#AttributeRegistry.v1_0_0.AttributeRegistry"},
  {"AttributeRegistry", "#AttributeRegistry.v1_2_1.AttributeRegistry"},
  {"@Redfish.Settings", "#Settings.v1_0_0.Settings"},
  {"@Redfish.Settings", "#Settings.v1_0_4.Settings"},
  {"SettingsObject", "#Bios.v1_0_2.Bios"},
  {"SettingsObject", "#Bios.v1_0_4.Bios"}
};

REDFISH_DEFAULT_ATTRIBUTE_REGISTRY_PROPERTY DefaultAttributeRegistryProperties [] = {
  {"@odata.type" ,
   (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryOdataType)
  },
  {"@odata.id" ,
   (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryOdataId)
  },
  {"Description" ,
   (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryDescription)
  },
  {"Id" ,
   (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryId)
  },
  {"Language" ,
   (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryLanguage)
  },
  {"Name" ,
   (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryName)
  },
  {"OwningEntity" ,
   (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryOwningEntity)
  },
  {"RegistryVersion" ,
   (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryVersion)
  }
};
#define DEFAULT_ATTRIBUTE_REGISTRY_PROPERTY_NUM (sizeof (DefaultAttributeRegistryProperties)/sizeof (REDFISH_DEFAULT_ATTRIBUTE_REGISTRY_PROPERTY))

EFI_REDFISH_PLATFORM_BIOS_RESOURCE_PROTOCOL *mRedfishPlatformBiosResourceProtocol = NULL;

/**
  Get the BIOS resource for this platform from Redfish service.

  @param[in]  Private         Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  RedfishService  The REDFISH_SERVICE instance to access the Redfish servier.

  @return     Redfish BIOS payload or NULL if operation failed.

**/
REDFISH_PAYLOAD
RedfishGetBiosPayload (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN REDFISH_SERVICE           RedfishService
  )
{
  EFI_STATUS        Status;
  CHAR8             *PlatformBiosUriRedPath;
  CHAR8             *PlatformComputerSystemId;
  CHAR8             *DefaultUriRedPath;
  REDFISH_RESPONSE  RedResponse;
  CHAR8             *PlatformResourceJson;

  ASSERT (RedfishService != NULL);

  PlatformBiosUriRedPath = NULL;
  DefaultUriRedPath = NULL;

  //
  // Get Bios property from Computer System.
  //
  Status = RedfishGetByPayload (Private->SystemPayload, "Bios", &RedResponse);
  if (!EFI_ERROR (Status)) {
    //
    // BIOS resource is already exist.
    //
    DEBUG((DEBUG_INFO, "BIOS Resource exist on Redfish service.\n"));
    DefaultUriRedPath = JsonValueGetAsciiString (JsonObjectGetValue(RedfishJsonInPayload (RedResponse.Payload), "@odata.id"));
    if (DefaultUriRedPath  != NULL) {
      Private->BiosUri = DefaultUriRedPath;
    }
    DEBUG((DEBUG_INFO, "The URI: %a\n", DefaultUriRedPath));
    //
    // Get payload use URI.
    //
    Status = RedfishGetByUri (RedfishService, DefaultUriRedPath, &RedResponse);
    if (!EFI_ERROR (Status)) {
      return RedResponse.Payload;
    }
    DEBUG((DEBUG_ERROR, "%a:%d Can't get BIOS resource from the existing BIOS URI.\n",__FUNCTION__, __LINE__));
    return NULL;
  }
  //
  // Quickly get BIOS URI from platform BIOS resource driver.
  //
  PlatformBiosUriRedPath = GetRedfishPlatformBiosResourceUriRedpath (EfiRedfishPlatformBiosResourceTypeBiosUri);
  if (PlatformBiosUriRedPath == NULL) {
    //
    // Platform has its own Computer System instance ID? Use this ID to construct BIOS URI.
    //
    PlatformComputerSystemId = GetRedfishPlatformBiosResourceUriRedpath (EfiRedfishPlatformBiosResourceTypeComputerSystemId);
    if (PlatformComputerSystemId != NULL) {
      PlatformBiosUriRedPath = RedfishBuildPathWithSystemUuid ("/v1/Systems[UUID~%a/Bios]", FALSE, PlatformComputerSystemId);
      if (PlatformBiosUriRedPath == NULL) {
        DEBUG((DEBUG_ERROR, "%a:%d Fail to construct BIOS URI!\n",__FUNCTION__, __LINE__));
        RedResponse.Payload = NULL;
        goto Error;
      }
    } else {
      //
      // Use UUID from SMMBIOS as Computer System instance ID and construct BIOS URI.
      //
      DefaultUriRedPath = RedfishBuildPathWithSystemUuid ("/v1/Systems[UUID~%g/Bios]", TRUE, NULL);
      if (DefaultUriRedPath == NULL) {
        DEBUG((DEBUG_ERROR, "%a:%d Fail to construct BIOS URI use UUID!\n",__FUNCTION__, __LINE__));
        RedResponse.Payload = NULL;
        goto Error;
      }
    }
  }

  //
  // Platform provides its own BIOS resource?
  //
  Status = GetRedfishPlatformBiosResource (
             EfiRedfishPlatformBiosResourceTypeBios,
             &Private->PlatformBiosResourceMergeType,
             &PlatformResourceJson,
             NULL
             );
  if (!EFI_ERROR (Status)) {
    if((Private->PlatformBiosResourceMergeType != EfiRedfishPlatformBiosResourceMergeToUri) &&
        (Private->PlatformBiosResourceMergeType != EfiRedfishPlatformBiosResourceOverrideByUri)) {
      //
      // Create JSON playload for the raw data returned from GetRedfishPlatformBiosResource.
      //
      RedResponse.Payload = RedfishCreatePayload (TextToJson (PlatformResourceJson), RedfishService);
      Status = EFI_SUCCESS;
    } else {
      //
      // Platform BIOS resource is indicated by URI.
      //
      if (Private->PlatformBiosResourceMergeType == EfiRedfishPlatformBiosResourceMergeToUri) {
        if (PlatformBiosUriRedPath == NULL) {
          DEBUG((DEBUG_ERROR, "%a:%d No platform BIOS URI provided!\n",__FUNCTION__, __LINE__));
          RedResponse.Payload = NULL;
          goto Error;
        }
        Status = RedfishGetByService(RedfishService, PlatformBiosUriRedPath, &RedResponse);
        if (EFI_ERROR (Status)) {
          DEBUG((DEBUG_ERROR, "%a:%d Error happened!\n",__FUNCTION__, __LINE__));
          if (RedResponse.Payload != NULL) {
            DEBUG ((DEBUG_ERROR, "%a:%d Error Message:\n",__FUNCTION__, __LINE__));
            RedfishDumpPayload (RedResponse.Payload);
            RedfishFreeResponse (
              NULL,
              0,
              NULL,
              RedResponse.Payload
              );
          }
          RedResponse.Payload = NULL;
          goto Error;
        }
        goto CheckOdataType;
      }
    }
  } else {
    if (Status == EFI_UNSUPPORTED) {
      DEBUG((DEBUG_INFO, "%a:%d No platform BIOS resource provided! BIOS resource generated by EDK2 will be used.\n",__FUNCTION__, __LINE__));
    }
  }
CheckOdataType:;
  if (!RedfishIsValidOdataType (RedResponse.Payload, "Bios", mOdataTypeList, ARRAY_SIZE (mOdataTypeList))) {
    DEBUG((DEBUG_ERROR, "%a:%d Odata type of BIOS resource type is not supported!\n",__FUNCTION__, __LINE__));
    RedfishFreeResponse (
        NULL,
        0,
        NULL,
        RedResponse.Payload
        );
    RedResponse.Payload = NULL;
    goto Error;
  }
  //
  // Only one has valid Redpath.
  //
  if (PlatformBiosUriRedPath != NULL) {
    FreePool (DefaultUriRedPath);
    DefaultUriRedPath = PlatformBiosUriRedPath;
    PlatformBiosUriRedPath = NULL;
  }
  Private->BiosRedPath = DefaultUriRedPath;

  //
  // POST BIOS resource to Redfish service.
  //
  // TODO

Error:;
  if (PlatformBiosUriRedPath != NULL) {
    FreePool (PlatformBiosUriRedPath);
  }
  if (DefaultUriRedPath != NULL) {
    FreePool (DefaultUriRedPath);
  }
  return RedResponse.Payload;
}
/**
  Get the Service version.

  This function will retrive the service version.

  @param[in]  Private         Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  RedfishService  The REDFISH_SERVICE instance to access the Redfish servier.

  @return     The CHAR8 * to versino string.
**/

/**
  Get the Service Root resource from Redfish service.

  This function will retrive the ServiceRoot property

  @param[in]  Private         Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  RedfishService  The REDFISH_SERVICE instance to access the Redfish servier.

  @return     Redfish ServiceRoot payload or NULL if operation failed.
**/
REDFISH_PAYLOAD
RedfishGetServiceRootPayload (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN REDFISH_SERVICE           RedfishService
  )
{
  EFI_STATUS        Status;
  CHAR8             RedPath [] = "/";
  REDFISH_RESPONSE  RedResponse;

  ASSERT (RedfishService != NULL);

  Status = RedfishGetByService (RedfishService, RedPath, &RedResponse);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a:%d] Error happened!\n",__FUNCTION__, __LINE__));
    if (RedResponse.Payload != NULL) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (RedResponse.Payload);
      RedfishFreeResponse (
        NULL,
        0,
        NULL,
        RedResponse.Payload
        );
    }
    return NULL;
  }

  RedfishFreeResponse (
    RedResponse.StatusCode,
    RedResponse.HeaderCount,
    RedResponse.Headers,
    NULL
    );
  return RedResponse.Payload;
}

/**
  Get the Registries resource from Redfish service.

  This function will retrive the ServiceRoot property

  @param[in]  Private         Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  RedfishService  The REDFISH_SERVICE instance to access the Redfish servier.

  @return     Redfish ServiceRoot payload or NULL if operation failed.
**/
REDFISH_PAYLOAD
RedfishGetRegistriesPayload (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN REDFISH_SERVICE           RedfishService
  )
{
  EFI_STATUS        Status;
  CHAR8             RedPath [] = "/v1/Registries";
  REDFISH_RESPONSE  RedResponse;

  ASSERT (RedfishService != NULL);

  Status = RedfishGetByService (RedfishService, RedPath, &RedResponse);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a:%d] Error happened!\n",__FUNCTION__, __LINE__));
    if (RedResponse.Payload != NULL) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (RedResponse.Payload);
      RedfishFreeResponse (
        NULL,
        0,
        NULL,
        RedResponse.Payload
        );
    }
    return NULL;
  }

  RedfishFreeResponse (
    RedResponse.StatusCode,
    RedResponse.HeaderCount,
    RedResponse.Headers,
    NULL
    );
  return RedResponse.Payload;
}
/**
  Get the ComputerSystem resource from Redfish service.

  This function will retrive the ComputerSystem from Redfish server, the ComputerSystem
  member is selected by comparing the ComputerSystem/UUID property and the system GUID.

  @param[in]  Private         Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  RedfishService        The REDFISH_SERVICE instance to access the Redfish servier.

  @return     Redfish ComputerSystem payload or NULL if operation failed.
**/
REDFISH_PAYLOAD
RedfishGetComputerSystemPayload (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN REDFISH_SERVICE           RedfishService
  )
{
  EFI_STATUS            Status;
  CHAR8                 *RedPath;
  CHAR8                 *Id;
  REDFISH_RESPONSE      RedResponse;

  ASSERT (RedfishService != NULL);
  Id = NULL;
  //
  // If platform provides URI to ComputerSystem instance. URI is returned
  // in Redpath format.
  //
  RedPath = GetRedfishPlatformBiosResourceUriRedpath (EfiRedfishPlatformBiosResourceTypeComputerSystemUri);
  if (RedPath == NULL) {
    // No platform specific URI to ComputerSystem instance.
    // Use platform computer system instance ID or UUID from SMBIOS.
    //
    Id = GetRedfishPlatformBiosResourceUriRedpath (EfiRedfishPlatformBiosResourceTypeComputerSystemId);
    if (Id != NULL) {
      RedPath = RedfishBuildPathWithSystemUuid("/v1/Systems[UUID~%a]", FALSE, Id);
      FreePool (Id);
    } else {
      RedPath = RedfishBuildPathWithSystemUuid("/v1/Systems[UUID~%g]", TRUE, NULL);
    }
  }
  if (RedPath == NULL) {
    return NULL;
  }

  DEBUG ((EFI_D_INFO, "[Redfish] RedfishGetComputerSystemPayload for RedPath: %a\n", RedPath));

  Status = RedfishGetByService (RedfishService, RedPath, &RedResponse);
  FreePool (RedPath);
  if (EFI_ERROR (Status) || !RedfishIsValidOdataType (RedResponse.Payload, "ComputerSystem", mOdataTypeList, ARRAY_SIZE (mOdataTypeList))) {
    DEBUG ((DEBUG_ERROR, "No Redfish system instance with this system UUID\n"));
    DEBUG ((DEBUG_ERROR, "[%a:%d] Error happened!\n",__FUNCTION__, __LINE__));
    if (RedResponse.Payload != NULL) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (RedResponse.Payload);

      RedfishFreeResponse (
        NULL,
        0,
        NULL,
        RedResponse.Payload
        );
    }

    return NULL;
  }

  RedfishFreeResponse (
    RedResponse.StatusCode,
    RedResponse.HeaderCount,
    RedResponse.Headers,
    NULL
    );

  return RedResponse.Payload;
}

/**
  Extract the current Attribute Registry to JSON represent text data.

  @param[out]  DataSize       The buffer size of the Data.
  @param[out]  Data           The buffer point saved the returned JSON text.

  @retval EFI_SUCCESS               The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES      The request could not be completed due to a lack of resources.

**/
EFI_STATUS
RedfishExtractAttributeRegistry (
  OUT  UINTN             *DataSize,
  OUT  VOID              **Data
 )
{
  EFI_STATUS                      Status;

  ASSERT (DataSize != NULL);
  ASSERT (Data != NULL);

  *DataSize = 0;
  Status = HiiRedfishExtractAttributeRegistryJson (DataSize, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  *Data = AllocateZeroPool (*DataSize);
  if (Data == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  Status = HiiRedfishExtractAttributeRegistryJson (DataSize, *Data);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  return EFI_SUCCESS;

ON_ERROR:
  if (*Data != NULL) {
    FreePool (*Data);
    *Data = NULL;
  }

  *DataSize = 0;
  return Status;
}

/**
  This function check if the MessageRegistryFile exist for BIOS
  Attribute Registry and return URI and Redpath.

  @param[in]  Private               Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  MessageRegFileRedpath Redpath to MessageRegistryFile.
  @param[in]  Uri                   Pointer to receive URI string.
                                    Caller has to free this memory if return status
                                    is EFI_SUCCESSl;
  @param[in]  UriRedpath            Pointer to receive URI string in redpath format.
                                    Caller has to free this memory if return status
                                    is EFI_SUCCESSl;

  @retval EFI_NOT_FOUND             MessageRefistryFile is not exist.
          EFI_NOT_READY             No messaeg file in default language.
          EFI_ABORTED               Improper MessageRefistryFile resource.
          EFI_INVALID_PARAMETER     Improper given parameters.
          EFI_SUCEESS               Everything is good.

**/
EFI_STATUS
RedfishCheckAttributeRegistryId (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN CHAR8 *MessageRegFileRedpath,
  OUT CHAR8 **RegistryUri,
  OUT CHAR8 **RegistryUriRedpath
)
{
  EFI_STATUS Status;
  REDFISH_RESPONSE MessageRegFileResponse;
  CHAR8 *Language;
  EDKII_JSON_VALUE JsonValue;
  EDKII_JSON_VALUE JsonValueLanguage;
  EDKII_JSON_VALUE JsonValueUri;
  EDKII_JSON_VALUE JsonArrayItem;
  UINTN ArrayCount;
  UINTN Index;
  UINTN UriStrSize;

  if (MessageRegFileRedpath == NULL || RegistryUri == NULL || RegistryUriRedpath == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  Language = (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryLanguage);

  Status = RedfishCheckIfRedpathExist (Private->RedfishService, MessageRegFileRedpath, &MessageRegFileResponse);
  if (EFI_ERROR(Status)) {
    return EFI_NOT_FOUND;
  }
  Private->MessageRegistryFile = MessageRegFileResponse.Payload;
  RedfishFreeResponse (
    MessageRegFileResponse.StatusCode,
    MessageRegFileResponse.HeaderCount,
    MessageRegFileResponse.Headers,
    NULL
    );
  //
  // Get message file with the matched language.
  //
  JsonValue = RedfishJsonInPayload (Private->MessageRegistryFile);
  JsonValue = JsonObjectGetValue (JsonValue, "Location");
  if (JsonValue == NULL || !JsonValueIsArray (JsonValue)) {
    return EFI_ABORTED;
  }
  ArrayCount = JsonArrayCount (JsonValue);
  if (ArrayCount == 0) {
    return EFI_ABORTED;
  }
  for (Index = 0; Index < ArrayCount; Index ++) {
    JsonArrayItem = JsonArrayGetValue (JsonValue, Index);
    JsonValueLanguage = JsonObjectGetValue (JsonArrayItem, "Language");
    if (JsonValueLanguage == NULL || !JsonValueIsString (JsonValueLanguage)) {
      return EFI_ABORTED;
    }
    if (AsciiStrCmp (JsonValueGetAsciiString (JsonValueLanguage), Language) == 0) {
      JsonValueUri = JsonObjectGetValue (JsonArrayItem, "Uri");
      if (JsonValueUri == NULL || !JsonValueIsString (JsonValueUri)) {
        return EFI_ABORTED;
      }
      UriStrSize = AsciiStrSize(JsonValueGetAsciiString(JsonValueUri));
      *RegistryUri = AllocatePool (UriStrSize);
      *RegistryUriRedpath = AllocatePool (UriStrSize - AsciiStrLen ("/redfish"));
      AsciiStrCpyS (*RegistryUri, UriStrSize, (CONST CHAR8 *)JsonValueGetAsciiString (JsonValueUri));
      AsciiStrCpyS (*RegistryUriRedpath, UriStrSize - AsciiStrLen ("/redfish"), (CONST CHAR8 *)JsonValueGetAsciiString (JsonValueUri) + AsciiStrLen ("/redfish"));
      return EFI_SUCCESS;
    }
  }
  return EFI_NOT_READY;
}
/**
  This function check and convert AttributeRegistry ID to
  Redpath and URI.

  @param[in]       Private            Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in, out]  HiiRedfishBuffer   Pointer to HII Redfish BIOS payload in JSON format.
                                      HiiRedfishBuffer could be overrided after merging with
                                      platform OEM BISO resource.
  @param[in]       ArrayName          The array name in BIOS Attribute Registry
  @param[in]       KeyInArrayName     Key in array to compare.

  @retval EFI_SUCCESS     Platform OEM BIOS resource exists and merged sucessfully.
  @retval EFI_NOT_FOUND   Platform OEM BIOS resource is not found.
  @retval Others          Other errors happened.

**/
EFI_STATUS
RedfishMergePlatformAttributeRegistryProperty (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN OUT CHAR8 **HiiRedfishBuffer,
  IN CHAR8 *ArrayName,
  IN CHAR8 *KeyInArrayName
  )
{
  BOOLEAN Modified;
  EFI_STATUS Status;
  EDKII_JSON_VALUE  PlatformBiosAttributeRegistryJsonValue;
  EDKII_JSON_VALUE  PlatformAttributeArrayJson;
  EDKII_JSON_VALUE  PlatformAttributeArrayEntity;
  EDKII_JSON_VALUE  PlatformAttributeArrayEntityObject;

  EDKII_JSON_VALUE  HiiRedfishJsonValue;
  EDKII_JSON_VALUE  HiiAttributeArrayJson;
  EDKII_JSON_VALUE  HiiAttributeArrayEntity;
  EDKII_JSON_OBJECT HiiAttributeArrayEntityObject;

  CHAR8             *HiiAttributeNameStr;
  CHAR8             *PlatformAttributeNameStr;
  UINTN             HiiArrayIndex;
  UINTN             PlatformArrayIndex;
  BOOLEAN           AttributeMatched;

  if (Private->PlatformAttrRegPayload == NULL) {
    return EFI_NOT_FOUND;
  }
  if (HiiRedfishBuffer == NULL || *HiiRedfishBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PlatformBiosAttributeRegistryJsonValue = RedfishJsonInPayload (Private->PlatformAttrRegPayload);
  if (Private->HiiRedfishBiosJsonValue == NULL) {
    HiiRedfishJsonValue = TextToJson(*HiiRedfishBuffer);
    Private->HiiRedfishBiosJsonValue = HiiRedfishJsonValue;
    DEBUG ((DEBUG_INFO, "HII BIOS Atrribute Registry:\n"));
    RedfishDumpJson (HiiRedfishJsonValue);
  } else {
    HiiRedfishJsonValue = Private->HiiRedfishBiosJsonValue;
  }

  if (PlatformBiosAttributeRegistryJsonValue == NULL ||
      HiiRedfishJsonValue == NULL
      ) {
    return EFI_DEVICE_ERROR;
  }

  //
  // Check if BIOS attribute is already in platform BIOS resource.
  //
  PlatformAttributeArrayJson = JsonValueGetArray (
                                 JsonObjectGetValue (
                                   JsonValueGetObject (
                                     JsonObjectGetValue(
                                       JsonValueGetObject (PlatformBiosAttributeRegistryJsonValue),
                                       "RegistryEntries"
                                     )
                                   ),
                                   ArrayName
                                 )
                               );

  HiiAttributeArrayJson = JsonValueGetArray (
                            JsonObjectGetValue (
                              JsonValueGetObject (
                                JsonObjectGetValue(
                                  JsonValueGetObject (HiiRedfishJsonValue),
                                  "RegistryEntries"
                                )
                              ),
                              ArrayName
                            )
                          );

  if (HiiAttributeArrayJson == NULL) {
    return EFI_SUCCESS;
  }
  if (PlatformAttributeArrayJson == NULL) {
    //
    // Copy all from HiiAttributeArrayJson to PlatformBiosAttributeRegistryJsonValue.
    //
    JsonObjectSetValue (JsonValueGetObject (PlatformBiosAttributeRegistryJsonValue), "RegistryEntries", HiiAttributeArrayJson);
    return EFI_SUCCESS;
  }

  Modified = FALSE;
  EDKII_JSON_ARRAY_FOREACH(HiiAttributeArrayJson, HiiArrayIndex, HiiAttributeArrayEntity) {
    HiiAttributeArrayEntityObject = JsonValueGetObject (HiiAttributeArrayEntity);
    HiiAttributeNameStr = JsonValueGetAsciiString (JsonObjectGetValue (HiiAttributeArrayEntityObject, KeyInArrayName));
    AttributeMatched = FALSE;
    EDKII_JSON_ARRAY_FOREACH(PlatformAttributeArrayJson, PlatformArrayIndex, PlatformAttributeArrayEntity) {
      PlatformAttributeArrayEntityObject = JsonValueGetObject (PlatformAttributeArrayEntity);
      PlatformAttributeNameStr = JsonValueGetAsciiString (JsonObjectGetValue (PlatformAttributeArrayEntityObject, KeyInArrayName));
      if (AsciiStrCmp(HiiAttributeNameStr, PlatformAttributeNameStr) == 0) {
         //
         // Already in Platform BIOS Attribute Registry. Skip to process next HII BIOS Attribute.
         //
         AttributeMatched = TRUE;
         break;
      }
    }
    if (!AttributeMatched) {
      //
      // Insert HII BIOS Attribute to Platform BIOS Attribute.
      //
      Status = JsonArrayAppendValue (PlatformAttributeArrayJson, HiiAttributeArrayEntity);
      if (EFI_ERROR (Status)) {
        DEBUG((DEBUG_ERROR, "%a: Error append HII BIOS Attribute to platform BIOS Attribute\n", __FUNCTION__));
        return EFI_DEVICE_ERROR;
      }
      Modified = TRUE;
    }
  }
  if (Modified) {
    return EFI_SUCCESS;
  }
  return EFI_ALREADY_STARTED;
}

/**
  This function check and convert AttributeRegistry ID to
  Redpath and URI.

  @param[in]       Private                   Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in, out]  HiiRedfishBuffer          Pointer to HII Redfish BIOS payload in JSON format.
                                             HiiRedfishBuffer could be overrided after merging with
                                             platform OEM BISO resource.

  @retval EFI_SUCCESS     Platform OEM BIOS resource exists and merged sucessfully.
  @retval EFI_NOT_FOUND   Platform OEM BIOS resource is not found.
  @retval Others          Other errors happened.

**/
EFI_STATUS
RedfishMergePlatformAttributeRegistry (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN OUT CHAR8 **HiiRedfishBuffer
  )
{
  EFI_STATUS Status;
  EFI_STATUS StatusMenu;
  EFI_STATUS StatusDependencies;

  Status = RedfishMergePlatformAttributeRegistryProperty (Private, HiiRedfishBuffer, "Attributes", "AttributeName");
  if (EFI_ERROR (Status) && Status != EFI_ALREADY_STARTED) {
    DEBUG ((DEBUG_ERROR, "%a: Merge Attributes properties failed. Skip merging the follow up Menus and Dependencies property.\n", __FUNCTION__));
    goto ERROR_EXIT;
  }
  StatusMenu = RedfishMergePlatformAttributeRegistryProperty(Private, HiiRedfishBuffer, "Menus", "MenuName");
  if ((EFI_ERROR (StatusMenu) && StatusMenu != EFI_ALREADY_STARTED)) {
    DEBUG ((DEBUG_ERROR, "%a: Merge Menu properties failed. Skip merging the follow up the Dependencies property.\n", __FUNCTION__));
    goto ERROR_EXIT;
  }
  //
  // There may have multiple dependencies for the same Attribute. Just treat the dependency has ever merged to
  // platform OEM BIOS attribute if we get occurrence once.
  //
  StatusDependencies = RedfishMergePlatformAttributeRegistryProperty(Private, HiiRedfishBuffer, "Dependencies", "DependencyFor");
  if ((EFI_ERROR (StatusDependencies) && StatusDependencies != EFI_ALREADY_STARTED)) {
    DEBUG ((DEBUG_ERROR, "%a: Merge dependencies properties failed.\n", __FUNCTION__));
    goto ERROR_EXIT;
  }
  if (Status == EFI_ALREADY_STARTED &&
      StatusMenu == EFI_ALREADY_STARTED &&
      StatusDependencies == EFI_ALREADY_STARTED
      ) {
    Status = EFI_ALREADY_STARTED;
  } else {
    Status = EFI_SUCCESS;
  }

  FreePool(*HiiRedfishBuffer);
  *HiiRedfishBuffer = JsonToText (RedfishJsonInPayload (Private->PlatformAttrRegPayload));
  RedfishDumpJsonStringFractions (*HiiRedfishBuffer);

ERROR_EXIT:
  if (Private->HiiRedfishBiosJsonValue != NULL) {
    JsonValueFree(Private->HiiRedfishBiosJsonValue);
    Private->HiiRedfishBiosJsonValue = NULL;
  }
  return Status;
}

/**
  This function checks ETag of BIOS Redfish Attribute Registry which
  is going to patch to redfish service. Skip patching if the Etag is identical
  in order to prevent from spend time on networking.

  @param[in]  Private              Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  HiiRedfishBuffer     HII Redfish BIOS payload in JSON format.

  @retval TRUE      Etag is the same.
  @retval TRUE      Etag is not the same.

**/
BOOLEAN
RedfishCheckAttributeRegistryEtag (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN CHAR8* HiiRedfishBuffer
  )
{
  return FALSE;
}

/**
  This functions build up edk2 default BIOS Attribute Registry properties,

  @param[in]   AttrRegObject    Pointer to JSON object on which platform OEM properties can be added.


  @retval EFI_SUCCESS           Platform OEM BIOS resource information returned.
  @retval Other                 Errors happened.

**/
EFI_STATUS
BuildDefaultAttributeRegistryProperties (
  IN EDKII_JSON_OBJECT *AttrRegObject
  )
{
  EFI_STATUS Status;
  UINTN Index;

  for (Index = 0; Index < DEFAULT_ATTRIBUTE_REGISTRY_PROPERTY_NUM; Index ++) {
    Status = JsonObjectSetValue (
               *AttrRegObject,
               DefaultAttributeRegistryProperties [Index].PropertyName,
               JsonValueInitAsciiString (DefaultAttributeRegistryProperties [Index].PropertyStr)
               );
    if (EFI_ERROR (Status)) {
      DEBUG((DEBUG_ERROR, "%a:%d Error to append property to AttrRegObject.\n",__FUNCTION__, __LINE__));
      return Status;
    }
  }

  return EFI_SUCCESS;
}
/**
  This function POST  full AttributeRegistry resource.

  @param[in]  Private                   Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  AttributeRegistry         Attribute Registry resouece

  @retval EFI_SUCCESS    The resource of AttributeRegistry is built up.
  @retval Others

**/
EFI_STATUS
RedfishPostAttributeRegistryResource (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN CHAR8 *AttributeRegistry
  )
{
  REDFISH_PAYLOAD    AttrRegPayload;
  EDKII_JSON_VALUE   AttrRegJsonValue;
  REDFISH_RESPONSE   PostResponse;
  EFI_STATUS Status;

  AttrRegJsonValue = TextToJson (AttributeRegistry);
  AttrRegPayload = RedfishCreatePayload (AttrRegJsonValue, Private->RedfishService);
  if (AttrRegPayload == NULL) {
    DEBUG ((DEBUG_ERROR, "%a:%d Failed to create JSON payload from JSON value!\n",__FUNCTION__, __LINE__));
    Status =  EFI_DEVICE_ERROR;
    goto EXIT_FREE_JSON_VALUE;
  }

  ZeroMem (&PostResponse, sizeof (REDFISH_RESPONSE));
  Status = RedfishPostToPayload(Private->Registries, AttrRegPayload, &PostResponse);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:%d Failed to POST Attribute Registry to Redfish service.\n",__FUNCTION__, __LINE__));
  }

  RedfishFreeResponse (
    PostResponse.StatusCode,
    PostResponse.HeaderCount,
    PostResponse.Headers,
    PostResponse.Payload
    );
  RedfishCleanupPayload (AttrRegPayload);

EXIT_FREE_JSON_VALUE:
  JsonValueFree(AttrRegJsonValue);
  return Status;
}

/**
  Get RegistryEntries JSON value and content.

  @param[in]    Content                   The content of resource
  @param[in]    RegistryEntriesContent    Pointer to receive content of RegistryEntries.
  @param[in]    RegistryEntriesJsonValue   Pointer to receive JSON value of RegistryEntries.

  @retval EFI_SUCCESS     Information returned.
  @retval Others          Other errors.
**/
EFI_STATUS
GetRegistryEntriesJsonValueContent (
  IN  CONST CHAR8 *Content,
  OUT CHAR8 **RegistryEntriesContent OPTIONAL,
  OUT EDKII_JSON_VALUE *RegistryEntriesJsonValue OPTIONAL
)
{
  EFI_STATUS Status;
  CHAR8 *PatchContent;
  EDKII_JSON_VALUE ContentJsonValue;
  EDKII_JSON_VALUE TempContentJsonValue;
  EDKII_JSON_OBJECT TempContentJsonObject;

  Status = EFI_SUCCESS;
  ContentJsonValue = TextToJson (Content);
  if (ContentJsonValue == NULL) {
    return EFI_DEVICE_ERROR;
  }
  //
  // Check if the buffer is a full resource of Attribute Registry.
  //
  // The resource could be a full Attribute Registry or only contains
  // RegistryEntries. Get "RegistryEntries" from resource to make
  // sure we only return RegistryEntries to caller.
  //
  TempContentJsonValue = JsonObjectGetValue (
                           JsonValueGetObject (ContentJsonValue),
                           "RegistryEntries"
                        );
  if (TempContentJsonValue == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto EXIT_ERROR;
  }
  TempContentJsonObject = JsonValueInitObject ();
  if (TempContentJsonObject == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto EXIT_ERROR;
  }
  JsonObjectSetValue (TempContentJsonObject, "RegistryEntries", TempContentJsonValue);
  if (RegistryEntriesContent != NULL) {
    PatchContent = JsonToText (TempContentJsonObject);
    if (PatchContent == NULL) {
      Status = EFI_DEVICE_ERROR;
      goto EXIT_ERROR_FREE_OBJECT;
    }
    *RegistryEntriesContent = PatchContent;
  }
  if (RegistryEntriesJsonValue != NULL) {
    *RegistryEntriesJsonValue = JsonValueClone (TempContentJsonObject);
  }
EXIT_ERROR_FREE_OBJECT:
  JsonValueFree (TempContentJsonObject);
EXIT_ERROR:
  JsonValueFree (ContentJsonValue);
  return Status;
}

/**
  Patch RegistryExtries to Redfish service.

  @param[in]    RedfishService        The Service to access the Redfish resources.
  @param[in]    Uri                   Relative path to address the resource.
  @param[in]    Content               JSON represented properties to be update, this could be the
                                      full resource of Attribute Registry or only the resource of
                                      RegistryEntries.
  @param[out]   RedResponse           Pointer to the Redfish response data.

  @retval EFI_SUCCESS             The opeartion is successful, indicates the HTTP StatusCode is not
                                  NULL and the value is 2XX. The Redfish resource will be returned
                                  in Payload within RedResponse if server send it back in the HTTP
                                  response message body.
  @retval EFI_INVALID_PARAMETER   RedfishService, Uri, Content, or RedResponse is NULL.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred. Callers can get
                                  more error info from returned HTTP StatusCode, Headers and Payload
                                  within RedResponse:
                                  1. If the returned StatusCode is NULL, indicates any error happen.
                                  2. If the returned StatusCode is not NULL and the value is not 2XX,
                                     indicates any error happen.
**/
EFI_STATUS
EFIAPI
RedfishPatchAttributeRegistryResource (
  IN     REDFISH_SERVICE            RedfishService,
  IN     CONST CHAR8                *Uri,
  IN     CONST CHAR8                *Content,
  OUT    REDFISH_RESPONSE           *RedResponse
  )
{
  EFI_STATUS Status;
  CHAR8 *PatchContent;

  Status = GetRegistryEntriesJsonValueContent (Content, &PatchContent, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = RedfishPatchToUri(RedfishService, Uri, PatchContent, RedResponse);
  FreePool (PatchContent);
  return Status;
}

/**
  This function constructs full AttributeRegistry resource.

  @param[in]   Private                     Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]   AttributeRegistry           Attribute Registry resouece
  @param[in]   AttributeRegistryLen        Length of Attribute Registry resouece
  @param[out]  AttributeRegistryJsonValue  Pointer to receive JSON value of AttributeRegistry
  @param[out]  RegistryEntriesJsonValue    Pointer to receive JSON value of RegistryEntries

  @retval EFI_SUCCESS    The resource of AttributeRegistry is built up.
  @retval Others

**/
EFI_STATUS
RedfishConstructFullAttributeRegistryResource (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN CHAR8 **AttributeRegistry,
  IN UINTN *AttributeRegistryLen,
  IN EDKII_JSON_VALUE *AttributeRegistryJsonValue OPTIONAL,
  IN EDKII_JSON_VALUE *RegistryEntriesJsonValue OPTIONAL
  )
{
  EFI_STATUS Status;
  EDKII_JSON_VALUE   AttrRegJsonValue;
  EDKII_JSON_VALUE   TempJsonValue;
  EDKII_JSON_OBJECT  NewAttrRegObject;

  if (AttributeRegistry == NULL ||
      *AttributeRegistry == NULL ||
      AttributeRegistryLen == NULL
      ) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  NewAttrRegObject = NULL;
  //
  // Only construct full property if PlatformAttributeRegisterResourceMergeType is
  // EfiRedfishPlatformBiosResourceNone
  //
  if (Private->PlatformAttributeRegisterResourceMergeType == EfiRedfishPlatformBiosResourceNone) {
    AttrRegJsonValue = TextToJson (*AttributeRegistry);
    if (AttrRegJsonValue == NULL) {
      DEBUG ((DEBUG_ERROR, "%a:%d Error happened to create JSON value from buffer!\n",__FUNCTION__, __LINE__));
      return EFI_DEVICE_ERROR;
    }
    //
    // EfiRedfishPlatformBiosResourceNone measn the given resource only contains
    // RegistryEntries generasted by EDKII.
    //
    // Check if Registries is the key of AttrRegObject;
    //
    TempJsonValue = JsonObjectGetValue (JsonValueGetObject (AttrRegJsonValue), "RegistryEntries");
    if (TempJsonValue == NULL) {
      DEBUG ((DEBUG_ERROR, "%a:%d Not the valid JSON resource for building up BIOS Attribute Registry.\n",__FUNCTION__, __LINE__));
      Status = EFI_DEVICE_ERROR;
    } else {
      //
      // Buffer only contains RegistryEntries. Build up other properties for AttributeRegistry.
      //
      NewAttrRegObject = JsonValueInitObject ();
      if (NewAttrRegObject == NULL) {
        DEBUG ((DEBUG_ERROR, "%a:%d Failed to create JSON object.\n",__FUNCTION__, __LINE__));
        Status = EFI_DEVICE_ERROR;
        goto EXIT_ERROR;
      }
      Status = BuildPlatformAttributeRegistryProperties (&NewAttrRegObject);
      if (Status == EFI_UNSUPPORTED) {
        Status = BuildDefaultAttributeRegistryProperties (&NewAttrRegObject);
      }
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a:%d Failed to build up platform Attribute Registry properties.\n",__FUNCTION__, __LINE__));
        Status = EFI_DEVICE_ERROR;
        goto EXIT_ERROR;
      }
      //
      // Add RegistryEntries to NewAttrRegObject.
      //
      Status = JsonObjectSetValue (NewAttrRegObject, "RegistryEntries", TempJsonValue);
      if (EFI_ERROR (Status)) {
        DEBUG((DEBUG_ERROR, "%a:%d Failed to append RegistryEntries.\n",__FUNCTION__, __LINE__));
        goto EXIT_ERROR;
      }
      FreePool (*AttributeRegistry);
      *AttributeRegistry = JsonToText (NewAttrRegObject);
      *AttributeRegistryLen = AsciiStrLen (*AttributeRegistry);
      if (AttributeRegistryJsonValue != NULL) {
        *AttributeRegistryJsonValue = AttrRegJsonValue;
      }
      if (RegistryEntriesJsonValue != NULL) {
        *RegistryEntriesJsonValue = TempJsonValue;
      }
      return EFI_SUCCESS;
    }
EXIT_ERROR:
    if (NewAttrRegObject != NULL) {
      JsonValueFree(NewAttrRegObject);
    }
    if (AttrRegJsonValue != NULL) {
      JsonValueFree(AttrRegJsonValue);
    }
  }
  return Status;
}
/**
  Patch the defualt string property.

  @param[in]   KeyToPatch    Key name of property to patch.
  @param[in]   Str           Key value of property to patch.

**/
VOID
PatchDefaultAttributeRegistryProperty (
  IN CHAR8 *KeyToPatch,
  IN CHAR8 *Str
  )
{
  UINTN Index;

  for (Index = 0; Index < DEFAULT_ATTRIBUTE_REGISTRY_PROPERTY_NUM; Index ++) {
    if (AsciiStrCmp (KeyToPatch, DefaultAttributeRegistryProperties [Index].PropertyName) == 0) {
      DefaultAttributeRegistryProperties [Index].PropertyStr = Str;
    }
  }
}

/**
  Get Attribute Registry ID from Attribute Registry redpath.

  @param[in]   Redpath      The Redpath of Attribute Registry.

  @retval NULL  Fail to get Attribute Registry ID

**/
CHAR8 *
GetRedfishAttributeRegistryIdFromRedpath (
  IN CHAR8 *Redpath
  )
{
  CHAR8 *TempStr;
  CHAR8 *IdStr;
  UINTN IdStrLen;

  if (Redpath == NULL) {
    return NULL;
  }
  TempStr = AsciiStrStr (Redpath, "[Id=") + AsciiStrLen ("[Id=");
  IdStrLen = 0;
  while (*(TempStr + IdStrLen) != ']') {
    IdStrLen ++;
  };
  IdStr = AllocateZeroPool (IdStrLen + 1);
  if (IdStr == NULL) {
    return NULL;
  }
  CopyMem (IdStr, TempStr, IdStrLen);
  return IdStr;
}
/**
  Build up Redpath of BIOS message registry file using Attribute Registry ID.

  @param[in]  Private               Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  RedfishService        The REDFISH_SERVICE instance to access the Redfish servier.
  @param[in]  BiosPayload           The Redfish payload of the current settings BIOS resource.

  @retval EFI_SUCCESS               The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES      The request could not be completed due to a lack of resources.
  @retval EFI_DEVICE_ERROR          Failed to generate a AttributeRegistry from system.

**/
EFI_STATUS
RedfishBiosBuildMessageRegFileRedpth (IN REDFISH_BIOS_PRIVATE_DATA *Private)
{

   return RedfishBuildRedpathUseId (
                              Private->ServiceVersionStr,
                              "/Registries",
                              Private->AttrRegIdStr,
                              &Private->MessageRegFileRedpath
                              );
}
/**
  Generate a new AttributeRegistry resource from system and patch it to Redfish service.

  @param[in]  Private               Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  RedfishService        The REDFISH_SERVICE instance to access the Redfish servier.
  @param[in]  BiosPayload           The Redfish payload of the current settings BIOS resource.

  @retval EFI_SUCCESS               The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES      The request could not be completed due to a lack of resources.
  @retval EFI_DEVICE_ERROR          Failed to generate a AttributeRegistry from system.

**/
EFI_STATUS
RedfishPatchAttributeRegistry (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN REDFISH_SERVICE           RedfishService,
  IN REDFISH_PAYLOAD           *BiosPayload
  )
{
  EFI_STATUS            Status;
  REDFISH_RESPONSE      AttrRegIdResponse;
  REDFISH_RESPONSE      PlatformAttrRegResponse;
  EDKII_JSON_VALUE      AttrRegIdJsonValue;
  CHAR8                 *Buffer;
  UINTN                 BufLen;
  REDFISH_RESPONSE      PatchResponse;
  CHAR8                 *PlatformBiosAttributeRegistryResourceJson;
  CHAR8                 *AttributeRegistryUriOnService;
  CHAR8                 *AttributeRegistryRedpath;

  ASSERT (RedfishService != NULL);
  ASSERT (BiosPayload != NULL);

  Buffer = NULL;
  AttributeRegistryUriOnService = NULL;
  AttributeRegistryRedpath = NULL;
  ZeroMem (&AttrRegIdResponse, sizeof (REDFISH_RESPONSE));
  ZeroMem (&PlatformAttrRegResponse, sizeof (REDFISH_RESPONSE));

  Private->AttrRegIdStr = NULL;
  Private->AttrRegUri = NULL;
  Private->MessageRegFileRedpath = NULL;
  Private->AttrRegRedpath = NULL;
  //
  // Get BIOS AttributeRegistry property in BIOS resource, which is the resource ID
  // of the Attribute Registry.
  //
  Status = RedfishGetByPayload (BiosPayload, "AttributeRegistry", &AttrRegIdResponse);
  if (EFI_ERROR (Status)) {
    //
    // AttributeRegistry is optional, don't treat this an error.
    //
    DEBUG ((DEBUG_INFO, "%a: Can't find AttributeRegistry in BIOS paylaod, this property is optional though.!\n",__FUNCTION__));
    if (AttrRegIdResponse.Payload != NULL) {
      RedfishDumpPayload (AttrRegIdResponse.Payload);
    }
  } else {
    DEBUG ((DEBUG_INFO, "%a: AttributeRegistry ID found in BIOS paylaod\n",__FUNCTION__));
    if (AttrRegIdResponse.Payload != NULL) {
      AttrRegIdJsonValue = RedfishJsonInPayload (AttrRegIdResponse.Payload);
      if (AttrRegIdJsonValue != NULL && JsonValueIsString (JsonObjectGetValue(AttrRegIdJsonValue, "AttributeRegistry"))) {
        Private->AttrRegIdStr = JsonValueGetAsciiString (JsonObjectGetValue(AttrRegIdJsonValue, "AttributeRegistry"));
        Status = RedfishBiosBuildMessageRegFileRedpth (Private);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_INFO, "%a: Build up Redpath for Attribute Registry fail!\n",__FUNCTION__));
        }
      } else {
        DEBUG ((DEBUG_INFO, "%a: AttributeRegistry ID in BIOS paylaod is incorrect\n",__FUNCTION__));
      }
    }
  }
  //
  // Overwrite by platform BIOS Attribute Registry?
  // Check if platform provides URI to the content of BIOS attribute Registry.
  //
  Status = GetRedfishPlatformBiosResourceUri (EfiRedfishPlatformBiosResourceTypeAttributeRegistryUri, &Private->AttrRegUri);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Get URI of AttributeRegistry from platform BIOS resource driver.\n",__FUNCTION__));
    Status = GetRedfishPlatformBiosResourceUri (EfiRedfishPlatformBiosResourceTypeAttributeRegistryId, &Private->AttrRegIdStr);
    if (!EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a: Get AttributeRegistry ID from platform BIOS resource driver.\n",__FUNCTION__));
      Status = RedfishBiosBuildMessageRegFileRedpth (Private);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_INFO, "%a: Build up Redpath for Attribute Registry from platform BIOS resource fail\n",__FUNCTION__));
      }
    }
  }

  if (Private->MessageRegFileRedpath == NULL) {
    //
    // No Redpath is built up, use the information from PCD.
    //
    Private->AttrRegIdStr = (CHAR8 *)PcdGetPtr (PcdRedfishAttributeRegistryId);
    Status = RedfishBiosBuildMessageRegFileRedpth (Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Build up Redpath for Attribute Registry from default PCD fail\n",__FUNCTION__));
      goto ON_EXIT;
    }
  } else {
    //
    // Get AttrRegIdStr from Private->MessageRegFileRedpath.
    //
    Private->AttrRegIdStr = GetRedfishAttributeRegistryIdFromRedpath (Private->MessageRegFileRedpath);
  }
  if (Private->AttrRegUri == NULL) {
    //
    // No URI to Attribute Registry.
    //
    Private->AttrRegUri = (CHAR8 *)PcdGetPtr(PcdRedfishAttributeRegistryOdataId);
  }

  PatchDefaultAttributeRegistryProperty ("Id", Private->AttrRegIdStr);
  PatchDefaultAttributeRegistryProperty ("@odata.id", Private->AttrRegUri);
  //
  // Check if platform provides BIOS attribute Registry.
  //
  Private->PlatformAttributeRegisterResourceMergeType = EfiRedfishPlatformBiosResourceNone;
  GetRedfishPlatformBiosResource (
     EfiRedfishPlatformBiosResourceTypeAttributeRegistry,
     &Private->PlatformAttributeRegisterResourceMergeType,
     &PlatformBiosAttributeRegistryResourceJson,
     NULL
   );

  //
  // Now we heve to check if Attribute Registry ID and URI is described in MessageRegistryFile.
  // - Check if the URI pointed in MessageRegistryFile is identical to Private->AttrRegUri.
  // - We will POST AttributeRegistry instead of PATCH if MessageRegistryFile associated with
  //   AttrRegId is not exist.
  //
  Status = RedfishCheckAttributeRegistryId (
             Private,
             Private->MessageRegFileRedpath,
             &AttributeRegistryUriOnService,
             &AttributeRegistryRedpath
             );
  if ((EFI_ERROR (Status) && Status != EFI_NOT_FOUND && Status != EFI_NOT_READY) ||
      AttributeRegistryUriOnService == NULL ||
      AttributeRegistryRedpath == NULL
      ) {
    if (Status == EFI_ABORTED) {
      DEBUG ((DEBUG_ERROR, "%a: Improper MessageRegistryFile resource\n",__FUNCTION__));
    }
    goto ON_EXIT_FREE_BUFFER;
  }
  Private->NeedToCreateAttrReg = FALSE;
  Private->NeedToCreateMessageFileLocation = FALSE;
  if (Status == EFI_NOT_FOUND) {
    //
    // We will have to post BIOS Attribute Registry to ServiceRoot/Registries, instead
    // of patching the changes.
    //
    Private->NeedToCreateAttrReg = TRUE;
    Private->NeedToCreateMessageFileLocation = TRUE;
  } else if (Status == EFI_NOT_READY){
    Private->NeedToCreateMessageFileLocation = TRUE;
  }
  //
  // Check URI.
  //
  if (AsciiStriCmp (AttributeRegistryUriOnService, Private->AttrRegUri) != 0) {
    DEBUG ((DEBUG_INFO, "%a: Private->AttrRegUri is not matched to the URI described in MessageRegistryFile.\n",__FUNCTION__));
    Private->AttrRegUri = AllocateCopyPool (AsciiStrSize(AttributeRegistryUriOnService), (VOID *)AttributeRegistryUriOnService);
  }
  Private->AttrRegRedpath = AttributeRegistryRedpath;

  if (Private->PlatformAttributeRegisterResourceMergeType != EfiRedfishPlatformBiosResourceNone) {
    if (Private->PlatformAttributeRegisterResourceMergeType == EfiRedfishPlatformBiosResourceMergeToUri) {
      Status = RedfishGetByUri (RedfishService, Private->AttrRegUri, &PlatformAttrRegResponse);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Get AttributeRegistry from Redfish service fail.\n",__FUNCTION__));
        goto ON_EXIT_FREE_BUFFER;
      }
      Private->PlatformAttrRegPayload = PlatformAttrRegResponse.Payload;
    } else if (Private->PlatformAttributeRegisterResourceMergeType == EfiRedfishPlatformBiosResourceOverrideByPlainText ||
               Private->PlatformAttributeRegisterResourceMergeType == EfiRedfishPlatformBiosResourceMergeToPlainText) {
      PlatformAttrRegResponse.Payload = RedfishCreatePayload (TextToJson (PlatformBiosAttributeRegistryResourceJson), RedfishService);
      Private->PlatformAttrRegPayload = PlatformAttrRegResponse.Payload;
    }
    else {
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
    }
  }

  Status = EFI_SUCCESS;
  if (Private->PlatformAttributeRegisterResourceMergeType == EfiRedfishPlatformBiosResourceNone ||
      Private->PlatformAttributeRegisterResourceMergeType == EfiRedfishPlatformBiosResourceMergeToUri ||
      Private->PlatformAttributeRegisterResourceMergeType == EfiRedfishPlatformBiosResourceMergeToPlainText) {
      Status = RedfishExtractAttributeRegistry(&BufLen, &Buffer);
    if (EFI_ERROR (Status)) {
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT;
    }
    ASSERT (BufLen != 0 && Buffer != NULL);
    if (Private->PlatformAttributeRegisterResourceMergeType == EfiRedfishPlatformBiosResourceNone) {
      //
      // For EfiRedfishPlatformBiosResourceNone, use everything generated by EDKII.
      //
      Status = EFI_SUCCESS;
      goto POST_ATTRIBUTE_REGISTRY;
    } else {
      //
      // Buffer contains the Redfish BIOS Attribute Registry which is converted from
      // HII formsets and HII statements with REST style set in HII form.
      // Check if platform OEM BOIS Attribute Registry exists and needs to be merged
      // with Redfish BIOS resource generated earlier.
      //
      //
      // Merge to the Attribute Register already on Redfish service.
      //
      Status = RedfishMergePlatformAttributeRegistry (
                 Private,
                 &Buffer
                 );
      if (Status == EFI_NOT_FOUND) {
        DEBUG((DEBUG_INFO,"%a: There is no existing BIOS Attribute Registry on Redfish service. Use EDKII generated Attribute Registry.\n", __FUNCTION__));
        if (Private->NeedToCreateAttrReg == FALSE) {
          DEBUG((DEBUG_ERROR,"%a: No existing BIOS Attribute Registry on Redfish service and NeedToCreateAttrReg is FALSE.\n", __FUNCTION__));
          ASSERT(FALSE);
        }
        goto POST_ATTRIBUTE_REGISTRY;
      }
    }

    if (EFI_ERROR(Status) && Status != EFI_ALREADY_STARTED) {
      Status = EFI_DEVICE_ERROR;
      goto ON_EXIT_FREE_BUFFER;
    }
    if (Status != EFI_ALREADY_STARTED) {
      BufLen = AsciiStrSize (Buffer);
    } else {
      //
      // No changes on Attribute Registry, skip patching.
      //
      DEBUG((DEBUG_INFO,"%a: There is no changes on BIOS Attribute Registry.\n", __FUNCTION__));
      Status = EFI_SUCCESS;
      goto SKIP_PATCHING;
    }
  } else {
    //
    // Platform OEM BIOS resource is provided and EDKII generated BIOS resource is ignored.
    //
    BufLen = AsciiStrSize (PlatformBiosAttributeRegistryResourceJson);
    //
    // Copy plain text of platform BIOS Attribute Registry.
    // ASSERT if AllocatePool (BufLen) returns NULL.
    //
    Buffer = AllocatePool (BufLen);
    AsciiStrCpyS (Buffer, BufLen, PlatformBiosAttributeRegistryResourceJson);
  }
POST_ATTRIBUTE_REGISTRY:
  //
  // Check if this is the first time BIOS Attribute Registry created.
  //
  if (Private->NeedToCreateAttrReg == TRUE) {
    //
    // Custruct fully AttributeRegistry resource for posting to Registries.
    //
   DEBUG((DEBUG_INFO,"%a: POST BIOS Attribute Registry to Redfish service.\n", __FUNCTION__));
    Status = RedfishConstructFullAttributeRegistryResource (Private, &Buffer, &BufLen, NULL, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a:%d Error happened to Construct BIOS Attribute Registry!\n",__FUNCTION__, __LINE__));
      goto ON_EXIT_FREE_BUFFER;
    }
    Status = RedfishPostAttributeRegistryResource (Private, Buffer);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a:%d Error happened to POST BIOS Attribute Registry!\n",__FUNCTION__, __LINE__));
      goto ON_EXIT_FREE_BUFFER;
    }
  } else {
    //
    // PATCH resource to Registries collection if the content is no identical.
    //
    DEBUG((DEBUG_INFO,"%a: PATCH BIOS Attribute Registry to Redfish service.\n", __FUNCTION__));
    if (RedfishCheckAttributeRegistryEtag (Private, Buffer) != TRUE) {
      Status = RedfishPatchAttributeRegistryResource (RedfishService, Private->AttrRegUri, Buffer, &PatchResponse);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a:%d Error happened to PATCH BIOS Attribute Registry!\n",__FUNCTION__, __LINE__));
        if (PatchResponse.Payload != NULL) {
          DEBUG ((DEBUG_ERROR, "%a:%d Error Message:\n",__FUNCTION__, __LINE__));
          RedfishDumpPayload (PatchResponse.Payload);
        }
      }
      RedfishFreeResponse (
        PatchResponse.StatusCode,
        PatchResponse.HeaderCount,
        PatchResponse.Headers,
        PatchResponse.Payload
      );
    }
  }
SKIP_PATCHING:
  //
  // Everything is good now, check to see if mHiiRedfishPrivateData has to be updated
  // for applying future settings later.
  //
  if (Private->PlatformAttributeRegisterResourceMergeType != EfiRedfishPlatformBiosResourceNone) {
    JsonValueFree (mHiiRedfishPrivateData.JsonData.RegistryEntries);
    GetRegistryEntriesJsonValueContent (Buffer, NULL, &mHiiRedfishPrivateData.JsonData.RegistryEntries);
    if (mHiiRedfishPrivateData.JsonData.RegistryEntries != NULL) {
      Status = JsonObjectSetValue (
                 JsonValueGetObject (mHiiRedfishPrivateData.JsonData.AttributeRegistry),
                   "RegistryEntries",
                   JsonObjectGetValue(
                     JsonValueGetObject (mHiiRedfishPrivateData.JsonData.RegistryEntries),
                     "RegistryEntries"
                   )
                );
    } else {
      Status = EFI_DEVICE_ERROR;
    }
  }

ON_EXIT_FREE_BUFFER:
  if (Private->MessageRegistryFile != NULL) {
    RedfishCleanupPayload (Private->MessageRegistryFile);
  }
  if (Buffer != NULL) {
    FreePool (Buffer);
  }
  if (AttributeRegistryUriOnService != NULL) {
    FreePool (AttributeRegistryUriOnService);
  }
  if (AttributeRegistryRedpath != NULL) {
    FreePool (AttributeRegistryRedpath);
  }
ON_EXIT:
  RedfishFreeResponse (
    PlatformAttrRegResponse.StatusCode,
    PlatformAttrRegResponse.HeaderCount,
    PlatformAttrRegResponse.Headers,
    NULL
    );

  RedfishFreeResponse (
    AttrRegIdResponse.StatusCode,
    AttrRegIdResponse.HeaderCount,
    AttrRegIdResponse.Headers,
    AttrRegIdResponse.Payload
    );

  return Status;
}

/**
  This function will get the pending Settings of Redfish BIOS resource, and route the
  attribute configurations to the system firmware.

  @param[in]  BiosPayload           The Redfish payload of the pending Settings resource.

  @retval EFI_SUCCESS               The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES      The request could not be completed due to a lack of resources.
  @retval EFI_UNSUPPORTED           Unsupported pending settings received.

**/
EFI_STATUS
RedfishRoutePendingSettings (
  IN REDFISH_PAYLOAD                 *BiosPayload
  )
{
  EFI_STATUS                      Status;
  REDFISH_RESPONSE                RedResponse;
  CHAR8                           *Result;

  ASSERT (BiosPayload != NULL);

  Result = NULL;

  Status = RedfishGetByPayload (BiosPayload, "@Redfish.Settings/SettingsObject", &RedResponse);
  if (EFI_ERROR (Status) || !RedfishIsValidOdataType (RedResponse.Payload, "SettingsObject", mOdataTypeList, ARRAY_SIZE (mOdataTypeList))) {

    DEBUG ((DEBUG_ERROR, "[%a:%d] Error happened!\n",__FUNCTION__, __LINE__));
    if (RedResponse.Payload != NULL) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (RedResponse.Payload);
    }

    if (!EFI_ERROR (Status)) {
      Status = EFI_UNSUPPORTED;
    }
    goto ON_EXIT;
  }

  ASSERT (RedResponse.Payload != NULL);

  Status = HiiRedfishRouteBiosSettingsJson (RedfishJsonInPayload (RedResponse.Payload), NULL, &Result);
  if (EFI_ERROR (Status) && Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "RedfishRoutePendingSettings Failed: %a\n", Result));
  } else if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "No pendding settings in BIOS resource\n"));
  }

ON_EXIT:
  RedfishFreeResponse (
    RedResponse.StatusCode,
    RedResponse.HeaderCount,
    RedResponse.Headers,
    RedResponse.Payload
    );

  if (Result != NULL) {
    FreePool (Result);
  }

  return Status;
}

/**
  This function cleanup the pending Settings of Redfish BIOS resource, in order to avoid
  duplicate configuration in future.

  @param[in]  RedfishService        The REDFISH_SERVICE instance to access the Redfish servier.
  @param[in]  BiosPayload           The Redfish payload of the pending settings BIOS resource.

  @retval EFI_SUCCESS               The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES      The request could not be completed due to a lack of resources.
  @retval EFI_INVALID_PARAMETER     RedfishService is NULL or BiosPayload is NULL.
  @retval EFI_DEVICE_ERROR          Failed to cleanup the pending Settings resource.

**/
EFI_STATUS
RedfishCleanupPendingSettings (
  IN REDFISH_SERVICE   RedfishService,
  IN REDFISH_PAYLOAD   *BiosPayload
  )
{
  EFI_STATUS        Status;
  REDFISH_RESPONSE  GetResponse;
  EDKII_JSON_VALUE  EmptyAttributeJsonValue;
  REDFISH_PAYLOAD   EmptyAttributePayload;
  REDFISH_RESPONSE  PatchResponse;
  CHAR8*            EmptySettings;

  if (BiosPayload == NULL || RedfishService == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  EmptyAttributeJsonValue = NULL;
  EmptyAttributePayload = NULL;

  Status = RedfishGetByPayload (BiosPayload, "@Redfish.Settings/SettingsObject", &GetResponse);
  if (EFI_ERROR (Status) || !RedfishIsValidOdataType (GetResponse.Payload, "SettingsObject", mOdataTypeList, ARRAY_SIZE (mOdataTypeList))) {
    DEBUG ((DEBUG_ERROR, "[%a:%d] Error happened!\n",__FUNCTION__, __LINE__));
    if (GetResponse.Payload != NULL) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (GetResponse.Payload);
    }

    if (!EFI_ERROR (Status)) {
      Status = EFI_UNSUPPORTED;
    }

    goto ON_EXIT;
  }

  ASSERT (GetResponse.Payload != NULL);

  EmptySettings = AllocatePool (AsciiStrSize ("{\"Attributes\":{}}"));
  AsciiSPrint (EmptySettings, AsciiStrSize ("{\"Attributes\":{}}"), "%a", "{\"Attributes\":{}}");
  EmptyAttributeJsonValue = TextToJson (EmptySettings);
  if (EmptyAttributeJsonValue == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto ON_EXIT;
  }

  EmptyAttributePayload = RedfishCreatePayload (EmptyAttributeJsonValue, RedfishService);
  JsonValueFree (EmptyAttributeJsonValue);
  if (EmptyAttributePayload == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto ON_EXIT;
  }

  Status = RedfishPatchToPayload (GetResponse.Payload, EmptyAttributePayload, &PatchResponse);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a:%d] Error happened!\n",__FUNCTION__, __LINE__));
    if (PatchResponse.Payload != NULL) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (PatchResponse.Payload);
    }
  }

  RedfishFreeResponse (
    PatchResponse.StatusCode,
    PatchResponse.HeaderCount,
    PatchResponse.Headers,
    PatchResponse.Payload
    );

  if (EmptyAttributePayload != NULL) {
    RedfishCleanupPayload (EmptyAttributePayload);
  }

ON_EXIT:
  RedfishFreeResponse (
    GetResponse.StatusCode,
    GetResponse.HeaderCount,
    GetResponse.Headers,
    GetResponse.Payload
    );

  return Status;
}

/**
  This function will extract the current configuration data from the system firmware and
  convert it to Redfish BIOS resource.

  @param[out]  DataSize       The buffer size of the Data.
  @param[out]  Data           The buffer point saved the returned JSON text.

  @retval EFI_SUCCESS               The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES      The request could not be completed due to a lack of resources.

**/
EFI_STATUS
RedfishExtractBios (
  OUT  UINTN             *DataSize,
  OUT  VOID              **Data
 )
{
  EFI_STATUS                      Status;

  ASSERT (DataSize != NULL);
  ASSERT (Data != NULL);

  *DataSize = 0;
  Status = HiiRedfishExtractBiosJson (DataSize, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    return Status;
  }

  *Data = AllocateZeroPool (*DataSize);
  if (Data == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  Status = HiiRedfishExtractBiosJson (DataSize, *Data);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  return EFI_SUCCESS;

ON_ERROR:
  if (*Data != NULL) {
    FreePool (*Data);
    *Data = NULL;
  }

  *DataSize = 0;
  return Status;
}

/**
  This function check and convert AttributeRegistry ID to
  Redpath and URI.

  @param[in]       Private                Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in, out]  HiiBiosBuffer          Pointer to HII Redfish BIOS attribute payload in JSON format.
                                          HiiRedfishBuffer could be freed and overrided after merging
                                          with platform OEM BIOS attribute resource.

  @retval EFI_SUCCESS     Platform OEM BIOS Attributes resource exists and merged sucessfully.
  @retval EFI_NOT_FOUND   Platform OEM BIOS Attributes resource is not found.
  @retval Others          Other errors happened.

**/
EFI_STATUS
RedfishMergePlatformBiosAttribute (
  IN REDFISH_BIOS_PRIVATE_DATA *Private,
  IN OUT CHAR8 **HiiBiosBuffer
  )
{
  EDKII_JSON_VALUE PlatformBiosJsonValue;
  EDKII_JSON_VALUE HiiBiosJsonValue;
  EDKII_JSON_VALUE PlatformAttributeKeysJson;
  EDKII_JSON_VALUE HiiAttributeKeysJson;
  EDKII_JSON_VALUE HiiAttributeKeyValueJson;
  UINTN PlatformAttibutesCount;
  UINTN HiiAttibutesCount;
  CHAR8 **PlatformAttributeKeyArray;
  CHAR8 **HiiAttributeKeyArray;
  UINTN IndexPlatformCount;
  UINTN IndexHiiCount;

  if (Private->BiosPayload == NULL) {
    return EFI_NOT_FOUND;
  }
  if (HiiBiosBuffer == NULL || *HiiBiosBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  PlatformBiosJsonValue = RedfishJsonInPayload (Private->BiosPayload);
  HiiBiosJsonValue = TextToJson (*HiiBiosBuffer);

  if (PlatformBiosJsonValue == NULL || HiiBiosJsonValue == NULL) {
    return EFI_DEVICE_ERROR;
  }
  PlatformAttributeKeysJson= JsonObjectGetValue(
                                 JsonValueGetObject (PlatformBiosJsonValue),
                                   "Attributes"
                               );

  HiiAttributeKeysJson = JsonObjectGetValue(
                            JsonValueGetObject (HiiBiosJsonValue),
                              "Attributes"
                            );
  if (PlatformAttributeKeysJson == NULL) {
    //
    // Copy all to PlatformAttributeKeysJson.
    //
    JsonObjectSetValue (JsonValueGetObject (PlatformBiosJsonValue), "Attributes", HiiAttributeKeysJson);
    return EFI_SUCCESS;
  }
  if (HiiAttributeKeysJson == NULL) {
    return EFI_SUCCESS;
  }
  PlatformAttibutesCount = 0;
  HiiAttibutesCount = 0;
  PlatformAttributeKeyArray = JsonObjectGetKeys(JsonValueGetObject (PlatformAttributeKeysJson), &PlatformAttibutesCount);
  if (PlatformAttributeKeyArray == NULL && PlatformAttibutesCount != 0) {
    return EFI_OUT_OF_RESOURCES;
  }
  HiiAttributeKeyArray = JsonObjectGetKeys(JsonValueGetObject(HiiAttributeKeysJson), &HiiAttibutesCount);
  if (HiiAttributeKeyArray == NULL && HiiAttibutesCount != 0) {
    FreePool (PlatformAttributeKeyArray);
    return EFI_OUT_OF_RESOURCES;
  }
  for (IndexHiiCount = 0; IndexHiiCount < HiiAttibutesCount; IndexHiiCount ++) {
    for (IndexPlatformCount = 0; IndexPlatformCount < PlatformAttibutesCount; IndexPlatformCount ++) {
      if (AsciiStrCmp (HiiAttributeKeyArray [IndexHiiCount], PlatformAttributeKeyArray [IndexPlatformCount]) == 0 ) {
        break;
      }
    }
    if (IndexPlatformCount == PlatformAttibutesCount) {
      //
      // HII BIOS Attribute is no found in platform BIOS Attribute. Add it to platform BIOS resource.
      //
      HiiAttributeKeyValueJson = JsonObjectGetValue (JsonValueGetObject(HiiAttributeKeysJson), HiiAttributeKeyArray [IndexHiiCount]);
      if (HiiAttributeKeyValueJson == NULL) {
        continue;
      }
      JsonObjectSetValue (JsonValueGetObject(PlatformAttributeKeysJson), HiiAttributeKeyArray [IndexHiiCount], HiiAttributeKeyValueJson);
    }
  }
  if (PlatformAttributeKeyArray != NULL) {
    FreePool(PlatformAttributeKeyArray);
  }
  if (HiiAttributeKeyArray != NULL) {
    FreePool(HiiAttributeKeyArray);
  }
  JsonValueFree (HiiBiosJsonValue);
  if (*HiiBiosBuffer != NULL) {
    FreePool(*HiiBiosBuffer);
  }
  *HiiBiosBuffer = JsonToText (PlatformBiosJsonValue);
  return EFI_SUCCESS;
}
/**
  Check if SettingsObject is exist or not.

  @param[in]  BiosPayload       The payload of Redfish BIOS current setting resource.

  @retval EFI_SUCCESS           Yes, SettingsObject is exist.
  @retval EFI_NOT_FOUND         No, SettingsObject is not exist.

**/
EFI_STATUS
RedfishCheckSettingObjectInBios(
  REDFISH_PAYLOAD BiosPayload
  )
{
  EDKII_JSON_VALUE SettingsJsonValue;
  EDKII_JSON_VALUE SettingsObjectJsonValue;

  mHiiRedfishPrivateData.JsonData.BiosSettingObject = NULL;
  SettingsJsonValue = JsonObjectGetValue(
                         RedfishJsonInPayload (BiosPayload),
                         "@Redfish.Settings"
                         );
  if (SettingsJsonValue == NULL) {
    return EFI_NOT_FOUND;
  }
  SettingsObjectJsonValue = JsonObjectGetValue (SettingsJsonValue, "SettingsObject");
  if (SettingsObjectJsonValue == NULL) {
    return EFI_NOT_FOUND;
  }
  mHiiRedfishPrivateData.JsonData.BiosSettingObject = JsonObjectGetValue (SettingsObjectJsonValue, "@odata.id");
  if (mHiiRedfishPrivateData.JsonData.BiosSettingObject == NULL) {
    return EFI_NOT_FOUND;
  }
  return EFI_SUCCESS;
}

/**
  This function will patch the current firmware configurations to the BIOS resource
  in Redfish server.

  @param[in]  Private           Pointer to REDFISH_BIOS_PRIVATE_DATA.
  @param[in]  RedfishService    The REDFISH_SERVICE instance to access the Redfish servier.
  @param[in]  BiosPayload       The payload of Redfish BIOS current setting resource.

  @retval EFI_SUCCESS               The operation completed successfully.
  @retval EFI_OUT_OF_RESOURCES      The request could not be completed due to a lack of resources.

**/
EFI_STATUS
RedfishPatchBios (
  IN REDFISH_BIOS_PRIVATE_DATA  *Private,
  IN REDFISH_SERVICE            RedfishService,
  IN REDFISH_PAYLOAD            *BiosPayload
  )
{
  EFI_STATUS                      Status;
  EDKII_JSON_VALUE                NewBiosJsonValue;
  REDFISH_PAYLOAD                 NewBiosPayload;
  CHAR8                           *Buffer;
  UINTN                           BufLen;
  REDFISH_RESPONSE                RedResponse;

  ASSERT (BiosPayload != NULL);

  Buffer = NULL;
  NewBiosJsonValue = NULL;
  NewBiosPayload = NULL;

  if (Private->PlatformBiosResourceMergeType == EfiRedfishPlatformBiosResourceMergeToPlainText ||
      Private->PlatformBiosResourceMergeType == EfiRedfishPlatformBiosResourceMergeToUri ||
      Private->PlatformBiosResourceMergeType == EfiRedfishPlatformBiosResourceNone
      ) {
    RedfishCheckSettingObjectInBios(*BiosPayload);
    Status = RedfishExtractBios(&BufLen, &Buffer);
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }
    if (Private->PlatformBiosResourceMergeType != EfiRedfishPlatformBiosResourceNone) {
      Status = RedfishMergePlatformBiosAttribute (
                 Private,
                 &Buffer
                 );
    }
    if (EFI_ERROR (Status) && Status != EFI_UNSUPPORTED) {
      goto ON_EXIT;
    }
    BufLen = AsciiStrSize (Buffer);
    ASSERT (BufLen != 0 && Buffer != NULL);

    NewBiosJsonValue = TextToJson (Buffer);
    if (NewBiosJsonValue == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ON_EXIT;
    }
    //
    // Add additional platform OEM BIOS properties.
    //
    Status = BuildPlatformBiosProperties (NewBiosJsonValue);
    if (EFI_ERROR (Status) && Status != EFI_UNSUPPORTED) {
      DEBUG((DEBUG_ERROR, "Fail to add platform BIOS properties!\n"));
      } else if (Status == EFI_UNSUPPORTED) {
      DEBUG((DEBUG_INFO, "No platform BIOS properties added!\n"));
      }
  } else {
    //
    // Use Redfish BIOS provided by platform.
    //
    NewBiosJsonValue = *BiosPayload;
  }

  NewBiosPayload = RedfishCreatePayload(NewBiosJsonValue, RedfishService);
  JsonValueFree (NewBiosJsonValue);
  if (NewBiosPayload == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  Status = RedfishPatchToPayload (*BiosPayload, NewBiosPayload, &RedResponse);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[%a:%d] Error happened!\n",__FUNCTION__, __LINE__));
    if (RedResponse.Payload != NULL) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (RedResponse.Payload);
    }
  }

  RedfishFreeResponse (
    RedResponse.StatusCode,
    RedResponse.HeaderCount,
    RedResponse.Headers,
    RedResponse.Payload
    );

ON_EXIT:
  if (NewBiosPayload != NULL) {
    RedfishCleanupPayload (NewBiosPayload);
  }

  if (Buffer != NULL) {
    FreePool (Buffer);
  }

  return Status;
}

/**
  Callback function executed when the ReadyToBoot event group is signaled.

  @param[in]  Event    Event whose notification function is being invoked.
  @param[in]  Context  Pointer to the REDFISH_BIOS_PRIVATE_DATA buffer.

**/
VOID
EFIAPI
RedfishBiosReadyToBootCallback (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  EFI_STATUS                 Status;
  REDFISH_SERVICE            RedfishService;
  REDFISH_BIOS_PRIVATE_DATA *Private;
  CHAR8                     *ServiceVersionStr;
  UINTN                     StrConverted;

  if (Context == NULL) {
    return;
  }

  Private = (REDFISH_BIOS_PRIVATE_DATA *) Context;
  RedfishService = Private->RedfishService;
  if (RedfishService == NULL) {
    gBS->CloseEvent(Event);
    return;
  }

  Private->SystemPayload = NULL;
  Private->BiosPayload = NULL;
  if (mRedfishPlatformBiosResourceProtocol == NULL) {
    //
    // Locate EFI_REDFISH_PLATFORM_BIOS_RESOURCE_PROTOCOL for
    // platform OEM BIOS resource.
    //
    Status = gBS->LocateProtocol (
                    &gEfiRedfishPlatformBiosResourceProtocolGuid,
                    NULL,
                    (VOID **)&mRedfishPlatformBiosResourceProtocol
                    );
    if (Status != EFI_SUCCESS && Status != EFI_NOT_FOUND) {
      gBS->CloseEvent(Event);
      return;
    }
  }

  if (Private->ServiceVersionStr == NULL) {
    Status = RedfishGetServiceVersion(RedfishService, &ServiceVersionStr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: No Redfish version provided.\n", __FUNCTION__));
      return;
    }

    Private->ServiceVersionStr = AllocatePool (AsciiStrSize(ServiceVersionStr));
    AsciiStrCpyS (Private->ServiceVersionStr, AsciiStrLen(Private->ServiceVersionStr) + 1, ServiceVersionStr);
    Private->ServiceVersionUnicodeStr = AllocatePool (AsciiStrSize(Private->ServiceVersionStr)* 2);
    AsciiStrnToUnicodeStrS (
      (CONST CHAR8 *)Private->ServiceVersionStr,
      AsciiStrLen (Private->ServiceVersionStr),
      Private->ServiceVersionUnicodeStr,
      AsciiStrLen (Private->ServiceVersionStr) + 1,
      &StrConverted
    );
  }
  DEBUG ((DEBUG_INFO, "%a: Redfish service version is %s.\n", __FUNCTION__, Private->ServiceVersionUnicodeStr));

  Private->ServiceRoot = RedfishGetServiceRootPayload (Private, RedfishService);
  if (Private->ServiceRoot == NULL) {
    goto ON_EXIT;
  }

  Private->Registries = RedfishGetRegistriesPayload (Private, RedfishService);
  if (Private->Registries == NULL) {
    goto ON_EXIT;
  }

  Private->SystemPayload = RedfishGetComputerSystemPayload (Private, RedfishService);
  if (Private->SystemPayload == NULL) {
    goto ON_EXIT;
  }

  Private->BiosPayload = RedfishGetBiosPayload (Private, RedfishService);
  if (Private->BiosPayload == NULL) {
    goto ON_EXIT;
  }

  Status = RedfishPatchAttributeRegistry (Private, RedfishService, Private->BiosPayload);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Patch to AttributeRegistry resource fail.\n"));
  }

  Status = RedfishRoutePendingSettings (Private->BiosPayload);
  if (EFI_ERROR (Status) && Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "Failed to apply the Bios Attributes settings.\n"));
  }
  if (Status != EFI_NOT_FOUND) {
    Status = RedfishCleanupPendingSettings(RedfishService, Private->BiosPayload);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to cleanup the pending setting.\n"));
    }
  } else {
      DEBUG ((DEBUG_INFO, "No BIOS pending settings.\n"));
  }

  Status = RedfishPatchBios (Private, RedfishService, &Private->BiosPayload);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "Redfish: Patch to Bios resource fail.\n"));
  }
ON_EXIT:
  gBS->CloseEvent(Event);
  RedfishCleanupPayload (Private->ServiceRoot);
  RedfishCleanupPayload (Private->PlatformAttrRegPayload);
  RedfishCleanupPayload (Private->BiosPayload);
  RedfishCleanupPayload (Private->SystemPayload);
  RedfishCleanupService (RedfishService);
  Private->RedfishService = NULL;
}

/**
  Initialize a Redfish configure handler.

  This function will be called by the Redfish config driver to initialize each Redfish configure
  handler.

  @param[in]   This                     Pointer to EFI_REDFISH_CONFIG_HANDLER_PROTOCOL instance.
  @param[in]   RedfishConfigServiceInfo Redfish service informaion.

  @retval EFI_SUCCESS                  The handler has been initialized successfully.
  @retval EFI_DEVICE_ERROR             Failed to create or configure the REST EX protocol instance.
  @retval EFI_ALREADY_STARTED          This handler has already been initialized.
  @retval Other                        Error happens during the initialization.
  @retval Other                        Error happens during the initialization.

**/
EFI_STATUS
EFIAPI
RedfishBiosInit (
  IN  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL  *This,
  IN  REDFISH_CONFIG_SERVICE_INFORMATION   *RedfishConfigServiceInfo
  )
{
  EFI_STATUS                      Status;
  REDFISH_BIOS_PRIVATE_DATA       *Private;

  Private = REDFISH_BIOS_PRIVATE_DATA_FROM_PROTOCOL (This);
  if (Private->RedfishService != NULL) {
    return EFI_ALREADY_STARTED;
  }

  Private->RedfishService = RedfishCreateService (RedfishConfigServiceInfo);
  if (Private->RedfishService == NULL) {
    return EFI_DEVICE_ERROR;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  RedfishBiosReadyToBootCallback,
                  (VOID*) Private,
                  &gEfiEventReadyToBootGuid,
                  &Private->Event
                  );
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  return EFI_SUCCESS;

ON_EXIT:
  RedfishCleanupService (Private->RedfishService);
  Private->RedfishService = NULL;
  return Status;
}

/**
  Stop a Redfish configure handler.

  @param[in]   This                Pointer to EFI_REDFISH_CONFIG_HANDLER_PROTOCOL instance.

  @retval EFI_SUCCESS              This handler has been stoped successfully.
  @retval Others                   Some error happened.

**/
EFI_STATUS
EFIAPI
RedfishBiosStop (
  IN  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL       *This
  )
{
  REDFISH_BIOS_PRIVATE_DATA     *Private;

  Private = REDFISH_BIOS_PRIVATE_DATA_FROM_PROTOCOL (This);

  if (Private->Event != NULL) {
    gBS->CloseEvent (Private->Event);
    Private->Event = NULL;
  }

  if (Private->RedfishService != NULL) {
    RedfishCleanupService (Private->RedfishService);
    Private->RedfishService = NULL;
  }

  return EFI_SUCCESS;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param[in]  ImageHandle           The firmware allocated handle for the UEFI image.
  @param[in]  SystemTable           A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.
**/
EFI_STATUS
EFIAPI
RedfishBiosDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                     Status;
  REDFISH_BIOS_PRIVATE_DATA      *Private;

  Private = AllocateZeroPool (sizeof (REDFISH_BIOS_PRIVATE_DATA));
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature          = REDFISH_BIOS_PRIVATE_DATA_SIGNATURE;
  Private->ConfigHandler.Init = RedfishBiosInit;
  Private->ConfigHandler.Stop = RedfishBiosStop;

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiRedfishConfigHandlerProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &Private->ConfigHandler
                  );
  if (EFI_ERROR (Status)) {
    FreePool (Private);
  }

  return Status;
}
