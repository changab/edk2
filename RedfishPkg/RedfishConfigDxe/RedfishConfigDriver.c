/** @file
  An UEFI driver model driver which is responsible for locating the correct
  Redfish host interface NIC device and executing Redfish config handlers.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "RedfishConfigDxe.h"

REDFISH_CONFIG_DRIVER_DATA      mRedfishConfigData;
EFI_REDFISH_CREDENTIAL_PROTOCOL *mCredential = NULL;
EFI_EVENT                       mEndOfDxeEvent = NULL;
EFI_EVENT                       mExitBootServiceEvent = NULL;
EFI_EVENT                       mEfiRedfishDiscoverProtocolEvent = NULL;
//
// Variables for using RFI Redfish Discover Protocol
//
VOID                            *mEfiRedfishDiscoverRegistration;
EFI_HANDLE                      mEfiRedfishDiscoverControllerHandle = NULL;
EFI_REDFISH_DISCOVER_PROTOCOL   *mEfiRedfishDiscoverProtocol = NULL;
BOOLEAN                         mRedfishDiscoverActivated = FALSE;
//
// Network interfaces discovered by EFI Redfish Discover Protocol.
//
UINTN                                 mNumberOfNetworkInterfaces;
EFI_REDFISH_DISCOVER_NETWORK_INSTANCE *mNetworkInterfaceInstances = NULL;
EFI_REDFISH_DISCOVERED_TOKEN          mRedfishDiscoveredToken;

///
/// Driver Binding Protocol instance
///
EFI_DRIVER_BINDING_PROTOCOL gRedfishConfigDriverBinding = {
  RedfishConfigDriverBindingSupported,
  RedfishConfigDriverBindingStart,
  RedfishConfigDriverBindingStop,
  REDFISH_CONFIG_VERSION,
  NULL,
  NULL
};

/**
  Callback function executed when a Redfish Config Handler Protocol is installed.

  @param[in]  Event    Event whose notification function is being invoked.
  @param[in]  Context  Pointer to the REDFISH_CONFIG_DRIVER_DATA buffer.

**/
VOID
EFIAPI
RedfishConfigHandlerInstalledCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS                            Status;
  EFI_HANDLE                            *HandleBuffer;
  UINTN                                 NumberOfHandles;
  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL   *ConfigHandler;
  REDFISH_CONFIG_DRIVER_DATA            *Private;
  UINTN                                 Index;
  UINT32                                Id;

  if (!mRedfishDiscoverActivated) {
    //
    // No Redfish service is discovered yet.
    //
    return;
  }

  Private = (REDFISH_CONFIG_DRIVER_DATA *) Context;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiRedfishConfigHandlerProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiCallerIdGuid,
                    (VOID **) &Id
                    );
    if (!EFI_ERROR (Status)) {
      continue;
    }

    Status = gBS->HandleProtocol (
                     HandleBuffer[Index],
                     &gEfiRedfishConfigHandlerProtocolGuid,
                     (VOID**) &ConfigHandler
                     );
    ASSERT_EFI_ERROR (Status);
    Status = ConfigHandler->Init (ConfigHandler, &mRedfishConfigData.RedfishServiceInfo);
    if (EFI_ERROR (Status) && Status != EFI_ALREADY_STARTED) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to init Redfish config handler %p.\n", ConfigHandler));
    }
    //
    // Install caller ID to indicate Redfish Configure Handler is initialized.
    //
    Status = gBS->InstallProtocolInterface (
                  &HandleBuffer[Index],
                  &gEfiCallerIdGuid,
                  EFI_NATIVE_INTERFACE,
                  (VOID *)&mRedfishConfigData.CallerId
                  );
    ASSERT_EFI_ERROR (Status);
  }
}
/**
  Stop acquiring Redfish service.

**/
VOID
RedfishConfigStopRedfishDiscovery (
  VOID
)
{
  if (mRedfishDiscoverActivated == TRUE) {
    //
    // No more EFI Discover Protocol.
    //
    if (mEfiRedfishDiscoverProtocolEvent != NULL) {
      gBS->CloseEvent (mEfiRedfishDiscoverProtocolEvent);
    }
    //
    // Stop Redfish service discovery.
    //
    mEfiRedfishDiscoverProtocol->AbortAcquireRedfishService (
                                   mEfiRedfishDiscoverProtocol,
                                   mNetworkInterfaceInstances
                                   );
    mEfiRedfishDiscoverControllerHandle = NULL;
    mEfiRedfishDiscoverProtocol = NULL;
    mRedfishDiscoverActivated = FALSE;
  }
}

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a handle for the specified child device.

  This function checks to see if the driver specified by This supports the device specified by
  ControllerHandle. Drivers will typically use the device path attached to
  ControllerHandle and/or the services from the bus I/O abstraction attached to
  ControllerHandle to determine if the driver supports ControllerHandle. This function
  may be called many times during platform initialization. In order to reduce boot times, the tests
  performed by this function must be very small, and take as little time as possible to execute. This
  function must not change the state of any hardware devices, and this function must be aware that the
  device specified by ControllerHandle may already be managed by the same driver or a
  different driver. This function must match its calls to AllocatePages() with FreePages(),
  AllocatePool() with FreePool(), and OpenProtocol() with CloseProtocol().
  Because ControllerHandle may have been previously started by the same driver, if a protocol is
  already in the opened state, then it must not be closed with CloseProtocol(). This is required
  to guarantee the state of ControllerHandle is not modified by this function.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For bus drivers, if this parameter is not NULL, then
                                   the bus driver must determine if the bus controller specified
                                   by ControllerHandle and the child controller specified
                                   by RemainingDevicePath are both supported by this
                                   bus driver.

  @retval EFI_SUCCESS              The device specified by ControllerHandle and
                                   RemainingDevicePath is supported by the driver specified by This.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by the driver
                                   specified by This.
  @retval EFI_ACCESS_DENIED        The device specified by ControllerHandle and
                                   RemainingDevicePath is already being managed by a different
                                   driver or an application that requires exclusive access.
                                   Currently not implemented.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle and
                                   RemainingDevicePath is not supported by the driver specified by This.
**/
EFI_STATUS
EFIAPI
RedfishConfigDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_REST_EX_PROTOCOL             *RestEx;
  EFI_STATUS                       Status;
  EFI_HANDLE                       ChildHandle;

  ChildHandle = NULL;

  //
  // Check if REST EX is ready. This just makes sure
  // the network stack is brought up.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->ImageHandle,
             &gEfiRestExServiceBindingProtocolGuid,
             &ChildHandle
             );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  //
  // Test if REST EX protocol is ready.
  //
  Status = gBS->OpenProtocol(
                  ChildHandle,
                  &gEfiRestExProtocolGuid,
                  (VOID**) &RestEx,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    Status = EFI_UNSUPPORTED;
  }
  NetLibDestroyServiceChild (
    ControllerHandle,
    This->ImageHandle,
    &gEfiRestExServiceBindingProtocolGuid,
    ChildHandle
    );
  return Status;
}

/**
  Starts a device controller or a bus controller.

  The Start() function is designed to be invoked from the EFI boot service ConnectController().
  As a result, much of the error checking on the parameters to Start() has been moved into this
  common boot service. It is legal to call Start() from other locations,
  but the following calling restrictions must be followed, or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE.
  2. If RemainingDevicePath is not NULL, then it must be a pointer to a naturally aligned
     EFI_DEVICE_PATH_PROTOCOL.
  3. Prior to calling Start(), the Supported() function for the driver specified by This must
     have been called with the same calling parameters, and Supported() must have returned EFI_SUCCESS.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to start. This handle
                                   must support a protocol interface that supplies
                                   an I/O abstraction to the driver.
  @param[in]  RemainingDevicePath  A pointer to the remaining portion of a device path.  This
                                   parameter is ignored by device drivers, and is optional for bus
                                   drivers. For a bus driver, if this parameter is NULL, then handles
                                   for all the children of Controller are created by this driver.
                                   If this parameter is not NULL and the first Device Path Node is
                                   not the End of Device Path Node, then only the handle for the
                                   child device specified by the first Device Path Node of
                                   RemainingDevicePath is created by this driver.
                                   If the first Device Path Node of RemainingDevicePath is
                                   the End of Device Path Node, no child handle is created by this
                                   driver.

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a device error.Currently not implemented.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.
  @retval Others                   The driver failded to start the device.

**/
EFI_STATUS
EFIAPI
RedfishConfigDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  VOID *ConfigHandlerRegistration;

  if (mRedfishConfigData.Event != NULL) {
    return EFI_ALREADY_STARTED;
  }

  mRedfishConfigData.Event = EfiCreateProtocolNotifyEvent (
                                &gEfiRedfishConfigHandlerProtocolGuid,
                                TPL_CALLBACK,
                                RedfishConfigHandlerInstalledCallback,
                                (VOID *)&mRedfishConfigData,
                                &ConfigHandlerRegistration
                                );
  return EFI_SUCCESS;
}

/**
  Stops a device controller or a bus controller.

  The Stop() function is designed to be invoked from the EFI boot service DisconnectController().
  As a result, much of the error checking on the parameters to Stop() has been moved
  into this common boot service. It is legal to call Stop() from other locations,
  but the following calling restrictions must be followed, or the system behavior will not be deterministic.
  1. ControllerHandle must be a valid EFI_HANDLE that was used on a previous call to this
     same driver's Start() function.
  2. The first NumberOfChildren handles of ChildHandleBuffer must all be a valid
     EFI_HANDLE. In addition, all of these handles must have been created in this driver's
     Start() function, and the Start() function must have called OpenProtocol() on
     ControllerHandle with an Attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped. The handle must
                                support a bus specific I/O protocol for the driver
                                to use to stop the device.
  @param[in]  NumberOfChildren  The number of child device handles in ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be NULL
                                if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
EFI_STATUS
EFIAPI
RedfishConfigDriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  EFI_STATUS                            Status;
  EFI_HANDLE                            *HandleBuffer;
  UINTN                                 NumberOfHandles;
  EFI_REDFISH_CONFIG_HANDLER_PROTOCOL   *ConfigHandler;
  UINTN                                 Index;

  if (ControllerHandle == mEfiRedfishDiscoverControllerHandle) {
    RedfishConfigStopRedfishDiscovery ();
  }
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiRedfishDiscoverProtocolGuid,
         mRedfishConfigData.Image,
         mRedfishConfigData.Image
         );

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiRedfishConfigHandlerProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) && Status != EFI_NOT_FOUND) {
    return Status;
  }

  Status = EFI_SUCCESS;
  for (Index = 0; Index < NumberOfHandles; Index++) {
    Status = gBS->HandleProtocol (
                     HandleBuffer[Index],
                     &gEfiRedfishConfigHandlerProtocolGuid,
                     (VOID**) &ConfigHandler
                     );
    ASSERT_EFI_ERROR (Status);

    Status = ConfigHandler->Stop (ConfigHandler);
    if (EFI_ERROR (Status) && Status != EFI_UNSUPPORTED) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to stop Redfish config handler %p.\n", ConfigHandler));
      break;
    }
  }

  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  if (mRedfishConfigData.Event != NULL) {
    gBS->CloseEvent (mRedfishConfigData.Event);
    mRedfishConfigData.Event = NULL;
  }
  return EFI_SUCCESS;
}

/**
  Callback function executed when the EndOfDxe event group is signaled.

  @param[in]   Event    Event whose notification function is being invoked.
  @param[OUT]  Context  Pointer to the Context buffer.

**/
VOID
EFIAPI
RedfishConfigOnEndOfDxe (
  IN  EFI_EVENT  Event,
  OUT VOID       *Context
  )
{
  EFI_STATUS                          Status;

  Status = mCredential->StopService (mCredential, ServiceStopTypeSecureBootDisabled);
  if (EFI_ERROR(Status) && Status != EFI_UNSUPPORTED) {
    DEBUG ((DEBUG_ERROR, "Redfish credential protocol faied to stop service on EndOfDxe: %r", Status));
  }

  //
  // Close event, so it will not be invoked again.
  //
  gBS->CloseEvent (mEndOfDxeEvent);
  mEndOfDxeEvent = NULL;
}

/**
  Callback function executed when the ExitBootService event group is signaled.

  @param[in]   Event    Event whose notification function is being invoked.
  @param[OUT]  Context  Pointer to the Context buffer

**/
VOID
EFIAPI
RedfishConfigOnExitBootService (
  IN  EFI_EVENT  Event,
  OUT VOID       *Context
  )
{
  EFI_STATUS                          Status;

  Status = mCredential->StopService (mCredential, ServiceStopTypeExitBootService);
  if (EFI_ERROR(Status) && Status != EFI_UNSUPPORTED) {
    DEBUG ((DEBUG_ERROR, "Redfish credential protocol faied to stop service on ExitBootService: %r", Status));
  }
}

/**
  Callback function when Redfish service is discovered.

  @param[in]   Event    Event whose notification function is being invoked.
  @param[OUT]  Context  Pointer to the Context buffer

**/
VOID
EFIAPI
RedfishServiceDiscoveredCallback (
  IN  EFI_EVENT  Event,
  OUT VOID       *Context
  )
{
  UINTN NumberOfService;
  EFI_REDFISH_DISCOVERED_TOKEN *RedfishDiscoveredToken;
  EFI_REDFISH_DISCOVERED_INSTANCE *RedfishInstance;

  RedfishDiscoveredToken = (EFI_REDFISH_DISCOVERED_TOKEN *)Context;
  RedfishInstance = RedfishDiscoveredToken->DiscoverList.RedfishInstances;
  for (NumberOfService = 0; NumberOfService < RedfishDiscoveredToken->DiscoverList.NumberOfServiceFound; NumberOfService ++) {
    //
    // Only pick up the first valid Redfish service.
    //
    if (RedfishInstance->Status == EFI_SUCCESS) {
      CopyMem (
        (VOID *)&mRedfishConfigData.RedfishServiceInfo,
        (VOID *)&RedfishInstance->Information,
        sizeof (REDFISH_CONFIG_SERVICE_INFORMATION));
      break;
    }
    RedfishInstance ++;
  }

  //
  // Invoke RedfishConfigHandlerInstalledCallback to execute
  // the initialization of Redfish Configure Handler instance.
  //
  RedfishConfigHandlerInstalledCallback (mRedfishConfigData.Event, &mRedfishConfigData);
}

/**
  Callback function executed when the EFI_REDFISH_DISCOVER_PROTOCOL
  protocol interface is installed.

  @param[in]   Event    Event whose notification function is being invoked.
  @param[OUT]  Context  Pointer to the Context buffer

**/
VOID
EFIAPI
RedfishDiscoverProtocolInstalled (
  IN  EFI_EVENT  Event,
  OUT VOID       *Context
  )
{
  EFI_STATUS Status;
  UINTN BufferSize;
  EFI_HANDLE HandleBuffer;
  UINTN NetworkInterfaceIndex;
  EFI_REDFISH_DISCOVER_NETWORK_INSTANCE *ThisNetworkInterface;

  DEBUG((DEBUG_INFO, "%a: New network interface is installed on system by EFI Redfish discover driver.\n", __FUNCTION__));

  BufferSize = sizeof (EFI_HANDLE);
  Status = gBS->LocateHandle (
                  ByRegisterNotify,
                  NULL,
                  mEfiRedfishDiscoverRegistration,
                  &BufferSize,
                  &HandleBuffer
                );
  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_ERROR, "%a: Can't locate handle with EFI_REDFISH_DISCOVER_PROTOCOL installed.\n", __FUNCTION__));
  }
  mRedfishDiscoverActivated = TRUE;
  if (mEfiRedfishDiscoverProtocol == NULL) {
     mEfiRedfishDiscoverControllerHandle = HandleBuffer;
    //
    // First time to open EFI_REDFISH_DISCOVER_PROTOCOL.
    //
    Status = gBS->OpenProtocol(
                      mEfiRedfishDiscoverControllerHandle,
                      &gEfiRedfishDiscoverProtocolGuid,
                      (VOID **)&mEfiRedfishDiscoverProtocol,
                      mRedfishConfigData.Image,
                      mRedfishConfigData.Image,
                      EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (EFI_ERROR (Status)) {
      mEfiRedfishDiscoverProtocol = NULL;
      mRedfishDiscoverActivated = FALSE;
      DEBUG((DEBUG_ERROR, "%a: Can't locate EFI_REDFISH_DISCOVER_PROTOCOL.\n", __FUNCTION__));
      return;
    }
  }
  //
  // Check the new found network interface.
  //
  if (mNetworkInterfaceInstances != NULL) {
    FreePool (mNetworkInterfaceInstances);
  }
  Status = mEfiRedfishDiscoverProtocol->GetNetworkInterfaceList(
                                          mEfiRedfishDiscoverProtocol,
                                          mRedfishConfigData.Image,
                                          &mNumberOfNetworkInterfaces,
                                          &mNetworkInterfaceInstances
                                          );
  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_ERROR, "%a: No network interfaces found on the handle.\n", __FUNCTION__));
    return;
  }
  //
  // Acquire for Redfish service which is reported by
  // Redfish Host Interface.
  //
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  RedfishServiceDiscoveredCallback,
                  (VOID *)&mRedfishDiscoveredToken,
                  &mRedfishDiscoveredToken.Event
                  );
  mRedfishDiscoveredToken.Signature = REDFISH_DISCOVER_TOKEN_SIGNATURE;
  mRedfishDiscoveredToken.DiscoverList.NumberOfServiceFound = 0;
  mRedfishDiscoveredToken.DiscoverList.RedfishInstances = NULL;
  ThisNetworkInterface = mNetworkInterfaceInstances;
  //
  // Loop to discover Redfish service on each network interface.
  //
  for (NetworkInterfaceIndex = 0; NetworkInterfaceIndex < mNumberOfNetworkInterfaces; NetworkInterfaceIndex ++) {
    Status = mEfiRedfishDiscoverProtocol->AcquireRedfishService(
                                             mEfiRedfishDiscoverProtocol,
                                             mRedfishConfigData.Image,
                                             ThisNetworkInterface,
                                             EFI_REDFISH_DISCOVER_HOST_INTERFACE,
                                             &mRedfishDiscoveredToken
                                             );
    if (!EFI_ERROR(Status)) {
      return;
    }
    ThisNetworkInterface++;
  }
  if (EFI_ERROR (Status)) {
    DEBUG((DEBUG_ERROR, "%a: Acquire Redfish service fail.\n"));
  }
}

/**
  Unloads an image.

  @param  ImageHandle           Handle that identifies the image to be unloaded.

  @retval EFI_SUCCESS           The image has been unloaded.
  @retval EFI_INVALID_PARAMETER ImageHandle is not a valid image handle.

**/
EFI_STATUS
EFIAPI
RedfishConfigDriverUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  RedfishConfigStopRedfishDiscovery ();

  if (mEndOfDxeEvent != NULL) {
    gBS->CloseEvent (mEndOfDxeEvent);
    mEndOfDxeEvent = NULL;
  }

  if (mExitBootServiceEvent != NULL) {
    gBS->CloseEvent (mExitBootServiceEvent);
    mExitBootServiceEvent = NULL;
  }

  if (mRedfishConfigData.Event != NULL) {
    gBS->CloseEvent (mRedfishConfigData.Event);
    mRedfishConfigData.Event = NULL;
  }
  return EFI_SUCCESS;
}

/**
  This is the declaration of an EFI image entry point. This entry point is
  the same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers including
  both device drivers and bus drivers.

  @param  ImageHandle           The firmware allocated handle for the UEFI image.
  @param  SystemTable           A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval Others                An unexpected error occurred.
**/
EFI_STATUS
EFIAPI
RedfishConfigDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS    Status;

  ZeroMem ((VOID *)&mRedfishConfigData, sizeof (REDFISH_CONFIG_DRIVER_DATA));
  mRedfishConfigData.Image     = ImageHandle;
  //
  // Register event for EFI_REDFISH_DISCOVER_PROTOCOL protocol install
  // notification.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  RedfishDiscoverProtocolInstalled,
                  NULL,
                  &gEfiRedfishDiscoverProtocolGuid,
                  &mEfiRedfishDiscoverProtocolEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to create event for the installation of EFI_REDFISH_DISCOVER_PROTOCOL.", __FUNCTION__));
    return Status;
  }
  Status = gBS->RegisterProtocolNotify (
                  &gEfiRedfishDiscoverProtocolGuid,
                  mEfiRedfishDiscoverProtocolEvent,
                  &mEfiRedfishDiscoverRegistration
                );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to register event for the installation of EFI_REDFISH_DISCOVER_PROTOCOL.", __FUNCTION__));
    return Status;
  }
  //
  // Locate Redfish Credential Protocol to get credential for
  // accessing to Redfish service.
  //
  Status = gBS->LocateProtocol (&gEfiRedfishCredentialProtocolGuid, NULL, (VOID **) &mCredential);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: No Redfish Credential Protocol is installed on system.", __FUNCTION__));
    gBS->CloseEvent (mEfiRedfishDiscoverProtocolEvent);
    mEfiRedfishDiscoverProtocolEvent = NULL;
    return Status;
  }
  //
  // Create EndOfDxe Event.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  RedfishConfigOnEndOfDxe,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &mEndOfDxeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to register End Of DXE event.", __FUNCTION__));
    return Status;
  }
  //
  // Create Exit Boot Service event.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  RedfishConfigOnExitBootService,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &mExitBootServiceEvent
                  );
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (mEndOfDxeEvent);
    mEndOfDxeEvent = NULL;
    gBS->CloseEvent (mEfiRedfishDiscoverProtocolEvent);
    mEfiRedfishDiscoverProtocolEvent = NULL;
    DEBUG ((DEBUG_ERROR, "%a: Fail to register Exit Boot Service event.", __FUNCTION__));
    return Status;
  }

  //
  // Install UEFI Driver Model protocol(s).
  //
  Status = EfiLibInstallDriverBinding (
             ImageHandle,
             SystemTable,
             &gRedfishConfigDriverBinding,
             ImageHandle
             );
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (mEndOfDxeEvent);
    mEndOfDxeEvent = NULL;
    gBS->CloseEvent (mExitBootServiceEvent);
    mExitBootServiceEvent = NULL;
    gBS->CloseEvent (mEfiRedfishDiscoverProtocolEvent);
    mEfiRedfishDiscoverProtocolEvent = NULL;
    DEBUG ((DEBUG_ERROR, "%a: Fail to install EFI Binding Protocol of EFI Redfish Config driver.", __FUNCTION__));
    return Status;
  }
  return Status;
}

