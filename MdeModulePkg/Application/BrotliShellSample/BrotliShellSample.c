/** @file
  Sample UEFI application: compress and decompress ASCII text using the
  Brotli custom format (same layout as BrotliCustomDecompressLib).

  This module links BrotliCustomCompressLib only. Decompression logic here is
  aligned with Library/BrotliCustomDecompressLib/BrotliDecompress.c so output
  from BrotliUefiCompress can be expanded without linking the decompress NULL
  library (which would duplicate brotli symbols).

  Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BrotliCustomCompressLib.h>

#include <BrotliDecUefiSupport.h>
#include <brotli/decode.h>

#define SAMPLE_FILE_BUFFER_SIZE  65536
#define SAMPLE_BROTLI_SCRATCH_MAX  16
#define SAMPLE_BROTLI_INFO_SIZE    8
#define SAMPLE_BROTLI_DECODE_MAX   8

typedef struct {
  VOID     *Buff;
  UINTN    BuffSize;
} SAMPLE_BROTLI_BUFF;

STATIC
VOID *
SampleBrAlloc (
  IN VOID    *Ptr,
  IN size_t  Size
  )
{
  VOID                *Addr;
  SAMPLE_BROTLI_BUFF  *Private;

  Private = (SAMPLE_BROTLI_BUFF *)Ptr;

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

STATIC
VOID
SampleBrFree (
  IN VOID  *Ptr,
  IN VOID  *Address
  )
{
}

STATIC
UINT64
SampleBrGetDecodedSizeOfBuf (
  IN UINT8  *EncodedData,
  IN UINT8  StartOffset,
  IN UINT8  EndOffset
  )
{
  UINT64  DecodedSize;
  INTN    Index;

  DecodedSize = 0;
  for (Index = EndOffset - 1; Index >= StartOffset; Index--) {
    DecodedSize = LShiftU64 (DecodedSize, 8) + EncodedData[Index];
  }

  return DecodedSize;
}

STATIC
EFI_STATUS
SampleBrotliDecompressInternal (
  IN CONST VOID  *Source,
  IN UINTN       SourceSize,
  IN OUT VOID    *Destination,
  IN VOID        *BuffInfo
  )
{
  UINT8                *Input;
  UINT8                *Output;
  const UINT8          *NextIn;
  UINT8                *NextOut;
  size_t               TotalOut;
  size_t               AvailableIn;
  size_t               AvailableOut;
  VOID                 *Temp;
  BrotliDecoderResult  Result;
  BrotliDecoderState   *BroState;

  TotalOut     = 0;
  AvailableOut = SAMPLE_FILE_BUFFER_SIZE;
  Result       = BROTLI_DECODER_RESULT_ERROR;
  BroState     = BrotliDecoderCreateInstance (SampleBrAlloc, SampleBrFree, BuffInfo);
  Temp         = Destination;

  if (BroState == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Input  = (UINT8 *)SampleBrAlloc (BuffInfo, SAMPLE_FILE_BUFFER_SIZE);
  Output = (UINT8 *)SampleBrAlloc (BuffInfo, SAMPLE_FILE_BUFFER_SIZE);
  if ((Input == NULL) || (Output == NULL)) {
    SampleBrFree (BuffInfo, Input);
    SampleBrFree (BuffInfo, Output);
    BrotliDecoderDestroyInstance (BroState);
    return EFI_INVALID_PARAMETER;
  }

  NextOut = Output;
  Result  = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
  while (1) {
    if (Result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
      if (SourceSize == 0) {
        break;
      }

      if (SourceSize >= SAMPLE_FILE_BUFFER_SIZE) {
        AvailableIn = SAMPLE_FILE_BUFFER_SIZE;
      } else {
        AvailableIn = SourceSize;
      }

      CopyMem (Input, Source, AvailableIn);
      Source      = (VOID *)((UINT8 *)Source + AvailableIn);
      SourceSize -= AvailableIn;
      NextIn      = Input;
    } else if (Result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
      CopyMem (Temp, Output, SAMPLE_FILE_BUFFER_SIZE);
      AvailableOut = SAMPLE_FILE_BUFFER_SIZE;
      Temp         = (VOID *)((UINT8 *)Temp + SAMPLE_FILE_BUFFER_SIZE);
      NextOut      = Output;
    } else {
      break;
    }

    Result = BrotliDecoderDecompressStream (
               BroState,
               &AvailableIn,
               &NextIn,
               &AvailableOut,
               &NextOut,
               &TotalOut
               );
  }

  if (NextOut != Output) {
    CopyMem (Temp, Output, (size_t)(NextOut - Output));
  }

  SampleBrFree (BuffInfo, Input);
  SampleBrFree (BuffInfo, Output);
  BrotliDecoderDestroyInstance (BroState);
  return (Result == BROTLI_DECODER_RESULT_SUCCESS) ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
}

STATIC
EFI_STATUS
SampleBrotliUefiDecompress (
  IN CONST VOID  *Source,
  IN UINTN       SourceSize,
  IN OUT VOID    *Destination,
  IN OUT VOID    *Scratch
  )
{
  SAMPLE_BROTLI_BUFF  BroBuff;
  UINT64              GetSize;
  UINT8               MaxOffset;

  MaxOffset = SAMPLE_BROTLI_SCRATCH_MAX;
  GetSize   = SampleBrGetDecodedSizeOfBuf (
                (UINT8 *)Source,
                MaxOffset - SAMPLE_BROTLI_INFO_SIZE,
                MaxOffset
                );

  BroBuff.Buff     = Scratch;
  BroBuff.BuffSize = (UINTN)GetSize;

  return SampleBrotliDecompressInternal (
           (VOID *)((UINT8 *)Source + SAMPLE_BROTLI_SCRATCH_MAX),
           SourceSize - SAMPLE_BROTLI_SCRATCH_MAX,
           Destination,
           (VOID *)(&BroBuff)
           );
}

/**
  UEFI application entry point.

  @param[in]  ImageHandle  Image handle.
  @param[in]  SystemTable  Pointer to the system table.

  @retval EFI_SUCCESS  The operation completed successfully (including self-test pass).
  @retval Others       Memory allocation, compress, or decompress failure.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS   Status;
  UINTN        PlainLen;
  UINTN        CompCap;
  UINTN        CompLen;
  UINT64       CompMax64;
  UINT64       EncScratch64;
  UINT64       DecScratch64;
  UINTN        EncScratch;
  UINTN        DecScratch;
  VOID         *EncScratchBuf;
  VOID         *DecScratchBuf;
  UINT8        *Compressed;
  UINT8        *PlainOut;
  UINT64       DecodedSize64;
  UINT8        Quality;
  STATIC CONST CHAR8  SampleText[] =
    "         \"Attributes\": [\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnActionOnBistFailure\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Action on BIST Failure\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCliptraErrorReporting\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Caliptra Error Reporting\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCmcNotificationType\",\r\n"
    "                \"CurrentValue\": \"CMCI\",\r\n"
    "                \"DefaultValue\": \"CMCI\",\r\n"
    "                \"DisplayName\": \"CMC H/W Error Notification type\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpu64BitMMIOCoverage\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"RMP Coverage for 64Bit MMIO Ranges\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpu64BitMMIORmpS0RBMask\",\r\n"
    "                \"CurrentValue\": 1,"
    "                \"DefaultValue\": 1,"
    "                \"DisplayName\": \"Socket0 RootBridge Mask for 64Bit MMIO RMP Coverage\",\r\n"
    "                \"LowerBound\": 1,"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpu64BitMMIORmpS1RBMask\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket1 RootBridge Mask for 64Bit MMIO RMP Coverage\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuAdaptiveAlloc\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Adaptive Allocation (AA)\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuAmdErmsbRepo\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"AMD_ERMSB Reporting\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuAvx512\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"AVX512\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCcd0DowncoreBitMap\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket 0 Die 0 CCD 0 DownCore Bitmap\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCcd1DowncoreBitMap\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket 0 Die 0 CCD 1 DownCore Bitmap\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCcd2DowncoreBitMap\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket 0 Die 0 CCD 2 DownCore Bitmap\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCcd3DowncoreBitMap\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket 0 Die 0 CCD 3 DownCore Bitmap\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCcd4DowncoreBitMap\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket 0 Die 1 CCD 0 DownCore Bitmap\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCcd5DowncoreBitMap\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket 0 Die 1 CCD 1 DownCore Bitmap\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCcd6DowncoreBitMap\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket 0 Die 1 CCD 2 DownCore Bitmap\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCcd7DowncoreBitMap\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Socket 0 Die 1 CCD 3 DownCore Bitmap\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCoreCc6\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Core CC6\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCpb\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Core Performance Boost\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCstC1Ctrl\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"ACPI _CST C1 Declaration\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCstC2Latency\",\r\n"
    "                \"CurrentValue\": 100,\r\n"
    "                \"DefaultValue\": 100,\r\n"
    "                \"DisplayName\": \"ACPI CST C2 Latency\",\r\n"
    "                \"LowerBound\": 18,"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 1000\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuCstC2LatencyCtrl\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"ACPI CST C2 Latency Control\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuDisFstStrErmsb\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"ERMSB Caching Behavior\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuDowncoreMode\",\r\n"
    "                \"CurrentValue\": \"Enablement Option\",\r\n"
    "                \"DefaultValue\": \"Enablement Option\",\r\n"
    "                \"DisplayName\": \"DownCore Mode\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuERMS\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Enhanced REP MOVSB/STOSB (ERSM)\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuEnReqMinFreq\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Enable Requested CPU min frequency\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuFP512\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"FP512\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuFSRM\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Fast Short REP MOVSB (FSRM)\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuGenWA05\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"RedirectForReturnDis\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuGlobalCstateCtrl\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Global C-state Control\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuLogTransparentErrors\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Log Transparent Errors\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuOcMode\",\r\n"
    "                \"CurrentValue\": \"Normal Operation\",\r\n"
    "                \"DefaultValue\": \"Normal Operation\",\r\n"
    "                \"DisplayName\": \"OC Mode\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuPauseCntSel_1_0\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"PauseCntSel_1_0\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuPfReqThrEn\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Prefetch/Request Throttle\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuPpinCtrl\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"PPIN Opt-in\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuRMSS\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"REP-MOV/STOS Streaming\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuReqMinFreq\",\r\n"
    "                \"CurrentValue\": 1200,\r\n"
    "                \"DefaultValue\": 1200,\r\n"
    "                \"DisplayName\": \"Requested CPU min frequency\",\r\n"
    "                \"LowerBound\": 1200,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 65535\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuScanDumpDbgEn\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Scan Dump Debug Enable\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuSevAsidSpaceLimit\",\r\n"
    "                \"CurrentValue\": 1,"
    "                \"DefaultValue\": 1,"
    "                \"DisplayName\": \"SEV-ES ASID Space Limit\",\r\n"
    "                \"LowerBound\": 1,"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 1007\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuSmee\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"SMEE\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuSmuPspDebugMode\",\r\n"
    "                \"CurrentValue\": \"Enabled\",\r\n"
    "                \"DefaultValue\": \"Enabled\",\r\n"
    "                \"DisplayName\": \"SMU and PSP Debug Mode\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCmnCpuStreamingStoresCtrl\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Streaming Stores Control\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsComboFlag\",\r\n"
    "                \"CurrentValue\": 12,\r\n"
    "                \"DefaultValue\": 254,\r\n"
    "                \"DisplayName\": \"Combo CBS\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuCofP0\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P0 Frequency (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuCofP1\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P1 Frequency (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuCofP2\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P2 Frequency (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuCofP3\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P3 Frequency (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuCofP4\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P4 Frequency (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuCofP5\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P5 Frequency (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuCofP6\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P6 Frequency (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuCofP7\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P7 Frequency (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuLatencyUnderLoad\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Latency Under Load (LUL)\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst0Fid\",\r\n"
    "                \"CurrentValue\": 16,\r\n"
    "                \"DefaultValue\": 16,\r\n"
    "                \"DisplayName\": \"Pstate0 FID\",\r\n"
    "                \"LowerBound\": 16,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 2047\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst0Freq\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Pstate0 Freq (MHz)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst0Vid\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"Pstate0 VID\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 511\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst1Fid\",\r\n"
    "                \"CurrentValue\": 16,\r\n"
    "                \"DefaultValue\": 16,\r\n"
    "                \"DisplayName\": \"Pstate1 FID\",\r\n"
    "                \"LowerBound\": 16,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst1Vid\",\r\n"
    "                \"CurrentValue\": 255,\r\n"
    "                \"DefaultValue\": 255,\r\n"
    "                \"DisplayName\": \"Pstate1 VID\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst2Fid\",\r\n"
    "                \"CurrentValue\": 16,\r\n"
    "                \"DefaultValue\": 16,\r\n"
    "                \"DisplayName\": \"Pstate2 FID\",\r\n"
    "                \"LowerBound\": 16,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst2Vid\",\r\n"
    "                \"CurrentValue\": 255,\r\n"
    "                \"DefaultValue\": 255,\r\n"
    "                \"DisplayName\": \"Pstate2 VID\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst3Fid\",\r\n"
    "                \"CurrentValue\": 16,\r\n"
    "                \"DefaultValue\": 16,\r\n"
    "                \"DisplayName\": \"Pstate3 FID\",\r\n"
    "                \"LowerBound\": 16,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst3Vid\",\r\n"
    "                \"CurrentValue\": 255,\r\n"
    "                \"DefaultValue\": 255,\r\n"
    "                \"DisplayName\": \"Pstate3 VID\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst4Fid\",\r\n"
    "                \"CurrentValue\": 16,\r\n"
    "                \"DefaultValue\": 16,\r\n"
    "                \"DisplayName\": \"Pstate4 FID\",\r\n"
    "                \"LowerBound\": 16,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst4Vid\",\r\n"
    "                \"CurrentValue\": 255,\r\n"
    "                \"DefaultValue\": 255,\r\n"
    "                \"DisplayName\": \"Pstate4 VID\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst5Fid\",\r\n"
    "                \"CurrentValue\": 16,\r\n"
    "                \"DefaultValue\": 16,\r\n"
    "                \"DisplayName\": \"Pstate5 FID\",\r\n"
    "                \"LowerBound\": 16,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst5Vid\",\r\n"
    "                \"CurrentValue\": 255,\r\n"
    "                \"DefaultValue\": 255,\r\n"
    "                \"DisplayName\": \"Pstate5 VID\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst6Fid\",\r\n"
    "                \"CurrentValue\": 16,\r\n"
    "                \"DefaultValue\": 16,\r\n"
    "                \"DisplayName\": \"Pstate6 FID\",\r\n"
    "                \"LowerBound\": 16,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst6Vid\",\r\n"
    "                \"CurrentValue\": 255,\r\n"
    "                \"DefaultValue\": 255,\r\n"
    "                \"DisplayName\": \"Pstate6 VID\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst7Fid\",\r\n"
    "                \"CurrentValue\": 16,\r\n"
    "                \"DefaultValue\": 16,\r\n"
    "                \"DisplayName\": \"Pstate7 FID\",\r\n"
    "                \"LowerBound\": 16,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPst7Vid\",\r\n"
    "                \"CurrentValue\": 255,\r\n"
    "                \"DefaultValue\": 255,\r\n"
    "                \"DisplayName\": \"Pstate7 VID\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 255\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPstCustomP0\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Custom Pstate0\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPstCustomP1\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Custom Pstate1\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPstCustomP2\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Custom Pstate2\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPstCustomP3\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Custom Pstate3\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPstCustomP4\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Custom Pstate4\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPstCustomP5\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Custom Pstate5\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPstCustomP6\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Custom Pstate6\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuPstCustomP7\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"Custom Pstate7\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuSmtCtrl\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"SMT Control\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuSpeculativeStoreModes\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"CPU Speculative Store Modes\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuSvmEnable\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"SVM Enable\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuSvmLock\",\r\n"
    "                \"CurrentValue\": \"Auto\",\r\n"
    "                \"DefaultValue\": \"Auto\",\r\n"
    "                \"DisplayName\": \"SVM Lock\",\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"Type\": \"Enumeration\""
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuVoltageP0\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P0 Voltage (uV)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuVoltageP1\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P1 Voltage (uV)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuVoltageP2\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P2 Voltage (uV)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuVoltageP3\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P3 Voltage (uV)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuVoltageP4\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
    "                \"DisplayName\": \"P4 Voltage (uV)\",\r\n"
    "                \"LowerBound\": 0,\r\n"
    "                \"ReadOnly\": false,\r\n"
    "                \"ResetRequired\": true,\r\n"
    "                \"ScalarIncrement\": 0,\r\n"
    "                \"Type\": \"Integer\",\r\n"
    "                \"UpperBound\": 4294967295\r\n"
    "            },\r\n"
    "            {\r\n"
    "                \"AttributeName\": \"/Bios/Attributes/CbsCpuVoltageP5\",\r\n"
    "                \"CurrentValue\": 0,\r\n"
    "                \"DefaultValue\": 0,\r\n"
      "                \"DisplayName\": \"P5 Voltage (uV)\",\r\n"
      "                \"LowerBound\": 0,\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"ScalarIncrement\": 0,\r\n"
      "                \"Type\": \"Integer\",\r\n"
      "                \"UpperBound\": 4294967295\r\n"
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsCpuVoltageP6\",\r\n"
      "                \"CurrentValue\": 0,\r\n"
      "                \"DefaultValue\": 0,\r\n"
      "                \"DisplayName\": \"P6 Voltage (uV)\",\r\n"
      "                \"LowerBound\": 0,\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"ScalarIncrement\": 0,\r\n"
      "                \"Type\": \"Integer\",\r\n"
      "                \"UpperBound\": 4294967295\r\n"
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsCpuVoltageP7\",\r\n"
      "                \"CurrentValue\": 0,\r\n"
      "                \"DefaultValue\": 0,\r\n"
      "                \"DisplayName\": \"P7 Voltage (uV)\",\r\n"
      "                \"LowerBound\": 0,\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"ScalarIncrement\": 0,\r\n"
      "                \"Type\": \"Integer\",\r\n"
      "                \"UpperBound\": 4294967295\r\n"
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsDbgCpuLApicMode\",\r\n"
      "                \"CurrentValue\": \"Auto\",\r\n"
      "                \"DefaultValue\": \"Auto\",\r\n"
      "                \"DisplayName\": \"Local APIC Mode\",\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"Type\": \"Enumeration\""
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsDbgCpuRmpSegmentSize\",\r\n"
      "                \"CurrentValue\": \"Auto\",\r\n"
      "                \"DefaultValue\": \"Auto\",\r\n"
      "                \"DisplayName\": \"RMP Segment Size\",\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"Type\": \"Enumeration\""
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsDbgCpuSegmentedRMP\",\r\n"
      "                \"CurrentValue\": \"Auto\",\r\n"
      "                \"DefaultValue\": \"Auto\",\r\n"
      "                \"DisplayName\": \"Segmented RMP Table\",\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"Type\": \"Enumeration\""
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsDbgCpuSnpMemCover\",\r\n"
      "                \"CurrentValue\": \"Auto\",\r\n"
      "                \"DefaultValue\": \"Auto\",\r\n"
      "                \"DisplayName\": \"SNP Memory (RMP Table) Coverage\",\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"Type\": \"Enumeration\""
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsDbgCpuSnpMemSizeCover\",\r\n"
      "                \"CurrentValue\": 16,\r\n"
      "                \"DefaultValue\": 16,\r\n"
      "                \"DisplayName\": \"Amount of Memory to Cover\",\r\n"
      "                \"LowerBound\": 16,\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"ScalarIncrement\": 0,\r\n"
      "                \"Type\": \"Integer\",\r\n"
      "                \"UpperBound\": 1048576\r\n"
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsDbgCpuSplitRMP\",\r\n"
      "                \"CurrentValue\": \"Auto\",\r\n"
      "                \"DefaultValue\": \"Auto\",\r\n"
      "                \"DisplayName\": \"Split RMP Table\",\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"Type\": \"Enumeration\""
      "            },\r\n"
      "            {\r\n"
      "                \"AttributeName\": \"/Bios/Attributes/CbsPspSevCtrl\",\r\n"
      "                \"CurrentValue\": \"Enable\",\r\n"
      "                \"DefaultValue\": \"Enable\",\r\n"
      "                \"DisplayName\": \"SEV Control\",\r\n"
      "                \"ReadOnly\": false,\r\n"
      "                \"ResetRequired\": true,\r\n"
      "                \"Type\": \"Enumeration\""
      "            }"
      "        ]"
      "    },\r\n";


  (VOID)ImageHandle;
  (VOID)SystemTable;

  Quality = 3;
  Print (L"BrotliShellSample: round-trip self-test\r\n");

  PlainLen = AsciiStrLen (SampleText);
  ASSERT (PlainLen >= SAMPLE_BROTLI_SCRATCH_MAX);

  Status = BrotliUefiCompressGetInfo (
             PlainLen,
             Quality,
             0,
             &CompMax64,
             &EncScratch64
             );
  if (EFI_ERROR (Status)) {
    Print (L"BrotliUefiCompressGetInfo failed: %r\r\n", Status);
    return Status;
  }

  if (CompMax64 > (UINT64)MAX_UINTN) {
    Print (L"Compressed size bound too large for UINTN.\r\n");
    return EFI_UNSUPPORTED;
  }

  if (EncScratch64 > (UINT64)MAX_UINTN) {
    Print (L"Encoder scratch too large for UINTN.\r\n");
    return EFI_UNSUPPORTED;
  }

  CompCap     = (UINTN)CompMax64;
  EncScratch  = (UINTN)EncScratch64;
  EncScratchBuf = AllocatePool (EncScratch);
  Compressed      = AllocatePool (CompCap);
  if ((EncScratchBuf == NULL) || (Compressed == NULL)) {
    Print (L"AllocatePool (compress) failed.\r\n");
    if (EncScratchBuf != NULL) {
      FreePool (EncScratchBuf);
    }

    if (Compressed != NULL) {
      FreePool (Compressed);
    }

    return EFI_OUT_OF_RESOURCES;
  }

  CompLen = CompCap;
  Status  = BrotliUefiCompress (
              SampleText,
              PlainLen,
              Compressed,
              &CompLen,
              EncScratchBuf,
              EncScratch,
              Quality,
              0
              );
  FreePool (EncScratchBuf);
  if (EFI_ERROR (Status)) {
    Print (L"BrotliUefiCompress failed: %r\r\n", Status);
    FreePool (Compressed);
    return Status;
  }

  Print (L"  Original %Lu bytes, compressed %Lu bytes\r\n", (UINT64)PlainLen, (UINT64)CompLen);

  DecodedSize64 = SampleBrGetDecodedSizeOfBuf (
                    Compressed,
                    SAMPLE_BROTLI_DECODE_MAX - SAMPLE_BROTLI_INFO_SIZE,
                    SAMPLE_BROTLI_DECODE_MAX
                    );
  DecScratch64 = SampleBrGetDecodedSizeOfBuf (
                   Compressed,
                   SAMPLE_BROTLI_SCRATCH_MAX - SAMPLE_BROTLI_INFO_SIZE,
                   SAMPLE_BROTLI_SCRATCH_MAX
                   );
  if ((DecodedSize64 > (UINT64)MAX_UINTN) || (DecScratch64 > (UINT64)MAX_UINTN)) {
    Print (L"Decoded or scratch size from header too large.\r\n");
    FreePool (Compressed);
    return EFI_UNSUPPORTED;
  }

  if (DecodedSize64 != (UINT64)PlainLen) {
    Print (
      L"Header uncompressed size %Lu does not match source %Lu.\r\n",
      DecodedSize64,
      (UINT64)PlainLen
      );
    FreePool (Compressed);
    return EFI_ABORTED;
  }

  DecScratch = (UINTN)DecScratch64;
  PlainOut   = AllocatePool ((UINTN)DecodedSize64 + 1);
  DecScratchBuf = AllocatePool (DecScratch);
  if ((PlainOut == NULL) || (DecScratchBuf == NULL)) {
    Print (L"AllocatePool (decompress) failed.\r\n");
    if (PlainOut != NULL) {
      FreePool (PlainOut);
    }

    if (DecScratchBuf != NULL) {
      FreePool (DecScratchBuf);
    }

    FreePool (Compressed);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = SampleBrotliUefiDecompress (
             Compressed,
             CompLen,
             PlainOut,
             DecScratchBuf
             );
  FreePool (DecScratchBuf);
  FreePool (Compressed);
  if (EFI_ERROR (Status)) {
    Print (L"SampleBrotliUefiDecompress failed: %r\r\n", Status);
    FreePool (PlainOut);
    return Status;
  }

  PlainOut[(UINTN)DecodedSize64] = '\0';
  if (AsciiStrnCmp ((CHAR8 *)PlainOut, SampleText, PlainLen) != 0) {
    Print (L"Round-trip data mismatch.\r\n");
    FreePool (PlainOut);
    return EFI_ABORTED;
  }

  Print (L"  Round-trip OK. Recovered text (ASCII):\r\n%a", (CHAR8 *)PlainOut);
  FreePool (PlainOut);

  Print (L"BrotliShellSample: success\r\n");
  return EFI_SUCCESS;
}
