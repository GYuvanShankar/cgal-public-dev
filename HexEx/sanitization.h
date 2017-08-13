#include<CGAL/Linear_cell_complex_for_combinatorial_map.h>
#include<cstdlib>
#include<iostream>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Aff_transformation_3.h>
#include <CGAL/Vector_3.h>
#include"typedefs.h"
#include"functions.h"
#include"handles.h"

void propagate_parameters(LCC_3 &lcc, Dart_handle& dh, std::vector<std::vector<Aff_transformation>>& g){
  Point p = (lcc.info(dh)).parameters;
  for(LCC_3::One_dart_per_incident_cell_range<2,0>::iterator f = lcc.one_dart_per_incident_cell<2,0>(dh).begin(), fend = lcc.one_dart_per_incident_cell<2,0>(dh).end(); f!=fend; f++){// for the vertex in each cell
    (lcc.info(f)).parameters = p;
    if(lcc.is_free<2>(f)) continue;
    else{
      Dart_handle fopp = lcc.opposite(lcc.beta(f, 2));
      //update the parameter 
      (lcc.info(fopp)).parameters = p.transform(g[(lcc.info(f)).cell_no][(lcc.info(fopp)).cell_no]);
    }    
  }
}

Aff_transformation get_transition(LCC_3& lcc, Dart_handle& dh, Dart_handle& singular_edge, std::vector<std::vector<Aff_transformation>>& g){
// If dh and singular_edge belong to the same tet, transition function is identity
  if((lcc.info(dh)).cell_no == (lcc.info(singular_edge).cell_no))
    return Aff_transformation(1,0,0,0,1,0,0,0,1,1); //return identity
  else{
    Aff_transformation temp(1,0,0,0,1,0,0,0,1,1);
    for(LCC_3::One_dart_per_incident_cell_range<2,1>::iterator f = lcc.one_dart_per_incident_cell<2,1>(dh).begin(),
fend = lcc.one_dart_per_incident_cell<2,1>(dh).end(); f != fend; f++){
      if(!lcc.is_free<3>(f)){
        if((lcc.info(dh)).cell_no == (lcc.info(singular_edge).cell_no)) return temp;
        else temp = temp * g[(lcc.info(f)).cell_no][(lcc.info(lcc.beta(f, 3))).cell_no]; 
      }
    }
    return temp;   
  }  
}

bool is_singular(LCC_3&lcc, Dart_handle& e, std::vector<std::vector<Aff_transformation>>& g){
  Aff_transformation temp(1, 0, 0, 0, 1, 0, 0, 0, 1, 1), identity(1, 0, 0, 0, 1, 0, 0, 0, 1, 1);
  for(LCC_3::One_dart_per_incident_cell_range<2,1>::iterator f = lcc.one_dart_per_incident_cell<2,1>(e).begin(),
fend = lcc.one_dart_per_incident_cell<2,1>(e).end(); f != fend; f++){
      if(!lcc.is_free<3>(f)){
        temp = temp * g[(lcc.info(f)).cell_no][(lcc.info(lcc.beta(f, 3))).cell_no]; 
      }
    }
  return !(temp.m(0,0) == 1 && temp.m(0,1) == 0 && temp.m(0,2) == 0 && temp.m(0,3) == 0
&&  temp.m(1,0) == 0 &&temp.m(1,1) == 1 &&temp.m(1,2) == 0 && temp.m(1,3) == 0 && temp.m(2,0) == 0 
&& temp.m(2,1) == 0 && temp.m(2,2) == 1 && temp.m(2,3) == 0); 
}

Dart_handle get_singular_edge(LCC_3& lcc, Dart_handle& v, std::vector<std::vector<Aff_transformation>>& g){
  for(LCC_3::One_dart_per_incident_cell_range<1,0>::iterator e = lcc.one_dart_per_incident_cell<1,0>(v).begin(), eend = lcc.one_dart_per_incident_cell<1,0>(v).end(); e != eend; e++){
    if(is_singular(lcc, e, g)) return e;
  }
  return Dart_handle();
}

Point_3 get_projected_parameters(LCC_3& lcc, Dart_handle& dh, Dart_handle& singular_edge, std::vector<std::vector<Aff_transformation>>& g){
  Point_3 from = lcc.point(dh), to = lcc.point(lcc.opposite(dh));
  Vector_3 dir(from, to);
  Aff_transformation at= get_transition(lcc, dh, singular_edge, g);
  dir = dir.transform(at);
}

void fix_singularity(LCC_3& lcc, Dart_handle& dh, std::vector<std::vector<Aff_transformation>>& g){
  Point_3 param = (lcc.info(dh)).parameters; Point_3 p;
  if(lcc.info(dh).singular_edges == 2){
    Dart_handle singular_edge = get_singular_edge(lcc, dh, g);
    p = get_projected_parameters(lcc, dh, singular_edge, g);
 }
  else{
     p = Point_3(std::round(param[0]), std::round(param[1]), std::round(param[2]));
  }
  Vector_3 v(param, p);
  double eps = 1e-4;
  if(v.squared_length() < eps)
    (lcc.info(dh)).parameters = p;
}

void check_singularity(LCC_3& lcc, std::vector<std::vector<Aff_transformation>>& g){
  for(LCC_3::One_dart_per_cell_range<1>::iterator e = lcc.one_dart_per_cell<1>().begin(), eend = lcc.one_dart_per_cell<1>().end(); e != eend; e++){
    if(is_singular(lcc, e, g)){
      for(LCC_3::Dart_of_cell_range<1>::iterator d = lcc.darts_of_cell<1>(e).begin(), dend = lcc.darts_of_cell<1>(e).end(); d != dend; d++){
        if((lcc.info(d)).singular == true) (lcc.info(d)).singular_edges++;
        else{
          (lcc.info(d)).singular = true; (lcc.info(d)).singular_edges = 1;
        }
      }      
    } 
  }
}


void truncate_precision(LCC_3& lcc, std::vector<std::vector<Aff_transformation>>& g){
  
  for(LCC_3::One_dart_per_cell_range<0>::iterator v=lcc.one_dart_per_cell<0>().begin(), vend=lcc.one_dart_per_cell<0>().end(); v!=vend; ++v){
    double max = std::numeric_limits<double>::min();
    Point p;// = (lcc.info(v)).parameters;  
    for(LCC_3::Dart_of_cell_range<0>::iterator dh = lcc.darts_of_cell<0>(v).begin(), dhend = lcc.darts_of_cell<0>(v).end(); dh != dhend; dh++){
      p = (lcc.info(dh)).parameters; 
      max = std::max(max, std::abs(p[0])); max = std::max(max, std::abs(p[1])); max = std::max(max, std::abs(p[2]));
    }
    p =  (lcc.info(v)).parameters;
    double delta = std::pow(2, std::ceil(std::log(max)/std::log(2)));
    //std::cout<<"Delta: "<<delta<<std::endl;
  //   double delta = std::pow(2, 10);
    std::vector<double> temp(3);
    for(unsigned int i = 0; i < 3; ++i){
      int sign = std::signbit(p[i]) ? -1 : 1;
      temp[i] = p[i] + sign*delta;
      temp[i] = temp[i] - sign*delta;
    }
//Now store tmp into parameters. This "sanitizes" the parameters. 
    ((lcc.info(v)).parameters) = Point(temp[0], temp[1], temp[2]);
    
//singularity check
    if((lcc.info(v)).singular == true){
      fix_singularity(lcc, v, g); //TODO:fix it!
    }
    propagate_parameters(lcc, v, g);  
  }

}

void sanitize(LCC_3& lcc, std::vector<std::vector<Aff_transformation>>& g){
  check_singularity(lcc, g);
  truncate_precision(lcc, g);
}
