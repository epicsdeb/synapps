< envPaths
errlogInit(20000)

dbLoadDatabase("$(AREA_DETECTOR)/dbd/pilatusDetectorApp.dbd")
pilatusDetectorApp_registerRecordDeviceDriver(pdbbase) 

epicsEnvSet("PREFIX", "13PIL1:")
epicsEnvSet("PORT",   "PIL")
epicsEnvSet("QSIZE",  "20")
epicsEnvSet("XSIZE",  "487")
epicsEnvSet("YSIZE",  "195")

###
# Create the asyn port to talk to the Pilatus on port 41234.
drvAsynIPPortConfigure("camserver","gse-pilatus2:41234")
# Set the input and output terminators.
asynOctetSetInputEos("camserver", 0, "\030")
asynOctetSetOutputEos("camserver", 0, "\n")

pilatusDetectorConfig("$(PORT)", "camserver", $(XSIZE), $(YSIZE), 50, 200000000)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/ADBase.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/pilatus.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1,CAMSERVER_PORT=camserver")

# Create a standard arrays plugin
NDStdArraysConfigure("Image1", 5, 0, "$(PORT)", 0, -1)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,TYPE=Int32,FTVL=LONG,NELEMENTS=94965")

# Load all other plugins using commonPlugins.cmd
< ../commonPlugins.cmd

# Create "fastSweep" drivers for the MCA record to do on-the-fly scanning of ROI data
initFastSweep("SweepTotal1", "ROI1", 1, 2048, "TOTAL_ARRAY", "CALLBACK_PERIOD")
initFastSweep("SweepNet1",   "ROI1", 1, 2048, "NET_ARRAY",   "CALLBACK_PERIOD")
initFastSweep("SweepTotal2", "ROI2", 1, 2048, "TOTAL_ARRAY", "CALLBACK_PERIOD")
initFastSweep("SweepNet2",   "ROI2", 1, 2048, "NET_ARRAY",   "CALLBACK_PERIOD")
initFastSweep("SweepTotal3", "ROI3", 1, 2048, "TOTAL_ARRAY", "CALLBACK_PERIOD")
initFastSweep("SweepNet3",   "ROI3", 1, 2048, "NET_ARRAY",   "CALLBACK_PERIOD")
initFastSweep("SweepTotal4", "ROI4", 1, 2048, "TOTAL_ARRAY", "CALLBACK_PERIOD")
initFastSweep("SweepNet4",   "ROI4", 1, 2048, "NET_ARRAY",   "CALLBACK_PERIOD")

# Load MCA records for the fast sweep drivers
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=$(PREFIX),M=ROI1:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(SweepTotal1,0)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=$(PREFIX),M=ROI2:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(SweepTotal2,0)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=$(PREFIX),M=ROI3:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(SweepTotal3,0)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=$(PREFIX),M=ROI4:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(SweepTotal4,0)")

dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=$(PREFIX),M=ROI1:NetArray,  DTYP=asynMCA,NCHAN=2048,INP=@asyn(SweepNet1,0)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=$(PREFIX),M=ROI2:NetArray,  DTYP=asynMCA,NCHAN=2048,INP=@asyn(SweepNet2,0)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=$(PREFIX),M=ROI3:NetArray,  DTYP=asynMCA,NCHAN=2048,INP=@asyn(SweepNet3,0)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=$(PREFIX),M=ROI4:NetArray,  DTYP=asynMCA,NCHAN=2048,INP=@asyn(SweepNet4,0)")

#asynSetTraceMask("$(PORT)",0,255)
#asynSetTraceMask("$(PORT)",0,3)


iocInit()

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30,"P=$(PREFIX),D=cam1:")
