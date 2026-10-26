// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "vehicle.hpp"

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "../Audio.hpp"
#include "../Graphics.hpp"
#include "../System.hpp"
#include "../physics.hpp"
#include "level.hpp"
#include "object.hpp"
#include "player.hpp"

#include <utility> // std::move

namespace {

using Horsepower = phys::MultipleOf<phys::Watts, 745.7l>;
constexpr Horsepower HORSEPOWER{};

constexpr phys::CollisionFilter VEHICLE_COLLISION_FILTER{
	.layers = COLLISION_LAYER_VEHICLE,
};

struct VehicleSettingsJSON {
	static constexpr CStringView FILEPATH = "schema/vehicle.json";

	struct Type {
		struct Model {
			String filepath{};
			vec3 rootTranslation{};
			quat rootRotation{0.0f, 0.0f, 0.0f, 1.0f};
			vec3 rootScale{1.0f};
		} model{};

		struct Body {
			phys::Mass mass = 1600_kilograms;
			phys::Material material{.linearDrag = 0.1_x, .angularDrag = 0.1_x};
			phys::Length3D downforceOffset{};
			phys::Area downforceEffectiveArea = 0.5_square_meters;
		} body{};

		struct Engine {
			phys::Coefficient flatness = 15_x; // Typical range: [10, 18]
			phys::Quantity<1, Horsepower> idealHP = 170.0f * HORSEPOWER;
			phys::Quantity<1, Horsepower> redlineHP = 140.0f * HORSEPOWER;
			phys::Quantity<1, phys::RevolutionsPerMinute> stallRPM = 100_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> idleRPM = 750_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> idealRPM = 3500_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> redlineRPM = 6000_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> automaticCruiseShiftUpThresholdRPM = 2000_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> automaticCruiseShiftDownThresholdRPM = min(idleRPM * 1.5_x, automaticCruiseShiftUpThresholdRPM * 0.5_x);
			phys::Quantity<1, phys::RevolutionsPerMinute> automaticGasShiftUpThresholdRPM = lerp(idealRPM, redlineRPM, 0.8_x);
			phys::Quantity<1, phys::RevolutionsPerMinute> automaticGasShiftDownThresholdRPM = min(idealRPM * 0.8_x, automaticGasShiftUpThresholdRPM * 0.8_x);
			phys::Time automaticShiftUpTime = 0.25_seconds;
			phys::Time automaticShiftDownTime = 0.5_seconds;
			phys::MomentOfInertia2D rotationalInertia = 0.2_kilogram_square_meters;
			phys::Frequency synchronizationFrequency = 30_Hertz;
			phys::Frequency frictionFrequency = 0.5_Hertz;
			phys::Time startupTime = 1_second;
		} engine{};

		struct Transmission {
			ArrayList<phys::Coefficient> gearRatios{3.6_x, 2.4_x, 1.8_x, 1.3_x, 1_x};
			phys::Coefficient reverseGearRatio = 3.5_x;
			bool automatic = false;
		} transmission{};

		struct Drive {
			ArrayList<size_t> wheels{0, 1};
		} drive{};

		struct Differential {
			phys::Coefficient locking = 0.4_x;
			phys::Coefficient ratio = 4.1_x;
		} differential{};

		struct Turning {
			ArrayList<size_t> wheels{0, 1};
			phys::Distance radius = 5_meters;
		} turning{};

		struct Brakes {
			phys::Coefficient balance = 0.65_x;
			phys::Torque2D torque = 3500_Newton_meters;
		} brakes{};

		struct Wheels {
			phys::Frequency synchronizationFrequency = 30_Hertz;
			phys::Frequency frictionFrequency = 0.05_Hertz;
			phys::MomentOfInertia2D rotationalInertia = 0.7_kilogram_square_meters;
		} wheels{};

		struct Tires {
			struct Friction {
				phys::Coefficient stiffness = 10_x;
				phys::Coefficient shape = 1.9_x;
				phys::Coefficient peak = 1_x;
				phys::Coefficient curvature = 0.97_x;

				[[nodiscard]] phys::Force1D getContactForce(phys::Coefficient slipRatio, phys::Force1D loadForce) const {
					const phys::Coefficient slip = stiffness * slipRatio;
					return loadForce * peak * sin(shape * atan(slip - curvature * (slip - atan(slip))));
				}
			};

			Friction turningLongitudalFriction{.peak = 1.7_x};
			Friction turningLateralFriction{.shape = 1.6_x, .peak = 1.4_x};
			Friction nonTurningLongitudalFriction{.peak = 1.7_x};
			Friction nonTurningLateralFriction{.shape = 1.6_x, .peak = 1.2_x};
		} tires{};

		struct Suspension {
			phys::Quantity<1, phys::NewtonPerMillimeter> stiffness = 100_Newton_per_millimeter;
			phys::Coefficient dampingRatio = 0.5_x;
			phys::Distance maxOffset = 0.25_meters;
			phys::Force1D maxForce = 100000_Newton;

			struct TurningWheelMultipliers {
				phys::Coefficient stiffness = 0.8_x;
				phys::Coefficient dampingRatio = 0.8_x;
				phys::Coefficient maxOffset = 0.8_x;
			} turningWheelMultipliers{};
		} suspension{};

		struct Seat {
			phys::Length3D offset{};
			phys::Length3D exitOffset{};
		};
		ArrayList<Seat> seats{Seat{}};
	};

	HashMap<String, Type> types{};
};

struct VehicleInfo {
	struct Wheel {
		phys::Length3D localOffset;
		phys::Distance radius;
		phys::Distance halfWidth;
		res::Model::JointIndex jointIndex;
		phys::MassFlowRate suspensionDamping;
		bool turns;
		bool drives;
	};

	ArrayList<Wheel> wheels{};
	phys::Length3D averageNonTurningWheelOffset{};
	phys::Angle maxTurnAngle{};
	gfx::Model3D model{};
	phys::Shape3D shape{};
};

// Resources:

struct VehicleSettings : VehicleSettingsJSON {
	explicit VehicleSettings(VehicleSettingsJSON&& settingsJSON)
		: VehicleSettingsJSON(std::move(settingsJSON)) {}
};

struct VehicleInfos : HashMap<const VehicleSettingsJSON::Type*, VehicleInfo> {};

struct VehicleExpiredEntities : ArrayList<phys::EntityID> {};

// Components:

using VehicleType = const VehicleSettingsJSON::Type*;

struct VehicleSeats {
	struct Occupant {
		phys::EntityID entityID;
	};

	ArrayList<Occupant> occupants{};
};

struct VehicleEngine {
	phys::AngularSpeed rotationalSpeed{};
	phys::AngularSpeed previousRotationalSpeed = rotationalSpeed;
	phys::Time startingTime{};
	phys::Time timeSpentBelowShiftDownThreshold{};
	phys::Time timeSpentAboveShiftUpThreshold{};
	bool running = false;
	bool starting = false;
};

struct VehicleDrivetrain {
	int32_t gear = 0; // -1: reverse, 0: neutral, 1+: drive
	phys::Torque2D torque{};
	phys::AngularVelocity2D rotationalVelocity{};
};

struct VehicleWheels {
	struct Wheel {
		phys::Length3D suspensionOffset{};
		phys::Length3D previousSuspensionOffset = suspensionOffset;
		phys::Orientation3D suspensionOrientation{};
		phys::Orientation3D previousSuspensionOrientation = suspensionOrientation;
		phys::AbsoluteAngle rotationAngle{};
		phys::AbsoluteAngle previousRotationAngle = rotationAngle;
		phys::AngularVelocity2D rotationalVelocity{};
		phys::AngularVelocity2D previousRotationalVelocity = rotationalVelocity;
	};

	ArrayList<Wheel> wheels{};
};

struct VehicleControls {
	enum class KeyPosition : uint8_t {
		OFF,
		IGNITION,
		START,
	};

	phys::Coefficient gasDirect{};
	phys::Coefficient gasSmoothTarget{};
	phys::Coefficient gasSmooth{};
	phys::Coefficient brakeDirect{};
	phys::Coefficient brakeSmoothTarget{};
	phys::Coefficient brakeSmooth{};
	phys::Coefficient handbrake = 1_x;
	phys::Coefficient clutchDirect{};
	phys::Coefficient clutchSmoothTarget{};
	phys::Coefficient clutchSmooth{};
	phys::Scale1D steeringDirect{};
	phys::Scale1D steeringSmoothTarget{};
	phys::Scale1D steeringSmooth{};
	phys::Scale1D steeringFromAim{};
	int32_t gearStickPosition = 0; // -1: reverse, 0: neutral, 1+: drive
	phys::Scale1D shiftingFromAim{};
	int32_t shiftingFromAimPreviousGearStickPosition = 0;
	KeyPosition keyPosition = KeyPosition::OFF;
};

// Systems:

void exitDestroyedVehicles(phys::EntityRegistry3D& registry, VehicleExpiredEntities& expiredEntities) {
	expiredEntities.clear();
	for (auto&& [entityID, occupant] : registry.getEntities<VehicleOccupant>()) {
		if (!registry.hasComponent<VehicleTag>(occupant.vehicleEntityID)) {
			expiredEntities.push_back(entityID);
		}
	}

	for (const phys::EntityID entityID : expiredEntities) {
		exitVehicle(registry, entityID);
	}
}

void resetVacantVehicleSeats(exec::Entities<VehicleSeats> vehicleEntities, exec::Entities<const VehicleOccupant> occupantEntities) {
	for (auto&& [entityID, seats] : vehicleEntities) {
		erase_if(seats.occupants, [&](const VehicleSeats::Occupant& occupant) -> bool {
			if (!occupantEntities.containsEntity(occupant.entityID)) {
				return true;
			}
			const auto& [occupantID, vehicleOccupant] = occupantEntities[occupant.entityID];
			return vehicleOccupant.vehicleEntityID != entityID;
		});
	}
}

void controlVehicles(exec::Entities<VehicleControls, const VehicleEngine, const VehicleType> vehicleEntities,
	exec::Entities<const PlayerInput, const VehicleOccupant, const VehicleDriverTag> driverEntities, const phys::SimulationOptions3D& simulationOptions) {
	const phys::Time deltaTime = simulationOptions.stepInterval;

	for (auto&& [entityID, input, occupant, driverTag] : driverEntities) {
		VehicleControls& controls = vehicleEntities.getComponent<VehicleControls>(occupant.vehicleEntityID);
		const VehicleEngine& engine = vehicleEntities.getComponent<VehicleEngine>(occupant.vehicleEntityID);
		const VehicleType type = vehicleEntities.getComponent<VehicleType>(occupant.vehicleEntityID);

		controls.gasDirect = clamp(input.getCurrentState(PlayerAction::GAS).value, 0.0f, 1.0f);
		controls.gasSmoothTarget = clamp(input.getCurrentState(PlayerAction::GAS_SMOOTH).value, 0.0f, 1.0f);
		controls.gasSmooth = expDecay(controls.gasSmooth, controls.gasSmoothTarget, input.settings.gasSmoothDecayRate, deltaTime);

		controls.brakeDirect = clamp(input.getCurrentState(PlayerAction::BRAKE).value, 0.0f, 1.0f);
		controls.brakeSmoothTarget = clamp(input.getCurrentState(PlayerAction::BRAKE_SMOOTH).value, 0.0f, 1.0f);
		controls.brakeSmooth = expDecay(controls.brakeSmooth, controls.brakeSmoothTarget, input.settings.brakeSmoothDecayRate, deltaTime);

		controls.handbrake = clamp(input.getCurrentState(PlayerAction::HANDBRAKE).value, 0.0f, 1.0f);

		controls.clutchDirect = clamp(input.getCurrentState(PlayerAction::CLUTCH).value, 0.0f, 1.0f);
		controls.clutchSmoothTarget = clamp(input.getCurrentState(PlayerAction::CLUTCH_SMOOTH).value, 0.0f, 1.0f);
		controls.clutchSmooth = expDecay(controls.clutchSmooth, controls.clutchSmoothTarget, input.settings.clutchSmoothDecayRate, deltaTime);

		controls.steeringDirect = clamp(input.getCurrentState1D(PlayerAction::STEER_LEFT, PlayerAction::STEER_RIGHT).value, -1.0f, 1.0f);
		controls.steeringSmoothTarget = clamp(input.getCurrentState1D(PlayerAction::STEER_LEFT_SMOOTH, PlayerAction::STEER_RIGHT_SMOOTH).value, -1.0f, 1.0f);
		controls.steeringSmooth = expDecay(controls.steeringSmooth, controls.steeringSmoothTarget, input.settings.steeringSmoothDecayRate, deltaTime);
		if (input.isPressed(PlayerAction::STEER_WITH_AIM) != input.settings.invertSteerWithAim) {
			controls.steeringFromAim += input.getRelativeState1D(PlayerAction::AIM_LEFT, PlayerAction::AIM_RIGHT).motion * input.settings.steeringFromAimSensitivity;
			controls.steeringFromAim = clamp(controls.steeringFromAim, -1_x, 1_x);
		} else {
			controls.steeringSmooth += controls.steeringFromAim;
			controls.steeringFromAim = {};
		}

		if (input.justPressed(PlayerAction::GEAR_SHIFT_UP)) {
			++controls.gearStickPosition;
		}
		if (input.justPressed(PlayerAction::GEAR_SHIFT_DOWN)) {
			--controls.gearStickPosition;
		}
		if (input.isPressed(PlayerAction::GEAR_SHIFT_WITH_AIM)) {
			controls.shiftingFromAim += input.getRelativeState1D(PlayerAction::AIM_DOWN, PlayerAction::AIM_UP).motion * input.settings.shiftFromAimSensitivity;
			if (controls.shiftingFromAim >= 1_x) {
				controls.gearStickPosition = controls.shiftingFromAimPreviousGearStickPosition + 1;
			} else if (controls.shiftingFromAim <= -1_x) {
				controls.gearStickPosition = controls.shiftingFromAimPreviousGearStickPosition - 1;
			} else {
				controls.gearStickPosition = controls.shiftingFromAimPreviousGearStickPosition;
			}
		} else {
			if (controls.shiftingFromAimPreviousGearStickPosition != controls.gearStickPosition) {
				controls.shiftingFromAimPreviousGearStickPosition = controls.gearStickPosition;
				controls.shiftingFromAim = {};
			} else {
				controls.shiftingFromAim = expDecay(controls.shiftingFromAim, 8_Hertz, deltaTime);
			}
		}
		controls.gearStickPosition = clamp(controls.gearStickPosition, -1, static_cast<int32_t>(type->transmission.gearRatios.size()));

		if (input.justPressed(PlayerAction::IGNITION)) {
			if (engine.running) {
				controls.keyPosition = VehicleControls::KeyPosition::OFF;
			} else {
				controls.keyPosition = VehicleControls::KeyPosition::START;
			}
		}
		if (input.justReleased(PlayerAction::IGNITION)) {
			if (controls.keyPosition == VehicleControls::KeyPosition::START) {
				controls.keyPosition = (engine.running) ? VehicleControls::KeyPosition::IGNITION : VehicleControls::KeyPosition::OFF;
			} else {
				controls.keyPosition = VehicleControls::KeyPosition::OFF;
			}
		}
	}
}

void simulateVehicleEngines(exec::Entities<phys::Position3D, phys::Orientation3D, phys::Scale3D, phys::LinearVelocity3D, phys::AngularVelocity3D, VehicleEngine, VehicleDrivetrain,
								const VehicleWheels, const VehicleControls, const VehicleType>
								vehicleEntities,
	const phys::SimulationOptions3D& simulationOptions) {
	const phys::Time deltaTime = simulationOptions.stepInterval;

	for (auto&& [entityID, position, orientation, scale, linearVelocity, angularVelocity, engine, drivetrain, wheels, controls, type] : vehicleEntities) {
		engine.previousRotationalSpeed = engine.rotationalSpeed;

		switch (controls.keyPosition) {
			case VehicleControls::KeyPosition::OFF:
				engine.running = false;
				engine.starting = false;
				break;
			case VehicleControls::KeyPosition::IGNITION: engine.starting = false; break;
			case VehicleControls::KeyPosition::START:
				if (!engine.running) {
					if (!engine.starting) {
						engine.starting = true;
						engine.startingTime = {};
					}
					if (engine.startingTime < type->engine.startupTime) {
						engine.rotationalSpeed = max(engine.rotationalSpeed, midpoint(type->engine.stallRPM, type->engine.idleRPM).as(phys::AngularSpeed::UNIT));
						if (countup(engine.startingTime, deltaTime, type->engine.startupTime)) {
							engine.running = true;
							engine.starting = false;
						}
					}
				}
				break;
		}

		const phys::Coefficient clutch = clamp(controls.clutchDirect + controls.clutchSmooth, 0_x, 1_x);
		const phys::Coefficient gas = clamp(controls.gasDirect + controls.gasSmooth, 0_x, 1_x);

		const phys::Coefficient oldEngagement = (drivetrain.gear == 0) ? 0_x : (1_x - clutch);
		const phys::Coefficient oldGearRatio =
			(drivetrain.gear == 0)  ? 1_x
			: (drivetrain.gear < 0) ? -type->transmission.reverseGearRatio
			                        : type->transmission.gearRatios[static_cast<size_t>(drivetrain.gear - 1)];
		const phys::AngularVelocity2D disengagedRotationalVelocity = engine.rotationalSpeed;
		const phys::AngularVelocity2D engagedRotationalVelocity = drivetrain.rotationalVelocity * oldGearRatio;
		const phys::AngularVelocity2D expectedRotationalVelocity =
			min(lerp(disengagedRotationalVelocity, engagedRotationalVelocity, oldEngagement), type->engine.redlineRPM.as(phys::AngularSpeed::UNIT));
		engine.rotationalSpeed = expDecay(engine.rotationalSpeed, expectedRotationalVelocity, type->engine.synchronizationFrequency, deltaTime);
		engine.rotationalSpeed = expDecay(engine.rotationalSpeed, type->engine.frictionFrequency, deltaTime);

		if (type->transmission.automatic) {
			if (engine.running) {
				if (controls.gearStickPosition == 0) {
					drivetrain.gear = 0;
				} else {
					const bool aboutToStall = engine.rotationalSpeed < type->engine.stallRPM.as(phys::AngularSpeed::UNIT) * 2_x;
					const bool cruising = gas < 0.5_x;
					const phys::AngularSpeed shiftUpThreshold =
						((cruising) ? type->engine.automaticCruiseShiftUpThresholdRPM : type->engine.automaticGasShiftUpThresholdRPM).as(phys::AngularSpeed::UNIT);
					const phys::AngularSpeed shiftDownThreshold =
						((cruising) ? type->engine.automaticCruiseShiftDownThresholdRPM : type->engine.automaticGasShiftDownThresholdRPM).as(phys::AngularSpeed::UNIT);
					if (aboutToStall && drivetrain.gear != 0) {
						drivetrain.gear = 0;
						engine.timeSpentBelowShiftDownThreshold = {};
						engine.timeSpentAboveShiftUpThreshold = {};
					} else if (drivetrain.gear != 0 && engine.rotationalSpeed < shiftDownThreshold) {
						if (countup(engine.timeSpentBelowShiftDownThreshold, deltaTime, type->engine.automaticShiftDownTime)) {
							drivetrain.gear = (controls.gearStickPosition < 0) ? 0 : max(drivetrain.gear - 1, int32_t{0});
							engine.timeSpentBelowShiftDownThreshold = {};
						}
					} else {
						engine.timeSpentBelowShiftDownThreshold = {};
						if (engine.rotationalSpeed > shiftUpThreshold ||
							(drivetrain.gear == 0 && engine.rotationalSpeed > type->engine.idleRPM.as(phys::AngularSpeed::UNIT) * 1.5_x)) {
							if (engine.rotationalSpeed > type->engine.redlineRPM.as(phys::AngularSpeed::UNIT) || drivetrain.gear == 0 ||
								countup(engine.timeSpentAboveShiftUpThreshold, deltaTime, type->engine.automaticShiftUpTime)) {
								drivetrain.gear = (controls.gearStickPosition < 0) ? -1 : min(drivetrain.gear + 1, static_cast<int32_t>(type->transmission.gearRatios.size()));
								engine.timeSpentAboveShiftUpThreshold = {};
							}
						} else {
							engine.timeSpentAboveShiftUpThreshold = {};
						}
					}
				}
			} else {
				drivetrain.gear = 0;
			}
		} else {
			drivetrain.gear = clamp(controls.gearStickPosition, -1, static_cast<int32_t>(type->transmission.gearRatios.size()));
		}

		const phys::Torque2D minTorque = type->engine.redlineHP.as(phys::Power::UNIT) / type->engine.redlineRPM.as(phys::AngularSpeed::UNIT);
		const phys::Torque2D maxTorque = type->engine.idealHP.as(phys::Power::UNIT) / type->engine.idealRPM.as(phys::AngularSpeed::UNIT);
		const phys::Coefficient torqueCurve =
			exp(-length2((engine.rotationalSpeed - type->engine.idealRPM.as(phys::AngularSpeed::UNIT)) / (length2(type->engine.flatness) * phys::Frequency::UNIT)));
		const phys::Torque2D availableTorque = (engine.running) ? lerp(minTorque, maxTorque, torqueCurve) : phys::Torque2D{};
		const phys::Torque2D idleTorque =
			(engine.running) ? min(type->engine.frictionFrequency * type->engine.idleRPM.as(phys::AngularSpeed::UNIT) * type->engine.rotationalInertia, availableTorque)
			                 : phys::Torque2D{};
		const phys::Torque2D torque = lerp(idleTorque, availableTorque, gas);

		const phys::Coefficient engagement = (drivetrain.gear == 0) ? 0_x : (1_x - clutch);
		const phys::Coefficient gearRatio =
			(drivetrain.gear == 0)  ? 1_x
			: (drivetrain.gear < 0) ? -type->transmission.reverseGearRatio
			                        : type->transmission.gearRatios[static_cast<size_t>(drivetrain.gear - 1)];
		drivetrain.torque = torque * engagement * gearRatio;

		engine.rotationalSpeed += torque * deltaTime / type->engine.rotationalInertia;
		if (engine.rotationalSpeed < type->engine.stallRPM.as(phys::AngularSpeed::UNIT)) {
			engine.rotationalSpeed = 0;
			engine.running = false;
		}

		drivetrain.rotationalVelocity = lerp(drivetrain.rotationalVelocity, engine.rotationalSpeed / gearRatio, engagement);
	}
}

void simulateVehicleWheels(
	exec::Entities<phys::Position3D, phys::Orientation3D, phys::Scale3D, phys::ObjectActivity, phys::LinearVelocity3D, phys::AngularVelocity3D, VehicleWheels, VehicleDrivetrain,
		const phys::InverseMass, const phys::InverseMomentOfInertiaTensor3D, const phys::FluidDensity, const VehicleControls, const VehicleType>
		vehicleEntities,
	exec::Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D> objectEntities,
	exec::Entities<phys::ObjectActivity, phys::LinearVelocity3D, phys::AngularVelocity3D, const phys::InverseMass, const phys::InverseMomentOfInertiaTensor3D> kinematicsEntities,
	exec::Entities<const phys::Material> materialEntities, const VehicleInfos& infos, const phys::Broadphase3D& broadphase, const phys::SimulationOptions3D& simulationOptions) {
	const phys::Time deltaTime = simulationOptions.stepInterval;

	for (auto&& [entityID, position, orientation, scale, activity, linearVelocity, angularVelocity, wheels, drivetrain, inverseMass, inverseMomentOfInertiaTensor,
			 surroundingFluidDensity, controls, type] : vehicleEntities) {
		const VehicleInfo& info = infos.at(type);

		const phys::OrthonormalBasis3D bodyBasis = rotate(orientation);
		const phys::LinearVelocity1D bodyForwardVelocity = max(dot(linearVelocity, -bodyBasis[phys::Z]), phys::LinearVelocity1D{});
		const phys::Force3D downforce = -0.5_x * surroundingFluidDensity * length2(bodyForwardVelocity) * type->body.downforceEffectiveArea * bodyBasis[phys::Y];
		phys::applyCorrectionImpulse(activity, linearVelocity, angularVelocity, inverseMass, inverseMomentOfInertiaTensor, downforce * deltaTime,
			orientation(scale * type->body.downforceOffset));

		const phys::Coefficient brake = clamp(controls.brakeDirect + controls.brakeSmooth, 0_x, 1_x);
		const phys::Coefficient handbrake = clamp(controls.handbrake, 0_x, 1_x);
		const phys::Scale1D steering = clamp(controls.steeringDirect + controls.steeringSmooth + controls.steeringFromAim, -1_x, 1_x);
		const phys::Angle centerTurnAngle = steering * info.maxTurnAngle;

		const size_t wheelCount = info.wheels.size();

		size_t fastestDrivenWheelIndex = wheelCount;
		size_t slowestDrivenWheelIndex = wheelCount;
		phys::Coefficient drivenWheelCount{};
		for (size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex) {
			const VehicleWheels::Wheel& wheel = wheels.wheels[wheelIndex];
			const VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];
			if (wheelInfo.drives) {
				if (fastestDrivenWheelIndex >= wheelCount || abs(wheel.rotationalVelocity) > abs(wheels.wheels[fastestDrivenWheelIndex].rotationalVelocity)) {
					fastestDrivenWheelIndex = wheelIndex;
				}
				if (slowestDrivenWheelIndex >= wheelCount || abs(wheel.rotationalVelocity) < abs(wheels.wheels[slowestDrivenWheelIndex].rotationalVelocity)) {
					slowestDrivenWheelIndex = wheelIndex;
				}
				drivenWheelCount += 1_x;
			}
		}
		const phys::AngularSpeed drivenWheelSpeedDifference =
			(fastestDrivenWheelIndex < wheelCount)
		        ? phys::AngularSpeed{abs(abs(wheels.wheels[fastestDrivenWheelIndex].rotationalVelocity) - abs(wheels.wheels[slowestDrivenWheelIndex].rotationalVelocity))}
		        : phys::AngularSpeed{};

		const phys::AngularVelocity2D driveRotationalVelocity = drivetrain.rotationalVelocity / type->differential.ratio;

		drivetrain.rotationalVelocity = {};
		for (size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex) {
			VehicleWheels::Wheel& wheel = wheels.wheels[wheelIndex];
			const VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];

			wheel.previousSuspensionOffset = wheel.suspensionOffset;
			wheel.previousSuspensionOrientation = wheel.suspensionOrientation;
			wheel.previousRotationAngle = wheel.rotationAngle;
			wheel.previousRotationalVelocity = wheel.rotationalVelocity;

			wheel.suspensionOrientation = orientation;
			if (wheelInfo.turns && centerTurnAngle != 0) {
				const phys::Angle turnAngle = atan2(info.averageNonTurningWheelOffset.getZ() - wheelInfo.localOffset.getZ(),
					info.averageNonTurningWheelOffset.getZ() / tan(centerTurnAngle) - wheelInfo.localOffset.getX());
				const phys::Angle turnYaw = (centerTurnAngle < 0) ? phys::Angle{phys::PI - turnAngle} : phys::Angle{-turnAngle};
				wheel.suspensionOrientation = wheel.suspensionOrientation * phys::Orientation3D::yaw(turnYaw);
			}

			const phys::Position3D restCenter = position + orientation(scale * wheelInfo.localOffset);
			const phys::OrthonormalBasis3D suspensionBasis = rotate(wheel.suspensionOrientation);
			const phys::Direction3D sideDirection = suspensionBasis[phys::X];
			const phys::Direction3D forwardDirection = -suspensionBasis[phys::Z];
			const phys::Direction3D suspensionDirection = suspensionBasis[phys::Y];
			const phys::Distance maxOffset = type->suspension.maxOffset * ((wheelInfo.turns) ? type->suspension.turningWheelMultipliers.maxOffset : 1_x);
			const phys::Distance radius = wheelInfo.radius * scale.getY();
			const phys::Distance halfWidth = wheelInfo.halfWidth * scale.getX();
			const phys::Length1D castOffset = maxOffset + radius;
			const phys::Transformation3D castTransformation = translateRotateScale(restCenter + suspensionDirection * castOffset,
				wheel.suspensionOrientation * phys::Orientation3D::roll(90_degrees), phys::Scale3D{radius / 1_meter, halfWidth / 1_meter, radius / 1_meter});

			wheel.rotationalVelocity = expDecay(wheel.rotationalVelocity, type->wheels.frictionFrequency, deltaTime);

			phys::Coefficient driveContribution = 0_x;
			if (wheelInfo.drives) {
				driveContribution = ((drivenWheelSpeedDifference < phys::AngularSpeed::MACHINE_EPSILON)
				                         ? 1_x / drivenWheelCount
				                         : ((wheelIndex == fastestDrivenWheelIndex) ? type->differential.locking : (1_x - type->differential.locking)) / (drivenWheelCount - 1_x));

				const phys::Torque2D driveTorque = driveContribution * drivetrain.torque * type->differential.ratio;
				wheel.rotationalVelocity += driveTorque * deltaTime / type->wheels.rotationalInertia;

				const phys::AngularVelocity2D expectedRotationalVelocity = driveContribution * driveRotationalVelocity;
				wheel.rotationalVelocity = expDecay(wheel.rotationalVelocity, expectedRotationalVelocity, type->wheels.synchronizationFrequency, deltaTime);
			}

			if (const Optional<phys::Broadphase3D::ShapecastResult> shapecastResult = broadphase.shapecastClosestHit(phys::SphereShape3D{.radius = 1_meter},
					VEHICLE_COLLISION_FILTER, castTransformation, -suspensionDirection, castOffset + 0.05_x * radius, objectEntities, simulationOptions.collisionAlgorithmOptions,
					phys::CollisionFilterTest::RESPONSE, [&](phys::EntityID otherObjectID) -> bool { return otherObjectID != entityID; })) {
				const phys::Length1D suspensionSpringOffset = clamp(castOffset - shapecastResult->distance, phys::Length1D{}, maxOffset);
				wheel.suspensionOffset = suspensionSpringOffset * suspensionDirection;
				const phys::Position3D contactPoint = restCenter + wheel.suspensionOffset - radius * shapecastResult->normal;

				const phys::Stiffness suspensionSpringStiffness = type->suspension.stiffness * ((wheelInfo.turns) ? type->suspension.turningWheelMultipliers.stiffness : 1_x);
				const phys::Force1D suspensionSpringForce = clampLength(suspensionSpringStiffness * suspensionSpringOffset, type->suspension.maxForce);

				const phys::Position3D positionB = objectEntities.getComponent<phys::Position3D>(shapecastResult->objectID);
				const phys::Length3D offsetA = contactPoint - position;
				const phys::Length3D offsetB = contactPoint - positionB;
				auto&& [entityIDB, activityB, linearVelocityB, angularVelocityB, inverseMassB, inverseMomentOfInertiaTensorB] = kinematicsEntities[shapecastResult->objectID];
				const phys::LinearVelocity3D velocityA = phys::calculatePointVelocity(linearVelocity, angularVelocity, offsetA);
				const phys::LinearVelocity3D velocityB = phys::calculatePointVelocity(linearVelocityB, angularVelocityB, offsetB);
				const phys::LinearVelocity3D relativeVelocity = velocityA - velocityB;
				const phys::LinearVelocity1D relativeVelocityAlongSuspensionAxis = dot(relativeVelocity, suspensionDirection);
				const phys::Force1D suspensionDampingForce = wheelInfo.suspensionDamping * relativeVelocityAlongSuspensionAxis;
				const phys::Force3D suspensionForce = suspensionSpringForce * shapecastResult->normal - suspensionDampingForce * suspensionDirection;

				const phys::LinearVelocity1D tireVelocity = wheel.rotationalVelocity * radius;
				const phys::LinearVelocity1D sideVelocity = dot(relativeVelocity, sideDirection);
				const phys::LinearVelocity1D forwardVelocity = dot(relativeVelocity, forwardDirection);
				const phys::Speed forwardSpeed = abs(forwardVelocity);
				const bool hasForwardSpeed = forwardSpeed > phys::Speed::MACHINE_EPSILON;
				const phys::Angle slipYaw = (sideVelocity == 0 && !hasForwardSpeed) ? phys::Angle{} : atan2(sideVelocity, forwardSpeed);
				const phys::Angle slipAngle = -((slipYaw >= phys::PI) ? phys::Angle{slipYaw - phys::PI * 2_x} : slipYaw);
				const phys::LinearVelocity1D slipVelocity = forwardVelocity - tireVelocity;
				const phys::Force1D maxForwardForce = abs(slipVelocity) * (type->wheels.rotationalInertia / (deltaTime * radius * radius));
				const phys::Coefficient slipRatio = (hasForwardSpeed) ? -slipVelocity / forwardSpeed : copysign(1_x, tireVelocity);
				const phys::Force1D loadForce = max(dot(suspensionForce, shapecastResult->normal), phys::Force1D{});
				const VehicleSettings::Type::Tires::Friction& sideFriction = (wheelInfo.turns) ? type->tires.turningLateralFriction : type->tires.nonTurningLateralFriction;
				const VehicleSettings::Type::Tires::Friction& forwardFriction =
					(wheelInfo.turns) ? type->tires.turningLongitudalFriction : type->tires.nonTurningLongitudalFriction;
				const phys::Force1D sideForce = sideFriction.getContactForce(slipAngle, loadForce);
				const phys::Force1D forwardForce = clampLength(forwardFriction.getContactForce(slipRatio, loadForce), maxForwardForce);
				const phys::Coefficient groundFriction =
					clamp(unlerp(materialEntities.getComponent<phys::Material>(shapecastResult->objectID).kineticFriction, 0_x, 0.707_x), 0_x, 1_x);
				const phys::Force1D sideForceLimit = loadForce * sideFriction.peak * groundFriction;
				const phys::Force1D forwardForceLimit = loadForce * forwardFriction.peak * groundFriction;
				const phys::Scale1D normalizedSideForce = tryDivide(sideForce, sideForceLimit).value_or(0_x);
				const phys::Scale1D normalizedForwardForce = tryDivide(forwardForce, forwardForceLimit).value_or(0_x);
				const phys::Force2D contactForce = phys::Force2D{sideForceLimit, forwardForceLimit} * clampLength(phys::Scale2D{normalizedSideForce, normalizedForwardForce}, 1_x);
				const phys::Force3D totalForce = suspensionForce + contactForce.getX() * sideDirection + contactForce.getY() * forwardDirection;
				const phys::LinearImpulse3D linearImpulse = totalForce * deltaTime;
				if (linearImpulse != 0) {
					phys::applyCorrectionImpulse(activity, linearVelocity, angularVelocity, inverseMass, inverseMomentOfInertiaTensor, linearImpulse, offsetA);
					phys::applyCorrectionImpulse(activityB, linearVelocityB, angularVelocityB, inverseMassB, inverseMomentOfInertiaTensorB, -linearImpulse, offsetB);
				}

				wheel.rotationalVelocity -= contactForce.getY() * radius * deltaTime / type->wheels.rotationalInertia;
			} else {
				wheel.suspensionOffset = expDecay(wheel.suspensionOffset, 5_Hertz, deltaTime);
			}

			const phys::Coefficient brakeAmount = (wheelInfo.turns) ? brake : max(brake * (1_x - type->brakes.balance) / type->brakes.balance, handbrake);
			const phys::Torque2D brakeTorque = copysign(brakeAmount * type->brakes.torque, wheel.rotationalVelocity);
			wheel.rotationalVelocity -= clampLength(brakeTorque * deltaTime / type->wheels.rotationalInertia, abs(wheel.rotationalVelocity));

			const phys::Angle deltaAngle = wheel.rotationalVelocity * deltaTime;
			const phys::AbsoluteAngle newAngle = wrap(wheel.rotationAngle + deltaAngle, phys::AbsoluteAngle{360_degrees});
			wheel.rotationAngle = newAngle;
			wheel.previousRotationAngle = newAngle - deltaAngle;

			drivetrain.rotationalVelocity += driveContribution * wheel.rotationalVelocity * type->differential.ratio;
		}
	}
}

void moveVehicleOccupants(exec::Entities<phys::Position3D, phys::Orientation3D, phys::LinearVelocity3D, phys::LinearAcceleration3D, const VehicleOccupant> occupantEntities,
	exec::Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::LinearVelocity3D, const VehicleSeats, const VehicleType> vehicleEntities) {
	for (auto&& [entityID, position, orientation, linearVelocity, gravityAcceleration, occupant] : occupantEntities) {
		if (vehicleEntities.containsEntity(occupant.vehicleEntityID)) {
			const auto& [vehicleID, vehiclePosition, vehicleOrientation, vehicleScale, vehicleLinearVelocity, seats, type] = vehicleEntities[occupant.vehicleEntityID];
			if (const auto it = findBy<&VehicleSeats::Occupant::entityID>(seats.occupants, entityID)) {
				const size_t seatIndex = static_cast<size_t>(it - seats.occupants.begin());
				position = vehiclePosition + vehicleOrientation(vehicleScale * type->seats[seatIndex].offset);
				orientation = vehicleOrientation;
				linearVelocity = vehicleLinearVelocity;
				gravityAcceleration = {};
			}
		}
	}
}

void poseVehicles(exec::Entities<ObjectModelInstance, const phys::Orientation3D, const ObjectPreviousOrientation, const phys::Scale3D, const ObjectPreviousScale,
					  const VehicleWheels, const VehicleType>
					  vehicleEntities,
	const VehicleInfos& infos, const app::FrameInfo& frameInfo) {
	for (auto&& [entityID, modelInstance, orientation, previousOrientation, scale, previousScale, wheels, type] : vehicleEntities) {
		const phys::Orientation3D inverseOrientation = inverse(mix(previousOrientation, orientation, frameInfo.tickInterpolationAlpha));
		const phys::Scale3D inverseScale = inverse(mix(previousScale, scale, frameInfo.tickInterpolationAlpha));

		const VehicleInfo& info = infos.at(type);
		const size_t wheelCount = info.wheels.size();
		for (size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex) {
			const VehicleWheels::Wheel& wheel = wheels.wheels[wheelIndex];
			const VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];
			const phys::Length3D suspensionOffset = mix(wheel.previousSuspensionOffset, wheel.suspensionOffset, frameInfo.tickInterpolationAlpha);
			const phys::Orientation3D suspensionOrientation = mix(wheel.previousSuspensionOrientation, wheel.suspensionOrientation, frameInfo.tickInterpolationAlpha);
			const phys::AbsoluteAngle rotationAngle = mix(wheel.previousRotationAngle, wheel.rotationAngle, frameInfo.tickInterpolationAlpha);
			res::Model::Joint& localJoint = modelInstance.pose.localJoints[wheelInfo.jointIndex];
			localJoint = info.model.getBindPose().localJoints[wheelInfo.jointIndex];
			localJoint.translation += vec3{inverseScale * inverseOrientation(suspensionOffset).in(phys::METERS)};
			localJoint.rotation = inverseOrientation * suspensionOrientation * phys::Orientation3D::pitch(0 - rotationAngle) * localJoint.rotation;
		}
	}
}

struct VehicleSystem final : System {
	void addRequiredResources(phys::ResourceRegistry3D& resources, Graphics& graphics, Audio&, const Filesystem& filesystem) override {
		VehicleSettingsJSON settingsJSON{};
		try {
			json::deserializeFromString(filesystem.readInputFileString(VehicleSettingsJSON::FILEPATH), settingsJSON);
		} catch (...) {
			Error::throwWithNestedFilepath(VehicleSettingsJSON::FILEPATH);
		}
		const VehicleSettings& settings = resources.addResource<VehicleSettings>(std::move(settingsJSON));
		VehicleInfos& infos = resources.addResource<VehicleInfos>();
		for (const auto& [name, type] : settings.types) {
			res::Model model{filesystem, type.model.filepath};
			const res::Model::Transformation transformation = buildBindPoseTransformation(model);
			VehicleInfo& info = infos[&type];
			for (const auto& [jointName, jointIndex] : model.jointMap) {
				if (jointName.starts_with("Wheel")) {
					const mat4& jointMatrix = transformation.jointMatrices[jointIndex];
					const float scale = sqrt(maxComponent(vec3{
						length2(vec3{jointMatrix[0]}),
						length2(vec3{jointMatrix[1]}),
						length2(vec3{jointMatrix[2]}),
					}));
					const vec3 translation{jointMatrix[3]};

					phys::Distance radius{};
					phys::Distance halfWidth{};
					for (const res::Model::Instance& instance : model.instances) {
						if (instance.jointIndex == jointIndex) {
							const res::Model::Mesh& mesh = model.meshes[instance.meshIndex];
							const Length<3, float> extents = mesh.boundingBox.max - mesh.boundingBox.min;
							radius = maxComponent(extents) * 0.5f * scale * phys::METERS;
							halfWidth = minComponent(extents) * 0.5f * scale * phys::METERS;
							break;
						}
					}

					info.wheels.push_back(VehicleInfo::Wheel{
						.localOffset = translation * phys::METERS,
						.radius = radius,
						.halfWidth = halfWidth,
						.jointIndex = jointIndex,
						.suspensionDamping{},
						.turns = false,
						.drives = false,
					});
				}
			}
			sortByAscending<&VehicleInfo::Wheel::jointIndex>(info.wheels);
			for (VehicleInfo::Wheel& wheelInfo : info.wheels) {
				wheelInfo.suspensionDamping = type.suspension.dampingRatio * 2.0f * sqrt(type.suspension.stiffness * type.body.mass / static_cast<float>(info.wheels.size()));
			}
			for (const size_t wheelIndex : type.turning.wheels) {
				if (wheelIndex < info.wheels.size()) {
					VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];
					wheelInfo.suspensionDamping *= type.suspension.turningWheelMultipliers.dampingRatio;
					wheelInfo.turns = true;
				}
			}
			for (const size_t wheelIndex : type.drive.wheels) {
				if (wheelIndex < info.wheels.size()) {
					VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];
					wheelInfo.drives = true;
				}
			}

			phys::Length3D nonTurningWheelOffsetSum{};
			phys::Coefficient nonTurningWheelCount{};
			for (const VehicleInfo::Wheel& wheelInfo : info.wheels) {
				if (!wheelInfo.turns) {
					nonTurningWheelOffsetSum += wheelInfo.localOffset;
					nonTurningWheelCount += 1_x;
				}
			}
			if (nonTurningWheelCount > 0) {
				info.averageNonTurningWheelOffset = nonTurningWheelOffsetSum / nonTurningWheelCount;
			}
			info.maxTurnAngle = atan2(abs(info.averageNonTurningWheelOffset.getZ()), type.turning.radius);

			info.shape = buildModelShape(model, transformation,
				[&](res::Model::JointIndex jointIndex) -> bool { return !containsBy<&VehicleInfo::Wheel::jointIndex>(info.wheels, jointIndex); });
			info.model = gfx::Model3D{graphics.device, graphics.renderer3D, std::move(model)};
		}
		resources.addResource<VehicleExpiredEntities>();
	}

	void removeResources(phys::ResourceRegistry3D& resources) noexcept override {
		resources.removeResource<VehicleSettings>();
		resources.removeResource<VehicleInfos>();
		resources.removeResource<VehicleExpiredEntities>();
	}

	void scheduleTick(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount) override {
		scheduler.addTask<exitDestroyedVehicles>("Exit destroyed vehicles");
		scheduler.addTask<resetVacantVehicleSeats>("Reset vacant vehicle seats");
		scheduler.addTask<controlVehicles>("Control vehicles");
		scheduler.addTask<simulateVehicleEngines>("Simulate vehicle engines");
		scheduler.addTask<simulateVehicleWheels>("Simulate vehicle wheels");
		scheduler.addTask<moveVehicleOccupants>("Move vehicle occupants");
	}

	void schedulePose(phys::Scheduler3D& scheduler, const phys::ResourceRegistry3D&, exec::Task::ParallelCount) override {
		scheduler.addTask<poseVehicles>("Pose vehicles");
	}

	void putGraphics(Graphics& graphics, const phys::Simulation3D& simulation) override {
		GREM_PROFILE_FUNCTION();

		const WorldView& worldView = simulation.resources.getResource<WorldView>();
		const app::FrameInfo& frameInfo = simulation.resources.getResource<app::FrameInfo>();

		const PlayerInput& input = simulation.registry.getComponent<PlayerInput>(worldView.playerEntityID);

		const auto drawPedalMeter = [&](Offset2D position, phys::Coefficient value, StringView label, PlayerAction action) -> void {
			constexpr float TEXT_SPACING = 8.0f;
			constexpr float METER_WIDTH = 8.0f;
			constexpr float METER_HEIGHT = 40.0f;
			constexpr float PEDAL_WIDTH = 16.0f;
			constexpr float PEDAL_HEIGHT = 4.0f;
			graphics.put2DText(Offset2D{position.x, position.y + static_cast<int32_t>(0.5f * METER_HEIGHT + TEXT_SPACING)}, Color::WHITE, label, 1.0f,
				gfx::TextAlign::CENTER_HORIZONTALLY_LAST_LINE_BASE);
			graphics.instances2D.putRectangleInstance({
				.position{static_cast<float>(position.x), static_cast<float>(position.y) - 0.5f * METER_HEIGHT},
				.size{METER_WIDTH, METER_HEIGHT},
				.origin{0.5f, 0.0f},
				.color = Color::GRAY,
			});
			graphics.instances2D.putRectangleInstance({
				.position{static_cast<float>(position.x), static_cast<float>(position.y) - 0.5f * METER_HEIGHT},
				.size{METER_WIDTH, METER_HEIGHT * value},
				.origin{0.5f, 0.0f},
				.color = Color::WHITE,
			});
			graphics.instances2D.putRectangleInstance({
				.position{static_cast<float>(position.x), static_cast<float>(position.y) - 0.5f * METER_HEIGHT + METER_HEIGHT * value},
				.size{PEDAL_WIDTH, PEDAL_HEIGHT},
				.origin{0.5f, 0.5f},
				.color = Color::BLACK,
			});
			if (const Span<const evt::Input> boundInputs = input.getBoundInputs(action); !boundInputs.empty()) {
				const float inputValue = input.getCurrentState(action).value;
				graphics.put2DText(Offset2D{position.x, position.y - static_cast<int32_t>(0.5f * METER_HEIGHT + TEXT_SPACING)}, mix(Color::GRAY, Color::WHITE, inputValue),
					formatString("[{}]", evt::getInputString(boundInputs.front())), 1.0f, gfx::TextAlign::CENTER_HORIZONTALLY_FIRST_LINE_TOP);
			}
		};

		const auto drawAnalogInstrumentMeter =
			[&](Offset2D position, float currentValue, StringView label, float minValue, float maxValue, float valueIncrement, float redlineValue) -> void {
			constexpr float RADIUS = 70.0f;
			constexpr float MIN_ANGLE = 180_degrees + 20_degrees;
			constexpr float MAX_ANGLE = -20_degrees;
			constexpr float TICK_WIDTH = 3.0f;
			constexpr float TICK_LENGTH = 15.0f;
			constexpr float HALFWAY_TICK_WIDTH = 2.0f;
			constexpr float HALFWAY_TICK_LENGTH = 10.0f;
			constexpr float CENTER_CIRCLE_RADIUS = 8.0f;
			constexpr float NEEDLE_BASE_WIDTH = 4.0f;
			constexpr float NEEDLE_TIP_WIDTH = 1.0f;
			const vec2 center{position};
			for (float value = minValue; value <= maxValue; value += valueIncrement) {
				const float halfwayValue = value - valueIncrement * 0.5f;
				if (halfwayValue > minValue) {
					const float halfwayAngle = lerp(MIN_ANGLE, MAX_ANGLE, unlerp(halfwayValue, minValue, maxValue));
					graphics.instances2D.putRectangleInstance({
						.position = center + (RADIUS + TICK_LENGTH - HALFWAY_TICK_LENGTH) * angledVector(halfwayAngle),
						.angle = halfwayAngle,
						.size{HALFWAY_TICK_LENGTH, HALFWAY_TICK_WIDTH},
						.origin{0.0f, 0.5f},
						.color = (halfwayValue >= redlineValue) ? Color::RED : Color::WHITE,
					});
				}
				const float angle = lerp(MIN_ANGLE, MAX_ANGLE, unlerp(value, minValue, maxValue));
				const vec2 direction = angledVector(angle);
				graphics.instances2D.putRectangleInstance({
					.position = center + RADIUS * direction,
					.angle = angle,
					.size{TICK_LENGTH, TICK_WIDTH},
					.origin{0.0f, 0.5f},
					.color = (value >= redlineValue) ? Color::RED : Color::WHITE,
				});
				graphics.instances2D.putTextStringInstance(graphics.mainFont, formatString("{}", value),
					{
						.characterSize = graphics.options.fontSize,
						.position = round(center + (RADIUS - 14.0f) * direction),
						.alignment = gfx::TextAlign::CENTER,
						.color = Color::WHITE,
					});
			}
			graphics.put2DText(position + Offset2D{0, static_cast<int32_t>(CENTER_CIRCLE_RADIUS * 2.0f)}, Color::WHITE, label, 1.0f,
				gfx::TextAlign::CENTER_HORIZONTALLY_LAST_LINE_BASE);
			graphics.instances2D.putRectangleInstance({
				.texture = &graphics.circleTexture,
				.position = position,
				.size{CENTER_CIRCLE_RADIUS * 2.0f},
				.origin{0.5f, 0.5f},
				.color = Color::BLACK,
			});
			const float currentAngle = lerp(MIN_ANGLE, MAX_ANGLE, unlerp(currentValue, minValue, maxValue));
			const vec2 currentDirection = angledVector(currentAngle);
			const vec2 needleBaseCenter = center + CENTER_CIRCLE_RADIUS * currentDirection;
			const vec2 needleTipCenter = center + (RADIUS + TICK_LENGTH) * currentDirection;
			const vec2 needleBaseOffset = NEEDLE_BASE_WIDTH * 0.5f * rotate90DegreesClockwise(currentDirection);
			const vec2 needleTipOffset = NEEDLE_TIP_WIDTH * 0.5f * rotate90DegreesClockwise(currentDirection);
			graphics.instances2D.putTriangleInstance({
				.pointA = needleBaseCenter - needleBaseOffset,
				.pointB = needleBaseCenter + needleBaseOffset,
				.pointC = needleTipCenter - needleTipOffset,
				.color = Color::RED,
			});
			graphics.instances2D.putTriangleInstance({
				.pointA = needleBaseCenter + needleBaseOffset,
				.pointB = needleTipCenter + needleTipOffset,
				.pointC = needleTipCenter - needleTipOffset,
				.color = Color::RED,
			});
		};

		const auto drawGearShifter = [&](Offset2D position, float value, StringView label) -> void {
			constexpr float TEXT_SPACING = 14.0f;
			constexpr float BASE_WIDTH = 8.0f;
			constexpr float BASE_HEIGHT = 64.0f;
			constexpr float STICK_RADIUS = 10.0f;
			graphics.put2DText(Offset2D{position.x, position.y + static_cast<int32_t>(0.5f * BASE_HEIGHT + TEXT_SPACING)}, Color::WHITE, "Gear Shifter", 1.0f,
				gfx::TextAlign::CENTER_HORIZONTALLY_LAST_LINE_BASE);
			graphics.instances2D.putRectangleInstance({
				.position{position},
				.size{BASE_WIDTH, BASE_HEIGHT},
				.origin{0.5f, 0.5f},
				.color = Color::GRAY,
			});
			graphics.instances2D.putRectangleInstance({
				.texture = &graphics.circleTexture,
				.position{static_cast<float>(position.x), static_cast<float>(position.y) + 0.5f * BASE_HEIGHT * value},
				.size{STICK_RADIUS * 2.0f},
				.origin{0.5f, 0.5f},
				.color = Color::BLACK,
			});
			graphics.instances2D.putTextStringInstance(graphics.mainFont, label,
				{
					.characterSize = graphics.options.fontSize,
					.position = Offset2D{position.x, position.y + static_cast<int32_t>(0.5f * BASE_HEIGHT * value)},
					.alignment = gfx::TextAlign::CENTER,
					.color = Color::WHITE,
				});
			if (const Span<const evt::Input> boundInputs = input.getBoundInputs(PlayerAction::GEAR_SHIFT_WITH_AIM); !boundInputs.empty()) {
				const float inputValue = input.getCurrentState(PlayerAction::GEAR_SHIFT_WITH_AIM).value;
				graphics.put2DText(Offset2D{position.x, position.y - static_cast<int32_t>(0.5f * BASE_HEIGHT + TEXT_SPACING)}, mix(Color::GRAY, Color::WHITE, inputValue),
					formatString("[{}]", evt::getInputString(boundInputs.front())), 1.0f, gfx::TextAlign::CENTER_HORIZONTALLY_FIRST_LINE_TOP);
			}
		};

		const auto drawSteeringWheel = [&](Offset2D position, float steering) -> void {
			constexpr float TEXT_SPACING = 8.0f;
			constexpr float RADIUS_HORIZONTAL = 256.0f;
			constexpr float RADIUS_VERTICAL = 128.0f;
			constexpr float MARK_WIDTH = 16.0f;
			constexpr float HANDLE_WIDTH = 64.0f;
			constexpr float HANDLE_HEIGHT = 54.0f;
			constexpr float MIN_ANGLE = 180_degrees;
			constexpr float MAX_ANGLE = 0_degrees;
			const float angle = lerp(MIN_ANGLE, MAX_ANGLE, unlerp(steering, -1.0f, 1.0f));
			const vec2 rectanglePosition = vec2{position} + vec2{RADIUS_HORIZONTAL, RADIUS_VERTICAL} * angledVector(angle);
			graphics.instances2D.putRectangleInstance({
				.position = rectanglePosition,
				.angle = angle,
				.size{HANDLE_HEIGHT, HANDLE_WIDTH},
				.origin{0.0f, 0.5f},
				.color = Color::BLACK * 0.5f,
			});
			graphics.instances2D.putRectangleInstance({
				.position = rectanglePosition,
				.angle = angle,
				.size{HANDLE_HEIGHT, MARK_WIDTH},
				.origin{0.0f, 0.5f},
				.color = Color::YELLOW * 0.5f,
			});
			if (const Span<const evt::Input> boundInputs = input.getBoundInputs(PlayerAction::STEER_WITH_AIM); !boundInputs.empty()) {
				const float inputValue = input.getCurrentState(PlayerAction::STEER_WITH_AIM).value;
				graphics.put2DText(Offset2D{position.x, position.y + static_cast<int32_t>(RADIUS_VERTICAL - TEXT_SPACING)}, mix(Color::GRAY, Color::WHITE, inputValue),
					formatString("[{}]", evt::getInputString(boundInputs.front())), 1.0f, gfx::TextAlign::CENTER_HORIZONTALLY_FIRST_LINE_TOP);
			}
			if (const Span<const evt::Input> boundInputs = input.getBoundInputs(PlayerAction::STEER_LEFT_SMOOTH); !boundInputs.empty()) {
				const float inputValue = input.getCurrentState(PlayerAction::STEER_LEFT_SMOOTH).value;
				graphics.put2DText(
					Offset2D{position.x - static_cast<int32_t>(RADIUS_HORIZONTAL + HANDLE_HEIGHT * 0.5f), position.y - static_cast<int32_t>(HANDLE_WIDTH * 0.5f + TEXT_SPACING)},
					mix(Color::GRAY, Color::WHITE, inputValue), formatString("[{}]", evt::getInputString(boundInputs.front())), 1.0f,
					gfx::TextAlign::CENTER_HORIZONTALLY_FIRST_LINE_TOP);
			}
			if (const Span<const evt::Input> boundInputs = input.getBoundInputs(PlayerAction::STEER_RIGHT_SMOOTH); !boundInputs.empty()) {
				const float inputValue = input.getCurrentState(PlayerAction::STEER_RIGHT_SMOOTH).value;
				graphics.put2DText(
					Offset2D{position.x + static_cast<int32_t>(RADIUS_HORIZONTAL + HANDLE_HEIGHT * 0.5f), position.y - static_cast<int32_t>(HANDLE_WIDTH * 0.5f + TEXT_SPACING)},
					mix(Color::GRAY, Color::WHITE, inputValue), formatString("[{}]", evt::getInputString(boundInputs.front())), 1.0f,
					gfx::TextAlign::CENTER_HORIZONTALLY_FIRST_LINE_TOP);
			}
		};

		const Offset2D screenSize = Offset2D{} + graphics.viewport.region.size;
		const Offset2D screenCenter = graphics.viewport.region.offset + screenSize / 2;
		const int32_t y = screenSize.y / 8;

		if (const VehicleOccupant* const occupant = simulation.registry.findComponent<VehicleOccupant>(worldView.playerEntityID)) {
			if (simulation.registry.hasComponent<VehicleDriverTag>(worldView.playerEntityID)) {
				const VehicleControls* const controls = simulation.registry.findComponent<VehicleControls>(occupant->vehicleEntityID);
				const VehicleEngine* const engine = simulation.registry.findComponent<VehicleEngine>(occupant->vehicleEntityID);
				const VehicleDrivetrain* const drivetrain = simulation.registry.findComponent<VehicleDrivetrain>(occupant->vehicleEntityID);
				const VehicleWheels* const wheels = simulation.registry.findComponent<VehicleWheels>(occupant->vehicleEntityID);
				const VehicleType* const type = simulation.registry.findComponent<VehicleType>(occupant->vehicleEntityID);
				const phys::Scale3D* const scale = simulation.registry.findComponent<phys::Scale3D>(occupant->vehicleEntityID);
				if (controls && engine && drivetrain && wheels && type && scale) {
					const VehicleInfos& infos = simulation.resources.getResource<VehicleInfos>();
					const VehicleInfo& info = infos.at(*type);

					const size_t wheelCount = info.wheels.size();
					phys::Speed drivenWheelSpeedSum{};
					phys::Coefficient drivenWheelCount{};
					for (size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex) {
						const VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];
						if (wheelInfo.drives) {
							const VehicleWheels::Wheel& wheel = wheels->wheels[wheelIndex];
							const phys::Distance radius = wheelInfo.radius * scale->getY();

							drivenWheelSpeedSum += radius * abs(mix(wheel.previousRotationalVelocity, wheel.rotationalVelocity, frameInfo.tickInterpolationAlpha));
							drivenWheelCount += 1_x;
						}
					}
					const phys::Speed speed = drivenWheelSpeedSum / drivenWheelCount;
					const phys::Coefficient clutch = clamp(controls->clutchDirect + controls->clutchSmooth, 0_x, 1_x);
					const phys::Coefficient brake = clamp(controls->brakeDirect + controls->brakeSmooth, 0_x, 1_x);
					const phys::Coefficient gas = clamp(controls->gasDirect + controls->gasSmooth, 0_x, 1_x);
					const float rpm = mix(engine->previousRotationalSpeed, engine->rotationalSpeed, frameInfo.tickInterpolationAlpha).in(phys::REVOLUTIONS_PER_MINUTE);
					const float kmph = speed.in(phys::KILOMETERS_PER_HOUR);
					const String gearString = (drivetrain->gear < 0) ? "R" : ((drivetrain->gear == 0) ? "N" : formatString("{}", drivetrain->gear));
					const float driveGearCount = ((*type)->transmission.automatic) ? 1.0f : static_cast<float>((*type)->transmission.gearRatios.size());
					const String gearStickLabel =
						(controls->gearStickPosition < 0)
					        ? "R"
					        : ((controls->gearStickPosition == 0)     ? "N"
					              : ((*type)->transmission.automatic) ? "D"
					                                                  : formatString("{}", controls->gearStickPosition));
					const float gearStickPosition =
						clamp(static_cast<float>(controls->shiftingFromAimPreviousGearStickPosition) + clamp(static_cast<float>(controls->shiftingFromAim), -1.0f, 1.0f), -1.0f,
							driveGearCount);
					const phys::Scale1D steering = clamp(controls->steeringDirect + controls->steeringSmooth + controls->steeringFromAim, -1_x, 1_x);

					if (const Span<const evt::Input> ignitionInputs = input.getBoundInputs(PlayerAction::IGNITION); !ignitionInputs.empty() && !engine->running) {
						const float ignitionInputValue = input.getCurrentState(PlayerAction::IGNITION).value;
						graphics.put2DText(screenCenter, mix(Color::GRAY, Color::WHITE, ignitionInputValue),
							formatString("[{}] Start Engine", evt::getInputString(ignitionInputs.front())), 2.0f, gfx::TextAlign::CENTER);
					}

					drawPedalMeter(Offset2D{96, y + 16}, clutch, "Clutch", PlayerAction::CLUTCH_SMOOTH);
					drawPedalMeter(Offset2D{168, y + 16}, brake, "Brake", PlayerAction::BRAKE_SMOOTH);
					drawPedalMeter(Offset2D{240, y + 16}, gas, "Gas", PlayerAction::GAS_SMOOTH);

					if (const Span<const evt::Input> handbrakeInputs = input.getBoundInputs(PlayerAction::HANDBRAKE); !handbrakeInputs.empty()) {
						const phys::Coefficient handbrake = clamp(controls->handbrake, 0_x, 1_x);
						graphics.put2DText(Offset2D{168, y - 40}, mix(Color::GRAY, Color::WHITE, handbrake),
							formatString("[{}] Handbrake", evt::getInputString(handbrakeInputs.front())), 1.0f, gfx::TextAlign::CENTER);
					}

					drawAnalogInstrumentMeter(Offset2D{screenCenter.x - 128, y}, rpm * 0.001f, "RPM\nx1000", 0.0f, 8.0f, 1.0f,
						((*type)->engine.redlineRPM.in(phys::REVOLUTIONS_PER_MINUTE) - 1.0f) * 0.001f);
					drawAnalogInstrumentMeter(Offset2D{screenCenter.x + 128, y}, kmph, "km/h", 0.0f, 140.0f, 20.0f, Limits<float>::MAX);

					graphics.instances2D.putTextStringInstance(graphics.mainFont, gearString,
						{
							.characterSize = graphics.options.fontSize,
							.position = Offset2D{screenCenter.x + 1, y + 4},
							.scale{4.0f},
							.alignment = gfx::TextAlign::CENTER_HORIZONTALLY_LAST_LINE_BASE,
							.color = Color::WHITE,
						});
					graphics.put2DText(Offset2D{screenCenter.x, y}, Color::WHITE, "Gear", 1.0f, gfx::TextAlign::CENTER_HORIZONTALLY_FIRST_LINE_TOP);

					drawGearShifter(Offset2D{screenSize.x - 168, y + 16}, (gearStickPosition > 0.0f) ? gearStickPosition / driveGearCount : gearStickPosition, gearStickLabel);

					drawSteeringWheel(Offset2D{screenCenter.x, y - 32}, steering);
				}
			}
		}
	}
} vehicleSystemImplementation{};

} // namespace

System* const vehicleSystem = &vehicleSystemImplementation;

void createVehicle(phys::EntityBuilder3D& entity, phys::ResourceRegistry3D& resources, phys::Position3D position, phys::Orientation3D orientation, phys::Scale3D scale,
	const String& typeName) {
	GREM_PROFILE_FUNCTION();

	const VehicleSettings& settings = resources.getResource<VehicleSettings>();
	const VehicleInfos& infos = resources.getResource<VehicleInfos>();
	const VehicleType type = &settings.types.at(typeName);
	const VehicleInfo& info = infos.at(type);
	createObject(entity, resources, info.model,
		phys::ObjectOptions3D{
			.position = position,
			.orientation = orientation,
			.scale = scale,
			.mass = type->body.mass,
			.collider{.shape = info.shape, .filter = VEHICLE_COLLISION_FILTER},
			.material = type->body.material,
			.enableResting = false,
		});
	entity.getComponent<ObjectModelInstance>().localTransformation =
		translateRotateScale(type->model.rootTranslation * phys::METERS, type->model.rootRotation, type->model.rootScale);
	entity.addComponent<VehicleTag>();
	entity.addComponent<VehicleType>(type);
	entity.addComponent<VehicleSeats>();
	entity.addComponent<VehicleEngine>();
	entity.addComponent<VehicleDrivetrain>();
	entity.addComponent<VehicleWheels>(VehicleWheels{.wheels{info.wheels.size(), VehicleWheels::Wheel{.suspensionOrientation = orientation}}});
	entity.addComponent<VehicleControls>();
}

void enterVehicle(phys::EntityRegistry3D& registry, phys::EntityID occupantEntityID, phys::EntityID vehicleEntityID) {
	VehicleSeats* const seats = registry.findComponent<VehicleSeats>(vehicleEntityID);
	const VehicleType* const type = registry.findComponent<VehicleType>(vehicleEntityID);
	if (!seats || seats->occupants.size() >= (*type)->seats.size()) {
		return;
	}
	exitVehicle(registry, occupantEntityID);
	phys::Collider3D* const collider = registry.findComponent<phys::Collider3D>(occupantEntityID);
	ObjectModelInstance* const modelInstance = registry.findComponent<ObjectModelInstance>(occupantEntityID);
	registry.addComponent<VehicleOccupant>(occupantEntityID,
		VehicleOccupant{
			.vehicleEntityID = vehicleEntityID,
			.oldCollisionResponseLayers = (collider) ? collider->filter.responseLayers : phys::CollisionLayers{},
			.oldVisibility = (modelInstance) ? modelInstance->transformation.jointsVisible[0] : true,
		});
	if (collider) {
		collider->filter.responseLayers = {};
	}
	if (modelInstance) {
		modelInstance->transformation.jointsVisible[0] = false;
	}
	if (seats->occupants.empty()) {
		registry.addComponent<VehicleDriverTag>(occupantEntityID);
	}
	seats->occupants.push_back(VehicleSeats::Occupant{.entityID = occupantEntityID});
}

void exitVehicle(phys::EntityRegistry3D& registry, phys::EntityID occupantEntityID) {
	if (const VehicleOccupant* const occupant = registry.findComponent<VehicleOccupant>(occupantEntityID)) {
		if (phys::Collider3D* const collider = registry.findComponent<phys::Collider3D>(occupantEntityID)) {
			collider->filter.responseLayers = occupant->oldCollisionResponseLayers;
		}
		if (ObjectModelInstance* const modelInstance = registry.findComponent<ObjectModelInstance>(occupantEntityID)) {
			modelInstance->transformation.jointsVisible[0] = occupant->oldVisibility;
		}
		if (VehicleSeats* const seats = registry.findComponent<VehicleSeats>(occupant->vehicleEntityID)) {
			if (const auto it = findBy<&VehicleSeats::Occupant::entityID>(seats->occupants, occupantEntityID)) {
				const size_t seatIndex = static_cast<size_t>(it - seats->occupants.begin());
				phys::Position3D* const position = registry.findComponent<phys::Position3D>(occupantEntityID);
				const phys::Position3D* const vehiclePosition = registry.findComponent<phys::Position3D>(occupant->vehicleEntityID);
				const phys::Orientation3D* const vehicleOrientation = registry.findComponent<phys::Orientation3D>(occupant->vehicleEntityID);
				const phys::Scale3D* const vehicleScale = registry.findComponent<phys::Scale3D>(occupant->vehicleEntityID);
				const VehicleType* const vehicleType = registry.findComponent<VehicleType>(occupant->vehicleEntityID);
				if (position && vehiclePosition && vehicleOrientation && vehicleScale && vehicleType) {
					const phys::Length3D desiredExitOffset = (*vehicleOrientation)(*vehicleScale * (*vehicleType)->seats[seatIndex].exitOffset);
					const phys::Length3D clampedExitOffset{desiredExitOffset.getX(), max(desiredExitOffset.getY(), phys::Length1D{}), desiredExitOffset.getZ()};
					const Optional<phys::Direction3D> clampedExitOffsetDirection = tryNormalize(clampedExitOffset);
					const phys::Length3D exitOffset = (clampedExitOffsetDirection) ? length(desiredExitOffset) * *clampedExitOffsetDirection : clampedExitOffset;
					*position = *vehiclePosition + exitOffset;
				}
				seats->occupants.erase(it);
			}
		}
		if (registry.hasComponent<VehicleDriverTag>(occupantEntityID)) {
			if (VehicleControls* const controls = registry.findComponent<VehicleControls>(occupant->vehicleEntityID)) {
				*controls = {};
			}
		}
		registry.removeComponent<VehicleOccupant>(occupantEntityID);
		registry.removeComponent<VehicleDriverTag>(occupantEntityID);
	}
}
