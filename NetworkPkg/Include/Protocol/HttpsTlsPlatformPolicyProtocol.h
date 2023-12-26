/** @file
  This file defines the EDKII HTTPS TLS Platform Protocol interface.

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL_H_
#define EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL_H_

#include <Protocol/Http.h>
#include <Protocol/Tls.h>

#define EEDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL_GUID \
  { \
    0xbfe8e3e3, 0xb884, 0x4a6f, {0xae, 0xd3, 0xb8, 0xdb, 0xeb, 0xc5, 0x58, 0xc0} \
  }

typedef struct _EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL;

///
/// EDKII_PLATFORM_HTTPS_TLS_CONFIG_DATA_VERSION
///
typedef struct {
  UINT8    Major;
  UINT8    Minor;
} EDKII_PLATFORM_HTTPS_TLS_CONFIG_DATA_VERSION;

typedef struct {
  EDKII_PLATFORM_HTTPS_TLS_CONFIG_DATA_VERSION  Version;
  ///
  /// EDKII_PLATFORM_HTTPS_TLS_CONFIG_DATA_VERSION V1.0
  ///
  EFI_TLS_CONNECTION_END                        ConnectionEnd;
  EFI_TLS_VERIFY                                VerifyMethod;
  EFI_TLS_VERIFY_HOST                           VerifyHost;
} EDKII_PLATFORM_HTTPS_TLS_CONFIG_DATA;

/**
  Function to get platform HTTPS TLS Policy.

  @param[in]   This                   Pointer to the EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL
                                      instance.
  @param[in]   HttpHandle             EFI_HTTP_PROTOCOL handle used to transfer HTTP payload.
  @param[out]  PlatformPolicy         Pointer to retrieve EDKII_PLATFORM_HTTPS_TLS_CONFIG_DATA.

  @retval EFI_SUCCESS                 Platform HTTPS TLS config data is returned in
                                      PlatformPolicy.
  @retval EFI_INVALID_PARAMETER       Either HttpHandle or PlatformPolicy is NULL, or both are NULL.
  @retval EFI_NOT_FOUND               No HTTP protocol insterface is found on HttpHandle.
  @retval EFI_UNSUPPORTED             HttpProtocolInstance is not the HTTP instance platform
                                      would like to config.
**/
typedef
EFI_STATUS
(EFIAPI *EDKII_HTTPS_TLS_GET_PLATFORM_POLICY)(
  IN   EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL  *This,
  IN   EFI_HANDLE                                HttpHandle,
  OUT  EDKII_PLATFORM_HTTPS_TLS_CONFIG_DATA      *PlatformPolicy
  );

///
/// Platform can install more than one EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL
/// instances to return the platfrom HTTP TLS policy config data for the
/// multiple HTTP instances.
///
struct _EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL {
  EDKII_HTTPS_TLS_GET_PLATFORM_POLICY  PlatformGetPolicy;
};

extern EFI_GUID  gEdkiiHttpsTlsPlatformPolicyProtocolGuid;
#endif // EDKII_HTTPS_TLS_PLATFORM_POLICY_PROTOCOL_H_
