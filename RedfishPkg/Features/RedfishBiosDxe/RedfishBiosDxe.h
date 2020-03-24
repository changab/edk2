/** @file
  Header file for RedfishBiosDxe driver private data.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_REDFISH_BIOS_DXE_H__
#define EFI_REDFISH_BIOS_DXE_H__

//
// Libraries
//
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HttpLib.h>
#include <Library/PrintLib.h>
#include <Library/RedfishLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

//
// Consumed Protocols
//
#include <Protocol/Smbios.h>
#include <Protocol/RedfishPlatformBiosResource.h>

//
// Produced Protocols
//
#include <Protocol/RedfishConfigHandler.h>

#include "HiiRedfishLib.h"
#include "HiiRedfishJson.h"
#include "RedfishBiosPlatformLib.h"

#define REDFISH_BIOS_PRIVATE_DATA_SIGNATURE   SIGNATURE_32 ('R', 'B', 'P', 'D')

typedef struct {
  UINT32                                 Signature;
  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL    ConfigHandler;
  REDFISH_SERVICE                        RedfishService;
  EFI_EVENT                              Event;
  //
  // HII Redfish BIOS Playload.
  //
  EDKII_JSON_VALUE                              HiiRedfishBiosJsonValue;

  //
  // Below is the payload information of this Redfish service
  //
  CHAR8                                         *ServiceVersionStr;
  CHAR16                                        *ServiceVersionUnicodeStr;
  // Service Root
  //
  REDFISH_PAYLOAD                               ServiceRoot;
  //
  // Registries
  //
  REDFISH_PAYLOAD                               Registries;
  //
  // MessageRegistryFile
  //
  REDFISH_PAYLOAD                               MessageRegistryFile;
  //
  // Computer System instance.
  //
  REDFISH_PAYLOAD                               SystemPayload;
  //
  // BIOS
  //
  EFI_REDFISH_PLATFORM_BIOS_RESOURCE_MERGE_TYPE PlatformBiosResourceMergeType;
  CHAR8                                         *BiosUri;
  CHAR8                                         *BiosRedPath;
  REDFISH_PAYLOAD                               BiosPayload;
  //
  // BIOS Attribute Registry
  //
  EFI_REDFISH_PLATFORM_BIOS_RESOURCE_MERGE_TYPE PlatformAttributeRegisterResourceMergeType;
  REDFISH_PAYLOAD                               PlatformAttrRegPayload;
  CHAR8                                         *AttrRegIdStr;
  CHAR8                                         *MessageRegFileRedpath;
  CHAR8                                         *AttrRegRedpath;
  CHAR8                                         *AttrRegUri;
  BOOLEAN                                       NeedToCreateAttrReg;
  BOOLEAN                                       NeedToCreateMessageFileLocation;
} REDFISH_BIOS_PRIVATE_DATA;

#define REDFISH_BIOS_PRIVATE_DATA_FROM_PROTOCOL(This) \
          CR ((This), REDFISH_BIOS_PRIVATE_DATA, ConfigHandler, REDFISH_BIOS_PRIVATE_DATA_SIGNATURE)

#endif
