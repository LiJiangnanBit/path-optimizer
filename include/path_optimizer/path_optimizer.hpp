//
// Created by ljn on 19-8-16.
//

#ifndef PATH_OPTIMIZER__PATHOPTIMIZER_HPP_
#define PATH_OPTIMIZER__PATHOPTIMIZER_HPP_

#include <iostream>
#include <ios>
#include <iomanip>
#include <string>
#include <vector>
#include <glog/logging.h>
#include <cmath>
#include <ctime>
#include <Eigen/Dense>
#include <cppad/cppad.hpp>
#include <cppad/ipopt/solve.hpp>
#include <chrono>
#include <Clothoid.hh>
#include <opt_utils/utils.hpp>
#include <internal_grid_map/internal_grid_map.hpp>
#include <tinyspline_ros/tinysplinecpp.h>
#include <OsqpEigen/OsqpEigen.h>
#include "reference_path_smoother/reference_path_smoother.hpp"
#include "tools/spline.h"
#include "tools/tools.hpp"
#include "FgEvalFrenet.hpp"
#include "tools/collosion_checker.hpp"
#include "config/config.hpp"

namespace PathOptimizationNS {

// Used to store divided smoothed path.
struct DividedSegments {
    DividedSegments() = default;
    // Copy some elements from another one.
    DividedSegments(const DividedSegments &divided_segments_, size_t target_index) {
        assert(target_index <= divided_segments_.seg_angle_list_.size());
        seg_s_list_.assign(divided_segments_.seg_s_list_.begin(), divided_segments_.seg_s_list_.begin() + target_index);
        seg_angle_list_.assign(divided_segments_.seg_angle_list_.begin(),
                               divided_segments_.seg_angle_list_.begin() + target_index);
        seg_k_list_.assign(divided_segments_.seg_k_list_.begin(), divided_segments_.seg_k_list_.begin() + target_index);
        seg_clearance_list_.assign(divided_segments_.seg_clearance_list_.begin(),
                                   divided_segments_.seg_clearance_list_.begin() + target_index);
        seg_x_list_.assign(divided_segments_.seg_x_list_.begin(), divided_segments_.seg_x_list_.begin() + target_index);
        seg_y_list_.assign(divided_segments_.seg_y_list_.begin(), divided_segments_.seg_y_list_.begin() + target_index);
    }
    std::vector<double> seg_s_list_;
    std::vector<double> seg_k_list_;
    std::vector<double> seg_x_list_;
    std::vector<double> seg_y_list_;
    std::vector<double> seg_angle_list_;
    std::vector<std::vector<double> > seg_clearance_list_;
};

class PathOptimizer {
public:
    PathOptimizer() = delete;
    PathOptimizer(const std::vector<hmpl::State> &points_list,
                  const hmpl::State &start_state,
                  const hmpl::State &end_state,
                  const hmpl::InternalGridMap &map,
                  bool densify_path = true);
    // Call this to get the final path.
    bool solve(std::vector<hmpl::State> *final_path);
    // Sample a set of candidate paths of various longitudinal distance and lateral offset.
    bool samplePaths(const std::vector<double> &lon_set,
                     const std::vector<double> &lat_set,
                     std::vector<std::vector<hmpl::State>> *final_path_set);
    // For dynamic obstacle avoidance.
    bool optimizeDynamic(const std::vector<double> &sr_list,
                         const std::vector<std::vector<double>> &clearance_list,
                         std::vector<double> *x_list,
                         std::vector<double> *y_list,
                         std::vector<double> *s_list);
    // Only for visualization purpose.
    // TODO: some of these functions are no longer used.
    const std::vector<hmpl::State> &getLeftBound() const;
    const std::vector<hmpl::State> &getRightBound() const;
    const std::vector<hmpl::State> &getSecondThirdPoint() const;
    const std::vector<hmpl::State> &getRearBounds() const;
    const std::vector<hmpl::State> &getCenterBounds() const;
    const std::vector<hmpl::State> &getFrontBounds() const;
    const std::vector<hmpl::State> &getSmoothedPath() const;

private:
    // TODO: abandon this function, use the config class instead.
    void setCarGeometry();
    void setConfig();
    bool optimizePath(std::vector<hmpl::State> *final_path);
    bool sampleSingleLongitudinalPaths(double lon,
                                       const std::vector<double> &lat_set,
                                       std::vector<std::vector<hmpl::State>> *final_path_set,
                                       bool max_lon_flag);
    std::vector<double> getClearanceWithDirectionStrict(hmpl::State state,
                                                        double radius,
                                                        bool safety_margin_flag) const;
    std::vector<double> getClearanceFor4Circles(const hmpl::State &state,
                                                const std::vector<double> &car_geometry,
                                                bool safety_margin_flag);
    bool divideSmoothedPath(bool safety_margin_flag);

    //OSQP solver related
    void setHessianMatrix(size_t horizon, Eigen::SparseMatrix<double> *matrix_h);
    void setDynamicMatrix(size_t n,
                          const std::vector<double> &seg_s_list,
                          const std::vector<double> &seg_k_list,
                          Eigen::Matrix<double, 2, 2> *matrix_a,
                          Eigen::Matrix<double, 2, 1> *matrix_b);
    void setConstraintMatrix(size_t horizon,
                             const DividedSegments &divided_segments,
                             Eigen::SparseMatrix<double> *matrix_constraints,
                             Eigen::VectorXd *lower_bound,
                             Eigen::VectorXd *upper_bound,
                             const std::vector<double> &init_state,
                             double end_heading,
                             bool constraint_end_psi);

    void setConstraintMatrix(size_t horizon,
                             const DividedSegments &divided_segments,
                             Eigen::SparseMatrix<double> *matrix_constraints,
                             Eigen::VectorXd *lower_bound,
                             Eigen::VectorXd *upper_bound,
                             const std::vector<double> &init_state,
                             double end_angle,
                             double offset,
                             double angle_error_allowed = 0,
                             double offset_error_allowed = 0);

    hmpl::InternalGridMap grid_map_;
    CollisionChecker collision_checker_;
    CarType car_type; ///x
    std::vector<double> car_geo_;///x
    Config config_;
    double rear_axle_to_center_dis;///x
    double wheel_base;///x
//    std::vector<double> seg_s_list_;
//    std::vector<double> seg_k_list_;
//    std::vector<double> seg_x_list_;
//    std::vector<double> seg_y_list_;
//    std::vector<double> seg_angle_list_;
    DividedSegments divided_segments_;
//    std::vector<std::vector<double> > seg_clearance_list_;
    hmpl::State start_state_;
    hmpl::State end_state_;
    double smoothed_max_s;
    size_t N_;
    bool use_end_psi;
    // Start position and angle error with smoothed path.
    double cte_;
    double epsi_;
    bool densify_result;
    // Only for smoothing phase
    std::vector<hmpl::State> points_list_;
    size_t point_num_;
    tk::spline smoothed_x_spline;
    tk::spline smoothed_y_spline;
    // For dynamic obstacle avoidace.
    OsqpEigen::Solver solver_dynamic;
    bool solver_dynamic_initialized;
    Eigen::VectorXd lower_bound_dynamic_;
    Eigen::VectorXd upper_bound_dynamic_;
    tk::spline xsr_, ysr_;
    // For visualization purpose.
    std::vector<std::vector<hmpl::State> > control_sampling_path_set_;
    std::vector<std::vector<hmpl::State> > failed_sampling_path_set_;
    std::vector<hmpl::State> left_bound_;
    std::vector<hmpl::State> right_bound_;
    std::vector<hmpl::State> second_third_point_;
    size_t best_sampling_index_;
    std::vector<hmpl::State> empty_;
    std::vector<hmpl::State> rear_bounds_;
    std::vector<hmpl::State> center_bounds_;
    std::vector<hmpl::State> front_bounds_;
    std::vector<hmpl::State> smoothed_path_;
};

}

#endif //PATH_OPTIMIZER__PATHOPTIMIZER_HPP_
