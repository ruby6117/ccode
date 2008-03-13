#include "Calib.h"
#include "Log.h"
#include "PolynomialFit.h"
#include <math.h>
#include "Conf.h"
#include "ConfParse.h"
#include <algorithm>

bool Calib::setTable(const Table & t)
{
  Coeffs c = fitData(t, 3);
  if (c.size() != 4) {
    Error() << "Calibration table specified failed to curve-fit to a 3rd degree polynomial\n";
    return false;
  }
  if (setCoeffs(c)) {
    calibTable = t;
    return true;
  }
  return false;
}


/// returns a vector of degree + 1 coefficients to fit the data
/// points specified by the first argument
/*static*/ 
Calib::Coeffs
Calib::fitData(const Table & data, unsigned degree)
{
  Coeffs ans = PolynomialFit::FitData(degree, data), ret;
  // now reverse, since the answer is in order of low->high and we want high->low
  std::reverse(ans.begin(), ans.end());
  return ans;
}


/// apply calibration by transformin an X-value to a calib-value
/// (essentially applies the equation: ret = a*x^3 + b*x^2 + c*x^1 + d*x^0)
double Calib::xform(double x) const
{
  double ret = 0.0;
  double power = 0.0;
  Coeffs::const_reverse_iterator it;
  for (it = coeffs.rbegin();  it != coeffs.rend(); ++it, ++power)
    ret += (*it) * ::pow(x, power);
  return ret;
}

bool Calib::save(Settings &ini, const String & name)
{
  ini.put(name, Conf::Keys::calib_table, Conf::Gen::calibTable(calibTable));
  ini.put(name, Conf::Keys::curve_fit_coeffs, Conf::Gen::calibCoeffs(coeffs));
  return ini.save();    
}

bool Calib::load(Settings &ini, const String & objname)
{
  calibTable = Conf::Parse::calibTable(ini.get(objname, Conf::Keys::calib_table)); 
  // validate or compute curve fit coefficients
  String cfc = ini.get(objname, Conf::Keys::curve_fit_coeffs);
  if (!cfc.length()) {
    if (!calibTable.size())  
      return false;
    Log() << "Calib obj. '" << objname << "':  auto-fitting calibration curve based on  calibration table from .ini file!\n";
    coeffs = Calib::fitData(calibTable);
  } else { // process cfc
    coeffs = Conf::Parse::calibCoeffs(cfc);
  }
  // at this point coeffs must be valid but calibTable may or may not be..
  return coeffs.size();
}

double Calib::inverse(double y) const
{
  if (coeffs.size() == 4) { // cubic
    // approximate solution using two points linear interpolation.. sorry but i don't think i want to implement a generic cubic equation solution
    double p1 = xform(0.), p2 = xform(100.);
    double b = p1, a = (p2-p1)/100.;
    // y = ax + b
    // 0 = ax + b -y
    //-ax = b - y
    // ax = -b + y
    // x = (y-b)/a
    return a != 0. ? (y-b)/a : y ;
  } else if (coeffs.size() == 3) { // quadratic, use quadratic equation
    double  a = coeffs[0], b = coeffs[1],  c = coeffs[2];
    // x = (-b +- sqrt(b^2-4ac)) / 2a - y
    if (a != 0)
      return  ((-b + sqrt(pow(b, 2)-4*a*c)) / 2*a) - y;
    else
      return y;
  } else if (coeffs.size() == 2) { // linear
    const double & a = coeffs[0], & b = coeffs[1];
    // y = ax^1 + b
    // 0 = ax + b - y
    // -ax = b - y
    // x = b/-a - y/-a
    return a != 0. ? (y-b)/a : y;
  } else if (coeffs.size() == 1) { // offset?
    // y = 0x + b
    // y = b
    // x = ANYTHING
    return 0; // infinitely many solutions
  }
  return y;
}
