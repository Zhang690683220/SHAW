
#include "CLI11.hpp"
#include "Kokkos_Core.hpp"
#include "../shared/constants.hpp"
#include "../shared/meta.hpp"
#include "../shared/io.hpp"
#include "KokkosBlas1_fill.hpp"
#include "KokkosBlas2_gemv.hpp"
#include "utility"

template <typename sc_t, typename snap_t, typename seismo_t, typename pod_t>
class ComputeSeismoRankOneState
{
  pod_t pod_;
  snap_t D_;
  seismo_t S_;

public:
  ComputeSeismoRankOneState() = delete;

  ComputeSeismoRankOneState(pod_t pod, snap_t D, seismo_t S)
    : pod_(pod), D_(D), S_(S){}

  KOKKOS_INLINE_FUNCTION
  void operator()(const std::size_t & i) const
  {
    // i is the index of the point where to reconstrcut the state
    // so i basically indexes the rows of D
    // here I have to reconstruct the state at the i-th point for all times
    const auto numTimeSteps = D_.extent(1);
    for (std::size_t j=0; j<numTimeSteps; ++j)
    {
      S_(i, j) = {};
      for (std::size_t k=0; k<pod_.extent(1); ++k){
	S_(i, j) += pod_(i, k) * D_(k,j);
      }
    }
  }
};

template<class dmat_t>
void readRowsFromBinaryMatrixWithSize(const std::string filename,
				      dmat_t M,
				      const std::vector<std::size_t> & pts)
{
  using int_t = std::size_t;
  using sc_t  = typename dmat_t::value_type;
  std::ifstream fin(filename, std::ios::in | std::ios::binary);
  fin.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  int_t rows={};
  int_t cols={};
  fin.read((char*) (&rows),sizeof(int_t));
  fin.read((char*) (&cols),sizeof(int_t));
  std::cout << rows << " " << cols << std::endl;

  // move past the initial size
  int_t shiftRealData = sizeof(int_t)*2;

  int_t lda = rows;
  int_t im = 0;
  for (auto & i : pts)
  {
    // find the shift for this point that takes us to correct row
    // of the first column because matrix is layoutleft
    const std::size_t rowShift = shiftRealData + i*sizeof(sc_t);

    // loop over the row to get all values in the row by
    // jumping by the leading extent
    for (std::size_t j=0; j<M.extent(1); ++j)
    {
      fin.seekg(rowShift + j*lda*sizeof(sc_t), fin.beg);

      if (!fin){
	std::cout << std::strerror(errno) << std::endl;
	throw std::runtime_error("ERROR READING binary file");
      }

      fin.read((char*) (&M(im,j)), sizeof(sc_t));
      //std::cout << i << " " << j << " " << M(im,j) << std::endl;
    }
    im++;
  }

  fin.close();
}

int main(int argc, char *argv[])
{
  CLI::App app{"Seismogram reconstructor"};

  using pair_t = std::pair<std::string, std::string>;
  pair_t podModes = {};
  pair_t romSnaps = {};
  std::size_t romSize = {};
  std::vector<std::size_t> targetGridPoints;
  std::size_t fSize = {};
  std::string outputFormat = {};
  std::string outFileAppend = {};

  app.add_option("--podmodes", podModes,
		 "Pair: fullpath_POD_modes binary/ascii")->required();

  app.add_option("--romsize", romSize,
		 "ROM size")->required();

  app.add_option("--romsnaps", romSnaps,
		 "Pair: fullpath_ROM_snaps binary/ascii")->required();

  app.add_option("--gridids", targetGridPoints,
		 "List of grid pts IDs where to compute seismo")->required();

  app.add_option("--fsize", fSize,
		 "Forcing size used to generate ROM snapshots")->required();

  app.add_option("--outformat", outputFormat,
		 "Outputformat: binary/ascii")->required();

  app.add_option("--outfileappend", outFileAppend,
		 "String to append to output file");

  CLI11_PARSE(app, argc, argv);

  std::cout << "POD modes = "
	    << std::get<0>(podModes) << " "
	    << std::get<1>(podModes) << std::endl;
  std::cout << "ROM snaps = "
	    << std::get<0>(romSnaps) << " "
	    << std::get<1>(romSnaps) << std::endl;

  std::cout << "ROM size = " << romSize << std::endl;
  std::cout << "Size of f = " << fSize << std::endl;
  std::cout << "Output format = " << outputFormat << std::endl;

  std::cout << "Target gids = ";
  for (auto & it : targetGridPoints)
    std::cout << it << " ";
  std::cout << std::endl;

  const auto podFilePath = std::get<0>(podModes);
  const bool podIsBinary = std::get<1>(podModes)=="binary";
  const auto romSnapFile   = std::get<0>(romSnaps);
  const bool romSnapBinary = std::get<1>(romSnaps)=="binary";


  //------------------------------------------------------------
  Kokkos::initialize (argc, argv);
  {
    using sc_t = double;
    using exe_space = Kokkos::DefaultExecutionSpace;
    using kll = Kokkos::LayoutLeft;
    static_assert(is_host_space<exe_space>::value, "");
    using pod_t = Kokkos::View<sc_t**, kll, exe_space>;

    const auto numPts = targetGridPoints.size();

    // load target rows of the modes
    pod_t phi("phi", numPts, romSize);
    readRowsFromBinaryMatrixWithSize<pod_t>(podFilePath, phi, targetGridPoints);

    if (fSize == 1)
    {
      // *** load ROM states ***
      using snap_t = Kokkos::View<sc_t**, kll, exe_space>;
      snap_t snapsRom("snapsRom", 1, 1);
      if (romSnapBinary) readBinaryMatrixWithSize(romSnapFile, snapsRom);
      else
      	throw std::runtime_error("Rank-1 ROM snaps ascii not supported yet");
      std::cout << "ROM snap size: "
      		<< snapsRom.extent(0) << " "
      		<< snapsRom.extent(1) << std::endl;

      // *** reconstruct seismogram ***
      // S: 2d view, each row contains the velocity time series for a target point
      using seismo_t = Kokkos::View<sc_t**, kll, exe_space>;
      seismo_t S("S", numPts, snapsRom.extent(1));

      // reconstruct seismogram, use parallel for over each target location
      using func_t = ComputeSeismoRankOneState<sc_t, snap_t, seismo_t, pod_t>;
      func_t F(phi, snapsRom, S);
      Kokkos::parallel_for(numPts, F);
      // write seismogram to file
      writeToFile("rom_seismo.txt", S, false, false);
    }
    else if (fSize >= 2)
    {
      ////////////////////////////
      // this is rank-2 case
      ////////////////////////////

      // *** load ROM states ***
      using snap_t = Kokkos::View<sc_t***, kll, exe_space>;
      snap_t snapsRom("snapsRom", 1, 1, 1);
      if (romSnapBinary) readBinaryMatrixWithSize(romSnapFile, snapsRom);
      else
      	throw std::runtime_error("Rank-2 ROM snaps ascii not supported yet");
      std::cout << "ROM snap size: "
      		<< snapsRom.extent(0) << " "
      		<< snapsRom.extent(1) << " "
      		<< snapsRom.extent(2) << std::endl;

      // *** reconstruct seismogram ***
      // S: 2d view, each row contains the velocity time series for a target point
      using seismo_t = Kokkos::View<sc_t**, kll, exe_space>;
      seismo_t S("S", numPts, snapsRom.extent(1));

      // loop over the forcing realizations
      for (std::size_t fId=0; fId<snapsRom.extent(2); ++fId)
      {
      	const std::string fString = "f_"+std::to_string(fId);

	auto currSnaps = Kokkos::subview(snapsRom, Kokkos::ALL(), Kokkos::ALL(), fId);
	KokkosBlas::fill(S, 0.0);

	// reconstruct seismogram, use parallel for over each target location
	using func_t = ComputeSeismoRankOneState<sc_t, decltype(currSnaps), seismo_t, pod_t>;
	func_t F(phi, currSnaps, S);
	Kokkos::parallel_for(numPts, F);

	auto seismoFile = "seismo_"+fString;
	if (outFileAppend.empty() == false) seismoFile += "_" + outFileAppend;
	writeToFile(seismoFile, S, (outputFormat=="binary"), true);
      }
    }
  }

  Kokkos::finalize();
  std::cout << "success" << std::endl;
  return 0;
}





  // // using M_t = Eigen::Matrix<sc_t,Eigen::Dynamic,Eigen::Dynamic,Eigen::ColMajor>;
  // // M_t A(7,4);
  // // int c=0;
  // // for (int i=0; i<A.rows(); ++i){
  // //   for (int j=0; j<A.cols(); ++j){
  // //     A(i,j) = c;
  // //     c++;
  // //   }
  // // }
  // // std::cout << A << std::endl;
  // // writeToFile("M.bin", A, true);
  // // //  0  1  2  3
  // // //  4  5  6  7
  // // //  8  9 10 11
  // // // 12 13 14 15
  // // // 16 17 18 19
  // // // 20 21 22 23
  // // // 24 25 26 27

  // //----
  // {
  //   std::size_t romSize = 404;
  //   std::vector<std::size_t> rows = {89417, 89433};

  //   std::string podFile  = "/Users/fnrizzi/Desktop/waveSample/t1/modes/lsv_vp";
  //   std::string snapFile = "/Users/fnrizzi/Desktop/waveSample/t1/rom/snaps_vp_0";

  //   // target locations where to reconstruct
  //   const auto numPts = rows.size();


  //   using exe_space = Kokkos::DefaultExecutionSpace;
  //   using kll = Kokkos::LayoutLeft;

  //   // load target rows of the modes
  //   using pod_t = Kokkos::View<sc_t**, kll, exe_space>;
  //   pod_t phi("phi", numPts, romSize);
  //   readRowsFromBinaryMatrixWithSize<pod_t>(podFile, phi, rows);

  //   // pod_t phi0("phi0", 7, romSize);
  //   // readBinaryMatrixWithSize(pod, phi0);
  //   // std::pair<std::size_t, std::size_t> pa(0, romSize);
  //   // auto phiF = Kokkos::subview(phi0, Kokkos::ALL, pa);
  //   // for (int j=0; j<4; ++j){
  //   //   std::cout << phi(0,j) << " " << phiF(rows[0],j) << std::endl;
  //   // }

  //   // using state_t = Kokkos::View<sc_t*, kll, exe_space>;
  //   // state_t romVp("romVp", romSize);
  //   // state_t fomVp("fomVp", numPts);
  //   // std::string romState = "/Users/fnrizzi/Desktop/waveSample/t1/rom/finalRomState_vp_0";
  //   // readAsciiVectorWithSize(romState, romVp);
  //   // // using func_t = ReconstructFomStateRankOneRom<pod_t, state_t, state_t>;
  //   // // func_t F(phi, fomVp, romVp);
  //   // // Kokkos::parallel_for(numPts, F);

  //   // // const char ct_N	= 'N';
  //   // // KokkosBlas::gemv(&ct_N, 1, phi, romVp, 0., fomVp);
  //   // // std::cout << fomVp(rows[0]) << " " << fomVp(rowsnn[1]) << std::endl;

  //   // for (std::size_t k=0; k<phi.extent(1); ++k){
  //   //   //fomVp(0) += phi(89417, k) * romVp(k);
  //   //   //fomVp(1) += phi(89433, k) * romVp(k);
  //   //   fomVp(0) += phi(0, k) * romVp(k);
  //   //   fomVp(1) += phi(1, k) * romVp(k);
  //   // }
  //   // std::cout << fomVp(0) << " " << fomVp(1) << std::endl;


  //   // for rank-1 rom state, snaps are 2d views
  //   using snap_t = Kokkos::View<sc_t**, kll, exe_space>;
  //   snap_t snapsRom("snapsRom", 1, 1);
  //   readBinaryMatrixWithSize(snapFile, snapsRom);
  //   std::cout << snapsRom.extent(0) << " " << snapsRom.extent(1) << std::endl;

  //   // S: 2d view, each row contains the velocity time series for a target point
  //   using seismo_t = Kokkos::View<sc_t**, kll, exe_space>;
  //   seismo_t S("S", numPts, snapsRom.extent(1));

  //   // reconstruct seismogram, use parallel for over each target location
  //   using func_t = ComputeSeismoRankOneState<sc_t, snap_t, seismo_t, pod_t>;
  //   func_t F(phi, snapsRom, S);
  //   Kokkos::parallel_for(numPts, F);

  //   // write seismogram to file
  //   writeToFile("rom_seismo.txt", S, false, false);
  // }
