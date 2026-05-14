/** @file
  Fixed-width types for third-party Brotli in the UEFI build.

  This file must not include BrotliEncUefiSupport.h (that header includes this
  file), or brotli/types.h cannot pull <stdint.h> before the shim without a
  circular include. GCC would then see duplicate int64_t definitions.

  Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Library/BaseLib.h>

typedef INT8    int8_t;
typedef INT16   int16_t;
typedef INT32   int32_t;
typedef INT64   int64_t;
typedef UINT8   uint8_t;
typedef UINT16  uint16_t;
typedef UINT32  uint32_t;
typedef UINT64  uint64_t;
typedef UINTN   size_t;
