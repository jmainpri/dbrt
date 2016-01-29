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

/**
 * \file robot_tracker_publisher.h
 * \date January 2016
 * \author Jan Issac (jan.issac@gmail.com)
 */

#pragma once

#include <memory>

#include <ros/ros.h>
#include <sensor_msgs/JointState.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/Image.h>
#include <image_transport/image_transport.h>

#include <dbot/util/rigid_body_renderer.hpp>
#include <dbot/util/camera_data.hpp>
#include <dbot/util/object_model.hpp>

#include <dbot_ros/tracker_publisher.h>

#include <robot_state_pub/robot_state_publisher.h>

#include <dbrt/util/kinematics_from_urdf.hpp>

namespace dbrt
{
/**
 * \brief Represents the object tracker publisher. This publishes the object
 * estimated state and its marker.
 */
template <typename State>
class RobotTrackerPublisher : public dbot::TrackerPublisher<State>
{
public:
    RobotTrackerPublisher(
        const std::shared_ptr<KinematicsFromURDF>& urdf_kinematics,
        const std::shared_ptr<dbot::RigidBodyRenderer>& renderer,
        const std::string& tf_prefix);

    void publish(State& state,
                 const sensor_msgs::Image& image,
                 const std::shared_ptr<dbot::CameraData>& camera_data);

    void publishTransform(const ros::Time& time,
                          const std::string& from,
                          const std::string& to);

    void publishImage(const Eigen::VectorXd& depth_image,
                      const std::shared_ptr<dbot::CameraData>& camera_data,
                      const ros::Time& time,
                      const std::string& tf_prefix);

    void publishPointCloud(const Eigen::VectorXd& depth_image,
                           const std::shared_ptr<dbot::CameraData>& camera_data,
                           const ros::Time& time,
                           const std::string& tf_prefix);

    void convert_to_rgb_depth_image_msg(
        const std::shared_ptr<dbot::CameraData>& camera_data,
        const Eigen::VectorXd& depth_image,
        sensor_msgs::Image& image);

    void convert_to_depth_image_msg(
        const std::shared_ptr<dbot::CameraData>& camera_data,
        const Eigen::VectorXd& depth_image,
        sensor_msgs::Image& image);

    bool has_image_subscribers() const;
    bool has_point_cloud_subscribers() const;

protected:
    ros::NodeHandle node_handle_;
    std::string tf_prefix_;
    std::string root_;
    std::shared_ptr<robot_state_pub::RobotStatePublisher>
        robot_state_publisher_;
    std::shared_ptr<ros::Publisher> pub_point_cloud_;
    image_transport::Publisher pub_rgb_image_;
    image_transport::Publisher pub_depth_image_;
    std::shared_ptr<dbot::RigidBodyRenderer> robot_renderer_;
};
}