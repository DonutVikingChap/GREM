// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_JOINTS_HPP
#define GREM_PHYSICS_JOINTS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Registry.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/physics/EntityID.hpp>
#include <GREM/physics/quantities.hpp>

namespace grem::physics {

/**
 * Default stiffness value for linear joint limits.
 */
inline constexpr Frequency DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS = 60.0f * HERTZ;

/**
 * Default damping value for linear joint limits.
 */
inline constexpr Coefficient DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO = 2.0f;

/**
 * Default stiffness value for angular joint limits.
 */
inline constexpr Frequency DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS = 60.0f * HERTZ;

/**
 * Default damping value for angular joint limits.
 */
inline constexpr Coefficient DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO = 2.0f;

/**
 * Ordered pair of handles to the objects connected by a joint.
 */
struct JointConnectedObjects : Pair<EntityID> {
	using Pair::Pair;

	constexpr JointConnectedObjects(Pair<EntityID> objectIDs) noexcept
		: Pair(objectIDs) {}
};

/**
 * Local offsets of the attachment frames of the objects connected by a joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointAttachmentOffsets : Pair<Length<N>> {
	using Pair<Length<N>>::Pair;

	constexpr JointAttachmentOffsets(Pair<Length<N>> localOffsets) noexcept
		: Pair<Length<N>>(localOffsets) {}
};
using JointAttachmentOffsets2D = JointAttachmentOffsets<2>; ///< Local offsets of the attachment frames of the objects connected by a joint in a 2-dimensional world.
using JointAttachmentOffsets3D = JointAttachmentOffsets<3>; ///< Local offsets of the attachment frames of the objects connected by a joint in a 3-dimensional world.

/**
 * Local orientations of the attachment frames of the objects connected by a
 * joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointAttachmentOrientations : Pair<Orientation<N>> {
	using Pair<Orientation<N>>::Pair;

	constexpr JointAttachmentOrientations(Pair<Orientation<N>> localOrientations) noexcept
		: Pair<Orientation<N>>(localOrientations) {}
};
using JointAttachmentOrientations2D = JointAttachmentOrientations<2>; ///< Local orientations of the attachment frames of the objects connected by a joint in a 2-dimensional world.
using JointAttachmentOrientations3D = JointAttachmentOrientations<3>; ///< Local orientations of the attachment frames of the objects connected by a joint in a 3-dimensional world.

/**
 * Linear limits of a joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointLinearConstraint {
	/**
	 * Target velocity of the drive along each local axis of the first connected
	 * object's attachment frame.
	 */
	LinearVelocity<N> driveTargetVelocities{};

	/**
	 * Maximum force applied by the drive to reach the target velocity along
	 * each local axis of the first connected object's attachment frame.
	 */
	Force<N> driveMaxForces{};

	/**
	 * Minimum signed distance of the attachment frame points along each local
	 * axis of the first connected object's attachment frame.
	 *
	 * Each component must be less than or equal to the corresponding component
	 * of #maxOffsets.
	 */
	Length<N> minOffsets = Length<N>::MIN;

	/**
	 * Maximum signed distance of the attachment frame points along each local
	 * axis of the first connected object's attachment frame.
	 *
	 * Each component must be greater than or equal to the corresponding
	 * component of #minOffsets.
	 */
	Length<N> maxOffsets = Length<N>::MAX;

	/**
	 * Stiffness of the constraint limits for each axis.
	 *
	 * Each component must be non-negative.
	 */
	Rate<N> limitStiffnesses{DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS};

	/**
	 * Damping ratio used when solving the constraint limits for each axis.
	 *
	 * Each component must be positive.
	 */
	Quantity<N, Unitless> limitDampingRatios{DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO};

	/**
	 * Compare this constraint to another constraint for equality.
	 *
	 * \param other constraint to compare against.
	 *
	 * \return true if the constraints are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const JointLinearConstraint& other) const noexcept = default;
};
using JointLinearConstraint2D = JointLinearConstraint<2>; ///< Linear limits of a joint in a 2-dimensional world.
using JointLinearConstraint3D = JointLinearConstraint<3>; ///< Linear limits of a joint in a 3-dimensional world.

/**
 * %Impulse applied by the linear limits of a joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointLinearConstraintImpulses {
	/**
	 * Momentum of the JointLinearConstraint<N>::driveMaxForces force along each
	 * local axis of the first object's attachment frame.
	 */
	LinearMomentum<N> driveMomentums{};

	/**
	 * %Impulse applied by the JointLinearConstraint<N>::driveMaxForces force
	 * along each local axis of the first object's attachment frame during the
	 * last simulation step.
	 */
	LinearImpulse<N> driveImpulses{};

	/**
	 * Momentum of the JointLinearConstraint<N>::minOffsets limit along each
	 * local axis of the first object's attachment frame.
	 */
	LinearMomentum<N> lowerLimitMomentums{};

	/**
	 * %Impulse applied by the JointLinearConstraint<N>::minOffsets limit along
	 * each local axis of the first object's attachment frame during the last
	 * simulation step.
	 */
	LinearImpulse<N> lowerLimitImpulses{};

	/**
	 * Momentum of the JointLinearConstraint<N>::maxOffsets limit along each
	 * local axis of the first object's attachment frame.
	 */
	LinearMomentum<N> upperLimitMomentums{};

	/**
	 * %Impulse applied by the JointLinearConstraint<N>::maxOffsets limit along
	 * each local axis of the first object's attachment frame during the last
	 * simulation step.
	 */
	LinearImpulse<N> upperLimitImpulses{};
};
using JointLinearConstraintImpulses2D = JointLinearConstraintImpulses<2>; ///< %Impulse applied by the linear limits of a joint in a 2-dimensional world.
using JointLinearConstraintImpulses3D = JointLinearConstraintImpulses<3>; ///< %Impulse applied by the linear limits of a joint in a 3-dimensional world.

/**
 * Distance limit of a joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointDistanceConstraint {
	/**
	 * Target velocity of the drive.
	 */
	LinearVelocity1D driveTargetVelocity{};

	/**
	 * Maximum force applied by the drive to reach the target velocity.
	 */
	Force1D driveMaxForce{};

	/**
	 * Minimum distance between the attachment frame points.
	 *
	 * Must be non-negative.
	 * Must be less than or equal to #maxDistance.
	 */
	Distance minDistance{};

	/**
	 * Maximum signed distance of the attachment frame points.
	 *
	 * Must be non-negative.
	 * Must be greater than or equal to #minDistance.
	 */
	Distance maxDistance = Distance::MAX;

	/**
	 * Stiffness of the constraint limits.
	 *
	 * Must be non-negative.
	 */
	Frequency limitStiffness = DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS;

	/**
	 * Damping ratio used when solving the constraint limits.
	 *
	 * Must be positive.
	 */
	Coefficient limitDampingRatio = DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO;

	/**
	 * Compare this constraint to another constraint for equality.
	 *
	 * \param other constraint to compare against.
	 *
	 * \return true if the constraints are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const JointDistanceConstraint& other) const noexcept = default;
};
using JointDistanceConstraint2D = JointDistanceConstraint<2>; ///< Distance limit of a joint in a 2-dimensional world.
using JointDistanceConstraint3D = JointDistanceConstraint<3>; ///< Distance limit of a joint in a 3-dimensional world.

/**
 * %Impulse applied by the distance limit of a joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointDistanceConstraintImpulses {
	/**
	 * Momentum of the JointDistanceConstraint<N>::driveMaxForce force between
	 * the attachment frame points.
	 */
	LinearMomentum1D driveMomentum{};

	/**
	 * %Impulse applied by the JointDistanceConstraint<N>::driveMaxForce force
	 * between the attachment frame points during the last simulation step.
	 */
	LinearImpulse1D driveImpulse{};

	/**
	 * Momentum of the JointDistanceConstraint<N>::minDistance limit between the
	 * attachment frame points.
	 */
	LinearMomentum1D lowerLimitMomentum{};

	/**
	 * %Impulse applied by the JointDistanceConstraint<N>::minDistance limit
	 * between the attachment frame points during the last simulation step.
	 */
	LinearImpulse1D lowerLimitImpulse{};

	/**
	 * Momentum of the JointDistanceConstraint<N>::maxDistance limit between the
	 * attachment frame points.
	 */
	LinearMomentum1D upperLimitMomentum{};

	/**
	 * %Impulse applied by the JointDistanceConstraint<N>::maxDistance limit
	 * between the attachment frame points during the last simulation step.
	 */
	LinearImpulse1D upperLimitImpulse{};

	/**
	 * World-space offsets of the attachment points from the objects' centers of
	 * mass on the last constraint solver iteration.
	 */
	Pair<Length<N>> lastOffsets{};
};
using JointDistanceConstraintImpulses2D = JointDistanceConstraintImpulses<2>; ///< %Impulse applied by the distance limit of a joint in a 2-dimensional world.
using JointDistanceConstraintImpulses3D = JointDistanceConstraintImpulses<3>; ///< %Impulse applied by the distance limit of a joint in a 3-dimensional world.

/**
 * Angular limits of a joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointAngularConstraint {
	/**
	 * Target velocity of the drive around each local axis of the first
	 * connected object's attachment frame.
	 */
	AngularVelocity<N> driveTargetVelocities{};

	/**
	 * Maximum torque applied by the drive to reach the target velocity around
	 * each local axis of the first connected object's attachment frame.
	 */
	Torque<N> driveMaxTorques{};

	/**
	 * Minimum angle of the attachment frames around each local axis of the
	 * first connected object's attachment frame.
	 *
	 * Each component must be less than or equal to the corresponding component
	 * of #maxAngles.
	 */
	Rotation<N> minAngles = Rotation<N>::MIN;

	/**
	 * Maximum angle of the attachment frames around each local axis of the
	 * first connected object's attachment frame.
	 *
	 * Each component must be greater than or equal to the corresponding
	 * component of #minAngles.
	 */
	Rotation<N> maxAngles = Rotation<N>::MAX;

	/**
	 * Stiffness of the constraint limits for each axis.
	 *
	 * Each component must be non-negative.
	 */
	AngularFrequency<N> limitStiffnesses{DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS};

	/**
	 * Damping ratio used when solving the constraint limits for each axis.
	 *
	 * Each component must be positive.
	 */
	AngularScale<N> limitDampingRatios{DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO};

	/**
	 * Compare this constraint to another constraint for equality.
	 *
	 * \param other constraint to compare against.
	 *
	 * \return true if the constraints are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const JointAngularConstraint& other) const noexcept = default;
};
using JointAngularConstraint2D = JointAngularConstraint<2>; ///< Angular limits of a joint in a 2-dimensional world.
using JointAngularConstraint3D = JointAngularConstraint<3>; ///< Angular limits of a joint in a 3-dimensional world.

/**
 * %Impulse applied by the angular limits of a joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointAngularConstraintImpulses {
	/**
	 * Momentum of the JointAngularConstraint<N>::driveMaxTorques torque around
	 * each local axis of the first object's attachment frame.
	 */
	AngularMomentum<N> driveMomentums{};

	/**
	 * %Impulse applied by the JointAngularConstraint<N>::driveMaxTorques torque
	 * around each local axis of the frist object's attachment frame during the
	 * last simulation step.
	 */
	AngularImpulse<N> driveImpulses{};

	/**
	 * Momentum of the JointAngularConstraint<N>::minAngles limit around each
	 * local axis of the first object's attachment frame.
	 */
	AngularMomentum<N> lowerLimitMomentums{};

	/**
	 * %Impulse applied by the JointAngularConstraint<N>::minAngles limit around
	 * each local axis of the first object's attachment frame during the last
	 * simulation step.
	 */
	AngularImpulse<N> lowerLimitImpulses{};

	/**
	 * Momentum of the JointAngularConstraint<N>::maxAngles limit around each
	 * local axis of the first object's attachment frame.
	 */
	AngularMomentum<N> upperLimitMomentums{};

	/**
	 * %Impulse applied by the JointAngularConstraint<N>::maxAngles limit around
	 * each local axis of the first object's attachment frame during the last
	 * simulation step.
	 */
	AngularImpulse<N> upperLimitImpulses{};
};
using JointAngularConstraintImpulses2D = JointAngularConstraintImpulses<2>; ///< %Impulse applied by the angular limits of a joint in a 2-dimensional world.
using JointAngularConstraintImpulses3D = JointAngularConstraintImpulses<3>; ///< %Impulse applied by the angular limits of a joint in a 3-dimensional world.

/**
 * Cone-shaped angular limit of a joint in a 3-dimensional world.
 */
struct JointConeConstraint3D {
	/**
	 * Minimum angle between the attachment frames.
	 *
	 * Must be less than or equal to #maxAngle.
	 */
	Angle minAngle = Angle::MIN;

	/**
	 * Maximum angle between the attachment frames.
	 *
	 * Must be greater than or equal to #minAngle.
	 */
	Angle maxAngle = Angle::MAX;

	/**
	 * Stiffness of the constraint limits.
	 *
	 * Must be non-negative.
	 */
	Frequency limitStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/**
	 * Damping ratio used when solving the constraint limits.
	 *
	 * Must be positive.
	 */
	Coefficient limitDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/**
	 * Compare this constraint to another constraint for equality.
	 *
	 * \param other constraint to compare against.
	 *
	 * \return true if the constraints are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const JointConeConstraint3D& other) const noexcept = default;
};

/**
 * %Impulse applied by the cone-shaped angular limit of a joint in a
 * 3-dimensional world.
 */
struct JointConeConstraintImpulses3D {
	/**
	 * Momentum of the JointConeConstraint3D::minAngle limit.
	 */
	AngularMomentum2D lowerLimitMomentum{};

	/**
	 * %Impulse applied by the JointConeConstraint3D::minAngle limit during
	 * the last simulation step.
	 */
	AngularImpulse2D lowerLimitImpulse{};

	/**
	 * Momentum of the JointConeConstraint3D::maxAngle limit.
	 */
	AngularMomentum2D upperLimitMomentum{};

	/**
	 * %Impulse applied by the JointConeConstraint3D::maxAngle limit during
	 * the last simulation step.
	 */
	AngularImpulse2D upperLimitImpulse{};
};

/**
 * Angular limit of a joint in a 3-dimensional world around the local Y axes of
 * the connected objects' attachment frames.
 */
struct JointTwistConstraint3D {
	/**
	 * Target velocity of the drive.
	 */
	AngularVelocity2D driveTargetVelocity{};

	/**
	 * Maximum torque applied by the drive.
	 */
	Torque2D driveMaxTorque{};

	/**
	 * Minimum twist angle.
	 *
	 * Must be less than or equal to #maxAngle.
	 */
	Angle minAngle = Angle::MIN;

	/**
	 * Maximum twist angle.
	 *
	 * Must be greater than or equal to #minAngle.
	 */
	Angle maxAngle = Angle::MAX;

	/**
	 * Stiffness of the constraint limits.
	 *
	 * Must be non-negative.
	 */
	Frequency limitStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/**
	 * Damping ratio used when solving the constraint limits.
	 *
	 * Must be positive.
	 */
	Coefficient limitDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/**
	 * Compare this constraint to another constraint for equality.
	 *
	 * \param other constraint to compare against.
	 *
	 * \return true if the constraints are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const JointTwistConstraint3D& other) const noexcept = default;
};

/**
 * %Impulse applied by the angular limit of a joint in a 3-dimensional world
 * around the local Y axes of the connected objects' attachment frames.
 */
struct JointTwistConstraintImpulses3D {
	/**
	 * Momentum of the JointTwistConstraint3D::driveMaxTorque torque.
	 */
	AngularMomentum2D driveMomentum{};

	/**
	 * %Impulse applied by the JointTwistConstraint3D::driveMaxTorque torque
	 * during the last simulation step.
	 */
	AngularImpulse2D driveImpulse{};

	/**
	 * Momentum of the JointTwistConstraint3D::minAngle limit.
	 */
	AngularMomentum2D lowerLimitMomentum{};

	/**
	 * %Impulse applied by the JointTwistConstraint3D::minAngle limit during
	 * the last simulation step.
	 */
	AngularImpulse2D lowerLimitImpulse{};

	/**
	 * Momentum of the JointTwistConstraint3D::maxAngle limit.
	 */
	AngularMomentum2D upperLimitMomentum{};

	/**
	 * %Impulse applied by the JointTwistConstraint3D::maxAngle limit during
	 * the last simulation step.
	 */
	AngularImpulse2D upperLimitImpulse{};
};

/**
 * Set of boolean properties of a joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct JointFlags {
	/**
	 * Compare these flags to another set of flags for equality.
	 *
	 * \param other the flags to compare these to.
	 *
	 * \return true if the flags are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const JointFlags& other) const noexcept = default;
};
using JointFlags2D = JointFlags<2>; ///< Set of boolean properties of a joint in a 2-dimensional world.
using JointFlags3D = JointFlags<3>; ///< Set of boolean properties of a joint in a 3-dimensional world.

/**
 * Tag component that is added to joints with active connected objects.
 */
struct JointActiveTag {};

/**
 * Configuration options for a generic joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct GenericJointOptions;

template <>
struct GenericJointOptions<2> {
	/** Local offsets of the attachment frames of the connected objects. */
	Pair<Length2D> attachmentOffsets{};

	/** Local orientations of the attachment frames of the connected objects. */
	Pair<Orientation2D> attachmentOrientations{};

	/** Linear limits of the joint. */
	Optional<JointLinearConstraint2D> linearConstraint{};

	/** Distance limit of the joint. */
	Optional<JointDistanceConstraint2D> distanceConstraint{};

	/** Angular limits of the joint. */
	Optional<JointAngularConstraint2D> angularConstraint{};

	/** Boolean properties of the joint. */
	JointFlags2D flags{};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const GenericJointOptions& other) const noexcept = default;
};

template <>
struct GenericJointOptions<3> {
	/** Local offsets of the attachment frames of the connected objects. */
	Pair<Length3D> attachmentOffsets{};

	/** Local orientations of the attachment frames of the connected objects. */
	Pair<Orientation3D> attachmentOrientations{};

	/** Linear limits of the joint. */
	Optional<JointLinearConstraint3D> linearConstraint{};

	/** Distance limit of the joint. */
	Optional<JointDistanceConstraint3D> distanceConstraint{};

	/** Angular limits of the joint. */
	Optional<JointAngularConstraint3D> angularConstraint{};

	/** Cone limit of the joint. */
	Optional<JointConeConstraint3D> coneConstraint{};

	/** Twist limit of the joint. */
	Optional<JointTwistConstraint3D> twistConstraint{};

	/** Boolean properties of the joint. */
	JointFlags3D flags{};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const GenericJointOptions& other) const noexcept = default;
};

using GenericJointOptions2D = GenericJointOptions<2>; ///< Configuration options for a generic joint in a 2-dimensional world.
using GenericJointOptions3D = GenericJointOptions<3>; ///< Configuration options for a generic joint in a 3-dimensional world.

/**
 * Configuration options for a fully constrained joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct WeldOptions {
	/** Local offsets of the attachment frames of the connected objects. */
	Pair<Length<N>> attachmentOffsets{};

	/** Local orientations of the attachment frames of the connected objects. */
	Pair<Orientation<N>> attachmentOrientations{};

	/** Linear stiffness of the weld. Must be non-negative. */
	Frequency linearStiffness = DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS;

	/** Linear damping ratio of the weld. Must be positive. */
	Coefficient linearDampingRatio = DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO;

	/** Angular stiffness of the weld. Must be non-negative. */
	Frequency angularStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Angular damping ratio of the weld. Must be positive. */
	Coefficient angularDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Boolean properties of the joint. */
	JointFlags<N> flags{};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const WeldOptions& other) const noexcept = default;
};
using WeldOptions2D = WeldOptions<2>; ///< Configuration options for a fully constrained joint in a 2-dimensional world.
using WeldOptions3D = WeldOptions<3>; ///< Configuration options for a fully constrained joint in a 3-dimensional world.

/**
 * Configuration options for a revolute joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct HingeJointOptions {
	/** Local offsets of the attachment frames of the connected objects. */
	Pair<Length<N>> attachmentOffsets{};

	/** Local orientations of the attachment frames of the connected objects. */
	Pair<Orientation<N>> attachmentOrientations{};

	/** Stiffness of the hinge when the attachment frame points are separated. Must be non-negative. */
	Frequency linearAttachmentStiffness = DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS;

	/** Damping ratio of the hinge when the attachment frame points are separated. Must be positive. */
	Coefficient linearAttachmentDampingRatio = DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO;

	/** Stiffness of the hinge when the attachment frame directions are separated. Must be non-negative. */
	Frequency angularAttachmentStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Damping ratio of the hinge when the attachment frame directions are separated. Must be positive. */
	Coefficient angularAttachmentDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Target velocity of the drive. */
	AngularVelocity2D driveTargetVelocity{};

	/** Maximum torque applied by the drive. */
	Torque2D driveMaxTorque{};

	/** Minimum angle of the hinge. Must be less than or equal to #maxAngle. */
	Angle minAngle = Angle::MIN;

	/** Maximum angle of the hinge. Must be greater than or equal to #minAngle. */
	Angle maxAngle = Angle::MAX;

	/** Stiffness of the hinge when extended beyond #minAngle/#maxAngle. Must be non-negative. */
	Frequency angularLimitStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Damping ratio of the hinge when extended beyond #minAngle/#maxAngle. Must be positive. */
	Coefficient angularLimitDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Boolean properties of the joint. */
	JointFlags<N> flags{};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const HingeJointOptions& other) const noexcept = default;
};
using HingeJointOptions2D = HingeJointOptions<2>; ///< Configuration options for a revolute joint in a 2-dimensional world.
using HingeJointOptions3D = HingeJointOptions<3>; ///< Configuration options for a revolute joint in a 3-dimensional world.

/**
 * Configuration options for a spherical joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct BallJointOptions {
	/** Local offsets of the attachment frames of the connected objects. */
	Pair<Length<N>> attachmentOffsets{};

	/** Local orientations of the attachment frames of the connected objects. */
	Pair<Orientation<N>> attachmentOrientations{};

	/** Stiffness of the ball joint when the attachment frame points are separated. Must be non-negative. */
	Frequency linearAttachmentStiffness = DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS;

	/** Damping ratio of the ball joint when the attachment frame points are separated. Must be positive. */
	Coefficient linearAttachmentDampingRatio = DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO;

	/** Target velocity of the twist drive. */
	AngularVelocity2D twistDriveTargetVelocity{};

	/** Maximum torque applied by the twist drive. */
	Torque2D twistDriveMaxTorque{};

	/** Minimum swing angle of the ball joint. Must be less than or equal to #maxSwingAngle. */
	Angle minSwingAngle = Angle::MIN;

	/** Maximum swing angle of the ball joint. Must be greater than or equal to #minSwingAngle. */
	Angle maxSwingAngle = Angle::MAX;

	/** Stiffness of the ball joint when extended beyond #minSwingAngle/#maxSwingAngle. Must be non-negative. */
	Frequency swingLimitStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Damping ratio of the ball joint when extended beyond #minSwingAngle/#maxSwingAngle. Must be positive. */
	Coefficient swingLimitDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Minimum twist angle of the ball joint. Must be less than or equal to #maxTwistAngle. */
	Angle minTwistAngle = Angle::MIN;

	/** Maximum twist angle of the ball joint. Must be greater than or equal to #minTwistAngle. */
	Angle maxTwistAngle = Angle::MAX;

	/** Stiffness of the ball joint when extended beyond #minTwistAngle/#maxTwistAngle. Must be non-negative. */
	Frequency twistLimitStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Damping ratio of the ball joint when extended beyond #minTwistAngle/#maxTwistAngle. Must be positive. */
	Coefficient twistLimitDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Boolean properties of the joint. */
	JointFlags<N> flags{};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const BallJointOptions& other) const noexcept = default;
};
using BallJointOptions2D = BallJointOptions<2>; ///< Configuration options for a spherical joint in a 2-dimensional world.
using BallJointOptions3D = BallJointOptions<3>; ///< Configuration options for a spherical joint in a 3-dimensional world.

/**
 * Configuration options for a prismatic joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct PrismaticJointOptions {
	/** Local offsets of the attachment frames of the connected objects. */
	Pair<Length<N>> attachmentOffsets{};

	/** Local orientations of the attachment frames of the connected objects. */
	Pair<Orientation<N>> attachmentOrientations{};

	/** Stiffness of the prismatic joint when the attachment frame points are separated perpendicular to the joint axis. Must be non-negative. */
	Frequency linearAttachmentStiffness = DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS;

	/** Damping ratio of the prismatic joint when the attachment frame points are separated perpendicular to the joint axis. Must be positive. */
	Coefficient linearAttachmentDampingRatio = DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO;

	/** Stiffness of the prismatic joint when the attachment frame directions are separated. Must be non-negative. */
	Frequency angularAttachmentStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Damping ratio of the prismatic joint when the attachment frame directions are separated. Must be positive. */
	Coefficient angularAttachmentDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Target velocity of the drive. */
	LinearVelocity1D driveTargetVelocity{};

	/** Maximum force applied by the drive. */
	Force1D driveMaxForce{};

	/** Minimum slide offset of the prismatic joint. Must be less than or equal to #maxOffset. */
	Length1D minOffset = Length1D::MIN;

	/** Maximum slide offset of the prismatic joint. Must be greater than or equal to #minOffset. */
	Length1D maxOffset = Length1D::MAX;

	/** Stiffness of the prismatic joint when extended beyond #minOffset/#maxOffset. Must be non-negative. */
	Frequency linearLimitStiffness = DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS;

	/** Damping ratio of the prismatic joint when extended beyond #minOffset/#maxOffset. Must be positive. */
	Coefficient linearLimitDampingRatio = DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO;

	/** Target angle of the prismatic joint. */
	Angle restAngle{};

	/** Stiffness of the prismatic joint when extended beyond #restAngle. Must be non-negative. */
	Frequency angularLimitStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Damping ratio of the prismatic joint when extended beyond #restAngle. Must be positive. */
	Coefficient angularLimitDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Boolean properties of the joint. */
	JointFlags<N> flags{};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const PrismaticJointOptions& other) const noexcept = default;
};
using PrismaticJointOptions2D = PrismaticJointOptions<2>; ///< Configuration options for a prismatic joint in a 2-dimensional world.
using PrismaticJointOptions3D = PrismaticJointOptions<3>; ///< Configuration options for a prismatic joint in a 3-dimensional world.

/**
 * Configuration options for a cylindrical joint.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct CylinderJointOptions {
	/** Local offsets of the attachment frames of the connected objects. */
	Pair<Length<N>> attachmentOffsets{};

	/** Local orientations of the attachment frames of the connected objects. */
	Pair<Orientation<N>> attachmentOrientations{};

	/** Stiffness of the cylindrical joint when the attachment frame points are separated perpendicular to the joint axis. Must be non-negative. */
	Frequency linearAttachmentStiffness = DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS;

	/** Damping ratio of the cylindrical joint when the attachment frame points are separated perpendicular to the joint axis. Must be positive. */
	Coefficient linearAttachmentDampingRatio = DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO;

	/** Stiffness of the prismatic joint when the attachment frame directions are separated. Must be non-negative. */
	Frequency angularAttachmentStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Damping ratio of the prismatic joint when the attachment frame directions are separated. Must be positive. */
	Coefficient angularAttachmentDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Target velocity of the linear drive. */
	LinearVelocity1D linearDriveTargetVelocity{};

	/** Maximum force applied by the linear drive. */
	Force1D linearDriveMaxForce{};

	/** Minimum slide offset of the cylindrical joint. Must be less than or equal to #maxOffset. */
	Length1D minOffset = Length1D::MIN;

	/** Maximum slide offset of the cylindrical joint. Must be greater than or equal to #minOffset. */
	Length1D maxOffset = Length1D::MAX;

	/** Stiffness of the cylindrical joint when extended beyond #minOffset/#maxOffset. Must be non-negative. */
	Frequency linearLimitStiffness = DEFAULT_JOINT_LINEAR_LIMIT_STIFFNESS;

	/** Damping ratio of the cylindrical joint when extended beyond #minOffset/#maxOffset. Must be positive. */
	Coefficient linearLimitDampingRatio = DEFAULT_JOINT_LINEAR_LIMIT_DAMPING_RATIO;

	/** Target velocity of the angular drive. */
	AngularVelocity2D angularDriveTargetVelocity{};

	/** Maximum torque applied by the angular drive. */
	Torque2D angularDriveMaxTorque{};

	/** Minimum angle of the cylindrical joint. Must be less than or equal to #maxAngle. */
	Angle minAngle = Angle::MIN;

	/** Maximum angle of the cylindrical joint. Must be greater than or equal to #minAngle. */
	Angle maxAngle = Angle::MAX;

	/** Stiffness of the cylindrical joint when extended beyond #minAngle/maxAngle. Must be non-negative. */
	Frequency angularLimitStiffness = DEFAULT_JOINT_ANGULAR_LIMIT_STIFFNESS;

	/** Damping ratio of the cylindrical joint when extended beyond #minAngle/maxAngle. Must be positive. */
	Coefficient angularLimitDampingRatio = DEFAULT_JOINT_ANGULAR_LIMIT_DAMPING_RATIO;

	/** Boolean properties of the joint. */
	JointFlags<N> flags{};

	/**
	 * Compare these options to another set of options for equality.
	 *
	 * \param other the options to compare these to.
	 *
	 * \return true if the options are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const CylinderJointOptions& other) const noexcept = default;
};
using CylinderJointOptions2D = CylinderJointOptions<2>; ///< Configuration options for a cylindrical joint in a 2-dimensional world.
using CylinderJointOptions3D = CylinderJointOptions<3>; ///< Configuration options for a cylindrical joint in a 3-dimensional world.

} // namespace grem::physics

#endif
