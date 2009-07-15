< envPaths
errlogInit(20000)
dbLoadDatabase("$(CCD)/dbd/roperCCDApp.dbd")
roperCCDApp_registerRecordDeviceDriver(pdbbase) 
dbLoadRecords("$(CCD)/ccdApp/Db/ccd.db","P=roperCCD:,C=det1:")

set_requestfile_path("./")
set_savefile_path("./autosave")
set_requestfile_path("$(CCD)/ccdApp/Db")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")
save_restoreSet_status_prefix("roperCCD:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=roperCCD:")

iocInit()

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30)

seq(roperCCD,"P=roperCCD:,C=det1:")
