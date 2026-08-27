// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_WORLD_HPP
#define GREM_EXAMPLES_TILES_WORLD_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/execution.hpp>

#include "Brushes.hpp"
#include "Contacts.hpp"
#include "Coordinate.hpp"
#include "Map.hpp"
#include "Schema.hpp"
#include "TileCategories.hpp"

struct World {
	using Position = MapPosition;

	struct PreviousPosition : Position {};

	struct LinearVelocity : vec2 {};

	struct Movement {
		vec2 desiredVelocity{};
		float minSpeed = 0.5f;
		float accelerationDuration = 0.15f;
	};

	struct Sprite {
		BrushID brushID{};
	};

	struct FourDirectionalMovementSprites {
		Array<BrushID, 4> idle{}; // Left, Right, Down, Up.
		Array<BrushID, 4> walk{}; // Left, Right, Down, Up.
		Array<BrushID, 4> run{};  // Left, Right, Down, Up.
	};

	struct Collider {
		float radius = 0.5f;
	};

	using EntityRegistry = exec::EntityRegistry< //
		Position,                                //
		PreviousPosition,                        //
		LinearVelocity,                          //
		Movement,                                //
		Sprite,                                  //
		FourDirectionalMovementSprites,          //
		Collider>;

	using ResourceRegistry = exec::ResourceTable< //
		Filesystem*,                              //
		Schema*,                                  //
		Map,                                      //
		Duration,                                 //
		Contacts>;

	EntityRegistry registry{};
	ResourceRegistry resources;
	exec::Schedule<EntityRegistry, ResourceRegistry> tickSchedule{};

	World(Filesystem& filesystem, Schema& schema)
		: resources(&filesystem, &schema, Map{filesystem, schema}, Duration{}, Contacts{}) {
		exec::Scheduler<EntityRegistry, ResourceRegistry> scheduler{};
		scheduler.addTask<storePreviousPositions>("Store previous positions");
		scheduler.addTask<controlMovement>("Control movement");
		scheduler.addTask<integrateLinearKinematics>("Integrate linear kinematics");
		scheduler.addTask<detectAndResolveCollisions>("Detect and resolve collisions");
		scheduler.addTask<moveMapEntities>("Move map entities");
		tickSchedule = scheduler.buildSchedule();
	}

	[[nodiscard]] exec::EntityID createPlayerEntity(Position position) {
		const Schema& schema = resources.getResource<Schema>();
		exec::EntityBuilder entityBuilder = registry.createEntity();
		entityBuilder.addComponent<Position>(position);
		entityBuilder.addComponent<PreviousPosition>(position);
		entityBuilder.addComponent<LinearVelocity>();
		entityBuilder.addComponent<Movement>();
		entityBuilder.addComponent<Sprite>(Sprite{.brushID = schema.brushIDs.at("PLAYER_IDLE_DOWN")});
		entityBuilder.addComponent<FourDirectionalMovementSprites>(FourDirectionalMovementSprites{
			.idle{
				schema.brushIDs.at("PLAYER_IDLE_LEFT"),
				schema.brushIDs.at("PLAYER_IDLE_RIGHT"),
				schema.brushIDs.at("PLAYER_IDLE_DOWN"),
				schema.brushIDs.at("PLAYER_IDLE_UP"),
			},
			.walk{
				schema.brushIDs.at("PLAYER_WALK_LEFT"),
				schema.brushIDs.at("PLAYER_WALK_RIGHT"),
				schema.brushIDs.at("PLAYER_WALK_DOWN"),
				schema.brushIDs.at("PLAYER_WALK_UP"),
			},
			.run{
				schema.brushIDs.at("PLAYER_RUN_LEFT"),
				schema.brushIDs.at("PLAYER_RUN_RIGHT"),
				schema.brushIDs.at("PLAYER_RUN_DOWN"),
				schema.brushIDs.at("PLAYER_RUN_UP"),
			},
		});
		entityBuilder.addComponent<Collider>();

		Map& map = resources.getResource<Map>();
		map.addEntity(entityBuilder.getEntityID(), position.getTileCoordinates());

		return entityBuilder.build();
	}

	exec::EntityID createFlagEntity(Position position) {
		const Schema& schema = resources.getResource<Schema>();
		exec::EntityBuilder entityBuilder = registry.createEntity();
		entityBuilder.addComponent<Position>(position);
		entityBuilder.addComponent<PreviousPosition>(position);
		entityBuilder.addComponent<Sprite>(Sprite{.brushID = schema.brushIDs.at("FLAG")});

		Map& map = resources.getResource<Map>();
		map.addEntity(entityBuilder.getEntityID(), position.getTileCoordinates());

		return entityBuilder.build();
	}

	void destroyEntity(exec::EntityID id) {
		if (const Position* const position = registry.findComponent<Position>(id)) {
			Map& map = resources.getResource<Map>();
			map.removeEntity(id, position->getTileCoordinates());
		}
		registry.destroyEntity(id);
	}

	[[nodiscard]] Optional<Position> getEntityDisplayPosition(exec::EntityID id, float interpolationAlpha) const {
		if (const Position* const position = registry.findComponent<Position>(id)) {
			if (const PreviousPosition* const previousPosition = registry.findComponent<PreviousPosition>(id)) {
				return Position{mix(previousPosition->coordinates, position->coordinates, interpolationAlpha), position->layer};
			}
			return *position;
		}
		return {};
	}

	void tick(exec::Executor& executor, Duration tickInterval) {
		GREM_PROFILE_FUNCTION();

		resources.getResource<Duration>() = tickInterval;
		executor.executeSchedule(tickSchedule, registry, resources);
	}

	void prepareForDisplay() {
		GREM_PROFILE_FUNCTION();

		updateFourDirectionalMovementSprites();
	}

private:
	static void storePreviousPositions(exec::Entities<PreviousPosition, const Position> entities) {
		for (auto&& [entityID, previousPosition, position] : entities) {
			previousPosition = PreviousPosition{position};
		}
	}

	static void controlMovement(exec::Entities<LinearVelocity, Movement, Position> entities, const Schema& schema, const Map& map, Duration tickInterval) {
		const float deltaTime = duration_cast<FloatSeconds>(tickInterval).count();

		for (auto&& [entityID, linearVelocity, movement, position] : entities) {
			float friction = 0.0f;

			// Look for something collideable at the current layer.
			const TileCategoryID tileCategoryID = map.getTile(position.getTileCoordinates()).categoryID;
			if (tileCategoryID && schema.tileCategories[tileCategoryID].collisionType != TileCollisionType::NONE) {
				friction = schema.tileCategories[tileCategoryID].friction;
			} else {
				// Find the first collideable tile below the current layer that doesn't have a solid tile above it.
				for (int32_t groundZ = position.layer; groundZ-- > 0;) {
					const TileCategoryID groundTileCategoryID = map.getTile(Position{position.coordinates, groundZ}.getTileCoordinates()).categoryID;
					if (groundTileCategoryID && schema.tileCategories[groundTileCategoryID].collisionType != TileCollisionType::NONE) {
						const TileCategoryID aboveTileCategoryID = map.getTile(position.getTileCoordinates()).categoryID;
						if (!aboveTileCategoryID || schema.tileCategories[aboveTileCategoryID].collisionType != TileCollisionType::SOLID) {
							friction = schema.tileCategories[groundTileCategoryID].friction;
							position.layer = groundZ + 1;
							break;
						}
					}
				}
				if (friction == 0.0f && tileCategoryID) {
					// No ground found. Just use the friction at the current layer.
					friction = schema.tileCategories[tileCategoryID].friction;
				}
			}

			const float currentSpeed = length(linearVelocity);
			const float potentialSpeedDelta = (length(movement.desiredVelocity) - currentSpeed) * friction * (deltaTime / movement.accelerationDuration);
			if ((movement.desiredVelocity == vec2{} && currentSpeed <= movement.minSpeed) || currentSpeed + potentialSpeedDelta <= 0) {
				linearVelocity = {};
			} else {
				const vec2 velocityRemaining = movement.desiredVelocity - vec2{linearVelocity};
				const vec2 addedVelocity = velocityRemaining * friction * (deltaTime / movement.accelerationDuration);
				linearVelocity += addedVelocity;
			}
		}
	}

	static void integrateLinearKinematics(exec::Entities<Position, const LinearVelocity> entities, Duration tickInterval) {
		const float deltaTime = duration_cast<FloatSeconds>(tickInterval).count();

		for (auto&& [entityID, position, linearVelocity] : entities) {
			position.coordinates += linearVelocity * deltaTime;
		}
	}

	static void detectAndResolveCollisions(exec::Entities<Position, LinearVelocity, const Collider> entities, Contacts& contacts, const Schema& schema, const Map& map,
		Duration tickInterval) {
		const float deltaTime = duration_cast<FloatSeconds>(tickInterval).count();

		for (auto&& [entityID, position, linearVelocity, collider] : entities) {
			const Coordinates2D colliderMin = position.coordinates - vec2{collider.radius};
			const Coordinates2D colliderMax = position.coordinates + vec2{collider.radius};
			const int32_t minTileX = colliderMin.x.getTileCoordinate();
			const int32_t minTileY = colliderMin.y.getTileCoordinate();
			const int32_t maxTileX = colliderMax.x.getTileCoordinate();
			const int32_t maxTileY = colliderMax.y.getTileCoordinate();

			contacts.contactPoints.clear();
			for (int64_t y = minTileY; y <= maxTileY; ++y) {
				for (int64_t x = minTileX; x <= maxTileX; ++x) {
					const TileCategoryID tileCategoryID = map.getTile(Offset3D{static_cast<int32_t>(x), static_cast<int32_t>(y), position.layer}).categoryID;
					if (tileCategoryID && schema.tileCategories[tileCategoryID].collisionType == TileCollisionType::SOLID) {
						const Box<2, Coordinate> tileShape{.min{static_cast<int32_t>(x), static_cast<int32_t>(y)}, .max{static_cast<int32_t>(x + 1), static_cast<int32_t>(y + 1)}};
						const ContactPoint contactPoint = findSmallestSeparation(tileShape, position.coordinates, collider.radius);
						contacts.contactPoints.push_back(contactPoint);
					}
				}
			}

			sort(contacts.contactPoints,
				[&](const ContactPoint& a, const ContactPoint& b) -> bool { return a.getPenetrationDepth(position.coordinates) > b.getPenetrationDepth(position.coordinates); });

			contacts.voidedVertices.clear();
			for (ContactPoint& contactPoint : contacts.contactPoints) {
				const float penetrationDepth = contactPoint.getPenetrationDepth(position.coordinates);
				if (penetrationDepth > 0.0f) {
					if (contactPoint.featureVerticesA.size() >= 2) {
						for (const Coordinates2D vertex : contactPoint.featureVerticesA) {
							contacts.voidedVertices.push_back(vertex);
						}
					} else if (contactPoint.featureVerticesA.size() == 1 && contains(contacts.voidedVertices, contactPoint.featureVerticesA.front())) {
						continue;
					}
					const vec2 correction = contactPoint.normal * penetrationDepth;
					linearVelocity += correction / deltaTime;
					position.coordinates += correction;
				}
			}
		}
	}

	static void moveMapEntities(exec::Entities<const Position, const PreviousPosition> entities, Map& map) {
		for (auto&& [entityID, position, previousPosition] : entities) {
			const Offset3D oldPosition = previousPosition.getTileCoordinates();
			const Offset3D newPosition = position.getTileCoordinates();
			if (newPosition != oldPosition) {
				map.moveEntity(entityID, oldPosition, newPosition);
			}
		}
	}

	void updateFourDirectionalMovementSprites() {
		constexpr float MAX_IDLE_SPEED = 1.0f;
		constexpr float MIN_RUNNING_SPEED = 6.0f;
		constexpr Array<vec2, 4> DIRECTIONS{{
			{-1.0f, 0.0f},
			{1.0f, 0.0f},
			{0.0f, -1.0f},
			{0.0f, 1.0f},
		}};

		for (auto&& [entityID, sprite, linearVelocity, movement, fourDirectionalMovementSprites] :
			registry.getEntities<Sprite, const LinearVelocity, const Movement, const FourDirectionalMovementSprites>()) {
			const Array leftBrushIDs{fourDirectionalMovementSprites.idle[0], fourDirectionalMovementSprites.walk[0], fourDirectionalMovementSprites.run[0]};
			const Array rightBrushIDs{fourDirectionalMovementSprites.idle[1], fourDirectionalMovementSprites.walk[1], fourDirectionalMovementSprites.run[1]};
			const Array downBrushIDs{fourDirectionalMovementSprites.idle[2], fourDirectionalMovementSprites.walk[2], fourDirectionalMovementSprites.run[2]};
			const Array upBrushIDs{fourDirectionalMovementSprites.idle[3], fourDirectionalMovementSprites.walk[3], fourDirectionalMovementSprites.run[3]};
			size_t directionIndex{};
			if (contains(leftBrushIDs, sprite.brushID)) {
				directionIndex = 0;
			} else if (contains(rightBrushIDs, sprite.brushID)) {
				directionIndex = 1;
			} else if (contains(downBrushIDs, sprite.brushID)) {
				directionIndex = 2;
			} else if (contains(upBrushIDs, sprite.brushID)) {
				directionIndex = 3;
			}

			const vec2 desiredHorizontalLinearVelocity{movement.desiredVelocity.x, movement.desiredVelocity.y};
			const float desiredHorizontalSpeed = length(desiredHorizontalLinearVelocity);
			if (dot(desiredHorizontalLinearVelocity, DIRECTIONS[directionIndex]) < desiredHorizontalSpeed * cos(convertDegreesToRadians(45.0001f))) {
				if (abs(desiredHorizontalLinearVelocity.x) >= abs(desiredHorizontalLinearVelocity.y)) {
					if (signbit(desiredHorizontalLinearVelocity.x)) {
						directionIndex = 0;
					} else {
						directionIndex = 1;
					}
				} else {
					if (signbit(desiredHorizontalLinearVelocity.y)) {
						directionIndex = 2;
					} else {
						directionIndex = 3;
					}
				}
			}

			const vec2 horizontalLinearVelocity{linearVelocity.x, linearVelocity.y};
			const float horizontalSpeed = length(horizontalLinearVelocity);
			if (horizontalSpeed == 0.0f || (desiredHorizontalSpeed == 0.0f && horizontalSpeed <= MAX_IDLE_SPEED)) {
				sprite.brushID = fourDirectionalMovementSprites.idle[directionIndex];
			} else if (horizontalSpeed < MIN_RUNNING_SPEED) {
				sprite.brushID = fourDirectionalMovementSprites.walk[directionIndex];
			} else {
				sprite.brushID = fourDirectionalMovementSprites.run[directionIndex];
			}
		}
	}
};

#endif
