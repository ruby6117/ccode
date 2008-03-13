#ifndef CID_H
#define CID_H

#define MAX_CHANS  ( (0x1<<8) )
#define MAX_RANGES ( (0x1<<8) )
#define MAX_SUBDEVS ( (0x1<<4) )

#ifdef __cplusplus
extern "C" {
#endif

  /** A struct that represents a unique dev/subdev/channel combo to locate
      the precise channel we are working with on a system. This structure
      is readily represented as an unsigned short for fast serialization. */
  struct CID
  {
    union {
      struct {
        unsigned dev    : 4;
        unsigned subdev : 4;
        unsigned chan   : 8;    
      };
      unsigned short word;
    };
#ifdef __cplusplus
    CID(unsigned short fromNum = 0) { *this = fromNum; }
    
    /** Convert an unsigned 16-bit to a CID by just copying the memory. */
    CID & operator=(unsigned short rhs) 
    {
      word = rhs;
      return *this;
    }

    /** Convert this into an unsigned short by just copying the memory */
    operator unsigned short() const 
    { 
      return word;
    }

    bool isValid() const { return word != static_cast<unsigned short>(-1); }
    void invalidate() { word = static_cast<unsigned short>(-1); }
#endif    
  };

#ifndef __cplusplus
  typedef struct CID CID;
#endif

#ifdef __cplusplus
}
#endif

#endif
