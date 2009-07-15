/*
 * seqCommands.c,v 1.12 2005/01/20 16:49:06 anj Exp
 *
 * DESCRIPTION: EPICS sequencer commands
 *
 * Author:  Eric Norum
 *
 * Experimental Physics and Industrial Control System (EPICS)
 *
 * Initial development by:
 *    Canadian Light Source
 *    107 North Road
 *    University of Saskatchewan
 *    Saskatoon, Saskatchewan, CANADA
 *    cls.usask.ca
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <epicsThread.h>
#include <cantProceed.h>

#include <iocsh.h>
#define epicsExportSharedSymbols
#include "seq.h"


struct sequencerProgram {
    struct seqProgram *prog;
    struct sequencerProgram *next;

};
static struct sequencerProgram *seqHead;

/*
 * This routine is called before multitasking has started, so there's
 * no race condition in creating the linked list.
 */
epicsShareFunc void epicsShareAPI seqRegisterSequencerProgram (struct seqProgram *p)
{
    struct sequencerProgram *sp;

    sp = (struct sequencerProgram *)mallocMustSucceed (sizeof *sp, "seqRegisterSequencerProgram");
    sp->prog = p;
    sp->next = seqHead;
    seqHead = sp;
}

/*
 * Find a thread by name or ID number
 */
static epicsThreadId
findThread (const char *name)
{
    epicsThreadId id;
    char *term;

    id = (epicsThreadId)strtoul (name, &term, 16);
    if ((term != name) && (*term == '\0'))
        return id;
    id = epicsThreadGetId (name);
    if (id)
        return id;
    printf ("No such thread.\n");
    return NULL;
}

/* seq */
static const iocshArg seqArg0 = { "sequencer",iocshArgString};
static const iocshArg seqArg1 = { "macro definitions",iocshArgString};
static const iocshArg seqArg2 = { "stack size",iocshArgInt};
static const iocshArg * const seqArgs[3] = { &seqArg0,&seqArg1,&seqArg2 };
static const iocshFuncDef seqFuncDef = {"seq",3,seqArgs};
static void seqCallFunc(const iocshArgBuf *args)
{
    char *table = args[0].sval;
    char *macroDef = args[1].sval;
    int stackSize = args[2].ival;
    struct sequencerProgram *sp;

    if (!table) {
        printf ("No sequencer specified.\n");
        return;
    }
    if (*table == '&')
        table++;
    for (sp = seqHead ; sp != NULL ; sp = sp->next) {
        if (!strcmp (table, sp->prog->pProgName)) {
            seq (sp->prog, macroDef, stackSize);
            return;
        }
    }
    printf ("Can't find sequencer `%s'.\n", table);
}

/* seqShow */
static const iocshArg seqShowArg0 = { "sequencer",iocshArgString};
static const iocshArg * const seqShowArgs[1] = {&seqShowArg0};
static const iocshFuncDef seqShowFuncDef = {"seqShow",1,seqShowArgs};
static void seqShowCallFunc(const iocshArgBuf *args)
{
    epicsThreadId id;
    char *name = args[0].sval;

    if (name == NULL)
        seqShow (NULL);
    else if ((id = findThread (name)) != NULL)
        seqShow (id);
}

/* seqQueueShow */
static const iocshArg seqQueueShowArg0 = { "sequencer",iocshArgString};
static const iocshArg * const seqQueueShowArgs[1] = {&seqQueueShowArg0};
static const iocshFuncDef seqQueueShowFuncDef = {"seqQueueShow",1,seqQueueShowArgs};
static void seqQueueShowCallFunc(const iocshArgBuf *args)
{
    epicsThreadId id;
    char *name = args[0].sval;

    if ((name != NULL) && ((id = findThread (name)) != NULL))
        seqQueueShow (id);
}

/* seqStop */
static const iocshArg seqStopArg0 = { "sequencer",iocshArgString};
static const iocshArg * const seqStopArgs[1] = {&seqStopArg0};
static const iocshFuncDef seqStopFuncDef = {"seqStop",1,seqStopArgs};
static void seqStopCallFunc(const iocshArgBuf *args)
{
    epicsThreadId id;
    char *name = args[0].sval;

    if ((name != NULL) && ((id = findThread (name)) != NULL))
        seqStop (id);
}

/* seqChanShow */
static const iocshArg seqChanShowArg0 = { "sequencer",iocshArgString};
static const iocshArg seqChanShowArg1 = { "channel",iocshArgString};
static const iocshArg * const seqChanShowArgs[2] = {&seqChanShowArg0,&seqChanShowArg1};
static const iocshFuncDef seqChanShowFuncDef = {"seqChanShow",2,seqChanShowArgs};
static void seqChanShowCallFunc(const iocshArgBuf *args)
{
    epicsThreadId id;
    char *name = args[0].sval;
    char *chan = args[1].sval;

    if ((name != NULL) && ((id = findThread (name)) != NULL))
        seqChanShow (id, chan);
}

/* seqcar */
static const iocshArg seqcarArg0 = { "verbosity",iocshArgInt};
static const iocshArg * const seqcarArgs[1] = {&seqcarArg0};
static const iocshFuncDef seqcarFuncDef = {"seqcar",1,seqcarArgs};
static void seqcarCallFunc(const iocshArgBuf *args)
{
    seqcar (args[0].ival);
}

/*
 * This routine is called before multitasking has started, so there's
 * no race condition in the test/set of firstTime.
 */
epicsShareFunc void epicsShareAPI seqRegisterSequencerCommands (void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&seqFuncDef,seqCallFunc);
        iocshRegister(&seqShowFuncDef,seqShowCallFunc);
        iocshRegister(&seqQueueShowFuncDef,seqQueueShowCallFunc);
        iocshRegister(&seqStopFuncDef,seqStopCallFunc);
        iocshRegister(&seqChanShowFuncDef,seqChanShowCallFunc);
        iocshRegister(&seqcarFuncDef,seqcarCallFunc);
    }
}
