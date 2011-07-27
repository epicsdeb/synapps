# Allocate 96MB of memory temporarily so that all code loads in 32MB.
mem = malloc(1024*1024*96)

# vxWorks startup file
< cdCommands

< ../nfsCommands

cd topbin
ld < devCamacApp.munch
cd startup

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from CARSApp
dbLoadDatabase("$(TOP)/dbd/devCamac.dbd")
devCamac_registerRecordDeviceDriver(pdbbase)

# Setup the ksc2917 hardware definitions
# These are all actually the defaults, so this is not really necessary
# num_cards, addrs, ivec, irq_level
ksc2917_setup(1, 0xFF00, 0x00A0, 2)

# Initialize the CAMAC library.  Note that this is normally done automatically
# in iocInit, but we need to get the CAMAC routines working before iocInit
# because we need to initialize the DXP hardware.
camacLibInit

# Generic CAMAC record
dbLoadRecords("$(TOP)/camacApp/Db/generic_camac.db","P=xxx:,R=camac1,SIZE=2048")

### Motors
# E500 driver setup parameters: 
#     (1) maximum # of controllers, 
#     (2) maximum # axis per card
#     (3) motor task polling rate (min=1Hz, max=60Hz)
E500Setup(2, 8, 10)

# E500 driver configuration parameters: 
#     (1) controller
#     (2) branch 
#     (3) crate
#     (4) slot
E500Config(0, 0, 0, 13)
E500Config(1, 0, 0, 14)

dbLoadTemplate  "motors.template"

### Scalers: CAMAC scaler
# CAMACScalerSetup(int max_cards)   /* maximum number of logical cards */
CAMACScalerSetup(1)

# CAMACScalerConfig(int card,       /* logical card */
#  int branch,                         /* CAMAC branch */
#  int crate,                          /* CAMAC crate */
#  int timer_type,                     /* 0=RTC-018 */
#  int timer_slot,                     /* Timer N */
#  int counter_type,                   /* 0=QS-450 */
#  int counter_slot)                   /* Counter N */
CAMACScalerConfig(0, 0, 0, 0, 20, 0, 21)
dbLoadRecords("$(TOP)/camacApp/Db/CamacScaler.db","P=xxx:,S=scaler1,C=0")

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (You need the data catcher
# or the equivalent for that.)  This database is configured to use the
# "alldone" database (above) to figure out when motors have stopped moving
# and it's time to trigger detectors.
dbLoadRecords("$(STD)/stdApp/Db/scan.db","P=xxx:,MAXPTS1=2000,MAXPTS2=200,MAXPTS3=20,MAXPTS4=10,MAXPTSH=10")

# vme test record
dbLoadRecords("$(STD)/stdApp/Db/vme.db", "P=xxx:,Q=vme1")

# Miscellaneous PV's, such as burtResult
dbLoadRecords("$(STD)/stdApp/Db/misc.db","P=xxx:")

################################################################################
# Setup device/driver support addresses, interrupt vectors, etc.

# dbrestore setup
sr_restore_incomplete_sets_ok = 1
#reboot_restoreDebug=5

iocInit

#Reset the CAMAC crate - may not want to do this after things are all working
#ext = 0
#cdreg &ext, 0, 0, 1, 0
#cccz ext


### Start up the autosave task and tell it what to do.
# The task is actually named "save_restore".
# (See also, 'initHooks' above, which is the means by which the values that
# will be saved by the task we're starting here are going to be restored.

< ../requestFileCommands
#
# save positions every five seconds
create_monitor_set("auto_positions.req",5,"P=xxx:")
# save other things every thirty seconds
create_monitor_set("auto_settings.req",30,"P=xxx:")

# Free the memory we allocated at the beginning of this script
free(mem)
