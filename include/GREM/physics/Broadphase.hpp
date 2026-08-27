// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_BROADPHASE_HPP
#define GREM_PHYSICS_BROADPHASE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/LooseOrthtree.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/physics/EntityID.hpp>
#include <GREM/physics/Shape.hpp>
#include <GREM/physics/collision.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include <utility> // std::forward

namespace grem::physics {

/**
 * Opaque handle to an entity in a Broadphase.
 */
struct BroadphaseID : LooseOrthtreeID {
	constexpr BroadphaseID() noexcept = default;

	constexpr BroadphaseID(LooseOrthtreeID id) noexcept
		: LooseOrthtreeID(id) {}

	constexpr BroadphaseID(LooseOrthtree<2, EntityID>::iterator pos) noexcept
		: LooseOrthtreeID(pos) {}

	constexpr BroadphaseID(LooseOrthtree<2, EntityID>::const_iterator pos) noexcept
		: LooseOrthtreeID(pos) {}

	constexpr BroadphaseID(LooseOrthtree<3, EntityID>::iterator pos) noexcept
		: LooseOrthtreeID(pos) {}

	constexpr BroadphaseID(LooseOrthtree<3, EntityID>::const_iterator pos) noexcept
		: LooseOrthtreeID(pos) {}

	using LooseOrthtreeID::operator==;
	using LooseOrthtreeID::operator bool;
};

/**
 * %Acceleration structure for spatial queries in a simulation.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class Broadphase {
private:
	using Tree = LooseOrthtree<N, EntityID>;

public:
	using value_type = typename Tree::value_type;                     ///< Value type of the container.
	using reference = typename Tree::reference;                       ///< Reference type of the container.
	using const_reference = typename Tree::const_reference;           ///< Const reference type of the container.
	using pointer = typename Tree::pointer;                           ///< Pointer type of the container.
	using const_pointer = typename Tree::const_pointer;               ///< Const pointer type of the container.
	using size_type = typename Tree::size_type;                       ///< Size type of the container.
	using difference_type = typename Tree::difference_type;           ///< Difference type of the container.
	using iterator = typename Tree::iterator;                         ///< Iterator type of the container.
	using const_iterator = typename Tree::const_iterator;             ///< Const iterator type of the container.
	using local_iterator = typename Tree::local_iterator;             ///< Local iterator type for the nodes of the container.
	using const_local_iterator = typename Tree::const_local_iterator; ///< Const local iterator type for the nodes of the container.

	/**
	 * Non-owning view over the elements in a broadphase.
	 */
	using ElementsView = Subrange<iterator, iterator, SubrangeKind::SIZED>;

	/**
	 * Non-owning read-only view over the elements in a broadphase.
	 */
	using ConstElementsView = Subrange<const_iterator, const_iterator, SubrangeKind::SIZED>;

	/**
     * Get a view over the elements in the broadphase.
     *
     * \return a view over the elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] ElementsView getElements() noexcept {
		return tree;
	}

	/**
     * Get a read-only view over the elements in the broadphase.
     *
     * \return a read-only view over the elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] ConstElementsView getElements() const noexcept {
		return tree;
	}

	/**
	 * Erase all elements from the broadphase and reset it to its empty state.
	 */
	void clear() noexcept {
		tree.clear();
	}

	/**
	 * Reset the broadphase to an empty state with new world parameters.
	 *
	 * \param worldBoundingBox total bounding box of the entities that will be
	 *        inserted.
	 * \param minEntitySizeApproximation approximation of the minimum width of
	 *        the entities that will be inserted.
	 */
	void reset(const Box<N>& worldBoundingBox, Distance minEntitySizeApproximation) noexcept {
		tree.reset(worldBoundingBox.in(Box<N>::UNIT), minEntitySizeApproximation.in(Distance::UNIT));
	}

	/**
	 * Get the current minimum entity size approximation of the broadphase.
	 *
	 * \return the minimum entity size approximation.
	 */
	[[nodiscard]] Distance getMinEntitySizeApproximation() const noexcept {
		return tree.getMinOrthantSize() * Distance::UNIT;
	}

	/**
	 * Get the current root box of the broadphase.
	 *
	 * \return the root box of the broadphase.
	 */
	[[nodiscard]] Box<N> getRootBox() const noexcept {
		return tree.getRootBox() * Box<N>::UNIT;
	}

	/**
	 * Insert an entity into the broadphase.
	 *
	 * \param boundingBox axis-aligned bounding box of the entity.
	 * \param entityID handle to the entity.
	 *
	 * \return an iterator to the newly inserted entity.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	iterator insert(const Box<N>& boundingBox, EntityID entityID) {
		return tree.insert(boundingBox.in(Box<N>::UNIT), entityID);
	}

	/**
	 * Remove an entity from the broadphase.
	 *
	 * \param pos iterator to the entity to remove. Must be valid.
	 *
	 * \sa clear()
	 */
	void erase(const_iterator pos) {
		tree.erase(pos);
	}

	/**
	 * Remove an entity from the broadphase.
	 *
	 * \param id handle to the entity to remove. Must be valid.
	 *
	 * \sa clear()
	 */
	void erase(BroadphaseID id) {
		tree.erase(id);
	}

	/**
	 * Get the entity with a specific handle.
	 *
	 * \param id handle to the entity to get. Must be valid.
	 *
	 * \return the handle of the entity with the given broadphase handle.
	 */
	[[nodiscard]] EntityID operator[](LooseOrthtreeID id) const {
		return tree[id];
	}

	/**
	 * Execute a callback function for each active node of the broadphase,
	 * including empty internal nodes.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameters (though they don't need to be used):
	 *        - `const Box<N>& boundingBox`: an axis-aligned box spanning all
	 *          entities in the node.
	 *        - `const_local_iterator first`: a read-only iterator to the
	 *          beginning of the range of entities in the node.
	 *        - `const_local_iterator last`: a read-only iterator to one past
	 *          the end of the range of entities in the node.
	 *        .
	 *        The callback function should return either void or a bool that
	 *        specifies whether to stop the traversal or not. A value of true
	 *        means to stop and return early, while a value of false means to
	 *        continue traversing.
	 * \param predicate condition that must be met in order to traverse deeper
	 *        into the hierarchy. Must return bool and accept the following
	 *        parameter:
	 *        - `const Box<N>& boundingBox`: an axis-aligned box spanning all
	 *          entities in the branch.
	 *        .
	 *        The predicate function should return a bool that is true if the
	 *        next node should be traversed, or false if the branch should be
	 *        ignored.
	 *
	 * \return void if the callback function returns void, true if the callback
	 *         returns bool and exited early, false if the callback function
	 *         returns bool but didn't exit early.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback or predicate functions.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 *
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
	 *
	 * \sa traverseEntities()
	 */
	template <typename Callback, typename Predicate>
	auto traverseNodes(Callback&& callback, Predicate&& predicate) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		return tree.traverseNodes([callback = std::forward<Callback>(callback)](const grem::Box<N, float>& boundingBox, const const_local_iterator& first,
									  const const_local_iterator& last) mutable { return callback(boundingBox * Box<N>::UNIT, first, last); },
			[predicate = std::forward<Predicate>(predicate)](const grem::Box<N, float>& boundingBox) mutable { return predicate(boundingBox * Box<N>::UNIT); });
	}

	/**
	 * Execute a callback function for each active node of the broadphase,
	 * including empty internal nodes.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameters (though they don't need to be used):
	 *        - `const Box<N>& boundingBox`: an axis-aligned box spanning all
	 *          entities in the node.
	 *        - `const_local_iterator first`: a read-only iterator to the
	 *          beginning of the range of entities in the node.
	 *        - `const_local_iterator last`: a read-only iterator to one past
	 *          the end of the range of entities in the node.
	 *        .
	 *        The callback function should return either void or a bool that
	 *        specifies whether to stop the traversal or not. A value of true
	 *        means to stop and return early, while a value of false means to
	 *        continue traversing.
	 *
	 * \return void if the callback function returns void, true if the callback
	 *         returns bool and exited early, false if the callback function
	 *         returns bool but didn't exit early.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback function.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 *
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
	 *
	 * \sa traverseEntities()
	 */
	template <typename Callback>
	auto traverseNodes(Callback&& callback) const {
		return traverseNodes(std::forward<Callback>(callback), [](const Box<N>&) -> bool { return true; });
	}

	/**
	 * Execute a callback function for each entity in the broadphase.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameter:
	 *        - `EntityID entityID`: the entity handle.
	 *        .
	 *        The callback function should return either void or a bool that
	 *        specifies whether to stop the traversal or not. A value of true
	 *        means to stop and return early, while a value of false means to
	 *        continue traversing.
	 * \param predicate condition that must be met in order to traverse deeper
	 *        into the hierarchy. Must return bool and accept the following
	 *        parameter:
	 *        - `const Box<N>& boundingBox`: an axis-aligned box spanning all
	 *          entities in the branch.
	 *        .
	 *        The predicate function should return a bool that is true if the
	 *        next node should be traversed, or false if the branch should be
	 *        ignored.
	 *
	 * \return void if the callback function returns void, true if the callback
	 *         returns bool and exited early, false if the callback function
	 *         returns bool but didn't exit early.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback or predicate functions.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 *
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
	 *
	 * \sa traverseNodes()
	 */
	template <typename Callback, typename Predicate>
	auto traverseEntities(Callback&& callback, Predicate&& predicate) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		return tree.traverseElements(std::forward<Callback>(callback),
			[predicate = std::forward<Predicate>(predicate)](const grem::Box<N, float>& boundingBox) mutable { return predicate(boundingBox * Box<N>::UNIT); });
	}

	/**
	 * Execute a callback function for each entity in the broadphase.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameter:
	 *        - `EntityID entityID`: the entity handle.
	 *        .
	 *        The callback function should return either void or a bool that
	 *        specifies whether to stop the traversal or not. A value of true
	 *        means to stop and return early, while a value of false means to
	 *        continue traversing.
	 *
	 * \return void if the callback function returns void, true if the callback
	 *         returns bool and exited early, false if the callback function
	 *         returns bool but didn't exit early.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback function.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 *
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
	 *
	 * \sa traverseNodes()
	 */
	template <typename Callback>
	auto traverseEntities(Callback&& callback) const {
		return traverseEntities(std::forward<Callback>(callback), [](const Box<N>&) -> bool { return true; });
	}

	/**
	 * Find all objects in the broadphase that contain a given point.
	 *
	 * \param point world-space point to check.
	 * \param filter collision filter of the point.
	 * \param entities objects to collide against.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision. The default test passes if the filters
	 *        of both colliders want either collision or response (or both) on
	 *        any layer(s). To limit to only colliders that want response (and
	 *        skip trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
	 * \param callback function to execute for each colliding object that passed
	 *        the filter test. Returns a bool that specifies whether to stop
	 *        searching or not. A value of true means to stop and return early,
	 *        while a value of false means to continue processing the rest of
	 *        the collisions. Note that at the end of the search, testPoint()
	 *        will return true if at least one collision was found, regardless
	 *        of what the callback returned.
	 * \param predicate condition that must be met in order to consider an
	 *        entity for collision. Must return bool and accept the following
	 *        parameter:
	 *        - `EntityID objectID`: an entity handle to the
	 *          candidate object.
	 *
	 * \return true if at least one collision was found, false otherwise.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback or predicate functions.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 * \note Only entities that are both in the broadphase and in the given set
	 *       will be considered for collision.
	 */
	GREM_API(physics)
	bool testPoint(
		Position<N> point, CollisionFilter filter, execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities,
		CollisionFilterTest filterTest = {}, FunctionView<bool(EntityID objectID, CollisionFilterTestResult filterTestResult)> callback = [](EntityID) -> bool { return true; },
		FunctionView<bool(EntityID objectID)> predicate = [](EntityID) -> bool { return true; }) const;

	/**
	 * Find all objects in the broadphase that collide with a given shape.
	 *
	 * \param minPenetrationDepth minimum penetration depth for a contact to be
	 *        considered to be colliding.
	 * \param shape shape to collide.
	 * \param filter collision filter of the shape.
	 * \param transformation world-space transformation of the shape.
	 * \param entities objects to collide against.
	 * \param options collision algorithm options, see
	 *        CollisionAlgorithmOptions. Should usually be
	 *        `simulation.resources.getResource<SimulationOptions<N>>().collisionAlgorithmOptions`.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision. The default test passes if the filters
	 *        of both colliders want either collision or response (or both) on
	 *        any layer(s). To limit to only colliders that want response (and
	 *        skip trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
	 * \param callback function to execute for each colliding object that passed
	 *        the filter test. Returns a bool that specifies whether to stop
	 *        searching or not. A value of true means to stop and return early,
	 *        while a value of false means to continue processing the rest of
	 *        the collisions. Note that at the end of the search, testShape()
	 *        will return true if at least one collision was found, regardless
	 *        of what the callback returned.
	 * \param predicate condition that must be met in order to consider an
	 *        entity for collision. Must return bool and accept the following
	 *        parameter:
	 *        - `EntityID objectID`: an entity handle to the
	 *          candidate object.
	 *
	 * \return true if at least one collision was found, false otherwise.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback or predicate functions.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 * \note Only entities that are both in the broadphase and in the given set
	 *       will be considered for collision.
	 */
	GREM_API(physics)
	bool testShape(
		Length1D minPenetrationDepth, ShapeView<N> shape, CollisionFilter filter, const Transformation<N>& transformation,
		execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities,
		const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest = {},
		FunctionView<bool(EntityID objectID, CollisionFilterTestResult filterTestResult)> callback = [](EntityID) -> bool { return true; },
		FunctionView<bool(EntityID objectID)> predicate = [](EntityID) -> bool { return true; }) const;

	/**
	 * Result of a collision check of a shape against the entities in a
	 * broadphase.
	 */
	struct CollisionResult {
		/**
		 * Handle to the object that the shape collided with.
		 */
		EntityID objectID;

		/**
		 * Contact manifold of the collision, where the first object is the
		 * given shape and the second object is the found entity.
		 */
		ContactManifold<N> manifold;
	};

	/**
	 * Find collisions between a shape and the objects in the broadphase.
	 *
	 * \param shape shape to collide.
	 * \param filter collision filter of the shape.
	 * \param transformation world-space transformation of the shape.
	 * \param entities objects to collide against.
	 * \param options collision algorithm options, see
	 *        CollisionAlgorithmOptions. Should usually be
	 *        `simulation.resources.getResource<SimulationOptions<N>>().collisionAlgorithmOptions`.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision. The default test passes if the filters
	 *        of both colliders want either collision or response (or both) on
	 *        any layer(s). To limit to only colliders that want response (and
	 *        skip trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
	 * \param callback function to execute for each colliding object that passed
	 *        the filter test. Returns a bool that specifies whether to stop
	 *        searching or not. A value of true means to stop and return early,
	 *        while a value of false means to continue processing the rest of
	 *        the collisions. Note that at the end of the search, collide()
	 *        will return true if at least one collision was found, regardless
	 *        of what the callback returned.
	 * \param predicate condition that must be met in order to consider an
	 *        entity for collision. Must return bool and accept the following
	 *        parameter:
	 *        - `EntityID objectID`: an entity handle to the
	 *          candidate object.
	 *
	 * \return true if at least one collision was found, false otherwise.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback or predicate functions.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 * \note Only entities that are both in the broadphase and in the given set
	 *       will be considered for collision.
	 */
	GREM_API(physics)
	bool collide(
		ShapeView<N> shape, CollisionFilter filter, const Transformation<N>& transformation,
		execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities,
		const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest = {},
		FunctionView<bool(const CollisionResult& collision)> callback = [](const CollisionResult&) -> bool { return true; },
		FunctionView<bool(EntityID objectID)> predicate = [](EntityID) -> bool { return true; }) const;

	/**
	 * Result of a raycast against the objects in a broadphase.
	 */
	struct RaycastResult {
		/**
		 * Handle to the object that the ray collided with.
		 */
		EntityID objectID;

		/**
		 * Offset from the object's center of mass in shape-local space at which
		 * the collision occured.
		 */
		Length<N> localOffset;

		/**
		 * Unit vector perpendicular to the separating plane of the collision,
		 * pointing away from the object's surface, in world space.
		 */
		Direction<N> normal;

		/**
		 * Distance from the raycast origin along the raycast direction at which
		 * which the ray collided.
		 */
		Distance distance;

		/**
		 * Result of the collision filter test.
		 */
		CollisionFilterTestResult filterTestResult;
	};

	/**
	 * Find collisions between a point and the objects in the broadphase when
	 * the point is cast like a ray from a given start position towards a given
	 * direction.
	 *
	 * \param ray ray to cast.
	 * \param filter collision filter of the ray.
	 * \param entities objects to collide against.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision. The default test passes if the filters
	 *        of both colliders want either collision or response (or both) on
	 *        any layer(s). To limit to only colliders that want response (and
	 *        skip trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
	 * \param callback function to execute for each colliding object that passed
	 *        the filter test. Returns a bool that specifies whether to stop
	 *        searching or not. A value of true means to stop and return early,
	 *        while a value of false means to continue processing the rest of
	 *        the collisions. Note that at the end of the search, raycast()
	 *        will return true if at least one collision was found, regardless
	 *        of what the callback returned.
	 * \param predicate condition that must be met in order to consider an
	 *        entity for collision. Must return bool and accept the following
	 *        parameter:
	 *        - `EntityID objectID`: an entity handle to the
	 *          candidate object.
	 *
	 * \return true if at least one collision was found, false otherwise.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback or predicate functions.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 * \note Only entities that are both in the broadphase and in the given set
	 *       will be considered for collision.
	 */
	GREM_API(physics)
	bool raycast(
		const Ray<N>& ray, CollisionFilter filter, execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities,
		CollisionFilterTest filterTest = {}, FunctionView<bool(const RaycastResult& hit)> callback = [](const RaycastResult&) -> bool { return true; },
		FunctionView<bool(EntityID objectID)> predicate = [](EntityID) -> bool { return true; }) const;

	/**
	 * Find the closest collision between a point and the objects in the
	 * broadphase when the point is cast like a ray from a given start position
	 * towards a given direction.
	 *
	 * \param ray ray to cast.
	 * \param filter collision filter of the ray.
	 * \param entities objects to collide against.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision. The default test passes if the filters
	 *        of both colliders want either collision or response (or both) on
	 *        any layer(s). To limit to only colliders that want response (and
	 *        skip trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
	 * \param predicate condition that must be met in order to consider an
	 *        entity for collision. Must return bool and accept the following
	 *        parameter:
	 *        - `EntityID objectID`: an entity handle to the
	 *          candidate object.
	 *
	 * \return the closest found collision that passed the filter test, or an
	 *         empty optional if no collisions were found that passed the test.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the predicate function.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 * \note Only entities that are both in the broadphase and in the given set
	 *       will be considered for collision.
	 */
	[[nodiscard]] Optional<RaycastResult> raycastClosestHit(
		const Ray<N>& ray, CollisionFilter filter, execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities,
		CollisionFilterTest filterTest = {}, FunctionView<bool(EntityID objectID)> predicate = [](EntityID) -> bool { return true; }) const {
		Optional<RaycastResult> result{};
		raycast(
			ray, filter, entities, filterTest,
			[&](const RaycastResult& hit) -> bool {
				if (hit.distance == 0) {
					result = hit;
					return true;
				}
				if (!result || hit.distance < result->distance) {
					result = hit;
				}
				return false;
			},
			predicate);
		return result;
	}

	/**
	 * Result of a shapecast against the objects in a broadphase.
	 */
	struct ShapecastResult {
		/**
		 * Handle to the object that the shape collided with.
		 */
		EntityID objectID;

		/**
		 * Offsets from the objects' centers of mass in shape-local space at
		 * which the collision occured.
		 */
		Pair<Length<N>> localOffsets;

		/**
		 * Unit vector perpendicular to the separating plane of the collision,
		 * pointing away from the second object's surface, in world space.
		 */
		Direction<N> normal;

		/**
		 * Distance along the shapecast direction at which the collision
		 * occured.
		 */
		Distance distance;

		/**
		 * Result of the collision filter test.
		 */
		CollisionFilterTestResult filterTestResult;
	};

	/**
	 * Find collisions between an object of a convex shape and the objects in
	 * the broadphase when the object is cast from a given start position
	 * towards a given direction.
	 *
	 * \param convexShape convex shape to cast.
	 * \param filter collision filter of the shape.
	 * \param transformation world-space transformation of the shape.
	 * \param direction direction to cast in.
	 * \param maxDistance maximum hit distance. Must be non-negative.
	 * \param entities objects to collide against.
	 * \param options collision algorithm options, see
	 *        CollisionAlgorithmOptions. Should usually be
	 *        `simulation.resources.getResource<SimulationOptions<N>>().collisionAlgorithmOptions`.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision. The default test passes if the filters
	 *        of both colliders want either collision or response (or both) on
	 *        any layer(s). To limit to only colliders that want response (and
	 *        skip trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
	 * \param callback function to execute for each colliding object that passed
	 *        the filter test. Returns a bool that specifies whether to stop
	 *        searching or not. A value of true means to stop and return early,
	 *        while a value of false means to continue processing the rest of
	 *        the collisions. Note that at the end of the search, shapecast()
	 *        will return true if at least one collision was found, regardless
	 *        of what the callback returned.
	 * \param predicate condition that must be met in order to consider an
	 *        entity for collision. Must return bool and accept the following
	 *        parameter:
	 *        - `EntityID objectID`: an entity handle to the
	 *          candidate object.
	 *
	 * \return true if at least one collision was found, false otherwise.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the callback or predicate functions.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 * \note Only entities that are both in the broadphase and in the given set
	 *       will be considered for collision.
	 */
	GREM_API(physics)
	bool shapecast(
		ConvexShapeView<N> convexShape, CollisionFilter filter, const Transformation<N>& transformation, Direction<N> direction, Distance maxDistance,
		execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities,
		const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest = {},
		FunctionView<bool(const ShapecastResult& hit)> callback = [](const ShapecastResult&) -> bool { return true; },
		FunctionView<bool(EntityID objectID)> predicate = [](EntityID) -> bool { return true; }) const;

	/**
	 * Find the closest collision between an object of a convex shape and the objects in
	 * the broadphase when the object is cast from a given start position towards a
	 * given direction.
	 *
	 * \param convexShape convex shape to cast.
	 * \param filter collision filter of the shape.
	 * \param transformation world-space transformation of the shape.
	 * \param direction direction to cast in.
	 * \param maxDistance maximum hit distance. Must be non-negative.
	 * \param entities objects to collide against.
	 * \param options collision algorithm options, see
	 *        CollisionAlgorithmOptions. Should usually be
	 *        `simulation.resources.getResource<SimulationOptions<N>>().collisionAlgorithmOptions`.
	 * \param filterTest test that potential colliders must pass in order to be
	 *        considered for collision. The default test passes if the filters
	 *        of both colliders want either collision or response (or both) on
	 *        any layer(s). To limit to only colliders that want response (and
	 *        skip trigger volumes, etc.), use CollisionFilterTest::RESPONSE.
	 * \param predicate condition that must be met in order to consider an
	 *        entity for collision. Must return bool and accept the following
	 *        parameter:
	 *        - `EntityID objectID`: an entity handle to the
	 *          candidate object.
	 *
	 * \return the closest found collision that passed the filter test, or an
	 *         empty optional if no collisions were found that passed the test.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the predicate function.
	 *
	 * \note The order of traversal is unspecified, though it is guaranteed that
	 *       outer nodes will be visited before their own inner nodes that they
	 *       contain.
	 * \note Only entities that are both in the broadphase and in the given set
	 *       will be considered for collision.
	 */
	[[nodiscard]] Optional<ShapecastResult> shapecastClosestHit(
		ConvexShapeView<N> convexShape, CollisionFilter filter, const Transformation<N>& transformation, Direction<N> direction, Distance maxDistance,
		execution::Entities<const Position<N>, const Orientation<N>, const Scale<N>, const Collider<N>, const ObjectBounds<N>> entities,
		const CollisionAlgorithmOptions<N>& options, CollisionFilterTest filterTest = {},
		FunctionView<bool(EntityID objectID)> predicate = [](EntityID) -> bool { return true; }) const {
		Optional<ShapecastResult> result{};
		shapecast(
			convexShape, filter, transformation, direction, maxDistance, entities, options, filterTest,
			[&](const ShapecastResult& hit) -> bool {
				if (hit.distance == 0) {
					result = hit;
					return true;
				}
				if (!result || hit.distance < result->distance) {
					result = hit;
				}
				return false;
			},
			predicate);
		return result;
	}

	/**
	 * Erase all entities that match a predicate from the broadphase.
	 *
	 * \param c broadphase to erase from.
	 * \param predicate condition to check.
	 *
	 * \return the number of entities that matched the predicate and were
	 *         subsequently erased.
	 *
	 * \throws any exception thrown by the predicate function.
	 */
	template <typename Predicate>
	friend size_type erase_if(Broadphase& c, Predicate&& predicate) {
		return erase_if(c.tree, std::forward<Predicate>(predicate));
	}

private:
	Tree tree{};
};

using Broadphase2D = Broadphase<2>; ///< %Acceleration structure for spatial queries in a 2-dimensional simulation.
using Broadphase3D = Broadphase<3>; ///< %Acceleration structure for spatial queries in a 3-dimensional simulation.

} // namespace grem::physics

#endif
