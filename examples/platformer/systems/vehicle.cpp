// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "vehicle.hpp"

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "../Audio.hpp"
#include "../Graphics.hpp"
#include "../System.hpp"
#include "../WorldView.hpp"
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
			phys::Material material{};
			phys::Length3D downforceOffset{};
			phys::Area downforceEffectiveArea = 0.5_square_meters;
		} body{};

		struct Engine {
			phys::Coefficient flatness = 12_x;               // Typical range: [10, 18]
			phys::Coefficient falloffCloseness = 0.05_x;     // Typical range: [0.01, 1]
			phys::Coefficient falloffSharpness = 13_x;       // Typical range: [10, 50]
			phys::Coefficient mechanicalEfficiency = 0.82_x; // Typical range: [0.5, 0.99]
			phys::Quantity<1, Horsepower> horsepower = 170.0f * HORSEPOWER;
			phys::Quantity<1, phys::RevolutionsPerMinute> stallRPM = 100_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> idleRPM = 750_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> idealRPM = 3500_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> redlineRPM = 6000_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> automaticCruiseShiftUpThresholdRPM = 2000_revolutions_per_minute;
			phys::Quantity<1, phys::RevolutionsPerMinute> automaticCruiseShiftDownThresholdRPM = min(idleRPM * 1.5_x, automaticCruiseShiftUpThresholdRPM * 0.5_x);
			phys::Quantity<1, phys::RevolutionsPerMinute> automaticGasShiftUpThresholdRPM = midpoint(idealRPM, redlineRPM);
			phys::Quantity<1, phys::RevolutionsPerMinute> automaticGasShiftDownThresholdRPM = min(idealRPM * 0.8_x, automaticGasShiftUpThresholdRPM * 0.8_x);
			phys::Quantity<1, phys::RevolutionsPerMinute> starterMotorRPM = midpoint(stallRPM, idleRPM);
			phys::Time automaticShiftUpTime = 0.25_seconds;
			phys::Time automaticShiftDownTime = 0.5_seconds;
			phys::MomentOfInertia2D momentOfInertia = 0.1_kilogram_square_meters;
			phys::AngularViscousDamping synchronizationDamping = 3_Newton_meter_seconds_per_radian;
			phys::Time startupTime = 1_second;

			[[nodiscard]] phys::Torque2D getAvailableTorque(phys::AngularSpeed angularSpeed) const {
				const phys::Coefficient torqueCurve = exp(-length2((angularSpeed - idealRPM.as(phys::AngularSpeed::UNIT)) / (length2(flatness) * phys::Frequency::UNIT)));
				const phys::Coefficient falloffCurve =
					1_x - falloffCloseness * exp(falloffSharpness * (angularSpeed - redlineRPM.as(phys::AngularSpeed::UNIT)) / redlineRPM.as(phys::AngularSpeed::UNIT));
				const phys::Torque2D torqueRangeBegin = mechanicalEfficiency * horsepower.as(phys::Power::UNIT) / redlineRPM.as(phys::AngularSpeed::UNIT);
				const phys::Torque2D torqueRangeEnd = horsepower.as(phys::Power::UNIT) / idealRPM.as(phys::AngularSpeed::UNIT);
				return lerp(torqueRangeBegin, torqueRangeEnd, torqueCurve) * falloffCurve;
			}
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
			phys::Coefficient locking = 0.25_x;
			phys::Coefficient gearRatio = 3.9_x;
			phys::Quantity<1, phys::RevolutionsPerMinute> maxLockedSpeedDifference = 60_revolutions_per_minute;
		} differential{};

		struct Turning {
			ArrayList<size_t> wheels{0, 1};
			phys::Distance radius = 5_meters;
		} turning{};

		struct Brakes {
			phys::Coefficient deadzone = 0.1_x;
			phys::Coefficient balance = 0.65_x;
			phys::Torque2D torque = 3800_Newton_meters;
		} brakes{};

		struct Wheels {
			phys::MomentOfInertia2D momentOfInertia = 0.6_kilogram_square_meters;
			phys::AngularViscousDamping damping = 0.05_Newton_meter_seconds_per_radian;
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

			Friction turningLongitudalFriction{.shape = 1.9_x, .peak = 1.7_x};
			Friction turningLateralFriction{.shape = 1.6_x, .peak = 1.6_x};
			phys::Coefficient turningRollingResistance = 0.005_x;
			Friction nonTurningLongitudalFriction{.shape = 1.9_x, .peak = 1.7_x};
			Friction nonTurningLateralFriction{.shape = 1.6_x, .peak = 1.6_x};
			phys::Coefficient nonTurningRollingResistance = 0.005_x;
		} tires{};

		struct Suspension {
			struct Springs {
				phys::Quantity<1, phys::NewtonPerMillimeter> stiffness = 75_Newton_per_millimeter;
				phys::Coefficient dampingRatio = 0.5_x;
				phys::Distance maxOffset = 0.25_meters;

				struct TurningWheelMultipliers {
					phys::Coefficient stiffness = 1.4_x;
					phys::Coefficient dampingRatio = 1.0_x;
					phys::Coefficient maxOffset = 1.0_x;
				} turningWheelMultipliers{};
			} springs{};

			struct AntiRollBar {
				phys::Quantity<1, phys::NewtonPerMillimeter> stiffness = 65_Newton_per_millimeter;

				struct TurningWheelMultipliers {
					phys::Coefficient stiffness = 1.35_x;
				} turningWheelMultipliers{};
			} antiRollBar{};
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
		phys::LinearViscousDamping suspensionDamping;
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
	int32_t gear = 0; // -1: reverse, 0: neutral, 1+: drive
	phys::AngularSpeed angularSpeed{};
	phys::AngularSpeed previousAngularSpeed = angularSpeed;
	phys::Time startingTime{};
	phys::Time timeSpentBelowShiftDownThreshold{};
	phys::Time timeSpentAboveShiftUpThreshold{};
	bool running = false;
	bool starting = false;
};

struct VehicleWheels {
	struct Wheel {
		struct Contact {
			phys::EntityID objectID;
			phys::Length3D offsetA;
			phys::Length3D offsetB;
			phys::Direction3D normal;
			phys::LinearVelocity3D relativeVelocity;
			phys::Length1D suspensionOffset;
		};

		phys::Length3D suspensionOffset{};
		phys::Length3D previousSuspensionOffset = suspensionOffset;
		phys::Orientation3D suspensionOrientation{};
		phys::Orientation3D previousSuspensionOrientation = suspensionOrientation;
		phys::AbsoluteAngle angle{};
		phys::AbsoluteAngle previousAngle = angle;
		phys::AngularVelocity2D angularVelocity{};
		phys::AngularVelocity2D previousAngularVelocity = angularVelocity;
		Optional<Contact> contact{};
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

void simulateVehicles(exec::Entities<phys::Position3D, phys::Orientation3D, phys::Scale3D, phys::ObjectActivity, phys::LinearVelocity3D, phys::AngularVelocity3D, VehicleEngine,
						  VehicleWheels, const phys::InverseMass, const phys::InverseMomentOfInertiaTensor3D, const phys::FluidDensity, const VehicleControls, const VehicleType>
						  vehicleEntities,
	exec::Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D> objectEntities,
	exec::Entities<phys::ObjectActivity, phys::LinearVelocity3D, phys::AngularVelocity3D, const phys::InverseMass, const phys::InverseMomentOfInertiaTensor3D> kinematicsEntities,
	exec::Entities<const phys::Material> materialEntities, const VehicleInfos& infos, const phys::Broadphase3D& broadphase, const phys::SimulationOptions3D& simulationOptions) {
	const phys::Time deltaTime = simulationOptions.stepInterval;

	for (auto&& [entityID, position, orientation, scale, activity, linearVelocity, angularVelocity, engine, wheels, inverseMass, inverseMomentOfInertiaTensor,
			 surroundingFluidDensity, controls, type] : vehicleEntities) {
		const VehicleInfo& info = infos.at(type);
		const size_t wheelCount = info.wheels.size();
		GREM_ASSERT(wheels.wheels.size() == wheelCount);

		// Save previous state for interpolation.
		engine.previousAngularSpeed = engine.angularSpeed;
		for (size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex) {
			VehicleWheels::Wheel& wheel = wheels.wheels[wheelIndex];
			wheel.previousSuspensionOffset = wheel.suspensionOffset;
			wheel.previousSuspensionOrientation = wheel.suspensionOrientation;
			wheel.previousAngle = wheel.angle;
			wheel.previousAngularVelocity = wheel.angularVelocity;
		}

		// Apply downforce to vehicle body.
		const phys::OrthonormalBasis3D bodyBasis = rotate(orientation);
		const phys::LinearVelocity1D bodyForwardVelocity = max(dot(linearVelocity, -bodyBasis[phys::Z]), phys::LinearVelocity1D{});
		const phys::Force3D downforce = -0.5_x * surroundingFluidDensity * length2(bodyForwardVelocity) * type->body.downforceEffectiveArea * bodyBasis[phys::Y];
		phys::applyCorrectionImpulse(activity, linearVelocity, angularVelocity, inverseMass, inverseMomentOfInertiaTensor, downforce * deltaTime,
			orientation(scale * type->body.downforceOffset));

		// Count the total number of driven wheels and find the fastest/slowest ones.
		size_t fastestDrivenWheelIndex = wheelCount;
		size_t slowestDrivenWheelIndex = wheelCount;
		phys::Coefficient drivenWheelCount{};
		for (size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex) {
			const VehicleWheels::Wheel& wheel = wheels.wheels[wheelIndex];
			const VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];
			if (wheelInfo.drives) {
				if (fastestDrivenWheelIndex >= wheelCount || abs(wheel.angularVelocity) > abs(wheels.wheels[fastestDrivenWheelIndex].angularVelocity)) {
					fastestDrivenWheelIndex = wheelIndex;
				}
				if (slowestDrivenWheelIndex >= wheelCount || abs(wheel.angularVelocity) < abs(wheels.wheels[slowestDrivenWheelIndex].angularVelocity)) {
					slowestDrivenWheelIndex = wheelIndex;
				}
				drivenWheelCount += 1_x;
			}
		}
		const phys::AngularSpeed drivenWheelSpeedDifference =
			(fastestDrivenWheelIndex < wheelCount)
		        ? phys::AngularSpeed{abs(abs(wheels.wheels[fastestDrivenWheelIndex].angularVelocity) - abs(wheels.wheels[slowestDrivenWheelIndex].angularVelocity))}
		        : phys::AngularSpeed{};

		// Handle engine start/stop.
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
						engine.angularSpeed = max(engine.angularSpeed, type->engine.starterMotorRPM.as(phys::AngularSpeed::UNIT));
						if (countup(engine.startingTime, deltaTime, type->engine.startupTime)) {
							engine.running = true;
							engine.starting = false;
						}
					}
				}
				break;
		}

		// Get driver controls.
		const phys::Coefficient clutch = clamp(controls.clutchDirect + controls.clutchSmooth, 0_x, 1_x);
		const phys::Coefficient brake = clamp(controls.brakeDirect + controls.brakeSmooth, 0_x, 1_x);
		const phys::Coefficient gas = clamp(controls.gasDirect + controls.gasSmooth, 0_x, 1_x);
		const phys::Coefficient handbrake = clamp(controls.handbrake, 0_x, 1_x);
		const phys::Scale1D steering = clamp(controls.steeringDirect + controls.steeringSmooth + controls.steeringFromAim, -1_x, 1_x);

		// Shift gears.
		phys::Coefficient engagement{};
		if (type->transmission.automatic) {
			if (engine.running) {
				if (controls.gearStickPosition == 0) {
					engine.gear = 0;
				} else {
					if (engine.angularSpeed > type->engine.stallRPM.as(phys::AngularSpeed::UNIT)) {
						engagement =
							clamp(unlerp(engine.angularSpeed, type->engine.stallRPM.as(phys::AngularSpeed::UNIT), type->engine.idleRPM.as(phys::AngularSpeed::UNIT)), 0_x, 1_x);
					}
					const bool braking = brake > type->brakes.deadzone;
					const bool cruising = engine.gear == 0 || (!braking && gas > 0.1_x && gas < 0.5_x);
					const phys::AngularSpeed shiftUpThreshold =
						((cruising) ? type->engine.automaticCruiseShiftUpThresholdRPM : type->engine.automaticGasShiftUpThresholdRPM).as(phys::AngularSpeed::UNIT);
					const phys::AngularSpeed shiftDownThreshold =
						((cruising) ? type->engine.automaticCruiseShiftDownThresholdRPM : type->engine.automaticGasShiftDownThresholdRPM).as(phys::AngularSpeed::UNIT);
					if (engine.gear > 1 && engine.angularSpeed < shiftDownThreshold) {
						if (braking || countup(engine.timeSpentBelowShiftDownThreshold, deltaTime, type->engine.automaticShiftDownTime)) {
							engine.gear = (controls.gearStickPosition < 0) ? 0 : max(engine.gear - 1, int32_t{0});
							engine.timeSpentBelowShiftDownThreshold = {};
						}
					} else {
						engine.timeSpentBelowShiftDownThreshold = {};
						if (engine.gear == 0 || engine.angularSpeed > shiftUpThreshold) {
							if (engine.angularSpeed > type->engine.redlineRPM.as(phys::AngularSpeed::UNIT) || engine.gear == 0 ||
								countup(engine.timeSpentAboveShiftUpThreshold, deltaTime, type->engine.automaticShiftUpTime)) {
								engine.gear = (controls.gearStickPosition < 0) ? -1 : min(engine.gear + 1, static_cast<int32_t>(type->transmission.gearRatios.size()));
								engine.timeSpentAboveShiftUpThreshold = {};
							}
						} else {
							engine.timeSpentAboveShiftUpThreshold = {};
						}
					}
				}
			} else {
				engine.gear = 0;
			}
		} else {
			engine.gear = clamp(controls.gearStickPosition, -1, static_cast<int32_t>(type->transmission.gearRatios.size()));
			engagement = (engine.gear == 0) ? 0_x : 1_x - clutch;
		}

		// Calculate engine torque.
		const phys::Torque2D engineWideOpenThrottleTorqueAtIdleSpeed = type->engine.getAvailableTorque(type->engine.idleRPM.as(phys::AngularSpeed::UNIT));
		const phys::Torque2D engineClosedThrottleTorque =
			(type->engine.mechanicalEfficiency - 1_x) * engineWideOpenThrottleTorqueAtIdleSpeed * (engine.angularSpeed - type->engine.idleRPM.as(phys::AngularSpeed::UNIT)) /
			type->engine.idleRPM.as(phys::AngularSpeed::UNIT);
		const phys::Torque2D engineWideOpenThrottleTorque = max(type->engine.getAvailableTorque(engine.angularSpeed), min(engineClosedThrottleTorque, phys::Torque2D{}));
		const phys::Torque2D engineTorque = (engine.running) ? lerp(engineClosedThrottleTorque, engineWideOpenThrottleTorque, gas) : phys::Torque2D{};

		// Calculate torque transmitted to driven wheels.
		const phys::Scale1D transmissionGearRatio =
			(engine.gear == 0)  ? 1_x
			: (engine.gear < 0) ? -type->transmission.reverseGearRatio
			                    : type->transmission.gearRatios[static_cast<size_t>(engine.gear - 1)];
		const phys::Coefficient combinedGearRatio = transmissionGearRatio * type->differential.gearRatio;
		const phys::Torque2D driveTorque = engagement * engineTorque * combinedGearRatio;

		// Apply torque to the engine.
		const phys::MomentOfInertia2D engineMomentOfInertia =
			type->engine.momentOfInertia + engagement * drivenWheelCount * type->wheels.momentOfInertia / length2(combinedGearRatio);
		const phys::MomentOfInertia2D driveMomentOfInertia =
			drivenWheelCount * type->wheels.momentOfInertia + engagement * type->engine.momentOfInertia * length2(combinedGearRatio);
		engine.angularSpeed += engineTorque * deltaTime / engineMomentOfInertia;

		// Calculate wheel contacts.
		phys::Length1D turningWheelRelativeSuspensionOffset{};
		phys::Length1D nonTurningWheelRelativeSuspensionOffset{};
		for (size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex) {
			VehicleWheels::Wheel& wheel = wheels.wheels[wheelIndex];
			const VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];

			// Handle steering.
			wheel.suspensionOrientation = orientation;
			if (wheelInfo.turns && steering != 0) {
				const phys::Angle centerTurnAngle = steering * info.maxTurnAngle;
				const phys::Angle turnAngle = atan2(info.averageNonTurningWheelOffset.getZ() - wheelInfo.localOffset.getZ(),
					info.averageNonTurningWheelOffset.getZ() / tan(centerTurnAngle) - wheelInfo.localOffset.getX());
				const phys::Angle turnYaw = (centerTurnAngle < 0) ? phys::Angle{phys::PI - turnAngle} : phys::Angle{-turnAngle};
				wheel.suspensionOrientation = wheel.suspensionOrientation * phys::Orientation3D::yaw(turnYaw);
			}

			// Simulate suspension by casting the wheel shape along the suspension axis and seeing how far it is able to extend. This offset determines the spring force.
			const phys::Position3D suspensionRestPositionWheelCenter = position + orientation(scale * wheelInfo.localOffset);
			const phys::Direction3D suspensionDirection = wheel.suspensionOrientation(phys::Y_AXIS_3D);
			const phys::Distance maxSuspensionOffset =
				(wheelInfo.turns) ? type->suspension.springs.maxOffset * type->suspension.springs.turningWheelMultipliers.maxOffset : type->suspension.springs.maxOffset;
			const phys::Distance wheelRadius = wheelInfo.radius * scale.getY();
			const phys::Distance wheelHalfWidth = wheelInfo.halfWidth * scale.getX();
			const phys::CylinderShape3D wheelShape{.radius = wheelRadius, .halfLength = wheelHalfWidth};
			const phys::Transformation3D wheelCastStartTransformation =
				translateRotate(suspensionRestPositionWheelCenter + suspensionDirection * maxSuspensionOffset, wheel.suspensionOrientation * phys::Orientation3D::roll(90_degrees));
			phys::Length1D suspensionOffset{};
			if (const Optional<phys::Broadphase3D::ShapecastResult> shapecastResult = broadphase.shapecastClosestHit(wheelShape, VEHICLE_COLLISION_FILTER,
					wheelCastStartTransformation, -suspensionDirection, maxSuspensionOffset + 0.05_x * wheelRadius, objectEntities, simulationOptions.collisionAlgorithmOptions,
					phys::CollisionFilterTest::RESPONSE, [&](phys::EntityID otherObjectID) -> bool { return otherObjectID != entityID; })) {
				const phys::Position3D positionB = objectEntities.getComponent<phys::Position3D>(shapecastResult->objectID);
				const phys::LinearVelocity3D linearVelocityB = kinematicsEntities.getComponent<phys::LinearVelocity3D>(shapecastResult->objectID);
				const phys::AngularVelocity3D angularVelocityB = kinematicsEntities.getComponent<phys::AngularVelocity3D>(shapecastResult->objectID);

				// Calculate suspension offset and tire contact point.
				suspensionOffset = clamp(maxSuspensionOffset - shapecastResult->distance, phys::Length1D{}, maxSuspensionOffset);
				wheel.suspensionOffset = suspensionOffset * suspensionDirection;
				const phys::Position3D contactPoint = suspensionRestPositionWheelCenter + wheel.suspensionOffset - wheelRadius * shapecastResult->normal;
				const phys::Length3D contactOffsetA = contactPoint - position;
				const phys::Length3D contactOffsetB = contactPoint - positionB;
				const phys::LinearVelocity3D contactVelocityA = phys::calculatePointVelocity(linearVelocity, angularVelocity, contactOffsetA);
				const phys::LinearVelocity3D contactVelocityB = phys::calculatePointVelocity(linearVelocityB, angularVelocityB, contactOffsetB);
				const phys::LinearVelocity3D contactRelativeVelocity = contactVelocityA - contactVelocityB;

				wheel.contact = VehicleWheels::Wheel::Contact{
					.objectID = shapecastResult->objectID,
					.offsetA = contactOffsetA,
					.offsetB = contactOffsetB,
					.normal = shapecastResult->normal,
					.relativeVelocity = contactRelativeVelocity,
					.suspensionOffset = suspensionOffset,
				};
			} else {
				// The wheel is in the air. Decay the suspension back towards its rest position.
				wheel.suspensionOffset = expDecay(wheel.suspensionOffset, 5_Hertz, deltaTime);

				wheel.contact.reset();
			}

			phys::Length1D& relativeSuspensionOffset = (wheelInfo.turns) ? turningWheelRelativeSuspensionOffset : nonTurningWheelRelativeSuspensionOffset;
			relativeSuspensionOffset += flipSignIf(suspensionOffset, signbit(wheelInfo.localOffset.getX()));
		}

		const phys::Stiffness turningWheelAntiRollBarStiffness = type->suspension.antiRollBar.stiffness * type->suspension.antiRollBar.turningWheelMultipliers.stiffness;
		const phys::Stiffness nonTurningWheelAntiRollBarStiffness = type->suspension.antiRollBar.stiffness;
		const phys::Force1D turningWheelAntiRollBarForce = turningWheelRelativeSuspensionOffset * turningWheelAntiRollBarStiffness;
		const phys::Force1D nonTurningWheelAntiRollBarForce = nonTurningWheelRelativeSuspensionOffset * nonTurningWheelAntiRollBarStiffness;

		// Simulate wheels.
		phys::AngularImpulse2D finalDriveReflectedAngularImpulse{};
		phys::AngularVelocity2D finalDriveAngularVelocity{};
		for (size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex) {
			VehicleWheels::Wheel& wheel = wheels.wheels[wheelIndex];
			const VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];

			// Calculate torque contribution coefficient from the differential.
			const phys::Coefficient driveContribution =
				(wheelInfo.drives)
			        ? ((drivenWheelSpeedDifference < type->differential.maxLockedSpeedDifference.as(phys::AngularSpeed::UNIT))
			                  ? 1_x / drivenWheelCount
			                  : ((wheelIndex == fastestDrivenWheelIndex) ? type->differential.locking : (1_x - type->differential.locking)) / (drivenWheelCount - 1_x))
			        : 0_x;

			// Apply angular acceleration from the engine.
			const phys::MomentOfInertia2D momentOfInertia = (wheelInfo.drives) ? driveMomentOfInertia : type->wheels.momentOfInertia;
			wheel.angularVelocity += driveContribution * driveTorque * deltaTime / momentOfInertia;

			// Apply angular deceleration caused by brakes and damping (internal hub friction).
			const phys::Coefficient brakeWithDeadzone = (brake < type->brakes.deadzone) ? 0_x : brake;
			const phys::Coefficient brakeAmount = (wheelInfo.turns) ? brakeWithDeadzone : max(brakeWithDeadzone * (1_x - type->brakes.balance) / type->brakes.balance, handbrake);
			const phys::AngularImpulse2D brakeImpulse =
				clampLength(copysign(brakeAmount * type->brakes.torque * deltaTime, -wheel.angularVelocity) - wheel.angularVelocity * type->wheels.damping * deltaTime,
					abs(wheel.angularVelocity) * momentOfInertia);
			wheel.angularVelocity += brakeImpulse / momentOfInertia;

			if (wheel.contact) {
				const phys::OrthonormalBasis3D suspensionBasis = rotate(wheel.suspensionOrientation);
				const phys::Direction3D suspensionDirection = suspensionBasis[phys::Y];
				const phys::Direction3D sideDirection = suspensionBasis[phys::X];
				const phys::Direction3D forwardDirection = -suspensionBasis[phys::Z];
				const phys::LinearVelocity1D sideVelocity = dot(wheel.contact->relativeVelocity, sideDirection);
				const phys::LinearVelocity1D forwardVelocity = dot(wheel.contact->relativeVelocity, forwardDirection);
				const phys::Speed forwardSpeed = abs(forwardVelocity);
				const bool hasForwardSpeed = forwardSpeed > phys::Speed::MACHINE_EPSILON;

				// Calculate suspension force.
				const phys::Stiffness suspensionSpringStiffness =
					(wheelInfo.turns) ? type->suspension.springs.stiffness * type->suspension.springs.turningWheelMultipliers.stiffness : type->suspension.springs.stiffness;
				const phys::Force1D suspensionSpringForce = suspensionSpringStiffness * wheel.contact->suspensionOffset;
				const phys::Force1D suspensionDampingForce = wheelInfo.suspensionDamping * dot(wheel.contact->relativeVelocity, wheel.contact->normal);
				const phys::Force1D suspensionNormalForce = suspensionSpringForce - suspensionDampingForce;

				// Calculate anti-roll bar force.
				const phys::Force1D antiRollBarForce =
					flipSignIf((wheelInfo.turns) ? turningWheelAntiRollBarForce : nonTurningWheelAntiRollBarForce, signbit(wheelInfo.localOffset.getX()));

				// Calculate friction forces.
				const phys::Force1D tireLoadForce = max(suspensionNormalForce, phys::Force1D{});
				const phys::Distance wheelRadius = wheelInfo.radius * scale.getY();
				const phys::LinearVelocity1D tireVelocity = wheel.angularVelocity * wheelRadius;
				const phys::Angle slipYaw = (sideVelocity == 0 && !hasForwardSpeed) ? phys::Angle{} : atan2(sideVelocity, forwardSpeed);
				const phys::Angle slipAngle = -((slipYaw >= phys::PI) ? phys::Angle{slipYaw - phys::PI * 2_x} : slipYaw);
				const phys::LinearVelocity1D slipVelocity = forwardVelocity - tireVelocity;
				const phys::Coefficient slipRatio = clamp((hasForwardSpeed) ? -slipVelocity / forwardSpeed : copysign(1_x, tireVelocity), -100_x, 100_x);
				const VehicleSettings::Type::Tires::Friction& sideFriction = (wheelInfo.turns) ? type->tires.turningLateralFriction : type->tires.nonTurningLateralFriction;
				const VehicleSettings::Type::Tires::Friction& forwardFriction =
					(wheelInfo.turns) ? type->tires.turningLongitudalFriction : type->tires.nonTurningLongitudalFriction;
				const phys::Coefficient rollingResistance = (wheelInfo.turns) ? type->tires.turningRollingResistance : type->tires.nonTurningRollingResistance;
				const phys::Force1D rollingResistanceForce = copysign(min(tireLoadForce * rollingResistance, forwardSpeed / (deltaTime * inverseMass)), forwardVelocity);
				const phys::Force1D sideForce = sideFriction.getContactForce(slipAngle, tireLoadForce);
				const phys::Force1D forwardForce = forwardFriction.getContactForce(slipRatio, tireLoadForce) - rollingResistanceForce;

				// Clamp to friction ellipse.
				const phys::Coefficient groundFriction =
					clamp(unlerp(materialEntities.getComponent<phys::Material>(wheel.contact->objectID).kineticFriction, 0_x, 0.707_x), 0_x, 1_x);
				const phys::Force1D sideForceLimit = groundFriction * tireLoadForce * sideFriction.peak;
				const phys::Force1D forwardForceLimit = groundFriction * tireLoadForce * forwardFriction.peak;
				const phys::Scale1D normalizedSideForce = tryDivide(sideForce, sideForceLimit).value_or(0_x);
				const phys::Scale1D normalizedForwardForce = tryDivide(forwardForce, forwardForceLimit).value_or(0_x);
				const phys::Force2D frictionForces =
					phys::Force2D{sideForceLimit, forwardForceLimit} * clampLength(phys::Scale2D{normalizedSideForce, normalizedForwardForce}, 1_x);

				// Apply forces to vehicle body.
				const phys::Force3D totalForce = suspensionNormalForce * wheel.contact->normal + antiRollBarForce * suspensionDirection + frictionForces.getX() * sideDirection +
				                                 frictionForces.getY() * forwardDirection;
				const phys::LinearImpulse3D linearImpulse = totalForce * deltaTime;
				if (linearImpulse != 0) {
					auto&& [entityIDB, activityB, linearVelocityB, angularVelocityB, inverseMassB, inverseMomentOfInertiaTensorB] = kinematicsEntities[wheel.contact->objectID];

					phys::applyCorrectionImpulse(activity, linearVelocity, angularVelocity, inverseMass, inverseMomentOfInertiaTensor, linearImpulse, wheel.contact->offsetA);
					phys::applyCorrectionImpulse(activityB, linearVelocityB, angularVelocityB, inverseMassB, inverseMomentOfInertiaTensorB, -linearImpulse, wheel.contact->offsetB);
				}

				// Apply reflected angular impulse caused by contact friction.
				const phys::AngularImpulse2D angularContactFrictionImpulse =
					clampLength(-frictionForces.getY() * wheelRadius * deltaTime, abs(slipVelocity) * momentOfInertia / wheelRadius);
				wheel.angularVelocity += angularContactFrictionImpulse / momentOfInertia;

				finalDriveReflectedAngularImpulse += driveContribution * angularContactFrictionImpulse;
			}
			finalDriveReflectedAngularImpulse += driveContribution * brakeImpulse;
			finalDriveAngularVelocity += driveContribution * wheel.angularVelocity;

			// Update the wheel's rotation angle based on its current angular velocity, wrapping to 360 degrees to make sure it doesn't get too large.
			const phys::Angle deltaAngle = wheel.angularVelocity * deltaTime;
			const phys::AbsoluteAngle newAngle = wrap(wheel.angle + deltaAngle, phys::AbsoluteAngle{360_degrees});
			wheel.angle = newAngle;
			// Make sure the delta from previous to new angle is the same as it would have been without wrapping, for proper interpolation.
			wheel.previousAngle = newAngle - deltaAngle;
		}

		// Apply reflected angular impulse back to the engine.
		engine.angularSpeed += engagement * finalDriveReflectedAngularImpulse / (combinedGearRatio * engineMomentOfInertia);

		// Synchronize the engine with the wheels.
		const phys::AngularVelocity2D engineAngularVelocityExpected = finalDriveAngularVelocity * combinedGearRatio;
		const phys::AngularVelocity2D engineAngularVelocityError = engineAngularVelocityExpected - engine.angularSpeed;
		engine.angularSpeed = moveTowards(engine.angularSpeed, engineAngularVelocityExpected,
			engagement * abs(engineAngularVelocityError) * type->engine.synchronizationDamping * deltaTime / engineMomentOfInertia);

		// Handle engine stall.
		if (engine.angularSpeed < type->engine.stallRPM.as(phys::AngularSpeed::UNIT)) {
			engine.angularSpeed = 0;
			engine.running = false;
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
			const phys::AbsoluteAngle angle = mix(wheel.previousAngle, wheel.angle, frameInfo.tickInterpolationAlpha);
			res::Model::Joint& localJoint = modelInstance.pose.localJoints[wheelInfo.jointIndex];
			localJoint = info.model.getBindPose().localJoints[wheelInfo.jointIndex];
			localJoint.translation += vec3{inverseScale * inverseOrientation(suspensionOffset).in(phys::METERS)};
			localJoint.rotation = inverseOrientation * suspensionOrientation * phys::Orientation3D::pitch(-(+angle)) * localJoint.rotation;
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
				wheelInfo.suspensionDamping =
					type.suspension.springs.dampingRatio * 2.0f * sqrt(type.suspension.springs.stiffness * type.body.mass / static_cast<float>(info.wheels.size()));
			}
			for (const size_t wheelIndex : type.turning.wheels) {
				if (wheelIndex < info.wheels.size()) {
					VehicleInfo::Wheel& wheelInfo = info.wheels[wheelIndex];
					wheelInfo.suspensionDamping *= type.suspension.springs.turningWheelMultipliers.dampingRatio;
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
		scheduler.addTask<simulateVehicles>("Simulate vehicles");
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
				const VehicleWheels* const wheels = simulation.registry.findComponent<VehicleWheels>(occupant->vehicleEntityID);
				const VehicleType* const type = simulation.registry.findComponent<VehicleType>(occupant->vehicleEntityID);
				const phys::Scale3D* const scale = simulation.registry.findComponent<phys::Scale3D>(occupant->vehicleEntityID);
				if (controls && engine && wheels && type && scale) {
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

							drivenWheelSpeedSum += radius * abs(mix(wheel.previousAngularVelocity, wheel.angularVelocity, frameInfo.tickInterpolationAlpha));
							drivenWheelCount += 1_x;
						}
					}
					const phys::Speed speed = drivenWheelSpeedSum / drivenWheelCount;
					const phys::Coefficient clutch = clamp(controls->clutchDirect + controls->clutchSmooth, 0_x, 1_x);
					const phys::Coefficient brake = clamp(controls->brakeDirect + controls->brakeSmooth, 0_x, 1_x);
					const phys::Coefficient gas = clamp(controls->gasDirect + controls->gasSmooth, 0_x, 1_x);
					const float rpm = mix(engine->previousAngularSpeed, engine->angularSpeed, frameInfo.tickInterpolationAlpha).in(phys::REVOLUTIONS_PER_MINUTE);
					const float kmph = speed.in(phys::KILOMETERS_PER_HOUR);
					const String gearString = (engine->gear < 0) ? "R" : ((engine->gear == 0) ? "N" : formatString("{}", engine->gear));
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
						graphics.put2DText(screenCenter, (engine->starting) ? Color::WHITE : Color::GRAY,
							formatString("[{}] Start Engine", evt::getInputString(ignitionInputs.front())), 2.0f, gfx::TextAlign::CENTER);
					}

					if ((*type)->transmission.automatic) {
						drawPedalMeter(Offset2D{132, y + 16}, brake, "Brake", PlayerAction::BRAKE_SMOOTH);
						drawPedalMeter(Offset2D{204, y + 16}, gas, "Gas", PlayerAction::GAS_SMOOTH);
					} else {
						drawPedalMeter(Offset2D{96, y + 16}, clutch, "Clutch", PlayerAction::CLUTCH_SMOOTH);
						drawPedalMeter(Offset2D{168, y + 16}, brake, "Brake", PlayerAction::BRAKE_SMOOTH);
						drawPedalMeter(Offset2D{240, y + 16}, gas, "Gas", PlayerAction::GAS_SMOOTH);
					}

					if (const Span<const evt::Input> handbrakeInputs = input.getBoundInputs(PlayerAction::HANDBRAKE); !handbrakeInputs.empty()) {
						const phys::Coefficient handbrake = clamp(controls->handbrake, 0_x, 1_x);
						graphics.put2DText(Offset2D{168, y - 40}, mix(Color::GRAY, Color::WHITE, handbrake),
							formatString("[{}] Handbrake", evt::getInputString(handbrakeInputs.front())), 1.0f, gfx::TextAlign::CENTER);
					}

					drawAnalogInstrumentMeter(Offset2D{screenCenter.x - 128, y}, rpm * 0.001f, "RPM\nx1000", 0.0f, 8.0f, 1.0f,
						((*type)->engine.redlineRPM.in(phys::REVOLUTIONS_PER_MINUTE) - 1.0f) * 0.001f);
					drawAnalogInstrumentMeter(Offset2D{screenCenter.x + 128, y}, kmph, "km/h", 0.0f, 160.0f, 20.0f, Limits<float>::MAX);

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
