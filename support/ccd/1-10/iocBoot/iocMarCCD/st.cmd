< envPaths
dbLoadDatabase("$(CCD)/dbd/marCCDApp.dbd")
marCCDApp_registerRecordDeviceDriver(pdbbase) 
dbLoadRecords("$(CCD)/ccdApp/Db/ccd.db","P=marCCD:,C=det1:")

set_requestfile_path("./")
set_requestfile_path("$(CCD)/ccdApp/Db")
set_savefile_path("./autosave")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")
save_restoreSet_status_prefix("marCCD:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=marCCD:")

iocInit()

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30)

seq(marCCD,"P=marCCD:,C=det1:,PORT=2222")
