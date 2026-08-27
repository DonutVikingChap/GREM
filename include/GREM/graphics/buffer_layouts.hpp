// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_BUFFER_LAYOUTS_HPP
#define GREM_GRAPHICS_BUFFER_LAYOUTS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/ConstantString.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/ParameterDescription.hpp>

#include <compare> // std::strong_ordering

namespace grem::graphics {

/** Type-erased read-only reference to a UniformBufferLayout. */
struct UniformBufferLayoutReference {
	CStringView name;                                       ///< Unique name of the uniform buffer.
	Span<const ParameterDescription> parameterDescriptions; ///< Read-only view over the parameter descriptions of the parameter struct stored by the buffer.
};

/** Type-erased read-only reference to a StorageBufferLayout. */
struct StorageBufferLayoutReference {
	CStringView name;                               ///< Unique name of the storage buffer.
	Span<const FieldDescription> fieldDescriptions; ///< Read-only view over the field descriptions of the field struct stored by the buffer.
};

struct BufferLayoutReference; // Forward declaration.

/** Type-erased read-only reference to a BufferSetLayout. */
struct BufferSetLayoutReference {
	CStringView name;                                ///< Unique name of the buffer set.
	Span<const BufferLayoutReference> bufferLayouts; ///< Read-only view over the buffer layout references of the buffers in the set.
};

/** Generic type-erased read-only reference to a buffer or buffer set layout. */
struct BufferLayoutReference : Variant<UniformBufferLayoutReference, StorageBufferLayoutReference, BufferSetLayoutReference> {
	CRC32 nameCRC32; ///< CRC32 of the buffer name, used as a unique ID for the buffer type.

	/**
	 * Construct a reference to a uniform buffer layout.
	 *
	 * \param uniformBufferLayout uniform buffer layout to reference.
	 */
	constexpr BufferLayoutReference(UniformBufferLayoutReference uniformBufferLayout) noexcept
		: Variant(uniformBufferLayout)
		, nameCRC32(uniformBufferLayout.name) {}

	/**
	 * Construct a reference to a storage buffer layout.
	 *
	 * \param storageBufferLayout storage buffer layout to reference.
	 */
	constexpr BufferLayoutReference(StorageBufferLayoutReference storageBufferLayout) noexcept
		: Variant(storageBufferLayout)
		, nameCRC32(storageBufferLayout.name) {}

	/**
	 * Construct a reference to a buffer set layout.
	 *
	 * \param bufferSetLayout buffer set layout to reference.
	 */
	constexpr BufferLayoutReference(BufferSetLayoutReference bufferSetLayout) noexcept
		: Variant(bufferSetLayout)
		, nameCRC32(bufferSetLayout.name) {}

	/**
	 * Compare this buffer layout reference to another for equality.
	 *
	 * \param other buffer layout reference to compare this one to.
	 *
	 * \return true if the buffer layout references refer to the same buffer
	 *         type, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const BufferLayoutReference& other) const noexcept {
		return nameCRC32 == other.nameCRC32;
	}

	/**
	 * Compare this buffer layout reference to another.
	 *
	 * \param other the buffer layout reference to compare this one to.
	 *
	 * \return a strong ordering between the two buffer types.
	 */
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const BufferLayoutReference& other) const noexcept {
		return static_cast<uint32_t>(nameCRC32) <=> static_cast<uint32_t>(other.nameCRC32);
	}
};

/**
 * Layout description of a UniformBuffer.
 *
 * \tparam ParameterStruct user-defined struct of parameters stored by the
 *         buffer.
 * \tparam Name name of the uniform buffer. Must be globally unique among all
 *         buffer type instantiations.
 */
template <typename ParameterStruct, ConstantString Name>
struct UniformBufferLayout {
	/**
	 * Get a reference to the buffer layout.
	 *
	 * \return a type-erased read-only reference to the buffer layout.
	 */
	constexpr operator UniformBufferLayoutReference() const noexcept {
		return {.name = Name, .parameterDescriptions = detail::PARAMETER_DESCRIPTIONS<ParameterStruct>};
	}
};

/**
 * Layout description of a StorageBuffer.
 *
 * \tparam FieldStruct user-defined struct of fields stored by the buffer.
 * \tparam Name name of the storage buffer. Must be globally unique among all
 *         buffer type instantiations.
 */
template <typename FieldStruct, ConstantString Name>
struct StorageBufferLayout {
	/**
	 * Get a reference to the buffer layout.
	 *
	 * \return a type-erased read-only reference to the buffer layout.
	 */
	constexpr operator StorageBufferLayoutReference() const noexcept {
		return {.name = Name, .fieldDescriptions = detail::FIELD_DESCRIPTIONS<FieldStruct>};
	}
};

namespace detail {

template <typename... Buffers>
inline constexpr ConstantString BUFFER_SET_NAME = [] {
	ConstantString<char, ((Buffers::NAME.size() + 2) + ...)> result{};
	size_t i = 0;
	result[i++] = '{';
	for (const CStringView name : {Buffers::NAME...}) {
		for (const char ch : name) {
			result[i++] = ch;
		}
		if (i + 2 < result.size()) {
			result[i++] = ',';
			result[i++] = ' ';
		}
	}
	result[i++] = '}';
	GREM_ASSERT(i == result.size());
	return result;
}();

template <typename... Buffers>
inline constexpr Array<BufferLayoutReference, sizeof...(Buffers)> BUFFER_LAYOUT_REFERENCES{Buffers::LAYOUT_REFERENCE...};

} // namespace detail

/**
 * Layout description of a BufferSet.
 *
 * \tparam Buffers buffer types in the buffer set.
 */
template <typename... Buffers>
struct BufferSetLayout {
	/**
	 * Get a reference to the buffer layout.
	 *
	 * \return a type-erased read-only reference to the buffer layout.
	 */
	constexpr operator BufferSetLayoutReference() const noexcept {
		return {
			.name = detail::BUFFER_SET_NAME<Buffers...>,
			.bufferLayouts = detail::BUFFER_LAYOUT_REFERENCES<Buffers...>,
		};
	}
};

} // namespace grem::graphics

#endif
