## @file
# Redfish Package
#
# Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
# (C) Copyright 2020 Hewlett-Packard Enterprise Development LP.
#
#    SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME                  = RedfishPkg
  PLATFORM_GUID                  = c4352870-5232-11e7-9522-005056c00008
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x0001001c
  OUTPUT_DIRECTORY               = Build/RedfishPkg
  SUPPORTED_ARCHITECTURES        = IA32|X64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiUsbLib|MdePkg/Library/UefiUsbLib/UefiUsbLib.inf
  UefiScsiLib|MdePkg/Library/UefiScsiLib/UefiScsiLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  DebugLib|MdePkg/Library/UefiDebugLibStdErr/UefiDebugLibStdErr.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  PostCodeLib|MdePkg/Library/BasePostCodeLibPort80/BasePostCodeLibPort80.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/DxeReportStatusCodeLib/DxeReportStatusCodeLib.inf

  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf

  HttpLib|NetworkPkg/Library/DxeHttpLib/DxeHttpLib.inf

  JanssonLib|RedfishPkg/Library/JanssonLib/JanssonLib.inf
  BaseJsonLib|RedfishPkg/Library/BaseJsonLib/BaseJsonLib.inf
  LibredfishLib|RedfishPkg/Library/LibredfishLib/LibredfishLib.inf

[Components]
  RedfishPkg/Library/JanssonLib/JanssonLib.inf
  RedfishPkg/Library/BaseJsonLib/BaseJsonLib.inf
  RedfishPkg/Library/LibredfishLib/LibredfishLib.inf

  RedfishPkg/RestJsonStructureDxe/RestJsonStructureDxe.inf
