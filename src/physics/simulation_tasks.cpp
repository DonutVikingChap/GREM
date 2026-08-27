// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include "simulation_tasks.hpp"

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/execution/Chunk.hpp>
#include <GREM/execution/EntityRegistry.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/ResourceRegistry.hpp>
#include <GREM/execution/Scheduler.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/Broadphase.hpp>
#include <GREM/physics/EntityID.hpp>
#include <GREM/physics/Error.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/Simulation.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/joints.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include <stdexcept> // std::length_error

namespace grem::physics {

namespace detail {

namespace {

template <size_t N>
[[nodiscard]] InplaceArrayList<Position<N>, 2> findClosestFeaturePoints(Span<const Position<N>> facePoints, Position<N> position) {
	InplaceArrayList<Position<N>, 2> result{};
	SquaredDistance squaredDistance = SquaredDistance::MAX;
	for (size_t i = 0; i < facePoints.size(); ++i) {
		const Position<N>& pointA = facePoints[i];
		const Position<N>& pointB = facePoints[(i + 1) % facePoints.size()];

		const SquaredDistance squaredVertexDistance = distance2(position, pointA);
		if (squaredVertexDistance < squaredDistance) {
			squaredDistance = squaredVertexDistance;
			result = {pointA};
		}

		const Coefficient t = dot(position - pointA, pointB - pointA) / distance2(pointA, pointB);
		if (t > Coefficient{0} && t < Coefficient{1}) {
			const Position<N> otherClosestPointOnEdge = mix(pointA, pointB, t);
			const SquaredDistance squaredEdgeDistance = distance2(position, otherClosestPointOnEdge);
			if (squaredEdgeDistance < squaredDistance) {
				squaredDistance = squaredEdgeDistance;
				result = {pointA, pointB};
			}
		}
	}
	return result;
}

template <size_t N>
struct SoftConstraintParameters {
	Rate<N> biasRate{};
	Scale<N> massScale{1.0f};
	Scale<N> momentumScale{};
};

template <size_t N>
[[nodiscard]] SoftConstraintParameters<N> getSoftConstraintParameters(Rate<N> stiffness, Scale<N> dampingRatio, Time deltaTime, Frequency inverseDeltaTime) {
	// Reference: Erin Catto: "Soft Constraints": Game Developers Conference (2011): https://box2d.org/files/ErinCatto_SoftConstraints_GDC2011.pdf

	const Rate<N> hertz = min(stiffness, Rate<N>{0.25f * inverseDeltaTime});
	const Scale<N> zeta = dampingRatio;
	const Time h = deltaTime;

	const Quantity<N, AngularSpeed::Unit> omega = 2.0f * PI * hertz;
	const Quantity<N, Angle::Unit> a1 = 2.0f * zeta + h * omega;
	const Scale<N> a2 = h * omega * a1;
	const Scale<N> a3 = 1.0f / (1.0f + a2);
	return {
		.biasRate = select(equal(stiffness, 0), Rate<N>{}, omega / a1),
		.massScale = select(equal(stiffness, 0), Scale<N>{}, a2 * a3),
		.momentumScale = select(equal(stiffness, 0), Scale<N>{}, a3),
	};
}

template <size_t N>
void updateObjectActivityAndBounds(
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, ObjectBounds<N>, BroadphaseID, const Collider<N>, const Position<N>, const Orientation<N>,
		const Scale<N>, const ObjectFlags, const InverseMass, const MomentOfInertiaTensor<N>, const InverseMomentOfInertiaTensor<N>, const LinearAcceleration<N>, const Force<N>,
		const Torque<N>, const CenterOfBuoyancy<N>, const FluidDensity, const Volume>
		objectEntities,
	const SimulationOptions<N>& simulationOptions) {
	const Time stepInterval = simulationOptions.stepInterval;
	const SquaredSpeed maxRestingSquaredSpeed = length2(simulationOptions.maxRestingSpeed);
	const SquaredAngularSpeed maxRestingSquaredAngularSpeed = length2(simulationOptions.maxRestingAngularSpeed);
	const Distance minAABBExpansion = simulationOptions.collisionAlgorithmOptions.maxCollisionTouchingDistance * 2.0f;

	for (auto&& [objectID, activity, linearVelocity, angularVelocity, bounds, broadphaseID, collider, position, orientation, scale, flags, inverseMass, momentOfInertiaTensor,
			 inverseMomentOfInertiaTensor, gravityAcceleration, externalForce, externalTorque, centerOfBuoyancy, fluidDensity, volume] : objectEntities) {
		activity.wasCorrected = 0;
		if (length2(linearVelocity) <= maxRestingSquaredSpeed && length2(angularVelocity) <= maxRestingSquaredAngularSpeed && externalForce == 0 && externalTorque == 0 &&
			flags.enableResting) {
			if (activity.energyLevel > 0) {
				--activity.energyLevel;
			}
			if (activity.energyLevel == 0) {
				linearVelocity = {};
				angularVelocity = {};
			}
		} else if (flags.enableWaking) {
			activity.energyLevel = max(ObjectActivity::EnergyLevel{activity.energyLevel}, ObjectActivity::EnergyLevel{1});
		}

		if (activity.energyLevel > 0) {
			if (const Optional<Box<N>> shapeAABB = ShapeView<N>{collider.shape}.getBoundingBox(translateRotateScale(position, orientation, scale))) {
				if (activity.isCorrectable) {
					const Force<N> buoyancyForce = fluidDensity * gravityAcceleration * volume * product(scale);
					const Torque<N> buoyancyTorque = cross(centerOfBuoyancy, buoyancyForce);
					const LinearVelocity<N> predictedLinearVelocity = linearVelocity + (inverseMass * (externalForce - buoyancyForce) + gravityAcceleration) * stepInterval;
					const AngularVelocity<N> predictedAngularVelocity =
						angularVelocity +
						inverseMomentOfInertiaTensor * (externalTorque - buoyancyTorque - cross(angularVelocity, momentOfInertiaTensor * angularVelocity)) * stepInterval;
					const Distance approximateRadius = distance(shapeAABB->min, shapeAABB->max) * 0.5f;
					const Distance expansion = minAABBExpansion + length(predictedLinearVelocity) * stepInterval +
					                           min(length(predictedAngularVelocity) * approximateRadius * stepInterval, approximateRadius);
					bounds.boundingBox = shapeAABB->getExpanded(expansion);
				} else {
					bounds.boundingBox = *shapeAABB;
				}
			} else {
				bounds.boundingBox = {.min{Position<N>::MIN}, .max{Position<N>::MAX}};
			}
			broadphaseID = {};
		}
	}
}

template <size_t N>
void clearEvents(CollisionEvents<N>& collisionEvents, SeparationEvents<N>& separationEvents) {
	collisionEvents.clear();
	separationEvents.clear();
}

template <size_t N>
void updateBroadphase(execution::Entities<BroadphaseID, const Collider<N>, const ObjectBounds<N>> objectEntities, Broadphase<N>& broadphase,
	const SimulationOptions<N>& simulationOptions) {
	const auto getNewWorldBoundingBox = [&]() -> Optional<Box<N>> {
		Box<N> worldBoundingBox{.min = Position<N>::MAX, .max = Position<N>::MIN};
		bool containsValidEntitySize = false;
		for (auto&& [objectID, broadphaseID, collider, bounds] : objectEntities) {
			if (collider.filter.layers.empty() || (collider.filter.detectionLayers.empty() && collider.filter.responseLayers.empty())) {
				continue;
			}
			if (bounds.boundingBox.min != Position<N>::MIN || bounds.boundingBox.max != Position<N>::MAX) {
				worldBoundingBox.min = min(worldBoundingBox.min, bounds.boundingBox.min);
				worldBoundingBox.max = max(worldBoundingBox.max, bounds.boundingBox.max);
				containsValidEntitySize = true;
			}
		}
		if (!containsValidEntitySize) {
			return Box<N>{};
		}

		if (broadphase.getMinEntitySizeApproximation() != simulationOptions.minObjectSizeApproximation) {
			return worldBoundingBox;
		}

		const Box<N> rootBox = broadphase.getRootBox();
		if (any(lessThan(worldBoundingBox.min, rootBox.min) | greaterThan(worldBoundingBox.max, rootBox.max))) {
			const Position<N> worldCenter = midpoint(worldBoundingBox.min, worldBoundingBox.max);
			const Length<N> worldExtents = worldBoundingBox.max - worldBoundingBox.min;
			const Length<N> newRootHalfExtents = worldExtents * 0.75f;
			return Box<N>{.min = worldCenter - newRootHalfExtents, .max = worldCenter + newRootHalfExtents};
		}

		return {};
	};

	if (const Optional<Box<N>> worldBoundingBox = getNewWorldBoundingBox()) {
		broadphase.reset(*worldBoundingBox, simulationOptions.minObjectSizeApproximation);
		for (auto&& [objectID, broadphaseID, collider, bounds] : objectEntities) {
			broadphaseID = {};
		}
	}

	erase_if(broadphase, [&](EntityID entityID) -> bool { return !objectEntities.containsEntity(entityID) || !objectEntities.template getComponent<BroadphaseID>(entityID); });
	for (auto&& [objectID, broadphaseID, collider, bounds] : objectEntities) {
		if (collider.filter.layers.empty() || (collider.filter.detectionLayers.empty() && collider.filter.responseLayers.empty())) {
			continue;
		}
		if (!broadphaseID) {
			broadphaseID = broadphase.insert(bounds.boundingBox, objectID);
		}
	}
}

template <size_t N>
void removeInvalidatedContacts(execution::Entities<ObjectActivity, ObjectContacts<N>, const ObjectFlags, const ObjectBounds<N>, const Position<N>, const Orientation<N>,
								   const Scale<N>, const LinearVelocity<N>, const AngularVelocity<N>>
								   objectEntities,
	Contacts<N>& contacts, SeparationEvents<N>& separationEvents) {
	erase_if(contacts, [&](const auto& kv) -> bool {
		const auto& [objectIDs, contact] = kv;
		const auto [objectIDA, objectIDB] = objectIDs;
		const bool isValidA = objectEntities.containsEntity(objectIDA);
		const bool isValidB = objectEntities.containsEntity(objectIDB);
		if (isValidA & isValidB) {
			if (intersects(objectEntities.template getComponent<ObjectBounds<N>>(objectIDA).boundingBox,
					objectEntities.template getComponent<ObjectBounds<N>>(objectIDB).boundingBox)) {
				return false; // Contact is still valid.
			}
		}

		// Contact is invalid. Remove it and emit final separation events if applicable.
		if (isValidA) {
			erase(objectEntities.template getComponent<ObjectContacts<N>>(objectIDA).otherObjectIDs, objectIDB);
		}
		if (isValidB) {
			erase(objectEntities.template getComponent<ObjectContacts<N>>(objectIDB).otherObjectIDs, objectIDA);
		}

		if (!contact.manifolds.empty()) {
			if (!isValidA) {
				if (isValidB) {
					auto&& [entityIDB, activityB, contactsB, flagsB, boundsB, positionB, orientationB, scaleB, linearVelocityB, angularVelocityB] = objectEntities[objectIDB];
					if (activityB.isCorrectable != 0 && flagsB.enableWaking) {
						activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					}
					if (flagsB.emitsSeparationEvents) {
						const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
						separationEvents.push_back(SeparationEvent<N>{
							.objectIDs{objectIDA, objectIDB},
							.objectTransformations{transformationB, transformationB},
							.objectLinearVelocities{{}, linearVelocityB},
							.objectAngularVelocities{{}, angularVelocityB},
						});
					}
				}
			} else if (!isValidB) {
				auto&& [entityIDA, activityA, contactsA, flagsA, boundsA, positionA, orientationA, scaleA, linearVelocityA, angularVelocityA] = objectEntities[objectIDA];
				if (activityA.isCorrectable != 0 && flagsA.enableWaking) {
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				}
				if (flagsA.emitsSeparationEvents) {
					const Transformation<N> transformationA = translateRotateScale(positionA, orientationA, scaleA);
					separationEvents.push_back(SeparationEvent<N>{
						.objectIDs{objectIDA, objectIDB},
						.objectTransformations{transformationA, transformationA},
						.objectLinearVelocities{linearVelocityA, {}},
						.objectAngularVelocities{angularVelocityA, {}},
					});
				}
			} else {
				auto&& [entityIDA, activityA, contactsA, flagsA, boundsA, positionA, orientationA, scaleA, linearVelocityA, angularVelocityA] = objectEntities[objectIDA];
				auto&& [entityIDB, activityB, contactsB, flagsB, boundsB, positionB, orientationB, scaleB, linearVelocityB, angularVelocityB] = objectEntities[objectIDB];
				if (activityB.energyLevel > activityA.energyLevel) {
					if (activityA.isCorrectable != 0 && flagsA.enableWaking) {
						activityA.energyLevel = activityB.energyLevel;
					}
				} else if (activityA.energyLevel > activityB.energyLevel) {
					if (activityB.isCorrectable != 0 && flagsB.enableWaking) {
						activityB.energyLevel = activityA.energyLevel;
					}
				}
				if (flagsA.emitsSeparationEvents || flagsB.emitsSeparationEvents) {
					const Transformation<N> transformationA = translateRotateScale(positionA, orientationA, scaleA);
					const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
					separationEvents.push_back(SeparationEvent<N>{
						.objectIDs{objectIDA, objectIDB},
						.objectTransformations{transformationA, transformationB},
						.objectLinearVelocities{linearVelocityA, linearVelocityB},
						.objectAngularVelocities{angularVelocityA, angularVelocityB},
					});
				}
			}
		}
		return true;
	});
}

template <size_t N>
void removeInvalidatedContactsWithoutEmittingEventsOrAffectingActivity(execution::Entities<ObjectContacts<N>, const ObjectBounds<N>> objectEntities, Contacts<N>& contacts) {
	erase_if(contacts, [&](const auto& kv) -> bool {
		const auto& [objectIDs, contact] = kv;
		const auto [objectIDA, objectIDB] = objectIDs;
		const bool isValidA = objectEntities.containsEntity(objectIDA);
		const bool isValidB = objectEntities.containsEntity(objectIDB);
		if (isValidA & isValidB) {
			if (intersects(objectEntities.template getComponent<ObjectBounds<N>>(objectIDA).boundingBox,
					objectEntities.template getComponent<ObjectBounds<N>>(objectIDB).boundingBox)) {
				return false; // Contact is still valid.
			}
		}

		// Contact is invalid. Remove it.
		if (isValidA) {
			erase(objectEntities.template getComponent<ObjectContacts<N>>(objectIDA).otherObjectIDs, objectIDB);
		}
		if (isValidB) {
			erase(objectEntities.template getComponent<ObjectContacts<N>>(objectIDB).otherObjectIDs, objectIDA);
		}
		return true;
	});
}

template <size_t N>
void updateActiveObjectSet(execution::Entities<const ObjectActivity> objectEntities, execution::ComponentPool<ObjectActiveTag> objectActiveTags) {
	for (auto&& [objectID, activity] : objectEntities) {
		if (activity.energyLevel == 0) {
			objectActiveTags.remove(objectID);
		} else {
			objectActiveTags.addIfMissing(objectID);
		}
	}
}

template <size_t N>
void addNewContacts(execution::Entities<ObjectContacts<N>, const Collider<N>, const ObjectBounds<N>, const ObjectActiveTag> activeObjectEntities,
	execution::Entities<ObjectContacts<N>, const Collider<N>, const ObjectBounds<N>> objectEntities, Contacts<N>& contacts, const Broadphase<N>& broadphase) {
	for (auto&& [objectIDA, contactsA, colliderA, boundsA, activeTagA] : activeObjectEntities) {
		if (colliderA.filter.layers.empty() || colliderA.filter.detectionLayers.empty()) {
			continue;
		}

		broadphase.traverseEntities(
			[&](EntityID objectIDB) -> void {
				if (objectIDA != objectIDB && objectEntities.containsEntity(objectIDB) &&
					CollisionFilterTest::DETECTION(colliderA.filter, objectEntities.template getComponent<Collider<N>>(objectIDB).filter) &&
					intersects(boundsA.boundingBox, objectEntities.template getComponent<ObjectBounds<N>>(objectIDB).boundingBox)) {
					const auto [orderedObjectIDA, orderedObjectIDB] = (objectIDA < objectIDB) ? Pair{objectIDA, objectIDB} : Pair{objectIDB, objectIDA};
					const Pair<EntityID> orderedObjectIDs{orderedObjectIDA, orderedObjectIDB};
					auto&& [orderedEntityIDA, orderedContactsA, orderedColliderA, orderedBoundsA] = objectEntities[orderedObjectIDA];
					auto&& [orderedEntityIDB, orderedContactsB, orderedColliderB, orderedBoundsB] = objectEntities[orderedObjectIDB];
					if (const auto [it, inserted] = contacts.try_emplace(orderedObjectIDs, orderedColliderA.shape, orderedColliderB.shape); inserted) {
						GREM_ASSERT(!contains(orderedContactsA.otherObjectIDs, orderedObjectIDB));
						GREM_ASSERT(!contains(orderedContactsB.otherObjectIDs, orderedObjectIDA));
						orderedContactsA.otherObjectIDs.push_back(orderedObjectIDB);
						orderedContactsB.otherObjectIDs.push_back(orderedObjectIDA);
					}
				}
			},
			[&](const Box<N>& boundingBox) -> bool { return intersects(boundingBox, boundsA.boundingBox); });
	}
}

template <size_t N>
void gatherActiveContacts(execution::Entities<const ObjectActiveTag> activeObjectEntities, ActiveContactList& activeContactList, const Contacts<N>& contacts) {
	if (contacts.end().getIndex() > size_t{Limits<ContactIndex>::MAX}) {
		throw std::length_error{"Maximum contact count exceeded."};
	}

	activeContactList.clear();
	for (auto it = contacts.begin(); it != contacts.end(); ++it) {
		const auto [objectIDA, objectIDB] = it->first;
		if (activeObjectEntities.containsEntity(objectIDA) || activeObjectEntities.containsEntity(objectIDB)) {
			activeContactList.push_back(static_cast<ContactIndex>(it.getIndex()));
		}
	}
}

template <size_t N>
void colorContacts(execution::Entities<const ObjectActivity, const Collider<N>> objectEntities, ContactColorGraph<N>& contactColorGraph, ContactColorOverflow& contactColorOverflow,
	const Contacts<N>& contacts, const ActiveContactList& activeContactList, const SimulationOptions<N>& simulationOptions) {
	for (ContactColor& contactColor : Span{contactColorGraph}.first(simulationOptions.contactColorCount)) {
		contactColor.clear();
	}
	contactColorOverflow.clear();

	for (const ContactIndex contactIndex : activeContactList) {
		const auto& [objectIDs, contact] = contacts.getAtIndex(contactIndex);

		const auto& [objectIDA, activityA, colliderA] = objectEntities[objectIDs.first];
		const auto& [objectIDB, activityB, colliderB] = objectEntities[objectIDs.second];

		if (!CollisionFilterTest::RESPONSE(colliderA.filter, colliderB.filter)) {
			continue;
		}

		bool colored = false;
		for (ContactColor& contactColor : Span{contactColorGraph}.first(simulationOptions.contactColorCount)) {
			if (contactColor.tryAddContact(contactIndex, objectIDA.getIndex(), activityA.isCorrectable != 0, objectIDB.getIndex(), activityB.isCorrectable != 0)) {
				colored = true;
				break;
			}
		}
		if (!colored) {
			contactColorOverflow.push_back(contactIndex);
		}
	}
}

template <size_t N>
void decayJointConstraintMomentums(execution::Entities<JointAngularConstraintImpulses<N>, const JointActiveTag> angularConstraintJointEntities,
	execution::Entities<JointLinearConstraintImpulses<N>, const JointActiveTag> linearConstraintJointEntities,
	execution::Entities<JointDistanceConstraintImpulses<N>, const JointActiveTag> distanceConstraintJointEntities, const SimulationOptions<N>& simulationOptions) {
	for (auto&& [jointID, angularConstraintImpulses, activeTag] : angularConstraintJointEntities) {
		angularConstraintImpulses.driveMomentums *= simulationOptions.jointWarmstartCoefficient;
		angularConstraintImpulses.driveImpulses = {};
		angularConstraintImpulses.lowerLimitMomentums *= simulationOptions.jointWarmstartCoefficient;
		angularConstraintImpulses.lowerLimitImpulses = {};
		angularConstraintImpulses.upperLimitMomentums *= simulationOptions.jointWarmstartCoefficient;
		angularConstraintImpulses.upperLimitImpulses = {};
	}

	for (auto&& [jointID, linearConstraintImpulses, activeTag] : linearConstraintJointEntities) {
		linearConstraintImpulses.driveMomentums *= simulationOptions.jointWarmstartCoefficient;
		linearConstraintImpulses.driveImpulses = {};
		linearConstraintImpulses.lowerLimitMomentums *= simulationOptions.jointWarmstartCoefficient;
		linearConstraintImpulses.lowerLimitImpulses = {};
		linearConstraintImpulses.upperLimitMomentums *= simulationOptions.jointWarmstartCoefficient;
		linearConstraintImpulses.upperLimitImpulses = {};
	}

	for (auto&& [jointID, distanceConstraintImpulses, activeTag] : distanceConstraintJointEntities) {
		distanceConstraintImpulses.driveMomentum *= simulationOptions.jointWarmstartCoefficient;
		distanceConstraintImpulses.driveImpulse = {};
		distanceConstraintImpulses.lowerLimitMomentum *= simulationOptions.jointWarmstartCoefficient;
		distanceConstraintImpulses.lowerLimitImpulse = {};
		distanceConstraintImpulses.upperLimitMomentum *= simulationOptions.jointWarmstartCoefficient;
		distanceConstraintImpulses.upperLimitImpulse = {};
	}
}

void decay3DOnlyJointConstraintMomentums(execution::Entities<JointConeConstraintImpulses3D, const JointActiveTag> coneConstraintJointEntities,
	execution::Entities<JointTwistConstraintImpulses3D, const JointActiveTag> twistConstraintJointEntities, const SimulationOptions3D& simulationOptions) {
	for (auto&& [jointID, coneConstraintImpulses, activeTag] : coneConstraintJointEntities) {
		coneConstraintImpulses.lowerLimitMomentum *= simulationOptions.jointWarmstartCoefficient;
		coneConstraintImpulses.lowerLimitImpulse = {};
		coneConstraintImpulses.upperLimitMomentum *= simulationOptions.jointWarmstartCoefficient;
		coneConstraintImpulses.upperLimitImpulse = {};
	}

	for (auto&& [jointID, twistConstraintImpulses, activeTag] : twistConstraintJointEntities) {
		twistConstraintImpulses.driveMomentum *= simulationOptions.jointWarmstartCoefficient;
		twistConstraintImpulses.driveImpulse = {};
		twistConstraintImpulses.lowerLimitMomentum *= simulationOptions.jointWarmstartCoefficient;
		twistConstraintImpulses.lowerLimitImpulse = {};
		twistConstraintImpulses.upperLimitMomentum *= simulationOptions.jointWarmstartCoefficient;
		twistConstraintImpulses.upperLimitImpulse = {};
	}
}

template <size_t N>
void decayContactConstraintMomentumsImplementation(Contacts<N>& contacts, Span<const ContactIndex> contactIndices, const SimulationOptions<N>& simulationOptions) {
	for (const ContactIndex contactIndex : contactIndices) {
		auto&& [objectIDs, contact] = contacts.getAtIndex(contactIndex);
		for (ContactManifold<N>& manifold : contact.manifolds) {
			for (ContactPoint<N>& point : manifold.points) {
				point.momentumInTangentSpace *= simulationOptions.contactWarmstartCoefficient;
				point.impulseInTangentSpace = {};
				point.restitutionMomentum = {};
				point.restitutionImpulse = {};
			}
			manifold.rollingResistanceMomentum *= simulationOptions.contactWarmstartCoefficient;
			manifold.rollingResistanceImpulse = {};
		}
	}
}

template <size_t N, size_t ColorIndex>
void decayColoredContactConstraintMomentums(Contacts<N>& contacts,
	execution::Chunk<const ContactColorGraph<N>, ContactColorGraph<N>::template getColoredContactIndices<ColorIndex>> coloredContacts,
	const SimulationOptions<N>& simulationOptions) {
	decayContactConstraintMomentumsImplementation<N>(contacts, coloredContacts, simulationOptions);
}

template <size_t N>
void decayOverflowContactConstraintMomentums(Contacts<N>& contacts, const ContactColorOverflow& contactColorOverflow, const SimulationOptions<N>& simulationOptions) {
	decayContactConstraintMomentumsImplementation<N>(contacts, contactColorOverflow, simulationOptions);
}

template <size_t N>
void detectCollisions(execution::Entities<const Collider<N>, const Position<N>, const Orientation<N>, const Scale<N>> objectEntities, Contacts<N>& contacts,
	execution::Chunk<const ActiveContactList> activeContactList, const SimulationOptions<N>& simulationOptions) {
	const Distance maxCollisionTouchingDistance = simulationOptions.collisionAlgorithmOptions.maxCollisionTouchingDistance;
	const Scale1D minContactManifoldDotProduct = cos(simulationOptions.maxContactManifoldAngleTolerance);

	Arena<8192> arena{};
	for (const ContactIndex contactIndex : activeContactList) {
		auto&& [objectIDs, contact] = contacts.getAtIndex(contactIndex);
		const auto& [objectIDA, colliderA, positionA, orientationA, scaleA] = objectEntities[objectIDs.first];
		const auto& [objectIDB, colliderB, positionB, orientationB, scaleB] = objectEntities[objectIDs.second];
		const Transformation<N> transformationA = translateRotateScale(positionA, orientationA, scaleA);
		const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
		// Note: This parallel mutation is safe because all indices in the ActiveContactList are guaranteed to be unique.
		contact.wasTouchingOnLastSubStep = !contact.manifolds.empty();
		contact.manifolds.clear();
		arena.release();
		typename Contact<N>::ManifoldMask mergedManifolds{};
		contact.collisionAlgorithm.detectCollisions(&arena, colliderA, transformationA, colliderB, transformationB, simulationOptions.collisionAlgorithmOptions, {},
			[&](const CollisionAlgorithmResult<N>& collision) -> void {
				GREM_ASSERT(!collision.manifold.points.empty());
				contact.insertCollision(mergedManifolds, collision, transformationA, transformationB, {}, {}, {}, {}, maxCollisionTouchingDistance, minContactManifoldDotProduct);
			});
	}
}

template <size_t N>
void detectPredictedCollisions(
	execution::Entities<const Collider<N>, const Position<N>, const Orientation<N>, const Scale<N>, const LinearVelocity<N>, const AngularVelocity<N>, const LinearAcceleration<N>,
		const Force<N>, const Torque<N>, const CenterOfBuoyancy<N>, const FluidDensity, const Volume, const InverseMass, const MomentOfInertiaTensor<N>,
		const InverseMomentOfInertiaTensor<N>>
		objectEntities,
	Contacts<N>& contacts, execution::Chunk<const ActiveContactList> activeContactList, const SimulationOptions<N>& simulationOptions) {
	const Time stepInterval = simulationOptions.stepInterval;
	const Distance maxCollisionTouchingDistance = simulationOptions.collisionAlgorithmOptions.maxCollisionTouchingDistance;
	const Scale1D minContactManifoldDotProduct = cos(simulationOptions.maxContactManifoldAngleTolerance);

	Arena<8192> arena{};
	for (const ContactIndex contactIndex : activeContactList) {
		auto&& [objectIDs, contact] = contacts.getAtIndex(contactIndex);

		const auto& [objectIDA, colliderA, positionA, orientationA, scaleA, linearVelocityA, angularVelocityA, gravityAccelerationA, externalForceA, externalTorqueA,
			centerOfBuoyancyA, fluidDensityA, volumeA, inverseMassA, momentOfInertiaTensorA, inverseMomentOfInertiaTensorA] = objectEntities[objectIDs.first];
		const auto& [objectIDB, colliderB, positionB, orientationB, scaleB, linearVelocityB, angularVelocityB, gravityAccelerationB, externalForceB, externalTorqueB,
			centerOfBuoyancyB, fluidDensityB, volumeB, inverseMassB, momentOfInertiaTensorB, inverseMomentOfInertiaTensorB] = objectEntities[objectIDs.second];

		const Force<N> buoyancyForceA = fluidDensityA * gravityAccelerationA * volumeA * product(scaleA);
		const Force<N> buoyancyForceB = fluidDensityB * gravityAccelerationB * volumeB * product(scaleB);
		const Torque<N> buoyancyTorqueA = cross(centerOfBuoyancyA, buoyancyForceA);
		const Torque<N> buoyancyTorqueB = cross(centerOfBuoyancyB, buoyancyForceB);
		const LinearVelocity<N> predictedLinearVelocityA = linearVelocityA + (inverseMassA * (externalForceA - buoyancyForceA) + gravityAccelerationA) * stepInterval;
		const LinearVelocity<N> predictedLinearVelocityB = linearVelocityB + (inverseMassB * (externalForceB - buoyancyForceB) + gravityAccelerationB) * stepInterval;
		const AngularVelocity<N> predictedAngularVelocityA =
			angularVelocityA +
			inverseMomentOfInertiaTensorA * (externalTorqueA - buoyancyTorqueA - cross(angularVelocityA, momentOfInertiaTensorA * angularVelocityA)) * stepInterval;
		const AngularVelocity<N> predictedAngularVelocityB =
			angularVelocityB +
			inverseMomentOfInertiaTensorB * (externalTorqueB - buoyancyTorqueB - cross(angularVelocityB, momentOfInertiaTensorB * angularVelocityB)) * stepInterval;

		const Position<N> predictedPositionA = positionA + predictedLinearVelocityA * stepInterval;
		const Position<N> predictedPositionB = positionB + predictedLinearVelocityB * stepInterval;
		const Orientation<N> predictedOrientationA = orientationA + predictedAngularVelocityA * stepInterval;
		const Orientation<N> predictedOrientationB = orientationB + predictedAngularVelocityB * stepInterval;

		const Transformation<N> predictedTransformationA = translateRotateScale(predictedPositionA, predictedOrientationA, scaleA);
		const Transformation<N> predictedTransformationB = translateRotateScale(predictedPositionB, predictedOrientationB, scaleB);

		// Note: This parallel mutation is safe because all indices in the ActiveContactList are guaranteed to be unique.
		contact.wasTouchingOnLastSubStep = !contact.manifolds.empty();
		for (ContactManifold<N>& manifold : contact.manifolds) {
			for (ContactPoint<N>& point : manifold.points) {
				const Length<N> offsetA = predictedTransformationA.getBasis() * point.localOffsets.first;
				const Length<N> offsetB = predictedTransformationB.getBasis() * point.localOffsets.second;
				point.offsets = {offsetA, offsetB};
			}
			erase_if(manifold.points, [&](const ContactPoint<N>& point) -> bool {
				const Position<N> pointA = predictedTransformationA.getOrigin() + point.offsets.first;
				const Position<N> pointB = predictedTransformationB.getOrigin() + point.offsets.second;
				return distance2(pointA, pointB) > length2(maxCollisionTouchingDistance);
			});
		}
		erase_if(contact.manifolds, [&](const ContactManifold<N>& manifold) -> bool { return manifold.points.empty(); });
		arena.release();
		typename Contact<N>::ManifoldMask mergedManifolds{};
		contact.collisionAlgorithm.detectCollisions(&arena, colliderA, predictedTransformationA, colliderB, predictedTransformationB, simulationOptions.collisionAlgorithmOptions,
			CollisionFilterTest{}, [&](const CollisionAlgorithmResult<N>& collision) -> void {
				GREM_ASSERT(!collision.manifold.points.empty());
				contact.insertCollision(mergedManifolds, collision, predictedTransformationA, predictedTransformationB, linearVelocityA, linearVelocityB, angularVelocityA,
					angularVelocityB, maxCollisionTouchingDistance, minContactManifoldDotProduct);
			});
		contact.removeManifolds(~mergedManifolds); // Remove old manifolds that weren't merged.
	}
}

template <size_t N>
void removeGhostContactManifolds(execution::Entities<const Collider<N>, const Position<N>, const Orientation<N>, const Scale<N>, const ObjectActivity> objectEntities,
	execution::Entities<const Collider<N>, const Position<N>, const Orientation<N>, const Scale<N>, const ObjectContacts<N>, const ObjectActiveTag> activeObjectEntities,
	Contacts<N>& contacts, ContactManifoldInvalidation<N>& contactManifoldInvalidation, const SimulationOptions<N>& simulationOptions) {
	// Prevent ghost collisions with internal edges/vertices.
	// Reference: Pierre Terdiman: "Contact generation for meshes": https://www.codercorner.com/MeshContacts.pdf

	using DelayedContactManifold = typename ContactManifoldInvalidation<N>::DelayedContactManifold;

	const Time stepInterval = simulationOptions.stepInterval;
	const SquaredDistance pointDistanceToleranceSquared = length2(simulationOptions.collisionAlgorithmOptions.maxCollisionTouchingDistance);

	for (auto&& [objectID, collider, position, orientation, scale, objectContacts, activeTag] : activeObjectEntities) {
		contactManifoldInvalidation.voidedPoints.clear();
		contactManifoldInvalidation.facePoints.clear();
		contactManifoldInvalidation.delayedContactManifolds.clear();
		contactManifoldInvalidation.affectedContacts.clear();

		for (const EntityID otherObjectID : objectContacts.otherObjectIDs) {
			auto&& [otherEntityID, otherCollider, otherPosition, otherOrientation, otherScale, otherActivity] = objectEntities[otherObjectID];
			if (!otherCollider.shape.template is<TriangleMeshShape<N>>() && (!otherCollider.shape.isConvexPolytopeShapeType() || otherActivity.isCorrectable != 0)) {
				continue;
			}

			const Transformation<N> transformation = translateRotateScale(position, orientation, scale);
			const Transformation<N> otherTransformation = translateRotateScale(otherPosition, otherOrientation, otherScale);

			const Transformation<N>& transformationA = (objectID < otherObjectID) ? transformation : otherTransformation;
			const Transformation<N>& transformationB = (objectID < otherObjectID) ? otherTransformation : transformation;

			const ContactIndex contactIndex = static_cast<ContactIndex>(contacts.find(minmax(objectID, otherObjectID)).getIndex());
			auto&& [objectIDs, contact] = contacts.getAtIndex(contactIndex);
			for (size_t manifoldIndex = 0; manifoldIndex < contact.manifolds.size(); ++manifoldIndex) {
				ContactManifold<N>& manifold = contact.manifolds[manifoldIndex];
				GREM_ASSERT(!manifold.points.empty());

				Length1D largestPenetrationDepth = Length1D::MIN;
				Position<N> averageContactPoint{};
				for (const ContactPoint<N>& point : manifold.points) {
					const Position<N> pointA = transformationA(point.localOffsets.first);
					const Position<N> pointB = transformationB(point.localOffsets.second);
					const Length1D penetrationDepth = dot(pointA - pointB, manifold.normal);
					largestPenetrationDepth = max(largestPenetrationDepth, penetrationDepth);
					averageContactPoint += midpoint(pointA, pointB) - 0;
				}
				averageContactPoint *= 1.0f / static_cast<float>(manifold.points.size());

				const size_t facePointOffset = contactManifoldInvalidation.facePoints.size();
				InplaceArrayList<Position<N>, 2> featurePoints{};
				const ContactFeatureType otherContactFeatureType = (objectID == objectIDs.first) ? manifold.featureTypes.second : manifold.featureTypes.first;
				switch (otherContactFeatureType) {
					case ContactFeatureType::GENERIC_CONVEX_SURFACE: [[fallthrough]];
					case ContactFeatureType::CONVEX_POLYTOPE_FACE: [[fallthrough]];
					case ContactFeatureType::CONVEX_POLYTOPE_EDGE: {
						GREM_ASSERT(otherCollider.shape.isConvexPolytopeShapeType());
						const InverseTransformation<N> otherInverseTransformation = inverseTranslateRotateScale(otherPosition, otherOrientation, otherScale);
						const Length<N> otherLocalOffset = otherInverseTransformation(position);
						const Direction<N> otherLocalNormal = otherInverseTransformation.getDirection(flipSignIf(manifold.normal, objectID == objectIDs.first));
						const Direction<N> otherLocalDirection = tryNormalize(otherLocalOffset).value_or(otherLocalNormal);
						const ConvexPolytopeShapeView<N> otherConvexPolytopeShape{otherCollider.shape};
						const ConvexPolytopeFaceIndex otherFaceIndex = otherConvexPolytopeShape.getFaceIndexWithMostFittingLocalNormal(otherLocalDirection, 0);
						forEachVertexIndexInFace(otherConvexPolytopeShape, otherFaceIndex, [&](ConvexPolytopeVertexIndex otherVertexIndex) -> void { //
							contactManifoldInvalidation.facePoints.push_back(otherTransformation(otherConvexPolytopeShape.getLocalVertexOffset(otherVertexIndex)));
						});
						if constexpr (N == 3) {
							if (abs(dot(otherLocalNormal, otherConvexPolytopeShape.getLocalFaceNormal(otherFaceIndex))) < cos(1.0f * DEGREES)) {
								featurePoints = findClosestFeaturePoints<3>(Span{contactManifoldInvalidation.facePoints}.subspan(facePointOffset), averageContactPoint);
							}
						} else {
							featurePoints = findClosestFeaturePoints<2>(Span{contactManifoldInvalidation.facePoints}.subspan(facePointOffset), averageContactPoint);
						}
						break;
					}
					case ContactFeatureType::TRIANGLE_MESH_FACE: {
						const TriangleMesh<N>& otherMesh = *otherCollider.shape.template as<TriangleMeshShape<N>>().getTriangleMesh();
						const Span<const TriangleMeshVertex<N>> otherVertices = otherMesh.getVertices();
						const Span<const TriangleMeshVertexIndex> otherIndices = otherMesh.getIndices();
						const TriangleMeshVertexIndex otherFaceIndex =
							static_cast<TriangleMeshVertexIndex>((objectID < otherObjectID) ? manifold.featureIndices.second : manifold.featureIndices.first);
						const size_t otherIndexOffset = static_cast<size_t>(otherFaceIndex) * 3;

						const Array otherLocalTrianglePointOffsets{
							otherVertices[otherIndices[otherIndexOffset + 0]] * Length<N>::UNIT,
							otherVertices[otherIndices[otherIndexOffset + 1]] * Length<N>::UNIT,
							otherVertices[otherIndices[otherIndexOffset + 2]] * Length<N>::UNIT,
						};

						const Array otherTrianglePoints{
							otherTransformation(otherLocalTrianglePointOffsets[0]),
							otherTransformation(otherLocalTrianglePointOffsets[1]),
							otherTransformation(otherLocalTrianglePointOffsets[2]),
						};

						if constexpr (N == 3) {
							const Length<N> ab = otherTrianglePoints[1] - otherTrianglePoints[0];
							const Length<N> ac = otherTrianglePoints[2] - otherTrianglePoints[0];
							const Direction<N> normal = normalize(cross(ab, ac));
							if (abs(dot(manifold.normal, normal)) < cos(1.0f * DEGREES)) {
								featurePoints = findClosestFeaturePoints<3>(otherTrianglePoints, averageContactPoint);
							}
						} else {
							featurePoints = findClosestFeaturePoints<2>(otherTrianglePoints, averageContactPoint);
						}

						for (const Position<N>& otherPoint : otherTrianglePoints) {
							contactManifoldInvalidation.facePoints.push_back(otherPoint);
						}
						break;
					}
					case ContactFeatureType::TRIANGLE_MESH_EDGE:
						if constexpr (N == 3) {
							const TriangleMesh<N>& otherMesh = *otherCollider.shape.template as<TriangleMeshShape<N>>().getTriangleMesh();
							const Span<const TriangleMeshVertex<N>> otherVertices = otherMesh.getVertices();
							const Span<const TriangleMeshVertexIndex> otherIndices = otherMesh.getIndices();
							const uint32_t otherFirstVertexIndexIndex =
								static_cast<uint32_t>((objectID < otherObjectID) ? manifold.featureIndices.second : manifold.featureIndices.first);
							const size_t otherIndexOffset = static_cast<size_t>((otherFirstVertexIndexIndex / 3) * 3);
							const uint32_t otherSecondVertexIndexIndex = static_cast<uint32_t>(otherIndexOffset + (otherFirstVertexIndexIndex + 1) % 3);
							const Length<N> otherLocalEdgeOrigin = otherVertices[otherIndices[otherFirstVertexIndexIndex]] * Length<N>::UNIT;
							const Length<N> otherLocalEdgeTarget = otherVertices[otherIndices[otherSecondVertexIndexIndex]] * Length<N>::UNIT;

							const Position<N> otherEdgeOrigin = otherTransformation(otherLocalEdgeOrigin);
							const Position<N> otherEdgeTarget = otherTransformation(otherLocalEdgeTarget);
							const Coefficient t = dot(position - otherEdgeOrigin, otherEdgeTarget - otherEdgeOrigin) / distance2(otherEdgeOrigin, otherEdgeTarget);
							if (t <= Coefficient{0}) {
								featurePoints = {otherEdgeOrigin};
							} else if (t >= Coefficient{1}) {
								featurePoints = {otherEdgeTarget};
							} else {
								featurePoints = {otherEdgeOrigin, otherEdgeTarget};
							}

							for (size_t i = 0; i < 3; ++i) {
								const Length<N> otherLocalPointOffset = otherVertices[otherIndices[otherIndexOffset + i]] * Length<N>::UNIT;
								contactManifoldInvalidation.facePoints.push_back(otherTransformation(otherLocalPointOffset));
							}
						} else {
							unreachable();
						}
						break;
				}

				if (featurePoints.empty()) {
					for (const Position<N>& point : Span{contactManifoldInvalidation.facePoints}.subspan(facePointOffset)) {
						contactManifoldInvalidation.voidedPoints.push_back(point);
					}
				} else {
					contactManifoldInvalidation.delayedContactManifolds.push_back(DelayedContactManifold{
						.contactIndex = contactIndex,
						.manifoldIndex = static_cast<uint32_t>(manifoldIndex),
						.featurePoints = featurePoints,
						.facePointOffset = static_cast<uint32_t>(facePointOffset),
						.facePointCount = static_cast<uint32_t>(contactManifoldInvalidation.facePoints.size() - facePointOffset),
						.largestPenetrationDepth = largestPenetrationDepth,
					});
				}
			}
		}

		stableSortByDescending<&DelayedContactManifold::largestPenetrationDepth>(contactManifoldInvalidation.delayedContactManifolds);

		const auto isVoidedFeature = [&](const InplaceArrayList<Position<N>, 2>& featurePoints) -> bool {
			GREM_ASSERT(!featurePoints.empty());
			return allOf(featurePoints, [&](const Position<N>& featurePoint) -> bool {
				return anyOf(contactManifoldInvalidation.voidedPoints,
					[&](const Position<N>& voidedPoint) -> bool { return distance2(featurePoint, voidedPoint) <= pointDistanceToleranceSquared; });
			});
		};

		for (const DelayedContactManifold& delayedContactManifold : contactManifoldInvalidation.delayedContactManifolds) {
			if (isVoidedFeature(delayedContactManifold.featurePoints)) {
				ContactManifold<N>& manifold = contacts.getAtIndex(delayedContactManifold.contactIndex).second.manifolds[delayedContactManifold.manifoldIndex];
				if (!manifold.points.empty()) {
					manifold.points.clear();
					contactManifoldInvalidation.affectedContacts.push_back(delayedContactManifold.contactIndex);
				}
			}

			for (const Position<N>& point : Span{contactManifoldInvalidation.facePoints}.subspan(delayedContactManifold.facePointOffset, delayedContactManifold.facePointCount)) {
				contactManifoldInvalidation.voidedPoints.push_back(point);
			}
		}

		for (const ContactIndex contactIndex : contactManifoldInvalidation.affectedContacts) {
			erase_if(contacts.getAtIndex(contactIndex).second.manifolds, [](const ContactManifold<N>& manifold) -> bool { return manifold.points.empty(); });
		}
	}
}

template <size_t N>
void emitCollisionAndSeparationEvents(
	execution::Entities<const ObjectFlags, const Position<N>, const Orientation<N>, const Scale<N>, const LinearVelocity<N>, const AngularVelocity<N>> objectEntities,
	CollisionEvents<N>& collisionEvents, SeparationEvents<N>& separationEvents, const Contacts<N>& contacts, const ActiveContactList& activeContactList) {
	for (const ContactIndex contactIndex : activeContactList) {
		const auto& [objectIDs, contact] = contacts.getAtIndex(contactIndex);
		if (contact.wasTouchingOnLastSubStep == !contact.manifolds.empty()) {
			continue;
		}

		auto&& [objectIDA, flagsA, positionA, orientationA, scaleA, linearVelocityA, angularVelocityA] = objectEntities[objectIDs.first];
		auto&& [objectIDB, flagsB, positionB, orientationB, scaleB, linearVelocityB, angularVelocityB] = objectEntities[objectIDs.second];
		if (contact.manifolds.empty()) {
			if (flagsA.emitsSeparationEvents || flagsB.emitsSeparationEvents) {
				const Transformation<N> transformationA = translateRotateScale(positionA, orientationA, scaleA);
				const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
				separationEvents.push_back(SeparationEvent<N>{
					.objectIDs{objectIDA, objectIDB},
					.objectTransformations{transformationA, transformationB},
					.objectLinearVelocities{linearVelocityA, linearVelocityB},
					.objectAngularVelocities{angularVelocityA, angularVelocityB},
				});
			}
		} else if (flagsA.emitsCollisionEvents || flagsB.emitsCollisionEvents) {
			const Transformation<N> transformationA = translateRotateScale(positionA, orientationA, scaleA);
			const Transformation<N> transformationB = translateRotateScale(positionB, orientationB, scaleB);
			collisionEvents.push_back(CollisionEvent<N>{
				.objectIDs{objectIDA, objectIDB},
				.objectTransformations{transformationA, transformationB},
				.localOffsets = contact.manifolds.front().points.front().localOffsets,
				.objectLinearVelocities{linearVelocityA, linearVelocityB},
				.objectAngularVelocities{angularVelocityA, angularVelocityB},
				.normal = contact.manifolds.front().normal,
			});
		}
	}
}

template <size_t N>
void activateSeparatedObjects(execution::Entities<ObjectActivity, const ObjectFlags> objectEntities, execution::ComponentPool<ObjectActiveTag> objectActiveTags,
	const Contacts<N>& contacts, const ActiveContactList& activeContactList) {
	for (const ContactIndex contactIndex : activeContactList) {
		const auto& [objectIDs, contact] = contacts.getAtIndex(contactIndex);
		if (contact.manifolds.empty() && contact.wasTouchingOnLastSubStep) {
			auto&& [objectIDA, activityA, flagsA] = objectEntities[objectIDs.first];
			auto&& [objectIDB, activityB, flagsB] = objectEntities[objectIDs.second];

			if (activityB.energyLevel > activityA.energyLevel) {
				if (activityA.isCorrectable != 0 && flagsA.enableWaking) {
					activityA.energyLevel = activityB.energyLevel;
					objectActiveTags.addOrAssign(objectIDA, ObjectActiveTag{});
				}
			} else if (activityA.energyLevel > activityB.energyLevel) {
				if (activityB.isCorrectable != 0 && flagsB.enableWaking) {
					activityB.energyLevel = activityA.energyLevel;
					objectActiveTags.addOrAssign(objectIDB, ObjectActiveTag{});
				}
			}
		}
	}
}

template <size_t N>
void updateActiveJointSet(execution::Entities<> allEntities, execution::Entities<const JointConnectedObjects> jointEntities,
	execution::Entities<const ObjectActiveTag> activeObjectEntities, execution::ComponentPool<JointActiveTag> jointActiveTags) {
	for (auto&& [jointID, connectedObjects] : jointEntities) {
		if (allEntities.containsEntity(connectedObjects.first) && allEntities.containsEntity(connectedObjects.second) &&
			(activeObjectEntities.containsEntity(connectedObjects.first) || activeObjectEntities.containsEntity(connectedObjects.second))) {
			jointActiveTags.addOrAssign(jointID, JointActiveTag{});
		} else {
			jointActiveTags.remove(jointID);
		}
	}
}

template <size_t N>
void integrateLinearVelocities(execution::Entities<LinearVelocity<N>, const Orientation<N>, const Scale<N>, const InverseMass, const Force<N>, const LinearAcceleration<N>,
								   const FluidDensity, const Volume, const Material, const Collider<N>, const ObjectActiveTag>
								   activeObjectEntities,
	const SimulationOptions<N>& simulationOptions) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);

	for (auto&& [objectID, linearVelocity, orientation, scale, inverseMass, externalForce, gravityAcceleration, fluidDensity, volume, material, collider, activeTag] :
		activeObjectEntities) {
		const Force<N> buoyancyForce = fluidDensity * gravityAcceleration * volume * product(scale);
		LinearVelocity<N> addedLinearVelocity = (inverseMass * (externalForce - buoyancyForce) + gravityAcceleration) * deltaTime;

		const LinearVelocity<N> newLinearVelocity = linearVelocity + addedLinearVelocity;
		const SquaredSpeed newSquaredSpeed = length2(newLinearVelocity);
		const Speed newSpeed = sqrt(newSquaredSpeed);
		if (newSpeed > 0) {
			const Direction<N> direction = Direction<N>::reinterpret(newLinearVelocity / newSpeed);
			if (const Optional<Area> referenceArea = ShapeView<N>{collider.shape}.getReferenceArea(rotateScale(orientation, scale), direction)) {
				const Force1D dragForce = -0.5f * Density{fluidDensity} * newSquaredSpeed * material.linearDrag * *referenceArea;
				const LinearVelocity1D speedDelta = inverseMass * dragForce * deltaTime;
				if (newSpeed + speedDelta <= 0) {
					addedLinearVelocity = -linearVelocity;
				} else {
					addedLinearVelocity += direction * speedDelta;
				}
			}
		}

		linearVelocity += addedLinearVelocity;
		GREM_ASSERT(all(isfinite(linearVelocity)));
	}
}

template <size_t N>
void integrateAngularVelocities(
	execution::Entities<AngularVelocity<N>, const Orientation<N>, const Scale<N>, const MomentOfInertiaTensor<N>, const InverseMomentOfInertiaTensor<N>, const Torque<N>,
		const LinearAcceleration<N>, const FluidDensity, const CenterOfBuoyancy<N>, const Volume, const Material, const Collider<N>, const ObjectActiveTag>
		activeObjectEntities,
	const SimulationOptions<N>& simulationOptions) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);

	for (auto&& [objectID, angularVelocity, orientation, scale, momentOfInertiaTensor, inverseMomentOfInertiaTensor, externalTorque, gravityAcceleration, fluidDensity,
			 centerOfBuoyancy, volume, material, collider, activeTag] : activeObjectEntities) {
		const Force<N> buoyancyForce = fluidDensity * gravityAcceleration * volume * product(scale);
		const Torque<N> buoyancyTorque = cross(centerOfBuoyancy, buoyancyForce);
		AngularVelocity<N> addedAngularVelocity =
			inverseMomentOfInertiaTensor * (externalTorque - buoyancyTorque - cross(angularVelocity, momentOfInertiaTensor * angularVelocity)) * deltaTime;

		const AngularVelocity<N> newAngularVelocity = angularVelocity + addedAngularVelocity;
		const SquaredAngularSpeed newSquaredAngularSpeed = length2(newAngularVelocity);
		const AngularSpeed newAngularSpeed = sqrt(newSquaredAngularSpeed);
		if (newAngularSpeed > 0) {
			if (const Optional<Distance> effectiveRadius = ShapeView<N>{collider.shape}.getBoundingRadius(rotateScale(orientation, scale))) {
				const AngularScale<N> axis = newAngularVelocity / newAngularSpeed;
				// This "drag torque" approximation is kinda bogus, but it's probably better than nothing. :)
				const Torque2D dragTorque = -0.5f * Density{fluidDensity} * newSquaredAngularSpeed * material.angularDrag * length2(length2(*effectiveRadius)) * *effectiveRadius;
				const AngularVelocity<N> angularVelocityDelta = inverseMomentOfInertiaTensor * axis * dragTorque * deltaTime;
				if (dot(newAngularVelocity + angularVelocityDelta, newAngularVelocity) <= 0) {
					addedAngularVelocity = -angularVelocity;
				} else {
					addedAngularVelocity += angularVelocityDelta;
				}
			}
		}

		angularVelocity += addedAngularVelocity;
		GREM_ASSERT(all(isfinite(angularVelocity)));
	}
}

template <size_t N>
void warmstartJointConstraints(
	execution::Entities<JointAngularConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOrientations<N>, const JointActiveTag> angularConstraintJointEntities,
	execution::Entities<JointLinearConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointAttachmentOrientations<N>, const JointActiveTag>
		linearConstraintJointEntities,
	execution::Entities<JointDistanceConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointActiveTag> distanceConstraintJointEntities,
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Position<N>, const Orientation<N>, const Scale<N>, const InverseMass,
		const InverseMomentOfInertiaTensor<N>>
		objectEntities) {
	for (auto&& [jointID, angularConstraintImpulses, connectedObjects, attachmentOrientations, activeTag] : angularConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const AngularImpulse<N> warmstartImpulse = (orientationA * attachmentOrientations.first)(
			angularConstraintImpulses.driveMomentums + angularConstraintImpulses.lowerLimitMomentums - angularConstraintImpulses.upperLimitMomentums);
		if (warmstartImpulse != 0) {
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				angularVelocityA += inverseMomentOfInertiaTensorA * warmstartImpulse;
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				angularVelocityB -= inverseMomentOfInertiaTensorB * warmstartImpulse;
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		angularConstraintImpulses.driveImpulses += angularConstraintImpulses.driveMomentums;
		angularConstraintImpulses.lowerLimitImpulses += angularConstraintImpulses.lowerLimitMomentums;
		angularConstraintImpulses.upperLimitImpulses += angularConstraintImpulses.upperLimitMomentums;
	}

	for (auto&& [jointID, linearConstraintImpulses, connectedObjects, attachmentOffsets, attachmentOrientations, activeTag] : linearConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const Length<N> offsetA = orientationA(scaleA * attachmentOffsets.first);
		const Length<N> offsetB = orientationB(scaleB * attachmentOffsets.second);

		const LinearImpulse<N> warmstartImpulse = (orientationA * attachmentOrientations.first)(
			linearConstraintImpulses.driveMomentums + linearConstraintImpulses.lowerLimitMomentums - linearConstraintImpulses.upperLimitMomentums);
		if (warmstartImpulse != 0) {
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				linearVelocityA += inverseMassA * warmstartImpulse;
				angularVelocityA += inverseMomentOfInertiaTensorA * cross(offsetA, warmstartImpulse);
				GREM_ASSERT(all(isfinite(linearVelocityA)));
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				linearVelocityB -= inverseMassB * warmstartImpulse;
				angularVelocityB -= inverseMomentOfInertiaTensorB * cross(offsetB, warmstartImpulse);
				GREM_ASSERT(all(isfinite(linearVelocityB)));
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		linearConstraintImpulses.driveImpulses += linearConstraintImpulses.driveMomentums;
		linearConstraintImpulses.lowerLimitImpulses += linearConstraintImpulses.lowerLimitMomentums;
		linearConstraintImpulses.upperLimitImpulses += linearConstraintImpulses.upperLimitMomentums;
	}

	for (auto&& [jointID, distanceConstraintImpulses, connectedObjects, attachmentOffsets, activeTag] : distanceConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const Length<N> offsetA = distanceConstraintImpulses.lastOffsets.first;
		const Length<N> offsetB = distanceConstraintImpulses.lastOffsets.second;

		const Position<N> pointA = positionA + offsetA;
		const Position<N> pointB = positionB + offsetB;
		const Length<N> difference = pointA - pointB;
		const Optional<Direction<N>> normalizedDifference = tryNormalize(difference);
		if (!normalizedDifference) {
			continue;
		}
		const Direction<N> normal = *normalizedDifference;

		const LinearImpulse1D warmstartImpulseAlongNormal =
			distanceConstraintImpulses.driveMomentum + distanceConstraintImpulses.lowerLimitMomentum - distanceConstraintImpulses.upperLimitMomentum;
		if (warmstartImpulseAlongNormal != 0) {
			const LinearImpulse<N> warmstartImpulse = normal * warmstartImpulseAlongNormal;
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				linearVelocityA += inverseMassA * warmstartImpulse;
				angularVelocityA += inverseMomentOfInertiaTensorA * cross(offsetA, warmstartImpulse);
				GREM_ASSERT(all(isfinite(linearVelocityA)));
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				linearVelocityB -= inverseMassB * warmstartImpulse;
				angularVelocityB -= inverseMomentOfInertiaTensorB * cross(offsetB, warmstartImpulse);
				GREM_ASSERT(all(isfinite(linearVelocityB)));
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		distanceConstraintImpulses.driveImpulse += distanceConstraintImpulses.driveMomentum;
		distanceConstraintImpulses.lowerLimitImpulse += distanceConstraintImpulses.lowerLimitMomentum;
		distanceConstraintImpulses.upperLimitImpulse += distanceConstraintImpulses.upperLimitMomentum;
	}
}

void warmstart3DOnlyJointConstraints(
	execution::Entities<JointConeConstraintImpulses3D, const JointConnectedObjects, const JointAttachmentOrientations3D, const JointActiveTag> coneConstraintJointEntities,
	execution::Entities<JointTwistConstraintImpulses3D, const JointConnectedObjects, const JointAttachmentOrientations3D, const JointActiveTag> twistConstraintJointEntities,
	execution::Entities<ObjectActivity, LinearVelocity3D, AngularVelocity3D, const Position3D, const Orientation3D, const Scale3D, const InverseMass,
		const InverseMomentOfInertiaTensor3D>
		objectEntities) {
	for (auto&& [jointID, coneConstraintImpulses, connectedObjects, attachmentOrientations, activeTag] : coneConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const OrthonormalBasis3D basisA = rotate(orientationA * attachmentOrientations.first);
		const OrthonormalBasis3D basisB = rotate(orientationB * attachmentOrientations.second);

		const Scale3D axisVector = cross(basisA[X], basisB[X]);
		const Optional<Direction3D> normalizedAxisVector = tryNormalize(axisVector);
		if (!normalizedAxisVector) {
			continue;
		}
		const Direction3D axis = *normalizedAxisVector;

		const AngularImpulse2D warmstartImpulseAroundAxis = coneConstraintImpulses.lowerLimitMomentum - coneConstraintImpulses.upperLimitMomentum;
		if (warmstartImpulseAroundAxis != 0) {
			const AngularImpulse3D warmstartImpulse = axis * warmstartImpulseAroundAxis;
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				angularVelocityA += inverseMomentOfInertiaTensorA * warmstartImpulse;
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				angularVelocityB -= inverseMomentOfInertiaTensorB * warmstartImpulse;
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		coneConstraintImpulses.lowerLimitImpulse += coneConstraintImpulses.lowerLimitMomentum;
		coneConstraintImpulses.upperLimitImpulse += coneConstraintImpulses.upperLimitMomentum;
	}

	for (auto&& [jointID, twistConstraintImpulses, connectedObjects, attachmentOrientations, activeTag] : twistConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const OrthonormalBasis3D basisA = rotate(orientationA * attachmentOrientations.first);
		const OrthonormalBasis3D basisB = rotate(orientationB * attachmentOrientations.second);

		const Scale3D axisVector = basisA[X] + basisB[X];
		const Optional<Direction3D> normalizedAxisVector = tryNormalize(axisVector);
		if (!normalizedAxisVector) {
			continue;
		}
		const Direction3D axis = *normalizedAxisVector;

		const AngularImpulse2D warmstartImpulseAroundAxis =
			twistConstraintImpulses.driveMomentum + twistConstraintImpulses.lowerLimitMomentum - twistConstraintImpulses.upperLimitMomentum;
		if (warmstartImpulseAroundAxis != 0) {
			const AngularImpulse3D warmstartImpulse = axis * warmstartImpulseAroundAxis;
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				angularVelocityA += inverseMomentOfInertiaTensorA * warmstartImpulse;
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				angularVelocityB -= inverseMomentOfInertiaTensorB * warmstartImpulse;
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		twistConstraintImpulses.driveImpulse += twistConstraintImpulses.driveMomentum;
		twistConstraintImpulses.lowerLimitImpulse += twistConstraintImpulses.lowerLimitMomentum;
		twistConstraintImpulses.upperLimitImpulse += twistConstraintImpulses.upperLimitMomentum;
	}
}

template <size_t N>
void warmstartContactConstraintsImplementation(
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const InverseMass,
		const InverseMomentOfInertiaTensor<N>>
		objectEntities,
	Contacts<N>& contacts, Span<const ContactIndex> contactIndices) {
	for (const ContactIndex contactIndex : contactIndices) {
		auto&& [objectIDs, contact] = contacts.getAtIndex(contactIndex);

		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, orientationA, scaleA, colliderA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[objectIDs.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, orientationB, scaleB, colliderB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[objectIDs.second];

		for (ContactManifold<N>& manifold : contact.manifolds) {
			if (!manifold.filterTestResult.respondsToCollision()) {
				continue;
			}

			const Direction<N> normal = manifold.normal;
			for (ContactPoint<N>& point : manifold.points) {
				if (point.momentumInTangentSpace != 0) {
					const OrthonormalBasis<N> tangentSpaceBasis = point.getTangentSpaceBasis(normal);
					const Length<N> offsetA = point.offsets.first;
					const Length<N> offsetB = point.offsets.second;

					const LinearImpulse<N> warmstartImpulse = tangentSpaceBasis * point.momentumInTangentSpace;
					if (activityA.isCorrectable != 0) {
						activityA.wasCorrected = 1;
						linearVelocityA += inverseMassA * warmstartImpulse;
						angularVelocityA += inverseMomentOfInertiaTensorA * cross(offsetA, warmstartImpulse);
						GREM_ASSERT(all(isfinite(linearVelocityA)));
						GREM_ASSERT(all(isfinite(angularVelocityA)));
					}
					if (activityB.isCorrectable != 0) {
						activityB.wasCorrected = 1;
						linearVelocityB -= inverseMassB * warmstartImpulse;
						angularVelocityB -= inverseMomentOfInertiaTensorB * cross(offsetB, warmstartImpulse);
						GREM_ASSERT(all(isfinite(linearVelocityB)));
						GREM_ASSERT(all(isfinite(angularVelocityB)));
					}
					point.impulseInTangentSpace += point.momentumInTangentSpace;
				}
			}

			if (manifold.rollingResistanceMomentum != 0) {
				const AngularImpulse<N> warmstartImpulse = manifold.rollingResistanceMomentum;
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					angularVelocityA += inverseMomentOfInertiaTensorA * warmstartImpulse;
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					angularVelocityB -= inverseMomentOfInertiaTensorB * warmstartImpulse;
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
				manifold.rollingResistanceImpulse += manifold.rollingResistanceMomentum;
			}
		}
	}
}

template <size_t N, size_t ColorIndex>
void warmstartColoredContactConstraints(
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const InverseMass,
		const InverseMomentOfInertiaTensor<N>>
		objectEntities,
	Contacts<N>& contacts, execution::Chunk<const ContactColorGraph<N>, ContactColorGraph<N>::template getColoredContactIndices<ColorIndex>> coloredContacts) {
	warmstartContactConstraintsImplementation<N>(objectEntities, contacts, coloredContacts);
}

template <size_t N>
void warmstartOverflowContactConstraints(
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const InverseMass,
		const InverseMomentOfInertiaTensor<N>>
		objectEntities,
	Contacts<N>& contacts, const ContactColorOverflow& contactColorOverflow) {
	warmstartContactConstraintsImplementation<N>(objectEntities, contacts, contactColorOverflow);
}

template <size_t N>
void applyJointDrives(execution::Entities<JointLinearConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointAttachmentOrientations<N>,
						  const JointLinearConstraint<N>, const JointActiveTag>
						  linearConstraintJointEntities,
	execution::Entities<JointDistanceConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointAttachmentOrientations<N>,
		const JointDistanceConstraint<N>, const JointActiveTag>
		distanceConstraintJointEntities,
	execution::Entities<JointAngularConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOrientations<N>, const JointAngularConstraint<N>, const JointActiveTag>
		angularConstraintJointEntities,
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Position<N>, const Orientation<N>, const Scale<N>, const InverseMass,
		const InverseMomentOfInertiaTensor<N>>
		objectEntities,
	const SimulationOptions<N>& simulationOptions) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);

	for (auto&& [jointID, angularConstraintImpulses, connectedObjects, attachmentOrientations, angularConstraint, activeTag] : angularConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const InverseMomentOfInertiaTensor<N> combinedInverseEffectiveMomentOfInertiaTensor = inverseMomentOfInertiaTensorA + inverseMomentOfInertiaTensorB;
		const MomentOfInertiaTensor<N> combinedEffectiveMomentOfInertiaTensor =
			(combinedInverseEffectiveMomentOfInertiaTensor == 0) ? MomentOfInertiaTensor<N>{0.0f * KILOGRAM_SQUARE_METERS} : inverse(combinedInverseEffectiveMomentOfInertiaTensor);

		const Orientation<N> referenceFrame = orientationA * attachmentOrientations.first;

		const AngularVelocity<N> relativeAngularVelocity = angularVelocityA - angularVelocityB;
		const AngularVelocity<N> relativeAngularVelocityInReferenceFrame = inverse(referenceFrame)(relativeAngularVelocity);
		const AngularVelocity<N> velocityError = relativeAngularVelocityInReferenceFrame - angularConstraint.driveTargetVelocities;
		const AngularImpulse<N> desiredImpulse = combinedEffectiveMomentOfInertiaTensor * -velocityError;

		const AngularMomentum<N> momentum = angularConstraintImpulses.driveMomentums;
		const AngularMomentum<N> maxMomentum = min(angularConstraint.driveMaxTorques * deltaTime, AngularMomentum<N>{simulationOptions.jointMaxAngularMomentum});
		const AngularMomentum<N> newMomentum = clamp(momentum + desiredImpulse, -maxMomentum, maxMomentum);
		const AngularImpulse<N> impulseInReferenceFrame = newMomentum - momentum;

		if (impulseInReferenceFrame != 0) {
			const AngularImpulse<N> angularImpulse = referenceFrame(impulseInReferenceFrame);
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				angularVelocityA += inverseMomentOfInertiaTensorA * angularImpulse;
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				angularVelocityB -= inverseMomentOfInertiaTensorB * angularImpulse;
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		angularConstraintImpulses.driveMomentums = newMomentum;
		angularConstraintImpulses.driveImpulses += impulseInReferenceFrame;
	}

	for (auto&& [jointID, linearConstraintImpulses, connectedObjects, attachmentOffsets, attachmentOrientations, linearConstraint, activeTag] : linearConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const Length<N> offsetA = orientationA(scaleA * attachmentOffsets.first);
		const Length<N> offsetB = orientationB(scaleB * attachmentOffsets.second);
		const Position<N> pointA = positionA + offsetA;
		const Position<N> pointB = positionB + offsetB;

		const OrthonormalBasis<N> basis = rotate(orientationA * attachmentOrientations.first);
		const OrthonormalBasis<N> inverseBasis = transpose(basis);
		const Length<N> offsetsInReferenceFrame = inverseBasis * (pointA - pointB);

		const MomentArmTensor<N> momentArmsA = cross(offsetA, basis);
		const MomentArmTensor<N> momentArmsB = cross(offsetB, basis);
		const AngularQuantity<N, InverseMass::Unit> inverseEffectiveMassesA = inverseMassA + dot(momentArmsA, inverseMomentOfInertiaTensorA * momentArmsA);
		const AngularQuantity<N, InverseMass::Unit> inverseEffectiveMassesB = inverseMassB + dot(momentArmsB, inverseMomentOfInertiaTensorB * momentArmsB);
		const AngularQuantity<N, InverseMass::Unit> combinedInverseEffectiveMasses = inverseEffectiveMassesA + inverseEffectiveMassesB;
		const AngularQuantity<N, Mass::Unit> combinedEffectiveMasses =
			select(equal(combinedInverseEffectiveMasses, 0), AngularQuantity<N, Mass::Unit>{}, inverse(combinedInverseEffectiveMasses));

		const LinearVelocity<N> relativeVelocity = (linearVelocityA + cross(offsetA, angularVelocityA)) - (linearVelocityB + cross(offsetB, angularVelocityB));
		const LinearVelocity<N> relativeVelocityInReferenceFrameA = inverseBasis * relativeVelocity;
		const LinearVelocity<N> velocityError = relativeVelocityInReferenceFrameA - linearConstraint.driveTargetVelocities;
		const LinearImpulse<N> desiredImpulse = combinedEffectiveMasses * -velocityError;

		const LinearMomentum<N> momentum = linearConstraintImpulses.driveMomentums;
		const LinearMomentum<N> maxMomentum = min(linearConstraint.driveMaxForces * deltaTime, LinearMomentum<N>{simulationOptions.jointMaxMomentum});
		const LinearMomentum<N> newMomentum = clamp(momentum + desiredImpulse, -maxMomentum, maxMomentum);
		const LinearImpulse<N> impulseInReferenceFrame = newMomentum - momentum;

		if (impulseInReferenceFrame != 0) {
			const LinearImpulse<N> linearImpulse = basis * impulseInReferenceFrame;
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				linearVelocityA += inverseMassA * linearImpulse;
				angularVelocityA += inverseMomentOfInertiaTensorA * cross(offsetA, linearImpulse);
				GREM_ASSERT(all(isfinite(linearVelocityA)));
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				linearVelocityB -= inverseMassB * linearImpulse;
				angularVelocityB -= inverseMomentOfInertiaTensorB * cross(offsetB, linearImpulse);
				GREM_ASSERT(all(isfinite(linearVelocityB)));
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		linearConstraintImpulses.driveMomentums = newMomentum;
		linearConstraintImpulses.driveImpulses += impulseInReferenceFrame;
	}

	for (auto&& [jointID, distanceConstraintImpulses, connectedObjects, attachmentOffsets, attachmentOrientations, distanceConstraint, activeTag] :
		distanceConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const Length<N> offsetA = orientationA(scaleA * attachmentOffsets.first);
		const Length<N> offsetB = orientationB(scaleB * attachmentOffsets.second);
		const Position<N> pointA = positionA + offsetA;
		const Position<N> pointB = positionB + offsetB;
		const Length<N> difference = pointA - pointB;
		const Optional<Direction<N>> normalizedDifference = tryNormalize(difference);
		if (!normalizedDifference) {
			continue;
		}
		const Direction<N> normal = *normalizedDifference;

		const MomentArm<N> momentArmA = cross(offsetA, normal);
		const MomentArm<N> momentArmB = cross(offsetB, normal);
		const InverseMass inverseEffectiveMassA = inverseMassA + dot(momentArmA, inverseMomentOfInertiaTensorA * momentArmA);
		const InverseMass inverseEffectiveMassB = inverseMassB + dot(momentArmB, inverseMomentOfInertiaTensorB * momentArmB);
		const InverseMass combinedInverseEffectiveMass = inverseEffectiveMassA + inverseEffectiveMassB;
		const Mass combinedEffectiveMass = (combinedInverseEffectiveMass == 0) ? Mass{} : inverse(combinedInverseEffectiveMass);

		const LinearVelocity1D relativeNormalVelocity = dot(normal, (linearVelocityA + cross(offsetA, angularVelocityA)) - (linearVelocityB + cross(offsetB, angularVelocityB)));
		const LinearVelocity1D velocityError = relativeNormalVelocity - distanceConstraint.driveTargetVelocity;
		const LinearImpulse1D desiredImpulse = combinedEffectiveMass * -velocityError;

		const LinearMomentum1D momentum = distanceConstraintImpulses.driveMomentum;
		const LinearMomentum1D maxMomentum = min(distanceConstraint.driveMaxForce * deltaTime, simulationOptions.jointMaxMomentum);
		const LinearMomentum1D newMomentum = clamp(momentum + desiredImpulse, -maxMomentum, maxMomentum);
		const LinearImpulse1D impulse = newMomentum - momentum;

		if (impulse != 0) {
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				linearVelocityA += normal * (inverseMassA * impulse);
				angularVelocityA += inverseMomentOfInertiaTensorA * (momentArmA * impulse);
				GREM_ASSERT(all(isfinite(linearVelocityA)));
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				linearVelocityB -= normal * (inverseMassB * impulse);
				angularVelocityB -= inverseMomentOfInertiaTensorB * (momentArmB * impulse);
				GREM_ASSERT(all(isfinite(linearVelocityB)));
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		distanceConstraintImpulses.driveMomentum = newMomentum;
		distanceConstraintImpulses.driveImpulse += impulse;
	}
}

void apply3DOnlyJointDrives(
	execution::Entities<JointTwistConstraintImpulses3D, const JointConnectedObjects, const JointAttachmentOrientations3D, const JointTwistConstraint3D, const JointActiveTag>
		twistConstraintJointEntities,
	execution::Entities<ObjectActivity, AngularVelocity3D, const Orientation3D, const InverseMass, const InverseMomentOfInertiaTensor3D> objectEntities,
	const SimulationOptions3D& simulationOptions) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);

	for (auto&& [jointID, twistConstraintImpulses, connectedObjects, attachmentOrientations, twistConstraint, activeTag] : twistConstraintJointEntities) {
		auto&& [objectIDA, activityA, angularVelocityA, orientationA, inverseMassA, inverseMomentOfInertiaTensorA] = objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, angularVelocityB, orientationB, inverseMassB, inverseMomentOfInertiaTensorB] = objectEntities[connectedObjects.second];

		const OrthonormalBasis3D basisA = rotate(orientationA * attachmentOrientations.first);
		const OrthonormalBasis3D basisB = rotate(orientationB * attachmentOrientations.second);

		const Scale3D axisVector = basisA[X] + basisB[X];
		const Optional<Direction3D> normalizedAxisVector = tryNormalize(axisVector);
		if (!normalizedAxisVector) {
			continue;
		}
		const Direction3D axis = *normalizedAxisVector;

		const InverseMomentOfInertia2D inverseEffectiveMomentOfInertiaA = dot(axis, inverseMomentOfInertiaTensorA * axis);
		const InverseMomentOfInertia2D inverseEffectiveMomentOfInertiaB = dot(axis, inverseMomentOfInertiaTensorB * axis);
		const InverseMomentOfInertia2D combinedInverseEffectiveMomentOfInertia = inverseEffectiveMomentOfInertiaA + inverseEffectiveMomentOfInertiaB;
		const MomentOfInertia2D combinedEffectiveMomentOfInertia =
			(combinedInverseEffectiveMomentOfInertia == 0) ? MomentOfInertia2D{} : inverse(combinedInverseEffectiveMomentOfInertia);

		const AngularVelocity2D relativeAngularVelocityAroundAxis = dot(axis, angularVelocityA - angularVelocityB);
		const AngularVelocity2D velocityError = relativeAngularVelocityAroundAxis - twistConstraint.driveTargetVelocity;

		const AngularImpulse2D desiredImpulse = combinedEffectiveMomentOfInertia * -velocityError;
		const AngularMomentum2D momentum = twistConstraintImpulses.driveMomentum;
		const AngularMomentum2D maxMomentum = min(twistConstraint.driveMaxTorque * deltaTime, simulationOptions.jointMaxAngularMomentum);
		const AngularMomentum2D newMomentum = clamp(momentum + desiredImpulse, -maxMomentum, maxMomentum);
		const AngularImpulse2D impulseAroundAxis = newMomentum - momentum;

		if (impulseAroundAxis != 0) {
			if (activityA.isCorrectable != 0) {
				activityA.wasCorrected = 1;
				activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				angularVelocityA += inverseMomentOfInertiaTensorA * (axis * impulseAroundAxis);
				GREM_ASSERT(all(isfinite(angularVelocityA)));
			}
			if (activityB.isCorrectable != 0) {
				activityB.wasCorrected = 1;
				activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
				angularVelocityB -= inverseMomentOfInertiaTensorB * (axis * impulseAroundAxis);
				GREM_ASSERT(all(isfinite(angularVelocityB)));
			}
		}
		twistConstraintImpulses.driveMomentum = newMomentum;
		twistConstraintImpulses.driveImpulse += impulseAroundAxis;
	}
}

template <size_t N>
void solveJointConstraintsImplementation(
	execution::Entities<JointAngularConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointAttachmentOrientations<N>,
		const JointAngularConstraint<N>, const JointActiveTag>
		angularConstraintJointEntities,
	execution::Entities<JointLinearConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointAttachmentOrientations<N>,
		const JointLinearConstraint<N>, const JointActiveTag>
		linearConstraintJointEntities,
	execution::Entities<JointDistanceConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointAttachmentOrientations<N>,
		const JointDistanceConstraint<N>, const JointActiveTag>
		distanceConstraintJointEntities,
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Position<N>, const Orientation<N>, const Scale<N>, const InverseMass,
		const InverseMomentOfInertiaTensor<N>>
		objectEntities,
	const SimulationOptions<N>& simulationOptions, bool useBias) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);
	const Frequency inverseDeltaTime = inverse(deltaTime);

	for (auto&& [jointID, angularConstraintImpulses, connectedObjects, attachmentOffsets, attachmentOrientations, angularConstraint, activeTag] : angularConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const Length<N> offsetA = orientationA(scaleA * attachmentOffsets.first);
		const Length<N> offsetB = orientationB(scaleB * attachmentOffsets.second);
		const Position<N> pointA = positionA + offsetA;
		const Position<N> pointB = positionB + offsetB;

		const InverseMomentOfInertiaTensor<N> combinedInverseEffectiveMomentOfInertiaTensor = inverseMomentOfInertiaTensorA + inverseMomentOfInertiaTensorB;
		const MomentOfInertiaTensor<N> combinedEffectiveMomentOfInertiaTensor =
			(combinedInverseEffectiveMomentOfInertiaTensor == 0) ? MomentOfInertiaTensor<N>{0.0f * KILOGRAM_SQUARE_METERS} : inverse(combinedInverseEffectiveMomentOfInertiaTensor);

		const auto [biasRates, massScales, momentumScales] =
			(useBias) ? getSoftConstraintParameters(angularConstraint.limitStiffnesses, angularConstraint.limitDampingRatios, deltaTime, inverseDeltaTime)
					  : SoftConstraintParameters<(N == 3) ? 3 : 1>{};

		const Orientation<N> referenceFrame = orientationA * attachmentOrientations.first;
		const Orientation<N> inverseReferenceFrame = inverse(referenceFrame);
		const Rotation<N> anglesInReferenceFrame = (orientationA * attachmentOrientations.first) - (orientationB * attachmentOrientations.second);

		{
			const Rotation<N> lowerLimitErrors = anglesInReferenceFrame - angularConstraint.minAngles;
			const AngularMask<N> isSpeculativeLowerLimits = greaterThan(lowerLimitErrors, 0);
			const AngularVelocity<N> relativeAngularVelocity = angularVelocityA - angularVelocityB;
			const AngularVelocity<N> relativeAngularVelocityInReferenceFrame = inverseReferenceFrame(relativeAngularVelocity);
			const AngularVelocity<N> lowerLimitErrorGradient = relativeAngularVelocityInReferenceFrame;
			const AngularVelocity<N> lowerLimitErrorBiases = lowerLimitErrors * select(isSpeculativeLowerLimits, AngularFrequency<N>{inverseDeltaTime}, biasRates * massScales);

			const AngularMomentum<N> lowerLimitMomentum = angularConstraintImpulses.lowerLimitMomentums;
			const AngularImpulse<N> desiredLowerLimitImpulse = combinedEffectiveMomentOfInertiaTensor * -(lowerLimitErrorGradient * massScales + lowerLimitErrorBiases) -
			                                                   select(isSpeculativeLowerLimits, AngularImpulse<N>{}, lowerLimitMomentum * momentumScales);

			const AngularMomentum<N> newLowerLimitMomentum =
				clamp(lowerLimitMomentum + desiredLowerLimitImpulse, AngularMomentum<N>{}, AngularMomentum<N>{simulationOptions.jointMaxAngularMomentum});
			const AngularImpulse<N> lowerLimitImpulse = newLowerLimitMomentum - lowerLimitMomentum;
			const AngularImpulse<N> impulseInReferenceFrame = lowerLimitImpulse;

			if (impulseInReferenceFrame != 0) {
				const AngularImpulse<N> angularImpulse = referenceFrame(impulseInReferenceFrame);
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityA += inverseMomentOfInertiaTensorA * angularImpulse;
					GREM_ASSERT(all(isfinite(linearVelocityA)));
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityB -= inverseMomentOfInertiaTensorB * angularImpulse;
					GREM_ASSERT(all(isfinite(linearVelocityB)));
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			angularConstraintImpulses.lowerLimitMomentums = newLowerLimitMomentum;
			angularConstraintImpulses.lowerLimitImpulses += lowerLimitImpulse;
		}

		{
			const Rotation<N> upperLimitErrors = angularConstraint.maxAngles - anglesInReferenceFrame;
			const AngularMask<N> isSpeculativeUpperLimits = greaterThan(upperLimitErrors, 0);
			const AngularVelocity<N> relativeAngularVelocity = angularVelocityA - angularVelocityB;
			const AngularVelocity<N> relativeAngularVelocityInReferenceFrame = inverseReferenceFrame(relativeAngularVelocity);
			const AngularVelocity<N> upperLimitErrorGradient = -relativeAngularVelocityInReferenceFrame;
			const AngularVelocity<N> upperLimitErrorBiases = upperLimitErrors * select(isSpeculativeUpperLimits, AngularFrequency<N>{inverseDeltaTime}, biasRates * massScales);

			const AngularMomentum<N> upperLimitMomentum = angularConstraintImpulses.upperLimitMomentums;
			const AngularImpulse<N> desiredUpperLimitImpulse = combinedEffectiveMomentOfInertiaTensor * -(upperLimitErrorGradient * massScales + upperLimitErrorBiases) -
			                                                   select(isSpeculativeUpperLimits, AngularImpulse<N>{}, upperLimitMomentum * momentumScales);

			const AngularMomentum<N> newUpperLimitMomentum =
				clamp(upperLimitMomentum + desiredUpperLimitImpulse, AngularMomentum<N>{}, AngularMomentum<N>{simulationOptions.jointMaxAngularMomentum});
			const AngularImpulse<N> upperLimitImpulse = newUpperLimitMomentum - upperLimitMomentum;
			const AngularImpulse<N> impulseInReferenceFrame = -upperLimitImpulse;

			if (impulseInReferenceFrame != 0) {
				const AngularImpulse<N> angularImpulse = referenceFrame(impulseInReferenceFrame);
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityA += inverseMomentOfInertiaTensorA * angularImpulse;
					GREM_ASSERT(all(isfinite(linearVelocityA)));
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityB -= inverseMomentOfInertiaTensorB * angularImpulse;
					GREM_ASSERT(all(isfinite(linearVelocityB)));
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			angularConstraintImpulses.upperLimitMomentums = newUpperLimitMomentum;
			angularConstraintImpulses.upperLimitImpulses += upperLimitImpulse;
		}
	}

	for (auto&& [jointID, linearConstraintImpulses, connectedObjects, attachmentOffsets, attachmentOrientations, linearConstraint, activeTag] : linearConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const Length<N> offsetA = orientationA(scaleA * attachmentOffsets.first);
		const Length<N> offsetB = orientationB(scaleB * attachmentOffsets.second);
		const Position<N> pointA = positionA + offsetA;
		const Position<N> pointB = positionB + offsetB;

		const OrthonormalBasis<N> basis = rotate(orientationA * attachmentOrientations.first);
		const OrthonormalBasis<N> inverseBasis = transpose(basis);
		const Length<N> offsetsInReferenceFrame = inverseBasis * (pointA - pointB);

		const MomentArmTensor<N> momentArmsA = cross(offsetA, basis);
		const MomentArmTensor<N> momentArmsB = cross(offsetB, basis);
		const AngularQuantity<N, InverseMass::Unit> inverseEffectiveMassesA = inverseMassA + dot(momentArmsA, inverseMomentOfInertiaTensorA * momentArmsA);
		const AngularQuantity<N, InverseMass::Unit> inverseEffectiveMassesB = inverseMassB + dot(momentArmsB, inverseMomentOfInertiaTensorB * momentArmsB);
		const AngularQuantity<N, InverseMass::Unit> combinedInverseEffectiveMasses = inverseEffectiveMassesA + inverseEffectiveMassesB;
		const AngularQuantity<N, Mass::Unit> combinedEffectiveMasses =
			select(equal(combinedInverseEffectiveMasses, 0), AngularQuantity<N, Mass::Unit>{}, inverse(combinedInverseEffectiveMasses));

		const auto [biasRates, massScales, momentumScales] =
			(useBias) ? getSoftConstraintParameters(linearConstraint.limitStiffnesses, linearConstraint.limitDampingRatios, deltaTime, inverseDeltaTime)
					  : SoftConstraintParameters<N>{};

		{
			const Length<N> lowerLimitErrors = offsetsInReferenceFrame - linearConstraint.minOffsets;
			const Mask<N> isSpeculativeLowerLimits = greaterThan(lowerLimitErrors, 0);

			const LinearVelocity<N> relativeVelocity = (linearVelocityA + cross(offsetA, angularVelocityA)) - (linearVelocityB + cross(offsetB, angularVelocityB));
			const LinearVelocity<N> relativeVelocityInReferenceFrame = inverseBasis * relativeVelocity;
			const LinearVelocity<N> lowerLimitErrorGradient = relativeVelocityInReferenceFrame;
			const LinearVelocity<N> lowerLimitErrorBiases = lowerLimitErrors * select(isSpeculativeLowerLimits, Rate<N>{inverseDeltaTime}, biasRates * massScales);
			const LinearMomentum<N> lowerLimitMomentum = linearConstraintImpulses.lowerLimitMomentums;
			const LinearImpulse<N> desiredLowerLimitImpulse = combinedEffectiveMasses * -(lowerLimitErrorGradient * massScales + lowerLimitErrorBiases) -
			                                                  select(isSpeculativeLowerLimits, LinearImpulse<N>{}, lowerLimitMomentum * momentumScales);

			const LinearMomentum<N> newLowerLimitMomentum =
				clamp(lowerLimitMomentum + desiredLowerLimitImpulse, LinearMomentum<N>{}, LinearMomentum<N>{simulationOptions.jointMaxMomentum});
			const LinearImpulse<N> lowerLimitImpulse = newLowerLimitMomentum - lowerLimitMomentum;
			const LinearImpulse<N> impulseInReferenceFrame = lowerLimitImpulse;

			if (impulseInReferenceFrame != 0) {
				const LinearImpulse<N> linearImpulse = basis * impulseInReferenceFrame;
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					linearVelocityA += inverseMassA * linearImpulse;
					angularVelocityA += inverseMomentOfInertiaTensorA * cross(offsetA, linearImpulse);
					GREM_ASSERT(all(isfinite(linearVelocityA)));
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					linearVelocityB -= inverseMassB * linearImpulse;
					angularVelocityB -= inverseMomentOfInertiaTensorB * cross(offsetB, linearImpulse);
					GREM_ASSERT(all(isfinite(linearVelocityB)));
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			linearConstraintImpulses.lowerLimitMomentums = newLowerLimitMomentum;
			linearConstraintImpulses.lowerLimitImpulses += lowerLimitImpulse;
		}

		{
			const Length<N> upperLimitErrors = linearConstraint.maxOffsets - offsetsInReferenceFrame;
			const Mask<N> isSpeculativeUpperLimits = greaterThan(upperLimitErrors, 0);

			const LinearVelocity<N> relativeVelocity = (linearVelocityA + cross(offsetA, angularVelocityA)) - (linearVelocityB + cross(offsetB, angularVelocityB));
			const LinearVelocity<N> relativeVelocityInReferenceFrame = inverseBasis * relativeVelocity;
			const LinearVelocity<N> upperLimitErrorGradient = -relativeVelocityInReferenceFrame;
			const LinearVelocity<N> upperLimitErrorBiases = upperLimitErrors * select(isSpeculativeUpperLimits, Rate<N>{inverseDeltaTime}, biasRates * massScales);
			const LinearMomentum<N> upperLimitMomentum = linearConstraintImpulses.upperLimitMomentums;
			const LinearImpulse<N> desiredUpperLimitImpulse = combinedEffectiveMasses * -(upperLimitErrorGradient * massScales + upperLimitErrorBiases) -
			                                                  select(isSpeculativeUpperLimits, LinearImpulse<N>{}, upperLimitMomentum * momentumScales);

			const LinearMomentum<N> newUpperLimitMomentum =
				clamp(upperLimitMomentum + desiredUpperLimitImpulse, LinearMomentum<N>{}, LinearMomentum<N>{simulationOptions.jointMaxMomentum});
			const LinearImpulse<N> upperLimitImpulse = newUpperLimitMomentum - upperLimitMomentum;
			const LinearImpulse<N> impulseInReferenceFrame = -upperLimitImpulse;

			if (impulseInReferenceFrame != 0) {
				const LinearImpulse<N> linearImpulse = basis * impulseInReferenceFrame;
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					linearVelocityA += inverseMassA * linearImpulse;
					angularVelocityA += inverseMomentOfInertiaTensorA * cross(offsetA, linearImpulse);
					GREM_ASSERT(all(isfinite(linearVelocityA)));
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					linearVelocityB -= inverseMassB * linearImpulse;
					angularVelocityB -= inverseMomentOfInertiaTensorB * cross(offsetB, linearImpulse);
					GREM_ASSERT(all(isfinite(linearVelocityB)));
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			linearConstraintImpulses.upperLimitMomentums = newUpperLimitMomentum;
			linearConstraintImpulses.upperLimitImpulses += upperLimitImpulse;
		}
	}

	for (auto&& [jointID, distanceConstraintImpulses, connectedObjects, attachmentOffsets, attachmentOrientations, distanceConstraint, activeTag] :
		distanceConstraintJointEntities) {
		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[connectedObjects.second];

		const Length<N> offsetA = orientationA(scaleA * attachmentOffsets.first);
		const Length<N> offsetB = orientationB(scaleB * attachmentOffsets.second);
		const Position<N> pointA = positionA + offsetA;
		const Position<N> pointB = positionB + offsetB;
		const Length<N> difference = pointA - pointB;
		const Distance distance = length(difference);
		if (distance <= Distance::MACHINE_EPSILON) {
			continue;
		}
		const Direction<N> normal = Direction<N>::reinterpret(difference / distance);

		const MomentArm<N> momentArmA = cross(offsetA, normal);
		const MomentArm<N> momentArmB = cross(offsetB, normal);
		const InverseMass inverseEffectiveMassA = inverseMassA + dot(momentArmA, inverseMomentOfInertiaTensorA * momentArmA);
		const InverseMass inverseEffectiveMassB = inverseMassB + dot(momentArmB, inverseMomentOfInertiaTensorB * momentArmB);
		const InverseMass combinedInverseEffectiveMass = inverseEffectiveMassA + inverseEffectiveMassB;
		const Mass combinedEffectiveMass = (combinedInverseEffectiveMass == 0) ? Mass{} : inverse(combinedInverseEffectiveMass);

		const auto [biasRate, massScale, momentumScale] =
			(useBias) ? getSoftConstraintParameters(distanceConstraint.limitStiffness, distanceConstraint.limitDampingRatio, deltaTime, inverseDeltaTime)
					  : SoftConstraintParameters<1>{};

		{
			const Length1D lowerLimitError = distance - distanceConstraint.minDistance;
			const Mask1D isSpeculativeLowerLimit = greaterThan(lowerLimitError, 0);
			const LinearVelocity1D relativeNormalVelocity =
				dot(normal, (linearVelocityA + cross(offsetA, angularVelocityA)) - (linearVelocityB + cross(offsetB, angularVelocityB)));
			const LinearVelocity1D lowerLimitErrorDerivative = relativeNormalVelocity;
			const LinearVelocity1D lowerLimitErrorBias = lowerLimitError * select(isSpeculativeLowerLimit, inverseDeltaTime, biasRate * massScale);
			const LinearMomentum1D lowerLimitMomentum = distanceConstraintImpulses.lowerLimitMomentum;
			const LinearImpulse1D desiredLowerLimitImpulse = combinedEffectiveMass * -(lowerLimitErrorDerivative * massScale + lowerLimitErrorBias) -
			                                                 select(isSpeculativeLowerLimit, LinearImpulse1D{}, lowerLimitMomentum * momentumScale);

			const LinearMomentum1D newLowerLimitMomentum = select(equal(distanceConstraint.minDistance, distanceConstraint.maxDistance), LinearMomentum1D{},
				clamp(lowerLimitMomentum + desiredLowerLimitImpulse, LinearMomentum1D{}, simulationOptions.jointMaxMomentum));
			const LinearImpulse1D lowerLimitImpulse = newLowerLimitMomentum - lowerLimitMomentum;

			const LinearImpulse1D impulse = lowerLimitImpulse;
			if (impulse != 0) {
				const LinearImpulse<N> linearImpulse = normal * impulse;
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					linearVelocityA += inverseMassA * linearImpulse;
					angularVelocityA += inverseMomentOfInertiaTensorA * (momentArmA * impulse);
					GREM_ASSERT(all(isfinite(linearVelocityA)));
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					linearVelocityB -= inverseMassB * linearImpulse;
					angularVelocityB -= inverseMomentOfInertiaTensorB * (momentArmB * impulse);
					GREM_ASSERT(all(isfinite(linearVelocityB)));
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			distanceConstraintImpulses.lowerLimitMomentum = newLowerLimitMomentum;
			distanceConstraintImpulses.lowerLimitImpulse += lowerLimitImpulse;
		}

		{
			const Length1D upperLimitError = distanceConstraint.maxDistance - distance;
			const Mask1D isSpeculativeUpperLimit = greaterThan(upperLimitError, 0);
			const LinearVelocity1D relativeNormalVelocity =
				dot(normal, (linearVelocityA + cross(offsetA, angularVelocityA)) - (linearVelocityB + cross(offsetB, angularVelocityB)));
			const LinearVelocity1D upperLimitErrorDerivative = -relativeNormalVelocity;
			const LinearVelocity1D upperLimitErrorBias = upperLimitError * select(isSpeculativeUpperLimit, inverseDeltaTime, biasRate * massScale);
			const LinearMomentum1D upperLimitMomentum = distanceConstraintImpulses.upperLimitMomentum;
			const LinearImpulse1D desiredUpperLimitImpulse = combinedEffectiveMass * -(upperLimitErrorDerivative * massScale + upperLimitErrorBias) -
			                                                 select(isSpeculativeUpperLimit, LinearImpulse1D{}, upperLimitMomentum * momentumScale);

			const LinearMomentum1D newUpperLimitMomentum = clamp(upperLimitMomentum + desiredUpperLimitImpulse,
				select(equal(distanceConstraint.minDistance, distanceConstraint.maxDistance), -simulationOptions.jointMaxMomentum, LinearMomentum1D{}),
				simulationOptions.jointMaxMomentum);
			const LinearImpulse1D upperLimitImpulse = newUpperLimitMomentum - upperLimitMomentum;

			const LinearImpulse1D impulse = -upperLimitImpulse;
			if (impulse != 0) {
				const LinearImpulse<N> linearImpulse = normal * impulse;
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					linearVelocityA += inverseMassA * linearImpulse;
					angularVelocityA += inverseMomentOfInertiaTensorA * (momentArmA * impulse);
					GREM_ASSERT(all(isfinite(linearVelocityA)));
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					linearVelocityB -= inverseMassB * linearImpulse;
					angularVelocityB -= inverseMomentOfInertiaTensorB * (momentArmB * impulse);
					GREM_ASSERT(all(isfinite(linearVelocityB)));
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			distanceConstraintImpulses.upperLimitMomentum = newUpperLimitMomentum;
			distanceConstraintImpulses.upperLimitImpulse += upperLimitImpulse;
		}

		distanceConstraintImpulses.lastOffsets = {offsetA, offsetB};
	}
}

template <size_t N, bool UseBias>
void solveJointConstraints(execution::Entities<JointAngularConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>,
							   const JointAttachmentOrientations<N>, const JointAngularConstraint<N>, const JointActiveTag>
							   angularConstraintJointEntities,
	execution::Entities<JointLinearConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointAttachmentOrientations<N>,
		const JointLinearConstraint<N>, const JointActiveTag>
		linearConstraintJointEntities,
	execution::Entities<JointDistanceConstraintImpulses<N>, const JointConnectedObjects, const JointAttachmentOffsets<N>, const JointAttachmentOrientations<N>,
		const JointDistanceConstraint<N>, const JointActiveTag>
		distanceConstraintJointEntities,
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Position<N>, const Orientation<N>, const Scale<N>, const InverseMass,
		const InverseMomentOfInertiaTensor<N>>
		objectEntities,
	const SimulationOptions<N>& simulationOptions) {
	solveJointConstraintsImplementation<N>(angularConstraintJointEntities, linearConstraintJointEntities, distanceConstraintJointEntities, objectEntities, simulationOptions,
		UseBias);
}

void solve3DOnlyJointConstraintsImplementation(
	execution::Entities<JointConeConstraintImpulses3D, const JointConnectedObjects, const JointAttachmentOrientations3D, const JointConeConstraint3D, const JointActiveTag>
		coneConstraintJointEntities,
	execution::Entities<JointTwistConstraintImpulses3D, const JointConnectedObjects, const JointAttachmentOrientations3D, const JointTwistConstraint3D, const JointActiveTag>
		twistConstraintJointEntities,
	execution::Entities<ObjectActivity, AngularVelocity3D, const Orientation3D, const InverseMass, const InverseMomentOfInertiaTensor3D> objectEntities,
	const SimulationOptions3D& simulationOptions, bool useBias) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);
	const Frequency inverseDeltaTime = inverse(deltaTime);

	for (auto&& [jointID, coneConstraintImpulses, connectedObjects, attachmentOrientations, coneConstraint, activeTag] : coneConstraintJointEntities) {
		auto&& [objectIDA, activityA, angularVelocityA, orientationA, inverseMassA, inverseMomentOfInertiaTensorA] = objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, angularVelocityB, orientationB, inverseMassB, inverseMomentOfInertiaTensorB] = objectEntities[connectedObjects.second];

		const OrthonormalBasis3D basisA = rotate(orientationA * attachmentOrientations.first);
		const OrthonormalBasis3D basisB = rotate(orientationB * attachmentOrientations.second);

		const Scale3D axisVector = cross(basisA[X], basisB[X]);
		const Optional<Direction3D> normalizedAxisVector = tryNormalize(axisVector);
		if (!normalizedAxisVector) {
			continue;
		}
		const Direction3D axis = *normalizedAxisVector;
		const Angle angle = getAngleDifferenceAroundAxis(axis, basisA[X], basisB[X]);

		const InverseMomentOfInertia2D inverseEffectiveMomentOfInertiaA = dot(axis, inverseMomentOfInertiaTensorA * axis);
		const InverseMomentOfInertia2D inverseEffectiveMomentOfInertiaB = dot(axis, inverseMomentOfInertiaTensorB * axis);
		const InverseMomentOfInertia2D combinedInverseEffectiveMomentOfInertia = inverseEffectiveMomentOfInertiaA + inverseEffectiveMomentOfInertiaB;
		const MomentOfInertia2D combinedEffectiveMomentOfInertia =
			(combinedInverseEffectiveMomentOfInertia == 0) ? MomentOfInertia2D{} : inverse(combinedInverseEffectiveMomentOfInertia);

		const auto [biasRate, massScale, momentumScale] =
			(useBias) ? getSoftConstraintParameters(coneConstraint.limitStiffness, coneConstraint.limitDampingRatio, deltaTime, inverseDeltaTime) : SoftConstraintParameters<1>{};

		{
			const Angle lowerLimitError = angle - coneConstraint.minAngle;
			const AngularMask2D isSpeculativeLowerLimit = greaterThan(lowerLimitError, 0);
			const AngularVelocity2D relativeAngularVelocityAroundAxis = dot(axis, angularVelocityA - angularVelocityB);
			const AngularVelocity2D lowerLimitErrorDerivative = relativeAngularVelocityAroundAxis;
			const AngularVelocity2D lowerLimitErrorBias = lowerLimitError * select(isSpeculativeLowerLimit, inverseDeltaTime, biasRate * massScale);
			const AngularMomentum2D lowerLimitMomentum = coneConstraintImpulses.lowerLimitMomentum;
			const AngularImpulse2D desiredLowerLimitImpulse = combinedEffectiveMomentOfInertia * -(lowerLimitErrorDerivative * massScale + lowerLimitErrorBias) -
			                                                  select(isSpeculativeLowerLimit, AngularImpulse2D{}, lowerLimitMomentum * momentumScale);

			const AngularMomentum2D newLowerLimitMomentum = clamp(lowerLimitMomentum + desiredLowerLimitImpulse, AngularMomentum2D{}, simulationOptions.jointMaxAngularMomentum);
			const AngularImpulse2D lowerLimitImpulse = newLowerLimitMomentum - lowerLimitMomentum;

			const AngularImpulse2D impulseAroundAxis = lowerLimitImpulse;
			if (impulseAroundAxis != 0) {
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityA += inverseMomentOfInertiaTensorA * (axis * impulseAroundAxis);
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityB -= inverseMomentOfInertiaTensorB * (axis * impulseAroundAxis);
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			coneConstraintImpulses.lowerLimitMomentum = newLowerLimitMomentum;
			coneConstraintImpulses.lowerLimitImpulse += lowerLimitImpulse;
		}

		{
			const Angle upperLimitError = coneConstraint.maxAngle - angle;
			const AngularMask2D isSpeculativeUpperLimit = greaterThan(upperLimitError, 0);
			const AngularVelocity2D relativeAngularVelocityAroundAxis = dot(axis, angularVelocityA - angularVelocityB);
			const AngularVelocity2D upperLimitErrorDerivative = -relativeAngularVelocityAroundAxis;
			const AngularVelocity2D upperLimitErrorBias = upperLimitError * select(isSpeculativeUpperLimit, inverseDeltaTime, biasRate * massScale);
			const AngularMomentum2D upperLimitMomentum = coneConstraintImpulses.upperLimitMomentum;
			const AngularImpulse2D desiredUpperLimitImpulse = combinedEffectiveMomentOfInertia * -(upperLimitErrorDerivative * massScale + upperLimitErrorBias) -
			                                                  select(isSpeculativeUpperLimit, AngularImpulse2D{}, upperLimitMomentum * momentumScale);

			const AngularMomentum2D newUpperLimitMomentum = clamp(upperLimitMomentum + desiredUpperLimitImpulse, AngularMomentum2D{}, simulationOptions.jointMaxAngularMomentum);
			const AngularImpulse2D upperLimitImpulse = newUpperLimitMomentum - upperLimitMomentum;

			const AngularImpulse2D impulseAroundAxis = -upperLimitImpulse;
			if (impulseAroundAxis != 0) {
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityA += inverseMomentOfInertiaTensorA * (axis * impulseAroundAxis);
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityB -= inverseMomentOfInertiaTensorB * (axis * impulseAroundAxis);
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			coneConstraintImpulses.upperLimitMomentum = newUpperLimitMomentum;
			coneConstraintImpulses.upperLimitImpulse += upperLimitImpulse;
		}
	}

	for (auto&& [jointID, twistConstraintImpulses, connectedObjects, attachmentOrientations, twistConstraint, activeTag] : twistConstraintJointEntities) {
		auto&& [objectIDA, activityA, angularVelocityA, orientationA, inverseMassA, inverseMomentOfInertiaTensorA] = objectEntities[connectedObjects.first];
		auto&& [objectIDB, activityB, angularVelocityB, orientationB, inverseMassB, inverseMomentOfInertiaTensorB] = objectEntities[connectedObjects.second];

		const OrthonormalBasis3D basisA = rotate(orientationA * attachmentOrientations.first);
		const OrthonormalBasis3D basisB = rotate(orientationB * attachmentOrientations.second);

		const Scale3D axisVector = basisA[X] + basisB[X];
		const Optional<Direction3D> normalizedAxisVector = tryNormalize(axisVector);
		if (!normalizedAxisVector) {
			continue;
		}
		const Direction3D axis = *normalizedAxisVector;
		const Angle angle = getAngleDifferenceAroundAxis(axis, basisA[Y], basisB[Y]);

		const InverseMomentOfInertia2D inverseEffectiveMomentOfInertiaA = dot(axis, inverseMomentOfInertiaTensorA * axis);
		const InverseMomentOfInertia2D inverseEffectiveMomentOfInertiaB = dot(axis, inverseMomentOfInertiaTensorB * axis);
		const InverseMomentOfInertia2D combinedInverseEffectiveMomentOfInertia = inverseEffectiveMomentOfInertiaA + inverseEffectiveMomentOfInertiaB;
		const MomentOfInertia2D combinedEffectiveMomentOfInertia =
			(combinedInverseEffectiveMomentOfInertia == 0) ? MomentOfInertia2D{} : inverse(combinedInverseEffectiveMomentOfInertia);

		const auto [biasRate, massScale, momentumScale] =
			(useBias) ? getSoftConstraintParameters(twistConstraint.limitStiffness, twistConstraint.limitDampingRatio, deltaTime, inverseDeltaTime) : SoftConstraintParameters<1>{};

		{
			const Angle lowerLimitError = angle - twistConstraint.minAngle;
			const AngularMask2D isSpeculativeLowerLimit = greaterThan(lowerLimitError, 0);
			const AngularVelocity2D relativeAngularVelocityAroundAxis = dot(axis, angularVelocityA - angularVelocityB);
			const AngularVelocity2D lowerLimitErrorDerivative = relativeAngularVelocityAroundAxis;
			const AngularVelocity2D lowerLimitErrorBias = lowerLimitError * select(isSpeculativeLowerLimit, inverseDeltaTime, biasRate * massScale);
			const AngularMomentum2D lowerLimitMomentum = twistConstraintImpulses.lowerLimitMomentum;
			const AngularImpulse2D desiredLowerLimitImpulse = combinedEffectiveMomentOfInertia * -(lowerLimitErrorDerivative * massScale + lowerLimitErrorBias) -
			                                                  select(isSpeculativeLowerLimit, AngularImpulse2D{}, lowerLimitMomentum * momentumScale);

			const AngularMomentum2D newLowerLimitMomentum = clamp(lowerLimitMomentum + desiredLowerLimitImpulse, AngularMomentum2D{}, simulationOptions.jointMaxAngularMomentum);
			const AngularImpulse2D lowerLimitImpulse = newLowerLimitMomentum - lowerLimitMomentum;

			const AngularImpulse2D impulseAroundAxis = lowerLimitImpulse;
			if (impulseAroundAxis != 0) {
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityA += inverseMomentOfInertiaTensorA * (axis * impulseAroundAxis);
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityB -= inverseMomentOfInertiaTensorB * (axis * impulseAroundAxis);
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			twistConstraintImpulses.lowerLimitMomentum = newLowerLimitMomentum;
			twistConstraintImpulses.lowerLimitImpulse += lowerLimitImpulse;
		}

		{
			const Angle upperLimitError = twistConstraint.maxAngle - angle;
			const AngularMask2D isSpeculativeUpperLimit = greaterThan(upperLimitError, 0);
			const AngularVelocity2D relativeAngularVelocityAroundAxis = dot(axis, angularVelocityA - angularVelocityB);
			const AngularVelocity2D upperLimitErrorDerivative = -relativeAngularVelocityAroundAxis;
			const AngularVelocity2D upperLimitErrorBias = upperLimitError * select(isSpeculativeUpperLimit, inverseDeltaTime, biasRate * massScale);
			const AngularMomentum2D upperLimitMomentum = twistConstraintImpulses.upperLimitMomentum;
			const AngularImpulse2D desiredUpperLimitImpulse = combinedEffectiveMomentOfInertia * -(upperLimitErrorDerivative * massScale + upperLimitErrorBias) -
			                                                  select(isSpeculativeUpperLimit, AngularImpulse2D{}, upperLimitMomentum * momentumScale);

			const AngularMomentum2D newUpperLimitMomentum = clamp(upperLimitMomentum + desiredUpperLimitImpulse, AngularMomentum2D{}, simulationOptions.jointMaxAngularMomentum);
			const AngularImpulse2D upperLimitImpulse = newUpperLimitMomentum - upperLimitMomentum;

			const AngularImpulse2D impulseAroundAxis = -upperLimitImpulse;
			if (impulseAroundAxis != 0) {
				if (activityA.isCorrectable != 0) {
					activityA.wasCorrected = 1;
					activityA.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityA += inverseMomentOfInertiaTensorA * (axis * impulseAroundAxis);
					GREM_ASSERT(all(isfinite(angularVelocityA)));
				}
				if (activityB.isCorrectable != 0) {
					activityB.wasCorrected = 1;
					activityB.energyLevel = ObjectActivity::MAX_ENERGY_LEVEL;
					angularVelocityB -= inverseMomentOfInertiaTensorB * (axis * impulseAroundAxis);
					GREM_ASSERT(all(isfinite(angularVelocityB)));
				}
			}
			twistConstraintImpulses.upperLimitMomentum = newUpperLimitMomentum;
			twistConstraintImpulses.upperLimitImpulse += upperLimitImpulse;
		}
	}
}

template <bool UseBias>
void solve3DOnlyJointConstraints(
	execution::Entities<JointConeConstraintImpulses3D, const JointConnectedObjects, const JointAttachmentOrientations3D, const JointConeConstraint3D, const JointActiveTag>
		coneConstraintJointEntities,
	execution::Entities<JointTwistConstraintImpulses3D, const JointConnectedObjects, const JointAttachmentOrientations3D, const JointTwistConstraint3D, const JointActiveTag>
		twistConstraintJointEntities,
	execution::Entities<ObjectActivity, AngularVelocity3D, const Orientation3D, const InverseMass, const InverseMomentOfInertiaTensor3D> objectEntities,
	const SimulationOptions3D& simulationOptions) {
	solve3DOnlyJointConstraintsImplementation(coneConstraintJointEntities, twistConstraintJointEntities, objectEntities, simulationOptions, UseBias);
}

template <size_t N>
void solveContactConstraintsImplementation(
	execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Position<N>, const Orientation<N>, const Scale<N>, const ObjectFlags, const Material,
		const InverseMass, const InverseMomentOfInertiaTensor<N>>
		objectEntities,
	Span<const ContactIndex> contactIndices, Contacts<N>& contacts, const SimulationOptions<N>& simulationOptions, bool useBias) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);
	const Frequency inverseDeltaTime = inverse(deltaTime);
	const Speed maxRestingSpeed = simulationOptions.maxRestingSpeed;
	const AngularSpeed maxRestingAngularSpeed = simulationOptions.maxRestingAngularSpeed;
	const SquaredSpeed maxRestingSpeedSquared = length2(maxRestingSpeed);
	const SquaredAngularSpeed maxRestingAngularSpeedSquared = length2(maxRestingAngularSpeed);
	const Quantity<1, Reciprocal<Speed::Unit>> energyPerUnitSpeed = inverse(maxRestingSpeed);
	const Quantity<1, Reciprocal<AngularSpeed::Unit>> energyPerUnitAngularSpeed = inverse(maxRestingAngularSpeed);
	const Momentum maxMomentum = simulationOptions.contactMaxMomentum;

	for (const ContactIndex contactIndex : contactIndices) {
		auto&& [objectIDs, contact] = contacts.getAtIndex(contactIndex);

		auto&& [objectIDA, activityA, linearVelocityA, angularVelocityA, positionA, orientationA, scaleA, flagsA, materialA, inverseMassA, inverseMomentOfInertiaTensorA] =
			objectEntities[objectIDs.first];
		auto&& [objectIDB, activityB, linearVelocityB, angularVelocityB, positionB, orientationB, scaleB, flagsB, materialB, inverseMassB, inverseMomentOfInertiaTensorB] =
			objectEntities[objectIDs.second];

		const bool isCorrectableA = activityA.isCorrectable != 0;
		const bool isCorrectableB = activityB.isCorrectable != 0;
		const bool isRestingA = activityA.energyLevel == 0;
		const bool isRestingB = activityB.energyLevel == 0;
		const bool isWakeableA = isCorrectableA && flagsA.enableWaking;
		const bool isWakeableB = isCorrectableB && flagsB.enableWaking;
		const Coefficient staticFriction =
			calculateCombinedStaticFriction(materialA.staticFriction, materialB.staticFriction, materialA.frictionCombine, materialB.frictionCombine);
		const Coefficient kineticFriction =
			calculateCombinedKineticFriction(materialA.kineticFriction, materialB.kineticFriction, materialA.frictionCombine, materialB.frictionCombine);
		const Coefficient rollingResistance =
			calculateCombinedRollingResistance(materialA.rollingResistance, materialB.rollingResistance, materialA.frictionCombine, materialB.frictionCombine);

		// Calculate soft constraint parameters.
		const auto [biasRate, massScale, momentumScale] =
			(useBias) ? getSoftConstraintParameters(simulationOptions.contactStiffness, simulationOptions.contactDampingRatio, deltaTime, inverseDeltaTime)
					  : SoftConstraintParameters<1>{};

		bool wakeA = false;
		bool wakeB = false;
		// Correct the velocity of each colliding contact point.
		for (ContactManifold<N>& manifold : contact.manifolds) {
			if (!manifold.filterTestResult.respondsToCollision()) {
				continue;
			}

			const Direction<N> normal = manifold.normal;
			LinearMomentum1D totalNormalMomentum{};
			SquaredDistance maxRadiusSquaredA{};
			SquaredDistance maxRadiusSquaredB{};
			for (ContactPoint<N>& point : manifold.points) {
				const OrthonormalBasis<N> tangentSpaceBasis = point.getTangentSpaceBasis(normal);

				// Get the world space positions of the contact points on the potentially colliding objects.
				const Position<N> pointA = positionA + orientationA(scaleA * point.localOffsets.first);
				const Position<N> pointB = positionB + orientationB(scaleB * point.localOffsets.second);

				// Calculate the positional error along the normal direction.
				const Length1D penetrationDepth = dot(pointA - pointB, normal);

				// Get the relative velocity of the objects' respective points along the contact tangent, bitangent and normal.
				const Length<N> offsetA = point.offsets.first;
				const Length<N> offsetB = point.offsets.second;
				const LinearVelocity<N> relativeVelocity = (linearVelocityA + cross(angularVelocityA, offsetA)) - (linearVelocityB + cross(angularVelocityB, offsetB));
				const LinearVelocity<N> relativeVelocityInTangentSpace = transpose(tangentSpaceBasis) * relativeVelocity;
				const LinearVelocity1D relativeNormalVelocity = getNormalComponent(relativeVelocityInTangentSpace);
				const LinearVelocity<N - 1> relativeTangentialVelocity = getTangentialComponent(relativeVelocityInTangentSpace);

				// Calculate the effective inverse masses (linear + angular) of the objects at the collision point along the tangent space directions.
				const MomentArmTensor<N> momentArmsA = cross(offsetA, tangentSpaceBasis);
				const MomentArmTensor<N> momentArmsB = cross(offsetB, tangentSpaceBasis);
				const AngularQuantity<N, InverseMass::Unit> inverseEffectiveMassesA = inverseMassA + dot(momentArmsA, inverseMomentOfInertiaTensorA * momentArmsA);
				const AngularQuantity<N, InverseMass::Unit> inverseEffectiveMassesB = inverseMassB + dot(momentArmsB, inverseMomentOfInertiaTensorB * momentArmsB);
				const AngularQuantity<N, InverseMass::Unit> combinedInverseEffectiveMasses = inverseEffectiveMassesA + inverseEffectiveMassesB;
				const AngularQuantity<N, Mass::Unit> combinedEffectiveMasses =
					select(equal(combinedInverseEffectiveMasses, 0), AngularQuantity<N, Mass::Unit>{}, inverse(combinedInverseEffectiveMasses));

				// Calculate the impulse to apply along the normal direction.
				const Length1D normalError = penetrationDepth;
				const Mask1D isSpeculative = lessThan(normalError, -simulationOptions.collisionAlgorithmOptions.collisionDistanceErrorTolerance);
				const LinearVelocity1D normalErrorDerivative = relativeNormalVelocity;
				const LinearVelocity1D normalErrorBias = normalError * select(isSpeculative, inverseDeltaTime, biasRate * massScale);
				const LinearMomentum1D normalMomentum = getNormalComponent(point.momentumInTangentSpace);
				const Mass normalCombinedEffectiveMass = getNormalComponent(combinedEffectiveMasses);
				const LinearImpulse1D desiredNormalImpulse =
					normalCombinedEffectiveMass * -(normalErrorDerivative * massScale + normalErrorBias) - select(isSpeculative, LinearImpulse1D{}, normalMomentum * momentumScale);
				const LinearMomentum1D newNormalMomentum = clamp(normalMomentum + desiredNormalImpulse, -maxMomentum, LinearMomentum1D{});
				const LinearImpulse1D normalImpulse = newNormalMomentum - normalMomentum;

				// Calculate the impulse to apply along the tangential direction.
				const LinearVelocity<N - 1> tangentialErrorGradient = relativeTangentialVelocity;
				const LinearMomentum<N - 1> tangentialMomentum = getTangentialComponent(point.momentumInTangentSpace);
				const Quantity<N - 1, Mass::Unit> tangentialCombinedEffectiveMasses = getTangentialComponent(combinedEffectiveMasses);
				const LinearImpulse<N - 1> desiredTangentialImpulse = (useBias) ? LinearImpulse<N - 1>{} : -tangentialErrorGradient * tangentialCombinedEffectiveMasses;
				const LinearMomentum<N - 1> desiredNewTangentialMomentum = tangentialMomentum + desiredTangentialImpulse;
				const bool isStaticFriction = length2(desiredNewTangentialMomentum) < staticFriction * length2(newNormalMomentum);
				const LinearMomentum1D maxFrictionMomentum = min(abs(newNormalMomentum * select(isStaticFriction, staticFriction, kineticFriction)), maxMomentum);
				const LinearMomentum<N - 1> newTangentialMomentum = clampLength(desiredNewTangentialMomentum, maxFrictionMomentum);
				const LinearImpulse<N - 1> tangentialImpulse = newTangentialMomentum - tangentialMomentum;

				// Apply the impulse to the objects.
				const LinearImpulse<N> impulse{tangentialImpulse, normalImpulse};
				if (impulse != 0) {
					const LinearImpulse<N> linearImpulse = tangentSpaceBasis * impulse;
					const LinearVelocity<N> deltaLinearVelocityA = inverseMassA * linearImpulse;
					const LinearVelocity<N> deltaLinearVelocityB = inverseMassB * linearImpulse;
					const AngularVelocity<N> deltaAngularVelocityA = inverseMomentOfInertiaTensorA * cross(offsetA, linearImpulse);
					const AngularVelocity<N> deltaAngularVelocityB = inverseMomentOfInertiaTensorB * cross(offsetB, linearImpulse);

					// If an object is movable and the impulse that we're about to apply to it is above the wake threshold, wake it up.
					wakeA |=
						isRestingA & isWakeableA & ((length2(deltaLinearVelocityA) > maxRestingSpeedSquared) | (length2(deltaAngularVelocityA) > maxRestingAngularSpeedSquared));
					wakeB |=
						isRestingB & isWakeableA & ((length2(deltaLinearVelocityB) > maxRestingSpeedSquared) | (length2(deltaAngularVelocityB) > maxRestingAngularSpeedSquared));

					if (isCorrectableA & ((!isRestingA) | wakeA)) {
						// Note: This parallel mutation is safe thanks to graph coloring.
						activityA.wasCorrected = 1;
						linearVelocityA += deltaLinearVelocityA;
						angularVelocityA += deltaAngularVelocityA;
						GREM_ASSERT(all(isfinite(linearVelocityA)));
						GREM_ASSERT(all(isfinite(angularVelocityA)));
					}
					if (isCorrectableB & ((!isRestingB) | wakeB)) {
						// Note: This parallel mutation is safe thanks to graph coloring.
						activityB.wasCorrected = 1;
						linearVelocityB -= deltaLinearVelocityB;
						angularVelocityB -= deltaAngularVelocityB;
						GREM_ASSERT(all(isfinite(linearVelocityB)));
						GREM_ASSERT(all(isfinite(angularVelocityB)));
					}
				}

				// Update the correction impulses.
				// Note: This parallel mutation is safe thanks to graph coloring.
				point.momentumInTangentSpace = LinearMomentum<N>{newTangentialMomentum, newNormalMomentum};
				point.impulseInTangentSpace += LinearImpulse<N>{tangentialImpulse, normalImpulse};

				totalNormalMomentum += newNormalMomentum;
				maxRadiusSquaredA = max(maxRadiusSquaredA, length2(offsetA));
				maxRadiusSquaredB = max(maxRadiusSquaredB, length2(offsetB));
			}

			// Apply rolling resistance.
			if (!useBias && rollingResistance > 0) {
				const SquaredDistance radiusSquaredA = (isCorrectableA) ? maxRadiusSquaredA : maxRadiusSquaredB;
				const SquaredDistance radiusSquaredB = (isCorrectableB) ? maxRadiusSquaredB : maxRadiusSquaredA;
				const Distance radius = sqrt(min(radiusSquaredA, radiusSquaredB));
				const InverseMomentOfInertiaTensor<N> combinedInverseEffectiveMomentOfInertiaTensor = inverseMomentOfInertiaTensorA + inverseMomentOfInertiaTensorB;
				const MomentOfInertiaTensor<N> combinedEffectiveMomentOfInertiaTensor =
					(combinedInverseEffectiveMomentOfInertiaTensor == 0)
						? MomentOfInertiaTensor<N>{0.0f * KILOGRAM_SQUARE_METERS}
						: inverse(combinedInverseEffectiveMomentOfInertiaTensor);

				const AngularVelocity<N> relativeAngularVelocity = angularVelocityA - angularVelocityB;
				const AngularVelocity<N> errorDerivative = relativeAngularVelocity;
				const AngularMomentum<N> momentum = manifold.rollingResistanceMomentum;
				const AngularImpulse<N> desiredImpulse = combinedEffectiveMomentOfInertiaTensor * -errorDerivative;
				const AngularMomentum2D maxRollingResistanceMomentum = min(abs(totalNormalMomentum * rollingResistance * radius), maxMomentum * radius);
				const AngularMomentum<N> newMomentum = clampLength(momentum + desiredImpulse, maxRollingResistanceMomentum);
				const AngularImpulse<N> impulse = newMomentum - momentum;

				if (impulse != 0) {
					const AngularVelocity<N> deltaAngularVelocityA = inverseMomentOfInertiaTensorA * impulse;
					const AngularVelocity<N> deltaAngularVelocityB = inverseMomentOfInertiaTensorB * impulse;

					wakeA |= isRestingA & isWakeableA & (length2(deltaAngularVelocityA) > maxRestingAngularSpeedSquared);
					wakeB |= isRestingB & isWakeableA & (length2(deltaAngularVelocityB) > maxRestingAngularSpeedSquared);

					if (isCorrectableA & ((!isRestingA) | wakeA)) {
						// Note: This parallel mutation is safe thanks to graph coloring.
						activityA.wasCorrected = 1;
						angularVelocityA += deltaAngularVelocityA;
						GREM_ASSERT(all(isfinite(angularVelocityA)));
					}
					if (isCorrectableB & ((!isRestingB) | wakeB)) {
						// Note: This parallel mutation is safe thanks to graph coloring.
						activityB.wasCorrected = 1;
						angularVelocityB -= deltaAngularVelocityB;
						GREM_ASSERT(all(isfinite(angularVelocityB)));
					}
				}
				manifold.rollingResistanceMomentum = newMomentum;
				manifold.rollingResistanceImpulse += impulse;
			}
		}

		// Note: This parallel mutation is safe thanks to graph coloring.
		GREM_ASSERT(!wakeA || !wakeB);
		if (wakeA) {
			const Coefficient energyFromLinearSpeedA = (length(linearVelocityA) - maxRestingSpeed) * energyPerUnitSpeed;
			const Coefficient energyFromAngularSpeedA = (length(angularVelocityA) - maxRestingAngularSpeed) * energyPerUnitAngularSpeed;
			activityA.energyLevel = max(ObjectActivity::EnergyLevel{activityA.energyLevel},
				static_cast<ObjectActivity::EnergyLevel>(
					clamp(max(energyFromLinearSpeedA, energyFromAngularSpeedA), Coefficient{1}, static_cast<Coefficient>(ObjectActivity::MAX_ENERGY_LEVEL))));
		} else if (wakeB) {
			const Coefficient energyFromLinearSpeedB = (length(linearVelocityB) - maxRestingSpeed) * energyPerUnitSpeed;
			const Coefficient energyFromAngularSpeedB = (length(angularVelocityB) - maxRestingAngularSpeed) * energyPerUnitAngularSpeed;
			activityB.energyLevel = max(ObjectActivity::EnergyLevel{activityB.energyLevel},
				static_cast<ObjectActivity::EnergyLevel>(
					clamp(max(energyFromLinearSpeedB, energyFromAngularSpeedB), Coefficient{1}, static_cast<Coefficient>(ObjectActivity::MAX_ENERGY_LEVEL))));
		}
	}
}

template <size_t N, size_t ColorIndex, bool UseBias>
void solveColoredContactConstraints(execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Position<N>, const Orientation<N>, const Scale<N>,
										const ObjectFlags, const Material, const InverseMass, const InverseMomentOfInertiaTensor<N>>
										objectEntities,
	execution::Chunk<const ContactColorGraph<N>, ContactColorGraph<N>::template getColoredContactIndices<ColorIndex>> coloredContacts, Contacts<N>& contacts,
	const SimulationOptions<N>& simulationOptions) {
	solveContactConstraintsImplementation<N>(objectEntities, coloredContacts, contacts, simulationOptions, UseBias);
}

template <size_t N, bool UseBias>
void solveOverflowContactConstraints(execution::Entities<ObjectActivity, LinearVelocity<N>, AngularVelocity<N>, const Position<N>, const Orientation<N>, const Scale<N>,
										 const ObjectFlags, const Material, const InverseMass, const InverseMomentOfInertiaTensor<N>>
										 objectEntities,
	const ContactColorOverflow& contactColorOverflow, Contacts<N>& contacts, const SimulationOptions<N>& simulationOptions) {
	solveContactConstraintsImplementation<N>(objectEntities, contactColorOverflow, contacts, simulationOptions, UseBias);
}

template <size_t N>
void integratePositions(execution::Entities<Position<N>, const LinearVelocity<N>, const ObjectActiveTag> activeObjectEntities, const SimulationOptions<N>& simulationOptions) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);

	for (auto&& [objectID, position, linearVelocity, activeTag] : activeObjectEntities) {
		position += linearVelocity * deltaTime;
		GREM_ASSERT(all(isfinite(position)));
	}
}

template <size_t N>
void integrateOrientations(execution::Entities<Orientation<N>, const AngularVelocity<N>, const ObjectActiveTag> activeObjectEntities,
	const SimulationOptions<N>& simulationOptions) {
	const Time deltaTime = simulationOptions.stepInterval / static_cast<float>(simulationOptions.subStepCount);

	for (auto&& [objectID, orientation, angularVelocity, activeTag] : activeObjectEntities) {
		orientation += angularVelocity * deltaTime;
		GREM_ASSERT(all(isfinite(orientation)));
	}
}

template <size_t N>
void applyContactRestitutionImplementation(
	execution::Entities<LinearVelocity<N>, AngularVelocity<N>, ObjectActivity, const Position<N>, const Orientation<N>, const Scale<N>, const ObjectFlags, const Material,
		const InverseMass, const InverseMomentOfInertiaTensor<N>, const LinearAcceleration<N>>
		objectEntities,
	Span<const ContactIndex> contactIndices, Contacts<N>& contacts, const SimulationOptions<N>& simulationOptions) {
	const Time stepInterval = simulationOptions.stepInterval;

	for (const ContactIndex contactIndex : contactIndices) {
		auto&& [objectIDs, contact] = contacts.getAtIndex(contactIndex);

		auto&& [objectIDA, linearVelocityA, angularVelocityA, activityA, positionA, orientationA, scaleA, flagsA, materialA, inverseMassA, inverseMomentOfInertiaTensorA,
			gravityAccelerationA] = objectEntities[objectIDs.first];
		auto&& [objectIDB, linearVelocityB, angularVelocityB, activityB, positionB, orientationB, scaleB, flagsB, materialB, inverseMassB, inverseMomentOfInertiaTensorB,
			gravityAccelerationB] = objectEntities[objectIDs.second];

		const bool isCorrectableA = activityA.isCorrectable != 0;
		const bool isCorrectableB = activityB.isCorrectable != 0;

		const Coefficient restitution = calculateCombinedRestitution(materialA.restitution, materialB.restitution, materialA.restitutionCombine, materialB.restitutionCombine);
		const LinearVelocity<N> gravityInducedRelativeVelocity = 2.0f * (gravityAccelerationA - gravityAccelerationB) * stepInterval;

		for (ContactManifold<N>& manifold : contact.manifolds) {
			if (!manifold.filterTestResult.respondsToCollision()) {
				continue;
			}

			const Direction<N> normal = manifold.normal;
			size_t iterationCount{};
			switch (manifold.points.size()) {
				case 0: iterationCount = 0; break;
				case 1: iterationCount = 1; break;
				default: iterationCount = 8; break;
			}

			LinearImpulse1D totalRestitutionImpulse{};
			for (size_t iterationIndex = 0; iterationIndex < iterationCount; ++iterationIndex) {
				for (ContactPoint<N>& point : manifold.points) {
					const LinearVelocity1D preSolveRelativeNormalVelocity = getNormalComponent(point.relativeVelocityInTangentSpace);
					const LinearVelocity1D gravityInducedRelativeNormalVelocity = dot(gravityInducedRelativeVelocity, normal);
					if (restitution * preSolveRelativeNormalVelocity <= gravityInducedRelativeNormalVelocity || getNormalComponent(point.impulseInTangentSpace) == 0) {
						continue;
					}

					const Length<N> offsetA = orientationA(scaleA * point.localOffsets.first);
					const Length<N> offsetB = orientationB(scaleB * point.localOffsets.second);

					const MomentArm<N> momentArmA = cross(offsetA, normal);
					const MomentArm<N> momentArmB = cross(offsetB, normal);
					const InverseMass inverseEffectiveMassA = inverseMassA + dot(momentArmA, inverseMomentOfInertiaTensorA * momentArmA);
					const InverseMass inverseEffectiveMassB = inverseMassB + dot(momentArmB, inverseMomentOfInertiaTensorB * momentArmB);
					const InverseMass combinedInverseEffectiveMass = inverseEffectiveMassA + inverseEffectiveMassB;
					const Mass combinedEffectiveMass = (combinedInverseEffectiveMass == 0) ? Mass{} : inverse(combinedInverseEffectiveMass);

					const LinearVelocity1D relativeNormalVelocity =
						dot(normal, (linearVelocityA + cross(angularVelocityA, offsetA)) - (linearVelocityB + cross(angularVelocityB, offsetB)));
					const LinearMomentum1D restitutionMomentum = point.restitutionMomentum;
					const LinearImpulse1D desiredRestitutionImpulse = combinedEffectiveMass * -(relativeNormalVelocity + restitution * preSolveRelativeNormalVelocity);
					const LinearMomentum1D newRestitutionMomentum = min(restitutionMomentum + desiredRestitutionImpulse, LinearMomentum1D{});
					const LinearImpulse1D restitutionImpulse = newRestitutionMomentum - restitutionMomentum;

					if (restitutionImpulse != 0) {
						const LinearImpulse<N> linearImpulse = normal * restitutionImpulse;
						if (isCorrectableA) {
							// Note: This parallel mutation is safe thanks to graph coloring.
							linearVelocityA += inverseMassA * linearImpulse;
							angularVelocityA += inverseMomentOfInertiaTensorA * (momentArmA * restitutionImpulse);
							GREM_ASSERT(all(isfinite(linearVelocityA)));
							GREM_ASSERT(all(isfinite(angularVelocityA)));
						}
						if (isCorrectableB) {
							// Note: This parallel mutation is safe thanks to graph coloring.
							linearVelocityB -= inverseMassB * linearImpulse;
							angularVelocityB -= inverseMomentOfInertiaTensorB * (momentArmB * restitutionImpulse);
							GREM_ASSERT(all(isfinite(linearVelocityB)));
							GREM_ASSERT(all(isfinite(angularVelocityB)));
						}

						// Note: This parallel mutation is safe thanks to graph coloring.
						point.restitutionMomentum = newRestitutionMomentum;
						point.restitutionImpulse += restitutionImpulse;
					}
				}
			}
		}
	}
}

template <size_t N, size_t ColorIndex>
void applyColoredContactRestitution(execution::Entities<LinearVelocity<N>, AngularVelocity<N>, ObjectActivity, const Position<N>, const Orientation<N>, const Scale<N>,
										const ObjectFlags, const Material, const InverseMass, const InverseMomentOfInertiaTensor<N>, const LinearAcceleration<N>>
										objectEntities,
	execution::Chunk<const ContactColorGraph<N>, ContactColorGraph<N>::template getColoredContactIndices<ColorIndex>> coloredContacts, Contacts<N>& contacts,
	const SimulationOptions<N>& simulationOptions) {
	applyContactRestitutionImplementation<N>(objectEntities, coloredContacts, contacts, simulationOptions);
}

template <size_t N>
void applyOverflowContactRestitution(execution::Entities<LinearVelocity<N>, AngularVelocity<N>, ObjectActivity, const Position<N>, const Orientation<N>, const Scale<N>,
										 const ObjectFlags, const Material, const InverseMass, const InverseMomentOfInertiaTensor<N>, const LinearAcceleration<N>>
										 objectEntities,
	const ContactColorOverflow& contactColorOverflow, Contacts<N>& contacts, const SimulationOptions<N>& simulationOptions) {
	applyContactRestitutionImplementation<N>(objectEntities, contactColorOverflow, contacts, simulationOptions);
}

template <size_t N>
void updateMomentOfInertiaTensors(
	execution::Entities<MomentOfInertiaTensor<N>, const InversePrincipalMomentsOfInertia<N>, const LocalInertiaOrientation<N>, const Orientation<N>, const ObjectActiveTag>
		activeObjectEntities) {
	for (auto&& [objectID, momentOfInertiaTensor, inversePrincipalMomentsOfInertia, localInertiaOrientation, orientation, activeTag] : activeObjectEntities) {
		momentOfInertiaTensor = calculateMomentOfInertiaTensor(calculateMomentOfInertia(inversePrincipalMomentsOfInertia), localInertiaOrientation, orientation);
	}
}

template <size_t N>
void updateInverseMomentOfInertiaTensors(
	execution::Entities<InverseMomentOfInertiaTensor<N>, const InversePrincipalMomentsOfInertia<N>, const LocalInertiaOrientation<N>, const Orientation<N>, const ObjectActiveTag>
		activeObjectEntities) {
	for (auto&& [objectID, inverseMomentOfInertiaTensor, inversePrincipalMomentsOfInertia, localInertiaOrientation, orientation, activeTag] : activeObjectEntities) {
		inverseMomentOfInertiaTensor = calculateInverseMomentOfInertiaTensor(inversePrincipalMomentsOfInertia, localInertiaOrientation, orientation);
	}
}

template <size_t N>
void updateObjectEnergyLevels(
	execution::Entities<ObjectActivity, const InverseMass, const MomentOfInertiaTensor<N>, const LinearVelocity<N>, const AngularVelocity<N>, const ObjectActiveTag>
		activeObjectEntities,
	const SimulationOptions<N>& simulationOptions) {
	const Quantity<1, Reciprocal<Energy::Unit>> inverseEnergyLevelUnit = inverse(simulationOptions.energyLevelUnit);
	for (auto&& [objectID, activity, inverseMass, momentOfInertiaTensor, linearVelocity, angularVelocity, activeTag] : activeObjectEntities) {
		const Energy linearKineticEnergy = (inverseMass == 0) ? Energy{} : Energy{(0.5f / inverseMass) * length2(linearVelocity)};
		const Energy angularKineticEnergy = 0.5f * dot(angularVelocity, momentOfInertiaTensor * angularVelocity);
		activity.energyLevel = max(ObjectActivity::EnergyLevel{activity.energyLevel},
			static_cast<ObjectActivity::EnergyLevel>(
				clamp(max(linearKineticEnergy, angularKineticEnergy) * inverseEnergyLevelUnit, Coefficient{1}, static_cast<Coefficient>(ObjectActivity::MAX_ENERGY_LEVEL))));
	}
}

template <size_t N>
void scheduleStep(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions, const ScheduleStepOptions<N>& scheduleStepOptions) {
	const execution::Task::ParallelCount parallelism = clamp(simulationOptions.targetParallelism, execution::Task::ParallelCount{1}, execution::Task::ParallelCount{64});

	scheduler.template addParallelTransformationTask<updateObjectActivityAndBounds<N>>(parallelism, "Update object activity and bounds");
	scheduler.template addTask<clearEvents<N>>("Clear events");

	scheduleStepOptions.addPreBroadphaseUpdateTasks(scheduler);
	scheduler.template addTask<updateBroadphase<N>>("Update broadphase");
	scheduleStepOptions.addPostBroadphaseUpdateTasks(scheduler);

	scheduler.template addTask<removeInvalidatedContacts<N>>("Remove invalidated contacts");
	scheduler.template addTask<updateActiveObjectSet<N>>("Update active object set");
	scheduler.template addTask<addNewContacts<N>>("Add new contacts");
	scheduler.template addTask<gatherActiveContacts<N>>("Gather active contacts");
	scheduler.template addTask<colorContacts<N>>("Color contacts");

	scheduleStepOptions.addPreCollisionDetectionTasks(scheduler);
	scheduler.template addUnsafeParallelTask<detectPredictedCollisions<N>>(parallelism, "Detect predicted collisions");
	scheduler.template addTask<removeGhostContactManifolds<N>>("Remove ghost contact manifolds");
	scheduleStepOptions.addPostCollisionDetectionTasks(scheduler);

	scheduler.template addTask<emitCollisionAndSeparationEvents<N>>("Emit collision and separation events");
	scheduler.template addTask<activateSeparatedObjects<N>>("Activate separated objects");
	scheduler.template addTask<updateActiveJointSet<N>>("Update active joint set");

	scheduleStepOptions.addPreSolverTasks(scheduler);

	scheduler.template addTask<decayJointConstraintMomentums<N>>("Decay joint constraint momentums");
	if constexpr (N == 3) {
		scheduler.template addTask<decay3DOnlyJointConstraintMomentums>("Decay 3D-only joint constraint momentums");
	}
	meta::forEachIndex<SimulationOptions<N>::MAX_CONTACT_COLOR_COUNT>([&](auto colorIndex) -> void { //
		if (colorIndex < simulationOptions.contactColorCount) {
			scheduler.template addUnsafeParallelTask<decayColoredContactConstraintMomentums<N, colorIndex>>(parallelism, "Decay colored contact constraint momentums");
		}
	});
	scheduler.template addTask<decayOverflowContactConstraintMomentums<N>>("Decay overflow contact constraint momentums");

	GREM_ASSERT(simulationOptions.subStepCount > 0);
	for (size_t subStepIndex = 0; subStepIndex < simulationOptions.subStepCount; ++subStepIndex) {
		scheduleStepOptions.addPreSolverIterationTasks(scheduler);

		scheduler.template addTask<integrateLinearVelocities<N>>("Integrate linear velocities");
		scheduler.template addTask<integrateAngularVelocities<N>>("Integrate angular velocities");

		scheduler.template addTask<warmstartJointConstraints<N>>("Warmstart joint constraints");
		if constexpr (N == 3) {
			scheduler.template addTask<warmstart3DOnlyJointConstraints>("Warmstart 3D-only joint constraints");
		}
		meta::forEachIndex<SimulationOptions<N>::MAX_CONTACT_COLOR_COUNT>([&](auto colorIndex) -> void { //
			if (colorIndex < simulationOptions.contactColorCount) {
				scheduler.template addUnsafeParallelTask<warmstartColoredContactConstraints<N, colorIndex>>(parallelism, "Warmstart colored contact constraints");
			}
		});
		scheduler.template addTask<warmstartOverflowContactConstraints<N>>("Warmstart overflow contact constraints");

		scheduler.template addTask<applyJointDrives<N>>("Apply joint drives");
		if constexpr (N == 3) {
			scheduler.template addTask<apply3DOnlyJointDrives>("Apply 3D-only joint drives");
		}

		scheduler.template addTask<solveJointConstraints<N, true>>("Solve joint constraints");
		if constexpr (N == 3) {
			scheduler.template addTask<solve3DOnlyJointConstraints<true>>("Solve 3D-only joint constraints");
		}
		meta::forEachIndex<SimulationOptions<N>::MAX_CONTACT_COLOR_COUNT>([&](auto colorIndex) -> void { //
			if (colorIndex < simulationOptions.contactColorCount) {
				scheduler.template addUnsafeParallelTask<solveColoredContactConstraints<N, colorIndex, true>>(parallelism, "Solve colored contact constraints");
			}
		});
		scheduler.template addTask<solveOverflowContactConstraints<N, true>>("Solve overflow contact constraints");

		scheduler.template addTask<integratePositions<N>>("Integrate positions");
		scheduler.template addTask<integrateOrientations<N>>("Integrate orientations");

		scheduler.template addTask<solveJointConstraints<N, false>>("Relax joint constraints");
		if constexpr (N == 3) {
			scheduler.template addTask<solve3DOnlyJointConstraints<false>>("Relax 3D-only joint constraints");
		}
		meta::forEachIndex<SimulationOptions<N>::MAX_CONTACT_COLOR_COUNT>([&](auto colorIndex) -> void { //
			if (colorIndex < simulationOptions.contactColorCount) {
				scheduler.template addUnsafeParallelTask<solveColoredContactConstraints<N, colorIndex, false>>(parallelism,
					formatString("Relax colored contact constraints (color {}/{})", colorIndex + 1, simulationOptions.contactColorCount));
			}
		});
		scheduler.template addTask<solveOverflowContactConstraints<N, false>>("Relax overflow contact constraints");

		scheduleStepOptions.addPostSolverIterationTasks(scheduler);
	}
	scheduleStepOptions.addPostSolverTasks(scheduler);

	scheduleStepOptions.addPreRestitutionTasks(scheduler);
	meta::forEachIndex<SimulationOptions<N>::MAX_CONTACT_COLOR_COUNT>([&](auto colorIndex) -> void { //
		if (colorIndex < simulationOptions.contactColorCount) {
			scheduler.template addUnsafeParallelTask<applyColoredContactRestitution<N, colorIndex>>(parallelism,
				formatString("Apply colored contact restitution (color {}/{})", colorIndex + 1, simulationOptions.contactColorCount));
		}
	});
	scheduler.template addTask<applyOverflowContactRestitution<N>>("Apply overflow contact restitution");
	scheduleStepOptions.addPostRestitutionTasks(scheduler);

	scheduler.template addTask<updateMomentOfInertiaTensors<N>>("Update moment of inertia tensors");
	scheduler.template addTask<updateInverseMomentOfInertiaTensors<N>>("Update inverse moment of inertia tensors");

	scheduler.template addParallelTransformationTask<updateObjectEnergyLevels<N>>(parallelism, "Update object energy levels");
}

template <size_t N>
void scheduleBroadphaseUpdate(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions) {
	(void)simulationOptions;

	scheduler.template addTask<updateBroadphase<N>>("Update broadphase");
}

template <size_t N>
void scheduleCollisionDetection(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions) {
	const execution::Task::ParallelCount parallelism = clamp(simulationOptions.targetParallelism, execution::Task::ParallelCount{1}, execution::Task::ParallelCount{64});

	scheduler.template addTask<removeInvalidatedContactsWithoutEmittingEventsOrAffectingActivity<N>>("Remove invalidated contacts");
	scheduler.template addTask<updateActiveObjectSet<N>>("Update active object set");
	scheduler.template addTask<addNewContacts<N>>("Add new contacts");
	scheduler.template addTask<gatherActiveContacts<N>>("Gather active contacts");
	scheduler.template addUnsafeParallelTask<detectCollisions<N>>(parallelism, "Detect collisions");
	scheduler.template addTask<removeGhostContactManifolds<N>>("Remove ghost contact manifolds");
}

} // namespace

void updateBroadphase2D(EntityRegistry2D& registry, ResourceRegistry2D& resources) {
	updateBroadphase<2>(registry.template getEntities<BroadphaseID, const Collider2D, const ObjectBounds2D>(), resources.template getResource<Broadphase2D>(),
		resources.template getResource<SimulationOptions2D>());
}

void updateBroadphase3D(EntityRegistry3D& registry, ResourceRegistry3D& resources) {
	updateBroadphase<3>(registry.template getEntities<BroadphaseID, const Collider3D, const ObjectBounds3D>(), resources.template getResource<Broadphase3D>(),
		resources.template getResource<SimulationOptions3D>());
}

void scheduleStep2D(Scheduler2D& scheduler, const SimulationOptions2D& simulationOptions, const ScheduleStepOptions2D& scheduleStepOptions) {
	scheduleStep<2>(scheduler, simulationOptions, scheduleStepOptions);
}

void scheduleStep3D(Scheduler3D& scheduler, const SimulationOptions3D& simulationOptions, const ScheduleStepOptions3D& scheduleStepOptions) {
	scheduleStep<3>(scheduler, simulationOptions, scheduleStepOptions);
}

void scheduleBroadphaseUpdate2D(Scheduler2D& scheduler, const SimulationOptions2D& simulationOptions) {
	scheduleBroadphaseUpdate<2>(scheduler, simulationOptions);
}

void scheduleBroadphaseUpdate3D(Scheduler3D& scheduler, const SimulationOptions3D& simulationOptions) {
	scheduleBroadphaseUpdate<3>(scheduler, simulationOptions);
}

void scheduleCollisionDetection2D(Scheduler2D& scheduler, const SimulationOptions2D& simulationOptions) {
	scheduleCollisionDetection<2>(scheduler, simulationOptions);
}

void scheduleCollisionDetection3D(Scheduler3D& scheduler, const SimulationOptions3D& simulationOptions) {
	scheduleCollisionDetection<3>(scheduler, simulationOptions);
}

} // namespace detail

} // namespace grem::physics
