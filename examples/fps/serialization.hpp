// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_SERIALIZATION_HPP
#define GREM_EXAMPLES_FPS_SERIALIZATION_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/SmallBuffer.hpp>
#include <GREM/core/data/Tuple.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/formats/CRC32.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/physics/objects.hpp>
#include <GREM/physics/quantities.hpp>

#include <type_traits> // std::is_empty_v, std::bool_constant, std::false_type, std::remove_cvref_t
#include <utility>     // std::declval, std::in_place_type_t

template <typename T>
struct packed_size : meta::Constant<sizeof(std::remove_cvref_t<T>)> {};

template <size_t N, typename UnitT>
struct packed_size<phys::Quantity<N, UnitT>> : meta::Constant<N * sizeof(float)> {};

template <>
struct packed_size<phys::ObjectActivity> : meta::Constant<sizeof(phys::ObjectActivity::value_type)> {};

template <typename... Ts>
struct packed_size<Tuple<Ts...>> : meta::Constant<(packed_size<std::remove_cvref_t<Ts>>::value + ... + size_t{0})> {};

template <typename T, size_t N>
struct packed_size<Array<T, N>> : meta::Constant<(packed_size<std::remove_cvref_t<T>>::value * N)> {};

template <typename T>
inline constexpr size_t packed_size_v = packed_size<T>::value;

template <typename T>
struct is_trivially_serializable : std::false_type {};

template <typename T>
requires(scalar<T> && !pointer<T> && !same_as<T, bool>) struct is_trivially_serializable<T> : std::bool_constant<sizeof(T) == 1 || HOST_IS_LITTLE_ENDIAN> {};

template <typename T>
requires(requires { typename T::TriviallySerializableTag; }) struct is_trivially_serializable<T> : std::bool_constant<HOST_IS_LITTLE_ENDIAN> {};

template <size_t N, typename UnitT>
struct is_trivially_serializable<phys::Quantity<N, UnitT>> : std::bool_constant<HOST_IS_LITTLE_ENDIAN && sizeof(phys::Quantity<N, UnitT>) == N * sizeof(float)> {};

template <size_t N>
struct is_trivially_serializable<phys::Orientation<N>> : std::bool_constant<HOST_IS_LITTLE_ENDIAN && sizeof(phys::Orientation<N>) == sizeof(quat)> {};

template <>
struct is_trivially_serializable<phys::ObjectActivity>
	: std::bool_constant<is_trivially_serializable<phys::ObjectActivity::value_type>::value && sizeof(phys::ObjectActivity) == sizeof(phys::ObjectActivity::value_type)> {};

template <>
struct is_trivially_serializable<Duration>
	: std::bool_constant<HOST_IS_LITTLE_ENDIAN && sizeof(Duration) == sizeof(Duration::rep) && alignof(Duration) == alignof(Duration::rep) && same_as<Duration::rep, int64_t> &&
						 same_as<Duration::period, Nanoseconds::period>> {};

template <>
struct is_trivially_serializable<rng::Xoroshiro128PlusPlusEngine> : std::bool_constant<HOST_IS_LITTLE_ENDIAN> {};

template <>
struct is_trivially_serializable<CRC32> : std::bool_constant<HOST_IS_LITTLE_ENDIAN> {};

template <typename T, size_t N>
struct is_trivially_serializable<Array<T, N>>
	: std::bool_constant<(N > 0 && is_trivially_serializable<std::remove_cvref_t<T>>::value && sizeof(Array<T, N>) == packed_size_v<Array<T, N>>)> {};

template <typename... Ts>
struct is_trivially_serializable<Tuple<Ts...>> : std::bool_constant<(is_trivially_serializable<std::remove_cvref_t<Ts>>::value && ...)> {};

template <aggregate T>
requires(!std::is_empty_v<T> && !requires { typename T::TriviallySerializableTag; }) struct is_trivially_serializable<T>
	: std::bool_constant<HOST_IS_LITTLE_ENDIAN && sizeof(T) == packed_size_v<std::remove_cvref_t<decltype(meta::getFields(std::declval<T>()))>> &&
						 is_trivially_serializable<decltype(meta::getFields(std::declval<T>()))>::value> {};

template <typename T>
inline constexpr bool is_trivially_serializable_v = is_trivially_serializable<T>::value;

template <typename T>
concept trivially_serializable = is_trivially_serializable_v<T>;

template <typename T>
inline void serialize(const T& value, Writer output) requires(requires { value.serializeTo(output); }) {
	value.serializeTo(output);
}

template <typename T>
[[nodiscard]] inline bool deserialize(T& value, SpanReader input) requires(requires { value.deserializeFrom(input); }) {
	return value.deserializeFrom(input);
}

template <typename T>
inline void serialize(T value, Writer output) requires(scalar<T> && !pointer<T> && !same_as<T, bool>) {
	value = convertHostEndianToLittleEndian(value);
	output.write(asBytes(Span{&value, 1}));
}

template <typename T>
[[nodiscard]] inline bool deserialize(T& value, SpanReader input) requires(scalar<T> && !pointer<T> && !same_as<T, bool>) {
	if (!input.tryRead(asWritableBytes(Span{&value, 1}))) {
		return false;
	}
	value = convertLittleEndianToHostEndian(value);
	return true;
}

inline void serialize(CRC32 value, Writer output) {
	output.writeUInt32LE(static_cast<uint32_t>(value));
}

[[nodiscard]] inline bool deserialize(CRC32& value, SpanReader input) {
	const Optional<uint32_t> integer = input.tryReadUInt32LE();
	if (!integer) {
		return false;
	}
	value = CRC32{*integer};
	return true;
}

inline void serialize(bool value, Writer output) {
	output.writeUInt8((value) ? uint8_t{1} : uint8_t{0});
}

[[nodiscard]] inline bool deserialize(bool& value, SpanReader input) {
	const Optional<uint8_t> integer = input.tryReadUInt8();
	if (!integer || (*integer != 0 && *integer != 1)) {
		return false;
	}
	value = *integer != 0;
	return true;
}

template <size_t N, typename T>
inline void serialize(vec<N, T> value, Writer output) {
	if constexpr (trivially_serializable<vec<N, T>>) {
		output.write(asBytes(Span{&value, 1}));
	} else {
		meta::forEachIndex<N>([&](auto i) -> void { serialize(value[i], output); });
	}
}

template <size_t N, typename T>
[[nodiscard]] inline bool deserialize(vec<N, T>& value, SpanReader input) {
	if constexpr (trivially_serializable<vec<N, T>>) {
		return input.tryRead(asWritableBytes(Span{&value, 1}));
	} else {
		bool success = true;
		meta::forEachIndex<N>([&](auto i) -> void { success = success && deserialize(value[i], input); });
		return success;
	}
}

inline void serialize(quat value, Writer output) {
	serialize(value.x, output), serialize(value.y, output), serialize(value.z, output), serialize(value.w, output);
}

[[nodiscard]] inline bool deserialize(quat& value, SpanReader input) {
	return deserialize(value.x, input) && deserialize(value.y, input) && deserialize(value.z, input) && deserialize(value.w, input);
}

template <size_t N, typename UnitT, size_t Index>
inline void serialize(phys::ConstComponentProxy<N, UnitT, Index> value, Writer output) {
	serialize(float{value.in(UnitT{})}, output);
}

template <size_t N, typename UnitT, size_t Index>
[[nodiscard]] inline bool deserialize(phys::ComponentProxy<N, UnitT, Index> value, SpanReader input) {
	float component{};
	if (!deserialize(component, input)) {
		return false;
	}
	value = phys::Quantity<1, UnitT>::reinterpret(component);
	return true;
}

template <size_t N, typename UnitT>
inline void serialize(phys::Quantity<N, UnitT> value, Writer output) {
	if constexpr (trivially_serializable<phys::Quantity<N, UnitT>>) {
		output.write(asBytes(Span{&value, 1}));
	} else if constexpr (N == 1) {
		serialize(float{value.in(UnitT{})}, output);
	} else {
		meta::forEachIndex<N>([&](auto i) -> void { serialize(value[i], output); });
	}
}

template <size_t N, typename UnitT>
[[nodiscard]] inline bool deserialize(phys::Quantity<N, UnitT>& value, SpanReader input) {
	if constexpr (trivially_serializable<phys::Quantity<N, UnitT>>) {
		return input.tryRead(asWritableBytes(Span{&value, 1}));
	} else if constexpr (N == 1) {
		float component{};
		if (!deserialize(component, input)) {
			return false;
		}
		value = phys::Quantity<1, UnitT>::reinterpret(component);
		return true;
	} else {
		bool success = true;
		meta::forEachIndex<N>([&](auto i) -> void { success = success && deserialize(value[i], input); });
		return success;
	}
}

inline void serialize(phys::Orientation2D value, Writer output) {
	serialize(float{value}, output);
}

[[nodiscard]] inline bool deserialize(phys::Orientation2D& value, SpanReader input) {
	float angle{};
	if (!deserialize(angle, input)) {
		return false;
	}
	value = phys::Orientation2D{angle};
	return true;
}

inline void serialize(phys::Orientation3D value, Writer output) {
	serialize(quat{value}, output);
}

[[nodiscard]] inline bool deserialize(phys::Orientation3D& value, SpanReader input) {
	quat quaternion{};
	if (!deserialize(quaternion, input)) {
		return false;
	}
	value = phys::Orientation3D{quaternion};
	return true;
}

inline void serialize(phys::ObjectActivity value, Writer output) {
	phys::ObjectActivity::value_type bits{};
	bits |= static_cast<phys::ObjectActivity::value_type>((phys::ObjectActivity::value_type{value.isCorrectable} << 7));
	bits |= static_cast<phys::ObjectActivity::value_type>((phys::ObjectActivity::value_type{value.wasCorrected} << 6));
	bits |= phys::ObjectActivity::value_type{value.energyLevel};
	serialize(bits, output);
}

[[nodiscard]] inline bool deserialize(phys::ObjectActivity& value, SpanReader input) {
	phys::ObjectActivity::value_type bits{};
	if (!deserialize(bits, input)) {
		return false;
	}
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
	value.isCorrectable = static_cast<phys::ObjectActivity::value_type>((bits >> 7) & 1);
	value.wasCorrected = static_cast<phys::ObjectActivity::value_type>((bits >> 6) & 1);
	value.energyLevel = static_cast<phys::ObjectActivity::value_type>(bits & 0b00111111);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	return true;
}

inline void serialize(Duration value, Writer output) {
	serialize(static_cast<int64_t>(duration_cast<Nanoseconds>(value).count()), output);
}

[[nodiscard]] inline bool deserialize(Duration& value, SpanReader input) {
	int64_t nanoseconds{};
	if (!deserialize(nanoseconds, input)) {
		return false;
	}
	value = duration_cast<Duration>(Nanoseconds{static_cast<Nanoseconds::rep>(nanoseconds)});
	return true;
}

inline void serialize(const rng::Xoroshiro128PlusPlusEngine& numberGenerator, Writer output) {
	const rng::Xoroshiro128PlusPlusEngine::State state = numberGenerator.getState();
	serialize(state[0], output), serialize(state[1], output);
}

[[nodiscard]] inline bool deserialize(rng::Xoroshiro128PlusPlusEngine& numberGenerator, SpanReader input) {
	rng::Xoroshiro128PlusPlusEngine::State state{};
	if (!deserialize(state[0], input) || !deserialize(state[1], input)) {
		return false;
	}
	numberGenerator.setState(state);
	return true;
}

inline void serialize(Color value, Writer output) {
	serialize(value.toLinearRGBA(), output);
}

[[nodiscard]] inline bool deserialize(Color& value, SpanReader input) {
	vec4 rgba{};
	if (!deserialize(rgba, input)) {
		return false;
	}
	value = Color::fromLinear(rgba);
	return true;
}

inline void serialize(const String& value, Writer output) {
	output.writeUIntLEB128(value.size());
	output.write(asBytes(Span{value}));
}

[[nodiscard]] inline bool deserialize(String& value, SpanReader input) {
	const Optional<uint64_t> size = input.tryReadUIntLEB128();
	if (!size || *size > uint64_t{value.max_size()}) {
		return false;
	}
	value.resize(static_cast<size_t>(*size));
	return input.tryRead(asWritableBytes(Span{value}));
}

template <typename T>
void serialize(const Optional<T>& value, Writer output);

template <typename T>
[[nodiscard]] bool deserialize(Optional<T>& value, SpanReader input);

template <typename... Ts>
void serialize(const Variant<Ts...>& value, Writer output);

template <typename... Ts>
[[nodiscard]] bool deserialize(Variant<Ts...>& value, SpanReader input);

template <typename T, size_t N>
void serialize(const Array<T, N>& value, Writer output);

template <typename T, size_t N>
[[nodiscard]] bool deserialize(Array<T, N>& value, SpanReader input);

template <typename T>
void serialize(const ArrayList<T>& value, Writer output);

template <typename T>
[[nodiscard]] bool deserialize(ArrayList<T>& value, SpanReader input);

template <typename T>
void serialize(const Buffer<T>& value, Writer output);

template <typename T>
[[nodiscard]] bool deserialize(Buffer<T>& value, SpanReader input);

template <typename T, size_t N>
void serialize(const SmallArrayList<T, N>& value, Writer output);

template <typename T, size_t N>
[[nodiscard]] bool deserialize(SmallArrayList<T, N>& value, SpanReader input);

template <typename T, size_t N>
void serialize(const SmallBuffer<T, N>& value, Writer output);

template <typename T, size_t N>
[[nodiscard]] bool deserialize(SmallBuffer<T, N>& value, SpanReader input);

template <typename T>
void serialize(const RingBuffer<T>& value, Writer output);

template <typename T>
[[nodiscard]] bool deserialize(RingBuffer<T>& value, SpanReader input);

template <typename T>
void serialize(const DoubleEndedQueue<T>& value, Writer output);

template <typename T>
[[nodiscard]] bool deserialize(DoubleEndedQueue<T>& value, SpanReader input);

template <typename T1, typename T2>
void serialize(const Pair<T1, T2>& value, Writer output);

template <typename T1, typename T2>
[[nodiscard]] bool deserialize(Pair<T1, T2>& value, SpanReader input);

template <aggregate Aggregate>
inline void serialize(const Aggregate& value, Writer output) requires(!requires { value.serializeTo(output); }) {
	if constexpr (trivially_serializable<Aggregate>) {
		output.write(asBytes(Span{&value, 1}));
	} else {
		meta::forEachField(value, [&](const auto& field) -> void { serialize(field, output); });
	}
}

template <aggregate Aggregate>
[[nodiscard]] inline bool deserialize(Aggregate& value, SpanReader input) requires(!requires { value.deserializeFrom(input); }) {
	if constexpr (trivially_serializable<Aggregate>) {
		return input.tryRead(asWritableBytes(Span{&value, 1}));
	} else {
		bool success = true;
		meta::forEachField(value, [&](auto& field) -> void { success = success && deserialize(field, input); });
		return success;
	}
}

template <typename T>
inline void serialize(const Optional<T>& value, Writer output) {
	serialize(value.has_value(), output);
	if (value) {
		serialize(*value, output);
	}
}

template <typename T>
inline bool deserialize(Optional<T>& value, SpanReader input) {
	bool hasValue{};
	if (!deserialize(hasValue, input)) {
		return false;
	}
	if (hasValue) {
		value.emplace();
		return deserialize(*value, input);
	}
	value.reset();
	return true;
}

template <typename... Ts>
inline void serialize(const Variant<Ts...>& value, Writer output) {
	using Index = typename Variant<Ts...>::index_type;
	const Index index = value.index();
	serialize(index, output);
	match(value)([&](const auto& value) -> void { serialize(value, output); });
}

template <typename... Ts>
inline bool deserialize(Variant<Ts...>& value, SpanReader input) {
	using Index = typename Variant<Ts...>::index_type;
	Index index{};
	if (!deserialize(index, input)) {
		return false;
	}
	return Variant<Ts...>::visitIndex(index,
		Overloaded{
			[&]<typename T>(std::in_place_type_t<T>) -> bool { //
				return deserialize(value.template emplace<T>(), input);
			},
			[&]() -> bool { return false; },
		});
}

template <typename T, size_t N>
inline void serialize(const Array<T, N>& value, Writer output) {
	if constexpr (trivially_serializable<Array<T, N>>) {
		output.write(asBytes(Span{value}));
	} else {
		for (const T& element : value) {
			serialize(element, output);
		}
	}
}

template <typename T, size_t N>
inline bool deserialize(Array<T, N>& value, SpanReader input) {
	if constexpr (trivially_serializable<Array<T, N>>) {
		return input.tryRead(asWritableBytes(Span{value}));
	} else {
		for (T& element : value) {
			if (!deserialize(element, input)) {
				return false;
			}
		}
		return true;
	}
}

template <typename T>
inline void serialize(const ArrayList<T>& value, Writer output) {
	output.writeUIntLEB128(value.size());
	for (const T& element : value) {
		serialize(element, output);
	}
}

template <typename T>
inline bool deserialize(ArrayList<T>& value, SpanReader input) {
	const Optional<uint64_t> size = input.tryReadUIntLEB128();
	if (!size || *size > uint64_t{value.max_size()}) {
		return false;
	}
	value.resize(static_cast<size_t>(*size));
	for (T& element : value) {
		if (!deserialize(element, input)) {
			return false;
		}
	}
	return true;
}

template <typename T>
inline void serialize(const Buffer<T>& value, Writer output) {
	output.writeUIntLEB128(value.size());
	for (const T& element : value) {
		serialize(element, output);
	}
}

template <typename T>
inline bool deserialize(Buffer<T>& value, SpanReader input) {
	const Optional<uint64_t> size = input.tryReadUIntLEB128();
	if (!size || *size > uint64_t{value.max_size()}) {
		return false;
	}
	value.resize(static_cast<size_t>(*size));
	for (T& element : value) {
		if (!deserialize(element, input)) {
			return false;
		}
	}
	return true;
}

template <typename T, size_t N>
inline void serialize(const SmallArrayList<T, N>& value, Writer output) {
	output.writeUIntLEB128(value.size());
	for (const T& element : value) {
		serialize(element, output);
	}
}

template <typename T, size_t N>
inline bool deserialize(SmallArrayList<T, N>& value, SpanReader input) {
	const Optional<uint64_t> size = input.tryReadUIntLEB128();
	if (!size || *size > uint64_t{value.max_size()}) {
		return false;
	}
	value.resize(static_cast<size_t>(*size));
	for (T& element : value) {
		if (!deserialize(element, input)) {
			return false;
		}
	}
	return true;
}

template <typename T, size_t N>
inline void serialize(const SmallBuffer<T, N>& value, Writer output) {
	output.writeUIntLEB128(value.size());
	for (const T& element : value) {
		serialize(element, output);
	}
}

template <typename T, size_t N>
inline bool deserialize(SmallBuffer<T, N>& value, SpanReader input) {
	const Optional<uint64_t> size = input.tryReadUIntLEB128();
	if (!size || *size > uint64_t{value.max_size()}) {
		return false;
	}
	value.resize(static_cast<size_t>(*size));
	for (T& element : value) {
		if (!deserialize(element, input)) {
			return false;
		}
	}
	return true;
}

template <typename T>
inline void serialize(const RingBuffer<T>& value, Writer output) {
	output.writeUIntLEB128(value.size());
	for (const T& element : value) {
		serialize(element, output);
	}
}

template <typename T>
inline bool deserialize(RingBuffer<T>& value, SpanReader input) {
	const Optional<uint64_t> size = input.tryReadUIntLEB128();
	if (!size || *size > uint64_t{value.max_size()}) {
		return false;
	}
	value.resize(static_cast<size_t>(*size));
	for (T& element : value) {
		if (!deserialize(element, input)) {
			return false;
		}
	}
	return true;
}

template <typename T>
inline void serialize(const DoubleEndedQueue<T>& value, Writer output) {
	output.writeUIntLEB128(value.size());
	for (const T& element : value) {
		serialize(element, output);
	}
}

template <typename T>
inline bool deserialize(DoubleEndedQueue<T>& value, SpanReader input) {
	const Optional<uint64_t> size = input.tryReadUIntLEB128();
	if (!size || *size > uint64_t{value.max_size()}) {
		return false;
	}
	value.resize(static_cast<size_t>(*size));
	for (T& element : value) {
		if (!deserialize(element, input)) {
			return false;
		}
	}
	return true;
}

template <typename T1, typename T2>
void serialize(const Pair<T1, T2>& value, Writer output) {
	serialize(value.first, output), serialize(value.second, output);
}

template <typename T1, typename T2>
[[nodiscard]] bool deserialize(Pair<T1, T2>& value, SpanReader input) {
	return deserialize(value.first, input) && deserialize(value.second, input);
}

#endif
