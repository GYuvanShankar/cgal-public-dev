// Copyright (c) 2020 GeometryFactory (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
//
// Author(s)     : Andreas Fabri, Ayush Saraswat


#ifndef CGAL_EMBREE_AABB_TREE_H
#define CGAL_EMBREE_AABB_TREE_H

// #include <CGAL/license/Embree.h>
#include <CGAL/intersections.h>
#include <CGAL/boost/graph/helpers.h>

#include <embree3/rtcore.h>

#include <boost/optional.hpp>

#include <vector>
#include <limits>
#include <unordered_map>

namespace CGAL {
namespace Embree {


template <typename TriangleMesh, bool b>
struct Id2descriptor
{};

template <typename TriangleMesh>
struct Id2descriptor<TriangleMesh,true>
{
  typedef typename boost::graph_traits<TriangleMesh>::face_descriptor face_descriptor;
  std::vector<face_descriptor> faceDescriptors;

  Id2descriptor(){}
  Id2descriptor(const TriangleMesh& tm)
  {
    for(face_descriptor fd : faces(tm))
    {
      faceDescriptors.push_back(fd);
    }
  }

  face_descriptor operator()(unsigned int i) const
  {
     return faceDescriptors[i];
  }
};

template <typename TriangleMesh>
struct Id2descriptor<TriangleMesh,false>
{
  typedef typename boost::graph_traits<TriangleMesh>::face_descriptor face_descriptor;

  Id2descriptor(){}
  Id2descriptor(const TriangleMesh& tm)
  {}


  face_descriptor operator()(unsigned int i) const
  {
     return face_descriptor(i);
  }
};


  // AF: This is what you had called SM

template <typename TriangleMesh, typename GeomTraits, bool b>
struct Triangle_mesh_geometry {

  typedef typename boost::graph_traits<TriangleMesh>::face_descriptor face_descriptor;
  typedef typename boost::graph_traits<TriangleMesh>::halfedge_descriptor halfedge_descriptor;
  typedef typename boost::graph_traits<TriangleMesh>::vertex_descriptor vertex_descriptor;

  typedef typename boost::property_map<TriangleMesh, vertex_point_t>::const_type Vertex_point_map;

  typedef std::pair<face_descriptor, TriangleMesh*> Primitive_id;
  typedef typename GeomTraits::Point_3 Point;
  typedef typename GeomTraits::Triangle_3 Triangle;
  typedef typename GeomTraits::Ray_3 Ray;
  typedef typename GeomTraits::Vector_3 Vector;

  const TriangleMesh* surface_mesh;
  RTCGeometry rtc_geometry;
  unsigned int rtc_geomID;
  Vertex_point_map vpm;
  Id2descriptor<TriangleMesh, b> id2desc;


  Triangle_mesh_geometry()
  {}


  Triangle_mesh_geometry(const TriangleMesh& tm)
    : surface_mesh(&tm), vpm(get(CGAL::vertex_point, tm)), id2desc(tm)
  {}


  static void bound_function(const struct RTCBoundsFunctionArguments* args)
  {
    Triangle_mesh_geometry* self = (Triangle_mesh_geometry*) args->geometryUserPtr;
    RTCBounds* bounds_o = args->bounds_o;
    unsigned int primID = args->primID;

    Bbox_3 bb;
    face_descriptor fd = self->id2desc(primID);
    halfedge_descriptor hf = halfedge(fd, *self->surface_mesh);
    for(halfedge_descriptor hi : halfedges_around_face(hf, *(self->surface_mesh))){
        vertex_descriptor vi = target(hi, *(self->surface_mesh));
        bb += get(self->vpm,vi).bbox();
    }
    bounds_o->lower_x = bb.xmin();
    bounds_o->lower_y = bb.ymin();
    bounds_o->lower_z = bb.zmin();
    bounds_o->upper_x = bb.xmax();
    bounds_o->upper_y = bb.ymax();
    bounds_o->upper_z = bb.zmax();
  }


  static void intersection_function(const RTCIntersectFunctionNArguments* args)
  {
    Triangle_mesh_geometry* self = (Triangle_mesh_geometry*) args->geometryUserPtr;
    int* valid = args->valid;
    if (!valid[0]) {
      return;
    }

    struct RTCRayHit* rayhit = (RTCRayHit*)args->rayhit;
    unsigned int primID = args->primID;

    assert(args->N == 1);
    std::vector<Point> face_points;
    face_points.reserve(3);

    face_descriptor fd = self->id2desc(primID);
    halfedge_descriptor hf = halfedge(fd, *self->surface_mesh);
    for(halfedge_descriptor hi : halfedges_around_face(hf, *(self->surface_mesh))){
        vertex_descriptor vi = target(hi, *(self->surface_mesh));
        Point data = get(self->vpm,vi);
        face_points.push_back(data);
    }
    Triangle face(face_points[0], face_points[1], face_points[2]);

    Vector ray_direction(rayhit->ray.dir_x, rayhit->ray.dir_y, rayhit->ray.dir_z);
    Point ray_orgin(rayhit->ray.org_x, rayhit->ray.org_y, rayhit->ray.org_z);
    Ray ray(ray_orgin, ray_direction);

    auto v = CGAL::intersection(ray, face);
    if(v){
        rayhit->hit.geomID = self->rtc_geomID;
        rayhit->hit.primID = primID;
        if (const Point *intersection_point = boost::get<Point>(&*v) ){
            float _distance = sqrt(CGAL::squared_distance(ray_orgin, *intersection_point));
            rayhit->ray.tfar = _distance;
        }
    }
  }


  void insert_primitives()
  {
    rtcSetGeometryUserPrimitiveCount(rtc_geometry, num_faces(*surface_mesh));
    rtcSetGeometryUserData(rtc_geometry, this);

    // AF: For the next two you have to find out how to write
    // the function pointer for a static member function

    // Ayush: for a static member function, we can directly pass a pointer, so the below should work fine.
    // https://isocpp.org/wiki/faq/pointers-to-members

    rtcSetGeometryBoundsFunction(rtc_geometry, bound_function, nullptr);
    rtcSetGeometryIntersectFunction(rtc_geometry, intersection_function);
    rtcCommitGeometry(rtc_geometry);

    rtcReleaseGeometry(rtc_geometry);
  }


  Primitive_id primitive_id(unsigned int primID) const
  {
    return std::make_pair(id2desc(primID), const_cast<TriangleMesh*>(surface_mesh));
  }
};

/**
 * \ingroup PkgEmbreeRef
 * This class...
 */


//  AF:  Geometry is the above class
// AF: For GeomTraits you take a kernel, that is Simple_cartesian
template <typename Geometry, typename GeomTraits>
class AABB_tree {

  typedef typename Geometry::Primitive_id Primitive_id;
public:
  typedef std::pair<typename Geometry::Point, Primitive_id> Intersection_and_primitive_id;

  RTCDevice device;
  RTCScene scene;

  std::unordered_map<unsigned int, Geometry> id2geometry;
  std::list<Geometry> geometries;

public:
  AABB_tree()
  {
    device = rtcNewDevice(NULL);
    scene = rtcNewScene(device);
  }


  /// T is the surface mesh
  template<typename T>
  void insert (const T& t)
  {
    geometries.push_back(Geometry(t));
    Geometry& geometry = geometries.back();

    geometry.rtc_geometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_USER);
    geometry.rtc_geomID = rtcAttachGeometry(scene, geometry.rtc_geometry);
    id2geometry.insert({geometry.rtc_geomID, geometry});
    geometry.insert_primitives();
    rtcCommitScene(scene);
  }


  template<typename Ray>
  boost::optional<Intersection_and_primitive_id> first_intersection(const Ray& query) const
  {
    struct RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    struct RTCRayHit rayhit;

    rayhit.ray.org_x =  query.source().x(); /*POINT.X*/
    rayhit.ray.org_y =  query.source().y(); /*POINT.Y*/
    rayhit.ray.org_z =  query.source().z(); /*POINT.Z*/

    rayhit.ray.dir_x = query.direction().dx()/ sqrt(square(query.direction().dx()) + square(query.direction().dy()) + square(query.direction().dz()));
    rayhit.ray.dir_y = query.direction().dy()/ sqrt(square(query.direction().dx()) + square(query.direction().dy()) + square(query.direction().dz()));
    rayhit.ray.dir_z = query.direction().dz()/ sqrt(square(query.direction().dx()) + square(query.direction().dy()) + square(query.direction().dz()));

    rayhit.ray.tnear = 0;
    rayhit.ray.tfar = std::numeric_limits<float>::infinity();
    rayhit.ray.mask = 0;
    rayhit.ray.flags = 0;

    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;

    rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

    rtcIntersect1(scene, &context, &rayhit);

    unsigned int rtc_geomID = rayhit.hit.geomID;
    if(rtc_geomID == RTC_INVALID_GEOMETRY_ID){
      return boost::none;
    }

    float factor = rayhit.ray.tfar/ sqrt(square(rayhit.ray.dir_x)+ square(rayhit.ray.dir_y)+ square(rayhit.ray.dir_z));
    float outX = rayhit.ray.org_x + factor * rayhit.ray.dir_x;
    float outY = rayhit.ray.org_y + factor * rayhit.ray.dir_y;
    float outZ = rayhit.ray.org_z + factor * rayhit.ray.dir_z;
    typename Geometry::Point p(outX, outY, outZ);

    Geometry geometry = id2geometry.at(rtc_geomID);
    return boost::make_optional(std::make_pair(p, geometry.primitive_id(rayhit.hit.primID)));
  }


  template<typename Ray>
  boost::optional<Primitive_id> first_intersected_primitive(const Ray& query) const
  {
    struct RTCIntersectContext context;
    rtcInitIntersectContext(&context);

    struct RTCRayHit rayhit;

    rayhit.ray.org_x =  query.source().x(); /*POINT.X*/
    rayhit.ray.org_y =  query.source().y(); /*POINT.Y*/
    rayhit.ray.org_z =  query.source().z(); /*POINT.Z*/

    rayhit.ray.dir_x = query.direction().dx()/ sqrt(square(query.direction().dx()) + square(query.direction().dy()) + square(query.direction().dz()));
    rayhit.ray.dir_y = query.direction().dy()/ sqrt(square(query.direction().dx()) + square(query.direction().dy()) + square(query.direction().dz()));
    rayhit.ray.dir_z = query.direction().dz()/ sqrt(square(query.direction().dx()) + square(query.direction().dy()) + square(query.direction().dz()));

    rayhit.ray.tnear = 0;
    rayhit.ray.tfar = std::numeric_limits<float>::infinity();
    rayhit.ray.mask = 0;
    rayhit.ray.flags = 0;

    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;

    rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

    rtcIntersect1(scene, &context, &rayhit);

    unsigned int rtc_geomID = rayhit.hit.geomID;
    if(rtc_geomID == RTC_INVALID_GEOMETRY_ID){
      return boost::none;
    }

    Geometry geometry = id2geometry.at(rtc_geomID);

    return boost::make_optional(geometry.primitive_id(rayhit.hit.primID));
  }

};

} // namespace Embree
} // namespace CGAL
#endif // CGAL_EMBREE_AABB_TREE_H