#include <asm/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/comedilib.h>
#include "test.h"

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

comedi_t *dev = 0;
int s = 0, c = 0, dc = 25, w = 10000, n = 0;
char *d = "/dev/comedi0";
MODULE_PARM(d, "s");
MODULE_PARM(s, "i");
MODULE_PARM(c, "i");
MODULE_PARM(dc, "i");
MODULE_PARM(w, "i");
MODULE_PARM(n, "i");

int init(void)
{  
  dev = comedi_open(d);
  if (!dev) {
    return EINVAL;
  } 
  doIt(dev, s, c, w, dc, n);
  return 0;
}

void cleanup(void)
{
  stopIt();
  comedi_close(dev);  
}

module_init(init);
module_exit(cleanup);
