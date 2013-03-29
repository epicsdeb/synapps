#include <epicsExport.h>
#include <string.h>
#include <stdio.h>
#include <asynShellCommands.h>
#include <drvAsynSerialPort.h>
#include <iocsh.h>

/* There are no include files and/or they are not installed for the following 
 * definitions.  
 * Must be careful to make sure these following remain consistent with the 
 * source files
 */

char * tyGSOctalDevCreate
    (
    char *       name,           /* name to use for this device          */
    const char * moduleID,       /* IP module name                       */
    int          port,           /* port on module for this device [0-7] */
    int          rdBufSize,      /* read buffer size, in bytes           */
    int          wrtBufSize      /* write buffer size, in bytes          */
    );
	
int tyGSAsynInitBuffsize = 1000;
epicsExportAddress(int, tyGSAsynInitBuffsize);

int tyGSAsynInit(char *port, char *moduleName, int channel, int baud, char parity, 
                 int sbits, int dbits, char handshake, 
                 char *inputEos, char *outputEos)
{

    char *deviceName;
    int status;
    #define SIZE 80
    char buffer[SIZE];

    deviceName = tyGSOctalDevCreate(port, moduleName, channel, tyGSAsynInitBuffsize, 
                                tyGSAsynInitBuffsize);
    if (deviceName == NULL) {
        printf("tyGSAsynInit: error calling tyGSOctalDevCreate\n");
        return(ERROR);
    }
    status = drvAsynSerialPortConfigure(port, port, 0, 0, 0);
    if (status) {
        printf("tyGSAsynInit: error calling drvAsynSerialPortConfigure\n");
        return(ERROR);
    }

    status = asynOctetConnect(port, port, 0, 0, 128, NULL);
    if (status) {
        printf("tyGSAsynInit: error calling asynOctetConnect\n");
        return(ERROR);
    }
    status = asynOctetSetInputEos(port, 0, inputEos);
    if (status) {
        printf("tyGSAsynInit: error calling asynOctetSetInputEos\n");
        return(ERROR);
    }
    status = asynOctetSetOutputEos(port, 0, outputEos);
    if (status) {
        printf("tyGSAsynInit: error calling asynOctetSetOutputEos\n");
        return(ERROR);
    }

    /* Port options */
    sprintf(buffer, "%d", baud);
    status = asynSetOption(port, 0, "baud", buffer);
    if (status) {
        printf("tyGSAsynInit: error calling asynSetOption for baud\n");
        return(ERROR);
    }

    if (parity == 'N') strcpy(buffer,"none");
    if (parity == 'E') strcpy(buffer,"even");
    if (parity == 'O') strcpy(buffer,"odd");
    status = asynSetOption(port, 0, "parity", buffer);
    if (status) {
        printf("tyGSAsynInit: error calling asynSetOption for parity\n");
        return(ERROR);
    }

    sprintf(buffer, "%d", sbits);
    status = asynSetOption(port, 0, "stop", buffer);
    if (status) {
        printf("tyGSAsynInit: error calling asynSetOption for stop bits\n");
        return(ERROR);
    }

    sprintf(buffer, "%d", dbits);
    status = asynSetOption(port, 0, "bits", buffer);
    if (status) {
        printf("tyGSAsynInit: error calling asynSetOption for data bits\n");
        return(ERROR);
    }

    if (handshake == 'H') strcpy(buffer,"N");
    if (handshake == 'N') strcpy(buffer,"Y");
    status = asynSetOption(port, 0, "clocal", buffer);
    if (status) {
        printf("tyGSAsynInit: error calling asynSetOption for handshake\n");
        return(ERROR);
    }
printf("tyGSAsynInit returning %d\n", status);
    return(status);
}


static const iocshArg tyGSAsynInitArg0 = { "portName",iocshArgString};
static const iocshArg tyGSAsynInitArg1 = { "module name",iocshArgString};
static const iocshArg tyGSAsynInitArg2 = { "module channel",iocshArgInt};
static const iocshArg tyGSAsynInitArg3 = { "baud",iocshArgInt};
static const iocshArg tyGSAsynInitArg4 = { "parity('N'=none,'E'=even,'O'=odd)",
                                          iocshArgInt};
static const iocshArg tyGSAsynInitArg5 = { "stop bits(1 or 2)",iocshArgInt};
static const iocshArg tyGSAsynInitArg6 = { "data bits(5-8)",iocshArgInt};
static const iocshArg tyGSAsynInitArg7 = { "handshake('N'=none,'H'=hardware)",
                                          iocshArgInt};
static const iocshArg tyGSAsynInitArg8 = { "Input EOS",iocshArgString};
static const iocshArg tyGSAsynInitArg9 = { "Output EOS",iocshArgString};
static const iocshArg * const tyGSAsynInitArgs[910] = {
    &tyGSAsynInitArg0,&tyGSAsynInitArg1,&tyGSAsynInitArg2,
    &tyGSAsynInitArg3,&tyGSAsynInitArg4,&tyGSAsynInitArg5,
    &tyGSAsynInitArg6,&tyGSAsynInitArg7,&tyGSAsynInitArg8,
    &tyGSAsynInitArg9};
static const iocshFuncDef tyGSAsynInitFuncDef = {
    "tyGSAsynInit",10,tyGSAsynInitArgs};
static void tyGSAsynInitCallFunc(const iocshArgBuf *args)
{
    tyGSAsynInit(
        args[0].sval,args[1].sval,args[2].ival,args[3].ival,
        args[4].ival,args[5].ival,args[6].ival,args[7].ival,
        args[8].sval,args[9].sval);
}

static void tyGSAsynInitRegister(void)
{
    iocshRegister(&tyGSAsynInitFuncDef,tyGSAsynInitCallFunc);
}
epicsExportRegistrar(tyGSAsynInitRegister);

