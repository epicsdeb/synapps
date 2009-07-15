## Serial-Link Protocol
# drvAsynSerialLinkInit(myport,ioport,ioaddr,inst,datoff,clkoff,enaoff,clear)
#     myport - Asyn port name (string)
#     ioport - Asyn IO port name (string)
#     ioaddr - Asyn IO port address
#     inst   - Driver instance
#     datoff - Data chan/bit number (start/w 0)
#     clkoff - Clock chan/bit number (start/w 0)
#     enaoff - Enable chan/bit number (start/w 0)
#     clear  - Clear chans/bits on startup
#drvAsynSerialLinkInit("SLP","ATHDIO",2,0,0,1,2,1)

# Load serialLink records
#dbLoadTemplate("serialLink.substitutions")

# Load ccdTiming records
#dbLoadTemplate("ccdTiming.substitutions")
