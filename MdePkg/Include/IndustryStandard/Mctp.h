/** @file

  The definitions of Management Component Transport Protocol (MCTP)
  Base Specification.

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  Management Component Transport Protocol (MCTP) Base Specification
  Version 1.3.1
**/

#ifndef MCTP_H_
#define MCTP_H_

///
/// The 32-bit Header of MCTP packet.
///
typedef union {
  struct {
    UINT8  Reserved:4;               ///< Reserved for future definitions.
    UINT8  HeaderVersion:4;          ///< The version of header.
    UINT8  DestinationEndpointId:8;  ///< Destination endpoint Id (EID).
    UINT8  SourceEndpointIdId:8;     ///< Source endpoint Id (EID)
    UINT8  StartOfMessage:1;         ///< Indicates the first packet of message.
    UINT8  EndOfMessage:1;           ///< Indicates the last packet of message.
    UINT8  PacketSequence:2;         ///< Sequence number increments Modulo 4 on
                                     ///< each packet.
    UINT8  TagOwner:1;               ///< Tag owner identifies the message was
                                     ///< originated by the source EID or
                                     ///< destination EID.
    UINT8  MessageTag:3;             ///< Check the MCTP Base specification for the
                                     ///< usages.
  } Bits;
  UINT32     Header;
} MCTP_HEADER;

///
/// MCTP Control Commands
///
#define   MCTP_CONTROL_RESERVED                            0x00
#define   MCTP_CONTROL_SET_ENDPOINT_ID                     0x01
#define   MCTP_CONTROL_GET_ENDPOINT_ID                     0x02
#define   MCTP_CONTROL_GET_ENDPOINT_UUID                   0x03
#define   MCTP_CONTROL_GET_MCTP_VERSION_SUPPORT            0x04
#define   MCTP_CONTROL_GET_MESSAGE_TYPE_SUPPORT            0x05
#define   MCTP_CONTROL_GET_VENDOR_DEFINED_MESSAGE_SUPPORT  0x06
#define   MCTP_CONTROL_RESOLVE_ENDPOINT_ID                 0x07
#define   MCTP_CONTROL_ALLOCATE_ENDPOINT_IDS               0x08
#define   MCTP_CONTROL_ROUTING_INFORMATION_UPDATE          0x09
#define   MCTP_CONTROL_GET_ROUTINE_TABLE_ENTRIES           0x0A
#define   MCTP_CONTROL_PREPARE_FOR_ENDPOINT_DISCOVERY      0x0B
#define   MCTP_CONTROL_ENDPOINT_DISCOVERY                  0x0C
#define   MCTP_CONTROL_DISCOVERY_NOTIFY                    0x0D
#define   MCTP_CONTROL_GET_NETWORK_ID                      0x0E
#define   MCTP_CONTROL_QUERY_HOP                           0x0F
#define   MCTP_CONTROL_RESOLVE_UUID                        0x10
#define   MCTP_CONTROL_QUERY_RATE_LIMIT                    0x11
#define   MCTP_CONTROL_REQUEST_TX_RATE_LIMIT               0x12
#define   MCTP_CONTROL_UPDATE_RATE_LIMIT                   0x13
#define   MCTP_CONTROL_QUERY_SUPPORTED_INTERFACES          0x14
#define   MCTP_CONTROL_TRANSPORT_SPECIFIC_START            0xF0
#define   MCTP_CONTROL_TRANSPORT_SPECIFIC_END              0xFF

///
/// MCTP Control Message Completion Codes
///
#define   MCTP_CONTROL_COMPLETION_CODES_SUCCESS                 0x00
#define   MCTP_CONTROL_COMPLETION_CODES_ERROR                   0x01
#define   MCTP_CONTROL_COMPLETION_CODES_ERROR_INVALID_DATA      0x02
#define   MCTP_CONTROL_COMPLETION_CODES_ERROR_INVALID_LENGTH    0x03
#define   MCTP_CONTROL_COMPLETION_CODES_ERROR_NOT_READY         0x04
#define   MCTP_CONTROL_COMPLETION_CODES_ERROR_UNSUPPORTED_CMD   0x05
#define   MCTP_CONTROL_COMPLETION_CODES_COMMAND_SPECIFIC_START  0x80
#define   MCTP_CONTROL_COMPLETION_CODES_COMMAND_SPECIFIC_END    0xFF

///
/// MCTP Control Message Types
///
#define   MCTP_MESSAGE_TYPE_CONTROL              0x00
#define   MCTP_MESSAGE_TYPE_PLDM                 0x01
#define   MCTP_MESSAGE_TYPE_NCSI                 0x02
#define   MCTP_MESSAGE_TYPE_ETHERNET             0x03
#define   MCTP_MESSAGE_TYPE_NVME                 0x04
#define   MCTP_MESSAGE_TYPE_SPDM                 0x05
#define   MCTP_MESSAGE_TYPE_SECURE_MESSAGE       0x06
#define   MCTP_MESSAGE_TYPE_CXL_FM_API           0x07
#define   MCTP_MESSAGE_TYPE_CXL_CCI              0x08
#define   MCTP_MESSAGE_TYPE_VENDOR_DEFINED_PCI   0x7E
#define   MCTP_MESSAGE_TYPE_VENDOR_DEFINED_IANA  0x7F

#define   MCTP_ENDPOINT_ID_NULL            0
#define   MCTP_ENDPOINT_ID_RESERVED_START  1
#define   MCTP_ENDPOINT_ID_RESERVED_END    7
#define   MCTP_ENDPOINT_ID_BROADCAST       0xff
///
/// MCTP Control Message Format
///
typedef struct {
  struct {
    UINT8  IntegrityCheck:1;  ///< Message integrity check.
    UINT8  MessageType:7;     ///< Message type.
    UINT8  RequestBit:1;      ///< Request bit.
    UINT8  DatagramBit:1;     ///< Datagram bit.
    UINT8  Reserved:1;        ///< Reserved bit.
    UINT8  InstanceId:5;      ///< Instance ID.
    UINT8  CommandCode:8;     ///< Command code.
    UINT8  CompletionCode:8;  ///< Completion code.
  } Bits;
  UINT32     BodyHeader;
} MCTP_CONTROL_MESSAGE;
#endif
