/*******************************************************************************
devScalerCamac.c
Device-support routines for scaler record for CAMAC scalers.
Current the DSP RTC-018 timer and QS-450 quad scaler are supported.  Other
CAMAC modules will be supported in the future.
  
  Author: Mark Rivers
  Revisions:
  .01  5/15/00    mlr     Created from devScaler.c by Tim Mooney
*******************************************************************************/
#include        <stdlib.h>
#include        <stdio.h>
#include        <string.h>
#include        <math.h>

#include        <devLib.h>
#include        <alarm.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <dbCommon.h>
#include        <dbScan.h>
#include        <recSup.h>
#include        <recGbl.h>
#include        <callback.h>
#include        <epicsTimer.h>
#include        <epicsExport.h>
#include        <devSup.h>
#include        <drvSup.h>
#include        <dbScan.h>
#include        <special.h>
#include        <module_types.h>
#include        <camacLib.h>
#include        "scalerRecord.h"
#include        "devScaler.h"

#ifndef vxWorks
#define ERROR -1
#define OK    0
#endif

/*** Debug support ***/
#define STATIC static

/* Define types of timers and counters */
#define RTC018 0

#define QS450 0
                           
volatile int devScalerCamacDebug=0;

STATIC long scaler_report(int level);
STATIC long scaler_init(int after);
STATIC long scaler_init_record(struct scalerRecord *psr);
STATIC long scaler_get_ioint_info(int cmd, struct dbCommon *p1, IOSCANPVT *p2);
STATIC long scaler_reset(int card);
STATIC long scaler_read(int card, long *val);
STATIC long scaler_write_preset(int card, int signal, long val);
STATIC long scaler_arm(int card, int val);
STATIC long scaler_done(int card);

SCALERDSET devScalerCamac = {
        7,
        scaler_report,
        scaler_init,
        scaler_init_record,
        scaler_get_ioint_info,
        scaler_reset,
        scaler_read,
        scaler_write_preset,
        scaler_arm,
        scaler_done
};
epicsExportAddress(dset, devScalerCamac);

STATIC int num_cards=0;
struct scaler_state {
        int card_exists;
        int num_channels;
        int card_in_use;
        IOSCANPVT ioscanpvt;
        int done;
        int timer_type;
        int branch;
        int crate;
        int timer_slot;
        int timerLam;
        int timerReadBCNA;
        int counter_type;
        int counter_slot;
        int counterLam;
        int counterReadBCNA[MAX_SCALER_CHANNELS];
        int preScale;
        int freqDiv;
        int presetCount;
        int preset[MAX_SCALER_CHANNELS];
        int overflow_counts[MAX_SCALER_CHANNELS];
        int prev_counting;
        CALLBACK *pcallback;
};
STATIC struct scaler_state **scaler_state = 0;

/**************************************************
* scaler_report()
***************************************************/
STATIC long scaler_report(int level)
{
   int card;

   if (num_cards <=0) {
      printf("    No CAMAC scaler cards found.\n");
   } else {
      for (card = 0; card < num_cards; card++) {
         if (scaler_state[card]) {
            printf("CAMAC scaler card %d\n", card);
            printf("    Timer type=%d, bcna=%x\n", 
               scaler_state[card]->timer_type,
               scaler_state[card]->timerReadBCNA);
            printf("    Counter type=%d, num_channels=%d, bcna[0]=%x\n", 
               scaler_state[card]->counter_type,
               scaler_state[card]->num_channels, 
               scaler_state[card]->counterReadBCNA[0]);
         }
      }
   }
   return (0);
}


/***************************************************
* initialize all software and hardware
* scaler_init()
****************************************************/
STATIC long scaler_init(int after)
{
   int card, i;

   if (devScalerCamacDebug >= 2) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("scaler_init(): entry, after = %d\n", after);
   }
   if (after) return(0);

   for (card=0; card<num_cards; card++) {
      for (i=0; i<MAX_SCALER_CHANNELS; i++) {
         scaler_state[card]->preset[i] = 0;
      }
   }

   return(0);
}

/***************************************************
* scaler_init_record()
****************************************************/
STATIC long scaler_init_record(struct scalerRecord *psr)
{
   int card = psr->out.value.vmeio.card;
   CALLBACK *pcallbacks;

   /* out must be an VME_IO */
   switch (psr->out.type)
   {
   case (VME_IO) : break;
   default:
      recGblRecordError(S_dev_badBus,(void *)psr,
         "devScaler (init_record) Illegal OUT Bus Type");
      return(S_dev_badBus);
   }

   if (devScalerCamacDebug >= 5) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("scaler_init_record: card %d\n", card);
   }
   if (!scaler_state[card]->card_exists)
   {
      recGblRecordError(S_dev_badCard,(void *)psr,
         "devScaler (init_record) card does not exist!");
      return(S_dev_badCard);
   }
   
   if (scaler_state[card]->card_in_use)
   {
      recGblRecordError(S_dev_badSignal,(void *)psr,
         "devScaler (init_record) card already in use!");
      return(S_dev_badSignal);
   }
   scaler_state[card]->card_in_use = 1;
   psr->nch = scaler_state[card]->num_channels;
   scaler_state[card]->prev_counting = 0;
   
   /* setup callbacks */
   pcallbacks = (CALLBACK *)psr->dpvt;
   scaler_state[card]->pcallback = (CALLBACK *)&(pcallbacks[3]);

   return(0);
}


/***************************************************
* scaler_get_ioint_info()
****************************************************/
STATIC long scaler_get_ioint_info(
   int cmd,
   struct dbCommon *prec,
   IOSCANPVT *ppvt)
{
   struct scalerRecord *psr = (struct scalerRecord *)prec;
   int card = psr->out.value.vmeio.card;
   
   if (devScalerCamacDebug >= 5) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("scaler_get_ioint_info: cmd = %d\n", cmd);
   }
   *ppvt = scaler_state[card]->ioscanpvt;
   return(0);
}

/***************************************************
* scaler_reset()
****************************************************/
STATIC long scaler_reset(int card)
{
   int i;
   
   if (devScalerCamacDebug >= 5) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("scaler_reset: card %d\n", card);
   }
   if ((card+1) > num_cards) return(ERROR);
   
   /* reset board */

   /* zero local copy of scaler presets */
   for (i=0; i<MAX_SCALER_CHANNELS; i++) {
      scaler_state[card]->preset[i] = 0;
   }

   /* clear hardware-done flag */
   scaler_state[card]->done = 0;

   return(0);
}


/***************************************************
* timerCallback()
* Callback function which is entered when a LAM is received from the timer
****************************************************/
STATIC void timerCallback(struct scaler_state *ss)
{
   if (devScalerCamacDebug >= 1) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("timerCallback, entry\n");
   }
   /* Process the record */
   callbackRequest((CALLBACK *)ss->pcallback);
}

/***************************************************
* counterCallback()
* Callback function which is entered when a LAM is received from the counter
* which happens when a channel overflows
****************************************************/
STATIC void counterCallback(struct scaler_state *ss)
{
   int i, counts, dummy, q;
   
   if (devScalerCamacDebug >= 1) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("counterCallback, entry\n");
   }
   /* There is a problem with the QS-450.  F(10)A(0) is supposed to clear the
    * overflow LAMs, but it does not, at least on my module (broken?).  The
    * only way to clear the LAM is to read and clear the overflowed channel. */
   switch(ss->counter_type) {
      case QS450:
         for (i=0; i < ss->num_channels; i++) {
            /* Check for overflow */
            cfsa(8, ss->counterReadBCNA[i], &dummy, &q);
            if (q) {
               /* Read and clear scaler. */
               cfsa(2, ss->counterReadBCNA[i], &counts, &q);
               ss->overflow_counts[i] += 16777216 + counts;
            }
         }
         break;
   }
}


/***************************************************
* scaler_read()
* return pointer to array of scaler values (on the card)
****************************************************/
STATIC long scaler_read(int card, long *val)
{
   long preset;
   int i;
   struct scaler_state *ss;
   int ticks;
   int q;
   int counting=1;
   int counts;

   if (devScalerCamacDebug >= 4) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("scaler_read: card %d\n", card);
   }
   if ((card+1) > num_cards) return(ERROR);

   ss = scaler_state[card];
   
   /* First read the timer into channel 0 */
   switch (ss->timer_type) {
      case RTC018:
         cfsa(0, ss->timerReadBCNA, &ticks, &q);
         /* Check if the timer is done counting */
         ticks = ~ticks & 0x0000ffff;  /* Ones complement, trim to 16 bits */
         if (ticks == 0) counting = 0;
         val[0] = (ss->presetCount - ticks) * ss->preScale;
         if (devScalerCamacDebug >= 4) {
            printf("%s(%d):",__FILE__,__LINE__);
            printf("scaler_read: card #%d, ticks=%d, val[0]=%ld\n", card, 
                   ticks, val[0]);
         }
         break;
   }
   switch(ss->counter_type) {
      case QS450:
         /* Note: overflow detection and correct occurs in counterCallback,
          * not necessary here */
         for (i=0; i < ss->num_channels; i++) {
            preset = ss->preset[i+1];
            cfsa(0, ss->counterReadBCNA[i], (int *) &counts, &q);
            counts += ss->overflow_counts[i];
            if ((preset > 0) && (counts >= preset)) counting = 0;
            val[i+1] = counts;
            if (devScalerCamacDebug >= 10) {
               printf("%s(%d):",__FILE__,__LINE__);
               printf("scaler_read: ...(preset %d = 0x%lx)\n", i, preset);
               printf("scaler_read: ...(chan %d = 0x%x)\n\n", i, counts);
            }
         }
         break;
   }
   
   /* If acquisition has completed issue callback request */
   if (devScalerCamacDebug >= 5) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("scaler_read: counting = %d\n", counting);
   }
   if ( (ss->prev_counting == 1) && (counting == 0)) {
      ss->done = 1;
      callbackRequest((CALLBACK *)ss->pcallback);
   }
   ss->prev_counting = counting;
   return(0);
}

/***************************************************
* scaler_write_preset()
****************************************************/
STATIC long scaler_write_preset(int card, int signal, long val)
{
   double div;
   struct scaler_state *ss;
   int n;
   
   if (devScalerCamacDebug >= 5) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("scaler_write_preset: card=%d signal=%d, val=%ld\n", 
             card, signal, val);
   }
   
   if ((card+1) > num_cards) return(ERROR);
   if ((signal+1) > MAX_SCALER_CHANNELS) return(ERROR);
   ss = scaler_state[card];

   /* save a copy in scaler_state */
   ss->preset[signal] = val;
   
   /* If this is the first channel program the timer */
   if (signal == 0) {
      switch (ss->timer_type) {
         case RTC018:
            /* We don't actually program the timer, but we compute the
               frequency divisor and preset counter */
            div = val - 1.0;
            for (n=0; n<7; n++) {
               ss->preScale = pow(8.0, n);
               if (div <= (65535.0 * ss->preScale)) break;
            }
            ss->freqDiv = 1<<n;
            ss->presetCount = (div / ss->preScale) + 0.5;
            if (devScalerCamacDebug >= 5) {
               printf("%s(%d):",__FILE__,__LINE__);
               printf("write_preset: preScale=%d, freqDiv=%d, presetCount=%d\n", 
                      ss->preScale, ss->freqDiv, ss->presetCount);
            }
      }
   }
   return(0);
}

/***************************************************
* scaler_arm()
* Make scaler ready to count.  Properly wired the scaler will
* actually start counting.
****************************************************/
STATIC long scaler_arm(int card, int val)
{
   struct scaler_state *ss;
   int i;
   int ext;
   int q;
   int ival;
   
   if (devScalerCamacDebug >= 5) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("scaler_arm: card=%d, val=%d\n", card, val);
   }
   if ((card+1) > num_cards) return(ERROR);
   ss = scaler_state[card];

   /* clear hardware-done flag */
   ss->done = 0;

   if (val) {
      /* Erase counters */
      switch (ss->counter_type) {
         case QS450:
            ival = 0;
            for (i=0; i<ss->num_channels; i++) {
               cdreg(&ext, ss->branch, ss->crate, ss->counter_slot, i);
               cfsa(9, ext, &ival, &q);
               ss->overflow_counts[i] = 0;
            }
            /* Enable LAM */
            cclm(ss->counterLam, 1);
         break;
      }
      /* Start clock */
      switch (ss->timer_type) {
         case RTC018:
            /* Program frequency divider */
            cdreg(&ext, ss->branch, ss->crate, ss->timer_slot, 1);
            cfsa(16, ext, &ss->freqDiv, &q);
            /* Enable LAM */
            cclm(ss->timerLam, 1);
            /* Program preset counter */
            cfsa(16, ss->timerReadBCNA, &ss->presetCount, &q);
            break;
      }
      
   } 
   else {
      /* Stop counting */
      switch (ss->timer_type) {
         case RTC018:
            /* Disable LAM */
            cclm(ss->timerLam, 0);
            /* If we are not currently counting do nothing.  If we are
               currently counting stop the clock.  However, we need to 
               read the current value of the clock so we can know when we
               stopped */
            cfsa(27, ss->timerReadBCNA, &ival, &q);
            if (q == 1) {  /* Clock was counting */
               /* Read preset counter */
               cfsa(0, ss->timerReadBCNA, &ival, &q);
               ival = ~ival & 0x0000ffff;  /* Ones complement, 16 bits */
               /* Decrease the preset count, which will be used to compute
                * elapsed time in read_scaler() */
               ss->presetCount -= ival;
               /* Program prescaler to minimum */
               cdreg(&ext, ss->branch, ss->crate, ss->timer_slot, 1);
               ival = 1;
               cfsa(16, ext, &ival, &q);
               /* Program preset counter */
               ival = 0;
               cfsa(16, ss->timerReadBCNA, &ival, &q);
            }
           break;
      }
      switch (ss->counter_type) {
         case QS450:
            /* Disable LAM */
            cclm(ss->counterLam, 0);
            break;
      }
   }

   return(0);
}


/***************************************************
* scaler_done()
****************************************************/
STATIC long scaler_done(int card)
{
   if ((card+1) > num_cards) return(ERROR);
   
   if (scaler_state[card]->done) {
      /* clear hardware-done flag */
      scaler_state[card]->done = 0;
      return(1);
   } else {
      return(0);
   }
}


/*****************************************************
* CAMACScalerSetup()
* User (startup file) calls this function to configure
* us for the hardware.
*****************************************************/
void CAMACScalerSetup(int max_cards)   /* maximum number of logical cards */
{
   int card;
   
   num_cards = max_cards;
   /* allocate scaler_state structures, array of pointers */
   scaler_state = (struct scaler_state **)
      calloc(1, num_cards * sizeof(struct scaler_state *));

   for (card=0; card<num_cards; card++) {
      scaler_state[card] = (struct scaler_state *)
         calloc(1, sizeof(struct scaler_state));
   }
}

/*****************************************************
* CAMACScalerConfig()
* User (startup file) calls this function to configure the hardware.
*****************************************************/
int CAMACScalerConfig(int card,       /* logical card */
   int branch,                         /* CAMAC branch */
   int crate,                          /* CAMAC crate */
   int timer_type,                     /* 0=RTC-018 */
   int timer_slot,                     /* Timer N */
   int counter_type,                   /* 0=QS-450 */
   int counter_slot)                   /* Counter N */
{
   struct scaler_state *ss;
   int i, dummy, q;
   lamParams lamParams;
   int timerExists = 0;
   int counterExists = 0;
   void *inta[2];
   
   if ((card >= num_cards) ||
       (scaler_state == NULL) ||
       (scaler_state[card] == NULL)) return(ERROR);

   ss = scaler_state[card];
   ss->branch = branch;
   ss->crate = crate;
   ss->timer_type = timer_type;
   ss->timer_slot = timer_slot;
   ss->counter_type = counter_type;
   ss->counter_slot = counter_slot;
   switch(timer_type) {
      case RTC018:
         cdreg(&ss->timerReadBCNA, branch, crate, timer_slot, 0);
         /* The RTC-018 does not have a module ID which can be read to verify
          * that the module at this address is actually an RTC-108.  Just
          * read from the preset counter to verify that there is at least some
          * module in this slot. */
         cfsa(0, ss->timerReadBCNA, &dummy, &q);
         if (q) timerExists = 1;
         if (timerExists) {
            inta[0] = &lamParams;
            inta[1] = ss;
            lamParams.a_test = 0;
            lamParams.f_test = 8;
            lamParams.a_clear = 0;
            lamParams.f_clear = 10;
            lamParams.a_enable = 0;
            lamParams.f_enable = 26;
            lamParams.mask_enable = 0;
            lamParams.a_disable = 0;
            lamParams.f_disable = 24;
            lamParams.mask_disable = 0;
            /* Initialize LAM */
            cdlam(&ss->timerLam, branch, crate, timer_slot, 0, inta);
            /* Link callback function */
            cclnk(ss->timerLam, (FUNCPTR) timerCallback);
         }
         break;
      default:
         printf("CAMACScalerSetup: invalid timer type %d\n", timer_type);
         break;
   }
   switch(counter_type) {
      case QS450:
         ss->num_channels = 4;
         for (i=0; i<scaler_state[card]->num_channels; i++) {
            cdreg(&ss->counterReadBCNA[i], branch, crate, counter_slot, i);
         }
         /* Read the module ID, make sure it is a QS-450. Should be 196 */
         cfsa(6, ss->counterReadBCNA[0], &dummy, &q);
         if ((q) && (dummy == 196)) counterExists = 1;
         if (counterExists) {
            inta[0] = &lamParams;
            inta[1] = ss;
            /* Don't use F(10)A(0) to clear LAM.  First of all this only seems
             * to clear the LAM on channel 0.  Furthermore, it prevents 
             * detection of which channel overflowed in the callback 
             * function. */
            lamParams.a_test = 0;
            lamParams.f_test = 8;
            lamParams.a_clear = 0;
            lamParams.f_clear = -1;
            lamParams.a_enable = 0;
            lamParams.f_enable = -1;
            lamParams.mask_enable = 0;
            lamParams.a_disable = 0;
            lamParams.f_disable = -1;
            lamParams.mask_disable = 0;
            /* Initialize LAM */
            cdlam(&ss->counterLam, branch, crate, counter_slot, 0, inta);
            /* Link callback function */
            cclnk(ss->counterLam, (FUNCPTR) counterCallback);
            break;
         }
      default:
         printf("CAMACScalerSetup: invalid scaler type %d\n", counter_type);
         break;
   }
   scaler_state[card]->card_exists = (timerExists && counterExists);

   if (devScalerCamacDebug >= 2) {
      printf("%s(%d):",__FILE__,__LINE__);
      printf("CAMACScalerConfig: card #%d, card_exists=%d\n", card, 
             scaler_state[card]->card_exists);
   }
   return (OK);
}
