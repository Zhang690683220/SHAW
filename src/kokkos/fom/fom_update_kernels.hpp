/*
//@HEADER
// ************************************************************************
//
// fom_update_kernels.hpp
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

#ifndef LEAP_FROG_FOM_VELOCITY_UPDATE_HPP_
#define LEAP_FROG_FOM_VELOCITY_UPDATE_HPP_

#include "KokkosSparse_spmv.hpp"
#include "KokkosBlas1_mult.hpp"
#include "KokkosBlas1_axpby.hpp"
#include <numeric>

namespace kokkosapp{

// rank-1 specialize
template <
  typename sc_t,
  typename state_d_t,
  typename jac_d_t,
  typename rho_inv_d_t,
  typename forcing_t
  >
typename std::enable_if<is_kokkos_1dview<state_d_t>::value>::type
updateVelocity(const sc_t & dt,
	       state_d_t xVp_d,
	       typename state_d_t::const_type xSp_d,
	       const jac_d_t jacVp_d,
	       const rho_inv_d_t rhoInvVp_d,
	       forcing_t & fObj)
{
  /* compute the velocity update:
   *	xVp = xVp + dt*jacVp*xSp + dt*rhoInvVp*f;
   *
   * which we split into two steps:
   *	A1	xVp = xVp + dt * Jvp * xSp
   *	A2	xVp = xVp + dt * rhoInvVp * f
   */

  constexpr auto one  = constants<sc_t>::one();
  KokkosSparse::spmv(KokkosSparse::NoTranspose, dt, jacVp_d, xSp_d, one, xVp_d);

  // maybe we should do the following on host directly since
  // for a single forcing, if pointwise, we only change a single element
  auto f_d = fObj.viewForcingDevice();
  KokkosBlas::mult(one, xVp_d, dt, rhoInvVp_d, f_d);
}

// rank-1 specialize
template <typename sc_t, typename state_d_t, typename jac_d_t>
typename std::enable_if<is_kokkos_1dview<state_d_t>::value>::type
updateStress(const sc_t & dt,
	     state_d_t xSp_d,
	     const typename state_d_t::const_type xVp_d,
	     jac_d_t jacSp_d)
{
  // xSp = xSp + dt * Jac * xVp

  constexpr auto one  = constants<sc_t>::one();
  KokkosSparse::spmv(KokkosSparse::NoTranspose, dt, jacSp_d, xVp_d, one, xSp_d);
}

template <class sc_t, class state_t, class gids_t, class f_t, class rho_inv_t>
struct AddForcingRank2
{
  sc_t dt_;
  state_t x_;
  gids_t gids_;
  f_t f_;
  rho_inv_t rhoInv_;

  AddForcingRank2(const sc_t & dt,
		  state_t x,
		  gids_t gids,
		  f_t f,
		  rho_inv_t rhoInv)
    : dt_(dt), x_(x), gids_(gids), f_(f), rhoInv_(rhoInv){}

  KOKKOS_INLINE_FUNCTION
  void operator() (std::size_t i) const
  {
    const auto gidValue = gids_(i);
    const auto & rhoInv = rhoInv_(gidValue);
    x_(gidValue, i) += rhoInv*f_(i)*dt_;
  }
};


// rank-2 specialize
template <
  typename sc_t,
  typename state_d_t,
  typename jac_d_t,
  typename rho_inv_d_t,
  typename forcing_t
  >
typename std::enable_if<is_kokkos_2dview<state_d_t>::value>::type
updateVelocity(const sc_t & dt,
	       state_d_t xVp_d,
	       typename state_d_t::const_type xSp_d,
	       const jac_d_t jacVp_d,
	       const rho_inv_d_t rhoInvVp_d,
	       forcing_t & fObj)
{
  /*
   *	A1	xVp = xVp + dt * Jvp * xSp
   *	A2	xVp = xVp + dt * rhoInvVp * f
   */

  constexpr auto one  = constants<sc_t>::one();
  KokkosSparse::spmv(KokkosSparse::NoTranspose, dt, jacVp_d, xSp_d, one, xVp_d);
  // auto f_d = fObj.viewForcingDevice();
  // KokkosBlas::mult(one, xVp_d, dt, rhoInvVp_d, f_d);

  // maybe we should do the following on host directly since
  // it might be too small for device
  auto vpGids_d = fObj.getVpGidsDevice();
  auto f_d = fObj.viewForcingDevice();
  using gids_t = decltype(vpGids_d);
  using f_d_t  = decltype(f_d);
  using functor_t = AddForcingRank2<sc_t, state_d_t, gids_t, f_d_t, rho_inv_d_t>;
  functor_t fnc(dt, xVp_d, vpGids_d, f_d, rhoInvVp_d);
  Kokkos::parallel_for(vpGids_d.extent(0), fnc);
}

// rank-2 specialize
template <typename sc_t, typename state_d_t, typename jac_d_t>
typename std::enable_if<is_kokkos_2dview<state_d_t>::value>::type
updateStress(const sc_t & dt,
	     state_d_t xSp_d,
	     const typename state_d_t::const_type xVp_d,
	     const jac_d_t jacSp_d)
{
  // xSp = xSp + dt * Jac * xVp

  constexpr auto one  = constants<sc_t>::one();
  KokkosSparse::spmv(KokkosSparse::NoTranspose, dt, jacSp_d, xVp_d, one, xSp_d);
}

}//end namespace kokkosapp
#endif
