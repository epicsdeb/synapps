# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from ipApp
dbLoadDatabase("../../dbd/ip.dbd")
registerRecordDeviceDriver(pdbbase)

routerInit
localMessageRouterStart(0)

# Set up 2 serial ports
initTtyPort("serial1", "/dev/ttyS0", 9600, "N", 1, 8, "N", 1000)
initTtyPort("serial2", "/dev/ttyS1", 19200, "N", 1, 8, "N", 1000)
initSerialServer("serial1", "serial1", 1000, 20, "")
initSerialServer("serial2", "serial2", 1000, 20, "")

# Serial 1 Keithley Multimeter
dbLoadRecords("../../../ip/ipApp/Db/Keithley2kDMM_mf.db", "P=ipTest:,Dmm=DMM1,C=0,SERVER=serial1")
#dbLoadRecords("../../../ip/ipApp/Db/serial_OI_block.db", "P=ipTest:,C=0,N=1,SERVER=serial1")

iocInit

seq &Keithley2kDMM, "P=ipTest:, Dmm=DMM1, stack=10000"
