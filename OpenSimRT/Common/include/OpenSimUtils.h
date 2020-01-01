/**
 * @file OpenSimUtils.h
 *
 * \brief Common OpenSim operations and functions.
 *
 * @author Dimitar Stanev <jimstanev@gmail.com>
 */
#ifndef OPENSIM_UTILS_H
#define OPENSIM_UTILS_H

#include "internal/CommonExports.h"
#include <OpenSim/Simulation/Model/Model.h>

namespace OpenSimRT {

struct Common_API ModelUtils {
    // Generates a unique identifier
    static int generateUID();
    // Extract model's coordinate names.
    static std::vector<std::string>
    getCoordinateNames(const OpenSim::Model &model);
    //Extract model's muscle names.
    static std::vector<std::string>
    getMuscleNames(const OpenSim::Model &model);
    // Extract model's actuator names.
    static std::vector<std::string>
    getActuatorNames(const OpenSim::Model &model);
    // Disable actuators.
    static void disableActuators(OpenSim::Model &model);
    // Remove actuators.
    static void removeActuators(OpenSim::Model &model);
};

} // namespace OpenSimRT

#endif
