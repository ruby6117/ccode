#ifndef ComediDevice_H
#define ComediDevice_H

#define CD_STRMAX 64

#include "CID.h"

#ifdef __cplusplus
extern "C" {
#endif

  /* same thing as the struct in userspace comedilib */
  struct ComediRange
  {
    double min;
    double max;
    unsigned id; /* range id */

#ifdef __cplusplus
    ComediRange() : min(0), max(0) { }
#endif    
  };

#ifdef __cplusplus
} // end extern "C"

template<class CR> void CR_From_comedi_range(ComediRange & r,
                                             const CR &cr, unsigned id = 0) 
{
      r.min = cr.min;
      r.max = cr.max;
      r.id = id;
      //if (cr.unit == 1) { r.min /= 1000.0; r.max /= 1000.0; }
}

template<class CR> CR CR_To_comedi_range(const ComediRange & r)
{
      CR ret;
      ret.min = r.min;
      ret.max = r.max;
      ret.unit = 0;
      return ret;
}

// re-begin extern "C"
extern "C" {
#endif
  
  typedef unsigned int Sample_t;

#ifndef __cplusplus
  typedef struct ComediRange ComediRange;
#endif

  struct ComediSubDevice
  {
    unsigned minor; /* the minor this subdev belongs to */
    unsigned id; /* subdev id */
    unsigned type;
    unsigned nChans;
    /* note: assumption is that maxdata and range info is *not* chan-specific!*/
    unsigned maxdata;
    unsigned nRanges; /* number of ranges for this channel */
    ComediRange range[MAX_RANGES];

    enum Type {
      Unused = 0, AI, AO, DI, DO, DIO, Counter, Timer, Memory, Calib, Proc, 
      Serial
    };

    enum DIOConfig {  Input = 0, Output = 1   };
    
  };

#ifndef __cplusplus
typedef struct ComediSubDevice ComediSubDevice
#endif

  struct ComediDevice
  {
    unsigned minor; /* /dev/comediXX, basically */
    char driverName[CD_STRMAX];
    char boardName [CD_STRMAX]; 
    unsigned versionCode;
    unsigned nSubdevs;
    ComediSubDevice subdev[MAX_SUBDEVS];
  };



#ifndef __cplusplus
  typedef struct ComediDevice ComediDevice;
#endif

#ifdef __cplusplus
}
#endif

#endif
