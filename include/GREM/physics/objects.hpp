// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_OBJECTS_HPP
#define GREM_PHYSICS_OBJECTS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Registry.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/system/synchronization.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/joints.hpp>
#include <GREM/physics/quantities.hpp>

namespace grem::physics {

/**
 * Effective fluid density of an object's surroundings.
 */
struct FluidDensity : Density {
	/**
	 * Compare this value to another for equality.
	 *
	 * \param other the value to compare this one to.
	 *
	 * \return true if the values are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const FluidDensity& other) const noexcept = default;
};

/**
 * World-space offset of the center of buoyancy of an object.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct CenterOfBuoyancy : Length<N> {
	/**
	 * Compare this value to another for equality.
	 *
	 * \param other the value to compare this one to.
	 *
	 * \return true if the values are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const CenterOfBuoyancy& other) const noexcept = default;
};
using CenterOfBuoyancy2D = CenterOfBuoyancy<2>; ///< World-space offset of the center of buoyancy of an object in a 2-dimensional world.
using CenterOfBuoyancy3D = CenterOfBuoyancy<3>; ///< World-space offset of the center of buoyancy of an object in a 3-dimensional world.

/**
 * Properties regarding the dynamics of an object.
 */
struct Material {
	/**
	 * Friction combination mode.
	 *
	 * When two objects collide, the friction combination mode with the lowest
	 * value of the two is used.
	 */
	enum class FrictionCombine : uint8_t {
		AVERAGE = 0,  ///< The two values are averaged.
		MINIMUM = 1,  ///< The smallest of the two values is used.
		MAXIMUM = 2,  ///< The largest of the two values is used.
		MULTIPLY = 3, ///< The two values are multiplied with each other.
	};

	/**
	 * Restitution combination mode.
	 *
	 * When two objects collide, the restitution combination mode with the
	 * lowest value of the two is used.
	 */
	enum class RestitutionCombine : uint8_t {
		AVERAGE = 0,  ///< The two values are averaged.
		MINIMUM = 1,  ///< The smallest of the two values is used.
		MAXIMUM = 2,  ///< The largest of the two values is used.
		MULTIPLY = 3, ///< The two values are multiplied with each other.
	};

	/**
	 * Coefficient of static friction of the object.
	 *
	 * Must be non-negative.
	 */
	Coefficient staticFriction = Scalar{0.8};

	/**
	 * Coefficient of kinetic friction of the object.
	 *
	 * Must be non-negative.
	 */
	Coefficient kineticFriction = Scalar{0.707};

	/**
	 * Coefficient of rolling resistance of the object.
	 *
	 * Must be non-negative.
	 */
	Coefficient rollingResistance = Scalar{0.01};

	/**
	 * Coefficient of restitution of the object.
	 *
	 * Must be between 0 and 1 (inclusive).
	 */
	Coefficient restitution = Scalar{0.5};

	/**
	 * Linear drag coefficient of the object.
	 *
	 * Must be non-negative.
	 */
	Coefficient linearDrag = Scalar{0.5};

	/**
	 * Angular drag coefficient of the object.
	 *
	 * Must be non-negative.
	 */
	Coefficient angularDrag = Scalar{0.5};

	/**
	 * How to combine the friction when this object collides with another.
	 */
	FrictionCombine frictionCombine = FrictionCombine::MINIMUM;

	/**
	 * How to combine the restitution when this object collides with another.
	 */
	RestitutionCombine restitutionCombine = RestitutionCombine::MAXIMUM;

	/**
	 * Compare this material to another for equality.
	 *
	 * \param other the material to compare this one to.
	 *
	 * \return true if the materials are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Material& other) const noexcept = default;
};

/**
 * Set of boolean properties of an object.
 */
struct ObjectFlags {
	bool emitsCollisionEvents : 1;  ///< Emit collision events.
	bool emitsSeparationEvents : 1; ///< Emit separation events.
	bool enableResting : 1;         ///< Decay the object's velocity towards 0 on when below the resting threshold.

	/**
	 * Compare these flags to another set of flags for equality.
	 *
	 * \param other the flags to compare these to.
	 *
	 * \return true if the flags are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const ObjectFlags& other) const noexcept = default;
};

/**
 * Potential contacts of an object.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct alignas(64) ObjectContacts {
	SmallArrayList<execution::EntityID, 6> otherObjectIDs{}; ///< List of objects that this object is potentially colliding with.
};
using ObjectContacts2D = ObjectContacts<2>; ///< Potential contacts of a 2-dimensional object.
using ObjectContacts3D = ObjectContacts<3>; ///< Potential contacts of a 3-dimensional object.

/**
 * Activity information of an object.
 */
struct ObjectActivity {
	/**
	 * May be affected by the constraint solver.
	 */
	bool isCorrectable : 1 = false;

	/**
	 * Had a correction impulse applied since the start of the last simulation
	 * step where it was active.
	 */
	bool wasCorrected : 1 = false;

	/**
	 * Currently considered active (moving) in the simulation.
	 */
	bool isActive : 1 = false;

	/**
	 * Number of consecutive simulation steps that have passed where the object
	 * was below resting speed.
	 */
	uint16_t stepsResting = 0;

	/**
	 * Construct the default object activity.
	 */
	constexpr ObjectActivity() noexcept = default;

	/**
	 * Compare this activity information to another for equality.
	 *
	 * \param other the activity information to compare this one to.
	 *
	 * \return true if the activity information are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const ObjectActivity& other) const noexcept = default;
};

/**
 * Tag component that is added to entities with a non-zero-energy ObjectActivity.
 */
struct ObjectActiveTag {};

/**
 * Bounds of an object.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ObjectBounds {
	Box<N> boundingBox{}; ///< Axis-aligned bounding box of the object.

	/**
	 * Compare these bounds to another for equality.
	 *
	 * \param other the bounds to compare these to.
	 *
	 * \return true if the bounds are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const ObjectBounds& other) const noexcept = default;
};
using ObjectBounds2D = ObjectBounds<2>; ///< Bounds of a 2-dimensional object.
using ObjectBounds3D = ObjectBounds<3>; ///< Bounds of a 3-dimensional object.

/**
 * Configuration options for an object.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ObjectOptions {
	/**
	 * Initial position of the object's center of mass in the world.
	 */
	Position<N> position{};

	/**
	 * Initial orientation of the object in the world.
	 */
	Orientation<N> orientation{};

	/**
	 * Initial scale of the object in shape-local space.
	 */
	Scale<N> scale{Scalar{1}};

	/**
	 * Initial linear velocity of the object's center of mass.
	 */
	LinearVelocity<N> linearVelocity{};

	/**
	 * Initial angular velocity of the object.
	 */
	AngularVelocity<N> angularVelocity{};

	/**
	 * Gravity acceleration that will be applied to the object on every
	 * simulation step.
	 */
	LinearAcceleration<N> gravityAcceleration = -Y_AXIS<N> * Scalar{9.82} * METERS_PER_SECOND_SQUARED;

	/**
	 * Effective fluid density of the object's surroundings, which will be used
	 * to calculate the drag to apply to the object at every simulation step.
	 *
	 * \sa Material::linearDrag
	 * \sa Material::angularDrag
	 */
	Density surroundingFluidDensity = Scalar{1.2041} * KILOGRAMS_PER_CUBIC_METER;

	/**
	 * World-space offset of the center of buoyancy of the object relative to
	 * its center of mass.
	 */
	Length<N> centerOfBuoyancy{};

	/**
	 * Mass of the object.
	 *
	 * Must be positive or 0. If set to 0, an estimate of the mass will be
	 * calculated automatically based on the shape's volume and an assumed
	 * density of 0.5 g/cm^3. If set to infinity, the object's linear velocity
	 * will be constant (except for gravity, if #gravityAcceleration is
	 * non-zero).
	 *
	 * \note If both #mass and #principalMomentsOfInertia are infinite, the
	 *       object will be considered immovable for the rest of its lifetime
	 *       unless its ObjectActivity::isCorrectable field is manually set to
	 *       true.
	 */
	Mass mass{};

	/**
	 * Principal moments of inertia of the object.
	 *
	 * Each component must be positive or 0. If set to 0, an estimate of the
	 * moment of inertia will be calculated automatically based on the object's
	 * shape and mass. The estimate will assume that #localInertiaOrientation is
	 * the identity orientation. If set to infinity, the object's angular
	 * velocity will be constant.
	 *
	 * \note If both #mass and #principalMomentsOfInertia are infinite, the
	 *       object will be considered immovable for the rest of its lifetime
	 *       unless its ObjectActivity::isCorrectable field is manually set to
	 *       true.
	 */
	PrincipalMomentsOfInertia<N> principalMomentsOfInertia{};

	/**
	 * Rotation from inertia major axis space to local space.
	 */
	LocalInertiaOrientation<N> localInertiaOrientation{};

	/**
	 * Collider of the object.
	 */
	Collider<N> collider{.shape = PointShape<N>{}};

	/**
	 * Dynamics-related properties of the object.
	 */
	Material material{};

	/**
	 * Whether the object should emit collision events or not.
	 */
	bool emitsCollisionEvents = false;

	/**
	 * Whether the object should emit separation events or not.
	 */
	bool emitsSeparationEvents = false;

	/**
	 * Whether the object should enter a resting state on inactivity or not.
	 *
	 * Set to false to make sure the object is always simulated regardless of
	 * its speed.
	 */
	bool enableResting = true;

	/**
	 * Compare this set of options to another set for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const ObjectOptions& other) const = default;
};
using ObjectOptions2D = ObjectOptions<2>; ///< Configuration options for an object in a 2-dimensional world.
using ObjectOptions3D = ObjectOptions<3>; ///< Configuration options for an object in a 3-dimensional world.

/**
 * Combine the static friction of two objects based on their friction
 * combination modes.
 *
 * \param staticFrictionA static friction of the first object.
 * \param staticFrictionB static friction of the second object.
 * \param frictionCombineA friction combination mode of the first object.
 * \param frictionCombineB friction combination mode of the second object.
 *
 * \return the combined static friction.
 */
[[nodiscard]] inline Coefficient calculateCombinedStaticFriction(Coefficient staticFrictionA, Coefficient staticFrictionB, Material::FrictionCombine frictionCombineA,
	Material::FrictionCombine frictionCombineB) {
	const Material::FrictionCombine frictionCombine = static_cast<Material::FrictionCombine>(min(static_cast<uint8_t>(frictionCombineA), static_cast<uint8_t>(frictionCombineB)));
	switch (frictionCombine) {
		case Material::FrictionCombine::AVERAGE: return midpoint(staticFrictionA, staticFrictionB);
		case Material::FrictionCombine::MINIMUM: return min(staticFrictionA, staticFrictionB);
		case Material::FrictionCombine::MAXIMUM: return max(staticFrictionA, staticFrictionB);
		case Material::FrictionCombine::MULTIPLY: return staticFrictionA * staticFrictionB;
	}
	unreachable();
}

/**
 * Combine the kinetic friction of two objects based on their friction
 * combination modes.
 *
 * \param kineticFrictionA kinetic friction of the first object.
 * \param kineticFrictionB kinetic friction of the second object.
 * \param frictionCombineA friction combination mode of the first object.
 * \param frictionCombineB friction combination mode of the second object.
 *
 * \return the combined kinetic friction.
 */
[[nodiscard]] inline Coefficient calculateCombinedKineticFriction(Coefficient kineticFrictionA, Coefficient kineticFrictionB, Material::FrictionCombine frictionCombineA,
	Material::FrictionCombine frictionCombineB) {
	const Material::FrictionCombine frictionCombine = static_cast<Material::FrictionCombine>(min(static_cast<uint8_t>(frictionCombineA), static_cast<uint8_t>(frictionCombineB)));
	switch (frictionCombine) {
		case Material::FrictionCombine::AVERAGE: return midpoint(kineticFrictionA, kineticFrictionB);
		case Material::FrictionCombine::MINIMUM: return min(kineticFrictionA, kineticFrictionB);
		case Material::FrictionCombine::MAXIMUM: return max(kineticFrictionA, kineticFrictionB);
		case Material::FrictionCombine::MULTIPLY: return kineticFrictionA * kineticFrictionB;
	}
	unreachable();
}

/**
 * Combine the rolling resistance of two objects based on their friction
 * combination modes.
 *
 * \param rollingResistanceA rolling resistance of the first object.
 * \param rollingResistanceB rolling resistance of the second object.
 * \param frictionCombineA friction combination mode of the first object.
 * \param frictionCombineB friction combination mode of the second object.
 *
 * \return the combined rolling resistance.
 */
[[nodiscard]] inline Coefficient calculateCombinedRollingResistance(Coefficient rollingResistanceA, Coefficient rollingResistanceB, Material::FrictionCombine frictionCombineA,
	Material::FrictionCombine frictionCombineB) {
	const Material::FrictionCombine frictionCombine = static_cast<Material::FrictionCombine>(min(static_cast<uint8_t>(frictionCombineA), static_cast<uint8_t>(frictionCombineB)));
	switch (frictionCombine) {
		case Material::FrictionCombine::AVERAGE: return midpoint(rollingResistanceA, rollingResistanceB);
		case Material::FrictionCombine::MINIMUM: return min(rollingResistanceA, rollingResistanceB);
		case Material::FrictionCombine::MAXIMUM: return max(rollingResistanceA, rollingResistanceB);
		case Material::FrictionCombine::MULTIPLY: return rollingResistanceA * rollingResistanceB;
	}
	unreachable();
}

/**
 * Combine the restitution of two objects based on their restitution
 * combination modes.
 *
 * \param restitutionA restitution of the first object.
 * \param restitutionB restitution of the second object.
 * \param restitutionCombineA restitution combination mode of the first object.
 * \param restitutionCombineB restitution combination mode of the second object.
 *
 * \return the combined restitution.
 */
[[nodiscard]] inline Coefficient calculateCombinedRestitution(Coefficient restitutionA, Coefficient restitutionB, Material::RestitutionCombine restitutionCombineA,
	Material::RestitutionCombine restitutionCombineB) {
	const Material::RestitutionCombine restitutionCombine =
		static_cast<Material::RestitutionCombine>(min(static_cast<uint8_t>(restitutionCombineA), static_cast<uint8_t>(restitutionCombineB)));
	switch (restitutionCombine) {
		case Material::RestitutionCombine::AVERAGE: return midpoint(restitutionA, restitutionB);
		case Material::RestitutionCombine::MINIMUM: return min(restitutionA, restitutionB);
		case Material::RestitutionCombine::MAXIMUM: return max(restitutionA, restitutionB);
		case Material::RestitutionCombine::MULTIPLY: return restitutionA * restitutionB;
	}
	unreachable();
}

/**
 * Calculate the inverse mass of a given mass, while accounting for 0 and
 * infinity.
 *
 * \param mass mass to get the inverse mass of.
 *
 * \return infinity if the mass is 0, 0 if the mass is infinite, the reciprocal
 *         of the mass otherwise.
 */
[[nodiscard]] inline InverseMass calculateInverseMass(Mass mass) {
	return (mass == 0) ? InverseMass::INF : (isinf(mass)) ? InverseMass{} : InverseMass{Scalar{1} / mass};
}

/**
 * Calculate the mass of a given inverse mass, while accounting for 0 and
 * infinity.
 *
 * \param inverseMass inverse mass to get the mass of.
 *
 * \return infinity if the inverse mass is 0, 0 if the inverse mass is infinite,
 *         the reciprocal of the inverse mass otherwise.
 */
[[nodiscard]] inline Mass calculateMass(InverseMass inverseMass) {
	return (inverseMass == 0) ? Mass::INF : Mass{Scalar{1} / inverseMass};
}

/**
 * Calculate the inverse moment of inertia of a given moment of inertia, while
 * accounting for 0 and infinity.
 *
 * \param momentOfInertia moment of inertia to get the inverse moment of inertia
 *        of.
 *
 * \return infinity if the moment of inertia is 0, 0 if the moment of inertia is
 *         infinite, the reciprocal of the moment of inertia otherwise.
 */
[[nodiscard]] inline InverseMomentOfInertia2D calculateInverseMomentOfInertia(MomentOfInertia2D momentOfInertia) {
	return (momentOfInertia == 0) ? InverseMomentOfInertia2D::INF : (isinf(momentOfInertia)) ? InverseMomentOfInertia2D{} : InverseMomentOfInertia2D{Scalar{1} / momentOfInertia};
}

/**
 * Calculate the inverse moment of inertia of a given moment of inertia, while
 * accounting for 0 and infinity.
 *
 * \param momentOfInertia moment of inertia to get the inverse moment of inertia
 *        of.
 *
 * \return infinity if the moment of inertia is 0, 0 if the moment of inertia is
 *         infinite, the reciprocal of the moment of inertia otherwise,
 *         separately for each axis.
 */
[[nodiscard]] inline InverseMomentOfInertia3D calculateInverseMomentOfInertia(MomentOfInertia3D momentOfInertia) {
	return InverseMomentOfInertia3D{
		calculateInverseMomentOfInertia(momentOfInertia.getX()),
		calculateInverseMomentOfInertia(momentOfInertia.getY()),
		calculateInverseMomentOfInertia(momentOfInertia.getZ()),
	};
}

/**
 * Calculate the moment of inertia of a given inverse moment of inertia, while
 * accounting for 0 and infinity.
 *
 * \param inverseMomentOfInertia inverse moment of inertia to get the moment of
 *        inertia of.
 *
 * \return infinity if the inverse moment of inertia is 0, 0 if the inverse
 *         moment of inertia is infinite, the reciprocal of the inverse moment
 *         of inertia otherwise.
 */
[[nodiscard]] inline MomentOfInertia2D calculateMomentOfInertia(InverseMomentOfInertia2D inverseMomentOfInertia) {
	return (inverseMomentOfInertia == 0) ? MomentOfInertia2D::INF : MomentOfInertia2D{Scalar{1} / inverseMomentOfInertia};
}

/**
 * Calculate the moment of inertia of a given inverse moment of inertia, while
 * accounting for 0 and infinity.
 *
 * \param inverseMomentOfInertia inverse moment of inertia to get the moment of
 *        inertia of.
 *
 * \return infinity if the inverse moment of inertia is 0, 0 if the inverse
 *         moment of inertia is infinite, the reciprocal of the inverse moment
 *         of inertia otherwise, separately for each axis.
 */
[[nodiscard]] inline MomentOfInertia3D calculateMomentOfInertia(InverseMomentOfInertia3D inverseMomentOfInertia) {
	return MomentOfInertia3D{
		calculateMomentOfInertia(inverseMomentOfInertia.getX()),
		calculateMomentOfInertia(inverseMomentOfInertia.getY()),
		calculateMomentOfInertia(inverseMomentOfInertia.getZ()),
	};
}

/**
 * Calculate the principal moments of inertia of an object given its shape and
 * mass.
 *
 * \param shape shape of the object.
 * \param mass mass of the object.
 *
 * \return an approximation of the principal moments of inertia of the object.
 */
template <size_t N>
[[nodiscard]] inline PrincipalMomentsOfInertia<N> calculatePrincipalMomentsOfInertia(const Shape<N>& shape, Mass mass) {
	if (mass == 0) {
		return PrincipalMomentsOfInertia<N>::INF;
	}
	if (isinf(mass)) {
		return {};
	}
	PrincipalMomentsOfInertia<N> result = shape.calculatePrincipalMomentsOfInertia(mass);
	if constexpr (N == 3) {
		meta::forEachIndex<3>([&](auto index) -> void {
			if (result[index] == 0) {
				result[index] = PrincipalMomentsOfInertia2D::INF;
			}
		});
	} else {
		if (result == 0) {
			result = PrincipalMomentsOfInertia<N>::INF;
		}
	}
	return result;
}

/**
 * Calculate the world-space moment of inertia tensor of an object given its
 * principal moments of inertia, the local orientation of the inertia frame, and
 * the world-space orientation of the object.
 *
 * \param principalMomentsOfInertia principal moments of inertia of the object.
 * \param localInertiaOrientation local orientation of the inertia frame.
 * \param orientation world-space orientation of the object.
 *
 * \return the world-space moment of inertia tensor of the object.
 */
[[nodiscard]] inline MomentOfInertiaTensor2D calculateMomentOfInertiaTensor(PrincipalMomentsOfInertia2D principalMomentsOfInertia,
	LocalInertiaOrientation2D localInertiaOrientation, Orientation2D orientation) {
	(void)localInertiaOrientation;
	(void)orientation;
	if (isinf(principalMomentsOfInertia)) {
		principalMomentsOfInertia = {};
	}
	return MomentOfInertiaTensor2D{principalMomentsOfInertia};
}

/**
 * Calculate the world-space moment of inertia tensor of an object given its
 * principal moments of inertia, the local orientation of the inertia frame, and
 * the world-space orientation of the object.
 *
 * \param principalMomentsOfInertia principal moments of inertia of the object.
 * \param localInertiaOrientation local orientation of the inertia frame.
 * \param orientation world-space orientation of the object.
 *
 * \return the world-space moment of inertia tensor of the object.
 */
[[nodiscard]] inline MomentOfInertiaTensor3D calculateMomentOfInertiaTensor(PrincipalMomentsOfInertia3D principalMomentsOfInertia,
	LocalInertiaOrientation3D localInertiaOrientation, Orientation3D orientation) {
	meta::forEachIndex<3>([&](auto index) -> void {
		if (isinf(principalMomentsOfInertia[index])) {
			principalMomentsOfInertia[index] = {};
		}
	});
	const MomentOfInertiaTensor3D localMomentOfInertiaTensor = rotateScale(localInertiaOrientation, principalMomentsOfInertia);
	const Basis3D rotation = rotate(orientation);
	return rotation * localMomentOfInertiaTensor * transpose(rotation);
}

/**
 * Calculate the world-space inverse moment of inertia tensor of an object given
 * its inverse principal moments of inertia, the local orientation of the
 * inertia frame, and the world-space orientation of the object.
 *
 * \param inversePrincipalMomentsOfInertia inverse principal moments of inertia
 *        of the object.
 * \param localInertiaOrientation local orientation of the inertia frame.
 * \param orientation world-space orientation of the object.
 *
 * \return the world-space inverse moment of inertia tensor of the object.
 */
[[nodiscard]] inline InverseMomentOfInertiaTensor2D calculateInverseMomentOfInertiaTensor(InversePrincipalMomentsOfInertia2D inversePrincipalMomentsOfInertia,
	LocalInertiaOrientation2D localInertiaOrientation, Orientation2D orientation) {
	(void)localInertiaOrientation;
	(void)orientation;
	if (isinf(inversePrincipalMomentsOfInertia)) {
		inversePrincipalMomentsOfInertia = {};
	}
	return InverseMomentOfInertiaTensor2D{inversePrincipalMomentsOfInertia};
}

/**
 * Calculate the world-space inverse moment of inertia tensor of an object given
 * its inverse principal moments of inertia, the local orientation of the
 * inertia frame, and the world-space orientation of the object.
 *
 * \param inversePrincipalMomentsOfInertia inverse principal moments of inertia
 *        of the object.
 * \param localInertiaOrientation local orientation of the inertia frame.
 * \param orientation world-space orientation of the object.
 *
 * \return the world-space inverse moment of inertia tensor of the object.
 */
[[nodiscard]] inline InverseMomentOfInertiaTensor3D calculateInverseMomentOfInertiaTensor(InversePrincipalMomentsOfInertia3D inversePrincipalMomentsOfInertia,
	LocalInertiaOrientation3D localInertiaOrientation, Orientation3D orientation) {
	meta::forEachIndex<3>([&](auto index) -> void {
		if (isinf(inversePrincipalMomentsOfInertia[index])) {
			inversePrincipalMomentsOfInertia[index] = {};
		}
	});
	const InverseMomentOfInertiaTensor3D inverseLocalMomentOfInertiaTensor = rotateScale(localInertiaOrientation, inversePrincipalMomentsOfInertia);
	const Basis3D rotation = rotate(orientation);
	return rotation * inverseLocalMomentOfInertiaTensor * transpose(rotation);
}

/**
 * Calculate the combined effective mass of two points on two different objects
 * along a given axis.
 *
 * \param inverseMassA inverse mass of the first object.
 * \param inverseMassB inverse mass of the second object.
 * \param inverseMomentOfInertiaTensorA world-space inverse moment of inertia
 *        tensor of the first object.
 * \param inverseMomentOfInertiaTensorB world-space inverse moment of inertia
 *        tensor of the second object.
 * \param offsetA world-space offset relative to the first object's center of
 *        mass to calculate the effective mass at.
 * \param offsetB world-space offset relative to the second object's center of
 *        mass to calculate the effective mass at.
 * \param axis world-space direction along which to calculate the combined
 *        effective mass of the two points.
 *
 * \return the combined effective mass of the two objects' points along the
 *         given axis.
 */
template <size_t N>
[[nodiscard]] inline Mass calculateCombinedEffectiveMassAlongAxis(InverseMass inverseMassA, InverseMass inverseMassB,
	const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensorA, const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensorB, Length<N> offsetA,
	Length<N> offsetB, Direction<N> axis) {
	const MomentArm<N> momentArmA = cross(offsetA, axis);
	const MomentArm<N> momentArmB = cross(offsetB, axis);
	const InverseMass inverseEffectiveMassA = inverseMassA + dot(momentArmA, inverseMomentOfInertiaTensorA * momentArmA);
	const InverseMass inverseEffectiveMassB = inverseMassB + dot(momentArmB, inverseMomentOfInertiaTensorB * momentArmB);
	const InverseMass combinedInverseEffectiveMass = inverseEffectiveMassA + inverseEffectiveMassB;
	const Mass combinedEffectiveMass = select(equal(combinedInverseEffectiveMass, 0), Mass{}, inverse(combinedInverseEffectiveMass));
	return combinedEffectiveMass;
}

/**
 * Calculate the combined effective mass of two points on two different objects
 * along each axis of a given orthonormal basis.
 *
 * \param inverseMassA inverse mass of the first object.
 * \param inverseMassB inverse mass of the second object.
 * \param inverseMomentOfInertiaTensorA world-space inverse moment of inertia
 *        tensor of the first object.
 * \param inverseMomentOfInertiaTensorB world-space inverse moment of inertia
 *        tensor of the second object.
 * \param offsetA world-space offset relative to the first object's center of
 *        mass to calculate the effective masses at.
 * \param offsetB world-space offset relative to the second object's center of
 *        mass to calculate the effective masses at.
 * \param axes basis of world-space directions along which to calculate the
 *        combined effective masses of the two points.
 *
 * \return the combined effective masses of the two objects' points along the
 *         given axes.
 */
template <size_t N>
[[nodiscard]] inline Quantity<N, Mass::Unit> calculateCombinedEffectiveMassesAlongAxes(InverseMass inverseMassA, InverseMass inverseMassB,
	const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensorA, const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensorB, Length<N> offsetA,
	Length<N> offsetB, OrthonormalBasis<N> axes) {
	const MomentArmTensor<N> momentArmsA = cross(offsetA, axes);
	const MomentArmTensor<N> momentArmsB = cross(offsetB, axes);
	const Quantity<N, InverseMass::Unit> inverseEffectiveMassesA = inverseMassA + dot(momentArmsA, inverseMomentOfInertiaTensorA * momentArmsA);
	const Quantity<N, InverseMass::Unit> inverseEffectiveMassesB = inverseMassB + dot(momentArmsB, inverseMomentOfInertiaTensorB * momentArmsB);
	const Quantity<N, InverseMass::Unit> combinedInverseEffectiveMasses = inverseEffectiveMassesA + inverseEffectiveMassesB;
	const Quantity<N, Mass::Unit> combinedEffectiveMasses = select(equal(combinedInverseEffectiveMasses, 0), Quantity<N, Mass::Unit>{}, inverse(combinedInverseEffectiveMasses));
	return combinedEffectiveMasses;
}

/**
 * Calculate the combined effective moment of inertia of two objects around a
 * given axis.
 *
 * \param inverseMomentOfInertiaTensorA world-space inverse moment of inertia
 *        tensor of the first object.
 * \param inverseMomentOfInertiaTensorB world-space inverse moment of inertia
 *        tensor of the second object.
 * \param axis world-space axis around which to calculate the combined effective
 *        moment of inertia of the two objects.
 *
 * \return the combined effective moment of inertia of the two objects around
 *         the given axis.
 */
[[nodiscard]] inline MomentOfInertia2D calculateCombinedEffectiveMomentOfInertiaAroundAxis(const InverseMomentOfInertiaTensor3D& inverseMomentOfInertiaTensorA,
	const InverseMomentOfInertiaTensor3D& inverseMomentOfInertiaTensorB, Direction3D axis) {
	const InverseMomentOfInertia2D inverseEffectiveMomentOfInertiaA = dot(axis, inverseMomentOfInertiaTensorA * axis);
	const InverseMomentOfInertia2D inverseEffectiveMomentOfInertiaB = dot(axis, inverseMomentOfInertiaTensorB * axis);
	const InverseMomentOfInertia2D combinedInverseEffectiveMomentOfInertia = inverseEffectiveMomentOfInertiaA + inverseEffectiveMomentOfInertiaB;
	const MomentOfInertia2D combinedEffectiveMomentOfInertia =
		select(equal(combinedInverseEffectiveMomentOfInertia, 0), MomentOfInertia2D{}, inverse(combinedInverseEffectiveMomentOfInertia));
	return combinedEffectiveMomentOfInertia;
}

/**
 * Calculate the combined effective moment of inertia tensor of two objects.
 *
 * \param inverseMomentOfInertiaTensorA world-space inverse moment of inertia
 *        tensor of the first object.
 * \param inverseMomentOfInertiaTensorB world-space inverse moment of inertia
 *        tensor of the second object.
 *
 * \return the combined effective world-space moment of inertia tensor of the
 *         two objects.
 */
template <size_t N>
[[nodiscard]] inline MomentOfInertiaTensor<N> calculateCombinedEffectiveMomentOfInertiaTensor(const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensorA,
	const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensorB) {
	const InverseMomentOfInertiaTensor<N> combinedInverseEffectiveMomentOfInertiaTensor = inverseMomentOfInertiaTensorA + inverseMomentOfInertiaTensorB;
	const MomentOfInertiaTensor<N> combinedEffectiveMomentOfInertiaTensor =
		(combinedInverseEffectiveMomentOfInertiaTensor == 0)
	        ? MomentOfInertiaTensor<N>{Scalar{0} * KILOGRAM_SQUARE_METERS}
	        : inverse(combinedInverseEffectiveMomentOfInertiaTensor);
	return combinedEffectiveMomentOfInertiaTensor;
}

/**
 * Calculate the linear velocity of a point on an object with linear and angular
 * velocity.
 *
 * \param linearVelocity linear velocity of the object's center of mass.
 * \param angularVelocity angular velocity of the object around its center of
 *        mass.
 * \param offset world-space offset relative to the object's center of mass to
 *        calculate the velocity at.
 *
 * \return the world-space linear velocity of the object at the given point.
 */
template <size_t N>
[[nodiscard]] inline LinearVelocity<N> calculatePointVelocity(LinearVelocity<N> linearVelocity, AngularVelocity<N> angularVelocity, Length<N> offset) {
	return linearVelocity + cross(angularVelocity, offset);
}

/**
 * Apply a world-space impulse to an object's velocity.
 *
 * \param linearVelocity linear velocity of the object to apply the impulse to.
 * \param angularVelocity angular velocity of the object to apply the impulse
 *        to.
 * \param inverseMass inverse mass of the object.
 * \param inverseMomentOfInertiaTensor world-space inverse moment of inertia
 *        tensor of the object.
 * \param impulse world-space impulse to apply.
 * \param offset world-space offset relative to the object's center of mass to
 *        apply the impulse at.
 */
template <size_t N>
inline void applyImpulse(LinearVelocity<N>& linearVelocity, AngularVelocity<N>& angularVelocity, InverseMass inverseMass,
	const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensor, LinearImpulse<N> impulse, Length<N> offset) {
	linearVelocity += inverseMass * impulse;
	angularVelocity += inverseMomentOfInertiaTensor * cross(offset, impulse);
}

/**
 * Apply a world-space impulse to an object's velocity, making it active and
 * marking it as corrected, unless the object is not correctable.
 *
 * \param activity activity state of the object. If
 *        ObjectActivity::isCorrectible is false, the impulse is ignored.
 *        Otherwise, the impulse is applied, and ObjectActivity::wasCorrected
 *        and ObjectActivity::isActive are both set to true.
 * \param linearVelocity linear velocity of the object to apply the impulse to.
 * \param angularVelocity angular velocity of the object to apply the impulse
 *        to.
 * \param inverseMass inverse mass of the object.
 * \param inverseMomentOfInertiaTensor world-space inverse moment of inertia
 *        tensor of the object.
 * \param impulse world-space impulse to apply.
 * \param offset world-space offset relative to the object's center of mass to
 *        apply the impulse at.
 */
template <size_t N>
inline void applyCorrectionImpulse(ObjectActivity& activity, LinearVelocity<N>& linearVelocity, AngularVelocity<N>& angularVelocity, InverseMass inverseMass,
	const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensor, LinearImpulse<N> impulse, Length<N> offset) {
	if (activity.isCorrectable) {
		activity.wasCorrected = true;
		activity.isActive = true;
		applyImpulse<N>(linearVelocity, angularVelocity, inverseMass, inverseMomentOfInertiaTensor, impulse, offset);
	}
}

/**
 * Apply a world-space linear impulse to an object's velocity at its center of
 * mass.
 *
 * \param linearVelocity linear velocity of the object to apply the impulse to.
 * \param inverseMass inverse mass of the object.
 * \param linearImpulse world-space impulse to apply.
 */
template <size_t N>
inline void applyLinearImpulse(LinearVelocity<N>& linearVelocity, InverseMass inverseMass, LinearImpulse<N> linearImpulse) {
	linearVelocity += inverseMass * linearImpulse;
}

/**
 * Apply a world-space linear impulse to an object's velocity at its center of
 * mass, making it active and marking it as corrected, unless the object is not
 * correctable.
 *
 * \param activity activity state of the object. If
 *        ObjectActivity::isCorrectible is false, the impulse is ignored.
 *        Otherwise, the impulse is applied, and ObjectActivity::wasCorrected
 *        and ObjectActivity::isActive are both set to true.
 * \param linearVelocity linear velocity of the object to apply the impulse to.
 * \param inverseMass inverse mass of the object.
 * \param linearImpulse world-space impulse to apply.
 */
template <size_t N>
inline void applyLinearCorrectionImpulse(ObjectActivity& activity, LinearVelocity<N>& linearVelocity, InverseMass inverseMass, LinearImpulse<N> linearImpulse) {
	if (activity.isCorrectable) {
		activity.wasCorrected = true;
		activity.isActive = true;
		applyLinearImpulse<N>(linearVelocity, inverseMass, linearImpulse);
	}
}

/**
 * Apply a world-space angular impulse to an object's velocity at its center of
 * mass.
 *
 * \param angularVelocity angular velocity of the object to apply the impulse
 *        to.
 * \param inverseMomentOfInertiaTensor world-space inverse moment of inertia
 *        tensor of the object.
 * \param angularImpulse world-space angular impulse to apply.
 */
template <size_t N>
inline void applyAngularImpulse(AngularVelocity<N>& angularVelocity, const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensor, AngularImpulse<N> angularImpulse) {
	angularVelocity += inverseMomentOfInertiaTensor * angularImpulse;
}

/**
 * Apply a world-space angular impulse to an object's velocity at its center of
 * mass, making it active and marking it as corrected, unless the object is not
 * correctable.
 *
 * \param activity activity state of the object. If
 *        ObjectActivity::isCorrectible is false, the impulse is ignored.
 *        Otherwise, the impulse is applied, and ObjectActivity::wasCorrected
 *        and ObjectActivity::isActive are both set to true.
 * \param angularVelocity angular velocity of the object to apply the impulse
 *        to.
 * \param inverseMomentOfInertiaTensor world-space inverse moment of inertia
 *        tensor of the object.
 * \param angularImpulse world-space angular impulse to apply.
 */
template <size_t N>
inline void applyAngularCorrectionImpulse(ObjectActivity& activity, AngularVelocity<N>& angularVelocity, const InverseMomentOfInertiaTensor<N>& inverseMomentOfInertiaTensor,
	AngularImpulse<N> angularImpulse) {
	if (activity.isCorrectable) {
		activity.wasCorrected = true;
		activity.isActive = true;
		applyAngularImpulse<N>(angularVelocity, inverseMomentOfInertiaTensor, angularImpulse);
	}
}

} // namespace grem::physics

#endif
