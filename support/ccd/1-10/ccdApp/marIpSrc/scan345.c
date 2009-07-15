/*********************************************************************
 *
 * scan345: scan345.c 
 *
 * Copyright by Dr. Claudio Klein, X-Ray Research GmbH.
 *
 * Version:     4.0
 * Date:        30/10/2002
 *
 * Version	Date		Mods
 * 4.0		30/10/2002	Support for center offset correction added
 *				Support for 32-bit nb_code added
 * 3.1		14/05/2002	Make compatible to VMS
 *
 **********************************************************************/

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#ifndef VMS
#include <sys/wait.h>
#endif

#ifdef __osf__
#include <sys/types.h>
#include <stdlib.h>
#endif

#if ( VMS  || __vms )
#include <stdlib.h>
#include <unixlib.h>
#include <processes.h>
#include <signal.h>
#endif

#define MAR_GLOBAL 
#include "version.h"
#include "marcmd.h"
#include "marglobals.h"
#include "mararrays.h"
#include "esd.h"

/*
 * Global variables 
 */

int		timeout			= 0;
int		input_offset		= -999999;
float		input_scale		= -1.0;
int		input_priority 		= -1;
int		status_interval		= 0;
char            do_xform		= 0;

char         	input_keep_spiral	= 0;
char   	 	input_keep_image 	= 1;	
char   	 	input_skip_op    	= 0;	
char   	 	skip_selftest    	= 1;	
char     	no_xform         	= 0;	

/*
 * Local variables 
 */

static char	mar_continue=0;

/*
 * Local functions
 */
int 		main		(int, char **);
static void	usage		(void);
void		mar_kill	(int);
extern void	mar_abort	(int);
static void	mar_sig		(int);

extern void     mar_quit( int );
/*
 *************************************************************************
 *
 * Main Program.
 *
 *************************************************************************
 */
int main(int argc, char **argv)
{
int 		scanner_status=0;
int 		i;

extern char     keep_image;

/*
 * External functions
 */

extern int 	get_status	();
extern int 	marStartupi	();
extern int	marTask		(int, float);

    strcpy( mar_host, "\0" );
    mar_port 	= 0;
    debug    	= 0;
    verbose  	= 0;

    /*
     * Parse command line
     */
    for (argv++; argc-- > 1; argv++ ) {

	/* Help */
        if (!strcmp(*argv,"-h") || strstr(*argv,"--help" )) {
	 	usage();
	}

	/* Keep spiral file on output */
        else if (!strcmp(*argv,"-k") || strstr( *argv, "-keep") ) {
	 	input_keep_spiral=1;	
	 	keep_spiral=1;	
	}

	/* Skip selftest */
        else if (strstr(*argv,"-nose") ) {
	 	skip_selftest=1;	
	}

	/* Skip transformation */
        else if (strstr(*argv,"-noxf") ) {
	 	input_keep_image=0;	
		keep_image = 0;
		no_xform = 1;
	}

	/* Skip image output */
        else if (strstr(*argv,"-noop") ) {
	 	input_skip_op=1;	
	}

	/* Debug */
        else if (!strcmp(*argv,"-d") || strstr( *argv, "-debug") ) {
		argc--;
		if ( argc <= 0 ) {
			usage();
		}
		else {
			argv++;
	 		debug = atoi( *argv );
		}
	}

	/* Priority */
        else if (strstr(*argv,"-prio") ) {
		argc--;
		if ( argc <= 0 ) {
			usage();
		}
		else {
			argv++;
	 		input_priority = atoi( *argv );
		}
	}

	/* Net port */
        else if (strstr(*argv,"-port") ) {
		argc--;
		if ( argc <= 0 ) {
			usage();
		}
		else {
			argv++;
	 		mar_port=atoi( *argv );
		}
	}

	/* Net host */
        else if (strstr(*argv,"-host") ) {
		argc--;
		if ( argc <= 0 ) {
			usage();
		}
		else {
			argv++;
			strcpy( mar_host, *argv );
		}
	}


	/* Apply scale factor to data */
        else if (strstr(*argv,"-s") ) {
		argc--;
		if ( argc <= 0 ) {
			usage();
		}
		else {
			argv++;
	 		input_scale = atof( *argv );
		}
	}

	/* Apply offset to data */
        else if (strstr(*argv,"-o") ) {
		argc--;
		if ( argc <= 0 ) {
			usage();
		}
		else {
			argv++;
	 		input_offset = atoi( *argv );
		}
	}

	/* Print all scanner messages on stdout */
        else if (strstr(*argv,"-v"))
		verbose++;

        else if (strstr(*argv,"-m")) {
		argc--;
		if ( argc <= 0 ) {
			usage();
		}
		else {
			argv++;
	 		verbose=atoi( *argv );
			if ( verbose < 0 || verbose > 5 ) {
				verbose = 0;
				usage();
			}
		}
	}
	else {
		usage();
		exit( 0 );
	}

    }

    if ( verbose > 2 ) {
	if ( !input_keep_image ) fprintf( stdout, "scan345: Images will not be transformed...\n");
	if ( input_keep_spiral       ) fprintf( stdout, "scan345: Spiral files will be produced ...\n");
	if ( skip_selftest           ) fprintf( stdout, "scan345: Selftest will be skipped ...\n");
    }

    /************************************************
     * Figure out if we are in control of scanner 
     ************************************************/

    scanner_status = marStartup( );

    if ( !netcontrol ) {
#ifndef VMS
	raise(SIGSTOP);
#endif
	fprintf( stdout, "scan345: continue WITHOUT network control !!!\n");
    }

    /* Read scanner status to get current scanner info */
    i = get_status();
    sleep( 1 );
    i = get_status();
    
    /* Register signal handlers */
    if ( netcontrol == 1 ) {
	signal( SIGINT,  mar_kill );
	signal( SIGKILL, mar_kill );
	signal( SIGTERM, mar_kill );
	/* Abort signal */
	signal( SIGQUIT, mar_abort);
	signal( SIGPIPE, mar_kill );
    }
#ifndef VMS
    else {
	signal( SIGCONT, mar_sig );
	signal( SIGUSR1, mar_sig );
    }
#endif

    /* Startup procedure */
    i=marTask( MDC_COM_STARTUP, 1.0 );

    /* Go into command loop */
    while( 1 ) {
    	i=get_status();
    }
    mar_quit( (int)0 );

    exit(0);
}

/******************************************************************
 * Function: usage
 ******************************************************************/
static void usage()
{
	printf( "scan345: Version %s (%s)\n",MAR_VERSION,__DATE__);
	printf( "scan345: wrong arguments on command line\n");
	printf( "Usage  : scan345 [options]\n");
	printf( "       [-more 0...3]         verbose level\n");
#ifdef DEBUG
	printf( "       [-debug 0...3]        debugging level\n");
#endif
	printf( "       [-noself]             skip selftest\n");
	printf( "       [-noxf]               do not transform spirals\n");
	printf( "       [-noop]               do not write output file\n");
	printf( "       [-host host]          host name of scanner\n");
	printf( "       [-port port]          port number of scanner\n");
	printf( "       [-off  offset]        apply offset to spiral data\n");
	printf( "       [-sc   scale ]        apply scale  to spiral data\n");
	printf( "       [-h]                  help\n");
	exit( 0 );
}

/******************************************************************
 * Function: mar_sig
 ******************************************************************/
static void mar_sig( int signo )
{
	/*
	 * The signal SIGUSR1 and SIGCONT come from marstart
	 * SIGUSR1: continue
	 * SIGUSR2: exit
 	 */
#ifndef VMS
	if ( signo == SIGCONT ) {
		printf( "scan345: SIGCONT caught\n");
		mar_continue = 1;
	}
	else if ( signo == SIGUSR1 ) {
		printf( "scan345: SIGUSR1 caught\n");
		if ( fpout != NULL ) fclose( fpout );
		exit( 0 );
	}
#endif
}

/******************************************************************
 * Function: mar_kill
 ******************************************************************/
void mar_kill( int signo )
{
printf("scan345: Killed by signal %d\n",signo);
                mar_quit( (int)0 );
}
