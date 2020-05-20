/** @file
  The internal definitions of EFI Source Coding (compress/decompress) Protocol

  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_SOURCE_CODING_INTERNAL_H_
#define EFI_SOURCE_CODING_INTERNAL_H_

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Protocol/SourceCoding.h>

/** Internal structure to maintain the information of source
    coding instance.
  *
**/
typedef struct _SOURCE_CODING_INSTANCE {
  LIST_ENTRY                    NextSourceCodingInstance;  ///< Next source coding instance
  EFI_SOURCE_CODING_IDENTIFIER  MethodIdentifer;           ///< Name of this source coding algorithm.
  EFI_SOURCE_CODING_COMPRESS    CompressMethod;            ///< Compress method provided by this insatnce.
  EFI_SOURCE_CODING_DECOMPRESS  DecompressMethod;          ///< Decompress method provided by this insatnce.
} SOURCE_CODING_INSTANCE;
#endif
