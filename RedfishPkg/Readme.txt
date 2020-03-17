
================================================================================
1. INTRODUCTION
================================================================================
  This Readme is a user guide for how to enable a Redfish client in UEFI 
environment. Before the start, the user needs to prepare a Redfish service 
provider and a complete code base of Redfish client, which can be downloaded 
from GitHub.

  When drafting this guide, we assume the user already has some basic knowledge 
about UEFI and Redfish, like how to enable secure boot on UEFI, or how to retrieve
a Redfish schema. If you have any question for this guide or meet any problems in
your system build, please contact us from EDKII community.

================================================================================
2. REDFISH SERVICE PROVIDER RESPONSIBILITIES
================================================================================
  UEFI as a Redfish client uses the well known base resource URI (consists of 
/redfish/v1, defined in DSP0266 specification) to start the enumeration of all 
Redfish resources. Redfish service provider shall set up the Redfish resource 
tree and guarantee corresponding URLs are valid. See following for an example 
list of URLs – UEFI does not limit the support of additional URLs:

	•	\redfish\v1\<Systems>
	•	\redfish\v1\<Systems>\<ComputerName>
	•	\redfish\v1\<Systems>\<ComputerName>\<Bios>
	•	\redfish\v1\<Systems>\<ComputerName>\<Bios>\<@Redfish.Settings>\
		<SettingsObject>
	•	\redfish\v1\<Systems>\<ComputerName>\<Bios>\<AttributeRegistry>

  Note that the above path with ‘<>’ may various in different Redfish service 
providers. The service provider should set up the resource tree according to the 
value of @odata.id defined in corresponding JSON files. If not, UEFI client will 
not be able to complete the Redfish configuration flow and will report error info 
to end user. Also Redfish service provider shall guarantee the available Redfish 
objects are compliant with Redfish schema defined by DMTF forum.

  UEFI client shall parse the Redfish objects retrieved from Redfish service 
provider and apply the settings during next boot. If the application of certain 
setting is failed, UEFI client may report error to Redfish service provider and 
stop processing next setting. The previous applied settings will not be rolled 
back automatically.

  Besides, Redfish service provider shall provide a privileged account to UEFI 
client for accessing and maintaining the Redfish objects which include capable 
of writing read-only attribute properties.

================================================================================
3. HOW TO ENABLE UEFI REDFISH CLIENT ON NT32 PLATFORM
================================================================================
1. Create a directory as workspace, and clone the Redfish repository from GitHub.

2. Clone submodule opensssl by command:

	$ git submodule update --init
	
   Enter in the openssl folder, and checkout to branch OpenSSL_1_1_0h by command:
	
	$ git checkout -b OpenSSL_1_1_0h OpenSSL_1_1_0h

3. Set edk2 workspace and build base tools.

4. Configure and Build NT32 platform (Redfish Client)

  4.1 Follow below guides to install third party libraries Jansson and Libredfish:

	  RedfishPkg/Library/JanssonLib/Jansson-HOWTO.txt
	  RedfishPkg/Library/LibredfishLib/Libredfish-HOWTO.txt

  4.2 Find below PCD flag in Nt32Pkg.dsc, and set value (005056C00002 as default)
	  to your own Mac address:

	  gEfiRedfishPkgTokenSpaceGuid.PcdRedfishRestExServiceInBandDevicePath.DevicePath|
	  {DEVICE_PATH("MAC(005056C00002,0x1)")}

  4.3 Build the NT32 platform (VS2015 is recommended):

	  build -a IA32 -t VS2015x86 -p Nt32Pkg\Nt32Pkg.dsc

5. Run and Initialize NT32 platform

  5.1 Make sure WinpCap is installed, and copy SnpNt32Io.dll to the building
	  directory of NT32 platform (Build\NT32IA32\DEBUG_VS2015x86\IA32). 

	  *NOTE: SnpNt32Io.dll can be built from code base in: "https://github.com/
	  tianocore/edk2-NetNt32Io".

  5.2 Run NT32 platform by command: 

	  build run -a IA32 -t VS2015x86

6. Configure Redfish platform driver via RedfishPlatformConfig tool, run below
   shell command (UEFI Shell V2.2 is recommended):

	  Format: RedfishPlatformConfig.efi -s SourceIpAddress SourceSubnetMask 
      ServerIpAddress ServerSubnetMask RedfishServicePort

	  Example: RedfishPlatformConfig.efi -s 192.168.10.101 255.255.255.0 
      192.168.10.123 255.255.255.0 5000

      *NOTE: RedfishPlatformDxe driver is used to publish the SMBIOS record for 
	  the required Redfish Server/Client information. These information needs to 
	  be configured via RedfishPlatformConfig tool.

7. Redfish client can work only when secure boot is enabled. The user needs to
   enable secure boot in secure boot configuration driver.

8. Reset platform to initialize Redfish resources. During the platform startup,
   if the Refish server is available, the supported attribute registry and current 
   UEFI configurations will be synchronized to the Redfish server.
   
================================================================================
4. HOW TO ENABLE UEFI REDFISH CLIENT ON REAL PLATFORMS
================================================================================

  Only Nt32 platform is fully tested now, if you are interested in building a UEFI
Redfish client on real platforms, please follow these tips:

1. Install third party libraries Jansson and Libredfish, and add below libraries
   into your platform dsc file:

   JanssonLib|RedfishPkg/Library/JanssonLib/JanssonLib.inf
   BaseJsonLib|RedfishPkg/Library/BaseJsonLib/BaseJsonLib.inf
   LibredfishLib|RedfishPkg/Library/LibredfishLib/LibredfishLib.inf
   RedfishLib|RedfishPkg/Library/DxeRedfishLib/DxeRedfishLib.inf

2. Add below Redfish drivers into your platform dsc and fdf files:

   RedfishPkg/RestExDxe/RestExDxe.inf
   RedfishPkg/RedfishConfigDxe/RedfishConfigDxe.inf
   RedfishPkg/Features/RedfishBiosDxe/RedfishBiosDxe.inf
   RedfishPkg/Features/RedfishBootInfoDxe/RedfishBootInfoDxe.inf

3. Add your own Redfish platform driver which shall follow DSP0270 to publish 
   Server/Client information to SMBIOS record and produce Redfish credential 
   protocol for retrieving a privileged account from Redfish server.
   
   *NOTE: Please refer to Nt32Pkg\RedfishPlatformDxe to build your own Redfish
   platform driver.

4. Configure gEfiRedfishPkgTokenSpaceGuid in your platform dsc file, and the
   format should be the same as it in Nt32Pkg.dsc.

5. Build your system and enable secure boot, then the Redfish client should work
   now.