/** @file
  This file defines the EFI_REDFISH_CONFIG_HANDLER_PROTOCOL interface.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_REDFISH_CONFIG_HANDLER_H_
#define EFI_REDFISH_CONFIG_HANDLER_H_

#include <Protocol/RedfishDiscover.h>

typedef struct _EFI_REDFISH_CONFIG_HANDLER_PROTOCOL EFI_REDFISH_CONFIG_HANDLER_PROTOCOL;

#define EFI_REDFISH_CONFIG_HANDLER_PROTOCOL_GUID \
    {  \
      0xbc0fe6bb, 0x2cc9, 0x463e, { 0x90, 0x82, 0xfa, 0x11, 0x76, 0xfc, 0x67, 0xde }  \
    }

typedef struct _EFI_REDFISH_DISCOVERED_INFORMATION  REDFISH_CONFIG_SERVICE_INFORMATION;

/**
  Initialize a Redfish configure handler.

  This function will be called by the Redfish config driver to initialize each Redfish configure
  handler.

  @param[in]   This                    Pointer to EFI_REDFISH_CONFIG_HANDLER_PROTOCOL instance.
  @param[in]   RedfishServiceinfo      Redfish service information.

  @retval EFI_SUCCESS                  The handler has been initialized successfully.
  @retval EFI_DEVICE_ERROR             Failed to create or configure the REST EX protocol instance.
  @retval EFI_ALREADY_STARTED          This handler has already been initialized.
  @retval Other                        Error happens during the initialization.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REDFISH_CONFIG_HANDLER_PROTOCOL_INIT) (
  IN  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL *This,
  IN  REDFISH_CONFIG_SERVICE_INFORMATION  *RedfishServiceinfo
  );

/**
  Stop a Redfish configure handler.

  @param[in]   This                Pointer to EFI_REDFISH_CONFIG_HANDLER_PROTOCOL instance.

  @retval EFI_SUCCESS              This handler has been stoped successfully.
  @retval Others                   Some error happened.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REDFISH_CONFIG_HANDLER_PROTOCOL_STOP) (
  IN     EFI_REDFISH_CONFIG_HANDLER_PROTOCOL    *This
  );

struct _EFI_REDFISH_CONFIG_HANDLER_PROTOCOL {
  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL_INIT      Init;
  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL_STOP      Stop;
};


extern EFI_GUID gEfiRedfishConfigHandlerProtocolGuid;

#endif
