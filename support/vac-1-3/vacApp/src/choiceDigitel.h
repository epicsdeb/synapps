/***************************************************************
 *
 *     Perkin Elmer Digitel Record
 *     Author :                     Greg Nawrocki
 *     Date:                        3-14-94
 *    
 *     This code is based heavily upon code originally written
 *     by John Winans and modified extensively by Greg Nawrocki
 *     meant specifically for the Digitel 500
 *
 ****************************************************************
 *                        COPYRIGHT NOTIFICATION
 ****************************************************************
 *
 * THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
 * AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
 * AND IN ALL SOURCE LISTINGS OF THE CODE.
 *
 * (C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 *
 * Argonne National Laboratory (ANL), with facilities in the States of 
 * Illinois and Idaho, is owned by the United States Government, and
 * operated by the University of Chicago under provision of a contract
 * with the Department of Energy.
 *
 * Portions of this material resulted from work developed under a U.S.
 * Government contract and are subject to the following license:  For
 * a period of five years from March 30, 1993, the Government is
 * granted for itself and others acting on its behalf a paid-up,
 * nonexclusive, irrevocable worldwide license in this computer
 * software to reproduce, prepare derivative works, and perform
 * publicly and display publicly.  With the approval of DOE, this
 * period may be renewed for two additional five year periods. 
 * Following the expiration of this period or periods, the Government
 * is granted for itself and others acting on its behalf, a paid-up,
 * nonexclusive, irrevocable worldwide license in this computer
 * software to reproduce, prepare derivative works, distribute copies
 * to the public, perform publicly and display publicly, and to permit
 * others to do so.
 *
 ****************************************************************
 *                              DISCLAIMER
 ****************************************************************
 *
 * NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
 * THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
 * MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
 * LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
 * USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
 * DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
 * OWNED RIGHTS.  
 *
 ****************************************************************
 * LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
 * DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 ****************************************************************
 *
 * Modification Log:
 * -----------------
 * 3-14-94	gjn	created
 *  
 ****************************************************************/


#ifndef INC_DGchoice_h
#define INC_DGchoice 1

#define REC_DG_CHOICE_DSPL	0
#define REC_DG_CHOICE_DSPL_VOLT	0
#define REC_DG_CHOICE_DSPL_CURR 1
#define REC_DG_CHOICE_DSPL_PRES 2

#define REC_DG_CHOICE_KLCK	1
#define REC_DG_CHOICE_KLCK_UNLK 0
#define REC_DG_CHOICE_KLCK_LOCK 1

#define REC_DG_CHOICE_OPER	2
#define REC_DG_CHOICE_OPER_SBY	0
#define REC_DG_CHOICE_OPER_BUSY	1

#define REC_DG_CHOICE_SP	3
#define	REC_DG_CHOICE_SP_OFF	0
#define REC_DG_CHOICE_SP_ON	1

#define REC_DG_CHOICE_SPMODE      4
#define REC_DG_CHOICE_SPMODE_PRES 0
#define REC_DG_CHOICE_SPMODE_CURR 1

#define REC_DG_CHOICE_HVI	5
#define REC_DG_CHOICE_HVI_OFF	0
#define REC_DG_CHOICE_HVI_ON	1

#define REC_DG_CHOICE_BAKE       6
#define REC_DG_CHOICE_BAKE_OFF   0
#define REC_DG_CHOICE_BAKE_ON    1

#define REC_DG_CHOICE_BTIME        7
#define REC_DG_CHOICE_BTIME_REAL   0
#define REC_DG_CHOICE_BTIME_HEAT   1

#define REC_DG_CHOICE_PUMP	8
#define REC_DG_CHOICE_PUMP_30	0	/* 30 Liter/sec */
#define REC_DG_CHOICE_PUMP_60	1	/* 60 Liter/sec */
#define REC_DG_CHOICE_PUMP_120	2	/* 120 Liter/sec */
#define REC_DG_CHOICE_PUMP_220	3	/* 220 Liter/sec */
#define REC_DG_CHOICE_PUMP_400  4       /* 400 Liter/sec */
#define REC_DG_CHOICE_PUMP_700  5       /* 700 Liter/sec */

#define REC_DG_CHOICE_COOL      9
#define REC_DG_CHOICE_COOL_OFF  0
#define REC_DG_CHOICE_COOL_ON   1

#define REC_DG_CHOICE_HAVE     10 
#define REC_DG_CHOICE_HAVE_NO   0
#define REC_DG_CHOICE_HAVE_YES  1

#define REC_DG_CHOICE_TYPE      11
#define REC_DG_CHOICE_TYPE_500   0
#define REC_DG_CHOICE_TYPE_1500  1

/* 
 * Additional fields used in record and device support that are associated
 * with the digitel.
 */

#define MOD_DSPL 0x0001
#define MOD_KLCK 0x0002
#define MOD_MODS 0x0004
#define MOD_BAKE 0x0008
#define MOD_SETP 0x0010

#define MOD_SP1S 0x0001
#define MOD_S1HS 0x0002
#define MOD_S1MS 0x0004
#define MOD_S1VS 0x0008
#define MOD_SP2S 0x0010
#define MOD_S2HS 0x0020
#define MOD_S2MS 0x0040
#define MOD_S2VS 0x0080
#define MOD_SP3S 0x0100
#define MOD_S3HS 0x0200
#define MOD_S3MS 0x0400
#define MOD_S3VS 0x0800
#define MOD_S3BS 0x1000
#define MOD_S3TS 0x2000

#endif
