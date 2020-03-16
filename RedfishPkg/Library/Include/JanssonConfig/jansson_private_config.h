/** @file
  Jansson private configurations for UEFI support.

 Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
 (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef JANSSON_PRIVATE_CONFIG_H_
#define JANSSON_PRIVATE_CONFIG_H_

#define HAVE_UNISTD_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1

#define HAVE_INT32_T 1
#ifndef HAVE_INT32_T
  #define int32_t INT32
#endif

#define HAVE_UINT32_T 1
#ifndef HAVE_UINT32_T
  #define uint32_t UINT32
#endif

#define HAVE_UINT16_T 1
#ifndef HAVE_UINT16_T
  #define uint16_t UINT16
#endif

#define HAVE_UINT8_T 1
#ifndef HAVE_UINT8_T
  #define uint8_t UINT8
#endif

#define HAVE_SSIZE_T 1

#ifndef HAVE_SSIZE_T
  #define ssize_t INTN
#endif

#define INITIAL_HASHTABLE_ORDER 3

#endif
