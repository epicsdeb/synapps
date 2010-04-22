# If running on a PowerPC with more than 32MB of memory and not built with long jump ...
# Allocate 96MB of memory temporarily so that all code loads in 32MB.
mem = malloc(1024*1024*96)

# vxWorks startup file
< cdCommands

< ../nfsCommands

cd topbin
ld < dxpApp.munch
cd startup

# Increase size of buffer for error logging from default 1256
errlogInit(50000)

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build.
dbLoadDatabase("$(TOP)/dbd/dxpVX.dbd")
dxpVX_registerRecordDeviceDriver(pdbbase)

putenv("EPICS_TS_MIN_WEST=300")

# Setup for save_restore
< ../save_restore_vxWorks.cmd
save_restoreSet_status_prefix("dxp2X:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxp2X:")
set_pass0_restoreFile("auto_settings16.sav")
set_pass1_restoreFile("auto_settings16.sav")

# Setup the ksc2917 hardware definitions
# These are all actually the defaults, so this is not really necessary
# num_cards, addrs, ivec, irq_level
ksc2917_setup(1, 0xFF00, 0x00A0, 2)

# Initialize the CAMAC library.  Note that this is normally done automatically
# in iocInit, but we need to get the CAMAC routines working before iocInit
# because we need to initialize the DXP hardware.
camacLibInit

# Set logging level (1=ERROR, 2=WARNING, 3=INFO, 4=DEBUG)
xiaSetLogLevel(2)
xiaInit("dxp4c2x_16.ini")
xiaStartSystem

# DXPConfig(serverName, ndetectors, ngroups, pollFrequency)
DXPConfig("DXP1",  16, 1, 100)

dbLoadTemplate("16element.template")

#xiaSetLogLevel(4)
#asynSetTraceMask DXP1 0 255
#asynSetTraceIOMask DXP1 0 2
#var dxpRecordDebug 10

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (See 'saveData' below for that.)
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db","P=dxp2X:,MAXPTS1=2000,MAXPTS2=1000,MAXPTS3=10,MAXPTS4=10,MAXPTSH=2048")

iocInit

seq &dxpMED, "P=dxp2X:, DXP=dxp, MCA=mca, N_DETECTORS=16"

### Start up the autosave task and tell it what to do.
# Save settings every thirty seconds
create_monitor_set("auto_settings16.req", 30, "P=dxp2X:")

### Start the saveData task.
saveData_Init("saveData.req", "P=dxp2X:")

# Free the memory allocated at the top
free(mem)

