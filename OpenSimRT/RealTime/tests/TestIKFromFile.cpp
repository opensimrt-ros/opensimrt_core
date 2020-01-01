/**
 * @file TestIKFromFile.cpp
 *
 * \brief Loads the marker trajectories and executes inverse kinematics in an
 * iterative manner in order to determine the model kinematics.
 *
 * @author Dimitar Stanev <jimstanev@gmail.com>
 */
#include "INIReader.h"
#include "OpenSimUtils.h"
#include "Settings.h"
#include "Simulation.h"
#include "Visualization.h"

#include <OpenSim/Common/STOFileAdapter.h>
#include <OpenSim/Common/TimeSeriesTable.h>
#include <iostream>
#include <chrono>

using namespace std;
using namespace OpenSim;
using namespace SimTK;
using namespace OpenSimRT;

void run() {
    // subject data
    INIReader ini(INI_FILE);
    // auto section = "RAJAGOPAL_2015";
    auto section = "TEST_IK_GAIT2392";
    auto subjectDir = DATA_DIR + ini.getString(section, "SUBJECT_DIR", "");
    auto modelFile = subjectDir + ini.getString(section, "MODEL_FILE", "");
    auto trcFile = subjectDir + ini.getString(section, "TRC_FILE", "");
    auto ikTaskSetFile = subjectDir + ini.getString(section, "IK_TASK_SET_FILE", "");

    // setup model
    Model model(modelFile);
    ModelUtils::removeActuators(model);

    // construct marker tasks from marker data (.trc)
    IKTaskSet ikTaskSet(ikTaskSetFile);
    MarkerData markerData(trcFile);
    vector<InverseKinematics::MarkerTask> markerTasks;
    vector<string> observationOrder;
    InverseKinematics::createMarkerTasksFromIKTaskSet(
            model, ikTaskSet, markerTasks, observationOrder);

    // initialize loggers
    auto coordinateColumnNames = ModelUtils::getCoordinateNames(model);
    TimeSeriesTable q;
    q.setColumnLabels(coordinateColumnNames);

    // initialize ik
    InverseKinematics ik(model, markerTasks,
                         vector<InverseKinematics::IMUTask>{}, 100);

    // visualizer
    BasicModelVisualizer visualizer(model);

    // mean delay
    int sumDelayMS = 0;

    // loop through marker frames
    for (int i = 0; i < markerData.getNumFrames(); ++i) {
        // get frame data
        auto frame = InverseKinematics::getFrameFromMarkerData(
                i, markerData, observationOrder, false);

        // perform ik
        chrono::high_resolution_clock::time_point t1;
        t1 = chrono::high_resolution_clock::now();

        auto pose = ik.solve(frame);

        chrono::high_resolution_clock::time_point t2;
        t2 = chrono::high_resolution_clock::now();
        sumDelayMS +=
                chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();

        // visualize
        visualizer.update(pose.q);

        // record
        q.appendRow(pose.t, ~pose.q);
    }

    cout << "Mean delay: " << (double) sumDelayMS / markerData.getNumFrames()
         << " ms" << endl;

    // store results
    STOFileAdapter::write(q, subjectDir + "real_time/inverse_kinematics/q.sto");
}

int main(int argc, char* argv[]) {
    try {
        run();
    } catch (exception& e) {
        cout << e.what() << endl;
        return -1;
    }
    return 0;
}
