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
#include "Visualization.h"
#include "Exception.h"
#include "Utils.h"
#include <OpenSim/Simulation/Model/Muscle.h>
#include <Simulation/Model/Frame.h>
#include <Simulation/Model/ModelComponent.h>
#include <Simulation/Model/PhysicalFrame.h>
#include <Simulation/Model/PhysicalOffsetFrame.h>
#include <Simulation/SimbodyEngine/Body.h>
#include <simbody/internal/common.h>
#include <sstream>



using namespace std;
using namespace chrono;
using namespace SimTK;
using namespace OpenSimRT;

/******************************************************************************/

FPSDecorator::FPSDecorator() : text("") {}

void FPSDecorator::generateDecorations(const State& state,
		Array_<DecorativeGeometry>& geometry) {
	DecorativeText info;
	info.setIsScreenText(true);
	info.setText(text);
	geometry.push_back(info);
}

milliseconds FPSDecorator::calculateLoopDelay() {
	static int counter = 0;
	static high_resolution_clock::time_point previousTime =
		high_resolution_clock::now();
	counter++;
	auto currentTime = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(currentTime - previousTime);
	if (duration > milliseconds(1000)) {
		previousTime = currentTime;
		if (actual_delay >0)
		{	
			std::ostringstream my_text;
			my_text	<< "FPS: "<< std::fixed << std::setprecision(0) << counter << " | RealDelay: " << std::fixed << std::setprecision(2) << actual_delay   <<"ms";
			text = my_text.str();
		}
		else
			text = "FPS: " + to_string(counter) +
				" | Delay: " + toString(1000.0 / counter, 2) + "ms";
		counter = 0;
	}
	return duration;
}

/******************************************************************************/

ForceDecorator::ForceDecorator(Vec3 color, double scaleFactor, int lineThikness)
	: color(color), scaleFactor(scaleFactor), lineThikness(lineThikness) {

		mbdIndex = GroundIndex;
	}

	void ForceDecorator::update(SimTK::Vec3 point, SimTK::Vec3 force) {
		this->point = point;
		this->force = force;
	}

void ForceDecorator::generateDecorations(const State& state,
		Array_<DecorativeGeometry>& geometry) {
	if(mbdIndex.isValid())
		geometry.push_back(
			DecorativeLine(point, point + scaleFactor *force)
			.setBodyId(mbdIndex)
			.setColor(color)
			.setLineThickness(lineThikness));
	else
	{cerr << "MobilizedBodyIndex isnt valid" <<endl;
		geometry.push_back(
			DecorativeLine(point, point + scaleFactor *force)
			.setColor(color)
			.setLineThickness(lineThikness));
	}
}
void ForceDecorator::setOriginByName(const OpenSim::Model& model, std::string name)
{
	if(model.hasModel()&&model.isValidSystem()) // is this enough to check if the model is valid?
	{
		int bodyIndex = model.getBodySet().getIndex(name,0);
		if ( bodyIndex > 0)
		{
			const auto& body = model.getBodySet()[bodyIndex];
			mbdIndex = body.getMobilizedBodyIndex();
			if (mbdIndex.isValid())
				cout << "all ok here: bodyindex: "<< bodyIndex << "MobilizedBodyIndex: "<< mbdIndex <<endl;
			else
				mbdIndex = GroundIndex;
		}
		else
		{
			cerr << "couldnt find bodyIndex" << endl;
		}

	}
	else
	{
		cerr << "i dont seem to have a valid model"<<endl;
	}
}

/******************************************************************************/

BasicModelVisualizer::BasicModelVisualizer(const OpenSim::Model& otherModel)
	: model(*otherModel.clone()), shouldTerminate(false), fps(new FPSDecorator()) {
#ifndef CONTINUOUS_INTEGRATION
		model.setUseVisualizer(true);
#endif

		state = model.initSystem();

#ifndef CONTINUOUS_INTEGRATION
		visualizer = &model.updVisualizer().updSimbodyVisualizer();
		silo = &model.updVisualizer().updInputSilo();
		visualizer->setShowFrameRate(false);
		visualizer->setShutdownWhenDestructed(true);
		visualizer->setMode(Visualizer::Mode::Sampling);
		visualizer->setDesiredBufferLengthInSec(5);
		visualizer->setDesiredFrameRate(60);

		// add menu to visualizer //// TODO: add more if required
		Array_<std::pair<String, int> > runMenuItems;
		runMenuItems.push_back(std::make_pair("Quit", int(SimMenuItem::QUIT)));
		visualizer->addMenu("Simulation", int(MenuID::SIMULATION), runMenuItems);

		// add fps decorator
		//fps = new FPSDecorator();
		visualizer->addDecorationGenerator(fps.get());
		//fps->actual_delay =
#endif
	}


void BasicModelVisualizer::refreshModel()
{
		if(model.isValidSystem())
		{
			cout << "model ok" <<endl;
		visualizer = &model.updVisualizer().updSimbodyVisualizer();
	
		}
		else
			cerr << "model not ok" << endl;
}

void BasicModelVisualizer::update(const Vector& q,
		const Vector& muscleActivations) {
#ifndef CONTINUOUS_INTEGRATION
	fps->calculateLoopDelay();
#endif
	// kinematics
	state.updQ() = q;
	model.realizePosition(state);
	// muscle activations
	// TODO handle path actuators
	if (muscleActivations.size() == model.getMuscles().getSize()) {
		for (int i = 0; i < model.getMuscles().getSize(); ++i) {
			model.updMuscles().get(i).getGeometryPath().setColor(
					state,
					Vec3(muscleActivations[i], 0, 1 - muscleActivations[i]));
		}
	}
#ifndef CONTINUOUS_INTEGRATION
	visualizer->report(state);
	// terminate if ESC key is pressed
	unsigned key, modifiers;
	if (silo->takeKeyHit(key, modifiers)) {
		if (key == Visualizer::InputListener::KeyEsc) {
			shouldTerminate = true;
			silo->clear();
		}
	}

	// terminate simulation when menu option is selected
	int menuId = -1, item = -1;
	silo->takeMenuPick(menuId, item);
	if (menuId == int(MenuID::SIMULATION) && item == int(SimMenuItem::QUIT)) {
		shouldTerminate = true;
	}

	if (shouldTerminate) {
		visualizer->shutdown();
		THROW_EXCEPTION("Shutdown visualizer message received.");
	}
#endif
}

void BasicModelVisualizer::updateReactionForceDecorator(
		const Vector_<SpatialVec>& reactionWrench, const string& reactionOnBody,
		ForceDecorator* reactionForceDecorator) {
	auto bodyIndex = model.getBodySet().getIndex(reactionOnBody, 0);
	const auto& body = model.getBodySet()[bodyIndex];
	auto force = -reactionWrench[bodyIndex](1); // mirror force (1)
	auto joint = body.findStationLocationInGround(state, Vec3(0));
	reactionForceDecorator->update(joint, force);
}

void BasicModelVisualizer::addDecorationGenerator(
		DecorationGenerator* generator) {
#ifndef CONTINUOUS_INTEGRATION
	visualizer->addDecorationGenerator(generator);
#endif
}

void BasicModelVisualizer::expressPositionInGround(
		const std::string& fromBodyName, const SimTK::Vec3& fromBodyPoint,
		SimTK::Vec3& toBodyPoint) {
#ifndef CONTINUOUS_INTEGRATION
	model.realizePosition(state);
	const OpenSim::PhysicalOffsetFrame* physicalFrame = nullptr;
	const OpenSim::Body* body = nullptr;
	if ((body = model.findComponent<OpenSim::Body>(fromBodyName))) {
		toBodyPoint = body->findStationLocationInGround(state, fromBodyPoint);
	} else if ((physicalFrame =
				model.findComponent<OpenSim::PhysicalOffsetFrame>(
					fromBodyName))) {
		toBodyPoint = physicalFrame->findStationLocationInGround(state,
				fromBodyPoint);
	} else {
		THROW_EXCEPTION("Named body or frame does not exist.");
	}
#endif // CONTINUOUS_INTEGRATION
}
void BasicModelVisualizer::expressPositionInAnotherFrame(
		const std::string& fromBodyName, const SimTK::Vec3& fromBodyPoint,
		const std::string& toBodyName, SimTK::Vec3& toBodyPoint) {
#ifndef CONTINUOUS_INTEGRATION
	model.realizePosition(state);
	const OpenSim::PhysicalOffsetFrame* physicalFrame = nullptr;
	const OpenSim::Body* body = nullptr;
	if ((body = model.findComponent<OpenSim::Body>(fromBodyName))) {
		const auto& toBody = model.findComponent<OpenSim::Body>(toBodyName);
		toBodyPoint = body->findStationLocationInAnotherFrame(
				state, fromBodyPoint, *toBody);
	} else if ((physicalFrame =
				model.findComponent<OpenSim::PhysicalOffsetFrame>(
					fromBodyName))) {
		const auto& toPhysicalFrame =
			model.findComponent<OpenSim::PhysicalOffsetFrame>(toBodyName);
		toBodyPoint = physicalFrame->findStationLocationInAnotherFrame(
				state, fromBodyPoint, *toPhysicalFrame);
	} else {
		THROW_EXCEPTION("Named body or frame does not exist.");
	}
#endif // CONTINUOUS_INTEGRATION
}

/******************************************************************************/
