/** @file
  IPMI KCS Register Definitions

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef IPMI_KCS_H_
#define IPMI_KCS_H_

#define IPMI_KCS_STATUS_REGISTER_OFFSET     1
#define IPMI_KCS_COMMAND_REGISTER_OFFSET    1
#define IPMI_KCS_DATA_OUT_REGISTER_OFFSET   0
#define IPMI_KCS_DATA_IN_REGISTER_OFFSET    0

///
/// IPMI KCS Interface Status Bits
///
#define IPMI_KCS_OBF              BIT0
#define IPMI_KCS_IBF              BIT1
#define IPMI_KCS_SMS_ATN          BIT2
#define IPMI_KCS_COMMAND_DATA     BIT3
#define IPMI_KCS_OEM1             BIT4
#define IPMI_KCS_OEM2             BIT5
#define IPMI_KCS_S0               BIT6
#define IPMI_KCS_S1               BIT7

///
/// IPMI KCS Interface Control Codes
///
#define IPMI_KCS_CONTROL_CODE_GET_STATUS_ABORT  0x60
#define IPMI_KCS_CONTROL_CODE_WRITE_START       0x61
#define IPMI_KCS_CONTROL_CODE_WRITE_END         0x62
#define IPMI_KCS_CONTROL_CODE_READ              0x68

///
/// Status Codes
///
#define IPMI_KCS_STATUS_NO_ERROR                0x00
#define IPMI_KCS_STATUS_ABORT                   0x01
#define IPMI_KCS_STATUS_ILLEGAL                 0x02
#define IPMI_KCS_STATUS_LENGTH_ERROR            0x06
#define IPMI_KCS_STATUS_UNSPECIFIED             0xFF

///
/// KCS Interface State Bit
///
typedef enum {
  IPMI_KCS_IDLE_STATE = 0,
  IPMI_KCS_READ_STATE,
  IPMI_KCS_WRITE_STATE,
  IPMI_KCS_ERROR_STATE
} IPMI_KCS_STATE;

///
/// IPMI KCS Interface Request Format
///
typedef struct {
  UINT8  NetFunc;
  UINT8  Command;
  UINT8  Data[0];
} IPMI_KCS_RESQUEST_HEADER;

///
/// IPMI KCS Interface Response Format
///
typedef struct {
  UINT8  NetFunc;
  UINT8  Command;
  UINT8  CompletionCode;
} IPMI_KCS_RESPONSE_HEADER;
#endif
