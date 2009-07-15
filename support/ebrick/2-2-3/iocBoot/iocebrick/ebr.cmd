## Board initialization
# drvBoardInit(addr,type)
#     addr - Board address
#     type - Board type (see docs/BOARDTYPES)
#

# Athena
#drvBoardInit(0x280,1)

# Poseidon initialization
#drvBoardInit(0x300,10)


## Digital I/O
# drvDioInit(port,type,inst,addr,regs,rate)
#     port - Asyn port name (string)
#     type - Board type (see docs/BOARDTYPES)
#     inst - Board instance
#     addr - Board address
#     regs - Register count
#     rate - Interrupt rate (in Hz)
#            =0 - Default (20Hz)
#            >0 - Specified rate
#            <0 - Disabled

# Athena
#drvDioInit("ATHDIO",1,0,0x280,3,200)
#dbLoadRecords("../../db/ebrDIOinp.db","P=ebrick:,H=ath01:,F=reg01:,D=inpb,R=0,PORT=ATHDIO")
#dbLoadRecords("../../db/ebrDIOinp.db","P=ebrick:,H=ath01:,F=reg02:,D=inpb,R=1,PORT=ATHDIO")
#dbLoadRecords("../../db/ebrDIOout.db","P=ebrick:,H=ath01:,F=reg03:,D=outb,R=2,PORT=ATHDIO")
#dbLoadTemplate("ebrickATHDIG.substitutions")

# Poseidon
#drvDioInit("POSDIO",10,0,0x300,3,200)
#dbLoadRecords("../../db/ebrDIOinp.db","P=ebrick:,H=pos01:,F=reg01:,D=inpb,R=0,PORT=POSDIO")
#dbLoadRecords("../../db/ebrDIOinp.db","P=ebrick:,H=pos01:,F=reg02:,D=inpb,R=1,PORT=POSDIO")
#dbLoadRecords("../../db/ebrDIOout.db","P=ebrick:,H=pos01:,F=reg03:,D=outb,R=2,PORT=POSDIO")
#dbLoadTemplate("ebrickPOSDIG.substitutions")

# RMM416
#drvDioInit("RMMDIO",2,1,0x380,3,200)
#dbLoadRecords("../../db/ebrDIOinp.db","P=ebrick:,H=rmm01:,F=reg01:,D=inpb,R=0,PORT=RMMDIO")
#dbLoadRecords("../../db/ebrDIOinp.db","P=ebrick:,H=rmm01:,F=reg02:,D=inpb,R=1,PORT=RMMDIO")
#dbLoadRecords("../../db/ebrDIOout.db","P=ebrick:,H=rmm01:,F=reg03:,D=outb,R=2,PORT=RMMDIO")
#dbLoadTemplate("ebrickRMMDIG.substitutions")

# PMM
#drvDioInit("PMMDIO",3,1,0x380,2,0)
#dbLoadTemplate("ebrickPMM.substitutions")

# OMM Instance
#drvDioInit("OMMDIO",4,1,0x390,3,200)
#dbLoadRecords("../../db/ebrDIOinp.db","P=ebrick:,H=omm01:,F=reg01:,D=inpb,R=0,PORT=OMMDIO")
#dbLoadRecords("../../db/ebrDIOinp.db","P=ebrick:,H=omm01:,F=reg02:,D=inpb,R=1,PORT=OMMDIO")
#dbLoadRecords("../../db/ebrDIOout.db","P=ebrick:,H=omm01:,F=reg03:,D=outb,R=2,PORT=OMMDIO")
#dbLoadTemplate("ebrickOMMDIG.substitutions")

## Analog input channels
# drvAdcInit(port,type,inst,addr,chns,pol,rate)
#     port - Asyn port name (string)
#     type - Board type (see docs/BOARDTYPES)
#     inst - Board instance
#     addr - Board address
#     chns - Channel count
#     pol  - Polarity (0=bipolar,1=unipolar)
#     rate - Interrupt rate (in Hz)
#            =0 - Default (20Hz)
#            >0 - Specified rate
#            <0 - Disabled

# Athena
#drvAdcInit("ATHADC",1,0,0x280,16,0,100)
#dbLoadTemplate("ebrickATHADC.substitutions")

# Poseidon
#drvAdcInit("POSADC",10,0,0x300,32,0,100)
#dbLoadTemplate("ebrickPOSADC.substitutions")

## Analog output channels
# drvDacInit(port,type,inst,addr,chns,pol,vlt)
#     port - Asyn port name (string)
#     type - Board type (see docs/BOARDTYPES)
#     inst - Board instance
#     addr - Board address
#     chns - Channel count
#     pol  - Polarity (0=bipolar,1=unipolar)
#     vlt  - Voltage range

# Athena
#drvDacInit("ATHDAC",1,0,0x280,4,0,10)
#dbLoadTemplate("ebrickATHDAC.substitutions")

# Poseidon
#drvDacInit("POSDAC",10,0,0x300,4,0,10)
#dbLoadTemplate("ebrickPOSDAC.substitutions")

# RMMSIG
#drvDacInit("RMMDAC",2,1,0x380,4,0,10)
#dbLoadTemplate("ebrickRMMDAC.substitutions")

# RMMDIF
#drvDacInit("RMMDIF",9,0,0x380,4,0,10)
#dbLoadTemplate("ebrickRMMDIF.substitutions")

## Sensoray's Smart A/D Model 518
# drvSenInit(port,type,inst,addr,chns,freq)
#     port - Asyn port name (string)
#     type - Board type (see docs/BOARDTYPES)
#     inst - Board instance
#     addr - Board address
#     chns - Channel count
#     freq - Sample frequency (1Hz..5Hz)
#drvSenInit("SEN",100,0,0x380,8,4)
#dbLoadTemplate("ebrickSEN.substitutions")

## System monitor
dbLoadRecords("../../db/ebrSysMon.db","P=ebrick:")
