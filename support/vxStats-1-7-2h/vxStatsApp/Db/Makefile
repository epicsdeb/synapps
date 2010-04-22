# Makefile.Host
TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (default: NO)
#DB_OPT = YES

#----------------------------------------------------
#  Optimization needs the name of the appropriate
#  dbd file (compare with ../src/Makefile.Vx)
#DBDNAME = exampleApp.dbd

#----------------------------------------------------
# Add databases, templates, substitutions like this
#DB += dbExample1.db dbExample2.db
DB += stats.template reboot.template 
DB += ClusterLine.template
#DB += vxStats-template.db
DB += my_IOC_heartbeat.template
DB += my_IOC_heartbeat_mod.template
DB += test_his_IOC_heartbeat.template
DB += ioc.substitutions

USES_TEMPLATE += reboot.template
USES_TEMPLATE += stats.template
USES_TEMPLATE += ClusterLine.template

#----------------------------------------------------
# The following line is for local dbd expansion
#DBDEXPAND = testCrateInclude.dbd

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
