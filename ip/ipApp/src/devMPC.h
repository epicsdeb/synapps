/* devMPC.h

 * Author: Mohan Ramanathan
 * Date: 29 April 1999
 * Modifications: 
 * Mark Rivers  17-Feb-2001  Added commands to support TSP and auto-restart
 * Change from enum to #define to make it easier to check signal definitions
 * in .db file against this file.  Note that the order of the GetSp* and SetSp*
 * commands must be preserved, although the absolute numbers can change.
 * Eliminated command string definition, moved into devMPC.cc
 */

#define	GetStatus           0
#define	GetPres             1
#define	GetCur              2
#define	GetVolt             3
#define GetSize             4
#define	GetSpVal12          5
#define	GetSpS12            6
#define	GetSpVal34          7
#define	GetSpS34            8
#define	GetSpVal56          9
#define	GetSpS56            10
#define	GetSpVal78          11
#define	GetSpS78            12
#define	GetAutoRestart      13
#define	GetTSPStat          14
#define	SetUnit             20
#define	SetDis              21
#define	SetSize             22
#define	SetSp12             23
#define	SetSp34             24
#define	SetSp56             25
#define	SetSp78             26
#define	SetStart            27
#define	SetStop             28
#define	SetLock             29
#define	SetUnlock           30
#define	SetAutoRestart      31
#define	SetTSPTimed         32
#define	SetTSPOff           33
#define	SetTSPFilament      34
#define	SetTSPClear         35
#define	SetTSPAutoAdv       36
#define	SetTSPContinuous    37
#define	SetTSPSublimation   38
#define	SetTSPDegas         39

#define MAX_MPC_COMMANDS 35
static char*  const DisplayStr[] = {",PRES", ",CUR", ",VOLT"};
