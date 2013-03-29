/*************************************************************************\
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/

/**************************************************************************
 * Brief     : Demonstrate responding to various user commands buttons.
 *
 * Abstract  : Using bo records, we can respond and reset various buttons
 *             provided to a user.  This technique also allows for the number
 *             of commands to change with little effort and easy debug.  
 *
 * Parameters: P - pv prefix.
 *
 * Author    : Wesley Moore (2012)
 *************************************************************************/

program seqCmdBtns ("P=cmdBtns:")

option -c;  /* don't wait on pv connections. */
option +r;  /* allow multiple instances.     */

/* Buttons (bi/bo records)          */
/*   startBtn: 0 = ready, 1 = start */
/*   stopBtn: 0 = ready, 1 = stop */
short startBtn;  assign startBtn to "{P}start";  monitor startBtn;
short stopBtn;   assign stopBtn to  "{P}stop";   monitor stopBtn;

/* stringout to display status or progress. */
string seqMsg;  assign seqMsg to "{P}seqMsg";

/* basic counter (local variable) */
int ix;

ss seqCmdBtns {

  state init {
    when (pvConnectCount() == pvChannelCount()) {
        printf("seqCmdBtns: All channels connected.\n");
        sprintf(seqMsg, "Ready");
        pvPut(seqMsg);
    } state idle
  }
  
  state idle {
    when (pvConnectCount() < pvChannelCount() ) {
        printf("seqCmdBtns: Lost pv connection.\n");
        sprintf(seqMsg, "Lost connection");
        pvPut(seqMsg);
    } state init

    /* Jump to another state or put logic here.  In this case, both. */
    when (startBtn) {
        sprintf(seqMsg, "0 s");
        pvPut(seqMsg);
    } state start
    
    /* Catch bogus stop. Otherwise next "start" will stop unexpectly. */
    when (stopBtn) {
        stopBtn = 0;
        pvPut(stopBtn);
    } state idle

    /* Add other when() statements as more command buttons are added. */
  }

  state start {
    /* Start counter, watch for stop to occur. */
    when (startBtn) {
        for (ix=1; ix<=10; ix++) {
            if (stopBtn) {
                stopBtn = 0;
                pvPut(stopBtn);
                break;
            } else {
                epicsThreadSleep(1);
                sprintf(seqMsg, "%d s", ix);
                pvPut(seqMsg);
                
            }
        }

        /* make sure to reset the button! */
        startBtn = 0;
        pvPut(startBtn);
    } state idle
  }
}
