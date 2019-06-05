#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/IO/read_xyz_points.h>
#include <CGAL/IO/Writer_OFF.h>
#include <CGAL/property_map.h>
#include <CGAL/Shape_detection_3.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygonal_surface_reconstruction.h>
#include <CGAL/SCIP_mixed_integer_program_traits.h>
#include <CGAL/Timer.h>

#include <fstream>


typedef CGAL::Exact_predicates_inexact_constructions_kernel			Kernel;

typedef Kernel::Point_3												Point;
typedef Kernel::Vector_3											Vector;

// Point with normal, and plane index
typedef boost::tuple<Point, Vector, int>							PNI;
typedef std::vector<PNI>											Point_vector;
typedef CGAL::Nth_of_tuple_property_map<0, PNI>						Point_map;
typedef CGAL::Nth_of_tuple_property_map<1, PNI>						Normal_map;
typedef CGAL::Nth_of_tuple_property_map<2, PNI>						Plane_index_map;

typedef CGAL::Shape_detection_3::Shape_detection_traits<Kernel, Point_vector, Point_map, Normal_map>	Traits;
typedef CGAL::Shape_detection_3::Efficient_RANSAC<Traits>			Efficient_ransac;
typedef CGAL::Shape_detection_3::Plane<Traits>						Plane;
typedef CGAL::Shape_detection_3::Point_to_shape_index_map<Traits>	Point_to_shape_index_map;

typedef	CGAL::Polygonal_surface_reconstruction<Kernel>				Polygonal_surface_reconstruction;
typedef CGAL::Surface_mesh<Point>									Surface_mesh;

typedef CGAL::SCIP_mixed_integer_program_traits<double>				MIP_Solver;

/*
* This example first extracts planes from the input point cloud
* (using RANSAC with default parameters) and then reconstructs 
* the surface model from the planes.
*/

int main()
{
	Point_vector points;

	// Loads point set from a file. 
	const std::string input_file("data/cube.pwn");
    std::ifstream input_stream(input_file.c_str());
	if (input_stream.fail()) {
		std::cerr << "failed open file \'" <<input_file << "\'" << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << "Loading point cloud: " << input_file << "...";

	CGAL::Timer t;
	t.start();
    if (!input_stream ||
		!CGAL::read_xyz_points(input_stream,
			std::back_inserter(points),
			CGAL::parameters::point_map(Point_map()).normal_map(Normal_map())))
	{
		std::cerr << "Error: cannot read file " << input_file << std::endl;
		return EXIT_FAILURE;
	}
	else
		std::cout << " Done. " << points.size() << " points. Time: " << t.time() << " sec." << std::endl;

	// Shape detection
	Efficient_ransac ransac;
	ransac.set_input(points);
	ransac.add_shape_factory<Plane>();

	std::cout << "Extracting planes...";
	t.reset();
	ransac.detect();

	Efficient_ransac::Plane_range planes = ransac.planes();
	std::size_t num_planes = planes.size();

	std::cout << " Done. " << num_planes << " planes extracted. Time: " << t.time() << " sec." << std::endl;

	// Stores the plane index of each point as the third element of the tuple.
	Point_to_shape_index_map shape_index_map(points, planes);
	for (std::size_t i = 0; i < points.size(); ++i) {
		// Uses the get function from the property map that accesses the 3rd element of the tuple.
		int plane_index = get(shape_index_map, i);
		points[i].get<2>() = plane_index;
	}

	//////////////////////////////////////////////////////////////////////////

	std::cout << "Generating candidate faces...";
	t.reset();

	Polygonal_surface_reconstruction algo(
		points,
		Point_map(),
		Normal_map(),
		Plane_index_map()
	);

	std::cout << " Done. Time: " << t.time() << " sec." << std::endl;

	//////////////////////////////////////////////////////////////////////////

	Surface_mesh model;

	std::cout << "Reconstructing...";
	t.reset();

	if (!algo.reconstruct<MIP_Solver>(model)) {
		std::cerr << " Failed: " << algo.error_message() << std::endl;
		return EXIT_FAILURE;
	}

	const std::string& output_file("data/cube_result.off");
	std::ofstream output_stream(output_file.c_str());
	if (output_stream && CGAL::write_off(output_stream, model))
		std::cout << " Done. Saved to " << output_file << ". Time: " << t.time() << " sec." << std::endl;
	else {
		std::cerr << " Failed saving file." << std::endl;
		return EXIT_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////

	// Also stores the candidate faces as a surface mesh to a file
	Surface_mesh candidate_faces;
	algo.output_candidate_faces(candidate_faces);
	const std::string& candidate_faces_file("data/cube_candidate_faces.off");
	std::ofstream candidate_stream(candidate_faces_file.c_str());
	if (candidate_stream && CGAL::write_off(candidate_stream, candidate_faces))
		std::cout << "Candidate faces saved to " << candidate_faces_file << "." << std::endl;

	return EXIT_SUCCESS;
}