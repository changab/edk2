/** @file
  Internal Functions for Redfish JSON file and HII configuration converting.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef HII_REDFISH_LIB_H_
#define HII_REDFISH_LIB_H_

#include <Protocol/Http.h>
#include <Library/BaseJsonLib.h>

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
  IN OUT UINTN    *DataSize,
     OUT VOID     *Data        OPTIONAL
  );

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
     OUT VOID     *Data        OPTIONAL
  );

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
  IN            EDKII_JSON_VALUE    BiosSettings,
  IN     CONST CHAR8                *EtagStr,   OPTIONAL
     OUT       CHAR8                **Result
  );

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
     OUT       CHAR8    **Result
  );

#endif
