TOP=../..
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
include $(TOP)/configure/RELEASE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this
DB += ebrSEN.db
DB += ebrDAC.db
DB += ebrSysMon.db
DB += ebrBObit.db ebrBIbit.db
DB += ebrDIOinp.db ebrDIOout.db
DB += ebrADC.db ebrATHADC.db ebrPOSADC.db
DB += serialLink.db serialLinkBC059.db
DB += ccdTiming.db ccdParam.db ccdFunc.db

DB += $(ASYN)/db/asynRecord.db
DB += $(AUTOSAVE)/asApp/Db/save_restoreStatus.db
DB += $(CALC)/calcApp/Db/userAve10.db
DB += $(CALC)/calcApp/Db/userCalcs10.db
DB += $(CALC)/calcApp/Db/userCalcOuts10.db
DB += $(CALC)/calcApp/Db/userStringCalcs10.db
DB += $(CALC)/calcApp/Db/userTransforms10.db
DB += $(SSCAN)/sscanApp/Db/scanParms.db
DB += $(SSCAN)/sscanApp/Db/scan.db
DB += $(STD)/stdApp/Db/scaler.db
DB += $(STD)/stdApp/Db/scaler16.db
DB += $(STD)/stdApp/Db/scaler32.db
DB += $(STD)/stdApp/Db/yySseq.db

DB += ebrQADC.db ebrPIO.db
DB += $(MCF5282)/mcf5282Sup/Db/devCpuStats.db
DB += $(MCF5282)/mcf5282Sup/Db/xxdevFlashBurner.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_TEMPLATE = <templatename>

include $(TOP)/configure/RULES

#----------------------------------------
#  ADD RULES AFTER THIS LINE

