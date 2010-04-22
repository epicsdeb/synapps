/*
 * I simulate the control of an ADSC detector via the detcon_lib_th detector
 * control interface.
 *
 * I am not thread-safe (nor is the real ADSC implementation).  The caller is
 * responsible for synchronizing access to this interface if more than one
 * thread wants to use it.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <detcon_entry.h>
#include <detcon_par.h>
#include <detcon_state.h>
#include "simadsc.h"


#define SIMADSC_NAME "simadsc"
#define THIS_FILE_NAME "simadsc.c"
#define XRAY_IMAGE_ENV_VAR_NAME "SIMADSC_XRAY_IMAGE"
#define IO_BUFFER_SIZE 8192

static double SimadscReadoutTimes[][3] = {
 /* 1,   2hw,  2sw (1x1, 2x2 hardware, 2x2 software) in seconds */
  { 6.0, 2.0,  6.0 }, /* Q4 (guessed) */
  { 3.0, 1.0,  3.0 }, /* Q4r          */
  { 1.1, 0.42, 1.1 }, /* Q210         */
  { 0.9, 0.25, 0.9 }, /* Q210r        */
  { 1.1, 0.37, 1.1 }, /* Q270         */
  { 1.1, 0.42, 1.1 }, /* Q315         */
  { 0.9, 0.25, 0.9 }  /* Q315r        */
};

static SimadscModel_t Model = SimadscQ210r;
static int Initialized = 0;
static int Initializing = 0;
static int State = DTC_STATE_ERROR;
static char *Status = "";
static char *LastError = "";
static int StopExposureRetryCount = 0;
static struct timeval StartInitializingTime;
static struct timeval StartReadingTime;

static int HwpBin;
static int HwpAdc;
static int HwpSaveRaw;
static int HwpNoXform;
static int HwpStoredDark;

static float FlpPhi;
static float FlpOmega;
static float FlpKappa;
static float FlpTwoTheta;
static float FlpDistance;
static float FlpWavelength;
static int   FlpAxis;
static float FlpOscRange;
static float FlpTime;
static float FlpBeamX;
static float FlpBeamY;
static int   FlpKind;
static char  FlpFileName[PARAMETER_STRING_SIZE];
static char  FlpComment[PARAMETER_STRING_SIZE];
static int   FlpLastImage;
static char  FlpSuffix[PARAMETER_STRING_SIZE];

static double computeTimeDifference(struct timeval *time1,
  struct timeval *time0);
static int copyFile(const char *srcFileName, const char *destFileName);
static SimadscBinMode_t getBinMode(void);
static void initialize(void);
static int shouldInitializeHaveFinishedByNow(void);
static int shouldReadoutHaveFinishedByNow(void);
static int writeSimulatedImageToDiskIfAvailable(void);


int CCDStartExposure() {
  if (!Initialized) {
    State = DTC_STATE_ERROR;
    Status = "detcon_lib NOT initialized";
    LastError = Status;
    return 1;
  }

  if (State != DTC_STATE_IDLE) {
    State = DTC_STATE_ERROR;
    Status = "state should be IDLE before starting exposure";
    LastError = Status;
    return 1;
  }

  State = DTC_STATE_EXPOSING;
  Status = "";
  return 0;
}

int CCDStopExposure() {
  int retval;

  if (!Initialized) {
    State = DTC_STATE_ERROR;
    Status = "detcon_lib NOT initialized";
    LastError = Status;
    return 1;
  }

  if (State != DTC_STATE_EXPOSING) {
    State = DTC_STATE_ERROR;
    Status = "state should be EXPOSING before stopping exposure";
    LastError = Status;
    return 1;
  }

  if (StopExposureRetryCount > 0) {
    State = DTC_STATE_RETRY;
    Status = "";
    StopExposureRetryCount--;
  } else {
    State = DTC_STATE_READING;
    Status = "";
    retval = gettimeofday(&StartReadingTime, 0);
    assert(retval == 0);
  }

  return 0;
}

char *CCDStatus() {
  return Status;
}

int CCDState() {
  if (State == DTC_STATE_READING && shouldReadoutHaveFinishedByNow()) {
    State = DTC_STATE_IDLE;
    Status = "";
  } else if (Initializing && shouldInitializeHaveFinishedByNow()) {
    State = DTC_STATE_IDLE;
    Status = "";
    Initializing = 0;
    Initialized = 1;
  }
  return State;
}

int CCDSetFilePar(int which_par, const char *p_value) {
  int ivalue;

  if (which_par == FLP_PHI) FlpPhi = *((float *)p_value);
  else if (which_par == FLP_OMEGA) FlpOmega = *((float *)p_value);
  else if (which_par == FLP_KAPPA) FlpKappa = *((float *)p_value);
  else if (which_par == FLP_TWOTHETA) FlpTwoTheta = *((float *)p_value);
  else if (which_par == FLP_DISTANCE) FlpDistance = *((float *)p_value);
  else if (which_par == FLP_WAVELENGTH) FlpWavelength = *((float *)p_value);
  else if (which_par == FLP_AXIS) {
    ivalue = *((int *)p_value);
    if (ivalue != 0 && ivalue != 1) return 1;
    FlpAxis = ivalue;
  }
  else if (which_par == FLP_OSC_RANGE) FlpOscRange = *((float *)p_value);
  else if (which_par == FLP_TIME) FlpTime = *((float *)p_value);
  else if (which_par == FLP_BEAM_X) FlpBeamX = *((float *)p_value);
  else if (which_par == FLP_BEAM_Y) FlpBeamY = *((float *)p_value);
  else if (which_par == FLP_KIND) {
    ivalue = *((int *)p_value);
    if (ivalue < 0 || ivalue > 5) return 1;
    FlpKind = ivalue;
  }
  else if (which_par == FLP_FILENAME) {
    if (p_value == 0) {
      FlpFileName[0] = '\0';
    } else {
      strncpy(FlpFileName, p_value, PARAMETER_STRING_SIZE - 1);
      FlpFileName[PARAMETER_STRING_SIZE - 1] = '\0';
    }
  }
  else if (which_par == FLP_COMMENT) {
    if (p_value == 0) {
      FlpComment[0] = '\0';
    } else {
      strncpy(FlpComment, p_value, PARAMETER_STRING_SIZE - 1);
      FlpComment[PARAMETER_STRING_SIZE - 1] = '\0';
    }
  }
  else if (which_par == FLP_LASTIMAGE) {
    ivalue = *((int *)p_value);
    if (ivalue < -1 || ivalue > 1) return 1;
    FlpLastImage = ivalue;
  }
  else if (which_par == FLP_SUFFIX) {
    if (p_value == 0) {
      FlpSuffix[0] = '\0';
    } else {
      strncpy(FlpSuffix, p_value, PARAMETER_STRING_SIZE - 1);
      FlpSuffix[PARAMETER_STRING_SIZE - 1] = '\0';
    }
  }
  else return 1;
  return 0;
}

int CCDGetFilePar(int which_par, const char *p_value) {
  /* WARNING: Size of *p_value is not specified so no good buffer overflow
   * protection for string value */
  if (which_par == FLP_PHI) *((float *)p_value) = FlpPhi;
  else if (which_par == FLP_OMEGA) *((float *)p_value) = FlpOmega;
  else if (which_par == FLP_KAPPA) *((float *)p_value) = FlpKappa;
  else if (which_par == FLP_TWOTHETA) *((float *)p_value) = FlpTwoTheta;
  else if (which_par == FLP_DISTANCE) *((float *)p_value) = FlpDistance;
  else if (which_par == FLP_WAVELENGTH) *((float *)p_value) = FlpWavelength;
  else if (which_par == FLP_AXIS) *((int *)p_value) = FlpAxis;
  else if (which_par == FLP_OSC_RANGE) *((float *)p_value) = FlpOscRange;
  else if (which_par == FLP_TIME) *((float *)p_value) = FlpTime;
  else if (which_par == FLP_BEAM_X) *((float *)p_value) = FlpBeamX;
  else if (which_par == FLP_BEAM_Y) *((float *)p_value) = FlpBeamY;
  else if (which_par == FLP_KIND) *((int *)p_value) = FlpKind;
  else if (which_par == FLP_FILENAME) strcpy((char *)p_value, FlpFileName);
  else if (which_par == FLP_COMMENT) strcpy((char *)p_value, FlpComment);
  else if (which_par == FLP_LASTIMAGE) *((int *)p_value) = FlpLastImage;
  else if (which_par == FLP_SUFFIX) strcpy((char *)p_value, FlpSuffix);
  else return 1;
  return 0;
}

int CCDSetHwPar(int which_par, const char *p_value) {
  int ivalue;

  if (which_par == HWP_BIN) {
    ivalue = *((int *)p_value);
    if (ivalue != 1 && ivalue != 2) return 1;
    HwpBin = ivalue;
  }
  else if (which_par == HWP_ADC) {
    ivalue = *((int *)p_value);
    if (ivalue != 0 && ivalue != 1) return 1;
    HwpAdc = ivalue;
  }
  else if (which_par == HWP_SAVE_RAW) {
    ivalue = *((int *)p_value);
    if (ivalue != 0 && ivalue != 1) return 1;
    HwpSaveRaw = ivalue;
  }
  else if (which_par == HWP_NO_XFORM) {
    ivalue = *((int *)p_value);
    if (ivalue != 0 && ivalue != 1) return 1;
    HwpNoXform = ivalue;
  }
  else if (which_par == HWP_STORED_DARK) {
    ivalue = *((int *)p_value);
    if (ivalue != 0 && ivalue != 1) return 1;
    HwpStoredDark = ivalue;
  }
  else return 1;
  return 0;
}

int CCDGetHwPar(int which_par, const char *p_value) {
  if (which_par == HWP_BIN) *((int *)p_value) = HwpBin;
  else if (which_par == HWP_ADC) *((int *)p_value) = HwpAdc;
  else if (which_par == HWP_SAVE_RAW) *((int *)p_value) = HwpSaveRaw;
  else if (which_par == HWP_NO_XFORM) *((int *)p_value) = HwpNoXform;
  else if (which_par == HWP_STORED_DARK) *((int *)p_value) = HwpStoredDark;
  else return 1;
  return 0;
}

int CCDSetBin(int val) {
  /* real ADSC implementation does nothing */
  return 0;
}

int CCDGetBin() {
  /* real ADSC implementation does nothing */
  return 0;
}

int CCDGetImage() {
  if (!Initialized) {
    State = DTC_STATE_ERROR;
    Status = "detcon_lib NOT initialized";
    LastError = Status;
    return 1;
  }

  return CCDWriteImage();
}

int CCDCorrectImage() {
  /* real ADSC implementation does nothing */
  return 0;
}

int CCDWriteImage() {
  if (!Initialized) {
    State = DTC_STATE_ERROR;
    Status = "detcon_lib NOT initialized";
    LastError = Status;
    return 1;
  }

  /* real ADSC implementation ignores return value so ignore it here too;
   * real ADSC implementation does not wait for image to be written to disk --
   * in this way the simulation differs (i.e. it *does* wait for the image to
   * be written to disk) to allow the simulation to remain very simple */
  writeSimulatedImageToDiskIfAvailable();

  return 0;
}

char *CCDGetLastError() {
  return LastError;
}

int CCDAbort() {
  if (Initializing) return 1;
  State = DTC_STATE_IDLE;
  Status = "";
  return 0;
}

int CCDReset() {
  State = DTC_STATE_IDLE;
  Status = "";
  LastError = "";
  return 0;
}

int CCDInitialize() {
  int retval;

  initialize();
  retval = gettimeofday(&StartInitializingTime, 0);
  assert(retval == 0);
  Initializing = 1;
  return 0;
}


static SimadscBinMode_t getBinMode(void) {
  if (HwpBin == 1) return SimadscBin1x1;
  if (HwpAdc == 0) return SimadscBinSw2x2;
  if (HwpAdc == 1) return SimadscBinHw2x2;
  assert(0);
}

static double computeTimeDifference(struct timeval *time1,
    struct timeval *time0) {
  return (double)(time1->tv_sec - time0->tv_sec) +
    ((double)(time1->tv_usec - time0->tv_usec) / 1000000.0);
}

static int copyFile(const char *srcFileName, const char *destFileName) {
  FILE *srcFile;
  FILE *destFile;
  int status;
  char *buffer[IO_BUFFER_SIZE];
  size_t numRead;
  size_t numWritten;
  int hadError;

  srcFile = fopen(srcFileName, "rb");
  if (srcFile == NULL) {
    fprintf(stderr, "%s:%s:%d:copyFile(): Failed to open source file \"%s\"; "
      "%s\n", SIMADSC_NAME, THIS_FILE_NAME, __LINE__, srcFileName,
      strerror(errno));
    return 1;
  }

  destFile = fopen(destFileName, "wb");
  if (destFile == NULL) {
    fprintf(stderr, "%s:%s:%d:copyFile(): Failed to open destination file "
      "\"%s\"; %s\n", SIMADSC_NAME, THIS_FILE_NAME, __LINE__, destFileName,
      strerror(errno));
    status = fclose(srcFile);
    if (status != 0)
      fprintf(stderr, "%s:%s:%d:copyFile(): Failed to close source file "
        "\"%s\"; %s\n", SIMADSC_NAME, THIS_FILE_NAME, __LINE__, srcFileName,
        strerror(errno));
    return 1;
  }

  hadError = 0;
  for (;;) {
    numRead = fread(buffer, sizeof(buffer[0]), IO_BUFFER_SIZE, srcFile);
    if (numRead == 0) {
      if (ferror(srcFile)) {
        fprintf(stderr, "%s:%s:%d:copyFile(): Failed to read source file "
          "\"%s\"\n", SIMADSC_NAME, THIS_FILE_NAME, __LINE__, srcFileName);
        hadError = 1;
      }
      break;
    }
    numWritten = fwrite(buffer, sizeof(buffer[0]), numRead, destFile);
    if (numWritten < numRead) {
      fprintf(stderr, "%s:%s:%d:copyFile(): Failed to write destination file "
        "\"%s\"\n", SIMADSC_NAME, THIS_FILE_NAME, __LINE__, destFileName);
      hadError = 1;
      break;
    }
  }

  status = fclose(destFile);
  if (status != 0) {
    fprintf(stderr, "%s:%s:%d:copyFile(): Failed to close destination file "
      "\"%s\"; %s\n", SIMADSC_NAME, THIS_FILE_NAME, __LINE__, destFileName,
      strerror(errno));
    hadError = 1;
  }

  status = fclose(srcFile);
  if (status != 0) {
    fprintf(stderr, "%s:%s:%d:copyFile(): Failed to close source file \"%s\" "
      "; %s\n", SIMADSC_NAME, THIS_FILE_NAME, __LINE__, srcFileName,
      strerror(errno));
    hadError = 1;
  }

  return hadError ? 1 : 0;
}

static void initialize(void) {
  Model = SimadscQ210r;
  Initialized = 0;
  Initializing = 0;
  State = DTC_STATE_ERROR;
  StopExposureRetryCount = 0;
  Status = "";
  LastError = "";
  StartInitializingTime.tv_sec = 0;
  StartInitializingTime.tv_usec = 0;
  StartReadingTime.tv_sec = 0;
  StartReadingTime.tv_usec = 0;

  HwpBin = 1;
  HwpAdc = 0;
  HwpSaveRaw = 0;
  HwpNoXform = 0;
  HwpStoredDark = 1;

  FlpPhi = 0.0;
  FlpOmega = 0.0;
  FlpKappa = 0.0;
  FlpTwoTheta = 0.0;
  FlpDistance = 0.0;
  FlpWavelength = 0.0;
  FlpAxis = 0;
  FlpOscRange = 0.0;
  FlpTime = 0.0;
  FlpBeamX = 0.0;
  FlpBeamY = 0.0;
  FlpKind = 0;
  FlpFileName[0] = '\0';
  FlpComment[0] = '\0';
  FlpLastImage = 0;
  strncpy(FlpSuffix, ".img", PARAMETER_STRING_SIZE - 1);
  FlpSuffix[PARAMETER_STRING_SIZE - 1] = '\0';
}

static int shouldInitializeHaveFinishedByNow(void) {
  struct timeval timeNow;
  int retval;

  retval = gettimeofday(&timeNow, 0);
  assert(retval == 0);
  return computeTimeDifference(&timeNow, &StartInitializingTime) >
    INITIALIZE_TIME;
}

static int shouldReadoutHaveFinishedByNow(void) {
  SimadscBinMode_t binMode;
  struct timeval timeNow;
  int status;

  binMode = getBinMode();
  status = gettimeofday(&timeNow, 0);
  assert(status == 0);
  return computeTimeDifference(&timeNow, &StartReadingTime) >
    SimadscReadoutTimes[Model][binMode];
}

static int writeSimulatedImageToDiskIfAvailable(void) {
  char *simulatedImageFileName;

  if (!(FlpKind == 4 || FlpKind == 5)) return 0;
  simulatedImageFileName = getenv(XRAY_IMAGE_ENV_VAR_NAME);
  if (simulatedImageFileName == NULL) return 0;
  if (strlen(simulatedImageFileName) == 0) return 0;

  return copyFile(simulatedImageFileName, FlpFileName);
}


void simadscMakeInitialized(void) {
  initialize();
  Initialized = 1;
  State = DTC_STATE_IDLE;
  StopExposureRetryCount = 0;
}

void simadscMakeUninitialized(void) {
  Initialized = 0;
  Initializing = 0;
  State = DTC_STATE_ERROR;
  Status = "";
  LastError = Status;
}

void simadscReset(void) {
  initialize();
}

void simadscSetModel(SimadscModel_t model) {
  Model = model;
}

void simadscSetStopExposureRetryCount(int count) {
  StopExposureRetryCount = count < 0 ? 0 : count;
}
