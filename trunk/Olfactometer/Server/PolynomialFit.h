#ifndef PolynomialFit_H
#define PolynomialFit_H

/// @file PolynomialFit.h -- a wrapper to Joachim Wuttke's  lmfit C library

#include <vector>
#include <map>
#include <string>

namespace PolynomialFit
{

  typedef std::map<double, double> Data;
  typedef std::vector<double> Coefficients;

  /**
     Find the polynomial curve of degree `degree' that fits the data points
     `dataPoints'.  
     
     @param degree the degree desired of the curve --  1 is linear, 2 quadratic, 3 cubic, etc (0 is invalid)
     @param dataPoints the data -- a map of x->y values.
     @param verbose -- if true, print really verbose stuff as the curvefit is done and once it finishes, if false, be silent
     @param statusMsg if not NULL, a string to write a status message to when done
     @param numEvaluations if not NULL, an unsigned integer to store the number of evaluations performed during the curve fit
     @returns a vector of coefficients in the order of lowest to highest degree.
  */
  extern Coefficients FitData(unsigned degree, const Data & dataPoints,
                              bool verbose = false,
                              std::string *statusMsg = 0, 
                              unsigned *numEvaluations = 0);
}


#endif

