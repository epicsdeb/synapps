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
                Main program, reporting and printing procedures
\*************************************************************************/
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdarg.h>

#include	"types.h"
#include	"parser.h"
#include	"analysis.h"
#include	"gen_code.h"
#include	"main.h"

#include <seq_release.h>

static Options options = DEFAULT_OPTIONS;

static char *in_file;	/* input file name */
static char *out_file;	/* output file name */

static int err_cnt;

static void parse_args(int argc, char *argv[]);
static void parse_option(char *s);
static void print_usage(void);

/* The streams stdin and stdout are redirected to files named in the
   command parameters. */
int main(int argc, char *argv[])
{
	FILE	*infp, *outfp;
	Program	*prg;
        Expr    *exp;

	/* Get command arguments */
	parse_args(argc, argv);

	/* Redirect input stream from specified file */
	infp = freopen(in_file, "r", stdin);
	if (infp == NULL)
	{
		perror(in_file);
		exit(EXIT_FAILURE);
	}

	/* Redirect output stream to specified file */
	outfp = freopen(out_file, "w", stdout);
	if (outfp == NULL)
	{
		perror(out_file);
		exit(EXIT_FAILURE);
	}

	/* stdin, i.e. the input file should be unbuffered,
           since the re2c generated lexer works fastest when
           it maintains its own buffer */
	setvbuf(stdin, NULL, _IONBF, 0);
	/* stdout, i.e. the generated C program should be
           block buffered with standard buffer size */
	setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
	/* stderr, i.e. messages should be output immediately */
	setvbuf(stderr, NULL, _IONBF, 0);

	printf("/* Generated with snc from %s */\n", in_file);

	exp = parse_program(in_file);

        prg = analyse_program(exp, options);

	if (err_cnt == 0)
		generate_code(prg);

	exit(err_cnt ? EXIT_FAILURE : EXIT_SUCCESS);
}

/* Initialize options, in_file, and out_file from arguments. */
static void parse_args(int argc, char *argv[])
{
	int i;

	if (argc < 2)
	{
		print_usage();
		exit(EXIT_FAILURE);
	}

	for (i=1; i<argc; i++)
	{
		char *s = argv[i];

		if (strcmp(s,"-o") == 0)
		{
			if (i+1 == argc)
			{
				report("missing filename after option -o\n");
				print_usage();
				exit(EXIT_FAILURE);
			}
			else
			{
				i++;
				out_file = argv[i];
				continue;
			}
		}
		else if (s[0] != '+' && s[0] != '-')
		{
			in_file = s;
			continue;
		}
		else
		{
			parse_option(s);
		}
	}
	if (options.safe && !options.reent) {
		options.reent = TRUE;
	}

	if (!in_file)
	{
		report("no input file argument given\n");
		print_usage();
		exit(EXIT_FAILURE);
	}

	if (!out_file)	/* no -o option given */
	{
		unsigned l = strlen(in_file);
		char *ext = strrchr(in_file, '.');

		if (ext && strcmp(ext,".st") == 0)
		{
			out_file = (char*)malloc(l);
			strcpy(out_file, in_file);
			strcpy(out_file+(ext-in_file), ".c\n");
		}
		else
		{
			out_file = (char*)malloc(l+3);
			sprintf(out_file, "%s.c", in_file);
		}
	}
}

static void parse_option(char *s)
{
	int		opt_val;

	opt_val = (*s == '+');

	switch (s[1])
	{
	case 'a':
		options.async = opt_val;
		break;
	case 'c':
		options.conn = opt_val;
		break;
	case 'd':
		options.debug = opt_val;
		break;
	case 'e':
		options.newef = opt_val;
		break;
	case 'r':
		options.reent = opt_val;
		break;
	case 'i':
		options.init_reg = opt_val;
		break;
	case 'l':
		options.line = opt_val;
		break;
	case 'm':
		options.main = opt_val;
		break;
	case 's':
		options.safe = opt_val;
		break;
	case 'w':
		options.warn = opt_val;
		break;
	default:
		report("unknown option ignored: '%s'\n", s);
		break;
	}
}

static void print_usage(void)
{
	report("%s\n", SEQ_RELEASE);
	report("usage: snc <options> <infile>\n");
	report("options:\n");
	report("  -o <outfile> - override name of output file\n");
	report("  +a           - do asynchronous pvGet\n");
	report("  -c           - don't wait for all connects\n");
	report("  +d           - turn on debug run-time option\n");
	report("  -e           - don't use new event flag mode\n");
	report("  -l           - suppress line numbering\n");
	report("  +m           - generate main program\n");
	report("  -i           - don't register commands/programs\n");
	report("  +r           - make reentrant at run-time\n");
	report("  +s           - safe mode (implies +r, overrides -r)\n");
	report("  -w           - suppress compiler warnings\n");
	report("example:\n snc +a -c vacuum.st\n");
}

void gen_line_marker_prim(int line_num, const char *src_file)
{
	if (options.line)
		printf("# line %d \"%s\"\n", line_num, src_file);
}

/* Errors and warnings */

void report_loc(const char *src_file, int line_num)
{
	fprintf(stderr, "%s:%d: ", src_file, line_num);
}

void report_at(const char *src_file, int line_num, const char *format, ...)
{
	va_list args;

	report_loc(src_file, line_num);

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void error_at(const char *src_file, int line_num, const char *format, ...)
{
	va_list args;

	err_cnt++;
	report_loc(src_file, line_num);

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void report_at_expr(Expr *ep, const char *format, ...)
{
	va_list args;

	report_loc(ep->src_file, ep->line_num);

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void warning_at_expr(Expr *ep, const char *format, ...)
{
	va_list args;

	if (!options.warn) return;
	report_loc(ep->src_file, ep->line_num);
	fprintf(stderr, "warning: ");

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void error_at_expr(Expr *ep, const char *format, ...)
{
	va_list args;

	err_cnt++;
	report_loc(ep->src_file, ep->line_num);
	fprintf(stderr, "error: ");

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void report(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
