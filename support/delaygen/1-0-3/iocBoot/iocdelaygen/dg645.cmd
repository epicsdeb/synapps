### Stanford Research Systems (SRS) DG645

# Initialize IP Asyn support
#drvAsynIPPortConfigure("D0","164.54.52.182:5024",0,0,0)

# Initialize input/output EOS
asynOctetSetOutputEos("D0",0,"\n")
asynOctetSetInputEos("D0",0,"\r\n")

## drvAsynDG645(myport,ioport,ioaddr)
#       myport  - Interface asyn port name (i.e. "DG0")
#       ioport  - Comm asyn port name (i.e. "L2")
#       ioaddr  - Comm asyn port addr
#
drvAsynDG645("DG0","D0",-1);

# Load database
dbLoadRecords("../../db/drvDG645.db","P=delaygen:,R=DG0:,PORT=DG0")
