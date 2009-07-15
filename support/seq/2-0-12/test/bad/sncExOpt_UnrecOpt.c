/* @(#)SNC Version 1.9.3.p2: Tue Sep 7 18:05:42 PDT 1999: sncExOpt_UnrecOpt.st */


/* Event flags */
/* Program "snctest" */
#define ANSI
#include "seqCom.h"

#define NUM_SS 1
#define NUM_CHANNELS 1
#define NUM_EVENTS 0
#define NUM_QUEUES 0
#define MAX_STRING_SIZE 40
#define ASYNC_OPT FALSE
#define CONN_OPT TRUE
#define DEBUG_OPT FALSE
#define REENT_OPT FALSE


extern struct seqProgram snctest;

/* Variable declarations */
static float	v;
/* Code for state "low" in state set "ss1" */

/* Entry function for state "low" in state set "ss1" */
static void I_ss1_low(ssId, pVar)
SS_ID	ssId;
struct UserVar	*pVar;
{/* Entry 1: */
# line 13 "sncExOpt_UnrecOpt.st"
	printf("Will do this on entry") ;
# line 14 "sncExOpt_UnrecOpt.st"
	printf("Another thing to do on entry") ;
}

/* Exit function for state "low" in state set "ss1" */
static void O_ss1_low(ssId, pVar)
SS_ID	ssId;
struct UserVar	*pVar;
{/* Exit 1: */
# line 25 "sncExOpt_UnrecOpt.st"
	printf("Something to do on exit") ;
}

/* Delay function for state "low" in state set "ss1" */
static void D_ss1_low(ssId, pVar)
SS_ID	ssId;
struct UserVar	*pVar;
{
# line 19 "sncExOpt_UnrecOpt.st"
# line 22 "sncExOpt_UnrecOpt.st"
	seq_delayInit(ssId, 0, (.1));
}

/* Event function for state "low" in state set "ss1" */
static long E_ss1_low(ssId, pVar, pTransNum, pNextState)
SS_ID	ssId;
struct UserVar	*pVar;
short	*pTransNum, *pNextState;
{
# line 19 "sncExOpt_UnrecOpt.st"
	if (v > 5.0)
	{
		*pNextState = 1;
		*pTransNum = 0;
		return TRUE;
	}
# line 22 "sncExOpt_UnrecOpt.st"
	if (seq_delay(ssId, 0))
	{
		*pNextState = 0;
		*pTransNum = 1;
		return TRUE;
	}
	return FALSE;
}

/* Action function for state "low" in state set "ss1" */
static void A_ss1_low(ssId, pVar, transNum)
SS_ID	ssId;
struct UserVar	*pVar;
short	transNum;
{
	switch(transNum)
	{
	case 0:
# line 18 "sncExOpt_UnrecOpt.st"
		printf("now changing to high\n") ;
		return;
	case 1:
		return;
	}
}
/* Code for state "high" in state set "ss1" */

/* Delay function for state "high" in state set "ss1" */
static void D_ss1_high(ssId, pVar)
SS_ID	ssId;
struct UserVar	*pVar;
{
# line 35 "sncExOpt_UnrecOpt.st"
# line 38 "sncExOpt_UnrecOpt.st"
	seq_delayInit(ssId, 0, (.1));
}

/* Event function for state "high" in state set "ss1" */
static long E_ss1_high(ssId, pVar, pTransNum, pNextState)
SS_ID	ssId;
struct UserVar	*pVar;
short	*pTransNum, *pNextState;
{
# line 35 "sncExOpt_UnrecOpt.st"
	if (v <= 5.0)
	{
		*pNextState = 0;
		*pTransNum = 0;
		return TRUE;
	}
# line 38 "sncExOpt_UnrecOpt.st"
	if (seq_delay(ssId, 0))
	{
		*pNextState = 1;
		*pTransNum = 1;
		return TRUE;
	}
	return FALSE;
}

/* Action function for state "high" in state set "ss1" */
static void A_ss1_high(ssId, pVar, transNum)
SS_ID	ssId;
struct UserVar	*pVar;
short	transNum;
{
	switch(transNum)
	{
	case 0:
# line 34 "sncExOpt_UnrecOpt.st"
		printf("changing to low\n") ;
		return;
	case 1:
		return;
	}
}
/* Exit handler */
static void exit_handler(ssId, pVar)
int	ssId;
struct UserVar	*pVar;
{
}

/************************ Tables ***********************/

/* Database Blocks */
static struct seqChan seqChan[NUM_CHANNELS] = {
  {"grw:xxxExample", (void *)&v, "v", 
    "float", 1, 1, 0, 1, 0, 0, 0},

};

/* Event masks for state set ss1 */
	/* Event mask for state low: */
static bitMask	EM_ss1_low[] = {
	0x00000002,
};
	/* Event mask for state high: */
static bitMask	EM_ss1_high[] = {
	0x00000002,
};

/* State Blocks */

static struct seqState state_ss1[] = {
	/* State "low" */ {
	/* state name */       "low",
	/* action function */ (ACTION_FUNC) A_ss1_low,
	/* event function */  (EVENT_FUNC) E_ss1_low,
	/* delay function */   (DELAY_FUNC) D_ss1_low,
	/* entry function */   (ENTRY_FUNC) I_ss1_low,
	/* exit function */   (EXIT_FUNC) O_ss1_low,
	/* event mask array */ EM_ss1_low,
	/* state options */   (0