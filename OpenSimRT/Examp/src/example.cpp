#include <OpenSim/OpenSim.h>
#include <SimTKcommon/internal/Transform.h>
#include <Simulation/Control/Controller.h>
#include <Simulation/Control/PrescribedController.h>
#include <Simulation/Model/Frame.h>
#include <Simulation/SimbodyEngine/Body.h>
using OpenSim::PhysicalOffsetFrame;

using namespace SimTK;
using namespace OpenSim;

class CustomController: public OpenSim::PrescribedController
{
	public:
		CustomController(const OpenSim::Model& model) : model_(model),
		from_frame(model_.findComponent<OpenSim::Body>("humerus")), 
		to_frame(model_.findComponent<OpenSim::Body>("radius"))
	{}	
		void computeControls(const SimTK::State& s, SimTK::Vector& controls) const override
		{
			if (from_frame )
			{
				if (to_frame)
				{
					SimTK::Transform a  = from_frame->findTransformBetween(s, *to_frame);
					std::cout << a.toMat44() << std::endl;
					auto rotationTranslation = a;
					std::cout << "Rotation Matrix:" << std::endl;
					std::cout << rotationTranslation.R() << std::endl;
					std::cout << "Translation Vector:" << std::endl;
					std::cout << rotationTranslation.p() << std::endl;
				}
				else
					std::cout << "no tf| ";
			}
			
			  {
			  std::cout <<"no ff|";
			  if (to_frame)
			  std::cout<< "toframeYYES";
			  else
			  std::cout << "toframeNO";
			  }
		}
	private:
		const OpenSim::Model& model_;
		const OpenSim::Body* from_frame = nullptr;
		const OpenSim::Body* to_frame = nullptr;

};

int main() {
	Model model;
	model.setName("bicep_curl");
	model.setUseVisualizer(true);

	// Create two links, each with a mass of 1 kg, center of mass at the body's
	// origin, and moments and products of inertia of zero.
	OpenSim::Body* humerus = new OpenSim::Body("humerus", 1, Vec3(0), Inertia(0));
	OpenSim::Body* radius  = new OpenSim::Body("radius",  1, Vec3(0), Inertia(0));

	// Connect the bodies with pin joints. Assume each body is 1 m long.
	PinJoint* shoulder = new PinJoint("shoulder",
			// Parent body, location in parent, orientation in parent.
			model.getGround(), Vec3(0), Vec3(0),
			// Child body, location in child, orientation in child.
			*humerus, Vec3(0, 1, 0), Vec3(0));
	PinJoint* elbow = new PinJoint("elbow",
			*humerus, Vec3(0), Vec3(0), *radius, Vec3(0, 1, 0), Vec3(0));

	// Add a muscle that flexes the elbow.
	Millard2012EquilibriumMuscle* biceps = new
		Millard2012EquilibriumMuscle("biceps", 200, 0.6, 0.55, 0);
	biceps->addNewPathPoint("origin",    *humerus, Vec3(0, 0.8, 0));
	biceps->addNewPathPoint("insertion", *radius,  Vec3(0, 0.7, 0));

	// Add a controller that specifies the excitation of the muscle.
	PrescribedController* brain = new PrescribedController();
	brain->addActuator(*biceps);
	// Muscle excitation is 0.3 for the first 0.5 seconds, then increases to 1.
	brain->prescribeControlForActuator("biceps",
			new StepFunction(0.5, 3, 0.3, 1));

	// Add components to the model.
	model.addBody(humerus);    model.addBody(radius);
	model.addJoint(shoulder);  model.addJoint(elbow);
	model.addForce(biceps);
	model.addController(brain);
	;CustomController* cc;
	cc = new CustomController(model);
	model.addController(cc);


	// Add a console reporter to print the muscle fiber force and elbow angle.
	ConsoleReporter* reporter = new ConsoleReporter();
	reporter->set_report_time_interval(0.1);
	reporter->addToReport(biceps->getOutput("fiber_force"));
	reporter->addToReport(
			elbow->getCoordinate(PinJoint::Coord::RotationZ).getOutput("value"),
			"elbow_angle");
	model.addComponent(reporter);

	// Add display geometry.
	Ellipsoid bodyGeometry(0.1, 0.5, 0.1);
	bodyGeometry.setColor(Gray);
	// Attach an ellipsoid to a frame located at the center of each body.
	PhysicalOffsetFrame* humerusCenter = new PhysicalOffsetFrame(
			"humerusCenter", *humerus, Transform(Vec3(0, 0.5, 0)));
	humerus->addComponent(humerusCenter);
	humerusCenter->attachGeometry(bodyGeometry.clone());
	PhysicalOffsetFrame* radiusCenter = new PhysicalOffsetFrame(
			"radiusCenter", *radius, Transform(Vec3(0, 0.5, 0)));
	radius->addComponent(radiusCenter);
	radiusCenter->attachGeometry(bodyGeometry.clone());
	cc = new CustomController(model);

	// Configure the model.
	State& state = model.initSystem();
	// Fix the shoulder at its default angle and begin with the elbow flexed.
	shoulder->getCoordinate().setLocked(state, true);
	elbow->getCoordinate().setValue(state, 0.5 * Pi);
	model.equilibrateMuscles(state);

	// Configure the visualizer.
	model.updMatterSubsystem().setShowDefaultGeometry(true);
	Visualizer& viz = model.updVisualizer().updSimbodyVisualizer();
	viz.setBackgroundType(viz.SolidColor);
	viz.setBackgroundColor(White);
	

	// Simulate.
	simulate(model, state, 10.0);

	return 0;
};
