/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1989-93, The Regents of the University of California.
		         Los Alamos National Laboratory

 	$Id: proto.h,v 1.2 2001-03-21 15:06:10 mrk Exp $
	DESCRIPTION: Function prototypes for state notation language parser
	ENVIRONMENT: UNIX
	HISTORY:
29apr99,wfl	Created.

***************************************************************************/

#ifndef INCLsnch
#define INCLsnch

/* Don't require parse.h to have been included */
#ifndef INCLparseh
#define Expr void
#define Var  void
#endif

/* Prototypes for external functions */
void Global_yyparse (void);

void phase2(void);

int  exprCount(Expr*);

void gen_ss_code(void);

void gen_tables(void);

void addVar(Var*);

void print_line_num(int,char*);

void traverseExprTree(Expr*,int,char*,void (*)(),void*);

void snc_err(char *err_txt);
void yyerror(char *err);

void program_name(char*pname, char *pparam);
void decl_stmt(int type, int class, char *name, char *s_length1, char *s_length2, char *value);
void option_stmt(char *option, int value);
void assign_single(char *name, char *db_name);
void assign_subscr(char *name, char *subscript, char *db_name);
void assign_list(char *name, Expr *db_name_list);
void monitor_stmt(char *name, char *subscript);
void sync_stmt(char *name, char *subscript, char *ef_name);
void syncq_stmt(char *name, char *subscript, char *ef_name, char *maxQueueSize);
void defn_c_stmt(Expr *c_list);
void global_c_stmt(Expr *c_list);
void set_debug_print(char *opt);
void program(Expr *prog_list);
int entry_code(Expr *ep);
int exit_code(Expr *ep);
void pp_code(char *line, char *fname);

#endif	/*INCLsnch*/
