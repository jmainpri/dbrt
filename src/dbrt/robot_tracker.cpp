/*
 * This is part of the Bayesian Robot Tracking
 *
 * Copyright (c) 2015 Max Planck Society,
 * 				 Autonomous Motion Department,
 * 			     Institute for Intelligent Systems
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License License (GNU GPL). A copy of the license can be found in the LICENSE
 * file distributed with this source code.
 */

/**
 * \file robot_tracker.cpp
 * \date December 2015
 * \author Jan Issac (jan.issac@gmail.com)
 * \author Manuel Wuthrich (manuel.wuthrich@gmail.com)
 */


#include <fl/util/profiling.hpp>

#include <dbrt/robot_tracker.hpp>

namespace dbrt
{
RobotTracker::RobotTracker(const std::shared_ptr<dbot::ObjectModel> &object_model,
                           const std::shared_ptr<dbot::CameraData> &camera_data)
    : object_model_(object_model), camera_data_(camera_data)
{
}

void RobotTracker::initialize(const std::vector<State>& initial_states,
    const Eigen::VectorXd &obsrv)
{
    std::lock_guard<std::mutex> lock(mutex_);

    on_initialize(initial_states, obsrv);
}

const std::shared_ptr<dbot::CameraData>& RobotTracker::camera_data() const
{
    return camera_data_;
}

auto RobotTracker::track(const Obsrv& image) -> State
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto state = on_track(image);
    return state;
}

RobotTracker::Input RobotTracker::zero_input() const
{
    return Input::Zero(1);
}

}
