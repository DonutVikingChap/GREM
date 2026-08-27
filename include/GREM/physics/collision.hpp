// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_COLLISION_HPP
#define GREM_PHYSICS_COLLISION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Any.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/BitArray.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/physics/EntityID.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/quantities.hpp>

#include <type_traits> // std::is_nothrow_move_constructible_v
#include <utility>     // std::move

namespace grem::physics {

namespace detail {

struct ContactsKeyHash {
	[[nodiscard]] size_t operator()(const Pair<EntityID>& key) const {
		return getHash(key.first, key.second);
	}
};

} // namespace detail

/**
 * Type of a potentially colliding feature in a contact between two objects.
 */
enum class ContactFeatureType : uint8_t {
	/**
	 * Generic convex surface.
	 *
	 * Does not have a corresponding feature index.
	 */
	GENERIC_CONVEX_SURFACE,

	/**
	 * Face on a convex polytope shape.
	 *
	 * Its feature index is the face index in the convex polytope.
	 */
	CONVEX_POLYTOPE_FACE,

	/**
	 * Edge on a convex polytope shape.
	 *
	 * Its feature index is the edge index in the convex polytope.
	 */
	CONVEX_POLYTOPE_EDGE,

	/**
	 * Face on a triangle mesh shape.
	 *
	 * Its feature index is the face index in the triangle mesh.
	 */
	TRIANGLE_MESH_FACE,

	/**
	 * Edge on a triangle mesh shape.
	 *
	 * Its feature index is the index of the first vertex index of the edge in
	 * the triangle mesh.
	 */
	TRIANGLE_MESH_EDGE,
};

/**
 * Collision start event between two objects in a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct CollisionEvent {
	/**
     * Ordered pair of handles to the objects involved in the collision.
     */
	Pair<EntityID> objectIDs;

	/**
     * Ordered pair of transformations of the objects in world space
	 * at the time of the collision.
     */
	Pair<Transformation<N>> objectTransformations;

	/**
     * Ordered pair of untransformed offsets from the objects' centers of mass
	 * to their respective surfaces where the collision occured in shape-local
	 * space.
     */
	Pair<Length<N>> localOffsets;

	/**
     * Ordered pair of linear velocities of the objects' centers of mass at the
     * time of the collision.
     */
	Pair<LinearVelocity<N>> objectLinearVelocities;

	/**
     * Ordered pair of angular velocities of the objects at the time of the
     * collision.
     */
	Pair<AngularVelocity<N>> objectAngularVelocities;

	/**
     * Unit vector perpendicular to the separating plane of the collision,
     * pointing away from the first object's surface and towards the second
	 * object's surface.
     */
	Direction<N> normal;
};
using CollisionEvent2D = CollisionEvent<2>; ///< Collision start event between two objects in a 2-dimensional Simulation.
using CollisionEvent3D = CollisionEvent<3>; ///< Collision start event between two objects in a 3-dimensional Simulation.

/**
 * List of collision start events between the objects in a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct CollisionEvents : ArrayList<CollisionEvent<N>> {};
using CollisionEvents2D = CollisionEvents<2>; ///< List of collision start events between the objects in a 2-dimensional Simulation.
using CollisionEvents3D = CollisionEvents<3>; ///< List of collision start events between the objects in a 3-dimensional Simulation.

/**
 * Collision end event between two objects in a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct SeparationEvent {
	/**
     * Ordered pair of handles to the objects that were previously in contact.
     */
	Pair<EntityID> objectIDs;

	/**
     * Ordered pair of transformations of the objects in world space at the time
	 * of the separation.
     */
	Pair<Transformation<N>> objectTransformations;

	/**
     * Ordered pair of linear velocities of the objects' centers of mass at the
     * time of the separation.
     */
	Pair<LinearVelocity<N>> objectLinearVelocities;

	/**
     * Ordered pair of angular velocities of the objects at the time of the
     * separation.
     */
	Pair<AngularVelocity<N>> objectAngularVelocities;
};
using SeparationEvent2D = SeparationEvent<2>; ///< Collision end event between two objects in a 2-dimensional Simulation.
using SeparationEvent3D = SeparationEvent<3>; ///< Collision end event between two objects in a 3-dimensional Simulation.

/**
 * List of collision end events between the objects in a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct SeparationEvents : ArrayList<SeparationEvent<N>> {};
using SeparationEvents2D = SeparationEvents<2>; ///< List of collision end events between the objects in a 2-dimensional Simulation.
using SeparationEvents3D = SeparationEvents<3>; ///< List of collision end events between the objects in a 3-dimensional Simulation.

/**
 * Contact point between two potentially colliding objects in a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ContactPoint {
	/**
     * Ordered pair of offsets from the objects' centers of mass to their
	 * respective surfaces where the contact point is located in world space at
	 * the start of the last simulation step.
     */
	Pair<Length<N>> offsets;

	/**
     * Ordered pair of offsets from the objects' centers of mass to their
	 * respective surfaces where the contact point is located in shape-local
	 * space at the time of impact.
     */
	Pair<Length<N>> localOffsets;

	/**
     * Unit vector in the separating plane of the collision, pointing in the
	 * direction of the objects' relative velocity at the contact point at the
	 * time of impact, projected onto the separating plane, in world space.
	 *
	 * \note If collision detection was performed outside of a simulation step,
	 *       the tangent will be zero.
     */
	Scale<N> tangent{};

	/**
	 * %Relative velocity of the contact points along the tangent, bitangent and
	 * normal (TBN) directions at the start of the last simulation step (or
	 * along the tangent and normal directions in 2D).
	 *
	 * \note If the contact was inactive in the last simulation step, this value
	 *       will not be up-to-date.
	 */
	LinearVelocity<N> relativeVelocityInTangentSpace{};

	/**
	 * Total correction momentum of this contact point along the tangent,
	 * bitangent and normal (TBN) directions (or along the tangent and normal
	 * directions in 2D).
	 *
	 * \note If the contact was inactive in the last simulation step, this value
	 *       will not be up-to-date.
	 */
	LinearMomentum<N> momentumInTangentSpace{};

	/**
	 * Total correction impulse applied to this contact point along the tangent,
	 * bitangent and normal (TBN) directions in the last simulation step (or
	 * along the tangent and normal directions in 2D).
	 *
	 * \note If the contact was inactive in the last simulation step, this value
	 *       will not be up-to-date.
	 */
	LinearImpulse<N> impulseInTangentSpace{};

	/**
	 * Total restitution momentum of this contact point along the normal
	 * direction.
	 *
	 * \note If the contact was inactive in the last simulation step, this value
	 *       will not be up-to-date.
	 */
	LinearMomentum1D restitutionMomentum{};

	/**
	 * Total restitution impulse applied to this contact point along the normal
	 * direction in the last simulation step.
	 *
	 * \note If the contact was inactive in the last simulation step, this value
	 *       will not be up-to-date.
	 */
	LinearImpulse1D restitutionImpulse{};

	/**
	 * Get the tangent-space basis of this contact point given the manifold
	 * normal.
	 *
	 * \param normal world-space normal of the contact manifold that contains
	 *        this point.
	 *
	 * \return the tangent-space basis in world space as a TBN matrix (tangent,
	 *         bitangent, normal) in 3D or a TN matrix (tangent, normal) in 2D.
	 */
	[[nodiscard]] OrthonormalBasis<N> getTangentSpaceBasis(Direction<N> normal) const {
		if constexpr (N == 3) {
			const Scale3D bitangent = cross(normal, tangent);
			return OrthonormalBasis3D::reinterpret(Basis3D{tangent, bitangent, normal});
		} else {
			return OrthonormalBasis2D::reinterpret(Basis2D{tangent, normal});
		}
	}
};
using ContactPoint2D = ContactPoint<2>; ///< Contact point between two potentially colliding objects in a 2-dimensional Simulation.
using ContactPoint3D = ContactPoint<3>; ///< Contact point between two potentially colliding objects in a 3-dimensional Simulation.

/**
 * Set of points on the contact surface between two potentially colliding objects.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ContactManifold {
	/**
	 * Maximum number of contact points in a single manifold.
	 */
	static constexpr size_t MAX_POINT_COUNT = (N == 3) ? 4 : 2;

	/**
	 * Bitset of point indices in a manifold.
	 */
	using PointMask = BitArray<MAX_POINT_COUNT>;

	/**
	 * Ordered pair of feature types in contact on the objects.
	 *
	 * \sa #featureIndices
	 */
	Pair<ContactFeatureType> featureTypes;

	/**
	 * Ordered pair of feature indices in contact on the objects, or an
	 * unspecified value if there is no specific index given the corresponding
	 * feature type.
	 *
	 * \sa #featureTypes
	 */
	Pair<uint32_t> featureIndices;

	/**
	 * Set of active contact points.
	 */
	InplaceArrayList<ContactPoint<N>, MAX_POINT_COUNT> points;

	/**
     * Unit vector perpendicular to the separating plane of the collision,
	 * pointing away from the surface of the first object and towards the
	 * surface of the second object, in world space.
     */
	Direction<N> normal;

	/**
	 * Total rolling resistance momentum of this contact manifold in world
	 * space.
	 *
	 * \note If the contact was inactive in the last simulation step, this value
	 *       will not be up-to-date.
	 */
	AngularMomentum<N> rollingResistanceMomentum{};

	/**
	 * Total rolling resistance impulse applied to this contact manifold in
	 * world space in the last simulation step.
	 *
	 * \note If the contact was inactive in the last simulation step, this value
	 *       will not be up-to-date.
	 */
	AngularImpulse<N> rollingResistanceImpulse{};

	/**
	 * Result of the collision filter test that generated the manifold.
	 */
	CollisionFilterTestResult filterTestResult;
};
using ContactManifold2D = ContactManifold<2>; ///< Set of points on the contact surface between two potentially colliding 2-dimensional objects.
using ContactManifold3D = ContactManifold<3>; ///< Set of points on the contact surface between two potentially colliding 3-dimensional objects.

/**
 * Result of a collision algorithm.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct CollisionAlgorithmResult {
	/**
	 * Contact manifold that was produced.
	 */
	ContactManifold<N> manifold;
};
using CollisionAlgorithmResult2D = CollisionAlgorithmResult<2>; ///< Result of a 2-dimensional collision algorithm.
using CollisionAlgorithmResult3D = CollisionAlgorithmResult<3>; ///< Result of a 3-dimensional collision algorithm.

/**
 * Configuration options for a collision algorithm.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct CollisionAlgorithmOptions {
	/**
	 * Maximum error in collision distance to allow in favor of improved
	 * performance and/or stability.
	 *
	 * \warning Must be greater than or equal to Distance::MACHINE_EPSILON.
	 */
	Distance collisionDistanceErrorTolerance = 0.0001f * METERS;

	/**
	 * Maximum separation distance of a contact point before it is considered to
	 * no longer be touching.
	 *
	 * \warning Must be non-negative.
	 */
	Distance maxCollisionTouchingDistance = 0.01f * METERS;

	/**
	 * Margin to use around shapes when performing Gilbert-Johnson-Keerthi-based
	 * raycasts/shapecasts.
	 *
	 * \warning Must be non-negative.
	 */
	Distance gjkRaycastMargin = 0.0001f * METERS;

	/**
	 * Bias distance factor that determines by how much face contacts are
	 * preferred over edge contacts in convex polytope collisions.
	 *
	 * \warning Must be non-negative.
	 */
	Distance biasFaceOverEdge = 0.00001f * METERS;

	/**
	 * Bias distance factor that determines by how much the second face is
	 * preferred to be chosen as the reference face in a face contact in convex
	 * polytope collisions.
	 */
	Length1D biasReferenceFaceBOverA = 0.00001f * METERS;

	/**
	 * Maximum number of iterations to run per query of the
	 * Gilbert-Johnson-Keerthi algorithm before bailing out.
	 */
	size_t maxGJKIterationCount = 16;

	/**
	 * Maximum number of iterations to run per query of the
	 * Gilbert-Johnson-Keerthi-based raycast/shapecast algorithm before bailing
	 * out.
	 */
	size_t maxGJKRaycastIterationCount = 256;

	/**
	 * Maximum number of iterations to run per query of the Expanding Polytope
	 * Algorithm before bailing out.
	 */
	size_t maxEPAIterationCount = 16;
};
using CollisionAlgorithmOptions2D = CollisionAlgorithmOptions<2>; ///< Configuration options for a 2-dimensional collision algorithm.
using CollisionAlgorithmOptions3D = CollisionAlgorithmOptions<3>; ///< Configuration options for a 3-dimensional collision algorithm.

/**
 * Base interface for a stateful collision algorithm that generates contact
 * points between two potentially colliding objects of specific shape types.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class CollisionAlgorithmImplementation {
public:
	/**
	 * Perform collision detection using the underlying collision algorithm and
	 * determine whether two objects have any touching surfaces or not.
	 *
	 * \param temporaryMemoryResource non-null non-owning pointer to a memory
	 *        resource to use for temporary memory allocations, which will all
	 *        be deallocated by the time the function returns.
	 * \param minPenetrationDepth minimum penetration depth for a contact to be
	 *        considered to be colliding.
	 * \param colliderA collider of the first object. Must have the same shape
	 *        type as the first of the two shapes that this collision algorithm
	 *        was created to handle.
	 * \param transformationA world-space transformation of the first object.
	 * \param colliderB collider of the second object. Must have the same shape
	 *        type as the second of the two shapes that this collision algorithm
	 *        was created to handle.
	 * \param transformationB world-space transformation of the second object.
	 * \param options collision algorithm options, see
	 *        CollisionAlgorithmOptions. Should usually be
	 *        `simulation.resources.getResource<SimulationOptions<N>>().collisionAlgorithmOptions`.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision.
	 *
	 * \return the result of the test, or an empty test result that converts to
	 *         false if the objects weren't colliding.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] virtual CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) = 0;

	/**
	 * Perform collision detection using the underlying collision algorithm and
	 * generate contact manifolds for touching surfaces.
	 *
	 * \param temporaryMemoryResource non-null non-owning pointer to a memory
	 *        resource to use for temporary memory allocations, which will all
	 *        be deallocated by the time the function returns.
	 * \param colliderA collider of the first object. Must have the same shape
	 *        type as the first of the two shapes that this collision algorithm
	 *        was created to handle.
	 * \param transformationA world-space transformation of the first object.
	 * \param colliderB collider of the second object. Must have the same shape
	 *        type as the second of the two shapes that this collision algorithm
	 *        was created to handle.
	 * \param transformationB world-space transformation of the second object.
	 * \param options collision algorithm options, see
	 *        CollisionAlgorithmOptions. Should usually be
	 *        `simulation.resources.getResource<SimulationOptions<N>>().collisionAlgorithmOptions`.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision.
	 * \param callback function to pass the results to.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback function.
	 */
	virtual void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) = 0;

protected:
	/** Construct a new collision algorithm. */
	constexpr CollisionAlgorithmImplementation() noexcept = default;
};

/**
 * Base interface for a stateful collision algorithm that generates contact
 * points between two potentially colliding 2-dimensional objects of specific
 * shape types.
 */
using CollisionAlgorithmImplementation2D = CollisionAlgorithmImplementation<2>;

/**
 * Base interface for a stateful collision algorithm that generates contact
 * points between two potentially colliding 3-dimensional objects of specific
 * shape types.
 */
using CollisionAlgorithmImplementation3D = CollisionAlgorithmImplementation<3>;

/**
 * Generic box type for any collision algorithm implementation derived from
 * CollisionAlgorithmImplementation<N>.
 */
template <size_t N>
class CollisionAlgorithm {
public:
	static constexpr size_t MAX_CAPACITY = max(sizeof(void*) * 20, size_t{144});     ///< Maximum capacity for concrete collision algorithm implementations.
	static constexpr size_t MAX_ALIGNMENT = max(alignof(void*), alignof(float) * 4); ///< Maximum alignment for concrete collision algorithm implementations.

	/**
	 * Create a boxed collision algorithm with a specific implementation.
	 *
	 * \tparam Implementation concrete collision algorithm type.
	 *
	 * \param args arguments to forward to the concrete collision algorithm
	 *        constructor.
	 *
	 * \return the new boxed collision algorithm.
	 *
	 * \throws any exception thrown by the concrete collision algorithm's
	 *         constructor.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning CollisionAlgorithmImplementation<N> must be the only base class of the
	 *          implementation type.
	 */
	template <typename Implementation, typename... Args>
	[[nodiscard]] static CollisionAlgorithm create(Args&&... args) {
		static_assert(sizeof(Implementation) <= MAX_CAPACITY);
		static_assert(alignof(Implementation) <= MAX_ALIGNMENT);
		static_assert(std::is_nothrow_move_constructible_v<Implementation>);
		return CollisionAlgorithm{AnyCollisionAlgorithm::template create<Implementation>(std::forward<Args>(args)...)};
	}

	/**
	 * Create a boxed collision algorithm with an implementation that can be
	 * used to generate contact points for a given ordered pair of shapes.
	 *
	 * \param shapeA the first shape in the pair of shapes to choose a
	 *        collision algorithm for.
	 * \param shapeB the second shape to the pair of shapes to choose a
	 *        collision algorithm for.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(physics) static CollisionAlgorithm chooseImplementation(ShapeView<N> shapeA, ShapeView<N> shapeB);

	/** \copydoc CollisionAlgorithmImplementation<3>::hasCollision */
	[[nodiscard]] CollisionFilterTestResult hasCollision(ArenaResource* temporaryMemoryResource, Length1D minPenetrationDepth, ColliderView<N> colliderA,
		const Transformation<N>& transformationA, ColliderView<N> colliderB, const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options,
		CollisionFilterTest filterTest) {
		return collisionAlgorithm.getUnsafeSmallObjectBasePointer()->hasCollision(temporaryMemoryResource, minPenetrationDepth, colliderA, transformationA, colliderB,
			transformationB, options, filterTest);
	}

	/** \copydoc CollisionAlgorithmImplementation<3>::detectCollisions */
	void detectCollisions(ArenaResource* temporaryMemoryResource, ColliderView<N> colliderA, const Transformation<N>& transformationA, ColliderView<N> colliderB,
		const Transformation<N>& transformationB, const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest,
		FunctionView<void(const CollisionAlgorithmResult<N>& collision)> callback) {
		collisionAlgorithm.getUnsafeSmallObjectBasePointer()->detectCollisions(temporaryMemoryResource, colliderA, transformationA, colliderB, transformationB, options, filterTest,
			callback);
	}

private:
	using AnyCollisionAlgorithm = AnyBase<CollisionAlgorithmImplementation<N>, MAX_CAPACITY, MAX_ALIGNMENT>;

	explicit CollisionAlgorithm(AnyCollisionAlgorithm collisionAlgorithm) noexcept
		: collisionAlgorithm(std::move(collisionAlgorithm)) {}

	AnyCollisionAlgorithm collisionAlgorithm;
};

/**
 * Generic box type for any collision algorithm implementation derived from
 * CollisionAlgorithmImplementation2D.
 */
using CollisionAlgorithm2D = CollisionAlgorithm<2>;

/**
 * Generic box type for any collision algorithm implementation derived from
 * CollisionAlgorithmImplementation3D.
 */
using CollisionAlgorithm3D = CollisionAlgorithm<3>;

/**
 * Potential collision between two objects in a Simulation, including a stateful
 * collision algorithm for generating the actual contact points between them.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct alignas(64) Contact {
	/**
	 * Maximum number of contact manifolds in a single contact.
	 */
	static constexpr size_t MAX_MANIFOLD_COUNT = (N == 3) ? 4 : 2;

	/**
	 * Bitset of manifold indices in a contact.
	 */
	using ManifoldMask = BitArray<MAX_MANIFOLD_COUNT>;

	/**
	 * Calculate a suitable orthonormal tangent-space basis for a contact point
	 * pair given its normal and current relative velocity.
	 *
	 * \param normal world-space contact normal.
	 * \param relativeVelocity relative world-space velocity of the contact
	 *        point (A - B).
	 *
	 * \return the tangent-space basis in world space as a TBN matrix (tangent,
	 *         bitangent, normal) in 3D or a TN matrix (tangent, normal) in 2D.
	 */
	[[nodiscard]] static OrthonormalBasis<N> getTangentSpaceBasis(Direction<N> normal, LinearVelocity<N> relativeVelocity) {
		const LinearVelocity<N> relativeTangentialVelocity = relativeVelocity - dot(relativeVelocity, normal) * normal;
		const Direction<N> tangent = [&] {
			if (const Optional<Direction<N>> relativeTangentialVelocityDirection = tryNormalize(relativeTangentialVelocity)) {
				return *relativeTangentialVelocityDirection;
			}
			if constexpr (N == 3) {
				if (const Optional<Direction3D> xCrossN = tryNormalize(cross(X_AXIS_3D, normal))) {
					return *xCrossN;
				}
				return Y_AXIS_3D;
			} else {
				return rotate90DegreesCounterclockwise(normal);
			}
		}();
		if constexpr (N == 3) {
			const Scale3D bitangent = cross(normal, tangent);
			return OrthonormalBasis3D::reinterpret(Basis3D{tangent, bitangent, normal});
		} else {
			return OrthonormalBasis2D::reinterpret(Basis2D{tangent, normal});
		}
	}

	InplaceArrayList<ContactManifold<N>, MAX_MANIFOLD_COUNT> manifolds{}; ///< List of contact manifolds between the potentially colliding objects.
	CollisionAlgorithm<N> collisionAlgorithm;                             ///< Collision algorithm for generating contact points between the potentially colliding objects.
	bool wasTouchingOnLastSubStep = false;                                ///< Whether the contact had at least one manifold during the last sub-step or not.

	/**
	 * Construct a contact with a collision algorithm that can be used to
	 * generate contact points for a given ordered pair of shapes.
	 *
	 * \param shapeA the first shape in the pair of shapes to choose a
	 *        collision algorithm for.
	 * \param shapeB the second shape to the pair of shapes to choose a
	 *        collision algorithm for.
	 *
	 * \throws std::bad_alloc on allocation failure.
	 */
	Contact(ShapeView<N> shapeA, ShapeView<N> shapeB)
		: collisionAlgorithm(CollisionAlgorithm<N>::chooseImplementation(shapeA, shapeB)) {}

	/**
	 * Merge in a new collision manifold with the existing manifolds, and
	 * calculate its new tangent and relative tangent-space velocity.
	 *
	 * The merging logic works as follows:
	 *
	 * - If there exists a manifold with equal featureTypes, featureIndices and
	 *   filterTestResult that has a similar enough normal (within
	 *   `minContactManifoldDotProduct`), and it has not already been merged,
	 *   its normal is replaced with the new manifold's normal and its points
	 *   are merged with the new manifold's points.
	 *   - Before merging, existing points that are no longer within touching
	 *     range are removed.
	 *   - When merging, existing points that are within
	 *     `maxCollisionTouchingDistance` of a new point get their offsets and
	 *     localOffsets replaced, but keep the values of other fields
	 *     (transformed to the new tangent-space basis).
	 *   - Other points are transformed to the new tangent-space basis, but
	 *     otherwise remain unchanged. If no nearby point was found, the new
	 *     point is added to the list, unless the list is full, in which case it
	 *     replaces the point with the smallest penetration depth.
	 * - Otherwise, if no manifold suitable for merging was found, the new
	 *   manifold is added to the list, unless the list is full, in which case
	 *   it replaces the manifold whose deepest point has the smallest
	 *   penetration depth.
	 *
	 * \param mergedManifolds mask of manifolds that have already been merged,
	 *        which will be excluded from being merged again (unless the list is
	 *        full). The mask will be updated to also contain the new index
	 *        chosen by this procedure.
	 * \param collision collision containing the new manifold to merge in.
	 * \param transformationA world-space transformation of the first object.
	 * \param transformationB world-space transformation of the second object.
	 * \param linearVelocityA world-space linear velocity of the first object.
	 * \param linearVelocityB world-space linear velocity of the second object.
	 * \param angularVelocityA world-space angular velocity of the first object.
	 * \param angularVelocityB world-space angular velocity of the second object.
	 * \param maxCollisionTouchingDistance maximum separation distance of a
	 *        contact point before it is considered to no longer be touching.
	 *        Must be non-negative.
	 * \param minContactManifoldDotProduct minimum dot product (cosine of the
	 *        angle) between two manifold normals before they are no longer
	 *        considered similar enough to be merged.
	 *
	 * \return the index of the merged manifold.
	 */
	size_t insertCollision(ManifoldMask& mergedManifolds, const CollisionAlgorithmResult<N>& collision, const Transformation<N>& transformationA,
		const Transformation<N>& transformationB, LinearVelocity<N> linearVelocityA, LinearVelocity<N> linearVelocityB, AngularVelocity<N> angularVelocityA,
		AngularVelocity<N> angularVelocityB, Distance maxCollisionTouchingDistance, Scale1D minContactManifoldDotProduct) {
		for (size_t manifoldIndex = 0; manifoldIndex < manifolds.size(); ++manifoldIndex) {
			if (mergedManifolds[manifoldIndex]) {
				continue;
			}
			ContactManifold<N>& manifold = manifolds[manifoldIndex];
			if (manifold.featureTypes != collision.manifold.featureTypes || manifold.featureIndices != collision.manifold.featureIndices ||
				manifold.filterTestResult != collision.manifold.filterTestResult || dot(manifold.normal, collision.manifold.normal) < minContactManifoldDotProduct) {
				continue;
			}

			const Direction<N> oldNormal = manifold.normal;
			manifold.normal = collision.manifold.normal;
			for (ContactPoint<N>& point : manifold.points) {
				transformContactPoint(point, manifold.normal, oldNormal, linearVelocityA, linearVelocityB, angularVelocityA, angularVelocityB);
			}

			for (const ContactPoint<N>& newPoint : collision.manifold.points) {
				typename ContactManifold<N>::PointMask nearbyPoints = findNearbyPoints(manifold, newPoint, transformationA, transformationB, maxCollisionTouchingDistance);
				if (nearbyPoints == typename ContactManifold<N>::PointMask{}) {
					if (manifold.points.size() < ContactManifold<N>::MAX_POINT_COUNT) {
						manifold.points.push_back(newPoint);
						initializeContactPoint(manifold.points.back(), manifold.normal, linearVelocityA, linearVelocityB, angularVelocityA, angularVelocityB);
					} else {
						const size_t pointIndexWithSmallestPenetrationDepth = findPointWithSmallestPenetrationDepth(manifold, transformationA, transformationB);
						ContactPoint<N>& point = manifold.points[pointIndexWithSmallestPenetrationDepth];
						point = newPoint;
						initializeContactPoint(point, manifold.normal, linearVelocityA, linearVelocityB, angularVelocityA, angularVelocityB);
					}
				} else {
					const size_t mergedPointIndex = static_cast<size_t>(countTrailingZeroBits(nearbyPoints.toInteger()));
					ContactPoint<N>& mergedPoint = manifold.points[mergedPointIndex];
					mergedPoint.offsets = newPoint.offsets;
					mergedPoint.localOffsets = newPoint.localOffsets;
					transformContactPoint(mergedPoint, manifold.normal, oldNormal, linearVelocityA, linearVelocityB, angularVelocityA, angularVelocityB);
					for (size_t pointIndex = mergedPointIndex + 1; pointIndex < manifold.points.size(); ++pointIndex) {
						if (nearbyPoints[pointIndex]) {
							ContactPoint<N>& point = manifold.points[pointIndex];
							point.offsets = newPoint.offsets;
							point.localOffsets = newPoint.localOffsets;
							transformContactPoint(point, manifold.normal, oldNormal, linearVelocityA, linearVelocityB, angularVelocityA, angularVelocityB);
							mergedPoint.momentumInTangentSpace += point.momentumInTangentSpace;
							mergedPoint.impulseInTangentSpace += point.impulseInTangentSpace;
							mergedPoint.restitutionMomentum += point.restitutionMomentum;
							mergedPoint.restitutionImpulse += point.restitutionImpulse;
						}
					}
					nearbyPoints[mergedPointIndex] = false;
					removeMarkedElements(manifold.points, nearbyPoints);
				}
			}
			mergedManifolds[manifoldIndex] = true;
			return manifoldIndex;
		}

		if (manifolds.size() < MAX_MANIFOLD_COUNT) {
			const size_t manifoldIndex = manifolds.size();
			manifolds.push_back(collision.manifold);
			ContactManifold<N>& manifold = manifolds.back();
			for (ContactPoint<N>& point : manifold.points) {
				initializeContactPoint(point, manifold.normal, linearVelocityA, linearVelocityB, angularVelocityA, angularVelocityB);
			}
			mergedManifolds[manifoldIndex] = true;
			return manifoldIndex;
		}

		const size_t manifoldIndexWithSmallestPenetrationDepth = findManifoldWithSmallestPenetrationDepth(transformationA, transformationB);
		ContactManifold<N>& manifold = manifolds[manifoldIndexWithSmallestPenetrationDepth];
		manifold = collision.manifold;
		for (ContactPoint<N>& point : manifold.points) {
			initializeContactPoint(point, manifold.normal, linearVelocityA, linearVelocityB, angularVelocityA, angularVelocityB);
		}
		mergedManifolds[manifoldIndexWithSmallestPenetrationDepth] = true;
		return manifoldIndexWithSmallestPenetrationDepth;
	}

	/**
	 * Remove a specific set of manifolds.
	 *
	 * \param removeMask bitset of manifolds to remove, where each set bit
	 *        refers to the corresponding index to remove from the list of
	 *        manifolds.
	 */
	void removeManifolds(ManifoldMask removeMask) {
		removeMarkedElements(manifolds, removeMask);
	}

private:
	static void removeMarkedElements(auto& list, auto mask) {
		const size_t oldSize = list.size();
		size_t bits = mask.toInteger();
		size_t newSize = static_cast<size_t>(countTrailingZeroBits(bits));
		if (newSize >= oldSize) {
			return;
		}
		bits >>= newSize;
		for (size_t i = newSize + 1; i < oldSize; ++i) {
			if ((bits & 1) == 0) {
				list[newSize++] = list[i];
			}
			bits >>= 1;
		}
		list.erase(list.begin() + static_cast<ptrdiff_t>(newSize), list.end());
	}

	[[nodiscard]] static typename ContactManifold<N>::PointMask findNearbyPoints(const ContactManifold<N>& manifold, const ContactPoint<N>& newPoint,
		const Transformation<N>& transformationA, const Transformation<N>& transformationB, Distance maxDistance) {
		const Position<N> newPointA = transformationA.getOrigin() + newPoint.offsets.first;
		const Position<N> newPointB = transformationB.getOrigin() + newPoint.offsets.second;
		const SquaredDistance maxDistanceSquared = length2(maxDistance);
		typename ContactManifold<N>::PointMask result{};
		for (size_t pointIndex = 0; pointIndex < manifold.points.size(); ++pointIndex) {
			const ContactPoint<N>& point = manifold.points[pointIndex];
			const Position<N> pointA = transformationA.getOrigin() + point.offsets.first;
			const Position<N> pointB = transformationB.getOrigin() + point.offsets.second;
			if (distance2(newPointA, pointA) <= maxDistanceSquared && distance2(newPointB, pointB) <= maxDistanceSquared) {
				result[pointIndex] = true;
			}
		}
		return result;
	}

	[[nodiscard]] static size_t findPointWithSmallestPenetrationDepth(const ContactManifold<N>& manifold, const Transformation<N>& transformationA,
		const Transformation<N>& transformationB) {
		size_t pointIndexWithSmallestPenetrationDepth = 0;
		Length1D smallestPenetrationDepth = Length1D::MAX;
		for (size_t pointIndex = 0; pointIndex < manifold.points.size(); ++pointIndex) {
			const ContactPoint<N>& point = manifold.points[pointIndex];
			const Position<N> pointA = transformationA.getOrigin() + point.offsets.first;
			const Position<N> pointB = transformationB.getOrigin() + point.offsets.second;
			const Length1D penetrationDepth = dot(pointA - pointB, manifold.normal);
			if (penetrationDepth < smallestPenetrationDepth) {
				smallestPenetrationDepth = penetrationDepth;
				pointIndexWithSmallestPenetrationDepth = pointIndex;
			}
		}
		return pointIndexWithSmallestPenetrationDepth;
	}

	[[nodiscard]] size_t findManifoldWithSmallestPenetrationDepth(const Transformation<N>& transformationA, const Transformation<N>& transformationB) const {
		size_t manifoldIndexWithSmallestPenetrationDepth = 0;
		Length1D smallestPenetrationDepth = Length1D::MAX;
		for (size_t manifoldIndex = 0; manifoldIndex < manifolds.size(); ++manifoldIndex) {
			const ContactManifold<N>& manifold = manifolds[manifoldIndex];
			Length1D manifoldPenetrationDepth = Length1D::MIN;
			for (const ContactPoint<N>& point : manifold.points) {
				const Position<N> pointA = transformationA.getOrigin() + point.offsets.first;
				const Position<N> pointB = transformationB.getOrigin() + point.offsets.second;
				const Length1D penetrationDepth = dot(pointA - pointB, manifold.normal);
				manifoldPenetrationDepth = max(manifoldPenetrationDepth, penetrationDepth);
			}
			if (manifoldPenetrationDepth < smallestPenetrationDepth) {
				smallestPenetrationDepth = manifoldPenetrationDepth;
				manifoldIndexWithSmallestPenetrationDepth = manifoldIndex;
			}
		}
		return manifoldIndexWithSmallestPenetrationDepth;
	}

	void initializeContactPoint(ContactPoint<N>& point, Direction<N> normal, LinearVelocity<N> linearVelocityA, LinearVelocity<N> linearVelocityB,
		AngularVelocity<N> angularVelocityA, AngularVelocity<N> angularVelocityB) {
		const LinearVelocity<N> newRelativeVelocity =
			(linearVelocityA + cross(point.offsets.first, angularVelocityA)) - (linearVelocityB + cross(point.offsets.second, angularVelocityB));
		const OrthonormalBasis<N> newTangentSpaceBasis = getTangentSpaceBasis(normal, newRelativeVelocity);
		point.tangent = newTangentSpaceBasis[X];
		point.relativeVelocityInTangentSpace = transpose(newTangentSpaceBasis) * newRelativeVelocity;
	}

	void transformContactPoint(ContactPoint<N>& point, Direction<N> normal, Direction<N> oldNormal, LinearVelocity<N> linearVelocityA, LinearVelocity<N> linearVelocityB,
		AngularVelocity<N> angularVelocityA, AngularVelocity<N> angularVelocityB) {
		const LinearVelocity<N> newRelativeVelocity =
			(linearVelocityA + cross(point.offsets.first, angularVelocityA)) - (linearVelocityB + cross(point.offsets.second, angularVelocityB));
		const OrthonormalBasis<N> oldTangentSpaceBasis = point.getTangentSpaceBasis(oldNormal);
		const OrthonormalBasis<N> newTangentSpaceBasis = getTangentSpaceBasis(normal, newRelativeVelocity);
		const OrthonormalBasis<N> tangentSpaceTransformation = transpose(newTangentSpaceBasis) * oldTangentSpaceBasis;
		const Scale1D normalTransformation = dot(normal, oldNormal);
		point.tangent = newTangentSpaceBasis[X];
		point.relativeVelocityInTangentSpace = transpose(newTangentSpaceBasis) * newRelativeVelocity;
		const auto old = point.momentumInTangentSpace;
		point.momentumInTangentSpace = tangentSpaceTransformation * point.momentumInTangentSpace;
		point.impulseInTangentSpace = tangentSpaceTransformation * point.impulseInTangentSpace;
		point.restitutionMomentum = normalTransformation * point.restitutionMomentum;
		point.restitutionImpulse = normalTransformation * point.restitutionImpulse;
	}
};

/**
 * Potential collision between two objects in a 2-dimensional Simulation,
 * including a stateful collision algorithm for generating the actual contact
 * points between them.
 */
using Contact2D = Contact<2>;

/**
 * Potential collision between two objects in a 3-dimensional Simulation,
 * including a stateful collision algorithm for generating the actual contact
 * points between them.
 */
using Contact3D = Contact<3>;

/**
 * Set of contacts in a Simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class Contacts : public HashMap<Pair<EntityID>, Contact<N>, detail::ContactsKeyHash> {};
using Contacts2D = Contacts<2>; ///< Set of contacts in a 2-dimensional Simulation.
using Contacts3D = Contacts<3>; ///< Set of contacts in a 3-dimensional Simulation.

/**
 * Check if a collider contains a given point.
 *
 * \param pointA point to test.
 * \param filterA collision filter of the point.
 * \param colliderB collider to check.
 * \param transformationB world-space transformation of the collider to check.
 * \param filterTest test that potential colliders must pass in order to be
 *        considered for collision. The default test passes if the filters of
 *        both colliders want either collision or response (or both) on any
 *        layer(s). To limit to only colliders that want response (and skip
 *        trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
 *
 * \return the result of the collision filter test, or an empty result that
 *         converts to false if there was no collision.
 */
[[nodiscard]] GREM_API(physics) CollisionFilterTestResult
	containsPoint(const Position2D& pointA, CollisionFilter filterA, ColliderView2D colliderB, const Transformation2D& transformationB, CollisionFilterTest filterTest = {});

/**
 * Check if a collider contains a given point.
 *
 * \param pointA point to test.
 * \param filterA collision filter of the point.
 * \param colliderB collider to check.
 * \param transformationB world-space transformation of the collider to check.
 * \param filterTest test that potential colliders must pass in order to be
 *        considered for collision. The default test passes if the filters of
 *        both colliders want either collision or response (or both) on any
 *        layer(s). To limit to only colliders that want response (and skip
 *        trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
 *
 * \return the result of the collision filter test, or an empty result that
 *         converts to false if there was no collision.
 */
[[nodiscard]] GREM_API(physics) CollisionFilterTestResult
	containsPoint(const Position3D& pointA, CollisionFilter filterA, ColliderView3D colliderB, const Transformation3D& transformationB, CollisionFilterTest filterTest = {});

/**
 * Find the first collision of a ray with a collider.
 *
 * \param rayA ray to cast.
 * \param filterA collision filter of the ray.
 * \param colliderB collider to cast against.
 * \param transformationB world-space transformation of the collider to cast
 *        against.
 * \param filterTest test that potential colliders must pass in order to be
 *        considered for collision. The default test passes if the filters of
 *        both colliders want either collision or response (or both) on any
 *        layer(s). To limit to only colliders that want response (and skip
 *        trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
 *
 * \return a pair of:
 *         - the result of the raycast.
 *         - the result of the collision filter test.
 */
[[nodiscard]] GREM_API(physics) Pair<RaycastResult2D, CollisionFilterTestResult> raycast(const Ray2D& rayA, CollisionFilter filterA, ColliderView2D colliderB,
	const Transformation2D& transformationB, CollisionFilterTest filterTest = {});

/**
 * Find the first collision of a ray with a collider.
 *
 * \param rayA ray to cast.
 * \param filterA collision filter of the ray.
 * \param colliderB collider to cast against.
 * \param transformationB world-space transformation of the collider to cast
 *        against.
 * \param filterTest test that potential colliders must pass in order to be
 *        considered for collision. The default test passes if the filters of
 *        both colliders want either collision or response (or both) on any
 *        layer(s). To limit to only colliders that want response (and skip
 *        trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
 *
 * \return a pair of:
 *         - the result of the raycast.
 *         - the result of the collision filter test.
 */
[[nodiscard]] GREM_API(physics) Pair<RaycastResult3D, CollisionFilterTestResult> raycast(const Ray3D& rayA, CollisionFilter filterA, ColliderView3D colliderB,
	const Transformation3D& transformationB, CollisionFilterTest filterTest = {});

/**
 * Result of a shapecast miss.
 */
struct ShapecastMiss {};

/**
 * Result of a shapecast hit.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ShapecastHit {
	Pair<Length<N>> localOffsets; ///< Offsets from the objects' centers of mass in shape-local space at which the collision occured.
	Direction<N> normal;          ///< Unit vector pointing away from the surface of the second object and towards the surface of the first object.
	Distance distance;            ///< Distance at which the hit occured.
};
using ShapecastHit2D = ShapecastHit<2>; ///< Result of a shapecast hit in 2-dimensional space.
using ShapecastHit3D = ShapecastHit<3>; ///< Result of a shapecast hit in 3-dimensional space.

/**
 * Result of an interor shapecast hit.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ShapecastHitInterior {};
using ShapecastHitInterior2D = ShapecastHitInterior<2>; ///< Result of an interior shapecast hit in 2-dimensional space.
using ShapecastHitInterior3D = ShapecastHitInterior<3>; ///< Result of an interior shapecast hit in 3-dimensional space.

/**
 * Result of a shapecast.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct ShapecastResult : Variant<ShapecastMiss, ShapecastHit<N>, ShapecastHitInterior<N>> {
	using Variant<ShapecastMiss, ShapecastHit<N>, ShapecastHitInterior<N>>::Variant;
};
using ShapecastResult2D = ShapecastResult<2>; ///< Result of a shapecast in 2-dimensional space.
using ShapecastResult3D = ShapecastResult<3>; ///< Result of a shapecast in 3-dimensional space.

/**
 * Find the first collision between a convex shape a collider when the convex
 * shape is cast from a given starting transformation towards a given direction.
 *
 * \param convexShapeA convex shape to cast.
 * \param filterA collision filter of the shape.
 * \param transformationA world-space transformation of the convex shape.
 * \param colliderB collider to cast against.
 * \param transformationB world-space transformation of the collider.
 * \param direction direction to cast the convex shape in.
 * \param maxDistance maximum hit distance. Must be non-negative.
 * \param options collision algorithm options, see CollisionAlgorithmOptions.
 *        Should usually be
 *        `simulation.resources.getResource<SimulationOptions<N>>().collisionAlgorithmOptions`.
 * \param filterTest test that potential colliders must pass in order to be
 *        considered for collision. The default test passes if the filters of
 *        both colliders want either collision or response (or both) on any
 *        layer(s). To limit to only colliders that want response (and skip
 *        trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
 *
 * \return a pair of:
 *         - the result of the shapecast.
 *         - the result of the collision filter test.
 */
[[nodiscard]] GREM_API(physics) Pair<ShapecastResult2D, CollisionFilterTestResult> shapecast(ConvexShapeView2D convexShapeA, CollisionFilter filterA,
	const Transformation2D& transformationA, ColliderView2D colliderB, const Transformation2D& transformationB, Direction2D direction, Distance maxDistance,
	const CollisionAlgorithmOptions2D& options, CollisionFilterTest filterTest = {});

/**
 * Find the first collision between a convex shape a collider when the convex
 * shape is cast from a given starting transformation towards a given direction.
 *
 * \param convexShapeA convex shape to cast.
 * \param filterA collision filter of the shape.
 * \param transformationA world-space transformation of the convex shape.
 * \param colliderB collider to cast against.
 * \param transformationB world-space transformation of the collider.
 * \param direction direction to cast the convex shape in.
 * \param maxDistance maximum hit distance. Must be non-negative.
 * \param options collision algorithm options, see CollisionAlgorithmOptions.
 *        Should usually be
 *        `simulation.resources.getResource<SimulationOptions<N>>().collisionAlgorithmOptions`.
 * \param filterTest test that potential colliders must pass in order to be
 *        considered for collision. The default test passes if the filters of
 *        both colliders want either collision or response (or both) on any
 *        layer(s). To limit to only colliders that want response (and skip
 *        trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
 *
 * \return a pair of:
 *         - the result of the shapecast.
 *         - the result of the collision filter test.
 */
[[nodiscard]] GREM_API(physics) Pair<ShapecastResult3D, CollisionFilterTestResult> shapecast(ConvexShapeView3D convexShapeA, CollisionFilter filterA,
	const Transformation3D& transformationA, ColliderView3D colliderB, const Transformation3D& transformationB, Direction3D direction, Distance maxDistance,
	const CollisionAlgorithmOptions3D& options, CollisionFilterTest filterTest = {});

} // namespace grem::physics

#endif
