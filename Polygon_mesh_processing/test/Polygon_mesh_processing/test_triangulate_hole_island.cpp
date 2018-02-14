#include <vector>
#include <string>
#include <fstream>
#include <math.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole_island.h>
#include <CGAL/Polyhedron_3.h>

#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>

/// extra code necessary for cgal's hole filling

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef typename K::Point_3 Point_3;
typedef CGAL::Polyhedron_3<K> Polyhedron;

template<class HDS, class K>
class Polyhedron_builder : public CGAL::Modifier_base<HDS> {
  typedef typename K::Point_3 Point_3;
public:
  Polyhedron_builder(std::vector<boost::tuple<int, int, int> >* triangles,
    std::vector<Point_3>* polyline)
    : triangles(triangles), polyline(polyline)
  { }

  void operator()(HDS& hds) {
    CGAL::Polyhedron_incremental_builder_3<HDS> B(hds, true);
    B.begin_surface(polyline->size() -1, triangles->size());

    for(typename std::vector<Point_3>::iterator it = polyline->begin();
      it != --polyline->end(); ++it) {
        B.add_vertex(*it);
    }

    for(typename std::vector<boost::tuple<int, int, int> >::iterator it = triangles->begin();
      it != triangles->end(); ++it) {
        B.begin_facet();
        B.add_vertex_to_facet(it->get<0>());
        B.add_vertex_to_facet(it->get<1>());
        B.add_vertex_to_facet(it->get<2>());
        B.end_facet();
    }

    B.end_surface();
  }

private:
  std::vector<boost::tuple<int, int, int> >* triangles;
  std::vector<Point_3>* polyline;
};

void check_triangles(std::vector<Point_3>& points, std::vector<boost::tuple<int, int, int> >& tris) {
  if(points.size() - 3 != tris.size()) {
    std::cerr << "  Error: there should be n-2 triangles in generated patch." << std::endl;
    assert(false);
  }

  const int max_index = static_cast<int>(points.size())-1;
  for(std::vector<boost::tuple<int, int, int> >::iterator it = tris.begin(); it != tris.end(); ++it) {
    if(it->get<0>() == it->get<1>() ||
      it->get<0>() == it->get<2>() ||
      it->get<1>() == it->get<2>() )
    {
      std::cerr << "Error: indices of triangles should be all different." << std::endl;
      assert(false);
    }

    if(it->get<0>() >= max_index ||
      it->get<1>() >= max_index ||
      it->get<2>() >= max_index )
    {
      std::cerr << "  Error: max possible index check failed." << std::endl;
      assert(false);
    }
  }
}

void check_constructed_polyhedron(const char* file_name,
  std::vector<boost::tuple<int, int, int> >* triangles,
  std::vector<Point_3>* polyline,
  const bool save_poly)
{
  Polyhedron poly;
  Polyhedron_builder<typename Polyhedron::HalfedgeDS,K> patch_builder(triangles, polyline);
  poly.delegate(patch_builder);

  if(!poly.is_valid()) {
    std::cerr << "  Error: constructed patch does not constitute a valid polyhedron." << std::endl;
    assert(false);
  }

  if (!save_poly)
    return;

  std::string out_file_name;
  out_file_name.append(file_name).append(".off");
  std::ofstream out(out_file_name.c_str());
  out << poly; out.close();
}

////////////////

typedef CGAL::Exact_predicates_inexact_constructions_kernel  Epic;
//typedef Epic::Point_3 Point_3;

template<typename PointRange>
using Domain = CGAL::internal::Domain<PointRange>;


void read_polyline_boundary_and_holes(const char* file_name,
                                      std::vector<Point_3>& points_b,
                                      std::vector<Point_3>& points_h)
{
  std::ifstream stream(file_name);
  if(!stream) {assert(false);}

  for(int i =0; i < 2; ++i) {
    int count;
    if(!(stream >> count)) { assert(false); }
    while(count-- > 0) {
      Point_3 p;
      if(!(stream >> p)) { assert(false); }
      i == 0 ? points_b.push_back(p) : points_h.push_back(p);
    }
  }
}

void read_polyline_one_line(const char* file_name, std::vector<Point_3>& points) {
  std::ifstream stream(file_name);
  if(!stream) { assert(false); }

  int count;
  if(!(stream >> count)) { assert(false); }
  while(count-- > 0) {
    Point_3 p;
    if(!(stream >> p)) { assert(false); }
    points.push_back(p);
  }
}

template <typename PointRange>
void test_split_domain(PointRange boundary)
{
  std::cout << "test_split_domain" << std::endl;

  boundary.pop_back(); // take out last(=first)

  // e_D (i, k)
  const int i = 0;
  const int k = boundary.size() - 1;
  // trird vertex - on the boundary
  //const int v = 4;
  std::vector<int> inds = {4};
  typename std::vector<int>::iterator v_it = inds.begin();


  std::vector<int> g_indices(boundary.size());
  for(std::size_t i = 0; i < boundary.size(); ++i)
    g_indices[i] = i;

  Domain<PointRange> D(g_indices);
  Domain<PointRange> D1;
  Domain<PointRange> D2;

  CGAL::internal::split_domain_case_2(D, D1, D2, i, v_it, k);

  assert(D1.b_ids.size() == 5);
  assert(D2.b_ids.size() == 2);

}

template <typename PointRange>
void test_join_domains(PointRange boundary, PointRange hole)
{
  std::cout << "test_join_domains" << std::endl;

  boundary.pop_back();
  hole.pop_back();

  // e_D (i, k)
  const int i = 0;
  const int k = boundary.size() - 1;


  std::vector<int> b_indices;
  for(std::size_t i = 0; i < boundary.size(); ++i)
    b_indices.push_back(i);

  std::vector<int> h_ids;
  std::size_t n_b =  b_indices.size();
  for(std::size_t i = b_indices.size(); i < n_b + hole.size(); ++i)
    h_ids.push_back(i);

  // trird vertex - index of hole vertices
  const int v = h_ids.front();


  Domain<PointRange> domain(b_indices); //todo: overload the constructor
  domain.add_hole(h_ids);
  Domain<PointRange> new_domain1;
  Domain<PointRange> new_domain2;

  CGAL::internal::split_domain_case_1(domain, new_domain1, new_domain2, i, v, k);

  // todo: hardcode points to assert
}


template <typename PointRange>
void test_permutations(PointRange boundary, PointRange hole)
{
  std::cout << "test_permutations" << std::endl;

  boundary.pop_back();
  hole.pop_back();

  Domain<PointRange> domain(boundary); // boundary points not really needed here, just construct the object for now
  // add 3 holes

  std::vector<int> b_indices;
  for(std::size_t i = 0; i < boundary.size(); ++i)
    b_indices.push_back(i);

  std::vector<int> h_ids;
  std::size_t n_b =  b_indices.size();
  for(std::size_t i = n_b; i < n_b + hole.size(); ++i)
    h_ids.push_back(i);

  domain.add_hole(h_ids);
  domain.add_hole(h_ids);
  domain.add_hole(h_ids);

  using Phi = CGAL::internal::Phi;
  Phi partitions;
  auto holes = domain.holes_list;

  CGAL::internal::do_permutations(holes, partitions);

  // all possible combinations: divide the number of holes to 2 sets.
  assert(partitions.size() == pow(2, domain.holes_list.size()));

}



void test_single_triangle(const char* file_name)
{
  std::cout << std::endl << "--- test_single_triangle ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_one_line(file_name, points_b);

  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/single_triangle.off");
  out << mesh;
  out.close();

  assert(count == 1);

}

void test_hexagon(const char* file_name)
{
  std::cout << std::endl << "--- test_hexagon ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_one_line(file_name, points_b);

  //test_split_domain(points_b); // todo: test this or get rid of.

  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/hexagon.off");
  out << mesh;
  out.close();

  assert(count == 40); // 40 without memoization, 20 with.
}

void test_quad(const char* file_name)
{
  std::cout << std::endl << "--- test_quad ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_one_line(file_name, points_b);

  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/quad.off");
  out << mesh;
  out.close();

  assert(count == 4);
}

void test_triangle_with_triangle_island(const char* file_name)
{
  std::cout << std::endl << "--- test_triangle_with_triangle_island ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_boundary_and_holes(file_name, points_b, points_h);

  test_permutations(points_b, points_h);
  //test_join_domains(points_b, points_h);

  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/triangle_with_triangle_island.off");
  out << mesh;
  out.close();

}


void test_square_triangle(const char* file_name)
{
  std::cout << std::endl << "--- test_square_triangle ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_boundary_and_holes(file_name, points_b, points_h);

  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/square_triangle.off");
  out << mesh;
  out.close();

}


void test_non_convex(const char* file_name)
{
  std::cout << std::endl << "--- test_non_convex ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_one_line(file_name, points_b);


  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/non_convex.off");
  out << mesh;
  out.close();

  //assert(count == 292);
}

void test_triangle_quad(const char* file_name)
{
  std::cout << std::endl << "--- test_triangle_quad ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_boundary_and_holes(file_name, points_b, points_h);


  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/triangle_quad.off");

  out << mesh;
  out.close();

  //assert(count == 292);
}

void test_quad_in_quad(const char* file_name)
{
  std::cout << std::endl << "--- test_quad_in_quad ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_boundary_and_holes(file_name, points_b, points_h);


  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/quad_in_quad.off");

  out << mesh;
  out.close();

  //assert(count == 292);
}

void test_quad_quad_non_convex(const char* file_name)
{
  std::cout << std::endl << "--- test_quad_quad_non_convex ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_boundary_and_holes(file_name, points_b, points_h);


  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/quad_quad_non_convex.off");

  out << mesh;
  out.close();

  //assert(count == 292);
}


void test_non_convex_non_convex(const char* file_name)
{
  std::cout << std::endl << "--- test_non_convex_non_convex ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_boundary_and_holes(file_name, points_b, points_h);


  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/non_convex_non_convex.off");

  out << mesh;
  out.close();

  //assert(count == 292);
}

void test_triangles_zaxis(const char* file_name)
{
  std::cout << std::endl << "--- test_triangles_zaxis ---" << std::endl;
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_boundary_and_holes(file_name, points_b, points_h);


  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  std::ofstream out("data/triangles_zaxis.off");
  out.close();

  //assert(count == 292);
}


void test_both_algorithms(const char* file_name)
{
  std::cout << std::endl << "--- test_both_algorithms ---" << std::endl;
  // cgal's hole filling

  std::vector<Point_3> points; // this will contain n and +1 repeated point
  read_polyline_one_line(file_name, points);


  std::vector<boost::tuple<int, int, int> > tris;
  CGAL::Polygon_mesh_processing::triangulate_hole_polyline(
    points, std::back_inserter(tris),
    CGAL::Polygon_mesh_processing::parameters::use_delaunay_triangulation(false));

  check_triangles(points, tris);
  check_constructed_polyhedron(file_name, &tris, &points, true);
  std::cerr << "  Done!" << std::endl;


  // recursive algorithm
  std::vector<Point_3> points_b;
  std::vector<Point_3> points_h;
  read_polyline_one_line(file_name, points_b);

  CGAL::Polyhedron_3<Epic> mesh;

  std::size_t count =
  CGAL::Polygon_mesh_processing::triangulate_hole_islands(points_b, points_h, mesh);

  std::cout << "Possible triangles tested: " << count << std::endl;

  // to optimize the output file name
  std::ofstream out("data/poly-recursive.off");
  out << mesh;
  out.close();


}



int main()
{

  std::vector<std::string> input_file = {"data/triangle.polylines.txt",
                                         "data/hexagon.polylines.txt",
                                         "data/triangle-island2.polylines.txt",
                                         "data/quad.polylines.txt",
                                         "data/non-convex.polylines.txt",
                                         "data/triangles-zaxis.polylines.txt",
                                         "data/triangle_quad.polylines.txt",
                                         "data/poly5.polylines.txt",
                                         "data/square_triangle.polylines.txt",
                                         "data/quad_in_quad.polylines.txt",
                                         "data/quad_quad_non_convex.polylines.txt",
                                         "data/non_convex_non_convex.polylines.txt"};

  const char* file_name0 = input_file[0].c_str();
  const char* file_name1 = input_file[1].c_str();
  const char* file_name2 = input_file[2].c_str();
  const char* file_name3 = input_file[3].c_str();
  const char* file_name4 = input_file[4].c_str();
  // const char* file_name5 = input_file[5].c_str();
  // const char* file_name6 = input_file[6].c_str();
  // const char* file_name7 = input_file[7].c_str();
  // const char* file_name8 = input_file[8].c_str(); // square triangle
  // const char* file_name9 = input_file[9].c_str();
  // const char* file_name10 = input_file[10].c_str();
  // const char* file_name11 = input_file[11].c_str();

  // 2D holes

  //test_single_triangle(file_name0);
  //test_quad(file_name3);
  //test_hexagon(file_name1);
  test_non_convex(file_name4);


  // 2D holes with islands
  //test_triangle_with_triangle_island(file_name2);
  //test_square_triangle(file_name8);
  //test_triangle_quad(file_name6);
  //test_quad_in_quad(file_name9);
  //test_quad_quad_non_convex(file_name10);
  //test_non_convex_non_convex(file_name11);

  //3D tests
  //test_triangles_zaxis(file_name5);


  // hexagon
  //test_both_algorithms(file_name1);






  return 0;
}