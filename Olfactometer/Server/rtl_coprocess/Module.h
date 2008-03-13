#ifndef Module_H
#define Module_H
/**
   @file Module.h
   @brief C <-> C++ interface for OlfCoprocess.o kernel module
*/
#include "SysDep.h"

#ifdef __cplusplus
extern "C" {
#endif

  extern int ModuleInit(void);
  extern void ModuleCleanup(void);

  extern void ModuleIncUseCount(void);
  extern void ModuleDecUseCount(void);

#define MODNAME "OlfCoprocess"

  extern int debug;

#define Error(x...) RTPrint(MODNAME ": *Error* " x)
#define Msg(x...) RTPrint(MODNAME ": " x)
#define Debug(x...) do { if (debug) RTPrint(MODNAME ": DEBUG " x); } while(0)
#ifdef __cplusplus
}
#endif

#endif
