// ============================================================================
//
// Copyright (c) 1998 The CGAL Consortium
//
// This software and related documentation is part of an INTERNAL release
// of the Computational Geometry Algorithms Library (CGAL). It is not
// intended for general use.
//
// ----------------------------------------------------------------------------
//
// release       :
// release_date  :
//
// file          : include/CGAL/Interval_arithmetic/IA_leda_real.h
// revision      : $Revision$
// revision_date : $Date$
// package       : Interval Arithmetic
// author(s)     : Sylvain Pion <Sylvain.Pion@sophia.inria.fr>
//
// coordinator   : INRIA Sophia-Antipolis (<Mariette.Yvinec@sophia.inria.fr>)
//
// ============================================================================

#ifndef CGAL_IA_LEDA_REAL_H
#define CGAL_IA_LEDA_REAL_H

template <>
inline
CGAL_Interval_nt_advanced
CGAL_convert_to <CGAL_Interval_nt_advanced> (const leda_real &z)
{
#ifdef CGAL_IA_DEBUG
    CGAL_assertion(CGAL_FPU_get_rounding_mode() == CGAL_FPU_PLUS_INFINITY);
#endif
    CGAL_FPU_set_rounding_to_nearest();
    const double approx = CGAL_to_double(z);
    const double rel_error = z.get_double_error();
    CGAL_FPU_set_rounding_to_infinity();
    const CGAL_Interval_nt_advanced result =
	( CGAL_Interval_nt_advanced(-rel_error,rel_error) + 1 )
	* CGAL_Interval_nt_advanced(approx);
#ifdef CGAL_IA_DEBUG
    CGAL_FPU_set_rounding_to_nearest();
    CGAL_assertion( leda_real(result.lower_bound()) <= z &&
		    leda_real(result.upper_bound()) >= z );
    CGAL_FPU_set_rounding_to_infinity();
#endif
    return result;
}

#endif	 // CGAL_IA_LEDA_REAL_H
