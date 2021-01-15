#include <CGAL/internal/disable_deprecation_warnings_and_errors.h>

#include <CGAL/Surface_mesh.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/boost/graph/generators.h>

#include <CGAL/boost/graph/IO/OFF.h>
#include <CGAL/boost/graph/IO/VTK.h>
typedef CGAL::Simple_cartesian<double>         Kernel;
typedef Kernel::Point_3                        Point_3;
typedef CGAL::Surface_mesh<Point_3>            SM;

int main()
{
  // OFF
  SM sm_in, sm_out;
  Point_3 p0(0,0,0), p1(1,0,0), p2(0,1,0);
  CGAL::make_triangle(p0, p1, p2, sm_out);
  bool ok = CGAL::write_off("tmp.off", sm_out, CGAL::parameters::all_default());
  assert(ok);
  ok = CGAL::read_off("tmp.off", sm_in, CGAL::parameters::all_default());
  assert(ok);
  assert(num_vertices(sm_in) == 3 && num_faces(sm_in) == 1);
  sm_in.clear();

  std::ofstream os("tmp.off");
  ok = CGAL::write_off(os, sm_out, CGAL::parameters::all_default());
  assert(ok);
  os.close();
  std::ifstream is("tmp.off");
  ok = CGAL::read_off(is, sm_in, CGAL::parameters::all_default());
  assert(ok);
  assert(num_vertices(sm_in) == 3 && num_faces(sm_in) == 1);
  is.close();
  sm_in.clear();

  //vtk
  os.open("tmp.vtp");
  ok = CGAL::write_vtp(os, sm_out, CGAL::parameters::all_default());
  assert(ok);
  os.close();

  ok = CGAL::read_VTP("tmp.vtp", sm_in, CGAL::parameters::all_default());
  assert(ok);
  assert(num_vertices(sm_in) == 3 && num_faces(sm_in) == 1);
  sm_in.clear();

  return EXIT_SUCCESS;
}
