/*
//@HEADER
// ************************************************************************
//
// fom_problem_rank_two_forcing.hpp
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

#ifndef FOM_PROBLEM_RANK_TWO_FORCING_HPP_
#define FOM_PROBLEM_RANK_TWO_FORCING_HPP_

namespace kokkosapp{

template<typename T>
class FomProblemRankTwoForcing
{
  using scalar_type     = typename T::scalar_type;
  using parser_type     = typename T::parser_type;
  using mesh_info_type  = typename T::mesh_info_type;
  using state_d_type    = typename T::state_d_type;
  using forcing_type    = typename T::forcing_type;
  using observer_type   = typename T::observer_type;
  using seismogram_type = typename T::seismogram_type;
  using signals_h_type  = typename T::signal_instances_h_type;
  using mesh_ord_type	= typename mesh_info_type::ordinal_type;

private:
  // parser with inputs
  const parser_type & parser_;

  // object with info about the mesh
  const mesh_info_type & meshInfo_;

  // how many forcing realization to handle simultaneously
  const int fSize_;

  // number of velocity DOFs
  const mesh_ord_type nVp_;
  // number of stresses DOFs
  const mesh_ord_type nSp_;

  // system object to construct operators
  ShWavePP<T> appObj_;
  // state vector for the velocity DOFs
  state_d_type xVp_d_;
  // state vector for the stress DOFs
  state_d_type xSp_d_;
  // observer object to monitor the time evolution
  observer_type observerObj_;

public:
  FomProblemRankTwoForcing(const parser_type & parser,
			   const mesh_info_type & meshInfo,
			   const MaterialModelBase<scalar_type> & materialObj)
    : parser_(parser),
      meshInfo_(meshInfo),
      fSize_(parser.getForcingSize()),
      nVp_(meshInfo_.getNumVpPts()),
      nSp_(meshInfo_.getNumSpPts()),
      appObj_(meshInfo_, materialObj),
      xVp_d_("xVp_d", nVp_, fSize_),
      xSp_d_("xSp_d", nSp_, fSize_),
      observerObj_(nVp_, nSp_, parser, fSize_)
  {}

public:
  void execute()
  {
    // seismogram
    seismogram_type seismoObj(parser_, meshInfo_, appObj_, fSize_);

    // create vector of signals using target samples
    const auto & depths   = parser_.viewDepths();
    const auto nDepths  = depths.size();
    const auto & periods  = parser_.viewPeriods();
    const auto nPeriods = periods.size();
    const auto & angles   = parser_.viewAngles();
    const auto nAngles  = angles.size();
    const auto & delays   = parser_.viewDelays();
    const auto nDelays  = delays.size();

    // need to run checks
    checkCflCondition();

    // check dispersion for each period
    for (const auto & iT : periods){
      const auto freq = static_cast<scalar_type>(1)/iT;
      checkDispersion(freq);
    }

    // how many total forcing realizations to do
    const std::size_t totFRealizations = nDepths*nPeriods*nAngles*nDelays;
    // for now, only support totFRealizations % fSize == 0
    if (totFRealizations % fSize_ != 0){
      throw std::runtime_error
	("totFRealizations & fSize != 0: tot num of forcing realizations \
must be divisible by fSize");
    }

    signals_h_type signalsForRun("s1",totFRealizations);
    Kokkos::View<scalar_type*, Kokkos::HostSpace> depthsForRun("d1",totFRealizations);
    Kokkos::View<scalar_type*, Kokkos::HostSpace> anglesForRun("a1",totFRealizations);

    std::size_t i=0;
    for (const auto & itDepth : depths)
    {
      for (const auto & itPeriod : periods)
      {
	for (const auto & itAngle : angles)
	{
	  for (const auto & itDelay : delays)
	  {
	    Signal<scalar_type> signal(parser_.getSourceSignalKind(), itDelay, itPeriod);
	    signalsForRun(i) = signal;
	    depthsForRun(i) = itDepth;
	    anglesForRun(i) = itAngle;
	    ++i;
	  }
	}
      }
    }

    const std::size_t numSets = signalsForRun.size()/fSize_;
    std::cout << "Doing rank-2 FOM" << std::endl;
    std::cout << "Total number of samples " << totFRealizations << std::endl;
    std::cout << "Total number of set " << numSets << std::endl;

    for (std::size_t i=0; i<numSets; ++i)
    {
      const std::size_t sInd = i*fSize_;
      const std::size_t eInd = sInd+fSize_;
      auto currSignals = Kokkos::subview(signalsForRun, std::make_pair(sInd, eInd));
      auto currDepths  = Kokkos::subview(depthsForRun,  std::make_pair(sInd, eInd));
      auto currAngles  = Kokkos::subview(anglesForRun,  std::make_pair(sInd, eInd));

      forcing_type forcing(currSignals, parser_,meshInfo_, appObj_,currDepths, currAngles);

      // reset observer and seismogram
      observerObj_.prepForNewRun(i);
      seismoObj.prepForNewRun(i);

      // run fom
      runFom(parser_.getNumSteps(), parser_.getTimeStepSize(),
	     appObj_, forcing, observerObj_, seismoObj,
	     xVp_d_, xSp_d_);

      processCollectedData(seismoObj);
    }

    // coordinates only need to be written once
    processCoordinates();
  }

  void checkDispersion(const scalar_type & freq)
  {
    if (parser_.checkDispersion()){
      checkDispersionCriterion(meshInfo_, freq, appObj_.getMinShearWaveVelocity());
    }
  }

  void checkCflCondition()
  {
    if (parser_.checkCfl()){
      checkCfl(meshInfo_, parser_.getTimeStepSize(), appObj_.getMaxShearWaveVelocity());
    }
  }

  void processCoordinates()
  {
    if(parser_.enableSnapshotMatrix()){
      appObj_.writeCoordinatesToFile(dofId::vp);
      appObj_.writeCoordinatesToFile(dofId::sp);
    }
  }

  template <typename seismo_t>
  void processCollectedData(const seismo_t & seismoObj)
  {
    const auto startTime  = std::chrono::high_resolution_clock::now();

    if(parser_.enableSnapshotMatrix()){
      observerObj_.writeSnapshotMatrixToFile(dofId::vp);
      observerObj_.writeSnapshotMatrixToFile(dofId::sp);
    }

    if(parser_.enableSeismogram()){
      seismoObj.writeReceiversToFile();
    }

    const auto finishTime = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> elapsed = finishTime - startTime;
    std::cout << "\nfinalProcessTime = " << std::fixed << std::setprecision(10)
	      << elapsed.count();
    std::cout << "\n";
  }

};

}//end namespace
#endif
