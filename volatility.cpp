//
//  volatility.cpp
//  
//
//  Created by David Tiobang on 21/01/2019.
//

#include "volatility.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <cstdlib>
//#include "solver.hpp"

namespace dauphine
{
volatility::volatility()
{
}

//Declare the fonction for the volatility
double volatility::get_volatility(std::vector<double> arguments) const
{
    //arguments[0]: Use in return if the volatility is path-dependent (depend on spot S)
    //arguments[1]: Use in return if the volatility is time-dependent (depend on time t)
    
    return 0.20; //Here in the exemple the vol is constant and equal 20%
}


volatility::~volatility()
{
}
    
}