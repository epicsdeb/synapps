#include "stdafx.h"
#include "CExpSetup20.h"
#include "CDocFile40.h"
#include "CROIRect0.h"
#define MAX_LOOPS 10

int main(int argc, void **argv) 
{
    CExpSetup20  *pExpSetup  = new(CExpSetup20);
    CDocFile40   *pDocFile   = new(CDocFile40);
    CROIRect0    *pROIRect   = new(CROIRect0);
    IDispatch  *pDocFileDispatch=NULL;
    IDispatch *pROIDispatch;
    VARIANT varResult;
    VARIANT varArg;
    HRESULT hr;
    short result;
    char errorMessage[256];
    int loop=0;
    double elapsedTime[10];
    double top, bottom, left, right;
    long binX, binY;
    DWORD start_ms;
    char fileName[80];

    try {
        VariantInit(&varArg);
        VariantInit(&varResult);
        hr = CoInitialize(NULL);
        if (hr != S_OK) {
            printf("error calling CoInitialize\n");
        }
        /* Connect to the ExpSetup COM object */
        if (!pExpSetup->CreateDispatch("WinX32.ExpSetup.2")) {
            printf("error creating ExpSetup COM object\n");
        }

        /* Set the number of accumulations to 1 */
        varArg.vt = VT_I4;
        varArg.lVal = 1;
        pExpSetup->SetParam(EXP_ACCUMS, &varArg);

        /* Set the number of images to 1 */
        varArg.vt = VT_I4;
        varArg.lVal = 1;
        pExpSetup->SetParam(EXP_SEQUENTS, &varArg);

        /* Set the exposure time to 0.01 seconds */
        varArg.vt = VT_R8;
        varArg.dblVal = 0.01;
        pExpSetup->SetParam(EXP_EXPOSURE, &varArg);

        /* Take 10 exposures without saving the files */
        for (loop=0; loop<MAX_LOOPS; loop++) {
            start_ms = GetTickCount();
            pExpSetup->Stop();
            elapsedTime[0] = (GetTickCount() - start_ms)/1000.;
            if (pDocFileDispatch) pDocFile->Close();
            elapsedTime[1] = (GetTickCount() - start_ms)/1000.;
            pDocFileDispatch = pExpSetup->Start2(&varArg);
            elapsedTime[2] = (GetTickCount() - start_ms)/1000.;
            pDocFile->AttachDispatch(pDocFileDispatch);
            elapsedTime[3] = (GetTickCount() - start_ms)/1000.;
            /* Wait for acquisition to complete */
            while (1) {
                Sleep(10);
                varResult = pExpSetup->GetParam(EXP_RUNNING, &result);
                if (varResult.lVal == 0) break;
            }
            elapsedTime[4] = (GetTickCount() - start_ms)/1000.;
            printf("\nLoop=%d\n"
                   "Time till stop acquisition:     %f\n" 
                   "Time till docFile closed:       %f\n"
                   "Time till acquisition started:  %f\n"
                   "Time till docFile attached:     %f\n"
                   "Time till acquisition complete: %f\n", 
                   loop, elapsedTime[0],elapsedTime[1],elapsedTime[2],elapsedTime[3],elapsedTime[4]);
        }
        /* Take 10 exposures with saving the files */
        for (loop=0; loop<MAX_LOOPS; loop++) {
            start_ms = GetTickCount();
            pExpSetup->Stop();
            elapsedTime[0] = (GetTickCount() - start_ms)/1000.;
            if (pDocFileDispatch) pDocFile->Close();
            elapsedTime[1] = (GetTickCount() - start_ms)/1000.;
            pDocFileDispatch = pExpSetup->Start2(&varArg);
            elapsedTime[2] = (GetTickCount() - start_ms)/1000.;
            pDocFile->AttachDispatch(pDocFileDispatch);
            elapsedTime[3] = (GetTickCount() - start_ms)/1000.;
            /* It seems like reading these parameters causes EXP_RUNNING poll to return 0? */
            /* Commenting out the first half of these polls crashes Windows! */
            varResult = pExpSetup->GetParam(EXP_REVERSE, &result);
            varResult = pExpSetup->GetParam(EXP_FLIP, &result);
            varResult = pExpSetup->GetParam(EXP_SEQUENTS, &result);
            varResult = pExpSetup->GetParam(EXP_SHUTTER_CONTROL, &result);
            varResult = pExpSetup->GetParam(EXP_TIMING_MODE, &result);
            varResult = pExpSetup->GetParam(EXP_AUTOD, &result);
            varResult = pExpSetup->GetParam(EXP_GAIN, &result);
            varResult = pExpSetup->GetParam(EXP_EXPOSURE, &result);
            varResult = pExpSetup->GetParam(EXP_ACTUAL_TEMP, &result);
            pROIDispatch = pExpSetup->GetROI(1);
            pROIRect->AttachDispatch(pROIDispatch);
            pROIRect->Get(&top, &left, &bottom, &right, &binX, &binY);
            varResult = pExpSetup->GetParam(EXP_CSEQUENTS, &result);
            varResult = pExpSetup->GetParam(EXP_CACCUMS, &result);

            /* Wait for acquisition to complete */
            while (1) {
                Sleep(10);
                varResult = pExpSetup->GetParam(EXP_RUNNING, &result);
                if (varResult.lVal == 0) break;
            }
            elapsedTime[4] = (GetTickCount() - start_ms)/1000.;
            sprintf(fileName, "C:\\Temp\\testA_%d.SPE", loop+1);
            pDocFile->SaveAs(fileName, 1);
            elapsedTime[5] = (GetTickCount() - start_ms)/1000.;
            printf("\nLoop=%d\n"
                   "Time till stop acquisition:     %f\n" 
                   "Time till docFile closed:       %f\n"
                   "Time till acquisition started:  %f\n"
                   "Time till docFile attached:     %f\n"
                   "Time till acquisition complete: %f\n" 
                   "Time till file saved:           %f\n", 
                   loop, elapsedTime[0],elapsedTime[1],elapsedTime[2],
                   elapsedTime[3],elapsedTime[4],elapsedTime[5]);
        }
    }
    catch(CException *pEx) {
        pEx->GetErrorMessage(errorMessage, sizeof(errorMessage));
        printf("Error, exception = %s\n", errorMessage);
        pEx->Delete();
        return(-1);
    }
    return(0);
}
