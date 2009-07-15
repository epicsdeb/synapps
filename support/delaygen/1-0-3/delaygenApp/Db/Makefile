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
DB += drvDG645.db
DB += colbyPDL100A.db
DB += digitalDelay.db
DB += devDG535.db

DB += $(ASYN)/db/asynRecord.db
DB += $(AUTOSAVE)/asApp/Db/save_restoreStatus.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_TEMPLATE = <templatename>

include $(TOP)/configure/RULES

#----------------------------------------
#  ADD RULES AFTER THIS LINE

