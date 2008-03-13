#ifndef PWMVParams_H
#define PWMVParams_H

struct PWMVParams
{
  unsigned windowSizeMicros; /**< the window size in microseconds */
  unsigned dutyCycle; /**< number from 0 -> 100 for percent duty cycle */
  unsigned dev, subdev, chan;  ///< comedi params
};

#endif
