/*
 * hqsg-xmap.c
 *
 * This code accompanies the XIA Application Note
 * "Handel Quick Start Guide: xMAP".
 *
 * Created 06/07/05 -- PJF
 *
 * Copyright (c) 2005, XIA LLC
 * All rights reserved
 *
 */

#include <stdio.h>
#include <stdlib.h>

/* For Sleep() */
#include <windows.h>

#include "handel.h"
#include "handel_errors.h"
#include "md_generic.h"


static void CHECK_ERROR(int status);

int main(int argc, char *argv[]) 
{
    int status;
	int dummy = 0;

    /* Acquisition Values */
    double pt     = 16.0;
    double thresh = 1000.0;
    double calib  = 5900.0;
	double dr     = 47200.0;

    unsigned long mcaLen = 0;

    unsigned long *mca = NULL;

    status = xiaInit("xmap_reset_std.ini");
    CHECK_ERROR(status);

    /* Setup logging here */
    xiaSetLogLevel(MD_ERROR);
    xiaSetLogOutput("errors.log");

	/* Boot hardware */
    status = xiaStartSystem();
    CHECK_ERROR(status);

	/* Configure acquisition values */
    status = xiaSetAcquisitionValues(0, "peaking_time", (void *)&pt);
    CHECK_ERROR(status);
    
    status = xiaSetAcquisitionValues(0, "trigger_threshold", (void *)&thresh);
    CHECK_ERROR(status);

    status = xiaSetAcquisitionValues(0, "calibration_energy", (void *)&calib);
    CHECK_ERROR(status);

	status = xiaSetAcquisitionValues(0, "dynamic_range", (void *)&dr);
	CHECK_ERROR(status);

	/* Apply new acquisition values */
	status = xiaBoardOperation(0, "apply", (void *)&dummy);
	CHECK_ERROR(status);

    /* Start a run w/ the MCA cleared */
    status = xiaStartRun(0, 0);
    CHECK_ERROR(status);

    Sleep((DWORD)5000);

    status = xiaStopRun(0);
    CHECK_ERROR(status);

    /* Prepare to read out MCA spectrum */
    status = xiaGetRunData(0, "mca_length", (void *)&mcaLen);
    CHECK_ERROR(status);

    /* If you don't want to dynamically allocate memory here,
     * then be sure to declare mca as an array of length 8192,
     * since that is the maximum length of the spectrum.
     */
    mca = (unsigned long *)malloc(mcaLen * sizeof(unsigned long));

    if (mca == NULL) {
	  /* Error allocating memory */
	  exit(1);
    }
    
    status = xiaGetRunData(0, "mca", (void *)mca);
    CHECK_ERROR(status);

    /* Display the spectrum, write it to a file, etc... */


    free(mca);

	status = xiaExit();
	CHECK_ERROR(status);

	return 0;
}


/********** 
 * This is just an example of how to handle error values.
 * A program of any reasonable size should
 * implement a more robust error handling mechanism.
 **********/
static void CHECK_ERROR(int status)
{
    /* XIA_SUCCESS is defined in handel_errors.h */
    if (status != XIA_SUCCESS) {
	printf("Error encountered! Status = %d\n", status);
	getch();
	exit(status);
    }
}



