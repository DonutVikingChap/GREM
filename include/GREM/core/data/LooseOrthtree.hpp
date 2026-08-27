// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_LOOSE_ORTHTREE_HPP
#define GREM_CORE_DATA_LOOSE_ORTHTREE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/InplaceBuffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>

#include <type_traits> // std::conditional_t, std::is_convertible_v
#include <utility>     // std::forward, std::move, std::exchange, std::declval

namespace grem {

template <size_t N, typename T>
class LooseOrthtree; // Forward declaration.

/**
 * Opaque handle to an element in a LooseOrthtree.
 */
class LooseOrthtreeID {
public:
	constexpr LooseOrthtreeID() noexcept = default;

	[[nodiscard]] constexpr bool operator==(const LooseOrthtreeID&) const noexcept = default;

	constexpr operator bool() const noexcept {
		return elementIndex != INVALID_ELEMENT;
	}

private:
	using ElementIndex = uint32_t;

	static constexpr ElementIndex INVALID_ELEMENT = Limits<ElementIndex>::MAX;

	template <size_t N, typename T>
	friend class LooseOrthtree;

	constexpr explicit LooseOrthtreeID(ElementIndex elementIndex) noexcept
		: elementIndex(elementIndex) {}

	ElementIndex elementIndex = INVALID_ELEMENT;
};

/**
 * Orthtree-based space subdivision structure, optimized for intersection
 * queries between axis-aligned boxes.
 *
 * \tparam N number of vector dimensions (must be 2 or 3).
 * \tparam T type of element to store in the tree.
 */
template <size_t N, typename T>
class LooseOrthtree {
public:
	static_assert(N == 2 || N == 3);

	using value_type = T;              ///< Value type of the container.
	using reference = T&;              ///< Reference type of the container.
	using const_reference = const T&;  ///< Const reference type of the container.
	using pointer = T*;                ///< Pointer type of the container.
	using const_pointer = const T*;    ///< Const pointer type of the container.
	using size_type = size_t;          ///< Size type of the container.
	using difference_type = ptrdiff_t; ///< Difference type of the container.

private:
	using NodeIndex = uint32_t;
	using ElementIndex = LooseOrthtreeID::ElementIndex;

	static constexpr ElementIndex INVALID_ELEMENT = LooseOrthtreeID::INVALID_ELEMENT;

	struct Node {
		using SubOrthantNodeIndices = Array<NodeIndex, (N == 2) ? 4 : 8>;

		SubOrthantNodeIndices subOrthantNodeIndices{};
		NodeIndex parentNodeIndex = 0;
		ElementIndex firstElementIndex = INVALID_ELEMENT;
	};

	struct Element {
		ElementIndex previousElementIndex = INVALID_ELEMENT;
		ElementIndex nextElementIndex = INVALID_ELEMENT;
		Optional<value_type> value{};
		NodeIndex nodeIndex = 0;
	};

	template <bool Const>
	class Iterator {
	public:
		using reference = std::conditional_t<Const, typename LooseOrthtree::const_reference, typename LooseOrthtree::reference>;
		using pointer = std::conditional_t<Const, typename LooseOrthtree::const_pointer, typename LooseOrthtree::pointer>;
		using difference_type = typename LooseOrthtree::difference_type;
		using value_type = typename LooseOrthtree::value_type;

		constexpr Iterator() noexcept = default;

		constexpr operator Iterator<true>() const noexcept requires(!Const) {
			return Iterator<true>{container, elementIndex};
		}

		constexpr operator LooseOrthtreeID() const noexcept {
			return LooseOrthtreeID{elementIndex};
		}

		[[nodiscard]] constexpr reference operator*() const {
			GREM_ASSERT(container);
			return *container->elements[elementIndex].value;
		}

		[[nodiscard]] constexpr pointer operator->() const {
			return &**this;
		}

		[[nodiscard]] constexpr bool operator==(const Iterator& other) const {
			GREM_ASSERT(container == other.container);
			return elementIndex == other.elementIndex;
		}

		constexpr Iterator& operator++() {
			GREM_ASSERT(container);
			GREM_ASSERT(elementIndex < container->elements.size());
			do {
				++elementIndex;
			} while (elementIndex < container->elements.size() && !container->elements[elementIndex].value);
			return *this;
		}

		constexpr Iterator operator++(int) {
			Iterator old = *this;
			++*this;
			return old;
		}

	private:
		friend LooseOrthtree;

		constexpr Iterator(std::conditional_t<Const, const LooseOrthtree, LooseOrthtree>* container, ElementIndex elementIndex) noexcept
			: container(container)
			, elementIndex(elementIndex) {}

		std::conditional_t<Const, const LooseOrthtree, LooseOrthtree>* container = nullptr;
		ElementIndex elementIndex = 0;
	};

	template <bool Const>
	class LocalIterator {
	public:
		using reference = std::conditional_t<Const, typename LooseOrthtree::const_reference, typename LooseOrthtree::reference>;
		using pointer = std::conditional_t<Const, typename LooseOrthtree::const_pointer, typename LooseOrthtree::pointer>;
		using difference_type = typename LooseOrthtree::difference_type;
		using value_type = typename LooseOrthtree::value_type;

		constexpr LocalIterator() noexcept = default;

		constexpr operator Iterator<true>() const noexcept requires(!Const) {
			return Iterator<true>{container, elementIndex};
		}

		constexpr operator LooseOrthtreeID() const noexcept {
			return LooseOrthtreeID{elementIndex};
		}

		[[nodiscard]] constexpr reference operator*() const {
			GREM_ASSERT(container);
			return *container->elements[elementIndex].value;
		}

		[[nodiscard]] constexpr pointer operator->() const {
			return &**this;
		}

		[[nodiscard]] constexpr bool operator==(const LocalIterator& other) const {
			GREM_ASSERT(container == other.container);
			return elementIndex == other.elementIndex;
		}

		constexpr LocalIterator& operator++() {
			GREM_ASSERT(container);
			elementIndex = container->elements[elementIndex].nextElementIndex;
			return *this;
		}

		constexpr LocalIterator operator++(int) {
			Iterator old = *this;
			++*this;
			return old;
		}

	private:
		friend LooseOrthtree;

		constexpr LocalIterator(std::conditional_t<Const, const LooseOrthtree, LooseOrthtree>* container, ElementIndex elementIndex) noexcept
			: container(container)
			, elementIndex(elementIndex) {}

		std::conditional_t<Const, const LooseOrthtree, LooseOrthtree>* container = nullptr;
		ElementIndex elementIndex = 0;
	};

public:
	using iterator = Iterator<false>;                 ///< Iterator type of the container.
	using const_iterator = Iterator<true>;            ///< Const iterator type of the container.
	using local_iterator = LocalIterator<false>;      ///< Local iterator type for the nodes of the container.
	using const_local_iterator = LocalIterator<true>; ///< Const local iterator type for the nodes of the container.

	/**
	 * Construct an invalid tree that must be reset with valid world parameters
	 * before use.
	 */
	LooseOrthtree() noexcept
		: LooseOrthtree(Box<N, float>{}, 0.0f) {}

	/**
	 * Construct an empty tree.
	 *
	 * \param worldBoundingBox bounding box of the world, or the full region
	 *        that contains all other possible axis-aligned boxes that may be
	 *        inserted into the tree.
	 * \param minOrthantSize minimum threshold for the size of a leaf orthant.
	 *        This should correspond roughly to the typical width of the boxes
	 *        that will be inserted into the tree.
	 */
	LooseOrthtree(const Box<N, float>& worldBoundingBox, float minOrthantSize) noexcept {
		reset(worldBoundingBox, minOrthantSize);
	}

	/**
	 * Reset the tree to an empty state with new world parameters.
	 *
	 * \param newWorldBoundingBox bounding box of the world, or the full region
	 *        that contains all other possible axis-aligned boxes that may be
	 *        inserted into the tree.
	 * \param newMinOrthantSize minimum threshold for the size of a leaf
	 *        orthant. This should correspond roughly to the typical width of
	 *        the boxes that will be inserted into the tree.
	 *
	 * \sa clear()
	 */
	void reset(const Box<N, float>& newWorldBoundingBox, float newMinOrthantSize) noexcept {
		clear();
		minOrthantSize = newMinOrthantSize;
		rootCenter = (newWorldBoundingBox.min + newWorldBoundingBox.max) * 0.5f;
		const Length<N, float> worldMaxExtents = max(newWorldBoundingBox.max - rootCenter, rootCenter - newWorldBoundingBox.min);
		const float worldMaxExtent = maxComponent(worldMaxExtents);
		// Double the root size until it fits the entire world.
		halfRootSize = newMinOrthantSize;
		while (halfRootSize < worldMaxExtent) {
			halfRootSize *= 2.0f;
		}
	}

	/**
	 * Erase all inserted elements from the tree.
	 *
	 * \sa reset()
	 * \sa erase()
	 */
	void clear() noexcept {
		tree.clear();
		elements.clear();
		elementCount = 0;
		firstFreeNodeIndex = 0;
		firstFreeElementIndex = INVALID_ELEMENT;
	}

	/**
	 * Get the current minimum orthant size of the tree.
	 *
	 * \return the minimum orthant size of the tree.
	 */
	[[nodiscard]] float getMinOrthantSize() const noexcept {
		return minOrthantSize;
	}

	/**
	 * Get the current root box of the tree.
	 *
	 * \return the root box of the tree.
	 */
	[[nodiscard]] Box<N, float> getRootBox() const noexcept {
		const Length<N, float> rootHalfExtents{halfRootSize};
		return Box<N, float>{.min = rootCenter - rootHalfExtents, .max = rootCenter + rootHalfExtents};
	}

	/**
     * Get the begin iterator of the elements in the tree.
     *
     * \return an iterator to the first element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] iterator begin() noexcept {
		ElementIndex elementIndex = 0;
		while (elementIndex < elements.size() && !elements[elementIndex].value) {
			++elementIndex;
		}
		return iterator{this, elementIndex};
	}

	/**
     * Get the begin iterator of the elements in the tree.
     *
     * \return an iterator to the first element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] const_iterator begin() const noexcept {
		ElementIndex elementIndex = 0;
		while (elementIndex < elements.size() && !elements[elementIndex].value) {
			++elementIndex;
		}
		return const_iterator{this, elementIndex};
	}

	/**
     * Get the begin iterator of the elements in the tree.
     *
     * \return an iterator to the first element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] const_iterator cbegin() const noexcept {
		return begin();
	}

	/**
     * Get the end iterator of the elements in the tree.
     *
     * \return an iterator past the last element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] iterator end() noexcept {
		return iterator{this, static_cast<ElementIndex>(elements.size())};
	}

	/**
     * Get the end iterator of the elements in the tree.
     *
     * \return an iterator past the last element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] const_iterator end() const noexcept {
		return const_iterator{this, static_cast<ElementIndex>(elements.size())};
	}

	/**
     * Get the end iterator of the elements in the tree.
     *
     * \return an iterator past the last element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] const_iterator cend() const noexcept {
		return end();
	}

	/**
     * Get the number of elements in the tree.
     *
     * \return the number of elements.
     */
	[[nodiscard]] size_type size() const noexcept {
		return elementCount;
	}

	/**
     * Check whether the tree is empty or not.
     *
     * \return true if there are no elements, false otherwise.
     */
	[[nodiscard]] bool empty() const noexcept {
		return size() == 0;
	}

	/**
	 * Construct a new element in the tree.
	 *
	 * \param elementBoundingBox axis-aligned bounding box of the element.
	 * \param args constructor arguments for the new element.
	 *
	 * \return an iterator to the newly inserted element.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the element constructor.
	 *
	 * \sa insert()
	 */
	template <typename... Args>
	iterator emplace(const Box<N, float>& elementBoundingBox, Args&&... args) {
		// Make sure the tree has a root.
		if (tree.empty()) {
			tree.emplace_back();
		}

		// Find the center of the AABB.
		const Length<N, float> aabbDiagonal = elementBoundingBox.max - elementBoundingBox.min;
		const Point<N, float> aabbCenter = elementBoundingBox.min + aabbDiagonal * 0.5f;

		// Find the largest extent of the AABB.
		const float aabbSize = [&] {
			if constexpr (N == 2) {
				return max(aabbDiagonal.x, aabbDiagonal.y);
			} else {
				return max(max(aabbDiagonal.x, aabbDiagonal.y), aabbDiagonal.z);
			}
		}();

		// Start at the root of the tree and descend to the smallest orthant that contains the entire AABB within its loose bounds.
		// The loose bounds is a box around the orthant that is twice as big as the orthant in every direction and shares the same center.
		// Since the loose bounds of adjacent orthants overlap, it could happen that the AABB is contained within multiple orthants' loose bounds at the same time.
		// In that case, the closest orthant, i.e. the one which contains the center of the AABB, is chosen.
		// The loop stops going lower in the tree when the AABB can no longer fit in a smaller orthant, or when we reach the minimum orthant size.
		float orthantSize = halfRootSize;
		Point<N, float> center = rootCenter;
		NodeIndex nodeIndex = 0;
		while (orthantSize >= aabbSize && orthantSize >= minOrthantSize) {
			orthantSize *= 0.5f;

			// Determine which orthant the AABB belongs to.
			const Length<N, float> difference = aabbCenter - center;
			const size_t subOrthantArrayIndex = getSubOrthantArrayIndex(difference);

			// Update the center.
			center += copysign(Length<N, float>{orthantSize}, difference);

			// Descend to the chosen orthant.
			const NodeIndex parentNodeIndex = nodeIndex;
			if (const NodeIndex orthantNodeIndex = tree[parentNodeIndex].subOrthantNodeIndices[subOrthantArrayIndex]) {
				// The orthant already exists in the tree. Go directly to it.
				nodeIndex = orthantNodeIndex;
			} else {
				// The orthant does not exist in the tree yet. Acquire a slot in the tree for a new node.
				if (firstFreeNodeIndex != 0) {
					// Re-use the first free node, update the free index and go to the new node.
					// Note: The first sub-orthant index in each free node leads to the next free node after it.
					nodeIndex = std::exchange(firstFreeNodeIndex, std::exchange(tree[firstFreeNodeIndex].subOrthantNodeIndices.front(), NodeIndex{0}));
				} else {
					// No free orthants available for re-use. Allocate a new node and go to it.
					if (tree.size() >= size_t{Limits<NodeIndex>::MAX}) {
						throw std::length_error{"Maximum node count exceeded."};
					}
					nodeIndex = static_cast<NodeIndex>(tree.size());
					tree.emplace_back();
				}
				// Set the new orthant's parent node index.
				tree[nodeIndex].parentNodeIndex = parentNodeIndex;
				// Update the parent node to point to the new node.
				tree[parentNodeIndex].subOrthantNodeIndices[subOrthantArrayIndex] = nodeIndex;
			}
		}

		// Acquire a slot for the new element.
		ElementIndex elementIndex = INVALID_ELEMENT;
		try {
			if (firstFreeElementIndex != INVALID_ELEMENT) {
				// Re-use the first free element and update the free index.
				// Note: The next element index in each free element leads to the next free element after it.
				elementIndex = std::exchange(firstFreeElementIndex, std::exchange(elements[firstFreeElementIndex].nextElementIndex, INVALID_ELEMENT));
			} else {
				// No free elements available for re-use. Allocate a new element.
				if (elements.size() >= size_t{Limits<ElementIndex>::MAX}) {
					throw std::length_error{"Maximum element count exceeded."};
				}
				elementIndex = static_cast<ElementIndex>(elements.size());
				elements.emplace_back();
			}
			// Construct the new element value.
			try {
				elements[elementIndex].value.emplace(std::forward<Args>(args)...);
			} catch (...) {
				cleanupElement(elementIndex);
				throw;
			}
		} catch (...) {
			cleanupNode(nodeIndex);
			throw;
		}

		// Insert the new element into the selected node.
		elements[elementIndex].nodeIndex = nodeIndex;
		ElementIndex& firstElementIndex = tree[nodeIndex].firstElementIndex;
		GREM_ASSERT(firstElementIndex != elementIndex);
		if (firstElementIndex != INVALID_ELEMENT) {
			elements[firstElementIndex].previousElementIndex = elementIndex;
		}
		GREM_ASSERT(elements[elementIndex].previousElementIndex == INVALID_ELEMENT);
		elements[elementIndex].nextElementIndex = std::exchange(firstElementIndex, elementIndex);
		GREM_ASSERT(elements[elementIndex].nextElementIndex != elementIndex);

		++elementCount;
		return iterator{this, elementIndex};
	}

	/**
	 * Copy an element into the tree.
	 *
	 * \param elementBoundingBox axis-aligned bounding box of the element.
	 * \param value value to be copied into the tree.
	 *
	 * \return an iterator to the newly inserted element.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the element copy constructor.
	 *
	 * \sa emplace()
	 */
	iterator insert(const Box<N, float>& elementBoundingBox, const T& value) {
		return emplace(elementBoundingBox, value);
	}

	/**
	 * Move an element into the tree.
	 *
	 * \param elementBoundingBox axis-aligned bounding box of the element.
	 * \param value value to be moved into the tree.
	 *
	 * \return an iterator to the newly inserted element.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * \throws any exception thrown by the element move constructor.
	 *
	 * \sa emplace()
	 */
	iterator insert(const Box<N, float>& elementBoundingBox, T&& value) {
		return emplace(elementBoundingBox, std::move(value));
	}

	/**
	 * Remove an element from the tree.
	 *
	 * \param pos iterator to the element to remove. Must be valid.
	 *
	 * \return an iterator to the next element after the erased element.
	 *
	 * \sa clear()
	 */
	iterator erase(const_iterator pos) {
		GREM_ASSERT(pos.container == this);
		iterator result{this, pos.elementIndex};
		++result;
		erase(LooseOrthtreeID{pos});
		return result;
	}

	/**
	 * Remove an element from the tree.
	 *
	 * \param id handle to the element to remove. Must be valid.
	 *
	 * \sa clear()
	 */
	void erase(LooseOrthtreeID id) {
		eraseAtIndex(id.elementIndex);
	}

	/**
	 * Get the element with a specific handle.
	 *
	 * \param id handle to the element to get. Must be valid.
	 *
	 * \return a reference to the given element, valid until the next mutation
	 *         to the tree.
	 */
	[[nodiscard]] reference operator[](LooseOrthtreeID id) {
		return *elements[id.elementIndex].value;
	}

	/**
	 * Get the element with a specific handle.
	 *
	 * \param id handle to the element to get. Must be valid.
	 *
	 * \return a read-only reference to the given element, valid until the next
	 *         mutation to the tree.
	 */
	[[nodiscard]] const_reference operator[](LooseOrthtreeID id) const {
		return *elements[id.elementIndex].value;
	}

	/**
	 * Execute a callback function for each active node of the tree, including
	 * empty internal nodes without an element.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameters (though they don't need to be used):
	 *        - `const grem::Box<N, float>& looseBounds`: an axis-aligned box
	 *          that defines the region that an element's bounding box must be
	 *          fully contained within in order to belong to the node.
	 *        - `const_local_iterator first`: a read-only iterator to the
	 *          beginning of the range of elements in the node.
	 *        - `const_local_iterator last`: a read-only iterator to one past
	 *          the end of the range of elements in the node.
	 *        .
	 *        The callback function should return either void or a bool that
	 *        specifies whether to stop the traversal or not. A value of true
	 *        means to stop and return early, while a value of false means to
	 *        continue traversing.
	 * \param predicate condition that must be met in order to traverse deeper
	 *        into the tree. Must return bool and accept the following
	 *        parameter:
	 *        - `const grem::Box<N, float>& boundingBox`: an axis-aligned box
	 *          spanning all elements in the branch.
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
	 * \sa traverseElements()
	 * \sa test()
	 */
	template <typename Callback, typename Predicate>
	auto traverseNodes(Callback&& callback, Predicate&& predicate) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		return traverseNodesImplementation(
			[this, callback = std::forward<Callback>(callback)](const Box<N, float>& looseBounds, NodeIndex nodeIndex) mutable {
				const const_local_iterator first{this, tree[nodeIndex].firstElementIndex};
				const const_local_iterator last{this, INVALID_ELEMENT};
				constexpr bool CALLBACK_RETURNS_BOOL = std::is_convertible_v<decltype(callback(looseBounds, first, last)), bool>;
				if constexpr (CALLBACK_RETURNS_BOOL) {
					return callback(looseBounds, first, last);
				} else {
					callback(looseBounds, first, last);
				}
			},
			std::forward<Predicate>(predicate));
	}

	/**
	 * Execute a callback function for each active node of the tree, including
	 * empty internal nodes without an element.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameters (though they don't need to be used):
	 *        - `const grem::Box<N, float>& looseBounds`: an axis-aligned box
	 *          that defines the region that an element's bounding box must be
	 *          fully contained within in order to belong to the node.
	 *        - `const_local_iterator first`: a read-only iterator to the
	 *          beginning of the range of elements in the node.
	 *        - `const_local_iterator last`: a read-only iterator to one past
	 *          the end of the range of elements in the node.
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
	 * \sa traverseElements()
	 * \sa test()
	 */
	template <typename Callback>
	auto traverseNodes(Callback&& callback) const {
		return traverseNodes(std::forward<Callback>(callback), [](const Box<N, float>&) -> bool { return true; });
	}

	/**
	 * Execute a callback function for each element in the tree.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameter:
	 *        - `const T& element`: a read-only reference to the element.
	 *        .
	 *        The callback function should return either void or a bool that
	 *        specifies whether to stop the traversal or not. A value of true
	 *        means to stop and return early, while a value of false means to
	 *        continue traversing.
	 * \param predicate condition that must be met in order to traverse deeper
	 *        into the tree. Must return bool and accept the following
	 *        parameter:
	 *        - `const grem::Box<N, float>& boundingBox`: an axis-aligned box
	 *          spanning all elements in the branch.
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
	 * \sa traverseNodes()
	 * \sa test()
	 */
	template <typename Callback, typename Predicate>
	auto traverseElements(Callback&& callback, Predicate&& predicate) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		return traverseNodesImplementation(
			[this, callback = std::forward<Callback>(callback)](const Box<N, float>&, NodeIndex nodeIndex) mutable {
				const const_local_iterator first{this, tree[nodeIndex].firstElementIndex};
				const const_local_iterator last{this, INVALID_ELEMENT};
				constexpr bool CALLBACK_RETURNS_BOOL = std::is_convertible_v<decltype(callback(*first)), bool>;
				for (const_local_iterator it = first; it != last; ++it) {
					if constexpr (CALLBACK_RETURNS_BOOL) {
						if (callback(*it)) {
							return true; // Callback requested early return.
						}
					} else {
						callback(*it);
					}
				}
				if constexpr (CALLBACK_RETURNS_BOOL) {
					return false;
				}
			},
			std::forward<Predicate>(predicate));
	}

	/**
	 * Execute a callback function for each element in the tree.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameter:
	 *        - `const T& element`: a read-only reference to the element.
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
	 * \sa traverseNodes()
	 * \sa test()
	 */
	template <typename Callback>
	auto traverseElements(Callback&& callback) const {
		return traverseElements(std::forward<Callback>(callback), [](const Box<N, float>&) -> bool { return true; });
	}

	/**
	 * Execute a callback function for each element in the tree that might
	 * contain a given point.
	 *
	 * \param point point to test.
	 * \param callback function to execute, which should accept the following
	 *        parameter:
	 *        - `const T& element`: a read-only reference to the element.
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
	 * \sa traverseNodes()
	 * \sa traverseElements()
	 */
	template <typename Callback>
	auto test(Point<N, float> point, Callback&& callback) const {
		return traverseElements(std::forward<Callback>(callback), [&point](const Box<N, float>& boundingBox) -> bool { return boundingBox.contains(point); });
	}

	/**
	 * Check if it is possible that some element in the tree contains a given
	 * point.
	 *
	 * \param point point to test.
	 *
	 * \return true if some element might contain the point, false otherwise.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa traverseNodes()
	 * \sa traverseElements()
	 */
	[[nodiscard]] bool test(Point<N, float> point) const noexcept {
		return traverseElements([](const T&) -> bool { return true; }, [&point](const Box<N, float>& boundingBox) -> bool { return boundingBox.contains(point); });
	}

	/**
	 * Execute a callback function for each element in the tree that might be
	 * intersecting with a given axis-aligned box.
	 *
	 * \param box box to test.
	 * \param callback function to execute, which should accept the following
	 *        parameter:
	 *        - `const T& element`: a read-only reference to the element.
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
	 * \sa traverseNodes()
	 * \sa traverseElements()
	 */
	template <typename Callback>
	auto test(const Box<N, float>& box, Callback&& callback) const {
		return traverseElements(std::forward<Callback>(callback), [&box](const Box<N, float>& boundingBox) -> bool { return intersects(boundingBox, box); });
	}

	/**
	 * Check if it is possible that some element in the tree is intersecting
	 * with a given axis-aligned box.
	 *
	 * \param box box to test.
	 *
	 * \return true if some element might be intersecting with the box, false
	 *         otherwise.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa traverseNodes()
	 * \sa traverseElements()
	 */
	[[nodiscard]] bool test(const Box<N, float>& box) const noexcept {
		return traverseElements([](const T&) -> bool { return true; }, [&box](const Box<N, float>& boundingBox) -> bool { return intersects(boundingBox, box); });
	}

	template <typename U>
	friend size_type erase(LooseOrthtree& c, const U& value) {
		return erase_if(c, [&](const value_type& v) -> bool { return v == value; });
	}

	template <typename Predicate>
	friend size_type erase_if(LooseOrthtree& c, Predicate predicate) {
		size_type result = 0;
		for (ElementIndex elementIndex = 0; elementIndex < c.elements.size(); ++elementIndex) {
			if (const Optional<value_type>& value = c.elements[elementIndex].value) {
				if (predicate(*value)) {
					c.eraseAtIndex(elementIndex);
					++result;
				}
			}
		}
		return result;
	}

private:
	[[nodiscard]] static constexpr size_t getSubOrthantArrayIndex(Length<N, float> difference) {
		size_t result = 0;
		for (size_t i = 0; i < N; ++i) {
			result |= static_cast<size_t>(!signbit(difference[i])) << (N - 1 - i);
		}
		return result;
	}

	template <typename Callback>
	static constexpr void forEachActiveOrthant(const Node::SubOrthantNodeIndices& subOrthantNodeIndices, Point<N, float> center, float halfOrthantSize,
		Callback&& callback) { // NOLINT(cppcoreguidelines-missing-std-forward)
		if constexpr (N == 2) {
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b00]) {
				callback(orthantNodeIndex, Point<2, float>{center.x - halfOrthantSize, center.y - halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b01]) {
				callback(orthantNodeIndex, Point<2, float>{center.x - halfOrthantSize, center.y + halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b10]) {
				callback(orthantNodeIndex, Point<2, float>{center.x + halfOrthantSize, center.y - halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b11]) {
				callback(orthantNodeIndex, Point<2, float>{center.x + halfOrthantSize, center.y + halfOrthantSize});
			}
		} else {
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b000]) {
				callback(orthantNodeIndex, Point<3, float>{center.x - halfOrthantSize, center.y - halfOrthantSize, center.z - halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b001]) {
				callback(orthantNodeIndex, Point<3, float>{center.x - halfOrthantSize, center.y - halfOrthantSize, center.z + halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b010]) {
				callback(orthantNodeIndex, Point<3, float>{center.x - halfOrthantSize, center.y + halfOrthantSize, center.z - halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b011]) {
				callback(orthantNodeIndex, Point<3, float>{center.x - halfOrthantSize, center.y + halfOrthantSize, center.z + halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b100]) {
				callback(orthantNodeIndex, Point<3, float>{center.x + halfOrthantSize, center.y - halfOrthantSize, center.z - halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b101]) {
				callback(orthantNodeIndex, Point<3, float>{center.x + halfOrthantSize, center.y - halfOrthantSize, center.z + halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b110]) {
				callback(orthantNodeIndex, Point<3, float>{center.x + halfOrthantSize, center.y + halfOrthantSize, center.z - halfOrthantSize});
			}
			if (const NodeIndex orthantNodeIndex = subOrthantNodeIndices[0b111]) {
				callback(orthantNodeIndex, Point<3, float>{center.x + halfOrthantSize, center.y + halfOrthantSize, center.z + halfOrthantSize});
			}
		}
	}

	template <typename Callback, typename Predicate>
	auto traverseNodesImplementation(Callback&& callback, Predicate&& predicate) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		constexpr bool CALLBACK_RETURNS_BOOL = std::is_convertible_v<decltype(callback(std::declval<const Box<N, float>&>(), std::declval<const NodeIndex&>())), bool>;

		if (!tree.empty()) {
			struct IterationState {
				Point<N, float> center;
				float orthantSize;
				NodeIndex nodeIndex;
			};
			SmallBuffer<IterationState, 24> iterationStack{};

			iterationStack.push_back(IterationState{
				.center = rootCenter,
				.orthantSize = halfRootSize,
				.nodeIndex = 0,
			});
			do {
				const IterationState iterationState = iterationStack.back();
				iterationStack.pop_back();

				const Point<N, float> center = iterationState.center;
				const float orthantSize = iterationState.orthantSize;
				const NodeIndex nodeIndex = iterationState.nodeIndex;
				const Length<N, float> looseBoundsHalfSize{orthantSize * 2.0f};
				const Box<N, float> looseBounds{center - looseBoundsHalfSize, center + looseBoundsHalfSize};
				if constexpr (CALLBACK_RETURNS_BOOL) {
					if (callback(looseBounds, nodeIndex)) {
						return true; // Callback requested early return.
					}
				} else {
					callback(looseBounds, nodeIndex);
				}

				const float halfOrthantSize = orthantSize * 0.5f;
				forEachActiveOrthant(tree[nodeIndex].subOrthantNodeIndices, center, halfOrthantSize,
					[&iterationStack, &predicate, orthantSize, halfOrthantSize](NodeIndex orthantNodeIndex, Point<N, float> orthantCenter) {
						const Box<N, float> looseBounds{orthantCenter - Length<N, float>{orthantSize}, orthantCenter + Length<N, float>{orthantSize}};
						if (predicate(looseBounds)) {
							iterationStack.push_back(IterationState{
								.center = orthantCenter,
								.orthantSize = halfOrthantSize,
								.nodeIndex = orthantNodeIndex,
							});
						}
					});
			} while (!iterationStack.empty());
		}

		if constexpr (CALLBACK_RETURNS_BOOL) {
			return false;
		}
	}

	[[nodiscard]] static bool isNodeEmpty(const Node& node) {
		return node.firstElementIndex == INVALID_ELEMENT && allOf(node.subOrthantNodeIndices, [](NodeIndex orthantNodeIndex) -> bool { return orthantNodeIndex == 0; });
	}

	void cleanupNode(NodeIndex nodeIndex) noexcept {
		Node* node = &tree[nodeIndex];
		while (isNodeEmpty(*node)) {
			if (nodeIndex == 0) {
				clear();
				break;
			}
			node->subOrthantNodeIndices.front() = std::exchange(firstFreeNodeIndex, std::exchange(nodeIndex, node->parentNodeIndex));
			node = &tree[nodeIndex];
			for (NodeIndex& orthantNodeIndex : node->subOrthantNodeIndices) {
				if (orthantNodeIndex == firstFreeNodeIndex) {
					orthantNodeIndex = 0;
					break;
				}
			}
		}
	}

	void cleanupElement(ElementIndex elementIndex) noexcept {
		Element& element = elements[elementIndex];
		const ElementIndex previousElementIndex = std::exchange(element.previousElementIndex, INVALID_ELEMENT);
		if (element.nextElementIndex != INVALID_ELEMENT) {
			elements[element.nextElementIndex].previousElementIndex = previousElementIndex;
		}
		ElementIndex& previousNextElementIndex =
			(previousElementIndex == INVALID_ELEMENT) ? tree[element.nodeIndex].firstElementIndex : elements[previousElementIndex].nextElementIndex;
		previousNextElementIndex = std::exchange(element.nextElementIndex, std::exchange(firstFreeElementIndex, elementIndex));
		GREM_ASSERT(elements[elementIndex].nextElementIndex != elementIndex);
		if (element.nodeIndex != 0) {
			cleanupNode(std::exchange(element.nodeIndex, NodeIndex{0}));
		}
	}

	void eraseAtIndex(ElementIndex elementIndex) {
		GREM_ASSERT(elementCount > 0);
		GREM_ASSERT(elementIndex < elements.size());
		GREM_ASSERT(elements[elementIndex].value.has_value());
		--elementCount;
		elements[elementIndex].value.reset();
		cleanupElement(elementIndex);
	}

	ArrayList<Node> tree{};
	ArrayList<Element> elements{};
	size_type elementCount = 0;
	float minOrthantSize = 0.0f;
	float halfRootSize = 0.0f;
	Point<N, float> rootCenter{};
	NodeIndex firstFreeNodeIndex = 0;
	ElementIndex firstFreeElementIndex = INVALID_ELEMENT;
};

/**
 * Opaque handle to an element in a LooseQuadtree.
 */
using LooseQuadtreeID = LooseOrthtreeID;

/**
 * Quadtree-based space subdivision container, optimized for intersection
 * queries between 2D axis-aligned boxes.
 *
 * \tparam T type of element to store in the tree.
 */
template <typename T>
using LooseQuadtree = LooseOrthtree<2, T>;

/**
 * Opaque handle to an element in a LooseOctree.
 */
using LooseOctreeID = LooseOrthtreeID;

/**
 * Octree-based space subdivision container, optimized for intersection
 * queries between 3D axis-aligned boxes.
 *
 * \tparam T type of element to store in the tree.
 */
template <typename T>
using LooseOctree = LooseOrthtree<3, T>;

} // namespace grem

#endif
