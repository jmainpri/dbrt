/*
 * This is part of the Bayesian Object Tracking (bot),
 * (https://github.com/bayesian-object-tracking)
 *
 * Copyright (c) 2015 Max Planck Society,
 * 				 Autonomous Motion Department,
 * 			     Institute for Intelligent Systems
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License License (GNU GPL). A copy of the license can be found in the LICENSE
 * file distributed with this source code.
 */

/*
 * This file implements a part of the algorithm published in:
 *
 * M. Wuthrich, J. Bohg, D. Kappler, C. Pfreundt, S. Schaal
 * The Coordinate Particle Filter -
 * A novel Particle Filter for High Dimensional Systems
 * IEEE Intl Conf on Robotics and Automation, 2015
 * http://arxiv.org/abs/1505.00251
 *
 */

/**
 * \file ros_particle_tracker_factory.hpp
 * \date March 2016
 * \author Jan Issac (jan.issac@gmail.com)
 */

#include <dbrt/tracker/visual_tracker_factory.h>

#include <dbot_ros/util/ros_interface.hpp>
#include <dbot/builder/rb_sensor_builder.h>

#include <dbrt/urdf_object_loader.h>
#include <dbrt/builder/visual_tracker_builder.hpp>

namespace dbrt
{
// void load_sampling_block_definition(
//     const std::string& prefix,
//     ros::NodeHandle& nh,
//     SamplingBlocksDefinition& sampling_blocks_definition)
// {
// }

typedef std::vector<std::map<std::string, std::vector<std::string>>>
    SamplingBlocksDefinition;

std::vector<std::vector<int>> definition_to_sampling_block(
    const SamplingBlocksDefinition& definition,
    const std::shared_ptr<KinematicsFromURDF>& kinematics)
{
    std::vector<std::vector<int>> sampling_blocks;
    for (auto block_definition : definition)
    {
        std::vector<int> block_vector;
        for (auto joint_name : block_definition.begin()->second)
        {
            block_vector.push_back(kinematics->name_to_index(joint_name));
        }
        sampling_blocks.push_back(block_vector);
    }

    return sampling_blocks;
}

SamplingBlocksDefinition merge_sampling_block_definitions(
    const SamplingBlocksDefinition& definition_A,
    const SamplingBlocksDefinition& definition_B)
{
    SamplingBlocksDefinition merged = definition_A;

    for (auto block_definition_B : definition_B)
    {
        auto block_B = block_definition_B.begin();

        bool added = false;
        for (auto& block_definition_A : merged)
        {
            auto block_A = block_definition_A.begin();
            if (block_A->first == block_B->first)
            {
                block_A->second.insert(std::end(block_A->second),
                                       std::begin(block_B->second),
                                       std::end(block_B->second));
                added = true;
            }
        }
        if (!added)
        {
            merged.push_back(block_definition_B);
        }
    }

    return merged;
}

/**
 * \brief Create a particle filter tracking the robot joints based on depth
 *     images measurements
 * \param prefix
 *     parameter prefix, e.g. fusion_tracker
 * \param kinematics
 *     URDF robot kinematics
 */
std::shared_ptr<dbrt::VisualTracker> create_visual_tracker(
    std::string prefix,
    std::shared_ptr<KinematicsFromURDF> kinematics,
    std::shared_ptr<dbot::CameraData> camera_data,
    sensor_msgs::JointState::ConstPtr joint_state)
{
    ros::NodeHandle nh("~");

    typedef dbrt::VisualTracker Tracker;
    typedef Tracker::State State;

    /* ------------------------------ */
    /* - Create the robot model     - */
    /* ------------------------------ */
    auto object_model_loader =
        std::make_shared<dbrt::UrdfObjectModelLoader>(kinematics);

    // Load the model usign the URDF loader
    auto object_model = std::make_shared<dbot::ObjectModel>(
        std::make_shared<dbrt::UrdfObjectModelLoader>(kinematics), false);

    ROS_INFO("Robot model loaded");

    /* ------------------------------ */
    /* - State transition function  - */
    /* ------------------------------ */
    dbrt::TransitionBuilder<Tracker>::Parameters transition_parameters;

    // linear state transition parameters
    transition_parameters.joint_sigmas = ri::read<std::vector<double>>(
        prefix + "joint_transition/joint_sigmas", nh);
    ROS_INFO("Transition parameter loaded");
    transition_parameters.joint_count = kinematics->num_joints();
    PV(transition_parameters.joint_count);

    ROS_INFO("Transition parameter loaded");

    auto transition_builder =
        std::make_shared<dbrt::TransitionBuilder<Tracker>>(
            transition_parameters);

    ROS_INFO("Transition model created");

    /* ------------------------------ */
    /* - Observation model          - */
    /* ------------------------------ */
    dbot::RbSensorBuilder<State>::Parameters sensor_parameters;

    sensor_parameters.use_gpu = ri::read<bool>(prefix + "use_gpu", nh);

    if (sensor_parameters.use_gpu)
    {
        sensor_parameters.sample_count =
            ri::read<int>(prefix + "gpu/sample_count", nh);
    }
    else
    {
        sensor_parameters.sample_count =
            ri::read<int>(prefix + "cpu/sample_count", nh);
    }

    sensor_parameters.occlusion.p_occluded_visible = ri::read<double>(
        prefix + "observation/occlusion/p_occluded_visible", nh);
    sensor_parameters.occlusion.p_occluded_occluded = ri::read<double>(
        prefix + "observation/occlusion/p_occluded_occluded", nh);
    sensor_parameters.occlusion.initial_occlusion_prob = ri::read<double>(
        prefix + "observation/occlusion/initial_occlusion_prob", nh);

    sensor_parameters.kinect.tail_weight =
        ri::read<double>(prefix + "observation/kinect/tail_weight", nh);
    sensor_parameters.kinect.model_sigma =
        ri::read<double>(prefix + "observation/kinect/model_sigma", nh);
    sensor_parameters.kinect.sigma_factor =
        ri::read<double>(prefix + "observation/kinect/sigma_factor", nh);
    sensor_parameters.delta_time =
        ri::read<double>(prefix + "observation/delta_time", nh);

    // gpu only parameters
    sensor_parameters.use_custom_shaders =
        ri::read<bool>(prefix + "gpu/use_custom_shaders", nh);
    sensor_parameters.vertex_shader_file =
        ri::read<std::string>(prefix + "gpu/vertex_shader_file", nh);
    sensor_parameters.fragment_shader_file =
        ri::read<std::string>(prefix + "gpu/fragment_shader_file", nh);
    sensor_parameters.geometry_shader_file =
        ri::read<std::string>(prefix + "gpu/geometry_shader_file", nh);

    auto sensor_builder = std::make_shared<dbot::RbSensorBuilder<State>>(
        object_model, camera_data, sensor_parameters);

    ROS_INFO("Observation model created");

    /* ------------------------------ */
    /* - Create Filter & Tracker    - */
    /* ------------------------------ */
    dbrt::VisualTrackerBuilder<Tracker>::Parameters tracker_parameters;
    tracker_parameters.evaluation_count = sensor_parameters.sample_count;

    tracker_parameters.moving_average_update_rate =
        ri::read<double>(prefix + "moving_average_update_rate", nh);
    tracker_parameters.max_kl_divergence =
        ri::read<double>(prefix + "max_kl_divergence", nh);

    // tracker_parameters.sampling_blocks =
    //     ri::read<std::vector<std::vector<int>>>(prefix + "sampling_blocks",
    //     nh);

    auto sampling_blocks_definition =
        ri::read<SamplingBlocksDefinition>("sampling_blocks", nh);

    auto camera_offset_sampling_blocks_definition =
        ri::read<SamplingBlocksDefinition>("camera_offset/sampling_blocks", nh);

    auto merged_definitions = merge_sampling_block_definitions(
        sampling_blocks_definition, camera_offset_sampling_blocks_definition);
    tracker_parameters.sampling_blocks =
        definition_to_sampling_block(merged_definitions, kinematics);

    for (auto block: tracker_parameters.sampling_blocks)
        {
            std::cout << "[";
            for (auto index: block)
                {
                    std::cout << index << ", ";
                }
            std::cout << "]\n";
        }

    auto tracker_builder =
        dbrt::VisualTrackerBuilder<Tracker>(kinematics,
                                            transition_builder,
                                            sensor_builder,
                                            object_model,
                                            camera_data,
                                            tracker_parameters);

    auto tracker = tracker_builder.build();

    /* ------------------------------ */
    /* - Initialize tracker         - */
    /* ------------------------------ */

    std::vector<Eigen::VectorXd> initial_states_vectors = {
        kinematics->sensor_msg_to_eigen(*joint_state)};
    std::vector<dbrt::RobotState<>> initial_states;
    for (auto state : initial_states_vectors) initial_states.push_back(state);
    tracker->initialize(initial_states);

    return tracker;
}
}
