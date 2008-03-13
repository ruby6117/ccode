#ifndef PIDFCParams_H
#define PIDFCParams_H

#include "PID.h"
#include "SysDep.h"

struct PIDFCParams 
{
        PID::ControlParams controlParams;

        unsigned dev_ai, subdev_ai, chan_ai, dev_ao, subdev_ao, chan_ao;
        unsigned rate_hz; ///< pid update rate in hz

        /// NB dont' copy or init these in nonrt kernel to avoid FPU problems
        double
          flow_set, 
          flow_actual,
          last_v_in, last_v_out,
          /// these correspond to the current range settings
          vmin_ai, vmin_ao, vmax_ai, vmax_ao, 
          /// these are clippings for AO -- allows servo to not use certain voltages
          vclip_ao_min, vclip_ao_max,

          a, b, c, d; ///< coeffs for cubic curve
  
        // these need to be here to hopefully prevent kernel FPE
        PIDFCParams() { Memset(this, 0, sizeof(*this)); }
        PIDFCParams(const PIDFCParams &o) { *this = o; }
        PIDFCParams & operator=(const PIDFCParams &o) 
          { Cpy(*this, o); return *this; }
};

#endif
