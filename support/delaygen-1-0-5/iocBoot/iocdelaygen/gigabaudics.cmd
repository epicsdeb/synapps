## GigaBaudics digital delay
dbLoadRecords("$(IOCDB)/digitalDelay.db","P=delaygen:,Q=digDelay1:,OUTDEV=delaygen:digBit,N=4096,BITS=12")

## Digital bits mapping
dbLoadTemplate("digitalBits.substitutions")
