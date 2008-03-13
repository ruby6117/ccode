#ifndef GenConf_H
#define GenConf_H

#include "Common.h"

/// structure that holds some general config options 
struct GenConf 
{
  String name;
  String description;
  int connectionTimeoutSecs;
};

#endif
