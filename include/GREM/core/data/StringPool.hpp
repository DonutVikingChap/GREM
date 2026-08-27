// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_STRING_POOL_HPP
#define GREM_CORE_DATA_STRING_POOL_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/StringView.hpp>

#include <algorithm> // std::min, std::max
#include <bit>       // std::bit_ceil, std::bit_cast
#include <cstddef>   // std::byte, std::size_t, std::ptrdiff_t
#include <cstdint>   // std::uint32_t
#include <limits>    // std::numeric_limits
#include <stdexcept> // std::length_error
#include <utility>   // std::move

namespace grem {

struct StringID {
	using index_type = std::uint32_t;

	static const StringID EMPTY;
	static const StringID INVALID;

	[[nodiscard]] static constexpr StringID atExplicitOffset(index_type index) noexcept {
		return StringID{index};
	}

	StringID() noexcept = default;

	[[nodiscard]] constexpr bool operator==(const StringID&) const = default;
	[[nodiscard]] constexpr auto operator<=>(const StringID&) const = default;

	[[nodiscard]] constexpr index_type getIndex() const noexcept {
		return index;
	}

private:
	index_type index;

	constexpr explicit StringID(index_type index) noexcept
		: index(index) {}
};

inline constexpr StringID StringID::EMPTY{StringID::index_type{0}};
inline constexpr StringID StringID::INVALID{std::numeric_limits<StringID::index_type>::max()};

template <typename CharT, typename Allocator = std::allocator<CharT>>
class StringPoolBase {
private:
	using Length = std::uint32_t;

	static constexpr std::size_t PADDED_LENGTH_SIZE_IN_CHARS = (sizeof(Length) + sizeof(CharT) - 1) / sizeof(CharT);
	static constexpr std::size_t PADDED_LENGTH_SIZE_IN_BYTES = PADDED_LENGTH_SIZE_IN_CHARS * sizeof(CharT);

	struct LengthAsBytes {
		std::byte bytes[sizeof(Length)];
	};

	struct PaddedLengthAsBytes {
		std::byte bytes[PADDED_LENGTH_SIZE_IN_BYTES]{};

		constexpr PaddedLengthAsBytes() = default;

		constexpr explicit PaddedLengthAsBytes(LengthAsBytes lengthBytes) {
			for (std::size_t i = 0; i < PADDED_LENGTH_SIZE_IN_BYTES; ++i) {
				bytes[i] = lengthBytes.bytes[i];
			}
		}

		constexpr explicit operator LengthAsBytes() const {
			LengthAsBytes result{};
			for (std::size_t i = 0; i < sizeof(Length); ++i) {
				result.bytes[i] = bytes[i];
			}
			return result;
		}
	};

	struct LengthAsChars {
		CharT chars[PADDED_LENGTH_SIZE_IN_CHARS];

		constexpr void writeTo(CharT* output) const {
			for (std::size_t i = 0; i < PADDED_LENGTH_SIZE_IN_CHARS; ++i) {
				output[i] = chars[i];
			}
		}

		constexpr void readFrom(const CharT* input) {
			for (std::size_t i = 0; i < PADDED_LENGTH_SIZE_IN_CHARS; ++i) {
				chars[i] = input[i];
			}
		}
	};

public:
	using allocator_type = Allocator;

	static constexpr std::size_t MAX_STRING_LENGTH{std::numeric_limits<Length>::max()};

	constexpr StringPoolBase() noexcept
		: StringPoolBase(0) {}

	constexpr explicit StringPoolBase(std::size_t initialCapacity, const Allocator& allocator = Allocator())
		: hashTableSlots(std::bit_ceil(initialCapacity + (initialCapacity + 1) / 2), StringID::INVALID, allocator)
		, buffer(initialCapacity * 8, allocator) {}

	constexpr explicit StringPoolBase(const Allocator& allocator)
		: StringPoolBase(0, allocator) {}

	constexpr StringPoolBase(const StringPoolBase& other)
		: StringPoolBase(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator())) {}

	constexpr StringPoolBase(const StringPoolBase& other, const Allocator& allocator)
		: StringPoolBase(allocator) {
		*this = other;
	}

	constexpr StringPoolBase(StringPoolBase&& other) noexcept = default;

	constexpr StringPoolBase(StringPoolBase&& other, const Allocator& allocator) noexcept
		: StringPoolBase(allocator) {
		*this = std::move(other);
	}

	constexpr ~StringPoolBase() = default;

	constexpr StringPoolBase& operator=(const StringPoolBase& other) = default;
	constexpr StringPoolBase& operator=(StringPoolBase&& other) noexcept = default;

	constexpr void clear() noexcept {
		hashTableSlots.clear();
		hashTableOccupancy = 0;
		buffer.clear();
	}

	[[nodiscard]] constexpr StringID insert(StringViewBase<CharT> string) {
		if (string.size() > MAX_STRING_LENGTH) {
			throw std::length_error{"Maximum string length exceeded."};
		}
		if (hashTableOccupancy >= (hashTableSlots.size() / 3) * 2) {
			const std::size_t newHashTableCapacity = std::max(hashTableSlots.size() * 2, std::size_t{64});
			if (newHashTableCapacity <= hashTableSlots.size()) {
				throw std::length_error{"Maximum string pool hash table capacity exceeded."};
			}
			Buffer<StringID, StringIDAllocator> newHashTableSlots(newHashTableCapacity, StringID::INVALID, hashTableSlots.get_allocator());
			for (const StringID key : hashTableSlots) {
				if (key != StringID::INVALID) {
					const std::size_t hash = std::hash<StringViewBase<CharT>>{}((*this)[key]);
					std::size_t slotIndex = hash & (newHashTableCapacity - 1);
					while (newHashTableSlots[slotIndex] != StringID::INVALID) {
						slotIndex = (slotIndex + 1) & (newHashTableCapacity - 1);
					}
					newHashTableSlots[slotIndex] = key;
				}
			}
			hashTableSlots = std::move(newHashTableSlots);
		}
		if (hashTableOccupancy == 0 && !string.empty()) {
			[[maybe_unused]] const StringID emptyStringID = insert({});
			GREM_ASSERT(emptyStringID == StringID::EMPTY);
		}
		const std::size_t hash = std::hash<StringViewBase<CharT>>{}(string);
		std::size_t slotIndex = hash & (hashTableSlots.size() - 1);
		while (hashTableSlots[slotIndex] != StringID::INVALID && (*this)[hashTableSlots[slotIndex]] != string) {
			slotIndex = (slotIndex + 1) & (hashTableSlots.size() - 1);
		}
		StringID& slot = hashTableSlots[slotIndex];
		if (slot != StringID::INVALID) {
			return slot;
		}
		const std::size_t offset = buffer.size();
		const std::size_t addedSize = sizeof(LengthAsChars) + string.size() + 1;
		constexpr std::size_t MAX_SIZE = std::min(std::size_t{std::numeric_limits<std::ptrdiff_t>::max()}, std::size_t{std::numeric_limits<StringID::index_type>::max()});
		if (addedSize > MAX_SIZE || offset > MAX_SIZE - addedSize) {
			throw std::length_error{"Maximum string pool size exceeded."};
		}
		buffer.resize(buffer.size() + addedSize);
		const Length length = static_cast<Length>(string.size());
		const LengthAsChars lengthAsChars = std::bit_cast<LengthAsChars>(PaddedLengthAsBytes{std::bit_cast<LengthAsBytes>(length)});
		lengthAsChars.writeTo(buffer.data() + offset);
		string.copy(buffer.data() + offset + sizeof(LengthAsChars), string.size());
		buffer[offset + sizeof(LengthAsChars) + string.size()] = CharT();
		const StringID newStringID = StringID::atExplicitOffset(static_cast<StringID::index_type>(offset));
		slot = newStringID;
		++hashTableOccupancy;
		return newStringID;
	}

	[[nodiscard]] constexpr StringID find(StringViewBase<CharT> string) const noexcept {
		if (string.size() > MAX_STRING_LENGTH || hashTableSlots.empty()) {
			return {};
		}
		const std::size_t hash = std::hash<StringViewBase<CharT>>{}(string);
		std::size_t slotIndex = hash & (hashTableSlots.size() - 1);
		while (hashTableSlots[slotIndex] != StringID::INVALID && (*this)[hashTableSlots[slotIndex]] != string) {
			slotIndex = (slotIndex + 1) & (hashTableSlots.size() - 1);
		}
		return hashTableSlots[slotIndex];
	}

	[[nodiscard]] constexpr StringViewBase<CharT> operator[](StringID id) const {
		GREM_ASSERT(std::size_t{id.getIndex()} < buffer.size());
		LengthAsChars lengthAsChars{};
		GREM_ASSERT(std::size_t{id.getIndex()} + sizeof(LengthAsChars) < buffer.size());
		lengthAsChars.readFrom(buffer.data() + id.getIndex());
		const Length length = std::bit_cast<Length>(static_cast<LengthAsBytes>(std::bit_cast<PaddedLengthAsBytes>(lengthAsChars)));
		GREM_ASSERT(std::size_t{id.getIndex()} + sizeof(LengthAsChars) + std::size_t{length} < buffer.size());
		return StringViewBase<CharT>{buffer.data() + std::size_t{id.getIndex()} + sizeof(LengthAsChars), std::size_t{length}};
	}

	[[nodiscard]] constexpr const CharT* c_str(StringID id) const {
		GREM_ASSERT(std::size_t{id.getIndex()} < buffer.size());
		GREM_ASSERT(std::size_t{id.getIndex()} + sizeof(LengthAsChars) < buffer.size());
		return buffer.data() + std::size_t{id.getIndex()} + sizeof(LengthAsChars);
	}

	[[nodiscard]] constexpr allocator_type get_allocator() const noexcept {
		return buffer.get_allocator();
	}

private:
	using StringIDAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<StringID>;

	Buffer<StringID, StringIDAllocator> hashTableSlots;
	std::size_t hashTableOccupancy = 0;
	Buffer<CharT, Allocator> buffer;
};

using StringPool = StringPoolBase<char>;

} // namespace grem

template <>
struct std::hash<grem::StringID> {
	[[nodiscard]] std::size_t operator()(grem::StringID id) const {
		return indexHasher(id.getIndex());
	}

private:
	[[no_unique_address]] std::hash<grem::StringID::index_type> indexHasher;
};

#endif
