< envPaths
errlogInit(20000)

dbLoadDatabase("$(AREA_DETECTOR)/dbd/marCCDApp.dbd")
marCCDApp_registerRecordDeviceDriver(pdbbase) 

###
# Create the asyn port to talk to the MAR on port 2222
drvAsynIPPortConfigure("marServer","gse-marccd1.cars.aps.anl.gov:2222")
# Set the input and output terminators.
asynOctetSetInputEos("marServer", 0, "\n")
asynOctetSetOutputEos("marServer", 0, "\n")
#asynSetTraceMask("marServer",0,9)
asynSetTraceIOMask("marServer",0,2)

marCCDConfig("MAR", "marServer", 20, 200000000)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/ADBase.template",  "P=13MARCCD1:,R=cam1:,PORT=MAR,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template","P=13MARCCD1:,R=cam1:,PORT=MAR,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/marCCD.template","P=13MARCCD1:,R=cam1:,PORT=MAR,ADDR=0,TIMEOUT=1,MARSERVER_PORT=marServer")

# Create a standard arrays plugin
drvNDStdArraysConfigure("MARImage", 5, 0, "MAR", 0, -1)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=13MARCCD1:,R=image1:,PORT=MARImage,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MAR,NDARRAY_ADDR=0")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDStdArrays.template", "P=13MARCCD1:,R=image1:,PORT=MARImage,ADDR=0,TIMEOUT=1,SIZE=16,FTVL=SHORT,NELEMENTS=1200000")

# Create a file saving plugin
drvNDFileConfigure("MARFile", 20, 0, "MAR", 0)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=13MARCCD1:,R=file1:,PORT=MARFile,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MAR,NDARRAY_ADDR=0")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template",      "P=13MARCCD1:,R=file1:,PORT=MARFile,ADDR=0,TIMEOUT=1")

# Create an ROI plugin
drvNDROIConfigure("MARROI", 5, 0, "MAR", 0, 10, 20, -1)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=13MARCCD1:,R=ROI1:,  PORT=MARROI,ADDR=0,TIMEOUT=1,NDARRAY_PORT=MAR,NDARRAY_ADDR=0")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROI.template",       "P=13MARCCD1:,R=ROI1:,  PORT=MARROI,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROIN.template",      "P=13MARCCD1:,R=ROI1:0:,PORT=MARROI,ADDR=0,TIMEOUT=1,HIST_SIZE=256")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROIN.template",      "P=13MARCCD1:,R=ROI1:1:,PORT=MARROI,ADDR=1,TIMEOUT=1,HIST_SIZE=256")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROIN.template",      "P=13MARCCD1:,R=ROI1:2:,PORT=MARROI,ADDR=2,TIMEOUT=1,HIST_SIZE=256")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROIN.template",      "P=13MARCCD1:,R=ROI1:3:,PORT=MARROI,ADDR=3,TIMEOUT=1,HIST_SIZE=256")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROIN.template",      "P=13MARCCD1:,R=ROI1:4:,PORT=MARROI,ADDR=3,TIMEOUT=1,HIST_SIZE=256")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROIN.template",      "P=13MARCCD1:,R=ROI1:5:,PORT=MARROI,ADDR=3,TIMEOUT=1,HIST_SIZE=256")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROIN.template",      "P=13MARCCD1:,R=ROI1:6:,PORT=MARROI,ADDR=3,TIMEOUT=1,HIST_SIZE=256")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDROIN.template",      "P=13MARCCD1:,R=ROI1:7:,PORT=MARROI,ADDR=3,TIMEOUT=1,HIST_SIZE=256")

# Create "fastSweep" drivers for the MCA record to do on-the-fly scanning of ROI data
initFastSweep("MARSweepTotal", "MARROI", 8, 2048, "TOTAL_ARRAY", "CALLBACK_PERIOD")
initFastSweep("MARSweepNet", "MARROI", 8, 2048, "NET_ARRAY", "CALLBACK_PERIOD")

# Load MCA records for the fast sweep drivers
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:0:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepTotal 0)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:1:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepTotal 1)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:2:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepTotal 2)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:3:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepTotal 3)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:4:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepTotal 4)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:5:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepTotal 5)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:6:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepTotal 6)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:7:TotalArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepTotal 7)")

dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:0:NetArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepNet 0)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:1:NetArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepNet 1)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:2:NetArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepNet 2)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:3:NetArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepNet 3)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:4:NetArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepNet 4)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:5:NetArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepNet 5)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:6:NetArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepNet 6)")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=13MARCCD1:,M=ROI1:7:NetArray,DTYP=asynMCA,NCHAN=2048,INP=@asyn(MARSweepNet 7)")


#asynSetTraceMask("MARROI",0,3)
#asynSetTraceIOMask("MARROI",0,4)

# Load scan records
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db", "P=13MARCCD1:,MAXPTS1=2000,MAXPTS2=200,MAXPTS3=20,MAXPTS4=10,MAXPTSH=10")

set_requestfile_path("./")
set_savefile_path("./autosave")
set_requestfile_path("$(AREA_DETECTOR)/ADApp/Db")
set_requestfile_path("$(SSCAN)/sscanApp/Db")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")
save_restoreSet_status_prefix("13MARCCD1:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=13MARCCD1:")

iocInit()

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30,"P=13MARCCD1:,D=cam1:")
