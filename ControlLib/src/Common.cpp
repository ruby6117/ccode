#include "Common.h"

void *operator new(unsigned int size) { return Alloc_mem(size); }
void operator delete(void *p) { Free_mem(p); }
void *operator new[](unsigned int size) { return Alloc_mem(size); }
void operator delete[](void *p) { Free_mem(p); }

#ifdef __KERNEL__
extern "C" void __cxa_pure_virtual(void) { RTPrint("pure virtual function called\n"); }
extern "C" void __cxa_atexit(void) {}
#endif
