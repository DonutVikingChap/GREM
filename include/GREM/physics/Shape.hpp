// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_SHAPE_HPP
#define GREM_PHYSICS_SHAPE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ConvexPolytope.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/TriangleMesh.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/physics/quantities.hpp>

#include <utility> // std::move

namespace grem::physics {

/**
 * Identifier for a collision layer that a collider may belong to.
 */
class CollisionLayer {
public:
	/**
	 * Maximum collision layer value.
	 */
	static const CollisionLayer MAX;

	/**
	 * Construct a layer identifier with a specific index.
	 *
	 * \param index index of the layer.
	 */
	constexpr explicit CollisionLayer(size_t index) noexcept
		: index(static_cast<uint8_t>(index)) {}

	/**
	 * Convert this layer identifier to its underlying index.
	 *
	 * \return the index of the layer.
	 */
	constexpr explicit operator size_t() const noexcept {
		return static_cast<size_t>(index);
	}

	/**
	 * Compare this layer identifier to another for equality.
	 *
	 * \param other the layer identifier to compare this one to.
	 *
	 * \return true if the layer identifiers are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const CollisionLayer& other) const noexcept = default;

private:
	uint8_t index;
};

inline constexpr CollisionLayer CollisionLayer::MAX = CollisionLayer{31};

/**
 * Set of collision layers that a collider may belong to.
 */
class CollisionLayers {
public:
	/**
	 * Set containing all possible layers.
	 */
	static const CollisionLayers ALL;

	/**
	 * Construct an empty layer set.
	 */
	constexpr CollisionLayers() noexcept = default;

	/**
	 * Construct a layer set containing only one specific layer.
	 *
	 * \param layer layer identifier to include.
	 *
	 * \note Layer sets can be combined using
	 *       operator|(CollisionLayer, CollisionLayer).
	 */
	constexpr CollisionLayers(CollisionLayer layer) noexcept
		: bits(uint32_t{1} << static_cast<size_t>(layer)) {}

	/**
	 * Compare this layer set to another for equality.
	 *
	 * \param other the layer set to compare this one to.
	 *
	 * \return true if the layer sets are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const CollisionLayers& other) const noexcept = default;

	/**
	 * Check if the layer set is empty.
	 *
	 * \return true if the set contains no layers, false otherwise.
	 */
	[[nodiscard]] constexpr bool empty() const noexcept {
		return bits == 0;
	}

	/**
	 * Check if the layer set contains the given layer.
	 *
	 * \param layer layer identifier to check for.
	 *
	 * \return true if the set contains the given layer, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(CollisionLayer layer) const noexcept {
		return (bits & CollisionLayers{layer}.bits) != 0;
	}

	/**
	 * Check if the layer set contains at least one of the given layers.
	 *
	 * \param layers layer set to check for.
	 *
	 * \return true if the set contains at least one of the given layers, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool containsAnyOf(CollisionLayers layers) const noexcept {
		return (bits & layers.bits) != 0;
	}

	/**
	 * Check if the layer set contains all of the given layers.
	 *
	 * \param layers layer set to check for.
	 *
	 * \return true if the set contains all of the given layers, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool containsAllOf(CollisionLayers layers) const noexcept {
		return (bits & layers.bits) == layers.bits;
	}

	/**
	 * Get the complement of a layer set.
	 *
	 * \param a the set to invert.
	 *
	 * \return a set containing all possible layers except those in the given
	 *         set.
	 */
	[[nodiscard]] friend constexpr CollisionLayers operator~(CollisionLayers a) noexcept {
		return CollisionLayers{~a.bits};
	}

	/**
	 * Get the intersection of two layer sets.
	 *
	 * \param a first layer set.
	 * \param b second layer set.
	 *
	 * \return a set containing all layers contained in both a and b.
	 */
	[[nodiscard]] friend constexpr CollisionLayers operator&(CollisionLayers a, CollisionLayers b) noexcept {
		return CollisionLayers{a.bits & b.bits};
	}

	/**
	 * Get the union of two layer sets.
	 *
	 * \param a first layer set.
	 * \param b second layer set.
	 *
	 * \return a set containing all layers contained in a or b or both.
	 */
	[[nodiscard]] friend constexpr CollisionLayers operator|(CollisionLayers a, CollisionLayers b) noexcept {
		return CollisionLayers{a.bits | b.bits};
	}

	/**
	 * Get the symmetric difference of two layer sets.
	 *
	 * \param a first layer set.
	 * \param b second layer set.
	 *
	 * \return a set containing all layers contained in either a or b, but not
	 *         both.
	 */
	[[nodiscard]] friend constexpr CollisionLayers operator^(CollisionLayers a, CollisionLayers b) noexcept {
		return CollisionLayers{a.bits ^ b.bits};
	}

	/**
	 * Assign the intersection of two layer sets to the first set.
	 *
	 * \param a first layer set.
	 * \param b second layer set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr CollisionLayers& operator&=(CollisionLayers& a, CollisionLayers b) noexcept {
		return a = a & b;
	}

	/**
	 * Assign the union of two layer sets to the first set.
	 *
	 * \param a first layer set.
	 * \param b second layer set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr CollisionLayers& operator|=(CollisionLayers& a, CollisionLayers b) noexcept {
		return a = a | b;
	}

	/**
	 * Assign the symmetric difference of two layer sets to the first set.
	 *
	 * \param a first layer set.
	 * \param b second layer set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr CollisionLayers& operator^=(CollisionLayers& a, CollisionLayers b) noexcept {
		return a = a ^ b;
	}

private:
	constexpr explicit CollisionLayers(uint32_t bits) noexcept
		: bits(bits) {}

	uint32_t bits = 0;
};

inline constexpr CollisionLayers CollisionLayers::ALL = ~CollisionLayers{};

/**
 * Get the complement of a collision layer.
 *
 * \param a the layer to invert.
 *
 * \return a set containing all possible layers except the given layer.
 */
constexpr CollisionLayers operator~(CollisionLayer a) noexcept {
	return ~CollisionLayers{a};
}

/**
 * Get the union of two collision layers.
 *
 * \param a first layer.
 * \param b second layer.
 *
 * \return a set containing both a and b.
 */
constexpr CollisionLayers operator|(CollisionLayer a, CollisionLayer b) noexcept {
	return CollisionLayers{a} | CollisionLayers{b};
}

/**
 * Collision filtering options for a collider.
 */
struct CollisionFilter {
	CollisionLayers layers = CollisionLayer{0};             ///< Layers that the collider belongs to.
	CollisionLayers detectionLayers = CollisionLayers::ALL; ///< Other colliders' layers that this collider wants to detect collisions with.
	CollisionLayers noDetectionLayers{};                    ///< Other colliders' layers that this collider skips collision detection with.
	CollisionLayers responseLayers = CollisionLayers::ALL;  ///< Other colliders' layers that this collider wants to respond to collisions with.
	CollisionLayers noResponseLayers{};                     ///< Other colliders' layers that this collider skips collision response with.
};

/**
 * Result of a CollisionFilterTest.
 */
struct CollisionFilterTestResult {
	using TestSet = uint8_t;

	enum : TestSet {
		DETECTS_COLLISION = 1 << 0,     ///< Both colliders wanted to detect collisions with each other on at least one of the tested layers.
		RESPONDS_TO_COLLISION = 1 << 1, ///< Both colliders wanted to respond to collisions with each other on at least one of the tested layers.
	};

	TestSet passed{}; ///< Set of filter tests that passed.

	/**
	 * Check if both colliders wanted to detect or respond to collisions on any
	 * of the tested layers.
	 *
	 * \return true if the filter test passed, false otherwise.
	 */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return passed != 0;
	}

	/**
	 * Compare this filter test result to another for equality.
	 *
	 * \param other the filter test result to compare this one to.
	 *
	 * \return true if the results are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const CollisionFilterTestResult& other) const noexcept = default;

	/**
	 * Check if both colliders want to detect collisions with each other on at
	 * least some tested layer.
	 *
	 * \return true if the filter test passed for collision detection, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool detectsCollision() const noexcept {
		return (passed & DETECTS_COLLISION) != 0;
	}

	/**
	 * Check if both colliders want to respond to collisions with each other on
	 * at least some tested layer.
	 *
	 * \return true if the filter test passed for collision response, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool respondsToCollision() const noexcept {
		return (passed & RESPONDS_TO_COLLISION) != 0;
	}
};

/**
 * Test that checks if a pair of colliders should collide.
 */
struct CollisionFilterTest {
	static const CollisionFilterTest DETECTION; ///< Test that checks if a pair of colliders may detect collisions with each other on any layer.
	static const CollisionFilterTest RESPONSE;  ///< Test that checks if a pair of colliders may respond to collisions with each other on any layer.

	CollisionLayers detectionLayers = CollisionLayers::ALL; ///< Layers to test for collision detection.
	CollisionLayers responseLayers = CollisionLayers::ALL;  ///< Layers to test for collision response.

	/**
	 * Check if a given pair of collider's filters meet the requirements.
	 *
	 * \param a collision filter of the first collider to test.
	 * \param b collision filter of the second collider to test.
	 *
	 * \return the result of the filter test.
	 */
	[[nodiscard]] constexpr CollisionFilterTestResult operator()(CollisionFilter a, CollisionFilter b) const noexcept {
		const CollisionLayers detectionLayersA = detectionLayers & a.detectionLayers & b.layers;
		const CollisionLayers detectionLayersB = detectionLayers & b.detectionLayers & a.layers;
		const CollisionLayers noDetectionLayersA = detectionLayers & a.noDetectionLayers & b.layers;
		const CollisionLayers noDetectionLayersB = detectionLayers & b.noDetectionLayers & a.layers;
		const bool wantsDetectionA = detectionLayersA != CollisionLayers{} && noDetectionLayersA == CollisionLayers{};
		const bool wantsDetectionB = detectionLayersB != CollisionLayers{} && noDetectionLayersB == CollisionLayers{};
		const CollisionLayers responseLayersA = responseLayers & a.responseLayers & b.layers;
		const CollisionLayers responseLayersB = responseLayers & b.responseLayers & a.layers;
		const CollisionLayers noResponseLayersA = responseLayers & a.noResponseLayers & b.layers;
		const CollisionLayers noResponseLayersB = responseLayers & b.noResponseLayers & a.layers;
		const bool wantsResponseA = responseLayersA != CollisionLayers{} && noResponseLayersA == CollisionLayers{};
		const bool wantsResponseB = responseLayersB != CollisionLayers{} && noResponseLayersB == CollisionLayers{};
		const bool detectsCollision = wantsDetectionA && wantsDetectionB;
		const bool respondsToCollision = wantsResponseA && wantsResponseB;
		CollisionFilterTestResult::TestSet passed{};
		static_assert(CollisionFilterTestResult::DETECTS_COLLISION == 1 << 0);
		passed |= static_cast<CollisionFilterTestResult::TestSet>(detectsCollision);
		static_assert(CollisionFilterTestResult::RESPONDS_TO_COLLISION == 1 << 1);
		passed |= static_cast<CollisionFilterTestResult::TestSet>(static_cast<CollisionFilterTestResult::TestSet>(respondsToCollision) << 1);
		return {.passed = passed};
	}
};

inline constexpr CollisionFilterTest CollisionFilterTest::DETECTION{
	.detectionLayers = CollisionLayers::ALL,
	.responseLayers{},
};

inline constexpr CollisionFilterTest CollisionFilterTest::RESPONSE{
	.detectionLayers{},
	.responseLayers = CollisionLayers::ALL,
};

/**
 * Description of a ray.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct Ray {
	using Unit = typename Position<N>::Unit;            ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless ray as a physical ray in default units.
	 *
	 * \param ray ray to reinterpret.
	 *
	 * \return the corresponding physical ray.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Ray reinterpret(const grem::Ray<N, float>& ray) noexcept {
		return {
			.origin = Position<N>::reinterpret(ray.origin),
			.direction = Direction<N>::reinterpret(ray.direction),
			.maxDistance = Distance::reinterpret(ray.maxDistance),
			.directionInverse = ray.directionInverse,
		};
	}

	/**
	 * Starting position of the ray.
	 */
	Position<N> origin;

	/**
	 * Unit vector pointing in the direction of the ray.
	 *
	 * \warning Must be a unit vector.
	 */
	Direction<N> direction;

	/**
	 * Maximum hit distance of the ray.
	 *
	 * \warning Must be non-negative.
	 */
	Distance maxDistance = Distance::MAX;

	/**
	 * Component-wise reciprocal of the ray direction.
	 *
	 * \warning Must be equal to the reciprocal of #direction.
	 */
	Scale<N> directionInverse = Scale<N>{1.0f} / direction;

	/**
	 * Convert this physical ray to a unitless ray given a desired unit.
	 *
	 * \param unit unit to get the ray in.
	 *
	 * \return the corresponding ray in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::Ray<N, float> in(OtherUnitT unit) const noexcept requires(OtherUnitT::DIMENSION == LENGTH) {
		return {
			.origin = origin.in(unit),
			.direction = direction,
			.maxDistance = maxDistance.in(unit),
			.directionInverse = directionInverse,
		};
	}
};
using Ray2D = Ray<2>; ///< Description of a ray in 2-dimensional space.
using Ray3D = Ray<3>; ///< Description of a ray in 3-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Ray<N> operator*(const grem::Ray<N, float>& ray, typename Ray<N>::Unit) {
	return Ray<N>::reinterpret(ray);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Ray<N> operator*(typename Ray<N>::Unit, const grem::Ray<N, float>& ray) {
	return Ray<N>::reinterpret(ray);
}

/**
 * Result of a raycast miss.
 */
struct RayMiss {
	using Unit = typename Distance::Unit;               ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless ray miss as a physical ray miss in default units.
	 *
	 * \param miss ray miss to reinterpret.
	 *
	 * \return the corresponding physical ray miss.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr RayMiss reinterpret(const grem::RayMiss& miss) noexcept {
		(void)miss;
		return {};
	}

	/**
	 * Convert this physical ray miss to a unitless ray miss given a desired
	 * unit.
	 *
	 * \param unit unit to get the ray miss in.
	 *
	 * \return the corresponding ray miss in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::RayMiss in(OtherUnitT unit) const noexcept requires(OtherUnitT::DIMENSION == LENGTH) {
		(void)unit;
		return {};
	}
};

[[nodiscard]] GREM_ALWAYS_INLINE constexpr RayMiss operator*(const grem::RayMiss& miss, typename RayMiss::Unit) {
	return RayMiss::reinterpret(miss);
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr RayMiss operator*(typename RayMiss::Unit, const grem::RayMiss& miss) {
	return RayMiss::reinterpret(miss);
}

/**
 * Result of a raycast hit.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct RayHit {
	using Unit = typename Position<N>::Unit;            ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless ray hit as a physical ray hit in default units.
	 *
	 * \param hit ray hit to reinterpret.
	 *
	 * \return the corresponding physical ray hit.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr RayHit reinterpret(const grem::RayHit<N, float>& hit) noexcept {
		return {
			.localOffset = Length<N>::reinterpret(hit.localOffset),
			.distance = Distance::reinterpret(hit.distance),
			.normal = Direction<N>::reinterpret(hit.normal),
		};
	}

	Length<N> localOffset; ///< Offset from the object's center of mass in shape-local space at which the hit occured.
	Distance distance;     ///< Distance from the ray origin, along the ray direction, at which the hit occured.
	Direction<N> normal;   ///< Unit normal vector of the surface that was hit.

	/**
	 * Convert this physical ray hit to a unitless ray hit given a desired unit.
	 *
	 * \param unit unit to get the ray hit in.
	 *
	 * \return the corresponding ray hit in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::RayHit<N, float> in(OtherUnitT unit) const noexcept requires(OtherUnitT::DIMENSION == LENGTH) {
		return {.localOffset = localOffset.in(unit), .distance = distance.in(unit), .normal = normal};
	}
};
using RayHit2D = RayHit<2>; ///< Result of a raycast hit in 2-dimensional space.
using RayHit3D = RayHit<3>; ///< Result of a raycast hit in 3-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr RayHit<N> operator*(const grem::RayHit<N, float>& hit, typename RayHit<N>::Unit) {
	return RayHit<N>::reinterpret(hit);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr RayHit<N> operator*(typename RayHit<N>::Unit, const grem::RayHit<N, float>& hit) {
	return RayHit<N>::reinterpret(hit);
}

/**
 * Result of a raycast interior hit.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct RayHitInterior {
	using Unit = typename Distance::Unit;               ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless ray hit as a physical ray hit in default units.
	 *
	 * \param hit ray hit to reinterpret.
	 *
	 * \return the corresponding physical ray hit.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr RayHitInterior reinterpret(const grem::RayHitInterior<N, float>& hit) noexcept {
		return {.localOffset = Length<N>::reinterpret(hit.localOffset)};
	}

	Length<N> localOffset; ///< Offset from the object's center of mass in shape-local space at which the hit occured.

	/**
	 * Convert this physical ray hit to a unitless ray hit given a desired unit.
	 *
	 * \param unit unit to get the ray hit in.
	 *
	 * \return the corresponding ray hit in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::RayHitInterior<N, float> in(OtherUnitT unit) const noexcept requires(OtherUnitT::DIMENSION == LENGTH) {
		return {.localOffset = localOffset.in(unit)};
	}
};
using RayHitInterior2D = RayHitInterior<2>; ///< Result of a raycast interior hit in 2-dimensional space.
using RayHitInterior3D = RayHitInterior<3>; ///< Result of a raycast interior hit in 3-dimensional space.

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr RayHitInterior<N> operator*(const grem::RayHitInterior<N, T>& hit, typename RayHitInterior<N>::Unit) {
	return RayHitInterior<N>::reinterpret(hit);
}

template <size_t N, typename T>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr RayHitInterior<N> operator*(typename RayHitInterior<N>::Unit, const grem::RayHitInterior<N, T>& hit) {
	return RayHitInterior<N>::reinterpret(hit);
}

/**
 * Result of a raycast.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct RaycastResult : Variant<RayMiss, RayHit<N>, RayHitInterior<N>> {
	using Unit = typename Position<N>::Unit;            ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless raycast result as a physical raycast result in
	 * default units.
	 *
	 * \param raycastResult raycast result to reinterpret.
	 *
	 * \return the corresponding physical raycast result.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr RaycastResult reinterpret(const grem::RaycastResult<N, float>& raycastResult) noexcept {
		GREM_MATCH(raycastResult) {
			GREM_CASE(const grem::RayMiss& miss) {
				return RayMiss::reinterpret(miss);
			}
			GREM_CASE(const grem::RayHit<N, float>& hit) {
				return RayHit<N>::reinterpret(hit);
			}
			GREM_CASE(const grem::RayHitInterior<N, float>& hitInterior) {
				return RayHitInterior<N>::reinterpret(hitInterior);
			}
		}
		unreachable();
	}

	using Variant<RayMiss, RayHit<N>, RayHitInterior<N>>::Variant;

	/**
	 * Convert this physical raycast result to a unitless raycast result given a
	 * desired unit.
	 *
	 * \param unit unit to get the raycast result in.
	 *
	 * \return the corresponding raycast result in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::RaycastResult<N, float> in(OtherUnitT unit) const requires(OtherUnitT::DIMENSION == LENGTH) {
		(void)unit;
		return match(*this)([](const auto& r) -> grem::RaycastResult<N, float> { return r.in(OtherUnitT{}); });
	}
};
using RaycastResult2D = RaycastResult<2>; ///< Result of a raycast in 2-dimensional space.
using RaycastResult3D = RaycastResult<3>; ///< Result of a raycast in 3-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr RaycastResult<N> operator*(const grem::RaycastResult<N, float>& raycastResult, typename RaycastResult<N>::Unit) {
	return RaycastResult<N>::reinterpret(raycastResult);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr RaycastResult<N> operator*(typename RaycastResult<N>::Unit, const grem::RaycastResult<N, float>& raycastResult) {
	return RaycastResult<N>::reinterpret(raycastResult);
}

/**
 * Description of an infinite plane in world space.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct Plane {
	using Unit = typename Position<N>::Unit;            ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	Position<N> point;   ///< Arbitrary point on the plane's surface.
	Direction<N> normal; ///< Unit direction vector perpendicular to the plane's surface, pointing in the direction the plane is facing.

	/**
	 * Compare this plane to another for equality.
	 *
	 * \param other the plane to compare this plane to.
	 *
	 * \return true if the planes are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Plane& other) const = default;
};
using Plane2D = Plane<2>; ///< Description of an infinite plane in 2-dimensional space.
using Plane3D = Plane<3>; ///< Description of an infinite plane in 3-dimensional space.

/**
 * Description of a line segment in world space.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct LineSegment {
	using Unit = typename Position<N>::Unit;            ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless line segment as a physical line segment in default
	 * units.
	 *
	 * \param line line segment to reinterpret.
	 *
	 * \return the corresponding physical line segment.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr LineSegment reinterpret(const grem::LineSegment<N, float>& line) noexcept {
		return {.pointA = Position<N>::reinterpret(line.pointA), .pointB = Position<N>::reinterpret(line.pointB)};
	}

	Position<N> pointA; ///< Position of the first point of the line segment.
	Position<N> pointB; ///< Position of the second point of the line segment.

	/**
	 * Convert this physical line segment to a unitless line segment given a
	 * desired unit.
	 *
	 * \param unit unit to get the line segment in.
	 *
	 * \return the corresponding line segment in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::LineSegment<N, float> in(OtherUnitT unit) const noexcept requires(OtherUnitT::DIMENSION == LENGTH) {
		return {.pointA = pointA.in(unit), .pointB = pointB.in(unit)};
	}

	/**
	 * Compare this line segment to another for equality.
	 *
	 * \param other the line segment to compare this line segment to.
	 *
	 * \return true if the line segments are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const LineSegment& other) const = default;
};
using LineSegment2D = LineSegment<2>; ///< Description of a line segment in 2-dimensional space.
using LineSegment3D = LineSegment<3>; ///< Description of a line segment in 3-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr LineSegment<N> operator*(const grem::LineSegment<N, float>& line, typename LineSegment<N>::Unit) {
	return LineSegment<N>::reinterpret(line);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr LineSegment<N> operator*(typename LineSegment<N>::Unit, const grem::LineSegment<N, float>& line) {
	return LineSegment<N>::reinterpret(line);
}

/**
 * Description of an axis-aligned box in world space.
 */
template <size_t N>
struct Box {
	using Unit = typename Position<N>::Unit;            ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless box as a physical box in default units.
	 *
	 * \param box box to reinterpret.
	 *
	 * \return the corresponding physical box.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Box reinterpret(const grem::Box<N, float>& box) noexcept {
		return {.min = Position<N>::reinterpret(box.min), .max = Position<N>::reinterpret(box.max)};
	}

	Position<N> min; ///< Position with the minimum coordinates of the box extents on each coordinate axis.
	Position<N> max; ///< Position with the maximum coordinates of the box extents on each coordinate axis.

	/**
	 * Convert this physical box to a unitless box given a desired unit.
	 *
	 * \param unit unit to get the box in.
	 *
	 * \return the corresponding box in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::Box<N, float> in(OtherUnitT unit) const noexcept requires(OtherUnitT::DIMENSION == LENGTH) {
		return {.min = min.in(unit), .max = max.in(unit)};
	}

	/**
	 * Compare this box to another for equality.
	 *
	 * \param other the box to compare this box to.
	 *
	 * \return true if the boxes are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Box& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this box.
	 *
	 * \param point point to check.
	 *
	 * \return true if the box contains the given point, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool contains(const Position<N>& point) const noexcept {
		return all(greaterThanEqual(point, min) & lessThan(point, max));
	}

	/**
	 * Check if two axis-aligned boxes intersect.
	 *
	 * \param a first box.
	 * \param b second box.
	 *
	 * \return true if the first and second boxes are colliding with each other,
	 *         false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool intersects(const Box& a, const Box& b) noexcept {
		return all(lessThan(a.min, b.max) & greaterThan(a.max, b.min));
	}

	/**
	 * Check if this box intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool intersects(const Ray<N>& ray) const noexcept {
		return in(UNIT).intersects(ray.in(UNIT));
	}

	/**
	 * Find the closest intersection of this box with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr RaycastResult<N> raycast(const Ray<N>& ray) const noexcept {
		return RaycastResult<N>::reinterpret(in(UNIT).raycast(ray.in(UNIT)));
	}

	/**
	 * Get the axis-aligned bounding box of the box.
	 *
	 * \return an axis-aligned box that contains the entire box.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Box<N> getBoundingBox() const noexcept {
		return *this;
	}

	/**
	 * Get a version of the box that is symmetrically expanded by a certain
	 * amount.
	 *
	 * \param expansion distance to expand the box by along each axis. Each
	 *        component must be non-negative.
	 *
	 * \return the expanded box.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Box<N> getExpanded(Length<N> expansion) const noexcept {
		GREM_ASSERT(all(greaterThanEqual(expansion, Length<N>{})));
		return {.min = min - expansion, .max = max + expansion};
	}

	/**
	 * Get a version of the box that is symmetrically expanded by a certain
	 * amount.
	 *
	 * \param expansion distance to expand the box by along each axis. Must be
	 *        non-negative.
	 *
	 * \return the expanded box.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Box<N> getExpanded(Distance expansion) const noexcept {
		return getExpanded(Length<N>{expansion});
	}
};
using Box2D = Box<2>; ///< Description of an axis-aligned box in 2-dimensional space.
using Box3D = Box<3>; ///< Description of an axis-aligned box in 3-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Box<N> operator*(const grem::Box<N, float>& box, typename Box<N>::Unit) {
	return Box<N>::reinterpret(box);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Box<N> operator*(typename Box<N>::Unit, const grem::Box<N, float>& box) {
	return Box<N>::reinterpret(box);
}

/**
 * Description of a sphere in world space.
 */
template <size_t N>
struct Sphere {
	using Unit = typename Position<N>::Unit;            ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless sphere as a physical sphere in default units.
	 *
	 * \param sphere sphere to reinterpret.
	 *
	 * \return the corresponding physical sphere.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Sphere reinterpret(const grem::Sphere<N, float>& sphere) noexcept {
		return {.center = Position<N>::reinterpret(sphere.center), .radius = Distance::reinterpret(sphere.radius)};
	}

	Position<N> center; ///< Position of the center of the sphere.
	Distance radius;    ///< Radius of the sphere.

	/**
	 * Convert this physical sphere to a unitless sphere given a desired unit.
	 *
	 * \param unit unit to get the sphere in.
	 *
	 * \return the corresponding sphere in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::Sphere<N, float> in(OtherUnitT unit) const noexcept requires(OtherUnitT::DIMENSION == LENGTH) {
		return {.center = center.in(unit), .radius = radius.in(unit)};
	}

	/**
	 * Compare this sphere to another for equality.
	 *
	 * \param other the sphere to compare this sphere to.
	 *
	 * \return true if the spheres are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Sphere& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this sphere.
	 *
	 * \param point point to check.
	 *
	 * \return true if the sphere contains the given point, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool contains(const Position<N>& point) const noexcept {
		return distance2(center, point) < length2(radius);
	}

	/**
	 * Check if two spheres intersect.
	 *
	 * \param a first sphere.
	 * \param b second sphere.
	 *
	 * \return true if the first and second spheres are colliding with each
	 *         other, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE friend constexpr bool intersects(const Sphere& a, const Sphere& b) noexcept {
		return distance2(a.center, b.center) < length2(a.radius + b.radius);
	}

	/**
	 * Check if this sphere intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool intersects(const Ray<N>& ray) const noexcept {
		return in(UNIT).intersects(ray.in(UNIT));
	}

	/**
	 * Find the closest intersection of this sphere with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr RaycastResult<N> raycast(const Ray<N>& ray) const noexcept {
		return RaycastResult<N>::reinterpret(in(UNIT).raycast(ray.in(UNIT)));
	}

	/**
	 * Get the axis-aligned bounding box of the sphere.
	 *
	 * \return an axis-aligned box that contains the entire sphere.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Box<N> getBoundingBox() const noexcept {
		return {.min = center - Length<N>{radius}, .max = center + Length<N>{radius}};
	}
};
using Sphere2D = Sphere<2>; ///< Description of a sphere in 2-dimensional space.
using Sphere3D = Sphere<3>; ///< Description of a sphere in 3-dimensional space.
using Circle = Sphere2D;    ///< Description of a circle in 2-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Sphere<N> operator*(const grem::Sphere<N, float>& sphere, typename Sphere<N>::Unit) {
	return Sphere<N>::reinterpret(sphere);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Sphere<N> operator*(typename Sphere<N>::Unit, const grem::Sphere<N, float>& sphere) {
	return Sphere<N>::reinterpret(sphere);
}

/**
 * Description of a triangle in world space.
 */
template <size_t N>
struct Triangle {
	using Unit = typename Position<N>::Unit;            ///< Unit type.
	using DimensionType = typename Unit::DimensionType; ///< Unit dimension type.
	using MagnitudeType = typename Unit::MagnitudeType; ///< Unit magnitude type.
	static constexpr Unit UNIT{};                       ///< Unit.
	static constexpr DimensionType DIMENSION{};         ///< Unit dimension.
	static constexpr MagnitudeType MAGNITUDE{};         ///< Unit magnitude.

	/**
	 * Reinterpret a unitless triangle as a physical triangle in default units.
	 *
	 * \param triangle triangle to reinterpret.
	 *
	 * \return the corresponding physical triangle.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Triangle reinterpret(const grem::Triangle<N, float>& triangle) noexcept {
		return {
			.pointA = Position<N>::reinterpret(triangle.pointA),
			.pointB = Position<N>::reinterpret(triangle.pointB),
			.pointC = Position<N>::reinterpret(triangle.pointC),
		};
	}

	Position<N> pointA; ///< First point of the triangle.
	Position<N> pointB; ///< Second point of the triangle.
	Position<N> pointC; ///< Third point of the triangle.

	/**
	 * Convert this physical triangle to a unitless triangle given a desired
	 * unit.
	 *
	 * \param unit unit to get the triangle in.
	 *
	 * \return the corresponding triangle in the given unit.
	 */
	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr grem::Triangle<N, float> in(OtherUnitT unit) const noexcept requires(OtherUnitT::DIMENSION == LENGTH) {
		return {
			.pointA = pointA.in(unit),
			.pointB = pointB.in(unit),
			.pointC = pointC.in(unit),
		};
	}

	/**
	 * Compare this triangle to another for equality.
	 *
	 * \param other the triangle to compare this triangle to.
	 *
	 * \return true if the triangles are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Triangle& other) const = default;

	/**
	 * Check if a given point is contained within the extents of this triangle.
	 *
	 * \param point point to check.
	 *
	 * \return true if the triangle contains the given point, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool contains(const Position<N>& point) const noexcept requires(N == 2) {
		return in(UNIT).contains(point.in(UNIT));
	}

	/**
	 * Check if this triangle intersects a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return true if an intersection was found, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool intersects(const Ray<N>& ray) const noexcept {
		return in(UNIT).intersects(ray.in(UNIT));
	}

	/**
	 * Find the closest intersection of this triangle with a given ray.
	 *
	 * \param ray ray to check.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr RaycastResult<N> raycast(const Ray<N>& ray) const noexcept {
		return RaycastResult<N>::reinterpret(in(UNIT).raycast(ray.in(UNIT)));
	}

	/**
	 * Get the axis-aligned bounding box of the triangle.
	 *
	 * \return an axis-aligned box that contains the entire triangle.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Box<N> getBoundingBox() const noexcept {
		return {.min = min(min(pointA, pointB), pointC), .max = max(max(pointA, pointB), pointC)};
	}
};
using Triangle2D = Triangle<2>; ///< Description of a triangle in 2-dimensional space.
using Triangle3D = Triangle<3>; ///< Description of a triangle in 3-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Triangle<N> operator*(const grem::Triangle<N, float>& triangle, typename Triangle<N>::Unit) {
	return Triangle<N>::reinterpret(triangle);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Triangle<N> operator*(typename Triangle<N>::Unit, const grem::Triangle<N, float>& triangle) {
	return Triangle<N>::reinterpret(triangle);
}

namespace detail {

template <size_t N>
[[nodiscard]] inline Box<N> getTransformedBoundingBox(const Transformation<N>& transformation, Length<N> halfExtents) {
	const Basis<N> basis = transformation.getBasis();
	Box<N> result{.min = transformation.getOrigin(), .max = transformation.getOrigin()};
	meta::forEachIndex<N>([&](auto y) -> void {
		meta::forEachIndex<N>([&](auto x) -> void {
			const Length1D a = basis[y][x] * -halfExtents[y];
			const Length1D b = basis[y][x] * halfExtents[y];
			result.min[x] += min(a, b);
			result.max[x] += max(a, b);
		});
	});
	return result;
}

} // namespace detail

/**
 * Get the axis-aligned bounding box of an axis-aligned box with a
 * transformation applied to it.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 *
 * \param transformation transformation to apply to the box.
 * \param box local box to transform and get the axis-aligned bounding box of.
 *
 * \return an axis-aligned box that contains the given box after applying the
 *         transformation.
 */
template <size_t N>
[[nodiscard]] inline Box<N> getTransformedBoundingBox(const Transformation<N>& transformation, const Box<N>& box) {
	const Length<N> localCenterOffset = midpoint(box.min, box.max) - 0;
	const Length<N> halfExtents = (box.max - box.min) * 0.5f;
	return detail::getTransformedBoundingBox<N>(Transformation<N>{transformation(localCenterOffset), transformation.getBasis()}, halfExtents);
}

/**
 * Concept that checks if a type is a valid shape type.
 *
 * \tparam T the type to check.
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <typename T, size_t N>
concept shape = requires(const T t, const Mass mass, const Transformation<N> transformation, const Basis<N> basis, const Direction<N> direction, const Length<N> localRayOrigin,
	const Direction<N> localRayDirection, const Distance maxLocalRayDistance) {
	{ t.calculateVolume() } -> convertible_to<Volume>;
	{ t.calculatePrincipalMomentsOfInertia(mass) } -> convertible_to<PrincipalMomentsOfInertia<N>>;
	{ t.getBoundingBox(transformation) } -> convertible_to<Optional<Box<N>>>;
	{ t.getBoundingRadius(basis) } -> convertible_to<Optional<Distance>>;
	{ t.getReferenceArea(basis, direction) } -> convertible_to<Optional<Area>>;
	{ t.castLocalRay(localRayOrigin, localRayDirection, maxLocalRayDistance) } -> convertible_to<RaycastResult<N>>;
};

/**
 * Concept that checks if a type is a valid 2-dimensional shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept shape_2d = shape<T, 2>;

/**
 * Concept that checks if a type is a valid 3-dimensional shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept shape_3d = shape<T, 3>;

/**
 * Concept that checks if a type is a valid convex shape type.
 *
 * \tparam T the type to check.
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <typename T, size_t N>
concept convex_shape = shape<T, N> && requires(const T t, const Length<N> localPoint, const Direction<N> localDirection) {
	{ t.containsLocalPoint(localPoint) } -> convertible_to<bool>;
	{ t.getLocalSupportPointOffset(localDirection) } -> convertible_to<Length<N>>;
};

/**
 * Concept that checks if a type is a valid 2-dimensional convex shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept convex_shape_2d = convex_shape<T, 2>;

/**
 * Concept that checks if a type is a valid 3-dimensional convex shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept convex_shape_3d = convex_shape<T, 3>;

/**
 * Concept that checks if a type is a valid convex polytope shape type.
 *
 * \tparam T the type to check.
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <typename T, size_t N>
concept convex_polytope_shape =
	convex_shape<T, N> &&
	requires(const T t, const ConvexPolytopeVertexIndex vertexIndex, const ConvexPolytopeFaceIndex faceIndex, const Direction<N> localDirection, const Direction<N> direction) {
		{ t.getLocalSupportPointVertexIndex(localDirection, vertexIndex) } -> convertible_to<ConvexPolytopeVertexIndex>;
		{ t.getVertexCount() } -> convertible_to<ConvexPolytopeVertexIndex>;
		{ t.getFaceCount() } -> convertible_to<ConvexPolytopeFaceIndex>;
		{ t.getLocalVertexOffset(vertexIndex) } -> convertible_to<Length<N>>;
		{ t.getLocalFaceOffset(faceIndex) } -> convertible_to<Length<N>>;
		{ t.getLocalFaceNormal(faceIndex) } -> convertible_to<Direction<N>>;
		{ t.getFaceIndexWithMostFittingLocalNormal(localDirection, faceIndex) } -> convertible_to<ConvexPolytopeFaceIndex>;
	} && (N != 3 || requires(const T t, const ConvexPolytopeVertexIndex vertexIndex, const ConvexPolytopeFaceIndex faceIndex, const ConvexPolytopeEdgeIndex edgeIndex) {
		{ t.getEdgeCount() } -> convertible_to<ConvexPolytopeEdgeIndex>;
		{ t.getFirstVertexIndexOfEdge(edgeIndex) } -> convertible_to<ConvexPolytopeVertexIndex>;
		{ t.getFaceIndexOfEdge(edgeIndex) } -> convertible_to<ConvexPolytopeFaceIndex>;
		{ t.getNextEdgeIndex(edgeIndex) } -> convertible_to<ConvexPolytopeEdgeIndex>;
		{ t.getFirstEdgeIndexOfFace(faceIndex) } -> convertible_to<ConvexPolytopeEdgeIndex>;
		{ t.getSomeEdgeIndexOfVertex(vertexIndex) } -> convertible_to<ConvexPolytopeEdgeIndex>;
	});

/**
 * Concept that checks if a type is a valid 2-dimensional convex polytope shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept convex_polytope_shape_2d = convex_polytope_shape<T, 2>;

/**
 * Concept that checks if a type is a valid 3-dimensional convex polytope shape type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept convex_polytope_shape_3d = convex_polytope_shape<T, 3>;

/**
 * Execute a function for each vertex of a face of a convex polytope shape.
 *
 * \param shape shape to iterate the vertices of.
 * \param faceIndex index of the face of the shape to iterate. Must be a valid
 *        face index.
 * \param callback function to execute for each vertex index of the face.
 */
inline void forEachVertexIndexInFace(const convex_polytope_shape_2d auto& shape, ConvexPolytopeFaceIndex faceIndex, auto callback) {
	callback(static_cast<ConvexPolytopeVertexIndex>(faceIndex));
	callback(static_cast<ConvexPolytopeVertexIndex>((faceIndex + 1) % shape.getFaceCount()));
}

/**
 * Execute a function for each vertex of a face of a convex polytope shape.
 *
 * \param shape shape to iterate the vertices of.
 * \param faceIndex index of the face of the shape to iterate. Must be a valid
 *        face index.
 * \param callback function to execute for each vertex index of the face.
 */
inline void forEachVertexIndexInFace(const convex_polytope_shape_3d auto& shape, ConvexPolytopeFaceIndex faceIndex, auto callback) {
	const ConvexPolytopeEdgeIndex firstEdgeIndex = shape.getFirstEdgeIndexOfFace(faceIndex);
	ConvexPolytopeEdgeIndex edgeIndex = firstEdgeIndex;
	do {
		callback(shape.getFirstVertexIndexOfEdge(edgeIndex));
		edgeIndex = shape.getNextEdgeIndex(edgeIndex);
	} while (edgeIndex != firstEdgeIndex);
}

/**
 * Execute a function for each face that is adjacent to a vertex of a convex
 * polytope shape.
 *
 * \param shape shape to iterate the faces of.
 * \param vertexIndex index of the vertex of the shape to iterate faces around.
 *        Must be a valid vertex index.
 * \param callback function to execute for each face index around the vertex.
 */
inline void forEachFaceIndexAroundVertex(const convex_polytope_shape_2d auto& shape, ConvexPolytopeVertexIndex vertexIndex, auto callback) {
	callback(static_cast<ConvexPolytopeFaceIndex>(vertexIndex));
	callback(static_cast<ConvexPolytopeFaceIndex>((vertexIndex == 0) ? shape.getFaceCount() - 1 : vertexIndex - 1));
}

/**
 * Execute a function for each face that is adjacent to a vertex of a convex
 * polytope shape.
 *
 * \param shape shape to iterate the faces of.
 * \param vertexIndex index of the vertex of the shape to iterate faces around.
 *        Must be a valid vertex index.
 * \param callback function to execute for each face index around the vertex.
 */
inline void forEachFaceIndexAroundVertex(const convex_polytope_shape_3d auto& shape, ConvexPolytopeVertexIndex vertexIndex, auto callback) {
	const ConvexPolytopeEdgeIndex firstEdgeIndex = shape.getSomeEdgeIndexOfVertex(vertexIndex);
	ConvexPolytopeEdgeIndex edgeIndex = firstEdgeIndex;
	do {
		callback(shape.getFaceIndexOfEdge(edgeIndex));
		edgeIndex = shape.getNextEdgeIndex(static_cast<ConvexPolytopeEdgeIndex>(edgeIndex ^ 1));
	} while (edgeIndex != firstEdgeIndex);
}

/**
 * Execute a function for each vertex that is directly connected to a specific
 * vertex of a convex polytope shape.
 *
 * \param shape shape to iterate the vertices of.
 * \param vertexIndex index of the vertex of the shape to iterate vertices
 *        around. Must be a valid vertex index.
 * \param callback function to execute for each vertex index neighboring the
 *        vertex.
 */
inline void forEachNeighboringVertexIndex(const convex_polytope_shape_2d auto& shape, ConvexPolytopeVertexIndex vertexIndex, auto callback) {
	callback(static_cast<ConvexPolytopeFaceIndex>((vertexIndex == 0) ? shape.getFaceCount() - 1 : vertexIndex - 1));
	callback(static_cast<ConvexPolytopeFaceIndex>((vertexIndex + 1) % shape.getFaceCount()));
}

/**
 * Execute a function for each vertex that is directly connected to a specific
 * vertex of a convex polytope shape.
 *
 * \param shape shape to iterate the vertices of.
 * \param vertexIndex index of the vertex of the shape to iterate vertices
 *        around. Must be a valid vertex index.
 * \param callback function to execute for each vertex index neighboring the
 *        vertex.
 */
inline void forEachNeighboringVertexIndex(const convex_polytope_shape_3d auto& shape, ConvexPolytopeVertexIndex vertexIndex, auto callback) {
	const ConvexPolytopeEdgeIndex firstEdgeIndex = shape.getSomeEdgeIndexOfVertex(vertexIndex);
	ConvexPolytopeEdgeIndex edgeIndex = firstEdgeIndex;
	do {
		const ConvexPolytopeEdgeIndex twinEdgeIndex = static_cast<ConvexPolytopeEdgeIndex>(edgeIndex ^ 1);
		const ConvexPolytopeVertexIndex twinVertexIndex = shape.getFirstVertexIndexOfEdge(twinEdgeIndex);
		callback(twinVertexIndex);
		edgeIndex = shape.getNextEdgeIndex(twinEdgeIndex);
	} while (edgeIndex != firstEdgeIndex);
}

/**
 * Point shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct PointShape {
	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_ALWAYS_INLINE Volume calculateVolume() const {
		return {};
	}

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_ALWAYS_INLINE PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const {
		(void)mass;
		return {};
	}

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		return Box<N>{.min = transformation.getOrigin(), .max = transformation.getOrigin()};
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		(void)basis;
		return Distance{};
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
		(void)basis;
		(void)direction;
		return Area{};
	}

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_ALWAYS_INLINE RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
		(void)localRayOrigin;
		(void)localRayDirection;
		(void)maxLocalRayDistance;
		return RayMiss{};
	}

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool containsLocalPoint(Length<N> localPoint) const {
		(void)localPoint;
		return false;
	}

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const {
		(void)localDirection;
		return {};
	}
};
extern template struct PointShape<2>;
extern template struct PointShape<3>;
using PointShape2D = PointShape<2>; ///< Point shape in 2-dimensional space.
using PointShape3D = PointShape<3>; ///< Point shape in 3-dimensional space.
static_assert(convex_shape_2d<PointShape2D>);
static_assert(convex_shape_3d<PointShape3D>);

/**
 * Line segment shape parallel to the local Y axis.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct LineSegmentShape {
	Distance halfLength; ///< Half of the line segment's length along its local Y axis. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_ALWAYS_INLINE Volume calculateVolume() const {
		GREM_ASSERT(halfLength > Distance{});
		return {};
	}

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_ALWAYS_INLINE PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const {
		GREM_ASSERT(halfLength > Distance{});
		(void)mass;
		return {};
	}

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		GREM_ASSERT(halfLength > Distance{});
		const Length<N> halfVectorA = transformation.getRelative(Y_AXIS<N> * -halfLength);
		const Length<N> halfVectorB = transformation.getRelative(Y_AXIS<N> * halfLength);
		return Box<N>{.min = transformation.getOrigin() - min(halfVectorA, halfVectorB), .max = transformation.getOrigin() + max(halfVectorA, halfVectorB)};
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		return length(basis[Y] * halfLength);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
		(void)basis;
		(void)direction;
		return Area{};
	}

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_ALWAYS_INLINE RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
		(void)localRayOrigin;
		(void)localRayDirection;
		(void)maxLocalRayDistance;
		return RayMiss{};
	}

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool containsLocalPoint(Length<N> localPoint) const {
		(void)localPoint;
		return false;
	}

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const {
		GREM_ASSERT(halfLength > Distance{});
		const Length<N> halfLineVector = Y_AXIS<N> * halfLength;
		return (localDirection.getY() >= 0) ? halfLineVector : -halfLineVector;
	}
};
extern template struct LineSegmentShape<2>;
extern template struct LineSegmentShape<3>;
using LineSegmentShape2D = LineSegmentShape<2>; ///< Line segment shape parallel to the local Y axis in 2-dimensional space.
using LineSegmentShape3D = LineSegmentShape<3>; ///< Line segment shape parallel to the local Y axis in 3-dimensional space.
static_assert(convex_shape_2d<LineSegmentShape2D>);
static_assert(convex_shape_3d<LineSegmentShape3D>);

/**
 * Infinite line shape parallel to the local Y axis.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct InfiniteLineShape {
	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_ALWAYS_INLINE Volume calculateVolume() const {
		return {};
	}

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_ALWAYS_INLINE PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const {
		(void)mass;
		return {};
	}

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		(void)transformation;
		return {};
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		(void)basis;
		return {};
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
		(void)basis;
		(void)direction;
		return {};
	}

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_ALWAYS_INLINE RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
		(void)localRayOrigin;
		(void)localRayDirection;
		(void)maxLocalRayDistance;
		return RayMiss{};
	}

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool containsLocalPoint(Length<N> localPoint) const {
		(void)localPoint;
		return false;
	}

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const {
		return Y_AXIS<N> * ((localDirection.getY() >= 0) ? Distance::MAX : Distance::MIN);
	}
};
extern template struct InfiniteLineShape<2>;
extern template struct InfiniteLineShape<3>;
using InfiniteLineShape2D = InfiniteLineShape<2>; ///< Infinite line shape parallel to the local Y axis in 2-dimensional space.
using InfiniteLineShape3D = InfiniteLineShape<3>; ///< Infinite line shape parallel to the local Y axis in 3-dimensional space.
static_assert(convex_shape_2d<InfiniteLineShape2D>);
static_assert(convex_shape_3d<InfiniteLineShape3D>);

/**
 * Infinite half-space shape perpendicular to the local Y axis.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct InfiniteHalfSpaceShape {
	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_ALWAYS_INLINE Volume calculateVolume() const {
		return {};
	}

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_ALWAYS_INLINE PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const {
		(void)mass;
		return {};
	}

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		(void)transformation;
		return {};
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		(void)basis;
		return {};
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
		(void)basis;
		(void)direction;
		return {};
	}

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;
};
extern template struct InfiniteHalfSpaceShape<2>;
extern template struct InfiniteHalfSpaceShape<3>;
using InfiniteHalfSpaceShape2D = InfiniteHalfSpaceShape<2>; ///< Infinite half-space shape perpendicular to the local Y axis in 2-dimensional space.
using InfiniteHalfSpaceShape3D = InfiniteHalfSpaceShape<3>; ///< Infinite half-space shape perpendicular to the local Y axis in 3-dimensional space.
static_assert(shape_2d<InfiniteHalfSpaceShape2D>);
static_assert(shape_3d<InfiniteHalfSpaceShape3D>);

/**
 * Infinite plane shape perpendicular to the local Y axis in 3-dimensional
 * space.
 */
struct InfinitePlaneShape3D {
	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_ALWAYS_INLINE Volume calculateVolume() const {
		return {};
	}

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_ALWAYS_INLINE PrincipalMomentsOfInertia3D calculatePrincipalMomentsOfInertia(Mass mass) const {
		(void)mass;
		return {};
	}

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box3D> getBoundingBox(const Transformation3D& transformation) const {
		(void)transformation;
		return {};
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis3D& basis) const {
		(void)basis;
		return {};
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Area> getReferenceArea(const Basis3D& basis, Direction3D direction) const {
		(void)basis;
		(void)direction;
		return {};
	}

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult3D castLocalRay(Length3D localRayOrigin, Direction3D localRayDirection, Distance maxLocalRayDistance) const;
};
static_assert(shape_3d<InfinitePlaneShape3D>);

/**
 * Box shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct BoxShape {
	Length<N> halfExtents; ///< Shortest lengths from the center of the box to its side along each local axis. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		GREM_ASSERT(all(greaterThan(halfExtents, Length<N>{})));
		return detail::getTransformedBoundingBox<N>(transformation, halfExtents);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		return length(basis * halfExtents);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_ALWAYS_INLINE RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
		GREM_ASSERT(all(greaterThan(halfExtents, Length<N>{})));
		return Box<N>{.min = -halfExtents, .max = halfExtents}.raycast(Ray<N>{.origin = localRayOrigin, .direction = localRayDirection, .maxDistance = maxLocalRayDistance});
	}

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool containsLocalPoint(Length<N> localPoint) const {
		return Box<N>{.min = -halfExtents, .max = halfExtents}.contains(localPoint);
	}

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const {
		GREM_ASSERT(all(greaterThan(halfExtents, Length<N>{})));
		return copysign(halfExtents, localDirection);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalSupportPointVertexIndex() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeVertexIndex getLocalSupportPointVertexIndex(Direction<N> localDirection, ConvexPolytopeVertexIndex searchStartVertexIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getVertexCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeVertexIndex getVertexCount() const {
		if constexpr (N == 2) {
			return 4;
		} else {
			return 8;
		}
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeFaceIndex getFaceCount() const {
		if constexpr (N == 2) {
			return 4;
		} else {
			return 6;
		}
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getEdgeCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getEdgeCount() const requires(N == 3) {
		return 24;
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalVertexOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalVertexOffset(ConvexPolytopeVertexIndex vertexIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalFaceOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalFaceOffset(ConvexPolytopeFaceIndex faceIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalFaceNormal() */
	[[nodiscard]] GREM_API(physics) Direction<N> getLocalFaceNormal(ConvexPolytopeFaceIndex faceIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceIndexWithMostFittingLocalNormal() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeFaceIndex getFaceIndexWithMostFittingLocalNormal(Direction<N> localDirection, ConvexPolytopeFaceIndex searchStartFaceIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getFirstVertexIndexOfEdge() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeVertexIndex getFirstVertexIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceIndexOfEdge() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeFaceIndex getFaceIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/** \copydoc ConvexPolytopeShapeView<3>::getNextEdgeIndex() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getNextEdgeIndex(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/** \copydoc ConvexPolytopeShapeView<3>::getFirstEdgeIndexOfFace() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getFirstEdgeIndexOfFace(ConvexPolytopeFaceIndex faceIndex) const requires(N == 3);

	/** \copydoc ConvexPolytopeShapeView<3>::getSomeEdgeIndexOfVertex() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getSomeEdgeIndexOfVertex(ConvexPolytopeVertexIndex vertexIndex) const requires(N == 3);
};
extern template struct BoxShape<2>;
extern template struct BoxShape<3>;
using BoxShape2D = BoxShape<2>; ///< Box shape in 2-dimensional space.
using BoxShape3D = BoxShape<3>; ///< Box shape in 3-dimensional space.
static_assert(convex_polytope_shape_2d<BoxShape2D>);
static_assert(convex_polytope_shape_3d<BoxShape3D>);
using RectangleShape2D = BoxShape2D; ///< Rectangle shape in 2-dimensional space.

/**
 * Cube shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct CubeShape {
	Distance halfExtent; ///< Shortest length from the center of the cube to one of its sides. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_ALWAYS_INLINE Volume calculateVolume() const {
		return BoxShape<N>{.halfExtents{halfExtent}}.calculateVolume();
	}

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_ALWAYS_INLINE PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.calculatePrincipalMomentsOfInertia(mass);
	}

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getBoundingBox(transformation);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getBoundingRadius(basis);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getReferenceArea(basis, direction);
	}

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_ALWAYS_INLINE RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.castLocalRay(localRayOrigin, localRayDirection, maxLocalRayDistance);
	}

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_ALWAYS_INLINE bool containsLocalPoint(Length<N> localPoint) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.containsLocalPoint(localPoint);
	}

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getLocalSupportPointOffset(localDirection);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalSupportPointVertexIndex() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeVertexIndex getLocalSupportPointVertexIndex(Direction<N> localDirection,
		ConvexPolytopeVertexIndex searchStartVertexIndex) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getLocalSupportPointVertexIndex(localDirection, searchStartVertexIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getVertexCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeVertexIndex getVertexCount() const {
		if constexpr (N == 2) {
			return 4;
		} else {
			return 8;
		}
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeFaceIndex getFaceCount() const {
		if constexpr (N == 2) {
			return 4;
		} else {
			return 6;
		}
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getEdgeCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getEdgeCount() const requires(N == 3) {
		return 24;
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalVertexOffset() */
	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalVertexOffset(ConvexPolytopeVertexIndex vertexIndex) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getLocalVertexOffset(vertexIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalFaceOffset() */
	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalFaceOffset(ConvexPolytopeFaceIndex faceIndex) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getLocalFaceOffset(faceIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalFaceNormal() */
	[[nodiscard]] GREM_ALWAYS_INLINE Direction<N> getLocalFaceNormal(ConvexPolytopeFaceIndex faceIndex) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getLocalFaceNormal(faceIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceIndexWithMostFittingLocalNormal() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeFaceIndex getFaceIndexWithMostFittingLocalNormal(Direction<N> localDirection,
		ConvexPolytopeFaceIndex searchStartFaceIndex) const {
		return BoxShape<N>{.halfExtents{halfExtent}}.getFaceIndexWithMostFittingLocalNormal(localDirection, searchStartFaceIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getFirstVertexIndexOfEdge() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeVertexIndex getFirstVertexIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
		return BoxShape3D{.halfExtents{halfExtent}}.getFirstVertexIndexOfEdge(edgeIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceIndexOfEdge() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeFaceIndex getFaceIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
		return BoxShape3D{.halfExtents{halfExtent}}.getFaceIndexOfEdge(edgeIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getNextEdgeIndex() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getNextEdgeIndex(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
		return BoxShape3D{.halfExtents{halfExtent}}.getNextEdgeIndex(edgeIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getFirstEdgeIndexOfFace() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getFirstEdgeIndexOfFace(ConvexPolytopeFaceIndex faceIndex) const requires(N == 3) {
		return BoxShape3D{.halfExtents{halfExtent}}.getFirstEdgeIndexOfFace(faceIndex);
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getSomeEdgeIndexOfVertex() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getSomeEdgeIndexOfVertex(ConvexPolytopeVertexIndex vertexIndex) const requires(N == 3) {
		return BoxShape3D{.halfExtents{halfExtent}}.getSomeEdgeIndexOfVertex(vertexIndex);
	}
};
extern template struct CubeShape<2>;
extern template struct CubeShape<3>;
using CubeShape2D = CubeShape<2>; ///< Cube shape in 2-dimensional space.
using CubeShape3D = CubeShape<3>; ///< Cube shape in 3-dimensional space.
static_assert(convex_polytope_shape_2d<CubeShape2D>);
static_assert(convex_polytope_shape_3d<CubeShape3D>);
using SquareShape2D = CubeShape2D; ///< Square shape in 2-dimensional space.

/**
 * Ellipsoid shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct EllipsoidShape {
	Length<N> radii; ///< Radius of the ellipsoid along each local axis. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		return BoxShape<N>{.halfExtents = radii}.getBoundingBox(transformation);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		return maxComponent(basis.getScale() * radii);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_API(physics) bool containsLocalPoint(Length<N> localPoint) const;

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const;
};
extern template struct EllipsoidShape<2>;
extern template struct EllipsoidShape<3>;
using EllipsoidShape2D = EllipsoidShape<2>; ///< Ellipsoid shape in 2-dimensional space.
using EllipsoidShape3D = EllipsoidShape<3>; ///< Ellipsoid shape in 3-dimensional space.
static_assert(convex_shape_2d<EllipsoidShape2D>);
static_assert(convex_shape_3d<EllipsoidShape3D>);
using EllipseShape2D = EllipsoidShape2D; ///< Ellipse shape in 2-dimensional space.

/**
 * Sphere shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct SphereShape {
	Distance radius; ///< Radius of the sphere. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_ALWAYS_INLINE Volume calculateVolume() const {
		return EllipsoidShape<N>{.radii{radius}}.calculateVolume();
	}

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_ALWAYS_INLINE PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const {
		return EllipsoidShape<N>{.radii{radius}}.calculatePrincipalMomentsOfInertia(mass);
	}

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		GREM_ASSERT(radius > Distance{});
		const Position<N> center = transformation.getOrigin();
		const Distance maxTransformedRadius = radius * sqrt(maxComponent(transformation.getBasis().getScale2()));
		return Box<N>{.min = center - Length<N>{maxTransformedRadius}, .max = center + Length<N>{maxTransformedRadius}};
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		return EllipsoidShape<N>{.radii{radius}}.getBoundingRadius(basis);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
		return EllipsoidShape<N>{.radii{radius}}.getReferenceArea(basis, direction);
	}

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_API(physics) bool containsLocalPoint(Length<N> localPoint) const;

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const;
};
extern template struct SphereShape<2>;
extern template struct SphereShape<3>;
using SphereShape2D = SphereShape<2>; ///< Sphere shape in 2-dimensional space.
using SphereShape3D = SphereShape<3>; ///< Sphere shape in 3-dimensional space.
static_assert(convex_shape_2d<SphereShape2D>);
static_assert(convex_shape_3d<SphereShape3D>);
using CircleShape2D = SphereShape2D; ///< Circle shape in 2-dimensional space.

/**
 * Capsule shape aligned with the local Y axis.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct CapsuleShape {
	Distance radius;     ///< Radius of the capsule. Must be positive.
	Distance halfLength; ///< Half of the length of the capsule's center line along its local Y axis. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		GREM_ASSERT(radius > Distance{});
		GREM_ASSERT(halfLength > Distance{});
		const Position<N> pointA = transformation(-Y_AXIS<N> * halfLength);
		const Position<N> pointB = transformation(Y_AXIS<N> * halfLength);
		const Distance maxTransformedRadius = radius * sqrt(maxComponent(transformation.getBasis().getScale2()));
		const Box<N> boxA{.min = pointA - Length<N>{maxTransformedRadius}, .max = pointA + Length<N>{maxTransformedRadius}};
		const Box<N> boxB{.min = pointB - Length<N>{maxTransformedRadius}, .max = pointB + Length<N>{maxTransformedRadius}};
		return Box<N>{.min = min(boxA.min, boxB.min), .max = max(boxA.max, boxB.max)};
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		GREM_ASSERT(radius > Distance{});
		GREM_ASSERT(halfLength > Distance{});
		return BoxShape<N>{.halfExtents = Y_AXIS<N> * halfLength + Length<N>{radius}}.getBoundingRadius(basis);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_API(physics) bool containsLocalPoint(Length<N> localPoint) const;

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const;
};
extern template struct CapsuleShape<2>;
extern template struct CapsuleShape<3>;
using CapsuleShape2D = CapsuleShape<2>; ///< Capsule shape aligned with the local Y axis in 2-dimensional space.
using CapsuleShape3D = CapsuleShape<3>; ///< Capsule shape aligned with the local Y axis in 3-dimensional space.
static_assert(convex_shape_2d<CapsuleShape2D>);
static_assert(convex_shape_3d<CapsuleShape3D>);
using StadiumShape2D = CapsuleShape2D; ///< Stadium shape in 2-dimensional space.

/**
 * Tapered capsule shape aligned with the local Y axis.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct TaperedCapsuleShape {
	Distance bottomRadius; ///< Radius of the bottom (-Y) sphere of the capsule. Must be non-negative. Must be positive if #topRadius is 0.
	Distance topRadius;    ///< Radius of the top (+Y) sphere of the capsule. Must be non-negative. Must be positive if #bottomRadius is 0.
	Distance halfLength;   ///< Half of the length of the capsule's center line along its local Y axis. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		GREM_ASSERT(bottomRadius >= Distance{});
		GREM_ASSERT(topRadius >= Distance{});
		GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
		GREM_ASSERT(halfLength > Distance{});
		return BoxShape<N>{.halfExtents = Y_AXIS<N> * halfLength + Length<N>{max(bottomRadius, topRadius)}}.getBoundingBox(transformation);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		GREM_ASSERT(bottomRadius >= Distance{});
		GREM_ASSERT(topRadius >= Distance{});
		GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
		GREM_ASSERT(halfLength > Distance{});
		return BoxShape<N>{.halfExtents = Y_AXIS<N> * halfLength + Length<N>{max(bottomRadius, topRadius)}}.getBoundingRadius(basis);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_API(physics) bool containsLocalPoint(Length<N> localPoint) const;

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const;
};
extern template struct TaperedCapsuleShape<2>;
extern template struct TaperedCapsuleShape<3>;
using TaperedCapsuleShape2D = TaperedCapsuleShape<2>; ///< Tapered capsule shape aligned with the local Y axis in 2-dimensional space.
using TaperedCapsuleShape3D = TaperedCapsuleShape<3>; ///< Tapered capsule shape aligned with the local Y axis in 3-dimensional space.
static_assert(convex_shape_2d<TaperedCapsuleShape2D>);
static_assert(convex_shape_3d<TaperedCapsuleShape3D>);

/**
 * Cylinder shape aligned with the local Y axis in 3-dimensional
 * world space.
 */
struct CylinderShape3D {
	Distance radius;     ///< Radius of the cylinder. Must be positive.
	Distance halfLength; ///< Half of the length of the cylinder's center line along its local Y axis. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia3D calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box3D> getBoundingBox(const Transformation3D& transformation) const {
		GREM_ASSERT(radius > Distance{});
		GREM_ASSERT(halfLength > Distance{});
		return BoxShape3D{.halfExtents{radius, halfLength, radius}}.getBoundingBox(transformation);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis3D& basis) const {
		GREM_ASSERT(radius > Distance{});
		GREM_ASSERT(halfLength > Distance{});
		return BoxShape3D{.halfExtents{radius, halfLength, radius}}.getBoundingRadius(basis);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis3D& basis, Direction3D direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult3D castLocalRay(Length3D localRayOrigin, Direction3D localRayDirection, Distance maxLocalRayDistance) const;

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_API(physics) bool containsLocalPoint(Length3D localPoint) const;

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_API(physics) Length3D getLocalSupportPointOffset(Direction3D localDirection) const;
};
static_assert(convex_shape_3d<CylinderShape3D>);

/**
 * Conical frustum shape aligned with the local Y axis in 3-dimensional space.
 */
struct TaperedCylinderShape3D {
	Distance bottomRadius; ///< Radius of the bottom (-Y) disc of the conical frustum. Must be non-negative. Must be positive if #topRadius is 0.
	Distance topRadius;    ///< Radius of the top (+Y) disc of the conical frustum. Must be non-negative. Must be positive if #bottomRadius is 0.
	Distance halfLength;   ///< Half of the length of the conical frustum's center line along its local Y axis. Must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia3D calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box3D> getBoundingBox(const Transformation3D& transformation) const {
		GREM_ASSERT(bottomRadius >= Distance{});
		GREM_ASSERT(topRadius >= Distance{});
		GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
		GREM_ASSERT(halfLength > Distance{});
		const Distance maxRadius = max(bottomRadius, topRadius);
		return BoxShape3D{.halfExtents{maxRadius, halfLength, maxRadius}}.getBoundingBox(transformation);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis3D& basis) const {
		GREM_ASSERT(bottomRadius >= Distance{});
		GREM_ASSERT(topRadius >= Distance{});
		GREM_ASSERT(bottomRadius > Distance{} || topRadius > Distance{});
		GREM_ASSERT(halfLength > Distance{});
		const Distance maxRadius = max(bottomRadius, topRadius);
		return BoxShape3D{.halfExtents{maxRadius, halfLength, maxRadius}}.getBoundingRadius(basis);
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis3D& basis, Direction3D direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult3D castLocalRay(Length3D localRayOrigin, Direction3D localRayDirection, Distance maxLocalRayDistance) const;

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_API(physics) bool containsLocalPoint(Length3D localPoint) const;

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_API(physics) Length3D getLocalSupportPointOffset(Direction3D localDirection) const;
};
static_assert(convex_shape_3d<TaperedCylinderShape3D>);

/**
 * Convex polytope shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class ConvexPolytopeShape {
public:
	using Vertex = typename ConvexPolytope<N>::Vertex; ///< Vertex type of the convex polytope shape.
	using Face = typename ConvexPolytope<N>::Face;     ///< Face type of the convex polytope shape.
	using Edge = typename ConvexPolytope<N>::Edge;     ///< Edge type of the convex polytope shape.

	/**
	 * Construct a convex polytope shape from an existing convex polytope.
	 *
	 * \param polytope shared pointer to the polytope. Must not be nullptr.
	 */
	GREM_ALWAYS_INLINE explicit ConvexPolytopeShape(SharedPointer<ConvexPolytope<N>> polytope) noexcept
		: polytope(std::move(polytope)) {
		GREM_ASSERT(this->polytope);
	}

	/**
	 * Construct a convex polytope shape from a set of vertices.
	 *
	 * \param vertices read-only view over a set of vertices to construct the
	 *        convex hull from.
	 * \param maxVertexCount maximum number of vertices to build the convex hull
	 *        out of.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(physics) explicit ConvexPolytopeShape(Span<const Vertex> vertices, ConvexPolytopeVertexIndex maxVertexCount = Limits<ConvexPolytopeVertexIndex>::MAX);

	/**
	 * Get the underlying convex polytope of the shape.
	 *
	 * \return a read-only reference to the underlying convex polytope.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const SharedPointer<ConvexPolytope<N>>& getConvexPolytope() const noexcept {
		return polytope;
	}

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		return getTransformedBoundingBox<N>(transformation, polytope->getBoundingBox() * Box<N>::UNIT);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		return sqrt(maxComponent(basis.getScale2())) * polytope->getBoundingRadius() * Distance::UNIT;
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;

	/** \copydoc ConvexShapeView<3>::containsLocalPoint() */
	[[nodiscard]] GREM_API(physics) bool containsLocalPoint(Length<N> localPoint) const;

	/** \copydoc ConvexShapeView<3>::getLocalSupportPointOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalSupportPointVertexIndex() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeVertexIndex getLocalSupportPointVertexIndex(Direction<N> localDirection, ConvexPolytopeVertexIndex searchStartVertexIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getVertexCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeVertexIndex getVertexCount() const {
		return static_cast<ConvexPolytopeVertexIndex>(polytope->getVertices().size());
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeFaceIndex getFaceCount() const {
		return static_cast<ConvexPolytopeFaceIndex>(polytope->getFaces().size());
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getEdgeCount() */
	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getEdgeCount() const requires(N == 3) {
		if constexpr (N == 3) {
			return static_cast<ConvexPolytopeEdgeIndex>(polytope->getEdges().size());
		} else {
			unreachable();
		}
	}

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalVertexOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalVertexOffset(ConvexPolytopeVertexIndex vertexIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalFaceOffset() */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalFaceOffset(ConvexPolytopeFaceIndex faceIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getLocalFaceNormal() */
	[[nodiscard]] GREM_API(physics) Direction<N> getLocalFaceNormal(ConvexPolytopeFaceIndex faceIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceIndexWithMostFittingLocalNormal() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeFaceIndex getFaceIndexWithMostFittingLocalNormal(Direction<N> localDirection, ConvexPolytopeFaceIndex searchStartFaceIndex) const;

	/** \copydoc ConvexPolytopeShapeView<3>::getFirstVertexIndexOfEdge() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeVertexIndex getFirstVertexIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/** \copydoc ConvexPolytopeShapeView<3>::getFaceIndexOfEdge() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeFaceIndex getFaceIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/** \copydoc ConvexPolytopeShapeView<3>::getNextEdgeIndex() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getNextEdgeIndex(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/** \copydoc ConvexPolytopeShapeView<3>::getFirstEdgeIndexOfFace() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getFirstEdgeIndexOfFace(ConvexPolytopeFaceIndex faceIndex) const requires(N == 3);

	/** \copydoc ConvexPolytopeShapeView<3>::getSomeEdgeIndexOfVertex() */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getSomeEdgeIndexOfVertex(ConvexPolytopeVertexIndex vertexIndex) const requires(N == 3);

private:
	SharedPointer<ConvexPolytope<N>> polytope;
};
extern template class ConvexPolytopeShape<2>;
extern template class ConvexPolytopeShape<3>;
using ConvexPolytopeShape2D = ConvexPolytopeShape<2>; ///< Convex polytope shape in 2-dimensional space.
using ConvexPolytopeShape3D = ConvexPolytopeShape<3>; ///< Convex polytope shape in 3-dimensional space.
static_assert(convex_polytope_shape_2d<ConvexPolytopeShape2D>);
static_assert(convex_polytope_shape_3d<ConvexPolytopeShape3D>);

/**
 * %Triangle mesh shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class TriangleMeshShape {
public:
	using Vertex = typename TriangleMesh<N>::Vertex;           ///< Vertex type of the triangle mesh shape.
	using VertexIndex = typename TriangleMesh<N>::VertexIndex; ///< Vertex index type of the triangle mesh shape.
	using FaceIndex = typename TriangleMesh<N>::FaceIndex;     ///< Face index type of the triangle mesh shape.

	/**
	 * Construct a triangle mesh shape from an existing triangle mesh.
	 *
	 * \param mesh shared pointer to the mesh. Must not be nullptr.
	 */
	GREM_ALWAYS_INLINE explicit TriangleMeshShape(SharedPointer<TriangleMesh<N>> mesh) noexcept
		: mesh(std::move(mesh)) {
		GREM_ASSERT(this->mesh);
	}

	/**
	 * Construct a triangle mesh shape from a set of vertices.
	 *
	 * \param vertices set of vertices to construct the triangle mesh from.
	 * \param indices set of vertex indices to construct the triangle mesh from.
	 *        Each index must be less than the number of vertices. Size must be
	 *        a multiple of 3.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(physics) TriangleMeshShape(Allocation<Vertex> vertices, Allocation<VertexIndex> indices);

	/**
	 * Construct a triangle mesh shape from a set of vertices and indices.
	 *
	 * \param vertices read-only view over a set of vertices to construct the
	 *        triangle mesh from.
	 * \param indices read-only view over a set of vertex indices to construct
	 *        the triangle mesh from. Each index must be less than the number of
	 *        vertices. Size must be a multiple of 3.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(physics) TriangleMeshShape(Span<const Vertex> vertices, Span<const VertexIndex> indices);

	/**
	 * Get the underlying triangle mesh of the shape.
	 *
	 * \return a read-only reference to the underlying triangle mesh.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const SharedPointer<TriangleMesh<N>>& getTriangleMesh() const noexcept {
		return mesh;
	}

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		return getTransformedBoundingBox<N>(transformation, mesh->getBoundingBox() * Box<N>::UNIT);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		return sqrt(maxComponent(basis.getScale2())) * mesh->getBoundingRadius() * Distance::UNIT;
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;

private:
	SharedPointer<TriangleMesh<N>> mesh;
};
extern template class TriangleMeshShape<2>;
extern template class TriangleMeshShape<3>;
using TriangleMeshShape2D = TriangleMeshShape<2>; ///< %Triangle mesh shape in 2-dimensional space.
using TriangleMeshShape3D = TriangleMeshShape<3>; ///< %Triangle mesh shape in 3-dimensional space.
static_assert(shape_2d<TriangleMeshShape2D>);
static_assert(shape_3d<TriangleMeshShape3D>);

/**
 * Generic shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct Shape;

/**
 * Wrapper of another shape with a local transformation applied to it.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct LocallyTransformedShape {
	SharedPointer<Shape<N>> shape;     ///< Shape being transformed. Must not be nullptr.
	Length<N> localOffset{};           ///< Local offset of the transformed shape relative to the outer shape.
	Orientation<N> localOrientation{}; ///< Local orientation of the transformed shape relative to the outer shape.
	Scale<N> localScale{1.0f};         ///< Local scale of the transformed shape relative to the outer shape. Each component must be positive.

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_API(physics) Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const;

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_API(physics) Optional<Distance> getBoundingRadius(const Basis<N>& basis) const;

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;
};
extern template struct LocallyTransformedShape<2>;
extern template struct LocallyTransformedShape<3>;
using LocallyTransformedShape2D = LocallyTransformedShape<2>; ///< Wrapper of another shape with a local transformation applied to it in 2-dimensional space.
using LocallyTransformedShape3D = LocallyTransformedShape<3>; ///< Wrapper of another shape with a local transformation applied to it in 3-dimensional space.
static_assert(shape_2d<LocallyTransformedShape2D>);
static_assert(shape_3d<LocallyTransformedShape3D>);

/**
 * Sub-collider of a compound collider shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct SubCollider;
using SubCollider2D = SubCollider<2>; ///< Sub-collider of a compound collider shape in 2-dimensional space.
using SubCollider3D = SubCollider<3>; ///< Sub-collider of a compound collider shape in 3-dimensional space.

/**
 * Shape that combines multiple colliders.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class CompoundColliderShape {
public:
	/**
	 * Construct a compound collider shape from a set of colliders.
	 *
	 * \param subColliders shared pointer to the colliders. Must not be nullptr.
	 */
	GREM_ALWAYS_INLINE explicit CompoundColliderShape(SharedPointer<SubCollider<N>[]> subColliders) noexcept
		: subColliders(std::move(subColliders))
		, boundingBox(calculateCompoundBoundingBox(getSubColliders()))
		, boundingRadius(calculateCompoundBoundingRadius(getSubColliders())) {
		GREM_ASSERT(this->subColliders);
	}

	/**
	 * Construct a compound shape from a set of colliders.
	 *
	 * \param subColliders colliders to construct the compound shape from.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(physics) explicit CompoundColliderShape(Span<const SubCollider<N>> subColliders);

	/**
	 * Get the underlying set of colliders of this compound collider shape.
	 *
	 * \return a read-only view over the underlying colliders.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Span<const SubCollider<N>> getSubColliders() const noexcept {
		return Span<const SubCollider<N>>{subColliders.get(), subColliders.size()};
	}

	/** \copydoc ShapeView<3>::calculateVolume() */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/** \copydoc ShapeView<3>::calculatePrincipalMomentsOfInertia() */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/** \copydoc ShapeView<3>::getBoundingBox() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		if (!boundingBox) {
			return {};
		}
		return getTransformedBoundingBox<N>(transformation, *boundingBox);
	}

	/** \copydoc ShapeView<3>::getBoundingRadius() */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		if (!boundingRadius) {
			return {};
		}
		return sqrt(maxComponent(basis.getScale2())) * *boundingRadius;
	}

	/** \copydoc ShapeView<3>::getReferenceArea() */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/** \copydoc ShapeView<3>::castLocalRay() */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;

private:
	[[nodiscard]] GREM_API(physics) static Optional<Box<N>> calculateCompoundBoundingBox(Span<const SubCollider<N>> subColliders) noexcept;
	[[nodiscard]] GREM_API(physics) static Optional<Distance> calculateCompoundBoundingRadius(Span<const SubCollider<N>> subColliders) noexcept;

	SharedPointer<SubCollider<N>[]> subColliders;
	Optional<Box<N>> boundingBox;
	Optional<Distance> boundingRadius;
};
extern template class CompoundColliderShape<2>;
extern template class CompoundColliderShape<3>;
using CompoundColliderShape2D = CompoundColliderShape<2>; ///< Shape that combines multiple colliders in 2-dimensional space.
using CompoundColliderShape3D = CompoundColliderShape<3>; ///< Shape that combines multiple colliders in 3-dimensional space.
static_assert(shape_2d<CompoundColliderShape2D>);
static_assert(shape_3d<CompoundColliderShape3D>);

/**
 * Shape in 2-dimensional space.
 */
template <>
struct Shape<2>
	: Variant<PointShape2D, LineSegmentShape2D, InfiniteLineShape2D, InfiniteHalfSpaceShape2D, RectangleShape2D, SquareShape2D, EllipseShape2D, CircleShape2D, CapsuleShape2D,
		  TaperedCapsuleShape2D, ConvexPolytopeShape2D, TriangleMeshShape2D, LocallyTransformedShape2D, CompoundColliderShape2D> {
	using value_type = Variant;

	using Variant::Variant;

	/**
	 * Check if the shape held by this wrapper is a convex shape type.
	 *
	 * \return true if the underlying shape satisfies the convex_shape concept,
	 *         false otherwise.
	 */
	[[nodiscard]] bool isConvexShapeType() const {
		return match(*this)([&](const convex_shape_2d auto&) -> bool { return true; }, [&](const auto&) -> bool { return false; });
	}

	/**
	 * Check if the shape held by this wrapper is a convex polytope shape type.
	 *
	 * \return true if the underlying shape satisfies the convex_polytope_shape
	 *         concept, false otherwise.
	 */
	[[nodiscard]] bool isConvexPolytopeShapeType() const {
		return match(*this)([&](const convex_polytope_shape_2d auto&) -> bool { return true; }, [&](const auto&) -> bool { return false; });
	}
};

/**
 * Shape in 3-dimensional space.
 */
template <>
struct Shape<3>
	: Variant<PointShape3D, LineSegmentShape3D, InfiniteLineShape3D, InfiniteHalfSpaceShape3D, InfinitePlaneShape3D, BoxShape3D, CubeShape3D, EllipsoidShape3D, SphereShape3D,
		  CapsuleShape3D, TaperedCapsuleShape3D, CylinderShape3D, TaperedCylinderShape3D, ConvexPolytopeShape3D, TriangleMeshShape3D, LocallyTransformedShape3D,
		  CompoundColliderShape3D> {
	using value_type = Variant;

	using Variant::Variant;

	/**
	 * Check if the shape held by this wrapper is a convex shape type.
	 *
	 * \return true if the underlying shape satisfies the convex_shape concept,
	 *         false otherwise.
	 */
	[[nodiscard]] bool isConvexShapeType() const {
		return match(*this)([&](const convex_shape_3d auto&) -> bool { return true; }, [&](const auto&) -> bool { return false; });
	}

	/**
	 * Check if the shape held by this wrapper is a convex polytope shape type.
	 *
	 * \return true if the underlying shape satisfies the convex_polytope_shape
	 *         concept, false otherwise.
	 */
	[[nodiscard]] bool isConvexPolytopeShapeType() const {
		return match(*this)([&](const convex_polytope_shape_3d auto&) -> bool { return true; }, [&](const auto&) -> bool { return false; });
	}
};
using Shape2D = Shape<2>; ///< Generic shape in 2-dimensional space.
using Shape3D = Shape<3>; ///< Generic shape in 3-dimensional space.

/**
 * Generic collider.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct Collider {
	Shape<N> shape;           ///< Collider shape.
	CollisionFilter filter{}; ///< Collision filter.
};
using Collider2D = Collider<2>; ///< Generic collider in 2-dimensional space.
using Collider3D = Collider<3>; ///< Generic collider in 3-dimensional space.

template <size_t N>
struct SubCollider {
	Collider<N> collider;              ///< Collider being transformed.
	Length<N> localOffset{};           ///< Local offset of the transformed shape relative to the outer shape.
	Orientation<N> localOrientation{}; ///< Local orientation of the transformed shape relative to the outer shape.
	Scale<N> localScale{1.0f};         ///< Local scale of the transformed shape relative to the outer shape. Each component must be positive.
};

/**
 * View of a generic shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class ShapeView {
public:
	/**
	 * Construct a shape view.
	 *
	 * \param shape reference to the shape to construct the view over. Must
	 *        outlive its use in the view.
	 */
	GREM_ALWAYS_INLINE constexpr ShapeView(const Shape<N>& shape) noexcept
		: shape(shape) {}

	/**
	 * Get a reference to the underlying shape.
	 *
	 * \return a read-only reference to the underlying shape.
	 */
	GREM_ALWAYS_INLINE constexpr operator const Shape<N>&() const noexcept {
		return shape;
	}

	/**
	 * Check if the shape held by this wrapper is a convex shape type.
	 *
	 * \return true if the underlying shape satisfies the convex_shape concept,
	 *         false otherwise.
	 */
	[[nodiscard]] bool isConvexShapeType() const noexcept {
		return shape.isConvexShapeType();
	}

	/**
	 * Check if the shape held by this wrapper is a convex polytope shape type.
	 *
	 * \return true if the underlying shape satisfies the convex_polytope_shape
	 *         concept, false otherwise.
	 */
	[[nodiscard]] bool isConvexPolytopeShapeType() const noexcept {
		return shape.isConvexPolytopeShapeType();
	}

	/**
	 * Calculate an estimate of the volume of the shape.
	 *
	 * \return the estimated volume of the shape.
	 */
	[[nodiscard]] GREM_API(physics) Volume calculateVolume() const;

	/**
	 * Calculate an estimate of the local moment of inertia of the shape given
	 * a certain mass.
	 *
	 * \param mass assumed mass of the shape.
	 *
	 * \return the estimated moment of inertia in shape-local space.
	 */
	[[nodiscard]] GREM_API(physics) PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const;

	/**
	 * Get the axis-aligned bounding box of the shape given its position and
	 * orientation in the world.
	 *
	 * \param transformation transformation of the shape in world space.
	 *
	 * \return an axis-aligned bounding box in world space that would contain
	 *         the entire shape if the shape was at the given position and
	 *         orientation in the world, or an empty optional if the shape
	 *         extends infinitely and cannot be fit inside a finite box.
	 */
	[[nodiscard]] GREM_API(physics) Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const;

	/**
	 * Get the radius of the bounding sphere of the shape.
	 *
	 * \param basis transformation basis of the shape in world space.
	 *
	 * \return the maximum distance from the origin of any point on the shape,
	 *         or an empty optional if the shape extends infinitely and cannot
	 *         be fit inside a finite sphere.
	 */
	[[nodiscard]] GREM_API(physics) Optional<Distance> getBoundingRadius(const Basis<N>& basis) const;

	/**
	 * Get an estimate of the reference area of the shape for a certain
	 * direction, typically calculated as the area of the largest cross section
	 * orthogonal to the given direction.
	 *
	 * \param basis transformation basis of the shape in world space.
	 * \param direction direction to get the reference area for.
	 *
	 * \return the reference area, or an empty optional if an area cannot be
	 *         determined, e.g. because the shape is infinite.
	 */
	[[nodiscard]] GREM_API(physics) Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const;

	/**
	 * Find the closest intersection of this shape with a given ray in local
	 * space.
	 *
	 * \param localRayOrigin offset of the ray origin in shape-local space.
	 * \param localRayDirection unit direction vector of the ray in shape-local
	 *        space. Must be a unit vector.
	 * \param maxLocalRayDistance maximum hit distance of the ray in shape-local
	 *        space. Must be non-negative.
	 *
	 * \return the result of the raycast.
	 */
	[[nodiscard]] GREM_API(physics) RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const;

protected:
	const Shape<N>& shape;
};
extern template class ShapeView<2>;
extern template class ShapeView<3>;
using ShapeView2D = ShapeView<2>; ///< View of a generic shape in 2-dimensional space.
using ShapeView3D = ShapeView<3>; ///< View of a generic shape in 3-dimensional space.
static_assert(shape_2d<ShapeView2D>);
static_assert(shape_3d<ShapeView3D>);

/**
 * View of a generic convex shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class ConvexShapeView : public ShapeView<N> {
public:
	/**
	 * Construct a shape view.
	 *
	 * \param shape reference to the shape to construct the view over. Must
	 *        outlive its use in the view.
	 */
	GREM_ALWAYS_INLINE constexpr explicit ConvexShapeView(const Shape<N>& shape)
		: ShapeView<N>(shape) {}

	/**
	 * Construct a shape view.
	 *
	 * \param shape reference to the shape to construct the view over. Must
	 *        outlive its use in the view.
	 */
	GREM_ALWAYS_INLINE constexpr explicit ConvexShapeView(ShapeView<N> shape)
		: ShapeView<N>(shape) {}

	/**
	 * Check if a local point relative to the origin of the shape is contained
	 * within the shape.
	 *
	 * \param localPoint point in shape-local space to check.
	 *
	 * \return true if the shape contains the given point, false otherwise.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not convex.
	 */
	[[nodiscard]] GREM_API(physics) bool containsLocalPoint(Length<N> localPoint) const;

	/**
	 * Get the local offset from the origin of the shape to the farthest point
	 * on the shape in a certain local direction.
	 *
	 * \param localDirection unit vector in shape-local space of the direction
	 *        to evaluate. Must be a unit vector.
	 *
	 * \return a relative vector from the origin of the shape to the farthest
	 *         point on the shape in the given direction, in shape-local space.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not convex.
	 */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const;
};
extern template class ConvexShapeView<2>;
extern template class ConvexShapeView<3>;
using ConvexShapeView2D = ConvexShapeView<2>; ///< View of a generic convex shape in 2-dimensional space.
using ConvexShapeView3D = ConvexShapeView<3>; ///< View of a generic convex shape in 3-dimensional space.
static_assert(convex_shape_2d<ConvexShapeView2D>);
static_assert(convex_shape_3d<ConvexShapeView3D>);

/**
 * View of a generic convex polytope shape.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class ConvexPolytopeShapeView : public ConvexShapeView<N> {
public:
	/**
	 * Construct a shape view.
	 *
	 * \param shape reference to the shape to construct the view over. Must
	 *        outlive its use in the view.
	 */
	GREM_ALWAYS_INLINE constexpr explicit ConvexPolytopeShapeView(const Shape<N>& shape)
		: ConvexShapeView<N>(shape) {}

	/**
	 * Construct a shape view.
	 *
	 * \param shape reference to the shape to construct the view over. Must
	 *        outlive its use in the view.
	 */
	GREM_ALWAYS_INLINE constexpr explicit ConvexPolytopeShapeView(ShapeView<N> shape)
		: ConvexShapeView<N>(shape) {}

	/**
	 * Get the vertex index of the farthest point on the shape in a certain
	 * local direction.
	 *
	 * \param localDirection unit vector in shape-local space of the direction
	 *        to evaluate. Must be a unit vector.
	 * \param searchStartVertexIndex initial vertex index to start the search
	 *        from. Must either be 0, or less than getVertexCount().
	 *
	 * \return the vertex index of the farthest point on the shape in the given
	 *         direction.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeVertexIndex getLocalSupportPointVertexIndex(Direction<N> localDirection, ConvexPolytopeVertexIndex searchStartVertexIndex) const;

	/**
	 * Get the total number of vertices in the convex polytope defined by this
	 * shape.
	 *
	 * \return the number of vertices in the shape.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeVertexIndex getVertexCount() const;

	/**
	 * Get the total number of faces on the convex polytope defined by this
	 * shape.
	 *
	 * \return the number of faces on the shape.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeFaceIndex getFaceCount() const;

	/**
	 * Get the total number of edges, including twin edges, on the convex
	 * polyhedron defined by this 3-dimensional shape.
	 *
	 * \return the number of edges, including twin edges, on the shape.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getEdgeCount() const requires(N == 3);

	/**
	 * Get the local offset of a specific vertex in the convex polytope defined
	 * by this shape.
	 *
	 * \param vertexIndex index of the vertex to get the offset of.
	 *
	 * \return the offset of the given vertex from the center of the shape, in
	 *         shape-local space.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalVertexOffset(ConvexPolytopeVertexIndex vertexIndex) const;

	/**
	 * Get the local offset of a specific face on the convex polytope defined by
	 * this shape.
	 *
	 * \param faceIndex index of the face to get the offset of.
	 *
	 * \return an arbitrary point the given face, in shape-local space.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) Length<N> getLocalFaceOffset(ConvexPolytopeFaceIndex faceIndex) const;

	/**
	 * Get the local normal of a specific face on the convex polytope defined by
	 * this shape.
	 *
	 * \param faceIndex index of the face to get the normal of.
	 *
	 * \return a unit direction vector perpendicular to the given face, in
	 *         shape-local space, pointing outwards from the center of the
	 *         shape.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) Direction<N> getLocalFaceNormal(ConvexPolytopeFaceIndex faceIndex) const;

	/**
	 * Get the index of the face on the convex polytope defined by this shape
	 * whose local normal is the most parallel with a given local direction.
	 *
	 * \param localDirection unit direction vector to search in, in object
	 *        space. Must be a unit vector.
	 * \param searchStartFaceIndex initial face index to start the search
	 *        from. Must either be 0, or less than getFaceCount().
	 *
	 * \return the index of the face whose normal is pointing the most towards
	 *         the given direction.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeFaceIndex getFaceIndexWithMostFittingLocalNormal(Direction<N> localDirection, ConvexPolytopeFaceIndex searchStartFaceIndex) const;

	/**
	 * Get the vertex index of the first point of a specific edge on the convex
	 * polyhedron defined by this 3-dimensional shape.
	 *
	 * \param edgeIndex index of the edge whose vertex index to get.
	 *
	 * \return the vertex index of the first point of the given edge.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeVertexIndex getFirstVertexIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/**
	 * Get the index of the face that a specific edge is part of on the convex
	 * polyhedron defined by this 3-dimensional shape.
	 *
	 * \param edgeIndex index of the edge to get the face index of.
	 *
	 * \return the index of the face that the given edge belongs to.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeFaceIndex getFaceIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/**
	 * Get the index of the next edge after another edge on the same face on the
	 * convex polyhedron defined by this 3-dimensional shape, which will
	 * eventually loop back around to the given edge again.
	 *
	 * The winding order is counter-clockwise when viewed from outside of the
	 * shape.
	 *
	 * \param edgeIndex index of the edge to get the next edge index of.
	 *
	 * \return the index of the next edge on the same face.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getNextEdgeIndex(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3);

	/**
	 * Get the index of the first edge that a specific face is made up of on the
	 * convex polyhedron defined by this 3-dimensional shape.
	 *
	 * \param faceIndex index of the face to get the first edge index of.
	 *
	 * \return the index of the first edge that belongs to the given face.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getFirstEdgeIndexOfFace(ConvexPolytopeFaceIndex faceIndex) const requires(N == 3);

	/**
	 * Get the index of some edge that has a specific vertex as its origin on
	 * the convex polyhedron defined by this 3-dimensional shape.
	 *
	 * \param vertexIndex index of the vertex to get the edge index of.
	 *
	 * \return some edge index that has the given vertex as its origin.
	 *
	 * \throws BadVariantAccess if the underlying shape type is not a convex
	 *         polytope.
	 */
	[[nodiscard]] GREM_API(physics) ConvexPolytopeEdgeIndex getSomeEdgeIndexOfVertex(ConvexPolytopeVertexIndex vertexIndex) const requires(N == 3);
};
extern template class ConvexPolytopeShapeView<2>;
extern template class ConvexPolytopeShapeView<3>;
using ConvexPolytopeShapeView2D = ConvexPolytopeShapeView<2>; ///< View of a generic convex polytope shape in 2-dimensional space.
using ConvexPolytopeShapeView3D = ConvexPolytopeShapeView<3>; ///< View of a generic convex polytope shape in 3-dimensional space.
static_assert(convex_polytope_shape_2d<ConvexPolytopeShapeView2D>);
static_assert(convex_polytope_shape_3d<ConvexPolytopeShapeView3D>);

/**
 * View of a generic collider.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ColliderView {
	ShapeView<N> shape;     ///< Collider shape.
	CollisionFilter filter; ///< Collision filter.

	/**
	 * Construct a collider view.
	 *
	 * \param collider reference to the collider to construct the view over.
	 *        Must outlive its use in the view.
	 */
	GREM_ALWAYS_INLINE constexpr ColliderView(const Collider<N>& collider) noexcept
		: shape(collider.shape)
		, filter(collider.filter) {}

	/**
	 * Construct a collider view from a shape and collision filter.
	 *
	 * \param shape reference to the shape to construct the view over. Must
	 *        outlive its use in the view.
	 * \param filter collision filter.
	 */
	GREM_ALWAYS_INLINE constexpr ColliderView(ShapeView<N> shape, CollisionFilter filter) noexcept
		: shape(shape)
		, filter(filter) {}
};
extern template struct ColliderView<2>;
extern template struct ColliderView<3>;
using ColliderView2D = ColliderView<2>; ///< View of a generic collider in 2-dimensional space.
using ColliderView3D = ColliderView<3>; ///< View of a generic collider in 3-dimensional space.

namespace detail {

template <size_t N>
struct TriangleShape {
	Length<N> pointA;
	Length<N> pointB;
	Length<N> pointC;

	[[nodiscard]] GREM_ALWAYS_INLINE Volume calculateVolume() const {
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(Mass mass) const {
		(void)mass;
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Box<N>> getBoundingBox(const Transformation<N>& transformation) const {
		const Position<N> transformedPointA = transformation(pointA);
		const Position<N> transformedPointB = transformation(pointB);
		const Position<N> transformedPointC = transformation(pointC);
		return {.min = min(min(transformedPointA, transformedPointB), transformedPointC), .max = max(max(transformedPointA, transformedPointB), transformedPointC)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Distance> getBoundingRadius(const Basis<N>& basis) const {
		const Length<N> offsetA = basis * pointA;
		const Length<N> offsetB = basis * pointB;
		const Length<N> offsetC = basis * pointC;
		return sqrt(max(max(length2(offsetA), length2(offsetB)), length2(offsetC)));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Area> getReferenceArea(const Basis<N>& basis, Direction<N> direction) const {
		const Length<N> offsetA = basis * pointA;
		const Length<N> offsetB = basis * pointB;
		const Length<N> offsetC = basis * pointC;
		return 0.5f * abs(dot(cross(offsetB - offsetA, offsetC - offsetA), direction));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE RaycastResult<N> castLocalRay(Length<N> localRayOrigin, Direction<N> localRayDirection, Distance maxLocalRayDistance) const {
		return Triangle<N>{.pointA = pointA, .pointB = pointB, .pointC = pointC}.raycast(
			Ray<N>{.origin = localRayOrigin, .direction = localRayDirection, .maxDistance = maxLocalRayDistance});
	}

	[[nodiscard]] GREM_ALWAYS_INLINE bool containsLocalPoint(Length<N> localPoint) const {
		(void)localPoint;
		return false;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalSupportPointOffset(Direction<N> localDirection) const {
		return getLocalVertexOffset(getLocalSupportPointVertexIndex(localDirection, 0));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeVertexIndex getLocalSupportPointVertexIndex(Direction<N> localDirection,
		ConvexPolytopeVertexIndex searchStartVertexIndex) const {
		(void)searchStartVertexIndex;
		ConvexPolytopeVertexIndex farthestVertexIndex = 0;
		Length1D farthestSignedDistance = dot(pointA, localDirection);
		const Length1D signedDistanceB = dot(pointB, localDirection);
		if (signedDistanceB > farthestSignedDistance) {
			farthestVertexIndex = 1;
			farthestSignedDistance = signedDistanceB;
		}
		const Length1D signedDistanceC = dot(pointC, localDirection);
		if (signedDistanceC > farthestSignedDistance) {
			farthestVertexIndex = 2;
			farthestSignedDistance = signedDistanceC;
		}
		return farthestVertexIndex;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeVertexIndex getVertexCount() const {
		return 3;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeFaceIndex getFaceCount() const {
		if constexpr (N == 2) {
			return 3;
		} else {
			return 2;
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getEdgeCount() const requires(N == 3) {
		return 6;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalVertexOffset(ConvexPolytopeVertexIndex vertexIndex) const {
		GREM_ASSERT(vertexIndex < getVertexCount());
		switch (vertexIndex) {
			case 0: return pointA;
			case 1: return pointB;
			case 2: return pointC;
			default: break;
		}
		unreachable();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> getLocalFaceOffset(ConvexPolytopeFaceIndex faceIndex) const {
		GREM_ASSERT(faceIndex < getFaceCount());
		if constexpr (N == 2) {
			return getLocalVertexOffset(static_cast<ConvexPolytopeVertexIndex>(faceIndex));
		} else {
			return (faceIndex == 0) ? pointA : pointB;
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Direction<N> getLocalFaceNormal(ConvexPolytopeFaceIndex faceIndex) const {
		if constexpr (N == 2) {
			const Length2D a = getLocalVertexOffset(static_cast<ConvexPolytopeVertexIndex>(faceIndex));
			const Length2D b = getLocalVertexOffset(static_cast<ConvexPolytopeVertexIndex>(faceIndex + 1) % getVertexCount());
			const Length2D difference = b - a;
			return normalize(rotate90DegreesClockwise(difference));
		} else {
			const Direction3D normal = normalize(cross(pointB - pointA, pointC - pointA));
			return flipSignIf(normal, faceIndex != 0);
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeFaceIndex getFaceIndexWithMostFittingLocalNormal(Direction<N> localDirection,
		ConvexPolytopeFaceIndex searchStartFaceIndex) const {
		(void)searchStartFaceIndex;
		if constexpr (N == 2) {
			ConvexPolytopeFaceIndex mostFittingFaceIndex = 0;
			Scale1D largestDotProduct = dot(localDirection, getLocalFaceNormal(0));
			const Scale1D dotProductB = dot(localDirection, getLocalFaceNormal(1));
			if (dotProductB > largestDotProduct) {
				mostFittingFaceIndex = 1;
				largestDotProduct = dotProductB;
			}
			const Scale1D dotProductC = dot(localDirection, getLocalFaceNormal(2));
			if (dotProductC > largestDotProduct) {
				mostFittingFaceIndex = 2;
				largestDotProduct = dotProductC;
			}
			return mostFittingFaceIndex;
		} else {
			const auto vector = cross(pointB - pointA, pointC - pointA);
			return static_cast<ConvexPolytopeFaceIndex>(signbit(dot(vector, localDirection)));
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeVertexIndex getFirstVertexIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
		GREM_ASSERT(edgeIndex < getEdgeCount());
		switch (edgeIndex) {
			case 0: return 0;
			case 1: return 1;
			case 2: return 1;
			case 3: return 2;
			case 4: return 2;
			case 5: return 0;
			default: break;
		}
		unreachable();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeFaceIndex getFaceIndexOfEdge(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
		GREM_ASSERT(edgeIndex < getEdgeCount());
		switch (edgeIndex) {
			case 0: return 0;
			case 1: return 1;
			case 2: return 0;
			case 3: return 1;
			case 4: return 0;
			case 5: return 1;
			default: break;
		}
		unreachable();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getNextEdgeIndex(ConvexPolytopeEdgeIndex edgeIndex) const requires(N == 3) {
		GREM_ASSERT(edgeIndex < getEdgeCount());
		switch (edgeIndex) {
			case 0: return 2;
			case 1: return 5;
			case 2: return 4;
			case 3: return 1;
			case 4: return 0;
			case 5: return 3;
			default: break;
		}
		unreachable();
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getFirstEdgeIndexOfFace(ConvexPolytopeFaceIndex faceIndex) const requires(N == 3) {
		GREM_ASSERT(faceIndex < getFaceCount());
		return static_cast<ConvexPolytopeEdgeIndex>(faceIndex);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE ConvexPolytopeEdgeIndex getSomeEdgeIndexOfVertex(ConvexPolytopeVertexIndex vertexIndex) const requires(N == 3) {
		GREM_ASSERT(vertexIndex < getVertexCount());
		return static_cast<ConvexPolytopeEdgeIndex>(vertexIndex << 1);
	}
};
using TriangleShape2D = TriangleShape<2>;
using TriangleShape3D = TriangleShape<3>;
static_assert(convex_polytope_shape_2d<TriangleShape2D>);
static_assert(convex_polytope_shape_3d<TriangleShape3D>);

} // namespace detail

} // namespace grem::physics

#endif
