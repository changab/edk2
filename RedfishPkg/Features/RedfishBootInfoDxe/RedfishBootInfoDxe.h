/** @file
  Provides data structures and function headers for EFI_REDFISH_CONFIG_HANDLER_PROTOCOL
  instance in RedfishBootInfoDxe module.

   Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_REDFISH_BOOT_ORDER_H__
#define EFI_REDFISH_BOOT_ORDER_H__

//
// Libraries
//
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/RedfishLib.h>

//
// Protocols
//
#include <Protocol/RedfishConfigHandler.h>
//
// Guids
//
#include <Guid/GlobalVariable.h>

#define REDFISH_BOOT_INFO_PRIVATE_DATA_SIGNATURE   SIGNATURE_32 ('R', 'B', 'I', 'P')

typedef struct {
  UINT32                                 Signature;
  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL    ConfigHandler;

  REDFISH_SERVICE                        RedfishService;
  EFI_EVENT                              Event;

  //
  // For Boot Option red path
  //
  CHAR8                                       *ComputerSystemRedPath;
  REDFISH_PAYLOAD                             ComputerSystemPayload;
  REDFISH_PAYLOAD                             AllowableBootSourcePayload;
  REDFISH_PAYLOAD                             BootOptCollPayload;
} REDFISH_BOOT_INFO_PRIVATE_DATA;

#define REDFISH_BOOT_INFO_PRIVATE_DATA_FROM_PROTOCOL(This) \
          CR ((This), REDFISH_BOOT_INFO_PRIVATE_DATA, ConfigHandler, REDFISH_BOOT_INFO_PRIVATE_DATA_SIGNATURE)


//
// Include files with function prototypes
//
#define BOOT_OPTION_NAME_LEN     sizeof ("Boot####")

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
  );

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
  );

#endif
