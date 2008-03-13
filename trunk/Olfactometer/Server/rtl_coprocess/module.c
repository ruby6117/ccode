#include <linux/module.h>
#include <linux/string.h>
#include <asm/bitops.h>
#include "Module.h"

int init(void) {  return ModuleInit(); }
void cleanup(void) {  ModuleCleanup(); }

void ModuleIncUseCount(void) { MOD_INC_USE_COUNT; }
void ModuleDecUseCount(void) { MOD_DEC_USE_COUNT; }

module_init(init);
module_exit(cleanup);

#ifdef MODULE_LICENSE
  MODULE_LICENSE("GPL");
#endif

MODULE_AUTHOR("Calin A. Culianu <calin@ajvar.org>");
MODULE_DESCRIPTION("RTLinux Coprocess for OlfactometerServer program.  Internal kernel module implementing RTLinux based PID and PWM control for use in conjunction with the OlfactometerServer program.");

int debug = 0;

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Set to nonzero to enable debug messaging");

