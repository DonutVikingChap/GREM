// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/execution/EntityRegistry.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/ResourceRegistry.hpp>
#include <GREM/execution/Scheduler.hpp>
#include <GREM/physics/Broadphase.hpp>
#include <GREM/physics/EntityID.hpp>
#include <GREM/physics/Simulation.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/joints.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include "simulation_tasks.hpp"

#include <utility> // std::move

#ifdef GREM_PHYSICS_USE_DEBUG_VISUALIZATION
#include <GREM/physics/DebugVisualization.hpp>
#endif

namespace grem::physics {

template <size_t N>
void Simulation<N>::addRequiredResources(ResourceRegistry<N>& resources, const SimulationOptions<N>& options) {
	resources.template getResource<SimulationOptions<N>>() = options;
	resources.template getResource<CollisionEvents<N>>().clear();
	resources.template getResource<SeparationEvents<N>>().clear();
	resources.template getResource<Broadphase<N>>() = {};
	resources.template getResource<Contacts<N>>().clear();
	resources.template addResource<detail::InvalidatedJoints>();
	resources.template addResource<detail::ActiveContactList>();
	resources.template addResource<detail::ContactColorGraph<N>>();
	resources.template addResource<detail::ContactColorOverflow>();
	resources.template addResource<detail::ContactManifoldInvalidation<N>>();
}

template <size_t N>
void Simulation<N>::removeResources(ResourceRegistry<N>& resources) noexcept {
	resources.template getResource<SimulationOptions<N>>() = {};
	resources.template getResource<CollisionEvents<N>>().clear();
	resources.template getResource<SeparationEvents<N>>().clear();
	resources.template getResource<Broadphase<N>>() = {};
	resources.template getResource<Contacts<N>>().clear();
	resources.template removeResource<detail::InvalidatedJoints>();
	resources.template removeResource<detail::ActiveContactList>();
	resources.template removeResource<detail::ContactColorGraph<N>>();
	resources.template removeResource<detail::ContactColorOverflow>();
	resources.template removeResource<detail::ContactManifoldInvalidation<N>>();
}

template <size_t N>
void Simulation<N>::addObjectComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID,
	ObjectOptions<N>&& options) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	registry.template addComponentIfMissing<Force<N>>(entityID, Force<N>{});
	registry.template addComponentIfMissing<Torque<N>>(entityID, Torque<N>{});
	registry.template addComponentIfMissing<Position<N>>(entityID, options.position);
	registry.template addComponentIfMissing<Orientation<N>>(entityID, options.orientation);
	registry.template addComponentIfMissing<Scale<N>>(entityID, options.scale);
	registry.template addComponentIfMissing<LinearVelocity<N>>(entityID, options.linearVelocity);
	registry.template addComponentIfMissing<AngularVelocity<N>>(entityID, options.angularVelocity);
	registry.template addComponentIfMissing<LinearAcceleration<N>>(entityID, options.gravityAcceleration);
	registry.template addComponentIfMissing<FluidDensity>(entityID, options.surroundingFluidDensity);
	registry.template addComponentIfMissing<CenterOfBuoyancy<N>>(entityID, options.centerOfBuoyancy);
	registry.template addComponentIfMissing<Collider<N>>(entityID, std::move(options.collider));
	if (Volume* const volume = registry.template addComponentIfMissing<Volume>(entityID, Volume{})) {
		*volume = ShapeView<N>{registry.template getComponent<Collider<N>>(entityID).shape}.calculateVolume();
	}
	Mass mass{};
	if (const InverseMass* const inverseMass = registry.template findComponent<InverseMass>(entityID)) {
		mass = calculateMass(*inverseMass);
	} else {
		mass = options.mass;
	}
	if (mass <= 0) {
		mass = registry.template getComponent<Volume>(entityID) * (0.5f * GRAMS_PER_CUBIC_CENTIMETER);
		if (mass <= 0) {
			mass = Mass::INF;
		}
	}
	registry.template addOrAssignComponent<InverseMass>(entityID, calculateInverseMass(mass));
	PrincipalMomentsOfInertia<N> principalMomentsOfInertia{};
	if (const InversePrincipalMomentsOfInertia<N>* const inversePrincipalMomentsOfInertia = registry.template findComponent<InversePrincipalMomentsOfInertia<N>>(entityID)) {
		principalMomentsOfInertia = calculateMomentOfInertia(*inversePrincipalMomentsOfInertia);
	} else {
		principalMomentsOfInertia = options.principalMomentsOfInertia;
	}
	if (any(equal(principalMomentsOfInertia, 0))) {
		const PrincipalMomentsOfInertia<N> calculatedPrincipalMomentsOfInertia =
			calculatePrincipalMomentsOfInertia(registry.template getComponent<Collider<N>>(entityID).shape, mass);
		if constexpr (N == 3) {
			meta::forEachIndex<3>([&](auto index) -> void {
				if (principalMomentsOfInertia[index] == 0) {
					principalMomentsOfInertia[index] = calculatedPrincipalMomentsOfInertia[index];
				}
			});
		} else {
			principalMomentsOfInertia = calculatedPrincipalMomentsOfInertia;
		}
	}
	const InversePrincipalMomentsOfInertia<N> inversePrincipalMomentsOfInertia = calculateInverseMomentOfInertia(principalMomentsOfInertia);
	registry.template addOrAssignComponent<InversePrincipalMomentsOfInertia<N>>(entityID, inversePrincipalMomentsOfInertia);
	registry.template addComponentIfMissing<LocalInertiaOrientation<N>>(entityID, options.localInertiaOrientation);
	registry.template addComponentIfMissing<Orientation<N>>(entityID, options.orientation);
	const LocalInertiaOrientation<N> localInertiaOrientation = registry.template getComponent<LocalInertiaOrientation<N>>(entityID);
	const Orientation<N> orientation = registry.template getComponent<Orientation<N>>(entityID);
	const MomentOfInertiaTensor<N> momentOfInertiaTensor =
		calculateMomentOfInertiaTensor(calculateMomentOfInertia(inversePrincipalMomentsOfInertia), localInertiaOrientation, orientation);
	const InverseMomentOfInertiaTensor<N> inverseMomentOfInertiaTensor =
		calculateInverseMomentOfInertiaTensor(inversePrincipalMomentsOfInertia, localInertiaOrientation, orientation);
	registry.template addComponentIfMissing<MomentOfInertiaTensor<N>>(entityID, momentOfInertiaTensor);
	registry.template addComponentIfMissing<InverseMomentOfInertiaTensor<N>>(entityID, inverseMomentOfInertiaTensor);
	registry.template addComponentIfMissing<Material>(entityID, options.material);
	registry.template addComponentIfMissing<ObjectFlags>(entityID,
		ObjectFlags{
			.emitsCollisionEvents = options.emitsCollisionEvents,
			.emitsSeparationEvents = options.emitsSeparationEvents,
			.enableResting = options.enableResting,
			.enableWaking = options.enableWaking,
		});
	registry.template addComponentIfMissing<ObjectContacts<N>>(entityID);
	if (ObjectActivity* const activity = registry.template addComponentIfMissing<ObjectActivity>(entityID)) {
		if (registry.template getComponent<InverseMass>(entityID) != 0 || registry.template getComponent<InversePrincipalMomentsOfInertia<N>>(entityID) != 0) {
			activity->isCorrectable = 1;
		}
		activity->energyLevel = options.energyLevel;
	}
	if (ObjectBounds<N>* const bounds = registry.template addComponentIfMissing<ObjectBounds<N>>(entityID)) {
		const Position<N> position = registry.template getComponent<Position<N>>(entityID);
		const Scale<N> scale = registry.template getComponent<Scale<N>>(entityID);
		const Transformation<N> transformation = translateRotateScale(position, orientation, scale);
		if (const Optional<Box<N>> shapeAABB = ShapeView<N>{registry.template getComponent<Collider<N>>(entityID).shape}.getBoundingBox(transformation)) {
			if (registry.template getComponent<ObjectActivity>(entityID).isCorrectable != 0) {
				const SimulationOptions<N>& simulationOptions = resources.template getResource<SimulationOptions<N>>();
				const Distance minAABBExpansion = simulationOptions.collisionAlgorithmOptions.maxCollisionTouchingDistance * 2.0f;
				const Time stepInterval = simulationOptions.stepInterval;
				const LinearVelocity<N> linearVelocity = registry.template getComponent<LinearVelocity<N>>(entityID);
				const AngularVelocity<N> angularVelocity = registry.template getComponent<AngularVelocity<N>>(entityID);
				const Distance approximateRadius = distance(shapeAABB->min, shapeAABB->max) * 0.5f;
				const Distance expansion =
					minAABBExpansion + length(linearVelocity) * stepInterval + min(length(angularVelocity) * approximateRadius * stepInterval, approximateRadius);
				bounds->boundingBox = shapeAABB->getExpanded(expansion);
			} else {
				bounds->boundingBox = *shapeAABB;
			}
		} else {
			bounds->boundingBox = {.min{Position<N>::MIN}, .max{Position<N>::MAX}};
		}
	}
	registry.template createComponentPool<ObjectActiveTag>();
	registry.template addComponentIfMissing<BroadphaseID>(entityID);
}

template <size_t N>
void Simulation<N>::removeObjectComponents(EntityRegistry<N>& registry, EntityID entityID) noexcept {
	registry.template removeComponent<Force<N>>(entityID);
	registry.template removeComponent<Torque<N>>(entityID);
	registry.template removeComponent<Position<N>>(entityID);
	registry.template removeComponent<Orientation<N>>(entityID);
	registry.template removeComponent<LinearVelocity<N>>(entityID);
	registry.template removeComponent<AngularVelocity<N>>(entityID);
	registry.template removeComponent<LinearAcceleration<N>>(entityID);
	registry.template removeComponent<FluidDensity>(entityID);
	registry.template removeComponent<CenterOfBuoyancy<N>>(entityID);
	registry.template removeComponent<Volume>(entityID);
	registry.template removeComponent<InverseMass>(entityID);
	registry.template removeComponent<Scale<N>>(entityID);
	registry.template removeComponent<InversePrincipalMomentsOfInertia<N>>(entityID);
	registry.template removeComponent<LocalInertiaOrientation<N>>(entityID);
	registry.template removeComponent<MomentOfInertiaTensor<N>>(entityID);
	registry.template removeComponent<InverseMomentOfInertiaTensor<N>>(entityID);
	registry.template removeComponent<Collider<N>>(entityID);
	registry.template removeComponent<Material>(entityID);
	registry.template removeComponent<ObjectFlags>(entityID);
	registry.template removeComponent<ObjectContacts<N>>(entityID);
	registry.template removeComponent<ObjectBounds<N>>(entityID);
	registry.template removeComponent<ObjectActivity>(entityID);
	registry.template removeComponent<ObjectActiveTag>(entityID);
	registry.template removeComponent<BroadphaseID>(entityID);
}

template <size_t N>
void Simulation<N>::updateObjectMomentOfInertiaTensor(EntityRegistry<N>& registry, const ResourceRegistry<N>&, EntityID objectID) {
	const InversePrincipalMomentsOfInertia<N> inversePrincipalMomentsOfInertia = registry.template getComponent<InversePrincipalMomentsOfInertia<N>>(objectID);
	const PrincipalMomentsOfInertia<N> principalMomentsOfInertia = calculateMomentOfInertia(inversePrincipalMomentsOfInertia);
	const LocalInertiaOrientation<N> localInertiaOrientation = registry.template getComponent<LocalInertiaOrientation<N>>(objectID);
	const Orientation<N> orientation = registry.template getComponent<Orientation<N>>(objectID);
	registry.template getComponent<MomentOfInertiaTensor<N>>(objectID) = calculateMomentOfInertiaTensor(principalMomentsOfInertia, localInertiaOrientation, orientation);
	registry.template getComponent<InverseMomentOfInertiaTensor<N>>(objectID) =
		calculateInverseMomentOfInertiaTensor(inversePrincipalMomentsOfInertia, localInertiaOrientation, orientation);
}

template <size_t N>
void Simulation<N>::updateObjectBounds(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID objectID) {
	const Collider<N>& collider = registry.template getComponent<Collider<N>>(objectID);
	const Position<N> position = registry.template getComponent<Position<N>>(objectID);
	const Orientation<N> orientation = registry.template getComponent<Orientation<N>>(objectID);
	const Scale<N> scale = registry.template getComponent<Scale<N>>(objectID);
	GREM_ASSERT(all(isfinite(position)));
	GREM_ASSERT(all(isfinite(orientation)));
	GREM_ASSERT(all(isfinite(scale)));
	Box<N> newBoundingBox{};
	const Transformation<N> transformation = translateRotateScale(position, orientation, scale);
	if (const Optional<Box<N>> shapeAABB = ShapeView<N>{collider.shape}.getBoundingBox(transformation)) {
		const LinearVelocity<N> linearVelocity = registry.template getComponent<LinearVelocity<N>>(objectID);
		const AngularVelocity<N> angularVelocity = registry.template getComponent<AngularVelocity<N>>(objectID);
		if (registry.template getComponent<ObjectActivity>(objectID).isCorrectable != 0) {
			const SimulationOptions<N>& simulationOptions = resources.template getResource<SimulationOptions<N>>();
			const Distance minAABBExpansion = simulationOptions.collisionAlgorithmOptions.maxCollisionTouchingDistance * 2.0f;
			const Time stepInterval = simulationOptions.stepInterval;
			const Distance approximateRadius = distance(shapeAABB->min, shapeAABB->max) * 0.5f;
			const Distance expansion =
				minAABBExpansion + length(linearVelocity) * stepInterval + min(length(angularVelocity) * approximateRadius * stepInterval, approximateRadius);
			newBoundingBox = shapeAABB->getExpanded(expansion);
		} else {
			newBoundingBox = *shapeAABB;
		}
	} else {
		newBoundingBox = {.min{Position<N>::MIN}, .max{Position<N>::MAX}};
	}
	registry.template getComponent<ObjectBounds<N>>(objectID).boundingBox = newBoundingBox;
	registry.template getComponent<BroadphaseID>(objectID) = {};
}

template <size_t N>
void Simulation<N>::updateBroadphase(EntityRegistry<N>& registry, ResourceRegistry<N>& resources) {
	if constexpr (N == 2) {
		detail::updateBroadphase2D(registry, resources);
	} else if constexpr (N == 3) {
		detail::updateBroadphase3D(registry, resources);
	}
}

template <size_t N>
void Simulation<N>::addGenericJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>&, EntityID entityID, Pair<EntityID> objectIDs,
	const GenericJointOptions<N>& options) {
	registry.template addComponentIfMissing<JointConnectedObjects>(entityID, objectIDs);
	registry.template addComponentIfMissing<JointAttachmentOffsets<N>>(entityID, options.attachmentOffsets);
	registry.template addComponentIfMissing<JointAttachmentOrientations<N>>(entityID, options.attachmentOrientations);
	if (options.linearConstraint) {
		registry.template addComponentIfMissing<JointLinearConstraint<N>>(entityID, *options.linearConstraint);
		registry.template addComponentIfMissing<JointLinearConstraintImpulses<N>>(entityID);
	}
	if (options.distanceConstraint) {
		registry.template addComponentIfMissing<JointDistanceConstraint<N>>(entityID, *options.distanceConstraint);
		registry.template addComponentIfMissing<JointDistanceConstraintImpulses<N>>(entityID);
	}
	if constexpr (N == 3) {
		if (options.coneConstraint) {
			registry.template addComponentIfMissing<JointConeConstraint3D>(entityID, *options.coneConstraint);
			registry.template addComponentIfMissing<JointConeConstraintImpulses3D>(entityID);
		}
		if (options.twistConstraint) {
			registry.template addComponentIfMissing<JointTwistConstraint3D>(entityID, *options.twistConstraint);
			registry.template addComponentIfMissing<JointTwistConstraintImpulses3D>(entityID);
		}
	}
	if (options.angularConstraint) {
		registry.template addComponentIfMissing<JointAngularConstraint<N>>(entityID, *options.angularConstraint);
		registry.template addComponentIfMissing<JointAngularConstraintImpulses<N>>(entityID);
	}
	registry.template addComponentIfMissing<JointFlags<N>>(entityID, options.flags);
	registry.template createComponentPool<JointActiveTag>();
}

template <size_t N>
void Simulation<N>::addWeldComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
	const WeldOptions<N>& options) {
	addGenericJointComponents(registry, resources, entityID, objectIDs,
		{
			.attachmentOffsets = options.attachmentOffsets,
			.attachmentOrientations = options.attachmentOrientations,
			.linearConstraint =
				JointLinearConstraint<N>{
					.minOffsets{},
					.maxOffsets{},
					.limitStiffnesses{options.linearStiffness},
					.limitDampingRatios{options.linearDampingRatio},
				},
			.angularConstraint =
				JointAngularConstraint<N>{
					.minAngles{},
					.maxAngles{},
					.limitStiffnesses{options.angularStiffness},
					.limitDampingRatios{options.angularDampingRatio},
				},
			.flags = options.flags,
		});
}

template <size_t N>
void Simulation<N>::addHingeJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
	const HingeJointOptions<N>& options) {
	GenericJointOptions<N> genericJointOptions{
		.attachmentOffsets = options.attachmentOffsets,
		.attachmentOrientations = options.attachmentOrientations,
		.linearConstraint =
			JointLinearConstraint<N>{
				.minOffsets{},
				.maxOffsets{},
				.limitStiffnesses{options.linearAttachmentStiffness},
				.limitDampingRatios{options.linearAttachmentDampingRatio},
			},
		.flags = options.flags,
	};
	if constexpr (N == 2) {
		if (options.minAngle > Angle::MIN || options.maxAngle < Angle::MAX || options.driveMaxTorque > 0) {
			genericJointOptions.angularConstraint = JointAngularConstraint2D{
				.driveTargetVelocities = options.driveTargetVelocity,
				.driveMaxTorques = options.driveMaxTorque,
				.minAngles = options.minAngle,
				.maxAngles = options.maxAngle,
				.limitStiffnesses = options.angularLimitStiffness,
				.limitDampingRatios = options.angularLimitDampingRatio,
			};
		}
	} else {
		genericJointOptions.coneConstraint = JointConeConstraint3D{
			.minAngle{},
			.maxAngle{},
			.limitStiffness = options.angularAttachmentStiffness,
			.limitDampingRatio = options.angularAttachmentDampingRatio,
		};
		if (options.minAngle > Angle::MIN || options.maxAngle < Angle::MAX || options.driveMaxTorque > 0) {
			genericJointOptions.twistConstraint = JointTwistConstraint3D{
				.driveTargetVelocity = options.driveTargetVelocity,
				.driveMaxTorque = options.driveMaxTorque,
				.minAngle = options.minAngle,
				.maxAngle = options.maxAngle,
				.limitStiffness = options.angularLimitStiffness,
				.limitDampingRatio = options.angularLimitDampingRatio,
			};
		}
	}
	addGenericJointComponents(registry, resources, entityID, objectIDs, genericJointOptions);
}

template <size_t N>
void Simulation<N>::addBallJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
	const BallJointOptions<N>& options) {
	GenericJointOptions<N> genericJointOptions{
		.attachmentOffsets = options.attachmentOffsets,
		.attachmentOrientations = options.attachmentOrientations,
		.linearConstraint =
			JointLinearConstraint<N>{
				.minOffsets{},
				.maxOffsets{},
				.limitStiffnesses{options.linearAttachmentStiffness},
				.limitDampingRatios{options.linearAttachmentDampingRatio},
			},
		.flags = options.flags,
	};
	if (options.minSwingAngle > Angle::MIN || options.maxSwingAngle < Angle::MAX) {
		if constexpr (N == 2) {
			genericJointOptions.angularConstraint = JointAngularConstraint2D{
				.minAngles = options.minSwingAngle,
				.maxAngles = options.maxSwingAngle,
				.limitStiffnesses{options.swingLimitStiffness},
				.limitDampingRatios{options.swingLimitDampingRatio},
			};
		} else {
			genericJointOptions.coneConstraint = JointConeConstraint3D{
				.minAngle = options.minSwingAngle,
				.maxAngle = options.maxSwingAngle,
				.limitStiffness = options.swingLimitStiffness,
				.limitDampingRatio = options.swingLimitDampingRatio,
			};
		}
	}
	if constexpr (N == 3) {
		if (options.minTwistAngle > Angle::MIN || options.maxTwistAngle < Angle::MAX || options.twistDriveMaxTorque > 0) {
			genericJointOptions.twistConstraint = JointTwistConstraint3D{
				.driveTargetVelocity = options.twistDriveTargetVelocity,
				.driveMaxTorque = options.twistDriveMaxTorque,
				.minAngle = options.minTwistAngle,
				.maxAngle = options.maxTwistAngle,
				.limitStiffness = options.twistLimitStiffness,
				.limitDampingRatio = options.twistLimitDampingRatio,
			};
		}
	}
	addGenericJointComponents(registry, resources, entityID, objectIDs, genericJointOptions);
}

template <size_t N>
void Simulation<N>::addPrismaticJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
	const PrismaticJointOptions<N>& options) {
	GenericJointOptions<N> genericJointOptions{
		.attachmentOffsets = options.attachmentOffsets,
		.attachmentOrientations = options.attachmentOrientations,
		.linearConstraint =
			JointLinearConstraint<N>{
				.minOffsets{},
				.maxOffsets{},
				.limitStiffnesses{options.linearAttachmentStiffness},
				.limitDampingRatios{options.linearAttachmentDampingRatio},
			},
		.flags = options.flags,
	};
	genericJointOptions.linearConstraint->minOffsets[X] = options.minOffset;
	genericJointOptions.linearConstraint->maxOffsets[X] = options.maxOffset;
	genericJointOptions.linearConstraint->limitStiffnesses[X] = options.linearLimitStiffness;
	genericJointOptions.linearConstraint->limitDampingRatios[X] = options.linearLimitDampingRatio;
	genericJointOptions.linearConstraint->driveTargetVelocities[X] = options.driveTargetVelocity;
	genericJointOptions.linearConstraint->driveMaxForces[X] = options.driveMaxForce;
	if constexpr (N == 2) {
		genericJointOptions.angularConstraint = JointAngularConstraint2D{
			.minAngles{},
			.maxAngles{},
			.limitStiffnesses = options.angularAttachmentStiffness,
			.limitDampingRatios = options.angularAttachmentDampingRatio,
		};
	} else {
		genericJointOptions.coneConstraint = JointConeConstraint3D{
			.minAngle{},
			.maxAngle{},
			.limitStiffness = options.angularAttachmentStiffness,
			.limitDampingRatio = options.angularAttachmentDampingRatio,
		};
		genericJointOptions.twistConstraint = JointTwistConstraint3D{
			.minAngle = options.restAngle,
			.maxAngle = options.restAngle,
			.limitStiffness = options.angularLimitStiffness,
			.limitDampingRatio = options.angularLimitDampingRatio,
		};
	}
	addGenericJointComponents(registry, resources, entityID, objectIDs, genericJointOptions);
}

template <size_t N>
void Simulation<N>::addCylinderJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
	const CylinderJointOptions<N>& options) {
	GenericJointOptions<N> genericJointOptions{
		.attachmentOffsets = options.attachmentOffsets,
		.attachmentOrientations = options.attachmentOrientations,
		.linearConstraint =
			JointLinearConstraint<N>{
				.minOffsets{},
				.maxOffsets{},
				.limitStiffnesses{options.linearAttachmentStiffness},
				.limitDampingRatios{options.linearAttachmentDampingRatio},
			},
		.flags = options.flags,
	};
	genericJointOptions.linearConstraint->minOffsets[X] = options.minOffset;
	genericJointOptions.linearConstraint->maxOffsets[X] = options.maxOffset;
	genericJointOptions.linearConstraint->limitStiffnesses[X] = options.linearLimitStiffness;
	genericJointOptions.linearConstraint->limitDampingRatios[X] = options.linearLimitDampingRatio;
	genericJointOptions.linearConstraint->driveTargetVelocities[X] = options.linearDriveTargetVelocity;
	genericJointOptions.linearConstraint->driveMaxForces[X] = options.linearDriveMaxForce;
	if constexpr (N == 2) {
		genericJointOptions.angularConstraint = JointAngularConstraint2D{
			.minAngles{},
			.maxAngles{},
			.limitStiffnesses = options.angularAttachmentStiffness,
			.limitDampingRatios = options.angularAttachmentDampingRatio,
		};
	} else {
		genericJointOptions.coneConstraint = JointConeConstraint3D{
			.minAngle{},
			.maxAngle{},
			.limitStiffness = options.angularAttachmentStiffness,
			.limitDampingRatio = options.angularAttachmentDampingRatio,
		};
		if (options.minAngle > Angle::MIN || options.maxAngle < Angle::MAX || options.angularDriveMaxTorque > 0) {
			genericJointOptions.twistConstraint = JointTwistConstraint3D{
				.driveTargetVelocity = options.angularDriveTargetVelocity,
				.driveMaxTorque = options.angularDriveMaxTorque,
				.minAngle = options.minAngle,
				.maxAngle = options.maxAngle,
				.limitStiffness = options.angularLimitStiffness,
				.limitDampingRatio = options.angularLimitDampingRatio,
			};
		}
	}
	addGenericJointComponents(registry, resources, entityID, objectIDs, genericJointOptions);
}

template <size_t N>
void Simulation<N>::removeJointComponents(EntityRegistry<N>& registry, EntityID entityID) noexcept {
	registry.template removeComponent<JointConnectedObjects>(entityID);
	registry.template removeComponent<JointAttachmentOffsets<N>>(entityID);
	registry.template removeComponent<JointAttachmentOrientations<N>>(entityID);
	registry.template removeComponent<JointLinearConstraint<N>>(entityID);
	registry.template removeComponent<JointLinearConstraintImpulses<N>>(entityID);
	registry.template removeComponent<JointDistanceConstraint<N>>(entityID);
	registry.template removeComponent<JointDistanceConstraintImpulses<N>>(entityID);
	registry.template removeComponent<JointConeConstraint3D>(entityID);
	registry.template removeComponent<JointConeConstraintImpulses3D>(entityID);
	registry.template removeComponent<JointTwistConstraint3D>(entityID);
	registry.template removeComponent<JointTwistConstraintImpulses3D>(entityID);
	registry.template removeComponent<JointAngularConstraint<N>>(entityID);
	registry.template removeComponent<JointAngularConstraintImpulses<N>>(entityID);
	registry.template removeComponent<JointFlags<N>>(entityID);
	registry.template removeComponent<JointActiveTag>(entityID);
}

template <size_t N>
void Simulation<N>::scheduleStep(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions, const ScheduleStepOptions<N>& scheduleStepOptions) {
	if constexpr (N == 2) {
		detail::scheduleStep2D(scheduler, simulationOptions, scheduleStepOptions);
	} else if constexpr (N == 3) {
		detail::scheduleStep3D(scheduler, simulationOptions, scheduleStepOptions);
	}
}

template <size_t N>
void Simulation<N>::scheduleBroadphaseUpdate(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions) {
	if constexpr (N == 2) {
		detail::scheduleBroadphaseUpdate2D(scheduler, simulationOptions);
	} else if constexpr (N == 3) {
		detail::scheduleBroadphaseUpdate3D(scheduler, simulationOptions);
	}
}

template <size_t N>
void Simulation<N>::scheduleCollisionDetection(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions) {
	if constexpr (N == 2) {
		detail::scheduleCollisionDetection2D(scheduler, simulationOptions);
	} else if constexpr (N == 3) {
		detail::scheduleCollisionDetection3D(scheduler, simulationOptions);
	}
}

template <size_t N>
void Simulation<N>::drawDebugVisualization([[maybe_unused]] DebugVisualization<N>& debugVisualization, [[maybe_unused]] const EntityRegistry<N>& registry,
	[[maybe_unused]] const ResourceRegistry<N>& resources) {
#ifdef GREM_PHYSICS_USE_DEBUG_VISUALIZATION
	// Draw broadphase.
	resources.template getResource<Broadphase<N>>().traverseNodes(
		[&](const Box<N>& boundingBox, typename Broadphase<N>::const_local_iterator first, typename Broadphase<N>::const_local_iterator last) -> void {
			debugVisualization.drawWorldAABBWireframe(boundingBox, (first == last) ? Color::DARK_MAGENTA : Color::MAGENTA);
		});

	// Draw contacts.
	const Contacts<N>& contacts = resources.template getResource<Contacts<N>>();
	for (const auto& [objectIDs, contact] : contacts) {
		const auto [objectIDA, objectIDB] = objectIDs;
		if (!registry.template hasComponent<Position<N>>(objectIDA) || !registry.template hasComponent<Collider<N>>(objectIDA) ||
			!registry.template hasComponent<Position<N>>(objectIDB) || !registry.template hasComponent<Collider<N>>(objectIDB)) {
			continue;
		}

		const Position<N> positionA = registry.template getComponent<Position<N>>(objectIDA);
		const Position<N> positionB = registry.template getComponent<Position<N>>(objectIDB);
		if (registry.template getComponent<Collider<N>>(objectIDA).shape.template is<InfiniteHalfSpaceShape<N>>() ||
			registry.template getComponent<Collider<N>>(objectIDB).shape.template is<InfiniteHalfSpaceShape<N>>()) {
			continue;
		}

		if constexpr (N == 3) {
			if (registry.template getComponent<Collider<N>>(objectIDA).shape.template is<InfinitePlaneShape3D>() ||
				registry.template getComponent<Collider<N>>(objectIDB).shape.template is<InfinitePlaneShape3D>()) {
				continue;
			}
		}
		debugVisualization.drawWorldLineSegment(positionA, positionB, Color::WHITE);
	}

	// Draw color summary.
	const detail::ContactColorGraph<N>& contactColorGraph = resources.template getResource<detail::ContactColorGraph<N>>();
	const SimulationOptions<N>& simulationOptions = resources.template getResource<SimulationOptions<N>>();
	const size_t activeColorCount = countIf(contactColorGraph, [&](const detail::ContactColor& contactColor) -> bool { return !contactColor.getContactIndices().empty(); });
	String colorDebugString = formatString("Contact colors: {}/{}", activeColorCount, simulationOptions.contactColorCount);
	size_t colorIndex = 0;
	for (const detail::ContactColor& contactColor : Span{contactColorGraph}.first(simulationOptions.contactColorCount)) {
		colorDebugString.append(formatString("\n  {}: {}", colorIndex, contactColor.getContactIndices().size()));
		++colorIndex;
	}
	colorDebugString.append(formatString("\n  Overflow: {}", resources.template getResource<detail::ContactColorOverflow>().size()));
	debugVisualization.drawUIText(vec2{4.0f, 48.0f + 9.0f * static_cast<float>(simulationOptions.contactColorCount + 1)}, 8, Color::LIGHT_GRAY, colorDebugString);

	// Draw collisions.
	const detail::ActiveContactList& activeContacts = resources.template getResource<detail::ActiveContactList>();
	for (const detail::ContactIndex contactIndex : activeContacts) {
		const auto& [objectIDs, contact] = contacts.getAtIndex(contactIndex);
		const auto [objectIDA, objectIDB] = objectIDs;
		if (!registry.template hasComponent<Position<N>>(objectIDA) || !registry.template hasComponent<Position<N>>(objectIDB)) {
			continue;
		}

		for (const ContactManifold<N>& manifold : contact.manifolds) {
			const Direction<N> normal = manifold.normal;
			for (const ContactPoint<N>& point : manifold.points) {
				const Position<N> positionA = registry.template getComponent<Position<N>>(objectIDA);
				const Position<N> positionB = registry.template getComponent<Position<N>>(objectIDB);
				const Position<N> pointA = positionA + point.offsets.first;
				const Position<N> pointB = positionB + point.offsets.second;
				debugVisualization.drawWorldPoint(pointA, Color::LIME);
				debugVisualization.drawWorldPoint(pointB, Color::YELLOW);
				debugVisualization.drawWorldVector(pointA, normal, Color::DARK_ORANGE);
				debugVisualization.drawWorldVector(pointB, -normal, Color::ORANGE);
				if (getNormalComponent(point.momentumInTangentSpace) != 0) {
					debugVisualization.drawWorldVector(pointA, normal, Color::MEDIUM_AQUA_MARINE, getNormalComponent(point.momentumInTangentSpace).in(Momentum::UNIT), 1.5f);
					debugVisualization.drawWorldVector(pointB, -normal, Color::AQUA, getNormalComponent(point.momentumInTangentSpace).in(Momentum::UNIT), 1.5f);
				}
				if (getTangentialComponent(point.momentumInTangentSpace) != 0) {
					if constexpr (N == 2) {
						debugVisualization.drawWorldVector(pointA, point.tangent, Color::CRIMSON, point.momentumInTangentSpace.getX().in(Momentum::UNIT), 1.5f);
					} else if constexpr (N == 3) {
						const Direction3D bitangent = Direction3D::reinterpret(cross(normal, point.tangent));
						debugVisualization.drawWorldVector(pointA, point.tangent, Color::CRIMSON, point.momentumInTangentSpace.getX().in(Momentum::UNIT), 1.5f);
						debugVisualization.drawWorldVector(pointA, bitangent, Color::FOREST_GREEN, point.momentumInTangentSpace.getY().in(Momentum::UNIT), 1.5f);
					}
				}
			}
		}
	}

	// Draw bounding boxes.
	const CollisionAlgorithmOptions<N>& collisionAlgorithmOptions = resources.template getResource<SimulationOptions<N>>().collisionAlgorithmOptions;
	for (auto&& [objectID, bounds, collider, position, orientation, scale] :
		registry.template getEntities<const ObjectBounds<N>, const Collider<N>, const Position<N>, const Orientation<N>, const Scale<N>>()) {
		if (const LocallyTransformedShape<N>* const locallyTransformedShape = collider.shape.template get_if<LocallyTransformedShape<N>>()) {
			const Transformation<N> transformation = translateRotateScale(position, orientation, scale);
			const LocalTransformation<N> localTransformation =
				translateRotateScale(locallyTransformedShape->localOffset, locallyTransformedShape->localOrientation, locallyTransformedShape->localScale);
			const Transformation<N> globalTransformation = transformation * localTransformation;
			if (const Optional<Box<N>> boundingBox = ShapeView<N>{*locallyTransformedShape->shape}.getBoundingBox(globalTransformation)) {
				debugVisualization.drawWorldAABBWireframe(boundingBox->getExpanded(collisionAlgorithmOptions.maxCollisionTouchingDistance),
					((registry.template hasComponent<ObjectActiveTag>(objectID)) ? Color::BLUE : Color::DIM_GRAY) * Color::fromLinear(0.7f));
			}
		} else if (const CompoundColliderShape<N>* const compoundColliderShape = collider.shape.template get_if<CompoundColliderShape<N>>()) {
			for (const SubCollider<N>& subCollider : compoundColliderShape->getSubColliders()) {
				const Transformation<N> transformation = translateRotateScale(position, orientation, scale);
				const LocalTransformation<N> localTransformation = translateRotateScale(subCollider.localOffset, subCollider.localOrientation, subCollider.localScale);
				const Transformation<N> globalTransformation = transformation * localTransformation;
				if (const Optional<Box<N>> boundingBox = ShapeView<N>{subCollider.collider.shape}.getBoundingBox(globalTransformation)) {
					debugVisualization.drawWorldAABBWireframe(boundingBox->getExpanded(collisionAlgorithmOptions.maxCollisionTouchingDistance),
						((registry.template hasComponent<ObjectActiveTag>(objectID)) ? Color::BLUE : Color::DIM_GRAY) * Color::fromLinear(0.7f));
				}
			}
		}
		debugVisualization.drawWorldAABBWireframe(bounds.boundingBox, (registry.template hasComponent<ObjectActiveTag>(objectID)) ? Color::BLUE : Color::DIM_GRAY);
	}

	// Draw velocities.
	for (auto&& [objectID, position, linearVelocity] : registry.template getEntities<const Position<N>, const LinearVelocity<N>>()) {
		debugVisualization.drawWorldVector(position, linearVelocity / LinearVelocity<N>::UNIT, Color::RED, 2.5f, 2.0f);
	}
#endif
}

template <size_t N>
Simulation<N>::Simulation(const SimulationOptions<N>& options, const ScheduleStepOptions<N>& scheduleStepOptions) {
	addRequiredResources(resources, options);

	Scheduler<N> scheduler{};
	scheduleStep(scheduler, options, scheduleStepOptions);
	stepSchedule = scheduler.buildSchedule();
}

template <size_t N>
void Simulation<N>::step(execution::Executor& executor) {
	executor.executeSchedule(stepSchedule, registry, resources);
}

template <size_t N>
void Simulation<N>::step() {
	execution::SequentialExecutor executor{};
	step(executor);
}

template struct Simulation<2>;
template struct Simulation<3>;

} // namespace grem::physics
