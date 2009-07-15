
# BEGIN serial.cmd ------------------------------------------------------------


# Set up ports 1-4 on Moxa box
# serial 1 connected to Keithley2K DMM at 19200 baud
#drvAsynIPPortConfigure("portName","hostInfo",priority,noAutoConnect,
#                        noProcessEos)
drvAsynIPPortConfigure("serial1", "dhcp4.cars.aps.anl.gov:4001", 0, 0, 0)
asynOctetConnect("serial1", "serial1")
asynOctetSetInputEos("serial1",0,"\r")
# asynOctetSetOutputEos(const char *portName, int addr,
#                       const char *eosin,const char *drvInfo)
asynOctetSetOutputEos("serial1",0,"\r")
#                     const char *eosin,const char *drvInfo)
asynOctetSetInputEos("serial1",0,"\r\n")
asynOctetSetOutputEos("serial1",0,"\r")
# Make port available from the iocsh command line
#asynOctetConnect(const char *entry, const char *port, int addr,
#                 int timeout, int buffer_len, const char *drvInfo)
asynOctetConnect("serial1", "serial1")

# serial 2 connected to Newport MM4000 at 38400 baud
drvAsynIPPortConfigure("serial2", "dhcp4.cars.aps.anl.gov:4002", 0, 0, 0)
asynOctetConnect("serial1", "serial1")
asynOctetSetInputEos("serial2",0,"\r")
asynOctetSetOutputEos("serial2",0,"\r")


# serial 3 is connected to the ACS MCB-4B at 9600 baud
drvAsynIPPortConfigure("serial3", "dhcp4.cars.aps.anl.gov:4003", 0, 0, 0)
asynOctetConnect("serial3", "serial3")
asynOctetSetInputEos("serial3",0,"\r")
asynOctetSetOutputEos("serial3",0,"\r")

# serial 4 not connected for now
drvAsynIPPortConfigure("serial4", "dhcp4.cars.aps.anl.gov:4004", 0, 0, 0)
asynOctetConnect("serial4", "serial4")
asynOctetSetInputEos("serial4",0,"\r")
asynOctetSetOutputEos("serial4",0,"\r")

##### Pico Motors (Ernest Williams MHATT-CAT)
##### Motors (see picMot.substitutions in same directory as this file) ####
#dbLoadTemplate("picMot.substitutions")

# Load asynRecord records on all ports
dbLoadTemplate("asynRecord.substitutions")

# Stanford Research Systems SR570 Current Preamplifier
#dbLoadRecords("$(IP)/ipApp/Db/SR570.db", "P=ipTest:,A=A1,PORT=serial1")

# Lakeshore DRC-93CA Temperature Controller
#dbLoadRecords("$(IP)/ipApp/Db/LakeShoreDRC-93CA.db", "P=ipTest:,Q=TC1,PORT=serial4")

# Huber DMC9200 DC Motor Controller
#dbLoadRecords("$(IP)/ipApp/Db/HuberDMC9200.db", "P=ipTest:,Q=DMC1:,PORT=serial5")

# Oriel 18011 Encoder Mike
#dbLoadRecords("$(IP)/ipApp/Db/eMike.db", "P=ipTest:,M=em1,PORT=serial3")

# Keithley 2000 DMM
dbLoadRecords("$(IP)/ipApp/Db/Keithley2kDMM_mf.db","P=ipTest:,Dmm=D1,PORT=serial1")

# Oxford Cyberstar X1000 Scintillation detector and pulse processing unit
#dbLoadRecords("$(IP)/ipApp/Db/Oxford_X1k.db","P=ipTest:,S=s1,PORT=serial4")

# Oxford ILM202 Cryogen Level Meter (Serial)
#dbLoadRecords("$(IP)/ipApp/Db/Oxford_ILM202.db","P=ipTest:,S=s1,PORT=serial5")

# Elcomat autocollimator
#dbLoadRecords("$(IP)/ipApp/Db/Elcomat.db", "P=ipTest:,PORT=serial8")

# Eurotherm temp controller
#dbLoadRecords("$(IP)/ipApp/Db/Eurotherm.db","P=ipTest:,PORT=serial7")

# MKS vacuum gauges
#dbLoadRecords("$(IP)/ipApp/Db/MKS.db","P=ipTest:,PORT=serial2,CC1=cc1,CC2=cc3,PR1=pr1,PR2=pr3")

# PI Digitel 500/1500 pump
#dbLoadRecords("$(IP)/ipApp/Db/Digitel.db","ipTest:,PUMP=ip1,PORT=serial3")

# PI MPC ion pump
#dbLoadRecords("$(IP)/ipApp/Db/MPC.db","P=ipTest:,PUMP=ip2,PORT=serial4,PA=0,PN=1")

# PI MPC TSP (titanium sublimation pump)
#dbLoadRecords("$(IP)/ipApp/Db/TSP.db","P=ipTest:,TSP=tsp1,PORT=serial4,PA=0")

# Heidenhain ND261 encoder (for PSL monochromator)
#dbLoadRecords("$(IP)/ipApp/Db/heidND261.db", "P=ipTest:,PORT=serial1")

# Love Controllers
#devLoveDebug=1
#loveServerDebug=1
#dbLoadRecords("$(IP)/ipApp/Db/love.db", "P=ipTest:,Q=Love_0,C=0,PORT=PORT2,ADDR=1")

# END serial.cmd --------------------------------------------------------------
