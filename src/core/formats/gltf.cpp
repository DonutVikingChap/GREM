// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Arena.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/base64.hpp>
#include <GREM/core/formats/gltf.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formats/uri.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>

#include <charconv>     // std::from_chars_result, std::from_chars
#include <new>          // std::launder
#include <stdexcept>    // std::invalid_argument, std::out_of_range
#include <system_error> // std::errc
#include <type_traits>  // std::underlying_type_t
#include <utility>      // std::move

namespace grem::gltf {

namespace {

[[nodiscard]] size_t appendToPath(String& path, const auto&... newSuffixes) {
	const size_t previousPathLength = path.size();
	(path.append(newSuffixes), ...);
	return previousPathLength;
}

void resetPathTo(String& path, size_t previousPathLength) {
	GREM_ASSERT(path.size() >= previousPathLength);
	path.erase(previousPathLength);
}

template <typename T>
struct Parser;

template <typename T>
[[nodiscard]] T parseRequiredProperty(String& path, pmr::json::Object& object, StringView propertyName) {
	const size_t previousPathLength = appendToPath(path, ".", propertyName);
	T result = Parser<T>::parse(path, object.at(propertyName));
	resetPathTo(path, previousPathLength);
	return result;
}

template <typename T>
[[nodiscard]] Optional<T> parseOptionalProperty(String& path, pmr::json::Object& object, StringView propertyName) {
	if (const auto it = object.find(propertyName); it != object.end()) {
		const size_t previousPathLength = appendToPath(path, ".", propertyName);
		T result = Parser<T>::parse(path, it->second);
		resetPathTo(path, previousPathLength);
		return result;
	}
	return {};
}

[[nodiscard]] json::Value copyValue(const pmr::json::Value& value) {
	json::Value result{};
	GREM_MATCH(value) {
		GREM_CASE(json::Null null) break;
		GREM_CASE(json::Boolean boolean) {
			result = boolean;
			break;
		}
		GREM_CASE(const pmr::json::String& string) {
			result.emplace<json::String>(string);
			break;
		}
		GREM_CASE(json::Number number) {
			result = number;
			break;
		}
		GREM_CASE(const pmr::json::Object& object) {
			json::Object resultObject = result.emplace<json::Object>();
			resultObject.reserve(object.size());
			for (const auto& [propertyKey, propertyValue] : object) {
				resultObject.emplace(json::String{propertyKey}, copyValue(propertyValue));
			}
			break;
		}
		GREM_CASE(const pmr::json::Array& array) {
			json::Array resultArray = result.emplace<json::Array>();
			resultArray.reserve(array.size());
			for (const pmr::json::Value& item : array) {
				resultArray.push_back(copyValue(item));
			}
			break;
		}
	}
	return result;
}

[[nodiscard]] json::Value getOptionalValue(pmr::json::Object& object, StringView propertyName) {
	if (const auto it = object.find(propertyName); it != object.end()) {
		return copyValue(it->second);
	}
	return {};
}

template <>
struct Parser<String> {
	[[nodiscard]] static String parse(String&, pmr::json::Value& value) {
		return String{value.get<pmr::json::String>()};
	}
};

template <>
struct Parser<bool> {
	[[nodiscard]] static bool parse(String&, pmr::json::Value& value) {
		return value.get<json::Boolean>();
	}
};

template <>
struct Parser<float> {
	[[nodiscard]] static float parse(String&, pmr::json::Value& value) {
		return static_cast<float>(value.get<json::Number>());
	}
};

template <>
struct Parser<size_t> {
	[[nodiscard]] static size_t parse(String&, pmr::json::Value& value) {
		const json::Number number = value.get<json::Number>();
		if (number < 0 || trunc(number) != number || number > static_cast<json::Number>(Limits<size_t>::MAX)) {
			throw std::invalid_argument{"Value is not a valid integer."};
		}
		return static_cast<size_t>(number);
	}
};

template <typename T>
struct Parser<ArrayList<T>> {
	[[nodiscard]] static ArrayList<T> parse(String& path, pmr::json::Value& value) {
		ArrayList<T> result{};
		pmr::json::Array& array = value.get<pmr::json::Array>();
		result.reserve(array.size());
		for (size_t i = 0; i < array.size(); ++i) {
			const size_t previousPathLength = appendToPath(path, formatString("[{}]", i));
			result.push_back(Parser<T>::parse(path, array[i]));
			resetPathTo(path, previousPathLength);
		}
		return result;
	}
};

template <typename T, size_t N>
struct Parser<Array<T, N>> {
	[[nodiscard]] static Array<T, N> parse(String& path, pmr::json::Value& value) {
		Array<T, N> result{};
		pmr::json::Array& array = value.get<pmr::json::Array>();
		for (size_t i = 0; i < array.size(); ++i) {
			const size_t previousPathLength = appendToPath(path, formatString("[{}]", i));
			if (i >= N) {
				throw std::invalid_argument{"Too many values in array."};
			}
			result[i] = Parser<T>::parse(path, array[i]);
			resetPathTo(path, previousPathLength);
		}
		return result;
	}
};

template <size_t N, typename T>
struct Parser<vec<N, T>> {
	[[nodiscard]] static vec<N, T> parse(String& path, pmr::json::Value& value) {
		pmr::json::Array& array = value.get<pmr::json::Array>();
		if (array.size() != N) {
			throw std::invalid_argument{"Invalid number of values in array."};
		}
		vec<N, T> result{};
		for (size_t i = 0; i < N; ++i) {
			const size_t previousPathLength = appendToPath(path, formatString("[{}]", i));
			result[i] = Parser<T>::parse(path, array[i]);
			resetPathTo(path, previousPathLength);
		}
		return result;
	}
};

template <size_t C, size_t R, typename T>
struct Parser<mat<C, R, T>> {
	[[nodiscard]] static mat<C, R, T> parse(String& path, pmr::json::Value& value) {
		pmr::json::Array& array = value.get<pmr::json::Array>();
		if (array.size() != C * R) {
			throw std::invalid_argument{"Invalid number of values in array."};
		}
		mat<C, R, T> result{};
		for (size_t column = 0; column < C; ++column) {
			for (size_t row = 0; row < R; ++row) {
				const size_t previousPathLength = appendToPath(path, formatString("[{}]", column * R + row));
				result[column][row] = Parser<T>::parse(path, array[column * R + row]);
				resetPathTo(path, previousPathLength);
			}
		}
		return result;
	}
};

template <typename T>
struct Parser<qua<T>> {
	[[nodiscard]] static qua<T> parse(String& path, pmr::json::Value& value) {
		pmr::json::Array& array = value.get<pmr::json::Array>();
		if (array.size() != 4) {
			throw std::invalid_argument{"Invalid number of values in array."};
		}
		qua<T> result{};
		{
			const size_t previousPathLength = appendToPath(path, "[0]");
			result.x = Parser<T>::parse(path, array[0]);
			resetPathTo(path, previousPathLength);
		}
		{
			const size_t previousPathLength = appendToPath(path, "[1]");
			result.y = Parser<T>::parse(path, array[1]);
			resetPathTo(path, previousPathLength);
		}
		{
			const size_t previousPathLength = appendToPath(path, "[2]");
			result.z = Parser<T>::parse(path, array[2]);
			resetPathTo(path, previousPathLength);
		}
		{
			const size_t previousPathLength = appendToPath(path, "[3]");
			result.w = Parser<T>::parse(path, array[3]);
			resetPathTo(path, previousPathLength);
		}
		return result;
	}
};

template <>
struct Parser<Accessor::ComponentType> {
	[[nodiscard]] static Accessor::ComponentType parse(String& path, pmr::json::Value& value) {
		const size_t index = Parser<size_t>::parse(path, value);
		if (index < 5120 || index > 5126) {
			throw std::invalid_argument{"Unknown accessor component type."};
		}
		return static_cast<Accessor::ComponentType>(static_cast<std::underlying_type_t<Accessor::ComponentType>>(index));
	}
};

template <>
struct Parser<Accessor::Type> {
	[[nodiscard]] static Accessor::Type parse(String&, pmr::json::Value& value) {
		const pmr::json::String string = value.get<pmr::json::String>();
		if (string == "SCALAR") {
			return Accessor::Type::SCALAR;
		}
		if (string == "VEC2") {
			return Accessor::Type::VEC2;
		}
		if (string == "VEC3") {
			return Accessor::Type::VEC3;
		}
		if (string == "VEC4") {
			return Accessor::Type::VEC4;
		}
		if (string == "MAT2") {
			return Accessor::Type::MAT2;
		}
		if (string == "MAT3") {
			return Accessor::Type::MAT3;
		}
		if (string == "MAT4") {
			return Accessor::Type::MAT4;
		}
		throw std::invalid_argument{formatString("Unknown accessor type \"{}\".", string)};
	}
};

template <>
struct Parser<Accessor::Sparse::Indices::ComponentType> {
	[[nodiscard]] static Accessor::Sparse::Indices::ComponentType parse(String& path, pmr::json::Value& value) {
		const size_t index = Parser<size_t>::parse(path, value);
		if (index != 5121 && index != 5123 && index != 5125) {
			throw std::invalid_argument{"Unknown accessor sparce indices component type."};
		}
		return static_cast<Accessor::Sparse::Indices::ComponentType>(static_cast<std::underlying_type_t<Accessor::Sparse::Indices::ComponentType>>(index));
	}
};

template <>
struct Parser<Accessor::Sparse::Indices> {
	[[nodiscard]] static Accessor::Sparse::Indices parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.bufferView = parseRequiredProperty<BufferViewIndex>(path, object, "bufferView"),
			.byteOffset = parseOptionalProperty<size_t>(path, object, "byteOffset").value_or(0),
			.componentType = parseRequiredProperty<Accessor::Sparse::Indices::ComponentType>(path, object, "componentType"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Accessor::Sparse::Values> {
	[[nodiscard]] static Accessor::Sparse::Values parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.bufferView = parseRequiredProperty<BufferViewIndex>(path, object, "bufferView"),
			.byteOffset = parseOptionalProperty<size_t>(path, object, "byteOffset").value_or(0),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Accessor::Sparse> {
	[[nodiscard]] static Accessor::Sparse parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.count = parseRequiredProperty<size_t>(path, object, "count"),
			.indices = parseRequiredProperty<Accessor::Sparse::Indices>(path, object, "indices"),
			.values = parseRequiredProperty<Accessor::Sparse::Values>(path, object, "values"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Accessor> {
	[[nodiscard]] static Accessor parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.bufferView = parseOptionalProperty<BufferViewIndex>(path, object, "bufferView"),
			.byteOffset = parseOptionalProperty<size_t>(path, object, "byteOffset").value_or(0),
			.componentType = parseRequiredProperty<Accessor::ComponentType>(path, object, "componentType"),
			.normalized = parseOptionalProperty<bool>(path, object, "normalized").value_or(false),
			.count = parseRequiredProperty<size_t>(path, object, "count"),
			.type = parseRequiredProperty<Accessor::Type>(path, object, "type"),
			.max = parseOptionalProperty<Array<float, 16>>(path, object, "max"),
			.min = parseOptionalProperty<Array<float, 16>>(path, object, "min"),
			.sparse = parseOptionalProperty<Accessor::Sparse>(path, object, "sparse"),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Animation::Channel::Target::Path> {
	[[nodiscard]] static Animation::Channel::Target::Path parse(String&, pmr::json::Value& value) {
		const pmr::json::String string = value.get<pmr::json::String>();
		if (string == "weights") {
			return Animation::Channel::Target::Path::WEIGHTS;
		}
		if (string == "translation") {
			return Animation::Channel::Target::Path::TRANSLATION;
		}
		if (string == "rotation") {
			return Animation::Channel::Target::Path::ROTATION;
		}
		if (string == "scale") {
			return Animation::Channel::Target::Path::SCALE;
		}
		throw std::invalid_argument{formatString("Unknown animation channel target path type \"{}\".", string)};
	}
};

template <>
struct Parser<Animation::Channel::Target> {
	[[nodiscard]] static Animation::Channel::Target parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.node = parseOptionalProperty<NodeIndex>(path, object, "node"),
			.path = parseRequiredProperty<Animation::Channel::Target::Path>(path, object, "path"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Animation::Channel> {
	[[nodiscard]] static Animation::Channel parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.sampler = parseRequiredProperty<Animation::SamplerIndex>(path, object, "sampler"),
			.target = parseRequiredProperty<Animation::Channel::Target>(path, object, "target"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Animation::Sampler::Interpolation> {
	[[nodiscard]] static Animation::Sampler::Interpolation parse(String&, pmr::json::Value& value) {
		const pmr::json::String string = value.get<pmr::json::String>();
		if (string == "LINEAR") {
			return Animation::Sampler::Interpolation::LINEAR;
		}
		if (string == "STEP") {
			return Animation::Sampler::Interpolation::STEP;
		}
		if (string == "CUBICSPLINE") {
			return Animation::Sampler::Interpolation::CUBIC_SPLINE;
		}
		throw std::invalid_argument{formatString("Unknown animation sampler interpolation type \"{}\".", string)};
	}
};

template <>
struct Parser<Animation::Sampler> {
	[[nodiscard]] static Animation::Sampler parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.input = parseRequiredProperty<AccessorIndex>(path, object, "input"),
			.interpolation = parseOptionalProperty<Animation::Sampler::Interpolation>(path, object, "interpolation").value_or(Animation::Sampler::Interpolation::LINEAR),
			.output = parseRequiredProperty<AccessorIndex>(path, object, "output"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Animation> {
	[[nodiscard]] static Animation parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.channels = parseRequiredProperty<ArrayList<Animation::Channel>>(path, object, "channels"),
			.samplers = parseRequiredProperty<ArrayList<Animation::Sampler>>(path, object, "samplers"),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Asset::Metadata::Version> {
	[[nodiscard]] static Asset::Metadata::Version parse(String&, pmr::json::Value& value) {
		const StringView string = value.get<pmr::json::String>();
		const size_t dotPosition = string.find('.');
		const StringView majorString = string.substr(0, dotPosition);
		const StringView minorString = (dotPosition == StringView::npos) ? "0" : string.substr(dotPosition + 1);
		Asset::Metadata::Version result{.major = 0, .minor = 0};
		if (const std::from_chars_result parseResult = std::from_chars(majorString.data(), majorString.data() + majorString.size(), result.major); parseResult.ec != std::errc{}) {
			throw std::invalid_argument{make_error_code(parseResult.ec).message()};
		}
		if (const std::from_chars_result parseResult = std::from_chars(minorString.data(), minorString.data() + minorString.size(), result.minor); parseResult.ec != std::errc{}) {
			throw std::invalid_argument{make_error_code(parseResult.ec).message()};
		}
		return result;
	}
};

template <>
struct Parser<Asset::Metadata> {
	[[nodiscard]] static Asset::Metadata parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.copyright = parseOptionalProperty<String>(path, object, "copyright").value_or(String{}),
			.generator = parseOptionalProperty<String>(path, object, "generator").value_or(String{}),
			.version = parseRequiredProperty<Asset::Metadata::Version>(path, object, "version"),
			.minVersion = parseOptionalProperty<Asset::Metadata::Version>(path, object, "minVersion"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<URI> {
	[[nodiscard]] static URI parse(String&, pmr::json::Value& value) {
		String decodedString = uri::percentDecode(value.get<pmr::json::String>());
		if (StringView string = decodedString; string.starts_with("data:")) {
			string.remove_prefix(5);
			const size_t commaPosition = string.find(',');
			if (commaPosition == StringView::npos) {
				throw std::invalid_argument{"Missing comma in data URI."};
			}
			StringView mediaTypeString = string.substr(0, commaPosition);
			const bool base64Encoded = mediaTypeString.ends_with(";base64");
			if (base64Encoded) {
				mediaTypeString.remove_suffix(7);
			}
			if (mediaTypeString != "application/octet-stream" && mediaTypeString != "application/gltf-buffer" && mediaTypeString != "image/jpeg" &&
				mediaTypeString != "image/png") {
				throw std::invalid_argument{formatString("Unsupported media type \"{}\" in data URI.", mediaTypeString)};
			}
			const StringView dataString = string.substr(commaPosition + 1);
			return InlineData{.data = (base64Encoded) ? base64::decode(dataString) : String{dataString}};
		}
		return RelativePath{.path = std::move(decodedString)};
	}
};

template <>
struct Parser<Buffer> {
	[[nodiscard]] static Buffer parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.uri = parseOptionalProperty<URI>(path, object, "uri").value_or(BinChunkData{}),
			.byteLength = parseRequiredProperty<size_t>(path, object, "byteLength"),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<BufferView::Target> {
	[[nodiscard]] static BufferView::Target parse(String& path, pmr::json::Value& value) {
		const size_t index = Parser<size_t>::parse(path, value);
		if (index != 34962 && index != 34963) {
			throw std::invalid_argument{"Unknown buffer view target."};
		}
		return static_cast<BufferView::Target>(static_cast<std::underlying_type_t<BufferView::Target>>(index));
	}
};

template <>
struct Parser<BufferView> {
	[[nodiscard]] static BufferView parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.buffer = parseRequiredProperty<BufferIndex>(path, object, "buffer"),
			.byteOffset = parseOptionalProperty<size_t>(path, object, "byteOffset").value_or(0),
			.byteLength = parseRequiredProperty<size_t>(path, object, "byteLength"),
			.byteStride = parseOptionalProperty<size_t>(path, object, "byteStride"),
			.target = parseOptionalProperty<BufferView::Target>(path, object, "target").value_or(BufferView::Target::UNSPECIFIED),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Camera::Orthographic> {
	[[nodiscard]] static Camera::Orthographic parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.magnification{
				parseRequiredProperty<float>(path, object, "xmag"),
				parseRequiredProperty<float>(path, object, "ymag"),
			},
			.zFar = parseRequiredProperty<float>(path, object, "zfar"),
			.zNear = parseRequiredProperty<float>(path, object, "znear"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Camera::Perspective> {
	[[nodiscard]] static Camera::Perspective parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.aspectRatio = parseOptionalProperty<float>(path, object, "aspectRatio"),
			.yFov = parseRequiredProperty<float>(path, object, "yfov"),
			.zFar = parseOptionalProperty<float>(path, object, "zfar"),
			.zNear = parseRequiredProperty<float>(path, object, "znear"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Camera> {
	[[nodiscard]] static Camera parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		const String& typeString = parseRequiredProperty<String>(path, object, "type");
		if (typeString == "orthographic") {
			return {
				.properties = parseRequiredProperty<Camera::Orthographic>(path, object, "orthographic"),
				.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
				.extensions{},
				.extras = getOptionalValue(object, "extras"),
			};
		}
		if (typeString == "perspective") {
			return {
				.properties = parseRequiredProperty<Camera::Perspective>(path, object, "perspective"),
				.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
				.extensions{},
				.extras = getOptionalValue(object, "extras"),
			};
		}
		throw std::invalid_argument{formatString("Unknown camera type \"{}\".", typeString)};
	}
};

template <>
struct Parser<Image> {
	[[nodiscard]] static Image parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.uri = parseOptionalProperty<URI>(path, object, "uri"),
			.mimeType = parseOptionalProperty<String>(path, object, "mimeType").value_or(String{}),
			.bufferView = parseOptionalProperty<BufferViewIndex>(path, object, "bufferView"),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<TextureInfo::Extension::KHRTextureTransform> {
	[[nodiscard]] static TextureInfo::Extension::KHRTextureTransform parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.offset = parseOptionalProperty<vec2>(path, object, "offset").value_or(vec2{0.0f, 0.0f}),
			.rotation = parseOptionalProperty<float>(path, object, "rotation").value_or(0.0f),
			.scale = parseOptionalProperty<vec2>(path, object, "scale").value_or(vec2{1.0f, 1.0f}),
			.textureCoordinatesChannel = parseOptionalProperty<size_t>(path, object, "texCoord"),
		};
	}
};

template <>
struct Parser<TextureInfo::Extension> {
	[[nodiscard]] static TextureInfo::Extension parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.KHR_texture_transform = parseOptionalProperty<TextureInfo::Extension::KHRTextureTransform>(path, object, "KHR_texture_transform"),
		};
	}
};

template <>
struct Parser<TextureInfo> {
	[[nodiscard]] static TextureInfo parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.index = parseRequiredProperty<TextureIndex>(path, object, "index"),
			.textureCoordinatesChannel = parseOptionalProperty<size_t>(path, object, "texCoord").value_or(0),
			.extensions = parseOptionalProperty<TextureInfo::Extension>(path, object, "extensions").value_or(TextureInfo::Extension{}),
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Material::Extension::KHRMaterialsEmissiveStrength> {
	[[nodiscard]] static Material::Extension::KHRMaterialsEmissiveStrength parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.emissiveStrength = parseOptionalProperty<float>(path, object, "emissiveStrength").value_or(1.0f),
		};
	}
};

template <>
struct Parser<Material::Extension::KHRMaterialsUnlit> {
	[[nodiscard]] static Material::Extension::KHRMaterialsUnlit parse(String&, pmr::json::Value&) {
		return {};
	}
};

template <>
struct Parser<Material::Extension::KHRMaterialsIOR> {
	[[nodiscard]] static Material::Extension::KHRMaterialsIOR parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.ior = parseOptionalProperty<float>(path, object, "ior").value_or(1.5f),
		};
	}
};

template <>
struct Parser<Material::Extension> {
	[[nodiscard]] static Material::Extension parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.KHR_materials_emissive_strength = parseOptionalProperty<Material::Extension::KHRMaterialsEmissiveStrength>(path, object, "KHR_materials_emissive_strength"),
			.KHR_materials_unlit = parseOptionalProperty<Material::Extension::KHRMaterialsUnlit>(path, object, "KHR_materials_unlit"),
			.KHR_materials_ior = parseOptionalProperty<Material::Extension::KHRMaterialsIOR>(path, object, "KHR_materials_ior"),
		};
	}
};

template <>
struct Parser<Material::PBRMetallicRoughness> {
	[[nodiscard]] static Material::PBRMetallicRoughness parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.baseColorFactor = parseOptionalProperty<vec4>(path, object, "baseColorFactor").value_or(vec4{1.0f, 1.0f, 1.0f, 1.0f}),
			.baseColorTexture = parseOptionalProperty<TextureInfo>(path, object, "baseColorTexture"),
			.metallicFactor = parseOptionalProperty<float>(path, object, "metallicFactor").value_or(1.0f),
			.roughnessFactor = parseOptionalProperty<float>(path, object, "roughnessFactor").value_or(1.0f),
			.metallicRoughnessTexture = parseOptionalProperty<TextureInfo>(path, object, "metallicRoughnessTexture"),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Material::NormalTextureInfo> {
	[[nodiscard]] static Material::NormalTextureInfo parse(String& path, pmr::json::Value& value) {
		Material::NormalTextureInfo result{Parser<TextureInfo>::parse(path, value)};
		pmr::json::Object& object = value.get<pmr::json::Object>();
		result.scale = parseOptionalProperty<float>(path, object, "scale").value_or(1.0f);
		return result;
	}
};

template <>
struct Parser<Material::OcclusionTextureInfo> {
	[[nodiscard]] static Material::OcclusionTextureInfo parse(String& path, pmr::json::Value& value) {
		Material::OcclusionTextureInfo result{Parser<TextureInfo>::parse(path, value)};
		pmr::json::Object& object = value.get<pmr::json::Object>();
		result.strength = parseOptionalProperty<float>(path, object, "strength").value_or(1.0f);
		return result;
	}
};

template <>
struct Parser<Material::AlphaMode> {
	[[nodiscard]] static Material::AlphaMode parse(String&, pmr::json::Value& value) {
		const pmr::json::String string = value.get<pmr::json::String>();
		if (string == "OPAQUE") {
			return Material::AlphaMode::ALPHA_OPAQUE;
		}
		if (string == "MASK") {
			return Material::AlphaMode::ALPHA_MASK;
		}
		if (string == "BLEND") {
			return Material::AlphaMode::ALPHA_BLEND;
		}
		throw std::invalid_argument{formatString("Unknown material alpha mode \"{}\".", string)};
	}
};

template <>
struct Parser<Material> {
	[[nodiscard]] static Material parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.pbrMetallicRoughness = parseOptionalProperty<Material::PBRMetallicRoughness>(path, object, "pbrMetallicRoughness")
		        .value_or(Material::PBRMetallicRoughness{
					.baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f},
					.baseColorTexture{},
					.metallicFactor = 1.0f,
					.roughnessFactor = 1.0f,
					.metallicRoughnessTexture{},
					.extensions{},
					.extras{},
				}),
			.normalTexture = parseOptionalProperty<Material::NormalTextureInfo>(path, object, "normalTexture"),
			.occlusionTexture = parseOptionalProperty<Material::OcclusionTextureInfo>(path, object, "occlusionTexture"),
			.emissiveTexture = parseOptionalProperty<TextureInfo>(path, object, "emissiveTexture"),
			.emissiveFactor = parseOptionalProperty<vec3>(path, object, "emissiveFactor").value_or(vec3{0.0f, 0.0f, 0.0f}),
			.alphaMode = parseOptionalProperty<Material::AlphaMode>(path, object, "alphaMode").value_or(Material::AlphaMode::ALPHA_OPAQUE),
			.alphaCutoff = parseOptionalProperty<float>(path, object, "alphaCutoff").value_or(0.5f),
			.doubleSided = parseOptionalProperty<bool>(path, object, "doubleSided").value_or(false),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions = parseOptionalProperty<Material::Extension>(path, object, "extensions").value_or(Material::Extension{}),
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Mesh::Primitive::Attributes> {
	[[nodiscard]] static Mesh::Primitive::Attributes parse(String& path, pmr::json::Value& value) {
		Mesh::Primitive::Attributes result{};
		pmr::json::Object& object = value.get<pmr::json::Object>();
		for (auto& [semantic, accessor] : object) {
			const AccessorIndex accessorIndex = Parser<AccessorIndex>::parse(path, accessor);
			if (semantic == "POSITION") {
				result.position = accessorIndex;
			} else if (semantic == "NORMAL") {
				result.normal = accessorIndex;
			} else if (semantic == "TANGENT") {
				result.tangent = accessorIndex;
			} else if (semantic == "TEXCOORD_0") {
				result.texcoord0 = accessorIndex;
			} else if (semantic == "TEXCOORD_1") {
				result.texcoord1 = accessorIndex;
			} else if (semantic == "COLOR_0") {
				result.color0 = accessorIndex;
			} else if (semantic == "JOINTS_0") {
				result.joints0 = accessorIndex;
			} else if (semantic == "WEIGHTS_0") {
				result.weights0 = accessorIndex;
			} else {
				result.others.emplace(std::move(semantic), accessorIndex);
			}
		}
		return result;
	}
};

template <>
struct Parser<Mesh::Primitive::Mode> {
	[[nodiscard]] static Mesh::Primitive::Mode parse(String& path, pmr::json::Value& value) {
		const size_t index = Parser<size_t>::parse(path, value);
		if (index > 6) {
			throw std::invalid_argument{"Unknown mesh primitive mode."};
		}
		return static_cast<Mesh::Primitive::Mode>(static_cast<std::underlying_type_t<Mesh::Primitive::Mode>>(index));
	}
};

template <>
struct Parser<Mesh::Primitive> {
	[[nodiscard]] static Mesh::Primitive parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.attributes = parseRequiredProperty<Mesh::Primitive::Attributes>(path, object, "attributes"),
			.indices = parseOptionalProperty<AccessorIndex>(path, object, "indices"),
			.material = parseOptionalProperty<MaterialIndex>(path, object, "material"),
			.mode = parseOptionalProperty<Mesh::Primitive::Mode>(path, object, "mode").value_or(Mesh::Primitive::Mode::TRIANGLES),
			.targets = parseOptionalProperty<ArrayList<Mesh::Primitive::Attributes>>(path, object, "targets").value_or(ArrayList<Mesh::Primitive::Attributes>{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Mesh> {
	[[nodiscard]] static Mesh parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.primitives = parseRequiredProperty<ArrayList<Mesh::Primitive>>(path, object, "primitives"),
			.weights = parseOptionalProperty<ArrayList<float>>(path, object, "weights").value_or(ArrayList<float>{}),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Node::Extension::KHRLightsPunctual> {
	[[nodiscard]] static Node::Extension::KHRLightsPunctual parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.light = parseRequiredProperty<Node::Extension::KHRLightsPunctual::LightIndex>(path, object, "light"),
		};
	}
};

template <>
struct Parser<Node::Extension::KHRNodeVisibility> {
	[[nodiscard]] static Node::Extension::KHRNodeVisibility parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.visible = parseOptionalProperty<bool>(path, object, "visible").value_or(true),
		};
	}
};

template <>
struct Parser<Node::Extension::KHRPhysicsRigidBodies::Geometry> {
	[[nodiscard]] static Node::Extension::KHRPhysicsRigidBodies::Geometry parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.shape = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::ImplicitShapeIndex>(path, object, "shape"),
			.mesh = parseOptionalProperty<MeshIndex>(path, object, "mesh"),
			.convexHull = parseOptionalProperty<bool>(path, object, "convexHull").value_or(false),
		};
	}
};

template <>
struct Parser<Node::Extension::KHRPhysicsRigidBodies::Motion> {
	[[nodiscard]] static Node::Extension::KHRPhysicsRigidBodies::Motion parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.isKinematic = parseOptionalProperty<bool>(path, object, "isKinematic").value_or(false),
			.mass = parseOptionalProperty<float>(path, object, "mass"),
			.inertiaOrientation = parseOptionalProperty<quat>(path, object, "inertiaOrientation").value_or(quat{0.0f, 0.0f, 0.0f, 1.0f}),
			.inertiaDiagonal = parseOptionalProperty<vec3>(path, object, "inertiaDiagonal"),
			.centerOfMass = parseOptionalProperty<vec3>(path, object, "centerOfMass").value_or(vec3{0.0f, 0.0f, 0.0f}),
			.linearVelocity = parseOptionalProperty<vec3>(path, object, "linearVelocity").value_or(vec3{0.0f, 0.0f, 0.0f}),
			.angularVelocity = parseOptionalProperty<vec3>(path, object, "angularVelocity").value_or(vec3{0.0f, 0.0f, 0.0f}),
			.gravityFactor = parseOptionalProperty<float>(path, object, "gravityFactor").value_or(1.0f),
		};
	}
};

template <>
struct Parser<Node::Extension::KHRPhysicsRigidBodies::Collider> {
	[[nodiscard]] static Node::Extension::KHRPhysicsRigidBodies::Collider parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.geometry = parseRequiredProperty<Node::Extension::KHRPhysicsRigidBodies::Geometry>(path, object, "geometry"),
			.physicsMaterial = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::PhysicsMaterialIndex>(path, object, "physicsMaterial"),
			.collisionFilter = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::CollisionFilterIndex>(path, object, "collisionFilter"),
		};
	}
};

template <>
struct Parser<Node::Extension::KHRPhysicsRigidBodies::Trigger> {
	[[nodiscard]] static Node::Extension::KHRPhysicsRigidBodies::Trigger parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.geometry = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::Geometry>(path, object, "geometry"),
			.nodes = parseOptionalProperty<ArrayList<NodeIndex>>(path, object, "nodes").value_or(ArrayList<NodeIndex>{}),
			.collisionFilter = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::CollisionFilterIndex>(path, object, "collisionFilter"),
		};
	}
};

template <>
struct Parser<Node::Extension::KHRPhysicsRigidBodies::Joint> {
	[[nodiscard]] static Node::Extension::KHRPhysicsRigidBodies::Joint parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.connectedNode = parseRequiredProperty<NodeIndex>(path, object, "connectedNode"),
			.joint = parseRequiredProperty<Node::Extension::KHRPhysicsRigidBodies::PhysicsJointIndex>(path, object, "joint"),
			.enableCollision = parseOptionalProperty<bool>(path, object, "enableCollision").value_or(false),
		};
	}
};

template <>
struct Parser<Node::Extension::KHRPhysicsRigidBodies> {
	[[nodiscard]] static Node::Extension::KHRPhysicsRigidBodies parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.motion = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::Motion>(path, object, "motion"),
			.collider = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::Collider>(path, object, "collider"),
			.trigger = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::Trigger>(path, object, "trigger"),
			.joint = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies::Joint>(path, object, "joint"),
		};
	}
};

template <>
struct Parser<Node::Extension> {
	[[nodiscard]] static Node::Extension parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.KHR_lights_punctual = parseOptionalProperty<Node::Extension::KHRLightsPunctual>(path, object, "KHR_lights_punctual"),
			.KHR_node_visibility = parseOptionalProperty<Node::Extension::KHRNodeVisibility>(path, object, "KHR_node_visibility"),
			.KHR_physics_rigid_bodies = parseOptionalProperty<Node::Extension::KHRPhysicsRigidBodies>(path, object, "KHR_physics_rigid_bodies"),
		};
	}
};

template <>
struct Parser<Node> {
	[[nodiscard]] static Node parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		vec3 translation{};
		quat rotation{};
		vec3 scale{};
		if (const Optional<mat4> matrix = parseOptionalProperty<mat4>(path, object, "matrix")) {
			const auto [decomposedTranslation, decomposedRotation, decomposedScale] = decomposeTranslationRotationScale(*matrix);
			translation = decomposedTranslation;
			rotation = decomposedRotation;
			scale = decomposedScale;
		} else {
			translation = parseOptionalProperty<vec3>(path, object, "translation").value_or(vec3{0.0f, 0.0f, 0.0f});
			rotation = parseOptionalProperty<quat>(path, object, "rotation").value_or(quat{0.0f, 0.0f, 0.0f, 1.0f});
			scale = parseOptionalProperty<vec3>(path, object, "scale").value_or(vec3{1.0f, 1.0f, 1.0f});
		}
		return {
			.camera = parseOptionalProperty<CameraIndex>(path, object, "camera"),
			.children = parseOptionalProperty<ArrayList<NodeIndex>>(path, object, "children").value_or(ArrayList<NodeIndex>{}),
			.skin = parseOptionalProperty<SkinIndex>(path, object, "skin"),
			.rotation = rotation,
			.scale = scale,
			.translation = translation,
			.mesh = parseOptionalProperty<MeshIndex>(path, object, "mesh"),
			.weights = parseOptionalProperty<ArrayList<float>>(path, object, "weights").value_or(ArrayList<float>{}),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions = parseOptionalProperty<Node::Extension>(path, object, "extensions").value_or(Node::Extension{}),
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Sampler::MagnificationFilter> {
	[[nodiscard]] static Sampler::MagnificationFilter parse(String& path, pmr::json::Value& value) {
		const size_t index = Parser<size_t>::parse(path, value);
		if (index != 9728 && index != 9729) {
			throw std::invalid_argument{"Unknown sampler magnification filter type."};
		}
		return static_cast<Sampler::MagnificationFilter>(static_cast<std::underlying_type_t<Sampler::MagnificationFilter>>(index));
	}
};

template <>
struct Parser<Sampler::MinificationFilter> {
	[[nodiscard]] static Sampler::MinificationFilter parse(String& path, pmr::json::Value& value) {
		const size_t index = Parser<size_t>::parse(path, value);
		if (index != 9728 && index != 9729 && index != 9984 && index != 9985 && index != 9986 && index != 9987) {
			throw std::invalid_argument{"Unknown sampler minification filter type."};
		}
		return static_cast<Sampler::MinificationFilter>(static_cast<std::underlying_type_t<Sampler::MinificationFilter>>(index));
	}
};

template <>
struct Parser<Sampler::WrappingMode> {
	[[nodiscard]] static Sampler::WrappingMode parse(String& path, pmr::json::Value& value) {
		const size_t index = Parser<size_t>::parse(path, value);
		if (index != 33071 && index != 33648 && index != 10497) {
			throw std::invalid_argument{"Unknown sampler wrapping mode."};
		}
		return static_cast<Sampler::WrappingMode>(static_cast<std::underlying_type_t<Sampler::WrappingMode>>(index));
	}
};

template <>
struct Parser<Sampler> {
	[[nodiscard]] static Sampler parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.magFilter = parseOptionalProperty<Sampler::MagnificationFilter>(path, object, "magFilter").value_or(Sampler::MagnificationFilter::UNSPECIFIED),
			.minFilter = parseOptionalProperty<Sampler::MinificationFilter>(path, object, "minFilter").value_or(Sampler::MinificationFilter::UNSPECIFIED),
			.wrapS = parseOptionalProperty<Sampler::WrappingMode>(path, object, "wrapS").value_or(Sampler::WrappingMode::REPEAT),
			.wrapT = parseOptionalProperty<Sampler::WrappingMode>(path, object, "wrapT").value_or(Sampler::WrappingMode::REPEAT),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Scene> {
	[[nodiscard]] static Scene parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.nodes = parseOptionalProperty<ArrayList<NodeIndex>>(path, object, "nodes").value_or(ArrayList<NodeIndex>{}),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Skin> {
	[[nodiscard]] static Skin parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.inverseBindMatrices = parseOptionalProperty<AccessorIndex>(path, object, "inverseBindMatrices"),
			.skeleton = parseOptionalProperty<NodeIndex>(path, object, "skeleton"),
			.joints = parseRequiredProperty<ArrayList<NodeIndex>>(path, object, "joints"),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions{},
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Texture::Extension::KHRTextureBasisu> {
	[[nodiscard]] static Texture::Extension::KHRTextureBasisu parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.source = parseRequiredProperty<ImageIndex>(path, object, "source"),
		};
	}
};

template <>
struct Parser<Texture::Extension> {
	[[nodiscard]] static Texture::Extension parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.KHR_texture_basisu = parseOptionalProperty<Texture::Extension::KHRTextureBasisu>(path, object, "KHR_texture_basisu"),
		};
	}
};

template <>
struct Parser<Texture> {
	[[nodiscard]] static Texture parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.sampler = parseOptionalProperty<SamplerIndex>(path, object, "sampler"),
			.source = parseOptionalProperty<ImageIndex>(path, object, "source"),
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.extensions = parseOptionalProperty<Texture::Extension>(path, object, "extensions").value_or(Texture::Extension{}),
			.extras = getOptionalValue(object, "extras"),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRLightsPunctual::Light::Type> {
	[[nodiscard]] static Asset::Extension::KHRLightsPunctual::Light::Type parse(String&, pmr::json::Value& value) {
		const pmr::json::String& string = value.get<pmr::json::String>();
		if (string == "directional") {
			return Asset::Extension::KHRLightsPunctual::Light::Type::DIRECTIONAL;
		}
		if (string == "point") {
			return Asset::Extension::KHRLightsPunctual::Light::Type::POINT;
		}
		if (string == "spot") {
			return Asset::Extension::KHRLightsPunctual::Light::Type::SPOT;
		}
		throw std::invalid_argument{"Invalid light type."};
	}
};

template <>
struct Parser<Asset::Extension::KHRLightsPunctual::Light::Spot> {
	[[nodiscard]] static Asset::Extension::KHRLightsPunctual::Light::Spot parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.innerConeAngle = parseOptionalProperty<float>(path, object, "innerConeAngle").value_or(0.0f),
			.outerConeAngle = parseOptionalProperty<float>(path, object, "outerConeAngle").value_or(numbers::PI / 4.0f),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRLightsPunctual::Light> {
	[[nodiscard]] static Asset::Extension::KHRLightsPunctual::Light parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.name = parseOptionalProperty<String>(path, object, "name").value_or(String{}),
			.color = parseOptionalProperty<vec3>(path, object, "color").value_or(vec3{1.0f, 1.0f, 1.0f}),
			.intensity = parseOptionalProperty<float>(path, object, "intensity").value_or(1.0f),
			.type = parseRequiredProperty<Asset::Extension::KHRLightsPunctual::Light::Type>(path, object, "type"),
			.range = parseOptionalProperty<float>(path, object, "range"),
			.spot = parseOptionalProperty<Asset::Extension::KHRLightsPunctual::Light::Spot>(path, object, "spot"),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRLightsPunctual> {
	[[nodiscard]] static Asset::Extension::KHRLightsPunctual parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.lights = parseRequiredProperty<ArrayList<Asset::Extension::KHRLightsPunctual::Light>>(path, object, "lights"),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRImplicitShapes::Plane> {
	[[nodiscard]] static Asset::Extension::KHRImplicitShapes::Plane parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.doubleSided = parseOptionalProperty<bool>(path, object, "doubleSided").value_or(false),
			.sizeX = parseOptionalProperty<float>(path, object, "sizeX"),
			.sizeZ = parseOptionalProperty<float>(path, object, "sizeZ"),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRImplicitShapes::Sphere> {
	[[nodiscard]] static Asset::Extension::KHRImplicitShapes::Sphere parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.radius = parseOptionalProperty<float>(path, object, "radius").value_or(0.5f),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRImplicitShapes::Box> {
	[[nodiscard]] static Asset::Extension::KHRImplicitShapes::Box parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.size = parseOptionalProperty<vec3>(path, object, "size").value_or(vec3{1.0f, 1.0f, 1.0f}),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRImplicitShapes::Cylinder> {
	[[nodiscard]] static Asset::Extension::KHRImplicitShapes::Cylinder parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.height = parseOptionalProperty<float>(path, object, "height").value_or(0.5f),
			.radiusBottom = parseOptionalProperty<float>(path, object, "radiusBottom").value_or(0.25f),
			.radiusTop = parseOptionalProperty<float>(path, object, "radiusTop").value_or(0.25f),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRImplicitShapes::Capsule> {
	[[nodiscard]] static Asset::Extension::KHRImplicitShapes::Capsule parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.height = parseOptionalProperty<float>(path, object, "height").value_or(0.5f),
			.radiusBottom = parseOptionalProperty<float>(path, object, "radiusBottom").value_or(0.25f),
			.radiusTop = parseOptionalProperty<float>(path, object, "radiusTop").value_or(0.25f),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRImplicitShapes::Shape> {
	[[nodiscard]] static Asset::Extension::KHRImplicitShapes::Shape parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		const String type = parseRequiredProperty<String>(path, object, "type");
		if (type == "plane") {
			return parseRequiredProperty<Asset::Extension::KHRImplicitShapes::Plane>(path, object, "plane");
		}
		if (type == "sphere") {
			return parseRequiredProperty<Asset::Extension::KHRImplicitShapes::Sphere>(path, object, "sphere");
		}
		if (type == "box") {
			return parseRequiredProperty<Asset::Extension::KHRImplicitShapes::Box>(path, object, "box");
		}
		if (type == "cylinder") {
			return parseRequiredProperty<Asset::Extension::KHRImplicitShapes::Cylinder>(path, object, "cylinder");
		}
		if (type == "capsule") {
			return parseRequiredProperty<Asset::Extension::KHRImplicitShapes::Capsule>(path, object, "capsule");
		}
		throw std::invalid_argument{formatString("Unknown implicit shape type \"{}\".", type)};
	}
};

template <>
struct Parser<Asset::Extension::KHRImplicitShapes> {
	[[nodiscard]] static Asset::Extension::KHRImplicitShapes parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.shapes = parseRequiredProperty<ArrayList<Asset::Extension::KHRImplicitShapes::Shape>>(path, object, "shapes"),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Material::FrictionCombine> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Material::FrictionCombine parse(String&, pmr::json::Value& value) {
		const pmr::json::String& string = value.get<pmr::json::String>();
		if (string == "average") {
			return Asset::Extension::KHRPhysicsRigidBodies::Material::FrictionCombine::AVERAGE;
		}
		if (string == "minimum") {
			return Asset::Extension::KHRPhysicsRigidBodies::Material::FrictionCombine::MINIMUM;
		}
		if (string == "maximum") {
			return Asset::Extension::KHRPhysicsRigidBodies::Material::FrictionCombine::MAXIMUM;
		}
		if (string == "multiply") {
			return Asset::Extension::KHRPhysicsRigidBodies::Material::FrictionCombine::MULTIPLY;
		}
		throw std::invalid_argument{formatString("Unknown frictionCombine type \"{}\".", string)};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Material::RestitutionCombine> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Material::RestitutionCombine parse(String&, pmr::json::Value& value) {
		const pmr::json::String& string = value.get<pmr::json::String>();
		if (string == "average") {
			return Asset::Extension::KHRPhysicsRigidBodies::Material::RestitutionCombine::AVERAGE;
		}
		if (string == "minimum") {
			return Asset::Extension::KHRPhysicsRigidBodies::Material::RestitutionCombine::MINIMUM;
		}
		if (string == "maximum") {
			return Asset::Extension::KHRPhysicsRigidBodies::Material::RestitutionCombine::MAXIMUM;
		}
		if (string == "multiply") {
			return Asset::Extension::KHRPhysicsRigidBodies::Material::RestitutionCombine::MULTIPLY;
		}
		throw std::invalid_argument{formatString("Unknown restitutionCombine type \"{}\".", string)};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Material> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Material parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.staticFriction = parseOptionalProperty<float>(path, object, "staticFriction").value_or(0.6f),
			.dynamicFriction = parseOptionalProperty<float>(path, object, "dynamicFriction").value_or(0.6f),
			.restitution = parseOptionalProperty<float>(path, object, "restitution").value_or(0.0f),
			.frictionCombine = parseOptionalProperty<Asset::Extension::KHRPhysicsRigidBodies::Material::FrictionCombine>(path, object, "frictionCombine"),
			.restitutionCombine = parseOptionalProperty<Asset::Extension::KHRPhysicsRigidBodies::Material::RestitutionCombine>(path, object, "restitutionCombine"),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::CollisionFilter> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::CollisionFilter parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.collisionSystems = parseOptionalProperty<ArrayList<String>>(path, object, "collisionSystems").value_or(ArrayList<String>{}),
			.collideWithSystems = parseOptionalProperty<ArrayList<String>>(path, object, "collideWithSystems").value_or(ArrayList<String>{}),
			.notCollideWithSystems = parseOptionalProperty<ArrayList<String>>(path, object, "notCollideWithSystems").value_or(ArrayList<String>{}),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::LinearAxes> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::LinearAxes parse(String& path, pmr::json::Value& value) {
		const pmr::json::Array& array = value.get<pmr::json::Array>();
		if (array.empty() || array.size() > 3) {
			throw std::invalid_argument{"Invalid axis count."};
		}
		Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::LinearAxes result{.x = false, .y = false, .z = false};
		for (size_t i = 0; i < array.size(); ++i) {
			const size_t previousPathLength = appendToPath(path, formatString("[{}]", i));
			const json::Number axis = array[i].get<json::Number>();
			if (axis == json::Number{0}) {
				if (result.x) {
					throw std::invalid_argument{"Duplicate axes."};
				}
				result.x = true;
			} else if (axis == json::Number{1}) {
				if (result.y) {
					throw std::invalid_argument{"Duplicate axes."};
				}
				result.y = true;
			} else if (axis == json::Number{2}) {
				if (result.z) {
					throw std::invalid_argument{"Duplicate axes."};
				}
				result.z = true;
			} else {
				throw std::invalid_argument{"Invalid axis index."};
			}
			resetPathTo(path, previousPathLength);
		}
		return result;
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::AngularAxes> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::AngularAxes parse(String& path, pmr::json::Value& value) {
		const pmr::json::Array& array = value.get<pmr::json::Array>();
		if (array.empty() || array.size() > 3) {
			throw std::invalid_argument{"Invalid axis count."};
		}
		Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::AngularAxes result{.x = false, .y = false, .z = false};
		for (size_t i = 0; i < array.size(); ++i) {
			const size_t previousPathLength = appendToPath(path, formatString("[{}]", i));
			const json::Number axis = array[i].get<json::Number>();
			if (axis == json::Number{0}) {
				if (result.x) {
					throw std::invalid_argument{"Duplicate axes."};
				}
				result.x = true;
			} else if (axis == json::Number{1}) {
				if (result.y) {
					throw std::invalid_argument{"Duplicate axes."};
				}
				result.y = true;
			} else if (axis == json::Number{2}) {
				if (result.z) {
					throw std::invalid_argument{"Duplicate axes."};
				}
				result.z = true;
			} else {
				throw std::invalid_argument{"Invalid axis index."};
			}
			resetPathTo(path, previousPathLength);
		}
		return result;
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		const Optional<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::LinearAxes> linearAxes =
			parseOptionalProperty<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::LinearAxes>(path, object, "linearAxes");
		const Optional<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::AngularAxes> angularAxes =
			parseOptionalProperty<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::AngularAxes>(path, object, "angularAxes");
		if (linearAxes.has_value() == angularAxes.has_value()) {
			throw std::invalid_argument{"Invalid axes."};
		}
		using Axes = Variant<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::LinearAxes, Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::AngularAxes>;
		return {
			.min = parseOptionalProperty<float>(path, object, "min"),
			.max = parseOptionalProperty<float>(path, object, "max"),
			.stiffness = parseOptionalProperty<float>(path, object, "stiffness"),
			.damping = parseOptionalProperty<float>(path, object, "damping").value_or(0.0f),
			.axes = (linearAxes) ? Axes{*linearAxes} : Axes{*angularAxes},
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Type> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Type parse(String&, pmr::json::Value& value) {
		const pmr::json::String& string = value.get<pmr::json::String>();
		if (string == "linear") {
			return Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Type::LINEAR;
		}
		if (string == "angular") {
			return Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Type::ANGULAR;
		}
		throw std::invalid_argument{"Invalid drive type."};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Mode> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Mode parse(String&, pmr::json::Value& value) {
		const pmr::json::String& string = value.get<pmr::json::String>();
		if (string == "force") {
			return Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Mode::FORCE;
		}
		if (string == "acceleration") {
			return Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Mode::ACCELERATION;
		}
		throw std::invalid_argument{"Invalid drive mode."};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.type = parseRequiredProperty<Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Type>(path, object, "type"),
			.mode = parseOptionalProperty<Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Mode>(path, object, "mode")
		        .value_or(Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Mode::FORCE),
			.axis = static_cast<uint8_t>(parseRequiredProperty<size_t>(path, object, "axis")),
			.maxForce = parseOptionalProperty<float>(path, object, "maxForce"),
			.positionTarget = parseOptionalProperty<float>(path, object, "positionTarget"),
			.velocityTarget = parseOptionalProperty<float>(path, object, "velocityTarget"),
			.damping = parseOptionalProperty<float>(path, object, "damping").value_or(0.0f),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies::Joint> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies::Joint parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.limits = parseOptionalProperty<ArrayList<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit>>(path, object, "limits")
		        .value_or(ArrayList<Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit>{}),
			.drives = parseOptionalProperty<ArrayList<Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive>>(path, object, "drives")
		        .value_or(ArrayList<Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive>{}),
		};
	}
};

template <>
struct Parser<Asset::Extension::KHRPhysicsRigidBodies> {
	[[nodiscard]] static Asset::Extension::KHRPhysicsRigidBodies parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.physicsMaterials = parseOptionalProperty<ArrayList<Asset::Extension::KHRPhysicsRigidBodies::Material>>(path, object, "physicsMaterials")
		        .value_or(ArrayList<Asset::Extension::KHRPhysicsRigidBodies::Material>{}),
			.collisionFilters = parseOptionalProperty<ArrayList<Asset::Extension::KHRPhysicsRigidBodies::CollisionFilter>>(path, object, "collisionFilters")
		        .value_or(ArrayList<Asset::Extension::KHRPhysicsRigidBodies::CollisionFilter>{}),
			.physicsJoints = parseOptionalProperty<ArrayList<Asset::Extension::KHRPhysicsRigidBodies::Joint>>(path, object, "physicsJoints")
		        .value_or(ArrayList<Asset::Extension::KHRPhysicsRigidBodies::Joint>{}),
		};
	}
};

template <>
struct Parser<Asset::Extension> {
	[[nodiscard]] static Asset::Extension parse(String& path, pmr::json::Value& value) {
		pmr::json::Object& object = value.get<pmr::json::Object>();
		return {
			.KHR_lights_punctual = parseOptionalProperty<Asset::Extension::KHRLightsPunctual>(path, object, "KHR_lights_punctual"),
			.KHR_implicit_shapes = parseOptionalProperty<Asset::Extension::KHRImplicitShapes>(path, object, "KHR_implicit_shapes"),
			.KHR_physics_rigid_bodies = parseOptionalProperty<Asset::Extension::KHRPhysicsRigidBodies>(path, object, "KHR_physics_rigid_bodies"),
		};
	}
};

} // namespace

Asset Asset::parse(StringView glTFString, const SourceLocation& source, Span<const byte> binChunk) {
	String path = "gltf";
	try {
		Arena arena{};
		pmr::json::Value json = pmr::json::Value::parse(glTFString, json::SourceLocation{.lineNumber = source.lineNumber, .columnNumber = source.columnNumber}, &arena);
		if (!json.is<pmr::json::Object>()) {
			throw gltf::Error{"Invalid glTF format: Top-level JSON value must be an object.", {.filepath = source.filepath}};
		}
		pmr::json::Object& object = json.as<pmr::json::Object>();

		Asset::Metadata asset = parseRequiredProperty<Asset::Metadata>(path, object, "asset");
		if (asset.minVersion) {
			if (asset.minVersion->major > 2 || asset.minVersion->minor > 0) {
				throw gltf::Error{"Unsupported glTF format: Minimum version required is greater than 2.0.", {.filepath = source.filepath}};
			}
		} else if (asset.version.major > 2) {
			throw gltf::Error{"Unsupported glTF format: Major version is greater than 2.", {.filepath = source.filepath}};
		}

		ArrayList<String> extensionsRequired = parseOptionalProperty<ArrayList<String>>(path, object, "extensionsRequired").value_or(ArrayList<String>{});
		ArrayList<bool> extensionsSupported{};
		bool allExtensionsSupported = true;
		for (const String& extension : extensionsRequired) {
			const bool isSupported =
				std::any_of(SUPPORTED_EXTENSIONS.begin(), SUPPORTED_EXTENSIONS.end(), [&](CStringView supportedExtension) -> bool { return supportedExtension == extension; });
			allExtensionsSupported = allExtensionsSupported && isSupported;
			extensionsSupported.push_back(isSupported);
		}
		if (!allExtensionsSupported) {
			String message{"Unsupported glTF asset: The following specified required extensions are not supported:"};
			for (size_t i = 0; i < extensionsRequired.size(); ++i) {
				if (!extensionsSupported[i]) {
					message.append("\n- ");
					message.append(extensionsRequired[i]);
				}
			}
			throw gltf::Error{message, {.filepath = source.filepath}};
		}

		return {
			.extensionsUsed = parseOptionalProperty<ArrayList<String>>(path, object, "extensionsUsed").value_or(ArrayList<String>{}),
			.extensionsRequired = std::move(extensionsRequired),
			.accessors = parseOptionalProperty<ArrayList<Accessor>>(path, object, "accessors").value_or(ArrayList<Accessor>{}),
			.animations = parseOptionalProperty<ArrayList<Animation>>(path, object, "animations").value_or(ArrayList<Animation>{}),
			.asset = std::move(asset),
			.buffers = parseOptionalProperty<ArrayList<Buffer>>(path, object, "buffers").value_or(ArrayList<Buffer>{}),
			.bufferViews = parseOptionalProperty<ArrayList<BufferView>>(path, object, "bufferViews").value_or(ArrayList<BufferView>{}),
			.cameras = parseOptionalProperty<ArrayList<Camera>>(path, object, "cameras").value_or(ArrayList<Camera>{}),
			.images = parseOptionalProperty<ArrayList<Image>>(path, object, "images").value_or(ArrayList<Image>{}),
			.materials = parseOptionalProperty<ArrayList<Material>>(path, object, "materials").value_or(ArrayList<Material>{}),
			.meshes = parseOptionalProperty<ArrayList<Mesh>>(path, object, "meshes").value_or(ArrayList<Mesh>{}),
			.nodes = parseOptionalProperty<ArrayList<Node>>(path, object, "nodes").value_or(ArrayList<Node>{}),
			.samplers = parseOptionalProperty<ArrayList<Sampler>>(path, object, "samplers").value_or(ArrayList<Sampler>{}),
			.scene = parseOptionalProperty<SceneIndex>(path, object, "scene"),
			.scenes = parseOptionalProperty<ArrayList<Scene>>(path, object, "scenes").value_or(ArrayList<Scene>{}),
			.skins = parseOptionalProperty<ArrayList<Skin>>(path, object, "skins").value_or(ArrayList<Skin>{}),
			.textures = parseOptionalProperty<ArrayList<Texture>>(path, object, "textures").value_or(ArrayList<Texture>{}),
			.extensions = parseOptionalProperty<Asset::Extension>(path, object, "extensions").value_or(Asset::Extension{}),
			.extras = getOptionalValue(object, "extras"),
			.binChunk = binChunk,
		};
	} catch (const json::Error& e) {
		throw gltf::Error{e.what(), SourceLocation{.filepath = source.filepath, .lineNumber = e.lineNumber, .columnNumber = e.columnNumber}};
	} catch (const BadVariantAccess&) {
		throw gltf::Error{formatString("Invalid glTF format: Incorrect type for property \"{}\".", path), {.filepath = source.filepath}};
	} catch (const std::out_of_range&) {
		throw gltf::Error{formatString("Invalid glTF format: Missing property \"{}\".", path), {.filepath = source.filepath}};
	} catch (const std::invalid_argument& e) {
		throw gltf::Error{formatString("Invalid glTF format: Invalid property \"{}\":\n{}", path, e.what()), {.filepath = source.filepath}};
	}
}

Asset Asset::parseBinary(Span<const byte> glbData, CStringView sourceFilepath) {
	if (glbData.size() < 12) {
		throw gltf::Error{"Invalid binary glTF format: Missing header.", {.filepath = sourceFilepath}};
	}
	const Span<const byte, 4> magicData = glbData.subspan<0, 4>();
	const Span<const byte, 4> versionData = glbData.subspan<4, 4>();
	const Span<const byte, 4> lengthData = glbData.subspan<8, 4>();
	if (magicData[0] != byte{'g'} || magicData[1] != byte{'l'} || magicData[2] != byte{'T'} || magicData[3] != byte{'F'}) {
		throw gltf::Error{"Invalid binary glTF format: Incorrect magic identifier.", {.filepath = sourceFilepath}};
	}
	uint32_t version = 0;
	uint32_t length = 0;
	memcpy(&version, versionData.data(), versionData.size());
	memcpy(&length, lengthData.data(), lengthData.size());
	version = convertLittleEndianToHostEndian(version);
	length = convertLittleEndianToHostEndian(length);
	if (version > 2) {
		throw gltf::Error{"Unsupported binary glTF format: Version is greater than 2.", {.filepath = sourceFilepath}};
	}
	if (static_cast<size_t>(length) > glbData.size() || length < 12) {
		throw gltf::Error{"Invalid binary glTF format: Invalid length.", {.filepath = sourceFilepath}};
	}
	Span<const byte> chunks = glbData.subspan(12, static_cast<size_t>(length) - 12);
	if (chunks.size() < 8) {
		throw gltf::Error{"Invalid binary glTF format: Missing JSON chunk header.", {.filepath = sourceFilepath}};
	}
	const Span<const byte, 4> jsonChunkLengthData = chunks.subspan<0, 4>();
	const Span<const byte, 4> jsonChunkTypeData = chunks.subspan<4, 4>();
	if (jsonChunkTypeData[0] != byte{'J'} || jsonChunkTypeData[1] != byte{'S'} || jsonChunkTypeData[2] != byte{'O'} || jsonChunkTypeData[3] != byte{'N'}) {
		throw gltf::Error{"Invalid binary glTF format: Incorrect JSON chunk type.", {.filepath = sourceFilepath}};
	}
	uint32_t jsonChunkLength = 0;
	memcpy(&jsonChunkLength, jsonChunkLengthData.data(), jsonChunkLengthData.size());
	jsonChunkLength = convertLittleEndianToHostEndian(jsonChunkLength);
	chunks = chunks.subspan(8);
	if (static_cast<size_t>(jsonChunkLength) > chunks.size()) {
		throw gltf::Error{"Invalid binary glTF format: Invalid JSON chunk length.", {.filepath = sourceFilepath}};
	}
	const Span<const byte> jsonChunk = chunks.subspan(0, static_cast<size_t>(jsonChunkLength));
	const StringView jsonString{std::launder(reinterpret_cast<const char*>(jsonChunk.data())), jsonChunk.size()};
	chunks = chunks.subspan(jsonChunk.size());
	Span<const byte> binChunk{};
	if (!chunks.empty()) {
		if (chunks.size() < 8) {
			throw gltf::Error{"Invalid binary glTF format: Invalid BIN chunk header length.", {.filepath = sourceFilepath}};
		}
		const Span<const byte, 4> binChunkLengthData = chunks.subspan<0, 4>();
		const Span<const byte, 4> binChunkTypeData = chunks.subspan<4, 4>();
		if (binChunkTypeData[0] != byte{'B'} || binChunkTypeData[1] != byte{'I'} || binChunkTypeData[2] != byte{'N'} || binChunkTypeData[3] != byte{0}) {
			throw gltf::Error{"Invalid binary glTF format: Incorrect BIN chunk type.", {.filepath = sourceFilepath}};
		}
		uint32_t binChunkLength = 0;
		memcpy(&binChunkLength, binChunkLengthData.data(), binChunkLengthData.size());
		binChunkLength = convertLittleEndianToHostEndian(binChunkLength);
		chunks = chunks.subspan(8);
		if (static_cast<size_t>(binChunkLength) > chunks.size()) {
			throw gltf::Error{"Invalid binary glTF format: Invalid BIN chunk length.", {.filepath = sourceFilepath}};
		}
		binChunk = chunks.subspan(0, static_cast<size_t>(binChunkLength));
	}
	return Asset::parse(jsonString, SourceLocation{.filepath = sourceFilepath}, binChunk);
}

} // namespace grem::gltf
