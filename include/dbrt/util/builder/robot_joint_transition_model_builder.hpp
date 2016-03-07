/*
 * This is part of the Bayesian Robot Tracking (brt),
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
 * \file robot_joint_transition_model_builder.hpp
 * \date January 2016
 * \author Jan Issac (jan.issac@gmail.com)
 */

#pragma once

#include <memory>

#include <fl/util/profiling.hpp>
#include <fl/util/meta.hpp>
#include <fl/model/process/linear_state_transition_model.hpp>

#include <Eigen/Dense>

#include <dbot/tracker/builder/state_transition_function_builder.hpp>

#include <dbrt/util/builder/invalid_number_of_joint_sigmas_exception.hpp>
#include <dbrt/util/builder/joint_index_out_of_bounds_exception.hpp>

namespace dbrt
{
class RobotJointTransitionModelBuilder
{
public:
    enum Dimension
    {
        DimState = 1,
        DimNoise = 1
    };

    typedef Eigen::Matrix<fl::Real, DimState, 1> State;
    typedef Eigen::Matrix<fl::Real, DimNoise, 1> Noise;
    typedef Eigen::Matrix<fl::Real, 1, 1> Input;
    typedef fl::LinearStateTransitionModel<State, Noise, Input> Model;

    struct Parameters
    {
        double joint_sigma;
        std::vector<double> joint_sigmas;
        int joint_count;
    };

    RobotJointTransitionModelBuilder(const Parameters& param) : param_(param) {}
    virtual std::shared_ptr<Model> build(int joint_index) const
    {
        if (param_.joint_count != param_.joint_sigmas.size())
        {
            throw InvalidNumberOfJointSigmasException();
        }

        if (joint_index < 0 || joint_index >= param_.joint_count)
        {
            throw JointIndexOutOfBoundsException();
        }

        auto model = std::make_shared<Model>(DimState, DimNoise, 1);

        auto A = model->create_dynamics_matrix();
        auto B = model->create_noise_matrix();
        auto C = model->create_input_matrix();

        A.setIdentity();
        B.setIdentity();
        B *= param_.joint_sigmas[joint_index];

        model->dynamics_matrix(A);
        model->noise_matrix(B);
        model->input_matrix(C);

        return model;
    }

private:
    Parameters param_;
};
}
