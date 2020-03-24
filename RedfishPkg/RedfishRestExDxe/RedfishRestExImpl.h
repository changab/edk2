/** @file
  RedfishRestExDxe support functions implementation.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_REDFISH_RESTEX_IMPL_H_
#define EFI_REDFISH_RESTEX_IMPL_H_

#include <Uefi.h>

#include <IndustryStandard/Http11.h>

//
// Libraries classes
//
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NetLib.h>
#include <Library/DebugLib.h>
#include <Library/DpcLib.h>
#include <Library/PrintLib.h>
#include <Library/HttpLib.h>
#include <Library/DevicePathLib.h>

//
// UEFI Driver Model Protocols
//
#include <Protocol/DriverBinding.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/ComponentName.h>
#include <Protocol/RestEx.h>

//
// Driver Version
//
#define REDFISH_RESTEX_DRIVER_VERSION  0x0100

#define RESTEX_STATE_UNCONFIGED     0
#define RESTEX_STATE_CONFIGED       1

#define CHUNK_SIZE_STRING_LEN       50
#define CHUNKED_TRANSFER_CODING_DATA_LENGTH 256

typedef enum  {
  RestExSendChunkNone = 0,
  RestExSendChunkHeaderZeroContent,
  RestExSendChunkContent,
  RestExSendChunkEndChunk,
  RestExSendChunkFinish
} RESTEX_SEND_CHUNK_PROCESS;

typedef enum  {
  RestExSendNonChunkNone = 0,
  RestExSendNonChunkHeaderZeroContent,
  RestExSendNonChunkContent,
  RestExSendNonChunkFinish
} RESTEX_SEND_NON_CHUNK_PROCESS;

//
// Chunk links for chunked transfer coding.
//
typedef struct {
  LIST_ENTRY  NextChunk;
  UINTN       Length;
  CHAR8       *Data;
} REST_EX_HTTP_CHUNKS;

//
// A wrapper structure to hold the received HTTP response data.
//
typedef struct {
  EFI_HTTP_RESPONSE_DATA      Response;
  UINTN                       HeaderCount;
  EFI_HTTP_HEADER             *Headers;
  UINTN                       BodyLength;
  CHAR8                       *Body;
  EFI_STATUS                  Status;
} HTTP_IO_RESPONSE_DATA;

//
// HTTP_IO configuration data for IPv4
//
typedef struct {
  EFI_HTTP_VERSION          HttpVersion;
  UINT32                    RequestTimeOut;  // In milliseconds.
  UINT32                    ResponseTimeOut; // In milliseconds.
  BOOLEAN                   UseDefaultAddress;
  EFI_IPv4_ADDRESS          LocalIp;
  EFI_IPv4_ADDRESS          SubnetMask;
  UINT16                    LocalPort;
} HTTP4_IO_CONFIG_DATA;

//
// HTTP_IO configuration data for IPv6
//
typedef struct {
  EFI_HTTP_VERSION          HttpVersion;
  UINT32                    RequestTimeOut;  // In milliseconds.
  EFI_IPv6_ADDRESS          LocalIp;
  UINT16                    LocalPort;
} HTTP6_IO_CONFIG_DATA;

//
// HTTP_IO configuration
//
typedef union {
  HTTP4_IO_CONFIG_DATA       Config4;
  HTTP6_IO_CONFIG_DATA       Config6;
} HTTP_IO_CONFIG_DATA;

//
// HTTP_IO wrapper of the EFI HTTP service.
//
typedef struct {
  UINT8                     IpVersion;
  EFI_HANDLE                Image;
  EFI_HANDLE                Controller;
  EFI_HANDLE                Handle;

  EFI_HTTP_PROTOCOL         *Http;

  EFI_HTTP_TOKEN            ReqToken;
  EFI_HTTP_MESSAGE          ReqMessage;
  EFI_HTTP_TOKEN            RspToken;
  EFI_HTTP_MESSAGE          RspMessage;

  BOOLEAN                   IsTxDone;
  BOOLEAN                   IsRxDone;

  EFI_EVENT                 TimeoutEvent;
  UINT32                    Timeout;
} HTTP_IO;

#include "RedfishRestExDriver.h"

//
// Protocol instances
//
extern EFI_COMPONENT_NAME_PROTOCOL   gRedfishRestExComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gRedfishRestExComponentName2;
extern EFI_UNICODE_STRING_TABLE      *gRedfishRestExControllerNameTable;

extern EFI_DRIVER_BINDING_PROTOCOL   gRedfishRestExDriverBinding;
extern EFI_SERVICE_BINDING_PROTOCOL  mRedfishRestExServiceBinding;
extern EFI_REST_EX_PROTOCOL          mRedfishRestExProtocol;

/**
  Notify the callback function when an event is triggered.

  @param[in]  Context         The opaque parameter to the function.

**/
VOID
EFIAPI
HttpIoNotifyDpc (
  IN VOID                *Context
  );

/**
  Request HttpIoNotifyDpc as a DPC at TPL_CALLBACK.

  @param[in]  Event                 The event signaled.
  @param[in]  Context               The opaque parameter to the function.

**/
VOID
EFIAPI
HttpIoNotify (
  IN EFI_EVENT              Event,
  IN VOID                   *Context
  );

/**
  Destroy the HTTP_IO and release the resources.

  @param[in]  HttpIo          The HTTP_IO which wraps the HTTP service to be destroyed.

**/
VOID
HttpIoDestroyIo (
  IN HTTP_IO                *HttpIo
  );

/**
  Create a HTTP_IO to access the HTTP service. It will create and configure
  a HTTP child handle.

  @param[in]  Image          The handle of the driver image.
  @param[in]  Controller     The handle of the controller.
  @param[in]  IpVersion      IP_VERSION_4 or IP_VERSION_6.
  @param[in]  ConfigData     The HTTP_IO configuration data.
  @param[out] HttpIo         The HTTP_IO.

  @retval EFI_SUCCESS            The HTTP_IO is created and configured.
  @retval EFI_INVALID_PARAMETER  One or more parameters are invalid.
  @retval EFI_UNSUPPORTED        One or more of the control options are not
                                 supported in the implementation.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.
  @retval Others                 Failed to create the HTTP_IO or configure it.

**/
EFI_STATUS
HttpIoCreateIo (
  IN EFI_HANDLE             Image,
  IN EFI_HANDLE             Controller,
  IN UINT8                  IpVersion,
  IN HTTP_IO_CONFIG_DATA    *ConfigData,
  OUT HTTP_IO               *HttpIo
  );

/**
  Synchronously send a HTTP REQUEST message to the server.

  @param[in]   HttpIo           The HttpIo wrapping the HTTP service.
  @param[in]   Request          A pointer to storage such data as URL and HTTP method.
  @param[in]   HeaderCount      Number of HTTP header structures in Headers list.
  @param[in]   Headers          Array containing list of HTTP headers.
  @param[in]   BodyLength       Length in bytes of the HTTP body.
  @param[in]   Body             Body associated with the HTTP request.

  @retval EFI_SUCCESS            The HTTP request is trasmitted.
  @retval EFI_INVALID_PARAMETER  One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.
  @retval EFI_DEVICE_ERROR       An unexpected network or system error occurred.
  @retval Others                 Other errors as indicated.

**/
EFI_STATUS
HttpIoSendRequest (
  IN  HTTP_IO                *HttpIo,
  IN  EFI_HTTP_REQUEST_DATA  *Request,
  IN  UINTN                  HeaderCount,
  IN  EFI_HTTP_HEADER        *Headers,
  IN  UINTN                  BodyLength,
  IN  VOID                   *Body
  );

/**
  Synchronously receive a HTTP RESPONSE message from the server.

  @param[in]   HttpIo           The HttpIo wrapping the HTTP service.
  @param[in]   RecvMsgHeader    TRUE to receive a new HTTP response (from message header).
                                FALSE to continue receive the previous response message.
  @param[out]  ResponseData     Point to a wrapper of the received response data.

  @retval EFI_SUCCESS            The HTTP response is received.
  @retval EFI_INVALID_PARAMETER  One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.
  @retval EFI_DEVICE_ERROR       An unexpected network or system error occurred.
  @retval Others                 Other errors as indicated.

**/
EFI_STATUS
HttpIoRecvResponse (
  IN      HTTP_IO                  *HttpIo,
  IN      BOOLEAN                  RecvMsgHeader,
  OUT     HTTP_IO_RESPONSE_DATA    *ResponseData
  );

/**
  Get the value of the content length if there is a "Content-Length" header.

  @param[in]    HeaderCount        Number of HTTP header structures in Headers.
  @param[in]    Headers            Array containing list of HTTP headers.
  @param[out]   ContentLength      Pointer to save the value of the content length.

  @retval EFI_SUCCESS              Successfully get the content length.
  @retval EFI_NOT_FOUND            No "Content-Length" header in the Headers.

**/
EFI_STATUS
HttpIoGetContentLength (
  IN     UINTN                HeaderCount,
  IN     EFI_HTTP_HEADER      *Headers,
  OUT    UINTN                *ContentLength
  );

/**
  This function check

  @param[in]  Instance          Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                REST service.
  @param[in]  HttpReceiveEfiStatus  This is the status return from HttpIoRecvResponse

  @retval EFI_SUCCESS           The payload receive from Redfish service in sucessfully.
  @retval EFI_NOT_READY         May need to resend the HTTP request.
  @retval EFI_DEVICE_ERROR      Something wrong and can't be resolved.
  @retval Others                Other errors as indicated.

**/
EFI_STATUS
RedfishCheckHttpReceiveStatus (
  IN RESTEX_INSTANCE *Instance,
  IN EFI_STATUS HttpIoReceiveStatus
  );

/**
  Synchronously receive a HTTP RESPONSE message from the server.

  @param[in]   HttpIo           The HttpIo wrapping the HTTP service.
  @param[in]   HeaderCount      Number of headers in Headers.
  @param[in]   Headers          Array containing list of HTTP headers.
  @param[out]  ChunkListHead    A pointer to receivce list head of chunked data.
                                Caller has to release memory of ChunkListHead
                                and all list entries.
  @param[out]  ContentLength    Total content length

  @retval EFI_SUCCESS            The HTTP chunked transfer is received.
  @retval EFI_NOT_FOUND          No chunked transfer coding header found.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.
  @retval EFI_INVALID_PARAMETER  Improper parameters.
  @retval Others                 Other errors as indicated.

**/
EFI_STATUS
HttpIoGetChunkedTransferContent (
  IN     HTTP_IO              *HttpIo,
  IN     UINTN                HeaderCount,
  IN     EFI_HTTP_HEADER      *Headers,
  OUT    LIST_ENTRY           **ChunkListHead,
  OUT    UINTN                *ContentLength
  );

/**
  Send HTTP request in chunks.

  @param[in]   HttpIo             The HttpIo wrapping the HTTP service.
  @param[in]   SendChunkProcess   Pointer to current chunk process status.
  @param[out]  RequestMessage     Request to send.

  @retval EFI_SUCCESS             Successfully to send chunk data according to SendChunkProcess.
  @retval Other                   Other errors.

**/
EFI_STATUS
HttpIoSendChunkedTransfer (
  IN  HTTP_IO                   *HttpIo,
  IN  RESTEX_SEND_CHUNK_PROCESS *SendChunkProcess,
  IN  EFI_HTTP_MESSAGE          *RequestMessage
);

/**
  This function send the HTTP request without body to see
  if the write to URL is permitted by Redfish service. This function
  checks if the HTTP request has Content-length in HTTP header. If yes,
  set HTTP body to NULL and then send to service. Check the HTTP status
  for the firther actions.

  @param[in]  This                    Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                      REST service.
  @param[in]  RequestMessage          Pointer to the HTTP request data for this resource
  @param[in]  PreservedRequestHeaders The pointer to save the request headers
  @param[in]  ItsWrite                This is write method to URL.

  @retval EFI_INVALID_PARAMETER  Improper given parameters.
  @retval EFI_SUCCESS            This HTTP request is free to send to Redfish service.
  @retval EFI_OUT_OF_RESOURCES   NOt enough memory to process.
  @retval EFI_ACCESS_DENIED      Not allowed to write to this URL.

  @retval Others                 Other errors as indicated.

**/
EFI_STATUS
RedfishHttpAddExpectation (
  IN EFI_REST_EX_PROTOCOL   *This,
  IN EFI_HTTP_MESSAGE       *RequestMessage,
  IN EFI_HTTP_HEADER        **PreservedRequestHeaders,
  IN BOOLEAN                *ItsWrite
  );

/**
  Provides a simple HTTP-like interface to send and receive resources from a REST service.

  The SendReceive() function sends an HTTP request to this REST service, and returns a
  response when the data is retrieved from the service. RequestMessage contains the HTTP
  request to the REST resource identified by RequestMessage.Request.Url. The
  ResponseMessage is the returned HTTP response for that request, including any HTTP
  status.

  @param[in]  This                Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                  REST service.
  @param[in]  RequestMessage      Pointer to the HTTP request data for this resource
  @param[out] ResponseMessage     Pointer to the HTTP response data obtained for this requested.

  @retval EFI_SUCCESS             operation succeeded.
  @retval EFI_INVALID_PARAMETER   This, RequestMessage, or ResponseMessage are NULL.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.

**/
EFI_STATUS
EFIAPI
RedfishRestExSendReceive (
  IN      EFI_REST_EX_PROTOCOL   *This,
  IN      EFI_HTTP_MESSAGE       *RequestMessage,
  OUT     EFI_HTTP_MESSAGE       *ResponseMessage
  );

/**
  Obtain the current time from this REST service instance.

  The GetServiceTime() function is an optional interface to obtain the current time from
  this REST service instance. If this REST service does not support to retrieve the time,
  this function returns EFI_UNSUPPORTED. This function must returns EFI_UNSUPPORTED if
  EFI_REST_EX_SERVICE_TYPE returned in EFI_REST_EX_SERVICE_INFO from GetService() is
  EFI_REST_EX_SERVICE_UNSPECIFIC.

  @param[in]  This                Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                  REST service.
  @param[out] Time                A pointer to storage to receive a snapshot of the current time of
                                  the REST service.

  @retval EFI_SUCCESS             operation succeeded.
  @retval EFI_INVALID_PARAMETER   This or Time are NULL.
  @retval EFI_UNSUPPORTED         The RESTful service does not support returning the time.
  @retval EFI_DEVICE_ERROR        An unexpected system or network error occurred.
  @retval EFI_NOT_READY           The configuration of this instance is not set yet. Configure() must
                                  be executed and returns successfully prior to invoke this function.

**/
EFI_STATUS
EFIAPI
RedfishRestExGetServiceTime (
  IN      EFI_REST_EX_PROTOCOL   *This,
  OUT     EFI_TIME               *Time
  );

/**
  This function returns the information of REST service provided by this EFI REST EX driver instance.

  The information such as the type of REST service and the access mode of REST EX driver instance
  (In-band or Out-of-band) are described in EFI_REST_EX_SERVICE_INFO structure. For the vendor-specific
  REST service, vendor-specific REST service information is returned in VendorSpecifcData.
  REST EX driver designer is well know what REST service this REST EX driver instance intends to
  communicate with. The designer also well know this driver instance is used to talk to BMC through
  specific platform mechanism or talk to REST server through UEFI HTTP protocol. REST EX driver is
  responsible to fill up the correct information in EFI_REST_EX_SERVICE_INFO. EFI_REST_EX_SERVICE_INFO
  is referred by EFI REST clients to pickup the proper EFI REST EX driver instance to get and set resource.
  GetService() is a basic and mandatory function which must be able to use even Configure() is not invoked
  in previously.

  @param[in]  This                Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                  REST service.
  @param[out] RestExServiceInfo   Pointer to receive a pointer to EFI_REST_EX_SERVICE_INFO structure. The
                                  format of EFI_REST_EX_SERVICE_INFO is version controlled for the future
                                  extension. The version of EFI_REST_EX_SERVICE_INFO structure is returned
                                  in the header within this structure. EFI REST client refers to the correct
                                  format of structure according to the version number. The pointer to
                                  EFI_REST_EX_SERVICE_INFO is a memory block allocated by EFI REST EX driver
                                  instance. That is caller's responsibility to free this memory when this
                                  structure is no longer needed. Refer to Related Definitions below for the
                                  definitions of EFI_REST_EX_SERVICE_INFO structure.

  @retval EFI_SUCCESS             EFI_REST_EX_SERVICE_INFO is returned in RestExServiceInfo. This function
                                  is not supported in this REST EX Protocol driver instance.
  @retval EFI_UNSUPPORTED         This function is not supported in this REST EX Protocol driver instance.

**/
EFI_STATUS
EFIAPI
RedfishRestExGetService (
  IN   EFI_REST_EX_PROTOCOL      *This,
  OUT  EFI_REST_EX_SERVICE_INFO  **RestExServiceInfo
  );

/**
  This function returns operational configuration of current EFI REST EX child instance.

  This function returns the current configuration of EFI REST EX child instance. The format of
  operational configuration depends on the implementation of EFI REST EX driver instance. For
  example, HTTP-aware EFI REST EX driver instance uses EFI HTTP protocol as the undying protocol
  to communicate with REST service. In this case, the type of configuration is
  EFI_REST_EX_CONFIG_TYPE_HTTP returned from GetService(). EFI_HTTP_CONFIG_DATA is used as EFI REST
  EX configuration format and returned to EFI REST client. User has to type cast RestExConfigData
  to EFI_HTTP_CONFIG_DATA. For those non HTTP-aware REST EX driver instances, the type of configuration
  is EFI_REST_EX_CONFIG_TYPE_UNSPECIFIC returned from GetService(). In this case, the format of
  returning data could be non industrial. Instead, the format of configuration data is system/platform
  specific definition such as BMC mechanism used in EFI REST EX driver instance. EFI REST client and
  EFI REST EX driver instance have to refer to the specific system /platform spec which is out of UEFI scope.

  @param[in]  This                This is the EFI_REST_EX_PROTOCOL instance.
  @param[out] RestExConfigData    Pointer to receive a pointer to EFI_REST_EX_CONFIG_DATA.
                                  The memory allocated for configuration data should be freed
                                  by caller. See Related Definitions for the details.

  @retval EFI_SUCCESS             EFI_REST_EX_CONFIG_DATA is returned in successfully.
  @retval EFI_UNSUPPORTED         This function is not supported in this REST EX Protocol driver instance.
  @retval EFI_NOT_READY           The configuration of this instance is not set yet. Configure() must be
                                  executed and returns successfully prior to invoke this function.

**/
EFI_STATUS
EFIAPI
RedfishRestExGetModeData (
  IN  EFI_REST_EX_PROTOCOL      *This,
  OUT EFI_REST_EX_CONFIG_DATA   *RestExConfigData
  );

/**
  This function is used to configure EFI REST EX child instance.

  This function is used to configure the setting of underlying protocol of REST EX child
  instance. The type of configuration is according to the implementation of EFI REST EX
  driver instance. For example, HTTP-aware EFI REST EX driver instance uses EFI HTTP protocol
  as the undying protocol to communicate with REST service. The type of configuration is
  EFI_REST_EX_CONFIG_TYPE_HTTP and RestExConfigData is the same format with EFI_HTTP_CONFIG_DATA.
  Akin to HTTP configuration, REST EX child instance can be configure to use different HTTP
  local access point for the data transmission. Multiple REST clients may use different
  configuration of HTTP to distinguish themselves, such as to use the different TCP port.
  For those non HTTP-aware REST EX driver instance, the type of configuration is
  EFI_REST_EX_CONFIG_TYPE_UNSPECIFIC. RestExConfigData refers to the non industrial standard.
  Instead, the format of configuration data is system/platform specific definition such as BMC.
  In this case, EFI REST client and EFI REST EX driver instance have to refer to the specific
  system/platform spec which is out of the UEFI scope. Besides GetService()function, no other
  EFI REST EX functions can be executed by this instance until Configure()is executed and returns
  successfully. All other functions must returns EFI_NOT_READY if this instance is not configured
  yet. Set RestExConfigData to NULL means to put EFI REST EX child instance into the unconfigured
  state.

  @param[in]  This                This is the EFI_REST_EX_PROTOCOL instance.
  @param[in]  RestExConfigData    Pointer to EFI_REST_EX_CONFIG_DATA. See Related Definitions in
                                  GetModeData() protocol interface.

  @retval EFI_SUCCESS             EFI_REST_EX_CONFIG_DATA is set in successfully.
  @retval EFI_DEVICE_ERROR        Configuration for this REST EX child instance is failed with the given
                                  EFI_REST_EX_CONFIG_DATA.
  @retval EFI_UNSUPPORTED         This function is not supported in this REST EX Protocol driver instance.

**/
EFI_STATUS
EFIAPI
RedfishRestExConfigure (
  IN  EFI_REST_EX_PROTOCOL    *This,
  IN  EFI_REST_EX_CONFIG_DATA RestExConfigData
  );

/**
  This function sends REST request to REST service and signal caller's event asynchronously when
  the final response is received by REST EX Protocol driver instance.

  The essential design of this function is to handle asynchronous send/receive implicitly according
  to REST service asynchronous request mechanism. Caller will get the notification once the response
  is returned from REST service.

  @param[in]  This                  This is the EFI_REST_EX_PROTOCOL instance.
  @param[in]  RequestMessage        This is the HTTP request message sent to REST service. Set RequestMessage
                                    to NULL to cancel the previous asynchronous request associated with the
                                    corresponding RestExToken. See descriptions for the details.
  @param[in]  RestExToken           REST EX token which REST EX Protocol instance uses to notify REST client
                                    the status of response of asynchronous REST request. See related definition
                                    of EFI_REST_EX_TOKEN.
  @param[in]  TimeOutInMilliSeconds The pointer to the timeout in milliseconds which REST EX Protocol driver
                                    instance refers as the duration to drop asynchronous REST request. NULL
                                    pointer means no timeout for this REST request. REST EX Protocol driver
                                    signals caller's event with EFI_STATUS set to EFI_TIMEOUT in RestExToken
                                    if REST EX Protocol can't get the response from REST service within
                                    TimeOutInMilliSeconds.

  @retval EFI_SUCCESS               Asynchronous REST request is established.
  @retval EFI_UNSUPPORTED           This REST EX Protocol driver instance doesn't support asynchronous request.
  @retval EFI_TIMEOUT               Asynchronous REST request is not established and timeout is expired.
  @retval EFI_ABORT                 Previous asynchronous REST request has been canceled.
  @retval EFI_DEVICE_ERROR          Otherwise, returns EFI_DEVICE_ERROR for other errors according to HTTP Status Code.
  @retval EFI_NOT_READY             The configuration of this instance is not set yet. Configure() must be executed
                                    and returns successfully prior to invoke this function.

**/
EFI_STATUS
EFIAPI
RedfishRestExAyncSendReceive (
  IN      EFI_REST_EX_PROTOCOL   *This,
  IN      EFI_HTTP_MESSAGE       *RequestMessage OPTIONAL,
  IN      EFI_REST_EX_TOKEN      *RestExToken,
  IN      UINTN                  *TimeOutInMilliSeconds OPTIONAL
  );

/**
  This function sends REST request to a REST Event service and signals caller's event
  token asynchronously when the URI resource change event is received by REST EX
  Protocol driver instance.

  The essential design of this function is to monitor event implicitly according to
  REST service event service mechanism. Caller will get the notification if certain
  resource is changed.

  @param[in]  This                  This is the EFI_REST_EX_PROTOCOL instance.
  @param[in]  RequestMessage        This is the HTTP request message sent to REST service. Set RequestMessage
                                    to NULL to cancel the previous event service associated with the corresponding
                                    RestExToken. See descriptions for the details.
  @param[in]  RestExToken           REST EX token which REST EX Protocol driver instance uses to notify REST client
                                    the URI resource which monitored by REST client has been changed. See the related
                                    definition of EFI_REST_EX_TOKEN in EFI_REST_EX_PROTOCOL.AsyncSendReceive().

  @retval EFI_SUCCESS               Asynchronous REST request is established.
  @retval EFI_UNSUPPORTED           This REST EX Protocol driver instance doesn't support asynchronous request.
  @retval EFI_ABORT                 Previous asynchronous REST request has been canceled or event subscription has been
                                    delete from service.
  @retval EFI_DEVICE_ERROR          Otherwise, returns EFI_DEVICE_ERROR for other errors according to HTTP Status Code.
  @retval EFI_NOT_READY             The configuration of this instance is not set yet. Configure() must be executed
                                    and returns successfully prior to invoke this function.

**/
EFI_STATUS
EFIAPI
RedfishRestExEventService (
  IN      EFI_REST_EX_PROTOCOL   *This,
  IN      EFI_HTTP_MESSAGE       *RequestMessage OPTIONAL,
  IN      EFI_REST_EX_TOKEN      *RestExToken
  );
/**
  Create a new TLS session becuase the previous on is closed.
  status.

  @param[in]  Instance            Pointer to EFI_REST_EX_PROTOCOL instance for a particular
                                  REST service.
  @retval EFI_SUCCESS             operation succeeded.
  @retval EFI Errors              Other errors.

**/
EFI_STATUS
ResetHttpTslSession (
  IN   RESTEX_INSTANCE  *Instance
);
#endif
