/*************************************************************************\
Copyright (c) 1990      The Regents of the University of California
                        and the University of Chicago.
                        Los Alamos National Laboratory
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                    Macro routines for Sequencer

TODO: Get rid of this and use the macLib from EPICS base.
\*************************************************************************/
#include "seq.h"
#include "seq_debug.h"

/* Macro table */
struct macro
{
	char	*name;
	char	*value;
	MACRO	*next;
};

static unsigned seqMacParseName(const char *str);
static unsigned seqMacParseValue(const char *str);
static const char *skipBlanks(const char *pchr);
static MACRO *seqMacTblGet(SPROG *sp, char *name);

/* 
 *seqMacEval - substitute macro value into a string containing:
 * ....{mac_name}....
 */
void seqMacEval(SPROG *sp, const char *inStr, char *outStr, size_t maxChar)
{
	char	name[50], *value, *tmp;
	size_t	valLth, nameLth;

	DEBUG("seqMacEval: InStr=%s, ", inStr);

	tmp = outStr;
	while (*inStr != 0 && maxChar > 0)
	{
		if (*inStr == '{')
		{	/* Do macro substitution */
			inStr++; /* points to macro name */
			/* Copy the macro name */
			nameLth = 0;
			while (*inStr != '}' && *inStr != 0)
			{
				name[nameLth] = *inStr++;
				if (nameLth < (sizeof(name) - 1))
					nameLth++;
			}
			name[nameLth] = 0;
			if (*inStr != 0)
				inStr++;
				
			DEBUG("Macro name=%s, ", name);

			/* Find macro value from macro name */
			value = seqMacValGet(sp, name);
			if (value != NULL)
			{	/* Substitute macro value */
				valLth = strlen(value);
				if (valLth > maxChar)
					valLth = maxChar;

				DEBUG("Value=%s, ", value);

				strncpy(outStr, value, valLth);
				maxChar -= valLth;
				outStr += valLth;
			}
			
		}
		else
		{	/* Straight substitution */
			*outStr++ = *inStr++;
			maxChar--;
		}
	}
	*outStr = 0;

	DEBUG("OutStr=%s\n", tmp);
}

/*
 * seqMacValGet - internal routine to convert macro name to macro value.
 */
char *seqMacValGet(SPROG *sp, const char *name)
{
	MACRO	*mac;

	DEBUG("seqMacValGet: name=%s", name);
	foreach(mac, sp->macros)
	{
		if (mac->name && strcmp(name, mac->name) == 0)
		{
			DEBUG(", value=%s\n", mac->value);
			return mac->value;
		}
	}
	DEBUG(", no value\n");
	return NULL;
}

/*
 * seqMacParse - parse the macro definition string and build
 * the macro table (name/value pairs). Returns number of macros parsed.
 * Assumes the table may already contain entries (values may be changed).
 * String for name and value are allocated dynamically from pool.
 */
void seqMacParse(SPROG *sp, const char *macStr)
{
	unsigned	nChar;
	MACRO		*mac;		/* macro tbl entry */
	char		*name, *value;

	if (macStr == NULL) return;

	while(*macStr)
	{
		/* Skip blanks */
		macStr = skipBlanks(macStr);

		/* Parse the macro name */
		nChar = seqMacParseName(macStr);
		if (nChar == 0)
			break;		/* finished or error */
		name = newArray(char, nChar+1);
		if (name == NULL)
		{
			errlogSevPrintf(errlogFatal, "seqMacParse: calloc failed\n");
			break;
		}
		memcpy(name, macStr, nChar);
		name[nChar] = '\0';

		DEBUG("name=%s, nChar=%d\n", name, nChar);

		macStr += nChar;

		/* Find a slot in the table */
		mac = seqMacTblGet(sp, name);
		if (mac == NULL)
			break;		/* table is full */
		if (mac->name == NULL)
		{	/* Empty slot, insert macro name */
			mac->name = name;
		}

		/* Skip over blanks and equal sign or comma */
		macStr = skipBlanks(macStr);
		if (*macStr == ',')
		{
			/* no value after the macro name */
			macStr++;
			continue;
		}
		if (*macStr == '\0' || *macStr++ != '=')
			break;
		macStr = skipBlanks(macStr);

		/* Parse the value */
		nChar = seqMacParseValue(macStr);
		if (nChar == 0)
			break;

		/* Remove previous value if it exists */
		value = mac->value;
		if (value != NULL)
			free(value);

		/* Copy value string into newly allocated space */
		value = newArray(char, nChar+1);
		if (value == NULL)
		{
			errlogSevPrintf(errlogFatal, "seqMacParse: calloc failed\n");
			break;
		}
		mac->value = value;
		memcpy(value, macStr, nChar);
		value[nChar] = '\0';

		DEBUG("value=%s, nChar=%d\n", value, nChar);

		/* Skip past last value and over blanks and comma */
		macStr += nChar;
		macStr = skipBlanks(macStr);
		if (*macStr == '\0' || *macStr++ != ',')
			break;
	}
}

/*
 * seqMacParseName() - Parse a macro name from the input string.
 */
static unsigned seqMacParseName(const char *str)
{
	unsigned nChar;

	/* First character must be [A-Z,a-z] */
	if (!isalpha((unsigned char)*str))
		return 0;
	str++;
	nChar = 1;
	/* Character must be [A-Z,a-z,0-9,_] */
	while ( isalnum((unsigned char)*str) || *str == '_' )
	{
		str++;
		nChar++;
	}
	/* Loop terminates on any non-name character */
	return nChar;
}

/*
 * seqMacParseValue() - Parse a macro value from the input string.
 */
static unsigned seqMacParseValue(const char *str)
{
	unsigned nChar;

	nChar = 0;
	/* Character string terminates on blank, comma, or EOS */
	while ( (*str != ' ') && (*str != ',') && (*str != 0) )
	{
		str++;
		nChar++;
	}
	return nChar;
}

/* skipBlanks() - skip blank characters */
static const char *skipBlanks(const char *pchr)
{
	while (*pchr == ' ')
		pchr++;
	return	pchr;
}

/*
 * seqMacTblGet - find a match for the specified name, otherwise
 * return a new empty slot in macro table.
 */
static MACRO *seqMacTblGet(SPROG *sp, char *name)
{
	MACRO	*mac, *lastMac = NULL;

	DEBUG("seqMacTblGet: name=%s\n", name);
	foreach(mac, sp->macros)
	{
		lastMac = mac;
		if (mac->name != NULL &&
			strcmp(name, mac->name) == 0)
		{
			return mac;
		}
	}
	/* Not found, allocate an empty slot */
	mac = new(MACRO);
	/* This assumes ptr assignment is atomic */
	if (lastMac != NULL)
		lastMac->next = mac;
	else
		sp->macros = mac;
	return mac;
}

/*
 * seqMacFree - free all the memory
 */
void seqMacFree(SPROG *sp)
{
	MACRO	*mac, *lastMac = NULL;

	foreach(mac, sp->macros)
	{
		free(mac->name);
		free(mac->value);
		free(lastMac);
		lastMac = mac;
	}
	free(lastMac);
	sp->macros = 0;
}
