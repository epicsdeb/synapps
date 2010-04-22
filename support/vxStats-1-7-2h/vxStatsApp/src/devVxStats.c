/* devAiStats.c - Device Support Routines for vxWorks statistics */
/*
 *      Author: Jim Kowalkowski
 *      Date:  2/1/96
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modifications at LBNL:
 * -----------------
 * 	97-11-21	SRJ	Added reports of max free mem block,
 *				Channel Access connections and CA clients.
 *				Repaired "artificial load" function.
 *	98-01-28	SRJ	Changes per M. Kraimer's devVXStats of 97-11-19:
 *				explicitly reports file descriptors used;
 *				uses Kraimer's method for CPU load average;
 *				some code simplification and name changes.
 *
 * Modifications for SNS at ORNL:
 * -----------------
 *	03-01-29	CAL 	Add stringin device support.
 *
 *	03-05-08	CAL	Add minMBuf
 */

/*
	add the following to devSup.ascii:

	"ai"  INST_IO    "devAiStats"   "VX stats"
	"ao"  INST_IO    "devAoStats"   "VX stats"
	"stringin"  INST_IO    "devStringinStats"   "VX stats"

	--------------------------------------------------------------------
	Add FD and CA connection information before release.
	Add ability to adjust the rate of everything through ao records

	sample db file to use all data available:
	Note that the valid values for the parm field of the link
	information are:

	ai:
		free_bytes	 - number of bytes in IOC not allocated
		allocated_bytes  - number of bytes allocated
		max_free	 - size of largest free block
		cpu		 - estimated percent CPU usage by tasks
		fd		 - number of file descriptors currently in use
		ca_clients	 - number of current CA clients
		ca_connections	 - number of current CA connections
                mbufs		 - minimum % free in all mbuf pools
	ao:
		memoryScanRate	 - max rate at which new memory stats can be read
		fdScanRate	- max rate at which file descriptors can be counted
		cpuScanRate	 - max rate at which cpu load can be calculated
		caConnScanRate	 - max rate at which CA connections can be calculated

	* scan rates are all in seconds

	default rates:
		10 - memory scan rate
		20 - cpu scan rate
		10 - fd scan rate
		15 - CA scan rate

        stringin:

                Some values displayed by the string records are
	 	longer than the 40 char string record length, so multiple 
		records must be used to display them.

		The supported stringin devices are all static; the record only
		needs to be processed once, for which PINI is convenient.

		startup_script_[12]	-path of startup script (2 records)
		bootline_[1-6]		-vxWorks bootline (6 records)
 		bsp_rev			-vxWorks board support package revision
		kernel_ver		-vxWorks kernel build version
		epics_ver		-EPICS base version

	# sample database using all the different types of statistics
	record(ai,"$(PRE):freeBytes")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@free_bytes")
	}
	record(ai,"$(PRE):allocatedBytes")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@allocated_bytes")
	}
	record(ai,"$(PRE):maxFree")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@max_free")
	}
	record(ai,"$(PRE):cpu")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@cpu")
		field(PREC,"3")
	}
	record(ai,"$(PRE):fd")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@fd")
	}
	record(ai,"$(PRE):caClients")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@ca_clients")
	}
	record(ai,"$(PRE):caConnections")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@ca_connections")
	}
	record(ai,"$(PRE):minDataMBuf")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
                field(EGU, "%")
		field(INP,"@min_data_mbuf")
	}
	record(ai,"$(PRE):minSysMBuf")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
                field(EGU, "%")
		field(INP,"@min_sys_mbuf")
	}
	record(ao,"$(PRE):memoryScanRate")
	{
		field(DTYP,"VX stats")
		field(OUT,"@memory_scan_rate")
	}
	record(ao,"$(PRE):fdScanRate")
	{
		field(DTYP,"VX stats")
		field(OUT,"@fd_scan_rate")
	}
	record(ao,"$(PRE):caConnScanRate")
	{
		field(DTYP,"VX stats")
		field(OUT,"@ca_scan_rate")
	}
	record(ao,"$(PRE):cpuScanRate")
	{
		field(DTYP,"VX stats")
		field(OUT,"@cpu_scan_rate")
	}
        record(stringin,"$(PRE):StartupScript1")
        {
		field(DTYP,"VX stats")
                field(INP,"@startup_script_1")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):StartupScript2")
        {
		field(DTYP,"VX stats")
                field(INP,"@startup_script_2")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):Bootline1")
        {
		field(DTYP,"VX stats")
                field(INP,"@bootline_1")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):Bootline2")
        {
		field(DTYP,"VX stats")
                field(INP,"@bootline_2")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):Bootline3")
        {
		field(DTYP,"VX stats")
                field(INP,"@bootline_3")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):Bootline4")
        {
		field(DTYP,"VX stats")
                field(INP,"@bootline_4")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):Bootline5")
        {
		field(DTYP,"VX stats")
                field(INP,"@bootline_5")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):Bootline6")
        {
		field(DTYP,"VX stats")
                field(INP,"@bootline_6")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):BSPRev")
        {
		field(DTYP,"VX stats")
                field(INP,"@bsp_rev")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):KernelVer")
        {
		field(DTYP,"VX stats")
                field(INP,"@kernel_ver")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):EPICSVer")
        {
		field(DTYP,"VX stats")
                field(INP,"@epics_ver")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):Engineer")
        {
		field(DTYP,"VX stats")
                field(INP,"@engineer")
           	field(PINI, "Yes")
        }
        record(stringin,"$(PRE):Location")
        {
		field(DTYP,"VX stats")
                field(INP,"@location")
           	field(PINI, "Yes")
        }
*/

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <taskLib.h>
#include <wdLib.h>
#include <math.h>
#include <time.h>
#include <tickLib.h>
#include <kernelLib.h>
#include <sysSymTbl.h>
#include <epicsDynLink.h>
#include <sysLib.h>

#include <netBufLib.h>

#include <alarm.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <callback.h>
#include <recSup.h>
#include <devSup.h>
#include <link.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <stringinRecord.h>
#include <recGbl.h>

/* for local memInfoGet routine */
#include <memLib.h>
#include "semLib.h"
#include "dllLib.h"
#include "smObjLib.h"
#include "private/memPartLibP.h"

/* for fdStats */
#include <private/iosLibP.h>
#include "epicsVersion.h"

#include "epicsExport.h"


#define MEMORY_TYPE	0
#define LOAD_TYPE	1
#define FD_TYPE		2
#define CA_TYPE		3
#define STATIC_TYPE	4
#define TOTAL_TYPES	5

#define START_ME 0
#define STOP_ME 1

typedef void (*statGetFunc)(double*);
typedef void (*statGetStrFunc)(char*);
typedef void (*statPutFunc)(int,unsigned long);
typedef struct vmeio vmeio;

struct aStats
{
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;
};
typedef struct aStats aStats;

struct sStats
{
	long 		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin;
};
typedef struct sStats sStats;

struct pvtArea
{
	int index;
	int type;
};
typedef struct pvtArea pvtArea;

struct pvtClustArea
{
        int pool;
        int size;
        int elem;
};
typedef struct pvtClustArea pvtClustArea;

struct validGetParms
{
	char* name;
	statGetFunc func;
	int type;
};
typedef struct validGetParms validGetParms;

struct validGetStrParms
{
	char* name;
	statGetStrFunc func;
	int type;
};
typedef struct validGetStrParms validGetStrParms;


struct scanInfo
{
	IOSCANPVT ioscan;
	WDOG_ID wd;
	volatile int total;					/* total users connected */
	volatile int on;					/* watch dog on? */
	volatile time_t last_read_sec;		/* last time seconds */
	volatile unsigned long rate_sec;	/* seconds */
	volatile unsigned long rate_tick;	/* ticks */
};
typedef struct scanInfo scanInfo;

extern void read_ca_stats(int *pncli, int *pnconn);


static long ai_init(int pass);
static long ai_init_record(aiRecord*);
static long ai_read(aiRecord*);
static long ai_ioint_info(int cmd,aiRecord* pr,IOSCANPVT* iopvt);

static long ai_clusts_init(int pass);
static long ai_clusts_init_record(aiRecord *);
static long ai_clusts_read(aiRecord *);

static long ao_init_record(aoRecord* pr);
static long ao_write(aoRecord*);

static long stringin_init_record(stringinRecord*);
static long stringin_read(stringinRecord*);

static void read_mem_stats(void);
static void read_fd_stats(void);
static void scan_time(int);
static void cpuUsageInit(void);

static void statsFreeBytes(double*);
static void statsFreeBlocks(double*);
static void statsAllocBytes(double*);
static void statsAllocBlocks(double*);
static void statsMaxFree(double*);
static void statsProcessLoad(double*);
static void statsFd(double*);
static void statsFdMax(double*);
static void statsCAConnects(double*);
static void statsCAClients(double*);
static void statsMinDataMBuf(double*);
static void statsMinSysMBuf(double*);
static void statsDataMBuf(double*);
static void statsSysMBuf(double*);
static void statsIFIErrs(double *);
static void statsIFOErrs(double *);

/* rate in seconds (memory,cpu,fd,ca) */
/* statsPutParms[] must be in the same order (see ao_init_record()) */
static int scan_rate_sec[] = { 10,20,10,15 };

static validGetParms statsGetParms[]={
	{ "free_bytes",			statsFreeBytes,		MEMORY_TYPE },
	{ "free_blocks",		statsFreeBlocks,	MEMORY_TYPE },
        { "max_free",                   statsMaxFree,           MEMORY_TYPE },
	{ "allocated_bytes",		statsAllocBytes,	MEMORY_TYPE },
	{ "allocated_blocks",		statsAllocBlocks,	MEMORY_TYPE },
	{ "cpu",			statsProcessLoad,	LOAD_TYPE },
	{ "fd",				statsFd,		FD_TYPE },
	{ "maxfd",			statsFdMax,		STATIC_TYPE },
	{ "ca_clients",			statsCAClients,		CA_TYPE },
	{ "ca_connections",		statsCAConnects,	CA_TYPE },
	{ "min_data_mbuf",		statsMinDataMBuf,	MEMORY_TYPE },
	{ "min_sys_mbuf",		statsMinSysMBuf,	MEMORY_TYPE },
	{ "data_mbuf",			statsDataMBuf,		STATIC_TYPE },
	{ "sys_mbuf",			statsSysMBuf,		STATIC_TYPE },
	{ "inp_errs",			statsIFIErrs,		MEMORY_TYPE },
	{ "out_errs",			statsIFOErrs,		MEMORY_TYPE },
	{ NULL,NULL,0 }
};

static void statsSScript1(char *);
static void statsSScript2(char *);
static void statsBootline1(char *);
static void statsBootline2(char *);
static void statsBootline3(char *);
static void statsBootline4(char *);
static void statsBootline5(char *);
static void statsBootline6(char *);
static void statsBSPRev(char *);
static void statsKernelVer(char *);
static void statsEPICSVer(char *);
static void statsEngineer(char *);
static void statsLocation(char *);

static validGetStrParms statsGetStrParms[]={
        { "startup_script_1",		statsSScript1,		STATIC_TYPE },
        { "startup_script_2",		statsSScript2, 		STATIC_TYPE },
        { "bootline_1",			statsBootline1,		STATIC_TYPE },
        { "bootline_2",			statsBootline2,		STATIC_TYPE },
        { "bootline_3",			statsBootline3,		STATIC_TYPE },
        { "bootline_4",			statsBootline4,		STATIC_TYPE },
        { "bootline_5",			statsBootline5,		STATIC_TYPE },
        { "bootline_6",			statsBootline6,		STATIC_TYPE },
        { "bsp_rev",			statsBSPRev, 		STATIC_TYPE },
        { "kernel_ver",			statsKernelVer,		STATIC_TYPE },
        { "epics_ver",			statsEPICSVer,		STATIC_TYPE },
        { "engineer",			statsEngineer,		STATIC_TYPE },
        { "location",			statsLocation,		STATIC_TYPE },
	{ NULL,NULL,0 }
};


/* These are in the same order as in scan_rate_sec[] */
static char* statsPutParms[]={
	"memory_scan_rate",
	"cpu_scan_rate",
	"fd_scan_rate",
	"ca_scan_rate",
	NULL
};

aStats devAiStats={ 6,NULL,ai_init,ai_init_record,ai_ioint_info,ai_read,NULL };
epicsExportAddress(dset,devAiStats);
aStats devAoStats={ 6,NULL,NULL,ao_init_record,NULL,ao_write,NULL };
epicsExportAddress(dset,devAoStats);
aStats devAiClusts = {6,NULL,ai_clusts_init,ai_clusts_init_record,NULL,ai_clusts_read,NULL };
epicsExportAddress(dset,devAiClusts);
sStats devStringinStats={5,NULL,NULL,stringin_init_record,NULL,stringin_read };
epicsExportAddress(dset,devStringinStats);

static scanInfo scan[TOTAL_TYPES];
static int fdinfo;
static int cainfo_clients;
static int cainfo_connex;
static int min_data_mbuf_percent;
static int min_sys_mbuf_percent;

/* ---------------------------------------------------------------------- */

static long ai_clusts_init(int pass) 
{
   return 0;
}

static long ai_init(int pass)
{
	int i;

	if(pass) return 0;

	for(i=0;i<TOTAL_TYPES;i++)
	{
		scanIoInit(&scan[i].ioscan);
		scan[i].wd=wdCreate();
		scan[i].total=0;
		scan[i].on=0;
		scan[i].rate_sec=scan_rate_sec[i];
		scan[i].rate_tick=scan_rate_sec[i]*sysClkRateGet();
		scan[i].last_read_sec=1000000;
	}
	cpuUsageInit();
	return 0;
}

static long ai_clusts_init_record(aiRecord *pr)
{
    int elem = 0, size = 0, pool = 0,  parms = 0;
    char	*parm;
    pvtClustArea	*pvt = NULL;

    if(pr->inp.type!=INST_IO)
    {
       recGblRecordError(S_db_badField,(void*)pr,
		"devAiClusts (init_record) Illegal INP field");
       return S_db_badField;
    }
    parm = pr->inp.value.instio.string;
    parms = sscanf(parm,"clust_info %d %d %d", &pool, &size, &elem);
    if (parms == 3)
    {
	pvt=(pvtClustArea*)malloc(sizeof(pvtClustArea));
        if (pvt)
        {
	   pvt->pool = pool;
           pvt->size = size;
           pvt->elem = elem;
        }
    }
    if (pvt == NULL)
    {
	recGblRecordError(S_db_badField,(void*)pr,
		"devAiClusts (init_record) Illegal INP parm field");
	return S_db_badField;
    }
    /* Make sure record processing routine does not perform any conversion*/
    pr->linr=0;
    pr->dpvt=pvt;
    return 0;
}

static long ai_init_record(aiRecord* pr)
{
    int		i;
    char	*parm;
    pvtArea	*pvt = NULL;

	if(pr->inp.type!=INST_IO)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devAiStats (init_record) Illegal INP field");
		return S_db_badField;
	}
	parm = pr->inp.value.instio.string;
	for(i=0;statsGetParms[i].name && pvt==NULL;i++)
	{
		if(strcmp(parm,statsGetParms[i].name)==0)
		{
			pvt=(pvtArea*)malloc(sizeof(pvtArea));
			pvt->index=i;
			pvt->type=statsGetParms[i].type;
		}
	}
        
	if(pvt==NULL)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devAiStats (init_record) Illegal INP parm field");
		return S_db_badField;
	}

    /* Make sure record processing routine does not perform any conversion*/
    pr->linr=0;
    pr->dpvt=pvt;
    return 0;
}

static long ao_init_record(aoRecord* pr)
{
    int		type;
    char	*parm;
    pvtArea	*pvt = NULL;

	if(pr->out.type!=INST_IO)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devAiStats (init_record) Illegal OUT field");
		return S_db_badField;
	}
	parm = pr->out.value.instio.string;
	for(type=0; type<TOTAL_TYPES; type++)
	{
		if(strcmp(parm,statsPutParms[type])==0)
		{
			pvt=(pvtArea*)malloc(sizeof(pvtArea));
			pvt->index=type;
			pvt->type=type;
		}
	}
	if(pvt==NULL)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devAiStats (init_record) Illegal INP parm field");
		return S_db_badField;
	}

    /* Initialize value with default */
#if 0
    pr->rbv=pr->rval=scan_rate_sec[--type];
#else
    pr->dpvt=pvt;
    /* If VAL field is non-zero use default */
    if (pr->val == 0.) pr->val = scan_rate_sec[pvt->type];
    /* Update the private fields for this time */
    ao_write(pr);
#endif

    /* Make sure record processing routine does not perform any conversion*/
    pr->linr=0;
#if 0
    pr->dpvt=pvt;
    return 0;
#else
    return 2;  /* Don't convert since we set VAL field directly */
#endif
}

static long stringin_init_record(stringinRecord* pr)
{
    int		i;
    char	*parm;
    pvtArea	*pvt = NULL;

	if(pr->inp.type!=INST_IO)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devStringinStats (init_record) Illegal INP field");
		return S_db_badField;
	}
	parm = pr->inp.value.instio.string;
	for(i=0;statsGetStrParms[i].name && pvt==NULL;i++)
	{
		if(strcmp(parm,statsGetStrParms[i].name)==0)
		{
			pvt=(pvtArea*)malloc(sizeof(pvtArea));
			pvt->index=i;
			pvt->type=statsGetStrParms[i].type;
		}
	}
	if(pvt==NULL)
	{
		recGblRecordError(S_db_badField,(void*)pr, 
		   "devStringinStats (init_record) Illegal INP parm field");
		return S_db_badField;
	}

    pr->dpvt=pvt;
    return 0;	/* success */
}


static void scan_time(int type)
{
	scanIoRequest(scan[type].ioscan);
	if(scan[type].on)
		wdStart(scan[type].wd,scan[type].rate_tick,(FUNCPTR)scan_time,type);
}

static long ai_ioint_info(int cmd,aiRecord* pr,IOSCANPVT* iopvt)
{
	pvtArea* pvt=(pvtArea*)pr->dpvt;

	if(cmd==0) /* added */
	{
		if(scan[pvt->type].total++ == 0)
		{
			/* start a watchdog */
			wdStart(scan[pvt->type].wd,scan[pvt->type].rate_tick,
				(FUNCPTR)scan_time,pvt->type);
			scan[pvt->type].on=1;
		}
	}
	else /* deleted */
	{
		if(--scan[pvt->type].total == 0)
			scan[pvt->type].on=0; /* stop the watchdog */
	}

	*iopvt=scan[pvt->type].ioscan;
	return 0;
}

static long ao_write(aoRecord* pr)
{
    unsigned long sec=pr->val;
    pvtArea	*pvt=(pvtArea*)pr->dpvt;
    int		type=pvt->type;

/*	type = pvt->type; */
	scan[type].rate_sec=sec;
	scan[type].rate_tick=sec*sysClkRateGet();
	return 0;
}

static long ai_read(aiRecord* pr)
{
    double val;
    pvtArea* pvt=(pvtArea*)pr->dpvt;

	statsGetParms[pvt->index].func(&val);
	pr->val=val;
	pr->udf=0;
	return 2; /* don't convert */
}

static long stringin_read(stringinRecord* pr)
{
    pvtArea* pvt=(pvtArea*)pr->dpvt;

    statsGetStrParms[pvt->index].func(pr->val);
    pr->udf=0;
    return(0);	/* success */

}

/* -------------------------------------------------------------------- */

extern char *sysBootLine;

static void statsSScript1(char *d) 
{ strncpy(d, strstr(sysBootLine," s=")+3, 39); d[39]=0; }

static void statsSScript2(char *d)
{ strncpy(d, strstr(sysBootLine," s=")+42, 39); d[39]=0; }

static void statsBootline1(char *d)
{ strncpy(d, sysBootLine, 39); d[39]=0; }

static void statsBootline2(char *d)
{ strncpy(d, sysBootLine+39, 39); d[39]=0; }

static void statsBootline3(char *d)
{ strncpy(d, sysBootLine+78, 39); d[39]=0; }

static void statsBootline4(char *d)
{ strncpy(d, sysBootLine+117, 39); d[39]=0; }

static void statsBootline5(char *d)
{ strncpy(d, sysBootLine+156, 39); d[39]=0; }

static void statsBootline6(char *d)
{ strncpy(d, sysBootLine+195, 39); d[39]=0; }

static void statsBSPRev(char *d)
{ strncpy(d, sysBspRev(), 39); d[39]=0; }

static void statsKernelVer(char *d)
{ strncpy(d, kernelVersion(), 39); d[39]=0; }

static void statsEPICSVer(char *d)
{ strncpy(d,  epicsReleaseVersion, 39); d[39]=0; }

static char *notavail = "not available";

static void statsEngineer(char *d)
{
    SYM_TYPE stype;
    char **eng = &notavail;
    symFindByNameEPICS(sysSymTbl, "engineer",  (char **)&eng, &stype);
    strncpy(d, *eng, 39); d[39]=0; 
}

static void statsLocation(char *d)
{
    SYM_TYPE stype;
    char **loc = &notavail;
    symFindByNameEPICS(sysSymTbl, "location", (char **)&loc, &stype);
    strncpy(d, *loc, 39); d[39]=0; 
}


static MEM_PART_STATS meminfo;

/* Added by LTH because memPartInfoGet() has a bug when "walking" the
list */

static STATUS memInfoGet(
          MEM_PART_STATS *  ppartStats  /* partition stats structure */
          ){


   FAST PART_ID partId = memSysPartId;
   BLOCK_HDR *  pHdr;
   DL_NODE *    pNode;
     ppartStats->numBytesFree = 0;
   ppartStats->numBlocksFree = 0;
   ppartStats->numBytesAlloc = 0;
   ppartStats->numBlocksAlloc = 0;
   ppartStats->maxBlockSizeFree = 0;
     if (ID_IS_SHARED (partId))  /* partition is shared? */
     {
       /* shared partitions not supported yet */
       return (ERROR);
     }
     /* partition is local */
     if (OBJ_VERIFY (partId, memPartClassId) != OK)
     return (ERROR);
     /* take and keep semaphore until done */
   semTake (&partId->sem, WAIT_FOREVER);
     for (pNode = DLL_FIRST (&partId->freeList);
        pNode != NULL;
        pNode = DLL_NEXT (pNode))
     {
       pHdr = NODE_TO_HDR (pNode);
       {
     ppartStats->numBlocksFree ++ ;
     ppartStats->numBytesFree += 2 * pHdr->nWords;
     if(2 * pHdr->nWords > ppartStats->maxBlockSizeFree) 
ppartStats->maxBlockSizeFree = 2 * pHdr->nWords;
       }
     }
     ppartStats->numBytesAlloc = 2 * partId->curWordsAllocated;
   ppartStats->numBlocksAlloc = partId->curBlocksAllocated;

   semGive (&partId->sem);
     return (OK);
   }


static void read_mem_stats(void)
{
	time_t nt;
	time(&nt);

	if((nt-scan[MEMORY_TYPE].last_read_sec)>=scan[MEMORY_TYPE].rate_sec)
	{
		if(memInfoGet(&meminfo)==OK)
			scan[MEMORY_TYPE].last_read_sec=nt;
	}
}


static void read_fd_stats(void)
{
	time_t nt;
	int i,tot;
	time(&nt);

	if((nt-scan[FD_TYPE].last_read_sec)>=scan[FD_TYPE].rate_sec)
	{
		for(tot=0,i=0;i<maxFiles;i++)
			if(fdTable[i].inuse)
				tot++;

		fdinfo=tot;
		scan[FD_TYPE].last_read_sec=nt;
	}
}


typedef struct cpuUsage {
    SEM_ID        startSem;
    int           didNotComplete;
    unsigned long ticksNoContention;
    int           nBurnNoContention;
    unsigned long ticksNow;
    int           nBurnNow;
    double        usage;
} cpuUsage;

static cpuUsage *pcpuUsage=0;

#define SECONDS_TO_BURN 5

static double cpuBurn()
{
    int i;
    double result = 0.0;

    for(i=0;i<5; i++) result += sqrt((double)i);
    return(result);
}

static void cpuUsageTask()
{
    while(TRUE) {
	int    i;
	unsigned long tickStart,tickEnd;

        semTake(pcpuUsage->startSem,WAIT_FOREVER);
	pcpuUsage->ticksNow=0;
	pcpuUsage->nBurnNow=0;
	tickStart = tickGet();
	for(i=0; i< pcpuUsage->nBurnNoContention; i++) {
	    cpuBurn();
	    pcpuUsage->ticksNow = tickGet() - tickStart;
	    ++pcpuUsage->nBurnNow;
	}
	tickEnd = tickGet();
	pcpuUsage->didNotComplete = FALSE;
	pcpuUsage->ticksNow = tickEnd - tickStart;
    }
}

static void cpuUsageStartMeasurement()
{
    if(pcpuUsage->didNotComplete && pcpuUsage->nBurnNow==0) {
	pcpuUsage->usage = 100.0;
    } else {
	double temp;
	double ticksNow,nBurnNow;

	ticksNow = (double)pcpuUsage->ticksNow;
	nBurnNow = (double)pcpuUsage->nBurnNow;
	ticksNow *= (double)pcpuUsage->nBurnNoContention/nBurnNow;
	temp = ticksNow - (double)pcpuUsage->ticksNoContention;
	temp = 100.0 * temp/ticksNow;
	if(temp<0.0 || temp>100.0) temp=0.0;/*take care of tick overflow*/
	pcpuUsage->usage = temp;
    }
    pcpuUsage->didNotComplete = TRUE;
    semGive(pcpuUsage->startSem);
}

static void cpuUsageInit(void)
{
    unsigned long tickStart,tickNow;
    int           nBurnNoContention=0;
    int		ticksToWait;

    ticksToWait = SECONDS_TO_BURN*sysClkRateGet();
    pcpuUsage = calloc(1,sizeof(cpuUsage));
    tickStart = tickGet();
    /*wait for a tick*/
    while(tickStart==(tickNow = tickGet())) {;}
    tickStart = tickNow;
    while(TRUE) {
	if((tickGet() - tickStart)>=ticksToWait) break;
	cpuBurn();
	nBurnNoContention++;
    }
    pcpuUsage->nBurnNoContention = nBurnNoContention;
    pcpuUsage->startSem = semBCreate (SEM_Q_FIFO,SEM_FULL);
    pcpuUsage->ticksNoContention = ticksToWait;
    taskSpawn("cpuUsageTask",255,VX_FP_TASK,1000,(FUNCPTR)cpuUsageTask,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* -------------------------------------------------------------------- */

#define MAXSMSG 120
#define CLUSTSIZES 10

static int clustinfo[2][CLUSTSIZES][4];
static int mbufnumber[2];
static int iferrors[2];

void minMBuf(int dataPool, int *ret)
{
   int i;
   double lowest = 1.0, comp;

   i = 0;
   dataPool = dataPool?1:0;
   while (clustinfo[dataPool][i][0] != 0)
   {
      comp = ((double)clustinfo[dataPool][i][2])/clustinfo[dataPool][i][1];
      if (comp < lowest) lowest = comp;
      i++;
   }
   *ret = (int)(lowest * 100);
}


void getClusts(int dataPool)
{
   NET_POOL_ID pNetPool;
   int i;
   int test;

   if (dataPool)
   {
      dataPool = 1;
      pNetPool = _pNetSysPool;
   }
   else
   {
      pNetPool = _pNetDpool;
   }

   test = pNetPool->clTbl[0]->clSize; 
   for (i = 0; i < CL_TBL_SIZE; i++)
   {
      /* first two are constant under current conditions and could be 
       * done just once per pool.
       */
      if (i > 0) 
	 if (pNetPool->clTbl[i]->clSize != (2 * test)) break;
      test = pNetPool->clTbl[i]->clSize; 
      clustinfo[dataPool][i][0] = test;
      clustinfo[dataPool][i][1] = pNetPool->clTbl[i]->clNum;
      clustinfo[dataPool][i][2] = pNetPool->clTbl[i]->clNumFree;
      clustinfo[dataPool][i][3] = pNetPool->clTbl[i]->clUsage;
   }
}

void getTotalClusts(int dataPool)
{
   NET_POOL_ID pNetPool;
   int i;

   if (dataPool)
   {
      dataPool = 1;
      pNetPool = _pNetSysPool;
   }
   else
   {
      pNetPool = _pNetDpool;
   }

   mbufnumber[dataPool] = 0;
   for (i = 0; i < NUM_MBLK_TYPES; i++)
      mbufnumber[dataPool] += pNetPool->pPoolStat->m_mtypes [i];
}

void getIFErrors()
{
   struct ifnet *ifp;
   
   /* add all interfaces' errors */
   iferrors[0] = 0; iferrors[1] = 0;
   for (ifp = ifnet;  ifp != NULL;  ifp = ifp->if_next) {
      iferrors[0] += ifp->if_ierrors;
      iferrors[1] += ifp->if_oerrors;
   }
}

int statsClustElement(int dataPool,int row,int elem)
{
   if (dataPool > 1 || dataPool < 0 || row < 0 || row >= CLUSTSIZES || 
         elem < 0 || elem > 3)
      return -1;

   return (clustinfo[dataPool][row][elem]);
}


static long ai_clusts_read(aiRecord* pr)
{
    pvtClustArea* pvt=(pvtClustArea*)pr->dpvt;
    
    pr->val = statsClustElement(pvt->pool, pvt->size, pvt->elem);
    if (pr->val > -1)
       pr->udf=0;
    return 2; /* don't convert */
}

static void statsFreeBytes(double* val)
{
	read_mem_stats();
	*val=(double)meminfo.numBytesFree;
}

static void statsFreeBlocks(double* val)
{
	read_mem_stats();
	*val=(double)meminfo.numBlocksFree;
}

static void statsAllocBytes(double* val)
{
	read_mem_stats();
	*val=(double)meminfo.numBytesAlloc;
}

static void statsAllocBlocks(double* val)
{
	read_mem_stats();
	*val=(double)meminfo.numBlocksAlloc;
}

static void statsMaxFree(double* val)
{
	read_mem_stats();
	*val=(double)meminfo.maxBlockSizeFree;
}

static void statsProcessLoad(double* val)
{
	cpuUsageStartMeasurement();
	*val=pcpuUsage->usage;
}

static void statsFd(double* val)
{
	read_fd_stats();
	*val=(double)fdinfo;
}

static void statsFdMax(double* val)
{
	*val=(double)maxFiles;
}

static void statsCAClients(double* val)
{
	read_ca_stats(&cainfo_clients,&cainfo_connex);
	*val=(double)cainfo_clients;
}

static void statsCAConnects(double* val)
{
	read_ca_stats(&cainfo_clients,&cainfo_connex);
	*val=(double)cainfo_connex;
}
      
static void statsMinSysMBuf(double* val)
{
        getClusts(1);	/* this refreshes data for all records that use it. */
        minMBuf(1, &min_sys_mbuf_percent);
        *val = (double)min_sys_mbuf_percent;
}
static void statsMinDataMBuf(double* val)
{
        getClusts(0);	/* this refreshes data for all records that use it. */
        minMBuf(0, &min_data_mbuf_percent);
        *val = (double)min_data_mbuf_percent;
}
static void statsSysMBuf(double* val)
{
        getTotalClusts(1);
        *val = mbufnumber[1];
}
static void statsDataMBuf(double* val)
{
        getTotalClusts(0);
        *val = mbufnumber[0];
}
static void statsIFIErrs(double* val)
{
        getIFErrors();
        *val = iferrors[0];
}
static void statsIFOErrs(double* val)
{
        *val = iferrors[1];
}

/* ----------test routines----------------- */

/* Some sample loads on MVME167:
*	
*	-> sp jbk_artificial_load, 100000000, 10000, 1
*		Load average: 69%
*	-> sp jbk_artificial_load, 100000000, 100000, 1
*		Load average: 95%
*	-> sp jbk_artificial_load, 100000000, 25000, 1
*		Load average: 88%
*/
int jbk_artificial_load(unsigned long iter,unsigned long often,unsigned long tick_delay)
{
	volatile unsigned long i;

	if(iter==0)
	{
		printf("Usage: jbk_artificial_load(num_iterations,iter_betwn_delays,tick_delay)\n");
		return 0;
	}

	for(i=0;i<iter;i++)
	{
		if(i%often==0) taskDelay(tick_delay);
	}

	return 0;
}


