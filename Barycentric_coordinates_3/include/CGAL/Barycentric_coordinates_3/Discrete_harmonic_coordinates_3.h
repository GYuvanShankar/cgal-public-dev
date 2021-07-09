// Copyright (c) 2021 GeometryFactory SARL (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
//
// Author(s)     : Antonio Gomes, Dmitry Anisimov
//

#ifndef CGAL_BARYCENTRIC_DISCRETE_HARMONIC_COORDINATES_3_H
#define CGAL_BARYCENTRIC_DISCRETE_HARMONIC_COORDINATES_3_H

// #include <CGAL/license/Barycentric_coordinates_3.h>

// Internal includes.
#include <CGAL/Barycentric_coordinates_3/internal/utils_3.h>
#include <CGAL/Barycentric_coordinates_3/barycentric_enum_3.h>

namespace CGAL {
namespace Barycentric_coordinates {

  template<
  typename PolygonMesh,
  typename GeomTraits,
  typename VertexToPointMap = typename property_map_selector<PolygonMesh,
    CGAL::vertex_point_t>::const_type>
  class Discrete_harmonic_coordinates_3 {

  public:
    using Polygon_mesh = PolygonMesh;
    using Geom_Traits = GeomTraits;
    using Vertex_to_point_map = VertexToPointMap;

    using Construct_vec_3 = typename GeomTraits::Construct_vector_3;
    using Cross_3 = typename GeomTraits::Construct_cross_product_vector_3;
    using Dot_3 = typename GeomTraits::Compute_scalar_product_3;
    using Sqrt = typename internal::Get_sqrt<GeomTraits>::Sqrt;

	  typedef typename GeomTraits::FT FT;
    typedef typename GeomTraits::Point_3 Point_3;
    typedef typename GeomTraits::Vector_3 Vector_3;

    Discrete_harmonic_coordinates_3(
      const PolygonMesh& polygon_mesh,
      const Computation_policy_3 policy,
      const VertexToPointMap vertex_to_point_map,
      const GeomTraits traits = GeomTraits()) :
    m_polygon_mesh(polygon_mesh),
    m_computation_policy(policy),
    m_vertex_to_point_map(vertex_to_point_map),
    m_traits(traits),
    m_construct_vector_3(m_traits.construct_vector_3_object()),
    m_cross_3(m_traits.construct_cross_product_vector_3_object()),
    m_dot_3(m_traits.compute_scalar_product_3_object()),
    sqrt(internal::Get_sqrt<GeomTraits>::sqrt_object(m_traits)){

      // Check if polyhedron is strongly convex
      CGAL_assertion(is_strongly_convex_3(m_polygon_mesh, m_traits));
      m_weights.resize(vertices(m_polygon_mesh).size());
    }

    Discrete_harmonic_coordinates_3(
      const PolygonMesh& polygon_mesh,
      const Computation_policy_3 policy =
      Computation_policy_3::DEFAULT,
      const GeomTraits traits = GeomTraits()) :
    Discrete_harmonic_coordinates_3(
      polygon_mesh,
      policy,
      get_const_property_map(CGAL::vertex_point, polygon_mesh),
      traits) { }

    template<typename OutIterator>
    OutIterator operator()(const Point_3& query, OutIterator c_begin) {
      return compute(query, c_begin);
    }

    VertexToPointMap get_vertex_to_point_map(){
      return m_vertex_to_point_map;
    }

  private:
    const PolygonMesh& m_polygon_mesh;
  	const Computation_policy_3 m_computation_policy;
  	const VertexToPointMap m_vertex_to_point_map; // use it to map vertex to Point_3
  	const GeomTraits m_traits;

    const Construct_vec_3 m_construct_vector_3;
    const Cross_3 m_cross_3;
    const Dot_3 m_dot_3;
    const Sqrt sqrt;

  	std::vector<FT> m_weights;

  	template<typename OutputIterator>
    OutputIterator compute(
      const Point_3& query, OutputIterator coordinates) {

      // Compute weights.
      const FT sum = compute_weights(query);
      CGAL_assertion(sum != FT(0));

      // The coordinates must be saved in the same order as vertices in the vertex range.
      const auto vd = vertices(m_polygon_mesh);
      CGAL_assertion(m_weights.size() == vd.size());

      for (std::size_t vi = 0; vi < vd.size(); vi++) {

        CGAL_assertion(vi < m_weights.size());
        const FT coordinate = m_weights[vi]/sum;
        *(coordinates++) = coordinate;
      }

      return coordinates;
    }

    FT compute_weights(const Point_3& query) {

      // Sum of weights to normalize them later.
      FT sum = FT(0);

	    // Vertex index.
      std::size_t vi = 0;
      const auto vd = vertices(m_polygon_mesh);
      for (const auto& vertex : vd) {

        // Call function to calculate coordinates
        const FT weight = compute_dh_vertex_query(vertex, query);

    	  CGAL_assertion(vi < m_weights.size());
    	  m_weights[vi] = weight;
    	  sum += weight;
    	  ++vi; // update vi
      }

      CGAL_assertion(sum != FT(0));
      return sum;
    }

    // Compute wp coordinates for a given vertex v and a query q
    template<typename Vertex>
    FT compute_dh_vertex_query(const Vertex& vertex, const Point_3& query){

      // Circulator of faces around the vertex
      CGAL::Face_around_target_circulator<Polygon_mesh>
      face_circulator(halfedge(vertex, m_polygon_mesh), m_polygon_mesh);

      CGAL::Face_around_target_circulator<Polygon_mesh>
      face_done(face_circulator);

      // Compute weight w_v
      FT weight = FT(0);

      // Iterate using the circulator
      do{

        //Vertices around face iterator
        const auto hedge = halfedge(*face_circulator, m_polygon_mesh);
        const auto vertices = vertices_around_face(hedge, m_polygon_mesh);
        auto vertex_itr = vertices.begin();
        CGAL_precondition(vertices.size() == 3);

        int vertex_parity = 1;
        std::vector<Point_3> points;
        points.resize(2);
        int point_count = 0;

        for(std::size_t i = 0; i < 3; i++){

          if(*vertex_itr!=vertex){

            points[point_count] = get(m_vertex_to_point_map, *vertex_itr);
            point_count++;
          }
          else
            vertex_parity *= (i & 1)? -1 : 1;

          vertex_itr++;
        }

        const Point_3& point2 = points[0];
        const Point_3& point1 = points[1];

        const Vector_3 opposite_edge = m_construct_vector_3(point2, point1);
        const FT edge_length = sqrt(opposite_edge.squared_length());

        const Vector_3 normal_query = vertex_parity * m_cross_3(m_construct_vector_3(query, point2),
         m_construct_vector_3(query, point1));

        const FT cot_dihedral = internal::cot_dihedral_angle(
          internal::get_face_normal(
            *face_circulator, m_vertex_to_point_map, m_polygon_mesh, m_traits),
            normal_query, m_traits);

        weight += (cot_dihedral * edge_length) / 2;
        face_circulator++;

      }while(face_circulator!=face_done);

      return weight;
    }

  };

  template<
  typename Point_3,
  typename Mesh,
  typename OutIterator>
  OutIterator discrete_harmonic_coordinates_3(
    const Mesh& surface_mesh,
    const Point_3& query,
    OutIterator c_begin,
    const Computation_policy_3 policy =
    Computation_policy_3::DEFAULT) {

    using Geom_Traits = typename Kernel_traits<Point_3>::Kernel;

    Discrete_harmonic_coordinates_3<Mesh, Geom_Traits> discrete_harmonic(surface_mesh, policy);
    return discrete_harmonic(query, c_begin);
  }

} // namespace Barycentric_coordinates
} // namespace CGAL

#endif // CGAL_BARYCENTRIC_DISCRETE_HARMONIC_COORDINATES_3_H
