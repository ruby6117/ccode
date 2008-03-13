#ifndef PID_H
#define PID_H

#include "Thread.h"
#include "Mutex.h"
#include "Deque.h"
#include "SysDep.h"
#include "Timer.h"

/** A genric PID controller that has a callback facility for
    application-specific usage.

    A generic PID controller that implementes PID control using
    parameters for the Ki, Kd, and Kp, as well as externally supplied
    'e' for error magnitude.  The PID controller doesn't know anything
    about what it is controlling.  It will periodically call its
    'GetECallback' to obtain a value of 'e' (a double), the error
    magnitude.  It will then compute the derivative and integral of
    'e' over time, use these in the formula below,  and spit out a 'u' value 
    which gets given to 'SetUCallback'.  Thus these two callbacks should 
    implement actual hardware reading/writing.

    The formula for PID control is:

    u = Kp * e + Ki * (integral of e) + Kd * (derivative of e)

    Where Kp, Ki, and Kd are parameters to the algorithm affecting its
    behavior for a particular system, e is the error magnitude (the amount
    of deviation from the desired value of the system) and u is the output 
    value of the controller to be applied to the system. 

    This controller uses callbacks to be application-neutral. The actual 
    control routine runs inside a separate thread and calls your 
    callbacks from there.  If you are using this library in an RTOS, it 
    will call your callback in RTOS context. */            
class PID : protected Thread
{
 public:
 
  PID();
  virtual ~PID();

  /** Client code can use (or inherit from) this to tell class PID
      what the callbacks are.

      Callbacks are called when trying to obtain the 'e' variable from the client code or to give the client code the 'u' controlling variable. */
  struct Callback {
    /** Make compiler warnings shut up */
    virtual ~Callback() {}

    /** this is called *by* class PID in its periodic loop to obtain 
        the current error magnitude, 'e', from your client code. */
    virtual double getE() = 0;
    /** this is called *by* class PID in its periodic loop to give external
        client code the new updated 'u' value, which tells client code
        what the output value should be to the control loop (or the output 
        delta, depending on usage). */
    virtual void setU(double u) = 0;
  };

  /// Used to communicate control parameters from client code the PID class.
  struct ControlParams 
  {
      /// NB don't init these in nonrt kernel to avoid FPU problems
      double Kp, Ki, Kd;
      unsigned numIntgrlPts; ///< limits of integral control state -- how many points of the error function to use for integration

      /// Needs to be here to prevent kernel FPE
      ControlParams()  : numIntgrlPts(0) { Clr(Kp), Clr(Ki), Clr(Kd); }
      /// Needs to be here to prevent kernel FPE
      ControlParams(const ControlParams &other) { *this = other; }
      /// Needs to be here to prevent kernel FPE
      ControlParams &operator=(const ControlParams &o) { Cpy(*this, o); return *this; }
  };

  static unsigned MAX_RATE;

  /// returns the current PID loop rate, in Hz
  unsigned rate() const;
  /// set the PID loop rate, in Hz.  If rate is 0 or too high (above MAX_RATE)
  /// false is returned.  Note if true is returned the new rate will take
  /// effect immediately!
  bool setRate(unsigned rate_Hz);
  
  /// get current control params
  ControlParams controlParams() const;
  /// set current control params -- they take effect right away!
  void setControlParams(const ControlParams &);
  

  /** Starts the PID controller, which loops at the sampling rate returned by
      rate() in Hz (so, to affect the loop period, call setRate()).
      Actually creates a new thread in the current environment, 
      so callbacks are called from within the context of that thread 
      (if in an RTOS, it is realtime). 
      @param cb the callback struct
      @param rateHz the period rate to use in hz, or 0 to use current rate.
      @param cp the control parameters to use, or NULL to use current. */
  void start(Callback *cb, unsigned rateHz = 0, const ControlParams * cp = 0, Thread::Priority = Thread::NormalPriority);
  /// kills the PWM thread, stopping PWM.
  void stop();

  /// just calls Thead::running()
  bool running() const { return Thread::running(); }

protected:
  /// from parent Thread class, implements the thread
  void run();

private:
  Callback *cb;
  mutable Mutex mut;
  volatile unsigned rateHz;
  volatile ControlParams cp;
  volatile bool pleaseStop;
  Timer timer;
  unsigned currRate;
  double period;
  void recomputePeriod(); ///< this is an rt function called inside of run()

  /// PID control state
  struct State { 
    double d, i;
    bool d_undef;
    Deque<double> pts; ///< keeps track of previous point values for integration
    unsigned num_pts; ///< the number of points in the deque pts
    State() { reset(); }
    void reset() { 
      Clr(d); 
      Clr(i); 
      d_undef = true; 
      num_pts = 0; 
      pts.clear(); 
    }
  }; 
  State state;
  double doPID(double e, double timestep_millis); ///< return u based on e, cp, etc

};

#endif
