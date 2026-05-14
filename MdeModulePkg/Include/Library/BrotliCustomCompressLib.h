/** @file
  Public interfaces for BrotliCustomCompressLib.

  This library compresses buffers into the same wire format consumed by
  BrotliCustomDecompressLib (16-byte custom header + Brotli stream).

  Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef BROTLI_CUSTOM_COMPRESS_LIB_H_
#define BROTLI_CUSTOM_COMPRESS_LIB_H_

#include <Base.h>

/**
  Return upper bounds for compressed output and encoder scratch memory.

  @param[in]  SourceSize          Uncompressed payload size in bytes.
  @param[in]  Quality             Brotli quality (clamped to BROTLI_MIN_QUALITY..BROTLI_MAX_QUALITY).
  @param[in]  LgWin               Window bits, or 0 to select automatically from SourceSize.
  @param[out] CompressedSizeMax   Maximum size of the full output (16-byte header + stream).
  @param[out] EncoderScratchSize  Scratch bytes required for BrotliUefiCompress with the same parameters.

  @retval EFI_SUCCESS            Sizes were returned.
  @retval EFI_INVALID_PARAMETER  CompressedSizeMax or EncoderScratchSize is NULL.
  @retval EFI_UNSUPPORTED        SourceSize is too large to represent safely.

**/
EFI_STATUS
EFIAPI
BrotliUefiCompressGetInfo (
  IN  UINTN   SourceSize,
  IN  UINT32  Quality,
  IN  UINT32  LgWin,
  OUT UINT64  *CompressedSizeMax,
  OUT UINT64  *EncoderScratchSize
  );

/**
  Compress a buffer into the Brotli custom format used by BrotliUefiDecompress.

  @param[in]      Source           Uncompressed data.
  @param[in]      SourceSize       Size of Source in bytes.
  @param[out]     Destination      Output buffer.
  @param[in,out]  DestinationSize  On input, capacity of Destination; on success, bytes written.
  @param[in]      Scratch          Encoder scratch buffer.
  @param[in]      ScratchSize      Size of Scratch in bytes.
  @param[in]      Quality          Brotli quality level.
  @param[in]      LgWin            Window bits, or 0 for automatic selection.

  @retval EFI_SUCCESS             Compression completed.
  @retval EFI_INVALID_PARAMETER   A pointer argument is invalid.
  @retval EFI_BUFFER_TOO_SMALL    Destination or scratch is too small.
  @retval EFI_OUT_OF_RESOURCES    Encoder could not be initialized.
  @retval EFI_UNSUPPORTED         SourceSize is not supported.

**/
EFI_STATUS
EFIAPI
BrotliUefiCompress (
  IN CONST VOID  *Source,
  IN UINTN       SourceSize,
  OUT VOID       *Destination,
  IN OUT UINTN   *DestinationSize,
  IN VOID        *Scratch,
  IN UINTN       ScratchSize,
  IN UINT32      Quality,
  IN UINT32      LgWin
  );

#endif
