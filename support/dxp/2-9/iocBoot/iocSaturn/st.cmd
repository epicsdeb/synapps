< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("../../dbd/dxp.dbd")
dxp_registerRecordDeviceDriver(pdbbase)

# On Linux execute the following command so that libusb uses /dev/bus/usb
# as the file system for the USB device.  
# On some Linux systems it uses /proc/bus/usb instead, but udev
# sets the permissions on /dev, not /proc.
epicsEnvSet USB_DEVFS_PATH /dev/bus/usb

# Initialize the XIA software
# Set logging level (1=ERROR, 2=WARNING, 3=XXX, 4=DEBUG)
xiaSetLogLevel(2)
# Edit saturn.ini to match your Saturn speed (20 or 40 MHz), 
# pre-amp type (reset or RC), and interface type (EPP, USB 1.0, USB 2.0)
xiaInit("saturn.ini")
xiaStartSystem

# DXPConfig(serverName, ndetectors, ngroups, pollFrequency)
DXPConfig("DXP1",  1, 1, 100)


# DXP record
# Execute the following line if you have a Vortex detector or 
# another detector with a reset pre-amplifier
dbLoadRecords("../../dxpApp/Db/dxp2x_reset.db","P=dxpSaturn:, R=dxp1, INP=@asyn(DXP1 0)")
# Execute the following line if you have a Ketek detector or 
# another detector with an RC pre-amplifier
#dbLoadRecords("../../dxpApp/Db/dxp2x_rc.db","P=dxpSaturn:, R=dxp1, INP=@asyn(DXP1 0)")

# MCA record
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=dxpSaturn:, M=mca1, DTYP=asynMCA,INP=@asyn(DXP1 0),NCHAN=2048")
dbLoadRecords("../../dxpApp/Db/mcaCallback.db", "P=dxpSaturn:, M=mca1,INP=@asyn(DXP1 0)")

# Template to copy MCA ROIs to DXP SCAs
dbLoadTemplate("roi_to_sca.substitutions")

# Setup for save_restore
< ../save_restore.cmd
save_restoreSet_status_prefix("dxpSaturn:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxpSaturn:")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (See 'saveData' below for that.)
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db","P=dxpSaturn:,MAXPTS1=2000,MAXPTS2=1000,MAXPTS3=10,MAXPTS4=10,MAXPTSH=2048")

# Debugging flags
#xiaSetLogLevel(4)
#asynSetTraceMask DXP1 0 255
#var mcaRecordDebug 10
#var dxpRecordDebug 10

iocInit

### Start up the autosave task and tell it what to do.

# Save settings every thirty seconds
create_monitor_set("auto_settings.req", 30, P=dxpSaturn:)

### Start the saveData task.
saveData_Init("saveData.req", "P=dxpSaturn:")

