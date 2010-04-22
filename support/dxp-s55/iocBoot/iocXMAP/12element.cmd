< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("$(DXP)/dbd/dxp.dbd")
dxp_registerRecordDeviceDriver(pdbbase)

# Setup for save_restore
< ../save_restore.cmd
save_restoreSet_status_prefix("dxpXMAP:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxpXMAP:")
set_pass0_restoreFile("auto_settings12.sav")
set_pass1_restoreFile("auto_settings12.sav")

# Set logging level (1=ERROR, 2=WARNING, 3=INFO, 4=DEBUG)
xiaSetLogLevel(2)
xiaInit("xmap12.ini")
xiaStartSystem

# DXPConfig(serverName, ndetectors, maxBuffers, maxMemory)
NDDxpConfig("DXP1",  12, -1, -1)

dbLoadTemplate("12element.substitutions")

# Create a netCDF file saving plugin
NDFileNetCDFConfigure("DXP1NetCDF", 100, 0, "DXP1", 0)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=dxpXMAP:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1,NDARRAY_PORT=DXP1,NDARRAY_ADDR=0")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template",      "P=dxpXMAP:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFileNetCDF.template","P=dxpXMAP:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1")

#xiaSetLogLevel(4)
#asynSetTraceMask DXP1 0 255
asynSetTraceIOMask DXP1 0 2

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (See 'saveData' below for that.)
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db","P=dxpXMAP:,MAXPTS1=2000,MAXPTS2=1000,MAXPTS3=10,MAXPTS4=10,MAXPTSH=2048")

iocInit

seq dxpMED, "P=dxpXMAP:, DXP=dxp, MCA=mca, N_DETECTORS=12, N_SCAS=16"

### Start up the autosave task and tell it what to do.
# Save settings every thirty seconds
create_monitor_set("auto_settings12.req", 30, "P=dxpXMAP:")

### Start the saveData task.
saveData_Init("saveData.req", "P=dxpXMAP:")

# Sleep for 20 seconds to let initialization complete and then turn on AutoApply and do Apply manually once
epicsThreadSleep(20.)
dbpf("dxpXMAP:AutoApply", "1")
dbpf("dxpXMAP:Apply", "1")
dbtr("dxpXMAP:PixelsPerRun");
