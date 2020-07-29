// Copyright (c) 2007-2020  INRIA (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
// Author(s)     : Jackson Campolattaro, Cédric Portaneri, Tong Zhao

#ifndef CGAL_OCTREE_3_H
#define CGAL_OCTREE_3_H

#include <CGAL/license/Octree.h>

#include <CGAL/Octree/Node.h>
#include <CGAL/Octree/Split_criterion.h>
#include <CGAL/Octree/Walker_criterion.h>
#include <CGAL/Octree/Walker_iterator.h>

#include <CGAL/bounding_box.h>
#include <CGAL/Aff_transformation_3.h>
#include <CGAL/aff_transformation_tags.h>

#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/Search_traits_adapter.h>
#include <CGAL/intersections.h>
#include <CGAL/squared_distance_3.h>

#include <boost/function.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/range/iterator_range.hpp>

#include <iostream>
#include <fstream>
#include <ostream>
#include <functional>

#include <stack>
#include <queue>
#include <vector>
#include <math.h>

using namespace std::placeholders;

namespace CGAL {

namespace Octree {

/*!
 * \ingroup PkgOctreeClasses
 *
 * \brief Class Octree is a data structure for efficient computations in 3D space.
 *
 * \details It builds a heirarchy of nodes which subdivide the space based on a collection of points.
 * Each node represents an axis aligned cubic region of space.
 * A node contains the range of points that are present in the region it defines,
 * and it may contain eight other nodes which further subdivide the region.
 *
 * \tparam PointRange is a range type that provides random access iterators over the indices of a set of points.
 * \tparam PointMap is a type that maps items in the PointRange to Point data
 */
template<class PointRange, class PointMap>
class Octree {

public:

  /// \name Public Types
  /// @{

  /*!
   * \brief The point type is deduced from the type of the property map used
   */
  typedef typename boost::property_traits<PointMap>::value_type Point;

  /*!
   * \brief The Kernel used is deduced from the point type
   */
  typedef typename CGAL::Kernel_traits<Point>::Kernel Kernel;

  /*!
   * \brief The floating point type is decided by the Kernel
   */
  typedef typename Kernel::FT FT;

  /*!
   * \brief
   */
  typedef boost::iterator_range<typename PointRange::iterator> Points_iterator_range;

  /*!
   * \brief The Sub-tree / Octant type
   */
  typedef Node::Node<typename PointRange::iterator> Node;

  /*!
   * \brief A function that determines whether a node needs to be split when refining a tree
   */
  typedef std::function<bool(const Node &)> Split_criterion_function;

  /*!
   * \brief A range that provides input-iterator access to the nodes of a tree
   */
  typedef boost::iterator_range<Walker_iterator<const Node>> Node_range;

  /*!
   * \brief A function that determines the next node in a traversal given the current one
   */
  typedef std::function<const Node *(const Node *)> Node_walker;

  /// @}

private: // Private types

  typedef typename Kernel::Vector_3 Vector;
  typedef typename Kernel::Iso_cuboid_3 Iso_cuboid;
  typedef typename Kernel::Sphere_3 Sphere;
  typedef typename CGAL::Bbox_3 Bbox;
  typedef typename PointRange::iterator Range_iterator;
  typedef typename std::iterator_traits<Range_iterator>::value_type Range_type;

private: // data members :

  Node m_root;                      /* root node of the octree */
  uint8_t m_max_depth_reached = 0;  /* octree actual highest depth reached */

  PointRange &m_ranges;              /* input point range */
  PointMap m_points_map;          /* property map: `value_type of InputIterator` -> `Point` (Position) */

  Point m_bbox_min;                  /* input bounding box min value */
  FT m_bbox_side;              /* input bounding box side length (cube) */

  std::vector<FT> m_side_per_depth;      /* side length per node's depth */

public:

  /// \name Construction, Destruction
  /// @{

  /*!
   * \brief Create an octree from a collection of points
   *
   * The resulting octree will have a root node with no children that contains the points passed.
   * That root node will have a bounding box that encloses all of the points passed,
   * with padding according to the enlarge_ratio
   * This single-node octree is valid and compatible with all Octree functionality,
   * but any performance benefits are unlikely to be realized unless the tree is refined.
   *
   * \param point_range random access iterator over the indices of the points
   * \param point_map maps the point indices to their coordinate locations
   * \param enlarge_ratio the degree to which the bounding box should be enlarged
   */
  Octree(
          PointRange &point_range,
          PointMap &point_map,
          const FT enlarge_ratio = 1.2) :
          m_ranges(point_range),
          m_points_map(point_map) {

    // compute bounding box that encloses all points
    Iso_cuboid bbox = CGAL::bounding_box(boost::make_transform_iterator
                                                 (m_ranges.begin(),
                                                  CGAL::Property_map_to_unary_function<PointMap>(
                                                          m_points_map)),
                                         boost::make_transform_iterator
                                                 (m_ranges.end(),
                                                  CGAL::Property_map_to_unary_function<PointMap>(
                                                          m_points_map)));

    // Find the center point of the box
    Point bbox_centroid = midpoint(bbox.min(), bbox.max());

    // scale bounding box to add padding
    bbox = bbox.transform(Aff_transformation_3<Kernel>(SCALING, enlarge_ratio));

    // Convert the bounding box into a cube
    FT x_len = bbox.xmax() - bbox.xmin();
    FT y_len = bbox.ymax() - bbox.ymin();
    FT z_len = bbox.zmax() - bbox.zmin();
    FT max_len = std::max({x_len, y_len, z_len});
    bbox = Iso_cuboid(bbox.min(), bbox.min() + max_len * Vector(1.0, 1.0, 1.0));

    // Shift the squared box to make sure it's centered in the original place
    Point bbox_transformed_centroid = midpoint(bbox.min(), bbox.max());
    Vector diff_centroid = bbox_centroid - bbox_transformed_centroid;
    bbox = bbox.transform(Aff_transformation_3<Kernel>(TRANSLATION, diff_centroid));

    // save octree attributes
    m_bbox_min = bbox.min();
    m_bbox_side = bbox.max()[0] - m_bbox_min[0];
    m_root.points() = {point_range.begin(), point_range.end()};

  }

  /// @}

  /// \name Tree Building
  /// @{

  /*!
   * \brief Subdivide an octree's nodes and sub-nodes until it meets the given criteria
   *
   * The split criterion can be any function pointer that takes a Node pointer
   * and returns a boolean value (where true implies that a Node needs to be split).
   * It's safe to call this function repeatedly, and with different criterion.
   *
   * \param split_criterion rule to use when determining whether or not a node needs to be subdivided
   */
  void refine(const Split_criterion_function &split_criterion) {

    // If the tree has already been refined, reset it
    if (!m_root.is_leaf())
      m_root.unsplit();

    // Make sure the max depth is reset as well
    m_max_depth_reached = 0;

    // create a side length map
    for (int i = 0; i <= (int) 32; i++)
      m_side_per_depth.push_back(m_bbox_side / (FT) (1 << i));

    // Initialize a queue of nodes that need to be refined
    std::queue<Node *> todo;
    todo.push(&m_root);

    // Process items in the queue until it's consumed fully
    while (!todo.empty()) {

      // Get the next element
      auto current = todo.front();
      todo.pop();
      int depth = current->depth();

      // Check if this node needs to be processed
      if (split_criterion(*current)) {

        // Split the node, redistributing its points to its children
        split((*current));

        // Process each of its children
        for (int i = 0; i < 8; ++i)
          todo.push(&(*current)[i]);

      }
    }
  }

  /*!
   * \brief Refine an octree using a max depth and max number of points in a node as split criterion
   *
   * This is equivalent to calling:
   *
   *     refine(Split_criterion::Max_depth_or_bucket_size(max_depth, bucket_size));
   *
   * This functionality is provided for consistency with older octree implementations
   * which did not allow for custom split criterion.
   *
   * \param max_depth deepest a tree is allowed to be (nodes at this depth will not be split)
   * \param bucket_size maximum points a node is allowed to contain
   */
  void refine(size_t max_depth = 10, size_t bucket_size = 20) {
    refine(Split_criterion::Max_depth_or_bucket_size(max_depth, bucket_size));
  }

  /// @}

  /// \name Accessors
  /// @{

  /*!
   * \brief Provides read-only access to the root node, and by extension the rest of the tree
   *
   * \return a const reference to the root node of the tree
   */
  const Node &root() const { return m_root; }

  /*!
   * \brief Finds the deepest level reached by a leaf node in this tree
   *
   * \return the deepest level, where root is 0
   */
  std::size_t max_depth_reached() const { return m_max_depth_reached; }

  /*!
   * \brief Constructs an input range of nodes using a tree walker function
   *
   * The result is a boost range created from iterators that meet the criteria defining a Forward Input Iterator
   * This is completely compatible with standard foreach syntax.
   * Dereferencing returns a const reference to a node.
   * \todo Perhaps I should add some discussion of recommended usage
   *
   * \tparam Walker type of the walker rule
   * \param walker the rule to use when determining the order of the sequence of points produced
   * \return a forward input iterator over the nodes of the tree
   */
  template<class Walker>
  Node_range walk(const Walker &walker = Walker()) const {

    const Node *first = walker.first(&m_root);

    Node_walker next = std::bind(&Walker::template next<typename PointRange::iterator>,
                                 walker, _1);

    return boost::make_iterator_range(Walker_iterator<const Node>(first, next),
                                      Walker_iterator<const Node>());
  }

  /*!
   * \brief Find the leaf node which would contain a point
   *
   * Traverses the octree and finds the deepest cell that has a domain enclosing the point passed.
   * The point passed must be within the region enclosed by the octree (bbox of the root node).
   *
   * \param p The point to find a node for
   * \return A const reference to the node which would contain the point
   */
  const Node &locate(const Point &p) const {

    // Make sure the point is enclosed by the octree
    assert(CGAL::do_intersect(p, bbox(m_root)));

    // Start at the root node
    auto *node_for_point = &m_root;

    // Descend the tree until reaching a leaf node
    while (!node_for_point->is_leaf()) {

      // Find the point to split around
      Point center = barycenter(*node_for_point);

      // Find the index of the correct sub-node
      typename Node::Index index;
      for (int dimension = 0; dimension < 3; ++dimension) {

        index[dimension] = center[dimension] < p[dimension];
      }

      // Find the correct sub-node of the current node
      node_for_point = &(*node_for_point)[index.to_ulong()];
    }

    // Return the result
    return *node_for_point;
  }

  /*!
   * \brief Find the bounding box of a node
   *
   * Creates a cubic region representing a node.
   * The size of the region is dependent on the node's depth in the tree.
   * The location of the region is dependent on the node's location.
   * The bounding box is useful for checking for collisions with a node.
   *
   * \param node the node to determine the bounding box of
   * \return the bounding box defined by that node's relationship to the tree
   */
  Bbox bbox(const Node &node) const {

    // Determine the side length of this node
    FT size = m_side_per_depth[node.depth()];

    // Determine the location this node should be split
    FT min_corner[3];
    FT max_corner[3];
    for (int i = 0; i < 3; i++) {

      min_corner[i] = m_bbox_min[i] + (node.location()[i] * size);
      max_corner[i] = min_corner[i] + size;
    }

    // Create the cube
    return {min_corner[0], min_corner[1], min_corner[2],
            max_corner[0], max_corner[1], max_corner[2]};
  }

  /*!
   * \brief Find the K points in a tree that are nearest to the search point and within a specific radius
   *
   * This function guarantees that there are no closer points than the ones returned,
   * but it does not guarantee that it will return at least K points.
   * For a query where the search radius encloses K or fewer points, all enclosed points will be returned.
   * If the search radius passed is too small, no points may be returned.
   * This function is useful when the user already knows how sparse the points are,
   * or if they don't care about points that are too far away.
   * Setting a small radius may have performance benefits.
   *
   * \tparam Point_output_iterator an output iterator type that will accept points
   * \param search_point the location to find points near
   * \param search_radius_squared the size of the region to search within
   * \param k the number of points to find
   * \param output the output iterator to add the found points to (in order of increasing distance)
   */
  template<typename Point_output_iterator>
  void nearest_k_neighbors_in_radius(const Point &search_point, FT search_radius_squared, std::size_t k,
                                     Point_output_iterator output) const {

    // Create an empty list of points
    std::vector<Point_with_distance> points_list;
    points_list.reserve(k);

    // Invoking the recursive function adds those points to the vector (passed by reference)
    auto search_bounds = Sphere(search_point, search_radius_squared);
    nearest_k_neighbors_recursive(search_bounds, m_root, points_list);

    // Add all the points found to the output
    for (auto &item : points_list)
      *output++ = item.point;
  }

  /*!
   * \brief Find the K points in a tree that are nearest to the search point
   *
   * This function is equivalent to invoking nearest_k_neighbors_in_radius for an infinite radius.
   * For a tree with K or fewer points, all points in the tree will be returned.
   *
   * \tparam Point_output_iterator an output iterator type that will accept points
   * \param search_point the location to find points near
   * \param k the number of points to find
   * \param output the output iterator to add the found points to (in order of increasing distance)
   */
  template<typename Point_output_iterator>
  void nearest_k_neighbors(const Point &search_point, std::size_t k, Point_output_iterator output) const {

    return nearest_k_neighbors_in_radius(search_point, std::numeric_limits<FT>::max(), k, output);
  }

  /// @}

  /// \name Operators
  /// @{

  /*!
   * \brief Compares the topology of a pair of Octrees
   *
   * Trees may be considered equivalent even if they contain different points.
   * Equivalent trees must have the same bounding box and the same node structure.
   * Node structure is evaluated by comparing the root nodes using the node equality operator.
   * \todo Should I link to that?
   *
   * \param rhs tree to compare with
   * \return whether the trees have the same topology
   */
  bool operator==(const Octree<PointRange, PointMap> &rhs) const {

    // Identical trees should have the same bounding box
    if (rhs.m_bbox_min != m_bbox_min || rhs.m_bbox_side != m_bbox_side)
      return false;

    // Identical trees should have the same depth
    if (rhs.m_max_depth_reached != m_max_depth_reached)
      return false;

    // If all else is equal, recursively compare the trees themselves
    return rhs.m_root == m_root;
  }

  /*!
   * \brief Compares the topology of a pair of Octrees
   * \param rhs tree to compare with
   * \return whether the trees have different topology
   */
  bool operator!=(const Octree<PointRange, PointMap> &rhs) const {
    return !operator==(rhs);
  }

  /// @}


private: // functions :

  // TODO: Could this method name be reduced to just "center" ?
  Point barycenter(const Node &node) const {

    // Determine the side length of this node
    FT size = m_side_per_depth[node.depth()];

    // Determine the location this node should be split
    FT bary[3];
    for (int i = 0; i < 3; i++)
      bary[i] = node.location()[i] * size + (size / 2.0) + m_bbox_min[i];

    // Convert that location into a point
    return {bary[0], bary[1], bary[2]};
  }

  void reassign_points(Node &node, Range_iterator begin, Range_iterator end, const Point &center,
                       std::bitset<3> coord = {},
                       std::size_t dimension = 0) {

    // Root case: reached the last dimension
    if (dimension == 3) {

      node[coord.to_ulong()].points() = {begin, end};

      return;
    }

    // Split the point collection around the center point on this dimension
    Range_iterator split_point = std::partition(begin, end,
                                                [&](const Range_type &a) -> bool {
                                                  return (get(m_points_map, a)[dimension] < center[dimension]);
                                                });

    // Further subdivide the first side of the split
    std::bitset<3> coord_left = coord;
    coord_left[dimension] = false;
    reassign_points(node, begin, split_point, center, coord_left, dimension + 1);

    // Further subdivide the second side of the split
    std::bitset<3> coord_right = coord;
    coord_right[dimension] = true;
    reassign_points(node, split_point, end, center, coord_right, dimension + 1);

  }

  void split(Node &node) {

    // Make sure the node hasn't already been split
    assert(node.is_leaf());

    // Split the node to create children
    node.split();

    // Find the point to around which the node is split
    Point center = barycenter(node);

    // Add the node's points to its children
    reassign_points(node, node.points().begin(), node.points().end(), center);
  }

  bool do_intersect(const Node &node, const Sphere &sphere) const {

    // Create a cubic bounding box from the node
    Bbox node_cube = bbox(node);

    // Check for overlap between the node's box and the sphere as a box, to quickly catch some cases
    // FIXME: Activating this causes slower times
//    if (!do_overlap(node_cube, sphere.bbox()))
//      return false;

    // Check for intersection between the node and the sphere
    return CGAL::do_intersect(node_cube, sphere);
  }

  // TODO: There has to be a better way than using structs like these!
  struct Point_with_distance {
    Point point;
    FT distance;
  };
  struct Node_index_with_distance {
    typename Node::Index index;
    FT distance;
  };

  void nearest_k_neighbors_recursive(Sphere &search_bounds, const Node &node,
                                     std::vector<Point_with_distance> &results, FT epsilon = 0) const {

    // Check whether the node has children
    if (node.is_leaf()) {

      // Base case: the node has no children

      // Loop through each of the points contained by the node
      // Note: there might be none, and that should be fine!
      for (auto point_index : node.points()) {

        // Retrieve each point from the octree's point map
        auto point = get(m_points_map, point_index);

        // Pair that point with its distance from the search point
        Point_with_distance current_point_with_distance =
                {point, squared_distance(point, search_bounds.center())};

        // Check if the new point is within the bounds
        if (current_point_with_distance.distance < search_bounds.squared_radius()) {

          // Check if the results list is full
          if (results.size() == results.capacity()) {

            // Delete a point if we need to make room
            results.pop_back();
          }

          // Add the new point
          results.push_back(current_point_with_distance);

          // Sort the list
          std::sort(results.begin(), results.end(), [=](auto &left, auto &right) {
            return left.distance < right.distance;
          });

          // Check if the results list is full
          if (results.size() == results.capacity()) {

            // Set the search radius
            search_bounds = Sphere(search_bounds.center(), results.back().distance + epsilon);
          }
        }
      }
    } else {

      // Recursive case: the node has children

      // Create a list to map children to their distances
      std::vector<Node_index_with_distance> children_with_distances;
      children_with_distances.reserve(8);

      // Fill the list with child nodes
      for (int index = 0; index < 8; ++index) {
        auto &child_node = node[index];

        // Add a child to the list, with its distance
        children_with_distances.push_back(
                {typename Node::Index(index),
                 CGAL::squared_distance(search_bounds.center(), barycenter(child_node))}
        );
      }

      // Sort the children by their distance from the search point
      std::sort(children_with_distances.begin(), children_with_distances.end(), [=](auto &left, auto &right) {
        return left.distance < right.distance;
      });

      // Loop over the children
      for (auto child_with_distance : children_with_distances) {
        auto &child_node = node[child_with_distance.index.to_ulong()];

        // Check whether the bounding box of the child intersects with the search bounds
        if (do_intersect(child_node, search_bounds)) {

          // Recursively invoke this function
          nearest_k_neighbors_recursive(search_bounds, child_node, results);
        }
      }
    }
  }

}; // end class Octree

} // namespace Octree

} // namespace CGAL

#endif // CGAL_OCTREE_3_H