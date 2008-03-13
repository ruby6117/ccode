#ifndef test_h
#define test_h

#ifdef __cplusplus
extern "C" {
#endif

  extern int doIt(comedi_t *dev, unsigned sdev, unsigned c, unsigned w_us, unsigned dc, unsigned n);
  extern void stopIt(void);

#ifdef __cplusplus
}
#endif

#endif
