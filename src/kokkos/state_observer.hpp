/*
//@HEADER
// ************************************************************************
//
// state_observer.hpp
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

#ifndef SHWAVEPP_OBSERVER_HPP_
#define SHWAVEPP_OBSERVER_HPP_

template <typename state_t, typename dest_t>
struct CopyState
{
  std::size_t colIndex_;
  state_t x_;
  dest_t M_;

  CopyState(const std::size_t & colIndex, const state_t & x, const dest_t & M)
    : colIndex_(colIndex), x_(x), M_(M){}

  template <typename _state_t = state_t>
  KOKKOS_INLINE_FUNCTION
  typename std::enable_if<is_kokkos_1dview<_state_t>::value>::type
  operator() (const std::size_t & i) const
  {
    M_(i, colIndex_, 0) = x_(i);
  }

  template <typename _state_t = state_t>
  KOKKOS_INLINE_FUNCTION
  typename std::enable_if<is_kokkos_2dview<_state_t>::value>::type
  operator() (const std::size_t & i) const
  {
    for (std::size_t j=0; j<M_.extent(2); ++j)
      M_(i, colIndex_, j) = x_(i,j);
  }
};


template <typename scalar_t>
struct StateObserver
{
  // here we have to specify the layout because of how we write to file
  using matrix_t = Kokkos::View<scalar_t***, Kokkos::LayoutLeft, Kokkos::HostSpace>;

private:
  bool useBinaryIO_   = {};
  bool enableSnapMat_ = {};
  std::array<std::string,2> snapFileName_ = {};

  std::array<std::size_t, 2> numDofs_ = {};

  // to count the snapshots
  std::array<std::size_t, 2> count_ = {};
  // sampling frequency
  std::array<std::size_t, 2> snapshotFreq_ = {};

  // snapshot matrices
  matrix_t Avp_;
  matrix_t Asp_;

  // runID used when we run many samples to prepend file
  std::size_t runID_ = 0;

public:
  template <typename parser_t>
  StateObserver(std::size_t numDof_vp,
		std::size_t numDof_sp,
		const parser_t & parser,
		std::size_t fSize = 1)
    : useBinaryIO_(parser.writeSnapshotsBinary()),
      enableSnapMat_{parser.enableSnapshotMatrix()},
      snapFileName_{{parser.getSnapshotFileName(dofId::vp),
		     parser.getSnapshotFileName(dofId::sp)}},
      numDofs_{{numDof_vp, numDof_sp}},
      snapshotFreq_{{parser.getSnapshotFreq(dofId::vp),
		     parser.getSnapshotFreq(dofId::sp)}}
  {
    if (enableSnapMat_)
    {
      std::cout << "\n";
      std::cout << "*** Constructing observer ***" << std::endl;

      const auto Nsteps = parser.getNumSteps();
      std::size_t numColsVp = 0;
      std::size_t numColsSp = 0;

      // make sure number of steps is divisible by sampling frequency
      if ( Nsteps % snapshotFreq_[0] == 0){
	numColsVp = Nsteps/snapshotFreq_[0];
      }
      else{
	throw std::runtime_error("Snapshot Vp frequency not a divisor of steps");
      }

      // make sure number of steps is divisible by sampling frequency
      if ( Nsteps % snapshotFreq_[1] == 0 ){
	numColsSp = Nsteps/snapshotFreq_[1];
      }
      else{
	throw std::runtime_error("Snapshot Sp frequency not a divisor of steps");
      }

      //resize matrix
      Kokkos::resize(Avp_, numDof_vp, numColsVp, fSize);
      Kokkos::resize(Asp_, numDof_sp, numColsSp, fSize);

      const double memAvp = Avp_.extent(0)*Avp_.extent(1)*Avp_.extent(2) * sizeof(scalar_t);
      const double memAsp = Asp_.extent(0)*Asp_.extent(1)*Asp_.extent(2) * sizeof(scalar_t);
      std::cout << "Observer: Vp snaps [GB] = " << memAvp/(1024.*1024.*1024.) << std::endl;
      std::cout << "Observer: Sp snaps [GB] = " << memAsp/(1024.*1024.*1024.) << std::endl;
    }
  }

  bool enabled() const
  {
    return enableSnapMat_;
  }

  void prepForNewRun(const std::size_t & runIdIn)
  {
    // assumes the new run has same sampling frequncies as before
    count_ = {0,0};
    runID_ = runIdIn;
  }

  const auto & viewSnapshotMatrix(const dofId & dof) const
  {
    switch (dof){
    case dofId::vp: return Avp_;
    case dofId::sp: return Asp_;
    }
  }

  template<typename state_t>
  void observe(dofId dof,
	       std::size_t step,
	       const state_t & xhv)
  {
    static_assert
      (std::is_same<typename state_t::memory_space, Kokkos::HostSpace>::value,
       "View not accessible on host");

    if (enableSnapMat_)
    {
      auto & A	      = (dof==dofId::vp) ? Avp_ : Asp_;
      const auto freq = (dof==dofId::vp) ? snapshotFreq_[0] : snapshotFreq_[1];
      auto & count    = (dof==dofId::vp) ? count_[0] : count_[1];

      if ( step % freq == 0 and step > 0)
      {
	using functor_t = CopyState<state_t, matrix_t>;
	functor_t fnc(count, xhv, A);

	// must specify an host exespace here otherwise it picks the default
	// which might be a device one
	using copy_exespace = Kokkos::DefaultHostExecutionSpace;
	Kokkos::RangePolicy<copy_exespace> policy(0, xhv.extent(0));
	Kokkos::parallel_for(policy, fnc);

  	count++;
      }
    }
  }

  void writeSnapshotMatrixToFile(const dofId & dof) const
  {
    if (enableSnapMat_)
    {
      std::cout << "Writing snapshots " + dofIdToString(dof);

      const auto dofName = dofIdToString(dof);
      auto & A = (dof==dofId::vp) ? Avp_ : Asp_;
      auto & fN = (dof==dofId::vp) ? snapFileName_[0] : snapFileName_[1];
      // append the runID to the file
      auto fN2 = fN + "_" + std::to_string(runID_);

      if (A.extent(2) == 1)
      {
	// if the forcing has rank-1 the third dim of A is 1
	const auto Av = Kokkos::subview(A, Kokkos::ALL(), Kokkos::ALL(), 0);
	writeToFile(fN2, Av, useBinaryIO_);
      }
      else{
	writeToFile(fN2, A, useBinaryIO_);
      }
      std::cout << "... Done" << std::endl;
    }
  }
};

#endif
