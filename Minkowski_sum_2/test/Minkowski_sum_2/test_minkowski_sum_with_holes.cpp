#include <CGAL/basic.h>

#include <CGAL/minkowski_sum_2.h>
#include <CGAL/Polygon_vertical_decomposition_2.h>
#include <CGAL/Polygon_triangulation_decomposition_2.h>
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Small_side_angle_bisector_decomposition_2.h>

#include "read_polygon.h"

#include <string.h>
#include <list>
#include <boost/timer.hpp>

typedef CGAL::Exact_predicates_exact_constructions_kernel Kernel;
typedef CGAL::Polygon_2<Kernel> Polygon_2;
typedef CGAL::Polygon_with_holes_2<Kernel> Polygon_with_holes_2;

bool are_equal(const Polygon_with_holes_2& ph1,
               const Polygon_with_holes_2& ph2)
{
  std::list<Polygon_with_holes_2> sym_diff;
  CGAL::symmetric_difference(ph1, ph2, std::back_inserter(sym_diff));
  return sym_diff.empty();
}

typedef enum {
  REDUCED_CONVOLUTION,
  VERTICAL_DECOMP,
  TRIANGULATION_DECOMP,
  VERTICAL_AND_ANGLE_BISECTOR_DECOMP,
  TRIANGULATION_AND_ANGLE_BISECTOR_DECOMP
} Strategy;

static const char* strategy_names[] = {
  "reduced convolution",
  "vertical decomposition",
  "constrained triangulation decomposition",
  "vertical and angle bisector decomposition",
  "constrained triangulation and angle bisector decomposition"
};

Polygon_with_holes_2 compute_minkowski_sum_2(Polygon_with_holes_2& p,
                                             Polygon_with_holes_2& q,
                                             Strategy strategy)
{
  switch (strategy) {
   case REDUCED_CONVOLUTION:
    {
     return CGAL::minkowski_sum_reduced_convolution_2(p, q);
    }
   case VERTICAL_DECOMP:
    {
     CGAL::Polygon_vertical_decomposition_2<Kernel> decomp;
     return CGAL::minkowski_sum_2(p, q, decomp);
    }
   case TRIANGULATION_DECOMP:
    {
     CGAL::Polygon_triangulation_decomposition_2<Kernel> decomp;
     return CGAL::minkowski_sum_2(p, q, decomp);
    }

   case VERTICAL_AND_ANGLE_BISECTOR_DECOMP:
    {
     typedef CGAL::Small_side_angle_bisector_decomposition_2<Kernel>
       No_hole_decomposition;
     typedef CGAL::Polygon_vertical_decomposition_2<Kernel>
       With_hole_decomposition;

     if (0 == p.number_of_holes()) {
       const Polygon_2& pnh = p.outer_boundary();
       No_hole_decomposition decomp_no_holes;
       if  (0 == q.number_of_holes()) {
         const Polygon_2& qnh = q.outer_boundary();
         return CGAL::minkowski_sum_2(pnh, qnh,
                                      decomp_no_holes, decomp_no_holes);
       }
       else {
         With_hole_decomposition decomp_with_holes;
         return
           CGAL::minkowski_sum_2(pnh, q, decomp_no_holes, decomp_with_holes);
       }
     }
     else {
       With_hole_decomposition decomp_with_holes;
       if  (0 == q.number_of_holes()) {
         const Polygon_2& qnh = q.outer_boundary();
         No_hole_decomposition decomp_no_holes;
         return
           CGAL::minkowski_sum_2(p, qnh, decomp_with_holes, decomp_no_holes);
       }
       else {
         return
           CGAL::minkowski_sum_2(p, q, decomp_with_holes, decomp_with_holes);
       }
     }
    }

   case TRIANGULATION_AND_ANGLE_BISECTOR_DECOMP:
    {
     typedef CGAL::Small_side_angle_bisector_decomposition_2<Kernel>
       No_hole_decomposition;
     typedef CGAL::Polygon_triangulation_decomposition_2<Kernel>
       With_hole_decomposition;
     if (0 == p.number_of_holes()) {
       const Polygon_2& pnh = p.outer_boundary();
       No_hole_decomposition decomp_no_holes;
       if  (0 == q.number_of_holes()) {
         const Polygon_2& qnh = q.outer_boundary();
         return CGAL::minkowski_sum_2(pnh, qnh,
                                      decomp_no_holes, decomp_no_holes);
       }
       else {
         With_hole_decomposition decomp_with_holes;
         return
           CGAL::minkowski_sum_2(pnh, q, decomp_no_holes, decomp_with_holes);
       }
     }
     else {
       With_hole_decomposition decomp_with_holes;
       if  (0 == q.number_of_holes()) {
         const Polygon_2& qnh = q.outer_boundary();
         No_hole_decomposition decomp_no_holes;
         return
           CGAL::minkowski_sum_2(p, qnh, decomp_with_holes, decomp_no_holes);
       }
       else {
         return
           CGAL::minkowski_sum_2(p, q, decomp_with_holes, decomp_with_holes);
       }
     }
    }

   default:
    std::cerr << "Invalid strategy" << std::endl;
    return Polygon_with_holes_2();
  }
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " [method flag] [polygon files]..."
              << std::endl;
    std::cerr << "For the method flag, use a subset of the letters 'rfsohg'."
              << std::endl;
    std::cerr << "The program will compute the Minkowski sum of the first "
              << "and second polygon, of the third and fourth, and so on."
              << std::endl;
    return 1;
  }

  Polygon_with_holes_2 p, q;
  boost::timer timer;

  std::list<Strategy> strategies;
  for (std::size_t i = 0; i < strlen(argv[1]); ++i) {
    switch (argv[1][i]) {
      case 'r':
        strategies.push_back(REDUCED_CONVOLUTION);
        break;

      case 'v':
        strategies.push_back(VERTICAL_DECOMP);
        break;

      case 't':
        strategies.push_back(TRIANGULATION_DECOMP);
        break;

      case 'w':
        strategies.push_back(VERTICAL_AND_ANGLE_BISECTOR_DECOMP);
        break;

      case 'u':
        strategies.push_back(TRIANGULATION_AND_ANGLE_BISECTOR_DECOMP);
        break;

     default:
        std::cerr << "Unknown flag '" << argv[1][i] << "'" << std::endl;
        return 1;
    }
  }

  int i = 2;
  while (i+1 < argc) {
    std::cout << "Testing " << argv[i] << " + " << argv[i+1] << std::endl;
    if (!read_polygon(argv[i], p)) return -1;
    if (!read_polygon(argv[i+1], q)) return -1;

    bool compare = false;
    Polygon_with_holes_2 reference;
    std::list<Strategy>::iterator it;
    for (it = strategies.begin(); it != strategies.end(); ++it) {
      std::cout << "Using " << strategy_names[*it] << ": ";
      timer.restart();
      Polygon_with_holes_2 result = compute_minkowski_sum_2(p, q, *it);
      double secs = timer.elapsed();
      std::cout << secs << " s " << std::flush;

      if (compare) {
        if (are_equal(reference, result)) std::cout << "(OK)";
        else {
          std::cout << "(ERROR: different result)";
          return 1;
        }
      }
      else {
        compare = true;
        reference = result;

        std::size_t n = result.outer_boundary().size();
        Polygon_with_holes_2::Hole_const_iterator it = result.holes_begin();
        while(it != result.holes_end()) n += (*it++).size();

        std::cout << std::endl << "Result has " << n << " vertices and "
                  << result.number_of_holes() << " holes.";
      }
      std::cout << std::endl;
    }

    std::cout << std::endl;
    i += 2;
  }

  return 0;
}
