/*
 *	State definitions for detcon_lib.
 *
 *	These are returned by CCDStatus.
 */

#define	DTC_STATE_IDLE		0
#define	DTC_STATE_EXPOSING	1
#define	DTC_STATE_READING	2
#define	DTC_STATE_ERROR		3
#define	DTC_STATE_CONFIGDET	4
#define	DTC_STATE_RETRY		5
#define	DTC_STATE_TEMPCONTROL	6
