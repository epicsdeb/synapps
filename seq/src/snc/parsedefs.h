#ifndef INCLparsedefsh
#define INCLparsedefsh

/* defined in snc_main.c */
void yyerror(char *err);
void snc_err(char *err_txt);

/* defined in parse.c */
void program(Expr *prog_list);
void program_name(char *pname, char *pparam);
void assign_single(
	char	*name,		/* ptr to variable name */
	char	*db_name	/* ptr to db name */
);
void assign_subscr(
	char	*name,		/* ptr to variable name */
	char	*subscript,	/* subscript value or NULL */
	char	*db_name	/* ptr to db name */
);
void assign_list(
	char	*name,		/* ptr to variable name */
	Expr	*db_name_list	/* ptr to db name list */
);
Expr *expression(
	int	type,		/* E_BINOP, E_ASGNOP, etc */
	char	*value,		/* "==", "+=", var name, constant, etc. */	
	Expr	*left,		/* LH side */
	Expr	*right		/* RH side */
);
void monitor_stmt(
	char	*name,		/* variable name (should be assigned) */
	char	*subscript	/* element number or NULL */
);
void set_debug_print(char *opt);
void decl_stmt(
	int	type,		/* variable type (e.g. V_FLOAT) */
	int	class,		/* variable class (e.g. VC_ARRAY) */
	char	*name,		/* ptr to variable name */
	char	*s_length1,	/* array lth (1st dim, arrays only) */
	char	*s_length2,	/* array lth (2nd dim, [n]x[m] arrays only) */
	char	*value		/* initial value or NULL */
);
void sync_stmt(char *name, char *subscript, char *ef_name);
void syncq_stmt(char *name, char *subscript, char *ef_name, char *maxQueueSize);
void defn_c_stmt(
	Expr *c_list	/* ptr to C code */
);
void option_stmt(
	char		*option,	/* "a", "r", ... */
	int		value		/* TRUE means +, FALSE means - */
);
int entry_code(Expr *ep);
int exit_code(Expr *ep);
void pp_code(char *line, char *fname);
void global_c_stmt(
	Expr		*c_list		/* ptr to C code */
);

#endif	/*INCLparsedefsh*/
