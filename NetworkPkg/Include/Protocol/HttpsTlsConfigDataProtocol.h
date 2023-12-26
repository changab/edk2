/** @file
  This file defines the EDKII HTTPS TLS Config Data Protocol

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef EDKII_HTTPS_TLS_CONFIG_DATA_PROTOCOL_H_
#define EDKII_HTTPS_TLS_CONFIG_DATA_PROTOCOL_H_

#include <Protocol/Http.h>
#include <Protocol/Tls.h>

#define EEDKII_HTTPS_TLS_CONFIG_DATA_PROTOCOL_GUID \
  { \
    0xbfe8e3e3, 0xb884, 0x4a6f, {0xae, 0xd3, 0xb8, 0xdb, 0xeb, 0xc5, 0x58, 0xc0} \
  }

///
/// HTTP TLS configuration structure version that  manages
/// structure format of EDKII_HTTPS_TLS_CONFIG_DATA_PROTOCOL.
///
typedef struct {
  UINT8    Major;
  UINT8    Minor;
} EDKII_HTTPS_TLS_CONFIG_DATA_VERSION;

///
/// HTTPS TLS configuration data structure.
///
typedef struct {
  EFI_TLS_VERSION           Version;
  EFI_TLS_CONNECTION_END    ConnectionEnd;
  EFI_TLS_VERIFY            VerifyMethod;
  EFI_TLS_VERIFY_HOST       VerifyHost;
  EFI_TLS_SESSION_STATE     SessionState;
} HTTPS_TLS_CONFIG_DATA;

typedef struct {
  EDKII_HTTPS_TLS_CONFIG_DATA_VERSION    Version;
  ///
  /// EDKII_PLATFORM_HTTPS_TLS_CONFIG_DATA_VERSION V1.0
  ///
  HTTPS_TLS_CONFIG_DATA                  HttpsTlsConfigData;
} EDKII_HTTPS_TLS_CONFIG_DATA_PROTOCOL;

extern EFI_GUID  gEdkiiHttpsTlsConfigDataProtocolGuid;
#endif // EDKII_HTTPS_TLS_CONFIG_DATA_PROTOCOL_H_
