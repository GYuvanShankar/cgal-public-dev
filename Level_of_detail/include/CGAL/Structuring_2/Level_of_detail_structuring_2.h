#ifndef CGAL_LEVEL_OF_DETAIL_STRUCTURING_2_H
#define CGAL_LEVEL_OF_DETAIL_STRUCTURING_2_H

#if defined(WIN32) || defined(_WIN32) 
#define PSR "\\"
#define PN "\r\n"
#else 
#define PSR "/" 
#define PN "\n"
#endif 

// STL includes.
#include <map>
#include <vector>
#include <cmath>
#include <unordered_set>

// CGAL includes.
#include <CGAL/number_utils.h>
#include <CGAL/utils.h>
#include <CGAL/Kd_tree.h>
#include <CGAL/Search_traits_2.h>
#include <CGAL/Fuzzy_sphere.h>
#include <CGAL/intersections.h>

// New CGAL includes.
#include <CGAL/Mylog/Mylog.h>
#include <CGAL/Level_of_detail_enum.h>

// NOTES:
// 1. Clutter has to be handled separately. See Section 2 Clutter.
// 2. All points associated with a line should come with epsilon parameter.

namespace CGAL {

	namespace LOD {

		template<class KernelTraits>
		class Level_of_detail_structuring_2 {

		private:
			typedef std::pair<int, int> Int_pair;

			class My_pair_equal {

			public:
				bool operator()(const Int_pair &a, const Int_pair &b) const {

					if ((a.first == b.first  && a.second == b.second) ||
						(a.first == b.second && a.second == b.first)) return true;

					return false;
				}
			};

			class My_pair_hasher {
				
			public:
				size_t operator()(const Int_pair &in) const {
					return std::hash<int>()(in.first + in.second);
				}
			};

		public:
			typedef KernelTraits Traits;

			typedef typename Traits::Point_2   Point;
			typedef typename Traits::Line_2    Line;
			typedef typename Traits::Segment_2 Segment;
			typedef typename Traits::FT 	   FT;

			typedef std::map<int, Point>             Points;
			typedef std::map<int, std::vector<int> > Connected_components;
			typedef std::vector<Line> 				 Lines;
			typedef std::vector<Segment> 			 Segments;

			typedef typename Connected_components::const_iterator 	 CC_iterator;
			typedef typename std::unordered_set<int>::const_iterator SS_iterator;

			typedef typename std::unordered_set<Int_pair, My_pair_hasher, My_pair_equal>::const_iterator PP_iterator;

        	typedef CGAL::Search_traits_2<Traits>       Search_traits_2;
        	typedef CGAL::Fuzzy_sphere<Search_traits_2> Fuzzy_circle;
        	typedef CGAL::Kd_tree<Search_traits_2>      Tree;

			// Remove later or change.
			enum class Occupancy_method { ALL, ONLY_PROJECTED };
			enum class Adjacency_method { STRUCTURED };

			using Log = CGAL::LOD::Mylog;

			using Structured_points  = std::vector< std::vector<Point> >; 			  // new structured points
			using Structured_labels  = std::vector< std::vector<Structured_label> >;  // for each structured point store a label: linear or corner
			using Structured_anchors = std::vector< std::vector<std::vector<int> > >; // for each structured point store all associated primitives: indices of lines (may be 1 or 2)

			typename Traits::Compute_squared_distance_2 squared_distance;
			typename Traits::Compute_scalar_product_2   dot_product;
			
			typedef typename Traits::Intersect_2 Intersect;


			Level_of_detail_structuring_2(const Points &points, const Connected_components &components, const Lines &lines) :
			m_points(points), m_cc(components), m_lines(lines), m_tol(FT(1) / FT(10000)), m_big_value(FT(100000000000000)), 
			m_eps_set(false), m_save_log(true), m_resample(true), m_empty(true), m_corner_algorithm(Structuring_corner_algorithm::GRAPH_BASED),
			m_adjacency_threshold_method(Structuring_adjacency_threshold_method::LOCAL), m_adjacency_threshold(-FT(1)),
			m_min_seg_points(2), m_max_neigh_segments(4), m_use_global_everywhere(true), m_silent(false), m_local_adjacency_value(-FT(1)) {

				assert(components.size() == lines.size());
			}


			void make_silent(const bool new_state) {
				m_silent = new_state;
			}

			void save_log(const bool new_state) {
				m_save_log = new_state;
			}

			void resample(const bool new_state) {
				m_resample = new_state;
			}

			bool is_empty() const {
				return m_empty;
			}

			void set_corner_algorithm(const Structuring_corner_algorithm new_algorithm) {
				m_corner_algorithm = new_algorithm;
			}

			void set_adjacency_threshold_method(const Structuring_adjacency_threshold_method new_method) {
				m_adjacency_threshold_method = new_method;
			}

			void set_adjacency_threshold(const FT new_value) {

				assert(new_value > FT(0));
				m_adjacency_threshold = new_value;
			}

			FT get_local_adjacency_value() {

				assert(m_local_adjacency_value > FT(0));
				return m_local_adjacency_value;
			}

			// This is a 2D version of the algorithm in the paper:
			// Surface Reconstruction Through Point Set Structuring, F. Lafarge, P. Alliez.
			int structure_point_set() {

				// (START) Create log.
				Log log;
				if (m_save_log) log.out << "START EXECUTION" + std::string(PN) + "" + std::string(PN) + "" + std::string(PN) + "";
				
				// -----------------------------------------

				auto number_of_structured_segments = -1;


				// (1) Project all points onto the given lines.
				project();

				if (m_save_log) log.out << "(1) All points are projected onto the given lines. The results are saved in tmp->projected" << std::endl;


				// (2) Compute min, average, and max distances from all points to the given lines.
				compute_distances();

				if (m_save_log) log.out << "(2) Min, avg, and max distances for each set of points are computed. The results are saved in tmp->distances" << std::endl;


				// (3) Find epsilon for each set of points.
				// Alternatively it can be set as one unique value using the corresponding function.
				if (!m_eps_set) {
					
					compute_epsilon_values();
					if (m_save_log) log.out << "(3) Epsilon values are computed for each component. The results are saved in tmp->epsilons" << std::endl;

				} else if (m_save_log) log.out << "(3) Epsilon values are set manually to one unique value." << std::endl;


				// (4) Find one unique segment for each set of points.
				find_segments();

				number_of_structured_segments = m_segments.size();

				if (m_save_log) log.out << "(4) Segments are extracted for each set of points. The results are saved in tmp->segments" << std::endl;


				// (5) Find side length Lp used in the occupancy grid for each segment.
				find_lp();

				if (m_save_log) log.out << "(5) Side lengths are found for each occupancy grid. The results are saved in tmp->lp" << std::endl;


				// (6) Fill in the occupancy grid for each segment.
				fill_in_occupancy_grid(Occupancy_method::ALL);

				if (m_save_log) log.out << "(6) The occupancy grid is projected and filled. The results are saved in tmp->occupancy" << std::endl;


				// (7) Create structured linear points using the occupancy grid above.
				create_linear_points();

				if (m_save_log) log.out << "(7) Linear points are created for each segment. The results are saved in tmp->linear_points" << std::endl;


				// (8) Insert corners between intersecting segments.
				if (m_save_log) log.out << "(8) Adding corners." << std::endl;

				add_corners(log);


				// (9) Extra: Create segment end points and labels.
				create_segment_end_points_labels_anchors();

				if (m_save_log) log.out << "(9) Segment end points and labels are found." << std::endl;


				// (10) Resample all points.
				if (m_resample) {

					resample_points();
					if (m_save_log) log.out << "(10) All segments are resampled." << std::endl;
				}


				// -------------------------------

				// (END) Save log.
				if (m_save_log) {
					log.out << "" + std::string(PN) + "" + std::string(PN) + "FINISH EXECUTION";
					log.save("structuring_2");
				}

				m_empty = false;
				return number_of_structured_segments;
			}

			const Structured_points& get_structured_points() const {

				assert(!is_empty());
				return m_str_points;
			}

			const Structured_labels& get_structured_labels() const {

				assert(!is_empty());
				return m_str_labels;
			}

			const Structured_anchors& get_structured_anchors() const {
				
				assert(!is_empty());
				return m_str_anchors;
			}

			const Structured_points& get_segment_end_points() const {
				
				assert(!is_empty());
				return m_segment_end_points;
			}

			const Structured_labels& get_segment_end_labels() const {
				
				assert(!is_empty());
				return m_segment_end_labels;
			}

			const Structured_anchors& get_segment_end_anchors() const {
				
				assert(!is_empty());
				return m_segment_end_anchors;
			}

			void set_epsilon(const FT value) {

				assert(value > FT(0));
				set_default_epsilon_values(value);
				m_eps_set = true;
			}

			void set_global_everywhere(const bool new_state) {
				m_use_global_everywhere = new_state;
			}

		private:
			const Points 			   &m_points;
			const Connected_components &m_cc;
			const Lines                &m_lines;

			std::vector<FT> m_min_dist, m_avg_dist, m_max_dist;
			std::vector<FT> m_eps;
			
			std::vector<FT>   m_lp;
			std::vector<int>  m_times;
			std::vector< std::vector<bool> > m_occupancy;

			Structured_points  m_str_points;
			Structured_labels  m_str_labels;
			Structured_anchors m_str_anchors;

			Points   m_projected;
			Segments m_segments;

			const FT m_tol;
			const FT m_big_value;

			std::vector<size_t> m_num_linear, m_num_corners;
			
			bool m_eps_set;
			bool m_save_log;
			bool m_resample;

			std::vector<std::unordered_set<int> > m_adjacency;
			std::unordered_set<Int_pair, My_pair_hasher, My_pair_equal> m_undirected_graph;

			Structured_points  m_segment_end_points;
			Structured_labels  m_segment_end_labels;
			Structured_anchors m_segment_end_anchors;

			bool m_empty;
			Structuring_corner_algorithm 		   m_corner_algorithm;
			Structuring_adjacency_threshold_method m_adjacency_threshold_method;
			FT 									   m_adjacency_threshold;

			size_t m_min_seg_points;
			size_t m_max_neigh_segments;
			bool m_use_global_everywhere;

			bool m_silent;
			FT m_local_adjacency_value;


			void project() {

				clear_projected();

				Log log;

				assert(!m_lines.empty());
				assert(m_lines.size() == m_cc.size());
				assert(m_projected.empty());

				size_t number_of_lines = 0;
				for (CC_iterator it = m_cc.begin(); it != m_cc.end(); ++it, ++number_of_lines) {
					
					const auto num_points = (*it).second.size();
					for (size_t i = 0; i < num_points; ++i) {

						const auto index = (*it).second[i];
						assert(index >= 0);

						const Point &p = m_points.at(index);
						m_projected[index] = project_onto_a_line(p, m_lines[number_of_lines]);

						assert(!std::isnan(CGAL::to_double(m_projected.at(index).x())) && !std::isnan(CGAL::to_double(m_projected.at(index).y())));
						assert(!std::isinf(CGAL::to_double(m_projected.at(index).x())) && !std::isinf(CGAL::to_double(m_projected.at(index).y())));

						if (m_save_log) log.out << "index: " << index << ", " << m_projected.at(index) << std::endl;
					}
					if (m_save_log) log.out << std::endl;
				}
				if (m_save_log) log.save("tmp" + std::string(PSR) + "projected");
			}

			void clear_projected() {

				m_projected.clear();
			}

			// Improve this function!
			Point project_onto_a_line(const Point &p, const Line &line) const {

				const auto a = line.point(0);
				const auto b = line.point(1);

				const auto projected = a + dot_product(p - a, b - a) / dot_product(b - a, b - a) * (b - a);
				
				if (std::isnan(CGAL::to_double(projected.x())) || std::isnan(CGAL::to_double(projected.y()))) return line.projection(p);
				else return projected;
			}

			void compute_distances() {
				
				clear_distances();

				Log log;

				assert(!m_cc.empty());
				assert(!m_points.empty());

				assert(m_min_dist.size() == m_cc.size());
				assert(m_avg_dist.size() == m_cc.size());
				assert(m_max_dist.size() == m_cc.size());

				assert(!m_projected.empty());
				assert(m_projected.size() == m_points.size());

				size_t number_of_components = 0;

				for (CC_iterator it = m_cc.begin(); it != m_cc.end(); ++it, ++number_of_components) {
					const auto num_points = (*it).second.size();

					m_min_dist[number_of_components] =  m_big_value; 
					m_avg_dist[number_of_components] =      FT(0); 
					m_max_dist[number_of_components] = -m_big_value;

					for (size_t i = 0; i < num_points; ++i) {
						
						const auto index = (*it).second[i];
						assert(index >= 0);

						const Point &p = m_points.at(index);
						const Point &q = m_projected.at(index);

						const FT dist = static_cast<FT>(CGAL::sqrt(CGAL::to_double(squared_distance(p, q))));

						m_min_dist[number_of_components] = CGAL::min(m_min_dist[number_of_components], dist);
						m_avg_dist[number_of_components] += dist;
						m_max_dist[number_of_components] = CGAL::max(m_max_dist[number_of_components], dist);
					}
					m_avg_dist[number_of_components] /= static_cast<FT>(num_points);

					assert(m_max_dist[number_of_components] <  m_big_value);
					assert(m_min_dist[number_of_components] > -m_big_value);

					if (m_save_log) log.out << "  min: " << m_min_dist[number_of_components] << 
					           				   "; avg: " << m_avg_dist[number_of_components] <<
					           				   "; max: " << m_max_dist[number_of_components] << std::endl;
				}
				if (m_save_log) log.save("tmp" + std::string(PSR) + "distances");
			}

			void clear_distances() {

				m_min_dist.clear(); m_avg_dist.clear(); m_max_dist.clear();
				m_min_dist.resize(m_cc.size(), FT(0)); m_avg_dist.resize(m_cc.size(), FT(0)); m_max_dist.resize(m_cc.size(), FT(0));
			}

			void compute_epsilon_values() {

				set_default_epsilon_values(FT(0));

				Log log;

				assert(!m_eps.empty());
				assert(m_eps.size() == m_cc.size());
				assert(!m_max_dist.empty());
				assert(m_max_dist.size() == m_eps.size());

				const auto number_of_components = m_max_dist.size();
				const FT default_eps = 0.025;

				for (size_t i = 0; i < number_of_components; ++i) {

					if (m_max_dist[i] < m_tol) m_eps[i] = default_eps;
					else m_eps[i] = m_max_dist[i];

					if (m_save_log) log.out << "eps: " << m_eps[i] << std::endl;
				}

				if (m_save_log) log.save("tmp" + std::string(PSR) + "epsilons");
				m_eps_set = true;
			}

			void set_default_epsilon_values(const FT value) {

				m_eps.clear();
				m_eps.resize(m_cc.size(), value);
			}

			// Improve this function.
			// Source is always left-sided and target is always right-sided. 
			// This is used for example in find_occupancy_cell() and create_linear_point() functions.
			void find_segments() {

				clear_segments();

				Log log;

				assert(!m_segments.empty());
				assert(m_segments.size() == m_lines.size() && m_segments.size() == m_cc.size());
				assert(!m_projected.empty());

				size_t number_of_segments = 0;
				for (CC_iterator it = m_cc.begin(); it != m_cc.end(); ++it, ++number_of_segments) {

					const auto num_points = (*it).second.size();

					auto minx =  m_big_value, miny =  m_big_value;
					auto maxx = -m_big_value, maxy = -m_big_value;				

					for (size_t i = 0; i < num_points; ++i) {
						const auto index = (*it).second[i];

						const Point &p = m_projected.at(index);

						minx = CGAL::min(minx, p.x());
						maxx = CGAL::max(maxx, p.x());

						miny = CGAL::min(miny, p.y());
						maxy = CGAL::max(maxy, p.y());
					}
					m_segments[number_of_segments] = Segment(Point(minx, miny), Point(maxx, maxy));

					auto v1 = m_lines[number_of_segments].to_vector();
					auto v2 = m_segments[number_of_segments].to_vector();

					// Rotate segments if needed.
					if ((v1.y() < FT(0) && v2.y() >= FT(0) && CGAL::abs(v1.y() - v2.y()) > m_tol) ||
						(v2.y() < FT(0) && v1.y() >= FT(0) && CGAL::abs(v1.y() - v2.y()) > m_tol) ||
						(v1.x() < FT(0) && v2.x() >= FT(0) && CGAL::abs(v1.x() - v2.x()) > m_tol) ||
						(v2.x() < FT(0) && v1.x() >= FT(0) && CGAL::abs(v1.x() - v2.x()) > m_tol)) {

						m_segments[number_of_segments] = Segment(Point(minx, maxy), Point(maxx, miny));

						if (m_save_log) log.out << "seg: " << m_segments[number_of_segments].source() << " ----- " << m_segments[number_of_segments].target() << std::endl;
						continue;
					}
					if (m_save_log) log.out << "seg: " << m_segments[number_of_segments].source() << " ----- " << m_segments[number_of_segments].target() << std::endl;
				}

				assert(number_of_segments == m_lines.size());
				if (m_save_log) log.save("tmp" + std::string(PSR) + "segments");
			}

			void clear_segments(){

				m_segments.clear();
				m_segments.resize(m_lines.size());
			}

			void find_lp() {

				clear_lp();

				Log log;

				assert(!m_lp.empty());
				assert(!m_times.empty());

				assert(m_lp.size() == m_cc.size() && m_lp.size() == m_eps.size());
				assert(m_lp.size() == m_segments.size());

				assert(m_times.size() == m_lp.size());

				const auto number_of_segments = m_segments.size();
				for (size_t i = 0; i < number_of_segments; ++i) {

					const FT seg_length = static_cast<FT>(CGAL::sqrt(CGAL::to_double(m_segments[i].squared_length())));
					
					const FT upper_bound = static_cast<FT>(CGAL::sqrt(2.0)) * m_eps[i];

					const auto initial = upper_bound / FT(2);

					const int times = static_cast<int>(std::floor(CGAL::to_double(seg_length / initial)));

					const auto rest = seg_length - times * initial;

					const auto extra = rest / times;

					if (initial + extra >= upper_bound) {
					
						m_lp[i]    = -FT(1);
						m_times[i] = -1;

						continue;
					}

					m_lp[i]    = initial + extra;
					m_times[i] = static_cast<int>(times);

					assert(m_lp[i] < upper_bound);
					assert(CGAL::abs(seg_length - m_lp[i] * m_times[i]) < m_tol);

					if (m_save_log) log.out << "lp: " << m_lp[i] << "; times: " << m_times[i] << std::endl;
				}
				if (m_save_log) log.save("tmp" + std::string(PSR) + "lp");
			}

			void clear_lp() {

				m_lp.clear();
				m_lp.resize(m_cc.size());

				m_times.clear();
				m_times.resize(m_cc.size());
			}

			void fill_in_occupancy_grid(const Occupancy_method method) {

				clear_occupancy_grid();

				assert(!m_occupancy.empty());
				assert(m_occupancy.size() == m_times.size() && m_occupancy.size() == m_lp.size());

				switch (method) {
					case Occupancy_method::ALL:
						fill_all();
						break;

					case Occupancy_method::ONLY_PROJECTED:
						fill_only_projected();
						break;

					default:
						fill_all();
						break;
				}

				// Save log. Can be removed in principle.
				Log log;
				if (m_save_log) {
					for (size_t i = 0; i < m_times.size(); ++i) {
						if (m_times[i] < 0) continue;

						for (size_t j = 0; j < static_cast<size_t>(m_times[i]); ++j) {
							log.out << m_occupancy[i][j] << " ";	
						}
						log.out << std::endl;
					}
					log.save("tmp" + std::string(PSR) + "occupancy");
				}
			}

			void clear_occupancy_grid() {

				m_occupancy.clear();
				m_occupancy.resize(m_times.size());

				for (size_t i = 0; i < m_occupancy.size(); ++i) {
					
					if (m_times[i] < 0) continue;
					m_occupancy[i].resize(m_times[i], false);
				}
			}

			void fill_all() {

				for (size_t i = 0; i < m_times.size(); ++i) {
					if (m_times[i] < 0) continue;

					for (size_t j = 0; j < static_cast<size_t>(m_times[i]); ++j)
						m_occupancy[i][j] = true;
				}
			}

			void fill_only_projected() {

				assert(!m_cc.empty());
				assert(!m_projected.empty());
				assert(m_projected.size() == m_points.size());

				size_t segment_index = 0;
				for (CC_iterator it = m_cc.begin(); it != m_cc.end(); ++it, ++segment_index) {
					
					const auto num_points = (*it).second.size();
					if (m_times[segment_index] < 0) continue;

					for (size_t i = 0; i < num_points; ++i) {
						const auto index = (*it).second[i];

						const Point &p = m_projected.at(index);
						const auto occupancy_index = find_occupancy_cell(p, segment_index);

						assert(occupancy_index >= 0 && occupancy_index < static_cast<int>(m_occupancy[segment_index].size()));
						m_occupancy[segment_index][occupancy_index] = true;
					}
				}
			}

			int find_occupancy_cell(const Point &p, const size_t segment_index) const {

				assert(!m_lp.empty());
				assert(!m_times.empty());
				assert(m_lp.size() == m_cc.size());
				assert(segment_index >= 0 && segment_index < m_cc.size());
				assert(m_lp[segment_index] > FT(0));
				assert(m_times[segment_index] >= 0);

				int occupancy_index = -1;

				const Point &source = m_segments[segment_index].source();
				const Point &target = m_segments[segment_index].target();

				if (CGAL::abs(source.x() - p.x()) < m_tol && CGAL::abs(source.y() - p.y()) < m_tol) return 0;
				if (CGAL::abs(target.x() - p.x()) < m_tol && CGAL::abs(target.y() - p.y()) < m_tol) 
					return static_cast<int>(m_times[segment_index] - 1);

				const FT length = static_cast<FT>(CGAL::sqrt(CGAL::to_double(squared_distance(source, p))));

				occupancy_index = static_cast<int>(std::floor(CGAL::to_double(length / m_lp[segment_index])));

				return occupancy_index;
			}

			void create_linear_points() {

				clear_main_data();

				Log log;

				assert(!m_str_points.empty());
				assert(!m_str_labels.empty());
				assert(!m_str_anchors.empty());

				assert(m_str_points.size() == m_cc.size());
				assert(m_str_points.size() == m_str_labels.size() && m_str_labels.size() == m_str_anchors.size());
				assert(m_occupancy.size() == m_lp.size() && m_lp.size() == m_times.size());

				for (size_t i = 0; i < m_occupancy.size(); ++i) {
					for (size_t j = 0; j < m_occupancy[i].size(); ++j) {

						if (m_occupancy[i][j] == true) {

							const Point new_point = create_linear_point(i, j);

							m_str_points[i].push_back(new_point);
							m_str_labels[i].push_back(Structured_label::LINEAR);

							std::vector<int> anchor(1, i);
							m_str_anchors[i].push_back(anchor);

							if (!m_silent) log.out << new_point << " " << 0 << std::endl;
						}
					}
				}
				if (!m_silent) log.save("tmp" + std::string(PSR) + "linear_points", ".xyz");
			}

			void clear_main_data() {

				m_num_linear.clear();
				m_num_corners.clear();

				m_num_linear.resize(m_cc.size(), 0);
				m_num_corners.resize(m_cc.size(), 0);

				m_str_points.clear();
				m_str_labels.clear();
				m_str_anchors.clear();

				m_str_points.resize(m_cc.size());
				m_str_labels.resize(m_cc.size());
				m_str_anchors.resize(m_cc.size());
			}

			Point create_linear_point(const int segment_index, const int occupancy_index) {

				assert(m_times[segment_index] >= 0);
				assert(segment_index >= 0 && segment_index < static_cast<int>(m_times.size()));
				assert(occupancy_index >= 0 && occupancy_index < static_cast<int>(m_times[segment_index]));
				
				const Point &source = m_segments[segment_index].source();
				const Point &target = m_segments[segment_index].target();

				// Compute barycentric coordinates.
				const FT b2_1 = FT(occupancy_index) / FT(m_times[segment_index]);
				const FT b1_1 = FT(1) - b2_1;

				const FT b2_2 = FT(occupancy_index + 1) / FT(m_times[segment_index]);
				const FT b1_2 = FT(1) - b2_2;

				// Compute middle point of the cell [p1, p2].
				const Point p1 = Point(b1_1 * source.x() + b2_1 * target.x(), b1_1 * source.y() + b2_1 * target.y());
				const Point p2 = Point(b1_2 * source.x() + b2_2 * target.x(), b1_2 * source.y() + b2_2 * target.y());

				++m_num_linear[segment_index];

				return Point((p1.x() + p2.x()) / FT(2), (p1.y() + p2.y()) / FT(2));
			}

			void add_corners(Log &log) {

				switch (m_corner_algorithm) {

					case Structuring_corner_algorithm::NO_CORNERS:
						if (m_save_log) log.out << "(a) NO CORNERS option is chosen!" << std::endl;
						break;

					case Structuring_corner_algorithm::GRAPH_BASED:
						add_graph_based_corners(log);
						break;

					case Structuring_corner_algorithm::INTERSECTION_BASED:
						assert(!"This method is not implemented!");
						add_intersection_based_corners(log);
						break;

					case Structuring_corner_algorithm::NO_T_CORNERS:
						add_graph_based_corners(log);
						break;

					default:
						assert(!"Wrong structuring corner algorithm!");
						break;
				}
			}

			void add_graph_based_corners(Log &log) {

				// (a) Create adjacency graph between segments.
				// Here I use structured segments, maybe better to use raw/unstructured point set?
				create_adjacency_graph(Adjacency_method::STRUCTURED);

				if (m_save_log) log.out << "(a) Adjacency graph between segments is created. The results are saved in tmp->adjacency" << std::endl;


				// (b) Create an undirected graph by removing all repeating adjacencies.
				create_undirected_graph();

				if (m_save_log) log.out << "(b) An undirected graph is created. The results are saved in tmp->undirected_graph" << std::endl;


				// (c) Create corners using the undirected graph above.
				create_graph_based_corners();

				if (m_save_log) log.out << "(c) Unique GRAPH_BASED corner points between all adjacent segments are inserted. The final results are saved in tmp->structured_points" << std::endl;
			}

			void add_intersection_based_corners(Log &log) {

				assert(!m_lines.empty());
				const int num_lines = static_cast<int>(m_lines.size());

				for (int i = 0; i < num_lines; ++i)
					for (int j = 0; j < num_lines; ++j)
						if (i != j) try_adding_intersection_based_corner(i, j);

				if (m_save_log) log.out << "(a) All possible INTERSECTION_BASED corners are inserted. The final results are saved in tmp->structured_points" << std::endl;
			}

			void try_adding_intersection_based_corner(const int ind_i, const int ind_j) {

				assert(ind_i != ind_j);
				Point corner = intersect_lines(ind_i, ind_j);

				assert(m_str_points.size() == m_str_labels.size() && m_str_labels.size() == m_str_anchors.size());
				assert(ind_i >= 0 && ind_i <= static_cast<int>(m_str_points.size()));

				if (is_valid_intersection()) {

					FT dist_thresh = -FT(1);
					if (m_adjacency_threshold_method == Structuring_adjacency_threshold_method::GLOBAL) dist_thresh = m_adjacency_threshold;

					add_corner(ind_i, ind_j, dist_thresh, corner);
				}
			}

			bool is_valid_intersection() {
				return true;
			}

			void create_adjacency_graph(const Adjacency_method method) {

				clear_adjacency();

				switch (method) {
					case Adjacency_method::STRUCTURED:
						create_structured_adjacency();
						break;

					default:
						create_structured_adjacency();
						break;
				}

				Log log;
				if (m_save_log) {
					for (size_t i = 0; i < m_adjacency.size(); ++i) {

						log.out << "Segment " << i << " adjacent to segments: ";
						for (const int index: m_adjacency[i]) log.out << index << " ";
						log.out << std::endl;
					}

					log.save("tmp" + std::string(PSR) + "adjacency");
				}
			}

			void create_structured_adjacency() {

				assert(!m_str_points.empty());
				assert(m_str_points.size() == m_cc.size());
				assert(m_adjacency.size() == m_cc.size());
				assert(!m_segments.empty());
				assert(m_segments.size() == m_cc.size());
				assert(m_eps.size() == m_cc.size());
				assert(!m_eps.empty());

				for (size_t i = 0; i < m_cc.size(); ++i) {
					if (m_times[i] < 0 || m_str_points[i].size() < m_min_seg_points) continue;

					for (size_t j = 0; j < m_cc.size(); ++j) {
						if (m_times[j] < 0 || m_str_points[j].size() < m_min_seg_points) continue;

						if (i != j) {

							// Instead we can use approximate range [eps_i, eps_j] in the fuzzy sphere below.
							const FT eps = get_adjacency_threshold(i, j); 
							assert(eps > FT(0));

							Tree tree(m_str_points[j].begin(), m_str_points[j].end());

							const Point &source = m_segments[i].source();
							bool adjacent = check_for_adjacency(source, eps, tree);

							if (!adjacent) {
								
								const Point &target = m_segments[i].target();
								adjacent = check_for_adjacency(target, eps, tree);

								if (adjacent) set_adjacency(i, j);

							} else set_adjacency(i, j);
						}
					}
				}
			}

			FT get_adjacency_threshold(const size_t ind_i, const size_t ind_j) {

				// Base method.
				if (m_use_global_everywhere) {
					
					assert(ind_i >= 0 && ind_i < m_eps.size() && ind_j >= 0 && ind_j < m_eps.size());
					const FT res = CGAL::max(m_eps[ind_i], m_eps[ind_j]);

					m_local_adjacency_value = CGAL::max(m_local_adjacency_value, res);
					return res;
				}

				// Alternative methods.
				switch (m_adjacency_threshold_method) {

					case Structuring_adjacency_threshold_method::LOCAL: {
						assert(ind_i >= 0 && ind_i < m_eps.size() && ind_j >= 0 && ind_j < m_eps.size());
						const FT res = CGAL::max(m_eps[ind_i], m_eps[ind_j]);

						m_local_adjacency_value = CGAL::max(m_local_adjacency_value, res);
						return res;
					}

					case Structuring_adjacency_threshold_method::GLOBAL: {

						m_local_adjacency_value = CGAL::max(m_local_adjacency_value, m_adjacency_threshold);
						return m_adjacency_threshold;
					}

					default:
						assert(!"Wrong adjacency threshold method!");
						return -FT(1);
				}
			}

			void clear_adjacency() {

				m_adjacency.clear();
				m_adjacency.resize(m_cc.size());
			}

			bool check_for_adjacency(const Point &centre, const FT radius, const Tree &tree) {

				bool adjacent = false;
				std::vector<Point> result;

				Fuzzy_circle circle(centre, radius);
				tree.search(std::back_inserter(result), circle);

				if (!result.empty()) adjacent = true;
				return adjacent;
			}

			void set_adjacency(const int i, const int j) {

				m_adjacency[i].insert(j);
				m_adjacency[j].insert(i);
			}

			void create_undirected_graph() {

				clear_undirected_graph();
				assert(m_undirected_graph.empty());

				assert(!m_adjacency.empty());
				const int number_of_components = static_cast<int>(m_adjacency.size());

				for (int i = 0; i < number_of_components; ++i) {
					if (m_adjacency[i].size() > m_max_neigh_segments) continue;

					for (const int j : m_adjacency[i])
						m_undirected_graph.insert(std::make_pair(i, j));
				}

				// Log function. Can be removed.
				Log log;

				if (m_save_log) {
					for (PP_iterator it = m_undirected_graph.begin(); it != m_undirected_graph.end(); ++it)
						log.out << (*it).first << " -- " << (*it).second << std::endl;

					log.save("tmp" + std::string(PSR) + "undirected_graph");
				}
			}

			void clear_undirected_graph() {
				m_undirected_graph.clear();
			}

			void create_graph_based_corners() {

				assert(!m_str_points.empty());
				assert(!m_str_labels.empty());

				assert(!m_str_anchors.empty());
				assert(m_str_points.size() == m_cc.size());
				assert(m_str_points.size() == m_str_labels.size() && m_str_labels.size() == m_str_anchors.size());

				Segments edges;
				for (const Int_pair el : m_undirected_graph) {

					const int i = el.first;
					const int j = el.second;

					Point corner = intersect_lines(i, j);
					adjust_corner(corner);

					FT dist_thresh = -FT(1);
					if (m_adjacency_threshold_method == Structuring_adjacency_threshold_method::GLOBAL) dist_thresh = m_adjacency_threshold;

					if (!m_silent) edges.push_back(Segment(edge_barycentre(i), edge_barycentre(j)));
					add_corner(i, j, dist_thresh, corner);
				}

				if (!m_silent) {
					Log logex;
					logex.export_segments_as_obj("tmp" + std::string(PSR) + "adjacency_graph", edges, "stub");
				}

				// Log function. Can be removed.
				Log log;

				// if (m_save_log) {
				if (!m_silent) {
					for (size_t i = 0; i < m_cc.size(); ++i) 
						for (size_t j = 0; j < m_str_points[i].size(); ++j)
							log.out << m_str_points[i][j] << " " << 0 << std::endl;
					
					log.save("tmp" + std::string(PSR) + "structured_points", ".xyz");
				}
				// }
			}

			void adjust_corner(Point & /* corner */) {
				
				return;

				/*
				const FT big_value = FT(1000000000);

				FT x, y;
				if (corner.x() > big_value) x = big_value;
				if (corner.y() > big_value) y = big_value;

				corner = Point(x, y); */
			}

			Point edge_barycentre(const int segment_index) {

				FT x = FT(0), y = FT(0);
				for (size_t i = 0; i < m_str_points[segment_index].size(); ++i) {

					x += m_str_points[segment_index][i].x();
					y += m_str_points[segment_index][i].y();
				}

				return Point(x / FT(m_str_points[segment_index].size()), y / FT(m_str_points[segment_index].size()));
			}

			Point intersect_lines(const int i, const int j) {

				const Line &line1 = m_lines[i];
				const Line &line2 = m_lines[j];

				typename CGAL::cpp11::result_of<Intersect(Line, Line)>::type result = intersection(line1, line2);

				if (CGAL::parallel(line1, line2)) return line1.point(0);
				return boost::get<Point>(*result);
			}

			void add_corner(const int i, const int j, const FT dist_thresh, Point &corner) {

				if (m_corner_algorithm == Structuring_corner_algorithm::NO_T_CORNERS) {
					
					if (!is_valid_segment(i, corner, (dist_thresh < FT(0) ? m_lp[i] : dist_thresh))) return;
					if (!is_valid_segment(j, corner, (dist_thresh < FT(0) ? m_lp[j] : dist_thresh))) return;
				}

				std::vector<int> cycle(2);
				const bool closest_i = is_closest_i(i, j, corner);

				if (/* is_unique(corner, i) */  closest_i) {

					assert(m_lp[i] > FT(0));

					cycle[0] = i;
					cycle[1] = j;

					if (dist_thresh < FT(0)) add_unique_corner(i, cycle, m_lp[i], corner);
					else add_unique_corner(i, cycle, dist_thresh, corner);
				}

				if ( /* is_unique(corner, j) */ !closest_i) {

					assert(m_lp[j] > FT(0));

					cycle[0] = j;
					cycle[1] = i;

					if (dist_thresh < FT(0)) add_unique_corner(j, cycle, m_lp[j], corner);
					else add_unique_corner(j, cycle, dist_thresh, corner);
				}
			}

			bool is_valid_segment(const int ind, const Point &corner, const FT dist_thresh) {

				assert(m_str_points[ind].size() >= m_min_seg_points);

				const Point &source = m_str_points[ind][0];
				const Point &target = m_str_points[ind][m_str_points[ind].size() - 1];

				const FT dist_s = static_cast<FT>(CGAL::sqrt(CGAL::to_double(squared_distance(source, corner))));
				const FT dist_t = static_cast<FT>(CGAL::sqrt(CGAL::to_double(squared_distance(target, corner))));

				if (dist_s > dist_thresh && dist_t > dist_thresh) return false;
				return true;
			}

			bool is_closest_i(const int i, const int j, const Point &corner) {

				const Point &source_i = m_str_points[i][0];
				const Point &target_i = m_str_points[i][m_str_points[i].size() - 1];

				const FT dist_s_i = squared_distance(corner, source_i);
				const FT dist_t_i = squared_distance(corner, target_i);

				const Point &source_j = m_str_points[j][0];
				const Point &target_j = m_str_points[j][m_str_points[j].size() - 1];

				const FT dist_s_j = squared_distance(corner, source_j);
				const FT dist_t_j = squared_distance(corner, target_j);

				const FT dist_i = CGAL::min(dist_s_i, dist_t_i);
				const FT dist_j = CGAL::min(dist_s_j, dist_t_j);

				if (dist_i < dist_j) return true;
				return false;
			}

			void add_unique_corner(const int segment_index, const std::vector<int> &cycle, const FT dist_thresh, Point &corner) {

				assert(dist_thresh > FT(0));
				assert(m_str_points[segment_index].size() > 1);

				const Point &source = m_str_points[segment_index][0];
				const Point &target = m_str_points[segment_index][m_str_points[segment_index].size() - 1];

				const FT dist_s = squared_distance(corner, source);
				const FT dist_t = squared_distance(corner, target);

				assert(source != target);

				// Choose the closest position for the corner within the given segment.
				if (dist_s < dist_t) add_begin_corner(segment_index, cycle, source, dist_thresh, corner);
				else add_end_corner(segment_index, cycle, target, dist_thresh, corner);

				++m_num_corners[segment_index];
			}

			void add_begin_corner(const int segment_index, const std::vector<int> &cycle, const Point &source, const FT dist_thresh, Point &corner) {

				correct_corner(segment_index, source, dist_thresh, corner);

				// if (!is_unique(corner, segment_index)) return;

				 m_str_points[segment_index].insert( m_str_points[segment_index].begin(), corner);
				 m_str_labels[segment_index].insert( m_str_labels[segment_index].begin(), Structured_label::CORNER);
				m_str_anchors[segment_index].insert(m_str_anchors[segment_index].begin(), cycle);
			}

			void add_end_corner(const int segment_index, const std::vector<int> &cycle, const Point &target, const FT dist_thresh, Point &corner) {

				correct_corner(segment_index, target, dist_thresh, corner);

				// if (!is_unique(corner, segment_index)) return;

				 m_str_points[segment_index].push_back(corner);
				 m_str_labels[segment_index].push_back(Structured_label::CORNER);
				m_str_anchors[segment_index].push_back(cycle);
			}

			// If the given corner is far away from the intersected segments, make it closer.
			void correct_corner(const int segment_index, const Point &closest_point, const FT dist_thresh, Point &corner) {

				assert(segment_index >= 0 && segment_index < static_cast<int>(m_lp.size()));
				const FT dist = squared_distance(corner, closest_point);

				if (dist > FT(5) * dist_thresh) {

					assert(m_lp[segment_index] > FT(0));

					const FT b1 = m_lp[segment_index] / dist;
					const FT b2 = FT(1) - b1;

					assert(b1 >= FT(0) && b1 <= FT(1));
					assert(b2 >= FT(0) && b2 <= FT(1));

					const FT x = corner.x() * b1 + closest_point.x() * b2;
					const FT y = corner.y() * b1 + closest_point.y() * b2;

					corner = Point(x, y);
				}
			}

			// Is it correct to use m_num_linear[i] here instead of m_str_points[i].size()?
			bool is_unique(const Point &q) const {
				
				for (size_t i = 0; i < m_cc.size(); ++i) {
					assert(m_num_linear[i] <= m_str_points[i].size());

					for (size_t j = m_num_linear[i]; j < m_str_points[i].size(); ++j) {

						const Point &p = m_str_points[i][j];
						if (points_equal(p, q)) return false;
					}
				}
				return true;
			}

			bool is_unique(const Point &q, const int segment_index) const {
				for (size_t i = 0; i < m_str_points[segment_index].size(); ++i) {

					const Point &p = m_str_points[segment_index][i];
					if (points_equal(p, q)) return false;
				}
				return true;
			}

			bool points_equal(const Point &p, const Point &q) const {

				if (CGAL::abs(p.x() - q.x()) < m_tol && CGAL::abs(p.y() - q.y()) < m_tol) return true;
				return false;
			}

			void create_segment_end_points_labels_anchors() {

				 m_segment_end_points.clear();
				 m_segment_end_points.resize(m_cc.size());

				 m_segment_end_labels.clear();
				 m_segment_end_labels.resize(m_cc.size());

				m_segment_end_anchors.clear();
				m_segment_end_anchors.resize(m_cc.size());

				assert( m_str_points.size() ==  m_segment_end_points.size());
				assert( m_str_labels.size() ==  m_segment_end_labels.size());
				assert(m_str_anchors.size() == m_segment_end_anchors.size());

				Log log;
				for (size_t i = 0; i < m_segment_end_points.size(); ++i) {
					if (m_str_points[i].size() < m_min_seg_points) continue;
					
					 m_segment_end_points[i].resize(2);
					 m_segment_end_labels[i].resize(2);
					m_segment_end_anchors[i].resize(2);

					 m_segment_end_points[i][0] = m_str_points[i][0];
					 m_segment_end_points[i][1] = m_str_points[i][m_str_points[i].size() - 1];
					
					 m_segment_end_labels[i][0] = m_str_labels[i][0];
					 m_segment_end_labels[i][1] = m_str_labels[i][m_str_labels[i].size() - 1];

					m_segment_end_anchors[i][0] = m_str_anchors[i][0];
					m_segment_end_anchors[i][1] = m_str_anchors[i][m_str_anchors[i].size() - 1];

					if (m_save_log) log.out << m_segment_end_points[i][0] << " " << 0 << std::endl;
					if (m_save_log) log.out << m_segment_end_points[i][1] << " " << 0 << std::endl;
				}
				if (m_save_log) log.save("tmp" + std::string(PSR) + "segment_end_points", ".xyz");
			}

			// It works only with Occupancy_method::ALL!
			void resample_points() {

				Log log;
				assert(m_str_points.size() == m_segments.size());

				for (size_t i = 0; i < m_str_points.size(); ++i) {
					if (m_str_points[i].size() < m_min_seg_points) continue;

					const Point &p = m_str_points[i][0];
					const Point &q = m_str_points[i][m_str_points[i].size() - 1];

					const FT seg_length  = static_cast<FT>(CGAL::sqrt(CGAL::to_double(squared_distance(p, q))));
					const FT upper_bound = static_cast<FT>(CGAL::sqrt(2.0)) * m_eps[i];
					const FT initial     = upper_bound / FT(2);
					const FT times 		 = static_cast<FT>(std::floor(CGAL::to_double(seg_length / initial)));
					
					resample_segment(i, times, log);
				}
				
				/* if (m_save_log) */ 
				if (!m_silent) log.save("tmp" + std::string(PSR) + "resampled_points", ".xyz");
			}

			void resample_segment(const size_t segment_index, const FT times, Log &log) {
					
				const Point p = m_str_points[segment_index][0];
				const Point q = m_str_points[segment_index][m_str_points[segment_index].size() - 1];

				const Structured_label lp = m_str_labels[segment_index][0];
				const Structured_label lq = m_str_labels[segment_index][m_str_labels[segment_index].size() - 1];

				const std::vector<int> ap = m_str_anchors[segment_index][0];
				const std::vector<int> aq = m_str_anchors[segment_index][m_str_anchors[segment_index].size() - 1];

				 m_str_points[segment_index].clear();
				 m_str_labels[segment_index].clear();
				m_str_anchors[segment_index].clear();

				  m_str_points[segment_index].push_back(p);
				 m_str_labels[segment_index].push_back(lp);
				m_str_anchors[segment_index].push_back(ap);

				/* if (m_save_log) */ log.out << p << " " << 0 << std::endl;

				for (size_t i = 1; i < static_cast<size_t>(CGAL::to_double(times)); ++i) {

					const FT b_2 = FT(i) / times;
					const FT b_1 = FT(1) - b_2;
					
					const Point newp = Point(b_1 * p.x() + b_2 * q.x(), b_1 * p.y() + b_2 * q.y());

					  m_str_points[segment_index].push_back(newp);
					 m_str_labels[segment_index].push_back(Structured_label::LINEAR);
					m_str_anchors[segment_index].push_back(std::vector<int>(1, segment_index));

					/* if (m_save_log) */ log.out << newp << " " << 0 << std::endl;
				}

				  m_str_points[segment_index].push_back(q);
				 m_str_labels[segment_index].push_back(lq);
				m_str_anchors[segment_index].push_back(aq);

				/* if (m_save_log) */ log.out << q << " " << 0 << std::endl;
			}
		};
	}
}

#endif // CGAL_LEVEL_OF_DETAIL_STRUCTURING_2_H