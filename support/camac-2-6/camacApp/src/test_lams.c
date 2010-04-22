#include <vxWorks.h>
#include <camacLib.h>
#include <timers.h>
#include <stdio.h>

#define BRANCH 0 	/* Branch */
#define CRATE 0 	/* Crate */
#define TG_SLOT 10	/* Kinetic Systems 3655 Timing Generator */
#define QS450_SLOT 14	/* DSP Quad scaler */

struct callbackParam {
	int count;
	int lamId;
};

static int tg_lam, qs_lam;

void testLamCallback1(struct callbackParam *ptr);
void testLamCallback2();

void testLam1()
/* This program causes the Kinetic Systems 3655 Timing Generator in slot
   2 to raise a LAM after 5 seconds, and then tests that we can handle that LAM
*/
{
	static struct callbackParam callbackParam;
	int ext, q, data;
	/* This module has non-standard LAM processing, set up structure */
	lamParams lamParams;
	void *inta[2] = {&lamParams, &callbackParam};
	lamParams.a_test = 15;
	lamParams.f_test = 8;
	lamParams.a_clear = 12;
	lamParams.f_clear = 11;
	lamParams.a_enable = 13;
	lamParams.f_enable = 17;
	lamParams.mask_enable = 0xff;
	lamParams.a_disable = 13;
	lamParams.f_disable = 17;
	lamParams.mask_disable = 0;
	
	/* Program the TG for 1 kHz, recycle, 1 channel */
	cdreg(&ext, BRANCH, CRATE, TG_SLOT, 0);
	data = 0x43;
	cfsa(17, ext, &data, &q); /* Recycle, 10^3 Hz */
	data = 5000;
	cfsa(16, ext, &data, &q); /* 5000 counts = 5 sec */
	cdreg(&ext, BRANCH, CRATE, TG_SLOT, 9);
	data = 0;
	cfsa(24, ext, &data, &q); /* Disable Inhibit ability */

	cdlam(&tg_lam, BRANCH, CRATE, TG_SLOT, 0, inta); /* Initialize LAM */
	cclc(tg_lam);  		  /* Clear LAM */
	cclm(tg_lam, 1);		  /* Enable LAM */
	cdreg(&ext, BRANCH, CRATE, TG_SLOT, 0);
	cfsa(25, ext, &data, &q); /* Start counting */
	callbackParam.lamId = tg_lam;
	callbackParam.count =  0;
	cclnk(tg_lam, (FUNCPTR) testLamCallback1); /* Link callback routine */
}

void testLam1Again()
/* This just re-enables the TG LAM, causing the callback routine to execute
   again
*/
{
	cclm(tg_lam, 1);		  /* Enable LAM */
}

void testLamCallback1(struct callbackParam *p)
{
	printf("TG callback routine entered, count=%d, LAMid=%d\n", 
		p->count, p->lamId);
	if (p->count >= 5) {
	  printf("Disabling TG LAMs\n");
	  cclm(p->lamId, 0);
	}
	p->count++;
}

void testLam2()
{
   /* 
      This program tests LAM handling of QS450.  It agains needs non-standard
      lamParams, because it does not support enable or disable LAM.
      It prints a message whenever a LAM is recieved from the Quad Scaler in
      slot 14.  This scaler overflows about once per minute. It disables
      LAMs after 3 calls.
   */
	static struct callbackParam callbackParam;
	lamParams lamParams;
	void *inta[2] = {&lamParams, &callbackParam};
	lamParams.a_test = 0;
	lamParams.f_test = 8;
	lamParams.a_clear = 0;
	lamParams.f_clear = 10;
	lamParams.a_enable = 0;
	lamParams.f_enable = -1;
	lamParams.mask_enable = 0;
	lamParams.a_disable = 0;
	lamParams.f_disable = -1;
	lamParams.mask_disable = 0;
	

	cdlam(&qs_lam, BRANCH, CRATE, QS450_SLOT, 0, inta); /* Initialize LAM */
	callbackParam.lamId = qs_lam;
	callbackParam.count =  0;
	cclnk(qs_lam, (FUNCPTR) testLamCallback2); /* Link callback routine */
	cclm(qs_lam, 1);		  /* Enable LAM */
}

void testLam2Again()
/* This just re-enables the QS LAM, causing the callback routine to execute
   again
*/
{
	cclm(qs_lam, 1);		  /* Enable LAM */
}

void testLamCallback2(struct callbackParam *p)
{
   printf("Got LAM from QS 450 in slot 14, count=%d\n", p->count);
	if (p->count >= 1) {
	  printf("Disabling QS450 LAMs\n");
	  cclm(p->lamId, 0);
	}
	p->count++;
}
