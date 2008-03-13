#ifndef PIDFlowController_USER_H
#define PIDFlowController_USER_H

#include "ComediOlfactometer.h"
#include "RTLCoprocess.h"
#include "ProbeComedi.h"
#include "Settings.h"
#include <string>
#include "rtl_coprocess/PIDFCParams.h" /* for PIDFCParams */
#include <map>
#include <vector>
#include "ComediChan.h"
#include "StartStoppable.h"
#include "Controller.h"
#include "Saveable.h"
#include "RTProxy.h"
#include "DataLogableProxy.h"
#include "Calib.h"

class PIDFlowController : public FlowController, public StartStoppable, public Controller, public Saveable, public Calib, virtual public RTProxy, virtual public DataLogableProxy, public DOEnableable
{
  friend class PIDFlowSensorProxy;
  friend class PIDFlowActuatorProxy;
public:
  PIDFlowController(Settings & ini, /* may be modified by save*() methods */
                    RTLCoprocess *, 
                    const ComediChan &rd, const ComediChan &wr, 
                    const ComediChan *hard_en, const ComediChan *soft_en,
                    Component *parent, const std::string &name);
  ~PIDFlowController();
  
  bool start(); ///< from StartStoppable base
  bool stop(); ///< from StartStoppable base..

  bool isValid() const { return valid; }
  std::string errorString() const { return error_str; }

  /// from Controller inherited class, vector is: kp,ki,kd,numpts
  unsigned numControlParams() const { return 4; }
  /// from Controller inherited class
  std::vector<double> controlParams() const;

  /// from Saveable interface -- saves: control params, calib table and calib coeffs to ini file
  bool save(); 

  /// from Calib interface -- sends 4 calibration coefficients to kernel coproc
  bool setCoeffs(const Coeffs & c);

  /// from our own interface -- sets the output voltage clipping
  bool setVClip(double min, double max);
  /// from our own interface -- query the output voltage clipping
  bool getVClip(double & min, double & max);

protected:
  /// from controller
  bool controller_SetControlParams(const std::vector<double> &);

  bool getControlParams(std::vector<double> & kpkikd_out, unsigned & numIntgrlPts_out) const;
  bool setControlParams(const std::vector<double> & kpkikd, unsigned numIntgrlPts);
  /// saves the control params to the settings file
  bool saveParams();

  bool valid;
  std::string error_str;
  // in RTProxy superclass RTLCoprocess *coprocess;
  // in RTProxy superclass RTLCoprocess::Handle handle;
  PIDFCParams params;

  Settings & ini;
};

class PIDFlowSensorProxy : public ComediSensor
{
  friend class PIDFlowController;
protected:
  PIDFlowSensorProxy(PIDFlowController *parent, const std::string & name,
                     const ComediChan & aio_params);
public:
  double read();  
  double readRaw();  

  // todo:
  // bool calibrate(Sensor *ref);

private:
  PIDFlowController *pfc;
};

class PIDFlowActuatorProxy : public ComediActuator
{
  friend class PIDFlowController;
protected:
  PIDFlowActuatorProxy(PIDFlowController *parent, const std::string & name,
                       const ComediChan & aio_params);
public:
  bool write(double flow);  
  bool writeRaw(double volts); 
  double read()  { return lastWritten(); }
  double readRaw() { return lastWrittenRaw(); }
  double lastWrittenRaw() const; 
private:
  PIDFlowController *pfc;
};

#endif
