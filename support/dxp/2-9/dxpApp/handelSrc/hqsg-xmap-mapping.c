/*
 * hqsg-xmap-mapping.c
 *
 * This code accompanies the XIA Application Note
 * "Handel Quick Start Guide: xMAP". This code should be used in conjunction
 * with the "Mapping Mode" section in the Quick Start Guide.
 *
 * Created 10/18/05 -- PJF
 *
 * Copyright (c) 2005, XIA LLC
 * All rights reserved
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include "handel.h"
#include "handel_errors.h"
#include "handel_constants.h"
#include "md_generic.h"


static void CHECK_ERROR(int status);

static int WaitForBuffer(char buf);
static int ReadBuffer(char buf, unsigned long *data);
static int SwitchBuffer(char *buf);
static int GetCurrentPixel(unsigned long *pixel);


int main(int argc, char *argv[]) 
{
  int status;
	int ignored = 0;

	char curBuffer = 'a';

	unsigned short isFull = 0;

	unsigned long curPixel  = 0;
	unsigned long bufferLen = 0;

  /* Acquisition Values */
	double nBins = 2048.0;

	/* Mapping parameters */
	double nMapPixels          = 200.0;
	double nMapPixelsPerBuffer = -1.0;
  double mapMode             = 1.0;
	double pixControl          = XIA_MAPPING_CTL_GATE;

	unsigned long *buffer = NULL;


  printf("Loading xmap.ini...\n");
  
  status = xiaInit("xmap.ini");
  CHECK_ERROR(status);

  printf("Configuring Handel logs...\n");

  /* Setup logging here */
  xiaSetLogLevel(MD_ERROR);
  xiaSetLogOutput("errors.log");

  printf("Starting system...\n");

	/* Boot hardware */
  status = xiaStartSystem();
  CHECK_ERROR(status);

  printf("Setting number of MCA channels to %0.1f...\n", nBins);

	/* Set mapping parameters. */
	status = xiaSetAcquisitionValues(-1, "number_mca_channels", (void *)&nBins);
	CHECK_ERROR(status);

  printf("Setting number of mapping pixels to %0.1f...\n", nMapPixels);

	status = xiaSetAcquisitionValues(0, "num_map_pixels", (void *)&nMapPixels);
	CHECK_ERROR(status);

  printf("Setting number of mapping pixels per buffer to %0.1f...\n",
         nMapPixelsPerBuffer);

	status = xiaSetAcquisitionValues(0, "num_map_pixels_per_buffer",
                                   (void *)&nMapPixelsPerBuffer);
	CHECK_ERROR(status);
	
  printf("Setting pixel advance mode to %0.1f...\n", pixControl);

  status = xiaSetAcquisitionValues(0, "pixel_advance_mode", (void *)&pixControl);
  CHECK_ERROR(status);

  printf("Applying acquisition values...\n");

	/* Apply the mapping parameters. */
	status = xiaBoardOperation(0, "apply", (void *)&ignored);
	CHECK_ERROR(status);

  printf("Enabling mapping mode...(firmware switching to mapping mode)\n");

  status = xiaSetAcquisitionValues(-1, "mapping_mode", (void *)&mapMode);
  CHECK_ERROR(status);

	/* Prepare the buffer we will use to read back the data from the board. */
	status = xiaGetRunData(0, "buffer_len", (void *)&bufferLen);
	CHECK_ERROR(status);

  printf("Mapping buffer length = %u...\n", bufferLen);

	buffer = (unsigned long *)malloc(bufferLen * sizeof(unsigned long));
	
	if (!buffer) {
	  return -1;
	}

  printf("Starting mapping run...\n");

	/* Start the mapping run. */
	status = xiaStartRun(-1, 0);
	CHECK_ERROR(status);

  printf("Starting main mapping loop...\n");

	/* The main loop that is described in the Quick Start Guide. */
	do {
	  status = WaitForBuffer(curBuffer);
	  CHECK_ERROR(status);

	  status = ReadBuffer(curBuffer, buffer);
	  CHECK_ERROR(status);

	  /* This is where you would ordinarily do something with the data:
	   * write it to a file, post-process it, etc.
	   */

	  status = SwitchBuffer(&curBuffer);
	  CHECK_ERROR(status);

	  status = GetCurrentPixel(&curPixel);
	  CHECK_ERROR(status);

	} while (curPixel < (unsigned long)nMapPixels);

	free(buffer);

	/* Stop the mapping run. */
	status = xiaStopRun(-1);
	CHECK_ERROR(status);

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


/**********
 * Waits for the specified buffer to fill.
 **********/
static int WaitForBuffer(char buf)
{
  int status;

  char bufString[15];

  unsigned short isFull = FALSE_;


  printf("\tWaiting for buffer '%c'...\n", buf);

  sprintf(bufString, "buffer_full_%c", buf);

  while (!isFull) {
    status = xiaGetRunData(0, bufString, (void *)&isFull);
	
    if (status != XIA_SUCCESS) {
      return status;
    }

    Sleep(1);
  }

  return XIA_SUCCESS;
}


/**********
 * Reads the requested buffer.
 **********/
static int ReadBuffer(char buf, unsigned long *data)
{
  int status;

  char bufString[9];


  printf("\tReading buffer '%c'...\n", buf);

  sprintf(bufString, "buffer_%c", buf);
  
  status = xiaGetRunData(0, bufString, (void *)data);
  
  return status;
}


/**********
 * Clears the current buffer and switches to the next buffer.
 **********/
static int SwitchBuffer(char *buf)
{
  int status;

  
  printf("\tSwitching from buffer '%c'", *buf);

  status = xiaBoardOperation(0, "buffer_done", (void *)buf);

  switch (*buf) {
  case 'a':
    *buf = 'b';
    break;
  case 'b':
    *buf = 'a';
    break;
  }

  printf(" to buffer '%c'...\n", *buf);

  return status;
}


/**********
 * Get the current mapping pixel.
 **********/
static int GetCurrentPixel(unsigned long *pixel)
{
  int status;


  status = xiaGetRunData(0, "current_pixel", (void *)pixel);
  
  printf("Current pixel = %u...\n", *pixel);

  return status;
}
