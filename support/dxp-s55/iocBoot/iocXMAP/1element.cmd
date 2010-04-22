< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("$(DXP)/dbd/dxp.dbd")
dxp_registerRecordDeviceDriver(pdbbase)

# Setup for save_restore
< ../save_restore.cmd
save_restoreSet_status_prefix("dxpXMAP:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxpXMAP:")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")

# Set logging level (1=ERROR, 2=WARNING, 3=INFO, 4=DEBUG)
xiaSetLogLevel(2)
# Execute the following line if you have a Vortex detector or
# another detector with a reset pre-amplifier
xiaInit("xmap4.ini")
xiaStartSystem

# DXPConfig(serverName, nchans)
DXPConfig("DXP1",  1)

# DXP record
dbLoadRecords("$(DXP)/dxpApp/Db/xmap_reset.db", "P=dxpXMAP:, R=dxp1, INP=@asyn(DXP1 0)")
# MCA record
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=dxpXMAP:, M=mca1, DTYP=asynMCA,INP=@asyn(DXP1 0),NCHAN=2048")

#dbLoadTemplate("roi_to_sca.substitutions")

# Debugging flags
xiaSetLogLevel(2)
#asynSetTraceMask DXP1 0 255
#var mcaRecordDebug 10
#var dxpRecordDebug 10

iocInit

### Start up the autosave task and tell it what to do.

# Save settings every thirty seconds
create_monitor_set("auto_settings.req", 30)

