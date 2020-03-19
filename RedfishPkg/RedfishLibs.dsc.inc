## @file
# Redfishh DSC include file for [LibraryClasses*] section of all Architectures.
#
# This file can be included to the [LibraryClasses*] section(s) of a platform DSC file
# by using "!include RedfishPkg/RedfisLibs.dsc.inc" to specify the library instances
# of EDKII network library classes.
#
# (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>
#
#    SPDX-License-Identifier: BSD-2-Clause-Patent
#
##
!if $(REDFISH_ENABLE) == TRUE
  JanssonLib|RedfishPkg/Library/JanssonLib/JanssonLib.inf
  BaseJsonLib|RedfishPkg/Library/BaseJsonLib/BaseJsonLib.inf
  LibredfishLib|RedfishPkg/Library/LibredfishLib/LibredfishLib.inf
  RedfishLib|RedfishPkg/Library/DxeRedfishLib/DxeRedfishLib.inf
  RestExLib|MdeModulePkg/Library/DxeRestExLib/DxeRestExLib.inf  
!endif  

