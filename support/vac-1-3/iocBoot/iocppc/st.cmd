## Example vxWorks startup file

## The following is needed if your board support package doesn't at boot time
## automatically cd to the directory containing its startup script
#cd "/home/oxygen6/MOHAN/R3.14.8/iocBoot/iocppc"

< cdCommands
#< ../nfsCommands

cd topbin
ld < vacApp.munch

## Register all support components
cd top
dbLoadDatabase("dbd/vacAppVX.dbd")
vacAppVX_registerRecordDeviceDriver(pdbbase)


cd asyn
dbLoadRecords("db/asynRecord.db", "P=MR:,R=asyn1,PORT=serial1,ADDR=0,OMAX=256, IMAX=256")

cd top
dbLoadRecords("db/vs.db", "P=MR:,GAUGE=VS1,PORT=serial1,ADDR=0,DEV=GP307,STN=0")
dbLoadRecords("db/vs.db", "P=MR:,GAUGE=VS2,PORT=serial2,ADDR=0,DEV=GP350,STN=0")

dbLoadRecords("db/vs.db", "P=MR:,GAUGE=VS3,PORT=serial3,ADDR=0,DEV=MM200,STN=3")
dbLoadRecords("db/vs.db", "P=MR:,GAUGE=VS4,PORT=serial3,ADDR=0,DEV=MM200,STN=4")

dbLoadRecords("db/digitelPump.db", "P=MR:,PUMP=IP1,PORT=serial5,ADDR=0,DEV=D500,STN=2")

dbLoadRecords("db/digitelPump.db", "P=MR:,PUMP=IP2,PORT=serial6,ADDR=5,DEV=MPC,STN=1")
dbLoadRecords("db/digitelPump.db", "P=MR:,PUMP=IP3,PORT=serial6,ADDR=5,DEV=MPC,STN=2")


#The following command is for a Greenspring VIP616-01 carrier
ipacAddVIPC616_01("0x6000,B0000000")
#The following is for a Greenspring TVME200 carrier
#ipacAddTVME200("602fb0")

#ipacReport(2)

#The following initialize a Greenspring octalUart in the second IP slot
tyGSOctalDrv(1)
tyGSOctalModuleInit("UART0","232", 0x80, 0, 0)
#tyGSOctalModuleInit("UART1","232", 0x90, 0, 1)

tyGSAsynInit("serial1",  "UART0", 0, 9600,'E',1,7,'N',"\r\n","\r\n")  
tyGSAsynInit("serial2",  "UART0", 1, 9600,'N',1,8,'N',"\r","\r")  
tyGSAsynInit("serial3",  "UART0", 2, 9600,'N',1,8,'N',"\r","\r")  

tyGSAsynInit("serial5",  "UART0", 4, 9600,'E',1,7,'N',"\n\r","\r")  
tyGSAsynInit("serial6",  "UART0", 5, 9600,'N',1,8,'N',"\r","\r")  

asynSetTraceMask("serial1",1,0x1)


cd startup
iocInit()

## Start any sequence programs
#seq &sncExample,"user=mohan"
