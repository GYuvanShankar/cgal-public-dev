// polygonal_surface_reconstruction_test.cpp
//
// Test the Polygonal Surface Reconstruction method with different 
//    - kernels(Simple_cartesian, EPICK)
//    - solvers(GLPK, SCIP)
//    - use/ignore provided planar segmentation
//    - input file formats (pwn, ply). For ply format, a property named "segment_index" 
//		stores the plane index for each point(-1 if the point is not assigned to a plane).



#include <CGAL/Simple_cartesian.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>

#include <CGAL/GLPK_mixed_integer_program_traits.h>
#include <CGAL/SCIP_mixed_integer_program_traits.h>

#include "polygonal_surface_reconstruction_test_framework.h"


// kernels:
typedef CGAL::Simple_cartesian<double>								Cartesian;
typedef CGAL::Exact_predicates_inexact_constructions_kernel			Epick;

// solvers:
typedef CGAL::GLPK_mixed_integer_program_traits<double>				GLPK_Solver;
typedef CGAL::SCIP_mixed_integer_program_traits<double>				SCIP_Solver;


int main(int argc, char * argv[])
{
	std::cerr << "Test the Polygonal Surface Reconstruction method" << std::endl;



//	argc = 2;
//    argv[1] = "data/cube.pwn";
//    argv[1] = "data/ball.ply";
//    argv[1] = "data/building.ply";



	// usage
	if (argc - 1 == 0) {
		std::cerr << "For the input point cloud, reconstruct a water-tight polygonal surface.\n";
		std::cerr << "\n";
		std::cerr << "Usage: " << argv[0] << " point_cloud_file" << std::endl;
		std::cerr << "Input file formats are \'pwn\' and \'ply\'. No output.\n";
		return EXIT_FAILURE;
	}

	char* input_file = argv[1];

	//---------------------------------------------------------------------

	std::cerr << "--- Using Simple cartesian kernel" << std::endl;

	//---------------------------------------------------------------------

	std::cerr << "\t---- Using GLPK solver" << std::endl;

	std::cerr << "\t\t---- using provided planes" << std::endl;
	reconstruct<Cartesian, GLPK_Solver>(input_file, false);

	std::cerr << "\t\t---- re-extract planes" << std::endl;
	reconstruct<Cartesian, GLPK_Solver>(input_file, true);

	std::cerr << "\t---- Using SCIP solver" << std::endl;

	std::cerr << "\t\t---- using provided planes" << std::endl;
	reconstruct<Cartesian, SCIP_Solver>(input_file, false);

	std::cerr << "\t\t---- re-extract planes" << std::endl;
	reconstruct<Cartesian, SCIP_Solver>(input_file, true);


	//---------------------------------------------------------------------

	std::cerr << "--- Using Epick kernel" << std::endl;

	//---------------------------------------------------------------------

	std::cerr << "\t---- Using GLPK solver" << std::endl;

	std::cerr << "\t\t---- using provided planes" << std::endl;
	reconstruct<Epick, GLPK_Solver>(input_file, false);

	std::cerr << "\t\t---- re-extract planes" << std::endl;
	reconstruct<Epick, GLPK_Solver>(input_file, true);

	std::cerr << "\t---- Using SCIP solver" << std::endl;

	std::cerr << "\t\t---- using provided planes" << std::endl;
	reconstruct<Epick, SCIP_Solver>(input_file, false);

	std::cerr << "\t\t---- re-extract planes" << std::endl;
	reconstruct<Epick, SCIP_Solver>(input_file, true);
}
