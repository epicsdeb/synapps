/*************************************************************************\
Copyright (c) 2010-2012 Helmholtz-Zentrum Berlin f. Materialien
                        und Energie GmbH, Germany (HZB)
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/*************************************************************************\
                    Lexer specification/implementation
\*************************************************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "snl.h"
#include "main.h"
#include "parser.h"

#define	EOI		0

typedef unsigned char uchar;

#define	BSIZE	8192

#define	YYCTYPE			uchar
#define	YYCURSOR		cursor
#define	YYLIMIT			s->lim
#define	YYMARKER		s->ptr
#define	YYFILL(dummy)		cursor = fill(s, cursor);
#define	YYDEBUG(state, current) report("state = %d, current = %c\n", state, current);

#define	RET(i,r) {\
	s->cur = cursor;\
	t->str = r;\
	return i;\
}

#define OPERATOR		RET
#define LITERAL(t,e,n) 		RET(t,n)
#define KEYWORD 		RET
#define TYPEWORD		RET
#define IDENTIFIER(t,e,n)	RET(t,n)
#define DELIMITER		RET

#define DONE			RET(EOI,"")

typedef struct Scanner {
	uchar	*bot;	/* pointer to bottom (start) of buffer */
	uchar	*tok;	/* pointer to start of current token */
	uchar	*end;	/* pointer to (temporary) end of current token */
	uchar	*ptr;	/* marker for backtracking (always > tok) */
	uchar	*cur;	/* saved scan position between calls to scan() */
	uchar	*lim;	/* pointer to one position after last read char */
	uchar	*top;	/* pointer to (one after) top of allocated buffer */
	uchar	*eof;	/* pointer to (one after) last char in file (or 0) */
	const char *file;	/* source file name */
	int	line;	/* line number */
} Scanner;

static void scan_report(Scanner *s, const char *format, ...)
{
	va_list args;

	report_loc(s->file, s->line);
	va_start(args, format);
	fprintf(stderr, "lexical error: ");
	vfprintf(stderr, format, args);
	va_end(args);
}

/*
From the re2c docs:

   The generated code "calls" YYFILL(n) when the buffer needs (re)filling: at
   least n additional characters should be provided. YYFILL(n) should adjust
   YYCURSOR, YYLIMIT, YYMARKER and YYCTXMARKER as needed. Note that for typical
   programming languages n will be the length of the longest keyword plus one.

We also add a '\n' byte at the end of the file as sentinel.
*/
static uchar *fill(Scanner *s, uchar *cursor) {
	/* does not touch s->cur, instead works with argument cursor */
	if (!s->eof) {
		uint read_cnt;			/* number of bytes read */
		uint garbage = s->tok - s->bot;	/* number of garbage bytes */
		uint valid = s->lim - s->tok;	/* number of still valid bytes to copy */
		uchar *token = s->tok;		/* start of valid bytes */
		uint space = (s->top - s->lim) + garbage;
						/* remaining space after garbage collection */
		int need_alloc = space < BSIZE;	/* do we need to allocate a new buffer? */

		/* anything below s->tok is garbage, collect it */
		if (garbage) {
#ifdef DEBUG
			report("fill: garbage=%u, need_alloc=%d\n", garbage, need_alloc);
#endif
			if (!need_alloc) {
				/* shift valid buffer content down to bottom of buffer */
				memmove(s->bot, token, valid);
			}
			/* adjust pointers */
			s->tok = s->bot;	/* same as s->tok -= garbage */
			s->ptr -= garbage;
			cursor -= garbage;
			s->lim -= garbage;
			s->end -= garbage;
			/* invariant: s->bot, s->top, s->eof, s->lim - s->tok */
		}
		/* increase the buffer size if necessary, ensuring that we have
		   at least BSIZE bytes of free space to fill (after s->lim) */
		if (need_alloc) {
			uchar *buf = (uchar*) malloc((s->lim - s->bot + BSIZE)*sizeof(uchar));
#ifdef DEBUG
			report("fill: need_alloc, bot: before=%p after=%p\n", s->bot, buf);
#endif
			memcpy(buf, token, valid);
			s->tok = buf;
			s->end = &buf[s->end - s->bot];
			s->ptr = &buf[s->ptr - s->bot];
			cursor = &buf[cursor - s->bot];
			s->lim = &buf[s->lim - s->bot];
			s->top = s->lim + BSIZE;
			free(s->bot);
			s->bot = buf;
		}
		/* fill the buffer, starting at s->lim, by reading a chunk of
		   BSIZE bytes (or less if eof is encountered) */
		if ((read_cnt = fread(s->lim, sizeof(uchar), BSIZE, stdin)) != BSIZE) {
			if (ferror(stdin)) {
				perror("error reading input");
				exit(EXIT_FAILURE);
			}
			if (feof(stdin)) {
				s->eof = &s->lim[read_cnt];
				/* insert sentinel and increase s->eof */
				*(s->eof)++ = '\n';
			}
		}
		s->lim += read_cnt;	/* adjust limit */
	}
	return cursor;
}

/* alias strdup_from_to: duplicate string from start to (exclusive) stop */
static char *strdupft(uchar *start, uchar *stop) {
	char *result;
	size_t n;
	assert (stop - start >= 0);
	n = (size_t)(stop - start);
	result = malloc(n+1);
	memcpy(result, start, n);
	result[n] = 0;
	return result;
}

/*
 * Note: Linemarkers differ between compilers. The MS C preprocessor outputs
 * "#line <linenum> <filename>" directives, while gcc leaves off the "line".
 */

/*!re2c
	NL	= [\n];
	ANY	= [^];
	SPC	= [ \t];
	OCT	= [0-7];
	DEC	= [0-9];
	LET	= [a-zA-Z_];
	HEX	= [a-fA-F0-9];
	EXP	= [Ee] [+-]? DEC+;
	FS	= [fFlL];
	IS	= [uUlL]*;
	ESC	= [\\] ([abfnrtv?'"\\] | "x" HEX+ | OCT+);
	LINE	= SPC* "#" (SPC* "line")? SPC+;
*/

static int scan(Scanner *s, Token *t) {
	uchar *cursor = s->cur;
	int in_c_code = 0;
	/*
	Note: Must use a temporary offset for (parts of) line_marker.
	Normally we use s->tok to remember start positions. But line_markers
	can appear nested inside c_code block tokens, so using s->tok for
	line_markers would destroy them.
	*/
	int line_marker_part = 0;

	s->end = 0;

snl:
	if (in_c_code)
		goto c_code;
	t->line = s->line;
	t->file = s->file;
	s->tok = cursor;

/*!re2c
	NL		{
				if(cursor == s->eof) DONE;
				s->line++;
				goto snl;
			}
	LINE		{
				line_marker_part = cursor - s->tok;
				goto line_marker;
			}
	["]		{
				s->tok = s->end = cursor;
				goto string_const;
			}
	"/*"		{ goto comment; }
	"%{"		{
				s->tok = cursor;
				in_c_code = 1;
#ifdef DEBUG
				report("in_c_code: cursor=%p, tok=%p\n", cursor, s->tok);
#endif
				goto c_code;
			}
	"%%" SPC*	{
				s->tok = s->end = cursor;
				goto c_code_line;
			}
	"assign"	{ KEYWORD(ASSIGN,	"assign"); }
	"break"		{ KEYWORD(BREAK,	"break"); }
	"char"		{ TYPEWORD(CHAR,	"char"); }
	"connect"	{ KEYWORD(CONNECT,	"connect"); }
	"continue"	{ KEYWORD(CONTINUE,	"continue"); }
	"double"	{ TYPEWORD(DOUBLE,	"double"); }
	"else"		{ KEYWORD(ELSE,		"else"); }
	"entry"		{ KEYWORD(ENTRY,	"entry"); }
	"evflag"	{ TYPEWORD(EVFLAG,	"evflag"); }
	"exit"		{ KEYWORD(EXIT,		"exit"); }
	"float"		{ TYPEWORD(FLOAT,	"float"); }
	"for"		{ KEYWORD(FOR,		"for"); }
	"foreign"	{ TYPEWORD(FOREIGN,	"foreign"); }
	"if"		{ KEYWORD(IF,		"if"); }
	"int"		{ TYPEWORD(INT,		"int"); }
	"long"		{ TYPEWORD(LONG,	"long"); }
	"monitor"	{ KEYWORD(MONITOR,	"monitor"); }
	"option"	{ KEYWORD(OPTION,	"option"); }
	"program"	{ KEYWORD(PROGRAM,	"program"); }
	"short"		{ TYPEWORD(SHORT,	"short"); }
	"ss"		{ KEYWORD(SS,		"ss"); }
	"state"		{ KEYWORD(STATE,	"state"); }
	"string"	{ KEYWORD(STRING,	"string"); }
	"syncQ"		{ KEYWORD(SYNCQ,	"syncQ"); }
	"syncq"		{ KEYWORD(SYNCQ,	"syncq"); }
	"sync"		{ KEYWORD(SYNC,		"sync"); }
	"to"		{ KEYWORD(TO,		"to"); }
	"unsigned"	{ TYPEWORD(UNSIGNED,	"unsigned"); }
	"when"		{ KEYWORD(WHEN,		"when"); }
	"while"		{ KEYWORD(WHILE,	"while"); }

	"int8_t"	{ TYPEWORD(INT8T,  	"int8_t"); }
	"uint8_t"	{ TYPEWORD(UINT8T, 	"uint8_t"); }
	"int16_t"	{ TYPEWORD(INT16T, 	"int16_t"); }
	"uint16_t"	{ TYPEWORD(UINT16T,	"uint16_t"); }
	"int32_t"	{ TYPEWORD(INT32T, 	"int32_t"); }
	"uint32_t"	{ TYPEWORD(UINT32T,	"uint32_t"); }

	LET (LET|DEC)*	{ IDENTIFIER(NAME, identifier, strdupft(s->tok, cursor)); }
	("0" [xX] HEX+ IS?) | ("0" OCT+ IS?) | (DEC+ IS?) | (['] (ESC|[^\n\\'])* ['])
			{ LITERAL(INTCON, integer_literal, strdupft(s->tok, cursor)); }

	(DEC+ EXP FS?) | (DEC* "." DEC+ EXP? FS?) | (DEC+ "." DEC* EXP? FS?)
			{ LITERAL(FPCON, floating_point_literal, strdupft(s->tok, cursor)); }

	">>="		{ OPERATOR(RSHEQ,	">>="); }
	"<<="		{ OPERATOR(LSHEQ,	"<<="); }
	"+="		{ OPERATOR(ADDEQ,	"+="); }
	"-="		{ OPERATOR(SUBEQ,	"-="); }
	"*="		{ OPERATOR(MULEQ,	"*="); }
	"/="		{ OPERATOR(DIVEQ,	"/="); }
	"%="		{ OPERATOR(MODEQ,	"%="); }
	"&="		{ OPERATOR(ANDEQ,	"&="); }
	"^="		{ OPERATOR(XOREQ,	"^="); }
	"|="		{ OPERATOR(OREQ,	"|="); }
	">>"		{ OPERATOR(RSHIFT,	">>"); }
	"<<"		{ OPERATOR(LSHIFT,	"<<"); }
	"++"		{ OPERATOR(INCR,	"++"); }
	"--"		{ OPERATOR(DECR,	"--"); }
	"->"		{ OPERATOR(POINTER,	"->"); }
	"&&"		{ OPERATOR(ANDAND,	"&&"); }
	"||"		{ OPERATOR(OROR,	"||"); }
	"<="		{ OPERATOR(LE,		"<="); }
	">="		{ OPERATOR(GE,		">="); }
	"=="		{ OPERATOR(EQ,		"=="); }
	"!="		{ OPERATOR(NE,		"!="); }
	";"		{ DELIMITER(SEMICOLON,	";"); }
	"{"		{ DELIMITER(LBRACE,	"{"); }
	"}"		{ DELIMITER(RBRACE,	"}"); }
	","		{ DELIMITER(COMMA,	","); }
	":"		{ OPERATOR(COLON,	":"); }
	"="		{ OPERATOR(EQUAL,	"="); }
	"("		{ DELIMITER(LPAREN,	"("); }
	")"		{ DELIMITER(RPAREN,	")"); }
	"["		{ DELIMITER(LBRACKET,	"["); }
	"]"		{ DELIMITER(RBRACKET,	"]"); }
	"."		{ OPERATOR(PERIOD,	"."); }
	"&"		{ OPERATOR(AMPERSAND,	"&"); }
	"!"		{ OPERATOR(NOT,		"!"); }
	"~"		{ OPERATOR(TILDE,	"~"); }
	"-"		{ OPERATOR(SUB,		"-"); }
	"+"		{ OPERATOR(ADD,		"+"); }
	"*"		{ OPERATOR(ASTERISK,	"*"); }
	"/"		{ OPERATOR(SLASH,	"/"); }
	"%"		{ OPERATOR(MOD,		"%"); }
	"<"		{ OPERATOR(LT,		"<"); }
	">"		{ OPERATOR(GT,		">"); }
	"^"		{ OPERATOR(CARET,	"^"); }
	"|"		{ OPERATOR(VBAR,	"|"); }
	"?"		{ OPERATOR(QUESTION,	"?"); }
	[ \t\v\f]+	{ goto snl; }
	ANY		{ scan_report(s, "invalid character\n"); DONE; }
*/

string_const:
/*!re2c
	(ESC | [^"\n\\])*
			{ goto string_const; }
	["]		{
				s->end = cursor - 1;
				goto string_cat;
			}
	ANY		{ scan_report(s, "invalid character in string constant\n"); DONE; }
*/

string_cat:
/*!re2c
	SPC+		{ goto string_cat; }
	NL		{
				if (cursor == s->eof) {
					cursor -= 1;
					LITERAL(STRCON, string_literal, strdupft(s->tok, s->end));
				}
				s->line++;
				goto string_cat;
			}
	["]		{
				uint len = s->end - s->tok;
				memmove(cursor - len, s->tok, len);
				s->tok = cursor - len;
				goto string_const;
			}
	ANY		{
				cursor -= 1;
				LITERAL(STRCON, string_literal, strdupft(s->tok, s->end));
			}
*/

line_marker:
/*!re2c
	DEC+SPC*	{
				s->line = atoi((char*)(s->tok + line_marker_part)) - 1;
				line_marker_part = cursor - s->tok;
				goto line_marker_str;
			}
	ANY		{ goto line_marker_skip; }
*/

line_marker_str:
/*!re2c
	(["] (ESC|[^\n\\"])* ["])
			{
				s->file = strdupft(s->tok + line_marker_part + 1, cursor-1);
				goto line_marker_skip;
			}
	NL		{
				cursor -= 1;
				goto line_marker_skip;
			}
	.		{ goto line_marker_skip; }
*/

line_marker_skip:
/*!re2c
	.*		{ goto snl; }
	NL		{ cursor -= 1; goto snl; }
*/

comment:
/*!re2c
	"*/"		{ goto snl; }
	.		{ goto comment; }
	NL		{
				if (cursor == s->eof) {
					scan_report(s, "at eof: unterminated comment\n");
					DONE;
				}
				s->tok = cursor;
				s->line++;
				goto comment;
			}
*/

c_code:
/*!re2c
	"}%"		{
#ifdef DEBUG
				report("c_code: tok=%p", s->tok);
#endif
				LITERAL(CCODE, embedded_c_code, strdupft(s->tok, cursor - 2));
			}
	.		{ goto c_code; }
	LINE		{
				line_marker_part = cursor - s->tok;
				goto line_marker;
			}
	NL		{
				if (cursor == s->eof) {
					scan_report(s, "at eof: unterminated literal c-code section\n");
					DONE;
				}
				s->line++;
				goto c_code;
			}
*/

c_code_line:
/*!re2c
	.		{
				s->end = cursor;
				goto c_code_line;
			}
	SPC* NL	{
				if (cursor == s->eof) {
					cursor -= 1;
				}
				s->line++;
				if (s->end > s->tok) {
					LITERAL(CCODE, embedded_c_code, strdupft(s->tok, s->end));
				}
				goto snl;
			}
*/
	DONE;		/* dead code, only here to make compiler warning go away */
}

#ifdef TEST_LEXER
void report_loc(const char *f, int l) {
	fprintf(stderr, "%s:%d: ", f, l);
}

int main() {
	Scanner s;
	int		tt;	/* token type */
	Token		tv;	/* token value */

	memset(&s, 0, sizeof(s));

	s.cur = fill(&s, s.cur);
	s.line = 1;

	while( (tt = scan(&s, &tv)) != EOI) {
		printf("%s:%d: %2d\t$%s$\n", tv.file, tv.line, tt, tv.str);
	}
	return 0;
}
#else

Expr *parse_program(const char *src_file)
{
	Scanner	s;
	int	tt;		/* token type */
	Token	tv;		/* token value */
	Expr	*result;	/* result of parsing */
	void	*parser;	/* the (lemon generated) parser */

	memset(&s, 0, sizeof(s));
	s.file = src_file;
	s.line = 1;

	parser = snlParserAlloc(malloc);
	do
	{
		tt = scan(&s, &tv);
#ifdef	DEBUG
		report_at(tv.file, tv.line, "%2d\t$%s$\n", tt, tv.str);
#endif
		snlParser(parser, tt, tv, &result);
	}
	while (tt);
	snlParserFree(parser, free);
	return result;
}

#endif /* TEST_LEXER */
