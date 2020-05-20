/** @file
  This file defines the EFI Source Coding Protocol interface.

  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_SOURCE_CODING_PROTOCOL_H_
#define EFI_SOURCE_CODING_PROTOCOL_H_

#include <Uefi.h>

//
// GUID definitions
//
#define EFI_SOURCE_CODING_PROTOCOL_GUID \
  { \
    0x8AF8A97C, 0x4EC7, 0xB45A, { 0x2A, 0x87, 0x94, 0xB1, 0x6E, 0xA7, 0xB1, 0x85 } \
  }

#define EFI_SOURCE_CODING_UEFI_COMPRESS L"UefiCompress"
#define EFI_SOURCE_CODING_BROTLI        L"Brotli"
#define EFI_SOURCE_CODING_GZIP          L"GZIP"
#define EFI_SOURCE_CODING_LZMA          L"LZMA"
#define EFI_SOURCE_CODING_LZW           L"LZW"
#define EFI_SOURCE_CODING_DEFLATE       L"Deflate"

typedef struct _EFI_SOURCE_CODING_PROTOCOL EFI_SOURCE_CODING_PROTOCOL;
typedef CHAR16 * EFI_SOURCE_CODING_IDENTIFIER;

/**
  Deompress function provided by this source coding instance

  @param[in]    This                 This is the EFI_SOURCE_CODING_PROTOCOL instance.
  @param[in]    Identifier           identifier of source coding.
  @param[in]    CompressedData       Pointer to compressed data.
  @param[in]    CompressedDataLen    Length of decompressed data.
  @param[in]    DecompressedData     Pointer to receive the pointer to decompressed data.
  @param[in]    DecompressedDataLen  Pointer to receive decompressed data length.

  @retval EFI_SUCCESS       Register successfully.
  @retval EFI_UNSUPPORTED   No source coding instance to decompress the source data.
  @retval Others            Fail to register.

--*/
typedef
EFI_STATUS
(EFIAPI *EFI_SOURCE_CODING_DECOMPRESS)(
  IN EFI_SOURCE_CODING_PROTOCOL   *This,
  IN EFI_SOURCE_CODING_IDENTIFIER Identifier,
  IN VOID    *CompressedData,
  IN UINTN   CompressedDataLen,
  OUT VOID   **DecompressedData,
  OUT UINTN  *DecompressedDataLen
);

/**
  Compress function provided by this source coding instance

  @param[in]    This                 This is the EFI_SOURCE_CODING_PROTOCOL instance.
  @param[in]    Identifier           identifier of source coding.
  @param[in]    SourceData           Pointer to the uncompressed data.
  @param[in]    SourceDataLen        Length of uncompressed data.
  @param[in]    CompressedData       Pointer to receive the pointer to compressed data.
  @param[in]    CompressedDataLen    Pointer to receive compressed data length.

  @retval EFI_SUCCESS       Register successfully.
  @retval EFI_UNSUPPORTED   No source coding instance to decompress the source data.
  @retval Others            Fail to register.

--*/
typedef
EFI_STATUS
(EFIAPI *EFI_SOURCE_CODING_COMPRESS)(
  IN EFI_SOURCE_CODING_PROTOCOL   *This,
  IN EFI_SOURCE_CODING_IDENTIFIER Identifier,
  IN VOID    *SourceData,
  IN UINTN   SourceDataLen,
  OUT VOID   **CompressedData,
  OUT UINTN  *CompressedDataLen
);

/**
  This function provides source coding supported on system.

  @param[in]    This                    This is the EFI_SOURCE_CODING_PROTOCOL instance.
  @param[in]    SourceCodingSupported   An array of EFI_SOURCE_CODING_IDENTIFIER of supported
                                        source coding.
  @retval EFI_SUCCESS             Register successfully.
  @retval Others                  Fail to register.

--*/
typedef
EFI_STATUS
(EFIAPI *EFI_SOURCE_CODING_SUPPORTED)(
  IN EFI_SOURCE_CODING_PROTOCOL       *This,
  IN EFI_SOURCE_CODING_IDENTIFIER      *Identifer
);

/**
  This function provides source coding algorithm registration.

  @param[in]    This        This is the EFI_SOURCE_CODING_PROTOCOL instance.
  @param[in]    Identifer   The name of this source coding algorithm.
  @param[in]    Compress    Compress function provided by this source coding instance.
  @param[in]    Decompress  Decompress function provided by this source coding instance.

  @retval EFI_SUCCESS             Register successfully.
  @retval Others                  Fail to register.

--*/
typedef
EFI_STATUS
(EFIAPI *EFI_SOURCE_CODING_REGISTER)(
  IN EFI_SOURCE_CODING_PROTOCOL       *This,
  IN EFI_SOURCE_CODING_IDENTIFIER      Identifer,
  IN EFI_SOURCE_CODING_COMPRESS        Compress,
  IN EFI_SOURCE_CODING_DECOMPRESS      Decompress
);

/** EFI REST JSON to C structure protocol definition.
  *
**/
typedef struct _EFI_SOURCE_CODING_PROTOCOL {
  EFI_SOURCE_CODING_REGISTER       Register;              ///< Register source coding algorithm
  EFI_SOURCE_CODING_SUPPORTED      SourceCodingSupported; ///< the supported source coding.
  EFI_SOURCE_CODING_COMPRESS       Compress;              ///< Compression provided by this source coding instance.
  EFI_SOURCE_CODING_DECOMPRESS     Decompress;            ///< Decompression provided by this source coding instance.
} EFI_SOURCE_CODING_PROTOCOL;

#endif
