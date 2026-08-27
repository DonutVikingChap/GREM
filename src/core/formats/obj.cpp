// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/obj.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>

namespace grem::obj {

namespace {

class Parser {
public:
	[[nodiscard]] static constexpr bool isWhitespace(char ch) noexcept {
		return ch == ' ' || ch == '\t';
	}

	[[nodiscard]] static constexpr bool isDecimalDigit(char ch) noexcept {
		return ch >= '0' && ch <= '9';
	}

	Parser(StringView string, size_t lineNumber) noexcept
		: it(string.begin())
		, end(string.end())
		, lineNumber(lineNumber) {}

	void parseAsset(Asset& output) {
		output.nodes.push_back(Node{.meshes{Mesh{}}});
		do {
			skipWhitespace();
			if (it == end) {
				break;
			}
			if (*it == '#') {
				++it;
			} else if (readCommand("mtllib")) {
				output.materialLibraryFilenames.push_back(parseString());
			} else if (readCommand("usemtl")) {
				if (!output.nodes.back().meshes.back().materialName.empty()) {
					output.nodes.back().meshes.push_back(Mesh{.materialName = parseString()});
				} else {
					output.nodes.back().meshes.back().materialName = parseString();
				}
			} else if (readCommand("o")) {
				if (!output.nodes.back().name.empty()) {
					output.nodes.push_back(Node{.name = parseString(), .meshes{Mesh{}}});
				} else {
					output.nodes.back().name = parseString();
				}
			} else if (readCommand("g")) {
				output.nodes.back().meshes.push_back(Mesh{.name = parseString()});
			} else if (readCommand("v")) {
				output.vertices.push_back(parseVec3());
			} else if (readCommand("vt")) {
				output.textureCoordinates.push_back(parseVec2());
			} else if (readCommand("vn")) {
				output.normals.push_back(parseVec3());
			} else if (readCommand("f")) {
				output.nodes.back().meshes.back().faces.push_back(parseFace(output.vertices.size(), output.textureCoordinates.size(), output.normals.size()));
			}
			skipLine();
		} while (it != end);
	}

	void parseMaterialLibrary(mtl::Library& output) {
		output.materials.push_back(mtl::Material{});
		do {
			skipWhitespace();
			if (it == end) {
				break;
			}
			if (*it == '#') {
				++it;
			} else if (readCommand("newmtl")) {
				if (!output.materials.back().name.empty()) {
					output.materials.push_back(mtl::Material{.name = parseString()});
				} else {
					output.materials.back().name = parseString();
				}
			} else if (readCommand("map_Ka")) {
				output.materials.back().ambientMapName = parseString();
			} else if (readCommand("map_Kd")) {
				output.materials.back().diffuseMapName = parseString();
			} else if (readCommand("map_Ks")) {
				output.materials.back().specularMapName = parseString();
			} else if (readCommand("map_Ke")) {
				output.materials.back().emissiveMapName = parseString();
			} else if (readCommand("map_Ns")) {
				output.materials.back().specularExponentMapName = parseString();
			} else if (readCommand("map_d")) {
				output.materials.back().dissolveFactorMapName = parseString();
			} else if (readCommand("map_bump") || readCommand("bump")) {
				output.materials.back().bumpMapName = parseString();
			} else if (readCommand("map_norm") || readCommand("norm")) {
				output.materials.back().normalMapName = parseString();
			} else if (readCommand("map_ORM")) {
				output.materials.back().occlusionRoughnessMetallicMapName = parseString();
			} else if (readCommand("Ka")) {
				output.materials.back().ambientColor = parseVec3();
			} else if (readCommand("Kd")) {
				output.materials.back().diffuseColor = parseVec3();
			} else if (readCommand("Ks")) {
				output.materials.back().specularColor = parseVec3();
			} else if (readCommand("Ke")) {
				output.materials.back().emissiveColor = parseVec3();
			} else if (readCommand("Ns")) {
				output.materials.back().specularExponent = parseFloat();
			} else if (readCommand("d")) {
				output.materials.back().dissolveFactor = parseFloat();
			} else if (readCommand("Pr")) {
				output.materials.back().roughnessFactor = parseFloat();
			} else if (readCommand("Pm")) {
				output.materials.back().metallicFactor = parseFloat();
			} else if (readCommand("Tr")) {
				output.materials.back().dissolveFactor = 1.0f - parseFloat();
			} else if (readCommand("illum")) {
				output.materials.back().illuminationModel = parseIlluminationModel();
			}
			skipLine();
		} while (it != end);
	}

private:
	void skipWhitespace() {
		while (it != end && isWhitespace(*it)) {
			++it;
		}
	}

	void skipLine() {
		while (it != end) {
			if (*it == '\r') {
				++it;
				if (it != end && *it == '\n') {
					++it;
				}
				++lineNumber;
				break;
			}
			if (*it == '\n') {
				++it;
				++lineNumber;
				break;
			}
			++it;
		}
	}

	[[nodiscard]] bool readCommand(StringView command) {
		StringView::iterator p = it;
		for (const char ch : command) {
			if (p == end || *p != ch) {
				return false;
			}
			++p;
		}
		if (p == end) {
			it = p;
			return true;
		}
		if (isWhitespace(*p)) {
			it = p;
			++it;
			skipWhitespace();
			return true;
		}
		return false;
	}

	[[nodiscard]] String parseString() {
		const StringView::iterator begin = it;
		while (it != end && *it != '\r' && *it != '\n') {
			++it;
		}
		if (it == begin) {
			throw obj::Error{"Missing string.", it, lineNumber};
		}
		return String{begin, it};
	}

	[[nodiscard]] bool parseSign() {
		if (it == end) {
			return false;
		}
		if (*it == '+') {
			++it;
			return false;
		}
		if (*it == '-') {
			++it;
			return true;
		}
		return false;
	}

	template <typename T>
	[[nodiscard]] T parseUnsignedInteger() {
		T result = 0;
		while (it != end && isDecimalDigit(*it)) {
			result = static_cast<T>(result * T{10} + static_cast<T>(*it++ - '0'));
		}
		return result;
	}

	template <typename T>
	[[nodiscard]] T parseSignedInteger() {
		const bool sign = parseSign();
		T result = parseUnsignedInteger<T>();
		result = (sign) ? -result : result;
		return result;
	}

	[[nodiscard]] float parseFloat() {
		const bool sign = parseSign();
		float result = parseUnsignedInteger<float>();
		if (it != end && *it == '.') {
			++it;
			float digitValue = 0.1f;
			while (it != end && isDecimalDigit(*it)) {
				result += static_cast<float>(*it++ - '0') * digitValue;
				digitValue *= 0.1f;
			}
		}

		if (it != end && (*it == 'e' || *it == 'E')) {
			++it;
			int exponent = parseSignedInteger<int>();
			if (exponent < 0) {
				exponent = -exponent;
				for (int i = 0; i < exponent; ++i) {
					result *= 0.1f;
				}
			} else {
				for (int i = 0; i < exponent; ++i) {
					result *= 10.0f;
				}
			}
		}
		result = (sign) ? -result : result;
		return result;
	}

	[[nodiscard]] vec2 parseVec2() {
		vec2 result{};
		result.x = parseFloat();
		skipWhitespace();
		result.y = parseFloat();
		return result;
	}

	[[nodiscard]] vec3 parseVec3() {
		vec3 result{};
		result.x = parseFloat();
		skipWhitespace();
		result.y = parseFloat();
		skipWhitespace();
		result.z = parseFloat();
		return result;
	}

	[[nodiscard]] FaceVertex parseFaceVertex(size_t vertexCount, size_t textureCoordinateCount, size_t normalCount) {
		FaceVertex result{};
		const int64_t vertexIndex = parseSignedInteger<int64_t>();
		result.vertexIndex = static_cast<uint32_t>((vertexIndex < 0) ? static_cast<int64_t>(vertexCount) + vertexIndex : vertexIndex - 1);
		if (it == end || *it != '/') {
			return result;
		}
		++it;
		const int64_t textureCoordinateIndex = parseSignedInteger<int64_t>();
		result.textureCoordinateIndex =
			static_cast<uint32_t>((textureCoordinateIndex < 0) ? static_cast<int64_t>(textureCoordinateCount) + textureCoordinateIndex : textureCoordinateIndex - 1);
		if (it == end || *it != '/') {
			return result;
		}
		++it;
		const int64_t normalIndex = parseSignedInteger<int64_t>();
		result.normalIndex = static_cast<uint32_t>((normalIndex < 0) ? static_cast<int64_t>(normalCount) + normalIndex : normalIndex - 1);
		return result;
	}

	[[nodiscard]] Face parseFace(size_t vertexCount, size_t textureCoordinateCount, size_t normalCount) {
		Face result{};
		while (it != end && isDecimalDigit(*it)) {
			result.vertices.push_back(parseFaceVertex(vertexCount, textureCoordinateCount, normalCount));
			skipWhitespace();
		}
		return result;
	}

	[[nodiscard]] mtl::IlluminationModel parseIlluminationModel() {
		const uint8_t illum = parseUnsignedInteger<uint8_t>();
		if (illum > 10) {
			throw obj::Error{"Invalid illumination model.", it, lineNumber};
		}
		return static_cast<mtl::IlluminationModel>(illum);
	}

	StringView::iterator it;
	StringView::iterator end;
	size_t lineNumber;
};

} // namespace

Asset Asset::parse(StringView objString) {
	Asset result{};
	Parser{objString, 1}.parseAsset(result);
	return result;
}

namespace mtl {

void Library::load(StringView mtlString) {
	Parser{mtlString, 1}.parseMaterialLibrary(*this);
}

} // namespace mtl
} // namespace grem::obj
