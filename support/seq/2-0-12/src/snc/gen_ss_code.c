/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory
 	gen_ss_code.c,v 1.2 1995/06/27 15:25:43 wright Exp
	DESCRIPTION: gen_ss_code.c -- routines to generate state set code
	ENVIRONMENT: UNIX
	HISTORY:
19nov91,ajk	Changed find_var() to findVar().
28apr92,ajk	Implemented efClear() & efTestAndClear().
01mar94,ajk	Changed table generation to the new structures defined 
		in seqCom.h.
09aug96,wfl	Supported pvGetQ().
13aug96,wfl	Supported pvFreeQ().
23jun97,wfl	Avoided SEGV if variable or event flag was undeclared.
13jan98,wfl     Fixed handling of compound expressions, using E_COMMA.
29apr99,wfl     Avoided compilation warnings.
29apr99,wfl	Removed unnecessary include files.
06jul99,wfl	Cosmetic changes to improve look of generated C code
07sep99,wfl	Added support for local declarations (not yet complete);
		Added support for "pvName", "pvMessage" and pvPutComplete";
		Supported "pv" functions with array length and optional parms;
		Added sequencer variable name to generated seq_pv*() calls
22sep99,grw	Supported entry and exit actions
18feb00,wfl	More partial support for local declarations (still not done)
31mar00,wfl	Put 'when' code in a block (allows local declarations);
		supported entry handler
***************************************************************************/
#include	<stdio.h>
#include	<string.h>

#include	"parse.h"
#include	"proto.h"

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif  /*TRUE*/

/* func_name_to_code - convert function name to a code */
enum	fcode { F_DELAY, F_EFSET, F_EFTEST, F_EFCLEAR, F_EFTESTANDCLEAR,
		F_PVGET, F_PVGETQ, F_PVFREEQ, F_PVPUT, F_PVTIMESTAMP, F_PVASSIGN,
		F_PVMONITOR, F_PVSTOPMONITOR, F_PVCOUNT, F_PVINDEX, F_PVNAME,
		F_PVSTATUS, F_PVSEVERITY, F_PVMESSAGE, F_PVFLUSH, F_PVERROR,
		F_PVGETCOMPLETE, F_PVASSIGNED, F_PVCONNECTED, F_PVPUTCOMPLETE,
		F_PVCHANNELCOUNT, F_PVCONNECTCOUNT, F_PVASSIGNCOUNT,
		F_PVDISCONNECT, F_SEQLOG, F_MACVALUEGET, F_OPTGET,
		F_NONE };

char	*fcode_str[] = { "delay", "efSet", "efTest", "efClear", "efTestAndClear",
		"pvGet", "pvGetQ", "pvFreeQ", "pvPut", "pvTimeStamp", "pvAssign",
		"pvMonitor", "pvStopMonitor", "pvCount", "pvIndex", "pvName",
		"pvStatus", "pvSeverity", "pvMessage", "pvFlush", "pvError",
		"pvGetComplete", "pvAssigned", "pvConnected", "pvPutComplete",
		"pvChannelCount", "pvConnectCount", "pvAssignCount",
		"pvDisconnect", "seqLog", "macValueGet", "optGet",
		NULL };

void gen_delay_func(Expr *sp, Expr *ssp);
void eval_delay(Expr *ep, Expr *sp);
void gen_action_func(Expr *sp, Expr *ssp);
void gen_event_func(Expr *sp, Expr *ssp);
void eval_expr(int stmt_type, Expr *ep, Expr *sp, int level);
void indent(int level);
void gen_ef_func(int stmt_type,Expr *ep,Expr *sp,char *fname,
    enum fcode func_code);
void gen_pv_func(int stmt_type,Expr *ep,Expr *sp,
    char *fname,enum fcode func_code,int add_length,int num_params);
void gen_entry_handler(void);
void gen_exit_handler(void);
void gen_change_func(Expr *sp,Expr *ssp);
void gen_entry_func(Expr *sp,Expr *ssp );
void gen_exit_func(Expr *sp,Expr *ssp );

int state_block_index_from_name(Expr *ssp,char *state_name);
int special_func(int stmt_type,Expr *ep,Expr *sp);

/*+************************************************************************
*  NAME: gen_ss_code
*
*  CALLING SEQUENCE
*	type		argument	I/O	description
*	---------------------------------------------------
*
*  RETURNS:
*
*  FUNCTION: Generate state set C code from tables.
*
*  NOTES: All inputs are external globals.
*-*************************************************************************/
/*#define	DEBUG		1*/

#define	EVENT_STMT	1
#define	ACTION_STMT	2
#define	DELAY_STMT	3
#define	ENTRY_STMT	4
#define	EXIT_STMT	5

void gen_ss_code()
{
	extern Expr		*ss_list;
	Expr			*ssp;
	Expr			*sp;

	/* Generate entry handler code */
	gen_entry_handler();

	/* For each state set ... */
	for (ssp = ss_list; ssp != NULL; ssp = ssp->next)
	{
		/* For each state ... */
		for (sp = ssp->left; sp != NULL; sp = sp->next)
		{
			printf("\f/* Code for state \"%s\" in state set \"%s\" */\n",
			 sp->value, ssp->value);

			/* Generate entry and exit functions */
                        gen_change_func(sp, ssp);

			/* Generate function to set up for delay processing */
			gen_delay_func(sp, ssp);

			/* Generate event processing function */
			gen_event_func(sp, ssp);

			/* Generate action processing function */
			gen_action_func(sp, ssp);
		}
	}

	/* Generate exit handler code */
	gen_exit_handler();
}
/* Generate functions for each state which perform the state entry and 
  * exit actions. 
  */
void gen_change_func(Expr *sp,Expr *ssp)
{
        Expr		*ep;
	int isEntry = FALSE, isExit = FALSE;

	/* Don't write code for an entry or exit function unless there
	   was at least one entry or exit statement */

	for ( ep = sp->left; ep != NULL; ep = ep->next )
	{
		if ( ep->type == E_ENTRY )
		       isEntry = TRUE;
		else if ( ep->type == E_EXIT )
		       isExit = TRUE;
	}
	if (isEntry) gen_entry_func( sp, ssp );
	if (isExit) gen_exit_func( sp, ssp );
}

void gen_entry_func(Expr *sp,Expr *ssp )
{	
        Expr	*ep,			/* Entry expression */
		*xp;			/* statement expression walker */
	int	entryI =  0, line_num = -1;

	/* Entry function declaration */
	printf("\n/* Entry function for state \"%s\" in state set \"%s\" */\n",
 		sp->value, ssp->value);
	printf("static void I_%s_%s(SS_ID ssId, struct UserVar *pVar)\n{\n",
		ssp->value, sp->value);

	for ( ep = sp->left; ep != NULL; ep = ep->next )
	{
		if ( ep->type == E_ENTRY )
		{
			printf("/* Entry %d: */\n", ++entryI);
			for (xp = ep->right; xp != NULL; xp = xp->next)
		        {
		                 if (line_num != xp->line_num) 
                                 { 
                                         print_line_num(xp->line_num, xp->src_file); 
                                         line_num = xp->line_num; 
                                 } 
				 eval_expr(ACTION_STMT, xp, sp, 1);
			}
		}
	}

	/* end of function */
	printf("}\n");
	return;
}

void gen_exit_func(Expr *sp,Expr *ssp )
{	
        Expr	*ep,			/* Entry expression */
		*xp;			/* statement expression walker */
	int	exitI =  0, line_num = -1;

	/* Exit function declaration */
	printf("\n/* Exit function for state \"%s\" in state set \"%s\" */\n",
 		sp->value, ssp->value);
	printf("static void O_%s_%s(SS_ID ssId, struct UserVar *pVar)\n{\n",
		ssp->value, sp->value);

	for ( ep = sp->left; ep != NULL; ep = ep->next )
	{
		if ( ep->type == E_EXIT )
		{
 			 printf("/* Exit %d: */\n", ++exitI);
			 for (xp = ep->right; xp != NULL; xp = xp->next)
			 {
		                 if (line_num != xp->line_num) 
                                 { 
                                         print_line_num(xp->line_num, xp->src_file); 
                                         line_num = xp->line_num; 
                                 } 
				 eval_expr(ACTION_STMT, xp, sp, 1);
			 }
		}
	}
	/* end of function */
	printf("}\n");
	return;
}

/* Generate a function for each state that sets up delay processing:
 * This function gets called prior to the event function to guarantee
 * that the initial delay value specified in delay() calls are used.
 * Each delay() call is assigned a unique id.  The maximum number of
 * delays is recorded in the state set structure.
 */
void gen_delay_func(Expr *sp, Expr *ssp)
{
	Expr		*tp;

	printf("\n/* Delay function for state \"%s\" in state set \"%s\" */\n",
	 sp->value, ssp->value);
	printf("static void D_%s_%s(SS_ID ssId, struct UserVar *pVar)\n{\n", ssp->value, sp->value);

	/* For each transition: */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
       		if ( tp->type == E_WHEN )
		{
		        print_line_num(tp->line_num, tp->src_file);
		        traverseExprTree(tp, E_FUNC, "delay", eval_delay, sp);
		}
	}
	printf("}\n");
}

/* Evaluate the expression within a delay() function and generate
 * a call to seq_delayInit().  Adds ssId, delay id parameters and cast to
 * double.
 * Example:  seq_delayInit(ssId, 1, (double)(<some expression>));
 */
void eval_delay(Expr *ep, Expr *sp)
{
	int		delay_id;

#ifdef	DEBUG
	fprintf(stderr, "eval_delay: type=%s\n", stype[ep->type]);
#endif	/*DEBUG*/

	/* Generate 1-st part of function w/ 1-st 2 parameters */
	delay_id = (int)ep->right; /* delay id was previously assigned */
	printf("\tseq_delayInit(ssId, %d, (", delay_id);

	/* Evaluate & generate the 3-rd parameter (an expression) */
	eval_expr(EVENT_STMT, ep->left, sp, 0);

	/* Complete the function call */
	printf("));\n");
}



/* Generate action processing functions:
   Each state has one action routine.  It's name is derived from the
   state set name and the state name.
*/
void gen_action_func(Expr *sp, Expr *ssp)
{
	Expr		*tp;
	Expr		*ap;
	int		trans_num;
	extern int	line_num;

	/* Action function declaration */
	printf("\n/* Action function for state \"%s\" in state set \"%s\" */\n",
	 sp->value, ssp->value);
	printf("static void A_%s_%s(SS_ID ssId, struct UserVar *pVar, short transNum)\n{\n",
	 ssp->value, sp->value);

	/* "switch" statment based on the transition number */
	printf("\tswitch(transNum)\n\t{\n");
	trans_num = 0;
	line_num = 0;
	/* For each transition ("when" statement) ... */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
		/* local declarations are handled as text */
		if (tp->type == E_TEXT)
		{
			printf("\t\t%s\n", tp->left);
		}

	        else if (tp->type == E_WHEN) 
		{
		         /* "case" for each transition */ 
		         printf("\tcase %d:\n", trans_num); 

			 /* block within case permits local variables */
		         printf("\t\t{\n"); 

		         /* For each action statement insert action code */
		         for (ap = tp->right; ap != NULL; ap = ap->next) 
                         { 
		                 if (line_num != ap->line_num) 
                                 { 
                                         print_line_num(ap->line_num, ap->src_file); 
                                         line_num = ap->line_num; 
                                 } 
                                 /* Evaluate statements */ 
                                 eval_expr(ACTION_STMT, ap, sp, 3); 
			 } 

			 /* end of block */
		         printf("\t\t}\n"); 

                         /* end of case */ 
                         printf("\t\treturn;\n"); 
                         trans_num++;
		}
	}
	/* end of switch stmt */
	printf("\t}\n");
	/* end of function */
	printf("}\n");
	return;
}

/* Generate a C function that checks events for a particular state */
void gen_event_func(Expr *sp, Expr *ssp)
{
	Expr		*tp;
	int		index, trans_num;

	printf("\n/* Event function for state \"%s\" in state set \"%s\" */\n",
	 sp->value, ssp->value);
	printf("static long E_%s_%s(SS_ID ssId, struct UserVar *pVar, short *pTransNum, short *pNextState)\n{\n",
	 ssp->value, sp->value);
	trans_num = 0;
	/* For each transition generate an "if" statement ... */
	for (tp = sp->left; tp != NULL; tp = tp->next)
	{
	        if (tp->type == E_WHEN) 
		{
		        print_line_num(tp->line_num, tp->src_file);
			printf("\tif (");
			if (tp->left == 0)
			      printf("TRUE");
			else
			      eval_expr(EVENT_STMT, tp->left, sp, 0);
			printf(")\n\t{\n");
			/* index is the transition number (0, 1, ...) */
			index = state_block_index_from_name(ssp, tp->value);
			if (index < 0)
			{
			       fprintf(stderr, "Line %d: ", tp->line_num);
			       fprintf(stderr, "No state %s in state set %s\n",
				       tp->value, ssp->value);
			       index = 0; /* default to 1-st state */
			       printf("\t\t/* state %s does not exist */\n",
				      tp->value);
			}
			printf("\t\t*pNextState = %d;\n", index);
			printf("\t\t*pTransNum = %d;\n", trans_num);
			printf("\t\treturn TRUE;\n\t}\n");
			trans_num++;
		}
	}
	printf("\treturn FALSE;\n");
	printf("}\n");
}

/* Given a state name and state set struct, find the corresponding
   state struct and return its index (1-st one is 0) */
int state_block_index_from_name(Expr *ssp,char *state_name)
{
	Expr		*sp;
	int		index;

	index = 0;
	for (sp = ssp->left; sp != NULL; sp = sp->next)
	{
		if (strcmp(state_name, sp->value) == 0)
			return index;
		index++;
	}
	return -1; /* State name non-existant */
}
/* Evaluate an expression.
*/
void eval_expr(int stmt_type, Expr *ep, Expr *sp, int level)
/*
    stmt_type - EVENT_STMT, ACTION_STMT, or DELAY_STMT
    ep 	  - ptr to expression
    sp 	  - ptr to current State struct
    level 	  - indentation level
*/
{
	Expr		*epf;
	int		nexprs;
	extern int	reent_opt;
	extern int	line_num;

	if (ep == 0)
		return;

	switch(ep->type)
	{
	case E_DECL:
		indent(level);
		printf("%s ",ep->value);
		eval_expr(stmt_type, ep->left, sp, 0);
		if (ep->right != 0)
		{
			printf(" = ");
			eval_expr(stmt_type, ep->right, sp, 0);
		}
		printf(";\n");
		line_num += 1;
		break;
	case E_CMPND:
		indent(level);
		printf("{\n");
		line_num += 1;
		for (epf = ep->left; epf != 0;	epf = epf->next)
		{
			eval_expr(stmt_type, epf, sp, level+1);
		}
		indent(level);
		printf("}\n");
		line_num += 1;
		break;
	case E_STMT:
		indent(level);
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(";\n");
		line_num += 1;
		break;
	case E_IF:
	case E_WHILE:
		indent(level);
		if (ep->type == E_IF)
			printf("if (");
		else
			printf("while (");
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(")\n");
		line_num += 1;
		epf = ep->right;
		if (epf->type == E_CMPND)
			eval_expr(stmt_type, ep->right, sp, level);
		else
			eval_expr(stmt_type, ep->right, sp, level+1);
		break;
	case E_FOR:
		indent(level);
		printf("for (");
		eval_expr(stmt_type, ep->left->left, sp, 0);
		printf("; ");
		eval_expr(stmt_type, ep->left->right, sp, 0);
		printf("; ");
		eval_expr(stmt_type, ep->right->left, sp, 0);
		printf(")\n");
		line_num += 1;
		epf = ep->right->right;
		if (epf->type == E_CMPND)
			eval_expr(stmt_type, epf, sp, level);
		else
			eval_expr(stmt_type, epf, sp, level+1);
		break;
	case E_ELSE:
		indent(level);
		printf("else\n");
		line_num += 1;
		epf = ep->left;
		/* Is it "else if" ? */
		if (epf->type == E_IF || epf->type == E_CMPND)
			eval_expr(stmt_type, ep->left, sp, level);
		else
			eval_expr(stmt_type, ep->left, sp, level+1);
		break;
	case E_VAR:
#ifdef	DEBUG
		fprintf(stderr, "E_VAR: %s\n", ep->value);
		fprintf(stderr, "ep->left is %s\n",ep->left ? "non-null" : "null");
#endif	/*DEBUG*/
		if(reent_opt)
		{	/* Make variables point to allocated structure */
			Var		*vp;
			vp = (Var *)ep->left;
			if (vp->type != V_NONE && vp->type != V_EVFLAG)
				printf("(pVar->%s)", ep->value);
			else
				printf("%s", ep->value);
		}
		else
			printf("%s", ep->value);
		break;
	case E_CONST:
		printf("%s", ep->value);
		break;
	case E_STRING:
		printf("\"%s\"", ep->value);
		break;
	case E_BREAK:
		indent(level);
		printf("break;\n");
		line_num += 1;
		break;
	case E_FUNC:
#ifdef	DEBUG
		fprintf(stderr, "E_FUNC: %s\n", ep->value);
#endif	/*DEBUG*/
		if (special_func(stmt_type, ep, sp))
			break;
		printf("%s(", ep->value);
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(")");
		break;
	case E_COMMA:
		for (epf = ep->left, nexprs = 0; epf != 0; epf = epf->next, nexprs++)
		{
			if (nexprs > 0)
				printf(", ");
			eval_expr(stmt_type, epf, sp, 0);
		}
		break;
	case E_ASGNOP:
	case E_BINOP:
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(" %s ", ep->value);
		eval_expr(stmt_type, ep->right, sp, 0);
		break;
	case E_PAREN:
		printf("(");
		eval_expr(stmt_type, ep->left, sp, 0);
		printf(")");
		break;
	case E_UNOP:
		printf("%s", ep->value);
		eval_expr(stmt_type, ep->left, sp, 0);
		break;
	case E_PRE:
		printf("%s", ep->value);
		eval_expr(stmt_type, ep->left, sp, 0);
		break;
	case E_POST:
		eval_expr(stmt_type, ep->left, sp, 0);
		printf("%s", ep->value);
		break;
	case E_SUBSCR:
		eval_expr(stmt_type, ep->left, sp, 0);
		printf("[");
		eval_expr(stmt_type, ep->right, sp, 0);
		printf("]");
		break;
	case E_TEXT:
		printf("%s\n", ep->left);
		line_num += 1;
		break;
	default:
		if (stmt_type == EVENT_STMT)
			printf("TRUE"); /* empty event statement defaults to TRUE */
		else
			printf(" ");
		break;
	}
}

void indent(int level)
{
	while (level-- > 0)
		printf("\t");
}


enum fcode func_name_to_code(fname)
char	*fname;
{
	int		i;

	for (i = 0; fcode_str[i] != NULL; i++)
	{
		if (strcmp(fname, fcode_str[i]) == 0)
			return (enum fcode)i;
	}
			
	return F_NONE;
}

/* Process special function (returns TRUE if this is a special function)
	Checks for one of the following special functions:
	 - event flag functions, e.g. pvSet()
	 - process variable functions, e.g. pvPut()
	 - delay()
	 - macValueGet()
	 - seqLog()
*/
int special_func(int stmt_type,Expr *ep,Expr *sp)
/*
    int		stmt_type;	 ACTION_STMT or EVENT_STMT
    Expr	*ep;		 ptr to function in the expression
    Expr	*sp;		 current State struct
*/
{
	char		*fname; /* function name */
	Expr		*ep1; /* parameters */
	enum		fcode func_code;
	int		delay_id;

	fname = ep->value;
	func_code = func_name_to_code(fname);
	if (func_code == F_NONE)
		return FALSE; /* not a special function */

#ifdef	DEBUG
	fprintf(stderr, "special_func: func_code=%d\n", func_code);
#endif	/*DEBUG*/
	switch (func_code)
	{
	    case F_DELAY:
		delay_id = (int)ep->right;
		printf("seq_delay(ssId, %d)", delay_id);
		return TRUE;

	    case F_EFSET:
	    case F_EFTEST:
	    case F_EFCLEAR:
	    case F_EFTESTANDCLEAR:
		/* Event flag funtions */
		gen_ef_func(stmt_type, ep, sp, fname, func_code);
		return TRUE;

	    case F_PVGETQ:
	    case F_PVFREEQ:
	    case F_PVTIMESTAMP:
	    case F_PVGETCOMPLETE:
	    case F_PVNAME:
	    case F_PVSTATUS:
	    case F_PVSEVERITY:
	    case F_PVMESSAGE:
	    case F_PVCONNECTED:
	    case F_PVASSIGNED:
	    case F_PVMONITOR:
	    case F_PVSTOPMONITOR:
	    case F_PVCOUNT:
	    case F_PVINDEX:
	    case F_PVDISCONNECT:
	    case F_PVASSIGN:
		/* DB functions requiring a channel id */
		gen_pv_func(stmt_type, ep, sp, fname, func_code, FALSE, 0);
		return TRUE;

	    case F_PVPUT:
	    case F_PVGET:
		/* DB functions requiring a channel id and defaulted
		   last 1 parameter */
		gen_pv_func(stmt_type, ep, sp, fname, func_code, FALSE, 1);
		return TRUE;

	    case F_PVPUTCOMPLETE:
		/* DB functions requiring a channel id, an array length and
		   defaulted last 2 parameters */
		gen_pv_func(stmt_type, ep, sp, fname, func_code, TRUE, 2);
		return TRUE;

	    case F_PVFLUSH:
	    case F_PVERROR:
	    case F_PVCHANNELCOUNT:
	    case F_PVCONNECTCOUNT:
	    case F_PVASSIGNCOUNT:
		/* DB functions NOT requiring a channel structure */
		printf("seq_%s(ssId)", fname);
		return TRUE;

	    case F_SEQLOG:
	    case F_MACVALUEGET:
	    case F_OPTGET:
		/* Any funtion that requires adding ssID as 1st parameter.
		 * Note:  name is changed by prepending "seq_". */
		printf("seq_%s(ssId", fname);
		/* now fill in user-supplied paramters */
		ep1 = ep->left;
		if (ep1 != 0 && ep1->type == E_COMMA)
			ep1 = ep1->left;
		for (; ep1 != 0; ep1 = ep1->next)
		{
			printf(", ");
			eval_expr(stmt_type, ep1, sp, 0);
		}
		printf(")");
		return TRUE;

	    default:
		/* Not a special function */
		return FALSE;
	}
}

/* Generate code for all event flag functions */
void gen_ef_func(int stmt_type,Expr *ep,Expr *sp,char *fname,enum fcode func_code)
/*
    int		stmt_type;	 ACTION_STMT or EVENT_STMT 
    Expr	*ep;		 ptr to function in the expression 
    Expr	*sp;		 current State struct 
    char	*fname;		 function name 
    enum	fcode func_code; function code 
*/
{
	Expr		*ep1;
	Var		*vp;

	ep1 = ep->left; /* ptr to 1-st parameters */
	if (ep1 != 0 && ep1->type == E_COMMA)
		ep1 = ep1->left;
	if ( (ep1 != 0) && (ep1->type == E_VAR) )
		vp = (Var *)findVar(ep1->value);
	else
		vp = 0;
		if (vp == 0 || vp->type != V_EVFLAG)
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "Parameter to \"%s\" must be an event flag\n", fname);
	}
	else if (func_code == F_EFSET && stmt_type == EVENT_STMT)
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "efSet() cannot be used as an event.\n");
	}
	else
	{
		printf("seq_%s(ssId, %s)", fname, vp->name);
	}

	return;
}

/* Generate code for pv functions requiring a database variable.
 * The channel id (index into channel array) is substituted for the variable
 *
 * "add_length" => the array length (1 if not an array) follows the channel id 
 * "num_params > 0" => add default (zero) parameters up to the spec. number
 */
void gen_pv_func(int stmt_type,Expr *ep,Expr *sp,
    char *fname,enum fcode func_code,int add_length,int num_params)
/*
    int		stmt_type;	 ACTION_STMT or EVENT_STMT 
    Expr	*ep;		 ptr to function in the expression 
    Expr	*sp;		 current State struct 
    char	*fname;		 function name 
    enum	fcode func_code; function code 
    int		add_length;	 add array length after channel id 
    int		num_params;	 number of params to add (if omitted) 
*/
{
	Expr		*ep1, *ep2, *ep3;
	Var		*vp;
	char		*vn;
	int		id;
	Chan		*cp;
	int		num;

	ep1 = ep->left; /* ptr to 1-st parameter in the function */
	if (ep1 != 0 && ep1->type == E_COMMA)
		ep1 = ep1->left;
	if (ep1 == 0)
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "Function \"%s\" requires a parameter.\n", fname);
		return;
	}

	vp = 0;
	vn = "?";
	id = -1;
	if (ep1->type == E_VAR)
	{
		vp = (Var *)findVar(ep1->value);
	}
	else if (ep1->type == E_SUBSCR)
	{	/* Form should be: <db variable>[<expression>] */
		ep2 = ep1->left;	/* variable */
		ep3 = ep1->right;	/* subscript */
		if ( ep2->type == E_VAR )
		{
			vp = (Var *)findVar(ep2->value);
		}
	}

	if (vp == 0)
	{
		fprintf(stderr, "Line %d: ", ep->line_num);
		fprintf(stderr,
		 "Parameter to \"%s\" is not a defined variable.\n", fname);
		cp=0;
	}
		
	else
	{
#ifdef	DEBUG
		fprintf(stderr, "gen_pv_func: var=%s\n", ep1->value);
#endif	/*DEBUG*/
		vn = vp->name;
		cp = vp->chan;
		if (cp == 0)
		{
			fprintf(stderr, "Line %d: ", ep->line_num);
			fprintf(stderr,
			 "Parameter to \"%s\" must be DB variable.\n", fname);
		}
		else
		{
			id = cp->index;
		}
	}

	printf("seq_%s(ssId, %d /* %s */", fname, id, vn);

	if (ep1->type == E_SUBSCR) /* subscripted variable? */
	{	/* e.g. pvPut(xyz[i+2]); => seq_pvPut(ssId, 3 + (i+2)); */
		printf(" + (");
		/* evalute the subscript expression */
		eval_expr(stmt_type, ep3, sp, 0);
		printf(")");
	}

	/* If requested, add length parameter (if subscripted variable,
	   length is always 1) */
	if (add_length)
	{
		if (vp != 0 && ep1->type != E_SUBSCR)
		{
			printf(", %d", vp->length1);
		}
		else
		{
			printf(", 1");
		}
	}

	/* Add any additional parameter(s) */
	num = 0;
	ep1 = ep1->next;
	while (ep1 != 0)
	{
		Expr *ep;

		printf(", ");
		eval_expr(stmt_type, ep1, sp, 0);

		/* Count parameters (makes use of knowledge that parameter
		   list will be tree with nodes of type E_COMMA, arguments
		   in "left" and next E_COMMA in "left->next") */
		ep = ep1;
		while (ep->type == E_COMMA)
		{
			num++;
			ep = ep->left->next;
		}
		num++;

		/* Advance to next parameter (always zero? multiple parameters
		   are always handled as compound expressions?) */
		ep1 = ep1->next;
	}

	/* If not enough additional parameter(s) were specified, add
	   extra zero parameters */
	for (; num < num_params; num++)
	{
		printf(", 0");
	}

	/* Close the parameter list */
	printf(")");

	return;
}

/* Generate entry handler code */
void gen_entry_handler()
{
	extern Expr	*entry_code_list;
	Expr		*ep;

	printf("\n/* Entry handler */\n");
	printf("static void entry_handler(SS_ID ssId, struct UserVar *pVar)\n{\n");
	for (ep = entry_code_list; ep != 0; ep = ep->next)
	{
		eval_expr(ENTRY_STMT, ep, (Expr *)NULL, 1);
	}
	printf("}\n\n");
}

/* Generate exit handler code */
void gen_exit_handler()
{
	extern Expr	*exit_code_list;
	Expr		*ep;

	printf("\n/* Exit handler */\n");
	printf("static void exit_handler(SS_ID ssId, struct UserVar *pVar)\n{\n");
	for (ep = exit_code_list; ep != 0; ep = ep->next)
	{
		eval_expr(EXIT_STMT, ep, (Expr *)NULL, 1);
	}
	printf("}\n\n");
}
