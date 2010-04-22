TOP = ../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#  Internal debug control
#USR_CPPFLAGS = -DDEBUG=TRUE

#  YACC options (generate y.tab.h; generate y.output)
YACCOPT	= -d -v

#  snc product
PROD_HOST	= snc

PROD_LIBS += $(EPICS_BASE_IOC_LIBS)


SRCS	= snc_main.c parse.c phase2.c gen_ss_code.c gen_tables.c snc.c \
          sncVersion.c

#  Lex library
SYS_PROD_LIBS_WIN32    += -nil-
SYS_PROD_LIBS_cygwin32 += -nil-
SYS_PROD_LIBS_Linux    += fl
SYS_PROD_LIBS_DEFAULT  += l

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

#  The generated lex file includes snc.h
#snc_lex.c: snc.h ../snc_lex.l
snc$(OBJ): snc_lex.c

#  How to generate the version file
sncVersion.c: $(TOP)/configure/CONFIG
	$(RM) $@
	$(PERL) ../sncVersion.pl $(SEQ_VERSION) > $@

clean::
	@$(RM) snc.c snc_lex.c

snc.c: snc_lex.c
