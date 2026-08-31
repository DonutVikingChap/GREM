// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_SCHEMA_HPP
#define GREM_EXAMPLES_FPS_SCHEMA_HPP

#include <GREM/aliases.hpp>
#include <GREM/audio/Sound.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/HashSet.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/execution/component.hpp>
#include <GREM/graphics_3d/Lights3D.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>
#include <GREM/resource/Model.hpp>

#include "EntityType.hpp"
#include "NamedType.hpp"
#include "SynchronizedEntityMap.hpp"
#include "System.hpp"
#include "build_config.hpp"
#include "game_data.hpp"

#include <stdexcept> // std::invalid_argument
#include <utility>   // std::move, std::in_place_type

class AssetCache;
class Schema;

struct SoundType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const SoundType&) const = default;
	[[nodiscard]] auto operator<=>(const SoundType&) const = default;
};

struct DecalMaterialType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const DecalMaterialType&) const = default;
	[[nodiscard]] auto operator<=>(const DecalMaterialType&) const = default;
};

struct SpriteType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const SpriteType&) const = default;
	[[nodiscard]] auto operator<=>(const SpriteType&) const = default;
};

struct ModelType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const ModelType&) const = default;
	[[nodiscard]] auto operator<=>(const ModelType&) const = default;

	FPS_SHARED_API void setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources);
};

struct ProjectileType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const ProjectileType&) const = default;
	[[nodiscard]] auto operator<=>(const ProjectileType&) const = default;

	FPS_SHARED_API void setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources);
};

struct WeaponType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const WeaponType&) const = default;
	[[nodiscard]] auto operator<=>(const WeaponType&) const = default;

	FPS_SHARED_API void setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources);
};

struct MovementType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const MovementType&) const = default;
	[[nodiscard]] auto operator<=>(const MovementType&) const = default;
};

struct ParticleType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const ParticleType&) const = default;
	[[nodiscard]] auto operator<=>(const ParticleType&) const = default;

	FPS_SHARED_API void setImpliedComponents(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources);
};

struct DamageableType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const DamageableType&) const = default;
	[[nodiscard]] auto operator<=>(const DamageableType&) const = default;
};

struct SoundDescription {
	String filepath{};
	aud::SoundOptions options{};
	phys::Time startTimeOffset{};
};

struct DecalMaterialDescription {
	Optional<String> baseColorMapImageFilepath{};
	Optional<String> normalMapImageFilepath{};
	Optional<String> occlusionRoughnessMetallicMapImageFilepath{};
	Optional<String> emissiveMapImageFilepath{};
	vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
	float occlusionStrength = 1.0f;
	float roughnessFactor = 1.0f;
	float metallicFactor = 1.0f;
	float normalScale = 1.0f;
	vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
	phys::Time maxLifetime = phys::Time::INF;
};

struct SpriteDescription {
	String imageFilepath{};
	phys::Length3D localOffset{};
	phys::Orientation3D localOrientation{};
	phys::Length2D size{1_meter};
	Color tintColor = Color::WHITE;
	vec2 frameSize{1.0f};
	uint32_t frameCount = 1;
	uint32_t framePadding = 0;
	phys::Frequency frameRate = 60_Hertz;
	phys::Coefficient unalignedAngularFadeFactor{};
	phys::Coefficient alignedAngularFadeFactor{};
	phys::Distance distanceOrderingBias{};
	OrientationAlignment orientationAlignment = OrientationAlignment::NONE;
	bool flipHorizontally = false;
	bool flipVertically = false;
	bool framesRightToLeft = false;
	bool framesTopToBottom = false;
	bool looping = true;
};

struct ModelDescription {
	enum class ShadowCasterType : uint8_t {
		NONE,
		FULL_QUALITY,
		INSET_CONVEX_HULL,
	};

	String filepath{};
	res::ModelOptions options{};
	phys::LocalTransformation3D rootTransformation{};
	Color tintColor = Color::WHITE;
	Optional<phys::Shape3D> shapeOverride{};
	Optional<phys::Mass> massOverride{};
	Optional<phys::Length3D> centerOfMassOverride{};
	Optional<phys::PrincipalMomentsOfInertia3D> principalMomentsOfInertiaOverride{};
	Optional<phys::LocalInertiaOrientation3D> localInertiaOrientationOverride{};
	Optional<phys::Material> materialOverride{};
	Optional<phys::LinearAcceleration3D> gravityAccelerationOverride{};
	ShadowCasterType shadowCasterType = ShadowCasterType::FULL_QUALITY;
	OrientationAlignment orientationAlignment = OrientationAlignment::NONE;
	bool excludeFromDepthPrepass = false;
	bool preloadObjectDescription = false;
	bool preloadConvexHullShape = false;
	bool preloadTriangleMeshShape = false;
};

struct ProjectileDescription {
	ModelType modelType{};
	DecalMaterialType impactDecalMaterialType{};
	phys::Length2D impactDecalSize{1_meter};
	phys::Distance impactDecalRange = 0.2_meters;
	ArrayList<ParticleType> impactParticleTypes{};
	phys::Mass mass = 10_grams;
	float damage = 40.0f;
};

struct WeaponDescription {
	enum class FireMode : uint8_t {
		SAFE,
		SEMI_AUTOMATIC,
		FULLY_AUTOMATIC,
	};

	struct Recoil {
		phys::Coefficient cosineCutoff = 1.4_x;
		phys::Coefficient linearInitial = 1_x;
		phys::Coefficient linearSlope = -0.8_x;
		phys::AngularVelocity2D strengthVerticalMin = 0.6_radians_per_second;
		phys::AngularVelocity2D strengthVerticalMax = 0.7_radians_per_second;
		phys::AngularVelocity2D strengthHorizontalMin = -0.17_radians_per_second;
		phys::AngularVelocity2D strengthHorizontalMax = 0.2_radians_per_second;
		phys::Coefficient aimingDownSightsStrengthCoefficient = 0.8_x;
		phys::Coefficient hipFireAimDeviationCoefficientVertical = 0.5_x;
		phys::Coefficient hipFireAimDeviationCoefficientHorizontal = 4_x;
		phys::Frequency aimDeviationExponentialDecayRate = 4_per_second;
	};

	struct ProceduralAnimation {
		phys::Wavenumber bobbingRate = 0.5_per_meter;
		phys::Frequency bobbingDecayRate = 4_radians_per_second;
		phys::Length3D caseEjectionLocalOffset{0.01_meters, 0.03_meters, -0.1_meters};
		phys::Orientation3D caseEjectionLocalOrientation = phys::Orientation3D::fromAngles(30_degrees, -90_degrees, 0);
		phys::Length3D muzzleFlashLocalOffset{0, 0.03_meters, -0.85_meters};
		phys::Orientation3D muzzleFlashLocalOrientation{};
		phys::Time muzzleFlashLightDuration = 50_milliseconds;
		phys::Distance muzzleFlashLightRange = 20_meters;
		Color muzzleFlashLightColor = Color::GOLDEN_ROD * Color::fromAlpha(4.0f);
		phys::Length3D hipFireBaseOffset{0.14_meters, -0.16_meters, -0.5_meters};
		phys::Length3D hipFireCrouchOffset{-0.02_meters, 0.07_meters, 0.0_meters};
		phys::Length3D aimingDownSightsBaseOffset{0, -0.086_meters, -0.412_meters};
		phys::Quantity<3, phys::Time::Unit> hipFireOffsetVelocityContribution{-0.004_seconds, -0.006_seconds, -0.004_seconds};
		phys::Quantity<3, phys::Time::Unit> aimingDownSightsOffsetVelocityContribution{0, -0.0017_seconds, -0.001_seconds};
		phys::LinearAbsement3D hipFireOffsetPitchRateContribution{0, -0.018_meter_seconds, 0};
		phys::LinearAbsement3D aimingDownSightsOffsetPitchRateContribution{0, -0.0018_meter_seconds, 0};
		phys::LinearAbsement3D hipFireOffsetYawRateContribution{0.005_meter_seconds, 0, 0};
		phys::LinearAbsement3D aimingDownSightsOffsetYawRateContribution{0.0008_meter_seconds, 0, 0};
		phys::LinearAbsement3D hipFireOffsetRecoilPitchRateContribution{0, 0, 0.17_meter_seconds};
		phys::LinearAbsement3D aimingDownSightsOffsetRecoilPitchRateContribution{0, 0, 0.06_meter_seconds};
		phys::LinearAbsement3D hipFireOffsetRecoilYawRateContribution{0, 0, 0};
		phys::LinearAbsement3D aimingDownSightsOffsetRecoilYawRateContribution{0, 0, 0};
		phys::Quantity<2, phys::Time::Unit> hipFireOffsetBobbingContribution{0.002_seconds, 0.001_seconds};
		phys::Quantity<2, phys::Time::Unit> aimingDownSightsOffsetBobbingContribution{0.001_seconds, 0.0005_seconds};
		phys::PitchYawRollRotations hipFireBaseRotations{0, 0, 0};
		phys::PitchYawRollRotations hipFireCrouchRotations{0, 0, 15_degrees};
		phys::PitchYawRollRotations aimingDownSightsBaseRotations{0, 0, 0};
		phys::Quantity<3, phys::Time::Unit> hipFireRotationsPitchRateContribution{0, 0, 0};
		phys::Quantity<3, phys::Time::Unit> aimingDownSightsRotationsPitchRateContribution{0, 0, 0};
		phys::Quantity<3, phys::Time::Unit> hipFireRotationsYawRateContribution{0, 0, 0.012_seconds};
		phys::Quantity<3, phys::Time::Unit> aimingDownSightsRotationsYawRateContribution{0, 0, 0};
	};

	ModelType modelType{};
	SoundType fireSoundType{};
	SoundType reloadSoundType{};
	SoundType dryFireSoundType{};
	SoundType changeFireModeSoundType{};
	ArrayList<FireMode> capableFireModes{FireMode::SAFE, FireMode::SEMI_AUTOMATIC, FireMode::FULLY_AUTOMATIC};
	FireMode mainFireMode = FireMode::SEMI_AUTOMATIC;
	ProjectileType projectileType{};
	ParticleType caseEjectionParticleType{};
	ArrayList<SpriteType> muzzleFlashSpriteTypes{};
	phys::Speed muzzleVelocity = 500_meters_per_second;
	phys::Time cycleDuration = 1_x / 550_per_minute;
	phys::Time reloadDuration = 1.2_seconds;
	phys::Time recoilDuration = 400_milliseconds;
	phys::Time drawDuration = 1.5_seconds;
	phys::Time projectileLifeTime = 3_seconds;
	phys::Time droppedDespawnTime = 10_seconds;
	uint16_t maxMagazineCapacity = 20;
	phys::Frequency crouchExponentialDecayRate = 8_per_second;
	phys::Frequency aimDownSightsExponentialDecayRate = 12_per_second;
	phys::Frequency unaimDownSightsExponentialDecayRate = 4_per_second;
	phys::Frequency aimAngleSmoothingExponentialDecayRate = 8_per_second;
	phys::Frequency velocitySmoothingExponentialDecayRate = 20_per_second;
	phys::Coefficient aimingDownSightsMovementSpeedCoefficient = 0.55_x;
	phys::Angle aimDeviationMax = 10_degrees;
	Recoil recoil{};
	ProceduralAnimation proceduralAnimation{};
};

struct MovementDescription {
	SoundType jumpSoundType{};
	SoundType landingSoundType{};
	ArrayList<SoundType> footstepSoundTypes{};
	phys::Acceleration gravityAcceleration = 20_meters_per_second_squared;
	phys::Distance jumpHeight = 1.2_meters;
	phys::Time jumpRegroundDelay = 200_milliseconds;
	phys::Speed minGroundSpeed = 1.8_kilometers_per_hour;
	phys::Speed targetAirSpeed = 1.8_kilometers_per_hour;
	phys::Time accelerationDuration = 0.15_seconds;
	phys::Acceleration airAcceleration = 65_meters_per_second_squared;
	phys::Speed baseSpeed = 18_kilometers_per_hour;
	phys::Coefficient crouchSpeedCoefficient = 0.5_x;
	phys::Coefficient sprintSpeedCoefficient = 1.5_x;
	phys::Coefficient flySpeedCoefficient = 5_x;
	phys::Coefficient crouchHeightScale = 0.6_x;
	phys::Time crouchDurationMin = 200_milliseconds;
	phys::Time crouchDurationMax = 500_milliseconds;
	phys::Coefficient aimOffsetCoefficient = 0.375_x;
	phys::Coefficient stepHeightCoefficient = 0.15_x;
};

struct ParticleDescription {
	ModelType modelType{};
	SpriteType spriteType{};
	Optional<phys::CollisionLayers> layersOverride{};
	Optional<phys::CollisionLayers> detectionLayersOverride{};
	Optional<phys::CollisionLayers> noDetectionLayersOverride{};
	Optional<phys::CollisionLayers> responseLayersOverride{};
	Optional<phys::CollisionLayers> noResponseLayersOverride{};
	phys::Length3D localOffset{};
	phys::Orientation3D localOrientation{};
	uint16_t launchCountMin = 2;
	uint16_t launchCountMax = 4;
	phys::Speed launchSpeedMin = 2_meters_per_second;
	phys::Speed launchSpeedMax = 5_meters_per_second;
	phys::Angle launchSpreadAngleMin = 0_degrees;
	phys::Angle launchSpreadAngleMax = 45_degrees;
	phys::Angle launchRollAngleMin = 0_degrees;
	phys::Angle launchRollAngleMax = 360_degrees;
	phys::AngularVelocity3D launchTangentSpaceAngularVelocityMin{};
	phys::AngularVelocity3D launchTangentSpaceAngularVelocityMax{};
	phys::LinearAcceleration3D launchTangentSpaceAccelerationMin{};
	phys::LinearAcceleration3D launchTangentSpaceAccelerationMax{};
	phys::LinearAcceleration3D gravityAcceleration{0, -9.82_meters_per_second_squared, 0};
	phys::Time maxLifetime = 3_seconds;
	DecalMaterialType landingDecalMaterialType{};
	phys::Length2D landingDecalSize{1_meter};
	phys::Distance landingDecalRange = 0.2_meters;
	bool orientationFollowsVelocity = false;
};

struct DamageableDescription {
	Color flashTintColor = Color::fromLinear(6.0f, 0.8f, 0.8f);
	phys::Time flashDuration = 0.2_seconds;
	phys::Speed speedLossPerUnitDamage{};
	DecalMaterialType impactDecalMaterialType{};
	phys::Length2D impactDecalSize{1_meter};
	phys::Distance impactDecalRange = 0.2_meters;
	ArrayList<ParticleType> impactParticleTypes{};
	ArrayList<ParticleType> deathParticleTypes{};
	SoundType deathSound{};
	bool unkillable = false;
};

struct ModelObjectDescription {
	struct JointDescription {
		Optional<res::Model::PhysicsObjectIndex> physicsObjectIndex{};
	};

	struct PhysicsObjectDescription {
		res::Model::JointIndex jointIndex = 0;
		phys::Length3D initialLocalOffset{};
		phys::Orientation3D initialLocalOrientation{};
		phys::Scale3D initialLocalScale{};
		phys::Mass mass{};
		phys::Length3D centerOfMass{};
		phys::PrincipalMomentsOfInertia3D principalMomentsOfInertia{};
		phys::LocalInertiaOrientation3D localInertiaOrientation{};
		phys::Collider3D collider{};
		phys::Material material{};
		phys::LinearVelocity3D initialLinearVelocity{};
		phys::AngularVelocity3D initialAngularVelocity{};
		phys::Coefficient gravityFactor = 1_x;
	};

	struct PhysicsJointDescription {
		Pair<res::Model::PhysicsObjectIndex> objectIndices{};
		phys::GenericJointOptions3D genericJointOptions{};
	};

	struct LightDescription {
		res::Model::JointIndex jointIndex = 0;
		Variant<gfx::DirectionalLightOptions3D, gfx::PointLightOptions3D, gfx::SpotLightOptions3D> lightOptions{};
	};

	ArrayList<JointDescription> jointDescriptions{};
	ArrayList<PhysicsObjectDescription> physicsObjectDescriptions{};
	ArrayList<PhysicsJointDescription> physicsJointDescriptions{};
	ArrayList<LightDescription> lightDescriptions{};
};

enum EntityFlag : EntityID::Flags { // NOLINT(performance-enum-size)
	ENTITY_CLIENTSIDE = 1 << 0,
	ENTITY_PART_OF_MAP = 1 << 1,
	ENTITY_PHYSICS_PREDICTED = 1 << 2,
	ENTITY_DISPLAY_PREDICTED = 1 << 3,
	ENTITY_DISPLAY_SUBTICK_PREDICTED = 1 << 4,
};

[[nodiscard]] inline const json::String& parseString(const json::Value& jsonValue) {
	return jsonValue.getString();
}

template <enumeration Enum>
[[nodiscard]] inline Enum parseEnumerand(const json::Value& jsonValue) {
	const json::String& string = parseString(jsonValue);
	Optional<Enum> result{};
	meta::forEachNamedEnumerand<Enum>([&](StringView name, auto type) -> void {
		if (!result && string == name) {
			result = type();
		}
	});
	if (!result) {
		throw json::Error{formatString("Invalid type \"{}\".", string), jsonValue.getSource()};
	}
	return *result;
}

[[nodiscard]] inline bool parseBoolean(const json::Value& jsonValue) {
	return jsonValue.getBoolean();
}

template <typename T>
[[nodiscard]] inline T parseNumber(const json::Value& jsonValue) {
	const json::Number number = jsonValue.getNumber();
	if constexpr (integral<T>) {
		if (isinf(number)) {
			return (signbit(number)) ? Limits<T>::MIN : Limits<T>::MAX;
		}
		if (trunc(number) != number) {
			throw json::Error{"Expected an integer.", jsonValue.getSource()};
		}
		if (number < static_cast<json::Number>(Limits<T>::MIN) || number > static_cast<json::Number>(Limits<T>::MAX)) {
			throw json::Error{"Value out of range.", jsonValue.getSource()};
		}
	}
	return static_cast<T>(number);
}

template <size_t N, typename T>
[[nodiscard]] inline vec<N, T> parseVector(const json::Value& jsonValue) {
	if (jsonValue.isArray()) {
		if (jsonValue.getArraySize() != N) {
			throw json::Error{"Invalid number of array items.", jsonValue.getSource()};
		}
		vec<N, T> result;
		for (size_t i = 0; i < N; ++i) {
			result[i] = parseNumber<T>(jsonValue.getItem(i));
		}
		return result;
	}
	return vec<N, T>{parseNumber<T>(jsonValue)};
}

[[nodiscard]] inline quat parseQuaternion(const json::Value& jsonValue) {
	if (jsonValue.isArray()) {
		if (jsonValue.getArraySize() == 3) {
			return convertAnglesToQuaternion(convertDegreesToRadians(parseVector<3, float>(jsonValue)));
		}
		const vec4 components = parseVector<4, float>(jsonValue);
		return quat{components.x, components.y, components.z, components.w};
	}

	if (jsonValue.isObject()) {
		const json::Value* const directionValue = jsonValue.findProperty("direction");
		const json::Value* const upValue = jsonValue.findProperty("up");
		if (directionValue && upValue) {
			const vec3 direction = parseVector<3, float>(*directionValue);
			const vec3 up = parseVector<3, float>(*upValue);
			return quatLookAt(direction, up);
		}

		const json::Value* const angleValue = jsonValue.findProperty("angle");
		const json::Value* const axisValue = jsonValue.findProperty("axis");
		if (angleValue && axisValue) {
			const float angle = parseNumber<float>(*angleValue);
			const vec3 axis = parseVector<3, float>(*axisValue);
			return angleAxis(convertDegreesToRadians(angle), axis);
		}

		return quat{0.0f, 0.0f, 0.0f, 1.0f};
	}

	throw json::Error{"Expected an array or an object.", jsonValue.getSource()};
}

template <size_t C, size_t R, typename T>
[[nodiscard]] inline mat<C, R, T> parseMatrix(const json::Value& jsonValue) {
	if constexpr (C == 4 && R == 4 && same_as<T, float>) {
		if (jsonValue.isObject()) {
			mat4 result{1.0f};
			if (const json::Value* const scaleValue = jsonValue.findProperty("scale")) {
				result = scale(parseVector<3, float>(*scaleValue)) * result;
			}
			if (const json::Value* const orientationValue = jsonValue.findProperty("orientation")) {
				result = rotate(parseQuaternion(*orientationValue)) * result;
			}
			if (const json::Value* const translationValue = jsonValue.findProperty("translation")) {
				result = translate(parseVector<3, float>(*translationValue)) * result;
			}
			return result;
		}
	}
	mat<C, R, T> result;
	if (jsonValue.getArraySize() == size_t{C}) {
		for (size_t i = 0; i < C; ++i) {
			result[i] = parseVector<R, T>(jsonValue.getItem(i));
		}
	} else if (jsonValue.getArraySize() == size_t{C * R}) {
		for (size_t i = 0; i < C; ++i) {
			for (size_t j = 0; j < R; ++j) {
				result[i][j] = parseNumber<T>(jsonValue.getItem(i * R + j));
			}
		}
	} else {
		throw json::Error{"Invalid number of array items.", jsonValue.getSource()};
	}
	return result;
}

[[nodiscard]] inline Color parseColor(const json::Value& jsonValue) {
	if (jsonValue.getArraySize() == 3) {
		return Color::fromLinear(parseVector<3, float>(jsonValue));
	}
	if (jsonValue.getArraySize() == 4) {
		return Color::fromLinear(parseVector<4, float>(jsonValue));
	}
	throw json::Error{"Invalid number of array items.", jsonValue.getSource()};
}

template <size_t N, typename UnitT>
[[nodiscard]] inline phys::Quantity<N, UnitT> parseQuantity(const json::Value& jsonValue) {
	if constexpr (N == 1) {
		if constexpr (same_as<UnitT, phys::Radians> || same_as<UnitT, phys::Absolute<phys::Radians>>) {
			return parseNumber<float>(jsonValue) * phys::DEGREES;
		} else {
			return parseNumber<float>(jsonValue) * UnitT{};
		}
	} else {
		if constexpr (same_as<UnitT, phys::Radians> || same_as<UnitT, phys::Absolute<phys::Radians>>) {
			return parseVector<N, float>(jsonValue) * phys::DEGREES;
		} else {
			return parseVector<N, float>(jsonValue) * UnitT{};
		}
	}
}

template <size_t N>
[[nodiscard]] inline phys::Orientation<N> parseOrientation(const json::Value& jsonValue) {
	if constexpr (N == 2) {
		return phys::Orientation2D{parseNumber<float>(jsonValue) * phys::DEGREES};
	} else {
		return phys::Orientation3D{parseQuaternion(jsonValue)};
	}
}

template <size_t N>
[[nodiscard]] inline phys::LocalTransformation<N> parseLocalTransformation(const json::Value& jsonValue) {
	if (jsonValue.isObject()) {
		phys::LocalTransformation<N> result{};
		if (const json::Value* const scaleValue = jsonValue.findProperty("scale")) {
			result = scale(parseQuantity<N, typename phys::Scale<N>::Unit>(*scaleValue)) * result;
		}
		if (const json::Value* const orientationValue = jsonValue.findProperty("orientation")) {
			result = rotate(parseOrientation<N>(*orientationValue)) * result;
		}
		if (const json::Value* const translationValue = jsonValue.findProperty("translation")) {
			result = translate(parseQuantity<N, typename phys::Length<N>::Unit>(*translationValue)) * result;
		}
		return result;
	}
	throw json::Error{"Expected an object.", jsonValue.getSource()};
}

[[nodiscard]] inline phys::ObjectActivity parseObjectActivity(const json::Value& jsonValue) {
	if (jsonValue.isNumber()) {
		const phys::ObjectActivity::EnergyLevel energyLevel = parseNumber<phys::ObjectActivity::EnergyLevel>(jsonValue);
		if (energyLevel > phys::ObjectActivity::MAX_ENERGY_LEVEL) {
			throw json::Error{"Value out of range.", jsonValue.getSource()};
		}
		return phys::ObjectActivity{energyLevel};
	}
	if (jsonValue.isObject()) {
		phys::ObjectActivity result{};
		if (const json::Value* const isCorrectableValue = jsonValue.findProperty("isCorrectable")) {
			result.isCorrectable = (parseBoolean(*isCorrectableValue)) ? 1 : 0;
		}
		if (const json::Value* const wasCorrectedValue = jsonValue.findProperty("wasCorrected")) {
			result.wasCorrected = (parseBoolean(*wasCorrectedValue)) ? 1 : 0;
		}
		if (const json::Value* const energyLevelValue = jsonValue.findProperty("energyLevel")) {
			const phys::ObjectActivity::EnergyLevel energyLevel = parseNumber<phys::ObjectActivity::EnergyLevel>(*energyLevelValue);
			if (energyLevel > phys::ObjectActivity::MAX_ENERGY_LEVEL) {
				throw json::Error{"Value out of range.", energyLevelValue->getSource()};
			}
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
			result.energyLevel = energyLevel;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
		}
		return result;
	}
	throw json::Error{"Expected a number or an object.", jsonValue.getSource()};
}

[[nodiscard]] inline phys::CollisionLayer parseCollisionLayer(const json::Value& jsonValue) {
	return phys::CollisionLayer{parseNumber<uint32_t>(jsonValue)};
}

[[nodiscard]] inline phys::CollisionLayers parseCollisionLayers(const json::Value& jsonValue) {
	phys::CollisionLayers result{};
	for (const json::Value& item : jsonValue.getArray()) {
		result |= parseCollisionLayer(item);
	}
	return result;
}

[[nodiscard]] phys::Shape3D parseShape3D(const json::Value& jsonValue);

template <typename T>
inline void parseValue(const json::Value& jsonValue, T& value) requires(requires { value.parseValueFrom(jsonValue); }) {
	value.parseValueFrom(jsonValue);
}

inline void parseValue(const json::Value& jsonValue, String& value) {
	value = parseString(jsonValue);
}

template <enumeration Enum>
inline void parseValue(const json::Value& jsonValue, Enum& value) {
	value = parseEnumerand<Enum>(jsonValue);
}

inline void parseValue(const json::Value& jsonValue, bool& value) {
	value = parseBoolean(jsonValue);
}

template <arithmetic T>
inline void parseValue(const json::Value& jsonValue, T& value) {
	value = parseNumber<T>(jsonValue);
}

template <size_t N, typename T>
inline void parseValue(const json::Value& jsonValue, vec<N, T>& value) {
	value = parseVector<N, T>(jsonValue);
}

inline void parseValue(const json::Value& jsonValue, quat& value) {
	value = parseQuaternion(jsonValue);
}

template <size_t C, size_t R, typename T>
inline void parseValue(const json::Value& jsonValue, mat<C, R, T>& value) {
	value = parseMatrix<C, R, T>(jsonValue);
}

inline void parseValue(const json::Value& jsonValue, Color& value) {
	value = parseColor(jsonValue);
}

template <size_t N, typename UnitT>
inline void parseValue(const json::Value& jsonValue, phys::Quantity<N, UnitT>& value) {
	value = parseQuantity<N, UnitT>(jsonValue);
}

template <size_t N>
inline void parseValue(const json::Value& jsonValue, phys::Orientation<N>& value) {
	value = parseOrientation<N>(jsonValue);
}

template <size_t N>
inline void parseValue(const json::Value& jsonValue, phys::LocalTransformation<N>& value) {
	value = parseLocalTransformation<N>(jsonValue);
}

inline void parseValue(const json::Value& jsonValue, phys::ObjectActivity& value) {
	value = parseObjectActivity(jsonValue);
}

inline void parseValue(const json::Value& jsonValue, phys::CollisionLayers& value) {
	value = parseCollisionLayers(jsonValue);
}

inline void parseValue(const json::Value& jsonValue, phys::Shape3D& value) {
	value = parseShape3D(jsonValue);
}

inline void parseValue(const json::Value& jsonValue, EntityFlag& entityFlag) {
	const json::String& string = parseString(jsonValue);
	if (string == "ENTITY_CLIENTSIDE") {
		entityFlag = ENTITY_CLIENTSIDE;
	} else if (string == "ENTITY_PART_OF_MAP") {
		entityFlag = ENTITY_PART_OF_MAP;
	} else if (string == "ENTITY_PHYSICS_PREDICTED") {
		entityFlag = ENTITY_PHYSICS_PREDICTED;
	} else if (string == "ENTITY_DISPLAY_PREDICTED") {
		entityFlag = ENTITY_DISPLAY_PREDICTED;
	} else if (string == "ENTITY_DISPLAY_SUBTICK_PREDICTED") {
		entityFlag = ENTITY_DISPLAY_SUBTICK_PREDICTED;
	} else {
		throw std::invalid_argument{formatString("Invalid type \"{}\".", string)};
	}
}

inline void parseValue(const json::Value& jsonValue, Any& value) {
	match(jsonValue)([&]<typename T>(const T& element) -> void { value.emplace<T>(element); });
}

template <typename T>
void parseValue(const json::Value& jsonValue, Optional<T>& value);

template <typename T, size_t N>
void parseValue(const json::Value& jsonValue, Array<T, N>& value);

template <typename T>
void parseValue(const json::Value& jsonValue, ArrayList<T>& value);

template <typename T, size_t N>
void parseValue(const json::Value& jsonValue, SmallArrayList<T, N>& value);

template <typename T>
void parseValue(const json::Value& jsonValue, Buffer<T>& value);

template <typename T, size_t N>
void parseValue(const json::Value& jsonValue, SmallBuffer<T, N>& value);

template <typename T1, typename T2>
void parseValue(const json::Value& jsonValue, Pair<T1, T2>& value);

template <aggregate Aggregate>
inline void parseValue(const json::Value& jsonValue, Aggregate& value) requires(!requires { value.parseValueFrom(jsonValue); }) {
	if (!jsonValue.isObject()) {
		throw json::Error{"Expected an object.", jsonValue.getSource()};
	}
	meta::forEachNamedField(value, [&](StringView name, auto& field) -> void {
		if (const json::Value* const fieldValue = jsonValue.findProperty(name)) {
			try {
				parseValue(*fieldValue, field);
			} catch (...) {
				Error::throwWithNested(json::Error{formatString("Invalid property \"{}\"", name), fieldValue->getSource()});
			}
		}
	});
}

template <typename T>
inline void parseValue(const json::Value& jsonValue, Optional<T>& value) {
	if (jsonValue.isNull()) {
		value.reset();
	} else {
		parseValue(jsonValue, value.emplace());
	}
}

template <typename T, size_t N>
inline void parseValue(const json::Value& jsonValue, Array<T, N>& value) {
	const json::Array& array = jsonValue.getArray();
	if (array.size() != N) {
		throw json::Error{formatString("Expected {} array elements.", N), jsonValue.getSource()};
	}
	for (size_t i = 0; i < N; ++i) {
		parseValue(array[i], value[i]);
	}
}

template <typename T>
inline void parseValue(const json::Value& jsonValue, ArrayList<T>& value) {
	const json::Array& array = jsonValue.getArray();
	value.clear();
	value.reserve(array.size());
	for (const json::Value& item : array) {
		parseValue(item, value.emplace_back());
	}
}

template <typename T, size_t N>
inline void parseValue(const json::Value& jsonValue, SmallArrayList<T, N>& value) {
	const json::Array& array = jsonValue.getArray();
	value.clear();
	value.reserve(array.size());
	for (const json::Value& item : array) {
		parseValue(item, value.emplace_back());
	}
}

template <typename T>
inline void parseValue(const json::Value& jsonValue, Buffer<T>& value) {
	const json::Array& array = jsonValue.getArray();
	value.clear();
	for (const json::Value& item : array) {
		parseValue(item, value.push_back_unspecified_value());
	}
}

template <typename T, size_t N>
inline void parseValue(const json::Value& jsonValue, SmallBuffer<T, N>& value) {
	const json::Array& array = jsonValue.getArray();
	value.clear();
	for (const json::Value& item : array) {
		parseValue(item, value.push_back_unspecified_value());
	}
}

template <typename T1, typename T2>
void parseValue(const json::Value& jsonValue, Pair<T1, T2>& value) {
	const json::Array& array = jsonValue.getArray();
	if (array.size() != 2) {
		throw std::invalid_argument{"Expected 2 array elements."};
	}
	parseValue(array[0], value.first);
	parseValue(array[1], value.second);
}

inline phys::Shape3D parseShape3D(const json::Value& jsonValue) {
	const json::Object& outerObject = jsonValue.getObject();

	phys::Shape3D result{};
	bool found = false;
	[&]<typename... Shapes>(const Variant<Shapes...>&) -> void {
		(
			[&]<typename Shape>(meta::Type<Shape>) -> void {
				if (!found) {
					StringView shapeName = meta::unqualified_type_name_v<Shape>;
					if (shapeName.ends_with("3D")) {
						shapeName.remove_suffix(2);
					}
					if (const auto itShape = outerObject.find(shapeName); itShape != outerObject.end()) {
						if constexpr (same_as<Shape, phys::ConvexPolytopeShape3D>) {
							const json::Object& object = itShape->second.getObject();
							Allocation<phys::ConvexPolytopeShape3D::Vertex> vertices{};
							if (const auto it = object.find("vertices"); it != object.end()) {
								const json::Array& array = it->second.getArray();
								const size_t vertexCount = array.size();
								vertices.resize(vertexCount);
								for (size_t i = 0; i < vertexCount; ++i) {
									vertices[i] = parseVector<3, float>(array[i]);
								}
							}
							ConvexPolytopeVertexIndex maxVertexCount = static_cast<ConvexPolytopeVertexIndex>(min(vertices.size(), size_t{32}));
							if (const auto it = object.find("maxVertexCount"); it != object.end()) {
								maxVertexCount = parseNumber<ConvexPolytopeVertexIndex>(it->second);
							}
							result.emplace<phys::ConvexPolytopeShape3D>(vertices, maxVertexCount);
						} else if constexpr (same_as<Shape, phys::TriangleMeshShape3D>) {
							const json::Object& object = itShape->second.getObject();
							Allocation<phys::TriangleMeshShape3D::Vertex> vertices{};
							if (const auto it = object.find("vertices"); it != object.end()) {
								const json::Array& array = it->second.getArray();
								const size_t vertexCount = array.size();
								vertices.resize(vertexCount);
								for (size_t i = 0; i < vertexCount; ++i) {
									vertices[i] = parseVector<3, float>(array[i]);
								}
							}
							Allocation<phys::TriangleMeshShape3D::VertexIndex> indices{};
							if (const auto it = object.find("indices"); it != object.end()) {
								const json::Array& array = it->second.getArray();
								const size_t indexCount = array.size();
								if (indexCount % 3 != 0) {
									throw json::Error{"Invalid index count.", it->second.getSource()};
								}
								indices.resize(indexCount);
								for (size_t i = 0; i < indexCount; ++i) {
									const phys::TriangleMeshShape3D::VertexIndex index = parseNumber<phys::TriangleMeshShape3D::VertexIndex>(array[i]);
									if (index >= vertices.size()) {
										throw json::Error{"Invalid vertex index.", array[i].getSource()};
									}
									indices[i] = index;
								}
							}
							result.emplace<phys::TriangleMeshShape3D>(std::move(vertices), std::move(indices));
						} else if constexpr (same_as<Shape, phys::LocallyTransformedShape3D>) {
							phys::LocallyTransformedShape3D shape{.shape = SharedPointer<phys::Shape3D>::create(parseShape3D(itShape->second.getProperty("shape")))};
							if (const json::Value* const localOffsetValue = itShape->second.findProperty("localOffset")) {
								shape.localOffset = parseQuantity<3, phys::Meters>(*localOffsetValue);
							}
							if (const json::Value* const localOrientationValue = itShape->second.findProperty("localOrientation")) {
								shape.localOrientation = parseOrientation<3>(*localOrientationValue);
							}
							if (const json::Value* const localScaleValue = itShape->second.findProperty("localScale")) {
								shape.localScale = parseQuantity<3, phys::Unitless>(*localScaleValue);
							}
							result.emplace<phys::LocallyTransformedShape3D>(std::move(shape));
						} else if constexpr (same_as<Shape, phys::CompoundColliderShape3D>) {
							const json::Array& array = itShape->second.getArray();
							const size_t colliderCount = array.size();
							SharedPointer<phys::SubCollider3D[]> subColliders = SharedPointer<phys::SubCollider3D[]>::create(colliderCount);
							for (size_t i = 0; i < colliderCount; ++i) {
								parseValue(array[i], subColliders[static_cast<std::ptrdiff_t>(i)]);
							}
							result.emplace<phys::CompoundColliderShape3D>(std::move(subColliders));
						} else {
							parseValue(itShape->second, result.emplace<Shape>());
						}
						found = true;
					}
				}
			}(meta::TYPE<Shapes>),
			...);
	}(result);
	if (!found) {
		throw std::invalid_argument{"Invalid shape."};
	}
	return result;
}

template <aggregate Aggregate>
inline bool parseProperty(const json::String& key, const json::Value& jsonValue, Aggregate& value) {
	bool found = false;
	meta::forEachNamedField(value, [&](StringView name, auto& field) -> void {
		if (!found && key == name) {
			try {
				parseValue(jsonValue, field);
				found = true;
			} catch (...) {
				Error::throwWithNested(json::Error{formatString("Invalid property \"{}\"", name), jsonValue.getSource()});
			}
		}
	});
	if (!found) {
		throw json::Error{formatString("Unknown property \"{}\".", key), jsonValue.getSource()};
	}
	return found;
}

struct ParseEntityComponentContext {
	rng::Xoroshiro128PlusPlusEngine numberGenerator{};
	uzvec3 stepIndex{};
	HashMap<String, SynchronizedEntityID> namedEntities{};
};

inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, auto& value) {
	(void)context;

	parseValue(jsonValue, value);
}

template <strict_arithmetic T>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, T& value) {
	if (jsonValue.isObject()) {
		const json::Value* const minValue = jsonValue.findProperty("min");
		const json::Value* const maxValue = jsonValue.findProperty("max");
		if (minValue && maxValue) {
			const T min = parseNumber<T>(*minValue);
			const T max = parseNumber<T>(*maxValue);
			if (min > max) {
				throw json::Error{"Invalid range.", jsonValue.getSource()};
			}
			if (const json::Value* const exclude = jsonValue.findProperty("exclude")) {
				const T rangeSize = max - min;
				const T minMargin = rangeSize / T{10};
				const T excludeMin = parseNumber<T>(exclude->getProperty("min"));
				const T excludeMax = parseNumber<T>(exclude->getProperty("max"));
				if (excludeMin > excludeMax) {
					throw json::Error{"Invalid exclude range.", exclude->getSource()};
				}
				if (excludeMin <= min + minMargin && excludeMax >= max - minMargin) {
					throw json::Error{"Exclude range is too wide.", exclude->getSource()};
				}
				while (true) {
					if constexpr (integral<T>) {
						value = rng::UniformIntegerDistribution<T>{min, max}(context.numberGenerator);
					} else {
						value = rng::UniformRealDistribution<T>{min, max}(context.numberGenerator);
					}
					if (excludeMin >= excludeMax || value < excludeMin || value > excludeMax) {
						break;
					}
				}
			} else {
				if constexpr (integral<T>) {
					value = rng::UniformIntegerDistribution<T>{min, max}(context.numberGenerator);
				} else {
					value = rng::UniformRealDistribution<T>{min, max}(context.numberGenerator);
				}
			}
			return;
		}

		const json::Value* const baseValue = jsonValue.findProperty("base");
		const json::Value* const stepValue = jsonValue.findProperty("step");
		if (baseValue && stepValue) {
			const T base = parseNumber<T>(*baseValue);
			const T step = parseNumber<T>(*stepValue);
			value = static_cast<T>(base + step * static_cast<T>(context.stepIndex.x));
			return;
		}
	}
	value = parseNumber<T>(jsonValue);
}

template <size_t N, typename T>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, vec<N, T>& value) {
	if (jsonValue.isObject()) {
		const json::Value* const minValue = jsonValue.findProperty("min");
		const json::Value* const maxValue = jsonValue.findProperty("max");
		if (minValue && maxValue) {
			const vec<N, T> min = parseVector<N, T>(*minValue);
			const vec<N, T> max = parseVector<N, T>(*maxValue);
			if (any(greaterThan(min, max))) {
				throw json::Error{"Invalid range.", jsonValue.getSource()};
			}
			if (const json::Value* const exclude = jsonValue.findProperty("exclude")) {
				const vec<N, T> rangeSize = max - min;
				const vec<N, T> minMargin = rangeSize / T{10};
				const vec<N, T> excludeMin = parseVector<N, T>(exclude->getProperty("min"));
				const vec<N, T> excludeMax = parseVector<N, T>(exclude->getProperty("max"));
				if (any(greaterThan(excludeMin, excludeMax))) {
					throw json::Error{"Invalid exclude range.", exclude->getSource()};
				}
				if (all(lessThanEqual(excludeMin, min + minMargin) & greaterThanEqual(excludeMax, max - minMargin))) {
					throw json::Error{"Exclude range is too wide.", exclude->getSource()};
				}
				while (true) {
					for (size_t i = 0; i < N; ++i) {
						value[i] = rng::UniformRealDistribution<T>{min[i], max[i]}(context.numberGenerator);
					}
					const vec<N, bool> outside = lessThan(value, excludeMin) | greaterThan(value, excludeMax);
					if (all(greaterThanEqual(excludeMin, excludeMax) | outside) || any(outside)) {
						break;
					}
				}
			} else {
				for (size_t i = 0; i < N; ++i) {
					value[i] = rng::UniformRealDistribution<T>{min[i], max[i]}(context.numberGenerator);
				}
			}
			return;
		}

		if constexpr (N == 3) {
			const json::Value* const baseValue = jsonValue.findProperty("base");
			const json::Value* const stepValue = jsonValue.findProperty("step");
			if (baseValue && stepValue) {
				const vec<N, T> base = parseVector<N, T>(*baseValue);
				const vec<N, T> step = parseVector<N, T>(*stepValue);
				value = base + step * vec<N, T>{context.stepIndex};
				return;
			}
		}
	}
	if (jsonValue.isArray()) {
		if (jsonValue.getArraySize() != N) {
			throw json::Error{"Invalid number of array items.", jsonValue.getSource()};
		}
		for (size_t i = 0; i < N; ++i) {
			parseEntityComponent(context, jsonValue.getItem(i), value[i]);
		}
	} else {
		T component{};
		parseEntityComponent(context, jsonValue, component);
		value = vec<N, T>{component};
	}
}

inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, quat& value) {
	if (jsonValue.isArray()) {
		if (jsonValue.getArraySize() == 3) {
			vec3 pitchYawRollAngles{};
			parseEntityComponent(context, jsonValue, pitchYawRollAngles);
			value = convertAnglesToQuaternion(convertDegreesToRadians(pitchYawRollAngles));
		} else {
			vec4 components{};
			parseEntityComponent(context, jsonValue, components);
			value = quat{components.x, components.y, components.z, components.w};
		}
	} else if (jsonValue.isObject()) {
		const json::Value* const directionValue = jsonValue.findProperty("direction");
		const json::Value* const upValue = jsonValue.findProperty("up");
		if (directionValue && upValue) {
			vec3 direction{};
			vec3 up{};
			parseEntityComponent(context, *directionValue, direction);
			parseEntityComponent(context, *upValue, up);
			value = quatLookAt(direction, up);
		} else {
			const json::Value* const angleValue = jsonValue.findProperty("angle");
			const json::Value* const axisValue = jsonValue.findProperty("axis");
			if (angleValue && axisValue) {
				float angle{};
				vec3 axis{};
				parseEntityComponent(context, *angleValue, angle);
				parseEntityComponent(context, *axisValue, axis);
				value = angleAxis(convertDegreesToRadians(angle), axis);
			} else {
				value = quat{0.0f, 0.0f, 0.0f, 1.0f};
			}
		}
	} else {
		throw json::Error{"Expected an array or an object.", jsonValue.getSource()};
	}
}

template <size_t C, size_t R, typename T>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, mat<C, R, T>& value) {
	if constexpr (C == 4 && R == 4 && same_as<T, float>) {
		if (jsonValue.isObject()) {
			value = mat4{1.0f};
			if (const json::Value* const scaleValue = jsonValue.findProperty("scale")) {
				vec3 scales{};
				parseEntityComponent(context, *scaleValue, scales);
				value = scale(scales) * value;
			}
			if (const json::Value* const orientationValue = jsonValue.findProperty("orientation")) {
				quat orientation{};
				parseEntityComponent(context, *orientationValue, orientation);
				value = rotate(orientation) * value;
			}
			if (const json::Value* const translationValue = jsonValue.findProperty("translation")) {
				vec3 translation{};
				parseEntityComponent(context, *translationValue, translation);
				value = translate(translation) * value;
			}
			return;
		}
	}
	if (jsonValue.getArraySize() == size_t{C}) {
		for (size_t i = 0; i < C; ++i) {
			parseEntityComponent(context, jsonValue.getItem(i), value[i]);
		}
	} else if (jsonValue.getArraySize() == size_t{C * R}) {
		for (size_t i = 0; i < C; ++i) {
			for (size_t j = 0; j < R; ++j) {
				parseEntityComponent(context, jsonValue.getItem(i * R + j), value[i][j]);
			}
		}
	} else {
		throw json::Error{"Invalid number of array items.", jsonValue.getSource()};
	}
}

inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Color& value) {
	if (jsonValue.isObject()) {
		const json::Value* const minValue = jsonValue.findProperty("min");
		const json::Value* const maxValue = jsonValue.findProperty("max");
		if (minValue && maxValue) {
			const vec4 min = parseVector<4, float>(*minValue);
			const vec4 max = parseVector<4, float>(*maxValue);
			if (any(greaterThan(min, max))) {
				throw json::Error{"Invalid range.", jsonValue.getSource()};
			}
			vec4 rgba{};
			for (size_t i = 0; i < 4; ++i) {
				rgba[i] = rng::UniformRealDistribution<float>{min[i], max[i]}(context.numberGenerator);
			}
			value = Color::fromLinear(rgba);
			return;
		}
	}
	if (jsonValue.getArraySize() == 3) {
		vec3 rgb{};
		parseEntityComponent(context, jsonValue, rgb);
		value = Color::fromLinear(rgb);
	} else if (jsonValue.getArraySize() == 4) {
		vec4 rgba{};
		parseEntityComponent(context, jsonValue, rgba);
		value = Color::fromLinear(rgba);
	} else {
		throw json::Error{"Invalid number of array items.", jsonValue.getSource()};
	}
}

template <size_t N, typename UnitT>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, phys::Quantity<N, UnitT>& value) {
	if constexpr (N == 1) {
		float component{};
		parseEntityComponent(context, jsonValue, component);
		if constexpr (same_as<UnitT, phys::Radians> || same_as<UnitT, phys::Absolute<phys::Radians>>) {
			value = component * phys::DEGREES;
		} else {
			value = component * UnitT{};
		}
	} else {
		vec<N, float> components{};
		parseEntityComponent(context, jsonValue, components);
		if constexpr (same_as<UnitT, phys::Radians> || same_as<UnitT, phys::Absolute<phys::Radians>>) {
			value = components * phys::DEGREES;
		} else {
			value = components * UnitT{};
		}
	}
}

template <size_t N>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, phys::Orientation<N>& value) {
	if constexpr (N == 2) {
		float angleInDegrees{};
		parseEntityComponent(context, jsonValue, angleInDegrees);
		value = phys::Orientation2D{angleInDegrees * phys::DEGREES};
	} else {
		quat quaternion{};
		parseEntityComponent(context, jsonValue, quaternion);
		value = phys::Orientation3D{quaternion};
	}
}

template <size_t N>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, phys::LocalTransformation<N>& value) {
	if (jsonValue.isObject()) {
		value = phys::LocalTransformation<N>{};
		if (const json::Value* const scaleValue = jsonValue.findProperty("scale")) {
			phys::Scale<N> scaleQuantity{};
			parseEntityComponent(context, *scaleValue, scaleQuantity);
			value = scale(scaleQuantity) * value;
		}
		if (const json::Value* const orientationValue = jsonValue.findProperty("orientation")) {
			phys::Orientation<N> orientationQuantity{};
			parseEntityComponent(context, *orientationValue, orientationQuantity);
			value = rotate(orientationQuantity) * value;
		}
		if (const json::Value* const translationValue = jsonValue.findProperty("translation")) {
			phys::Length<N> translationQuantity{};
			parseEntityComponent(context, *translationValue, translationQuantity);
			value = translate(translationQuantity) * value;
		}
	} else {
		throw json::Error{"Expected an object.", jsonValue.getSource()};
	}
}

inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, SynchronizedEntityID& value) {
	if (jsonValue.isNumber()) {
		parseValue(jsonValue, value);
	} else if (jsonValue.isString()) {
		if (const auto it = context.namedEntities.find(jsonValue.getString()); it != context.namedEntities.end()) {
			value = it->second;
		} else {
			throw json::Error{formatString("No entity named \"{}\".", jsonValue.getString()), jsonValue.getSource()};
		}
	} else {
		throw json::Error{"Expected a string or a non-negative integer.", jsonValue.getSource()};
	}
}

template <typename T>
void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Optional<T>& value);

template <typename T, size_t N>
void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Array<T, N>& value);

template <typename T>
void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, ArrayList<T>& value);

template <typename T, size_t N>
void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, SmallArrayList<T, N>& value);

template <typename T>
void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Buffer<T>& value);

template <typename T, size_t N>
void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, SmallBuffer<T, N>& value);

template <typename T1, typename T2>
void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Pair<T1, T2>& value);

template <aggregate Aggregate>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Aggregate& value) requires(!requires { value.parseValueFrom(jsonValue); }) {
	const json::Object& object = jsonValue.getObject();
	meta::forEachNamedField(value, [&](StringView name, auto& field) -> void {
		if (const auto it = object.find(name); it != object.end()) {
			try {
				parseEntityComponent(context, it->second, field);
			} catch (...) {
				Error::throwWithNested(json::Error{formatString("Invalid property \"{}\"", name), jsonValue.getSource()});
			}
		}
	});
}

template <typename T>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Optional<T>& value) {
	if (jsonValue.isNull()) {
		value.reset();
	} else {
		parseEntityComponent(context, jsonValue, value.emplace());
	}
}

template <typename T, size_t N>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Array<T, N>& value) {
	const json::Array& array = jsonValue.getArray();
	if (array.size() != N) {
		throw json::Error{formatString("Expected {} array elements.", N), jsonValue.getSource()};
	}
	for (size_t i = 0; i < N; ++i) {
		parseEntityComponent(context, array[i], value[i]);
	}
}

template <typename T>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, ArrayList<T>& value) {
	const json::Array& array = jsonValue.getArray();
	value.clear();
	value.reserve(array.size());
	for (const json::Value& item : array) {
		parseEntityComponent(context, item, value.emplace_back());
	}
}

template <typename T, size_t N>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, SmallArrayList<T, N>& value) {
	const json::Array& array = jsonValue.getArray();
	value.clear();
	value.reserve(array.size());
	for (const json::Value& item : array) {
		parseEntityComponent(context, item, value.emplace_back());
	}
}

template <typename T>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Buffer<T>& value) {
	const json::Array& array = jsonValue.getArray();
	value.clear();
	for (const json::Value& item : array) {
		parseEntityComponent(context, item, value.push_back_unspecified_value());
	}
}

template <typename T, size_t N>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, SmallBuffer<T, N>& value) {
	const json::Array& array = jsonValue.getArray();
	value.clear();
	for (const json::Value& item : array) {
		parseEntityComponent(context, item, value.push_back_unspecified_value());
	}
}

template <typename T1, typename T2>
inline void parseEntityComponent(ParseEntityComponentContext& context, const json::Value& jsonValue, Pair<T1, T2>& value) {
	const json::Array& array = jsonValue.getArray();
	if (array.size() != 2) {
		throw json::Error{"Expected 2 array elements.", jsonValue.getSource()};
	}
	parseEntityComponent(context, array[0], value.first);
	parseEntityComponent(context, array[1], value.second);
}

using SynchronizedEntityIDAddresses = SmallArrayList<SynchronizedEntityID*, 2>;

inline void collectSynchronizedEntityIDAddresses(SynchronizedEntityIDAddresses& output, auto& value) {
	(void)output;
	(void)value;
}

inline void collectSynchronizedEntityIDAddresses(SynchronizedEntityIDAddresses& output, SynchronizedEntityID& value) {
	output.push_back(&value);
}

template <aggregate Aggregate>
inline void collectSynchronizedEntityIDAddresses(SynchronizedEntityIDAddresses& output, Aggregate& value) {
	meta::forEachField(value, [&](auto& field) -> void { collectSynchronizedEntityIDAddresses(output, field); });
}

struct ComponentInitializer {
	struct Compare {
		[[nodiscard]] bool operator()(const ComponentInitializer& a, const ComponentInitializer& b) const noexcept {
			return static_cast<uint32_t>(a.componentNameCRC32) < static_cast<uint32_t>(b.componentNameCRC32);
		}

		[[nodiscard]] bool operator()(CRC32 a, const ComponentInitializer& b) const noexcept {
			return static_cast<uint32_t>(a) < static_cast<uint32_t>(b.componentNameCRC32);
		}

		[[nodiscard]] bool operator()(const ComponentInitializer& a, CRC32 b) const noexcept {
			return static_cast<uint32_t>(a.componentNameCRC32) < static_cast<uint32_t>(b);
		}
	};

	CRC32 componentNameCRC32;
	const void* value;
};

struct ComponentInitializersView {
	Span<const ComponentInitializer> sortedInitializers{};
};

template <typename... Components>
struct ComponentInitializers {
	Array<ComponentInitializer, sizeof...(Components)> sortedInitializers;

	constexpr ComponentInitializers(const Components&... components);
};

template <typename... Components>
ComponentInitializers(const Components&...) -> ComponentInitializers<Components...>;

class EntityInitializer : private Variant<Monostate, const json::Object*, Span<const byte>*, ComponentInitializersView, Span<const EntityInitializer>> {
public:
	constexpr EntityInitializer() noexcept = default;

	constexpr EntityInitializer(const json::Object& object)
		: Variant(std::in_place_type<const json::Object*>, &object) {}

	constexpr EntityInitializer(json::Object&&) = delete;

	constexpr EntityInitializer(Span<const byte>& input)
		: Variant(std::in_place_type<Span<const byte>*>, &input) {}

	constexpr EntityInitializer(ComponentInitializersView componentInitializersView)
		: Variant(std::in_place_type<ComponentInitializersView>, componentInitializersView) {}

	template <typename... Components>
	constexpr EntityInitializer(ComponentInitializers<Components...>&& componentInitializers)
		: Variant(std::in_place_type<ComponentInitializersView>, ComponentInitializersView{.sortedInitializers = std::move(componentInitializers).sortedInitializers}) {}

	constexpr EntityInitializer(Span<const EntityInitializer> initializers)
		: Variant(std::in_place_type<Span<const EntityInitializer>>, initializers) {}

	constexpr explicit operator bool() const noexcept {
		return !is<Monostate>();
	}

	template <typename Component>
	[[nodiscard]] bool hasComponentInitializer() const;

	template <typename Component>
	bool initializeComponent(Component& component, ParseEntityComponentContext& context, const Schema& schema) const;

	template <typename Component>
	bool addOrAssignComponent(Component& component, ParseEntityComponentContext& context) const;
};

struct StateComponentDescription {
	template <exec::component T>
	[[nodiscard]] static constexpr StateComponentDescription create(CStringView name) noexcept {
		return {
			.name = name,
			.nameCRC32 = CRC32{name},
			.constructInPrefab = [](SynchronizedEntityIDAddresses& synchronizedEntityIDAddresses, const ArenaAllocator<byte>& allocator, ParseEntityComponentContext& context,
									 [[maybe_unused]] const Schema& schema, const json::Object& object, CStringView name) -> void* {
				if (const auto it = object.find(name); it != object.end()) {
					T* const value = new (ArenaAllocator<T>{allocator}.allocate(1)) T{}; // NOLINT(cppcoreguidelines-owning-memory)
					try {
						if constexpr (requires(T component, ParseEntityComponentContext& context_, const Schema& schema_, const EntityInitializer initializer) {
										  component.initializeComponentDefault(context_, schema_, initializer);
									  }) {
							value->initializeComponentDefault(context, schema, EntityInitializer{object});
						} else if constexpr (requires(T component, ParseEntityComponentContext& context_, const Schema& schema_, const EntityInitializer initializer) {
												 initializeComponentDefault(component, context_, schema_, initializer);
											 }) {
							initializeComponentDefault(*value, context, schema, EntityInitializer{object});
						}
						parseEntityComponent(context, it->second, *value);
						collectSynchronizedEntityIDAddresses(synchronizedEntityIDAddresses, *value);
					} catch (...) {
						value->~T();
						throw;
					}
					return value;
				}
				return nullptr;
			},
			.destroyInPrefab = [](void* value) noexcept -> void { static_cast<T*>(value)->~T(); },
			.add = [](EntityBuilder& entityBuilder, ParseEntityComponentContext& context, const Schema& schema, const EntityInitializer& initializer) -> void {
				T value{};
				initializer.initializeComponent(value, context, schema);
				entityBuilder.addOrAssignComponent<T>(std::move(value));
			},
			.setImpliedComponents =
				[]([[maybe_unused]] EntityBuilder& entityBuilder, [[maybe_unused]] EntityRegistry& registry, [[maybe_unused]] ResourceRegistry& resources) -> void {
				if constexpr (requires(T component, EntityBuilder& entityBuilder_, EntityRegistry& registry_, ResourceRegistry& resources_) {
								  component.setImpliedComponents(entityBuilder_, registry_, resources_);
							  }) {
					entityBuilder.getComponent<T>().setImpliedComponents(entityBuilder, registry, resources);
				}
			},
			.transform = []([[maybe_unused]] EntityRegistry& registry, [[maybe_unused]] EntityID entityID, [[maybe_unused]] phys::Position3D position,
							 [[maybe_unused]] phys::Orientation3D orientation) -> void {
				if constexpr (same_as<T, phys::Position3D>) {
					phys::Position3D& p = registry.getComponent<phys::Position3D>(entityID);
					p = position + orientation(p - 0);
				} else if constexpr (same_as<T, phys::Orientation3D>) {
					phys::Orientation3D& o = registry.getComponent<phys::Orientation3D>(entityID);
					o = orientation * o;
				}
			},
			.hasStateDelta = [](const EntityRegistry& oldRegistry, EntityID oldEntityID, const EntityRegistry& newRegistry, EntityID newEntityID) -> bool {
				return newRegistry.getComponent<T>(newEntityID) != oldRegistry.getComponent<T>(oldEntityID);
			},
			.serializeState = [](const EntityRegistry& registry, EntityID entityID, Writer output) -> void { serialize(registry.getComponent<T>(entityID), output); },
			.deserializeState = [](EntityRegistry& registry, EntityID entityID, SpanReader input) -> void {
				if (!deserialize(registry.getComponent<T>(entityID), input)) {
					throw std::invalid_argument{"Invalid component layout."};
				}
			},
			.mutationInvalidatesPhysicsObjectBounds = same_as<T, phys::Position3D> || same_as<T, phys::Orientation3D> || same_as<T, phys::Scale3D>,
		};
	}

	CStringView name;
	CRC32 nameCRC32;
	void* (*constructInPrefab)(SynchronizedEntityIDAddresses& synchronizedEntityIDAddresses, const ArenaAllocator<byte>& allocator, ParseEntityComponentContext& context,
		const Schema& schema, const json::Object& object, CStringView name);
	void (*destroyInPrefab)(void* value) noexcept;
	void (*add)(EntityBuilder& entityBuilder, ParseEntityComponentContext& context, const Schema& schema, const EntityInitializer& initializer);
	void (*setImpliedComponents)(EntityBuilder& entityBuilder, EntityRegistry& registry, ResourceRegistry& resources);
	void (*transform)(EntityRegistry& registry, EntityID entityID, phys::Position3D position, phys::Orientation3D orientation);
	bool (*hasStateDelta)(const EntityRegistry& oldRegistry, EntityID oldEntityID, const EntityRegistry& newRegistry, EntityID newEntityID);
	void (*serializeState)(const EntityRegistry& registry, EntityID entityID, Writer output);
	void (*deserializeState)(EntityRegistry& registry, EntityID entityID, SpanReader input);
	bool mutationInvalidatesPhysicsObjectBounds;
};

struct IntermediateComponentDescription {
	template <exec::component T>
	[[nodiscard]] static constexpr IntermediateComponentDescription create(CStringView name) noexcept {
		return {
			.name = name,
			.nameCRC32 = CRC32{name},
			.add = [](EntityBuilder& entityBuilder) -> void { entityBuilder.addComponentIfMissing<T>(); },
		};
	}

	CStringView name;
	CRC32 nameCRC32;
	void (*add)(EntityBuilder& entityBuilder);
};

struct ClientsideComponentDescription {
	template <exec::component T>
	[[nodiscard]] static constexpr ClientsideComponentDescription create(CStringView name) noexcept {
		return {
			.name = name,
			.nameCRC32 = CRC32{name},
			.add = [](EntityBuilder& entityBuilder) -> void { entityBuilder.addComponentIfMissing<T>(); },
		};
	}

	CStringView name;
	CRC32 nameCRC32;
	void (*add)(EntityBuilder& entityBuilder);
};

struct EntityDescription {
	String name{};
	EntityID::Flags flags{};
	ArrayList<StateComponentDescription> stateComponents{};
	ArrayList<IntermediateComponentDescription> intermediateComponents{};
	ArrayList<ClientsideComponentDescription> clientsideComponents{};
	Optional<phys::ObjectOptions3D> physicsObjectOptions{};
};

class Schema {
public:
	explicit Schema(EntityID::Flags entityFlags)
		: entityFlags(entityFlags) {}

	void extend(String fileContents, CStringView filepath = {}, const Filesystem* filesystem = nullptr, HashSet<CStringView>* visitedFilepaths = nullptr) {
		const CRC32 newCRC32 = crc32 + fileContents;
		extendImplementation(std::move(fileContents), filepath, filesystem, visitedFilepaths);
		crc32 = newCRC32;
	}

	FPS_SHARED_API void preloadAssets(AssetCache& assetCache);

	[[nodiscard]] EntityID::Flags getEntityFlags() const noexcept {
		return entityFlags;
	}

	[[nodiscard]] CRC32 getCRC32() const noexcept {
		return crc32;
	}

	[[nodiscard]] StringView getName() const noexcept {
		return name;
	}

	[[nodiscard]] CStringView getPlayerPrefabFilepath() const noexcept {
		return playerPrefabFilepath;
	}

	[[nodiscard]] CStringView getDeadPlayerPrefabFilepath() const noexcept {
		return deadPlayerPrefabFilepath;
	}

	[[nodiscard]] phys::CollisionLayers getModelObjectDefaultDetectionLayers() const noexcept {
		return modelObjectDefaultDetectionLayers;
	}

	[[nodiscard]] phys::CollisionLayers getModelObjectDefaultResponseLayers() const noexcept {
		return modelObjectDefaultResponseLayers;
	}

	[[nodiscard]] phys::CollisionLayer getModelCollisionLayer(const String& layerName) {
		if (layerName.empty()) {
			return modelObjectDefaultLayer;
		}
		if (const auto it = modelCollisionLayerMap.find(name); it != modelCollisionLayerMap.end()) {
			return it->second;
		}
		const auto [it, inserted] = modelCollisionLayerMap.try_emplace(layerName, nextCollisionLayer);
		if (inserted && nextCollisionLayer != phys::CollisionLayer::MAX) {
			nextCollisionLayer = phys::CollisionLayer{static_cast<size_t>(nextCollisionLayer) + 1};
		}
		return it->second;
	}

	[[nodiscard]] auto getSoundDescriptions() const noexcept {
		return Subrange{soundDescriptions};
	}

	[[nodiscard]] const SoundDescription* findSoundDescription(SoundType soundType) const noexcept {
		if (const auto it = soundDescriptions.find(soundType); it != soundDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const SoundDescription& getSoundDescription(SoundType soundType) const {
		const SoundDescription* const result = findSoundDescription(soundType);
		if (!result) {
			throw Error{"Specified SoundType missing from schema."};
		}
		return *result;
	}

	[[nodiscard]] auto getDecalMaterialDescriptions() const noexcept {
		return Subrange{decalMaterialDescriptions};
	}

	[[nodiscard]] const DecalMaterialDescription* findDecalMaterialDescription(DecalMaterialType decalMaterialType) const noexcept {
		if (const auto it = decalMaterialDescriptions.find(decalMaterialType); it != decalMaterialDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const DecalMaterialDescription& getDecalMaterialDescription(DecalMaterialType decalMaterialType) const {
		const DecalMaterialDescription* const result = findDecalMaterialDescription(decalMaterialType);
		if (!result) {
			throw Error{"Specified DecalMaterialType missing from schema."};
		}
		return *result;
	}

	[[nodiscard]] auto getSpriteDescriptions() const noexcept {
		return Subrange{spriteDescriptions};
	}

	[[nodiscard]] const SpriteDescription* findSpriteDescription(SpriteType spriteType) const noexcept {
		if (const auto it = spriteDescriptions.find(spriteType); it != spriteDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const SpriteDescription& getSpriteDescription(SpriteType spriteType) const {
		const SpriteDescription* const result = findSpriteDescription(spriteType);
		if (!result) {
			throw Error{"Specified SpriteType missing from schema."};
		}
		return *result;
	}

	FPS_SHARED_API const ModelDescription& loadModelDescription(const Filesystem& filesystem, ModelType modelType);

	[[nodiscard]] auto getLoadedModelDescriptions() const noexcept {
		return Subrange{modelDescriptions};
	}

	[[nodiscard]] const ModelDescription* findLoadedModelDescription(ModelType modelType) const noexcept {
		if (const auto it = modelDescriptions.find(modelType); it != modelDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const ModelDescription& getLoadedModelDescription(ModelType modelType) const {
		const ModelDescription* const result = findLoadedModelDescription(modelType);
		if (!result) {
			throw Error{"Specified ModelType missing from schema."};
		}
		return *result;
	}

	[[nodiscard]] auto getProjectileDescriptions() const noexcept {
		return Subrange{projectileDescriptions};
	}

	[[nodiscard]] const ProjectileDescription* findProjectileDescription(ProjectileType projectileType) const noexcept {
		if (const auto it = projectileDescriptions.find(projectileType); it != projectileDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const ProjectileDescription& getProjectileDescription(ProjectileType projectileType) const {
		const ProjectileDescription* const result = findProjectileDescription(projectileType);
		if (!result) {
			throw Error{"Specified ProjectileType missing from schema."};
		}
		return *result;
	}

	[[nodiscard]] auto getWeaponDescriptions() const noexcept {
		return Subrange{weaponDescriptions};
	}

	[[nodiscard]] const WeaponDescription* findWeaponDescription(WeaponType weaponType) const noexcept {
		if (const auto it = weaponDescriptions.find(weaponType); it != weaponDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const WeaponDescription& getWeaponDescription(WeaponType weaponType) const {
		const WeaponDescription* const result = findWeaponDescription(weaponType);
		if (!result) {
			throw Error{"Specified WeaponType missing from schema."};
		}
		return *result;
	}

	[[nodiscard]] auto getMovementDescriptions() const noexcept {
		return Subrange{movementDescriptions};
	}

	[[nodiscard]] const MovementDescription* findMovementDescription(MovementType movementType) const noexcept {
		if (const auto it = movementDescriptions.find(movementType); it != movementDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const MovementDescription& getMovementDescription(MovementType movementType) const {
		const MovementDescription* const result = findMovementDescription(movementType);
		if (!result) {
			throw Error{"Specified MovementType missing from schema."};
		}
		return *result;
	}

	[[nodiscard]] auto getParticleDescriptions() const noexcept {
		return Subrange{particleDescriptions};
	}

	[[nodiscard]] const ParticleDescription* findParticleDescription(ParticleType particleType) const noexcept {
		if (const auto it = particleDescriptions.find(particleType); it != particleDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const ParticleDescription& getParticleDescription(ParticleType particleType) const {
		const ParticleDescription* const result = findParticleDescription(particleType);
		if (!result) {
			throw Error{"Specified ParticleType missing from schema."};
		}
		return *result;
	}

	[[nodiscard]] auto getDamageableDescriptions() const noexcept {
		return Subrange{damageableDescriptions};
	}

	[[nodiscard]] const DamageableDescription* findDamageableDescription(DamageableType damageableType) const noexcept {
		if (const auto it = damageableDescriptions.find(damageableType); it != damageableDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const DamageableDescription& getDamageableDescription(DamageableType damageableType) const {
		const DamageableDescription* const result = findDamageableDescription(damageableType);
		if (!result) {
			throw Error{"Specified DamageableType missing from schema."};
		}
		return *result;
	}

	[[nodiscard]] auto getEntityDescriptions() const noexcept {
		return Subrange{entityDescriptions};
	}

	[[nodiscard]] const EntityDescription* findEntityDescription(EntityType entityType) const noexcept {
		if (const auto it = entityDescriptions.find(entityType); it != entityDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const EntityDescription& getEntityDescription(EntityType entityType) const {
		const EntityDescription* const result = findEntityDescription(entityType);
		if (!result) {
			throw Error{"Specified EntityType missing from schema."};
		}
		return *result;
	}

	FPS_SHARED_API const ModelObjectDescription& loadModelObjectDescription(AssetCache& assetCache, ModelType modelType);

	[[nodiscard]] auto getLoadedModelObjectDescriptions() const noexcept {
		return Subrange{modelObjectDescriptions};
	}

	[[nodiscard]] const ModelObjectDescription* findLoadedModelObjectDescription(ModelType modelType) const noexcept {
		if (const auto it = modelObjectDescriptions.find(modelType); it != modelObjectDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const ModelObjectDescription& getLoadedModelObjectDescription(ModelType modelType) const {
		const ModelObjectDescription* const result = findLoadedModelObjectDescription(modelType);
		if (!result) {
			throw Error{"Specified ModelType object missing from schema."};
		}
		return *result;
	}

	FPS_SHARED_API const phys::Shape3D& loadModelConvexHullShape(AssetCache& assetCache, ModelType modelType);

	[[nodiscard]] auto getLoadedModelConvexHullShapes() const noexcept {
		return Subrange{modelConvexHullShapes};
	}

	[[nodiscard]] const phys::Shape3D* findLoadedModelConvexHullShape(ModelType modelType) const noexcept {
		if (const auto it = modelConvexHullShapes.find(modelType); it != modelConvexHullShapes.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const phys::Shape3D& getLoadedModelConvexHullShape(ModelType modelType) const {
		const phys::Shape3D* const result = findLoadedModelConvexHullShape(modelType);
		if (!result) {
			throw Error{"Specified ModelType convex hull shape missing from schema."};
		}
		return *result;
	}

	FPS_SHARED_API const phys::Shape3D& loadModelTriangleMeshShape(AssetCache& assetCache, ModelType modelType);

	[[nodiscard]] auto getLoadedModelTriangleMeshShapes() const noexcept {
		return Subrange{modelTriangleMeshShapes};
	}

	[[nodiscard]] const phys::Shape3D* findLoadedModelTriangleMeshShape(ModelType modelType) const noexcept {
		if (const auto it = modelTriangleMeshShapes.find(modelType); it != modelTriangleMeshShapes.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const phys::Shape3D& getLoadedModelTriangleMeshShape(ModelType modelType) const {
		const phys::Shape3D* const result = findLoadedModelTriangleMeshShape(modelType);
		if (!result) {
			throw Error{"Specified ModelType triangle mesh shape missing from schema."};
		}
		return *result;
	}

private:
	FPS_SHARED_API void extendImplementation(String fileContents, CStringView filepath, const Filesystem* filesystem, HashSet<CStringView>* visitedFilepaths);

	EntityID::Flags entityFlags;
	CRC32 crc32{};
	String name{};
	String playerPrefabFilepath{};
	String deadPlayerPrefabFilepath{};
	phys::CollisionLayer modelObjectDefaultLayer{5};
	phys::CollisionLayers modelObjectDefaultDetectionLayers =
		phys::CollisionLayer{0} | phys::CollisionLayer{1} | phys::CollisionLayer{2} | phys::CollisionLayer{3} | phys::CollisionLayer{4};
	phys::CollisionLayers modelObjectDefaultResponseLayers =
		phys::CollisionLayer{0} | phys::CollisionLayer{1} | phys::CollisionLayer{2} | phys::CollisionLayer{3} | phys::CollisionLayer{4};
	HashMap<String, phys::CollisionLayer> modelCollisionLayerMap{};
	phys::CollisionLayer nextCollisionLayer{6};
	HashMap<SoundType, SoundDescription> soundDescriptions{};
	HashMap<DecalMaterialType, DecalMaterialDescription> decalMaterialDescriptions{};
	HashMap<SpriteType, SpriteDescription> spriteDescriptions{};
	HashMap<ModelType, ModelDescription> modelDescriptions{};
	HashMap<ProjectileType, ProjectileDescription> projectileDescriptions{};
	HashMap<WeaponType, WeaponDescription> weaponDescriptions{};
	HashMap<MovementType, MovementDescription> movementDescriptions{};
	HashMap<ParticleType, ParticleDescription> particleDescriptions{};
	HashMap<DamageableType, DamageableDescription> damageableDescriptions{};
	HashMap<EntityType, EntityDescription> entityDescriptions{};
	HashMap<ModelType, ModelObjectDescription> modelObjectDescriptions{};
	HashMap<ModelType, phys::Shape3D> modelConvexHullShapes{};
	HashMap<ModelType, phys::Shape3D> modelTriangleMeshShapes{};
};

struct SpawnEntityResult {
	EntityID entityID;
	SynchronizedEntityID synchronizedEntityID;
};

FPS_SHARED_API void setImpliedModelJointComponents(EntityBuilder& entityBuilder, ModelType modelType, EntityType entityType, res::Model::JointIndex jointIndex,
	EntityRegistry& registry, ResourceRegistry& resources);

FPS_SHARED_API SpawnEntityResult spawnEntity(EntityRegistry& registry, ResourceRegistry& resources, ParseEntityComponentContext& context, EntityType entityType,
	EntityID::Flags flags, const EntityInitializer& initializer = {}, phys::Position3D position = {}, phys::Orientation3D orientation = {});

inline SpawnEntityResult spawnEntity(EntityRegistry& registry, ResourceRegistry& resources, EntityType entityType, EntityID::Flags flags, const EntityInitializer& initializer = {},
	phys::Position3D position = {}, phys::Orientation3D orientation = {}) {
	ParseEntityComponentContext context{};
	return spawnEntity(registry, resources, context, entityType, flags, initializer, position, orientation);
}

FPS_SHARED_API void spawnDecal(EntityRegistry& registry, ResourceRegistry& resources, DecalMaterialType decalMaterialType, EntityID::Flags flags, phys::Position3D position,
	phys::Orientation3D orientation, phys::Length2D size, phys::Distance range, EntityID targetEntityID, TickIndex tickIndex, Duration tickInterval);

FPS_SHARED_API void spawnParticle(EntityRegistry& registry, ResourceRegistry& resources, rng::Xoroshiro128PlusPlusEngine& numberGenerator, ParticleType particleType,
	EntityID::Flags flags, phys::Position3D position, phys::Orientation3D orientation, phys::LinearVelocity3D linearVelocity, EntityID ownerEntityID, TickIndex tickIndex,
	Duration tickInterval);

FPS_SHARED_API void killEntity(EntityRegistry& registry, ResourceRegistry& resources, EntityID entityID);

#endif
