/*
FILENAME...  AG_CONEX.h
USAGE...      Motor driver support for the Newport Agilis AG-UC series of controllers.

Mark Rivers
April 11, 2013

*/

#include "asynMotorController.h"
#include "asynMotorAxis.h"

// No controller-specific parameters yet
#define NUM_AG_CONEX_PARAMS 0  

class AG_CONEXAxis : public asynMotorAxis
{
public:
  /* These are the methods we override from the base class */
  AG_CONEXAxis(class AG_CONEXController *pC);
  void report(FILE *fp, int level);
  asynStatus move(double position, int relative, double min_velocity, double max_velocity, double acceleration);
  asynStatus moveVelocity(double min_velocity, double max_velocity, double acceleration);
  asynStatus home(double min_velocity, double max_velocity, double acceleration, int forwards);
  asynStatus stop(double acceleration);
  asynStatus poll(bool *moving);
  asynStatus setPosition(double position);
  asynStatus setClosedLoop(bool closedLoop);
  asynStatus setPGain(double pGain);
  asynStatus setIGain(double iGain);
  asynStatus setDGain(double dGain);

private:
  AG_CONEXController *pC_;        /**< Pointer to the asynMotorController to which this axis belongs.
                                   *   Abbreviated because it is used very frequently */
  asynStatus getClosedLoop(bool *closedLoop);
  double currentPosition_;
  double positionOffset_;
  double encoderIncrement_;
  double interpolationFactor_;
  double stepSize_;
  double highLimit_;
  double lowLimit_;
  double KP_;
  double KI_;
  double LF_;
  char   stageID_[40];
  
friend class AG_CONEXController;
};

class AG_CONEXController : public asynMotorController {
public:
  AG_CONEXController(const char *portName, const char *serialPortName, int controllerID, double movingPollPeriod, double idlePollPeriod);

  void report(FILE *fp, int level);
  AG_CONEXAxis* getAxis(asynUser *pasynUser);
  AG_CONEXAxis* getAxis(int axisNo);
  asynStatus writeAgilis();
  asynStatus writeAgilis(const char *output, double timeout);

private:
  int controllerID_;
  char controllerVersion_[40];

  friend class AG_CONEXAxis;
};
