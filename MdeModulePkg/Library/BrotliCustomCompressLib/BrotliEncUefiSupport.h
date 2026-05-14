/** @file
  BROTLI encoder UEFI / edk2 compatibility shim for third-party sources.

  Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <stdint.h>

#define memcpy   CopyMem
#define memmove  CopyMem
#define memset(dest, ch, count)  SetMem(dest,(UINTN)(count),(UINT8)(ch))
#define malloc   BrDummyMallocEnc
#define free     BrDummyFreeEnc
#define exit(x)  CpuDeadLoop ()

#define EXIT_FAILURE  1

VOID *
BrDummyMallocEnc (
  IN size_t  Size
  );

VOID
BrDummyFreeEnc (
  IN VOID  *Ptr
  );
