errlogInit(20000)

< envPaths

dbLoadDatabase("$(AREA_DETECTOR)/dbd/firewireWinDCAMApp.dbd")

firewireWinDCAMApp_registerRecordDeviceDriver(pdbbase) 

epicsEnvSet("PREFIX", "13FW1:")
epicsEnvSet("PORT",   "FW1")
epicsEnvSet("QSIZE",  "20")
epicsEnvSet("XSIZE",  "1376")
epicsEnvSet("YSIZE",  "1024")

# This is the Thorlabs camera
#WinFDC_Config("$(PORT)", "116442682213159680", 50, 200000000)

# This is the SONY camera
#WinFDC_Config("$(PORT)", "163818473825504512", 50, 200000000)

# This will use the first camera found without needing to know its ID
WinFDC_Config("$(PORT)", "", 50, 200000000)

asynSetTraceIOMask("$(PORT)",0,2)
#asynSetTraceMask("$(PORT)",0,255)

dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/ADBase.template",       "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template",       "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/firewireDCAM.template", "P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadTemplate("firewire.substitutions")

# Create a standard arrays plugin, set it to get 8-bit data from the driver.
NDStdArraysConfigure("Image1", 5, 0, "$(PORT)", 0, -1)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

# Use the following line for an 8-bit camera.  This is enough elements for 1376*1024*3, increase if needed.
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,TYPE=Int8,FTVL=UCHAR,NELEMENTS=4227072")

# Use the following line for an 12-bit or 16-bit camera.  This is enough elements for 1500x1000x1, increase if needed.
#dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=1500000")

# Create a second standard arrays plugin, set it to get 16-bit data from the driver.
NDStdArraysConfigure("Image2", 5, 0, "$(PORT)", 0, -1)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=$(PREFIX),R=image2:,PORT=Image2,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),NDARRAY_ADDR=0")

# This is enough elements for 1376*1024*3
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDStdArrays.template", "P=$(PREFIX),R=image2:,PORT=Image2,ADDR=0,TIMEOUT=1,TYPE=Int16,FTVL=SHORT,NELEMENTS=4227072")

# Load all other plugins using commonPlugins.cmd
< ../commonPlugins.cmd

#asynSetTraceMask("$(PORT)",0,255)


iocInit()

#asynSetTraceMask("$(PORT)",0,1)

# save things every thirty seconds
create_monitor_set("auto_settings.req", 30,"P=$(PREFIX),D=cam1:")
