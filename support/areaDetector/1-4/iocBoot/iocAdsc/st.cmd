< envPaths
errlogInit(20000)

dbLoadDatabase("$(AREA_DETECTOR)/dbd/adscApp.dbd")
adscApp_registerRecordDeviceDriver(pdbbase) 

#
# adscConfig(const char *portName, const char *modelName)
#   portName   asyn port name
#   modelName  ADSC detector model name; must be one of "Q4", "Q4r", "Q210",
#              "Q210r", "Q270", "Q315", or "Q315r"
#
adscConfig("ADSC1","Q210r")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/ADBase.template","P=13ADSC1:,R=det1:,PORT=ADSC1,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/adsc.template","P=13ADSC1:,R=det1:,PORT=ADSC1,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template","P=13ADSC1:,R=det1:,PORT=ADSC1,ADDR=0,TIMEOUT=1")

#asynSetTraceMask("ADSC1",0,255)

set_requestfile_path("./")
set_savefile_path("./autosave")
set_requestfile_path("$(AREA_DETECTOR)/ADApp/Db")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")
save_restoreSet_status_prefix("13ADSC1:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db","P=13ADSC1:")

iocInit()

# save things every thirty seconds
create_monitor_set("auto_settings.req",30,"P=13ADSC1:,R=det1:")
