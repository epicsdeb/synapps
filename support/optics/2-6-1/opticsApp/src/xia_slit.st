/* Includes Pete's modification for new format of ID string (XIA dropped
the leading zero. */

program xia_slit ("name=hsc1, S=ito:serial:b0, P=ito:, HSC=hsc1:")

option +r;  /* re-usable code, can load more than once */
option +d;  /* turn ON run-time debugging messages */

/*
 * usage:
 ld < xia_slit.o
 seq &xia_slit "name=hsc1, S=ito:serial:b0, P=ito:, HSC=hsc1:"
 
 * to kill the running sequence program:
 td "hsc1"

       WISH LIST for XIAHSC
       ====================
   * support device aliases
   * motor velocity control
   * gear backlash setting
   * status bits
   * read limits from controller
   * progress indication (not provided on device)
   * support for INQUIRY command
   * user coordinates
   * user can change the serial port to be used

 */

/*------------------------------------------------ 
  ------------------------------------------------   assignments
  ------------------------------------------------ */

#define ASSIGN(SIZE, LOCAL_VAR, PV) \
        SIZE LOCAL_VAR;             \
        assign LOCAL_VAR to PV;

#define ASSIGN_MONITOR(SIZE, LOCAL_VAR, PV) \
        ASSIGN(SIZE, LOCAL_VAR, PV)         \
        monitor LOCAL_VAR;

#define ASSIGN_MON_FLAG(SIZE, LOCAL_VAR, PV, LOCAL_FLAG) \
        ASSIGN_MONITOR(SIZE, LOCAL_VAR, PV)              \
        evflag LOCAL_FLAG;                               \
        sync   LOCAL_VAR LOCAL_FLAG;

#define DEBUG_PRINT(DEBUG_LEVEL,FMT)         \
  if (xiahscDebug >= DEBUG_LEVEL) {          \
    printf("(%s,%d): ", __FILE__, __LINE__); \
    printf(FMT);				\
    printf("\n");				\
  }

#define DEBUG_PRINT1(DEBUG_LEVEL,FMT,V)      \
  if (xiahscDebug >= DEBUG_LEVEL) {          \
    printf("(%s,%d): ", __FILE__, __LINE__); \
    printf(FMT,V);				\
    printf("\n");				\
  }

#define DEBUG_PRINT2(DEBUG_LEVEL,FMT,V1,V2)  \
  if (xiahscDebug >= DEBUG_LEVEL) {          \
    printf("(%s,%d): ", __FILE__, __LINE__); \
    printf(FMT,V1,V2);				\
    printf("\n");				\
  }

#define DEBUG_PRINT3(DEBUG_LEVEL,FMT,V1,V2,V3) \
  if (xiahscDebug >= DEBUG_LEVEL) {            \
    printf("(%s,%d): ", __FILE__, __LINE__);   \
    printf(FMT,V1,V2,V3);				\
    printf("\n");				\
  }

ASSIGN_MON_FLAG (int,  xiahscDebug, "{P}{HSC}debug", xiahscDebug_mon)

    /**** 
     **** asyn record (was "generic serial record")
     ****/
ASSIGN_MON_FLAG(string, s_ainp, "{S}.AINP", s_ainp_mon)
ASSIGN_MONITOR(string, s_tinp, "{S}.TINP")
ASSIGN        (string, s_aout,  "{S}.AOUT")
ASSIGN        (short,  s_baud,  "{S}.BAUD")
ASSIGN        (short,  s_dbit,  "{S}.DBIT")
ASSIGN        (short,  s_fctl,  "{S}.FCTL")
/* ASSIGN        (short,  s_idel,  "{S}.IDEL") not in asyn record */
ASSIGN        (string,  s_ieos,  "{S}.IEOS") /* replaces idel */
ASSIGN        (short,  s_ifmt,  "{S}.IFMT")
ASSIGN_MONITOR(short,  s_nord,  "{S}.NORD")
ASSIGN        (short,  s_nrrd,  "{S}.NRRD")
/* ASSIGN        (short,  s_odel,  "{S}.ODEL") not in asyn record */
ASSIGN        (string,  s_oeos,  "{S}.OEOS") /* replaces odel */
ASSIGN        (short,  s_ofmt,  "{S}.OFMT")
ASSIGN        (short,  s_proc,  "{S}.PROC")
ASSIGN        (short,  s_prty,  "{S}.PRTY")
ASSIGN        (short,  s_sbit,  "{S}.SBIT")
ASSIGN        (short,  s_scan,  "{S}.SCAN")
ASSIGN_MONITOR(int,    s_sevr,  "{S}.SEVR")
ASSIGN        (short,  s_tmod,  "{S}.TMOD")
ASSIGN_MONITOR(double,   s_tmot,  "{S}.TMOT") /* was long milliseconds */


    /**** 
     **** XIAHSC database 
     ****/
ASSIGN         (string, port,      "{P}{HSC}port")
ASSIGN_MON_FLAG(string, hID,       "{P}{HSC}hID",     hID_mon)
ASSIGN_MON_FLAG(string, vID,       "{P}{HSC}vID",     vID_mon)
ASSIGN_MON_FLAG(short,  hOrient,   "{P}{HSC}hOrient", hOrient_mon)
ASSIGN_MON_FLAG(short,  vOrient,   "{P}{HSC}vOrient", vOrient_mon)

ASSIGN         (short,  hBusy,         "{P}{HSC}hBusy")
ASSIGN_MONITOR (short,  h_Busy,        "{P}{HSC}h_Busy")
ASSIGN         (short,  h_BusyOff,     "{P}{HSC}h_BusyOff.PROC")
ASSIGN         (short,  h_BusyDisable, "{P}{HSC}h_BusyDisable")

ASSIGN         (short,  vBusy,         "{P}{HSC}vBusy")
ASSIGN_MONITOR (short,  v_Busy,        "{P}{HSC}v_Busy")
ASSIGN         (short,  v_BusyOff,     "{P}{HSC}v_BusyOff.PROC")
ASSIGN         (short,  v_BusyDisable, "{P}{HSC}v_BusyDisable")

ASSIGN_MON_FLAG(short,  stop,         "{P}{HSC}stop",    stop_mon)
ASSIGN_MONITOR (short,  locate,       "{P}{HSC}locate")
ASSIGN_MONITOR (short,  init,         "{P}{HSC}init")
ASSIGN_MONITOR (short,  enable,       "{P}{HSC}enable")
ASSIGN_MONITOR (short,  calibrate,    "{P}{HSC}calib")

/* slit positions
 * Note - we want to be notified when a client writes to these positions,
 * so we define an event associated with them.  Each slit position forward
 * links to a <position>_event record, which posts a monitor every time it
 * processes (MDEL=-1).  But sometimes we will want to write to the positions,
 * and we don't want to get events from these writes, because we would then
 * have to distinguish client events from our write-back events.  The database
 * is constructed so we can write to, e.g., load_l, which will disable l_event,
 * write to l, and then re-enable l_event. 
 */ 
ASSIGN_MONITOR (double, t,            "{P}{HSC}t")
ASSIGN_MON_FLAG(double, t_event,      "{P}{HSC}t_event", t_mon)
ASSIGN_MONITOR (double, t_hi,         "{P}{HSC}t.HOPR")
ASSIGN_MONITOR (double, t_lo,         "{P}{HSC}t.LOPR")
ASSIGN_MONITOR (double, t_RB,         "{P}{HSC}tRB")
ASSIGN         (double, load_t,       "{P}{HSC}load_t.B")

ASSIGN_MONITOR (double, l,            "{P}{HSC}l")
ASSIGN_MON_FLAG(double, l_event,      "{P}{HSC}l_event", l_mon)
ASSIGN_MONITOR (double, l_hi,         "{P}{HSC}l.HOPR")
ASSIGN_MONITOR (double, l_lo,         "{P}{HSC}l.LOPR")
ASSIGN_MONITOR (double, l_RB,         "{P}{HSC}lRB")
ASSIGN         (double, load_l,       "{P}{HSC}load_l.B")

ASSIGN_MONITOR (double, b,            "{P}{HSC}b")
ASSIGN_MON_FLAG(double, b_event,      "{P}{HSC}b_event", b_mon)
ASSIGN_MONITOR (double, b_hi,         "{P}{HSC}b.HOPR")
ASSIGN_MONITOR (double, b_lo,         "{P}{HSC}b.LOPR")
ASSIGN_MONITOR (double, b_RB,         "{P}{HSC}bRB")
ASSIGN         (double, load_b,       "{P}{HSC}load_b.B")

ASSIGN_MONITOR (double, r,            "{P}{HSC}r")
ASSIGN_MON_FLAG(double, r_event,      "{P}{HSC}r_event", r_mon)
ASSIGN_MONITOR (double, r_hi,         "{P}{HSC}r.HOPR")
ASSIGN_MONITOR (double, r_lo,         "{P}{HSC}r.LOPR")
ASSIGN_MONITOR (double, r_RB,         "{P}{HSC}rRB")
ASSIGN         (double, load_r,       "{P}{HSC}load_r.B")

ASSIGN_MONITOR (double, h0,            "{P}{HSC}h0")
ASSIGN_MON_FLAG(double, h0_event,     "{P}{HSC}h0_event", h0_mon)
ASSIGN_MONITOR (double, h0_hi,        "{P}{HSC}h0.HOPR")
ASSIGN_MONITOR (double, h0_lo,        "{P}{HSC}h0.LOPR")
ASSIGN_MONITOR (double, h0_RB,        "{P}{HSC}h0RB")
ASSIGN         (double, load_h0,      "{P}{HSC}load_h0.B")

ASSIGN_MONITOR (double, v0,            "{P}{HSC}v0")
ASSIGN_MON_FLAG(double, v0_event,     "{P}{HSC}v0_event", v0_mon)
ASSIGN_MONITOR (double, v0_hi,        "{P}{HSC}v0.HOPR")
ASSIGN_MONITOR (double, v0_lo,        "{P}{HSC}v0.LOPR")
ASSIGN_MONITOR (double, v0_RB,        "{P}{HSC}v0RB")
ASSIGN         (double, load_v0,      "{P}{HSC}load_v0.B")

ASSIGN_MONITOR (double, width,        "{P}{HSC}width")
ASSIGN_MON_FLAG(double, width_event,  "{P}{HSC}width_event", width_mon)
ASSIGN_MONITOR (double, width_hi,     "{P}{HSC}width.HOPR")
ASSIGN_MONITOR (double, width_lo,     "{P}{HSC}width.LOPR")
ASSIGN_MONITOR (double, width_RB,     "{P}{HSC}widthRB")
ASSIGN         (double, load_width,    "{P}{HSC}load_width.B")

ASSIGN_MONITOR (double, height,       "{P}{HSC}height")
ASSIGN_MON_FLAG(double, height_event, "{P}{HSC}height_event",  height_mon)
ASSIGN_MONITOR (double, height_hi,    "{P}{HSC}height.HOPR")
ASSIGN_MONITOR (double, height_lo,    "{P}{HSC}height.LOPR")
ASSIGN_MONITOR (double, height_RB,    "{P}{HSC}heightRB")
ASSIGN         (double, load_height,  "{P}{HSC}load_height.B")

ASSIGN         (int,    error,        "{P}{HSC}error")
ASSIGN         (string, errMsg,       "{P}{HSC}errMsg")

    /****  
     **** local variables 
     ****/
char   scratch[256];
char   myChar;
double dScratch;
long   numWords;
string word1;         string word2; string word3; 
string word4;         string word5; string word6;
string movingMotorID;
int h_isMoving;
int v_isMoving;
long   pos_a;         long   pos_b;
double pos_ag;        double pos_bg;
long l_raw; long r_raw; long t_raw; long b_raw;
string id;            short  hasID;
string hscErrors[14];
evflag start_H_move;  evflag start_V_move;
evflag wait_H_input;  evflag wait_V_input;
evflag new_H_RB;      evflag new_V_RB;
evflag move_h;        evflag move_v;
double t_old;         double l_old;
double b_old;         double r_old;
double width_old;     double height_old;
double h0_old;        double v0_old;
int    init_delay;
#define  AINP_FIFO_SIZE  20
string ainp_fifo[AINP_FIFO_SIZE];
int    ainp_fifo_write;
int    ainp_fifo_read;
int    i;

/* for firmware limits */
int    hOrigin;      int    vOrigin;
int    hOuterLimit;  int    vOuterLimit;
string hsc_command;
string hsc_response;
int    hsc_timeout;

%% #include <math.h>
%% #include <string.h>
%% #include <epicsThread.h>
/*
%% static int i;
*/

/*------------------------------------------------ 
  ------------------------------------------------   macros
  ------------------------------------------------ */

#define SCAN_PASSIVE    0
#define SCAN_RAPID      7
#define TMOD_WRITE_READ 0
#define TMOD_WRITE      1
#define TMOD_READ       2
#define LINE_FEED       "\n"      /* was 10 for generic serial record */
#define CARRIAGE_RETURN "\r"      /* was 13 for generic serial record */
#define ORIENT_LEFT_RIGHT      0
#define ORIENT_RIGHT_LEFT      1
#define ORIENT_TOP_BOTTOM      0
#define ORIENT_BOTTOM_TOP      1
#define MOTOR_IDLE             0
#define MOTOR_BUSY             1
#define STOP_IDLE              0
#define STOP_STOP              1
#define CALIBRATE_DONE         0
#define CALIBRATE_START        1

#define BAUD_9600	6 /* was 5 for serial record */
#define DBIT_8      4 /* was 3 for serial record */
#define SBIT_1      1 /* was 0 for serial record */
#define PRTY_NONE	1 /* was 0 for generic serial */
#define FCTL_NONE	1 /* was 0 for serial record */

#define MOVING_POLL_INTERVAL 0.2
#define IDLE_POLL_INTERVAL   30
#define MOVE_TIMEOUT         300.0
#define ERROR_RECONNECT_INTERVAL   (10 * 60.0)

#define STEPS_PER_mm    400.0

/*
 * there are 14 error messages from the HSC
 * (numbered in HSC from 0-13, I move them to 1-14)
 * error 0 means no error
 */
#define NO_ERROR             0
#define ERROR_SOFT_LIMITS   15
#define ERROR_UNKNOWN       16
#define ERROR_BAD_ID        17
#define ERROR_COMM_ERROR    18

#define WRITE_HSC(MODE, HSC_ID, HSC_COMMAND)  {		\
  sprintf (hsc_command, "!%s %s", HSC_ID, HSC_COMMAND);	\
  DEBUG_PRINT1(10,"WRITE_HSC: <%s>", hsc_command);	\
  PVPUT(s_scan,  SCAN_PASSIVE);				\
  PVPUT(s_tmod,  MODE);					\
  PVPUTSTR(s_ieos,  LINE_FEED);				\
  PVPUTSTR(s_oeos,  CARRIAGE_RETURN);			\
  PVPUTSTR(s_aout,  hsc_command);			\
}

#define WAIT_HSC_RESPONSE(TICKS) {					\
  for (i = TICKS; i; i--) {						\
    if (efTest (s_ainp_mon)) {						\
      hsc_timeout = 0;  /* Aha! Got a response. Thus, no timeout */	\
      break;	/* input is ready */					\
    }									\
    epicsThreadSleep(.02);						\
  }									\
  if (hsc_timeout == 1) {						\
    strcpy(hsc_response, "");						\
    DEBUG_PRINT1(10,"\nWAIT_HSC_RESPONSE: <%s> -> <<< TIMEOUT >>>",	\
        s_tinp);							\
  } else {								\
    strcpy(hsc_response, s_ainp);					\
    DEBUG_PRINT3(10,"\nWAIT_HSC_RESPONSE: <%s> -> NORD=%d  TINP=%s",	\
        hsc_command, s_nord, s_tinp);					\
  }									\
}

#define TIMEOUT_TICKS 30 /*15*/   /* wait up to 0.25 seconds */

#define WRITE_READ_HSC(HSC_ID, HSC_COMMAND)  {				\
  efClear (s_ainp_mon); /* will wait for the record to process */	\
  hsc_timeout = 1;	  /* assume a timeout */			\
  WRITE_HSC(TMOD_WRITE_READ, HSC_ID, HSC_COMMAND);			\
  WAIT_HSC_RESPONSE(TIMEOUT_TICKS);					\
}

#define PVPUT(PV,VAL)  {PV = (VAL);  pvPut(PV);}
#define PVPUTSTR(MSG,TEXT)  {strcpy(MSG, TEXT); pvPut(MSG);}

#define READ_HSC()   {					\
  PVPUT(s_tmod,  TMOD_READ);				\
  efClear (s_ainp_mon);					\
  PVPUT(s_proc, 1);					\
  for (i = 0; i <= TIMEOUT_TICKS; i++) {		\
    if (efTest (s_ainp_mon)) {				\
      DEBUG_PRINT(10,"READ_HSC: got a message.");	\
      break;						\
    }							\
    epicsThreadSleep(.02);				\
  }							\
}

#define FIFO_PTR_CHECK(PTR) {			\
   if (PTR >= AINP_FIFO_SIZE) PTR = 0; }

int delta;
#define INPUT_TO_FIFO()   {						\
  if (efTest(s_ainp_mon)) {						\
    strcpy(ainp_fifo[ainp_fifo_read++], s_ainp);			\
    FIFO_PTR_CHECK(ainp_fifo_read);					\
    delta = ainp_fifo_read-ainp_fifo_write;				\
    if (delta < 0) delta += AINP_FIFO_SIZE;				\
    DEBUG_PRINT2(10,"FIFO_IN: adding msg %d:\n ->'%s'", delta, s_ainp);	\
  }}

#define FIFO_TO_OUTPUT(OUTPUT)   {					\
  delta = ainp_fifo_read-ainp_fifo_write;				\
  if (delta < 0) delta += AINP_FIFO_SIZE;				\
  strcpy (OUTPUT, ainp_fifo[ainp_fifo_write++]);			\
  DEBUG_PRINT2(10,"FIFO_OUT: got message %d:\n ->'%s'", delta, OUTPUT);	\
  FIFO_PTR_CHECK(ainp_fifo_write);						\
  }

#define S_PORT_CONFIG(BAUD, DBIT, SBIT, PRTY, FCTL) {	\
      PVPUT(s_baud,  BAUD);				\
      PVPUT(s_dbit,  DBIT);				\
      PVPUT(s_sbit,  SBIT);				\
      PVPUT(s_prty,  PRTY);				\
      PVPUT(s_fctl,  FCTL); }

#define MIN(A,B,C) {if ((A)<(B)) {C=(A);} else {C=(B);}}
#define MAX(A,B,C) {if ((A)>(B)) {C=(A);} else {C=(B);}}
#define SMALL 1.e-9
#define DIFFERENT(A,B) (fabs((A)-(B))>SMALL)

#define RAW_TO_DIAL(A, ORIGIN) ( (A-ORIGIN)/STEPS_PER_mm )
#define DIAL_TO_RAW(A, ORIGIN) ((A) * STEPS_PER_mm + 0.5 + (ORIGIN))

#define LIMIT_TEST(LO,VAL,HI)	( (LO <= VAL) && (VAL <= HI) )

/* 
 * macros to command the HSC-1
 */
#define HSC1_OUTER_MOTION_LIMIT "R 1"
#define HSC1_ORIGIN_POSITION    "R 2"
#define HSC1_MOTOR_A_POSITION   "R 3"
#define HSC1_MOTOR_B_POSITION   "R 4"
#define HSC1_MOTOR_DELAY_CODE   "R 5"
#define HSC1_GEAR_BACKLASH      "R 6"
#define HSC1_CONTROL_WORD       "R 7"

#define HSC1_KILL_MOVEMENT       "K"
#define HSC1_POSITION_INQUIRY    "P"
#define HSC1_CALIBRATE_IMMEDIATE "0 I"

/* 
 * slit state:  xiahsc
 */
ss xiahsc {

  state startup {
    when () {
      pvGet (s_ainp);
      pvGet (s_aout);
      PVPUT(init, 1);		/* need to initialize */
      init_delay = 1.0;		/* time to delay before re-init */
      /*
       * users have requested some mention of the associated serial port name
       * for the future:  Can use change the serial port via this PV?
       */
      strcpy (port, macValueGet("S"));
      pvPut(port);

      strcpy (hscErrors[0],  "Missing Command");
      strcpy (hscErrors[1],  "Unrecognized Command");
      strcpy (hscErrors[2],  "Input Buffer Overflow");
      strcpy (hscErrors[3],  "No new Alias Given");
      strcpy (hscErrors[4],  "Alias too long");
      strcpy (hscErrors[5],  "Invalid Field Parameter");
      strcpy (hscErrors[6],  "Value Out of Range");
      strcpy (hscErrors[7],  "Parameter is read-only");
      strcpy (hscErrors[8],  "Invalid/Missing Argument");
      strcpy (hscErrors[9],  "No Movement Required");
      strcpy (hscErrors[10], "Uncalibrated: no motion allowed");
      strcpy (hscErrors[11], "Motion out of range");
      strcpy (hscErrors[12], "Invalid/missing direction character");
      strcpy (hscErrors[13], "Invalid Motor Specified");

      /*
       * keep track of last known good value
       * to impose motor limits
       */
      t_old = t;	b_old = b;
      l_old = l;	r_old = r;
      h0_old = h0;	width_old = width;
      v0_old = v0;	height_old = height;

      /* controller default settings */
      hOrigin = vOrigin = 400;  		/* origin position */
      hOuterLimit = vOuterLimit = 4400;		/* outer limit (inner limit always 0) */

      PVPUT(enable,       1);
      PVPUT(s_tmot,    .250); /* seconds (generic serial used milliseconds) */

    } state init
  }

  /* 
   */
  state disable { 
    when (enable) { 
      init_delay = 1.0;
      PVPUT(init, 1);
    } state init
  }

  /*
   * _any_ communications errors will land us here
   */
  state comm_error {
    when (init || !s_sevr || delay(ERROR_RECONNECT_INTERVAL)) { 
      DEBUG_PRINT2 (20, "comm_error: init=%d  s_sevr=%d", init, s_sevr)
      init_delay = 1.0;
      s_sevr = 0;  /* try this, just to see if communications are working now */
      PVPUT(init, 1);
    } state init
  }

 /*  */
  state init {
    when (!enable) {} state disable
    when ((efTest (hID_mon) || efTest (vID_mon))) {
      /*
       * configure the serial port parameters
       * users have requested some mention of the associated serial port name
       * reset it here in case some curious user changed it
       */
      strcpy (port, macValueGet("S"));
      pvPut(port);
      /*
       * make a quick check after user has changed an ID
       */
      init_delay = 0;
      efClear (hID_mon); 
      efClear (vID_mon); 
      efClear (hOrient_mon);
      efClear (vOrient_mon);
    } state init
    when (init && delay(init_delay)) { 
      /*
       * verify that the ID strings look OK
       */
      error = 0;
      if (!strcmp (hID, vID)) { 
        PVPUTSTR(errMsg, "H & V IDs must be different");
        error++;
      }

      if (sscanf (hID, "XIAHSC-%c-%ld%s", &myChar, &numWords, scratch) != 2) { 
        if (sscanf (hID, "%c-%ld%s", &myChar, &numWords, scratch) != 2) { 
          PVPUTSTR(errMsg, "H ID not a valid HSC ID");
          error++;
        }
      }
      if (sscanf (vID, "XIAHSC-%c-%ld%s", &myChar, &numWords, scratch) != 2) { 
        if (sscanf (vID, "%c-%ld%s", &myChar, &numWords, scratch) != 2) { 
          PVPUTSTR(errMsg, "V ID not a valid HSC ID");
          error++;
        }
      }
      if (!strlen (hID)) { 
        PVPUTSTR(errMsg, "define H ID string");
        error++;
      }
      if (!strlen (vID)) { 
        PVPUTSTR(errMsg, "define V ID string");
        error++;
      }
      /*
       * could also try to use each ID string
       */
      if (error) { 
        PVPUT(error, ERROR_BAD_ID);
      } else { 
        PVPUT(init, 0);
        PVPUTSTR(errMsg, "no error");
      }
      pvPut(error);
      efClear (hID_mon); 
      efClear (vID_mon); 
      /* recheck every 30 seconds for appropriate ID strings */
      init_delay = 30;
    } state init
    when (!init && delay(0.1)) {
      PVPUT(hBusy,   MOTOR_IDLE);
      PVPUT(vBusy,   MOTOR_IDLE);
      PVPUT(error,   NO_ERROR);
      PVPUT(h_BusyOff,   0);
      PVPUT(v_BusyOff,   0);
      /*
       * serial port configuration
       */
      S_PORT_CONFIG(BAUD_9600, DBIT_8, SBIT_1, PRTY_NONE, FCTL_NONE);
      /*
       * asyn record configuration
       */
      PVPUTSTR(s_ieos, LINE_FEED);	/* input delimiter is <LF> */
      PVPUT(s_ifmt,  0);	/* ASCII input format */
      PVPUT(s_nrrd,  0);	/* number of characters to read */
      PVPUTSTR(s_oeos, CARRIAGE_RETURN);	/* output delim is <CR> */
      PVPUT(s_ofmt, 0);	/* ASCII output format */
      PVPUT(s_tmod,  0);	/* write/read transfer mode */
      PVPUT(s_scan,  SCAN_PASSIVE);	/* serial port is idle */
      PVPUT(s_tmot, .250);	/* timeout in 1/4 second */

      efClear (s_ainp_mon);	/* watch serial record process */

      efClear (wait_H_input);   /* flag to watch for H axis input */
      efClear (wait_V_input);   /* flag to watch for V axis input */

      efClear (move_h);		/* flag H axis to be moved */
      efClear (move_v);		/* flag V axis to be moved */
      efClear (new_H_RB);	/* new readback data available */
      efClear (new_V_RB);	/* new readback data available */

      PVPUT(stop, STOP_IDLE);	/* presume the motors are stopped */
      epicsThreadSleep(.02);	/* allow the command to process */
      efClear (stop_mon);

      efClear (t_mon);  efClear (l_mon);
      efClear (b_mon);  efClear (r_mon);
      efClear (h0_mon); efClear (width_mon);
      efClear (v0_mon); efClear (height_mon);
      PVPUT(calibrate, CALIBRATE_DONE);	/* don't reset pos. to zero */

      efClear (hID_mon);
      efClear (vID_mon);
      efClear (hOrient_mon);
      efClear (vOrient_mon);

      /*
       * Now, everything is initialized
       * stop all the motors and ask where are we
       */
      WRITE_HSC (TMOD_WRITE, "ALL", HSC1_KILL_MOVEMENT);
      epicsThreadSleep(.1);	/* wait for record to process */
      WRITE_HSC (TMOD_WRITE, "ALL", HSC1_POSITION_INQUIRY);
      epicsThreadSleep(.1);	/* wait for record to process */
      ainp_fifo_read = 0;	/* next FIFO position to store input */
      ainp_fifo_write = 0;	/* next FIFO position to process */
      while (!s_sevr) {		/* read until no more input data */
        READ_HSC();		/* process generic serial port */
        INPUT_TO_FIFO();	/* put input string into FIFO */
      }
    } state init_limits
  }

  state init_limits {
    when () {
      /* should check the response for (in order):
         	timeout
         	proper controller ID
         	proper response length
         What to do when each of these fails?
       */
      error = 0;
      WRITE_READ_HSC(hID, HSC1_OUTER_MOTION_LIMIT);
      if (sscanf (hsc_response, "%*s %*s %d", &hOuterLimit) != 1) {
        error++;
        DEBUG_PRINT(5, "Could not read horizontal outer limit");
      }
      WRITE_READ_HSC(hID, HSC1_ORIGIN_POSITION);
      if (sscanf (hsc_response, "%*s %*s %d", &hOrigin) != 1) {
        error++;
        DEBUG_PRINT(5, "Could not read horizontal origin position");
      }
      if (!error) {
        pos_ag = RAW_TO_DIAL(0,           hOrigin);
        pos_bg = RAW_TO_DIAL(hOuterLimit, hOrigin);
        PVPUT(l_lo, pos_ag);  PVPUT(l_hi, pos_bg);
        PVPUT(r_lo, pos_ag);  PVPUT(r_hi, pos_bg);
          /*
           * Calculate the absolute limits of gap and center.
           * (Note the actual limits are also dependent on
           *  current values of l and r!  However, these
           *  situations will be trapped by the limits
           *  on the corresponding axes.)
           */
        MAX(l_lo, r_lo, dScratch);
        MAX(dScratch, 0.0, width_lo);  pvPut(width_lo);
        PVPUT(width_hi, l_hi + r_hi);
        PVPUT(h0_hi, (r_hi - l_lo)/2);
        PVPUT(h0_lo, (r_lo - l_hi)/2);
      }

      error = 0;
      WRITE_READ_HSC(vID, HSC1_OUTER_MOTION_LIMIT);
      if (sscanf (hsc_response, "%*s %*s %d", &vOuterLimit)!= 1) {
        error++;
        DEBUG_PRINT(5, "Could not read vertical outer limit");
      }
      WRITE_READ_HSC(vID, HSC1_ORIGIN_POSITION);
      if (sscanf (hsc_response, "%*s %*s %d", &vOrigin)!= 1) {
        error++;
        DEBUG_PRINT(5, "Could not read vertical origin position");
      }
      if (!error) {
        pos_ag = RAW_TO_DIAL(0,           vOrigin);
        pos_bg = RAW_TO_DIAL(vOuterLimit, vOrigin);
        PVPUT(t_lo, pos_ag);    PVPUT(t_hi, pos_bg);
        PVPUT(b_lo, pos_ag);    PVPUT(b_hi, pos_bg);
          /*
           * Calculate the absolute limits of gap and center.
           * (Note the actual limits are also dependent on 
           *  current values of t and b!  However, these
           *  situations will be trapped by the limits
           *  on the corresponding axes.)
           */
        MAX(t_lo, b_lo, dScratch);
        MAX(dScratch, 0.0, height_lo);  pvPut(height_lo);
        PVPUT(height_hi, t_hi + b_hi);

        PVPUT(v0_hi, (t_hi - b_lo)/2);
        PVPUT(v0_lo, (t_lo - b_hi)/2);
      }
      PVPUT(locate, 1);  /* update the positions from the controller */

    } state idle
  }

/* 
 */
  state idle {

    /*
     * user can disable this program
     */
    when (!enable) { } state disable

    /*
     * user can re-initialize this program
     */
    when (init) {
      init_delay = 1; PVPUT(init, 1); PVPUT(h_BusyOff, 0);  PVPUT(v_BusyOff, 0);
    } state init

    /*
     * user can stop the motors
     */
    when (efTest (stop_mon) && stop) { 
      WRITE_HSC (TMOD_WRITE, "ALL", HSC1_KILL_MOVEMENT);
      epicsThreadSleep(.1);
      efClear (stop_mon);
    } state idle

    /*
     * target value has changed
     */
    when ( efTest (l_mon) ) {
      DEBUG_PRINT1(2,"efTest(l_mon) -> premove, l=%f", l);
    } state premove

    when ( efTest (l_mon)     || efTest (r_mon) 
    ||     efTest (t_mon)     || efTest (b_mon) 
    ||     efTest (h0_mon)    || efTest (height_mon) 
    ||     efTest (v0_mon)    || efTest (width_mon)   ) {
      DEBUG_PRINT(2,"efTest() -> premove");
    } state premove

    /*
     * send a MOVE command to HSC
     */
    when (efTest (start_H_move)) {
      if (hBusy) { 		/* allow an active move to be interrupted */
        DEBUG_PRINT(2,"efTest(start_H_move) and hBusy");
        WRITE_HSC (TMOD_WRITE, hID, HSC1_KILL_MOVEMENT);
        epicsThreadSleep(.5);		/* wait for it to happen */
      } else {
        PVPUT(hBusy, MOTOR_BUSY);	/* tell the world we are busy */
      }
      if (hOrient == ORIENT_LEFT_RIGHT) { 
        pos_ag = l;  pos_bg = r;
      } else { 
        pos_ag = r;  pos_bg = l;
      }
      h_isMoving = 1;
      efSet (wait_H_input);		/* could enable a timeout on the move */
      efClear (start_H_move);		/* OK, we told it to move now */
if (ainp_fifo_read != ainp_fifo_write) printf("start_H_move: fifo not empty\n");
      while (!s_sevr) {		/* read until no more input data */
        READ_HSC();		/* process generic serial port */
        /* discard possibly outdated messages involving hID */
        sscanf(s_ainp, "%%%s", id);
        if (!strcmp(id, hID)) {
          printf("discarding hID message '%s' id='%s'\n", s_tinp, id);
        } else {
          INPUT_TO_FIFO();
        }
      }
      pos_a = DIAL_TO_RAW (pos_ag, hOrigin);	/* convert to raw (motor) coordinates */
      pos_b = DIAL_TO_RAW (pos_bg, hOrigin);	/* convert to raw (motor) coordinates */
      sprintf (scratch, "M %ld %ld", pos_a, pos_b);
      DEBUG_PRINT(2,"start_H_move: writing to motor");
      WRITE_HSC(TMOD_WRITE, hID, scratch);
    } state idle

    when (efTest (start_V_move)) {
      if (vBusy) {  		/* allow an active move to be interrupted */
        WRITE_HSC (TMOD_WRITE, vID, HSC1_KILL_MOVEMENT);
        epicsThreadSleep(.5);		/* wait for it to happen */
      } else {
        PVPUT(vBusy, MOTOR_BUSY);	/* tell the world we are busy */
      }
      if (vOrient == ORIENT_TOP_BOTTOM) { 
        pos_ag = t;  pos_bg = b;
      } else { 
        pos_ag = b;  pos_bg = t;
      }
      v_isMoving = 1;
      efSet (wait_V_input);		/* could enable a timeout on the move */
      efClear (start_V_move);
if (ainp_fifo_read != ainp_fifo_write) printf("start_V_move: fifo not empty\n");
      while (!s_sevr) {		/* read until no more input data */
        READ_HSC();		/* process generic serial port */
        /* discard possibly outdated messages involving vID */
        sscanf(s_ainp, "%%%s", id);
        if (!strcmp(id, vID)) {
          printf("discarding vID message '%s' id='%s'\n", s_tinp, id);
        } else {
          INPUT_TO_FIFO();
        }
      }
      pos_a = DIAL_TO_RAW (pos_ag, vOrigin);	/* convert to raw (motor) coordinates */
      pos_b = DIAL_TO_RAW (pos_bg, vOrigin);	/* convert to raw (motor) coordinates */
      sprintf (scratch, "M %ld %ld", pos_a, pos_b);
      WRITE_HSC(TMOD_WRITE, vID, scratch);
    } state idle

    /*
     * user can request device to resend coordinates
     */
    when (locate) { 	/* poll the generic serial port for input */
      /*
       * users have requested some mention of the associated serial port name
       * reset here in case some curious user changed it
       */
      strcpy (port, macValueGet("S"));
      pvPut(port);
      PVPUT(locate, 0);
      WRITE_HSC (TMOD_WRITE, "ALL", HSC1_POSITION_INQUIRY);
      epicsThreadSleep(.1);
      while (!s_sevr) {		/* read until no more input data */
        DEBUG_PRINT(10,"locate: calling READ_HSC");
        READ_HSC();		/* process generic serial port */
        if (!s_sevr) INPUT_TO_FIFO();	/* put input string into FIFO */
      }
      DEBUG_PRINT(10,"locate: s_sevr = TRUE");
    } state idle

    when (calibrate) {
      /*
       * user requested a device calibration
       * ASSUME the user has closed the slits down
       * all the way.
       */
      WRITE_HSC(TMOD_WRITE, "ALL", HSC1_CALIBRATE_IMMEDIATE)  
      epicsThreadSleep(.1);		/* wait for record to process */
      PVPUT(calibrate, CALIBRATE_DONE);	/* OK, we're done here */
      PVPUT(locate, 1);	/* update with new controller positions */
    } state idle


    /*
     * process the input data
     */
    when (ainp_fifo_read != ainp_fifo_write) { 
      FIFO_TO_OUTPUT(scratch);
      id[0] = word1[0] = word2[0] = word3[0] = word4[0] = 
        word5[0] = word6[0] = 0;
      numWords = sscanf (scratch, "%s %s %s %s %s %s", 
          word1, word2, word3, word4, word5, word6);
      hasID = sscanf (word1, "%%%s", id);
      if (xiahscDebug >= 5 && (numWords>0)) {
        printf("xia_slit:fifo_read: (%ld words)...\n'%s'\n", numWords, scratch);
      }
    } state process_response

    /*
     * new H axis readback data is available
     */
    when (efTest (new_H_RB)) { 
      efClear (new_H_RB);
      /*
       * update the target positions
       * but only if we have not interrupted a move-in-progress
       */
      DEBUG_PRINT3(2,"efTest(new_H_RB)==1; h_Busy|hBusy=%d, l=%f,l_RB=%f", h_Busy|hBusy, l, l_RB);
      if (!hBusy && !h_Busy) {
        if (DIFFERENT(l, l_RB)) PVPUT(load_l, l_RB);
        if (DIFFERENT(r, r_RB)) PVPUT(load_r, r_RB);
        if (DIFFERENT(h0, h0_RB)) PVPUT(load_h0, h0_RB);
        if (DIFFERENT(width, width_RB)) PVPUT(load_width, width_RB);
        efClear (wait_H_input);
      }
    } state idle

    /*
     * new V axis readback data is available
     */
    when (efTest (new_V_RB)) { 
      efClear (new_V_RB);
      /*
       * update the target positions
       * but only if we have not interrupted a move-in-progress
       */
      if (!vBusy && !v_Busy) {
        if (DIFFERENT(t, t_RB)) PVPUT(load_t, t_RB);
        if (DIFFERENT(b, b_RB)) PVPUT(load_b, b_RB);
        if (DIFFERENT(v0, v0_RB)) PVPUT(load_v0, v0_RB);
        if (DIFFERENT(height, height_RB)) PVPUT(load_height, height_RB);
        efClear (wait_V_input);
      }
    } state idle

    /*
     * signal that the KILL command has finished
     */
    when (stop && !efTest (wait_H_input) && !efTest (wait_V_input)) { 
      PVPUT(stop, STOP_IDLE);
      efClear (stop_mon);
    } state idle

    /*
     * things to do in the background ...
     */
    when ((hBusy || vBusy) && delay (MOVING_POLL_INTERVAL)) { 
      PVPUT(locate, 1);		/* signal for a position update */
    } state idle
    when (delay (IDLE_POLL_INTERVAL)) { 
      PVPUT(locate, 1);		/* signal for a position update */
      DEBUG_PRINT(2, "IDLE POLL");
    } state idle
  }

 /* 
 */
  state premove { 
    /*
     * ignore any non-movement monitors
     */
    when (efTest (l_mon) && (l == l_old)) {
      efClear (l_mon);
      PVPUT(h_BusyOff, 0);
      DEBUG_PRINT(2, "l==l_old: ->h_BusyOff");
    } state idle
    when (efTest (r_mon) && (r == r_old)) {
      efClear (r_mon);
      PVPUT(h_BusyOff, 0);
      DEBUG_PRINT(2, "r==r_old: ->h_BusyOff");
    } state idle
    when (efTest (t_mon) && (t == t_old)) {
      efClear (t_mon);
      PVPUT(v_BusyOff, 0);
      DEBUG_PRINT(2, "t==t_old: ->v_BusyOff");
    } state idle
    when (efTest (b_mon) && (b == b_old)) {
      efClear (b_mon);
      PVPUT(v_BusyOff, 0);
      DEBUG_PRINT(2, "b==b_old: ->h_BusyOff");
    } state idle
    when (efTest (h0_mon) && (h0 == h0_old)) {
      efClear (h0_mon);
      PVPUT(h_BusyOff, 0);
      DEBUG_PRINT(2, "h0==h0_old: ->h_BusyOff");
    } state idle
    when (efTest (v0_mon) && (v0 == v0_old)) {
      efClear (v0_mon);
      PVPUT(v_BusyOff, 0);
      DEBUG_PRINT(2, "v0==v0_old: ->v_BusyOff");
    } state idle
    when (efTest (width_mon) && (width == width_old)) {
      efClear (width_mon);
      PVPUT(h_BusyOff, 0);
      DEBUG_PRINT(2, "width==width_old: ->h_BusyOff");
    } state idle
    when (efTest (height_mon) && (height == height_old)) {
      efClear (height_mon);
      PVPUT(v_BusyOff, 0);
      DEBUG_PRINT(2, "height==height_old: ->v_BusyOff");
    } state idle
    when () { 
      /*
       * horizontal axis
       */
      if (efTest (l_mon) || efTest (r_mon) ||
          efTest (h0_mon) || efTest (width_mon)) { 
        /* find out which coordinate scheme changed, calc the other */
        if (efTest (l_mon) || efTest (r_mon)) {
          width = r + l;
          h0 = (r - l) / 2;
        } else {
          l = width / 2 - h0;
          r = width / 2 + h0;
        }
        /* check the limits */
        error = 0;
        if (!LIMIT_TEST (l_lo, l, l_hi)) { l = l_old; error++; }
        if (!LIMIT_TEST (r_lo, r, r_hi)) { r = r_old; error++; }
        if (!LIMIT_TEST (h0_lo, h0, h0_hi)) { h0 = h0_old; error++; }
        if (!LIMIT_TEST (width_lo, width, width_hi)) { 
           width = width_old; error++; }
        if (error) { 
          /* reset values if there was an error */
          if (efTest (l_mon)) PVPUT(load_l, l);
          if (efTest (r_mon)) PVPUT(load_r, r);
          if (efTest (h0_mon)) PVPUT(load_h0, h0);
          if (efTest (width_mon)) PVPUT(load_width, width);
          error = ERROR_SOFT_LIMITS;
          PVPUTSTR(errMsg, "H soft limits exceeded");
          PVPUT(h_BusyOff, 0);
        } else { 
          PVPUTSTR(errMsg, "no error");
          /* update the new coordinates */
          efSet (start_H_move);
          DEBUG_PRINT(2, "premove: efSet(start_H_move)");
          if (efTest (l_mon) || efTest (r_mon)) {
            PVPUT(load_h0, h0);  PVPUT(load_width, width);
          } else {
            PVPUT(load_l, l);  PVPUT(load_r, r);
          }
        }
        pvPut(error);
        efClear(l_mon);   efClear(r_mon);
        efClear(h0_mon);  efClear(width_mon);
      }
      /*
       * vertical axis
       */
      if (efTest (t_mon) || efTest (b_mon) 
      ||  efTest (v0_mon) || efTest (height_mon)) { 
        /* find out which coordinate scheme changed, calc the other */
        if (efTest (t_mon) || efTest (b_mon)) {
          height = t + b;
          v0 = (t - b) / 2;
        } else {
          t = height / 2 + v0;
          b = height / 2 - v0;
        }
        /* check the limits */
        error = 0;
        if (!LIMIT_TEST (t_lo, t, t_hi)) { t = t_old; error++; }
        if (!LIMIT_TEST (b_lo, b, b_hi)) { b = b_old; error++; }
        if (!LIMIT_TEST (v0_lo, v0, v0_hi)) { v0 = v0_old; error++; }
        if (!LIMIT_TEST (height_lo, height, height_hi)) { 
           height = height_old; error++; }
        if (error) { 
          /* reset values if there was an error */
          if (efTest (t_mon)) PVPUT(load_t, t);
          if (efTest (b_mon)) PVPUT(load_b, b);
          if (efTest (v0_mon)) PVPUT(load_v0, v0);
          if (efTest (height_mon)) PVPUT(load_height, height);
          error = ERROR_SOFT_LIMITS;
          PVPUTSTR(errMsg, "V soft limits exceeded");
          PVPUT(v_BusyOff, 0);
        } else { 
          PVPUTSTR(errMsg, "no error");
          /* update the new coordinates */
          efSet (start_V_move);
          if (efTest (t_mon) || efTest (b_mon)) {
            PVPUT(load_v0, v0);  PVPUT(load_height, height);
          } else {
            PVPUT(load_t, t);  PVPUT(load_b, b);
          }
        }
        pvPut(error);
        efClear(t_mon);   efClear(b_mon);
        efClear(v0_mon);  efClear(height_mon);
      }
    } state idle
  }

 /* 
 */
  state process_response {
    entry {
      DEBUG_PRINT1(5, "process_response:entry numWords=%ld", numWords);
    }
    /*
     * various error messages
     */
    when (numWords == -1) {
      /*
       * The last read of a series times out and returns nothing.
       * That's how we know we got everything there was to get.
       */
    } state idle
    when ( (numWords == 2) && (!strcmp(word2, "OK;")) ) { 
      /* Ignore command acknowledge from controller. */
    } state idle
    when ( (numWords >= 2) && (!strcmp(word2, "ERROR;")) ) {
      if (numWords == 2) {
        PVPUT(error, ERROR_UNKNOWN);
        sprintf (errMsg, "%s: unknown error", id);
        pvPut(errMsg);
      } else {
        sscanf (word3, "%d", &error);
        pvPut(error);
        PVPUTSTR(errMsg, hscErrors[error]);
      }
      if (!strcmp(word1, hID)) {
        DEBUG_PRINT1(2, "'%s' reported an error: ->h_BusyOff", hID);
        PVPUT(h_BusyOff, 0);
        h_isMoving = 0;
      } else if (!strcmp(word1, vID)) {
        DEBUG_PRINT1(2, "'%s' reported an error: ->v_BusyOff", vID);
        PVPUT(v_BusyOff, 0);
        v_isMoving = 0;
      }
    } state idle
    /*
     * controller is busy
     */
    when ( (numWords == 2) && (!strcmp(word2, "BUSY;")) ) { 
      /* axes are moving */
      if (!strcmp(id, hID)) { 
        PVPUT(hBusy,   MOTOR_BUSY);
      } else {
        PVPUT(vBusy,   MOTOR_BUSY);
      }
      if (error) { /* don't update unless previous error exists */
        PVPUT(error, 0);
        PVPUTSTR(errMsg, "no error");
      }
    } state idle

    /*
     * position update
     */
    when ( (numWords == 4) && (!strcmp(word4, "DONE;")) ) { 
      /* position info received */
      sscanf (word2, "%ld", &pos_a);
      sscanf (word3, "%ld", &pos_b);
      if (error) { /* don't update unless previous error exists */
        PVPUT(error, 0);
        PVPUTSTR(errMsg, "no error");
      }
      /* did we get where we want to go? */
      if (!strcmp(id, hID)) {
        DEBUG_PRINT2(5,"hDONE: pos_a = %ld, pos_b = %ld", pos_a, pos_b);
        pos_ag = RAW_TO_DIAL(pos_a, hOrigin);
        pos_bg = RAW_TO_DIAL(pos_b, hOrigin);
        h_isMoving = 0;
        l_raw = DIAL_TO_RAW(l, vOrigin);
        r_raw = DIAL_TO_RAW(r, vOrigin);
        DEBUG_PRINT2(10,"hRaw; l=%ld, r=%ld", l_raw, r_raw);
        if (hOrient == ORIENT_LEFT_RIGHT) { 
          if ((pos_a == l_raw) && (pos_b == r_raw)) PVPUT(h_BusyOff, 0);
        } else { 
          if ((pos_a == r_raw) && (pos_b == l_raw)) PVPUT(h_BusyOff, 0);
        }
      } else if (!strcmp(id, vID)){
        DEBUG_PRINT2(5,"vDONE: pos_a = %ld, pos_b = %ld", pos_a, pos_b);
        pos_ag = RAW_TO_DIAL(pos_a, vOrigin);
        pos_bg = RAW_TO_DIAL(pos_b, vOrigin);
        v_isMoving = 0;
        t_raw = DIAL_TO_RAW(t, vOrigin);
        b_raw = DIAL_TO_RAW(b, vOrigin);
        DEBUG_PRINT2(10,"vRaw; t=%ld, b=%ld", t_raw, b_raw);
        if (vOrient == ORIENT_TOP_BOTTOM) { 
          if ((pos_a == t_raw) && (pos_b == b_raw)) PVPUT(v_BusyOff, 0);
        } else { 
          if ((pos_a == b_raw) && (pos_b == t_raw)) PVPUT(v_BusyOff, 0);
        }
      }
    } state update_positions

    when ( (numWords == 5) && (!strcmp(word2, "OK")) ) { 
      /* position info received */
      sscanf (word3, "%ld", &pos_a);
      sscanf (word4, "%ld", &pos_b);
      if (error) { /* don't update unless previous error exists */
        PVPUT(error, 0);
        PVPUTSTR(errMsg, "no error");
      }
      if (!strcmp(id, hID)) {
        pos_ag = RAW_TO_DIAL(pos_a, hOrigin);
        pos_bg = RAW_TO_DIAL(pos_b, hOrigin);
      } else {
        pos_ag = RAW_TO_DIAL(pos_a, vOrigin);
        pos_bg = RAW_TO_DIAL(pos_b, vOrigin);
      }
    } state update_positions

    when () {
      /* keep us from getting stuck here */
      printf("xia_slit: did not understand reply: '%s' '%s' '%s' '%s' '%s' '%s'\n",
          word1, word2, word3, word4, word5, word6);
      epicsThreadSleep(.1);
    } state idle
  }

 /* 
 */
  state update_positions {
    when () {
      /* horizontal */
      if (!strcmp(id, hID)) { 
        if (hOrient == ORIENT_LEFT_RIGHT) { 
          PVPUT(l_RB, pos_ag);
          PVPUT(r_RB, pos_bg);
        } else { 
          PVPUT(r_RB, pos_ag);
          PVPUT(l_RB, pos_bg);
        }
        PVPUT(width_RB, l_RB+r_RB);
        PVPUT(h0_RB, (r_RB-l_RB)/2);
        DEBUG_PRINT(2,"update_positions: efSet(new_H_RB)");
        efSet (new_H_RB);	/* new readback data available */
        PVPUT(hBusy,   MOTOR_IDLE);
        /*
         * keep track of last known good values
         * to impose motor limits
         */
        l_old = l_RB;	r_old = r_RB;
        h0_old = h0_RB;	width_old = width_RB;
      }
      /* vertical */
      if (!strcmp(id, vID)) { 
        if (vOrient == ORIENT_TOP_BOTTOM) { 
          PVPUT(t_RB, pos_ag);
          PVPUT(b_RB, pos_bg);
        } else { 
          PVPUT(b_RB, pos_ag);
          PVPUT(t_RB, pos_bg);
        }
        PVPUT(height_RB, t_RB+b_RB);
        PVPUT(v0_RB, (t_RB-b_RB)/2);
        efSet (new_V_RB);	/* new readback data available */
        PVPUT(vBusy,   MOTOR_IDLE);
        /*
         * keep track of last known good values
         * to impose motor limits
         */
        t_old = t_RB;	b_old = b_RB;
        v0_old = v0_RB;	height_old = height_RB;
      }
    } state idle
  }

}

/*
#############################################################################
#  Illinois Open Source License
#  University of Illinois
#  Open Source License
#
#
#  Copyright (c) 2004,  UNICAT.  All rights reserved.
#
#
#  Developed by:
#
#  UNICAT, Advanced Photon Source, Argonne National Laboratory
#
#  Frederick Seitz Materials Research Laboratory,
#  University of Illinois at Urbana-Champaign
#
#  http://www.uni.aps.anl.gov
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the
#  "Software"), to deal with the Software without restriction, including
#  without limitation the rights to use, copy, modify, merge, publish,
#  distribute, sublicense, and/or sell copies of the Software, and to
#  permit persons to whom the Software is furnished to do so, subject to
#  the following conditions:
#
#
#  Redistributions of source code must retain the above copyright notice,
#  this list of conditions and the following disclaimers.
#
#
#  Redistributions in binary form must reproduce the above copyright
#  notice, this list of conditions and the following disclaimers in the
#  documentation and/or other materials provided with the distribution.
#
#
#  Neither the names of UNICAT, Frederick Seitz Materials Research
#  Laboratory, University of Illinois at Urbana-Champaign,
#  nor the names of its contributors may be used to endorse or promote
#  products derived from this Software without specific prior written
#  permission.
#
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
#  IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR
#  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
#  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
#  SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
#############################################################################
 */
