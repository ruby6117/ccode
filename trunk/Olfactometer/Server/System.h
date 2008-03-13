#ifndef System_H
#define System_H

#include "Common.h"

/// miscellaneous system-related functions
class System
{
public:
  /// returns the ip addresses (in dot notation) for all interfaces on the system (except the loopback interface)  
  static StringList ipAddressList();
  /// returns the system uptime in seconds
  static double uptime();
};
#endif
