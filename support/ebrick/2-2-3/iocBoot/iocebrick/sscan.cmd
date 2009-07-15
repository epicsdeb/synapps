#------------------------------------------------------------------------------
# Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches 1D
# data, but it doesn't store anything to disk.  (See 'saveData' below for that.)
dbLoadRecords("../../db/scan.db","P=ebrick:,MAXPTS1=8000,MAXPTS2=1000,MAXPTS3=10,MAXPTS4=10,MAXPTSH=8000")
