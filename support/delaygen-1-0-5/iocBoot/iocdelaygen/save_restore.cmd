# Debug-output level
save_restoreSet_Debug(0)

# Set PV prefix
save_restoreSet_status_prefix("delaygen:")

# Ok to save/restore save sets with missing values (no CA connection to PV)?
save_restoreSet_IncompleteSetsOk(1)

# Save dated backup files?
save_restoreSet_DatedBackupFiles(1)

# Number of sequenced backup files to write
save_restoreSet_NumSeqFiles(3)

# Time interval between sequenced backups
save_restoreSet_SeqPeriodInSeconds(300)

# Specify directories to search for save files
set_savefile_path("$(STARTUP)","autosave")

# Specify directories to search for request files
set_requestfile_path($(IOCDB),"")
set_requestfile_path($(STARTUP),"")
set_requestfile_path($(AUTOSAVE),"asApp/Db")
set_requestfile_path($(IP),"ipApp/Db")
set_requestfile_path($(IPAC),"ipacApp/Db")
set_requestfile_path($(STREAM),"streamApp/Db")
set_requestfile_path($(STD),"stdApp/Db")

# Load database
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=delaygen:")
