< envPaths

###
# Load the EPICS database file
dbLoadDatabase("$(PILATUS)/dbd/pilatus.dbd")
pilatus_registerRecordDeviceDriver(pdbbase)

###
# Create the asyn port to talk to the Pilatus on port 41234.
drvAsynIPPortConfigure("pilatus","gse-pilatus1:41234")
# Set the input and output terminators.
asynOctetSetInputEos("pilatus", 0, "\030")
asynOctetSetOutputEos("pilatus", 0, "\n")
# Define the environment variable pointing to stream protocol files.
epicsEnvSet("STREAM_PROTOCOL_PATH", "$(PILATUS)/pilatusApp/Db")

###
# Specify where save files should be
set_savefile_path(".", "autosave")

###
# Specify what save files should be restored.  Note these files must be
# in the directory specified in set_savefile_path(), or, if that function
# has not been called, from the directory current when iocInit is invoked
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")

###
# Specify directories in which to to search for included request files
set_requestfile_path("./")
set_requestfile_path("$(AUTOSAVE)", "asApp/Db")
set_requestfile_path("$(CALC)",     "calcApp/Db")
set_requestfile_path("$(SSCAN)",    "sscanApp/Db")
set_requestfile_path("$(PILATUS)",  "pilatusApp/Db")

###
# Load the save/restore status PVs
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=PILATUS:")

###
# Load the substitutions for for this IOC
dbLoadTemplate("PILATUS_all.subs")
# Load an asyn record for debugging
dbLoadRecords("$(ASYN)/db/asynRecord.db", "P=PILATUS:,R=asyn1,PORT=pilatus,ADDR=0,IMAX=80,OMAX=80")

# Load sscan records for scanning
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db", "P=PILATUS:,MAXPTS1=2000,MAXPTS2=200,MAXPTS3=20,MAXPTS4=10,MAXPTSH=2048")

###
# Set debugging flags if desired
#asynSetTraceIOMask("pilatus",0,2)
#asynSetTraceMask("pilatus",0,3)

###
# Start the IOC
iocInit

###
# Save settings every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=PILATUS:")

###
# Start the SNL program
seq(pilatusROIs, "DET=PILATUS:, PORT=pilatus, NROIS=16")

