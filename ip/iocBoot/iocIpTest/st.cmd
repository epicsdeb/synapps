< envPaths
dbLoadDatabase("../../dbd/ip.dbd")
ip_registerRecordDeviceDriver(pdbbase)


### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (You need the data catcher
# or the equivalent for that.)  This database is configured to use the
# "alldone" database (above) to figure out when motors have stopped moving
# and it's time to trigger detectors.
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db", "P=13Keithley1:,MAXPTS1=2000,MAXPTS2=200,MAXPTS3=20,MAXPTS4=10,MAXPTSH=10")

# Free-standing user string/number calculations (sCalcout records)
dbLoadRecords("$(CALC)/calcApp/Db/userStringCalcs10.db", "P=13Keithley1:")

# Free-standing user transforms (transform records)
dbLoadRecords("$(CALC)/calcApp/Db/userTransforms10.db", "P=13Keithley1:")

# Miscellaneous PV's, such as burtResult
dbLoadRecords("$(STD)/stdApp/Db/misc.db", "P=13Keithley1:")

< ../save_restore_IOCSH.cmd
save_restoreSet_status_prefix("13Keithley1:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=13Keithley1:")

iocInit

# save positions every five seconds
create_monitor_set("auto_positions.req", 5, "P=13Keithley1:")
# save other things every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=13Keithley1:")

seq &Keithley2kDMM, "P=13Keithley1:, Dmm=DMM1, channels=22, model=2700, stack=10000"

