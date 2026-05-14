/** @file
  MSVC linker float stub for UEFI applications (VS2026 / MSFT family).

  This file is listed with | MSFT in the INF so it is only compiled by MSVC.
  AutoGen.obj references _fltused when floating-point appears anywhere in the
  link. BrotliShellSample.inf uses /GL- so this TU is not /GL-compiled; that
  avoids LNK1237 (LTCG cross-module _fltused) while still satisfying the link.

  Copyright (c) 2026, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

int  _fltused = 1;
