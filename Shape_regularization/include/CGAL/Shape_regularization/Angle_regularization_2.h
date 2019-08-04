#ifndef CGAL_SHAPE_REGULARIZATION_ANGLE_REGULARIZATION_2
#define CGAL_SHAPE_REGULARIZATION_ANGLE_REGULARIZATION_2

// #include <CGAL/license/Shape_regularization.h>

#include <map>
#include <utility>
#include <vector>

#include <CGAL/property_map.h>
#include <CGAL/Shape_regularization/internal/utils.h>
#include <CGAL/Shape_regularization/internal/Segment_data_2.h>
#include <CGAL/Shape_regularization/internal/Grouping_segments_2.h>
#include <CGAL/Shape_regularization/internal/Conditions_angles_2.h>


namespace CGAL {
namespace Regularization {

  template<
    typename GeomTraits, 
    typename InputRange,
    typename SegmentMap>
  class Angle_regularization_2 {
  public:
    using Traits = GeomTraits;
    using Input_range = InputRange;
    using Segment_map = SegmentMap;
    using FT = typename GeomTraits::FT;
    using Segment = typename GeomTraits::Segment_2;
    using Point = typename GeomTraits::Point_2;
    using Segment_data = typename internal::Segment_data_2<Traits>;
    using Conditions = typename internal::Conditions_angles_2<Traits>;
    using Grouping = internal::Grouping_segments_2<Traits, Conditions>;
    using Vector  = typename GeomTraits::Vector_2;
    using Targets_map = std::map <std::pair<std::size_t, std::size_t>, std::pair<FT, std::size_t>>;
    using Relations_map = std::map <std::pair<std::size_t, std::size_t>, std::pair<int, std::size_t>>;

    Angle_regularization_2 (
      InputRange& input_range, 
      const FT theta_max = FT(25),
      const SegmentMap segment_map = SegmentMap()) :
    m_input_range(input_range),
    m_theta_max(theta_max),
    m_segment_map(segment_map) {

      CGAL_precondition(m_input_range.size() > 0);
    }

    FT target_value(const std::size_t i, const std::size_t j) {
      check_groups();
 
      CGAL_precondition(m_segments.size() > 0);
      CGAL_precondition(m_segments.find(i) != m_segments.end());
      CGAL_precondition(m_segments.find(j) != m_segments.end());

      const Segment_data & s_i = m_segments.at(i);
      const Segment_data & s_j = m_segments.at(j);

      const FT mes_ij = s_i.m_orientation - s_j.m_orientation;
      const double mes90 = std::floor(CGAL::to_double(mes_ij / FT(90)));

      const FT to_lower = FT(90) *  static_cast<FT>(mes90)          - mes_ij;
      const FT to_upper = FT(90) * (static_cast<FT>(mes90) + FT(1)) - mes_ij;

      const FT tar_val = CGAL::abs(to_lower) < CGAL::abs(to_upper) ? to_lower : to_upper;
      if (CGAL::abs(tar_val) < bound(i) + bound(j)) {
        m_targets[std::make_pair(i, j)] = tar_val;
 
        int rel_val;
        if (CGAL::abs(to_lower) < CGAL::abs(to_upper))
            rel_val = ((90 * static_cast<int>(mes90)) % 180 == 0 ? 0 : 1);
        else
            rel_val = ((90 * static_cast<int>(mes90 + 1.0)) % 180 == 0 ? 0 : 1);
        
        m_relations[std::make_pair(i, j)] = rel_val;
      } 
  
      return tar_val;
    }

    FT bound(const std::size_t i) const {
      CGAL_precondition(i >= 0 && i < m_input_range.size());
      return m_theta_max;
    }

    bool check_segments() const {
      if(m_segments.size() == 0) return false;
      return true;
    }

    template<typename OutputIterator>
    OutputIterator parallel_groups(OutputIterator groups) {
      // CGAL_precondition(m_parallel_groups_angle_map.size() > 0);

      for(const auto & mi : m_parallel_groups_angle_map) {
        const std::vector <std::size_t> & group = mi.second;
        *(groups++) = group;
      }
      return groups;
    }

    void update(std::vector<FT> & result) {

      const std::size_t n = m_input_range.size();
      Targets_map targets;
      Relations_map relations;
      std::map <std::size_t, Segment_data> segments;
      std::map <FT, std::vector<std::size_t>> parallel_groups_angle_map;

      CGAL_precondition(m_targets.size() > 0);
      CGAL_precondition(m_targets.size() == m_relations.size());

      for (const auto & group : m_groups) {
        if (group.size() < 2) continue; 

        parallel_groups_angle_map.clear();
        segments.clear();
        targets.clear();
        build_grouping_data(group, segments, targets, relations);

        if(segments.size() > 0) {
          m_grouping.make_groups(m_theta_max, n, segments, result, parallel_groups_angle_map, targets, relations);
          rotate_parallel_segments(parallel_groups_angle_map);
        }
      }
    }

    template<typename Range, typename IndexMap = CGAL::Identity_property_map<std::size_t>>
  	void add_group(const Range& group, const IndexMap index_map = IndexMap()) { 
      std::vector<std::size_t> gr;
      for (const auto & item : group) {
        const std::size_t seg_index = get(index_map, item);
        gr.push_back(seg_index);
      }

      if (gr.size() > 1) {
        m_groups.push_back(gr);
        build_segment_data_map(gr);
      }
    }

  private:
    Input_range& m_input_range;
    const Segment_map  m_segment_map;
    std::map <std::size_t, Segment_data> m_segments;
    const FT m_theta_max;
    std::map <std::pair<std::size_t, std::size_t>, FT> m_targets;
    std::map <std::pair<std::size_t, std::size_t>, int> m_relations;
    Grouping m_grouping;
    std::map <FT, std::vector<std::size_t>> m_parallel_groups_angle_map;
    std::vector <std::vector<std::size_t>> m_groups;

    void check_groups() {
      if(m_groups.size() == 0) {
        std::vector<std::size_t> vec;
        vec.resize(m_input_range.size());
        std::iota(vec.begin(), vec.end(), 0);

        add_group(vec);
      }
    }

    void build_segment_data_map(const std::vector<std::size_t> & group) {
      if (group.size() < 2) return;

      for(std::size_t i = 0; i < group.size(); ++i) {
        const std::size_t seg_index = group[i];

        CGAL_precondition(m_segments.find(seg_index) == m_segments.end());
        if(m_segments.find(seg_index) != m_segments.end())
          continue;

        const Segment& seg = get(m_segment_map, *(m_input_range.begin() + seg_index));
        const Segment_data seg_data(seg, seg_index);

        m_segments.emplace(seg_index, seg_data);
      }
    }

    void build_grouping_data (const std::vector <std::size_t> & group,
                              std::map <std::size_t, Segment_data> & segments,
                              Targets_map & targets,
                              Relations_map & relations) {
      for (const std::size_t it : group) {
        const std::size_t seg_index = it;

        CGAL_precondition(m_segments.find(seg_index) != m_segments.end());
        const Segment_data& seg_data = m_segments.at(seg_index);

        segments.emplace(seg_index, seg_data);
        std::size_t tar_index = 0;

        auto ri = m_relations.begin();
        for (const auto & ti : m_targets) {
          const std::size_t seg_index_tar_i = ti.first.first;
          const std::size_t seg_index_tar_j = ti.first.second;
          const FT tar_val = ti.second;

          const std::size_t seg_index_rel_i = ri->first.first;
          const std::size_t seg_index_rel_j = ri->first.second;
          const int rel_val = ri->second;

          CGAL_precondition(seg_index_tar_i == seg_index_rel_i);
          CGAL_precondition(seg_index_tar_j == seg_index_rel_j);

          if (seg_index_tar_i == seg_index && seg_index_rel_i == seg_index) {
            targets[std::make_pair(seg_index_tar_i, seg_index_tar_j)] = std::make_pair(tar_val, tar_index);
            relations[std::make_pair(seg_index_rel_i, seg_index_rel_j)] = std::make_pair(rel_val, tar_index);
          }

          ++ri;
          ++tar_index;
        }
      }

      CGAL_postcondition(targets.size() == relations.size());
    }

    void rotate_parallel_segments(const std::map <FT, std::vector<std::size_t>> & parallel_groups_angle_map) {
      for (const auto & mi : parallel_groups_angle_map) {
        const FT theta = mi.first;
        const std::vector<std::size_t> & group = mi.second;
        if(m_parallel_groups_angle_map.find(theta) == m_parallel_groups_angle_map.end()) {
          m_parallel_groups_angle_map[theta] = group;
        }

        // Each group of parallel segments has a normal vector that we compute with alpha.
        const FT x = static_cast<FT>(cos(CGAL::to_double(theta * static_cast<FT>(CGAL_PI) / FT(180))));
        const FT y = static_cast<FT>(sin(CGAL::to_double(theta * static_cast<FT>(CGAL_PI) / FT(180))));

        Vector v_dir = Vector(x, y);
        const Vector v_ort = Vector(-v_dir.y(), v_dir.x());
        
        const FT a = v_ort.x();
        const FT b = v_ort.y();

        // Rotate segments with precision.
        for (std::size_t i = 0; i < group.size(); ++i) {

          std::size_t seg_index = group[i];

          // Compute equation of the supporting line of the rotated segment.
          const Segment_data & seg_data = m_segments.at(seg_index);
          const Point & barycentre = seg_data.m_barycentre;
          const FT c = -a * barycentre.x() - b * barycentre.y();

          set_orientation(seg_index, a, b, c, v_dir);
        }
      }
    }

    void set_orientation(const std::size_t i, const FT a, const FT b, const FT c, const Vector &direction) {
      Vector l_direction = direction;
      
      if (l_direction.y() < FT(0) || (l_direction.y() == FT(0) && l_direction.x() < FT(0))) 
        l_direction = -l_direction;
      FT x1, y1, x2, y2;
      const Segment_data & seg_data = m_segments.at(i);
      const Point barycentre = seg_data.m_barycentre;
      const FT length = seg_data.m_length;
      if (CGAL::abs(l_direction.x()) > CGAL::abs(l_direction.y())) { 
        x1 = barycentre.x() - length * l_direction.x() / FT(2);
        x2 = barycentre.x() + length * l_direction.x() / FT(2);

        y1 = (-c - a * x1) / b;
        y2 = (-c - a * x2) / b;
      } 
      else {
        y1 = barycentre.y() - length * l_direction.y() / FT(2);
        y2 = barycentre.y() + length * l_direction.y() / FT(2);

        x1 = (-c - b * y1) / a;
        x2 = (-c - b * y2) / a;
      }
      const Point source = Point(x1, y1);
      const Point target = Point(x2, y2);

      m_input_range[i] = Segment(source, target);
    } 

  };

} // namespace Regularization
} // namespace CGAL

#endif // CGAL_SHAPE_REGULARIZATION_ANGLE_REGULARIZATION_2
