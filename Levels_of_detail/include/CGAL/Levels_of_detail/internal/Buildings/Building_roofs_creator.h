// Copyright (c) 2019 INRIA Sophia-Antipolis (France).
// All rights reserved.
//
// This file is a part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0+
//
//
// Author(s)     : Dmitry Anisimov, Simon Giraudot, Pierre Alliez, Florent Lafarge, and Andreas Fabri
//

#ifndef CGAL_LEVELS_OF_DETAIL_INTERNAL_ROOFS_CREATOR_H
#define CGAL_LEVELS_OF_DETAIL_INTERNAL_ROOFS_CREATOR_H

// STL includes.
#include <vector>

// CGAL includes.
#include <CGAL/assertions.h>

// Internal includes.
#include <CGAL/Levels_of_detail/internal/utils.h>
#include <CGAL/Levels_of_detail/internal/struct.h>

// Spatial search.
#include <CGAL/Levels_of_detail/internal/Spatial_search/Sphere_neighbor_query.h>

// Shape detection.
#include <CGAL/Levels_of_detail/internal/Shape_detection/Region_growing.h>
#include <CGAL/Levels_of_detail/internal/Shape_detection/Estimate_normals_3.h>
#include <CGAL/Levels_of_detail/internal/Shape_detection/Least_squares_plane_fit_region.h>
#include <CGAL/Levels_of_detail/internal/Shape_detection/Least_squares_plane_fit_sorting.h>

namespace CGAL {
namespace Levels_of_detail {
namespace internal {

  template<
  typename GeomTraits,
  typename PointMap3>
  class Building_roofs_creator {

  public:
    using Traits = GeomTraits;
    using Point_map_3 = PointMap3;

    using FT = typename Traits::FT;
    using Point_3 = typename Traits::Point_3;
    using Vector_3 = typename Traits::Vector_3;
    using Indices = std::vector<std::size_t>;

    using Pair_item_3 = std::pair<Point_3, Vector_3>;
    using Pair_range_3 = std::vector<Pair_item_3>;
    using First_of_pair_map = CGAL::First_of_pair_property_map<Pair_item_3>;
    using Second_of_pair_map = CGAL::Second_of_pair_property_map<Pair_item_3>;

    using Sphere_neighbor_query =
    internal::Sphere_neighbor_query<Traits, Indices, Point_map_3>;
    using Normal_estimator_3 = 
    internal::Estimate_normals_3<Traits, Indices, Point_map_3, Sphere_neighbor_query>;
    using LSPF_region = 
    internal::Least_squares_plane_fit_region<Traits, Pair_range_3, First_of_pair_map, Second_of_pair_map>;
    using LSPF_sorting =
    internal::Least_squares_plane_fit_sorting<Traits, Indices, Sphere_neighbor_query, Point_map_3>;
    using Region_growing_3 = 
    internal::Region_growing<Indices, Sphere_neighbor_query, LSPF_region, typename LSPF_sorting::Seed_map>;

    Building_roofs_creator(
      const Point_map_3 point_map_3) :
    m_point_map_3(point_map_3)
    { }

    void create_cluster(
      const Indices& input,
      const FT region_growing_scale_3,
      const FT region_growing_angle_3,
      Indices& cluster) const {

      // Compute normals.
      std::vector<Vector_3> normals;
      Sphere_neighbor_query neighbor_query(
        input, region_growing_scale_3, m_point_map_3);
      Normal_estimator_3 estimator(
        input, neighbor_query, m_point_map_3);
      estimator.get_normals(normals);
      CGAL_assertion(normals.size() == input.size());

      // Remove vertical points.
      cluster.clear();
      const Vector_3 ref = Vector_3(FT(0), FT(0), FT(1));
      for (std::size_t i = 0; i < input.size(); ++i) {
        
        const auto& vec = normals[i];
        FT angle = angle_3d(vec, ref);
        if (angle > FT(90)) angle = FT(180) - angle;
        angle = FT(90) - angle;
        if (angle > region_growing_angle_3) 
          cluster.push_back(input[i]);
      }
    }

    void create_roof_regions(
      const Indices& cluster,
      const FT region_growing_scale_3,
      const FT region_growing_noise_level_3,
      const FT region_growing_angle_3,
      const FT region_growing_min_area_3,
      const FT region_growing_distance_to_line_3,
      const FT alpha_shape_size_2,
      std::vector<Indices>& roofs) const {
        
      roofs.clear();
      Sphere_neighbor_query neighbor_query(
        cluster, region_growing_scale_3, m_point_map_3);

      std::vector<Vector_3> normals;
      Normal_estimator_3 estimator(
        cluster, neighbor_query, m_point_map_3);
      estimator.get_normals(normals);

      CGAL_assertion(cluster.size() == normals.size());
      Pair_range_3 range;
      range.reserve(cluster.size());
      for (std::size_t i = 0; i < cluster.size(); ++i) {
        const Point_3& p = get(m_point_map_3, cluster[i]);
        range.push_back(std::make_pair(p, normals[i]));
      }

      First_of_pair_map point_map;
      Second_of_pair_map normal_map;
      LSPF_region region(
        range, 
        region_growing_noise_level_3,
        region_growing_angle_3,
        region_growing_min_area_3,
        region_growing_distance_to_line_3,
        alpha_shape_size_2,
        point_map,
        normal_map);

      LSPF_sorting sorting(
        cluster, neighbor_query, m_point_map_3);
      sorting.sort();

      Region_growing_3 region_growing(
        cluster,
        neighbor_query,
        region,
        sorting.seed_map());
      region_growing.detect(std::back_inserter(roofs));
    }

  private:
    const Point_map_3 m_point_map_3;
  };

} // internal
} // Levels_of_detail
} // CGAL

#endif // CGAL_LEVELS_OF_DETAIL_INTERNAL_ROOFS_CREATOR_H