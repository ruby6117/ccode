#ifndef Version_H
#define Version_H
#define PRG_VERSION_CODE(a,b,c) ((a&0xf)<<16 | (b&0xf)<<8  | (c&0xf))
#define PRG_VERSION PRG_VERSION_CODE(0,3,3)
#define PRG_VERSION_STR "0.3.3"
#endif
