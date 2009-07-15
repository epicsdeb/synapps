/*
 *	dset.h -- Device Support Entry Table structures & function prototypes
 *
 */

/* xxxxRead(): returns: (-1,0,2)=>(failure, success, success no convert) */
/*	if convert then raw value stored in rval */
/* xxxWrite() returns: (-1,0)=>(failure, success) */
/* xxxInitRecord(): returns: (-1,0)=>(failure, success)  */

#define	OK_CONVERT	0
#define	OK_NO_CONVERT	2
#define	FAILURE		-1


#ifdef	INCaiRecordh
typedef void	aiDevReport (struct aiRecord *pRec);
typedef long	aiInit (struct aiRecord *pRec);
typedef long	aiInitRecord (struct aiRecord *pRec);
typedef long	aiGetIoIntInfo (struct aiRecord *pRec);
typedef long	aiRead (struct aiRecord *pRec);
typedef long	aiSpecialLinConv (struct aiRecord *pRec);

/* ai (analog input) dset */
struct aidset {
	long			number;
	aiDevReport		*dev_report;
	aiInit			*init;
	aiInitRecord		*init_record;
	aiGetIoIntInfo		*get_ioint_info;
	aiRead			*read;
	aiSpecialLinConv	*special_linconv;
};
#endif	/* INCaiRecordh */

#ifdef	INCaoRecordh
typedef void	aoDevReport (struct aoRecord *pRec);
typedef long	aoInit (struct aoRecord *pRec);
typedef long	aoInitRecord (struct aoRecord *pRec);
typedef long	aoGetIoIntInfo (struct aoRecord *pRec);
typedef long	aoWrite (struct aoRecord *pRec);
typedef long	aoSpecialLinConv (struct aoRecord *pRec);

/* ao (analog output) dset */
struct aodset {
	long			number;
	aoDevReport		*dev_report;
	aoInit			*init;
	aoInitRecord		*init_record;
	aoGetIoIntInfo		*get_ioint_info;
	aoWrite			*write;
	aoSpecialLinConv	*special_linconv;
};
#endif	/* INCaoRecordh */

#ifdef	INCbiRecordh
typedef void	biDevReport (struct biRecord *pRec);
typedef long	biInit (struct biRecord *pRec);
typedef long	biInitRecord (struct biRecord *pRec);
typedef long	biGetIoIntInfo (struct biRecord *pRec);
typedef long	biRead (struct biRecord *pRec);
typedef long	biSpecialLinConv (struct biRecord *pRec);

/* bi (binary input) dset */
struct bidset {
	long			number;
	biDevReport		*dev_report;
	biInit			*init;
	biInitRecord		*init_record;
	biGetIoIntInfo		*get_ioint_info;
	biRead			*read;
};
#endif	/* INCbiRecordh */

#ifdef	INCboRecordh
typedef void	boDevReport (struct boRecord *pRec);
typedef long	boInit (struct boRecord *pRec);
typedef long	boInitRecord (struct boRecord *pRec);
typedef long	boGetIoIntInfo (struct boRecord *pRec);
typedef long	boWrite (struct boRecord *pRec);

/* bo (binary output) dset */
struct bodset {
	long			number;
	boDevReport		*dev_report;
	boInit			*init;
	boInitRecord		*init_record;
	boGetIoIntInfo		*get_ioint_info;
	boWrite			*write;
};
#endif	/* INCboRecordh */


#ifdef	INClonginRecordh
typedef void	longinDevReport (struct longinRecord *pRec);
typedef long	longinInit (struct longinRecord *pRec);
typedef long	longinInitRecord (struct longinRecord *pRec);
typedef long	longinGetIoIntInfo (struct longinRecord *pRec);
typedef long	longinRead (struct longinRecord *pRec);

/* longin input dset */
struct longindset {
	long			number;
	longinDevReport		*dev_report;
	longinInit		*init;
	longinInitRecord	*init_record;
	longinGetIoIntInfo	*get_ioint_info;
	longinRead		*read;
};
#endif	/* INClonginRecordh */

#ifdef	INClongoutRecordh
typedef void	longoutDevReport (struct longoutRecord *pRec);
typedef long	longoutInit (struct longoutRecord *pRec);
typedef long	longoutInitRecord (struct longoutRecord *pRec);
typedef long	longoutGetIoIntInfo (struct longoutRecord *pRec);
typedef long	longoutWrite (struct longoutRecord *pRec);

/* longout output dset */
struct longoutdset {
	long			number;
	longoutDevReport	*dev_report;
	longoutInit		*init;
	longoutInitRecord	*init_record;
	longoutGetIoIntInfo	*get_ioint_info;
	longoutWrite		*write;
};
#endif	/* INClongoutRecordh */

#ifdef	INCmbbiRecordh
typedef void	mbbiDevReport (struct mbbiRecord *pRec);
typedef long	mbbiInit (struct mbbiRecord *pRec);
typedef long	mbbiInitRecord (struct mbbiRecord *pRec);
typedef long	mbbiGetIoIntInfo (struct mbbiRecord *pRec);
typedef long	mbbiRead (struct mbbiRecord *pRec);

/* mbbi (multi bit binary input) dset */
struct mbbidset {
	long			number;
	mbbiDevReport		*dev_report;
	mbbiInit		*init;
	mbbiInitRecord		*init_record;
	mbbiGetIoIntInfo	*get_ioint_info;
	mbbiRead		*read;
};
#endif	/* INCmbbiRecordh */

#ifdef	INCmbboRecordh
typedef void	mbboDevReport (struct mbboRecord *pRec);
typedef long	mbboInit (struct mbboRecord *pRec);
typedef long	mbboInitRecord (struct mbboRecord *pRec);
typedef long	mbboGetIoIntInfo (struct mbboRecord *pRec);
typedef long	mbboWrite (struct mbboRecord *pRec);

/* mbbo (multi bit binary output) dset */
struct mbbodset {
	long			number;
	mbboDevReport		*dev_report;
	mbboInit		*init;
	mbboInitRecord		*init_record;
	mbboGetIoIntInfo	*get_ioint_info;
	mbboWrite		*write;
};
#endif	/* INCmbboRecordh */


#ifdef	INCeventRecordh
typedef void	eventDevReport (struct eventRecord *pRec);
typedef long	eventInit (struct eventRecord *pRec);
typedef long	eventInitRecord (struct eventRecord *pRec);
typedef long	eventGetIoIntInfo (struct eventRecord *pRec);
typedef long	eventRead (struct eventRecord *pRec);

/* event input dset */
struct eventdset {
	long			number;
	eventDevReport		*dev_report;
	eventInit		*init;
	eventInitRecord		*init_record;
	eventGetIoIntInfo	*get_ioint_info;
	eventRead		*read;
};
#endif	/* INCeventRecordh */

#ifdef	INChistogramRecordh
typedef void	histogramDevReport (struct histogramRecord *pRec);
typedef long	histogramInit (struct histogramRecord *pRec);
typedef long	histogramInitRecord (struct histogramRecord *pRec);
typedef long	histogramGetIoIntInfo (struct histogramRecord *pRec);
typedef long	histogramRead (struct histogramRecord *pRec);
typedef long	histogramSpecialLinConv (struct histogramRecord *pRec);

/* histogram input dset */
struct histogramdset {
	long			number;
	histogramDevReport	*dev_report;
	histogramInit		*init;
	histogramInitRecord	*init_record;
	histogramGetIoIntInfo	*get_ioint_info;
	histogramRead		*read;
	histogramSpecialLinConv	*special_linconv;
};
#endif	/* INChistogramRecordh */

#ifdef	INCpulseCounterRecordh
typedef void	pulseCounterDevReport (struct pulseCounterRecord *pRec);
typedef long	pulseCounterInit (struct pulseCounterRecord *pRec);
typedef long	pulseCounterInitRecord (struct pulseCounterRecord *pRec);
typedef long	pulseCounterGetIoIntInfo (struct pulseCounterRecord *pRec);
typedef long	pulseCounterCmdPc (struct pulseCounterRecord *pRec);

/* pulseCounter input dset */
struct pcdset {
	long			number;
	pulseCounterDevReport	*dev_report;
	pulseCounterInit	*init;
	pulseCounterGetIoIntInfo *init_record;
	pulseCounterGetIoIntInfo *get_ioint_info;
	pulseCounterCmdPc	*cmd_pc;
};
#endif	/* INCpulseCounterRecordh */

#ifdef	INCpulseDelayRecordh
typedef void	pulseDelayDevReport (struct pulseDelayRecord *pRec);
typedef long	pulseDelayInit (struct pulseDelayRecord *pRec);
typedef long	pulseDelayInitRecord (struct pulseDelayRecord *pRec);
typedef long	pulseDelayGetIoIntInfo (struct pulseDelayRecord *pRec);
typedef long	pulseDelayWrite (struct pulseDelayRecord *pRec);
typedef long	pulseDelayGetEnum (struct pulseDelayRecord *pRec);

/* pulseDelay output dset */
struct pddset {
	long			number;
	pulseDelayDevReport	*dev_report;
	pulseDelayInit		*init;
	pulseDelayInitRecord	*init_record;
	pulseDelayGetIoIntInfo	*get_ioint_info;
	pulseDelayWrite		*write;
	pulseDelayGetEnum	*get_enum;
};
#endif	/* INCpulseDelayRecordh */

#ifdef	INCpulseTrainRecordh
typedef void	pulseTrainDevReport (struct pulseTrainRecord *pRec);
typedef long	pulseTrainInit (struct pulseTrainRecord *pRec);
typedef long	pulseTrainInitRecord (struct pulseTrainRecord *pRec);
typedef long	pulseTrainGetIoIntInfo (struct pulseTrainRecord *pRec);
typedef long	pulseTrainWrite (struct pulseTrainRecord *pRec);

/* pulseTrain input dset */
struct ptdset {
	long			number;
	pulseTrainDevReport	*dev_report;
	pulseTrainInit		*init;
	pulseTrainInitRecord	*init_record;
	pulseTrainGetIoIntInfo	*get_ioint_info;
	pulseTrainWrite		*write;
};
#endif	/* INCpulseTrainRecordh */

#ifdef	INCstringinRecordh
typedef void	stringinDevReport (struct stringinRecord *pRec);
typedef long	stringinInit (struct stringinRecord *pRec);
typedef long	stringinInitRecord (struct stringinRecord *pRec);
typedef long	stringinGetIoIntInfo (struct stringinRecord *pRec);
typedef long	stringinRead (struct stringinRecord *pRec);

/* stringin input dset */
struct stringindset {
	long			number;
	stringinDevReport	*dev_report;
	stringinInit		*init;
	stringinInitRecord	*init_record;
	stringinGetIoIntInfo	*get_ioint_info;
	stringinRead		*read;
};
#endif	/* INCstringinRecordh */

#ifdef	INCstringoutRecordh
typedef void	stringoutDevReport (struct stringoutRecord *pRec);
typedef long	stringoutInit (struct stringoutRecord *pRec);
typedef long	stringoutInitRecord (struct stringoutRecord *pRec);
typedef long	stringoutGetIoIntInfo (struct stringoutRecord *pRec);
typedef long	stringoutWrite (struct stringoutRecord *pRec);

/* stringout output dset */
struct stringoutdset {
	long			number;
	stringoutDevReport	*dev_report;
	stringoutInit		*init;
	stringoutInitRecord	*init_record;
	stringoutGetIoIntInfo	*get_ioint_info;
	stringoutWrite		*write;
};
#endif	/* INCstringoutRecordh */

#ifdef	INCtimerRecordh
typedef void	timerDevReport (struct timerRecord *pRec);
typedef long	timerInit (struct timerRecord *pRec);
typedef long	timerInitRecord (struct timerRecord *pRec);
typedef long	timerGetIoIntInfo (struct timerRecord *pRec);
typedef long	timerRead (struct timerRecord *pRec);
typedef long	timerWrite (struct timerRecord *pRec);

/* timer dset */
struct timerdset {
	long			number;
	timerDevReport		*dev_report;
	timerInit		*init;
	timerInitRecord		*init_record;
	timerGetIoIntInfo	*get_ioint_info;
	timerRead		*read;
	timerWrite		*write;
};
#endif	/* INCtimerRecordh */

#ifdef	INCwaveformRecordh
typedef void	waveformDevReport (struct waveformRecord *pRec);
typedef long	waveformInit (struct waveformRecord *pRec);
typedef long	waveformInitRecord (struct waveformRecord *pRec);
typedef long	waveformGetIoIntInfo (struct waveformRecord *pRec);
typedef long	waveformRead (struct waveformRecord *pRec);

/* waveform dset */
struct waveformdset {
	long			number;
	waveformDevReport	*dev_report;
	waveformInit		*init;
	waveformInitRecord	*init_record;
	waveformGetIoIntInfo	*get_ioint_info;
	waveformRead		*read;
};
#endif	/* INCwaveformRecordh */
/* How to set up and use a Device Support Entry Table (DSET) */

