// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_SIMULATION_HPP
#define GREM_PHYSICS_SIMULATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Thread.hpp>
#include <GREM/execution/EntityRegistry.hpp>
#include <GREM/execution/ResourceRegistry.hpp>
#include <GREM/execution/Schedule.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/Broadphase.hpp>
#include <GREM/physics/EntityID.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/joints.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include <type_traits> // std::conditional_t
#include <utility>     // std::move

namespace grem::execution {

class Executor; // Forward declaration, to avoid including Executor.hpp.

template <typename EntReg, typename ResReg>
class Scheduler; // Forward declaration, to avoid including Scheduler.hpp.

} // namespace grem::execution

namespace grem::physics {

template <size_t N>
class DebugVisualization; // Forward declaration, to avoid including DebugVisualization.hpp.

template <size_t N>
class Broadphase; // Forward declaration, to avoid including Broadphase.hpp.

/**
 * Configuration options for a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct SimulationOptions {
	/**
	 * Maximum supported number of contact colors.
	 */
	static constexpr size_t MAX_CONTACT_COLOR_COUNT = N * 4;

	/**
	 * Time delta to advance the state of the world by on each simulation step.
	 *
	 * \warning Must be positive.
	 */
	Time stepInterval = 1.0f / (60.0f * HERTZ);

	/**
	 * Assumed level of parallelism available when a simulation step is
	 * scheduled, and also later when the schedule is executed.
	 *
	 * \note Should roughly correspond to the maximum parallelism of the
	 *       executor that the simulation is executed on, which can be queried
	 *       with execution::Executor::getMaxParallelism().
	 */
	execution::Task::ParallelCount targetParallelism = static_cast<execution::Task::ParallelCount>(clamp(Thread::hardware_concurrency(), 2u, 32u) - 1);

	/**
	 * How many sub-steps of the constraint solver to run in the simulation step.
	 *
	 * \warning Must be positive.
	 */
	size_t subStepCount = 8;

	/**
	 * Number of contact colors to use for graph coloring in order to
	 * parallelize the solvers.
	 *
	 * \warning Must be less than or equal to #MAX_CONTACT_COLOR_COUNT.
	 */
	size_t contactColorCount = (targetParallelism >= 2) ? MAX_CONTACT_COLOR_COUNT : 0;

	/**
	 * Collision algorithm options.
	 */
	CollisionAlgorithmOptions<N> collisionAlgorithmOptions{};

	/**
	 * Maximum angle between the normals of the old and new versions of a
	 * contact manifold before they're considered to be different manifolds and
	 * the old accumulated contact momentums are discarded.
	 *
	 * \warning Must be non-negative.
	 * \warning Must be less than 90 degrees.
	 */
	Angle maxContactManifoldAngleTolerance = 20.0f * DEGREES;

	/**
	 * Approximation of the minimum width of the shapes of the objects that will
	 * be simulated (excluding point shapes).
	 *
	 * Objects smaller than this are allowed, but may cause worse performance.
	 *
	 * \warning Must be non-negative.
	 */
	Distance minObjectSizeApproximation = 0.01f * METERS;

	/**
	 * Linear speed at or below which objects are considered to be at rest.
	 *
	 * Going below this speed causes an object's activity level to drop until it
	 * is eventually frozen in place.
	 *
	 * \warning Must be non-negative.
	 */
	Speed maxRestingSpeed = 0.1f * METERS_PER_SECOND;

	/**
	 * Angular speed at or below which objects are considered to be at rest.
	 *
	 * Going below this speed causes an object's activity level to drop until it
	 * is eventually frozen in place.
	 *
	 * \warning Must be non-negative.
	 */
	AngularSpeed maxRestingAngularSpeed = 4.0f * DEGREES_PER_SECOND;

	/**
	 * Value of each energy level step represented by an object's
	 * ObjectActivity::energyLevel.
	 *
	 * This determines how the energy level is recalulated at the end of each
	 * simulation step.
	 *
	 * \warning Must be positive.
	 */
	Energy energyLevelUnit = 1.0f * JOULES;

	/**
	 * Safety coefficient to multiply the velocities of objects by when
	 * building their bounding volumes, which are used to detect potential
	 * collisions, for the full step.
	 *
	 * This aims to account for potential acceleration changes during the step.
	 *
	 * \warning Must be non-negative.
	 */
	Coefficient potentialCollisionVelocitySafetyCoefficient = 2.0f;

	/**
	 * How much of the contact constraint momentums from the last simulation
	 * step to preserve to the current step for the purpose of improving
	 * convergence in the constraint solver.
	 *
	 * \warning Must be between 0 and 1 (inclusive).
	 */
	Coefficient contactWarmstartCoefficient = 0.99f;

	/**
	 * How much of the joint constraint momentums from the last simulation step
	 * to preserve to the current step for the purpose of improving convergence
	 * in the constraint solver.
	 *
	 * \warning Must be between 0 and 1 (inclusive).
	 */
	Coefficient jointWarmstartCoefficient = 0.99f;

	/**
	 * Stiffness of contact constraints. Controls how fast overlap is corrected
	 * on the velocity level, at the cost of potential jitter.
	 *
	 * \warning Must be non-negative.
	 */
	Frequency contactStiffness = 60.0f * HERTZ;

	/**
	 * Damping ratio used when solving contact constraints.
	 *
	 * \warning Must be positive.
	 */
	Coefficient contactDampingRatio = 2.0f;

	/**
	 * Maximum linear momentum limit of joint constraints.
	 *
	 * \warning Must be positive.
	 */
	Momentum jointMaxMomentum = 50000.0f * KILOGRAM_METERS_PER_SECOND;

	/**
	 * Maximum angular momentum limit of joint constraints.
	 *
	 * \warning Must be positive.
	 */
	AngularMomentum2D jointMaxAngularMomentum = 50000.0f * KILOGRAM_SQUARE_METERS_PER_SECOND;

	/**
	 * Maximum momentum limit of contact constraints.
	 *
	 * \warning Must be positive.
	 */
	Momentum contactMaxMomentum = 50000.0f * KILOGRAM_METERS_PER_SECOND;
};
using SimulationOptions2D = SimulationOptions<2>; ///< Configuration options for a 2-dimensional Simulation.
using SimulationOptions3D = SimulationOptions<3>; ///< Configuration options for a 3-dimensional Simulation.

/**
 * Entity registry used by a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using EntityRegistry = std::conditional_t<N == 3,
	execution::EntityRegistry<              //
		Force3D,                            //
		Torque3D,                           //
		Position3D,                         //
		Orientation3D,                      //
		Scale3D,                            //
		LinearVelocity3D,                   //
		AngularVelocity3D,                  //
		LinearAcceleration3D,               //
		FluidDensity,                       //
		CenterOfBuoyancy3D,                 //
		Volume,                             //
		InverseMass,                        //
		InversePrincipalMomentsOfInertia3D, //
		LocalInertiaOrientation3D,          //
		MomentOfInertiaTensor3D,            //
		InverseMomentOfInertiaTensor3D,     //
		Collider3D,                         //
		Material,                           //
		ObjectFlags,                        //
		ObjectContacts3D,                   //
		ObjectBounds3D,                     //
		ObjectActivity,                     //
		ObjectActiveTag,                    //
		BroadphaseID,                       //
		JointConnectedObjects,              //
		JointAttachmentOffsets3D,           //
		JointAttachmentOrientations3D,      //
		JointLinearConstraint3D,            //
		JointLinearConstraintImpulses3D,    //
		JointDistanceConstraint3D,          //
		JointDistanceConstraintImpulses3D,  //
		JointAngularConstraint3D,           //
		JointAngularConstraintImpulses3D,   //
		JointConeConstraint3D,              //
		JointConeConstraintImpulses3D,      //
		JointTwistConstraint3D,             //
		JointTwistConstraintImpulses3D,     //
		JointFlags3D,                       //
		JointActiveTag>,                    //
	execution::EntityRegistry<              //
		Force2D,                            //
		Torque2D,                           //
		Position2D,                         //
		Orientation2D,                      //
		Scale2D,                            //
		LinearVelocity2D,                   //
		AngularVelocity2D,                  //
		LinearAcceleration2D,               //
		FluidDensity,                       //
		CenterOfBuoyancy2D,                 //
		Volume,                             //
		InverseMass,                        //
		InversePrincipalMomentsOfInertia2D, //
		LocalInertiaOrientation2D,          //
		MomentOfInertiaTensor2D,            //
		InverseMomentOfInertiaTensor2D,     //
		Collider2D,                         //
		Material,                           //
		ObjectFlags,                        //
		ObjectContacts2D,                   //
		ObjectBounds2D,                     //
		ObjectActivity,                     //
		ObjectActiveTag,                    //
		BroadphaseID,                       //
		JointConnectedObjects,              //
		JointAttachmentOffsets2D,           //
		JointAttachmentOrientations2D,      //
		JointLinearConstraint2D,            //
		JointLinearConstraintImpulses2D,    //
		JointDistanceConstraint2D,          //
		JointDistanceConstraintImpulses2D,  //
		JointAngularConstraint2D,           //
		JointAngularConstraintImpulses2D,   //
		JointFlags2D,                       //
		JointActiveTag>>;
using EntityRegistry2D = EntityRegistry<2>; ///< Entity registry used by a 2-dimensional Simulation.
using EntityRegistry3D = EntityRegistry<3>; ///< Entity registry used by a 3-dimensional Simulation.

/**
 * Resource registry used by a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using ResourceRegistry = execution::ResourceRegistry< //
	SimulationOptions<N>,                             //
	CollisionEvents<N>,                               //
	SeparationEvents<N>,                              //
	Broadphase<N>,                                    //
	Contacts<N>>;
using ResourceRegistry2D = ResourceRegistry<2>; ///< Resource registry used by a 2-dimensional Simulation.
using ResourceRegistry3D = ResourceRegistry<3>; ///< Resource registry used by a 3-dimensional Simulation.

/**
 * Entity builder used by a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using EntityBuilder = execution::EntityBuilder<EntityRegistry<N>>;
using EntityBuilder2D = EntityBuilder<2>; ///< Entity builder used by a 2-dimensional Simulation.
using EntityBuilder3D = EntityBuilder<3>; ///< Entity builder used by a 3-dimensional Simulation.

/**
 * Scheduler used by a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using Scheduler = execution::Scheduler<EntityRegistry<N>, ResourceRegistry<N>>;
using Scheduler2D = Scheduler<2>; ///< Scheduler used by a 2-dimensional Simulation.
using Scheduler3D = Scheduler<3>; ///< Scheduler used by a 3-dimensional Simulation.

/**
 * Schedule used by a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using Schedule = execution::Schedule<EntityRegistry<N>, ResourceRegistry<N>>;
using Schedule2D = Schedule<2>; ///< Schedule used by a 2-dimensional Simulation.
using Schedule3D = Schedule<3>; ///< Schedule used by a 3-dimensional Simulation.

/**
 * Options for scheduling a step of a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ScheduleStepOptions {
	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **before** updating the broadphase.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPreBroadphaseUpdateTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **after** updating the broadphase.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPostBroadphaseUpdateTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **before** collision detection.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPreCollisionDetectionTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **after** collision detection.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPostCollisionDetectionTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **before** any iterations of the constraint solver.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPreSolverTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **before** each iteration of the constraint solver.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPreSolverIterationTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **after** each iteration of the constraint solver.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPostSolverIterationTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **after** all iterations of the constraint solver.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPostSolverTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **before** applying contact restitution.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPreRestitutionTasks = [](Scheduler<N>&) -> void {
	};

	/**
	 * Callback for scheduling additional user-defined tasks to be executed
	 * **after** applying contact restitution.
	 */
	FunctionView<void(Scheduler<N>& scheduler)> addPostRestitutionTasks = [](Scheduler<N>&) -> void {
	};
};
using ScheduleStepOptions2D = ScheduleStepOptions<2>; ///< Options for scheduling a step of a 2-dimensional Simulation.
using ScheduleStepOptions3D = ScheduleStepOptions<3>; ///< Options for scheduling a step of a 3-dimensional Simulation.

/**
 * Discrete-time simulation of physical interactions between objects and joints.
 *
 * The simulation uses a right-handed coordinate system and SI units for any
 * physical quantities.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct Simulation {
	/**
	 * Add all resources required to run a simulation to a resource registry.
	 *
	 * \param resources resource registry to add the resources to.
	 * \param options initial configuration options of the simulation, see
	 *        SimulationOptions.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The added resources are:
	 *       - SimulationOptions<N>: Configuration options for the simulation.
	 *       - CollisionEvents<N>: Set of collision events that occured in the
	 *                             last simulation step.
	 *       - SeparationEvents<N>: Set of separation events that occured in the
	 *                              last simulation step.
	 *       - Broadphase<N>: An acceleration structure for spatial object
	 *                        queries.
	 *       - Contacts<N>: The set of contacts between objects in the
	 *                      simulation.
	 *       - Internal storage for implementation-specific mid-simulation-step
	 *         caches that are not user-accessible.
	 *
	 * \sa removeResources()
	 */
	GREM_API(physics) static void addRequiredResources(ResourceRegistry<N>& resources, const SimulationOptions<N>& options = {});

	/**
	 * Remove all resources required to run a simulation from a resource
	 * registry.
	 *
	 * \param resources resource registry to remove the resources from.
	 */
	GREM_API(physics) static void removeResources(ResourceRegistry<N>& resources) noexcept;

	/**
	 * Add all components required to create a simulated object to an entity in
	 * a registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param entityID handle to the entity to add the components to.
	 * \param options configuration of the object to create, see ObjectOptions.
	 *
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If any of the components to be added are already present, they are
	 *       left unmodified, except for InverseMass and
	 *       InversePrincipalMomentsOfInertia, which are recalculated if they
	 *       are infinite.
	 * \note The added components are:
	 *       - Force<N>: External force being applied to the object.
	 *       - Torque<N>: External torque being applied to the object.
	 *       - Position<N>: Position of the object in the world.
	 *       - Orientation<N>: Orientation of the object in the world.
	 *       - Scale<N>: Scale of the object in shape-local space.
	 *       - LinearVelocity<N>: Linear velocity of the object.
	 *       - AngularVelocity<N>: Angular velocity of the object.
	 *       - LinearAcceleration<N>: Gravity acceleration of the object.
	 *       - FluidDensity: Effective fluid density of the object's
	 *                       surroundings.
	 *       - CenterOfBuoyancy<N>: World-space offset of the center of buoyancy
	 *                              of the object relative to its center of
	 *                              mass.
	 *       - Volume: Unscaled volume of the object's shape.
	 *       - InverseMass: Reciprocal of the mass of the object, or 0 for
	 *                      infinite mass.
	 *       - InversePrincipalMomentsOfInertia<N>: Reciprocals of the principal
	 *                                              moments of inertia of the
	 *                                              object, or 0 for infinite
	 *                                              moment of inertia.
	 *       - LocalInertiaOrientation<N>: Local inertia orientation of the
	 *                                     object, rotating from inertia major
	 *                                     axis space to local space.
	 *       - MomentOfInertiaTensor<N>: Moment of inertia tensor of the object.
	 *       - InverseMomentOfInertiaTensor<N>: Inverse moment of inertia tensor
	 *                                          of the object.
	 *       - Collider<N>: Collider of the object.
	 *       - Material: Dynamics-related properties of the object.
	 *       - ObjectFlags: Boolean properties of the object.
	 *       - ObjectContacts<N>: Potential contacts of the object.
	 *       - ObjectBounds<N>: Bounds of the object in the broadphase.
	 *       - ObjectActivity: Activity information of the object.
	 *       - ObjectActiveTag: Tag type that is added in a simulation step if
	 *                          the object's energy level is non-zero.
	 *       - BroadphaseID: Handle to the object in the broadphase, which may
	 *                       be null if the object has not been inserted yet.
	 *
	 * \sa addRequiredResources()
	 * \sa removeObjectComponents()
	 */
	GREM_API(physics) static void addObjectComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, ObjectOptions<N>&& options);

	/**
	 * Remove all components added by addObjectComponents() from an entity in a
	 * registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param entityID handle to the entity to remove the components from.
	 *
	 * \sa addObjectComponents()
	 */
	GREM_API(physics) static void removeObjectComponents(EntityRegistry<N>& registry, EntityID entityID) noexcept;

	/**
	 * Update the MomentOfInertiaTensor<N> and InverseMomentOfInertiaTensor<N>
	 * components of an object given its current orientation in the world.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param objectID handle to the object entity to update the moment of
	 *        inertia tensor of.
	 *
	 * \sa addRequiredResources()
	 */
	GREM_API(physics) static void updateObjectMomentOfInertiaTensor(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID objectID);

	/**
	 * Update the ObjectBounds<N> component of an object given its current
	 * shape, local center of mass, position and orientation in the world, while
	 * accounting for one step of potential movement given the object's current
	 * velocitiy and the step interval of the simulation.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param objectID handle to the object entity to update the bounds of.
	 *
	 * \note This function automatically sets the object's broadphase handle to
	 *       null to indicate that it needs to be reinserted into the
	 *       broadphase on the next broadphase update.
	 *
	 * \sa addRequiredResources()
	 * \sa updateBroadphase()
	 */
	GREM_API(physics) static void updateObjectBounds(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID objectID);

	/**
	 * Update the broadphase structure of a simulation by inserting all objects
	 * with a broadphase handle of null to reflect their new bounds, and
	 * cleaning up any references to objects that no longer exist.
	 *
	 * \param registry entity registry containing the objects of the world.
	 * \param resources resource registry containing the broadphase to update.
	 *        Must contain all resources added by addRequiredResources().
	 *
	 * \note A broadphase update is performed automatically at the start of each
	 *       simulation step, but it may need to be done manually if objects are
	 *       updated after a simulation step, and the application depends on the
	 *       broadphase being up-to-date for those objects at some point before
	 *       the next step is performed.
	 *
	 * \sa addRequiredResources()
	 * \sa scheduleBroadphaseUpdate()
	 */
	GREM_API(physics) static void updateBroadphase(EntityRegistry<N>& registry, ResourceRegistry<N>& resources);

	/**
	 * Add all components required to create a simulated generic joint that
	 * connects two objects to an entity in a registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param entityID handle to the entity to add the components to.
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        GenericJointOptions.
	 *
	 * \throws physics::Error if the object IDs are not valid for the given
	 *         registry.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If any of the components to be added are already present, they are
	 *       left unmodified.
	 * \note The components of the joint entity are:
	 *       - JointConnectedObjects: Ordered pair of handles to the connected
	 *                                objects.
	 *       - JointAttachmentOffsets<N>: Local offsets of the attachment frames
	 *                                    of the connected objects.
	 *       - JointAttachmentOrientations<N>: Local orientations of the
	 *                                         attachment frames of the
	 *                                         connected objects.
	 *       - JointLinearConstraint<N>: Linear constraint, added if GenericJointOptions<N>::linearConstraint
	 *                                   is set.
	 *       - JointLinearConstraintImpulses<N>: Impulses applied by the linear
	 *                                           constraint, added if GenericJointOptions<N>::linearConstraint
	 *                                           is set.
	 *       - JointDistanceConstraint<N>: Distance constraint, added if GenericJointOptions<N>::distanceConstraint
	 *                                     is set.
	 *       - JointDistanceConstraintImpulses<N>: Impulses applied by the
	 *                                             distance constraint, added if
	 *                                             GenericJointOptions<N>::distanceConstraint
	 *                                             is set.
	 *       - JointAngularConstraint<N>: Angular constraint, added if GenericJointOptions<N>::angularConstraint
	 *                                    is set.
	 *       - JointAngularConstraintImpulses<N>: Impulses applied by the
	 *                                            angular constraint, added if GenericJointOptions<N>::angularConstraint
	 *                                            is set.
	 *       - JointConeConstraint3D: Cone constraint, added if GenericJointOptions3D::coneConstraint
	 *                                is set.
	 *       - JointConeConstraintImpulses3D: Impulses applied by the cone
	 *                                        constraint, added if GenericJointOptions3D::coneConstraint
	 *                                        is set.
	 *       - JointTwistConstraint3D: Twist constraint, added if GenericJointOptions3D::twistConstraint
	 *                                 is set.
	 *       - JointTwistConstraintImpulses3D: Impulses applied by the twist
	 *                                         constraint, added if GenericJointOptions3D::twistConstraint
	 *                                         is set.
	 *       - JointFlags<N>: Boolean properties of the joint.
	 *       - JointActiveTag: Tag type that indicates that the joint is active.
	 *
	 * \sa addRequiredResources()
	 * \sa addWeldComponents()
	 * \sa addHingeJointComponents()
	 * \sa addBallJointComponents()
	 * \sa addPrismaticJointComponents()
	 * \sa addCylinderJointComponents()
	 * \sa removeJointComponents()
	 */
	GREM_API(physics)
	static void addGenericJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
		const GenericJointOptions<N>& options);

	/**
	 * Add all components required to create a simulated weld joint that
	 * restricts all motion between two objects to an entity in a registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param entityID handle to the entity to add the components to.
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see WeldOptions.
	 *
	 * \throws physics::Error if the object IDs are not valid for the given
	 *         registry.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If any of the components to be added are already present, they are
	 *       left unmodified.
	 *
	 * \sa addRequiredResources()
	 * \sa addGenericJointComponents()
	 * \sa removeJointComponents()
	 */
	GREM_API(physics)
	static void addWeldComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
		const WeldOptions<N>& options = {});

	/**
	 * Add all components required to create a simulated hinge joint that
	 * restricts the linear motion between and aligns the X axes of two objects
	 * to an entity in a registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param entityID handle to the entity to add the components to.
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        HingeJointOptions.
	 *
	 * \throws physics::Error if the object IDs are not valid for the given
	 *         registry.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If any of the components to be added are already present, they are
	 *       left unmodified.
	 *
	 * \sa addRequiredResources()
	 * \sa addGenericJointComponents()
	 * \sa removeJointComponents()
	 */
	GREM_API(physics)
	static void addHingeJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
		const HingeJointOptions<N>& options = {});

	/**
	 * Add all components required to create a simulated ball joint that
	 * restricts the linear motion between two objects to an entity in a
	 * registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param entityID handle to the entity to add the components to.
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        BallJointOptions.
	 *
	 * \throws physics::Error if the object IDs are not valid for the given
	 *         registry.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If any of the components to be added are already present, they are
	 *       left unmodified.
	 *
	 * \sa addRequiredResources()
	 * \sa addGenericJointComponents()
	 * \sa removeJointComponents()
	 */
	GREM_API(physics)
	static void addBallJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
		const BallJointOptions<N>& options = {});

	/**
	 * Add all components required to create a simulated prismatic joint that
	 * restricts the angular motion between and aligns the X and Y axes of two
	 * objects to an entity in a registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param entityID handle to the entity to add the components to.
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        PrismaticJointOptions.
	 *
	 * \throws physics::Error if the object IDs are not valid for the given
	 *         registry.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If any of the components to be added are already present, they are
	 *       left unmodified.
	 *
	 * \sa addRequiredResources()
	 * \sa addGenericJointComponents()
	 * \sa removeJointComponents()
	 */
	GREM_API(physics)
	static void addPrismaticJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
		const PrismaticJointOptions<N>& options = {});

	/**
	 * Add all components required to create a simulated cylindrical joint that
	 * restricts the angular motion between and aligns the X axes of two objects
	 * to an entity in a registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param resources resource registry of the simulation. Must contain all
	 *        resources added by addRequiredResources().
	 * \param entityID handle to the entity to add the components to.
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        CylinderJointOptions.
	 *
	 * \throws physics::Error if the object IDs are not valid for the given
	 *         registry.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If any of the components to be added are already present, they are
	 *       left unmodified.
	 *
	 * \sa addRequiredResources()
	 * \sa addGenericJointComponents()
	 * \sa removeJointComponents()
	 */
	GREM_API(physics)
	static void addCylinderJointComponents(EntityRegistry<N>& registry, const ResourceRegistry<N>& resources, EntityID entityID, Pair<EntityID> objectIDs,
		const CylinderJointOptions<N>& options = {});

	/**
	 * Remove all components added by addGenericJointComponents() from an entity
	 * in a registry.
	 *
	 * \param registry entity registry containing the given entity.
	 * \param entityID handle to the entity to remove the components from.
	 *
	 * \sa addGenericJointComponents()
	 * \sa addWeldComponents()
	 * \sa addHingeJointComponents()
	 * \sa addBallJointComponents()
	 * \sa addPrismaticJointComponents()
	 * \sa addCylinderJointComponents()
	 */
	GREM_API(physics) static void removeJointComponents(EntityRegistry<N>& registry, EntityID entityID) noexcept;

	/**
	 * Schedule a full step of the physics simulation.
	 *
	 * \param scheduler scheduler to schedule the simulation step on.
	 * \param simulationOptions configuration options of the simulation that the
	 *        schedule will be executed on.
	 * \param scheduleStepOptions step scheduling options, see
	 *        ScheduleStepOptions.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The given schedule should typically be executed once every tick
	 *       during the application::Application::tick() callback, using the
	 *       application's tick interval as the step interval.
	 * \note The given `simulationOptions` must match the current simulation
	 *       options when the schedule is executed.
	 *
	 * \warning A schedule containing a simulation step can only be executed
	 *          given a resource registry containing all of the required
	 *          resources added by addRequiredResources().
	 *
	 * \sa addRequiredResources()
	 */
	GREM_API(physics) static void scheduleStep(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions, const ScheduleStepOptions<N>& scheduleStepOptions = {});

	/**
	 * Schedule a standalone broadphase update pass, separate from a full
	 * simulation step, that updates the broadphase for any objects whose
	 * broadphase handle is null.
	 *
	 * \param scheduler scheduler to schedule the simulation step on.
	 * \param simulationOptions configuration options of the simulation that the
	 *        schedule will be executed on.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note All object bounds should be up-to-date before executing a
	 *       broadphase update.
	 *
	 * \warning A schedule containing a broadphase update can only be executed
	 *          given a resource registry containing all of the required
	 *          resources added by addRequiredResources().
	 *
	 * \sa addRequiredResources()
	 * \sa updateBroadphase()
	 */
	GREM_API(physics) static void scheduleBroadphaseUpdate(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions);

	/**
	 * Schedule a standalone collision detection pass, separate from a full
	 * simulation step, that updates the set of collisions between all objects
	 * without emitting any events.
	 *
	 * \param scheduler scheduler to schedule the simulation step on.
	 * \param simulationOptions configuration options of the simulation that the
	 *        schedule will be executed on.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note A broadphase update should be scheduled before collision detection
	 *       for accurate results.
	 *
	 * \warning A schedule containing a simulation step can only be executed
	 *          given a resource registry containing all of the required
	 *          resources added by addRequiredResources().
	 *
	 * \sa addRequiredResources()
	 */
	GREM_API(physics) static void scheduleCollisionDetection(Scheduler<N>& scheduler, const SimulationOptions<N>& simulationOptions);

	/**
	 * Get the debug visualization of the current simulation state.
	 *
	 * \param debugVisualization debug visualization to append the draw
	 *        commands to.
	 * \param registry entity registry containing the entities involved in the
	 *        simulation.
	 * \param resources resource registry containing the resources required by
	 *        the simulation.
	 *
	 * \sa addRequiredResources()
	 */
	GREM_API(physics) static void drawDebugVisualization(DebugVisualization<N>& debugVisualization, const EntityRegistry<N>& registry, const ResourceRegistry<N>& resources);

	EntityRegistry<N> registry{};    ///< Entity registry of the simulation.
	ResourceRegistry<N> resources{}; ///< Resource registry of the simulation.
	Schedule<N> stepSchedule{};      ///< Schedule for performing one step of the physics simulation.

	/**
	 * Construct a physics simulation with all required resources added, using
	 * the default SimulationOptions.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Simulation()
		: Simulation(SimulationOptions<N>{}) {}

	/**
	 * Construct a physics simulation with all required resources added.
	 *
	 * \param options initial configuration options of the simulation, see
	 *        SimulationOptions.
	 * \param scheduleStepOptions step scheduling options, see
	 *        ScheduleStepOptions.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(physics) explicit Simulation(const SimulationOptions<N>& options, const ScheduleStepOptions<N>& scheduleStepOptions = {});

	/**
	 * Create a new simulated object entity and add it to the simulation.
	 *
	 * \param options configuration of the object to create, see ObjectOptions.
	 * \param flags user-defined flags to add to the ID of the created entity.
	 *
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \return an entity builder for building the object entity. To finish
	 *         creating the object, call EntityBuilder::build().
	 *
	 * \note See addObjectComponents() for a list of components added to the new
	 *       entity, which can be accessed through the simulation's #registry
	 *       member.
	 * \note To destroy the object, call `registry.destroyEntity(entityID)` on
	 *       the #registry of the simulation, where `entityID` is the entity
	 *       handle returned from EntityBuilder::build().
	 */
	[[nodiscard]] EntityBuilder<N> createObject(ObjectOptions<N>&& options, EntityID::Flags flags = {}) {
		EntityBuilder<N> entityBuilder = registry.createEntity(flags);
		entityBuilder.extend([&](EntityRegistry<N>& r, EntityID entityID) { addObjectComponents(r, resources, entityID, std::move(options)); });
		return entityBuilder;
	}

	/**
	 * Update the MomentOfInertiaTensor<N> and InverseMomentOfInertiaTensor<N>
	 * components of an object given its current orientation in the world.
	 *
	 * \param objectID handle to the object entity to update the moment of
	 *        inertia tensor of.
	 */
	void updateObjectMomentOfInertiaTensor(EntityID objectID) {
		updateObjectMomentOfInertiaTensor(registry, resources, objectID);
	}

	/**
	 * Update the ObjectBounds<N> component of an object given its current
	 * shape, local center of mass, position and orientation in the world, while
	 * accounting for one step of potential movement given the object's current
	 * velocitiy and the step interval of the simulation.
	 *
	 * \param objectID handle to the object entity to update the bounds of.
	 *
	 * \note This function automatically sets the object's broadphase handle to
	 *       null to indicate that it needs to be reinserted into the
	 *       broadphase on the next broadphase update.
	 *
	 * \sa updateBroadphase()
	 */
	void updateObjectBounds(EntityID objectID) {
		updateObjectBounds(registry, resources, objectID);
	}

	/**
	 * Update the broadphase structure of a simulation by inserting all objects
	 * with a broadphase handle of null to reflect their new bounds, and
	 * cleaning up any references to objects that no longer exist.
	 *
	 * \note A broadphase update is performed automatically at the start of each
	 *       simulation step, but it may need to be done manually if objects are
	 *       updated after a simulation step, and the application depends on the
	 *       broadphase being up-to-date for those objects at some point before
	 *       the next step is performed.
	 */
	void updateBroadphase() {
		updateBroadphase(registry, resources);
	}

	/**
	 * Create a new simulated generic joint entity that connects two objects.
	 *
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        GenericJointOptions.
	 * \param flags user-defined flags to add to the ID of the created entity.
	 *
	 * \return an entity builder for building the joint entity. To finish
	 *         creating the joint, call EntityBuilder::build().
	 *
	 * \throws physics::Error if the object IDs are not valid for this
	 *         simulation.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note See addGenericJointComponents() for a list of components added to
	 *       the new entity, which can be accessed through the simulation's
	 *       #registry member.
	 * \note To destroy the joint, call `registry.destroyEntity(entityID)` on
	 *       the #registry of the simulation, where `entityID` is the entity
	 *       handle returned from EntityBuilder::build().
	 *
	 * \sa createWeld()
	 * \sa createHingeJoint()
	 * \sa createBallJoint()
	 * \sa createPrismaticJoint()
	 * \sa createCylinderJoint()
	 */
	[[nodiscard]] EntityBuilder<N> createGenericJoint(Pair<EntityID> objectIDs, const GenericJointOptions<N>& options, EntityID::Flags flags = {}) {
		EntityBuilder<N> entityBuilder = registry.createEntity(flags);
		entityBuilder.extend([&](EntityRegistry<N>& r, EntityID entityID) { addGenericJointComponents(r, resources, entityID, objectIDs, options); });
		return entityBuilder;
	}

	/**
	 * Create a new simulated weld joint entity that restricts all motion
	 * between two objects.
	 *
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see WeldOptions.
	 * \param flags user-defined flags to add to the ID of the created entity.
	 *
	 * \return an entity builder for building the joint entity. To finish
	 *         creating the joint, call EntityBuilder::build().
	 *
	 * \throws physics::Error if the object IDs are not valid for this
	 *         simulation.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note To destroy the joint, call `registry.destroyEntity(entityID)` on
	 *       the #registry of the simulation, where `entityID` is the entity
	 *       handle returned from EntityBuilder::build().
	 *
	 * \sa createGenericJoint()
	 */
	[[nodiscard]] EntityBuilder<N> createWeld(Pair<EntityID> objectIDs, const WeldOptions<N>& options = {}, EntityID::Flags flags = {}) {
		EntityBuilder<N> entityBuilder = registry.createEntity(flags);
		entityBuilder.extend([&](EntityRegistry<N>& r, EntityID entityID) { addWeldComponents(r, resources, entityID, objectIDs, options); });
		return entityBuilder;
	}

	/**
	 * Create a new simulated hinge joint entity that restricts the linear
	 * motion between and aligns the X axes of two objects.
	 *
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        HingeJointOptions.
	 * \param flags user-defined flags to add to the ID of the created entity.
	 *
	 * \return an entity builder for building the joint entity. To finish
	 *         creating the joint, call EntityBuilder::build().
	 *
	 * \throws physics::Error if the object IDs are not valid for this
	 *         simulation.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note To destroy the joint, call `registry.destroyEntity(entityID)` on
	 *       the #registry of the simulation, where `entityID` is the entity
	 *       handle returned from EntityBuilder::build().
	 *
	 * \sa createGenericJoint()
	 */
	[[nodiscard]] EntityBuilder<N> createHingeJoint(Pair<EntityID> objectIDs, const HingeJointOptions<N>& options = {}, EntityID::Flags flags = {}) {
		EntityBuilder<N> entityBuilder = registry.createEntity(flags);
		entityBuilder.extend([&](EntityRegistry<N>& r, EntityID entityID) { addHingeJointComponents(r, resources, entityID, objectIDs, options); });
		return entityBuilder;
	}

	/**
	 * Create a new simulated ball joint entity that restricts the linear motion
	 * between two objects.
	 *
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        BallJointOptions.
	 * \param flags user-defined flags to add to the ID of the created entity.
	 *
	 * \return an entity builder for building the joint entity. To finish
	 *         creating the joint, call EntityBuilder::build().
	 *
	 * \throws physics::Error if the object IDs are not valid for this
	 *         simulation.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note To destroy the joint, call `registry.destroyEntity(entityID)` on
	 *       the #registry of the simulation, where `entityID` is the entity
	 *       handle returned from EntityBuilder::build().
	 *
	 * \sa createGenericJoint()
	 */
	[[nodiscard]] EntityBuilder<N> createBallJoint(Pair<EntityID> objectIDs, const BallJointOptions<N>& options = {}, EntityID::Flags flags = {}) {
		EntityBuilder<N> entityBuilder = registry.createEntity(flags);
		entityBuilder.extend([&](EntityRegistry<N>& r, EntityID entityID) { addBallJointComponents(r, resources, entityID, objectIDs, options); });
		return entityBuilder;
	}

	/**
	 * Create a new simulated prismatic joint entity that restricts the angular
	 * motion between and aligns the X and Y axes of two objects.
	 *
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        PrismaticJointOptions.
	 * \param flags user-defined flags to add to the ID of the created entity.
	 *
	 * \return an entity builder for building the joint entity. To finish
	 *         creating the joint, call EntityBuilder::build().
	 *
	 * \throws physics::Error if the object IDs are not valid for this
	 *         simulation.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note To destroy the joint, call `registry.destroyEntity(entityID)` on
	 *       the #registry of the simulation, where `entityID` is the entity
	 *       handle returned from EntityBuilder::build().
	 *
	 * \sa createGenericJoint()
	 */
	[[nodiscard]] EntityBuilder<N> createPrismaticJoint(Pair<EntityID> objectIDs, const PrismaticJointOptions<N>& options = {}, EntityID::Flags flags = {}) {
		EntityBuilder<N> entityBuilder = registry.createEntity(flags);
		entityBuilder.extend([&](EntityRegistry<N>& r, EntityID entityID) { addPrismaticJointComponents(r, resources, entityID, objectIDs, options); });
		return entityBuilder;
	}

	/**
	 * Create a new simulated cylindrical joint that restricts the angular
	 * motion between and aligns the X axes of two objects.
	 *
	 * \param objectIDs ordered pair of handles to the two object entities to
	 *        connect with the joint.
	 * \param options configuration of the joint to create, see
	 *        CylinderJointOptions.
	 * \param flags user-defined flags to add to the ID of the created entity.
	 *
	 * \return an entity builder for building the joint entity. To finish
	 *         creating the joint, call EntityBuilder::build().
	 *
	 * \throws physics::Error if the object IDs are not valid for this
	 *         simulation.
	 * \throws execution::Error on failure to add the components.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note To destroy the joint, call `registry.destroyEntity(entityID)` on
	 *       the #registry of the simulation, where `entityID` is the entity
	 *       handle returned from EntityBuilder::build().
	 *
	 * \sa createGenericJoint()
	 */
	[[nodiscard]] EntityBuilder<N> createCylinderJoint(Pair<EntityID> objectIDs, const CylinderJointOptions<N>& options = {}, EntityID::Flags flags = {}) {
		EntityBuilder<N> entityBuilder = registry.createEntity(flags);
		entityBuilder.extend([&](EntityRegistry<N>& r, EntityID entityID) { addCylinderJointComponents(r, resources, entityID, objectIDs, options); });
		return entityBuilder;
	}

	/**
	 * Perform a step of the physics simulation.
	 *
	 * \param executor executor to execute the step schedule on.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(physics) void step(execution::Executor& executor);

	/**
	 * Perform a step of the physics simulation using a single-threaded
	 * executor.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(physics) void step();

	/**
	 * Get the debug visualization of the current simulation state.
	 *
	 * \param debugVisualization debug visualization to append the draw
	 *        commands to.
	 */
	void drawDebugVisualization(DebugVisualization<N>& debugVisualization) const {
		drawDebugVisualization(debugVisualization, registry, resources);
	}
};

extern template struct Simulation<2>;
extern template struct Simulation<3>;

/**
 * Discrete-time simulation of physical interactions between objects and joints
 * in 2-dimensional space.
 *
 * The simulation uses a right-handed coordinate system and SI units for any
 * physical quantities.
 */
using Simulation2D = Simulation<2>;

/**
 * Discrete-time simulation of physical interactions between objects and joints
 * in 3-dimensional space.
 *
 * The simulation uses a right-handed coordinate system and SI units for any
 * physical quantities.
 */
using Simulation3D = Simulation<3>;

} // namespace grem::physics

#endif
