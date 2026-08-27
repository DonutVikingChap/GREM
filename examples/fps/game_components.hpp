// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_COMPONENTS_HPP
#define GREM_EXAMPLES_FPS_GAME_COMPONENTS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/execution/component.hpp>
#include <GREM/graphics_3d/Lights3D.hpp>
#include <GREM/physics/quantities.hpp>
#include <GREM/resource/Model.hpp>

#include "AssetCache.hpp"
#include "Flags.hpp"
#include "PlayerEntityMap.hpp"
#include "Schema.hpp"
#include "SynchronizedEntityMap.hpp"

template <typename Component>
struct ComponentTypeDeclaration {
	CStringView name;
};

//==============================================================================
// State components:
//==============================================================================

struct Aim {
	phys::Length3D offset{};
	phys::Length3D decayingVisualOffset{};
	phys::PitchYaw angles{};
	phys::PitchYawRates rotationRates{};

	[[nodiscard]] bool operator==(const Aim&) const = default;
};

struct SpriteAnimationState {
	Timestamp animationStartTimestamp{};

	[[nodiscard]] bool operator==(const SpriteAnimationState&) const = default;
};

struct FlashlightState {
	phys::Distance range = 100_meters;
	phys::Angle innerConeAngle = 22.5_degrees;
	phys::Angle outerConeAngle = 45_degrees;
	Color color = Color::WHITE * Color::fromAlpha(100.0f);
	bool on = false;

	[[nodiscard]] bool operator==(const FlashlightState&) const = default;
};

struct MovementState {
	enum Flag : uint8_t {
		JUMPING = 1 << 0,
		ALREADY_JUMPED = 1 << 1,
		CROUCHING = 1 << 2,
		SPRINTING = 1 << 3,
		FLYING = 1 << 4,
	};

	phys::Scale2D desiredDirectionScale{};
	TickIndex lastJumpTickIndex{};
	TickIndex lastLandingTickIndex{};
	phys::Coefficient crouchAmount{};
	phys::Time timeSpentChangingCrouchAmount{};
	phys::Angle bobbingPhase{};
	Flags<Flag> flags{};
	Optional<phys::Direction3D> groundNormal{};
	phys::Position3D oldPosition{};
	phys::LinearVelocity3D oldLinearVelocity{};

	[[nodiscard]] bool operator==(const MovementState&) const = default;
};

struct NPCInfo {
	phys::Speed targetSpeed = 10_meters_per_second;
	phys::Time accelerationDuration = 1_second;
	phys::AngularVelocity2D turnRate = 1_radian_per_second;

	[[nodiscard]] bool operator==(const NPCInfo&) const = default;
};

struct NPCState {
	phys::Angle targetAngle{};
	phys::Scale2D desiredDirectionScale{};
	float mood = 0.0f;

	[[nodiscard]] bool operator==(const NPCState&) const = default;
};

struct ModelPose {
	struct AnimationLayer {
		res::Model::AnimationIndex animationIndex;
		float blendWeight;

		[[nodiscard]] bool operator==(const AnimationLayer&) const = default;
	};

	phys::Time animationTime{};
	SmallArrayList<AnimationLayer, 2> animationLayers{};
	SmallArrayList<float, 8> morphTargetWeights{};

	[[nodiscard]] bool operator==(const ModelPose&) const = default;
};

struct ProjectileState {
	SynchronizedEntityID owner{};
	phys::Position3D position{};
	phys::Position3D previousPosition{};
	phys::LinearVelocity3D linearVelocity{};

	[[nodiscard]] bool operator==(const ProjectileState&) const = default;
};

struct WeaponState {
	enum Flag : uint8_t {
		PULLING_TRIGGER = 1 << 0,
		TRIGGER_CLICKED = 1 << 1,
		FIRING = 1 << 2,
		STARTING_RELOAD = 1 << 3,
		RELOADING = 1 << 4,
		AIMING_DOWN_SIGHTS = 1 << 5,
	};

	SynchronizedEntityID holder{};
	WeaponDescription::FireMode fireMode = WeaponDescription::FireMode::SAFE;
	uint16_t loadedRoundCount = 0;
	phys::Time smoothingInterpolationTime{};
	phys::LinearVelocity3D smoothedVelocity{};
	phys::LinearVelocity3D previousSmoothedVelocity = smoothedVelocity;
	phys::PitchYawRates smoothedAimAngularRates{};
	phys::PitchYawRates previousSmoothedAimAngularRates = smoothedAimAngularRates;
	phys::PitchYawRotations recoilInducedAimDeviation{};
	phys::PitchYawRotations rotationInducedAimDeviation{};
	Timestamp lastFiredTimestamp{};
	phys::Time drawTimeRemaining{};
	phys::Time reloadTimeRemaining{};
	phys::Coefficient decayedCrouchAmount{};
	phys::Coefficient aimingDownSightsAmount{};
	phys::PitchYawRates recoilStrengthOfLatestShot{};
	phys::PitchYawRates recoilAngularRates{};
	Flags<Flag> flags{};

	void initializeComponentDefault(ParseEntityComponentContext& context, const Schema& schema, const EntityInitializer& initializer) {
		WeaponType weaponType{};
		initializer.initializeComponent(weaponType, context, schema);
		if (weaponType != WeaponType{}) {
			const WeaponDescription& weaponDescription = schema.getWeaponDescription(weaponType);
			fireMode = weaponDescription.mainFireMode;
			loadedRoundCount = weaponDescription.maxMagazineCapacity;
			drawTimeRemaining = weaponDescription.drawDuration;
		}
	}

	[[nodiscard]] bool operator==(const WeaponState&) const = default;
};

struct ParticleState {
	SynchronizedEntityID owner{};

	[[nodiscard]] bool operator==(const ParticleState&) const = default;
};

struct DamageableState {
	TickIndex lastDamagedOnTickIndex{};

	[[nodiscard]] bool operator==(const DamageableState&) const = default;
};

struct Inventory {
	SynchronizedEntityID equippedWeapon{};

	[[nodiscard]] bool operator==(const Inventory&) const = default;
};

struct DecalAttachmentFrame {
	SynchronizedEntityID target{};
	phys::Length3D localOffset{};
	phys::Orientation3D localOrientation{};
	phys::Length2D size{1_meter};
	phys::Distance range = 0.2_meters;

	[[nodiscard]] bool operator==(const DecalAttachmentFrame&) const = default;
};

struct DestroyCountdown {
	TickIndex destroyOnTickIndex{};

	[[nodiscard]] bool operator==(const DestroyCountdown&) const = default;

	void initializeComponentDefault(ParseEntityComponentContext&, const Schema&, const EntityInitializer&) {
		destroyOnTickIndex = TickIndex{}.getNext(Limits<TickCount>::MAX);
	}
};

struct PlayerRespawnCountdown {
	TickIndex respawnOnTickIndex{};

	[[nodiscard]] bool operator==(const PlayerRespawnCountdown&) const = default;
};

struct Health {
	float health = 100.0f;

	[[nodiscard]] bool operator==(const Health&) const = default;
};

inline void initializeComponentDefault(phys::Scale3D& component, ParseEntityComponentContext&, const Schema&, const EntityInitializer&) {
	component = phys::Scale3D{1_x};
}

inline void initializeComponentDefault(phys::ObjectActivity& objectActivity, ParseEntityComponentContext&, const Schema&, const EntityInitializer&) {
	objectActivity = phys::ObjectActivity{phys::ObjectActivity::MAX_ENERGY_LEVEL};
}

struct ModelJointController {
	SynchronizedEntityID target{};
	res::Model::JointIndex jointIndex = 0;

	[[nodiscard]] bool operator==(const ModelJointController&) const = default;

	void setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources) { // NOLINT(readability-make-member-function-const)
		const SynchronizedEntityMap& synchronizedEntityMap = resources.getResource<SynchronizedEntityMap>();
		const EntityID targetEntityID = synchronizedEntityMap.findEntity(registry, target);
		if (!targetEntityID) {
			return;
		}
		const ModelType modelType = registry.getComponent<ModelType>(targetEntityID);
		const EntityType entityType = registry.getComponent<EntityType>(targetEntityID);
		setImpliedModelJointComponents(entityBuilder, modelType, entityType, jointIndex, registry, resources);
	}
};

struct ModelJointLight {
	SynchronizedEntityID target{};
	res::Model::JointIndex jointIndex = 0;
	res::Model::LightIndex lightIndex = 0;

	[[nodiscard]] bool operator==(const ModelJointLight&) const = default;

	void setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources) { // NOLINT(readability-make-member-function-const)
		if ((entityBuilder.getEntityID().getFlags() & ENTITY_CLIENTSIDE) == 0) {
			return;
		}
		AssetCache& assetCache = resources.getResource<AssetCache>();
		Schema& schema = resources.getResource<Schema>();
		const SynchronizedEntityMap& synchronizedEntityMap = resources.getResource<SynchronizedEntityMap>();
		const EntityID targetEntityID = synchronizedEntityMap.findEntity(registry, target);
		if (!targetEntityID) {
			return;
		}
		const ModelType modelType = registry.getComponent<ModelType>(targetEntityID);
		const ModelObjectDescription& modelObjectDescription = schema.loadModelObjectDescription(assetCache, modelType);
		if (lightIndex < modelObjectDescription.lightDescriptions.size()) {
			const ModelObjectDescription::LightDescription& lightDescription = modelObjectDescription.lightDescriptions[lightIndex];
			if (lightDescription.jointIndex == jointIndex) {
				GREM_MATCH(lightDescription.lightOptions) {
					GREM_CASE(const gfx::DirectionalLightOptions3D& directionalLightOptions) {
						entityBuilder.addOrAssignComponent<gfx::DirectionalLightOptions3D>(directionalLightOptions);
						break;
					}
					GREM_CASE(const gfx::PointLightOptions3D& pointLightOptions) {
						entityBuilder.addOrAssignComponent<gfx::PointLightOptions3D>(pointLightOptions);
						break;
					}
					GREM_CASE(const gfx::SpotLightOptions3D& spotLightOptions) {
						entityBuilder.addOrAssignComponent<gfx::SpotLightOptions3D>(spotLightOptions);
						break;
					}
				}
			}
		}
	}
};

struct JointConnectedObjects {
	SynchronizedEntityID first;
	SynchronizedEntityID second;

	[[nodiscard]] bool operator==(const JointConnectedObjects&) const = default;
};

constexpr Tuple VALID_STATE_COMPONENT_TYPES{
	ComponentTypeDeclaration<PlayerID>{"PlayerID"},
	ComponentTypeDeclaration<LocalPlayerID>{"LocalPlayerID"},
	ComponentTypeDeclaration<Aim>{"Aim"},
	ComponentTypeDeclaration<SpriteAnimationState>{"SpriteAnimationState"},
	ComponentTypeDeclaration<FlashlightState>{"FlashlightState"},
	ComponentTypeDeclaration<MovementState>{"MovementState"},
	ComponentTypeDeclaration<NPCInfo>{"NPCInfo"},
	ComponentTypeDeclaration<NPCState>{"NPCState"},
	ComponentTypeDeclaration<ModelPose>{"ModelPose"},
	ComponentTypeDeclaration<ProjectileState>{"ProjectileState"},
	ComponentTypeDeclaration<WeaponState>{"WeaponState"},
	ComponentTypeDeclaration<ParticleState>{"ParticleState"},
	ComponentTypeDeclaration<DamageableState>{"DamageableState"},
	ComponentTypeDeclaration<Inventory>{"Inventory"},
	ComponentTypeDeclaration<DecalAttachmentFrame>{"DecalAttachmentFrame"},
	ComponentTypeDeclaration<DestroyCountdown>{"DestroyCountdown"},
	ComponentTypeDeclaration<PlayerRespawnCountdown>{"PlayerRespawnCountdown"},
	ComponentTypeDeclaration<Health>{"Health"},
	ComponentTypeDeclaration<ModelJointController>{"ModelJointController"},
	ComponentTypeDeclaration<ModelJointLight>{"ModelJointLight"},
	ComponentTypeDeclaration<gfx::AmbientLightOptions3D>{"AmbientLightOptions3D"},
	ComponentTypeDeclaration<gfx::SunLightOptions3D>{"SunLightOptions3D"},
	ComponentTypeDeclaration<gfx::DirectionalLightOptions3D>{"DirectionalLightOptions3D"},
	ComponentTypeDeclaration<gfx::PointLightOptions3D>{"PointLightOptions3D"},
	ComponentTypeDeclaration<gfx::SpotLightOptions3D>{"SpotLightOptions3D"},
	ComponentTypeDeclaration<phys::Position3D>{"Position3D"},
	ComponentTypeDeclaration<phys::Orientation3D>{"Orientation3D"},
	ComponentTypeDeclaration<phys::Scale3D>{"Scale3D"},
	ComponentTypeDeclaration<phys::LinearVelocity3D>{"LinearVelocity3D"},
	ComponentTypeDeclaration<phys::AngularVelocity3D>{"AngularVelocity3D"},
	ComponentTypeDeclaration<phys::LinearAcceleration3D>{"LinearAcceleration3D"},
	ComponentTypeDeclaration<phys::ObjectActivity>{"ObjectActivity"},
	ComponentTypeDeclaration<JointConnectedObjects>{"JointConnectedObjects"},
	ComponentTypeDeclaration<phys::GenericJointOptions3D>{"GenericJointOptions3D"},
	ComponentTypeDeclaration<DecalMaterialType>{"DecalMaterialType"},
	ComponentTypeDeclaration<SpriteType>{"SpriteType"},
	ComponentTypeDeclaration<ModelType>{"ModelType"},
	ComponentTypeDeclaration<ProjectileType>{"ProjectileType"},
	ComponentTypeDeclaration<WeaponType>{"WeaponType"},
	ComponentTypeDeclaration<MovementType>{"MovementType"},
	ComponentTypeDeclaration<ParticleType>{"ParticleType"},
	ComponentTypeDeclaration<DamageableType>{"DamageableType"},
};

//==============================================================================
// Intermediate components:
//==============================================================================

struct ExcludeFromLightBakeTag {};

struct DestroyIfOutsideMapBoundsTag {};

struct TeleportIfPhysicsObjectOutsideMapBoundsTag {};

struct OrientPhysicsObjectByVelocity {
	phys::Orientation3D localOrientation{};
};

struct WeaponIntermediateState {
	size_t projectilesToFire = 0;
};

constexpr Tuple VALID_INTERMEDIATE_COMPONENT_TYPES{
	ComponentTypeDeclaration<ExcludeFromLightBakeTag>{"ExcludeFromLightBakeTag"},
	ComponentTypeDeclaration<DestroyIfOutsideMapBoundsTag>{"DestroyIfOutsideMapBoundsTag"},
	ComponentTypeDeclaration<TeleportIfPhysicsObjectOutsideMapBoundsTag>{"TeleportIfPhysicsObjectOutsideMapBoundsTag"},
	ComponentTypeDeclaration<OrientPhysicsObjectByVelocity>{"OrientPhysicsObjectByVelocity"},
	ComponentTypeDeclaration<WeaponIntermediateState>{"WeaponIntermediateState"},
};

//==============================================================================
// Clientside components:
//==============================================================================

struct LocalPlayerPerspective {
	phys::Position3D aimPosition{};
	phys::PitchYaw aimAngles{};
	phys::Position3D viewPosition{};
	phys::Position3D position{};
	phys::LinearVelocity3D linearVelocity{};
};

constexpr Tuple VALID_CLIENTSIDE_COMPONENT_TYPES{
	ComponentTypeDeclaration<LocalPlayerPerspective>{"LocalPlayerPerspective"},
};

//==============================================================================

static_assert(
	[]() -> bool {
		constexpr size_t TOTAL_COMPONENT_TYPE_COUNT =                    //
			tuple_size_v<decltype(VALID_STATE_COMPONENT_TYPES)> +        //
			tuple_size_v<decltype(VALID_INTERMEDIATE_COMPONENT_TYPES)> + //
			tuple_size_v<decltype(VALID_CLIENTSIDE_COMPONENT_TYPES)>;
		Array<CRC32, TOTAL_COMPONENT_TYPE_COUNT> allComponentNameCRC32s{};
		size_t index = 0;
		const auto addNameCRC32 = [&](const auto& validComponent) -> void {
			allComponentNameCRC32s[index++] = CRC32{validComponent.name};
		};
		meta::forEach(VALID_STATE_COMPONENT_TYPES, addNameCRC32);
		meta::forEach(VALID_INTERMEDIATE_COMPONENT_TYPES, addNameCRC32);
		meta::forEach(VALID_CLIENTSIDE_COMPONENT_TYPES, addNameCRC32);
		sort(allComponentNameCRC32s, [&](const CRC32& a, const CRC32& b) -> bool { return static_cast<uint32_t>(a) < static_cast<uint32_t>(b); });
		return adjacentFind(allComponentNameCRC32s) == allComponentNameCRC32s.end();
	}(),
	"All component name CRC32 hashes must be unique.");

template <exec::component T>
constexpr CStringView COMPONENT_NAME = []() -> CStringView {
	CStringView result{};
	meta::forEach(VALID_STATE_COMPONENT_TYPES, [&]<typename Component>(const ComponentTypeDeclaration<Component>& validComponent) -> void {
		if constexpr (same_as<Component, T>) {
			result = validComponent.name;
		}
	});
	meta::forEach(VALID_INTERMEDIATE_COMPONENT_TYPES, [&]<typename Component>(const ComponentTypeDeclaration<Component>& validComponent) -> void {
		if constexpr (same_as<Component, T>) {
			result = validComponent.name;
		}
	});
	meta::forEach(VALID_CLIENTSIDE_COMPONENT_TYPES, [&]<typename Component>(const ComponentTypeDeclaration<Component>& validComponent) -> void {
		if constexpr (same_as<Component, T>) {
			result = validComponent.name;
		}
	});
	return result;
}();

template <exec::component T>
constexpr CRC32 COMPONENT_NAME_CRC32{COMPONENT_NAME<T>};

template <typename... Components>
constexpr ComponentInitializers<Components...>::ComponentInitializers(const Components&... components)
	: sortedInitializers{ComponentInitializer{.componentNameCRC32 = COMPONENT_NAME_CRC32<Components>, .value = &components}...} {
	static_assert(noneOf(Array{COMPONENT_NAME_CRC32<Components>...}, [&](CRC32 componentNameCRC32) -> bool { return componentNameCRC32 == CRC32{}; }),
		"Invalid component type in initializer.");
	sort(sortedInitializers, ComponentInitializer::Compare{});
}

template <typename Component>
bool EntityInitializer::hasComponentInitializer() const {
	GREM_MATCH(static_cast<const Variant&>(*this)) {
		GREM_CASE(Monostate none) break;
		GREM_CASE(const json::Object* object) {
			return object->contains(COMPONENT_NAME<Component>);
		}
		GREM_CASE(Span<const byte> * input) {
			return true;
		}
		GREM_CASE(ComponentInitializersView componentInitializersView) {
			const auto it = lowerBound(componentInitializersView.sortedInitializers, COMPONENT_NAME_CRC32<Component>, ComponentInitializer::Compare{});
			return it != componentInitializersView.sortedInitializers.end() && it->componentNameCRC32 == COMPONENT_NAME_CRC32<Component>;
		}
		GREM_CASE(Span<const EntityInitializer> initializers) {
			for (const EntityInitializer& initializer : initializers) {
				if (initializer.hasComponentInitializer<Component>()) {
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

template <typename Component>
bool EntityInitializer::initializeComponent(Component& component, ParseEntityComponentContext& context, const Schema& schema) const {
	if constexpr (requires { component.initializeComponentDefault(context, schema, *this); }) {
		if (!is<Span<const byte>*>()) {
			component.initializeComponentDefault(context, schema, *this);
		}
	} else if constexpr (requires { initializeComponentDefault(component, context, schema, *this); }) {
		if (!is<Span<const byte>*>()) {
			initializeComponentDefault(component, context, schema, *this);
		}
	}
	GREM_MATCH(static_cast<const Variant&>(*this)) {
		GREM_CASE(Monostate none) break;
		GREM_CASE(const json::Object* object) {
			if (const auto it = object->find(COMPONENT_NAME<Component>); it != object->end()) {
				parseEntityComponent(context, it->second, component);
				return true;
			}
			return false;
		}
		GREM_CASE(Span<const byte> * input) {
			if (!deserialize(component, *input)) {
				throw std::invalid_argument{"Invalid component layout."};
			}
			return true;
		}
		GREM_CASE(ComponentInitializersView componentInitializersView) {
			if (const auto it = lowerBound(componentInitializersView.sortedInitializers, COMPONENT_NAME_CRC32<Component>, ComponentInitializer::Compare{});
				it != componentInitializersView.sortedInitializers.end() && it->componentNameCRC32 == COMPONENT_NAME_CRC32<Component>) {
				component = *static_cast<const Component*>(it->value);
				return true;
			}
			return false;
		}
		GREM_CASE(Span<const EntityInitializer> initializers) {
			for (const EntityInitializer& initializer : initializers) {
				if (initializer.initializeComponent<Component>(component, context, schema)) {
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

template <typename Component>
bool EntityInitializer::addOrAssignComponent(Component& component, ParseEntityComponentContext& context) const {
	GREM_MATCH(static_cast<const Variant&>(*this)) {
		GREM_CASE(Monostate none) break;
		GREM_CASE(const json::Object* object) {
			if (const auto it = object->find(COMPONENT_NAME<Component>); it != object->end()) {
				parseEntityComponent(context, it->second, component);
				return true;
			}
			return false;
		}
		GREM_CASE(Span<const byte> * input) {
			if (!deserialize(component, *input)) {
				throw std::invalid_argument{"Invalid component layout."};
			}
			return true;
		}
		GREM_CASE(ComponentInitializersView componentInitializersView) {
			if (const auto it = lowerBound(componentInitializersView.sortedInitializers, COMPONENT_NAME_CRC32<Component>, ComponentInitializer::Compare{});
				it != componentInitializersView.sortedInitializers.end() && it->componentNameCRC32 == COMPONENT_NAME_CRC32<Component>) {
				component = *static_cast<const Component*>(it->value);
				return true;
			}
			return false;
		}
		GREM_CASE(Span<const EntityInitializer> initializers) {
			for (const EntityInitializer& initializer : initializers) {
				if (initializer.addOrAssignComponent<Component>(component, context)) {
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

#endif
