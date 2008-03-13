#include "RTFifo.h"
#include "SysDep.h"

#ifndef RTF_NO
#  define RTF_NO 1024
#endif

RTFifo * RTFifo::handlerMap[RTF_NO] = { 0 };

int RTFifo::handlerWrapper(unsigned fifo)
{
  RTFifo *self = 0;
  if (fifo < RTF_NO) self = handlerMap[fifo];
  if (self) return self->handler();
  return 0;
}

int RTFifo::handlerWrapper_RT(unsigned fifo)
{
  RTFifo *self = 0;
  if (fifo < RTF_NO) self = handlerMap[fifo];
  if (self) return self->handlerRT();
  return 0;
}

RTFifo::RTFifo()
  : minor(-1), sz_bytes(-1), buddy(0)
{}

RTFifo::RTFifo(int s, int m, bool useHandler, bool useRTHandler)
  : minor(-1), sz_bytes(-1), buddy(0)
{
  if ( open(s, m) ) {
    enableHandler(useHandler);
    enableRTHandler(useRTHandler);
  }
}

RTFifo::~RTFifo()
{
  close();
}

bool RTFifo::open(int s, int m)
{
  bool ok = false;
  close();
  int descr = RTFOpen(m, s);
  if (descr > -1) {
    minor = descr;
    handlerMap[minor] = this;
    handlerMap[minor] = this;
    ok = true;
  }
  return ok;
}

void RTFifo::enableHandler(bool b)
{
  int (*func)(unsigned int) = 0;
  if (b) func = handlerWrapper;
  RTFHandler(minor, func);
}

void RTFifo::enableRTHandler(bool b)
{
  int (*func)(unsigned int) = 0;
  if (b) func = handlerWrapper_RT;
  RTFHandler_RT(minor, func);
}

void RTFifo::close()
{
  if (minor > -1) {
    disableHandler();
    disableRTHandler();
    RTFClose(minor);
    handlerMap[minor] = 0;
    minor = -1;
  }
  if (buddy)  {
    buddy->buddy = 0; // need this call to avoid recursion below..
    RTFifo *b = buddy;
    buddy = 0;
    b->close();
  }
}

int RTFifo::read(void *buf, unsigned nbytes)
{
  return RTFRead(minor, buf, nbytes);
}

int RTFifo::write(const void *buf, unsigned nbytes)
{
  return RTFWrite(minor, buf, nbytes);
}

// static
bool RTFifo::makeUserPair(RTFifo *get, RTFifo *put)
{

  if ( RTFMakeUserPair(get->minor, put->minor) ) {
    get->buddy = put;
    put->buddy = get;
    return true;
  }
  return false;
}

unsigned int RTFifo::fionread() const
{
  return RTFReadBytesAvailable(minor);
}

#ifdef __KERNEL__
bool RTFifo::waitData(int num_usecs) const
{
  (void)num_usecs;
  return fionread() != 0;
}
#else
#include <sys/select.h>
bool RTFifo::waitData(int num_usecs) const
{
  if (num_usecs == 0) return fionread() > 0;
  fd_set rfd;
  FD_ZERO(&rfd);
  FD_SET(minor, &rfd);  
  struct timeval *tvp = 0;
  if (num_usecs > -1) {
    struct timeval tv;
    tv.tv_sec = num_usecs / 1000000;
    tv.tv_usec = num_usecs % 1000000;
    tvp = &tv;
  }
  return ::select(minor, &rfd, 0, 0, tvp) > 0;
}
#endif
