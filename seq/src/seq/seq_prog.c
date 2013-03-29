/*************************************************************************\
Copyright (c) 1991      The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                        Program list management
\*************************************************************************/
#include "seq.h"
#include "seq_debug.h"

/*
 * seqFindProg() - find a program in the state program list from thread id.
 */
SPROG *seqFindProg(epicsThreadId threadId)
{
    SSCB *ss = seqFindStateSet(threadId);
    return ss ? ss->sprog : NULL;
}

struct findStateSetArgs {
    SSCB *ss;
    epicsThreadId threadId;
};

static int findStateSet(SPROG *sp, void *param)
{
    struct findStateSetArgs *pargs = (struct findStateSetArgs *)param;
    unsigned nss;

    for (nss = 0; nss < sp->numSS; nss++) {
        SSCB *ss = sp->ss + nss;

        DEBUG("findStateSet trying %s[%d] ss[%d].threadId=%p\n",
            sp->progName, sp->instance, nss, ss->threadId);
        if (ss->threadId == pargs->threadId) {
            pargs->ss = ss;
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * seqFindStateSet() - find a state set in the state program list from thread id.
 */
SSCB *seqFindStateSet(epicsThreadId threadId)
{
    struct findStateSetArgs args;

    args.ss = 0;
    args.threadId = threadId;
    seqTraverseProg(findStateSet, &args);
    return args.ss;
}

struct findByNameArgs {
    SPROG *sp;
    const char *progName;
    int instance;
};

static int findByName(SPROG *sp, void *param)
{
    struct findByNameArgs *pargs = (struct findByNameArgs *)param;
    int found = strcmp(sp->progName, pargs->progName) == 0 && sp->instance == pargs->instance;
    if (found)
        pargs->sp = sp;
    return found;
}

/*
 * seqFindProgByName() - find a program in the program instance list by name
 * and instance number.
 */
epicsShareFunc SPROG *epicsShareAPI seqFindProgByName(const char *progName, int instance)
{
    struct findByNameArgs args;

    args.sp = 0;
    args.progName = progName;
    args.instance = instance;
    seqTraverseProg(findByName, &args);
    return args.sp;
}

struct traverseInstancesArgs {
    seqTraversee *func;
    void *param;
};

static int traverseInstances(SPROG **ppInstances, seqProgram *pseq, void *param)
{
    struct traverseInstancesArgs *pargs = (struct traverseInstancesArgs *)param;
    SPROG *sp;
    if (!ppInstances) return FALSE;
    foreach(sp, *ppInstances) {
        if (pargs->func(sp, pargs->param))
            return TRUE;    /* terminate traversal */
    }
    return FALSE;           /* continue traversal */
}

/*
 * seqTraverseProg() - visit all existing program instances and
 * call the specified routine or function.  Passes one parameter of
 * pointer size.
 */
void seqTraverseProg(seqTraversee * func, void *param)
{
    struct traverseInstancesArgs args;
    args.func = func;
    args.param = param;
    traverseSequencerPrograms(traverseInstances, &args);
}

static int addProg(SPROG **ppInstances, seqProgram *pseq, void *param)
{
    SPROG *sp = (SPROG *)param;

    if (!ppInstances || !pseq) {
        if (!sp->pvSys) {
            seqCreatePvSys(sp);
        }
        return FALSE;
    }
    assert(sp->pvSysName);
    /* search for an instance with the same pvSysName */
    if (!sp->pvSys) {
        SPROG *curSP;
        foreach(curSP, *ppInstances) {
            if (strcmp(sp->pvSysName, curSP->pvSysName) == 0) {
                DEBUG("Found a program instance (%p[%d]) with pvSys=%s.\n",
                    curSP, curSP->instance, curSP->pvSysName);
                sp->pvSys = curSP->pvSys;
            }
            break;
        }
    }
    /* same program name */
    if (strcmp(sp->progName, pseq->progName) == 0) {
        SPROG *curSP, *lastSP = NULL;
        int instance = -1;

        /* determine maximum instance number for this program */
        foreach(curSP, *ppInstances) {
            lastSP = curSP;
            /* check precondition */
            assert(curSP != sp);
            instance = max(curSP->instance, instance);
        }
        sp->instance = instance + 1;
        if (lastSP) {
            lastSP->next = sp;
        } else {
            *ppInstances = sp;
        }
        DEBUG("Added program %p, instance %d to instance list.\n",
            sp, sp->instance);
        if (sp->pvSys)
            return TRUE;
    }
    return FALSE;
}

/*
 * seqAddProg() - add a program to the program instance list.
 * Precondition: must not be already in the list.
 */
void seqAddProg(SPROG *sp)
{
    traverseSequencerPrograms(addProg, sp);
}

static int delProg(SPROG **ppInstances, seqProgram *pseq, void *param)
{
    SPROG *sp = (SPROG *)param;

    if (!ppInstances || !pseq)
        return FALSE;
    if (strcmp(sp->progName, pseq->progName) == 0) {
        SPROG *curSP;

        if (*ppInstances == sp) {
            *ppInstances = sp->next;
            DEBUG("Deleted program %p, instance %d from instance list.\n", sp, sp->instance);
            return TRUE;
        }
        foreach(curSP, *ppInstances) {
            if (curSP->next == sp) {
                curSP->next = sp->next;
                DEBUG("Deleted program %p, instance %d from instance list.\n", sp, sp->instance);
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*
 * seqDelProg() - delete a program from the program instance list.
 */
void seqDelProg(SPROG *sp)
{
    traverseSequencerPrograms(delProg, sp);
}
