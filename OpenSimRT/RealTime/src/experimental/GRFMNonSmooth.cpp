/**
 * -----------------------------------------------------------------------------
 * Copyright 2019-2021 OpenSimRT developers.
 *
 * This file is part of OpenSimRT.
 *
 * OpenSimRT is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * OpenSimRT is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * OpenSimRT. If not, see <https://www.gnu.org/licenses/>.
 * -----------------------------------------------------------------------------
 */
#include "GRFMNonSmooth.h"
#include "Exception.h"
#include "OpenSimUtils.h"
#include "Utils.h"

using namespace std;
using namespace OpenSim;
using namespace OpenSimRT;
using namespace SimTK;

GRFMNonSmooth::GRFMNonSmooth(const Model& otherModel, string pelvisBodyName)
        : model(*otherModel.clone()), pelvisBodyName(pelvisBodyName) {

    // disable muscles, otherwise they apply passive forces
    OpenSimUtils::disableActuators(model);

    // initialize system
    state = model.initSystem();

}

GRFMNonSmooth::Method
GRFMNonSmooth::selectMethod(const std::string& methodName) {
    // lsits of lower-case valid names of the input strings
    vector<string> validNENames = {"newtoneuler", "newton-euler",
                                   "newton_euler", "ne"};
    vector<string> validIDNames = {"inversedynamics", "inverse-dynamics",
                                   "inverse_dynamics", "id"};

    // find if input method exists in lists and return selected enum type
    if (find(validNENames.begin(), validNENames.end(),
             String::toLower(methodName)) != validNENames.end())
        return Method::NewtonEuler;
    else if (find(validIDNames.begin(), validIDNames.end(),
                  String::toLower(methodName)) != validIDNames.end())
        return Method::InverseDynamics;
    else
        THROW_EXCEPTION("Wrong input method. Select appropriate input name.");
}

void GRFMNonSmooth::computeTotalReactionComponents(const Input& input,
                                                    Vec3& totalReactionForce,
                                                    Vec3& totalReactionMoment) {
    // get matter subsystem
    const auto& matter = model.getMatterSubsystem();

    // total forces / moments
    if (method == Method::InverseDynamics) {
        // ====================================================================
        // method 1: compute total forces/moment from pelvis using ID
        // ====================================================================

        // get applied mobility (generalized) forces generated by components of
        // the model, like actuators
        const Vector& appliedMobilityForces =
                model.getMultibodySystem().getMobilityForces(state,
                                                             Stage::Dynamics);

        // get all applied body forces like those from contact
        const Vector_<SpatialVec>& appliedBodyForces =
                model.getMultibodySystem().getRigidBodyForces(state,
                                                              Stage::Dynamics);

        // perform inverse dynamics
        Vector tau;
        model.getMultibodySystem()
                .getMatterSubsystem()
                .calcResidualForceIgnoringConstraints(
                        state, appliedMobilityForces, appliedBodyForces,
                        input.qDDot, tau);

        //====================================================================
        // spatial forces/moments in pelvis wrt the ground
        Vector_<SpatialVec> spatialGenForces;
        matter.multiplyBySystemJacobian(state, tau, spatialGenForces);
        const auto& idx = model.getBodySet()
                                  .get(pelvisBodyName)
                                  .getMobilizedBodyIndex();
        totalReactionForce = spatialGenForces[idx][1];
        totalReactionMoment = spatialGenForces[idx][0];

        // ===================================================================
        // method 2: compute the reaction forces/moment based on the
        // Newton-Euler equations
        //====================================================================
    } else if (method == Method::NewtonEuler) {
        // compute body velocities and accelerations
        SimTK::Vector_<SimTK::SpatialVec> bodyVelocities;
        SimTK::Vector_<SimTK::SpatialVec> bodyAccelerations;
        matter.multiplyBySystemJacobian(state, input.qDot, bodyVelocities);
        matter.calcBodyAccelerationFromUDot(state, input.qDDot,
                                            bodyAccelerations);

        for (int i = 0; i < model.getNumBodies(); ++i) {
            const auto& body = model.getBodySet()[i];
            const auto& bix = body.getMobilizedBodyIndex();

            // F_ext
            totalReactionForce += body.getMass() * (bodyAccelerations[bix][1] -
                                                    model.getGravity());

            // M_ext
            const auto& I = body.getInertia();
            totalReactionMoment +=
                    I * bodyAccelerations[bix][0] +
                    cross(bodyVelocities[bix][0], I * bodyVelocities[bix][0]);
        }
    }
}

GRFMNonSmooth::Output
GRFMNonSmooth::solve(const GRFMNonSmooth::Input& input) {
    Output output;
    output.t = input.t;
    output.right.force = Vec3(0.0);
    output.right.torque = Vec3(0.0);
    output.right.point = Vec3(0.0);
    output.left.force = Vec3(0.0);
    output.left.torque = Vec3(0.0);
    output.left.point = Vec3(0.0);

    return output;
}

