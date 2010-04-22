# Example vxWorks startup file
#Following must be added for many board support packages
#cd <full path to target bin directory>

#< cdCommands

nfsMount "ics-srv02", "/ade/epics/iocTop/R3.13.7/front_end/3-1","/bootBase"
nfsMount "ics-srv02", "/sns/ADE/home/carl/vxStats/3-2", "/startup"
nfsMount "ics-srv02", "/sns/ADE/home/carl/vxStats/3-2", "/stats"
nfsMount "ics-srv02", "/sns/ADE/home/carl/timestampRecord/1-1", "/timestamp"
#nfsMount "ics-srv02", "/ade/epics/supTop/share/R3.13.7/timestampRecord/R1-3","/timestamp"

#< ../nfsCommands

ld < /bootBase/bin/ppc603/epicsArchLd.o

epicsArchLd("/bootBase/bin/%s/iocCore");
epicsArchLd("/bootBase/bin/%s/bootBaseLib");
epicsArchLd("/bootBase/bin/%s/seq")
dbLoadDatabase("/bootBase/dbd/bootBase.dbd");

epicsArchLd("/timestamp/bin/%s/tsRecordLib");
dbLoadDatabase("/timestamp/dbd/tsRecord.dbd");

epicsArchLd("/startup/bin/%s/vxStatsLib");
dbLoadDatabase("/startup/dbd/vxStats.dbd")

#dbLoadTemplate("/startup/db/ioc.substitutions")
dbLoadRecords("/startup/db/vxStats-template.db", "IOCNAME=icsdev:ioc12")

iocInit

iosDevShow
#seq &<some snc program>
