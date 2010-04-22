/*
 * 10/29/96  tmm  v2.0 conversion to EPICS 3.13
 * 01/03/97  tmm  v2.1 use backup save file if save file can't be opened
 * 04/26/99  tmm  v2.2 check first character of restore file.  If not '#',
 *                then file is not trusted.
 * 11/24/99  tmm  v2.3 file-ok marker is now <END> and is placed at end of file.
 *                allow caller to choose whether boot-backup file is written,
 *                provide option of dated boot-backup files.
 * 02/27/02  tmm  v2.4 Added some features from Frank Lenkszus's (FRL) code:
 *                added path to request files
 *                added set_pass0_restoreFile( char *filename)
 *                added set_pass1_restoreFile( char *filename)
 *                a few more tweaks: 
 *                changed date-time stamp suffix to use FRL's fGetDateStr()
 *                don't write redundant backup files
 * 03/15/02  tmm  v2.5 check saveRestoreFilePath before using it.
 * 03/19/02  tmm  v2.6 initialize fname before using it.
 * 04/05/02  tmm  v2.7 Don't use copy for backup file.  It uses mode 640.
 * 08/01/03  mlr  v3.0 Convert to R3.14.2.  Fix a couple of bugs where failure
 *                 was assumed to be status<0, while it should be status!=0.
 * 11/18/03  tmm  v3.01 Save files with versions earlier than 1.8 don't have to
 *                pass the <END> test.
 * 04/20/04  tmm  v4.0 3.13-compatible v3.5, with array support, converted to
 *                EPICS 3.14, though NFS stuff is #ifdef vxWorks
 *                In addition to .sav and .savB, can save/restore <= 10
 *                sequenced files .sav0 -.sav9, which are written at preset
 *                intervals independent of the channel-list settings.
 *                Attempt to restore scalars to DBF_NOACCESS fields by calling
 *                dbNameToAddr() and then a dbFastPutConvert[][] routine.  (It's
 *                not an error to attempt this in pass 0, though it must fail.)
 *                Previously, restoreFileList.pass<n>Status wasn't initialized.
 * 10/04/04  tmm  Allow DOS line termination (CRLF) or unix (LF) in .sav files.
 *                Also, collapsed some code, and modified the way sequenced
 *                backup-file dates are compared to current date.
 * 10/29/04  tmm  v4.1 Added revision descriptions that should have been in
 *                previous revisions.  Changed VERSION number, in agreement
 *                with save_restore's SRVERSION string.
 * 11/30/04  tmm  v4.3 Added debug for curr_num_elements.
 * 01/26/05  tmm  v4.4 Strip trailing '\r' from value string.  (Previously, only
 *                '\n' was stripped.)
 * 01/28/05  tmm  v4.5 Filenames specified in set_pass<n>_restoreFile() now
 *                initialized with status SR_STATUS_INIT ('No Status')
 * 02/03/05  tmm  v4.6 copy VERSION to a variable, instead of using it directly
 *                as a string arg.  Check that restoreFileList.pass<n>files[i]
 *                is not NULL before comparing the string it's supposed to be
 *                pointing to.  Neither of those things fixed the crash, but
 *                replacing errlogPrintf with printf in reboot_restore did.
 * 06/27/05  tmm  v4.7 Dirk Zimoch (SLS) found and fixed problems with .sav
 *                files that lack header lines, or lack a version number.
 *                Check return codes from some calls to fseek().
 * 03/28/06  tmm  Replaced all Debug macros with straight code
 * 10/05/06  tmm  v4.8 Use binary mode for fopen() calls in myFileCopy, to avoid
 *                file-size differences caused by different line terminators
 *                on different operating systems.  (Thanks to Kay Kasemir.)
 * 01/02/07  tmm  v4.9 Convert empty SPC_CALC fields to "0" before restoring.
 * 03/19/07  tmm  v4.10 Don't print errno unless function returns an error.
 * 08/03/07  tmm  v4.11 Added functions makeAutosaveFileFromDbInfo() and makeAutosaveFiles()
 *                which search through the loaded database, looking for info nodes indicating
 *                fields that are to be autosaved.
 * 09/11/09  tmm  v4.12 If recordname is an alias (>=3.14.11), don't search for info nodes.
 *                
 */
#define VERSION "4.12"

#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#include	<sys/stat.h>
#include	<string.h>
#include	<ctype.h>
#include	<time.h>
/* added for 3.14 port */
#include	<math.h>	/* for safeDoubleToFloat() */
#include	<float.h>	/* for safeDoubleToFloat() */

#include	<dbStaticLib.h>
#include	<dbAccess.h>	/* includes dbDefs.h, dbBase.h, dbAddr.h, dbFldTypes.h */
#include	<recSup.h>		/* rset */
#include	<dbConvert.h> 	/* dbPutConvertRoutine */
#include	<dbConvertFast.h>	/* dbFastPutConvertRoutine */
#include	<initHooks.h>
#include	<epicsThread.h>
#include	<errlog.h>
#include	<iocsh.h>
#include 	"fGetDateStr.h"
#include	"save_restore.h"
#include	<epicsExport.h>
#include	<special.h>

#ifndef vxWorks
#define OK 0
#define ERROR -1
#endif

/* EPICS base version tests.*/
#define LT_EPICSBASE(v,r,l) ((EPICS_VERSION<=(v)) && (EPICS_REVISION<=(r)) && (EPICS_MODIFICATION<(l)))
#define GE_EPICSBASE(v,r,l) ((EPICS_VERSION>=(v)) && (EPICS_REVISION>=(r)) && (EPICS_MODIFICATION>=(l)))

STATIC char 	*RESTORE_VERSION = VERSION;

struct restoreList restoreFileList = {0, 0, 
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
			{0,0,0,0,0,0,0,0},
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
			{0,0,0,0,0,0,0,0},
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

void myPrintErrno(char *s, char *file, int line) {
	errlogPrintf("%s(%d): [0x%x]=%s:%s\n", file, line, errno, s, strerror(errno));
}

STATIC float mySafeDoubleToFloat(double d)
{
	float f;
    double abs = fabs(d);
    if (d==0.0) {
        f = 0.0;
    } else if (abs>=FLT_MAX) {
        if (d>0.0) f = FLT_MAX; else f = -FLT_MAX;
    } else if (abs<=FLT_MIN) {
        if (d>0.0) f = FLT_MIN; else f = -FLT_MIN;
    } else {
        f = d;
    }
	return(f);
}

void dbrestoreShow(void)
{
	int i;
	printf("  '     filename     ' -  status  - 'message'\n");
	printf("  pass 0:\n");
	for (i=0; i<MAXRESTOREFILES; i++) {
		if (restoreFileList.pass0files[i]) {
			printf("  '%s' - %s - '%s'\n", restoreFileList.pass0files[i],
				SR_STATUS_STR[restoreFileList.pass0Status[i]],
				restoreFileList.pass0StatusStr[i]);
		}
	}
	printf("  pass 1:\n");
	for (i=0; i<MAXRESTOREFILES; i++) {
		if (restoreFileList.pass1files[i]) {
			printf("  '%s' - %s - '%s'\n", restoreFileList.pass1files[i],
				SR_STATUS_STR[restoreFileList.pass1Status[i]],
				restoreFileList.pass1StatusStr[i]);
		}
	}
}

STATIC int myFileCopy(const char *source, const char *dest)
{
	FILE 	*source_fd, *dest_fd;
	char	buffer[BUF_SIZE], *bp;
	struct stat fileStat;
	int		chars_printed, size=0;

	if (save_restoreDebug >= 5)
		errlogPrintf("dbrestore:myFileCopy: copying '%s' to '%s'\n", source, dest);

	if (stat(source, &fileStat) == 0) size = (int)fileStat.st_size;
	errno = 0;
	if ((source_fd = fopen(source,"rb")) == NULL) {
		errlogPrintf("save_restore:myFileCopy: Can't open file '%s'\n", source);
		/* if (errno) myPrintErrno("myFileCopy", __FILE__, __LINE__); */
		if (++save_restoreIoErrors > save_restoreRemountThreshold) 
			save_restoreNFSOK = 0;
		return(ERROR);
	}
	errno = 0;
	/* Note: under vxWorks, the following fopen() frequently will set errno
	 * to S_nfsLib_NFSERR_NOENT even though it succeeds.  Probably this means
	 * a failed attempt was retried. (System calls never set errno to zero.)
	 */
	if ((dest_fd = fopen(dest,"wb")) == NULL) {
		errlogPrintf("save_restore:myFileCopy: Can't open file '%s'\n", dest);
		/* if (errno) myPrintErrno("myFileCopy", __FILE__, __LINE__); */
		fclose(source_fd);
		return(ERROR);
	}
	chars_printed = 0;
	while ((bp=fgets(buffer, BUF_SIZE, source_fd))) {
		errno = 0;
		chars_printed += fprintf(dest_fd, "%s", bp);
		/* if (errno) {myPrintErrno("myFileCopy", __FILE__, __LINE__); errno = 0;} */
	}
	errno = 0;
	if (fclose(source_fd) != 0){
                errlogPrintf("save_restore:myFileCopy: Error closing file '%s'\n", source);
		/* if (errno) myPrintErrno("myFileCopy", __FILE__, __LINE__); */
	}
	errno = 0;
	if (fclose(dest_fd) != 0){
		errlogPrintf("save_restore:myFileCopy: Error closing file '%s'\n", dest);
		/* if (errno) myPrintErrno("myFileCopy", __FILE__, __LINE__); */
	}
	errno = 0;
	if (size && (chars_printed != size)) {
		errlogPrintf("myFileCopy: size=%d, chars_printed=%d\n",
			size, chars_printed);
		return(ERROR);
	}
	return(OK);
}


STATIC long scalar_restore(int pass, DBENTRY *pdbentry, char *PVname, char *value_string)
{
	long 	n, status = 0;
	char 	*s;
	DBADDR	dbaddr;
	DBADDR	*paddr = &dbaddr;
	dbfType field_type = pdbentry->pflddes->field_type;
	short special = pdbentry->pflddes->special;
	
	if (save_restoreDebug >= 5) errlogPrintf("dbrestore:scalar_restore:entry:field type '%s'\n", pamapdbfType[field_type].strvalue);
	switch (field_type) {
	case DBF_STRING: case DBF_ENUM:
	case DBF_CHAR:   case DBF_UCHAR:
	case DBF_SHORT:  case DBF_USHORT:
	case DBF_LONG:   case DBF_ULONG:
	case DBF_FLOAT:  case DBF_DOUBLE:
		/*
		 * check SPC_CALC fields against new (3.13.9) requirement that CALC
		 * fields not be empty.
		 */
		if ((field_type==DBF_STRING) && (special==SPC_CALC)){
			if (*value_string == 0) strcpy(value_string, "0");
		}

		status = dbPutString(pdbentry, value_string);
		if (save_restoreDebug >= 15) {
			errlogPrintf("dbrestore:scalar_restore: dbPutString() returns %ld:", status);
			errMessage(status, " ");
		}
		if ((s = dbVerify(pdbentry, value_string))) {
			errlogPrintf("save_restore: for '%s', dbVerify() says '%s'\n", PVname, s);
			status = -1;
		}
		break;

	case DBF_INLINK: case DBF_OUTLINK: case DBF_FWDLINK:
		/* Can't restore links in pass 1 */
		if (pass == 0) {
			status = dbPutString(pdbentry, value_string);
			if (save_restoreDebug >= 15) {
				errlogPrintf("dbrestore:scalar_restore: dbPutString() returns %ld:", status);
				errMessage(status, " ");
			}
			if ((s = dbVerify(pdbentry, value_string))) {
				errlogPrintf("save_restore: for '%s', dbVerify() says '%s'\n", PVname, s);
				status = -1;
			}
		} else if (save_restoreDebug >= 1) {
				errlogPrintf("dbrestore:scalar_restore: Can't restore link field (%s) in pass 1.\n", PVname);
		}
		break;

	case DBF_MENU:
		n = (int)atol(value_string);
		status = dbPutMenuIndex(pdbentry, n);
		if (save_restoreDebug >= 15) {
			errlogPrintf("dbrestore:scalar_restore: dbPutMenuIndex() returns %ld:", status);
			errMessage(status, " ");
		}
		break;

	case DBF_NOACCESS:
		if (pass == 1) {
			status = dbNameToAddr(PVname, paddr);
			if (!status) {
				/* record initilization may have changed the field type */
				field_type = paddr->field_type;
				if (field_type <= DBF_MENU) {
					status = (*dbFastPutConvertRoutine[DBR_STRING][field_type])
						(value_string, paddr->pfield, paddr);
					if (status) {
						errlogPrintf("dbFastPutConvert failed (status=%ld) for field '%s'.\n",
							status, PVname);
					}
				}
			}
		} else if (save_restoreDebug >= 1) {
			errlogPrintf("dbrestore:scalar_restore: Can't restore DBF_NOACCESS field (%s) in pass 0.\n", PVname);
		}
		break;

	default:
		status = -1;
		if (save_restoreDebug >= 1) {
			errlogPrintf("dbrestore:scalar_restore: field_type '%d' not handled\n", field_type);
		}
		break;
	}
	if (status) {
		errlogPrintf("save_restore: dbPutString/dbPutMenuIndex of '%s' for '%s' failed\n",
			value_string, PVname);
		errMessage(status," ");
	}
	if (save_restoreDebug >= 15) {
		errlogPrintf("dbrestore:scalar_restore: dbGetString() returns '%s'\n",dbGetString(pdbentry));
	}
	return(status);
}

STATIC void *p_data = NULL;
STATIC long p_data_size = 0;

long SR_put_array_values(char *PVname, void *p_data, long num_values)
{
	DBADDR dbaddr;
	DBADDR *paddr = &dbaddr;
	long status, max_elements=0;
	STATIC long curr_no_elements=0, offset=0;
	struct rset	*prset;
	dbfType field_type;
						
	if ((status = dbNameToAddr(PVname, paddr)) != 0) {
		errlogPrintf("save_restore: dbNameToAddr can't find PV '%s'\n", PVname);
		return(status);
	}
	/* restore array values */
	max_elements = paddr->no_elements;
	field_type = paddr->field_type;
	prset = dbGetRset(paddr);
	if (prset && (prset->get_array_info) ) {
		status = (*prset->get_array_info)(paddr, &curr_no_elements, &offset);
	} else {
		offset = 0;
	}
	if (save_restoreDebug >= 5) {
		errlogPrintf("dbrestore:SR_put_array_values: restoring %ld values to %s (max_elements=%ld)\n", num_values, PVname, max_elements);
	}
	if (VALID_DB_REQ(field_type)) {
		status = (*dbPutConvertRoutine[field_type][field_type])(paddr,p_data,num_values,max_elements,offset);
	} else {
		errlogPrintf("save_restore:SR_put_array_values: PV %s: bad field type '%d'\n",
			PVname, (int) field_type);
		status = -1;
	}
	/* update array info */
	if (prset && (prset->put_array_info) && !status) {
		status = (*prset->put_array_info)(paddr, num_values);
	}
	return(status);
}


/* SR_array_restore()
 *
 * Parse file *inp_fd, starting with value_string, to extract array data into *p_data
 * ((re)allocate if existing space is insufficient).  If init_state permits, write array
 * to PV named *PVname.
 *
 * Expect the following syntax:
 * <white>[...]<begin>[<white>...<element>]...<white>...<end>[<anything>]
 * where
 *    <white> is whitespace
 *    [] surround an optional syntax element
 *    ... means zero or more repetitions of preceding syntax element
 *    <begin> is the character ARRAY_BEGIN
 *    <end> is the character ARRAY_END (must be different from ARRAY_BEGIN)
 *    <element> is
 *       <e_begin><character><e_end>
 *       where
 *          <e_begin> is the character ELEMENT_BEGIN (must be different from ARRAY_*)
 *          <e_end> is the character ELEMENT_END (may be the same as ELEMENT_BEGIN)
 *          <character> is any ascii character, or <escape><e_begin> or <escape><e_end>
 *          where <escape> is the character ESCAPE.
 *
 * Examples:
 *    { "1.23" " 2.34" " 3.45" }
 *    { "abc" "de\"f" "g{hi\"" "jkl mno} pqr" }
 */
long SR_array_restore(int pass, FILE *inp_fd, char *PVname, char *value_string)
{
	int				j, end_mark_found=0, begin_mark_found=0, end_of_file=0, found=0, in_element=0;
	long			status, max_elements=0, num_read=0;
	char			buffer[BUF_SIZE], *bp = NULL;
	char			string[MAX_STRING_SIZE];
	DBADDR			dbaddr;
	DBADDR			*paddr = &dbaddr;
	dbfType			field_type = DBF_NOACCESS;
	int				field_size = 0;
	char			*p_char = NULL;
	short			*p_short = NULL;
	long			*p_long = NULL;
	unsigned char	*p_uchar = NULL;
	unsigned short	*p_ushort = NULL;
	unsigned long	*p_ulong = NULL;
	float			*p_float = NULL;
	double			*p_double = NULL;


	if (save_restoreDebug >= 1) {
		errlogPrintf("dbrestore:SR_array_restore:entry: PV = '%s'\n", PVname);
	}
	if ((status = dbNameToAddr(PVname, paddr)) != 0) {
		errlogPrintf("save_restore: dbNameToAddr can't find PV '%s'\n", PVname);
	} else {
		/*** collect array elements from file into local array ***/
		max_elements = paddr->no_elements;
		field_type = paddr->field_type;
		field_size = paddr->field_size;
		/* if we've already allocated a big enough memory block, use it */
		if ((p_data == NULL) || ((max_elements * field_size) > p_data_size)) {
			if (save_restoreDebug >= 1) {
				errlogPrintf("dbrestore:SR_array_restore: p_data = %p, p_data_size = %ld\n", p_data, p_data_size);
			}
			if (p_data) free(p_data);
			p_data = (void *)calloc(max_elements, field_size);
			p_data_size = p_data ? max_elements * field_size : 0;
		} else {
			memset(p_data, 0, p_data_size);
		}
		if (save_restoreDebug >= 10) {
			errlogPrintf("dbrestore:SR_array_restore: Looking for up to %ld elements of field-size %d\n", max_elements, field_size);
			errlogPrintf("dbrestore:SR_array_restore: ...field_type is '%s' (%d)\n", pamapdbfType[field_type].strvalue, field_type);
		}

		switch (field_type) {
		case DBF_STRING: case DBF_CHAR:                p_char = (char *)p_data;             break;
		case DBF_UCHAR:                                p_uchar = (unsigned char *)p_data;   break;
		case DBF_ENUM: case DBF_USHORT: case DBF_MENU: p_ushort = (unsigned short *)p_data; break;
		case DBF_SHORT:                                p_short = (short *)p_data;           break;
		case DBF_ULONG:                                p_ulong = (unsigned long *)p_data;   break;
		case DBF_LONG:                                 p_long = (long *)p_data;             break;
		case DBF_FLOAT:                                p_float = (float *)p_data;           break;
		case DBF_DOUBLE:                               p_double = (double *)p_data;         break;
		case DBF_NOACCESS:
			break; /* just go through the motions, so we can parse the file */
		default:
			errlogPrintf("save_restore: field_type '%s' not handled\n", pamapdbfType[field_type].strvalue);
			status = -1;
			break;
		}
		/** read array values **/
		if (save_restoreDebug >= 11) {
			errlogPrintf("dbrestore:SR_array_restore: parsing buffer '%s'\n", value_string);
		}
		if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) != NULL) {
			begin_mark_found = 1;
			if (save_restoreDebug >= 10) {
				errlogPrintf("dbrestore:SR_array_restore: parsing array buffer '%s'\n", bp);
			}
			for (num_read=0; (num_read<max_elements) && bp && !end_mark_found; ) {
				/* Find beginning of array element */
				if (save_restoreDebug >= 10) {
					errlogPrintf("dbrestore:SR_array_restore: looking for element[%ld] \n", num_read);
				}
				while ((*bp != ELEMENT_BEGIN) && !end_mark_found && !end_of_file) {
					if (save_restoreDebug >= 12) {
						errlogPrintf("dbrestore:SR_array_restore: ...buffer contains '%s'\n", bp);
					}
					switch (*bp) {
					case '\0':
						if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
							errlogPrintf("save_restore: *** EOF during array-parse\n");
							end_of_file = 1;
						}
						break;
					case ARRAY_END:
						end_mark_found = 1;
						break;
					default:
						++bp;
						break;
					}
				}
				/*
				 * Read one element: Accumulate characters of element value into string[],
				 * ignoring any nonzero control characters, and append the value to the local array.
				 */
				if (bp && !end_mark_found && !end_of_file) {
					/* *bp == ELEMENT_BEGIN */
					if (save_restoreDebug >= 11) {
						errlogPrintf("dbrestore:SR_array_restore: Found element-begin; buffer contains '%s'\n", bp);
					}
					for (bp++, j=0; (j < MAX_STRING_SIZE-1) && (*bp != ELEMENT_END); bp++) {
						if (*bp == '\0') {
							if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
								errlogPrintf("save_restore:array_restore: *** premature EOF.\n");
								end_of_file = 1;
								break;
							}
							if (save_restoreDebug >= 11) {
								errlogPrintf("dbrestore:SR_array_restore: new buffer: '%s'\n", bp);
							}
							if (*bp == ELEMENT_END) break;
						} else if ((*bp == ESCAPE) && ((bp[1] == ELEMENT_BEGIN) || (bp[1] == ELEMENT_END))) {
							/* escaped character */
							bp++;
						}
						if (isprint((int)(*bp))) string[j++] = *bp; /* Ignore, e.g., embedded newline */
					}
					string[j] = '\0';
					if (save_restoreDebug >= 10) {
						errlogPrintf("dbrestore:SR_array_restore: element[%ld] value = '%s'\n", num_read, string);
						if (bp) errlogPrintf("dbrestore:SR_array_restore: look for element-end: buffer contains '%s'\n", bp);
					}

					/*
					 * We've accumulated all the characters, or all we can handle in string[].
					 * If there are more characters than we can handle, just pretend we read them.
					 */
					/* *bp == ELEMENT_END ,*/
 					for (found = 0; (found == 0) && !end_of_file; ) {
						while (*bp && (*bp != ELEMENT_END) && (*bp != ESCAPE)) bp++;
						switch (*bp) {
						case ELEMENT_END:
							found = 1; bp++; break;
						case ESCAPE:
							if (*(++bp) == ELEMENT_END) bp++; break;
						default:
							if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
								end_of_file = 1;
								found = 1;
							}
						}
					}
					/* Append value to local array. */
					if (p_data) {
						switch (field_type) {
						case DBF_STRING:
							/* future: translate escape sequence */
							strncpy(&(p_char[(num_read++)*MAX_STRING_SIZE]), string, MAX_STRING_SIZE);
							break;
						case DBF_ENUM: case DBF_USHORT: case DBF_MENU:
							p_ushort[num_read++] = (unsigned short)atol(string);
							break;
						case DBF_UCHAR:
							p_uchar[num_read++] = (unsigned char)atol(string);
							break;
						case DBF_CHAR:
							p_char[num_read++] = (char)atol(string);
							break;
						case DBF_SHORT:
							p_short[num_read++] = (short)atol(string);
							break;
						case DBF_LONG:
							p_long[num_read++] = atol(string);
							break;
						case DBF_ULONG:
							p_ulong[num_read++] = (unsigned long)atol(string);
							break;
						case DBF_FLOAT:
							p_float[num_read++] = mySafeDoubleToFloat(atof(string));
							break;
						case DBF_DOUBLE:
							p_double[num_read++] = atof(string);
							break;
						case DBF_NOACCESS:
						default:
							break;
						}
					}
				}
			} /* for (num_read=0; (num_read<max_elements) && bp && !end_mark_found; ) */

			if ((save_restoreDebug >= 10) && p_data) {
				errlogPrintf("\nsave_restore: %ld array values:\n", num_read);
				for (j=0; j<num_read; j++) {
					switch (field_type) {
					case DBF_STRING:
						errlogPrintf("	'%s'\n", &(p_char[j*MAX_STRING_SIZE])); break;
					case DBF_ENUM: case DBF_USHORT: case DBF_MENU:
						errlogPrintf("	%u\n", p_ushort[j]); break;
					case DBF_SHORT:
						errlogPrintf("	%d\n", p_short[j]); break;
					case DBF_UCHAR:
						errlogPrintf("	'%c' (%u)\n", p_uchar[j], p_uchar[j]); break;
					case DBF_CHAR:
						errlogPrintf("	'%c' (%d)\n", p_char[j], p_char[j]); break;
					case DBF_ULONG:
						errlogPrintf("	%lu\n", p_ulong[j]); break;
					case DBF_LONG:
						errlogPrintf("	%ld\n", p_long[j]); break;
					case DBF_FLOAT:
						errlogPrintf("	%f\n", p_float[j]); break;
					case DBF_DOUBLE:
						errlogPrintf("	%g\n", p_double[j]); break;
					case DBF_NOACCESS:
					default:
						break;
					}
				}
				errlogPrintf("save_restore: end of array values.\n\n");
				epicsThreadSleep(0.5);
			}

		} /* if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) != NULL) */
	}
	/* leave the file pointer ready for next PV (next fgets() should yield next PV) */
	if (begin_mark_found) {
		/* find ARRAY_END (but ARRAY_END inside an element is just another character) */
		if (save_restoreDebug >= 10) {
			errlogPrintf("dbrestore:SR_array_restore: looking for ARRAY_END\n");
		}
		in_element = 0;
		while (!end_mark_found && !end_of_file) {
			if (save_restoreDebug >= 11) {
				errlogPrintf("dbrestore:SR_array_restore: ...buffer contains '%s'\n", bp);
			}
			switch (*bp) {
			case ESCAPE:
				if (in_element && (bp[1] == ELEMENT_END)) bp++; /* two chars treated as one */
				break;
			case ARRAY_END:
				if (!in_element) end_mark_found = 1;
				break;
			case '\0':
				if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
					errlogPrintf("save_restore: *** EOF during array-end search\n");
					end_of_file = 1;
				}
				break;
			default:
				/* Can't use ELEMENT_BEGIN, ELEMENT_END as cases; they might be the same. */
				if ((*bp == ELEMENT_BEGIN) || (*bp == ELEMENT_END)) in_element = !in_element;
				break;
			}
			if (bp) ++bp;
		}
	} else {
		if (save_restoreDebug >= 10) {
			errlogPrintf("dbrestore:SR_array_restore: ARRAY_BEGIN wasn't found; going to next line of input file\n");
		}
		status = -1;
		/* just get next line, assuming it contains the next PV */
		if (!end_of_file) {
			if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) end_of_file = 1;
		}
	}
	if (!status && end_of_file) status = end_of_file;
	if (pass == 0) {
		if (save_restoreDebug >= 1) {
			errlogPrintf("dbrestore:SR_array_restore: No array write in pass 0.\n");
		}
	} else {
		if (!status && p_data) {
			if (save_restoreDebug >= 1) {
				errlogPrintf("dbrestore:SR_array_restore: Writing array to database\n");
			}
			status = SR_put_array_values(PVname, p_data, num_read);
		} else {
			if (save_restoreDebug >= 1) {
				errlogPrintf("dbrestore:SR_array_restore: No array write to database attempted because of error condition\n");
			}
		}
	}
	if (p_data == NULL) status = -1;
	return(status);
}


/*
 * file_restore
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database values during iocInit.  Before database initialization
 * must use STATIC database access routines.
 * Expect the following syntax:
 *    [<error string>]
 *    which is "! <number> <optional descriptive text>"
 * OR
 *    [<ignore>]<PV name><white>...<value>
 *    where
 *       <ignore> is the character '#'
 *       <PV name> is any legal EPICS PV name
 *       <white> is whitespace
 *       <value> is
 *          <printable>[<printable>...][<white>...][<anything>]
 *       e.g., "1.2"
 *       OR
 *          @array@ { "<val>" "<val>" }
 */
int reboot_restore(char *filename, initHookState init_state)
{
	char		PVname[81]; /* Must be greater than max field width ("%80s") in the sscanf format below */
	char		bu_filename[PATH_SIZE+1], fname[PATH_SIZE+1] = "";
	char		buffer[BUF_SIZE], *bp;
	char		value_string[BUF_SIZE];
	char		datetime[32];
	char		c;
	FILE		*inp_fd;
	int			i, found_field, pass;
	DBENTRY		dbentry;
	DBENTRY		*pdbentry = &dbentry;
	long		status;
	int			n, write_backup, num_errors, is_scalar;
	long		*pStatusVal = 0;
	char		*statusStr = 0;
	
	errlogPrintf("reboot_restore: entry for file '%s'\n", filename);
	printf("reboot_restore (v%s): entry for file '%s'\n", RESTORE_VERSION, filename);
	/* initialize database access routines */
	if (!pdbbase) {
		errlogPrintf("reboot_restore: No Database Loaded\n");
		return(OK);
	}
	dbInitEntry(pdbbase,pdbentry);

	/* what are we supposed to do here? */
	if (init_state >= initHookAfterInitDatabase) {
		pass = 1;
		for (i = 0; i < restoreFileList.pass1cnt; i++) {
			if (restoreFileList.pass1files[i] &&
				(strcmp(filename, restoreFileList.pass1files[i]) == 0)) {
				pStatusVal = &(restoreFileList.pass1Status[i]);
				statusStr = restoreFileList.pass1StatusStr[i];
			}
		}
	} else {
		pass = 0;
		for (i = 0; i < restoreFileList.pass0cnt; i++) {
			if (restoreFileList.pass0files[i] &&
				(strcmp(filename, restoreFileList.pass0files[i]) == 0)) {
				pStatusVal = &(restoreFileList.pass0Status[i]);
				statusStr = restoreFileList.pass0StatusStr[i];
			}
		}
	}

	if ((pStatusVal == 0) || (statusStr == 0)) {
		errlogPrintf("reboot_restore: Can't find filename '%s' in list.\n",
			filename);
	} else {
		errlogPrintf("reboot_restore: Found filename '%s' in restoreFileList.\n",
			filename);
	}

	/* open file */
	strNcpy(fname, saveRestoreFilePath, sizeof(fname) -1);
	strncat(fname, filename, MAX(sizeof(fname) -1 - strlen(fname),0));
	errlogPrintf("*** restoring from '%s' at initHookState %d (%s record/device init) ***\n",
		fname, (int)init_state, pass ? "after" : "before");
	if ((inp_fd = fopen_and_check(fname, &status)) == NULL) {
		errlogPrintf("save_restore: Can't open save file.");
		if (pStatusVal) *pStatusVal = SR_STATUS_FAIL;
		if (statusStr) strcpy(statusStr, "Can't open save file.");
		dbFinishEntry(pdbentry);
		return(ERROR);
	}
	if (status) {
		if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
		if (statusStr) strcpy(statusStr, "Bad .sav(B) files; used seq. backup");
	}

	(void)fgets(buffer, BUF_SIZE, inp_fd); /* discard header line */
	if (save_restoreDebug >= 1) {
		errlogPrintf("dbrestore:reboot_restore: header line '%s'\n", buffer);
	}
	status = fseek(inp_fd, 0, SEEK_SET); /* go to beginning */
	if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);

	/* restore from data file */
	num_errors = 0;
	while ((bp=fgets(buffer, BUF_SIZE, inp_fd))) {
		/*
		 * get PV_name, one space character, value
		 * (value may be a string with leading whitespace; it may be
		 * entirely whitespace; the number of spaces may be crucial;
		 * it might consist of zero characters; and it might be an array.)
		 * If the value is an array, it has the form @array@ { "val1" "val2" .. }
		 * the array of values may be broken at any point by '\n', or other
		 * character for which isprint() is false.
		 * sample input lines:
		 * xxx:interp.E 100
		 * xxx:interp.C @array@ { "1" "0.99" }
		 */
		n = sscanf(bp,"%80s%c%[^\n\r]", PVname, &c, value_string);
		if (n<3) *value_string = 0;
		if (PVname[0] == '#') /* user must have edited the file manually; accept this line as a comment */
			continue;
		if (strlen(PVname) >= 80) {
			/* must a munged input line */
			errlogPrintf("save_restore: '%s' is too long to be a PV name.\n", PVname);
			continue;
		}
		if (isalpha((int)PVname[0]) || isdigit((int)PVname[0])) {
			if (strchr(PVname,'.') == 0) strcat(PVname,".VAL"); /* if no field name, add default */
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (save_restoreDebug > 9) errlogPrintf("\n");
			if (save_restoreDebug >= 10) {
				errlogPrintf("dbrestore:reboot_restore: Attempting to put %s '%s' to '%s'\n", is_scalar?"scalar":"array", value_string, PVname);
			}
			found_field = 1;
			if ((status = dbFindRecord(pdbentry, PVname)) != 0) {
				errlogPrintf("dbFindRecord for '%s' failed\n", PVname);
				num_errors++; found_field = 0;
			} else if (dbFoundField(pdbentry) == 0) {
				errlogPrintf("save_restore: dbFindRecord did not find field '%s'\n", PVname);
				num_errors++; found_field = 0;
			}
			if (found_field) {
				if (is_scalar) {
					status = scalar_restore(pass, pdbentry, PVname, value_string);
				} else {
					status = SR_array_restore(pass, inp_fd, PVname, value_string);
				}
				if (status) {
					errlogPrintf("save_restore: restore for PV '%s' failed\n", PVname);
					num_errors++;
				}
			} /* if (found_field) {... */
		} else if (PVname[0] == '!') {
			/*
			* string is an error message -- something like:
			* '! 7 channel(s) not connected - or not all gets were successful'
			*/
			n = (int)atol(&bp[1]);
			errlogPrintf("%d %s had no saved value.\n", n, n==1?"PV":"PVs");
			if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
			if (statusStr) strcpy(statusStr, ".sav file contained an error message");
			if (!save_restoreIncompleteSetsOk) {
				errlogPrintf("aborting restore\n");
				fclose(inp_fd);
				dbFinishEntry(pdbentry);
				if (pStatusVal) *pStatusVal = SR_STATUS_FAIL;
				if (statusStr) strcpy(statusStr, "restore aborted");
				return(ERROR);
			}
		} else if (PVname[0] == '<') {
			/* end of file */
			break;
		}
	}
	fclose(inp_fd);
	dbFinishEntry(pdbentry);

	/* If this is the second pass for a restore file, don't write backup file again.*/
	write_backup = 1;
	if (init_state >= initHookAfterInitDatabase) {
		for(i = 0; i < restoreFileList.pass0cnt; i++) {
			if (strcmp(filename, restoreFileList.pass0files[i]) == 0) {
				write_backup = 0;
				break;
			}
		}
	}

	if (write_backup) {
		/* write  backup file*/
		if (save_restoreDatedBackupFiles && (fGetDateStr(datetime) == 0)) {
			strNcpy(bu_filename, fname, sizeof(bu_filename) - 1 - strlen(datetime));
			strcat(bu_filename, "_");
			strcat(bu_filename, datetime);
		} else {
			strNcpy(bu_filename, fname, sizeof(bu_filename) - 3);
			strcat(bu_filename, ".bu");
		}
		if (save_restoreDebug >= 1) {
			errlogPrintf("dbrestore:reboot_restore: writing boot-backup file '%s'.\n", bu_filename);
		}
		status = (long)myFileCopy(fname,bu_filename);
		if (status) {
			errlogPrintf("save_restore: Can't write backup file.\n");
			if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
			if (statusStr) strcpy(statusStr, "Can't write backup file");
			return(OK);
		}
	}

	/* Record status */
	if (pStatusVal && statusStr) {
		if (*pStatusVal != 0) {
			/* Status and message have already been recorded */
			;
		} else if (num_errors != 0) {
			sprintf(statusStr, "%d %s", num_errors, num_errors==1?"PV error":"PV errors");
			*pStatusVal = SR_STATUS_WARN;
		} else {
			strcpy(statusStr, "No errors");
			*pStatusVal = SR_STATUS_OK;
		}
	}
	if (p_data) {
		free(p_data);
		p_data = NULL;
		p_data_size = 0;
	}
	errlogPrintf("reboot_restore: done with file '%s'\n\n", filename);
	return(OK);
}


int set_pass0_restoreFile( char *filename)
{
	char *cp;
	int fileNum = restoreFileList.pass0cnt;

	if (fileNum >= MAXRESTOREFILES) {
		errlogPrintf("set_pass0_restoreFile: MAXFILE count exceeded\n");
		return(ERROR);
	}
	cp = (char *)calloc(strlen(filename) + 4,sizeof(char));
	restoreFileList.pass0files[fileNum] = cp;
	if (cp == NULL) {
		errlogPrintf("set_pass0_restoreFile: calloc failed\n");
		restoreFileList.pass0StatusStr[fileNum] = (char *)0;
		return(ERROR);
	}
	strcpy(cp, filename);
	cp = (char *)calloc(STRING_LEN, 1);
	restoreFileList.pass0StatusStr[fileNum] = cp;
	strcpy(cp, "Unknown, probably failed");
	restoreFileList.pass0Status[fileNum] = SR_STATUS_INIT;
	restoreFileList.pass0cnt++;
	return(OK);
}

int set_pass1_restoreFile(char *filename)
{
	char *cp;
	int fileNum = restoreFileList.pass1cnt;
	
	if (fileNum >= MAXRESTOREFILES) {
		errlogPrintf("set_pass1_restoreFile: MAXFILE count exceeded\n");
		return(ERROR);
	}
	cp = (char *)calloc(strlen(filename) + 4,sizeof(char));
	restoreFileList.pass1files[fileNum] = cp;
	if (cp == NULL) {
		errlogPrintf("set_pass1_restoreFile: calloc failed\n");
		restoreFileList.pass1StatusStr[fileNum] = (char *)0;
		return(ERROR);
	}
	strcpy(cp, filename);
	cp = (char *)calloc(STRING_LEN, 1);
	restoreFileList.pass1StatusStr[fileNum] = cp;
	strcpy(cp, "Unknown, probably failed");
	restoreFileList.pass1Status[fileNum] = SR_STATUS_INIT;
	restoreFileList.pass1cnt++;
	return(OK);
}

/* file is ok if it ends in either of the two following ways:
 * <END>?
 * <END>??
 * where '?' is any character - typically \n or \r
 */
FILE *checkFile(const char *file)
{
	FILE *inp_fd = NULL;
	char tmpstr[PATH_SIZE+50], *versionstr;
	double version;
	char datetime[32];
	int status;

	if ((inp_fd = fopen(file, "r")) == NULL) {
		errlogPrintf("save_restore: Can't open file '%s'.\n", file);
		return(0);
	}

	/* Get the version number of the code that wrote the file */
	fgets(tmpstr, 29, inp_fd);
	versionstr = strchr(tmpstr,(int)'V');
	if (!versionstr) {
		/* file has no version number */
		status = fseek(inp_fd, 0, SEEK_SET); /* go to beginning */
		if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
		return(inp_fd);	/* Assume file is ok */
	}
	if (isdigit((int)versionstr[1]))
		version = atof(versionstr+1);
	else
		version = 0;

	/* <END> check started in v1.8 */
	if (version < 1.8) {
		status = fseek(inp_fd, 0, SEEK_SET); /* go to beginning */
		if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
		return(inp_fd);	/* Assume file is ok. */
	}
	/* check out "successfully written" marker */
	status = fseek(inp_fd, -6, SEEK_END);
	if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
	fgets(tmpstr, 6, inp_fd);
	if (strncmp(tmpstr, "<END>", 5) == 0) {
		status = fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
		if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
		return(inp_fd);
	}
	
	status = fseek(inp_fd, -7, SEEK_END);
	if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
	fgets(tmpstr, 7, inp_fd);
	if (strncmp(tmpstr, "<END>", 5) == 0) {
		status = fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
		if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
		return(inp_fd);
	}

	/* file is bad */
	fclose(inp_fd);
	errlogPrintf("save_restore: File '%s' is not trusted.\n", file);
	strcpy(tmpstr, file);
	strcat(tmpstr, "_RBAD_");
	if (save_restoreDatedBackupFiles) {
		fGetDateStr(datetime);
		strcat(tmpstr, datetime);
	}
	(void)myFileCopy(file, tmpstr);
	return(0);
}


FILE *fopen_and_check(const char *fname, long *status)
{
	FILE *inp_fd = NULL;
	char file[PATH_SIZE+1];
	int i, backup_sequence_num;
	struct stat fileStat;
	char *p;
	time_t currTime;
	double dTime, min_dTime;
	
	*status = 0;	/* presume success */
	strncpy(file, fname, PATH_SIZE);
	inp_fd = checkFile(file);
	if (inp_fd) return(inp_fd);

	/* Still here?  Try the backup file. */
	strncat(file, "B", 1);
	errlogPrintf("save_restore: Trying backup file '%s'\n", file);
	inp_fd = checkFile(file);
	if (inp_fd) return(inp_fd);

	/*** Still haven't found a good file?  Try the sequenced backups ***/
	/* Find the most recent one. */
	*status = 1;
	strcpy(file, fname);
	backup_sequence_num = -1;
	p = &file[strlen(file)];
	currTime = time(NULL);
	min_dTime = 1.e9;
	for (i=0; i<save_restoreNumSeqFiles; i++) {
		sprintf(p, "%1d", i);
		if (stat(file, &fileStat) == 0) {
			/*
			 * Clocks might be unsynchronized, so it's possible
			 * the most recent file has a time in the future.
			 * For now, just choose the file whose date/time is
			 * closest to the current date/time.
			 */
			dTime = fabs(difftime(currTime, fileStat.st_mtime));
			if (save_restoreDebug >= 5) {
				errlogPrintf("'%s' modified at %s\n", file,
					ctime(&fileStat.st_mtime));
				errlogPrintf("'%s' is %f seconds old\n", file, dTime);
			}
			if (dTime < min_dTime) {
				min_dTime = dTime;
				backup_sequence_num = i;
			}
		}
	}

	if (backup_sequence_num == -1) {
		/* Clock are way messed up.  Just try backup 0. */
		backup_sequence_num = 0;
		sprintf(p, "%1d", backup_sequence_num);
		errlogPrintf("save_restore: Can't figure out which seq file is most recent,\n");
		errlogPrintf("save_restore: so I'm just going to start with '%s'.\n", file);
	}

	/* Try the sequenced backup files. */
	for (i=0; i<save_restoreNumSeqFiles; i++) {
		sprintf(p, "%1d", backup_sequence_num);
		errlogPrintf("save_restore: Trying backup file '%s'\n", file);
		inp_fd = checkFile(file);
		if (inp_fd) return(inp_fd);

		/* Next.  Order might be, e.g., "1,2,0", if 1 is most recent of 3 files */
		if (++backup_sequence_num >= save_restoreNumSeqFiles)
			backup_sequence_num = 0;
	}

	errlogPrintf("save_restore: Can't find a file to restore from...");
	errlogPrintf("save_restore: ...last tried '%s'. I give up.\n", file);
	errlogPrintf("save_restore: **********************************\n\n");
	return(0);
}


/*
 * These functions really belong to save_restore.c, but they use
 * database access, which is incompatible with cadef.h included in
 * save_restore.c.
 */
 
long SR_get_array_info(char *name, long *num_elements, long *field_size, long *field_type)
{
	DBADDR		dbaddr;
	DBADDR		*paddr = &dbaddr;
	long		status;

	status = dbNameToAddr(name, paddr);
	if (status) return(status);
	*num_elements = paddr->no_elements;
	*field_size = paddr->field_size;
	*field_type = paddr->field_type;
	return(0);
}


long SR_get_array(char *PVname, void *pArray, long *pnum_elements)
{
	DBADDR		dbaddr;
	DBADDR		*paddr = &dbaddr;
	long		status;
	dbfType		request_field_type;

	status = dbNameToAddr(PVname, paddr);
	if (status) return(status);
	dbScanLock((dbCommon *)paddr->precord);
	request_field_type = paddr->field_type;
	/*
	 * Not clear what we should do if someone has an array of enums
	 * or menu items.  For now, just do something that will work
	 * in the simplest case.
	 */
	if ((request_field_type == DBF_ENUM) || (request_field_type == DBF_MENU)) {
		errlogPrintf("save_restore:SR_get_array: field_type %s array read as DBF_USHORT\n",
			pamapdbfType[request_field_type].strvalue);
		request_field_type = DBF_USHORT;
	}
	status = dbGet(paddr, request_field_type, pArray, NULL, pnum_elements, NULL);
	if (save_restoreDebug >= 10) {
		errlogPrintf("dbrestore:SR_get_array: '%s' currently has %ld elements\n", PVname, *pnum_elements);
	}
	dbScanUnlock((dbCommon *)paddr->precord);
	return(status);
}

long SR_write_array_data(FILE *out_fd, char *name, void *pArray, long num_elements)
{
	DBADDR		dbaddr;
	DBADDR		*paddr = &dbaddr;
	long		status;
	dbfType		field_type;
	long		i, j, n;
	char			*p_char = NULL, *pc;
	short			*p_short = NULL;
	long			*p_long = NULL;
	unsigned char	*p_uchar = NULL;
	unsigned short	*p_ushort = NULL;
	unsigned long	*p_ulong = NULL;
	float			*p_float = NULL;
	double			*p_double = NULL;

	status = dbNameToAddr(name, paddr);
	if (status) return(0);
	field_type = paddr->field_type;

	n = fprintf(out_fd, "%-s %1c ", ARRAY_MARKER, ARRAY_BEGIN);
	for (i=0; i<num_elements; i++) {
		switch(field_type) {
		case DBF_STRING:
			p_char = (char *)pArray;
			pc = &p_char[i*MAX_STRING_SIZE];
			n += fprintf(out_fd, "%1c", ELEMENT_BEGIN);
			for (j=0; j<MAX_STRING_SIZE-1 && *pc; j++, pc++) {
				if ((*pc == ELEMENT_BEGIN) || (*pc == ELEMENT_END)){
					n += fprintf(out_fd, "%1c", ESCAPE);
					j++;
				}
				n += fprintf(out_fd, "%1c", *pc);
			}
			n += fprintf(out_fd, "%1c ", ELEMENT_END);
			break;
		case DBF_CHAR:
			p_char = (char *)pArray;
			n += fprintf(out_fd, "%1c%d%1c ", ELEMENT_BEGIN, p_char[i], ELEMENT_END);
			break;
		case DBF_UCHAR:
			p_uchar = (unsigned char *)pArray;
			n += fprintf(out_fd, "%1c%u%1c ", ELEMENT_BEGIN, p_uchar[i], ELEMENT_END);
			break;
		case DBF_SHORT:
			p_short = (short *)pArray;
			n += fprintf(out_fd, "%1c%d%1c ", ELEMENT_BEGIN, p_short[i], ELEMENT_END);
			break;
		case DBF_ENUM: case DBF_USHORT: case DBF_MENU:
			p_ushort = (unsigned short *)pArray;
			n += fprintf(out_fd, "%1c%u%1c ", ELEMENT_BEGIN, p_ushort[i], ELEMENT_END);
			break;
		case DBF_LONG:
			p_long = (long *)pArray;
			n += fprintf(out_fd, "%1c%ld%1c ", ELEMENT_BEGIN, p_long[i], ELEMENT_END);
			break;
		case DBF_ULONG:
			p_ulong = (unsigned long *)pArray;
			n += fprintf(out_fd, "%1c%lu%1c ", ELEMENT_BEGIN, p_ulong[i], ELEMENT_END);
			break;
		case DBF_FLOAT:
			p_float = (float *)pArray;
			n += fprintf(out_fd, "%1c", ELEMENT_BEGIN);
			n += fprintf(out_fd, FLOAT_FMT, p_float[i]);
			n += fprintf(out_fd, "%1c ", ELEMENT_END);
			break;
		case DBF_DOUBLE:
			p_double = (double *)pArray;
			n += fprintf(out_fd, "%1c", ELEMENT_BEGIN);
			n += fprintf(out_fd, DOUBLE_FMT, p_double[i]);
			n += fprintf(out_fd, "%1c ", ELEMENT_END);
			break;
		default:
			errlogPrintf("save_restore: field_type %d not handled.\n", (int) field_type);
			break;
		}
	}
	n += fprintf(out_fd, "%1c\n", ARRAY_END);
	return(n);
}

#define BUFFER_SIZE 100
/*
 * Look through the database for info nodes with the specified info_name, and get the
 * associated info_value string.  Interpret this string as a list of field names.  Write
 * the PV's thus accumulated to the file <fileBaseName>.  (If <fileBaseName> doesn't contain
 * ".req", append it.)
 */
void makeAutosaveFileFromDbInfo(char *fileBaseName, char *info_name)
{
	DBENTRY		dbentry;
	DBENTRY		*pdbentry = &dbentry;
	const char *info_value, delimiters[] = " \t\n\r.";
	char		buf[BUFFER_SIZE], *field, *fields=buf;
	FILE 		*out_fd;
	int			searchRecord;

	if (!pdbbase) {
		errlogPrintf("autosave:makeAutosaveFileFromDbInfo: No Database Loaded\n");
		return;
	}
	if (strstr(fileBaseName, ".req")) {
		strncpy(buf, fileBaseName, BUFFER_SIZE);
	} else {
		sprintf(buf, "%s.req", fileBaseName);
	}
	if ((out_fd = fopen(buf,"w")) == NULL) {
		errlogPrintf("save_restore:makeAutosaveFileFromDbInfo - unable to open file '%s'\n", buf);
		return;
	}

	dbInitEntry(pdbbase,pdbentry);
	/* loop over all record types */
	dbFirstRecordType(pdbentry);
	do {
		/* loop over all records of current type*/
		dbFirstRecord(pdbentry);
#if GE_EPICSBASE(3,14,11)
		searchRecord = dbIsAlias(pdbentry) ? 0 : 1;
#else
		searchRecord = 1;
#endif
		do {
			if (searchRecord) {
			info_value = dbGetInfo(pdbentry, info_name);
			if (info_value) {
				/* printf("record %s.autosave = '%s'\n", dbGetRecordName(pdbentry), info_value); */
				strncpy(fields, info_value, BUFFER_SIZE);
				for (field = strtok(fields, delimiters); field; field = strtok(NULL, delimiters)) {
					if (dbFindField(pdbentry, field) == 0) {
						fprintf(out_fd, "%s.%s\n", dbGetRecordName(pdbentry), field);
					} else {
						printf("makeAutosaveFileFromDbInfo: %s.%s not found\n", dbGetRecordName(pdbentry), field);
					}
				}
				}
			}
		} while (dbNextRecord(pdbentry) == 0);
	} while (dbNextRecordType(pdbentry) == 0);
	dbFinishEntry(pdbentry);
	fclose(out_fd);
	return;
}

/* set_pass0_restoreFile() */
STATIC const iocshArg set_passN_Arg = {"file",iocshArgString};
STATIC const iocshArg * const set_passN_Args[1] = {&set_passN_Arg};
STATIC const iocshFuncDef set_pass0_FuncDef = {"set_pass0_restoreFile",1,set_passN_Args};
STATIC void set_pass0_CallFunc(const iocshArgBuf *args)
{
    set_pass0_restoreFile(args[0].sval);
}

/* set_pass1_restoreFile() */
STATIC const iocshFuncDef set_pass1_FuncDef = {"set_pass1_restoreFile",1,set_passN_Args};
STATIC void set_pass1_CallFunc(const iocshArgBuf *args)
{
    set_pass1_restoreFile(args[0].sval);
}

/* void dbrestoreShow(void) */
STATIC const iocshFuncDef dbrestoreShow_FuncDef = {"dbrestoreShow",0,NULL};
STATIC void dbrestoreShow_CallFunc(const iocshArgBuf *args)
{
    dbrestoreShow();
}

/* void makeAutosaveFileFromDbInfo(char *filename, char *info_name) */
STATIC const iocshArg makeAutosaveFileFromDbInfo_Arg0 = {"filename",iocshArgString};
STATIC const iocshArg makeAutosaveFileFromDbInfo_Arg1 = {"info_name",iocshArgString};
STATIC const iocshArg * const makeAutosaveFileFromDbInfo_Args[2] = {&makeAutosaveFileFromDbInfo_Arg0, &makeAutosaveFileFromDbInfo_Arg1};
STATIC const iocshFuncDef makeAutosaveFileFromDbInfo_FuncDef = {"makeAutosaveFileFromDbInfo",2,makeAutosaveFileFromDbInfo_Args};
STATIC void makeAutosaveFileFromDbInfo_CallFunc(const iocshArgBuf *args)
{
    makeAutosaveFileFromDbInfo(args[0].sval, args[1].sval);
}

/* void makeAutosaveFiles(void) */
STATIC const iocshFuncDef makeAutosaveFiles_FuncDef = {"makeAutosaveFiles",0,NULL};
STATIC void makeAutosaveFiles_CallFunc(const iocshArgBuf *args)
{
    makeAutosaveFileFromDbInfo("info_settings.req", "autosaveFields");
    makeAutosaveFileFromDbInfo("info_positions.req", "autosaveFields_pass0");
}

void dbrestoreRegister(void)
{
    iocshRegister(&set_pass0_FuncDef, set_pass0_CallFunc);
    iocshRegister(&set_pass1_FuncDef, set_pass1_CallFunc);
	iocshRegister(&dbrestoreShow_FuncDef, dbrestoreShow_CallFunc);
	iocshRegister(&makeAutosaveFileFromDbInfo_FuncDef, makeAutosaveFileFromDbInfo_CallFunc);
	iocshRegister(&makeAutosaveFiles_FuncDef, makeAutosaveFiles_CallFunc);
}

epicsExportRegistrar(dbrestoreRegister);
