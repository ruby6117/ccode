#include "PolynomialFit.h"
#include "lmmin.h"
#include "lm_eval.h"

static inline double POW(double a, unsigned b) 
{
  double ret = 1.0;
  while (b--) ret *= a;  
  return ret;
}

static double fit_function( void *arg, double t, double* p ) 
{
  unsigned degree = reinterpret_cast<unsigned>(arg);
  double ret = 0.0;
  for (unsigned i = 0; i <= degree; ++i) 
    ret += p[i] * POW(t, i);
  return ret;
}

static 
void dont_print( int n_par, double* par, int m_dat, double* fvec, 
                 void *data, int iflag, int iter, int nfev )
{
  (void)n_par; (void)par; (void)m_dat; (void) fvec; (void)data; (void)iflag;
  (void)iter; (void)nfev;
  // do nothing..
}

PolynomialFit::Coefficients 
PolynomialFit::FitData(unsigned deg, const Data & pts,
                       bool verbose,
                       std::string * msg, unsigned * num_evals)
{
  Coefficients ret;
  unsigned num_pts = pts.size();
  if (!deg || !num_pts) return ret;
  ret.resize(deg+1);

  // start off with 1, 1, 1.. etc coefficients?
  for (unsigned i = 0; i < ret.size(); ++i) ret[i] = 1.0;

  lm_data_type mydata;
  std::vector<double> t_pts;
  std::vector<double> y_pts;
  t_pts.resize(num_pts);
  y_pts.resize(num_pts);
  mydata.user_t = &t_pts[0];
  mydata.user_y = &y_pts[0];
  mydata.user_func = fit_function;
  mydata.user_arg = reinterpret_cast<void *>(deg);
  int i; 
  Data::const_iterator it;
  for (i = 0, it = pts.begin(); it != pts.end(); ++it, ++i) {
    t_pts[i] = it->first;
    y_pts[i] = it->second;
  }

  lm_control_type control; 
  lm_initialize_control( &control );
  control.maxcall = 10000; // 10,000 max evals?
  
  // perform the fit..
  lm_minimize(num_pts, ret.size(), &ret[0], lm_evaluate_default, verbose ? lm_print_default : dont_print, &mydata, &control);

  if (msg) *msg = lm_shortmsg[control.info];
  if (num_evals) *num_evals = control.nfev;
  
  return ret;
}
