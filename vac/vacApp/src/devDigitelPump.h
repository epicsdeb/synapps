/* devDigitelPump.h
*
*  Author: Mohan Ramanathan
*  July 2007  
*  A common one for Digitel 500, Digitel 1500 and MPC/LPC/SPC
*
*
*/


typedef enum {cmdRead, cmdControl, cmdReset} cmdType;

static const char * const readCmdString[] = 
{
    "0D", "0B", "0A", "0C", "11", "3C", "3C", "3C" ,"","", 
    "RD", "RC",  "RS1",  "RS2",  "RS3","","","","",""
};

static const char * const ctlCmdString[] = 
{
    "25", "38", "37", "45", "44", "3D"
};

static char*  const displayStr[] = { "VOLT", "CUR", "PRES" };

