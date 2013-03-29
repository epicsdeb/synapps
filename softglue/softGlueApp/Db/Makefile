TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

DB += softGlue_FPGAContent.db
DB += softGlue_FPGAInt.db
DB_INSTALLS += ../softGlue_SignalShow.db  ../softGlue_convenience.db
DB_INSTALLS += ../SoftGlue_2_2.hex

# optional support: extra components needed for octupole magnet power supply
DB_INSTALLS += ../SoftGlue_2_2_Octupole_0_0.hex
DB += softGlue_FPGAContent_octupole.db

# optional support: extra components needed for velocity gate
DB_INSTALLS += ../SoftGlue_2_2_1ID_Vgate_0_1.hex
DB += softGlue_FPGAContent_s1ID_Vgate.db

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
