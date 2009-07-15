; This program tests the roperCCD EPICS server using manual file save, like Elaine's SPEC macro

    fileName = 'roper_test2'
    exposureTime = .1
    nimages = 20
    counts = dblarr(nimages)
    pollTime = .01
    prefix = 'roperCCD:det1:'

    autosavePV = prefix + 'AutoSave'
    savePV     = prefix + 'SaveFile'
    startPV    = prefix + 'AcquireCLBK'
    computePV  = prefix + 'ComputeROICts'
    ROIPV      = prefix + 'ROITotal'
    seqPV      = prefix + 'SeqNumber'
    filePV     = prefix + 'FileTemplate'
    formatPV   = prefix + 'FilenameFormat'
    timePV     = prefix + 'Seconds'
    framesPV   = prefix + 'NumFrames'

    for i=0, nimages-1 do begin
        status = caput(framesPV, 1)
        status = caput(timePV, exposureTime)
        status = caput(autosavePV, 0)
        status = caput(computePV, 1)

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
        ; Save the file
        status = caput(seqPV, i+1)
        status = caput(filePV, fileName)
        status = caput(formatPV, '%s_%3.3d')
        status = caput(savePV, 1)
    endfor
    plot, counts, ystyle=1, psym=-1

end