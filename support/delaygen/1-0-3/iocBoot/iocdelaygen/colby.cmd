### Colby Instruments PDL100A - Programmable Delay Line

# Initialize IP Asyn support
#drvAsynIPPortConfigure("D0","colby1.aps.anl.gov:7000",0,0,0)

# Initialize IP input/output EOS
#asynOctetSetInputEos("D0",-1,"\r\n")
#asynOctetSetOutputEos("D0",-1,"\r\n")

# Initialize SERIAL input/output EOS
asynOctetSetInputEos("D0",-1,":")
asynOctetSetOutputEos("D0",-1,"\r\n")

## drvAsynColby(myport,ioport,addr,units,iface)
#       myport - Interface asyn port driver name (i.e. "COL")
#       ioport - Comm asyn port name (i.e. "D0")
#       addr   - Comm asyn port addr
#       units  - Default units string ("ps" or "ns")
#       iface  - Communication interface (0=Ethernet,1=Serial)
#
drvAsynColby("COL","D0",-1,"ns",1)

# Load databases
dbLoadRecords("../../db/colbyPDL100A.db","P=delaygen:,R=Colby:,PREC=3,A=Colby,PORT=COL")
