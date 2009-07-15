#ifndef SIMADSC_H
#define SIMADSC_H

#ifdef __cplusplus
extern "C" {
#endif

#define INITIALIZE_TIME 2.0
#define PARAMETER_STRING_SIZE 256

typedef enum {
  SimadscQ4,
  SimadscQ4r,
  SimadscQ210,
  SimadscQ210r,
  SimadscQ270,
  SimadscQ315,
  SimadscQ315r
} SimadscModel_t;

typedef enum {
  SimadscBin1x1,
  SimadscBinHw2x2,
  SimadscBinSw2x2
} SimadscBinMode_t;

extern void simadscMakeInitialized(void);
extern void simadscMakeUninitialized(void);
extern void simadscReset(void);
extern void simadscSetModel(SimadscModel_t model);
extern void simadscSetStopExposureRetryCount(int count);

#ifdef __cplusplus
}
#endif

#endif
