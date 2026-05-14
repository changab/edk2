/** @file
  BROTLI UEFI compression internal definitions.

  Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <PiPei.h>
#include <BrotliEncUefiSupport.h>
#include <brotli/encode.h>
#include <brotli/types.h>
#include "../BrotliCustomDecompressLib/brotli/c/common/constants.h"

typedef struct {
  VOID     *Buff;
  UINTN    BuffSize;
} BROTLI_ENC_BUFF;

#define FILE_BUFFER_SIZE    65536
#define BROTLI_HEADER_BYTES 16

EFI_STATUS
EFIAPI
BrotliUefiCompressGetInfo (
  IN  UINTN   SourceSize,
  IN  UINT32  Quality,
  IN  UINT32  LgWin,
  OUT UINT64  *CompressedSizeMax,
  OUT UINT64  *EncoderScratchSize
  );

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
