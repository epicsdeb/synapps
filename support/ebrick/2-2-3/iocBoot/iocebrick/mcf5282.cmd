## QADC
dbLoadRecords("../../db/ebrQADC.db","P=ebrick:,R=uCDIMM:,C=1,I=0")
dbLoadRecords("../../db/ebrQADC.db","P=ebrick:,R=uCDIMM:,C=2,I=1")
dbLoadRecords("../../db/ebrQADC.db","P=ebrick:,R=uCDIMM:,C=3,I=2")
dbLoadRecords("../../db/ebrQADC.db","P=ebrick:,R=uCDIMM:,C=4,I=3")
dbLoadRecords("../../db/ebrQADC.db","P=ebrick:,R=uCDIMM:,C=5,I=6")
dbLoadRecords("../../db/ebrQADC.db","P=ebrick:,R=uCDIMM:,C=6,I=7")

## CPU statistics
dbLoadRecords("../../db/devCpuStats.db","P=ebrick:,R=uCDIMM:")

## PIO driver
drvAsynC5282Init("PIO")
dbLoadTemplate("ebrickPIODIG.substitutions")

## Bootstrap flash
drvCFIFlashBurnerConfigure("bootFlash",0x10000000,0,16)
dbLoadRecords("../../db/xxdevFlashBurner.db","P=ebrick:,R=uCDIMM:,INP=bootFlash")
