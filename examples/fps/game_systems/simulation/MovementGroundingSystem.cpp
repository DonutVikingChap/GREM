// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/Broadphase.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/Simulation.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../PlayerEntityMap.hpp"
#include "../../System.hpp"
#include "../../game_components.hpp"
#include "../../game_events.hpp"

class MovementGroundingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<updateMovementGrounding>("Update movement grounding");
	}

private:
	static constexpr phys::Angle MAX_SLOPE_ANGLE = 45_degrees;

	struct CollisionResolution {
		phys::Length3D positionCorrection{};
		phys::LinearVelocity3D linearVelocityCorrection{};
	};

	[[nodiscard]] static CollisionResolution getCollisionResolution(phys::Position3D positionA, phys::Orientation3D orientationA, phys::Scale3D scaleA,
		phys::LinearVelocity3D linearVelocityA, phys::InverseMass inverseMassA, const phys::InverseMomentOfInertiaTensor3D& inverseMomentOfInertiaTensorA, phys::EntityID objectIDB,
		const Pair<phys::Length3D>& localOffsets, phys::Scale3D normal,
		Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D> objectEntities,
		Entities<const phys::LinearVelocity3D, const phys::InverseMass, const phys::InverseMomentOfInertiaTensor3D> kinematicsEntities) {
		CollisionResolution result{};
		const auto& [objectEntityIDB, positionB, orientationB, scaleB, colliderB, boundsB] = objectEntities[objectIDB];
		const phys::Length3D offsetA = orientationA(scaleA * localOffsets.first);
		const phys::Length3D offsetB = orientationB(scaleB * localOffsets.second);
		const phys::Position3D pointA = positionA + offsetA;
		const phys::Position3D pointB = positionB + offsetB;
		const phys::Length1D penetrationDepth = dot(pointA - pointB, normal);
		if (penetrationDepth > 0) {
			result.positionCorrection = -penetrationDepth * normal;
			if (kinematicsEntities.containsEntity(objectIDB)) {
				const auto& [kinematicEntityIDB, linearVelocityB, inverseMassB, inverseMomentOfInertiaTensorB] = kinematicsEntities[objectIDB];
				const phys::MomentArm3D momentArmA = cross(offsetA, normal);
				const phys::MomentArm3D momentArmB = cross(offsetB, normal);
				const phys::InverseMass generalizedInverseMassA = inverseMassA + dot(momentArmA, inverseMomentOfInertiaTensorA * momentArmA);
				const phys::InverseMass generalizedInverseMassB = inverseMassB + dot(momentArmB, inverseMomentOfInertiaTensorB * momentArmB);
				const phys::InverseMass combinedGeneralizedInverseMass = generalizedInverseMassA + generalizedInverseMassB;
				const phys::Mass combinedGeneralizedMass = (combinedGeneralizedInverseMass > 0) ? 1.0f / combinedGeneralizedInverseMass : phys::Mass{};
				const phys::LinearVelocity1D relativeNormalVelocity = dot(linearVelocityA - linearVelocityB, normal);
				const phys::LinearImpulse1D normalImpulse = -relativeNormalVelocity * combinedGeneralizedMass;
				result.linearVelocityCorrection = inverseMassA * normalImpulse * normal;
			} else {
				result.linearVelocityCorrection = -dot(linearVelocityA, normal) * normal;
			}
		}
		return result;
	}

	static void updateMovementGrounding(
		Entities<MovementState, phys::Position3D, phys::LinearVelocity3D, const phys::LinearAcceleration3D, const phys::InverseMass, const phys::InverseMomentOfInertiaTensor3D,
			const phys::Volume, const phys::FluidDensity, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const MovementType, const phys::ObjectContacts3D>
			movementEntities,
		Entities<Aim> aimEntities, Entities<const PlayerID> playerEntities,
		Entities<const phys::Position3D, const phys::Orientation3D, const phys::Scale3D, const phys::Collider3D, const phys::ObjectBounds3D> objectEntities,
		Entities<const phys::LinearVelocity3D, const phys::InverseMass, const phys::InverseMomentOfInertiaTensor3D> kinematicsEntities, Events& events,
		const phys::Broadphase3D& broadphase, const phys::SimulationOptions3D& simulationOptions, const Schema& schema, TickIndex tickIndex) {
		const phys::Time deltaTime = simulationOptions.stepInterval;
		const phys::Scale1D minGroundNormalY = cos(MAX_SLOPE_ANGLE);

		for (auto&& [entityID, aim] : aimEntities) {
			aim.decayingVisualOffset = expDecay(aim.decayingVisualOffset, 16_per_second, deltaTime);
		}

		for (auto&& [entityID, movementState, position, linearVelocity, gravityAcceleration, inverseMass, inverseMomentOfInertiaTensor, volume, fluidDensity, orientation, scale,
				 collider, movementType, objectContacts] : movementEntities) {
			if (!collider.shape.isConvexShapeType()) {
				continue;
			}

			const phys::ConvexShapeView3D shape{collider.shape};
			const Optional<phys::Box3D> transformedBoundingBox = shape.getBoundingBox(translateRotateScale(position, orientation, scale));
			const Optional<phys::Box3D> untransformedBoundingBox = shape.getBoundingBox(phys::Transformation3D{});
			if (!transformedBoundingBox || !untransformedBoundingBox) {
				continue;
			}
			const phys::Length3D shapeSize = transformedBoundingBox->max - transformedBoundingBox->min;
			const phys::Distance shapeSizeMinComponent = minComponent(shapeSize);
			const phys::Distance shapeTolerance = 0.1_x * shapeSizeMinComponent;
			const phys::Distance shapeHeight = shapeSize.getY();
			const phys::Distance halfShapeHeight = 0.5_x * shapeHeight;
			const phys::Distance baseHeight = untransformedBoundingBox->max.getY() - untransformedBoundingBox->min.getY();

			const MovementDescription& movementDescription = schema.getMovementDescription(movementType);
			const phys::Distance maxStepHeight = min(baseHeight * movementDescription.stepHeightCoefficient, halfShapeHeight);

			const phys::Length3D removeFeetShapeOffset{0, 0.5_x * maxStepHeight, 0};
			const phys::Scale3D removeFeetShapeScale{1_x, (shapeHeight - maxStepHeight) / shapeHeight, 1_x};

			// Rewind our position and velocity to the old state at the start of the tick, plus the full predicted movement across the tick,
			// then resolve collisions manually using a modified shape scale that removes our feet (the feet being the lowermost maxStepHeight part of the shape),
			// and finally do a shapecast downwards to find ground level.
			// The shapecast has a maximum range of 1 maxStepHeight if we're in the air, and 2 if we're on the ground to allow smoothly stepping up and down stairs.

			const phys::Force3D buoyancyForce = fluidDensity * gravityAcceleration * volume * product(scale);
			const phys::LinearVelocity3D predictedLinearVelocity = movementState.oldLinearVelocity + (inverseMass * buoyancyForce + gravityAcceleration) * deltaTime;
			const phys::Length3D predictedMotion = predictedLinearVelocity * deltaTime;
			const phys::Distance predictedMotionDistance = length(predictedMotion);
			const phys::Position3D predictedPosition = movementState.oldPosition + predictedMotion;
			position = predictedPosition;
			linearVelocity = predictedLinearVelocity;

			if (predictedMotionDistance > shapeSizeMinComponent * 0.3_x) {
				// We're going really fast. Do an initial shapecast along our predicted motion to make sure we don't go through thin walls.
				const phys::Length3D shapeSizeWithoutFeet = shapeSize * removeFeetShapeScale;
				const phys::Length3D extraShapeOffset = removeFeetShapeOffset + phys::Length3D{0, shapeTolerance, 0};
				const phys::Scale3D extraShapeScale = removeFeetShapeScale * (shapeSizeWithoutFeet - 2_x * shapeTolerance) / shapeSizeWithoutFeet;
				const phys::Direction3D predictedMotionDirection = phys::Direction3D::reinterpret(predictedMotion / predictedMotionDistance);
				const Optional<phys::Broadphase3D::ShapecastResult> shapecastResult = broadphase.shapecastClosestHit(shape, collider.filter,
					translateRotateScale(movementState.oldPosition + extraShapeOffset, orientation, scale * extraShapeScale), predictedMotionDirection, predictedMotionDistance,
					objectEntities, simulationOptions.collisionAlgorithmOptions, phys::CollisionFilterTest::RESPONSE,
					[&](EntityID otherObjectID) -> bool { return otherObjectID != entityID; });
				if (shapecastResult && shapecastResult->distance != 0) {
					const CollisionResolution collisionResolution = getCollisionResolution(position + extraShapeOffset, orientation, scale * extraShapeScale, linearVelocity,
						inverseMass, inverseMomentOfInertiaTensor, shapecastResult->objectID, shapecastResult->localOffsets, -shapecastResult->normal, objectEntities,
						kinematicsEntities);
					position += collisionResolution.positionCorrection;
					linearVelocity += collisionResolution.linearVelocityCorrection;
				}
			}

			const auto resolveCollisions = [&](phys::Length3D extraShapeOffset, phys::Scale3D extraShapeScale) -> Optional<phys::Direction3D> {
				Optional<phys::Direction3D> mostVerticalNormal{};
				broadphase.collide(
					shape, collider.filter, translateRotateScale(position + extraShapeOffset, orientation, scale * extraShapeScale), objectEntities,
					simulationOptions.collisionAlgorithmOptions, phys::CollisionFilterTest::RESPONSE,
					[&](const phys::Broadphase3D::CollisionResult& collision) -> bool {
						for (const phys::ContactPoint3D& point : collision.manifold.points) {
							const CollisionResolution collisionResolution = getCollisionResolution(position + extraShapeOffset, orientation, scale * extraShapeScale,
								linearVelocity, inverseMass, inverseMomentOfInertiaTensor, collision.objectID, point.localOffsets, collision.manifold.normal, objectEntities,
								kinematicsEntities);
							position += collisionResolution.positionCorrection;
							linearVelocity += collisionResolution.linearVelocityCorrection;
							if (!mostVerticalNormal || -collision.manifold.normal.getY() > mostVerticalNormal->getY()) {
								mostVerticalNormal = -collision.manifold.normal;
							}
						}
						return false;
					},
					[&](EntityID otherEntityID) -> bool { return otherEntityID != entityID; });
				return mostVerticalNormal;
			};

			// Resolve collisions (ignoring our feet).
			resolveCollisions(removeFeetShapeOffset, removeFeetShapeScale);

			// Do a downwards shapecast to find the ground.
			const phys::Length3D extraGroundingOffset{0, maxStepHeight - shapeTolerance, 0};
			const phys::Scale3D extraGroundingScale = (shapeSize - 2_x * shapeTolerance) / shapeSize;
			const phys::Distance castDistance = (movementState.groundNormal) ? 2_x * maxStepHeight : maxStepHeight;
			Optional<phys::Broadphase3D::ShapecastResult> shapecastResult = broadphase.shapecastClosestHit(shape, collider.filter,
				translateRotateScale(position + extraGroundingOffset, orientation, scale * extraGroundingScale), -phys::Y_AXIS_3D, castDistance, objectEntities,
				simulationOptions.collisionAlgorithmOptions, phys::CollisionFilterTest::RESPONSE, [&](EntityID otherObjectID) -> bool { return otherObjectID != entityID; });

			if (!shapecastResult) {
				// We're in the air.
				movementState.groundNormal.reset();
				continue;
			}

			if (shapecastResult->distance == 0) {
				// The shapecast got stuck somehow (probably in a ceiling), despite being scaled down by shapeTolerance.
				// Resolve collisions again using the full vertical shape, and use the most vertical collision normal to determine the new ground normal.
				const Optional<phys::Direction3D> mostVerticalNormal =
					resolveCollisions(phys::Length3D{}, phys::Scale3D{extraGroundingScale.getX(), 1_x, extraGroundingScale.getZ()});
				if (mostVerticalNormal && mostVerticalNormal->getY() >= minGroundNormalY) {
					movementState.groundNormal = *mostVerticalNormal;
				} else {
					movementState.groundNormal.reset();
				}
				continue;
			}

			if (shapecastResult->normal.getY() < minGroundNormalY) {
				// We detected ground below us, but it's too sloped to be walkable.
				// Resolve collisions again using the full vertical shape.
				// Then, do a second shapecast to check if going a bit further along the horizontal correction direction would make the ground walkable.
				// If it would, undo the correction and pretend we can still walk on the slope (with a perfectly vertical ground normal).
				const phys::Position3D positionBeforeResolve = position;
				const phys::LinearVelocity3D linearVelocityBeforeResolve = linearVelocity;
				resolveCollisions(phys::Length3D{}, phys::Scale3D{extraGroundingScale.getX(), 1_x, extraGroundingScale.getZ()});

				bool walkable = false;
				Optional<phys::Direction2D> horizontalDirection = tryNormalize(positionBeforeResolve.get(phys::X, phys::Z) - position.get(phys::X, phys::Z));
				if (!horizontalDirection) {
					horizontalDirection = tryNormalize(linearVelocityBeforeResolve.get(phys::X, phys::Z));
				}
				if (horizontalDirection) {
					const phys::Distance extraHorizontalDistance = 1.707_x * shapeTolerance;
					if (const Optional<phys::Broadphase3D::ShapecastResult> secondShapecastResult = broadphase.shapecastClosestHit(shape, collider.filter,
							translateRotateScale(positionBeforeResolve + extraHorizontalDistance * horizontalDirection->get(phys::X, 0, phys::Y) + extraGroundingOffset,
								orientation, scale * extraGroundingScale),
							-phys::Y_AXIS_3D, castDistance, objectEntities, simulationOptions.collisionAlgorithmOptions, phys::CollisionFilterTest::RESPONSE,
							[&](EntityID otherObjectID) -> bool { return otherObjectID != entityID; })) {
						if (secondShapecastResult->distance != 0) {
							if (secondShapecastResult->normal.getY() >= minGroundNormalY) {
								walkable = true;
							} else if (abs(secondShapecastResult->distance - shapecastResult->distance) < extraHorizontalDistance) {
								walkable = true;
							}
						}
					}
				}

				if (!walkable) {
					movementState.groundNormal.reset();
					continue;
				}

				position = positionBeforeResolve;
				linearVelocity = linearVelocityBeforeResolve;
				shapecastResult->normal = phys::Y_AXIS_3D;
			}

			// We're on the ground. Perform stair stepping and update the ground normal.
			const phys::Length3D step{0, maxStepHeight - shapecastResult->distance, 0};
			if (movementState.groundNormal) {
				// We were already grounded. Step up/down stairs and other elevation changes.
				position += step;
				linearVelocity.setY(0);
				// Smoothly transition the visual aim position to its new position after stepping.
				if (aimEntities.containsEntity(entityID)) {
					aimEntities.getComponent<Aim>(entityID).decayingVisualOffset -= step;
				}
				movementState.lastLandingTickIndex = tickIndex;
				movementState.groundNormal = shapecastResult->normal;
			} else {
				if (linearVelocity.getY() > phys::Speed::MACHINE_EPSILON) {
					// We're leaving the ground. Don't stick to it.
					if (step.getY() > 0) {
						position += step;
					}
					movementState.groundNormal.reset();
				} else {
					// We just landed.
					position += step;
					linearVelocity.setY(0);

					// Play landing sound.
					const phys::Time timeSinceLastLanding = static_cast<float>(tickIndex - movementState.lastLandingTickIndex) * deltaTime;
					if (movementDescription.landingSoundType != SoundType{} && timeSinceLastLanding >= movementDescription.jumpRegroundDelay) {
						const PlayerID playerID = playerEntities.getComponentOr<PlayerID>(entityID, PlayerID{});
						if (playerID) {
							events.push_back(PlayerAssociatedWorldSpaceSoundPlayedEvent{
								.soundType = movementDescription.landingSoundType,
								.position = position - halfShapeHeight * phys::Y_AXIS_3D,
								.associatedPlayerID = playerID,
							});
						} else {
							events.push_back(WorldSpaceSoundPlayedEvent{
								.soundType = movementDescription.landingSoundType,
								.position = position - halfShapeHeight * phys::Y_AXIS_3D,
							});
						}
					}
					movementState.lastLandingTickIndex = tickIndex;
					movementState.groundNormal = shapecastResult->normal;
				}
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createMovementGroundingSystem() { // NOLINT(misc-use-internal-linkage)
	return new MovementGroundingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
