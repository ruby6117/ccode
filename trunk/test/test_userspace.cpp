#include "PWM.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <time.h>
#include <unistd.h>
#include <signal.h>

struct CB : public PWM::Callback
{
  CB(const std::string & msg): msg(msg) {
    struct timespec ts;
    ::clock_gettime(CLOCK_REALTIME, &ts);    
    t0 = ts.tv_sec + static_cast<double>(ts.tv_nsec)/1000000000.0;        
  }
  void operator()() {
    struct timespec ts;
    ::clock_gettime(CLOCK_REALTIME, &ts);    
    double t = ts.tv_sec + static_cast<double>(ts.tv_nsec)/1000000000.0;    
    t -= t0;
    std::cout << msg << " " << std::setprecision(12) << t << std::endl;
  }
  std::string msg;
  double t0;
};

void sighand(int d)
{
  (void)d;
  std::cout << "SIGINT caught.." << std::endl;
}

int main(void)
{
  CB h ("high"), l ("low");
  PWM p;

  p.setTimeScale(PWM::Milliseconds);
  p.setWindow(1000);
  p.setDutyCycle(10);
  p.start(&h, &l, 5);
  ::signal(SIGINT, sighand);
  pause();
  p.stop();
  std::cout << "Exiting..." << std::endl;
  ::sleep(1);
  return 0;
}
