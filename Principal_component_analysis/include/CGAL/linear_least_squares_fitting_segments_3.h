// Copyright (c) 2005  INRIA Sophia-Antipolis (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
// Author(s) : Pierre Alliez and Sylvain Pion and Ankit Gupta

#ifndef CGAL_LINEAR_LEAST_SQUARES_FITTING_SEGMENTS_3_H
#define CGAL_LINEAR_LEAST_SQUARES_FITTING_SEGMENTS_3_H

#include <CGAL/license/Principal_component_analysis.h>


#include <CGAL/basic.h>
#include <CGAL/centroid.h>
#include <CGAL/pca_fitting_3.h>
#include <CGAL/compute_moment_3.h>
#include <CGAL/linear_least_squares_fitting_points_3.h>
#include <CGAL/Subiterator.h>

#include <list>
#include <iterator>

namespace CGAL {

namespace internal {



// fits a plane to a 3D segment set
template < typename InputIterator,
           typename K,
           typename DiagonalizeTraits >
typename K::FT
linear_least_squares_fitting_3(InputIterator first,
                               InputIterator beyond,
                               typename K::Plane_3& plane,   // best fit plane
                               typename K::Point_3& c,       // centroid
                               const typename K::Segment_3*,  // used for indirection
                               const K& k,                   // kernel
                               const CGAL::Dimension_tag<1>& tag,
                               const DiagonalizeTraits& diagonalize_traits)
{
  typedef typename K::Segment_3  Segment;

  typename DiagonalizeTraits::Covariance_matrix covariance = {{ 0., 0., 0., 0., 0., 0. }};
  compute_centroid_and_covariance(first, beyond, c, covariance, (Segment*)nullptr, k, tag);

  // compute fitting plane
  return fitting_plane_3(covariance, c, plane, k, diagonalize_traits);

} // end linear_least_squares_fitting_segments_3

// fits a line to a 3D segment set
template < typename InputIterator,
    typename K,
    typename DiagonalizeTraits >
    typename K::FT
    linear_least_squares_fitting_3(InputIterator first,
        InputIterator beyond,
        typename K::Line_3& line,      // best fit line
        typename K::Point_3& c,        // centroid
        const typename K::Segment_3*,  // used for indirection
        const K& k,                    // kernel
        const CGAL::Dimension_tag<1>& tag,
        const DiagonalizeTraits& diagonalize_traits)
{
    typedef typename K::Segment_3  Segment;

    typename DiagonalizeTraits::Covariance_matrix covariance = { { 0., 0., 0., 0., 0., 0. } };
    compute_centroid_and_covariance(first, beyond, c, covariance, (Segment*)nullptr, k, tag);

    // compute fitting line
    return fitting_line_3(covariance, c, line, k, diagonalize_traits);

} // end linear_least_squares_fitting_segments_3


// fits a plane to the vertices of a 3D segment set
template < typename InputIterator,
           typename K,
           typename DiagonalizeTraits >
typename K::FT
linear_least_squares_fitting_3(InputIterator first,
                               InputIterator beyond,
                               typename K::Plane_3& plane,   // best fit plane
                               typename K::Point_3& c,       // centroid
                               const typename K::Segment_3*, // used for indirection
                               const K& k,                   // kernel
                               const CGAL::Dimension_tag<0>& tag,
                               const DiagonalizeTraits& diagonalize_traits)
{
  typedef typename K::Segment_3  Segment;
  typedef typename K::Point_3  Point;
  auto converter = [](const Segment& s, std::size_t idx) -> Point { return s[idx]; };

  // precondition: at least one element in the container.
  CGAL_precondition(first != beyond);

  return linear_least_squares_fitting_3
    (make_subiterator<Point, 2> (first, converter),
     make_subiterator<Point, 2> (beyond),
     plane,c,(Point*)nullptr,k,tag,
     diagonalize_traits);
} // end linear_least_squares_fitting_segments_3


// fits a line to the vertices of a 3D segment set
template < typename InputIterator,
           typename K,
           typename DiagonalizeTraits >
typename K::FT
linear_least_squares_fitting_3(InputIterator first,
                               InputIterator beyond,
                               typename K::Line_3& line,      // best fit line
                               typename K::Point_3& c,        // centroid
                               const typename K::Segment_3*,  // used for indirection
                               const K& k,                    // kernel
                               const CGAL::Dimension_tag<0>& tag,
                               const DiagonalizeTraits& diagonalize_traits)
{
  typedef typename K::Segment_3  Segment;
  typedef typename K::Point_3  Point;
  auto converter = [](const Segment& s, std::size_t idx) -> Point { return s[idx]; };

  // precondition: at least one element in the container.
  CGAL_precondition(first != beyond);

  return linear_least_squares_fitting_3
    (make_subiterator<Point, 2> (first, converter),
     make_subiterator<Point, 2> (beyond),
     line,c,(Point*)nullptr,k,tag,
     diagonalize_traits);
} // end linear_least_squares_fitting_segments_3

} // end namespace internal

} //namespace CGAL

#endif // CGAL_LINEAR_LEAST_SQUARES_FITTING_SEGMENTS_3_H
