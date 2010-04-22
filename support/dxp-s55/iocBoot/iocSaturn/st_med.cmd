< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("../../dbd/dxp.dbd")
dxp_registerRecordDeviceDriver(pdbbase)

# Initialize the XIA software
# Set logging level (1=ERROR, 2=WARNING, 3=XXX, 4=DEBUG)
xiaSetLogLevel(2)
# Execute the following line if you have a Vortex detector or 
# another detector with a reset pre-amplifier
#xiaInit("vortex.ini")
xiaInit("vortex40MHz.ini")
# Execute the following line if you have a Ketek detector or 
# another detector with an RC pre-amplifier
#xiaInit("ketek.ini")
#xiaInit("ketek40MHz.ini")
xiaStartSystem

# DXPConfig(serverName, ndetectors, ngroups, pollFrequency)
DXPConfig("DXP1",  1, 1, 100)


# DXP record
# Execute the following line if you have a Vortex detector or 
# another detector with a reset pre-amplifier
dbLoadTemplate("1element.template")

# Setup for save_restore
< ../save_restore.cmd
save_restoreSet_status_prefix("dxpSaturn:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxpSaturn:")
set_pass0_restoreFile("auto_settings_med.sav")
set_pass1_restoreFile("auto_settings_med.sav")

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (See 'saveData' below for that.)
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db","P=dxpSaturn:,MAXPTS1=2000,MAXPTS2=1000,MAXPTS3=10,MAXPTS4=10,MAXPTSH=2048")

# Debugging flags
#asynSetTraceMask DXP1 0 255
#var mcaRecordDebug 10
#var dxpRecordDebug 10

iocInit

### Start up the autosave task and tell it what to do.

# Save settings every thirty seconds
create_monitor_set("auto_settings_med.req", 30, P=dxpSaturn:)

### Start the saveData task.
saveData_Init("saveData.req", "P=dxpSaturn:")

# Start the sequence program for multi-element detector
seq &dxpMED, "P=dxpSaturn:, DXP=dxp, MCA=mca, N_DETECTORS=1"
