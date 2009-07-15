< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from ipApp
dbLoadDatabase("../../dbd/ip.dbd")
ip_registerRecordDeviceDriver(pdbbase)

# Load IP serial port stuff
< ip_serial.cmd

# Load local serial port stuff
< local_serial.cmd

iocInit

seq &Keithley2kDMM, "P=ipTest:, Dmm=DMM1, stack=10000"
