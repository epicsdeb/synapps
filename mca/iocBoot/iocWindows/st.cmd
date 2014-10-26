< envPaths
epicsEnvSet(STARTUP,$(TOP)/iocBoot/$(IOC))

dbLoadDatabase("../../dbd/mcaCanberra.dbd",0,0)
mcaCanberra_registerRecordDeviceDriver(pdbbase) 

#var aimDebug 10

# AIMConfig(portName, ethernet_address, portNumber(1 or 2), maxChans,
#           maxSignals, maxSequences, ethernetDevice)
# On Windows the ethernetDevice name is unique to each network card.  
# The simplest way to get this name is to run this startup script with the string
# provided in the distribution.  This will fail, but the driver will then print
# out a list of all the valid network device names on your system, along with
# their descriptions.  Find the one that describes the network that your
# Canberra hardware is attached to, and replace the string in the AIMConfig
# commands below with that string.

# This is my office 32-bit computer
#AIMConfig("AIM1/1", 0x59e, 1, 2048, 1, 1, "\Device\NPF_{5EDD2D78-B567-49D2-B7C3-9340BE526C25}")
#AIMConfig("AIM1/2", 0x59e, 2, 2048, 1, 1, "\Device\NPF_{5EDD2D78-B567-49D2-B7C3-9340BE526C25}")
# This is my office 64-bit computer
AIMConfig("AIM1/1", 0x59e, 1, 2048, 1, 1, "\Device\NPF_{73750E33-093D-41D2-8915-CA2078FE8E88}")
AIMConfig("AIM1/2", 0x59e, 2, 2048, 1, 1, "\Device\NPF_{73750E33-093D-41D2-8915-CA2078FE8E88}")
# This is the lab computer
#AIMConfig("AIM1/1", 0x59e, 1, 2048, 1, 1, "\Device\NPF_{864AA927-88D6-4407-96F5-A5DF9E43D684}")
#AIMConfig("AIM1/2", 0x59e, 2, 2048, 1, 1, "\Device\NPF_{864AA927-88D6-4407-96F5-A5DF9E43D684}")
# This is the lab 64-bit computer
#AIMConfig("AIM1/1", 0x59e, 1, 2048, 1, 1, "\Device\NPF_{FBAE6033-0C1B-4166-A3B4-C377270DE4A3}")
#AIMConfig("AIM1/2", 0x59e, 2, 2048, 1, 1, "\Device\NPF_{FBAE6033-0C1B-4166-A3B4-C377270DE4A3}")

dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=mcaTest:,M=aim_adc1,DTYP=asynMCA,INP=@asyn(AIM1/1 0),NCHAN=2048")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=mcaTest:,M=aim_adc2,DTYP=asynMCA,INP=@asyn(AIM1/2 0),NCHAN=2048")

#icbConfig(portName, module, ethernetAddress, icbAddress, moduleType)
#   portName to give to this asyn port
#   ethernetAddress - Ethernet address of module, low order 16 bits
#   icbAddress - rotary switch setting inside ICB module
#   moduleType
#      0 = ADC
#      1 = Amplifier
#      2 = HVPS
#      3 = TCA
#      4 = DSP
icbConfig("icbAdc1", 0x59e, 5, 0)
dbLoadRecords("$(MCA)/mcaApp/Db/icb_adc.db", "P=mcaTest:,ADC=adc1,PORT=icbAdc1")
icbConfig("icbAmp1", 0x59e, 3, 1)
dbLoadRecords("$(MCA)/mcaApp/Db/icb_amp.db", "P=mcaTest:,AMP=amp1,PORT=icbAmp1")
icbConfig("icbHvps1", 0x59e, 2, 2)
dbLoadRecords("$(MCA)/mcaApp/Db/icb_hvps.db", "P=mcaTest:,HVPS=hvps1,PORT=icbHvps1,LIMIT=1000")
icbConfig("icbTca1", 0x59e, 8, 3)
dbLoadRecords("$(MCA)/mcaApp/Db/icb_tca.db", "P=mcaTest:,TCA=tca1,MCA=aim_adc2,PORT=icbTca1")
#icbConfig("icbDsp1", 0x8058, 0, 4)
#dbLoadRecords("$(MCA)/mcaApp/Db/icbDsp.db", "P=mcaTest:,DSP=dsp1,PORT=icbDsp1")

mcaAIMShowModules

#asynSetTraceMask "AIM1/1",0,0xff
#asynSetTraceMask "icbTca1",0,0x13
#asynSetTraceMask "icbHvps1",0,0xff

< save_restore.cmd

iocInit()

# save settings every thirty seconds
create_monitor_set("auto_settings.req",30,"P=mcaTest:")

