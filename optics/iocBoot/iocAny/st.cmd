# Linux startup script

< envPaths

# save_restore.cmd needs the full path to the startup directory, which
# envPaths currently does not provide
epicsEnvSet(STARTUP,$(TOP)/iocBoot/$(IOC))

# Specify largest array CA will transport
# Note for N sscanRecord data points, need (N+1)*8 bytes, else MEDM
# plot doesn't display
epicsEnvSet EPICS_CA_MAX_ARRAY_BYTES 64008

################################################################################
# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in the software we just loaded (xxx.munch)
dbLoadDatabase("../../dbd/iocxxxLinux.dbd")
iocxxxLinux_registerRecordDeviceDriver(pdbbase)

### Slits
#dbLoadRecords("$(OPTICS)/opticsApp/Db/2slit.db","P=xxx:,SLIT=Slit1V,mXp=m3,mXn=m4")
#dbLoadRecords("$(OPTICS)/opticsApp/Db/2slit.db","P=xxx:,SLIT=Slit1H,mXp=m5,mXn=m6")

# X-ray Instrumentation Associates Huber Slit Controller
# supported by a customized version of the SNL program written by Pete Jemian
#dbLoadRecords("$(OPTICS)/opticsApp/Db/xia_slit.db", "P=xxx:, HSC=hsc1:")
#dbLoadRecords("$(OPTICS)/opticsApp/Db/xia_slit.db", "P=xxx:, HSC=hsc2:")
#dbLoadRecords("$(IP)/ipApp/Db/generic_serial.db", "P=xxx:,C=0,SERVER=serial7")


### 2-post mirror
#dbLoadRecords("$(OPTICS)/opticsApp/Db/2postMirror.db","P=xxx:,Q=M1,mDn=m1,mUp=m2,LENGTH=0.3")

### User filters
#dbLoadRecords("$(OPTICS)/opticsApp/Db/filterMotor.db","P=xxx:,Q=fltr1:,MOTOR=m1,LOCK=fltr_1_2:")
#dbLoadRecords("$(OPTICS)/opticsApp/Db/filterMotor.db","P=xxx:,Q=fltr2:,MOTOR=m2,LOCK=fltr_1_2:")
#dbLoadRecords("$(OPTICS)/opticsApp/Db/filterLock.db","P=xxx:,Q=fltr2:,LOCK=fltr_1_2:,LOCK_PV=xxx:DAC1_1.VAL")

### Optical tables
#tableRecordDebug=1
#dbLoadRecords("$(OPTICS)/opticsApp/Db/table.db","P=xxx:,Q=Table1,T=table1,M0X=m1,M0Y=m2,M1Y=m3,M2X=m4,M2Y=m5,M2Z=m6,GEOM=SRI")

### Monochromator support ###
# Kohzu and PSL monochromators: Bragg and theta/Y/Z motors
# standard geometry (geometry 1)
#dbLoadRecords("$(OPTICS)/opticsApp/Db/kohzuSeq.db","P=xxx:,M_THETA=m1,M_Y=m2,M_Z=m3,yOffLo=17.4999,yOffHi=17.5001")
# modified geometry (geometry 2)
#dbLoadRecords("$(OPTICS)/opticsApp/Db/kohzuSeq.db","P=xxx:,M_THETA=m1,M_Y=m2,M_Z=m2,yOffLo=4,yOffHi=36")

# Spherical grating monochromator
dbLoadRecords("$(OPTICS)/opticsApp/Db/SGM.db","P=xxx:,N=1,M_x=m7,M_rIn=m6,M_rOut=m8,M_g=m9")

# 4-bounce high-resolution monochromator
#dbLoadRecords("$(OPTICS)/opticsApp/Db/hrSeq.db","P=xxx:,N=1,M_PHI1=m1,M_PHI2=m2")

### Orientation matrix, four-circle diffractometer (see seq program 'orient' below)
#dbLoadRecords("$(OPTICS)/opticsApp/Db/orient.db", "P=xxx:,O=1,PREC=4")
#dbLoadTemplate("orient_xtals.substitutions")

# PID-based feedback
#dbLoadTemplate "fb_epid.substitutions"

### Load database records for dual PF4 filters
#dbLoadRecords("$(OPTICS)/opticsApp/Db/pf4common.db","P=xxx:,H=pf4:,A=A,B=B")
#dbLoadRecords("$(OPTICS)/opticsApp/Db/pf4bank.db","P=xxx:,H=pf4:,B=A")
#dbLoadRecords("$(OPTICS)/opticsApp/Db/pf4bank.db","P=xxx:,H=pf4:,B=B")

### Load database records for alternative PF4-filter support
dbLoadTemplate "filter.substitutions"

###############################################################################
iocInit

### startup State Notation Language programs
#seq &kohzuCtl, "P=xxx:, M_THETA=m1, M_Y=m2, M_Z=m3, GEOM=1, logfile=kohzuCtl.log"
#seq &hrCtl, "P=xxx:, N=1, M_PHI1=m1, M_PHI2=m2, logfile=hrCtl1.log"

# X-ray Instrumentation Associates Huber Slit Controller
# supported by a SNL program written by Pete Jemian and modified (TMM) for use with the
# sscan record
#seq  &xia_slit, "name=hsc1, P=xxx:, HSC=hsc1:, S=xxx:seriala[6]"

# Orientation-matrix
#seq &orient, "P=xxx:orient1:,PM=xxx:,mTTH=m9,mTH=m10,mCHI=m11,mPHI=m12"

# Start PF4 filter sequence program
#seq pf4,"name=pf1,P=xxx:,H=pf4:,B=A,M=xxx:BraggEAO,B1=xxx:Unidig1Bo3,B2=xxx:Unidig1Bo4,B3=xxx:Unidig1Bo5,B4=xxx:Unidig1Bo6"
#seq pf4,"name=pf2,P=xxx:,H=pf4:,B=B,M=xxx:BraggEAO,B1=xxx:Unidig1Bo7,B2=xxx:Unidig1Bo8,B3=xxx:Unidig1Bo9,B4=xxx:Unidig1Bo10"

# Alternative pf4 filter seq program
seq &filterDrive,"NAME=filterDrive,P=xxx:,R=filter:,NUM_FILTERS=16"

dbcar(0,1)
