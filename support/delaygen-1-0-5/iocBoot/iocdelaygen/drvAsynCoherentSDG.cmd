### Coherent SDG support

# Initialize input/output EOS
asynOctetSetOutputEos("D0",0,"\r")
asynOctetSetInputEos("D0",0,"\r")

## Coherent SDG controls
#   drvAsynCoherentSDG(myport,ioport,ioaddr)
#       myport  - Interface asyn port name (i.e. "SDG0")
#       ioport  - Comm asyn port name (i.e. "S2")
#       ioaddr  - Comm asyn port addr
#
drvAsynCoherentSDG("SDG","D0",-1);
dbLoadRecords("$(IOCDB)/drvAsynCoherentSDG.db","P=delaygen:,R=sdg:,PORT=SDG")
