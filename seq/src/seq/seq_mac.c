/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory


 	seq_mac.c,v 1.2 1995/06/27 15:25:56 wright Exp

	DESCRIPTION: Macro routines for Sequencer.
	The macro table contains name & value pairs.  These are both pointers
	to strings.

	ENVIRONMENT: VxWorks

	HISTORY:
01mar94,ajk	Added seq_macValueGet() as state program interface routine.
29apr99,wfl     Avoided compilation warnings.
17may99,wfl	Replaced VxWorks dependencies with OSI calls; avoided step
		beyond end of macro definition string.
29feb00,wfl	Converted to new OSI (and errlogPrintf).
***************************************************************************/

#include	<ctype.h>
#include	<stdlib.h>
#include	<string.h>

#define epicsExportSharedSymbols
#include	"seq.h"

LOCAL int seqMacParseName(char *);
LOCAL int seqMacParseValue(char *);
LOCAL char *skipBlanks(char *);
LOCAL MACRO *seqMacTblGet(MACRO *, char *);

/*#define	DEBUG*/

/* 
 *seqMacEval - substitute macro value into a string containing:
 * ....{mac_name}....
 */
void seqMacEval(pInStr, pOutStr, maxChar, pMac)
char	*pInStr;
char	*pOutStr;
long	maxChar;
MACRO	*pMac;
{
	char		name[50], *pValue, *pTmp;
	int		nameLth, valLth;

#ifdef	DEBUG
	errlogPrintf("seqMacEval: InStr=%s\n", pInStr);
	epicsThreadSleep(0.5);
#endif	/*DEBUG*/
	pTmp = pOutStr;
	while (*pInStr != 0 && maxChar > 0)
	{
		if (*pInStr == '{')
		{	/* Do macro substitution */
			pInStr++; /* points to macro name */
			/* Copy the macro name */
			nameLth = 0;
			while (*pInStr != '}' && *pInStr != 0)
			{
				name[nameLth] = *pInStr++;
				if (nameLth < (sizeof(name) - 1))
					nameLth++;
			}
			name[nameLth] = 0;
			if (*pInStr != 0)
				pInStr++;
				
#ifdef	DEBUG
			errlogPrintf("Macro name=%s\n", name);
			epicsThreadSleep(0.5);
#endif	/*DEBUG*/
			/* Find macro value from macro name */
			pValue = seqMacValGet(pMac, name);
			if (pValue != NULL)
			{	/* Substitute macro value */
				valLth = strlen(pValue);
				if (valLth > maxChar)
					valLth = maxChar;
#ifdef	DEBUG
				errlogPrintf("Value=%s\n", pValue);
#endif	/*DEBUG*/
				strncpy(pOutStr, pValue, valLth);
				maxChar -= valLth;
				pOutStr += valLth;
			}
			
		}
		else
		{	/* Straight susbstitution */
			*pOutStr++ = *pInStr++;
			maxChar--;
		}
	}
	*pOutStr = 0;
#ifdef	DEBUG
	errlogPrintf("OutStr=%s\n", pTmp);
	epicsThreadSleep(0.5);
#endif	/*DEBUG*/
}
/* 
 * seq_macValueGet - given macro name, return pointer to its value.
 */
epicsShareFunc char	*epicsShareAPI seq_macValueGet(ssId, pName)
SS_ID		ssId;
char		*pName;
{
	SPROG		*pSP;
	MACRO		*pMac;

	pSP = ((SSCB *)ssId)->sprog;
	pMac = pSP->pMacros;

	return seqMacValGet(pMac, pName);
}
/*
 * seqMacValGet - internal routine to convert macro name to macro value.
 */
char *seqMacValGet(pMac, pName)
MACRO		*pMac;
char		*pName;
{
	int		i;

#ifdef	DEBUG
	errlogPrintf("seqMacValGet: name=%s", pName);
#endif	/*DEBUG*/
	for (i = 0 ; i < MAX_MACROS; i++, pMac++)
	{
		if (pMac->pName != NULL)
		{
			if (strcmp(pName, pMac->pName) == 0)
			{
#ifdef	DEBUG
				errlogPrintf(", value=%s\n", pMac->pValue);
#endif	/*DEBUG*/
				return pMac->pValue;
			}
		}
	}
#ifdef	DEBUG
	errlogPrintf(", no value\n");
#endif	/*DEBUG*/
	return NULL;
}
/*
 * seqMacParse - parse the macro definition string and build
 * the macro table (name/value pairs). Returns number of macros parsed.
 * Assumes the table may already contain entries (values may be changed).
 * String for name and value are allocated dynamically from pool.
 */
long seqMacParse(pMacStr, pSP)
char		*pMacStr;	/* macro definition string */
SPROG		*pSP;
{
	int		nChar;
	MACRO		*pMac;		/* macro table */
	MACRO		*pMacTbl;	/* macro tbl entry */
	char		*pName, *pValue;

	if (pMacStr == NULL) pMacStr = "";

	pMac = pSP->pMacros;
	for ( ;; )
	{
		/* Skip blanks */
		pMacStr = skipBlanks(pMacStr);

		/* Parse the macro name */

		nChar = seqMacParseName(pMacStr);
		if (nChar == 0)
			break; /* finished or error */
		pName = (char *)calloc(nChar+1, 1);
		if (pName == NULL)
			break;
		memcpy(pName, pMacStr, nChar);
		pName[nChar] = '\0';
#ifdef	DEBUG
		errlogPrintf("name=%s, nChar=%d\n", pName, nChar);
		epicsThreadSleep(0.5);
#endif	/*DEBUG*/
		pMacStr += nChar;

		/* Find a slot in the table */
		pMacTbl = seqMacTblGet(pMac, pName);
		if (pMacTbl == NULL)
			break; /* table is full */
		if (pMacTbl->pName == NULL)
		{	/* Empty slot, insert macro name */
			pMacTbl->pName = pName;
		}

		/* Skip over blanks and equal sign or comma */
		pMacStr = skipBlanks(pMacStr);
		if (*pMacStr == ',')
		{
			/* no value after the macro name */
			if (*pMacStr != '\0')
				pMacStr++;
			continue;
		}
		if (*pMacStr == '\0' || *pMacStr++ != '=')
			break;
		pMacStr = skipBlanks(pMacStr);

		/* Parse the value */
		nChar = seqMacParseValue(pMacStr);
		if (nChar == 0)
			break;

		/* Remove previous value if it exists */
		pValue = pMacTbl->pValue;
		if (pValue != NULL)
			free(pValue);

		/* Copy value string into newly allocated space */
		pValue = (char *)calloc(nChar+1, 1);
		if (pValue == NULL)
			break;
		pMacTbl->pValue = pValue;
		memcpy(pValue, pMacStr, nChar);
		pValue[nChar] = '\0';
#ifdef	DEBUG
		errlogPrintf("value=%s, nChar=%d\n", pValue, nChar);
		epicsThreadSleep(0.5);
#endif	/*DEBUG*/

		/* Skip past last value and over blanks and comma */
		pMacStr += nChar;
		pMacStr = skipBlanks(pMacStr);
		if (*pMacStr == '\0' || *pMacStr++ != ',')
			break;
	}
	if (*pMacStr == '\0')
		return 0;
	else
		return -1;
}

/*
 * seqMacParseName() - Parse a macro name from the input string.
 */
LOCAL int seqMacParseName(pStr)
char	*pStr;
{
	int	nChar;

	/* First character must be [A-Z,a-z] */
	if (!isalpha((int)*pStr))
		return 0;
	pStr++;
	nChar = 1;
	/* Character must be [A-Z,a-z,0-9,_] */
	while ( isalnum((int)*pStr) || *pStr == '_' )
	{
		pStr++;
		nChar++;
	}
	/* Loop terminates on any non-name character */
	return nChar;
}

/*
 * seqMacParseValue() - Parse a macro value from the input string.
 */
LOCAL int seqMacParseValue(pStr)
char	*pStr;
{
	int	nChar;

	nChar = 0;
	/* Character string terminates on blank, comma, or EOS */
	while ( (*pStr != ' ') && (*pStr != ',') && (*pStr != 0) )
	{
		pStr++;
		nChar++;
	}
	return nChar;
}

/* skipBlanks() - skip blank characters */
LOCAL char *skipBlanks(pChar)
char	*pChar;
{
	while (*pChar == ' ')
		pChar++;
	return	pChar;
}

/*
 * seqMacTblGet - find a match for the specified name, otherwise
 * return an empty slot in macro table.
 */
LOCAL MACRO *seqMacTblGet(pMac, pName)
MACRO	*pMac;
char	*pName;	/* macro name */
{
	int		i;
	MACRO		*pMacTbl;

#ifdef	DEBUG
	errlogPrintf("seqMacTblGet: name=%s\n", pName);
	epicsThreadSleep(0.5);
#endif	/*DEBUG*/
	for (i = 0, pMacTbl = pMac; i < MAX_MACROS; i++, pMacTbl++)
	{
		if (pMacTbl->pName != NULL)
		{
			if (strcmp(pName, pMacTbl->pName) == 0)
			{
				return pMacTbl;
			}
		}
	}

	/* Not found, find an empty slot */
	for (i = 0, pMacTbl = pMac; i < MAX_MACROS; i++, pMacTbl++)
	{
		if (pMacTbl->pName == NULL)
		{
			return pMacTbl;
		}
	}

	/* No empty slots available */
	return NULL;
}
