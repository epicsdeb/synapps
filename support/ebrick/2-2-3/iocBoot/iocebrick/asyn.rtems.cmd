drvAsynSerialPortConfigure ("S1", "/dev/tty01", 0, 0, 0)
asynSetOption("S1", -1, "baud",    "9600")
asynSetOption("S1", -1, "bits",    "8")
asynSetOption("S1", -1, "parity",  "none")
asynSetOption("S1", -1, "stop",    "1")
asynSetOption("S1", -1, "clocal",  "Y")
asynSetOption("S1", -1, "crtscts", "N")

dbLoadRecords("../../db/asynRecord.db","P=ebrick:,R=ttyS1,PORT=S1,ADDR=0,OMAX=0,IMAX=0")
