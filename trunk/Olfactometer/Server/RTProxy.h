#ifndef RTProxy_H
#define RTProxy_H

#include "RTLCoprocess.h"

class RTProxy
{
protected:
  RTProxy() : coprocess(0), handle(RTLCoprocess::Failure) {}
  RTLCoprocess *coprocess;
  RTLCoprocess::Handle handle;
};

#endif
