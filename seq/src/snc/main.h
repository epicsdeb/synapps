/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
            Various reporting and printing procedures
***************************************************************************/
#ifndef INCLsncmainh
#define INCLsncmainh

/* append '# <line_num> "<src_file>"\n' to output (if not disabled by cmd-line option) */
void gen_line_marker_prim(int line_num, const char *src_file);

/* this gets the shorter name as it is the common way to call above */
#define gen_line_marker(ep) gen_line_marker_prim((ep)->line_num, (ep)->src_file)

/* Error and warning message support */

/* just the location info */
void report_loc(const char *src_file, int line_num);

/* location plus message */
void report_at(const char *src_file, int line_num, const char *format, ...);

/* location plus message and increase error count */
void error_at(const char *src_file, int line_num, const char *format, ...);

/* with location from this expression */
struct expression;
void report_at_expr(struct expression *ep, const char *format, ...);

/* with location from this expression but only if warnings are enabled */
void warning_at_expr(struct expression *ep, const char *format, ...);

/* with location from this expression and increase error count */
void error_at_expr(struct expression *ep, const char *format, ...);

/* message only */
void report(const char *format, ...);

#endif	/*INCLsncmainh*/
