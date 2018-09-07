#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Heat_method_3/Heat_method_3.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <iostream>
#include <string>

typedef CGAL::Simple_cartesian<double>                       Kernel;
typedef Kernel::Point_3                                      Point;
typedef Kernel::Point_2                                      Point_2;
typedef CGAL::Surface_mesh<Point>                            Surface_mesh;

typedef boost::graph_traits<Surface_mesh>::vertex_descriptor vertex_descriptor;
typedef Surface_mesh::Property_map<vertex_descriptor,double> Vertex_distance_map;
typedef CGAL::Heat_method_3::Heat_method_3<Surface_mesh,Kernel, Vertex_distance_map> Heat_method;



int main(int argc, char* argv[])
{
  //read in mesh
  Surface_mesh sm;
  const char* filename = (argc > 1) ? argv[1] : "./data/bunny.off";
  std::ifstream in(filename);
  in >> sm;
  //the heat intensity will hold the distance values from the source set
  Vertex_distance_map heat_intensity = sm.add_property_map<vertex_descriptor, double>("v:heat_intensity", 0).first;
  Heat_method hm(sm, heat_intensity);

  //add the first vertex as the source set
  vertex_descriptor source = *(vertices(sm).first);
  hm.add_source(source);
  hm.update();

  BOOST_FOREACH(vertex_descriptor vd , vertices(sm)){
    std::cout << vd << "  is at distance " << get(heat_intensity, vd) << " from " << source << std::endl;
  }

  return 0;
}
