/*
FILENAME... StandaDriver.cpp
USAGE...    Motor driver support for the Standa 8SMC5 controller.

*/

#include <iocsh.h>
#include <epicsExit.h>
#include <epicsExport.h>

#include "StandaDriver.h"
#include <cstring>
#include <ximc.h>

#define NINT(f) (int)((f)>0 ? (f)+0.5 : (f)-0.5)

static void standaShutdownCallback(void* p)
{
    StandaController* ctrl = (StandaController*)p;
    delete ctrl;  // calls the destructor
}

/** Creates a new StandaController object.
  * \param[in] portName          The name of the asyn port that will be created for this driver
  * \param[in] StandaPortName    The name of the drvAsynSerialPort that was created previously to connect to the Standa controller
  * \param[in] numAxes           The number of axes that this controller supports
  * \param[in] movingPollPeriod  The time between polls when any axis is moving
  * \param[in] idlePollPeriod    The time between polls when no axis is moving
  */
StandaController::StandaController(const char *portName, const char *StandaPortName, int numAxes, double movingPollPeriod, double idlePollPeriod)
    :  asynMotorController(portName, numAxes, NUM_STANDA_PARAMS,
                           0, // No additional interfaces beyond those in base class
                           0, // No additional callback interfaces beyond those in base class
                           ASYN_CANBLOCK | ASYN_MULTIDEVICE,
                           1, // autoconnect
                           0, 0)  // Default priority and stack size
{
    int axis;
    StandaAxis *pAxis;
    device_enumeration_t devenum;
    char axis_name[256];
    int axis_count;
    const int probe_flags = ENUMERATE_PROBE;
    const char* enumerate_hints = "";
    char ximc_version_str[32];
    static const char *functionName = "StandaController::StandaController";

    //	ximc_version returns library version string.
    ximc_version( ximc_version_str );
    printf( "libximc version %s\n", ximc_version_str );

    //	Device enumeration function. Returns an opaque pointer to device enumeration data.
    devenum = enumerate_devices( probe_flags, enumerate_hints );

    //	Gets device count from device enumeration data
    axis_count = get_device_count( devenum );
    printf("found standa axis: %i\n", axis_count );
    if (axis_count!=numAxes){
        printf("note: but you want to connect %i axis\n", numAxes);
    }

    //	Terminate if there are no connected devices
    if (axis_count <= 0)
    {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s: no axis found or cannot connect to Standa controller\n", functionName);
        return;
    }

    for (axis=0; axis<axis_count; axis++)
    {
        //	Copy first found device name into a string
        strcpy( axis_name, get_device_name( devenum, axis ) );
        pAxis = new StandaAxis(this, axis, axis_name);
    }

    //	Free memory used by device enumeration data
    free_enumerate_devices( devenum );

    startPoller(movingPollPeriod, idlePollPeriod, 2);

    epicsAtExit(&standaShutdownCallback, this);
}

/** Creates a new StandaController object.
  * Configuration command, called directly or from iocsh
  * \param[in] portName          The name of the asyn port that will be created for this driver
  * \param[in] StandaPortName    The name of the drvSerialPort that was created previously to connect to the Standa controller
  * \param[in] numAxes           The number of axes that this controller supports
  * \param[in] movingPollPeriod  The time in ms between polls when any axis is moving
  * \param[in] idlePollPeriod    The time in ms between polls when no axis is moving
  */
extern "C" int StandaCreateController(const char *portName, const char *StandaPortName, int numAxes, int movingPollPeriod, int idlePollPeriod)
{
    StandaController *pStandaController
        = new StandaController(portName, StandaPortName, numAxes, movingPollPeriod/1000., idlePollPeriod/1000.);
    pStandaController = NULL;
    return(asynSuccess);
}

/* Destroys the StandaController object. */
StandaController::~StandaController()
{
    printf("Shutting down Standa controller...\n");

    for (int i = 0; i < numAxes_; i++) {
        StandaAxis *axis = static_cast<StandaAxis*>(getAxis(i));

        if (axis && axis->device_) {
            printf("  Axis %d: closing device...\n", i);
            //command_power_off(axis->device_);
            close_device(&axis->device_);
        }
    }
    printf("Standa controller shut down complete.\n");
}

/** Reports on status of the driver
  * \param[in] fp The file pointer on which report information will be written
  * \param[in] level The level of report detail desired
  *
  * If details > 0 then information is printed about each axis.
  * After printing controller-specific information it calls asynMotorController::report()
  */
void StandaController::report(FILE *fp, int level)
{
    fprintf(fp, "Standa motor driver\n");
    fprintf(fp, "  port name=%s\n", this->portName);
    fprintf(fp, "  moving poll period=%f\n", movingPollPeriod_);
    fprintf(fp, "  idle poll period=%f\n", idlePollPeriod_);

    // Call the base class method
    asynMotorController::report(fp, level);
}

// These are the StandaAxis methods

/** Creates a new StandaAxis object.
  * \param[in] pC Pointer to the StandaController to which this axis belongs.
  * \param[in] axisNo Index number of this axis, range 0 to pC->numAxes_-1.
  * Initializes register numbers, etc.
  */
StandaAxis::StandaAxis(StandaController *pC, int axisNo, const char* deviceName)
    : asynMotorAxis(pC, axisNo),
    pC_(pC),
    device_(0),
    alarmActive_(false)
{
    static const char *functionName = "StandaAxis::StandaAxis";

    //printf("Opening device %s for axis %d\n", deviceName, axisNo);
    device_ = open_device(deviceName);

    if (device_ == 0) {
        asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR, "%s: axis %d: cannot open device %s\n",
                  functionName, axisNo_, deviceName);
        setIntegerParam(pC_->motorStatusProblem_, 1);
        return;
    }

    callParamCallbacks();
}

/** Reports on status of the axis
  * \param[in] fp The file pointer on which report information will be written
  * \param[in] level The level of report detail desired
  *
  * After printing device-specific information calls asynMotorAxis::report()
  */
void StandaAxis::report(FILE *fp, int level)
{
    if (level > 0)
    {
        fprintf(fp, "  axis %d\n", axisNo_);
    }

    // Call the base class method
    asynMotorAxis::report(fp, level);
}

asynStatus StandaAxis::move(double position, int relative, double minVelocity, double maxVelocity, double acceleration)
{
    static const char *functionName = "StandaAxis::move";
    result_t result;
    move_settings_t ms;
    //double mres;

    //printf("minV=%.2f, maxV=%.2f, acc=%.2f\n", minVelocity, maxVelocity, acceleration);

    // 1. get the move-settings
    result = get_move_settings(device_, &ms);
    if (result != result_ok) {
        printf("ERROR: get_move_settings failed\n");
        return asynError;
    }
    //printf("Current settings: Speed=%u, Accel=%u, Decel=%u\n", ms.Speed, ms.Accel, ms.Decel);
    //pC_->getDoubleParam(pC_->motorResolution_, &mres);
    //printf("MRES = %.9g mm/step\n", mres);


    // 2. set the move-settings
    // factor 10k for velo is ok but there must be a better way to calc the velo...
    ms.Speed = NINT(maxVelocity / 10000.0);
    //ms.Accel = (unsigned int)acceleration;
    //ms.Decel = (unsigned int)acceleration;

    result = set_move_settings(device_, &ms);
    if (result != result_ok) {
        printf("ERROR: set_move_settings failed\n");
        return asynError;
    }

    // 3. move
    if (relative) {
        result = command_movr(device_, (unsigned int)relative, 0);
    } else {
        result = command_move(device_, (int)position, 0);
    }

    if (result != result_ok) {
        asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR, "%s: axis %d: failed, result=%d\n",
                  functionName, axisNo_, result);
        return asynError;
    }
    return asynSuccess;
}

/*asynStatus StandaAxis::setClosedLoop(bool closedLoop)
{
    static const char *functionName = "StandaAxis::setClosedLoop";
    result_t result;

    // enable/disable closed-loop control
    if (closedLoop) {
        result = command_movr(device_, 0, 0);
    } else {
        result = command_power_off (device_);
    }

    if (result != result_ok) {
        asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR, "%s: axis %d: failed, result=%d\n",
                  functionName, axisNo_, result);
        return asynError;
    }
    return asynSuccess;
}*/

asynStatus StandaAxis::home(double minVelocity, double maxVelocity, double acceleration, int forwards)
{
    static const char *functionName = "StandaAxis::home";
    result_t result;

    // there seems to be only one direction for homing
    result = command_home(device_);

    if (result != result_ok) {
        asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR, "%s: axis %d: failed, result=%d\n",
                  functionName, axisNo_, result);
        return asynError;
    }
    return asynSuccess;
}

asynStatus StandaAxis::stop(double acceleration)
{
    static const char *functionName = "StandaAxis::stop";
    result_t result;

    // Immediately stops the engine, moves it to the STOP state, and sets switches to BREAK mode (windings are short-circuited)
    if (alarmActive_) {
        result = command_stop(device_);
    } else {
    // Soft stop the engine. The motor is slowing down with the deceleration specified in move_settings.
        result = command_sstp(device_);
    }

    if (result != result_ok) {
        asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR, "%s: axis %d: failed, result=%d\n",
                  functionName, axisNo_, result);
        return asynError;
    }
    return asynSuccess;
}

/** Polls the axis.
  * This function reads the motor position and the moving status.
  * It calls setIntegerParam() and setDoubleParam() for each item that it polls,
  * and then calls callParamCallbacks() at the end.
  * \param[out] moving A flag that is set indicating that the axis is moving (true) or done (false). */
asynStatus StandaAxis::poll(bool *moving)
{
    result_t result;
    int done = 0;
    int limit = 0;
    bool isMoving = false;

    // get the Device state
    result = get_status( device_, &status_ );
    if (result != result_ok) goto skip;

    if (status_.Flags & STATE_ALARM) {
        setIntegerParam(pC_->motorStatusProblem_, 1);
        alarmActive_ = true;
    } else {
        setIntegerParam(pC_->motorStatusProblem_, 0);
        alarmActive_ = false;
    }
    //epicsStdoutPrintf("Status Flags = 0x%08X\n", status_.Flags);

    // get the position
    //printf( "position %d, encoder %lld, speed %d, bitmask %i\n", status_.CurPosition, status_.EncPosition, status_.CurSpeed, status_.MvCmdSts);
    setDoubleParam(pC_->motorPosition_, status_.CurPosition);
    isMoving = (status_.MvCmdSts & MVCMD_RUNNING) != 0;
    done = isMoving ? 0 : 1;


    // Read the limit status
    if (status_.GPIOFlags & STATE_RIGHT_EDGE) {
        setIntegerParam(pC_->motorStatusHighLimit_, 1);
    } else {
        setIntegerParam(pC_->motorStatusHighLimit_, 0);
    }
    if (status_.GPIOFlags & STATE_LEFT_EDGE) {
        setIntegerParam(pC_->motorStatusLowLimit_, 1);
    } else {
        setIntegerParam(pC_->motorStatusLowLimit_, 0);
    }

    /*if (status_.PWRSts & PWR_STATE_OFF) {
        // Allow CNEN to turn motor power on/off
        setIntegerParam(pC_->motorStatusGainSupport_, 0);
        printf("power off\n");
    } else {
        setIntegerParam(pC_->motorStatusGainSupport_, 1);
        printf("power on\n");
    }*/

    // set moving state
    setIntegerParam(pC_->motorStatusDone_, done);
    setIntegerParam(pC_->motorStatusMoving_, !done);
    *moving = done ? false:true;

skip:
    callParamCallbacks();
    if (result != result_ok) {
        asynPrint(pC_->pasynUserSelf, ASYN_TRACE_ERROR, "poll: axis %d: failed, result=%d\n", axisNo_, result);
        return asynError;
    }
    return asynSuccess;
}

/** Code for iocsh registration */
static const iocshArg StandaCreateControllerArg0 = {"Port name", iocshArgString};
static const iocshArg StandaCreateControllerArg1 = {"Standa port name", iocshArgString};
static const iocshArg StandaCreateControllerArg2 = {"Number of axes", iocshArgInt};
static const iocshArg StandaCreateControllerArg3 = {"Moving poll period (ms)", iocshArgInt};
static const iocshArg StandaCreateControllerArg4 = {"Idle poll period (ms)", iocshArgInt};
static const iocshArg * const StandaCreateControllerArgs[] = {&StandaCreateControllerArg0,
                                                            &StandaCreateControllerArg1,
                                                            &StandaCreateControllerArg2,
                                                            &StandaCreateControllerArg3,
                                                            &StandaCreateControllerArg4
                                                           };
static const iocshFuncDef StandaCreateControllerDef = {"StandaCreateController", 5, StandaCreateControllerArgs};
static void StandaCreateContollerCallFunc(const iocshArgBuf *args)
{
    StandaCreateController(args[0].sval, args[1].sval, args[2].ival, args[3].ival, args[4].ival);
}

static void StandaRegister(void)
{
    iocshRegister(&StandaCreateControllerDef, StandaCreateContollerCallFunc);
}

extern "C" {
    epicsExportRegistrar(StandaRegister);
}
