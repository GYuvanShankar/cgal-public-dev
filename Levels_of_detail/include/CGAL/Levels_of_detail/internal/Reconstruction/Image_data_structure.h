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

#ifndef CGAL_LEVELS_OF_DETAIL_INTERNAL_IMAGE_DATA_STRUCTURE_H
#define CGAL_LEVELS_OF_DETAIL_INTERNAL_IMAGE_DATA_STRUCTURE_H

// STL includes.
#include <map>
#include <set>
#include <list>
#include <queue>
#include <vector>
#include <utility>
#include <memory>

// CGAL includes.
#include <CGAL/enum.h>
#include <CGAL/property_map.h>

// Internal includes.
#include <CGAL/Levels_of_detail/internal/utils.h>
#include <CGAL/Levels_of_detail/internal/struct.h>

// Other includes.
#include <CGAL/Levels_of_detail/internal/Reconstruction/Image.h>
#include <CGAL/Levels_of_detail/internal/Shape_detection/Region_growing.h>
#include <CGAL/Levels_of_detail/internal/Spatial_search/K_neighbor_query.h>
#include <CGAL/Levels_of_detail/internal/Reconstruction/Image_face_simplifier.h>
#include <CGAL/Levels_of_detail/internal/Reconstruction/Image_face_simple_regularizer.h>
#include <CGAL/Levels_of_detail/internal/Reconstruction/Image_face_complex_regularizer.h>

// Testing.
#include "../../../../../test/Levels_of_detail/include/Saver.h"
#include "../../../../../test/Levels_of_detail/include/Utilities.h"

namespace CGAL {
namespace Levels_of_detail {
namespace internal {

template<typename GeomTraits>
struct Image_data_structure {

public:
  using Traits = GeomTraits;

  using FT = typename Traits::FT;
  using Point_2 = typename Traits::Point_2;
  using Point_3 = typename Traits::Point_3;
  using Segment_2 = typename Traits::Segment_2;
  using Segment_3 = typename Traits::Segment_3;
  using Line_2 = typename Traits::Line_2;
  using Line_3 = typename Traits::Line_3;
  using Vector_2 = typename Traits::Vector_2;
  using Plane_3 = typename Traits::Plane_3;
  using Triangle_2 = typename Traits::Triangle_2;
  using Triangle_3 = typename Traits::Triangle_3;
  using Intersect_2 = typename Traits::Intersect_2;
  using Intersect_3 = typename Traits::Intersect_3;
  
  using Image = internal::Image<Traits>;
  
  using My_point = typename Image::My_point;
  using Contour = typename Image::Contour;
  using Point_type = typename Image::Point_type;
  using Pixels = typename Image::Pixels;
  using Pixel_point_map = typename Image::Pixel_point_map;

  using Indices = std::vector<std::size_t>;
  using Size_pair = std::pair<std::size_t, std::size_t>;
  using Triangle_set_3 = std::vector<Triangle_3>;

  using Saver = Saver<Traits>;
  using Inserter = Polygon_inserter<Traits>;
  using Indexer = internal::Indexer<Point_3>;
  using Color = CGAL::Color;

  using K_neighbor_query =
  internal::K_neighbor_query<Traits, Pixels, Pixel_point_map>;
  using Triangulation = 
  internal::Triangulation<Traits>;
  using LF_circulator = 
  typename Triangulation::Delaunay::Line_face_circulator;

  struct Vertex {
    Point_2 point;
    std::set<std::size_t> labels;
    std::size_t index = std::size_t(-1);
    std::size_t bd_idx = std::size_t(-1);
    Point_type type = Point_type::DEFAULT;
    Indices hedges;
    Indices neighbors;
    bool used  = false;
    bool skip  = false;
    bool state = false;

    void clear() {
      labels.clear();
      point = Point_2();
      index = std::size_t(-1);
      bd_idx = std::size_t(-1);
      type = Point_type::DEFAULT;
      hedges.clear();
      neighbors.clear();
    }
  };

  enum class Edge_type {
    DEFAULT  = 0,
    BOUNDARY = 1,
    INTERNAL = 2
  };

  struct Edge {
    std::size_t index = std::size_t(-1);
    std::size_t from_vertex = std::size_t(-1);
    std::size_t to_vertex = std::size_t(-1);
    Size_pair labels = std::make_pair(std::size_t(-1), std::size_t(-1));
    Segment_2 segment;
    Edge_type type = Edge_type::DEFAULT;
    bool skip = false;
  };

  struct Halfedge {
    std::size_t index = std::size_t(-1);
    std::size_t edg_idx = std::size_t(-1);
    std::size_t next = std::size_t(-1);
    std::size_t opposite = std::size_t(-1);
    std::size_t from_vertex = std::size_t(-1);
    std::size_t to_vertex = std::size_t(-1);
  };

  enum class Face_type {
    DEFAULT = 0,
    CLOSED = 1,
    BOUNDARY = 2,
    INTERNAL = 3
  };

  struct Face {
    std::size_t index = std::size_t(-1);
    Indices hedges;
    Indices neighbors;
    std::size_t label = std::size_t(-1);
    Face_type type = Face_type::DEFAULT;
    std::set<std::size_t> probs; // label probabilities
    Triangulation tri;
    FT area = FT(0);
    bool skip = false;
  };

  class DS_neighbor_query {

  public:
    DS_neighbor_query(
      const std::vector<Vertex>& vertices,
      const std::vector<Edge>& edges,
      const std::vector<Halfedge>& halfedges) : 
    m_vertices(vertices),
    m_edges(edges),
    m_halfedges(halfedges) 
    { }

    void operator()(
      const std::size_t query_index, 
      Indices& neighbors) const {

      neighbors.clear();
      const auto& he = m_halfedges[query_index];
      if (he.next != std::size_t(-1))
        neighbors.push_back(he.next);
    }

  private:
    const std::vector<Vertex>& m_vertices;
    const std::vector<Edge>& m_edges;
    const std::vector<Halfedge>& m_halfedges;
  };

  class DS_region_type {
    
  public:
    DS_region_type(
      const std::vector<Vertex>& vertices,
      const std::vector<Edge>& edges,
      const std::vector<Halfedge>& halfedges) : 
    m_vertices(vertices),
    m_edges(edges),
    m_halfedges(halfedges) 
    { }

    bool is_already_visited(
      const std::size_t, const std::size_t, const bool) const { 
      return false;
    }

    bool is_part_of_region(
      const std::size_t, const std::size_t, 
      const Indices&) const {
      return true;
    }

    bool is_valid_region(const Indices& region) const {
      return region.size() >= 3;
    }

    void update(const Indices&) {
      // skipped!
    }

  private:
    const std::vector<Vertex>& m_vertices;
    const std::vector<Edge>& m_edges;
    const std::vector<Halfedge>& m_halfedges;
  };

  using Region_growing = internal::Region_growing<
    std::vector<Halfedge>, DS_neighbor_query, DS_region_type>;

  Image_data_structure(
    const std::vector<Segment_2>& boundary,
    const std::vector<Image>& ridges,
    const Image& image,
    const std::map<std::size_t, Plane_3>& plane_map,
    const FT noise_level_2,
    const FT min_length_2,
    const FT angle_bound_2,
    const FT ordinate_bound_2) :
  m_boundary(boundary),
  m_ridges(ridges),
  m_image(image),
  m_plane_map(plane_map),
  m_noise_level_2(noise_level_2),
  m_min_length_2(min_length_2),
  m_angle_bound_2(angle_bound_2),
  m_ordinate_bound_2(ordinate_bound_2),
  m_pi(static_cast<FT>(CGAL_PI)),
  m_knq(
    m_image.pixels, 
    FT(24), 
    m_image.pixel_point_map)
  { }

  void build() {

    initialize_vertices();
    initialize_edges();
    initialize_faces();

    CGAL_assertion(
      m_halfedges.size() == m_edges.size() * 2);

    /*
    std::cout.precision(30);
    std::cout << "num vertices: " << m_vertices.size() << std::endl;
    for (const auto& vertex : m_vertices) {
      if (
        vertex.type != Point_type::FREE && 
        vertex.type != Point_type::LINEAR) {

        if (vertex.type != Point_type::CORNER) 
          continue;
        
        std::cout << 
        int(vertex.type) << " : " << 
        vertex.bd_idx << " , " <<
        vertex.labels.size() << " , " << 
        vertex.neighbors.size() << " , " <<
        vertex.hedges.size() << std::endl;
      }
    } */

    /*
    std::cout << "num edges: " << m_edges.size() << std::endl;
    std::cout << "num halfedges: " << m_halfedges.size() << std::endl;
    for (const auto& edge : m_edges) {
      std::cout << 
        edge.labels.first << " " << 
        edge.labels.second << std::endl;
    } */

    /*
    std::cout << "num edges: " << m_edges.size() << std::endl;
    std::cout << "num halfedges: " << m_halfedges.size() << std::endl;
    for (const auto& he : m_halfedges) {
      std::cout <<
      he.from_vertex << " " << he.to_vertex << " : " <<  
      int(m_vertices[he.from_vertex].type) << " " <<
      int(m_vertices[he.to_vertex].type) << " : " <<
      he.edg_idx << " " << 
      he.index << " " << he.opposite << std::endl;
    } */

    std::cout << "num faces: " << m_faces.size() << std::endl;
    for (const auto& face : m_faces) {
      std::cout <<
      int(face.type) << " : " <<
      face.label << " , " <<
      face.probs.size() << " , " <<
      face.neighbors.size() << " , " <<
      face.hedges.size() << std::endl;
    }
    
    save_faces_polylines("initial");
    save_faces_ply("initial");
  }

  void simplify() {
    default_vertex_states();

    using Image_face_simplifier = internal::Image_face_simplifier<
      Traits, Vertex, Edge, Halfedge, Face>;
    Image_face_simplifier image_face_simplifier(
      m_boundary,
      m_vertices, m_edges, m_halfedges, 
      m_image, m_plane_map, 
      m_noise_level_2);

    for (const auto& face : m_faces) {
      if (face.skip) continue;
      image_face_simplifier.simplify_face(face);
    }

    make_skip();
    default_vertex_states();
    for (auto& face : m_faces)
      update_face(face);
    save_faces_polylines("simplified");
    save_faces_ply("simplified");
  }

  void regularize() {
    regularize_simple();
  }

  void regularize_simple() {
    default_vertex_states();

    using Image_face_regularizer = internal::Image_face_simple_regularizer<
      Traits, Vertex, Edge, Halfedge, Face, Edge_type>;
    Image_face_regularizer image_face_regularizer(
      m_boundary,
      m_vertices, m_edges, m_halfedges, 
      m_image, m_plane_map, 
      m_min_length_2, m_angle_bound_2, m_ordinate_bound_2);

    for (auto& face : m_faces) {
      if (face.skip) continue;
      image_face_regularizer.compute_multiple_directions(face);
      image_face_regularizer.regularize_face(face);
    }

    make_skip();
    default_vertex_states();
    for (auto& face : m_faces)
      update_face(face);
    save_faces_polylines("regularized");
    save_faces_ply("regularized");
  }

  void regularize_complex() {
    default_vertex_states();

    using Image_face_regularizer = internal::Image_face_complex_regularizer<
      Traits, Vertex, Edge, Halfedge, Face, Edge_type>;
    Image_face_regularizer image_face_regularizer(
      m_boundary,
      m_vertices, m_edges, m_halfedges, 
      m_image, m_plane_map, 
      m_min_length_2, m_angle_bound_2, m_ordinate_bound_2);

    for (auto& face : m_faces) {
      if (face.skip) continue;
      image_face_regularizer.compute_multiple_directions(face);
      image_face_regularizer.regularize_face(face);
    }

    /* make_skip(); */
    default_vertex_states();
    for (auto& face : m_faces)
      update_face(face);
    save_faces_polylines("regularized");
    save_faces_ply("regularized");
  }

  void default_vertex_states() {
    for (auto& vertex : m_vertices) {
      vertex.used  = false;
      vertex.state = false;
    }
  }

  void get_wall_outer_segments(
    std::vector<Segment_3>& segments_3) {

    segments_3.clear();
    std::vector<Edge> edges;

    for (const auto& face : m_faces) {
      if (face.skip) continue;
      create_face_edges(face, edges);

      const auto& plane = m_plane_map.at(face.label);
      for (const auto& edge : edges) {
        if (edge.type == Edge_type::BOUNDARY) {
          const auto& s = edge.segment.source();
          const auto& t = edge.segment.target();

          const Point_3 p = internal::position_on_plane_3(s, plane);
          const Point_3 q = internal::position_on_plane_3(t, plane);
          segments_3.push_back(Segment_3(p, q));
        }
      }
    }
  }

  void get_wall_inner_segments(
    std::vector<Segment_3>& segments_3) {

    segments_3.clear();
    std::vector<Edge> edges;

    for (const auto& face : m_faces) {
      if (face.skip) continue;
      create_face_edges(face, edges);

      const auto& plane = m_plane_map.at(face.label);
      for (const auto& edge : edges) {
        if (edge.type == Edge_type::INTERNAL) {
          const auto& s = edge.segment.source();
          const auto& t = edge.segment.target();

          const Point_3 p = internal::position_on_plane_3(s, plane);
          const Point_3 q = internal::position_on_plane_3(t, plane);
          segments_3.push_back(Segment_3(p, q));
        }
      }
    }
  }

  void get_roof_triangles(
    std::vector<Triangle_set_3>& triangle_sets_3) {

    triangle_sets_3.clear();
    for (const auto& face : m_faces) {
      if (face.skip) continue;
      
      Triangle_set_3 triangle_set_3;
      const auto& plane = m_plane_map.at(face.label);
      const auto& tri = face.tri.delaunay;

      for (auto fh = tri.finite_faces_begin();
      fh != tri.finite_faces_end(); ++fh) {
        if (!fh->info().interior) continue;

        const auto& p0 = fh->vertex(0)->point();
        const auto& p1 = fh->vertex(1)->point();
        const auto& p2 = fh->vertex(2)->point();

        const Point_3 q0 = internal::position_on_plane_3(p0, plane);
        const Point_3 q1 = internal::position_on_plane_3(p1, plane);
        const Point_3 q2 = internal::position_on_plane_3(p2, plane);

        triangle_set_3.push_back(Triangle_3(q0, q1, q2));
      }
      triangle_sets_3.push_back(triangle_set_3);
    }
  }

  void clear() {
    m_vertices.clear();
    m_edges.clear();
    m_faces.clear();
    m_halfedges.clear();
  }

private:
  const std::vector<Segment_2>& m_boundary;
  const std::vector<Image>& m_ridges;
  const Image& m_image;
  const std::map<std::size_t, Plane_3>& m_plane_map;
  const FT m_noise_level_2;
  const FT m_min_length_2;
  const FT m_angle_bound_2;
  const FT m_ordinate_bound_2;
  const FT m_pi;

  K_neighbor_query m_knq;

  std::vector<Vertex>   m_vertices;
  std::vector<Edge>     m_edges;
  std::vector<Halfedge> m_halfedges;
  std::vector<Face>     m_faces;

  std::map<Point_2, std::size_t> m_vertex_map;
  std::map<Size_pair, Halfedge> m_halfedge_map;

  void make_skip() {
    for (auto& vertex : m_vertices)
      if (!vertex.skip) 
        vertex.skip = vertex.state;
  }

  void initialize_vertices() {

    m_vertices.clear();
    for (const auto& ridge : m_ridges) {
      for (const auto& contour : ridge.contours) {
        if (contour.is_degenerated) continue;

        if (contour.is_closed) 
          insert_vertices_closed(contour);
        else
          insert_vertices_open(contour);
      }
    }
    insert_vertices_boundary();
    clean_vertices();
    initialize_vertex_neighbors();
  }

  void clean_vertices() {

    std::size_t count = 0;
    std::vector<Vertex> clean;

    m_vertex_map.clear();
    for (std::size_t i = 0; i < m_vertices.size(); ++i) {
      auto& vertex = m_vertices[i];

      auto it = m_vertex_map.find(vertex.point);
      if (it == m_vertex_map.end()) {
        vertex.index = count; ++count;
        clean.push_back(vertex);
        m_vertex_map[vertex.point] = vertex.index;
      } else {
        auto& prev = clean[it->second];
        prev.type = vertex.type;
        prev.bd_idx = vertex.bd_idx;
        for (const std::size_t label : vertex.labels)
          prev.labels.insert(label); 
      }
    }
    m_vertices = clean;
  }

  void insert_vertices_closed(
    const Contour& contour) {

    Vertex vertex;
    const auto& items = contour.points;
    const std::size_t m = items.size() - 1;

    for (std::size_t i = 0; i < m; ++i) {
      const auto& item = items[i];
      const auto& point = item.point();
      
      vertex.clear();
      vertex.point = point;
      vertex.labels = item.labels;
      vertex.type = item.end_type;
      m_vertices.push_back(vertex);
    }
  }

  void insert_vertices_open(
    const Contour& contour) {

    Vertex vertex;
    const auto& items = contour.points;
    const std::size_t m = items.size();
    CGAL_assertion(m >= 2);

    insert_corner_vertex(items[0]);
    for (std::size_t i = 1; i < m - 1; ++i) {
      const auto& item = items[i];
      const auto& point = item.point();

      vertex.clear();
      vertex.point = point;
      vertex.labels = item.labels;
      vertex.type = item.end_type;
      m_vertices.push_back(vertex);
    }
    insert_corner_vertex(items[m - 1]);
  }

  void insert_corner_vertex(const My_point& query) {

    for (auto& vertex : m_vertices) {
      if (internal::are_equal_points_2(vertex.point, query.point())) {
        for (const std::size_t label : query.labels)
          vertex.labels.insert(label);
        return;
      }
    }

    const bool is_boundary = ( query.end_type == Point_type::BOUNDARY );

    Vertex vertex;
    vertex.point = query.point();
    vertex.labels = query.labels;
    vertex.type = is_boundary ? Point_type::OUTER_BOUNDARY : Point_type::CORNER;
    vertex.bd_idx = is_boundary ? query.bd_idx : std::size_t(-1);
    m_vertices.push_back(vertex);
  }

  void insert_vertices_boundary() {

    Vertex vertex;
    for (std::size_t i = 0; i < m_boundary.size(); ++i) {
      const auto& segment = m_boundary[i];
      const auto& point = segment.source();

      for (auto& vertex : m_vertices) {
        if (internal::are_equal_points_2(vertex.point, point)) {

          vertex.type = Point_type::OUTER_CORNER;
          vertex.bd_idx = i;
          continue;
        }
      }

      vertex.clear();
      vertex.point = point;
      vertex.type = Point_type::OUTER_CORNER;
      vertex.bd_idx = i;
      m_vertices.push_back(vertex);
    }
  }

  void initialize_vertex_neighbors() {

    for (const auto& ridge : m_ridges) {
      for (const auto& contour : ridge.contours) {
        if (contour.is_degenerated) continue;

        if (contour.is_closed) 
          insert_vertex_neighbors_closed(contour);
        else
          insert_vertex_neighbors_open(contour);
      }
    }
    insert_vertex_neighbors_boundary();
  }

  void insert_vertex_neighbors_closed(
    const Contour& contour) {

    const auto& items = contour.points;
    const std::size_t m = items.size() - 1;

    const auto& pr0 = items[m - 1].point();
    const auto& cr0 = items[0].point();
    const auto& nx0 = items[1].point();
    auto& ns0 = m_vertices[m_vertex_map.at(cr0)].neighbors;
    ns0.push_back(m_vertex_map.at(pr0));
    ns0.push_back(m_vertex_map.at(nx0));

    for (std::size_t i = 1; i < m - 1; ++i) {
      const std::size_t im = i - 1;
      const std::size_t ip = i + 1;

      const auto& prev = items[im].point();
      const auto& curr = items[i].point();
      const auto& next = items[ip].point();
      auto& neighbors = m_vertices[m_vertex_map.at(curr)].neighbors;
      neighbors.push_back(m_vertex_map.at(prev));
      neighbors.push_back(m_vertex_map.at(next));
    }

    const auto& pr1 = items[m - 2].point();
    const auto& cr1 = items[m - 1].point();
    const auto& nx1 = items[0].point();
    auto& ns1 = m_vertices[m_vertex_map.at(cr1)].neighbors;
    ns1.push_back(m_vertex_map.at(pr1));
    ns1.push_back(m_vertex_map.at(nx1));
  }

  void insert_vertex_neighbors_open(
    const Contour& contour) {

    const auto& items = contour.points;
    const std::size_t m = items.size();

    insert_corner_neighbors(items[0], items[1]);
    for (std::size_t i = 1; i < m - 1; ++i) {
      const std::size_t im = i - 1;
      const std::size_t ip = i + 1;

      const auto& prev = items[im].point();
      const auto& curr = items[i].point();
      const auto& next = items[ip].point();
      auto& neighbors = m_vertices[m_vertex_map.at(curr)].neighbors;
      neighbors.push_back(m_vertex_map.at(prev));
      neighbors.push_back(m_vertex_map.at(next));
    }
    insert_corner_neighbors(items[m - 1], items[m - 2]);
  }

  void insert_corner_neighbors(
    const My_point& query, const My_point& other) {

    const auto& curr = query.point();
    auto& neighbors = m_vertices[m_vertex_map.at(curr)].neighbors;
    neighbors.push_back(m_vertex_map.at(other.point()));
  }

  void insert_vertex_neighbors_boundary() {

    std::vector<Indices> boundary_map;
    create_boundary_map(boundary_map);

    const std::size_t m = m_boundary.size();
    for (std::size_t i = 0; i < m; ++i) {

      const auto& curr = m_boundary[i].source();
      const std::size_t curr_idx = m_vertex_map.at(curr);
      auto& neighbors = m_vertices[curr_idx].neighbors;

      const std::size_t j = find_target_index(i, curr);
      CGAL_assertion(j != std::size_t(-1));
      
      add_one_boundary_point(curr, j, boundary_map, neighbors);
      add_one_boundary_point(curr, i, boundary_map, neighbors);
    }

    for (const auto& vertex : m_vertices) {
      if (vertex.type == Point_type::OUTER_BOUNDARY) {
        const auto& curr = vertex.point;
        const std::size_t curr_idx = m_vertex_map.at(curr);
        auto& neighbors = m_vertices[curr_idx].neighbors;
        add_two_boundary_points(
          curr_idx, vertex.bd_idx, boundary_map, neighbors);
      }
    }
  }

  void create_boundary_map(
    std::vector<Indices>& boundary_map) {
    
    const std::size_t m = m_boundary.size();

    boundary_map.clear();
    boundary_map.resize(m);

    for (std::size_t i = 0; i < m; ++i) {
      const auto& curr = m_boundary[i].source();
      const std::size_t curr_idx = m_vertex_map.at(curr);

      const auto& next = m_boundary[i].target();
      const std::size_t next_idx = m_vertex_map.at(next);
      
      boundary_map[i].push_back(curr_idx);
      boundary_map[i].push_back(next_idx);
    }

    for (const auto& vertex : m_vertices) {
      if (vertex.type == Point_type::OUTER_BOUNDARY) {
        const auto& curr = vertex.point;
        const std::size_t curr_idx = m_vertex_map.at(curr);
        boundary_map[vertex.bd_idx].push_back(curr_idx);
      }
    }
  }

  std::size_t find_target_index(
    const std::size_t skip, const Point_2& source) {
    
    const std::size_t m = m_boundary.size();
    for (std::size_t i = 0; i < m; ++i) {
      if (i == skip) continue;

      const auto& target = m_boundary[i].target();
      if (internal::are_equal_points_2(source, target))
        return i;
    }
    return std::size_t(-1);
  }

  void add_one_boundary_point(
    const Point_2& query,
    const std::size_t bd_idx, 
    const std::vector<Indices>& boundary_map,
    Indices& neighbors) {

    auto ns = boundary_map[bd_idx];
    std::sort(ns.begin(), ns.end(), 
    [this, &query](const std::size_t i, const std::size_t j) -> bool { 
        const FT length_1 = internal::distance(query, m_vertices[i].point);
        const FT length_2 = internal::distance(query, m_vertices[j].point);
        return length_1 < length_2;
    });

    CGAL_assertion(ns.size() >= 2);
    neighbors.push_back(ns[1]);
  }

  void add_two_boundary_points(
    const std::size_t query_index,
    const std::size_t bd_idx, 
    const std::vector<Indices>& boundary_map,
    Indices& neighbors) {

    const auto& ref = m_boundary[bd_idx].source();
    auto ns = boundary_map[bd_idx];

    std::sort(ns.begin(), ns.end(), 
    [this, &ref](const std::size_t i, const std::size_t j) -> bool { 
        const FT length_1 = internal::distance(ref, m_vertices[i].point);
        const FT length_2 = internal::distance(ref, m_vertices[j].point);
        return length_1 < length_2;
    });

    CGAL_assertion(ns.size() >= 3);
    for (std::size_t i = 1; i < ns.size() - 1; ++i) {
      const std::size_t im = i - 1;
      const std::size_t ip = i + 1;

      if (ns[i] == query_index) {
        neighbors.push_back(ns[im]);
        neighbors.push_back(ns[ip]);
        return;
      }
    }
  }

  void initialize_edges() {
    
    create_edges_and_halfedges();
    add_boundary_labels();
    set_edge_types();

    /* sort_vertex_halfedges(); */
    /* compute_next_halfedges(); */
  }

  void create_edges_and_halfedges() {

    m_edges.clear();
    m_halfedges.clear();
    m_halfedge_map.clear();

    std::size_t he_index = 0;
    std::size_t ed_index = 0;

    for (std::size_t i = 0; i < m_vertices.size(); ++i) {
      auto& vertexi = m_vertices[i];

      for (const std::size_t j : vertexi.neighbors) {
        auto& vertexj = m_vertices[j];
        
        CGAL_assertion(i != j);
        const auto to = std::make_pair(i, j);
        const auto op = std::make_pair(j, i);

        auto it = m_halfedge_map.find(op);
        if (it != m_halfedge_map.end()) {
          auto& other = it->second;

          Halfedge he;
          he.index = he_index; ++he_index;
          he.edg_idx = other.edg_idx;
          he.opposite = other.index;
          other.opposite = he.index;
          he.from_vertex = i;
          he.to_vertex = j;
          m_halfedge_map[to] = he;
          m_halfedges[other.index].opposite = he.index;
          m_halfedges.push_back(he);
          update_edge_labels(
            vertexi, vertexj, m_edges[he.edg_idx]);

          vertexi.hedges.push_back(he.index);

        } else {

          Edge edge;
          edge.index = ed_index; ++ed_index;
          update_edge_labels(
            vertexi, vertexj, edge);
          edge.from_vertex = i;
          edge.to_vertex = j;
          edge.segment = Segment_2(vertexi.point, vertexj.point);
          m_edges.push_back(edge);

          Halfedge he;
          he.index = he_index; ++he_index;
          he.edg_idx = edge.index;
          he.from_vertex = i;
          he.to_vertex = j;
          m_halfedge_map[to] = he;
          m_halfedges.push_back(he);

          vertexi.hedges.push_back(he.index);
        }
      }
    }
    m_halfedge_map.clear();
  }

  void update_edge_labels(
    const Vertex& vertexi, const Vertex& vertexj,
    Edge& edge) {

    if (
      edge.labels.first  != std::size_t(-1) &&
      edge.labels.second != std::size_t(-1) )
    return;

    if (
      vertexi.type != Point_type::OUTER_BOUNDARY &&
      vertexi.type != Point_type::OUTER_CORNER) {
      
      if (vertexi.labels.size() <= 2) {
        auto it = vertexi.labels.begin();

        if (vertexi.labels.size() >= 1) {
          edge.labels.first = *it;
        }
        if (vertexi.labels.size() == 2) {
          ++it; edge.labels.second = *it;
        }
        
      } else if (vertexj.labels.size() <= 2) {
        auto it = vertexj.labels.begin();

        if (vertexj.labels.size() >= 1) {
          edge.labels.first = *it;
        }
        if (vertexj.labels.size() == 2) {
          ++it; edge.labels.second = *it;
        }
      } else {
        std::cout << "Error: cannot be here!" << std::endl;
      }
    }
  }

  void add_boundary_labels() {

    for (auto& edge : m_edges) {
      auto& labels = edge.labels;
      if (
        labels.first  == std::size_t(-1) && 
        labels.second == std::size_t(-1)) {
        add_boundary_label(edge);
      }
    }
  }

  void add_boundary_label(Edge& edge) {
    
    const auto& s = edge.segment.source();
    const auto& t = edge.segment.target();
    const auto  m = internal::middle_point_2(s, t);
    set_labels(m, edge);
  }

  void set_labels(
    const Point_2& query, Edge& edge) {

    Indices neighbors;
    m_knq(query, neighbors);

    auto& labels = edge.labels;
    const auto& pixels = m_image.pixels;
    for (const std::size_t n : neighbors) {
      if (pixels[n].label != std::size_t(-1)) {
        labels.first = pixels[n].label;
        return;
      }
    }
  }

  void set_edge_types() {

    for (auto& edge : m_edges) {
      const auto& labels = edge.labels;
      edge.type = Edge_type::DEFAULT;

      const std::size_t l1 = labels.first;
      const std::size_t l2 = labels.second;

      if (l1 == std::size_t(-1) || l2 == std::size_t(-1)) {
        edge.type = Edge_type::BOUNDARY; continue;
      }

      const auto& from = m_vertices[edge.from_vertex];
      const auto& to   = m_vertices[edge.to_vertex];

      if (from.type != Point_type::LINEAR || to.type != Point_type::LINEAR) {
        edge.type = Edge_type::INTERNAL; continue;
      }
    }
  }

  void sort_vertex_halfedges() {

    for (auto& vertex : m_vertices) {
      auto& hedges = vertex.hedges;

      std::sort(hedges.begin(), hedges.end(), 
      [this](const std::size_t i, const std::size_t j) -> bool { 
          
        const auto& hedgei = m_halfedges[i];
        const auto& hedgej = m_halfedges[j];

        const std::size_t idx_toi = hedgei.to_vertex;
        const std::size_t idx_toj = hedgej.to_vertex;

        const auto typei = m_vertices[idx_toi].type;
        const auto typej = m_vertices[idx_toj].type;

        const std::size_t pi = get_priority(typei);
        const std::size_t pj = get_priority(typej);

        return pi > pj;
      });
    }
  }

  std::size_t get_priority(const Point_type type) {

    switch (type) {
      case Point_type::FREE:
        return 4;
      case Point_type::LINEAR:
        return 4;
      case Point_type::CORNER:
        return 3;
      case Point_type::OUTER_BOUNDARY:
        return 2;
      case Point_type::OUTER_CORNER:
        return 1;
      default:
        return 0;
    }
    return 0;
  }

  void compute_next_halfedges() {
    
    for (const auto& he : m_halfedges) {
      if (he.next != std::size_t(-1)) continue;
      if (m_halfedges[he.opposite].next != std::size_t(-1)) continue;

      const auto& labels = m_edges[he.edg_idx].labels;
      
      const std::size_t l1 = labels.first;
      const std::size_t l2 = labels.second;
      if (l1 == std::size_t(-1) || l2 == std::size_t(-1))
        continue;

      traverse(he.index, l1);
      traverse(he.opposite, l2);
    } 
  }

  void traverse(
    const std::size_t idx, 
    const std::size_t ref_label) {

    const auto& he = m_halfedges[idx];
    if (he.next != std::size_t(-1)) return;
    if (ref_label == std::size_t(-1)) return;
    if (m_halfedges[he.opposite].next != std::size_t(-1)) return;

    std::vector<Segment_2> segments;

    std::size_t count = 0;
    const std::size_t start = he.index; 
    std::size_t curr = start;
    do {
      
      auto& other = m_halfedges[curr];
      const std::size_t to_idx = other.to_vertex;
      const auto& to = m_vertices[to_idx];
      find_next(ref_label, to, other);
      
      const auto& s = m_vertices[other.from_vertex].point;
      const auto& t = m_vertices[other.to_vertex].point;

      segments.push_back(Segment_2(s, t));
      curr = other.next;

      /*      
      if (m_halfedges[curr].next != std::size_t(-1)) {
        other.next = std::size_t(-1); return;
      }
      if (m_halfedges[m_halfedges[curr].opposite].next != std::size_t(-1)) {
        other.next = std::size_t(-1); return;
      } */

      if (count >= 10000) {
        std::cout << "Error: traverse() max count reached!" << std::endl;

        Saver saver;
        saver.save_polylines(
        segments, "/Users/monet/Documents/lod/logs/buildings/tmp/debug-count");
        exit(1);
      }

      if (curr == std::size_t(-1)) {
        std::cout << "Error: traverse() failed!" << std::endl;

        Saver saver;
        saver.save_polylines(
        segments, "/Users/monet/Documents/lod/logs/buildings/tmp/debug-fail");
        exit(1);
      }
      ++count;

    } while (curr != start);
  }

  void find_next(
    const std::size_t ref_label,
    const Vertex& to, Halfedge& he) {

    for (const std::size_t other_idx : to.hedges) {
      const auto& other = m_halfedges[other_idx];
      if (other.edg_idx == he.edg_idx) 
        continue;

      const auto& edge = m_edges[other.edg_idx];
      const auto& labels = edge.labels;
      const std::size_t l1 = labels.first;
      const std::size_t l2 = labels.second;

      if (l1 == ref_label && l2 != ref_label) {
        he.next = other.index; 
        /* traverse(other.opposite, l2); */
        continue;
      }
      if (l2 == ref_label && l1 != ref_label) {
        he.next = other.index;
        /* traverse(other.opposite, l1); */
        continue;
      }
      if (l1 != ref_label && l2 != ref_label) {
        continue;
      }
      
      std::cout << "Error: find_next() failed!" << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  void initialize_faces() {

    m_faces.clear();
    std::vector<Indices> regions;

    /*
    regions.clear();
    add_face_regions(regions);
    create_faces(regions); */

    std::set<std::size_t> labels;
    for (const auto& edge : m_edges) {

      if (edge.labels.first != std::size_t(-1))
        labels.insert(edge.labels.first);
      if (edge.labels.second != std::size_t(-1))
        labels.insert(edge.labels.second);
    }

    for (const std::size_t label : labels) {
      /* compute_next_he(label); */
      compute_next_vt(label, regions);
      /* 
      regions.clear();
      add_face_regions(regions);
      add_faces(regions); */
    }

    clear_next_halfedges();
    remove_duplicated_faces();
    for (std::size_t i = 0; i < m_faces.size(); ++i) {
      m_faces[i].index = i;
      initialize_face(m_faces[i]);
    }
    sort_faces();
    mark_bad_faces();
  }

  void compute_next_he(
    const std::size_t ref_label) {

    clear_next_halfedges();
    for (const auto& he : m_halfedges) {
      if (he.next != std::size_t(-1)) continue;
      if (m_halfedges[he.opposite].next != std::size_t(-1)) continue;

      const auto& edge = m_edges[he.edg_idx];
      const auto& labels = edge.labels;

      const std::size_t l1 = labels.first;
      const std::size_t l2 = labels.second;
      
      if (l1 == std::size_t(-1) || l2 == std::size_t(-1))
        continue;

      if (l1 == ref_label) {
        traverse(he.index, l1); continue;
      }

      if (l2 == ref_label) {
        traverse(he.index, l2); continue;
      }
    }
  }

  void compute_next_vt(
    const std::size_t ref_label,
    std::vector<Indices>& regions) {

    regions.clear();
    clear_next_halfedges();

    for (const auto& vt : m_vertices) {
      if (!(
        vt.type == Point_type::CORNER || 
        vt.type == Point_type::OUTER_BOUNDARY) ) continue;

      for (const std::size_t he_idx : vt.hedges) {
        const auto& he = m_halfedges[he_idx];

        if (he.next != std::size_t(-1)) continue;
        if (m_halfedges[he.opposite].next != std::size_t(-1)) continue;

        const auto& edge = m_edges[he.edg_idx];
        const auto& labels = edge.labels;

        const std::size_t l1 = labels.first;
        const std::size_t l2 = labels.second;
        
        if (l1 == std::size_t(-1) || l2 == std::size_t(-1))
          continue;

        if (l1 == ref_label) {
          traverse(he.index, l1);
          add_face_regions(regions); 
          add_faces(regions);
          regions.clear();
          clear_next_halfedges();
          continue;
        }

        if (l2 == ref_label) {
          traverse(he.index, l2);
          add_face_regions(regions);
          add_faces(regions);
          regions.clear();
          clear_next_halfedges();
          continue;
        }
      }
    }
  }

  void clear_next_halfedges() {
    for (auto& he : m_halfedges)
      he.next = std::size_t(-1);
  }

  void add_face_regions(
    std::vector<Indices>& regions) {

    DS_neighbor_query neighbor_query(
      m_vertices, m_edges, m_halfedges);
    DS_region_type region_type(
      m_vertices, m_edges, m_halfedges);
    Region_growing region_growing(
      m_halfedges, neighbor_query, region_type);
    
    region_growing.detect(std::back_inserter(regions));
    /* std::cout << "num face regions: " << regions.size() << std::endl; */
  }

  void add_faces(
    const std::vector<Indices>& regions) {

    if (regions.size() == 0)
      return;

    Face face;
    for (const auto& region : regions) {
      face.hedges = region; m_faces.push_back(face);
    }
  }

  /*
  void create_faces(
    const std::vector<Indices>& regions) {
    
    const std::size_t numr = regions.size();
    if (numr == 0) return;

    m_faces.clear();
    m_faces.reserve(numr);

    Face face;
    for (std::size_t i = 0; i < numr - 1; ++i) {
      if (are_equal_regions(regions[i], regions[i + 1]))
        continue;  
      
      face.hedges.clear();
      face.hedges = regions[i];
      m_faces.push_back(face);
    }

    face.hedges.clear();
    face.hedges = regions[numr - 1];
    m_faces.push_back(face);
  }
  */

  bool are_equal_regions(
    const Indices& r1, const Indices& r2) {
    
    if (r1.size() != r2.size())
      return false;

    const std::size_t numr = r1.size();
    for (std::size_t i = 0; i < numr; ++i) {
      const std::size_t j = numr - i - 1;

      const std::size_t ei = m_halfedges[r1[i]].edg_idx;
      const std::size_t ej = m_halfedges[r2[j]].edg_idx;
      if (ei != ej) return false;
    }
    return true;
  }

  void remove_duplicated_faces() {

    std::vector<Face> clean;
    std::vector<bool> states(m_faces.size(), true);

    Indices dindices;
    for (std::size_t i = 0; i < m_faces.size(); ++i) {
      if (!states[i]) continue;

      dindices.clear();
      find_duplicates(i, m_faces[i], dindices);
      for (const std::size_t didx : dindices)
        states[didx] = false;
    }

    for (std::size_t i = 0; i < m_faces.size(); ++i)
      if (states[i]) clean.push_back(m_faces[i]);
    m_faces = clean;
  }

  void find_duplicates(
    const std::size_t skip,
    const Face& f1,
    Indices& dindices) {

    for (std::size_t i = 0; i < m_faces.size(); ++i) {
      if (i == skip) continue;
      if (are_equal_faces(f1, m_faces[i])) 
        dindices.push_back(i);
    }
  }

  bool are_equal_faces(
    const Face& f1, const Face& f2) {

    const auto& hedges1 = f1.hedges;
    const auto& hedges2 = f2.hedges;

    if (hedges1.size() != hedges2.size())
      return false;

    std::size_t count = 0;
    for (const std::size_t idx1 : hedges1) {
      const std::size_t edg_idx1 = m_halfedges[idx1].edg_idx;
      for (const std::size_t idx2 : hedges2) {
        const std::size_t edg_idx2 = m_halfedges[idx2].edg_idx;
        if (edg_idx1 == edg_idx2) ++count;
      }
    }
    return hedges1.size() == count;
  }

  void initialize_face(Face& face) {
    
    face.type = Face_type::DEFAULT;
    create_face_probs(face);
    create_face_triangulation(face);
    create_face_visibility(face);
    create_face_label(face);
    compute_face_area(face);
  }

  void update_face(Face& face) {

    if (face.skip) return;
    create_face_triangulation(face);
    create_face_visibility(face);
    update_face_label(face);
    compute_face_area(face);
    if (face.area < internal::tolerance<FT>())
      face.skip = true;
  }

  void create_face_probs(Face& face) {
    
    face.probs.clear();
    for (const std::size_t he_idx : face.hedges) {
      const auto& he = m_halfedges[he_idx];
      const auto& edge = m_edges[he.edg_idx];
      const auto& labels = edge.labels;

      if (labels.first != std::size_t(-1))
        face.probs.insert(labels.first);
      if (labels.second != std::size_t(-1))
        face.probs.insert(labels.second);
    }
  }

  void create_face_triangulation(Face& face) {

    auto& tri = face.tri.delaunay;
    tri.clear();

    std::vector<Edge> edges;
    create_face_edges(face, edges);

    for (const auto& edge : edges) {
      const auto& s = edge.segment.source();
      const auto& t = edge.segment.target();

      const auto vh1 = tri.insert(s);
      const auto vh2 = tri.insert(t);
      
      if (vh1 != vh2)
        tri.insert_constraint(vh1, vh2);
    }
  }

  void create_face_visibility(
    Face& face) {

    std::vector<Point_2> bbox;
    create_face_bbox(face, bbox);

    auto& tri = face.tri.delaunay;
    for (auto fh = tri.finite_faces_begin();
    fh != tri.finite_faces_end(); ++fh) {
      fh->info().object_index = face.index;

      const auto& p0 = fh->vertex(0)->point();
      const auto& p1 = fh->vertex(1)->point();
      const auto& p2 = fh->vertex(2)->point();

      const FT x = (p0.x() + p1.x() + p2.x()) / FT(3);
      const FT y = (p0.y() + p1.y() + p2.y()) / FT(3);
      const Point_2 p = Point_2(x, y);

      FT in = FT(1); FT out = FT(1);
      for (std::size_t i = 0; i < bbox.size(); ++i) {
        const auto& q = bbox[i];
        if (tri.oriented_side(fh, p) == CGAL::ON_NEGATIVE_SIDE)
          continue;

        LF_circulator circ = tri.line_walk(p, q, fh);
        const LF_circulator end = circ;
        if (circ.is_empty()) continue;

        std::size_t inter = 0;
        do {

          LF_circulator f1 = circ; ++circ;
          LF_circulator f2 = circ;

          const bool success = are_neighbors(f1, f2);
          if (!success) break;

          const std::size_t idx = f1->index(f2);
          const auto edge = std::make_pair(f1, idx);
          if (tri.is_constrained(edge)) ++inter;
          if (tri.is_infinite(f2)) break;

        } while (circ != end);

        if (inter % 2 == 0) out += FT(1);
        else in += FT(1);
      }

      const FT sum = in + out;
      in /= sum; out /= sum;

      if (in > FT(1) / FT(2)) {
        fh->info().interior = true;
        fh->info().tagged   = true;
      } else { 
        fh->info().interior = false;
        fh->info().tagged   = false;
      }
    }
  }

  void create_face_bbox(
    const Face& face,
    std::vector<Point_2>& bbox) {

    bbox.clear();
    const auto& tri = face.tri.delaunay;
    
    std::vector<Point_2> points;
    for (auto fh = tri.finite_faces_begin();
    fh != tri.finite_faces_end(); ++fh) {
      const auto& p0 = fh->vertex(0)->point();
      const auto& p1 = fh->vertex(1)->point();
      const auto& p2 = fh->vertex(2)->point();
      points.push_back(p0);
      points.push_back(p1);
      points.push_back(p2);
    }

    CGAL::Identity_property_map<Point_2> pmap;
    internal::bounding_box_2(points, pmap, bbox);
  }

  bool are_neighbors(
    LF_circulator f1, LF_circulator f2) const {

    for (std::size_t i = 0; i < 3; ++i) {
      const std::size_t ip = (i + 1) % 3;

      const auto p1 = f1->vertex(i);
      const auto p2 = f1->vertex(ip);

      for (std::size_t j = 0; j < 3; ++j) {
        const std::size_t jp = (j + 1) % 3;

        const auto q1 = f2->vertex(j);
        const auto q2 = f2->vertex(jp);

        if ( 
          ( p1 == q1 && p2 == q2) ||
          ( p1 == q2 && p2 == q1) ) {

          return true;
        }
      }
    }
    return false;
  }

  void create_face_label(Face& face) {
    clear_face_probabilities(face);
    compute_face_probabilities(face);
    initialize_face_labels(face);
    set_face_label(face);
  }

  void clear_face_probabilities(Face& face) {

    const std::size_t num_labels = m_image.num_labels;
    auto& tri = face.tri.delaunay;
    for (auto fh = tri.finite_faces_begin();
    fh != tri.finite_faces_end(); ++fh) {
      if (!fh->info().interior) continue;

      fh->info().probabilities.clear();
      fh->info().probabilities.resize(num_labels, FT(0));
    }
  }

  void compute_face_probabilities(Face& face) {

    auto& tri = face.tri.delaunay;
    for (const auto& pixel : m_image.pixels) {
      if (pixel.label == std::size_t(-1))
        continue;

      const auto& p = pixel.point;
      auto fh = tri.locate(p);
      if (tri.is_infinite(fh) || !fh->info().interior) 
        continue;
      fh->info().probabilities[pixel.label] += FT(1);
    }
  }

  void initialize_face_labels(Face& face) {

    const std::size_t num_labels = m_image.num_labels;
    auto& tri = face.tri.delaunay;
    for (auto fh = tri.finite_faces_begin();
    fh != tri.finite_faces_end(); ++fh) {
      if (!fh->info().interior) continue;

      FT max_prob = FT(-1);
      std::size_t best_label = std::size_t(-1);
      for (std::size_t i = 0; i < num_labels; ++i) {
        const FT prob = fh->info().probabilities[i];
        if (prob > max_prob) {
          max_prob = prob; best_label = i;
        }
      }

      if (max_prob != FT(0))
        fh->info().label = best_label;
      else
        fh->info().label = std::size_t(-1);
    }
  }

  void set_face_label(Face& face) {

    const std::size_t num_labels = m_image.num_labels;
    std::vector<std::size_t> probs(num_labels, FT(0));

    auto& tri = face.tri.delaunay;
    for (auto fh = tri.finite_faces_begin();
    fh != tri.finite_faces_end(); ++fh) {
      if (!fh->info().interior) continue;
      if (fh->info().label != std::size_t(-1))
        probs[fh->info().label] += FT(1);
    }

    FT max_prob = FT(-1);
    std::size_t best_label = std::size_t(-1);
    for (std::size_t i = 0; i < num_labels; ++i) {
      if (probs[i] > max_prob) {
        max_prob = probs[i]; best_label = i;
      }
    }

    for (auto fh = tri.finite_faces_begin();
    fh != tri.finite_faces_end(); ++fh) {
      if (!fh->info().interior) {
        fh->info().label = std::size_t(-1); continue;
      }
      fh->info().label = best_label;
    }
    face.label = best_label;
  }

  void update_face_label(Face& face) {

    auto& tri = face.tri.delaunay;
    for (auto fh = tri.finite_faces_begin();
    fh != tri.finite_faces_end(); ++fh) {
      if (!fh->info().interior) {
        fh->info().label = std::size_t(-1); continue;
      }
      fh->info().label = face.label;
    }
  }

  void compute_face_area(Face& face) {

    face.area = FT(0);
    const auto& tri = face.tri.delaunay;
    for (auto fh = tri.finite_faces_begin();
    fh != tri.finite_faces_end(); ++fh) {
      if (!fh->info().interior) continue;

      const auto& p0 = fh->vertex(0)->point();
      const auto& p1 = fh->vertex(1)->point();
      const auto& p2 = fh->vertex(2)->point();

      const Triangle_2 triangle = Triangle_2(p0, p1, p2);
      face.area += triangle.area();
    } 
  }

  void sort_faces() {

    std::sort(m_faces.begin(), m_faces.end(), 
    [](const Face& f1, const Face& f2) -> bool { 
      return f1.area > f2.area;
    });
  }

  void mark_bad_faces() {
    /* mark_bad_faces_area_based(); */
  }

  void mark_bad_faces_area_based() {

    FT avg_area = FT(0);
    for (const auto& face : m_faces)
      avg_area += face.area;
    avg_area /= static_cast<FT>(m_faces.size());

    for (auto& face : m_faces)
      if (face.area < avg_area / FT(4)) 
        face.skip = true;
  }

  void save_faces_polylines(const std::string folder) {

    std::vector<Edge> edges;
    std::vector<Segment_2> segments;

    for (const auto& face : m_faces) {
      if (face.skip) continue;

      create_face_edges(face, edges);
      
      segments.clear();
      for (const auto& edge : edges) {
        segments.push_back(edge.segment);
      }

      Saver saver;
      saver.save_polylines(
        segments, "/Users/monet/Documents/lod/logs/buildings/tmp/" + 
        folder + "/image-faces-" + 
        std::to_string(face.index));
    }
  }
  
  void create_face_edges(
    const Face& face,
    std::vector<Edge>& edges) {

    edges.clear();
    for (std::size_t i = 0; i < face.hedges.size(); ++i) {
      
      const std::size_t he_idx = face.hedges[i];
      const auto& he = m_halfedges[he_idx];
      const std::size_t from = he.from_vertex;
      const std::size_t to = he.to_vertex;

      const auto& s = m_vertices[from];
      const auto& t = m_vertices[to];

      if (!s.skip && !t.skip) {
        
        Edge edge;
        edge.from_vertex = from;
        edge.to_vertex = to;
        edge.segment = Segment_2(s.point, t.point);
        edge.type = m_edges[he.edg_idx].type;

        edges.push_back(edge);
        continue;
      }

      if (!s.skip && t.skip) {
        i = get_next(face, i);

        const std::size_t next_idx = face.hedges[i];
        const auto& next = m_halfedges[next_idx];
        const std::size_t end = next.to_vertex;
        const auto& other = m_vertices[end];

        Edge edge;
        edge.from_vertex = from;
        edge.to_vertex = end;
        edge.segment = Segment_2(s.point, other.point);
        edge.type = Edge_type::INTERNAL;

        edges.push_back(edge);
        continue;
      }
    }
  }

  std::size_t get_next(
    const Face& face,
    const std::size_t start) {

    for (std::size_t i = start; i < face.hedges.size(); ++i) {
      const std::size_t he_idx = face.hedges[i];
      const auto& he = m_halfedges[he_idx];
      const std::size_t to = he.to_vertex;
      if (m_vertices[to].skip) continue;
      return i;
    }

    const std::size_t i = face.hedges.size() - 1;
    const std::size_t he_idx = face.hedges[i];
    const auto& he = m_halfedges[he_idx];
    const std::size_t to = he.to_vertex;
    return i;
  }

  void save_faces_ply(const std::string folder) {

    for (const auto& face : m_faces) {
      if (face.skip) continue;

      const FT z = FT(0);
      std::size_t num_vertices = 0;
      Indexer indexer;

      std::vector<Point_3> vertices; 
      std::vector<Indices> faces; 
      std::vector<Color> fcolors;

      Inserter inserter(faces, fcolors);
      auto output_vertices = std::back_inserter(vertices);
      auto output_faces = boost::make_function_output_iterator(inserter);
      face.tri.output_with_label_color(
        indexer, num_vertices, output_vertices, output_faces, z);

      Saver saver;
      saver.export_polygon_soup(
        vertices, faces, fcolors, 
        "/Users/monet/Documents/lod/logs/buildings/tmp/" + 
        folder + "/faces-" + 
        std::to_string(face.index));
    }
  }
};

} // internal
} // Levels_of_detail
} // CGAL

#endif // CGAL_LEVELS_OF_DETAIL_INTERNAL_IMAGE_DATA_STRUCTURE_H