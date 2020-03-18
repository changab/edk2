/** @file
  Provides driver's entry point and unload functions.
  Provides EFI_REDFISH_CONFIG_HANDLER_PROTOCOL instance for configuring Redfish Boot info,
  such as BootOrder, BootNext, etc.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Protocol/RedfishPlatformBiosResource.h>

#include "RedfishBootInfoDxe.h"

REDFISH_ODATA_TYPE_MAPPING    mOdataTypeList[] = {
  {"ComputerSystem", "#ComputerSystem.v1_1_0.ComputerSystem"},
  {"ComputerSystem", "#ComputerSystem.v1_5_1.ComputerSystem"},
  {"BootOptions", "#BootOptionCollection.BootOptionCollection"},
  {"BootOption", "#BootOption.v1_0_0.BootOption"},
  {"BootOption", "#BootOption.v1_0_1.BootOption"}
};

EFI_REDFISH_PLATFORM_BIOS_RESOURCE_PROTOCOL *mRedfishPlatformBiosResourceProtocol;

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
  This get the URI to platform computer system resource in Redpath format.

  @param[in]   ValueLength      Length of bytes of BIOS Attribute value.

  @retval NULL                  No URI to platform BIOS resource.
  @retval Other                 URI to platform BIOS resource.

**/
CHAR8 *
GetRedfishPlatformComputerSystemResourceUriRedpath (
  IN EFI_REDFISH_PLATFORM_BIOS_RESOURCE_TYPE ResourceType
  )
{
  EFI_STATUS Status;
  CHAR8 *Uri;

  Status = GetRedfishPlatformBiosResourceUri (ResourceType, &Uri);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "__FUNCTION__: No URI returned from Platform BIOS Resource Protocol.\n"));
    return NULL;
  }

  if (ResourceType == EfiRedfishPlatformBiosResourceTypeComputerSystemId) {
    return Uri; // This is just a string of computer system instance ID.
  }
  return NULL;
}
/**
  Initialize the Computer System Payload, Allowable Boot Source Payload and Boot Option
  Collection Payload in Redfish boot info structure.

  This function gets Redfish boot info from Private->RedfishService via DxeRedfishLib API.
  If Private is NULL, then ASSERT().

  @param[in, out]  Private       A pointer to private structure of Redfish boot info.

  @retval EFI_SUCCESS            The Redfish boot info structure is initialized.
  @retval EFI_UNSUPPORTED        OdataType returned from Redfish service is not supported.
  @retval EFI_OUT_OF_RESOURCES   The initialization could not be completed due to lack of resource.
  @retval EFI_INVALID_PARAMETER  Any input parameter in Private is NULL.

**/
EFI_STATUS
InitializeBootOrderPayload (
  IN OUT REDFISH_BOOT_INFO_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS            Status;
  REDFISH_RESPONSE      RedResponse;
  CHAR8                 *Id;
  ASSERT (Private != NULL);

  Status = EFI_SUCCESS;
  Id = NULL;
  //
  // 1. Initialize Computer System Payload
  //
  Private->ComputerSystemRedPath = GetRedfishPlatformComputerSystemResourceUriRedpath (EfiRedfishPlatformBiosResourceTypeComputerSystemUri);
  if (Private->ComputerSystemRedPath == NULL) {
    // No platform specific URI to ComputerSystem instance.
    // Use platform computer system instance ID or UUID from SMBIOS.
    //
    Id = GetRedfishPlatformComputerSystemResourceUriRedpath (EfiRedfishPlatformBiosResourceTypeComputerSystemId);
    if (Id != NULL) {
      Private->ComputerSystemRedPath = RedfishBuildPathWithSystemUuid("/v1/Systems[UUID~%a]", FALSE, Id);
      FreePool (Id);
    } else {
      Private->ComputerSystemRedPath = RedfishBuildPathWithSystemUuid("/v1/Systems[UUID~%g]", TRUE, NULL);
    }
  }

  if (Private->ComputerSystemRedPath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = RedfishGetByService (
             Private->RedfishService,
             Private->ComputerSystemRedPath,
             &RedResponse
             );
  if (EFI_ERROR (Status) || !RedfishIsValidOdataType (RedResponse.Payload, "ComputerSystem", mOdataTypeList, ARRAY_SIZE (mOdataTypeList))) {

    DEBUG ((EFI_D_ERROR, "[%a:%d] Error Happened!\n",__FUNCTION__, __LINE__));
    if (RedResponse.Payload != NULL) {
      DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (RedResponse.Payload);

      RedfishFreeResponse (
        NULL,
        0,
        NULL,
        RedResponse.Payload
        );
    }

    if (!EFI_ERROR (Status)) {
      Status = EFI_UNSUPPORTED;
    }
    goto ON_EXIT;
  }

  Private->ComputerSystemPayload = RedResponse.Payload;
  ASSERT (Private->ComputerSystemPayload != NULL);

  //
  // 2. Initialize Redfish Allowable Boot Source Payload
  // If it is a NULL value, all Boot Source Override Target will be supported
  //
  Status = RedfishGetByPayload (
             Private->ComputerSystemPayload,
             "Boot/BootSourceOverrideTarget@Redfish.AllowableValues",
             &RedResponse
             );
  if (EFI_ERROR (Status)) {

    if (RedResponse.Payload != NULL) {

      RedfishFreeResponse (
        NULL,
        0,
        NULL,
        RedResponse.Payload
        );
    }
    Private->AllowableBootSourcePayload = NULL;

  } else {
    Private->AllowableBootSourcePayload = RedResponse.Payload;
  }

  //
  // 3. Initialize Boot Option Collection Payload
  //
  Status = RedfishGetByPayload (
             Private->ComputerSystemPayload,
             "Boot/BootOptions",
             &RedResponse
             );
  if (EFI_ERROR (Status) || !RedfishIsValidOdataType (RedResponse.Payload, "BootOptions", mOdataTypeList, ARRAY_SIZE (mOdataTypeList))) {
    DEBUG ((EFI_D_ERROR, "[%a:%d] Error Happened!\n",__FUNCTION__, __LINE__));
    if (RedResponse.Payload != NULL) {
      DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (RedResponse.Payload);

      RedfishFreeResponse (
        NULL,
        0,
        NULL,
        RedResponse.Payload
        );
    }

    if (Private->AllowableBootSourcePayload != NULL) {
      RedfishCleanupPayload(Private->AllowableBootSourcePayload);
    }
    Private->AllowableBootSourcePayload = NULL;

    RedfishCleanupPayload(Private->ComputerSystemPayload);
    Private->ComputerSystemPayload = NULL;

    if (!EFI_ERROR (Status)) {
      Status = EFI_UNSUPPORTED;
    }
    goto ON_EXIT;
  }

  Private->BootOptCollPayload = RedResponse.Payload;
  ASSERT (Private->BootOptCollPayload != NULL);

ON_EXIT:
  RedfishFreeResponse (
    RedResponse.StatusCode,
    RedResponse.HeaderCount,
    RedResponse.Headers,
    NULL
    );

  return Status;
}

/**
  Check if the Payload is a valid boot order payload.

  @param[in]  Payload            A pointer to Redfish payload.

  @retval FALSE                  The input Redfish payload is NULL or invalid.
  @retval TRUE                   The input Redfish payload is a valid boot order payload.

**/
BOOLEAN
IsValidBootOrderPayload (
  IN  REDFISH_PAYLOAD     Payload
  )
{
  EDKII_JSON_VALUE  JsonValue;

  JsonValue = NULL;

  if (Payload == NULL) {
    return FALSE;
  }

  JsonValue = RedfishJsonInPayload(Payload);
  if (JsonValue == NULL) {
    return FALSE;
  }

  if (!JsonValueIsArray (JsonValue)) {
    return FALSE;
  }

  if (JsonArrayCount ((EDKII_JSON_ARRAY) JsonValue) == 0) {
    return FALSE;
  }

  return TRUE;
}

/**
  Check if the Boot Source Override Target is supported or not.

  @param[in]  Payload            A pointer to Redfish payload.
  @param[in]  OverrideTarget     A pointer to Boot Source Override target.

  @retval FALSE                  The override target is not supported.
  @retval TRUE                   The override target is supported.

**/
BOOLEAN
IsOverrideTargetSupported (
  IN  REDFISH_PAYLOAD     Payload,
  IN  CONST CHAR8         *OverrideTarget
  )
{
  UINTN               Index;
  CONST CHAR8         *TempAsciiStr;
  EDKII_JSON_VALUE    Json;

  if (Payload == NULL) {
    return TRUE;
  }

  ASSERT (JsonValueIsArray (RedfishJsonInPayload (Payload)));

  TempAsciiStr = NULL;
  EDKII_JSON_ARRAY_FOREACH (RedfishJsonInPayload (Payload), Index, Json) {

    TempAsciiStr = JsonValueGetAsciiString (Json);
    if (TempAsciiStr == NULL) {
      return FALSE;
    }

    if (AsciiStrCmp (TempAsciiStr, OverrideTarget) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}


/**
  Get HEX number xxxx from "Bootxxxx".

  @param[in]  BootStr            A boot option string, such as "Bootxxxx".

  @return return HEX number or LoadOptionNumberMax if not valid.
**/
UINTN
GetOptionNumber (
  IN CONST CHAR8        *BootStr
  )
{
  EFI_STATUS   Status;
  UINTN        Number;

  if ((AsciiStrSize (BootStr) != 9) && (CompareMem("Boot", BootStr, 4) != 0)) {
    return LoadOptionNumberMax;
  }

  Status = AsciiStrHexToUintnS (BootStr + 4, NULL, &Number);
  if (EFI_ERROR (Status)) {
    return LoadOptionNumberMax;
  }

  return Number;
}

/**
  Save configured Boot Order to global variable L"BootOrder".

  @param[in]  BootOrderPayload   A pointer to Redfish payload which contains Boot order.

  @retval EFI_SUCCESS            The configured Boot Order is saved to EFI variable.
  @retval EFI_OUT_OF_RESOURCES   The operation could not be completed due to lack of resource.
  @retval EFI_INVALID_PARAMETER  Any input parameter is NULL.

**/
EFI_STATUS
SaveBootOrderToNvVariable (
  IN    REDFISH_PAYLOAD           BootOrderPayload
  )
{
  UINTN                         BootOptCount;
  UINT16                        *OptionOrder;
  UINTN                         Index;
  EDKII_JSON_VALUE              Json;
  UINTN                         Number;
  EFI_STATUS                    Status;

  if (BootOrderPayload == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ASSERT (JsonValueIsArray (RedfishJsonInPayload (BootOrderPayload)));

  OptionOrder = NULL;
  Status      = EFI_SUCCESS;

  BootOptCount = JsonArrayCount ((EDKII_JSON_ARRAY) RedfishJsonInPayload (BootOrderPayload));

  OptionOrder = AllocateZeroPool (BootOptCount * sizeof(UINT16));
  if (OptionOrder == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  BootOptCount = 0;
  EDKII_JSON_ARRAY_FOREACH(RedfishJsonInPayload(BootOrderPayload), Index, Json) {
    Number = GetOptionNumber (JsonValueGetAsciiString (Json));
    if (Number != LoadOptionNumberMax) {
      OptionOrder[BootOptCount++] = (UINT16) Number;
    }
  }

  if (BootOptCount != 0) {
    Status = gRT->SetVariable (
                    EFI_BOOT_ORDER_VARIABLE_NAME,
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                    BootOptCount * sizeof (UINT16),
                    OptionOrder
                    );
  } else {
    FreePool (OptionOrder);
  }

  return Status;
}

/**
  Read current content in L"BootOrder" then create a Redfish payload which contains the content.

  @param[in]   RedfishService    REDFISH_SERVICE instance in use.
  @param[out]  BootOrderPayload  A pointer to Redfish payload which contains Boot order.

  @retval EFI_NOT_FOUND          Variable L"BootOrder" does not exist.
  @retval EFI_SUCCESS            The Redfish payload contains Boot Order is created successfully.
  @retval EFI_OUT_OF_RESOURCES   The operation could not be completed due to lack of resource.
  @retval EFI_INVALID_PARAMETER  Any input parameter is NULL.

**/
EFI_STATUS
CreatePayloadToPatchBootOrder (
  IN  REDFISH_SERVICE                     RedfishService,
  OUT REDFISH_PAYLOAD                     *BootOrderPayload
  )
{
  EFI_STATUS                    Status;
  UINT16                        *OptionOrder;
  UINTN                         OptionOrderSize;
  UINTN                         OptionCount;
  UINTN                         Index;
  UINT16                        OptionNumber;
  CHAR8                         OptionName[BOOT_OPTION_NAME_LEN];
  EDKII_JSON_VALUE              JsonArray;
  EDKII_JSON_VALUE              JsonObjectBoot;
  EDKII_JSON_VALUE              JsonObjectForPatch;
  EDKII_JSON_VALUE              JsonTemp;

  if (BootOrderPayload == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OptionOrder        = NULL;
  JsonArray          = NULL;
  JsonObjectBoot     = NULL;
  JsonObjectForPatch = NULL;

  Status = GetEfiGlobalVariable2 (
             EFI_BOOT_ORDER_VARIABLE_NAME,
             (VOID **) &OptionOrder,
             &OptionOrderSize
             );
  if (EFI_ERROR (Status) || OptionOrder == NULL) {
    return EFI_NOT_FOUND;
  }

  JsonArray = JsonValueInitArray ();
  if (JsonArray == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  JsonObjectBoot = JsonValueInitObject ();
  if (JsonObjectBoot == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  OptionCount = OptionOrderSize / sizeof (UINT16);
  for (Index = 0; Index < OptionCount; Index++) {
    OptionNumber = OptionOrder[Index];
    AsciiSPrint (OptionName, sizeof (OptionName), "Boot%04x", OptionNumber);

    JsonTemp = JsonValueInitAsciiString (OptionName);
    Status = JsonArrayAppendValue (
               JsonArray,
               JsonTemp
               );
    JsonValueFree (JsonTemp);
    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }
  }

  Status = JsonObjectSetValue (JsonObjectBoot, "BootOrder", JsonArray);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  JsonObjectForPatch = JsonValueInitObject ();
  if (JsonObjectForPatch == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  Status = JsonObjectSetValue (JsonObjectForPatch, "Boot", JsonObjectBoot);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  *BootOrderPayload = RedfishCreatePayload (JsonObjectForPatch, RedfishService);

  FreePool (OptionOrder);
  JsonValueFree (JsonArray);
  JsonValueFree (JsonObjectBoot);
  JsonValueFree (JsonObjectForPatch);

  return (*BootOrderPayload != NULL) ? EFI_SUCCESS : EFI_DEVICE_ERROR;

ON_ERROR:

  if (OptionOrder != NULL) {
    FreePool (OptionOrder);
  }
  if (JsonArray != NULL) {
    JsonValueFree (JsonArray);
  }
  if (JsonObjectBoot != NULL) {
    JsonValueFree (JsonObjectBoot);
  }
  if (JsonObjectForPatch != NULL) {
    JsonValueFree (JsonObjectForPatch);
  }

  return Status;
}

/**
  Convert string value of BootSourceOverrideEnabled to JSON then create a Redfish payload which
  contains the content. The string value is "Once", "Disabled" or "Continuous" according to schema.

  @param[in]   RedfishService         REDFISH_SERVICE instance in use.
  @param[in]   StatusStr               A pointer to string value of BootSourceOverrideEnabled.
  @param[out]  OverrideEnabledPayload  A pointer to Redfish payload which contains
                                       BootSourceOverrideEnabled.

  @retval EFI_SUCCESS            The Redfish payload is created successfully.
  @retval EFI_OUT_OF_RESOURCES   The operation could not be completed due to lack of resource.
  @retval EFI_INVALID_PARAMETER  Any input parameter is NULL.

**/
EFI_STATUS
CreatePayloadToPatchOverrideEnabled (
  IN    REDFISH_SERVICE                       RedfishService,
  IN    CHAR8                                 *StatusStr,
  OUT   REDFISH_PAYLOAD                       *OverrideEnabledPayload
  )
{
  EFI_STATUS          Status;
  EDKII_JSON_VALUE    JsonObjectBoot = NULL;
  EDKII_JSON_VALUE    JsonObjectForPatch = NULL;
  EDKII_JSON_VALUE    JsonTemp;

  if ((StatusStr == NULL) || (OverrideEnabledPayload == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (AsciiStrCmp (StatusStr, "Once") != 0 &&
    AsciiStrCmp (StatusStr, "Disabled") != 0 &&
    AsciiStrCmp (StatusStr, "Continuous") != 0) {

    return EFI_INVALID_PARAMETER;
  }

  JsonObjectBoot = JsonValueInitObject ();
  if (JsonObjectBoot == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  JsonObjectForPatch = JsonValueInitObject ();
  if (JsonObjectForPatch == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  JsonTemp = JsonValueInitAsciiString(StatusStr);
  Status = JsonObjectSetValue (
             JsonObjectBoot,
             "BootSourceOverrideEnabled",
             JsonTemp
             );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = JsonObjectSetValue (JsonObjectForPatch, "Boot", JsonObjectBoot);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  *OverrideEnabledPayload = RedfishCreatePayload (JsonObjectForPatch, RedfishService);

  JsonValueFree (JsonObjectBoot);
  JsonValueFree (JsonObjectForPatch);

  return (*OverrideEnabledPayload != NULL) ? EFI_SUCCESS : EFI_DEVICE_ERROR;

ON_ERROR:

  if (JsonObjectBoot != NULL) {
    JsonValueFree (JsonObjectBoot);
  }

  if (JsonObjectForPatch != NULL) {
    JsonValueFree (JsonObjectForPatch);
  }

  return Status;
}

/**
  Create Redfish payload for Boot Option according to BootOption.v1_0_1 schema, which contains
  odata.type, Name, Description, BootOptionReference, Id, DisplayName, UefiDevicePath.

  @param[in]   RedfishService     REDFISH_SERVICE instance in use.
  @param[in]   BootOption         A pointer to common structure which has BootOption.
  @param[out]  BootOptionPayload  A pointer to Redfish payload which contains BootOption content.

  @retval EFI_SUCCESS            The Redfish payload is created successfully.
  @retval EFI_OUT_OF_RESOURCES   The operation could not be completed due to lack of resource.
  @retval EFI_INVALID_PARAMETER  Any input parameter is NULL.

**/
EFI_STATUS
CreateBootOptionPayload (
  IN  REDFISH_SERVICE                  RedfishService,
  IN  EFI_BOOT_MANAGER_LOAD_OPTION     *BootOption,
  OUT REDFISH_PAYLOAD                  *BootOptionPayload
  )
{
  EDKII_JSON_VALUE     BootOptionJson;
  CHAR8                BootOptRef[sizeof"Bootxxxx"];
  CHAR16               *DevicePathStr;
  EFI_STATUS           Status;
  UINTN                Count;
  EDKII_JSON_VALUE     JsonTemp;

  if ((BootOption == NULL) || (BootOptionPayload == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Count = AsciiSPrint (BootOptRef, sizeof (BootOptRef), "Boot%04x", BootOption->OptionNumber);
  if (Count == 0) {
    return EFI_INVALID_PARAMETER;
  }

  BootOptionJson = JsonValueInitObject();
  if (BootOptionJson == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  JsonTemp = JsonValueInitAsciiString ("#BootOption.v1_0_0.BootOption");
  Status   = JsonObjectSetValue (
               BootOptionJson,
               "@odata.type",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  JsonTemp = JsonValueInitAsciiString ("Boot Option");
  Status   = JsonObjectSetValue (
               BootOptionJson,
               "Name",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  JsonTemp = JsonValueInitAsciiString ("UEFI Boot Option");
  Status   = JsonObjectSetValue (
               BootOptionJson,
               "Description",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  JsonTemp = JsonValueInitAsciiString (BootOptRef);
  Status   = JsonObjectSetValue (
               BootOptionJson,
               "BootOptionReference",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  JsonTemp = JsonValueInitAsciiString (BootOptRef);
  Status   = JsonObjectSetValue (
               BootOptionJson,
               "Id",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  JsonTemp = JsonValueInitUnicodeString (BootOption->Description);
  Status   = JsonObjectSetValue (
               BootOptionJson,
               "DisplayName",
               JsonTemp
               );
  JsonValueFree (JsonTemp);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  DevicePathStr = ConvertDevicePathToText (BootOption->FilePath, TRUE, FALSE);
  if (DevicePathStr != NULL) {

    JsonTemp = JsonValueInitUnicodeString (DevicePathStr);
    Status   = JsonObjectSetValue (
                 BootOptionJson,
                 "UefiDevicePath",
                 JsonTemp
                 );
    JsonValueFree (JsonTemp);

    FreePool (DevicePathStr);
    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }
  }

  *BootOptionPayload = RedfishCreatePayload (BootOptionJson, RedfishService);
  JsonValueFree (BootOptionJson);
  if (*BootOptionPayload == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
  } else {
    return EFI_SUCCESS;
  }

ON_ERROR:

  if (BootOptionJson != NULL) {
    JsonValueFree (BootOptionJson);
  }
  return Status;
}

/**
  Get ComputerSystem/Boot/BootOrder from Redfish service and process it. If the service in use does
  not have a valid BootOrder, this function will send current BootOrder value in L"BootOrder" to
  Redfish service, otherwise it updates L"BootOrder" by using the data from Redfish service.

  @param[in]   Private            Private structure data in this module.

  @retval EFI_SUCCESS            The BootOrder is processed successfully.
  @retval EFI_INVALID_PARAMETER  Any input parameter is NULL.

**/
EFI_STATUS
RedfishProcessBootOrder (
  IN REDFISH_BOOT_INFO_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                      Status;
  REDFISH_RESPONSE                RedResponse;
  REDFISH_PAYLOAD                 BootOrderPayload;
  REDFISH_PAYLOAD                 PatchPayload;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  BootOrderPayload = NULL;
  PatchPayload = NULL;

  //
  // Get ComputerSystem/Boot/BootOrder from Redfish service
  //
  Status = RedfishGetByPayload (Private->ComputerSystemPayload, "Boot/BootOrder", &RedResponse);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "[%a:%d] Error Happened!\n",__FUNCTION__, __LINE__));
    if (RedResponse.Payload != NULL) {
      DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (RedResponse.Payload);
    }

    goto ON_EXIT;
  }

  BootOrderPayload = RedResponse.Payload;
  ASSERT (BootOrderPayload != NULL);

  if (!IsValidBootOrderPayload (BootOrderPayload)) {
    //
    // The Redfish service doesn't have a valid "BootOrder" now, use PATCH method to send
    // current BootOrder to Redfish service for initializatin.
    //
    Status = CreatePayloadToPatchBootOrder (Private->RedfishService, &PatchPayload);
    if (EFI_ERROR (Status)) {
      goto ON_EXIT;
    }

    RedfishFreeResponse (
      RedResponse.StatusCode,
      RedResponse.HeaderCount,
      RedResponse.Headers,
      RedResponse.Payload
      );

    Status = RedfishPatchToPayload (Private->ComputerSystemPayload, PatchPayload, &RedResponse);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "[%a:%d] Error Happened!\n",__FUNCTION__, __LINE__));
      if (RedResponse.Payload != NULL) {
        DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
        RedfishDumpPayload (RedResponse.Payload);
      }

      goto ON_EXIT;
    }

  } else {
    //
    // Save the BootOrder to NV variable.
    //
    Status = SaveBootOrderToNvVariable (BootOrderPayload);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Redfish: Failed to save the BootOrder to NV variable.\n"));
    }
  }

ON_EXIT:
  RedfishFreeResponse (
    RedResponse.StatusCode,
    RedResponse.HeaderCount,
    RedResponse.Headers,
    RedResponse.Payload
    );

  return Status;
}

/**
  Get Boot/BootOptions from Redfish service. If the service in use does not have valid BootOptions,
  this function will create BootOptions resource in Redfish service, otherwise it patches BootOptions
  resource in Redfish service by using properties in current BootOption.

  @param[in]   Private            Private structure data in this module.

  @retval EFI_SUCCESS            The BootOptions is processed successfully.
  @retval EFI_UNSUPPORTED        The OdataType extracted from Redfish service is not supported.
  @retval EFI_INVALID_PARAMETER  Any input parameter is NULL.

**/
EFI_STATUS
RedfishProcessBootOptions (
  IN REDFISH_BOOT_INFO_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                    Status;
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptionArray;
  UINTN                         BootOptionCount;
  UINTN                         Index;
  CHAR8                         Query[sizeof("Boot/BootOptions[BootOptionReference=Bootxxxx]")];
  REDFISH_PAYLOAD               BootOptionPayload;
  REDFISH_RESPONSE              BootOptResponse;
  REDFISH_RESPONSE              ResultResponse;
  UINTN                         Count;

  BootOptionCount = 0;
  BootOptionArray = EfiBootManagerGetLoadOptions(&BootOptionCount, LoadOptionTypeBoot);
  for (Index = 0; Index < BootOptionCount; Index ++) {
    Status = CreateBootOptionPayload (Private->RedfishService, &BootOptionArray[Index], &BootOptionPayload);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Count = AsciiSPrint (
              Query,
              sizeof (Query),
              "Boot/BootOptions[BootOptionReference=Boot%04x]",
              BootOptionArray[Index].OptionNumber
              );
    if (Count == 0) {
      return EFI_INVALID_PARAMETER;
    }
    Status = RedfishGetByPayload (Private->ComputerSystemPayload, Query, &BootOptResponse);
    if (EFI_ERROR (Status) || !RedfishIsValidOdataType (BootOptResponse.Payload, "BootOption", mOdataTypeList, ARRAY_SIZE (mOdataTypeList))) {
      DEBUG ((EFI_D_WARN, "[%a:%d] Warning: Doesn't get BootOption from %a!\n",__FUNCTION__, __LINE__, Query));
      if (BootOptResponse.Payload != NULL) {
        DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
        RedfishDumpPayload (BootOptResponse.Payload);
      }

      RedfishFreeResponse (
        BootOptResponse.StatusCode,
        BootOptResponse.HeaderCount,
        BootOptResponse.Headers,
        BootOptResponse.Payload
        );

      if (!EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
      }
    }

    if (BootOptResponse.Payload != NULL) {
      Status = RedfishPatchToPayload (BootOptResponse.Payload, BootOptionPayload, &ResultResponse);
    } else {
      Status = RedfishPostToPayload (Private->BootOptCollPayload, BootOptionPayload, &ResultResponse);
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "[%a:%d] Error Happened!\n",__FUNCTION__, __LINE__));
      if (ResultResponse.Payload != NULL) {
        DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
        RedfishDumpPayload (ResultResponse.Payload);
      }
      RedfishCleanupPayload (BootOptionPayload);
      goto ON_EXIT;
    }

    RedfishFreeResponse (
      BootOptResponse.StatusCode,
      BootOptResponse.HeaderCount,
      BootOptResponse.Headers,
      BootOptResponse.Payload
      );

    RedfishCleanupPayload (BootOptionPayload);

    RedfishFreeResponse (
      ResultResponse.StatusCode,
      ResultResponse.HeaderCount,
      ResultResponse.Headers,
      ResultResponse.Payload
      );
  }

  return EFI_SUCCESS;

ON_EXIT:
  RedfishFreeResponse (
    BootOptResponse.StatusCode,
    BootOptResponse.HeaderCount,
    BootOptResponse.Headers,
    BootOptResponse.Payload
    );

  RedfishFreeResponse (
    ResultResponse.StatusCode,
    ResultResponse.HeaderCount,
    ResultResponse.Headers,
    ResultResponse.Payload
    );

  return Status;
}

/**
  Get Boot/BootNext from Redfish service. If the service in use does not have "Boot/BootNext",
  this function will directly return error, otherwise it will get the content from Redfish service
  and save it to global variable L"BootNext".

  @param[in]   Private            Private structure data in this module.

  @retval EFI_SUCCESS            The "BootNext" is processed successfully.
  @retval EFI_UNSUPPORTED        The "BootNext" extracted from Redfish service is not a string.

**/
EFI_STATUS
RedfishProcessBootNext (
  IN REDFISH_BOOT_INFO_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                    Status;
  REDFISH_RESPONSE              BootNextResponse;
  EDKII_JSON_VALUE              BootNextJsonValue;
  CONST CHAR8                   *BootNext;
  UINTN                         OptionNum;
  REDFISH_PAYLOAD               PatchPayload;
  REDFISH_RESPONSE              ResultResponse;

  BootNextJsonValue = NULL;

  Status = RedfishGetByPayload (Private->ComputerSystemPayload, "Boot/BootNext", &BootNextResponse);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_WARN, "[%a:%d] Doesn't get Boot/BootNext from computer system payload.\n",__FUNCTION__, __LINE__));
    if (BootNextResponse.Payload != NULL) {
      DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (BootNextResponse.Payload);
    }

    RedfishFreeResponse (
      BootNextResponse.StatusCode,
      BootNextResponse.HeaderCount,
      BootNextResponse.Headers,
      BootNextResponse.Payload
      );
  }

  if (BootNextResponse.Payload != NULL) {

    BootNextJsonValue = RedfishJsonInPayload (BootNextResponse.Payload);
    if (BootNextJsonValue == NULL || !JsonValueIsString (JsonObjectGetValue(BootNextJsonValue, "BootNext"))) {
      Status = EFI_UNSUPPORTED;
      goto ON_EXIT;
    }

    BootNext = JsonValueGetAsciiString (JsonObjectGetValue(BootNextJsonValue, "BootNext"));

    if (BootNext != NULL) {
      OptionNum = GetOptionNumber (BootNext);
      if (OptionNum != LoadOptionNumberMax) {
        Status = gRT->SetVariable (
                        EFI_BOOT_NEXT_VARIABLE_NAME,
                        &gEfiGlobalVariableGuid,
                        EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                        sizeof (UINT16),
                        &OptionNum
                        );
        if (EFI_ERROR (Status)) {
          DEBUG ((EFI_D_ERROR, "Redfish: Failed to save the BootNext to NV variable.\n"));
          goto ON_EXIT;
        }

        //
        // "On its next boot cycle, the system will boot (one time) to the Boot Source Override Target. The value of
        // BootSourceOverrideEnabled is then reset back to Disabled." -- From the description of "Once" option
        //
        Status = CreatePayloadToPatchOverrideEnabled (
                   Private->RedfishService,
                   "Disabled",
                   &PatchPayload
                   );
        if (EFI_ERROR (Status)) {
          goto ON_EXIT;
        }

        Status = RedfishPatchToPayload (Private->ComputerSystemPayload, PatchPayload, &ResultResponse);
        if (EFI_ERROR (Status)) {
          DEBUG ((EFI_D_ERROR, "[%a:%d] Error Happened!\n",__FUNCTION__, __LINE__));
          if (ResultResponse.Payload != NULL) {
            DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
            RedfishDumpPayload (ResultResponse.Payload);
          }
        }

        RedfishCleanupPayload (PatchPayload);

        RedfishFreeResponse (
            ResultResponse.StatusCode,
            ResultResponse.HeaderCount,
            ResultResponse.Headers,
            ResultResponse.Payload
            );
      }
    }
  }

ON_EXIT:
  RedfishFreeResponse (
    BootNextResponse.StatusCode,
    BootNextResponse.HeaderCount,
    BootNextResponse.Headers,
    BootNextResponse.Payload
    );

  return Status;
}

/**
  The callback function when gEfiEventReadyToBootGuid is signaled.

  This function will firstly initialize the Computer System Payload, allowable Boot
  Source Payload and Boot Option Collection Payload in Redfish boot info structure.
  Then it will process BootOrder, BootOptions, BootSourceOverrideTarget, BootNext in order.

  @param[in]   Event         Event which will be signaled when ReadyToBoot.
  @param[in]   Context       Private structure data in this module.

**/
VOID
EFIAPI
RedfishBootInfoEventCallBack (
  IN  EFI_EVENT                Event,
  IN  VOID                     *Context
  )
{
  EFI_STATUS                        Status;
  REDFISH_BOOT_INFO_PRIVATE_DATA    *Private;

  REDFISH_RESPONSE           OverrideEnabledResponse;
  EDKII_JSON_VALUE           OverrideEnabledJsonValue;
  REDFISH_RESPONSE           OverrideTargetResponse;
  EDKII_JSON_VALUE           OverrideTargetJsonValue;
  CONST CHAR8                *OverrideEnabledTag;
  CONST CHAR8                *OverrideTarget;

  Status = EFI_SUCCESS;
  ZeroMem (&OverrideEnabledResponse, sizeof (REDFISH_RESPONSE));
  OverrideEnabledJsonValue = NULL;
  ZeroMem (&OverrideTargetResponse, sizeof (REDFISH_RESPONSE));
  OverrideTargetJsonValue  = NULL;

  gBS->CloseEvent(Event);

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

  ASSERT (Context != NULL);
  Private = (REDFISH_BOOT_INFO_PRIVATE_DATA *) Context;

  Status = InitializeBootOrderPayload (Private);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = RedfishProcessBootOrder (Private);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = RedfishProcessBootOptions (Private);
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // Check if Boot Source Override is enabled.
  //
  Status = RedfishGetByPayload (
             Private->ComputerSystemPayload,
             "Boot/BootSourceOverrideEnabled",
             &OverrideEnabledResponse
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "[%a:%d] Error Happened!\n",__FUNCTION__, __LINE__));
    if (OverrideEnabledResponse.Payload != NULL) {
      DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (OverrideEnabledResponse.Payload);
    }

    goto ON_EXIT;
  }

  OverrideEnabledJsonValue = RedfishJsonInPayload (OverrideEnabledResponse.Payload);
  if (OverrideEnabledJsonValue == NULL || !JsonValueIsString (JsonObjectGetValue(OverrideEnabledJsonValue, "BootSourceOverrideEnabled"))) {
    goto ON_EXIT;
  }

  OverrideEnabledTag = JsonValueGetAsciiString (JsonObjectGetValue(OverrideEnabledJsonValue, "BootSourceOverrideEnabled"));

  if (OverrideEnabledTag == NULL ||
    (AsciiStrCmp (OverrideEnabledTag, "Once") != 0 && AsciiStrCmp (OverrideEnabledTag, "Continuous") != 0)) {

    goto ON_EXIT;;
  }

  //
  // Check if Boot Source Override Target is Supported.
  //
  Status = RedfishGetByPayload (
             Private->ComputerSystemPayload,
             "Boot/BootSourceOverrideTarget",
             &OverrideTargetResponse
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "[%a:%d] Error Happened!\n",__FUNCTION__, __LINE__));
    if (OverrideTargetResponse.Payload != NULL) {
      DEBUG ((EFI_D_ERROR, "[%a:%d] Error Message:\n",__FUNCTION__, __LINE__));
      RedfishDumpPayload (OverrideTargetResponse.Payload);
    }

    goto ON_EXIT;
  }

  OverrideTargetJsonValue = RedfishJsonInPayload (OverrideTargetResponse.Payload);
  if (OverrideTargetJsonValue == NULL || !JsonValueIsString (JsonObjectGetValue(OverrideTargetJsonValue, "BootSourceOverrideTarget"))) {
    goto ON_EXIT;
  }

  OverrideTarget = JsonValueGetAsciiString (JsonObjectGetValue(OverrideTargetJsonValue, "BootSourceOverrideTarget"));
  if (OverrideTarget == NULL){
    goto ON_EXIT;
  }

  if (IsOverrideTargetSupported (Private->AllowableBootSourcePayload, OverrideTarget)) {

    //
    // Processing Boot Source Target Override ...
    //
    if (AsciiStrCmp (OverrideTarget, "UefiBootNext") == 0) {

      //
      // "BootSourceOverrideEnabled = Continuous is not supported for UEFI BootNext as this setting is
      // defined in UEFI as a one-time boot only." -- From Schema ComputerSystem.v1_5_1.ComputerSystem
      //
      if (AsciiStrCmp (OverrideEnabledTag, "Once") == 0) {

        DEBUG ((EFI_D_INFO, "[Redfish]: Process Boot Next ...\n"));
        RedfishProcessBootNext (Private);
      }
    }
  }

ON_EXIT:
  RedfishFreeResponse (
    OverrideEnabledResponse.StatusCode,
    OverrideEnabledResponse.HeaderCount,
    OverrideEnabledResponse.Headers,
    OverrideEnabledResponse.Payload
    );

  RedfishFreeResponse (
    OverrideTargetResponse.StatusCode,
    OverrideTargetResponse.HeaderCount,
    OverrideTargetResponse.Headers,
    OverrideTargetResponse.Payload
    );

  RedfishCleanupService (Private->RedfishService);
  Private->RedfishService = NULL;

  return;
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

**/
EFI_STATUS
EFIAPI
RedfishBootInfoInit (
  IN  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL  *This,
  IN  REDFISH_CONFIG_SERVICE_INFORMATION   *RedfishConfigServiceInfo
  )
{
  REDFISH_BOOT_INFO_PRIVATE_DATA    *Private;
  EFI_STATUS                        Status;

  Private = REDFISH_BOOT_INFO_PRIVATE_DATA_FROM_PROTOCOL (This);
  if (Private->RedfishService != NULL) {
    return EFI_ALREADY_STARTED;
  }

  //
  // Create REDFISH_SERVICE handler
  //
  Private->RedfishService = RedfishCreateService (RedfishConfigServiceInfo);
  if (Private->RedfishService == NULL) {
    Status = EFI_DEVICE_ERROR;
    goto ON_ERROR;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  RedfishBootInfoEventCallBack,
                  (VOID*) Private,
                  &gEfiEventReadyToBootGuid,
                  &Private->Event
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  return EFI_SUCCESS;

ON_ERROR:

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
RedfishBootInfoStop (
  IN  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL       *This
  )
{
  REDFISH_BOOT_INFO_PRIVATE_DATA     *Private;

  Private = REDFISH_BOOT_INFO_PRIVATE_DATA_FROM_PROTOCOL (This);

  if (Private->Event != NULL) {
    gBS->CloseEvent (Private->Event);
    Private->Event = NULL;
  }

  if (Private->RedfishService != NULL) {
    RedfishCleanupService (Private->RedfishService);
    Private->RedfishService = NULL;
  }

  if (Private->ComputerSystemRedPath != NULL) {
    FreePool (Private->ComputerSystemRedPath);
    Private->ComputerSystemRedPath = NULL;
  }

  if (Private->ComputerSystemPayload != NULL) {
    RedfishCleanupPayload (Private->ComputerSystemPayload);
    Private->ComputerSystemPayload = NULL;
  }

  if (Private->BootOptCollPayload != NULL) {
    RedfishCleanupPayload (Private->BootOptCollPayload);
    Private->BootOptCollPayload = NULL;
  }

  return EFI_SUCCESS;
}

/**
  Unloads an image.

  @param  ImageHandle           Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
  @retval EFI_INVALID_PARAMETER ImageHandle is not a valid image handle.

**/
EFI_STATUS
EFIAPI
RedfishBootInfoUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                             Status;
  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL    *ConfigHandler;
  REDFISH_BOOT_INFO_PRIVATE_DATA         *Private;

  ConfigHandler = NULL;
  Private = NULL;

  //
  // Firstly, find ConfigHandler Protocol interface in this ImageHandle.
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiRedfishConfigHandlerProtocolGuid,
                  (VOID **) &ConfigHandler,
                  NULL,
                  NULL,
                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
                  );
  if (EFI_ERROR (Status) || ConfigHandler == NULL) {
    return Status;
  }

  //
  // Then, Stop the ConfigHandler service.
  //
  Private = REDFISH_BOOT_INFO_PRIVATE_DATA_FROM_PROTOCOL (ConfigHandler);

  ASSERT (Private != NULL);

  if (Private->RedfishService != NULL) {
    ConfigHandler->Stop (ConfigHandler);
  }
  FreePool (Private);

  //
  // Last, uninstall ConfigHandler Protocol.
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
                  ImageHandle,
                  &gEfiRedfishConfigHandlerProtocolGuid, ConfigHandler,
                  NULL
                  );

  return Status;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers. It initialize the global variables and
  publish the driver binding protocol.

  @param[in]   ImageHandle      The firmware allocated handle for the UEFI image.
  @param[in]   SystemTable      A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_ACCESS_DENIED     EFI_ISCSI_INITIATOR_NAME_PROTOCOL was installed unexpectedly.
  @retval Others                Other errors as indicated.
**/
EFI_STATUS
EFIAPI
RedfishBootInfoEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                     Status;
  REDFISH_BOOT_INFO_PRIVATE_DATA      *Private;

  Private = AllocateZeroPool (sizeof (REDFISH_BOOT_INFO_PRIVATE_DATA));
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature          = REDFISH_BOOT_INFO_PRIVATE_DATA_SIGNATURE;
  Private->ConfigHandler.Init = RedfishBootInfoInit;
  Private->ConfigHandler.Stop = RedfishBootInfoStop;

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
