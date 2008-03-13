#ifndef Cmd_H
#define Cmd_H

#include "RTFifo.h"
#include "PIDFCParams.h"
#include "PWMVParams.h"

#ifdef __KERNEL__
#include "kcomedilib.h"
#else
#include <comedilib.h>
#endif

/**
   @file Cmd.h
   @brief Remote procedure call data for us in communicating with the RTL coprocess.
*/

struct Cmd 
{
#define CMD_MAGIC (0x0f1711a1)
    Cmd() : magic1(CMD_MAGIC), 
            cmd(Undefined), 
            status(Error), 
            magic2(~CMD_MAGIC) 
  { 
      daqname_in[0] = daqname_out[0] = 0; 
      datalog.id = 0;
      datalog.mask = 0;
      daqParams.dioMode = Unspecified; 
      daqParams.use_override = 0; 
      daqGetPut.doit = false; 
  }

    int magic1; ///< a header of sorts
    
    
    enum Command {
      Undefined = 0,
      Create, ///<  issued from user -> kernel to create PWM/PID objects
      Query,  ///<  issued from user -> kernel to query existing PWM/PID object
      Destroy, ///< issued from user -> kernel to delete existing PWM/PID obj
      Start, ///< issued from user -> kernel to turn on PWM/PID
      Stop, ///< issued from user -> kernel to turn off PWM/PID
      DestroyAll, ///< issued from user -> kernel to clear/destroy ALL created objects!
      Modify, ///< apply new params
      N_Command
    };
    enum Object {   PID = N_Command, PWM, DAQ, N_Object  };
    enum Status {   Ok = N_Object, Error, N_Status };
    typedef unsigned long Handle;

    Command cmd; 
    Object  object;
    Status  status; ///< userspace reads this field in response from kernel
    Handle  handle; /**< userspace sets this field on Query/Modify/Destroy
                       or reads this field in response from kernel on create */
    char daqname_in[64]; ///< for referring to daq tasks by name in create/destroy
    char daqname_out[64]; ///< for referring to daq tasks by name in create/destroy
  
    struct PIDFCParams pidParams;
    struct PWMVParams pwmParams;

    enum DIOMode { Unspecified = 0, Input = 1, Output = 2 };
    struct {
        unsigned minor, subdev, chanmask, range, aref, rate_hz;
        int override_min, override_max, use_override;        
        DIOMode dioMode; 
    } daqParams; /// for daq Create 
    struct {
        bool doit;
        unsigned chan;
        lsampl_t sample;
    } daqGetPut; /// for daq Query or Modify which gets or puts samples
  
    struct {
      unsigned id;
      unsigned ids[sizeof(unsigned)*8];
      unsigned mask;
    } datalog;

    /// a footer of sorts
    int magic2;

  // public methods

  /// returns true iff magic1 and magic2 have the correct field values
  bool verify() const { return magic1 == CMD_MAGIC && magic2 == ~CMD_MAGIC; }

  bool writeFifo(RTFifo *put) const {
    int ret = put->write(this, sizeof(*this));
    if (ret != sizeof(*this)) return false;
    return true;
  }
  
  bool readFifo(RTFifo *get) {
    int ret = get->read(this, sizeof(*this));
    if (ret != sizeof(*this) || !verify()) return false;
    return true;
  }
  

};

#endif
