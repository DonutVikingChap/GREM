// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_LINEAR_BOUNDING_VOLUME_HIERARCHY_HPP
#define GREM_CORE_DATA_LINEAR_BOUNDING_VOLUME_HIERARCHY_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>

#include <algorithm>   // std::sort
#include <new>         // std::launder
#include <tuple>       // std::forward_as_tuple
#include <type_traits> // std::conditional_t, std::is_convertible_v
#include <utility>     // std::forward, std::move, std::exchange, std::piecewise_construct, std::declval

namespace grem {

/**
 * Binary tree-based space subdivision container, Z-ordered using morton codes,
 * optimized for intersection queries between axis-aligned boxes.
 *
 * \tparam N number of vector dimensions (must be 2 or 3).
 * \tparam T type of element to store in the tree.
 */
template <size_t N, typename T>
class LinearBoundingVolumeHierarchy {
public:
	static_assert(N == 2 || N == 3);

	using value_type = Pair<Box<N, float>, T>;                   ///< Value type of the container.
	using reference = Pair<const Box<N, float>, T>&;             ///< Reference type of the container.
	using const_reference = const Pair<const Box<N, float>, T>&; ///< Const reference type of the container.
	using pointer = Pair<const Box<N, float>, T>*;               ///< Pointer type of the container.
	using const_pointer = const Pair<const Box<N, float>, T>*;   ///< Const pointer type of the container.
	using size_type = size_t;                                    ///< Size type of the container.
	using difference_type = ptrdiff_t;                           ///< Difference type of the container.
	using iterator = pointer;                                    ///< Iterator type of the container.
	using const_iterator = const_pointer;                        ///< Const iterator type of the container.

	/**
	 * Construct an empty tree.
	 */
	LinearBoundingVolumeHierarchy() noexcept = default;

	/**
	 * Erase all inserted elements from the tree.
	 *
	 * \sa erase()
	 */
	void clear() noexcept {
		dirty = dirty || !elements.empty();
		elements.clear();
	}

	/**
     * Get the begin iterator of the elements in the tree.
     *
     * \return an iterator to the first element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] iterator begin() noexcept {
		return std::launder(reinterpret_cast<pointer>(elements.data()));
	}

	/**
     * Get the begin iterator of the elements in the tree.
     *
     * \return an iterator to the first element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] const_iterator begin() const noexcept {
		return std::launder(reinterpret_cast<const_pointer>(elements.data()));
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
		return begin() + elements.size();
	}

	/**
     * Get the end iterator of the elements in the tree.
     *
     * \return an iterator past the last element in the sequence of elements.
	 *
	 * \note The element order is unspecified.
     */
	[[nodiscard]] const_iterator end() const noexcept {
		return begin() + elements.size();
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
		return elements.size();
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
	 * \warning The returned iterator is invalidated on the next call to any
	 *          non-const member function on the tree.
	 *
	 * \sa insert()
	 */
	template <typename... Args>
	iterator emplace(const Box<N, float>& elementBoundingBox, Args&&... args) {
		if (elements.size() >= static_cast<size_t>(INVALID_ELEMENT)) {
			throw std::length_error{"Maximum element count exceeded."};
		}
		value_type& element = elements.emplace_back(std::piecewise_construct, std::forward_as_tuple(elementBoundingBox), std::forward_as_tuple(std::forward<Args>(args)...));
		dirty = true;
		return std::launder(reinterpret_cast<pointer>(&element));
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
	 * \warning The returned iterator is invalidated on the next call to any
	 *          non-const member function on the tree.
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
	 * \warning The returned iterator is invalidated on the next call to any
	 *          non-const member function on the tree.
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
	 * \sa clear()
	 */
	void erase(const_iterator pos) {
		dirty = true;
		elements.erase(elements.begin() + (pos - cbegin()));
	}

	/**
	 * Execute a callback function for each active node of the tree, including
	 * empty internal nodes without an element.
	 *
	 * \param callback function to execute, which should accept the following
	 *        parameters (though they don't need to be used):
	 *        - `const grem::Box<N, float>& boundingBox`: an axis-aligned box
	 *          that defines the region that an element's bounding box must be
	 *          intersecting in order to belong to the node.
	 *        - `const T* element`: a non-owning read-only pointer to the
	 *          element occupying the node, or nullptr if it does not have one.
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
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
	 *
	 * \sa traverseElements()
	 * \sa test()
	 */
	template <typename Callback, typename Predicate>
	auto traverseNodes(Callback&& callback, Predicate&& predicate) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		return traverseNodesImplementation(
			[this, callback = std::forward<Callback>(callback)](const Node& node) mutable {
				const T* const element = (node.elementIndex == INVALID_ELEMENT) ? nullptr : &elements[node.elementIndex].second;
				constexpr bool CALLBACK_RETURNS_BOOL = std::is_convertible_v<decltype(callback(node.boundingBox, element)), bool>;
				if constexpr (CALLBACK_RETURNS_BOOL) {
					return callback(node.boundingBox, element);
				} else {
					callback(node.boundingBox, element);
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
	 *        - `const grem::Box<N, float>& boundingBox`: an axis-aligned box
	 *          that defines the region that an element's bounding box must be
	 *          intersecting in order to belong to the node.
	 *        - `const T* element`: a non-owning read-only pointer to the
	 *          element occupying the node, or nullptr if it does not have one.
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
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
	 *
	 * \sa traverseNodes()
	 * \sa test()
	 */
	template <typename Callback, typename Predicate>
	auto traverseElements(Callback&& callback, Predicate&& predicate) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		return traverseNodesImplementation(
			[this, callback = std::forward<Callback>(callback)](const Node& node) mutable {
				constexpr bool CALLBACK_RETURNS_BOOL = std::is_convertible_v<decltype(callback(elements[node.elementIndex].second)), bool>;
				if (node.elementIndex != INVALID_ELEMENT) {
					if constexpr (CALLBACK_RETURNS_BOOL) {
						if (callback(elements[node.elementIndex].second)) {
							return true; // Callback requested early return.
						}
					} else {
						callback(elements[node.elementIndex].second);
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
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
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
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
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
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
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
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
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
	 * \warning Although it is sematically const, this function is not
	 *          thread-safe since it mutates an internal memory cache. Exclusive
	 *          access is therefore required for safety.
	 *
	 * \sa traverseNodes()
	 * \sa traverseElements()
	 */
	[[nodiscard]] bool test(const Box<N, float>& box) const noexcept {
		return traverseElements([](const T&) -> bool { return true; }, [&box](const Box<N, float>& boundingBox) -> bool { return intersects(boundingBox, box); });
	}

private:
	using NodeIndex = uint32_t;
	using ElementIndex = uint32_t;

	static constexpr NodeIndex INVALID_NODE = Limits<NodeIndex>::MAX;
	static constexpr ElementIndex INVALID_ELEMENT = Limits<ElementIndex>::MAX;

	struct Node {
		NodeIndex firstChildIndex = INVALID_NODE;
		NodeIndex secondChildIndex = INVALID_NODE;
		ElementIndex elementIndex = INVALID_ELEMENT;
		Box<N, float> boundingBox{};
	};

	[[nodiscard]] static uint32_t separateMortonCodeBits(uint32_t x) noexcept {
		if constexpr (N == 2) {
			x = (x ^ (x << 8)) & 0b00000000111111110000000011111111;
			x = (x ^ (x << 4)) & 0b00001111000011110000111100001111;
			x = (x ^ (x << 2)) & 0b00110011001100110011001100110011;
			x = (x ^ (x << 1)) & 0b01010101010101010101010101010101;
		} else if constexpr (N == 3) {
			x = (x ^ (x << 16)) & 0b11111111000000000000000011111111;
			x = (x ^ (x << 8)) & 0b00001111000000001111000000001111;
			x = (x ^ (x << 4)) & 0b11000011000011000011000011000011;
			x = (x ^ (x << 2)) & 0b01001001001001001001001001001001;
		}
		return x;
	}

	[[nodiscard]] static uint32_t getMortonCode(const Point<N, float>& normalizedPosition) noexcept {
		if constexpr (N == 2) {
			constexpr float MAX_EXTENT = 0b1111111111111111; // Use 16 bits, which fits in a uint32_t when expanded to 2 dimensions.
			const uint32_t x = separateMortonCodeBits(static_cast<uint32_t>(clamp(normalizedPosition.x * MAX_EXTENT, 0.0f, MAX_EXTENT)));
			const uint32_t y = separateMortonCodeBits(static_cast<uint32_t>(clamp(normalizedPosition.y * MAX_EXTENT, 0.0f, MAX_EXTENT)));
			return x | (y << 1);
		} else if constexpr (N == 3) {
			constexpr float MAX_EXTENT = 0b1111111111; // Use 10 bits, which fits in a uint32_t when expanded to 3 dimensions.
			const uint32_t x = separateMortonCodeBits(static_cast<uint32_t>(clamp(normalizedPosition.x * MAX_EXTENT, 0.0f, MAX_EXTENT)));
			const uint32_t y = separateMortonCodeBits(static_cast<uint32_t>(clamp(normalizedPosition.y * MAX_EXTENT, 0.0f, MAX_EXTENT)));
			const uint32_t z = separateMortonCodeBits(static_cast<uint32_t>(clamp(normalizedPosition.z * MAX_EXTENT, 0.0f, MAX_EXTENT)));
			return x | (y << 1) | (z << 2);
		}
	}

	[[nodiscard]] NodeIndex flushSubTree(Span<const Pair<uint32_t, ElementIndex>> elementIndices) const {
		if (elementIndices.empty()) {
			return INVALID_NODE;
		}

		if (tree.size() >= static_cast<size_t>(INVALID_NODE)) {
			throw std::length_error{"Maximum node count exceeded."};
		}
		const NodeIndex nodeIndex = static_cast<NodeIndex>(tree.size());
		tree.emplace_back();
		if (elementIndices.size() == 1) {
			Node& node = tree[nodeIndex];
			node.firstChildIndex = INVALID_NODE;
			node.secondChildIndex = INVALID_NODE;
			node.elementIndex = elementIndices.front().second;
			node.boundingBox = elements[elementIndices.front().second].first;
		} else {
			const size_t halfSize = elementIndices.size() / 2;
			const NodeIndex firstChildIndex = flushSubTree(elementIndices.first(halfSize));
			const NodeIndex secondChildIndex = flushSubTree(elementIndices.subspan(halfSize));
			GREM_ASSERT(firstChildIndex != INVALID_NODE);
			GREM_ASSERT(secondChildIndex != INVALID_NODE);
			Node& node = tree[nodeIndex];
			const Node& firstChild = tree[firstChildIndex];
			const Node& secondChild = tree[secondChildIndex];
			node.firstChildIndex = firstChildIndex;
			node.secondChildIndex = secondChildIndex;
			node.elementIndex = INVALID_ELEMENT;
			node.boundingBox = {
				.min = min(firstChild.boundingBox.min, secondChild.boundingBox.min),
				.max = max(firstChild.boundingBox.max, secondChild.boundingBox.max),
			};
		}
		return nodeIndex;
	}

	void flush() const {
		if (!dirty) {
			return;
		}

		Box<N, float> worldBoundingBox{.min{Limits<float>::MAX}, .max{Limits<float>::MIN}};
		for (const value_type& element : elements) {
			worldBoundingBox.min = min(worldBoundingBox.min, element.first.min);
			worldBoundingBox.max = max(worldBoundingBox.max, element.first.max);
		}

		const Point<N, float> worldCenter = (worldBoundingBox.min + worldBoundingBox.max) * 0.5f;
		const Length<N, float> worldExtents = worldBoundingBox.max - worldBoundingBox.min;
		const vec<N, float> inverseWorldExtents = 1.0f / worldExtents;

		elementIndicesOrderedByMortonCode.clear();
		for (size_t elementIndex = 0; elementIndex < elements.size(); ++elementIndex) {
			const Box<N, float>& boundingBox = elements[elementIndex].first;
			const Point<N, float> center = (boundingBox.min + boundingBox.max) * 0.5f;
			elementIndicesOrderedByMortonCode.push_back({getMortonCode((center + worldCenter) * inverseWorldExtents), static_cast<ElementIndex>(elementIndex)});
		}

		std::sort(elementIndicesOrderedByMortonCode.begin(), elementIndicesOrderedByMortonCode.end(),
			[](const Pair<uint32_t, ElementIndex>& a, const Pair<uint32_t, ElementIndex>& b) -> bool { return a.first < b.first; });

		tree.clear();
		[[maybe_unused]] const NodeIndex nodeIndex = flushSubTree(elementIndicesOrderedByMortonCode);
		GREM_ASSERT((nodeIndex == 0 && !tree.empty()) || (nodeIndex == INVALID_NODE && tree.empty()));

		dirty = false;
	}

	template <typename Callback, typename Predicate>
	auto traverseNodesImplementation(Callback&& callback, Predicate&& predicate) const { // NOLINT(cppcoreguidelines-missing-std-forward)
		constexpr bool CALLBACK_RETURNS_BOOL = std::is_convertible_v<decltype(callback(std::declval<const Node&>())), bool>;
		using CallbackResult = std::conditional_t<CALLBACK_RETURNS_BOOL, bool, void>;

		flush();

		const auto traverseSubTree = [&](const auto& self, NodeIndex nodeIndex) -> CallbackResult {
			const Node& node = tree[nodeIndex];
			if (predicate(node.boundingBox)) {
				if constexpr (CALLBACK_RETURNS_BOOL) {
					if (callback(node)) {
						return true; // Callback requested early return.
					}
				} else {
					callback(node);
				}

				if (node.firstChildIndex != INVALID_NODE) {
					if constexpr (CALLBACK_RETURNS_BOOL) {
						if (self(self, node.firstChildIndex)) {
							return true;
						}
					} else {
						self(self, node.firstChildIndex);
					}
				}
				if (node.secondChildIndex != INVALID_NODE) {
					if constexpr (CALLBACK_RETURNS_BOOL) {
						if (self(self, node.secondChildIndex)) {
							return true;
						}
					} else {
						self(self, node.secondChildIndex);
					}
				}
			}
			if constexpr (CALLBACK_RETURNS_BOOL) {
				return false;
			}
		};

		if (!tree.empty()) {
			return traverseSubTree(traverseSubTree, 0);
		}
		if constexpr (CALLBACK_RETURNS_BOOL) {
			return false;
		}
	}

	ArrayList<value_type> elements{};
	mutable ArrayList<Node> tree{};
	mutable Buffer<Pair<uint32_t, ElementIndex>> elementIndicesOrderedByMortonCode{};
	mutable bool dirty = false;
};

/**
 * Binary tree-based space subdivision container, optimized for intersection
 * queries between 2D axis-aligned boxes.
 *
 * \tparam T type of element to store in the tree.
 */
template <typename T>
using BoundingVolumeHierarchy2D = LinearBoundingVolumeHierarchy<2, T>;

/**
 * Binary tree-based space subdivision container, optimized for intersection
 * queries between 3D axis-aligned boxes.
 *
 * \tparam T type of element to store in the tree.
 */
template <typename T>
using BoundingVolumeHierarchy3D = LinearBoundingVolumeHierarchy<3, T>;

} // namespace grem

#endif
