; This program tests the roperCCD EPICS server

    fileName = 'roper_test'
    exposureTime = .3
    nimages = 100
    counts = dblarr(nimages)
    pollTime = .01
    prefix = 'roperCCD:det1:'

    autosavePV = prefix + 'AutoSave'
    startPV    = prefix + 'AcquireCLBK'
    ROIPV      = prefix + 'ROITotal'
    seqPV      = prefix + 'SeqNumber'
    filePV     = prefix + 'FileTemplate'
    timePV     = prefix + 'Seconds'

    status = caput(filePV, fileName)
    status = caput(timePV, exposureTime)
    status = caput(autosavePV, 1)
    status = caput(seqPV, 1)

    for i=0, nimages-1 do begin
        status = caput(startPV, 1)
        ; Wait for acquisition to complete
        while (1) do begin
            status = caget(startPV, busy)
            if (busy eq 0) then break
            wait, pollTime
        endwhile
        ; Read the ROI counts
        status = caget(ROIPV, temp)
        counts[i] = temp
        print, i, counts[i]
    endfor
    plot, counts, ystyle=1, psym=-1

end