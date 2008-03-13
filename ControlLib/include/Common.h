#ifndef Common_H
#define Common_H

#include "SysDep.h"

#ifdef __KERNEL__

#  ifdef __cplusplus
// operator new and delete implemented in Common.cpp
extern void *operator new(unsigned int size);
extern void operator delete(void *p);
extern void *operator new[](unsigned int size);
extern void operator delete[](void *p);

#   endif /* __cplusplus */

#endif /* ___KERNEL__ */

#endif


