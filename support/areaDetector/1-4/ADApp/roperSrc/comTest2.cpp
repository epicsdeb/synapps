#include "stdafx.h"
#include "CExpSetup20.h"
#include "CDocFile40.h"

int main(int argc, void **argv) 
{
    CExpSetup20  *pExpSetup  = new(CExpSetup20);
    CDocFile40   *pDocFile   = new(CDocFile40);
    IDispatch  *pDocFileDispatch;
    VARIANT varResult;
    VARIANT varArg;
    HRESULT hr;
    short result;
    char errorMessage[256];
    int counter=0;

    try {
        VariantInit(&varArg);
        hr = CoInitialize(NULL);
        if (hr != S_OK) {
            printf("error calling CoInitialize\n");
        }
        /* Connect to the ExpSetup COM object */
        if (!pExpSetup->CreateDispatch("WinX32.ExpSetup.2")) {
            printf("error creating ExpSetup COM object\n");
        }

        /* Set the number of accumulations to 3 */
        varArg.vt = VT_I4;
        varArg.lVal = 3;
        pExpSetup->SetParam(EXP_ACCUMS, &varArg);

        /* Set the number of images to 10 */
        varArg.vt = VT_I4;
        varArg.lVal = 10;
        pExpSetup->SetParam(EXP_SEQUENTS, &varArg);

        /* Set the exposure time to 0.5 seconds */
        varArg.vt = VT_R8;
        varArg.dblVal = 0.5;
        pExpSetup->SetParam(EXP_EXPOSURE, &varArg);

        /* Start the exposure */
        pDocFileDispatch = pExpSetup->Start2(&varArg);
        pDocFile->AttachDispatch(pDocFileDispatch);
        /* Wait for acquisition to complete.  Every 0.1 seconds print out the
         * values of EXP_CSEQUENTS, EXP_CACCUMS and EXP_RUNNING */
        while (1) {
            Sleep(200);
            VariantInit(&varResult);
            varResult = pExpSetup->GetParam(EXP_CSEQUENTS, &result);
            printf("Loop=%d\n", counter++);
            printf("EXP_CSEQUENTS: vt=%d, value=%d, result=%d\n", varResult.vt, varResult.lVal, result);
            varResult = pExpSetup->GetParam(EXP_CACCUMS, &result);
            printf("EXP_CACCUMS:   vt=%d, value=%d, result=%d\n", varResult.vt, varResult.lVal, result);
            varResult = pExpSetup->GetParam(EXP_RUNNING, &result);
            printf("EXP_RUNNING:   vt=%d, value=%d, result=%d\n", varResult.vt, varResult.lVal, result);
            if (varResult.lVal == 0) break;
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
