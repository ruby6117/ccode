#ifndef Calib_H
#define Calib_H

#include "Common.h"
#include <map>
#include <vector>
#include "Settings.h"

/** Calib objects support the getTable() and setTable()
    and getCoeffs() and setCoeffs() methods.

    Note that setCoeffs() is pure virtual and is the method you 
    must implement to actually actualize the calibration in some real 
    system.

    @see PIDFlowController which implements setCoeffs() and calls the rt 
                           coprocess passing it the coeffs
 */

class Calib
{
public:

  typedef std::map<double, double> Table;
  typedef std::vector<double> Coeffs;

  Calib() {}
  virtual ~Calib() {}
  
  const Table & getTable() const { return calibTable; }
  const Coeffs & getCoeffs() const { return coeffs; }

  bool setTable(const Table &);
  /// implement this to actualize the calibration by affecting some external object...
  virtual bool setCoeffs(const Coeffs & coeffs) = 0;

  /// returns a vector of degree + 1 coefficients to fit the data
  /// points specified by the first argument
  static Coeffs fitData(const Table & data,
                        unsigned degree = 3);
  
  /// apply calibration by transforming an X-value to a calibrated-value
  /// (essentially applies the equation: ret = a*x^3 + b*x^2 + c*x^1 + d*x^0)
  /// where a,b,c,d are the coefficients 
  double xform(double x) const; 
  /// tries to reverse the calibration transformation -- doesn't always work
  /// only works if there is a unique mapping from x -> y in calibration equation
  /// if there isn't result is going to be the first solution to the cubic equation solved for x
  double inverse(double y) const; 


  /// convenience method to save a calib to an ini file section
  bool save(Settings &ini, const String & objname);
  
  /// convenience method to construct a Calib from an ini file section
  bool load(Settings &ini, const String & objname);

protected:

  Coeffs coeffs;
  Table calibTable;

};

#endif
