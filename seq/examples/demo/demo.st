/*************************************************************************\
This file is distributed subject to a Software License Agreement found
in the file LICENSE that is included with this distribution.
\*************************************************************************/
/* Demo EPICS sequence */

#define PRINTF errlogPrintf

/* message system choice */
#ifdef KTL

#define PVSYS    "pvsys=ktl"

#define LIGHT    "{prefix}.light"
#define LIGHTON  "{prefix}.lighton"
#define LIGHTOFF "{prefix}.lightoff"
#define VOLTAGE  "{prefix}.voltage"
#define LOLIMIT  "{prefix}.lolimit"
#define HILIMIT  "{prefix}.hilimit"

#else

#define PVSYS    "pvsys=ca"

#define LIGHT    "{prefix}:light"
#define LIGHTON  "{prefix}:lightOn"
#define LIGHTOFF "{prefix}:lightOff"
#define VOLTAGE  "{prefix}:voltage"
#define LOLIMIT  "{prefix}:loLimit"
#define HILIMIT  "{prefix}:hiLimit"

#endif

program demo( PVSYS )

%%#include "errlog.h"

/* options */
option  +r;

/* control variables and transition points for light control */
int     light;
assign  light    to LIGHT;

double  lightOn;
assign  lightOn  to LIGHTON;
monitor lightOn;

double  lightOff;
assign  lightOff to LIGHTOFF;
monitor lightOff;

/* control variables and limits for voltage control */
double  voltage;
assign  voltage  to VOLTAGE;
monitor voltage;

double  loLimit;
assign  loLimit  to LOLIMIT;
monitor loLimit;
evflag  loFlag;
sync    loLimit  to loFlag;

double  hiLimit;
assign  hiLimit  to HILIMIT;
monitor hiLimit;
evflag  hiFlag;
sync    hiLimit  to hiFlag;


/* light control state-set */
ss light {
    state START {
        when () {
            light = 0;
            pvPut( light );
        } state LIGHT_OFF
    }

    state LIGHT_OFF {
        when ( voltage > lightOn ) {
            light = 1;
            pvPut( light );
            PRINTF( "light_off -> light_on\n" );
        } state LIGHT_ON
    }

    state LIGHT_ON {
        when ( voltage < lightOff ) {
            light = 0;
            pvPut( light );
            PRINTF( "light_on -> light_off\n" );
        } state LIGHT_OFF
    }
}

/* voltage ramp state set */
ss ramp {
    state START {
        when () {
            voltage = loLimit;
            pvPut( voltage );
            PRINTF("start -> ramp_up\n");
        } state RAMP_UP
    }

    state RAMP_UP {
        when ( voltage > hiLimit ) {
            PRINTF("ramp_up -> ramp_down\n");
        } state RAMP_DOWN

        when ( delay( 0.1 ) ) {
            voltage += 0.1;
            if ( pvPut( voltage ) < 0 )
                PRINTF( "pvPut( voltage ) failed\n" );
            /* PRINTF( "voltage increment\n" ); */
        } state RAMP_UP
    }

    state RAMP_DOWN {
        when ( voltage < loLimit ) {
            PRINTF("ramp_down -> ramp_up\n");
        } state RAMP_UP

        when ( delay( 0.1 ) ) {
            voltage -= 0.1;
            if ( pvPut( voltage ) < 0 )
                PRINTF( "pvPut( voltage ) failed\n" );
            /* PRINTF( "voltage decrement\n" ); */
        } state RAMP_DOWN
    }
}

/* hi / lo limit constraint state set */
ss limit {
    state START {
        when ( efTestAndClear( loFlag ) && loLimit > hiLimit ) {
            hiLimit = loLimit;
            pvPut( hiLimit );
        } state START

        when ( efTestAndClear( hiFlag ) && hiLimit < loLimit ) {
            loLimit = hiLimit;
            pvPut( loLimit );
        } state START
    }
}
