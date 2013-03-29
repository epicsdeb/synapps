ld < ../../bin/vxWorks-68040/demoLibrary.munch
dbLoadDatabase "../../dbd/cmdButtons.dbd"
cmdButtons_registerRecordDeviceDriver(pdbbase)
dbLoadRecords "cmdButtons.db"
iocInit
seq &seqCmdBtns
