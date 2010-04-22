# Makefile.Host
T_A=$EPICS_HOST_ARCH
TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE


# EDM Screens to install
EDLS += IOC_Bootline.edl    IOC_Status.edl    IOC_Reboot.edl IOC_Security.edl FE-IOCs.edl
EDLS +=  IOC-Stats.edl IOC-Reboot.edl IOC_Clusters.edl
EDLS += GlobalSym_heartbeat.edl    IOC_2_IOC_heartbeat.edl



#
# SNS Color and Font maps
#    define only if EDLS is defined
#

#LISTS = colors.list fonts.list

include $(TOP)/configure/RULES.Opi
#----------------------------------------
#  ADD RULES AFTER THIS LINE


