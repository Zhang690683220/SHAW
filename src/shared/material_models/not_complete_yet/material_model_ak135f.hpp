/*
//@HEADER
// ************************************************************************
//
// material_model_ak135f.hpp
//                     		Pressio/SHAW
//                         Copyright 2019
// National Technology & Engineering Solutions of Sandia, LLC (NTESS)
//
// Under the terms of Contract DE-NA0003525 with NTESS, the
// U.S. Government retains certain rights in this software.
//
// Pressio is licensed under BSD-3-Clause terms of use:
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Francesco Rizzi (fnrizzi@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#ifndef MATERIAL_MODEL_AK135F_HPP_
#define MATERIAL_MODEL_AK135F_HPP_

#include "material_model_base.hpp"

template<typename scalar_t, typename parser_t>
class Ak135fMaterialModel final : public MaterialModelBase<scalar_t>
{
  using profile_params_t = typename parser_t::profile_params_t;

  const parser_t & parser_;

public:
  Ak135fMaterialModel(const parser_t & parser) : parser_(parser){}

  void computeAt(const scalar_t & radiusFromEarthCenterMeters,
		 const scalar_t & angleRadians,
		 scalar_t & rho,
		 scalar_t & vs) const final
  {
    constexpr auto thousand  = constants<scalar_t>::thousand();
    constexpr auto esrKm     = constants<scalar_t>::earthSurfaceRadiusKm();
    constexpr auto esrMeters = constants<scalar_t>::earthSurfaceRadiusMeters();

    //http://rses.anu.edu.au/seismology/ak135/ak135f.html

    const auto rKm = radiusFromEarthCenterMeters/thousand;
    const auto d   = esrKm - rKm;
    const auto dSq = d*d;
    const auto dCu = d*d*d;

    if(d >= 0 and d <= 3.){
      rho = 1.02;
      vs  = 1.;
    }
    else if(d > 3. and d <= 3.3){
      rho = 2.;
      vs  = 1.;
    }
    else if(d > 3.3 and d <= 10.){
      rho = 2.6;
      vs  = 3.2;
    }
    else if(d > 10. and d <= 18.){
      rho = 2.92;
      vs  = 3.9;
    }
    else if(d > 18. and d <= 80.){
      rho = 3.688908 - 0.002755944*d + 5.244987e-6*dSq;
      vs  = 4.479938 - 2.838134e-4*d - 3.537925e-6*dSq;
    }
    else if(d > 80. and d <= 120.){
      rho = 3.6524 - 0.00188*d;
      vs  = 4.47 + 0.00025*d;
    }
    else if(d > 120. and d <= 210.){
      rho = 3.561983 - 0.001138889*d;
      vs  = 4.4754   + 0.00020444*d;
    }
    else if(d > 210. and d <= 410.){
      rho = 3.130252 + 0.0009128*d;
      vs  = 4.151532 + 0.0017548*d;
    }
    else if(d > 410. and d <= 660.){
      rho = 3.948468 - 4.548571e-5*d;
      vs  = 4.211150 + 2.120343e-3*d;
    }
    else if(d > 660. and d <= 2740.){
      rho = 3.789334 + 8.533642e-4*d - 9.455671e-8*dSq;
      vs  = 5.530732 + 9.439965e-4*d - 1.202153e-7*dSq;
    }
    else if(d > 2740. and d <= 2891.5){
      rho = 4.268296 + 0.0005202*d;
      vs  = 6.648918 + 0.0002188*d;
    }
    else if(d > 2891.5 and d <= 5153.5){
      rho = 3.523060 + 2.916881e-3*d - 2.421664e-7*dSq;
      vs  = 0.0;
    }
    else if(d > 5153.5 and d <= 6371){
      rho = 4.565499   + 2.651449e-3*d - 2.080745e-7*dSq;
      vs  = -0.8068098 + 1.405390e-3*d - 1.103537e-7*dSq;
    }

    // convert vs from m/s to km/s
    vs *= thousand;
    // convert rho from g/cm^3 to km/m^3
    rho *= thousand;
  }
};

#endif
