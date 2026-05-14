/** @file
  Brotli compression interfaces for UEFI (matches BrotliDecompress.c layout).

  Output format matches BrotliUefiDecompress / BrotliUefiDecompressGetInfo:
  - First 8 bytes: uncompressed size (little-endian UINT64).
  - Next 8 bytes: decoder scratch requirement (little-endian UINT64), conservative.
  - Remaining bytes: Brotli stream.

  Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <BrotliCompressLibInternal.h>

/**
  Allocation routine used by BROTLI compression (scratch bump allocator).

  @param Ptr              Pointer to the BROTLI_ENC_BUFF instance.
  @param Size             The size in bytes to be allocated.

  @return The allocated pointer address, or NULL on failure.
**/
VOID *
BrEncAlloc (
  IN VOID    *Ptr,
  IN size_t  Size
  )
{
  VOID            *Addr;
  BROTLI_ENC_BUFF  *Private;

  Private = (BROTLI_ENC_BUFF *)Ptr;

  if (Private->BuffSize >= Size) {
    Addr               = Private->Buff;
    Private->Buff      = (VOID *)((UINT8 *)Addr + Size);
    Private->BuffSize -= Size;
    return Addr;
  } else {
    ASSERT (FALSE);
    return NULL;
  }
}

/**
  Free routine used by BROTLI compression.

  @param Ptr              Pointer to the BROTLI_ENC_BUFF instance.
  @param Address          The address to be freed.
**/
VOID
BrEncFree (
  IN VOID  *Ptr,
  IN VOID  *Address
  )
{
  //
  // Scratch buffer is owned by the caller; no per-block free is required.
  //
}

/**
  Write a 64-bit value in little-endian form (matches BrotliUefiDecompress header).

  @param Dest             Destination buffer (8 bytes).
  @param Value            Value to store.
**/
STATIC
VOID
BrPutUint64Le (
  OUT UINT8   *Dest,
  IN  UINT64  Value
  )
{
  UINTN  Index;

  for (Index = 0; Index < sizeof (UINT64); Index++) {
    Dest[Index] = (UINT8)(Value & 0xFF);
    Value       = RShiftU64 (Value, 8);
  }
}

/**
  Conservative decoder scratch estimate for the custom 8-byte header field.

  @param LgWin            Effective BROTLI_PARAM_LGWIN value used when encoding.
  @param UncompressedSize Size of the uncompressed payload (unused; reserved).

  @return Recommended UINT64 for the second header field.
**/
STATIC
UINT64
BrRecommendedDecodeScratch (
  IN UINT32  LgWin,
  IN UINTN   UncompressedSize
  )
{
  UINT64  Base;
  UINT64  Ring;
  UINT32  Wbits;

  (VOID)UncompressedSize;

  Wbits = LgWin;
  if (Wbits < BROTLI_MIN_WINDOW_BITS) {
    Wbits = BROTLI_MIN_WINDOW_BITS;
  }

  if (Wbits > BROTLI_MAX_WINDOW_BITS) {
    Wbits = BROTLI_MAX_WINDOW_BITS;
  }

  Base = (UINT64)256 * 1024 + (UINT64)2 * (UINT64)FILE_BUFFER_SIZE;
  Ring = LShiftU64 (1, Wbits);
  return Base + Ring;
}

/**
  Select lgwin based on input size (same policy as BaseTools BrotliCompress).

  @param SourceSize       Uncompressed size in bytes.

  @return Window parameter in the BROTLI_PARAM_LGWIN range.
**/
STATIC
UINT32
BrotliSelectLgWin (
  IN UINTN  SourceSize
  )
{
  UINT32  LgWin;

  LgWin = BROTLI_MIN_WINDOW_BITS;
  if (SourceSize > 0) {
    while ((BROTLI_MAX_BACKWARD_LIMIT (LgWin) < SourceSize) &&
           (LgWin < BROTLI_MAX_WINDOW_BITS))
    {
      LgWin++;
    }
  }

  return LgWin;
}

/**
  Resolve caller lgwin (0 selects automatically).

  @param LgWin            Requested window bits, or 0 for automatic selection.
  @param SourceSize       Uncompressed size in bytes.

  @return Effective window bits.
**/
STATIC
UINT32
BrotliResolveLgWin (
  IN UINT32  LgWin,
  IN UINTN   SourceSize
  )
{
  if (LgWin == 0) {
    return BrotliSelectLgWin (SourceSize);
  }

  if (LgWin < BROTLI_MIN_WINDOW_BITS) {
    return BROTLI_MIN_WINDOW_BITS;
  }

  if (LgWin > BROTLI_MAX_WINDOW_BITS) {
    return BROTLI_MAX_WINDOW_BITS;
  }

  return LgWin;
}

/**
  Clamp quality into the range supported by the Brotli encoder.

  @param Quality          Requested quality.

  @return Clamped quality.
**/
STATIC
UINT32
BrotliClampQuality (
  IN UINT32  Quality
  )
{
  if (Quality < BROTLI_MIN_QUALITY) {
    return BROTLI_MIN_QUALITY;
  }

  if (Quality > BROTLI_MAX_QUALITY) {
    return BROTLI_MAX_QUALITY;
  }

  return Quality;
}

STATIC
EFI_STATUS
BrotliCompressInternal (
  IN CONST VOID  *Source,
  IN UINTN       SourceSize,
  OUT UINT8      *DestBody,
  IN UINTN       DestBodyCapacity,
  OUT UINTN      *CompressedBodySize,
  IN VOID        *BuffInfo,
  IN UINT32      Quality,
  IN UINT32      LgWin
  )
{
  BrotliEncoderState   *EncState;
  UINT8                *Input;
  UINT8                *Output;
  const UINT8          *NextIn;
  UINT8                *NextOut;
  size_t                AvailableIn;
  size_t                AvailableOut;
  UINTN                 SourceConsumed;
  UINTN                 BodyWritten;
  UINTN                 Chunk;
  BROTLI_BOOL           Ok;
  BROTLI_BOOL           InputFinished;
  BrotliEncoderOperation Op;

  AvailableIn = 0;

  EncState = BrotliEncoderCreateInstance (BrEncAlloc, BrEncFree, BuffInfo);
  if (EncState == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Ok = BrotliEncoderSetParameter (
         EncState,
         BROTLI_PARAM_QUALITY,
         Quality
         );
  if (!Ok) {
    BrotliEncoderDestroyInstance (EncState);
    return EFI_INVALID_PARAMETER;
  }

  Ok = BrotliEncoderSetParameter (EncState, BROTLI_PARAM_LGWIN, LgWin);
  if (!Ok) {
    BrotliEncoderDestroyInstance (EncState);
    return EFI_INVALID_PARAMETER;
  }

  if (SourceSize > 0) {
    if (SourceSize < ((UINTN)1 << 30)) {
      Ok = BrotliEncoderSetParameter (
             EncState,
             BROTLI_PARAM_SIZE_HINT,
             (uint32_t)SourceSize
             );
    } else {
      Ok = BrotliEncoderSetParameter (EncState, BROTLI_PARAM_SIZE_HINT, (1u << 30));
    }

    if (!Ok) {
      BrotliEncoderDestroyInstance (EncState);
      return EFI_INVALID_PARAMETER;
    }
  }

  Input  = (UINT8 *)BrEncAlloc (BuffInfo, FILE_BUFFER_SIZE);
  Output = (UINT8 *)BrEncAlloc (BuffInfo, FILE_BUFFER_SIZE);
  if ((Input == NULL) || (Output == NULL)) {
    BrotliEncoderDestroyInstance (EncState);
    return EFI_OUT_OF_RESOURCES;
  }

  SourceConsumed = 0;
  BodyWritten    = 0;
  InputFinished  = BROTLI_FALSE;

  while (!BrotliEncoderIsFinished (EncState)) {
    if (!InputFinished && (AvailableIn == 0)) {
      if (SourceConsumed >= SourceSize) {
        InputFinished = BROTLI_TRUE;
        AvailableIn   = 0;
        NextIn        = Input;
      } else {
        Chunk = SourceSize - SourceConsumed;
        if (Chunk > FILE_BUFFER_SIZE) {
          Chunk = FILE_BUFFER_SIZE;
        }

        CopyMem (Input, (CONST UINT8 *)Source + SourceConsumed, Chunk);
        SourceConsumed += Chunk;
        AvailableIn = (size_t)Chunk;
        NextIn      = Input;
      }
    }

    if (InputFinished) {
      Op = BROTLI_OPERATION_FINISH;
    } else {
      Op = BROTLI_OPERATION_PROCESS;
    }

    if (BodyWritten >= DestBodyCapacity) {
      BrotliEncoderDestroyInstance (EncState);
      return EFI_BUFFER_TOO_SMALL;
    }

    AvailableOut = DestBodyCapacity - BodyWritten;
    if (AvailableOut > FILE_BUFFER_SIZE) {
      AvailableOut = FILE_BUFFER_SIZE;
    }

    NextOut = Output;
    Ok      = BrotliEncoderCompressStream (
               EncState,
               Op,
               &AvailableIn,
               &NextIn,
               &AvailableOut,
               &NextOut,
               NULL
               );
    if (!Ok) {
      BrotliEncoderDestroyInstance (EncState);
      return EFI_INVALID_PARAMETER;
    }

    if ((UINTN)(NextOut - Output) > 0) {
      if (BodyWritten + (UINTN)(NextOut - Output) > DestBodyCapacity) {
        BrotliEncoderDestroyInstance (EncState);
        return EFI_BUFFER_TOO_SMALL;
      }

      CopyMem (DestBody + BodyWritten, Output, (UINTN)(NextOut - Output));
      BodyWritten += (UINTN)(NextOut - Output);
    }
  }

  BrotliEncoderDestroyInstance (EncState);
  *CompressedBodySize = BodyWritten;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
BrotliUefiCompressGetInfo (
  IN  UINTN   SourceSize,
  IN  UINT32  Quality,
  IN  UINT32  LgWin,
  OUT UINT64  *CompressedSizeMax,
  OUT UINT64  *EncoderScratchSize
  )
{
  UINT64               MaxEnc64;
  UINT64               ScratchEnc;
  UINT32               Q;
  UINT32               Win;
  size_t               Peak;

  if ((CompressedSizeMax == NULL) || (EncoderScratchSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Q   = BrotliClampQuality (Quality);
  Win = BrotliResolveLgWin (LgWin, SourceSize);

  MaxEnc64 = (UINT64)BrotliEncoderMaxCompressedSize ((size_t)SourceSize);
  if ((SourceSize != 0) && (MaxEnc64 == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if (MaxEnc64 > MAX_UINT64 - BROTLI_HEADER_BYTES) {
    return EFI_UNSUPPORTED;
  }

  *CompressedSizeMax = MaxEnc64 + BROTLI_HEADER_BYTES;

  Peak = BrotliEncoderEstimatePeakMemoryUsage ((int)Q, (int)Win, (size_t)SourceSize);
  ScratchEnc = (UINT64)Peak + (UINT64)2 * (UINT64)FILE_BUFFER_SIZE;
  *EncoderScratchSize = ScratchEnc;
  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS          Status;
  BROTLI_ENC_BUFF     BroBuff;
  UINT64              EncScratchNeed;
  UINT64              MaxEnc64;
  UINT32              Q;
  UINT32              Win;
  UINTN               BodyCapacity;
  UINTN               BodyWritten;
  UINT8               *DestBytes;
  UINT64              DecodeScratch;

  if ((Destination == NULL) || (DestinationSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((SourceSize != 0) && (Source == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (*DestinationSize <= BROTLI_HEADER_BYTES) {
    return EFI_BUFFER_TOO_SMALL;
  }

  Q   = BrotliClampQuality (Quality);
  Win = BrotliResolveLgWin (LgWin, SourceSize);

  Status = BrotliUefiCompressGetInfo (SourceSize, Q, Win, &MaxEnc64, &EncScratchNeed);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (*DestinationSize < (UINTN)MaxEnc64) {
    return EFI_BUFFER_TOO_SMALL;
  }

  if (EncScratchNeed > (UINT64)MAX_UINTN) {
    return EFI_UNSUPPORTED;
  }

  if (ScratchSize < (UINTN)EncScratchNeed) {
    return EFI_BUFFER_TOO_SMALL;
  }

  BroBuff.Buff     = Scratch;
  BroBuff.BuffSize = (UINTN)EncScratchNeed;

  BodyCapacity = *DestinationSize - BROTLI_HEADER_BYTES;
  DestBytes    = (UINT8 *)Destination;

  Status = BrotliCompressInternal (
             Source,
             SourceSize,
             DestBytes + BROTLI_HEADER_BYTES,
             BodyCapacity,
             &BodyWritten,
             (VOID *)&BroBuff,
             Q,
             Win
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DecodeScratch = BrRecommendedDecodeScratch (Win, SourceSize);
  BrPutUint64Le (DestBytes, (UINT64)SourceSize);
  BrPutUint64Le (DestBytes + 8, DecodeScratch);
  *DestinationSize = BROTLI_HEADER_BYTES + BodyWritten;
  return EFI_SUCCESS;
}
