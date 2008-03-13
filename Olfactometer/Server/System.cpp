#include "System.h"

#ifdef LINUX

#  include <sys/types.h>
#  include <sys/socket.h>
#  include <sys/ioctl.h>
#  include <net/if.h> 
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netinet/ip.h>

StringList System::ipAddressList()
{
  StringList ret;
  int sockfd = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sockfd > -1) {
    struct ifreq ifaces[32];
    struct ifconf ifc;
    ifc.ifc_len = sizeof(ifaces);
    ifc.ifc_req = ifaces;
    int iret = ::ioctl(sockfd, SIOCGIFCONF, &ifc);
    if (iret == 0) {
      unsigned i, numifaces = ifc.ifc_len / sizeof(ifaces[0]);
      for (i = 0; i < numifaces; ++i) {
        struct sockaddr_in * addr = reinterpret_cast<struct sockaddr_in *>(&ifaces[i].ifr_addr);
        if (String(ifaces[i].ifr_name) != "lo")
          ret.push_back(inet_ntoa(addr->sin_addr));
      }
    }
    ::close(sockfd);
  }
  return ret;
}

#include <fstream>

double System::uptime()
{
  double ret = 0.0;
  std::ifstream f;
  f.open("/proc/uptime", std::ios_base::in);
  if (f.is_open()) {
    f >> ret;
    f.close();
  }
  return ret;
}

#else 
#  error System functions implemented only for LINUX target! FIXME!
#endif



