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
 * \file fusion_robot_tracker_simulation_node.hpp
 * \date January 2016
 * \author Jan Issac (jan.issac@gmail.com)
 */

#include <memory>
#include <thread>
#include <functional>

#include <cv.h>
#include <cv_bridge/cv_bridge.h>

#include <ros/ros.h>
#include <pcl_ros/point_cloud.h>
#include <sensor_msgs/Image.h>

#include <fl/util/profiling.hpp>

#include <dbot/util/rigid_body_renderer.hpp>
#include <dbot/util/virtual_camera_data_provider.hpp>
#include <dbot/tracker/builder/rbc_particle_filter_tracker_builder.hpp>

#include <dbot_ros/tracker_publisher.h>
#include <dbot_ros/utils/ros_interface.hpp>
#include <dbot_ros/utils/tracking_dataset.h>
#include <dbot_ros/utils/data_set_camera_data_provider.hpp>

#include <dbrt/robot_state.hpp>
#include <dbrt/robot_tracker.hpp>
#include <dbrt/fusion_tracker_node.h>
#include <dbrt/robot_tracker_publisher.h>
#include <dbrt/util/urdf_object_loader.hpp>
#include <dbrt/util/virtual_robot.h>
#include <dbrt/gaussian_joint_filter_robot_tracker.hpp>
#include <dbrt/fusion_robot_tracker.h>
#include <dbrt/gaussian_joint_filter_robot_tracker.hpp>
#include <dbrt/rbc_particle_filter_robot_tracker.hpp>
#include <dbrt/util/builder/rbc_particle_filter_robot_tracker_builder.hpp>
#include <dbrt/util/builder/gaussian_joint_filter_robot_tracker_builder.hpp>

/**
 * \brief Create a gaussian filter tracking the robot joints based on joint
 *     measurements
 * \param prefix
 *     parameter prefix, e.g. fusion_robot_tracker
 * \param urdf_kinematics
 *     URDF robot kinematics
 */
std::shared_ptr<dbrt::GaussianJointFilterRobotTracker>
create_joint_robot_tracker(
    const std::string& prefix,
    const std::shared_ptr<KinematicsFromURDF>& urdf_kinematics)
{
    ros::NodeHandle nh("~");

    typedef dbrt::GaussianJointFilterRobotTracker Tracker;

    /* ------------------------------ */
    /* - State transition function  - */
    /* ------------------------------ */
    dbrt::RobotJointTransitionModelBuilder<Tracker>::Parameters params_state;

    // linear state transition parameters
    nh.getParam(prefix + "joint_transition/joint_sigmas",
                params_state.joint_sigmas);
    nh.getParam(prefix + "joint_transition/bias_sigmas",
                params_state.bias_sigmas);
    nh.getParam(prefix + "joint_transition/bias_factors",
                params_state.bias_factors);
    params_state.joint_count = urdf_kinematics->num_joints();

    auto state_trans_builder =
        std::make_shared<dbrt::RobotJointTransitionModelBuilder<Tracker>>(
            (params_state));

    /* ------------------------------ */
    /* - Observation model          - */
    /* ------------------------------ */
    dbrt::RobotJointObservationModelBuilder<Tracker>::Parameters
        params_joint_obsrv;

    nh.getParam(prefix + "joint_observation/joint_sigmas",
                params_joint_obsrv.joint_sigmas);
    params_joint_obsrv.joint_count = urdf_kinematics->num_joints();

    auto joint_obsrv_model_builder =
        std::make_shared<dbrt::RobotJointObservationModelBuilder<Tracker>>(
            params_joint_obsrv);

    /* ------------------------------ */
    /* - Build the tracker          - */
    /* ------------------------------ */
    auto tracker_builder =
        dbrt::GaussianJointFilterRobotTrackerBuilder<Tracker>(
            urdf_kinematics, state_trans_builder, joint_obsrv_model_builder);

    return tracker_builder.build();
}

/**
 * \brief Create a particle filter tracking the robot joints based on depth
 *     images measurements
 * \param prefix
 *     parameter prefix, e.g. fusion_robot_tracker
 * \param urdf_kinematics
 *     URDF robot kinematics
 */
std::shared_ptr<dbrt::RbcParticleFilterRobotTracker>
create_rbc_particle_filter_robot_tracker(
    const std::string& prefix,
    const std::shared_ptr<KinematicsFromURDF>& urdf_kinematics,
    const std::shared_ptr<dbot::ObjectModel>& object_model,
    const std::shared_ptr<dbot::CameraData>& camera_data)
{
    ros::NodeHandle nh("~");

    typedef dbrt::RbcParticleFilterRobotTracker Tracker;
    typedef Tracker::State State;

    /* ------------------------------ */
    /* - State transition function  - */
    /* ------------------------------ */
    dbrt::RobotStateTransitionModelBuilder<Tracker>::Parameters params_state;

    // linear state transition parameters
    nh.getParam(prefix + "joint_transition/joint_sigmas",
                params_state.joint_sigmas);
    params_state.joint_count = urdf_kinematics->num_joints();

    auto state_trans_builder =
        std::make_shared<dbrt::RobotStateTransitionModelBuilder<Tracker>>(
            params_state);

    /* ------------------------------ */
    /* - Observation model          - */
    /* ------------------------------ */
    dbot::RbObservationModelBuilder<State>::Parameters params_obsrv;
    nh.getParam(prefix + "use_gpu", params_obsrv.use_gpu);

    if (params_obsrv.use_gpu)
    {
        nh.getParam(prefix + "gpu/sample_count", params_obsrv.sample_count);
    }
    else
    {
        nh.getParam(prefix + "cpu/sample_count", params_obsrv.sample_count);
    }

    nh.getParam(prefix + "observation/occlusion/p_occluded_visible",
                params_obsrv.occlusion.p_occluded_visible);
    nh.getParam(prefix + "observation/occlusion/p_occluded_occluded",
                params_obsrv.occlusion.p_occluded_occluded);
    nh.getParam(prefix + "observation/occlusion/initial_occlusion_prob",
                params_obsrv.occlusion.initial_occlusion_prob);

    nh.getParam(prefix + "observation/kinect/tail_weight",
                params_obsrv.kinect.tail_weight);
    nh.getParam(prefix + "observation/kinect/model_sigma",
                params_obsrv.kinect.model_sigma);
    nh.getParam(prefix + "observation/kinect/sigma_factor",
                params_obsrv.kinect.sigma_factor);
    params_obsrv.delta_time = 1. / 6.;

    // gpu only parameters
    nh.getParam(prefix + "gpu/use_custom_shaders",
                params_obsrv.use_custom_shaders);
    nh.getParam(prefix + "gpu/vertex_shader_file",
                params_obsrv.vertex_shader_file);
    nh.getParam(prefix + "gpu/fragment_shader_file",
                params_obsrv.fragment_shader_file);
    nh.getParam(prefix + "gpu/geometry_shader_file",
                params_obsrv.geometry_shader_file);

    auto obsrv_model_builder =
        std::make_shared<dbot::RbObservationModelBuilder<State>>(
            object_model, camera_data, params_obsrv);

    /* ------------------------------ */
    /* - Create Filter & Tracker    - */
    /* ------------------------------ */
    dbrt::RbcParticleFilterRobotTrackerBuilder<Tracker>::Parameters
        params_tracker;
    params_tracker.evaluation_count = params_obsrv.sample_count;
    nh.getParam(prefix + "moving_average_update_rate",
                params_tracker.moving_average_update_rate);
    nh.getParam(prefix + "max_kl_divergence", params_tracker.max_kl_divergence);
    ri::ReadParameter(
        prefix + "sampling_blocks", params_tracker.sampling_blocks, nh);

    auto tracker_builder =
        dbrt::RbcParticleFilterRobotTrackerBuilder<Tracker>(urdf_kinematics,
                                                            state_trans_builder,
                                                            obsrv_model_builder,
                                                            object_model,
                                                            camera_data,
                                                            params_tracker);

    return tracker_builder.build();
}

/**
 * \brief Node entry point
 */
int main(int argc, char** argv)
{
    ros::init(argc, argv, "fusion_robot_tracker_simulation");
    ros::NodeHandle nh("~");

    // parameter shorthand prefix
    std::string pre = "fusion_tracker/";

    /* ------------------------------ */
    /* - Create the robot kinematics- */
    /* - and robot mesh model       - */
    /* ------------------------------ */
    auto urdf_kinematics = std::make_shared<KinematicsFromURDF>();

    auto object_model = std::make_shared<dbot::ObjectModel>(
        std::make_shared<dbrt::UrdfObjectModelLoader>(urdf_kinematics), false);

    /* ------------------------------ */
    /* - Setup camera data          - */
    /* ------------------------------ */
    int downsampling_factor;
    nh.getParam("downsampling_factor", downsampling_factor);
    auto camera_data = std::make_shared<dbot::CameraData>(
        std::make_shared<dbot::VirtualCameraDataProvider>(downsampling_factor,
                                                          "/XTION"));

    /* ------------------------------ */
    /* - Robot renderer             - */
    /* ------------------------------ */
    auto renderer = std::make_shared<dbot::RigidBodyRenderer>(
        object_model->vertices(),
        object_model->triangle_indices(),
        camera_data->camera_matrix(),
        camera_data->resolution().height,
        camera_data->resolution().width);

    /* ------------------------------ */
    /* - Our state representation   - */
    /* ------------------------------ */
    dbrt::RobotState<>::kinematics_ = urdf_kinematics;
    typedef dbrt::RobotState<> State;

    /* ------------------------------ */
    /* - Create Tracker and         - */
    /* - tracker publisher          - */
    /* ------------------------------ */

    auto rbc_particle_filter_tracker = create_rbc_particle_filter_robot_tracker(
        pre, urdf_kinematics, object_model, camera_data);

    ROS_INFO("creating trackers ... ");
    auto joint_robot_tracker = create_joint_robot_tracker(pre, urdf_kinematics);
    dbrt::FusionRobotTracker fusion_robot_tracker(joint_robot_tracker);

    auto tracker_publisher = std::shared_ptr<dbot::TrackerPublisher<State>>(
        new dbrt::RobotTrackerPublisher<State>(
            urdf_kinematics, renderer, "/estimated"));

    /* ------------------------------ */
    /* - Setup Simulation           - */
    /* ------------------------------ */
    ROS_INFO("setting up simulation ... ");
    auto simulation_camera_data = std::make_shared<dbot::CameraData>(
        std::make_shared<dbot::VirtualCameraDataProvider>(1, "/XTION"));

    auto simulation_renderer = std::make_shared<dbot::RigidBodyRenderer>(
        object_model->vertices(),
        object_model->triangle_indices(),
        simulation_camera_data->camera_matrix(),
        simulation_camera_data->resolution().height,
        simulation_camera_data->resolution().width);

    std::vector<double> joints;
    nh.getParam("simulation/initial_state", joints);
    State state;
    state = Eigen::Map<Eigen::VectorXd>(joints.data(), joints.size());
    ROS_INFO("creating virtual robot ... ");
    dbrt::VirtualRobot<State> robot(object_model,
                                    urdf_kinematics,
                                    simulation_renderer,
                                    simulation_camera_data,
                                    1000.,  // joint sensor rate
                                    30,     // visual sensor rate
                                    state);

    // register obsrv callbacks
    robot.joint_sensor_callback(
        [&](const State& state)
        {
            fusion_robot_tracker.joints_obsrv_callback(state);
        });

    robot.image_sensor_callback(
        [&](const Eigen::VectorXd& depth_image)
        {
            fusion_robot_tracker.image_obsrv_callback(depth_image);
        });

    ROS_INFO("Initializing tracker ... ");
    /* ------------------------------ */
    /* - Initialize from config     - */
    /* ------------------------------ */
    fusion_robot_tracker.initialize({robot.state()},
                                    robot.observation_vector());

    /* ------------------------------ */
    /* - Run tracker node           - */
    /* ------------------------------ */

    ROS_INFO("Starting robot ... ");
    ros::AsyncSpinner spinner(4);
    spinner.start();

    robot.run();
    ROS_INFO("Robot running ... ");

    fusion_robot_tracker.run();

    ros::Rate visualization_rate(24);
    while (ros::ok())
    {
        visualization_rate.sleep();
        auto current_state = fusion_robot_tracker.current_state();

        tracker_publisher->publish(
            current_state, robot.observation(), camera_data);

        ros::spinOnce();
    }

    ROS_INFO("Shutting down ...");

    fusion_robot_tracker.shutdown();
    robot.shutdown();

    return 0;
}
