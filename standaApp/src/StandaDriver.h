/*
FILENAME...   StandaDriver.h
USAGE...      Motor driver support for the Standa 8SMC5 controller.

*/

#include "asynMotorController.h"
#include "asynMotorAxis.h"

#include <ximc.h>

// No controller-specific parameters yet
#define NUM_STANDA_PARAMS 0

class StandaAxis : public asynMotorAxis
{
public:
    /* These are the methods we override from the base class */
    StandaAxis(class StandaController *pC, int axis, const char* deviceName);

    void report(FILE *fp, int level);
    asynStatus move(double position, int relative, double min_velocity, double max_velocity, double acceleration);
    //asynStatus setClosedLoop(bool closedLoop);
    asynStatus home(double min_velocity, double max_velocity, double acceleration, int forwards);
    asynStatus stop(double acceleration);
    asynStatus poll(bool *moving);

private:
    StandaController *pC_;          /**< Pointer to the asynMotorController to which this axis belongs.
                                   *   Abbreviated because it is used very frequently */
    device_t device_;
    status_t status_;
    bool alarmActive_;

    friend class StandaController;
};

class StandaController : public asynMotorController
{
public:
    StandaController(const char *portName, const char *StandaPortName, int numAxes, double movingPollPeriod, double idlePollPeriod);
    ~StandaController(); // Destruktor

    /* These are the methods that we override from asynMotorDriver */
    void report(FILE *fp, int level);

    friend class StandaAxis;
};
