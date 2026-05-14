/** @file
  Implements functions declared in BrotliEncUefiSupport.h.

  Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <BrotliEncUefiSupport.h>

/**
  Dummy malloc function for compiler when third-party code references malloc.

**/
VOID *
BrDummyMallocEnc (
  IN size_t  Size
  )
{
  ASSERT (FALSE);
  return NULL;
}

/**
  Dummy free function for compiler when third-party code references free.

**/
VOID
BrDummyFreeEnc (
  IN VOID  *Ptr
  )
{
  ASSERT (FALSE);
}
