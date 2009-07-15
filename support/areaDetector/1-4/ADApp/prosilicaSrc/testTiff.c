#include <stdlib.h>
#include <stdio.h>
#include "PvApi.h"
#include "ImageLib.h"

int main(int argc, char **argv)
{
    tPvFrame *pFrame;
    unsigned char *pData, data;
    int nx=100;
    int ny=200;
    int i, status;
    /* If we make our buffer bigger than it needs to be then ImageWriteTiff fails! */
    int bufferSize = nx*ny*sizeof(*pData)*10;
    /* If bufferSize is exactly correct then ImageWriteTiff does not fail */
    /* int bufferSize = nx*ny*sizeof(*pData); */
    char *fileName = "test1.tif";
    
    /* Allocate the frame */
    pFrame = (tPvFrame *)calloc(1, sizeof(tPvFrame));
    if (!pFrame) {
        printf("Error allocating frame\n");
        return(-1);
    } 
    
    /* Allocate the data */
    pData = (unsigned char *)malloc(bufferSize);
    if (!pData) {
        printf("Error allocating data\n");
        return(-1);
    } 

    /* Make up some data */
    for (i=0, data=0; i<nx*ny; i++, data++) pData[i] = data;

    /* Fill out the frame structure */
    pFrame->ImageBuffer = (void *)pData;
    pFrame->ImageBufferSize = bufferSize;
    pFrame->Width = nx;
    pFrame->Height = ny;
    pFrame->RegionX = 0;
    pFrame->RegionY = 0;
    pFrame->Format = ePvFmtMono8;
    pFrame->BitDepth = 8;
    pFrame->FrameCount = 0;
    
    /* Write the TIFF file */
    status = ImageWriteTiff(fileName, pFrame);
    if (status != 1) {
        printf("Error writing tiff file, status=%d\n", status);
        return(-1);
    } 
    
    /* Done */
    return(0);
}
