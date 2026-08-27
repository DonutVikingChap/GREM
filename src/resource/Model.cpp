// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/ConvexPolytope.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/ascii.hpp>
#include <GREM/core/formats/gltf.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/formats/obj.hpp>
#include <GREM/core/formats/unicode.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/core/time.hpp>
#include <GREM/resource/Error.hpp>
#include <GREM/resource/Image.hpp>
#include <GREM/resource/Model.hpp>

#include <new>       // std::launder
#include <stdexcept> // std::length_error
#include <utility>   // std::move

namespace grem::resource {

namespace {

[[nodiscard]] constexpr bool isPotentiallyTransparentImageFormat(ImageFormat format) noexcept {
	switch (format) {
		case ImageFormat::UNKNOWN: [[fallthrough]];
		case ImageFormat::R8_UINT: [[fallthrough]];
		case ImageFormat::R16_FLOAT: [[fallthrough]];
		case ImageFormat::R32_FLOAT: [[fallthrough]];
		case ImageFormat::R8G8_UINT: [[fallthrough]];
		case ImageFormat::R16G16_FLOAT: [[fallthrough]];
		case ImageFormat::R32G32_FLOAT: [[fallthrough]];
		case ImageFormat::R8G8B8_UINT: [[fallthrough]];
		case ImageFormat::R16G16B16_FLOAT: [[fallthrough]];
		case ImageFormat::R32G32B32_FLOAT: [[fallthrough]];
		case ImageFormat::R5G6B5_UINT_PACK16: [[fallthrough]];
		case ImageFormat::B10G11R11_UFLOAT_PACK32: [[fallthrough]];
		case ImageFormat::BC1_RGB_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::BC4_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::BC5_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::BC6H_RGB_UFLOAT_BLOCK: [[fallthrough]];
		case ImageFormat::BC6H_RGB_FLOAT_BLOCK: [[fallthrough]];
		case ImageFormat::ETC2_R8G8B8_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::EAC_R11_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::EAC_R11G11_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_R_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: return false;
		case ImageFormat::R8G8B8A8_UINT: [[fallthrough]];
		case ImageFormat::R16G16B16A16_FLOAT: [[fallthrough]];
		case ImageFormat::R32G32B32A32_FLOAT: [[fallthrough]];
		case ImageFormat::A1R5G5B5_UINT_PACK16: [[fallthrough]];
		case ImageFormat::A2B10G10R10_UINT_PACK32: [[fallthrough]];
		case ImageFormat::ASTC_4x4_RGBA_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::BC3_RGBA_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::BC7_RGBA_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::ETC2_R8G8B8A8_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::PVRTC1_4BPP_RGBA_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: [[fallthrough]];
		case ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return true;
	}
	unreachable();
}

template <typename OutputValue>
void fillWithZeroedValues(Span<byte> output) {
	GREM_ASSERT(output.size() % sizeof(OutputValue) == 0);
	if (!output.empty()) {
		memset(output.data(), 0, output.size());
	}
}

template <typename OutputValue>
void fillWithZeroedValues(StridedSpan<byte> output) {
	GREM_ASSERT(output.stride() >= sizeof(OutputValue));
	if (output.stride() == sizeof(OutputValue)) {
		if (!output.empty()) {
			memset(output.base(), 0, output.size() * sizeof(OutputValue));
		}
	} else {
		const auto end = output.end();
		for (auto it = output.begin(); it != end; ++it) {
			memset(it.base(), 0, sizeof(OutputValue));
		}
	}
}

template <typename OutputValue>
void fillWithValues(Span<byte> output, const OutputValue& value) {
	GREM_ASSERT(output.size() % sizeof(OutputValue) == 0);
	const size_t n = output.size() / sizeof(OutputValue);
	for (size_t i = 0; i < n; ++i) {
		memcpy(output.data() + i * sizeof(OutputValue), &value, sizeof(OutputValue));
	}
}

template <typename OutputValue>
void fillWithValues(StridedSpan<byte> output, const OutputValue& value) {
	GREM_ASSERT(output.stride() >= sizeof(OutputValue));
	const auto end = output.end();
	for (auto it = output.begin(); it != end; ++it) {
		memcpy(it.base(), &value, sizeof(OutputValue));
	}
}

template <typename Value>
void writeValues(byte* output, StridedSpan<const Value> values) {
	if (values.stride() == sizeof(Value)) {
		if (!values.empty()) {
			memcpy(output, values.base(), values.size() * sizeof(Value));
		}
	} else {
		const auto end = values.end();
		for (auto it = values.begin(); it != end; ++it) {
			memcpy(output, it.base(), sizeof(Value));
			output += sizeof(Value);
		}
	}
}

// clang-format off
template <typename T> struct component_type : meta::Type<T> {};
template <> struct component_type<iA2B10G10R10vec4norm> : meta::Type<void> {};
template <> struct component_type<uA2B10G10R10vec4norm> : meta::Type<void> {};
template <> struct component_type<mat2> : meta::Type<float> {};
template <> struct component_type<mat3> : meta::Type<float> {};
template <> struct component_type<mat4> : meta::Type<float> {};
template <typename T> requires(requires { typename T::Component; }) struct component_type<T> : meta::Type<typename T::Component> {};
// clang-format on

template <typename T>
using component_type_t = typename component_type<T>::type;

// clang-format off
template <typename T> struct unnormalized_component_type : meta::Type<T> {};
template <> struct unnormalized_component_type<iA2B10G10R10vec4norm> : meta::Type<void> {};
template <> struct unnormalized_component_type<uA2B10G10R10vec4norm> : meta::Type<void> {};
template <> struct unnormalized_component_type<i8norm> : meta::Type<int8_t> {};
template <> struct unnormalized_component_type<u8norm> : meta::Type<uint8_t> {};
template <> struct unnormalized_component_type<i16norm> : meta::Type<int16_t> {};
template <> struct unnormalized_component_type<u16norm> : meta::Type<uint16_t> {};
template <> struct unnormalized_component_type<mat2> : meta::Type<float> {};
template <> struct unnormalized_component_type<mat3> : meta::Type<float> {};
template <> struct unnormalized_component_type<mat4> : meta::Type<float> {};
template <typename T> requires(requires { typename T::Component; }) struct unnormalized_component_type<T> : unnormalized_component_type<typename T::Component> {};
// clang-format on

template <typename T>
using unnormalized_component_type_t = typename unnormalized_component_type<T>::type;

// clang-format off
template <typename T> struct component_count : meta::Constant<size_t{1}> {};
template <typename T> requires(requires { T::RANK; }) struct component_count<T> : meta::Constant<T::RANK> {};
template <> struct component_count<mat2> : meta::Constant<size_t{4}> {};
template <> struct component_count<mat3> : meta::Constant<size_t{9}> {};
template <> struct component_count<mat4> : meta::Constant<size_t{16}> {};
// clang-format on

template <typename T>
constexpr size_t component_count_v = component_count<T>::value;

// clang-format off
template <typename T> struct is_normalized_component_type : meta::Constant<false> {};
template <> struct is_normalized_component_type<float> : meta::Constant<true> {};
template <> struct is_normalized_component_type<i8norm> : meta::Constant<true> {};
template <> struct is_normalized_component_type<u8norm> : meta::Constant<true> {};
template <> struct is_normalized_component_type<i16norm> : meta::Constant<true> {};
template <> struct is_normalized_component_type<u16norm> : meta::Constant<true> {};
// clang-format on

template <typename T>
constexpr bool is_normalized_component_type_v = is_normalized_component_type<T>::value;

class GlTFBufferView {
public:
	GlTFBufferView(const gltf::Asset& glTFAsset, Span<const Span<const byte>> glTFBufferContents, StringView name, const gltf::BufferView& bufferView, size_t count,
		size_t componentCount, size_t componentSize, size_t byteOffset) {
		const size_t valueSize = componentCount * componentSize;

		const size_t inputByteStride = bufferView.byteStride.value_or(valueSize);
		if (inputByteStride < valueSize || inputByteStride > 252 || inputByteStride % componentSize != 0) {
			throw resource::Error{formatString("Invalid byte stride in {} buffer view.", name)};
		}
		if (bufferView.byteLength < 1) {
			throw resource::Error{formatString("Invalid byte length in {} buffer view.", name)};
		}
		const size_t accessorByteLength = inputByteStride * (count - 1) + valueSize;
		if (byteOffset >= bufferView.byteLength || accessorByteLength > bufferView.byteLength - byteOffset) {
			throw resource::Error{formatString("Invalid buffer view range in {} accessor.", name)};
		}

		if (bufferView.buffer >= glTFAsset.buffers.size()) {
			throw resource::Error{formatString("Invalid buffer index in {} buffer view.", name)};
		}
		const gltf::Buffer& buffer = glTFAsset.buffers[bufferView.buffer];
		if (buffer.byteLength < 1) {
			throw resource::Error{formatString("Invalid byte length in {} buffer.", name)};
		}
		if (bufferView.byteOffset >= buffer.byteLength || bufferView.byteLength > buffer.byteLength - bufferView.byteOffset) {
			throw resource::Error{formatString("Invalid buffer range in {} buffer view.", name)};
		}

		if (bufferView.buffer >= glTFBufferContents.size()) {
			throw resource::Error{formatString("Invalid buffer index in {} buffer view.", name)};
		}
		const Span<const byte> bytes = glTFBufferContents[bufferView.buffer];
		if (bytes.size() < buffer.byteLength) {
			throw resource::Error{formatString("Invalid byte length in {} buffer.", name)};
		}
		const size_t finalByteOffset = bufferView.byteOffset + byteOffset;
		if (finalByteOffset >= buffer.byteLength || accessorByteLength > buffer.byteLength - finalByteOffset) {
			throw resource::Error{formatString("Invalid accessor range in {} buffer view.", name)};
		}

		data = bytes.data() + finalByteOffset;
		byteStride = inputByteStride;
		byteLength = accessorByteLength;
	}

	GlTFBufferView(const gltf::Asset& glTFAsset, Span<const Span<const byte>> glTFBufferContents, StringView name, const gltf::BufferView& bufferView) {
		const size_t inputByteStride = bufferView.byteStride.value_or(1);
		if (bufferView.byteLength < 1) {
			throw resource::Error{formatString("Invalid byte length in {} buffer view.", name)};
		}

		if (bufferView.buffer >= glTFAsset.buffers.size()) {
			throw resource::Error{formatString("Invalid buffer index in {} buffer view.", name)};
		}
		const gltf::Buffer& buffer = glTFAsset.buffers[bufferView.buffer];
		if (buffer.byteLength < 1) {
			throw resource::Error{formatString("Invalid byte length in {} buffer.", name)};
		}
		if (bufferView.byteOffset >= buffer.byteLength || bufferView.byteLength > buffer.byteLength - bufferView.byteOffset) {
			throw resource::Error{formatString("Invalid buffer range in {} buffer view.", name)};
		}

		if (bufferView.buffer >= glTFBufferContents.size()) {
			throw resource::Error{formatString("Invalid buffer index in {} buffer view.", name)};
		}
		const Span<const byte> bytes = glTFBufferContents[bufferView.buffer];
		if (bytes.size() < buffer.byteLength) {
			throw resource::Error{formatString("Invalid byte length in {} buffer.", name)};
		}
		if (bufferView.byteOffset >= buffer.byteLength || bufferView.byteLength > buffer.byteLength - bufferView.byteOffset) {
			throw resource::Error{formatString("Invalid buffer range in {} buffer view.", name)};
		}

		data = bytes.data() + bufferView.byteOffset;
		byteStride = inputByteStride;
		byteLength = bufferView.byteLength;
	}

	[[nodiscard]] size_t getByteLength() const noexcept {
		return byteLength;
	}

	[[nodiscard]] Span<const byte> readTemporaryBytes(Allocation<byte>& outputStorage, size_t inputByteLength) const noexcept {
		GREM_PROFILE_FUNCTION();

		const byte* inputData = data;
		const size_t inputByteStride = byteStride;
		if (inputByteStride == 1) {
			return Span<const byte>{inputData, inputByteLength};
		}
		outputStorage.resize(inputByteLength);
		for (byte& outputByte : outputStorage) {
			memcpy(&outputByte, inputData, sizeof(byte));
			inputData += inputByteStride;
		}
		return outputStorage;
	}

	template <typename InputComponent, typename OutputValue>
	void readValues(Span<byte> outputValues, size_t componentCount) const noexcept {
		GREM_PROFILE_FUNCTION();

		const byte* inputData = data;
		const size_t inputByteStride = byteStride;
		if constexpr (same_as<InputComponent, component_type_t<OutputValue>> && HOST_IS_LITTLE_ENDIAN) {
			if (inputByteStride == sizeof(OutputValue)) {
				if (!outputValues.empty()) {
					memcpy(outputValues.data(), inputData, outputValues.size_bytes());
				}
			} else {
				const size_t valueCount = outputValues.size_bytes() / sizeof(OutputValue);
				for (size_t valueIndex = 0; valueIndex < valueCount; ++valueIndex) {
					memcpy(outputValues.data() + valueIndex * sizeof(OutputValue), inputData, sizeof(OutputValue));
					inputData += inputByteStride;
				}
			}
		} else {
			const size_t valueCount = outputValues.size_bytes() / sizeof(OutputValue);
			for (size_t valueIndex = 0; valueIndex < valueCount; ++valueIndex) {
				readValue<InputComponent, OutputValue>(outputValues.data() + valueIndex * sizeof(OutputValue), inputData, componentCount);
				inputData += inputByteStride;
			}
		}
	}

	template <typename InputComponent, typename OutputValue>
	void readValues(StridedSpan<byte> outputValues, size_t componentCount) const noexcept {
		GREM_PROFILE_FUNCTION();

		GREM_ASSERT(outputValues.stride() >= sizeof(OutputValue));
		const byte* inputData = data;
		byte* outputData = outputValues.base();
		const size_t inputByteStride = byteStride;
		const size_t outputByteStride = outputValues.stride();
		if constexpr (same_as<InputComponent, component_type_t<OutputValue>> && HOST_IS_LITTLE_ENDIAN) {
			if (componentCount == component_count_v<OutputValue>) {
				if (inputByteStride == sizeof(OutputValue) && outputByteStride == sizeof(OutputValue)) {
					if (!outputValues.empty()) {
						memcpy(outputValues.base(), inputData, outputValues.size() * sizeof(OutputValue));
					}
				} else {
					for (size_t valueIndex = 0; valueIndex < outputValues.size(); ++valueIndex) {
						memcpy(outputData, inputData, sizeof(OutputValue));
						inputData += inputByteStride;
						outputData += outputByteStride;
					}
				}
			} else {
				for (size_t valueIndex = 0; valueIndex < outputValues.size(); ++valueIndex) {
					readValue<InputComponent, OutputValue>(outputData, inputData, componentCount);
					inputData += inputByteStride;
					outputData += outputByteStride;
				}
			}
		} else {
			for (size_t valueIndex = 0; valueIndex < outputValues.size(); ++valueIndex) {
				readValue<InputComponent, OutputValue>(outputData, inputData, componentCount);
				inputData += inputByteStride;
				outputData += outputByteStride;
			}
		}
	}

	template <typename InputComponent, typename OutputValue>
	void readValuesSparse(Span<byte> outputValues, size_t componentCount, Span<const uint32_t> outputIndices) const {
		GREM_PROFILE_FUNCTION();

		GREM_ASSERT(outputValues.size_bytes() / sizeof(OutputValue) <= size_t{Limits<uint32_t>::MAX});
		const uint32_t valueCount = static_cast<uint32_t>(outputValues.size_bytes() / sizeof(OutputValue));

		const byte* inputData = data;
		const size_t inputByteStride = byteStride;
		for (const uint32_t outputIndex : outputIndices) {
			if (outputIndex >= valueCount) {
				throw resource::Error{"Sparse accessor index is outside the range of values."};
			}
			readValue<InputComponent, OutputValue>(outputValues.data() + outputIndex * sizeof(OutputValue), inputData, componentCount);
			inputData += inputByteStride;
		}
	}

	template <typename InputComponent, typename OutputValue>
	void readValuesSparse(StridedSpan<byte> outputValues, size_t componentCount, Span<const uint32_t> outputIndices) const {
		GREM_PROFILE_FUNCTION();

		GREM_ASSERT(outputValues.size() <= size_t{Limits<uint32_t>::MAX});

		const byte* inputData = data;
		byte* const outputData = outputValues.base();
		const size_t inputByteStride = byteStride;
		const size_t outputByteStride = outputValues.stride();
		for (const uint32_t outputIndex : outputIndices) {
			if (outputIndex >= outputValues.size()) {
				throw resource::Error{"Sparse accessor index is outside the range of values."};
			}
			readValue<InputComponent, OutputValue>(outputData + outputIndex * outputByteStride, inputData, componentCount);
			inputData += inputByteStride;
		}
	}

private:
	template <typename InputComponent, typename OutputValue>
	static void readValue(byte* outputValue, const byte* inputData, size_t componentCount) noexcept {
		constexpr size_t OUTPUT_COMPONENT_COUNT = component_count_v<OutputValue>;
		GREM_ASSERT(componentCount <= OUTPUT_COMPONENT_COUNT);
		if constexpr (same_as<OutputValue, iA2B10G10R10vec4norm> || same_as<OutputValue, uA2B10G10R10vec4norm>) {
			vec4 components{};
			for (size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex) {
				readComponent<InputComponent, float>(components[componentIndex], inputData + componentIndex * sizeof(InputComponent));
			}
			const OutputValue value = components;
			memcpy(outputValue, &value, sizeof(OutputValue));
		} else {
			using OutputComponent = component_type_t<OutputValue>;
			Array<OutputComponent, OUTPUT_COMPONENT_COUNT> components{};
			for (size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex) {
				readComponent<InputComponent, OutputComponent>(components[componentIndex], inputData + componentIndex * sizeof(InputComponent));
			}
			memcpy(outputValue, components.data(), sizeof(OutputValue));
		}
	}

	template <typename InputComponent, typename OutputComponent>
	static void readComponent(OutputComponent& outputComponent, const byte* inputData) {
		InputComponent inputComponent;
		memcpy(&inputComponent, inputData, sizeof(InputComponent));
		if constexpr (!HOST_IS_LITTLE_ENDIAN) {
			inputComponent = convertLittleEndianToHostEndian(inputComponent);
		}
		if constexpr (same_as<InputComponent, OutputComponent>) {
			outputComponent = inputComponent;
		} else if constexpr (is_normalized_component_type_v<OutputComponent>) {
			using UnnormalizedOutputComponent = unnormalized_component_type_t<OutputComponent>;
			if constexpr (same_as<InputComponent, UnnormalizedOutputComponent>) {
				outputComponent = bit_cast<OutputComponent>(inputComponent);
			} else {
				float f{};
				if constexpr (same_as<InputComponent, int8_t>) {
					f = max(static_cast<float>(inputComponent) / 127.0f, -1.0f);
				} else if constexpr (same_as<InputComponent, uint8_t>) {
					f = static_cast<float>(inputComponent) / 255.0f;
				} else if constexpr (same_as<InputComponent, int16_t>) {
					f = max(static_cast<float>(inputComponent) / 32767.0f, -1.0f);
				} else if constexpr (same_as<InputComponent, uint16_t>) {
					f = static_cast<float>(inputComponent) / 65535.0f;
				} else {
					f = static_cast<float>(inputComponent);
				}
				outputComponent = f;
			}
		} else {
			if constexpr (integral<InputComponent> && integral<OutputComponent>) {
				if constexpr (static_cast<intmax_t>(Limits<OutputComponent>::MIN) > static_cast<intmax_t>(Limits<InputComponent>::MIN)) {
					if (inputComponent < static_cast<InputComponent>(Limits<OutputComponent>::MIN)) {
						throw resource::Error{"Accessor component value is outside of the supported range."};
					}
				}
				if constexpr (uintmax_t{Limits<OutputComponent>::MAX} < uintmax_t{Limits<InputComponent>::MAX}) {
					if (inputComponent > InputComponent{Limits<OutputComponent>::MAX}) {
						throw resource::Error{"Accessor component value is outside of the supported range."};
					}
				}
			}
			outputComponent = static_cast<OutputComponent>(inputComponent);
		}
	}

	const byte* data = nullptr;
	size_t byteStride = 0;
	size_t byteLength = 0;
};

class GlTFModelLoader {
public:
	GlTFModelLoader(Model& model, const gltf::Asset& glTFAsset, FunctionView<Model::Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
		FunctionView<Variant<Allocation<byte>, Span<const byte>>(CStringView relativeFilepath)> loadBufferData, const ModelOptions& options)
		: model(model)
		, glTFAsset(glTFAsset)
		, loadImage(loadImage)
		, loadBufferData(loadBufferData)
		, options(options) {}

	void load() && {
		GREM_PROFILE_FUNCTION();

		validateInputScenes();
		validateInputSkins();
		validateInputAnimations();
		validateInputNodes();
		validateInputMeshes();
		validateInputTextures();
		validateInputMaterials();
		validateInputLights();
		validateInputCollisionShapes();
		validateInputPhysicsJoints();
		initializeBufferContents();
		initializeTemporaryData();
		markJoints();
		allocateTextures();
		allocateMaterials();
		allocateMeshes();
		allocateNodes();
		allocateAnimations();
		loadSkins();
		loadTextures();
		loadMaterials();
		loadMeshes();
		loadNodes();
		translateJointIndices();
		loadAnimations();
		loadCollidersAndPhysics();
	}

private:
	struct TemporaryMeshData {
		// Set when loading meshes,
		// needed when loading nodes to determine which mesh to assign to the mesh instance,
		// and when loading colliders to determine which meshes to read mesh data from:
		Model::MeshIndex meshOffset = 0;

		// Set when loading nodes,
		// needed when translating joint indices to find the old joint indices:
		Optional<gltf::SkinIndex> glTFSkinIndex{};
	};

	struct TemporaryNodeData {
		// Set when loading nodes,
		// needed when loading animations to know which joint to affect,
		// and when loading physics joints to determine the attachment frame joints,
		// and when translating joint indices to find the new joint index of nodes:
		Model::JointIndex jointIndex = 0;

		// Set when loading nodes,
		// needed when loading animations to know which morph target weights to affect:
		Model::MorphTargetWeightIndex morphTargetWeightOffset = 0;
		Model::MorphTargetWeightCount morphTargetWeightCount = 0;

		// Set when loading nodes,
		// needed when loading physics objects to populate jointPhysicsObjectIndices,
		// and when loading physics joints to determine the connected objects:
		Model::PhysicsObjectIndex physicsObjectIndex = 0;

		// Is directly referenced by a skin, animation, light or physics joint, or has a non-identity transformation and is a descendant of a dynamic joint.
		// Set when marking joints,
		// needed when allocating/loading nodes to determine if the node should add a new dynamic joint:
		bool isDynamicJoint = false;

		// Is not a dynamic joint, and is not a descendant of a dynamic joint, but has a non-identity transformation.
		// Set when marking joints,
		// needed when allocating/loading nodes to determine if the node should add a new static joint:
		bool isStaticJoint = false;
	};

	struct TemporarySkinData {
		struct TemporarySkinnedNodeData {
			// Set when loading skins,
			// needed when loading nodes to determine if an inverse bind matrix override should be used for a dynamic joint.
			// If this is not set, calculate the inverse matrix of the initial global node transformation relative to the skeleton root:
			Optional<mat4> inverseBindPoseMatrix{};
		};

		Allocation<TemporarySkinnedNodeData> skinnedNodesData{};

		// Set when loading skins,
		// needed when loading skinned nodes to determine the skinDataOffset to assign to the mesh instance:
		Model::ValueOffset skinDataOffset;
	};

	void validateInputScenes();
	void validateInputSkins();
	void validateInputAnimations();
	void validateInputNodes();
	void validateInputMeshes();
	void validateInputTextures();
	void validateInputMaterials();
	void validateInputLights();
	void validateInputCollisionShapes();
	void validateInputPhysicsJoints();
	void initializeBufferContents();
	void initializeTemporaryData();
	void markJoints();
	void allocateTextures();
	void allocateMaterials();
	void allocateMeshes();
	void allocateNodes();
	void allocateAnimations();
	void loadSkins();
	void loadTextures();
	void loadMaterials();
	void loadMeshes();
	void loadNodes();
	void translateJointIndices();
	void loadAnimations();
	void loadCollidersAndPhysics();

	template <typename OutputValue>
	void readAccessorValues(Span<byte> output, StringView name, Span<const gltf::Accessor::ComponentType> allowedComponentTypes,
		Span<const gltf::Accessor::Type> allowedAccessorTypes, const gltf::Accessor& accessor) {
		GREM_PROFILE_FUNCTION();

		GREM_ASSERT(output.size_bytes() / sizeof(OutputValue) == accessor.count);

		const size_t count = accessor.count;

		const gltf::Accessor::Type type = accessor.type;
		if (!contains(allowedAccessorTypes, type)) {
			throw resource::Error{formatString("Invalid {} accessor type.", name)};
		}

		const gltf::Accessor::ComponentType componentType = accessor.componentType;
		if (!contains(allowedComponentTypes, componentType)) {
			throw resource::Error{formatString("Invalid {} accessor component type.", name)};
		}

		const size_t componentCount = [type]() -> size_t {
			switch (type) {
				case gltf::Accessor::Type::SCALAR: return 1;
				case gltf::Accessor::Type::VEC2: return 2;
				case gltf::Accessor::Type::VEC3: return 3;
				case gltf::Accessor::Type::VEC4: return 4;
				case gltf::Accessor::Type::MAT2: return 4;
				case gltf::Accessor::Type::MAT3: return 9;
				case gltf::Accessor::Type::MAT4: return 16;
			}
			return 0;
		}();
		GREM_ASSERT(componentCount <= component_count_v<OutputValue>);
		const size_t componentSize = [componentType]() -> size_t {
			switch (componentType) {
				case gltf::Accessor::ComponentType::I8: return sizeof(int8_t);
				case gltf::Accessor::ComponentType::U8: return sizeof(uint8_t);
				case gltf::Accessor::ComponentType::I16: return sizeof(int16_t);
				case gltf::Accessor::ComponentType::U16: return sizeof(uint16_t);
				case gltf::Accessor::ComponentType::U32: return sizeof(uint32_t);
				case gltf::Accessor::ComponentType::F32: return sizeof(float);
			}
			return 0;
		}();

		if (accessor.bufferView) {
			if (*accessor.bufferView >= glTFAsset.bufferViews.size()) {
				throw resource::Error{formatString("Invalid {} accessor buffer view index.", name)};
			}
			const GlTFBufferView bufferView{glTFAsset, glTFBufferContents, name, glTFAsset.bufferViews[*accessor.bufferView], count, componentCount, componentSize,
				accessor.byteOffset};
			switch (componentType) {
				case gltf::Accessor::ComponentType::I8: bufferView.readValues<int8_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::U8: bufferView.readValues<uint8_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::I16: bufferView.readValues<int16_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::U16: bufferView.readValues<uint16_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::U32: bufferView.readValues<uint32_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::F32: bufferView.readValues<float, OutputValue>(output, componentCount); break;
			}
		} else if (!output.empty()) {
			memset(output.data(), 0, output.size_bytes());
		}

		if (accessor.sparse) {
			const size_t sparseCount = accessor.sparse->count;

			const gltf::Accessor::Sparse::Indices::ComponentType sparseIndicesComponentType = accessor.sparse->indices.componentType;
			const size_t sparseIndicesComponentSize = [sparseIndicesComponentType]() -> size_t {
				switch (sparseIndicesComponentType) {
					case gltf::Accessor::Sparse::Indices::ComponentType::U8: return sizeof(uint8_t);
					case gltf::Accessor::Sparse::Indices::ComponentType::U16: return sizeof(uint16_t);
					case gltf::Accessor::Sparse::Indices::ComponentType::U32: return sizeof(uint32_t);
				}
				return 0;
			}();

			if (accessor.sparse->indices.bufferView >= glTFAsset.bufferViews.size()) {
				throw resource::Error{formatString("Invalid {} accessor sparse indices buffer view index.", name)};
			}
			const GlTFBufferView sparseIndicesBufferView{glTFAsset, glTFBufferContents, formatString("{} sparse indices", name),
				glTFAsset.bufferViews[accessor.sparse->indices.bufferView], sparseCount, 1, sparseIndicesComponentSize, accessor.sparse->indices.byteOffset};
			Allocation<uint32_t> sparseIndices{};
			sparseIndices.resize(sparseCount);
			switch (sparseIndicesComponentType) {
				case gltf::Accessor::Sparse::Indices::ComponentType::U8:
					sparseIndicesBufferView.readValues<uint8_t, uint32_t>(asWritableBytes(Span{sparseIndices}), componentCount);
					break;
				case gltf::Accessor::Sparse::Indices::ComponentType::U16:
					sparseIndicesBufferView.readValues<uint16_t, uint32_t>(asWritableBytes(Span{sparseIndices}), componentCount);
					break;
				case gltf::Accessor::Sparse::Indices::ComponentType::U32:
					sparseIndicesBufferView.readValues<uint32_t, uint32_t>(asWritableBytes(Span{sparseIndices}), componentCount);
					break;
			}

			if (accessor.sparse->values.bufferView >= glTFAsset.bufferViews.size()) {
				throw resource::Error{formatString("Invalid {} accessor sparse values buffer view index.", name)};
			}
			const GlTFBufferView sparseValuesBufferView{glTFAsset, glTFBufferContents, formatString("{} sparse values", name),
				glTFAsset.bufferViews[accessor.sparse->values.bufferView], sparseCount, componentCount, componentSize, accessor.sparse->values.byteOffset};
			switch (componentType) {
				case gltf::Accessor::ComponentType::I8: sparseValuesBufferView.readValuesSparse<int8_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::U8: sparseValuesBufferView.readValuesSparse<uint8_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::I16: sparseValuesBufferView.readValuesSparse<int16_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::U16: sparseValuesBufferView.readValuesSparse<uint16_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::U32: sparseValuesBufferView.readValuesSparse<uint32_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::F32: sparseValuesBufferView.readValuesSparse<float, OutputValue>(output, componentCount, sparseIndices); break;
			}
		}
	}

	template <typename OutputValue>
	void readAccessorValues(StridedSpan<byte> output, StringView name, Span<const gltf::Accessor::ComponentType> allowedComponentTypes,
		Span<const gltf::Accessor::Type> allowedAccessorTypes, const gltf::Accessor& accessor) {
		GREM_PROFILE_FUNCTION();

		GREM_ASSERT(output.size() == accessor.count);

		const size_t count = accessor.count;

		const gltf::Accessor::Type type = accessor.type;
		if (!contains(allowedAccessorTypes, type)) {
			throw resource::Error{formatString("Invalid {} accessor type.", name)};
		}

		const gltf::Accessor::ComponentType componentType = accessor.componentType;
		if (!contains(allowedComponentTypes, componentType)) {
			throw resource::Error{formatString("Invalid {} accessor component type.", name)};
		}

		const size_t componentCount = [type]() -> size_t {
			switch (type) {
				case gltf::Accessor::Type::SCALAR: return 1;
				case gltf::Accessor::Type::VEC2: return 2;
				case gltf::Accessor::Type::VEC3: return 3;
				case gltf::Accessor::Type::VEC4: return 4;
				case gltf::Accessor::Type::MAT2: return 4;
				case gltf::Accessor::Type::MAT3: return 9;
				case gltf::Accessor::Type::MAT4: return 16;
			}
			return 0;
		}();
		GREM_ASSERT(componentCount <= component_count_v<OutputValue>);
		const size_t componentSize = [componentType]() -> size_t {
			switch (componentType) {
				case gltf::Accessor::ComponentType::I8: return sizeof(int8_t);
				case gltf::Accessor::ComponentType::U8: return sizeof(uint8_t);
				case gltf::Accessor::ComponentType::I16: return sizeof(int16_t);
				case gltf::Accessor::ComponentType::U16: return sizeof(uint16_t);
				case gltf::Accessor::ComponentType::U32: return sizeof(uint32_t);
				case gltf::Accessor::ComponentType::F32: return sizeof(float);
			}
			return 0;
		}();

		if (accessor.bufferView) {
			if (*accessor.bufferView >= glTFAsset.bufferViews.size()) {
				throw resource::Error{formatString("Invalid {} accessor buffer view index.", name)};
			}
			const GlTFBufferView bufferView{glTFAsset, glTFBufferContents, name, glTFAsset.bufferViews[*accessor.bufferView], count, componentCount, componentSize,
				accessor.byteOffset};
			switch (componentType) {
				case gltf::Accessor::ComponentType::I8: bufferView.readValues<int8_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::U8: bufferView.readValues<uint8_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::I16: bufferView.readValues<int16_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::U16: bufferView.readValues<uint16_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::U32: bufferView.readValues<uint32_t, OutputValue>(output, componentCount); break;
				case gltf::Accessor::ComponentType::F32: bufferView.readValues<float, OutputValue>(output, componentCount); break;
			}
		} else {
			fillWithZeroedValues<OutputValue>(output);
		}

		if (accessor.sparse) {
			const size_t sparseCount = accessor.sparse->count;

			const gltf::Accessor::Sparse::Indices::ComponentType sparseIndicesComponentType = accessor.sparse->indices.componentType;
			const size_t sparseIndicesComponentSize = [sparseIndicesComponentType]() -> size_t {
				switch (sparseIndicesComponentType) {
					case gltf::Accessor::Sparse::Indices::ComponentType::U8: return sizeof(uint8_t);
					case gltf::Accessor::Sparse::Indices::ComponentType::U16: return sizeof(uint16_t);
					case gltf::Accessor::Sparse::Indices::ComponentType::U32: return sizeof(uint32_t);
				}
				return 0;
			}();

			if (accessor.sparse->indices.bufferView >= glTFAsset.bufferViews.size()) {
				throw resource::Error{formatString("Invalid {} accessor sparse indices buffer view index.", name)};
			}
			const GlTFBufferView sparseIndicesBufferView{glTFAsset, glTFBufferContents, formatString("{} sparse indices", name),
				glTFAsset.bufferViews[accessor.sparse->indices.bufferView], sparseCount, 1, sparseIndicesComponentSize, accessor.sparse->indices.byteOffset};
			Allocation<uint32_t> sparseIndices{};
			sparseIndices.resize(sparseCount);
			switch (sparseIndicesComponentType) {
				case gltf::Accessor::Sparse::Indices::ComponentType::U8:
					sparseIndicesBufferView.readValues<uint8_t, uint32_t>(asWritableBytes(Span{sparseIndices}), componentCount);
					break;
				case gltf::Accessor::Sparse::Indices::ComponentType::U16:
					sparseIndicesBufferView.readValues<uint16_t, uint32_t>(asWritableBytes(Span{sparseIndices}), componentCount);
					break;
				case gltf::Accessor::Sparse::Indices::ComponentType::U32:
					sparseIndicesBufferView.readValues<uint32_t, uint32_t>(asWritableBytes(Span{sparseIndices}), componentCount);
					break;
			}

			if (accessor.sparse->values.bufferView >= glTFAsset.bufferViews.size()) {
				throw resource::Error{formatString("Invalid {} accessor sparse values buffer view index.", name)};
			}
			const GlTFBufferView sparseValuesBufferView{glTFAsset, glTFBufferContents, formatString("{} sparse values", name),
				glTFAsset.bufferViews[accessor.sparse->values.bufferView], sparseCount, componentCount, componentSize, accessor.sparse->values.byteOffset};
			switch (componentType) {
				case gltf::Accessor::ComponentType::I8: sparseValuesBufferView.readValuesSparse<int8_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::U8: sparseValuesBufferView.readValuesSparse<uint8_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::I16: sparseValuesBufferView.readValuesSparse<int16_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::U16: sparseValuesBufferView.readValuesSparse<uint16_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::U32: sparseValuesBufferView.readValuesSparse<uint32_t, OutputValue>(output, componentCount, sparseIndices); break;
				case gltf::Accessor::ComponentType::F32: sparseValuesBufferView.readValuesSparse<float, OutputValue>(output, componentCount, sparseIndices); break;
			}
		}
	}

	void traverseRootNodes(auto visitNode) {
		if (!glTFAsset.scenes.empty()) {
			for (const gltf::NodeIndex glTFRootNodeIndex : glTFAsset.scenes[glTFAsset.scene.value_or(0)].nodes) {
				auto visitNodeCopy = visitNode;
				visitNodeCopy(glTFRootNodeIndex);
			}
		}
	}

	void traverseNodeTree(gltf::NodeIndex glTFNodeIndex, auto visitNode) {
		visitNode(glTFNodeIndex);

		for (const gltf::NodeIndex glTFChildNodeIndex : glTFAsset.nodes[glTFNodeIndex].children) {
			traverseNodeTree(glTFChildNodeIndex, visitNode);
		}
	}

	Model& model;
	const gltf::Asset& glTFAsset;
	FunctionView<Model::Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage;
	FunctionView<Variant<Allocation<byte>, Span<const byte>>(CStringView relativeFilepath)> loadBufferData;
	ModelOptions options;
	Buffer<Allocation<byte>> ownedBufferData{};
	Allocation<Span<const byte>> glTFBufferContents{};
	Allocation<TemporaryMeshData> meshesData{};
	Allocation<TemporaryNodeData> nodesData{};
	Allocation<TemporarySkinData> skinsData{};
};

void GlTFModelLoader::validateInputScenes() {
	GREM_PROFILE_FUNCTION();

	if (glTFAsset.scene && *glTFAsset.scene >= glTFAsset.scenes.size()) {
		throw resource::Error{"Scene index out of range."};
	}

	for (gltf::SceneIndex glTFSceneIndex = 0; glTFSceneIndex < glTFAsset.scenes.size(); ++glTFSceneIndex) {
		const gltf::Scene& glTFScene = glTFAsset.scenes[glTFSceneIndex];
		for (size_t rootIndex = 0; rootIndex < glTFScene.nodes.size(); ++rootIndex) {
			const gltf::NodeIndex glTFRootNodeIndex = glTFScene.nodes[rootIndex];
			if (glTFRootNodeIndex >= glTFAsset.nodes.size()) {
				throw resource::Error{formatString("Node index of root {} in scene {} (\"{}\") is out of range.", rootIndex, glTFSceneIndex, glTFScene.name)};
			}
		}
	}
}

void GlTFModelLoader::validateInputSkins() {
	GREM_PROFILE_FUNCTION();

	for (gltf::SkinIndex glTFSkinIndex = 0; glTFSkinIndex < glTFAsset.skins.size(); ++glTFSkinIndex) {
		const gltf::Skin& glTFSkin = glTFAsset.skins[glTFSkinIndex];
		if (glTFSkin.inverseBindMatrices) {
			if (*glTFSkin.inverseBindMatrices >= glTFAsset.accessors.size()) {
				throw resource::Error{formatString("Inverse bind matrix accessor index of skin {} (\"{}\") is out of range.", glTFSkinIndex, glTFSkin.name)};
			}
			if (glTFAsset.accessors[*glTFSkin.inverseBindMatrices].count != glTFSkin.joints.size()) {
				throw resource::Error{formatString("Value count of inverse bind matrix accessor does not match the number of joints.")};
			}
		}

		for (size_t glTFJointIndex = 0; glTFJointIndex < glTFSkin.joints.size(); ++glTFJointIndex) {
			const gltf::NodeIndex glTFNodeIndex = glTFSkin.joints[glTFJointIndex];
			if (glTFNodeIndex >= glTFAsset.nodes.size()) {
				throw resource::Error{formatString("Node index of joint {} in skin {} (\"{}\") is out of range.", glTFJointIndex, glTFSkinIndex, glTFSkin.name)};
			}
		}

		if (glTFSkin.skeleton && *glTFSkin.skeleton >= glTFAsset.nodes.size()) {
			throw resource::Error{formatString("Skeleton index of skin \"{}\" is out of range.", glTFSkin.name)};
		}
	}
}

void GlTFModelLoader::validateInputAnimations() {
	if (options.excludeAnimations) {
		return;
	}

	GREM_PROFILE_FUNCTION();

	for (size_t glTFAnimationIndex = 0; glTFAnimationIndex < glTFAsset.animations.size(); ++glTFAnimationIndex) {
		const gltf::Animation& glTFAnimation = glTFAsset.animations[glTFAnimationIndex];

		for (size_t glTFAnimationChannelIndex = 0; glTFAnimationChannelIndex < glTFAnimation.channels.size(); ++glTFAnimationChannelIndex) {
			const gltf::Animation::Channel& glTFAnimationChannel = glTFAnimation.channels[glTFAnimationChannelIndex];
			if (!glTFAnimationChannel.target.node) {
				continue;
			}

			const gltf::NodeIndex glTFNodeIndex = *glTFAnimationChannel.target.node;
			if (glTFNodeIndex >= glTFAsset.nodes.size()) {
				throw resource::Error{formatString("Target node index of animation channel {} in animation {} (\"{}\") is out of range.", glTFAnimationChannelIndex,
					glTFAnimationIndex, glTFAnimation.name)};
			}
			size_t morphTargetCount = 0;
			if (const gltf::Node& glTFNode = glTFAsset.nodes[glTFNodeIndex]; glTFNode.mesh) {
				if (*glTFNode.mesh >= glTFAsset.meshes.size()) {
					throw resource::Error{formatString("Mesh index of node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
				}
				const gltf::Mesh& glTFMesh = glTFAsset.meshes[*glTFNode.mesh];
				if (!glTFMesh.primitives.empty()) {
					morphTargetCount = glTFMesh.primitives.front().targets.size();
				}
			}

			if (glTFAnimationChannel.sampler >= glTFAnimation.samplers.size()) {
				throw resource::Error{formatString("Sampler index of animation channel {} in animation {} (\"{}\") is out of range.", glTFAnimationChannelIndex, glTFAnimationIndex,
					glTFAnimation.name)};
			}
			const gltf::Animation::Sampler& glTFAnimationSampler = glTFAnimation.samplers[glTFAnimationChannel.sampler];
			if (glTFAnimationSampler.input >= glTFAsset.accessors.size()) {
				throw resource::Error{formatString("Input accessor index of animation channel {} in animation {} (\"{}\") is out of range.", glTFAnimationChannelIndex,
					glTFAnimationIndex, glTFAnimation.name)};
			}
			if (glTFAnimationSampler.output >= glTFAsset.accessors.size()) {
				throw resource::Error{formatString("Output accessor index of animation channel {} in animation {} (\"{}\") is out of range.", glTFAnimationChannelIndex,
					glTFAnimationIndex, glTFAnimation.name)};
			}
			const gltf::Accessor& glTFInputAccessor = glTFAsset.accessors[glTFAnimationSampler.input];
			const gltf::Accessor& glTFOutputAccessor = glTFAsset.accessors[glTFAnimationSampler.output];
			const size_t inputCount = glTFInputAccessor.count;
			const size_t outputCount = glTFOutputAccessor.count;
			size_t minInputCount = 1;
			size_t expectedOutputCount = (glTFAnimationChannel.target.path == gltf::Animation::Channel::Target::Path::WEIGHTS) ? inputCount * morphTargetCount : inputCount;
			if (glTFAnimationSampler.interpolation == gltf::Animation::Sampler::Interpolation::CUBIC_SPLINE) {
				minInputCount = 2;
				expectedOutputCount *= 3;
			}
			if (inputCount < minInputCount) {
				throw resource::Error{formatString("Invalid input count in sampler of animation channel {} in animation {} (\"{}\").", glTFAnimationChannelIndex,
					glTFAnimationIndex, glTFAnimation.name)};
			}
			if (outputCount != expectedOutputCount) {
				throw resource::Error{formatString("Inconsistent input/output count in sampler of animation channel {} in animation {} (\"{}\").", glTFAnimationChannelIndex,
					glTFAnimationIndex, glTFAnimation.name)};
			}
		}
	}
}

void GlTFModelLoader::validateInputNodes() {
	GREM_PROFILE_FUNCTION();

	for (gltf::NodeIndex glTFNodeIndex = 0; glTFNodeIndex < glTFAsset.nodes.size(); ++glTFNodeIndex) {
		const gltf::Node& glTFNode = glTFAsset.nodes[glTFNodeIndex];

		if (glTFNode.camera && *glTFNode.camera >= glTFAsset.cameras.size()) {
			throw resource::Error{formatString("Camera index of node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
		}

		if (glTFNode.mesh && *glTFNode.mesh >= glTFAsset.meshes.size()) {
			throw resource::Error{formatString("Mesh index of node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
		}

		if (glTFNode.skin) {
			if (*glTFNode.skin >= glTFAsset.skins.size()) {
				throw resource::Error{formatString("Skin index of node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
			}
			if (!glTFNode.mesh) {
				throw resource::Error{formatString("Skinned node {} (\"{}\") is missing its mesh.", glTFNodeIndex, glTFNode.name)};
			}
			const gltf::MeshIndex glTFMeshIndex = *glTFNode.mesh;
			const gltf::Mesh& glTFMesh = glTFAsset.meshes[glTFMeshIndex];
			for (size_t glTFMeshPrimitiveIndex = 0; glTFMeshPrimitiveIndex < glTFMesh.primitives.size(); ++glTFMeshPrimitiveIndex) {
				const gltf::Mesh::Primitive& glTFMeshPrimitive = glTFMesh.primitives[glTFMeshPrimitiveIndex];

				if (!glTFMeshPrimitive.attributes.joints0 && !glTFMeshPrimitive.attributes.weights0) {
					throw resource::Error{formatString("Mesh primitive {} in referenced mesh {} (\"{}\") of skinned node {} (\"{}\") is missing its skinning attributes.",
						glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name, glTFNodeIndex, glTFNode.name)};
				}
			}
		}

		for (size_t glTFNodeChildIndex = 0; glTFNodeChildIndex < glTFNode.children.size(); ++glTFNodeChildIndex) {
			const gltf::NodeIndex glTFChildNodeIndex = glTFNode.children[glTFNodeChildIndex];

			if (glTFChildNodeIndex >= glTFAsset.nodes.size()) {
				throw resource::Error{formatString("Node index of child {} in node {} (\"{}\") is out of range.", glTFNodeChildIndex, glTFNodeIndex, glTFNode.name)};
			}
		}

		if (!glTFNode.weights.empty()) {
			if (!glTFNode.mesh) {
				throw resource::Error{formatString("Morphed node {} (\"{}\") is missing its mesh.", glTFNodeIndex, glTFNode.name)};
			}
			const gltf::Mesh& glTFMesh = glTFAsset.meshes[*glTFNode.mesh];
			if (!glTFMesh.primitives.empty() && glTFMesh.primitives.front().targets.size() != glTFNode.weights.size()) {
				throw resource::Error{formatString("Inconsistent morph target count between node {} (\"{}\") and its mesh.", glTFNodeIndex, glTFNode.name)};
			}
		}

		if (!options.excludeLights && glTFNode.extensions.KHR_lights_punctual) {
			if (!glTFAsset.extensions.KHR_lights_punctual || glTFNode.extensions.KHR_lights_punctual->light >= glTFAsset.extensions.KHR_lights_punctual->lights.size()) {
				throw resource::Error{formatString("Light index of node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
			}
		}

		if (!options.excludePhysics && glTFNode.extensions.KHR_physics_rigid_bodies) {
			if (!glTFNode.extensions.KHR_physics_rigid_bodies->motion && !glTFNode.extensions.KHR_physics_rigid_bodies->collider &&
				!glTFNode.extensions.KHR_physics_rigid_bodies->trigger && !glTFNode.extensions.KHR_physics_rigid_bodies->joint) {
				throw resource::Error{formatString("Rigid body of node {} (\"{}\") is invalid.", glTFNodeIndex, glTFNode.name)};
			}

			if (const Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::Collider>& glTFCollider = glTFNode.extensions.KHR_physics_rigid_bodies->collider) {
				if (glTFCollider->geometry.shape.has_value() == glTFCollider->geometry.mesh.has_value()) {
					throw resource::Error{formatString("Rigid body collider geometry of node {} (\"{}\") is invalid.", glTFNodeIndex, glTFNode.name)};
				}

				if (glTFCollider->geometry.shape) {
					if (!glTFAsset.extensions.KHR_implicit_shapes || *glTFCollider->geometry.shape >= glTFAsset.extensions.KHR_implicit_shapes->shapes.size()) {
						throw resource::Error{formatString("Shape index of rigid body collider geometry in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
					}
				} else if (glTFCollider->geometry.mesh && *glTFCollider->geometry.mesh >= glTFAsset.meshes.size()) {
					throw resource::Error{formatString("Mesh index of rigid body collider geometry in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
				}

				if (glTFCollider->physicsMaterial) {
					if (!glTFAsset.extensions.KHR_physics_rigid_bodies ||
						*glTFCollider->physicsMaterial >= glTFAsset.extensions.KHR_physics_rigid_bodies->physicsMaterials.size()) {
						throw resource::Error{formatString("Physics material index of rigid body collider in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
					}
				}

				if (glTFCollider->collisionFilter) {
					if (!glTFAsset.extensions.KHR_physics_rigid_bodies ||
						*glTFCollider->collisionFilter >= glTFAsset.extensions.KHR_physics_rigid_bodies->collisionFilters.size()) {
						throw resource::Error{formatString("Collision filter index of rigid body collider in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
					}
				}
			}

			if (const Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::Trigger>& glTFTrigger = glTFNode.extensions.KHR_physics_rigid_bodies->trigger) {
				if (glTFTrigger->geometry.has_value() == !glTFTrigger->nodes.empty()) {
					throw resource::Error{formatString("Rigid body trigger of node {} (\"{}\") is invalid.", glTFNodeIndex, glTFNode.name)};
				}

				if (glTFTrigger->geometry) {
					if (glTFTrigger->geometry->shape.has_value() == glTFTrigger->geometry->mesh.has_value()) {
						throw resource::Error{formatString("Rigid body trigger geometry of node {} (\"{}\") is invalid.", glTFNodeIndex, glTFNode.name)};
					}

					if (glTFTrigger->geometry->shape) {
						if (!glTFAsset.extensions.KHR_implicit_shapes || *glTFTrigger->geometry->shape >= glTFAsset.extensions.KHR_implicit_shapes->shapes.size()) {
							throw resource::Error{formatString("Shape index of rigid body trigger geometry in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
						}
					} else if (glTFTrigger->geometry->mesh && *glTFTrigger->geometry->mesh >= glTFAsset.meshes.size()) {
						throw resource::Error{formatString("Mesh index of rigid body trigger geometry in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
					}
				} else {
					for (size_t i = 0; i < glTFTrigger->nodes.size(); ++i) {
						if (glTFTrigger->nodes[i] >= glTFAsset.nodes.size()) {
							throw resource::Error{formatString("Node index {} of rigid body trigger in node {} (\"{}\") is out of range.", i, glTFNodeIndex, glTFNode.name)};
						}
					}
				}

				if (glTFTrigger->collisionFilter) {
					if (!glTFAsset.extensions.KHR_physics_rigid_bodies || *glTFTrigger->collisionFilter >= glTFAsset.extensions.KHR_physics_rigid_bodies->collisionFilters.size()) {
						throw resource::Error{formatString("Collision filter index of rigid body trigger in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
					}
				}
			}

			if (const Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::Joint>& glTFJoint = glTFNode.extensions.KHR_physics_rigid_bodies->joint) {
				if (glTFJoint->connectedNode >= glTFAsset.nodes.size()) {
					throw resource::Error{formatString("Connected node index of rigid body joint in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
				}
				if (!glTFAsset.extensions.KHR_physics_rigid_bodies || glTFJoint->joint >= glTFAsset.extensions.KHR_physics_rigid_bodies->physicsJoints.size()) {
					throw resource::Error{formatString("Joint index of rigid body joint in node {} (\"{}\") is out of range.", glTFNodeIndex, glTFNode.name)};
				}
			}
		}
	}

	Allocation<bool> nodesVisited(glTFAsset.nodes.size(), false);
	traverseRootNodes([&](gltf::NodeIndex glTFRootNodeIndex) -> void {
		traverseNodeTree(glTFRootNodeIndex, [&](gltf::NodeIndex glTFNodeIndex) -> void {
			if (nodesVisited[glTFNodeIndex]) {
				throw resource::Error{"Cycle detected in node hierarchy."};
			}
			nodesVisited[glTFNodeIndex] = true;
		});
	});
}

void GlTFModelLoader::validateInputMeshes() {
	GREM_PROFILE_FUNCTION();

	for (gltf::MeshIndex glTFMeshIndex = 0; glTFMeshIndex < glTFAsset.meshes.size(); ++glTFMeshIndex) {
		const gltf::Mesh& glTFMesh = glTFAsset.meshes[glTFMeshIndex];

		size_t morphTargetCount = glTFMesh.weights.size();

		for (size_t glTFMeshPrimitiveIndex = 0; glTFMeshPrimitiveIndex < glTFMesh.primitives.size(); ++glTFMeshPrimitiveIndex) {
			const gltf::Mesh::Primitive& glTFMeshPrimitive = glTFMesh.primitives[glTFMeshPrimitiveIndex];

			if (glTFMeshPrimitive.material && *glTFMeshPrimitive.material >= glTFAsset.materials.size()) {
				throw resource::Error{
					formatString("Material index of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
			}

			if (glTFMeshPrimitive.indices) {
				if (*glTFMeshPrimitive.indices >= glTFAsset.accessors.size()) {
					throw resource::Error{
						formatString("Accessor index for indices of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
				}
			}

			size_t vertexCount = 0;
			if (glTFMeshPrimitive.attributes.position) {
				if (*glTFMeshPrimitive.attributes.position >= glTFAsset.accessors.size()) {
					throw resource::Error{formatString("Accessor index for POSITION attribute of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
				const gltf::Accessor& glTFPositionAccessor = glTFAsset.accessors[*glTFMeshPrimitive.attributes.position];
				if (!glTFPositionAccessor.min || !glTFPositionAccessor.max) {
					throw resource::Error{formatString("Accessor for POSITION attribute of mesh primitive {} in mesh {} (\"{}\") is missing its min/max properties.",
						glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
				}
				vertexCount = glTFPositionAccessor.count;
				if (vertexCount == 0) {
					throw resource::Error{
						formatString("Accessor for POSITION attribute of mesh primitive {} in mesh {} (\"{}\") is empty.", glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
				}
			}

			if (glTFMeshPrimitive.attributes.normal) {
				if (*glTFMeshPrimitive.attributes.normal >= glTFAsset.accessors.size()) {
					throw resource::Error{formatString("Accessor index for NORMAL attribute of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
				if (glTFAsset.accessors[*glTFMeshPrimitive.attributes.normal].count != vertexCount) {
					throw resource::Error{formatString("Inconsistent vertex count between the attributes of mesh primitive {} in mesh {} (\"{}\").", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
			}

			if (glTFMeshPrimitive.attributes.tangent) {
				if (*glTFMeshPrimitive.attributes.tangent >= glTFAsset.accessors.size()) {
					throw resource::Error{formatString("Accessor index for TANGENT attribute of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
				if (glTFAsset.accessors[*glTFMeshPrimitive.attributes.tangent].count != vertexCount) {
					throw resource::Error{formatString("Inconsistent vertex count between the attributes of mesh primitive {} in mesh {} (\"{}\").", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
			}

			if (glTFMeshPrimitive.attributes.texcoord0) {
				if (*glTFMeshPrimitive.attributes.texcoord0 >= glTFAsset.accessors.size()) {
					throw resource::Error{formatString("Accessor index for TEXCOORD_0 attribute of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
				if (glTFAsset.accessors[*glTFMeshPrimitive.attributes.texcoord0].count != vertexCount) {
					throw resource::Error{formatString("Inconsistent vertex count between the attributes of mesh primitive {} in mesh {} (\"{}\").", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
			}

			if (glTFMeshPrimitive.attributes.texcoord1) {
				if (*glTFMeshPrimitive.attributes.texcoord1 >= glTFAsset.accessors.size()) {
					throw resource::Error{formatString("Accessor index for TEXCOORD_1 attribute of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
				if (glTFAsset.accessors[*glTFMeshPrimitive.attributes.texcoord1].count != vertexCount) {
					throw resource::Error{formatString("Inconsistent vertex count between the attributes of mesh primitive {} in mesh {} (\"{}\").", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
			}

			if (glTFMeshPrimitive.attributes.color0) {
				if (*glTFMeshPrimitive.attributes.color0 >= glTFAsset.accessors.size()) {
					throw resource::Error{formatString("Accessor index for COLOR_0 attribute of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
				if (glTFAsset.accessors[*glTFMeshPrimitive.attributes.color0].count != vertexCount) {
					throw resource::Error{formatString("Inconsistent vertex count between the attributes of mesh primitive {} in mesh {} (\"{}\").", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
			}

			if (glTFMeshPrimitive.attributes.joints0) {
				if (*glTFMeshPrimitive.attributes.joints0 >= glTFAsset.accessors.size()) {
					throw resource::Error{formatString("Accessor index for JOINTS_0 attribute of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
				if (glTFAsset.accessors[*glTFMeshPrimitive.attributes.joints0].count != vertexCount) {
					throw resource::Error{formatString("Inconsistent vertex count between the attributes of mesh primitive {} in mesh {} (\"{}\").", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
			}

			if (glTFMeshPrimitive.attributes.weights0) {
				if (*glTFMeshPrimitive.attributes.weights0 >= glTFAsset.accessors.size()) {
					throw resource::Error{formatString("Accessor index for WEIGHTS_0 attribute of mesh primitive {} in mesh {} (\"{}\") is out of range.", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
				if (glTFAsset.accessors[*glTFMeshPrimitive.attributes.weights0].count != vertexCount) {
					throw resource::Error{formatString("Inconsistent vertex count between the attributes of mesh primitive {} in mesh {} (\"{}\").", glTFMeshPrimitiveIndex,
						glTFMeshIndex, glTFMesh.name)};
				}
			}

			if (morphTargetCount == 0) {
				morphTargetCount = glTFMeshPrimitive.targets.size();
			} else if (morphTargetCount != glTFMeshPrimitive.targets.size()) {
				throw resource::Error{formatString("Inconsistent morph target count between mesh {} (\"{}\") and its primitives.", glTFMeshIndex, glTFMesh.name)};
			}

			for (size_t glTFMorphTargetIndex = 0; glTFMorphTargetIndex < glTFMeshPrimitive.targets.size(); ++glTFMorphTargetIndex) {
				const gltf::Mesh::Primitive::Attributes& glTFMorphTargetAttributes = glTFMeshPrimitive.targets[glTFMorphTargetIndex];

				if (glTFMorphTargetAttributes.position) {
					if (vertexCount == 0) {
						throw resource::Error{
							formatString("Morph target {} in mesh primitive {} in mesh {} (\"{}\") has a POSITION attribute, which the original mesh primitive does not.",
								glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (*glTFMorphTargetAttributes.position >= glTFAsset.accessors.size()) {
						throw resource::Error{formatString("Accessor index for POSITION attribute of morph target {} in mesh primitive {} in mesh {} (\"{}\") is out of range.",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					const gltf::Accessor& glTFPositionOffsetsAccessor = glTFAsset.accessors[*glTFMorphTargetAttributes.position];
					if (!glTFPositionOffsetsAccessor.min || !glTFPositionOffsetsAccessor.max) {
						throw resource::Error{
							formatString("Accessor for POSITION attribute of morph target {} in mesh primitive {} in mesh {} (\"{}\") is missing its min/max properties.",
								glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (glTFPositionOffsetsAccessor.count != vertexCount) {
						throw resource::Error{formatString("Inconsistent vertex count between the attributes of morph target {} in mesh primitive {} in mesh {} (\"{}\").",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
				}

				if (glTFMorphTargetAttributes.normal) {
					if (vertexCount == 0) {
						throw resource::Error{
							formatString("Morph target {} in mesh primitive {} in mesh {} (\"{}\") has a NORMAL attribute, which the original mesh primitive does not.",
								glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (*glTFMorphTargetAttributes.normal >= glTFAsset.accessors.size()) {
						throw resource::Error{formatString("Accessor index for NORMAL attribute of morph target {} in mesh primitive {} in mesh {} (\"{}\") is out of range.",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (glTFAsset.accessors[*glTFMorphTargetAttributes.normal].count != vertexCount) {
						throw resource::Error{formatString("Inconsistent vertex count between the attributes of morph target {} in mesh primitive {} in mesh {} (\"{}\").",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
				}

				if (glTFMorphTargetAttributes.tangent) {
					if (vertexCount == 0) {
						throw resource::Error{
							formatString("Morph target {} in mesh primitive {} in mesh {} (\"{}\") has a TANGENT attribute, which the original mesh primitive does not.",
								glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (*glTFMorphTargetAttributes.tangent >= glTFAsset.accessors.size()) {
						throw resource::Error{formatString("Accessor index for TANGENT attribute of morph target {} in mesh primitive {} in mesh {} (\"{}\") is out of range.",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (glTFAsset.accessors[*glTFMorphTargetAttributes.tangent].count != vertexCount) {
						throw resource::Error{formatString("Inconsistent vertex count between the attributes of morph target {} in mesh primitive {} in mesh {} (\"{}\").",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
				}

				if (glTFMorphTargetAttributes.texcoord0) {
					if (!glTFMeshPrimitive.attributes.texcoord0) {
						throw resource::Error{
							formatString("Morph target {} in mesh primitive {} in mesh {} (\"{}\") has a TEXCOORD_0 attribute, which the original mesh primitive does not.",
								glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (*glTFMorphTargetAttributes.texcoord0 >= glTFAsset.accessors.size()) {
						throw resource::Error{formatString("Accessor index for TEXCOORD_0 attribute of morph target {} in mesh primitive {} in mesh {} (\"{}\") is out of range.",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (glTFAsset.accessors[*glTFMorphTargetAttributes.texcoord0].count != vertexCount) {
						throw resource::Error{formatString("Inconsistent vertex count between the attributes of morph target {} in mesh primitive {} in mesh {} (\"{}\").",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
				}

				if (glTFMorphTargetAttributes.texcoord1) {
					if (!glTFMeshPrimitive.attributes.texcoord1) {
						throw resource::Error{
							formatString("Morph target {} in mesh primitive {} in mesh {} (\"{}\") has a TEXCOORD_1 attribute, which the original mesh primitive does not.",
								glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (*glTFMorphTargetAttributes.texcoord1 >= glTFAsset.accessors.size()) {
						throw resource::Error{formatString("Accessor index for TEXCOORD_1 attribute of morph target {} in mesh primitive {} in mesh {} (\"{}\") is out of range.",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (glTFAsset.accessors[*glTFMorphTargetAttributes.texcoord1].count != vertexCount) {
						throw resource::Error{formatString("Inconsistent vertex count between the attributes of morph target {} in mesh primitive {} in mesh {} (\"{}\").",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
				}

				if (glTFMorphTargetAttributes.color0) {
					if (!glTFMeshPrimitive.attributes.color0) {
						throw resource::Error{
							formatString("Morph target {} in mesh primitive {} in mesh {} (\"{}\") has a COLOR_0 attribute, which the original mesh primitive does not.",
								glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (*glTFMorphTargetAttributes.color0 >= glTFAsset.accessors.size()) {
						throw resource::Error{formatString("Accessor index for COLOR_0 attribute of morph target {} in mesh primitive {} in mesh {} (\"{}\") is out of range.",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
					if (glTFAsset.accessors[*glTFMorphTargetAttributes.color0].count != vertexCount) {
						throw resource::Error{formatString("Inconsistent vertex count between the attributes of morph target {} in mesh primitive {} in mesh {} (\"{}\").",
							glTFMorphTargetIndex, glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name)};
					}
				}
			}
		}
	}
}

void GlTFModelLoader::validateInputTextures() {
	GREM_PROFILE_FUNCTION();

	for (gltf::TextureIndex glTFTextureIndex = 0; glTFTextureIndex < glTFAsset.textures.size(); ++glTFTextureIndex) {
		const gltf::Texture& glTFTexture = glTFAsset.textures[glTFTextureIndex];

		if (glTFTexture.extensions.KHR_texture_basisu) {
			if (glTFTexture.extensions.KHR_texture_basisu->source >= glTFAsset.images.size()) {
				throw resource::Error{formatString("Source image index in KHR_texture_basisu of texture {} is out of range.", glTFTextureIndex)};
			}
		}

		if (glTFTexture.source) {
			if (*glTFTexture.source >= glTFAsset.images.size()) {
				throw resource::Error{formatString("Source image index of texture {} (\"{}\") is out of range.", glTFTextureIndex, glTFTexture.name)};
			}
			const gltf::Image& glTFImage = glTFAsset.images[*glTFTexture.source];
			if (glTFImage.uri) {
				if (glTFImage.uri->is<gltf::BinChunkData>()) {
					throw resource::Error{formatString("Invalid path for source image of texture {} (\"{}\").", glTFTextureIndex, glTFTexture.name)};
				}
				if (glTFImage.bufferView) {
					throw resource::Error{formatString("Ambiguous source image of texture {} (\"{}\").", glTFTextureIndex, glTFTexture.name)};
				}
			} else {
				if (!glTFImage.bufferView) {
					throw resource::Error{formatString("Missing path for source image of texture {} (\"{}\").", glTFTextureIndex, glTFTexture.name)};
				}
				if (*glTFImage.bufferView >= glTFAsset.bufferViews.size()) {
					throw resource::Error{formatString("Source image buffer view index of texture {} (\"{}\") is out of range.", glTFTextureIndex, glTFTexture.name)};
				}
			}
		}

		if (glTFTexture.sampler && *glTFTexture.sampler >= glTFAsset.samplers.size()) {
			throw resource::Error{formatString("Sampler index of texture {} (\"{}\") is out of range.", glTFTextureIndex, glTFTexture.name)};
		}
	}
}

void GlTFModelLoader::validateInputMaterials() {
	GREM_PROFILE_FUNCTION();

	for (gltf::MaterialIndex glTFMaterialIndex = 0; glTFMaterialIndex < glTFAsset.materials.size(); ++glTFMaterialIndex) {
		const gltf::Material& glTFMaterial = glTFAsset.materials[glTFMaterialIndex];

		if (glTFMaterial.pbrMetallicRoughness.baseColorTexture && glTFMaterial.pbrMetallicRoughness.baseColorTexture->index >= glTFAsset.textures.size()) {
			throw resource::Error{formatString("Base color texture index of material {} (\"{}\") is out of range.", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture && glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture->index >= glTFAsset.textures.size()) {
			throw resource::Error{formatString("Metallic-roughness texture index of material {} (\"{}\") is out of range.", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (glTFMaterial.occlusionTexture && glTFMaterial.occlusionTexture->index >= glTFAsset.textures.size()) {
			throw resource::Error{formatString("Occlusion texture index of material {} (\"{}\") is out of range.", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (glTFMaterial.normalTexture && glTFMaterial.normalTexture->index >= glTFAsset.textures.size()) {
			throw resource::Error{formatString("Normal texture index of material {} (\"{}\") is out of range.", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (glTFMaterial.emissiveTexture && glTFMaterial.emissiveTexture->index >= glTFAsset.textures.size()) {
			throw resource::Error{formatString("Emissive texture index of material {} (\"{}\") is out of range.", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (any(lessThan(glTFMaterial.emissiveFactor, vec3{0.0f}) | greaterThan(glTFMaterial.emissiveFactor, vec3{1.0f}))) {
			throw resource::Error{formatString("Emissive factor of material {} (\"{}\") is out of range.", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (glTFMaterial.alphaCutoff < 0.0f) {
			throw resource::Error{formatString("Alpha cutoff of material {} (\"{}\") is out of range.", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (glTFMaterial.extensions.KHR_materials_emissive_strength && glTFMaterial.extensions.KHR_materials_unlit) {
			throw resource::Error{formatString("Conflicting extensions in material {} (\"{}\").", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (glTFMaterial.extensions.KHR_materials_ior && (glTFMaterial.extensions.KHR_materials_ior->ior < 1.0f && glTFMaterial.extensions.KHR_materials_ior->ior != 0.0f)) {
			throw resource::Error{formatString("Index of refraction of material {} (\"{}\") is invalid.", glTFMaterialIndex, glTFMaterial.name)};
		}

		if (glTFMaterial.extensions.KHR_materials_unlit && glTFMaterial.extensions.KHR_materials_ior) {
			throw resource::Error{formatString("Conflicting extensions in material {} (\"{}\").", glTFMaterialIndex, glTFMaterial.name)};
		}
	}
}

void GlTFModelLoader::validateInputLights() {
	if (options.excludeLights || !glTFAsset.extensions.KHR_lights_punctual) {
		return;
	}

	GREM_PROFILE_FUNCTION();

	for (size_t glTFLightIndex = 0; glTFLightIndex < glTFAsset.extensions.KHR_lights_punctual->lights.size(); ++glTFLightIndex) {
		const gltf::Asset::Extension::KHRLightsPunctual::Light& glTFLight = glTFAsset.extensions.KHR_lights_punctual->lights[glTFLightIndex];
		if (glTFLight.range) {
			if (glTFLight.type != gltf::Asset::Extension::KHRLightsPunctual::Light::Type::POINT && glTFLight.type != gltf::Asset::Extension::KHRLightsPunctual::Light::Type::SPOT) {
				throw resource::Error{formatString("Range specified for incompatible light type in light {}.", glTFLightIndex)};
			}
			if (*glTFLight.range <= 0.0f) {
				throw resource::Error{formatString("Invalid range in light {}.", glTFLightIndex)};
			}
		}
		if (glTFLight.spot) {
			if (glTFLight.type != gltf::Asset::Extension::KHRLightsPunctual::Light::Type::SPOT) {
				throw resource::Error{formatString("Spot parameters specified for incompatible light type in light {}.", glTFLightIndex)};
			}
			if (glTFLight.spot->innerConeAngle < 0.0f) {
				throw resource::Error{formatString("Invalid inner cone angle in light {}.", glTFLightIndex)};
			}
			if (glTFLight.spot->outerConeAngle > numbers::PI / 2.0f + 0.001f) {
				throw resource::Error{formatString("Invalid outer cone angle in light {}.", glTFLightIndex)};
			}
			// Note: This test should actually be >= according to the spec,
			// but apparently Blender exports lights with equal cone angles sometimes,
			// so we make it slightly less strict for convenience.
			if (glTFLight.spot->innerConeAngle > glTFLight.spot->outerConeAngle) {
				throw resource::Error{formatString("Inconsistent cone angles in light {}.", glTFLightIndex)};
			}
		} else {
			if (glTFLight.type == gltf::Asset::Extension::KHRLightsPunctual::Light::Type::SPOT) {
				throw resource::Error{formatString("Missing spot parameters for light {}.", glTFLightIndex)};
			}
		}
	}
}

void GlTFModelLoader::validateInputCollisionShapes() {
	if ((options.excludeColliders && options.excludePhysics) || !glTFAsset.extensions.KHR_implicit_shapes) {
		return;
	}

	GREM_PROFILE_FUNCTION();

	for (size_t glTFCollisionShapeIndex = 0; glTFCollisionShapeIndex < glTFAsset.extensions.KHR_implicit_shapes->shapes.size(); ++glTFCollisionShapeIndex) {
		const gltf::Asset::Extension::KHRImplicitShapes::Shape& glTFShape = glTFAsset.extensions.KHR_implicit_shapes->shapes[glTFCollisionShapeIndex];
		GREM_MATCH(glTFShape) {
			GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Plane& glTFPlane) {
				if (glTFPlane.sizeX && *glTFPlane.sizeX <= 0.0f) {
					throw resource::Error{formatString("Plane sizeX of collision shape {} is invalid.", glTFCollisionShapeIndex)};
				}
				if (glTFPlane.sizeZ && *glTFPlane.sizeZ <= 0.0f) {
					throw resource::Error{formatString("Plane sizeZ of collision shape {} is invalid.", glTFCollisionShapeIndex)};
				}
				break;
			}
			GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Sphere& glTFSphere) {
				if (glTFSphere.radius <= 0.0f) {
					throw resource::Error{formatString("Sphere radius of collision shape {} is invalid.", glTFCollisionShapeIndex)};
				}
				break;
			}
			GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Box& glTFBox) {
				if (glTFBox.size.x <= 0.0f || glTFBox.size.y <= 0.0f || glTFBox.size.z <= 0.0f) {
					throw resource::Error{formatString("Box size of collision shape {} is invalid.", glTFCollisionShapeIndex)};
				}
				break;
			}
			GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Cylinder& glTFCylinder) {
				if (glTFCylinder.height <= 0.0f) {
					throw resource::Error{formatString("Cylinder height of collision shape {} is invalid.", glTFCollisionShapeIndex)};
				}
				if (glTFCylinder.radiusBottom < 0.0f || glTFCylinder.radiusTop < 0.0f || (glTFCylinder.radiusBottom <= 0.0f && glTFCylinder.radiusTop <= 0.0f)) {
					throw resource::Error{formatString("Cylinder radii of collision shape {} are invalid.", glTFCollisionShapeIndex)};
				}
				break;
			}
			GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Capsule& glTFCapsule) {
				if (glTFCapsule.height <= 0.0f) {
					throw resource::Error{formatString("Capsule height of collision shape {} is invalid.", glTFCollisionShapeIndex)};
				}
				if (glTFCapsule.radiusBottom < 0.0f || glTFCapsule.radiusTop < 0.0f || (glTFCapsule.radiusBottom <= 0.0f && glTFCapsule.radiusTop <= 0.0f)) {
					throw resource::Error{formatString("Capsule radii of collision shape {} are invalid.", glTFCollisionShapeIndex)};
				}
				break;
			}
		}
	}
}

void GlTFModelLoader::validateInputPhysicsJoints() {
	if (options.excludePhysics || !glTFAsset.extensions.KHR_physics_rigid_bodies) {
		return;
	}

	GREM_PROFILE_FUNCTION();

	for (size_t glTFPhysicsJointIndex = 0; glTFPhysicsJointIndex < glTFAsset.extensions.KHR_physics_rigid_bodies->physicsJoints.size(); ++glTFPhysicsJointIndex) {
		const gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint& glTFJoint = glTFAsset.extensions.KHR_physics_rigid_bodies->physicsJoints[glTFPhysicsJointIndex];
		for (size_t glTFDriveIndex = 0; glTFDriveIndex < glTFJoint.drives.size(); ++glTFDriveIndex) {
			switch (glTFJoint.drives[glTFDriveIndex].axis) {
				case 0: [[fallthrough]];
				case 1: [[fallthrough]];
				case 2: break;
				default: throw resource::Error{formatString("Axis of drive {} in physics joint {} is invalid.", glTFDriveIndex, glTFPhysicsJointIndex)};
			}
		}
	}
}

void GlTFModelLoader::initializeBufferContents() {
	GREM_PROFILE_FUNCTION();

	glTFBufferContents.resize(glTFAsset.buffers.size());
	for (gltf::BufferIndex glTFBufferIndex = 0; glTFBufferIndex < glTFAsset.buffers.size(); ++glTFBufferIndex) {
		GREM_MATCH(glTFAsset.buffers[glTFBufferIndex].uri) {
			GREM_CASE(const gltf::BinChunkData& binChunkData) {
				glTFBufferContents[glTFBufferIndex] = glTFAsset.binChunk;
				break;
			}
			GREM_CASE(const gltf::InlineData& inlineData) {
				glTFBufferContents[glTFBufferIndex] = asBytes(Span{inlineData.data});
				break;
			}
			GREM_CASE(const gltf::RelativePath& relativePath) {
				GREM_MATCH(loadBufferData(relativePath.path)) {
					GREM_CASE(Allocation<byte> & bufferData) {
						ownedBufferData.push_back(std::move(bufferData));
						glTFBufferContents[glTFBufferIndex] = ownedBufferData.back();
						break;
					}
					GREM_CASE(Span<const byte> bufferData) {
						glTFBufferContents[glTFBufferIndex] = bufferData;
						break;
					}
				}
				break;
			}
		}
	}
}

void GlTFModelLoader::initializeTemporaryData() {
	GREM_PROFILE_FUNCTION();

	meshesData.resize(glTFAsset.meshes.size());
	nodesData.resize(glTFAsset.nodes.size());
	skinsData.resize(glTFAsset.skins.size());
	for (TemporarySkinData& skinData : skinsData) {
		skinData.skinnedNodesData.resize(glTFAsset.nodes.size());
	}
}

void GlTFModelLoader::markJoints() {
	GREM_PROFILE_FUNCTION();

	for (const gltf::Skin& glTFSkin : glTFAsset.skins) {
		for (const gltf::NodeIndex glTFNodeIndex : glTFSkin.joints) {
			nodesData[glTFNodeIndex].isDynamicJoint = true;
		}
	}

	if (!options.excludeAnimations) {
		for (const gltf::Animation& glTFAnimation : glTFAsset.animations) {
			for (const gltf::Animation::Channel& glTFAnimationChannel : glTFAnimation.channels) {
				if (!glTFAnimationChannel.target.node) {
					continue;
				}

				nodesData[*glTFAnimationChannel.target.node].isDynamicJoint = true;
			}
		}
	}

	const auto nodeHasLight = [&](const gltf::Node& glTFNode) -> bool {
		return !options.excludeLights && glTFNode.extensions.KHR_lights_punctual;
	};

	const auto nodeHasPhysicsMotion = [&](const gltf::Node& glTFNode) -> bool {
		return !options.excludePhysics && glTFNode.extensions.KHR_physics_rigid_bodies && glTFNode.extensions.KHR_physics_rigid_bodies->motion;
	};

	const auto nodeHasIdentityTransformation = [&](const gltf::Node& glTFNode) -> bool {
		return glTFNode.rotation == quat{0.0f, 0.0f, 0.0f, 1.0f} && glTFNode.scale == vec3{1.0f, 1.0f, 1.0f} && glTFNode.translation == vec3{0.0f, 0.0f, 0.0f};
	};

	traverseRootNodes([&](gltf::NodeIndex glTFRootNodeIndex) -> void {
		traverseNodeTree(glTFRootNodeIndex, [&, hasDynamicParent = false, hasVisibilityTogglableParent = false](gltf::NodeIndex glTFNodeIndex) mutable -> void {
			const gltf::Node& glTFNode = glTFAsset.nodes[glTFNodeIndex];
			TemporaryNodeData& nodeData = nodesData[glTFNodeIndex];

			if (hasDynamicParent) {
				if (!nodeData.isDynamicJoint && (nodeHasLight(glTFNode) || nodeHasPhysicsMotion(glTFNode) || !nodeHasIdentityTransformation(glTFNode))) {
					nodeData.isDynamicJoint = true;
				}
			} else if (nodeData.isDynamicJoint) {
				hasDynamicParent = true;
			} else if ((nodeHasLight(glTFNode) || nodeHasPhysicsMotion(glTFNode)) && glTFNodeIndex != glTFRootNodeIndex) {
				nodeData.isDynamicJoint = true;
				hasDynamicParent = true;
			} else if (hasVisibilityTogglableParent) {
				nodeData.isStaticJoint = true;
			} else if (glTFNode.extensions.KHR_node_visibility) {
				hasVisibilityTogglableParent = true;
				nodeData.isStaticJoint = true;
			} else if (!nodeHasIdentityTransformation(glTFNode)) {
				nodeData.isStaticJoint = true;
			}
		});
	});
}

void GlTFModelLoader::allocateTextures() {
	GREM_PROFILE_FUNCTION();

	const size_t totalTextureCount = glTFAsset.textures.size();
	if (totalTextureCount > static_cast<size_t>(Limits<Model::TextureCount>::MAX)) {
		throw std::length_error{"Too many textures in asset."};
	}
	model.textures.resize(totalTextureCount);
}

void GlTFModelLoader::allocateMaterials() {
	GREM_PROFILE_FUNCTION();

	const size_t totalMaterialCount = glTFAsset.materials.size();
	if (totalMaterialCount > static_cast<size_t>(Limits<Model::MaterialCount>::MAX)) {
		throw std::length_error{"Too many materials in asset."};
	}
	model.materials.resize(totalMaterialCount);
}

void GlTFModelLoader::allocateMeshes() {
	GREM_PROFILE_FUNCTION();

	size_t totalMeshCount = 0;
	for (const gltf::Mesh& glTFMesh : glTFAsset.meshes) {
		for (const gltf::Mesh::Primitive& glTFMeshPrimitive : glTFMesh.primitives) {
			if (!glTFMeshPrimitive.attributes.position) {
				continue;
			}

			++totalMeshCount;
		}
	}

	if (totalMeshCount > static_cast<size_t>(Limits<Model::MeshCount>::MAX)) {
		throw std::length_error{"Too many mesh primitives in asset."};
	}
	model.meshes.resize(totalMeshCount);

	size_t meshIndex = 0;
	size_t totalMeshDataSize = 0;
	size_t totalMorphTargetDataSize = 0;
	size_t totalMorphTargetCount = 0;
	for (const gltf::Mesh& glTFMesh : glTFAsset.meshes) {
		for (const gltf::Mesh::Primitive& glTFMeshPrimitive : glTFMesh.primitives) {
			if (!glTFMeshPrimitive.attributes.position) {
				continue;
			}

			Model::Mesh& mesh = model.meshes[meshIndex];
			mesh.vertexFlags = {};

			const size_t indexCount = (glTFMeshPrimitive.indices) ? glTFAsset.accessors[*glTFMeshPrimitive.indices].count : 0;
			const size_t vertexCount = glTFAsset.accessors[*glTFMeshPrimitive.attributes.position].count;
			mesh.indexCount = static_cast<Model::IndexCount>(indexCount);
			mesh.vertexCount = static_cast<Model::VertexCount>(vertexCount);

			totalMeshDataSize += indexCount * sizeof(uint32_t);
			totalMeshDataSize += vertexCount * (sizeof(vec3) + sizeof(iA2B10G10R10vec4norm) + sizeof(iA2B10G10R10vec4norm));
			if (glTFMeshPrimitive.attributes.texcoord0) {
				totalMeshDataSize += vertexCount * sizeof(vec2);
				mesh.vertexFlags |= Model::VERTEX_TEXTURED_ON_CHANNEL_0;
			}
			if (glTFMeshPrimitive.attributes.texcoord1) {
				totalMeshDataSize += vertexCount * sizeof(vec2);
				mesh.vertexFlags |= Model::VERTEX_TEXTURED_ON_CHANNEL_1;
			}
			if (glTFMeshPrimitive.attributes.color0) {
				totalMeshDataSize += vertexCount * sizeof(u8vec4norm);
				mesh.vertexFlags |= Model::VERTEX_COLORED;
			}
			if (glTFMeshPrimitive.attributes.joints0 || glTFMeshPrimitive.attributes.weights0) {
				totalMeshDataSize += vertexCount * (sizeof(u8vec4) + sizeof(u8vec4norm));
				mesh.vertexFlags |= Model::VERTEX_SKINNED;
			}

			bool hasMorphedPosition = false;
			bool hasMorphedNormal = false;
			bool hasMorphedTangent = false;
			bool hasMorphedTextureCoordinatesChannel0 = false;
			bool hasMorphedTextureCoordinatesChannel1 = false;
			bool hasMorphedColor = false;
			for (const gltf::Mesh::Primitive::Attributes& glTFMorphTargetAttributes : glTFMeshPrimitive.targets) {
				hasMorphedPosition = hasMorphedPosition || glTFMorphTargetAttributes.position;
				hasMorphedNormal = hasMorphedNormal || glTFMorphTargetAttributes.normal;
				hasMorphedTangent = hasMorphedTangent || glTFMorphTargetAttributes.tangent;
				hasMorphedTextureCoordinatesChannel0 = hasMorphedTextureCoordinatesChannel0 || glTFMorphTargetAttributes.texcoord0;
				hasMorphedTextureCoordinatesChannel1 = hasMorphedTextureCoordinatesChannel1 || glTFMorphTargetAttributes.texcoord1;
				hasMorphedColor = hasMorphedColor || glTFMorphTargetAttributes.color0;
			}

			mesh.morphedVertexStride = 0;
			if (hasMorphedPosition) {
				mesh.morphedVertexStride += sizeof(vec3) / 4;
				mesh.vertexFlags |= Model::VERTEX_MORPHED_POSITION;
			}
			if (hasMorphedPosition || hasMorphedNormal) {
				mesh.morphedVertexStride += (sizeof(vec3) + sizeof(vec3)) / 4;
				mesh.vertexFlags |= Model::VERTEX_MORPHED_NORMAL;
				mesh.vertexFlags |= Model::VERTEX_MORPHED_TANGENT;
			} else if (hasMorphedTangent) {
				mesh.morphedVertexStride += sizeof(vec3) / 4;
				mesh.vertexFlags |= Model::VERTEX_MORPHED_TANGENT;
			}
			if (hasMorphedTextureCoordinatesChannel0) {
				mesh.morphedVertexStride += sizeof(vec2) / 4;
				mesh.vertexFlags |= Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0;
			}
			if (hasMorphedTextureCoordinatesChannel1) {
				mesh.morphedVertexStride += sizeof(vec2) / 4;
				mesh.vertexFlags |= Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1;
			}
			if (hasMorphedColor) {
				mesh.morphedVertexStride += sizeof(vec4) / 4;
				mesh.vertexFlags |= Model::VERTEX_MORPHED_COLOR;
			}

			const size_t morphTargetCount = glTFMeshPrimitive.targets.size();
			const size_t morphedVertexCount = morphTargetCount * vertexCount;
			totalMorphTargetDataSize += morphedVertexCount * static_cast<size_t>(mesh.morphedVertexStride) * 4;
			totalMorphTargetCount += morphTargetCount;
			++meshIndex;
		}
	}

	GREM_ASSERT(meshIndex == totalMeshCount);
	GREM_ASSERT(totalMeshDataSize % 4 == 0);
	GREM_ASSERT(totalMorphTargetDataSize % 4 == 0);

	if (totalMeshDataSize / 4 > static_cast<size_t>(Limits<Model::ValueOffset>::MAX)) {
		throw std::length_error{"Too much mesh data in asset."};
	}
	if (totalMorphTargetDataSize / 4 > static_cast<size_t>(Limits<Model::ValueOffset>::MAX)) {
		throw std::length_error{"Too much morph target data in asset."};
	}
	if (totalMorphTargetCount > static_cast<size_t>(Limits<Model::MorphTargetCount>::MAX)) {
		throw std::length_error{"Too many morph targets in asset."};
	}

	model.meshData.resize(totalMeshDataSize);
	model.morphTargetData.resize(totalMorphTargetDataSize);
}

void GlTFModelLoader::allocateNodes() {
	GREM_PROFILE_FUNCTION();

	model.staticJointCount = 1;

	size_t totalStaticJointCount = 1; // The origin joint is always at index 0.
	size_t totalDynamicJointCount = 0;
	size_t totalMorphTargetWeightCount = 0;
	size_t totalInstanceCount = 0;
	size_t totalLightCount = 0;
	size_t totalPhysicsObjectCount = 0;
	size_t totalPhysicsJointCount = 0;

	traverseRootNodes([&](gltf::NodeIndex glTFRootNodeIndex) -> void {
		traverseNodeTree(glTFRootNodeIndex, [&](gltf::NodeIndex glTFNodeIndex) -> void {
			const gltf::Node& glTFNode = glTFAsset.nodes[glTFNodeIndex];
			const TemporaryNodeData& nodeData = nodesData[glTFNodeIndex];

			if (nodeData.isDynamicJoint) {
				++totalDynamicJointCount;
			} else if (nodeData.isStaticJoint) {
				++totalStaticJointCount;
			}

			if (glTFNode.mesh) {
				const gltf::Mesh& glTFMesh = glTFAsset.meshes[*glTFNode.mesh];

				const size_t morphTargetCount = (glTFMesh.primitives.empty()) ? 0 : glTFMesh.primitives.front().targets.size();
				totalMorphTargetWeightCount += morphTargetCount;

				for (const gltf::Mesh::Primitive& glTFMeshPrimitive : glTFMesh.primitives) {
					if (!glTFMeshPrimitive.attributes.position) {
						continue;
					}

					++totalInstanceCount;
				}
			}

			if (!options.excludeLights && glTFNode.extensions.KHR_lights_punctual) {
				++totalLightCount;
			}

			if (!options.excludePhysics && glTFNode.extensions.KHR_physics_rigid_bodies) {
				if (glTFNode.extensions.KHR_physics_rigid_bodies->motion) {
					++totalPhysicsObjectCount;
				}
				if (glTFNode.extensions.KHR_physics_rigid_bodies->joint) {
					++totalPhysicsJointCount;
				}
			}
		});
	});

	const size_t totalJointCount = totalStaticJointCount + totalDynamicJointCount;
	const size_t totalInverseBindPoseMatrixCount = totalJointCount * glTFAsset.skins.size();
	const size_t totalSkinDataSize = totalInverseBindPoseMatrixCount * sizeof(mat4);
	GREM_ASSERT(totalSkinDataSize % 4 == 0);

	if (totalSkinDataSize / 4 > static_cast<size_t>(Limits<Model::ValueOffset>::MAX)) {
		throw std::length_error{"Too many inverse bind matrices in asset."};
	}
	if (totalJointCount > static_cast<size_t>(Limits<Model::JointCount>::MAX)) {
		throw std::length_error{"Too many joints in asset."};
	}
	if (totalMorphTargetWeightCount > static_cast<size_t>(Limits<Model::MorphTargetWeightCount>::MAX)) {
		throw std::length_error{"Too many morph target weights in asset."};
	}
	if (totalInstanceCount > static_cast<size_t>(Limits<Model::InstanceCount>::MAX)) {
		throw std::length_error{"Too many instances in asset."};
	}
	if (totalLightCount > static_cast<size_t>(Limits<Model::LightCount>::MAX)) {
		throw std::length_error{"Too many lights in asset."};
	}
	if (totalPhysicsObjectCount > static_cast<size_t>(Limits<Model::PhysicsObjectCount>::MAX)) {
		throw std::length_error{"Too many physics objects in asset."};
	}
	if (totalPhysicsJointCount > static_cast<size_t>(Limits<Model::PhysicsJointCount>::MAX)) {
		throw std::length_error{"Too many physics joints in asset."};
	}

	model.skinData.resize(totalSkinDataSize);
	model.bindPose.localJoints.reserve(totalJointCount);
	model.bindPose.localJoints.resize(totalJointCount);
	model.bindPose.localMorphTargetWeights.reserve(totalMorphTargetWeightCount);
	model.bindPose.localMorphTargetWeights.resize(totalMorphTargetWeightCount);
	model.jointParentIndices.resize(totalJointCount);
	model.jointColliders.resize(totalJointCount, nullopt);
	model.jointPhysicsObjectIndices.resize(totalJointCount, Limits<Model::PhysicsObjectIndex>::MAX);
	model.staticJointCount = static_cast<Model::JointCount>(totalStaticJointCount);
	model.instances.resize(totalInstanceCount);
	model.lights.resize(totalLightCount);
	model.physicsObjects.resize(totalPhysicsObjectCount);
	model.physicsJoints.resize(totalPhysicsJointCount);
}

void GlTFModelLoader::allocateAnimations() {
	if (options.excludeAnimations) {
		return;
	}

	GREM_PROFILE_FUNCTION();

	size_t totalAnimationCount = 0;
	size_t totalAnimationChannelCount = 0;
	size_t totalKeyframeTimePointCount = 0;
	size_t totalKeyframeOutputValueDataSize = 0;

	for (const gltf::Animation& glTFAnimation : glTFAsset.animations) {
		const size_t animationChannelOffset = totalAnimationChannelCount;

		for (size_t glTFAnimationChannelIndex = 0; glTFAnimationChannelIndex < glTFAnimation.channels.size(); ++glTFAnimationChannelIndex) {
			const gltf::Animation::Channel& glTFAnimationChannel = glTFAnimation.channels[glTFAnimationChannelIndex];
			if (!glTFAnimationChannel.target.node) {
				continue;
			}

			const gltf::Animation::Sampler& glTFAnimationSampler = glTFAnimation.samplers[glTFAnimationChannel.sampler];
			const gltf::Accessor& glTFInputAccessor = glTFAsset.accessors[glTFAnimationSampler.input];
			const gltf::Accessor& glTFOutputAccessor = glTFAsset.accessors[glTFAnimationSampler.output];
			const size_t inputCount = glTFInputAccessor.count;
			const size_t outputCount = glTFOutputAccessor.count;

			totalKeyframeTimePointCount += inputCount;
			switch (glTFAnimationChannel.target.path) {
				case gltf::Animation::Channel::Target::Path::TRANSLATION: totalKeyframeOutputValueDataSize += outputCount * sizeof(vec3); break;
				case gltf::Animation::Channel::Target::Path::ROTATION: totalKeyframeOutputValueDataSize += outputCount * sizeof(quat); break;
				case gltf::Animation::Channel::Target::Path::SCALE: totalKeyframeOutputValueDataSize += outputCount * sizeof(vec3); break;
				case gltf::Animation::Channel::Target::Path::WEIGHTS: totalKeyframeOutputValueDataSize += outputCount * sizeof(float); break;
			}

			++totalAnimationChannelCount;
		}

		const size_t animationChannelCount = totalAnimationChannelCount - animationChannelOffset;

		if (animationChannelCount > 0) {
			++totalAnimationCount;
		}
	}

	GREM_ASSERT(totalKeyframeOutputValueDataSize % 4 == 0);

	if (totalAnimationCount > static_cast<size_t>(Limits<Model::AnimationCount>::MAX)) {
		throw std::length_error{"Too many animations in asset."};
	}
	if (totalAnimationChannelCount > static_cast<size_t>(Limits<Model::AnimationChannelCount>::MAX)) {
		throw std::length_error{"Too many animation channels in asset."};
	}
	if (totalKeyframeTimePointCount > static_cast<size_t>(Limits<Model::ValueOffset>::MAX)) {
		throw std::length_error{"Too many keyframe time points in asset."};
	}
	if (totalKeyframeOutputValueDataSize / 4 > static_cast<size_t>(Limits<Model::ValueOffset>::MAX)) {
		throw std::length_error{"Too many keyframe output values in asset."};
	}

	model.animations.resize(totalAnimationCount);
	model.animationChannels.resize(totalAnimationChannelCount);
	model.keyframeInputTimePoints.resize(totalKeyframeTimePointCount);
	model.keyframeOutputValueData.resize(totalKeyframeOutputValueDataSize);
}

void GlTFModelLoader::loadSkins() {
	GREM_PROFILE_FUNCTION();

	constexpr size_t SKIN_STRIDE_BYTES = sizeof(mat4);

	const Model::JointCount totalJointCount = static_cast<Model::JointCount>(model.bindPose.localJoints.size());
	if (totalJointCount == 0) {
		return;
	}

	size_t skinDataByteOffset = 0;

	for (gltf::SkinIndex glTFSkinIndex = 0; glTFSkinIndex < glTFAsset.skins.size(); ++glTFSkinIndex) {
		const gltf::Skin& glTFSkin = glTFAsset.skins[glTFSkinIndex];
		TemporarySkinData& skinData = skinsData[glTFSkinIndex];

		if (glTFSkin.inverseBindMatrices) {
			Allocation<mat4> inverseBindPoseMatrices(glTFSkin.joints.size());
			readAccessorValues<mat4>(asWritableBytes(Span{inverseBindPoseMatrices}), "inverseBindMatrices", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::MAT4},
				glTFAsset.accessors[*glTFSkin.inverseBindMatrices]);
			for (size_t glTFJointIndex = 0; glTFJointIndex < glTFSkin.joints.size(); ++glTFJointIndex) {
				const gltf::NodeIndex glTFNodeIndex = glTFSkin.joints[glTFJointIndex];
				const mat4& inverseBindPoseMatrix = inverseBindPoseMatrices[glTFJointIndex];
				skinData.skinnedNodesData[glTFNodeIndex].inverseBindPoseMatrix = inverseBindPoseMatrix;
			}
		}

		GREM_ASSERT(skinDataByteOffset % 4 == 0);
		skinData.skinDataOffset = static_cast<Model::ValueOffset>(skinDataByteOffset / 4);

		StridedSpan<byte> inverseBindPoseMatrixData{model.skinData.data() + skinDataByteOffset, totalJointCount, SKIN_STRIDE_BYTES};
		fillWithValues<mat4>(inverseBindPoseMatrixData, mat4{1.0f});
		StridedSpan<mat4> inverseBindPoseMatrices{std::launder(reinterpret_cast<mat4*>(inverseBindPoseMatrixData.base())), totalJointCount, SKIN_STRIDE_BYTES};

		Model::JointIndex dynamicJointIndex = model.staticJointCount;
		traverseRootNodes([&, &skinData = skinData](gltf::NodeIndex glTFRootNodeIndex) -> void {
			traverseNodeTree(glTFRootNodeIndex, [&, globalTransformation = mat4{1.0f}](gltf::NodeIndex glTFNodeIndex) mutable -> void {
				const gltf::Node& glTFNode = glTFAsset.nodes[glTFNodeIndex];
				const TemporaryNodeData& nodeData = nodesData[glTFNodeIndex];

				globalTransformation = globalTransformation * translateRotateScale(glTFNode.translation, glTFNode.rotation, glTFNode.scale);

				if (nodeData.isDynamicJoint) {
					const TemporarySkinData::TemporarySkinnedNodeData& skinnedNodeData = skinData.skinnedNodesData[glTFNodeIndex];
					inverseBindPoseMatrices[dynamicJointIndex] = (skinnedNodeData.inverseBindPoseMatrix) ? *skinnedNodeData.inverseBindPoseMatrix : inverse(globalTransformation);
					++dynamicJointIndex;
				}
			});
		});

		skinDataByteOffset += totalJointCount * SKIN_STRIDE_BYTES;
	}
}

void GlTFModelLoader::loadTextures() {
	GREM_PROFILE_FUNCTION();

	const auto loadGlTFImage = [&](const gltf::Image& glTFImage) -> Model::Image {
		if (glTFImage.uri) {
			GREM_MATCH(*glTFImage.uri) {
				GREM_CASE(const gltf::BinChunkData& binChunkData) {
					unreachable();
				}
				GREM_CASE(const gltf::InlineData& inlineData) {
					return Image{asBytes(Span{inlineData.data}), ImageOptions{.requiredType = ImageType::IMAGE_2D}};
				}
				GREM_CASE(const gltf::RelativePath& relativePath) {
					return loadImage(relativePath.path, ImageOptions{.requiredType = ImageType::IMAGE_2D});
				}
			}
		}
		const gltf::BufferView& glTFBufferView = glTFAsset.bufferViews[*glTFImage.bufferView];
		const GlTFBufferView bufferView{glTFAsset, glTFBufferContents, "source", glTFBufferView};
		Allocation<byte> imageFileContents{};
		return Image{bufferView.readTemporaryBytes(imageFileContents, bufferView.getByteLength()), ImageOptions{.requiredType = ImageType::IMAGE_2D}};
	};

	for (gltf::TextureIndex glTFTextureIndex = 0; glTFTextureIndex < glTFAsset.textures.size(); ++glTFTextureIndex) {
		const gltf::Texture& glTFTexture = glTFAsset.textures[glTFTextureIndex];

		Model::Texture texture{
			.image{},
			.minificationFilter = Model::MinificationFilter::LINEAR_MIPMAP_LINEAR,
			.magnificationFilter = Model::MagnificationFilter::LINEAR,
			.horizontalWrappingMode = Model::WrappingMode::REPEAT,
			.verticalWrappingMode = Model::WrappingMode::REPEAT,
		};
		if (glTFTexture.extensions.KHR_texture_basisu) {
			texture.image = loadGlTFImage(glTFAsset.images[glTFTexture.extensions.KHR_texture_basisu->source]);
		} else if (glTFTexture.source) {
			texture.image = loadGlTFImage(glTFAsset.images[*glTFTexture.source]);
		}
		if (glTFTexture.sampler) {
			const gltf::Sampler& glTFSampler = glTFAsset.samplers[*glTFTexture.sampler];
			texture.magnificationFilter = static_cast<Model::MagnificationFilter>(glTFSampler.magFilter);
			texture.minificationFilter = static_cast<Model::MinificationFilter>(glTFSampler.minFilter);
			texture.horizontalWrappingMode = static_cast<Model::WrappingMode>(glTFSampler.wrapS);
			texture.verticalWrappingMode = static_cast<Model::WrappingMode>(glTFSampler.wrapT);
		}

		if (!glTFTexture.name.empty()) {
			model.textureMap[glTFTexture.name] = static_cast<Model::TextureIndex>(glTFTextureIndex);
		}
		model.textures[glTFTextureIndex] = std::move(texture);
	}
}

void GlTFModelLoader::loadMaterials() {
	GREM_PROFILE_FUNCTION();

	const auto calculateTextureTransformation = [](vec2 uvTranslation, float uvRotation, vec2 uvScale) -> mat3 {
		return translateScale(vec2{0.0f, 1.0f}, vec2{1.0f, -1.0f}) * translateRotateScale(uvTranslation, -uvRotation, uvScale);
	};

	const mat3 defaultTextureTransformation = calculateTextureTransformation(vec2{0.0f, 0.0f}, 0.0f, vec2{1.0f, 1.0f});
	const Model::Material::TextureInfo defaultTextureInfo{
		.textureIndex = static_cast<Model::TextureIndex>(model.textures.size()),
		.textureOffset = vec2{defaultTextureTransformation[2]},
		.textureBasis = mat2{defaultTextureTransformation},
	};

	const auto loadGLTFTextureInfo = [&](Model::Material::TextureInfo& textureInfo, const gltf::TextureInfo& glTFTextureInfo) -> size_t {
		textureInfo.textureIndex = glTFTextureInfo.index;
		size_t textureCoordinatesChannel = glTFTextureInfo.textureCoordinatesChannel;
		if (glTFTextureInfo.extensions.KHR_texture_transform) {
			const mat3 textureTransformation = calculateTextureTransformation(glTFTextureInfo.extensions.KHR_texture_transform->offset,
				glTFTextureInfo.extensions.KHR_texture_transform->rotation, glTFTextureInfo.extensions.KHR_texture_transform->scale);
			textureInfo.textureOffset = vec2{textureTransformation[2]};
			textureInfo.textureBasis = mat2{textureTransformation};
			if (glTFTextureInfo.extensions.KHR_texture_transform->textureCoordinatesChannel) {
				textureCoordinatesChannel = *glTFTextureInfo.extensions.KHR_texture_transform->textureCoordinatesChannel;
			}
		}
		return textureCoordinatesChannel;
	};

	for (gltf::MaterialIndex glTFMaterialIndex = 0; glTFMaterialIndex < glTFAsset.materials.size(); ++glTFMaterialIndex) {
		const gltf::Material& glTFMaterial = glTFAsset.materials[glTFMaterialIndex];

		Model::Material material{
			.materialType = (glTFMaterial.extensions.KHR_materials_unlit) ? Model::MaterialType::UNLIT : Model::MaterialType::METALLIC_ROUGHNESS,
			.baseColorFactor = glTFMaterial.pbrMetallicRoughness.baseColorFactor,
			.occlusionStrength = (glTFMaterial.occlusionTexture) ? glTFMaterial.occlusionTexture->strength : 1.0f,
			.roughnessFactor = glTFMaterial.pbrMetallicRoughness.roughnessFactor,
			.metallicFactor = glTFMaterial.pbrMetallicRoughness.metallicFactor,
			.normalScale = (glTFMaterial.normalTexture) ? glTFMaterial.normalTexture->scale : 1.0f,
			.emissiveFactor = (glTFMaterial.extensions.KHR_materials_emissive_strength)
		                          ? glTFMaterial.emissiveFactor * glTFMaterial.extensions.KHR_materials_emissive_strength->emissiveStrength
		                          : glTFMaterial.emissiveFactor,
			.alphaCutoff = glTFMaterial.alphaCutoff,
			.indexOfRefraction = (glTFMaterial.extensions.KHR_materials_ior) ? glTFMaterial.extensions.KHR_materials_ior->ior : 1.5f,
			.baseColorMap = defaultTextureInfo,
			.occlusionRoughnessMetallicMap = defaultTextureInfo,
			.normalMap = defaultTextureInfo,
			.emissiveMap = defaultTextureInfo,
			.fragmentFlags{},
		};

		if (glTFMaterial.pbrMetallicRoughness.baseColorTexture) {
			const size_t textureCoordinatesChannel = loadGLTFTextureInfo(material.baseColorMap, *glTFMaterial.pbrMetallicRoughness.baseColorTexture);
			if (textureCoordinatesChannel == 0) {
				material.fragmentFlags |= Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0;
			} else if (textureCoordinatesChannel == 1) {
				material.fragmentFlags |= Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1;
			}
		}

		if (glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture) {
			const size_t textureCoordinatesChannel = loadGLTFTextureInfo(material.occlusionRoughnessMetallicMap, *glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture);
			if (textureCoordinatesChannel == 0) {
				material.fragmentFlags |= Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0;
			} else if (textureCoordinatesChannel == 1) {
				material.fragmentFlags |= Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1;
			}
		}

		if (glTFMaterial.occlusionTexture && (!glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture ||
												 glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture->index == glTFMaterial.occlusionTexture->index)) {
			size_t textureCoordinatesChannel{};
			if (glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture) {
				vec2 textureOffset = defaultTextureInfo.textureOffset;
				mat2 textureBasis = defaultTextureInfo.textureBasis;
				if (glTFMaterial.occlusionTexture->extensions.KHR_texture_transform) {
					const mat3 textureTransformation = calculateTextureTransformation(glTFMaterial.occlusionTexture->extensions.KHR_texture_transform->offset,
						glTFMaterial.occlusionTexture->extensions.KHR_texture_transform->rotation, glTFMaterial.occlusionTexture->extensions.KHR_texture_transform->scale);
					textureOffset = vec2{textureTransformation[2]};
					textureBasis = mat2{textureTransformation};
				}
				if (textureOffset != material.occlusionRoughnessMetallicMap.textureOffset || textureBasis != material.occlusionRoughnessMetallicMap.textureBasis) {
					throw resource::Error{"The occlusion and occlusion-roughness-metallic textures have incompatible texture transformations."};
				}

				textureCoordinatesChannel =
					(glTFMaterial.occlusionTexture->extensions.KHR_texture_transform && glTFMaterial.occlusionTexture->extensions.KHR_texture_transform->textureCoordinatesChannel)
						? *glTFMaterial.occlusionTexture->extensions.KHR_texture_transform->textureCoordinatesChannel
						: glTFMaterial.occlusionTexture->textureCoordinatesChannel;
				const size_t metallicRoughnessTextureCoordinatesChannel =
					(glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture->extensions.KHR_texture_transform &&
						glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture->extensions.KHR_texture_transform->textureCoordinatesChannel)
						? *glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture->extensions.KHR_texture_transform->textureCoordinatesChannel
						: glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture->textureCoordinatesChannel;
				if (textureCoordinatesChannel != metallicRoughnessTextureCoordinatesChannel) {
					throw resource::Error{"The occlusion and occlusion-roughness-metallic textures have incompatible texture coordinate channels."};
				}
			} else {
				textureCoordinatesChannel = loadGLTFTextureInfo(material.occlusionRoughnessMetallicMap, *glTFMaterial.occlusionTexture);
			}
			if (textureCoordinatesChannel == 0) {
				material.fragmentFlags |= Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0;
			} else if (textureCoordinatesChannel == 1) {
				material.fragmentFlags |= Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1;
			}
		}

		if (glTFMaterial.normalTexture) {
			const size_t textureCoordinatesChannel = loadGLTFTextureInfo(material.normalMap, *glTFMaterial.normalTexture);
			if (textureCoordinatesChannel == 0) {
				material.fragmentFlags |= Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0;
			} else if (textureCoordinatesChannel == 1) {
				material.fragmentFlags |= Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1;
			}
		}

		if (glTFMaterial.emissiveTexture) {
			const size_t textureCoordinatesChannel = loadGLTFTextureInfo(material.emissiveMap, *glTFMaterial.emissiveTexture);
			if (textureCoordinatesChannel == 0) {
				material.fragmentFlags |= Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0;
			} else if (textureCoordinatesChannel == 1) {
				material.fragmentFlags |= Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1;
			}
		}

		switch (glTFMaterial.alphaMode) {
			case gltf::Material::AlphaMode::ALPHA_OPAQUE: break;
			case gltf::Material::AlphaMode::ALPHA_MASK: material.fragmentFlags |= Model::FRAGMENT_ALPHA_MASKED; break;
			case gltf::Material::AlphaMode::ALPHA_BLEND: material.fragmentFlags |= Model::FRAGMENT_ALPHA_BLENDED; break;
		}

		if (glTFMaterial.doubleSided) {
			material.fragmentFlags |= Model::FRAGMENT_DOUBLE_SIDED;
		}

		if (!glTFMaterial.name.empty()) {
			model.materialMap[glTFMaterial.name] = static_cast<Model::MaterialIndex>(glTFMaterialIndex);
		}
		model.materials[glTFMaterialIndex] = material;
	}
}

void GlTFModelLoader::loadMeshes() {
	GREM_PROFILE_FUNCTION();

	Model::MeshIndex meshIndex = 0;
	size_t meshDataByteOffset = 0;
	size_t morphTargetDataByteOffset = 0;

	for (gltf::MeshIndex glTFMeshIndex = 0; glTFMeshIndex < glTFAsset.meshes.size(); ++glTFMeshIndex) {
		const gltf::Mesh& glTFMesh = glTFAsset.meshes[glTFMeshIndex];
		TemporaryMeshData& meshData = meshesData[glTFMeshIndex];

		meshData.meshOffset = meshIndex;

		for (const gltf::Mesh::Primitive& glTFMeshPrimitive : glTFMesh.primitives) {
			if (!glTFMeshPrimitive.attributes.position) {
				continue;
			}

			Model::Mesh& mesh = model.meshes[meshIndex];

			const gltf::Accessor& glTFPositionAccessor = glTFAsset.accessors[*glTFMeshPrimitive.attributes.position];
			const size_t indexCount = (glTFMeshPrimitive.indices) ? glTFAsset.accessors[*glTFMeshPrimitive.indices].count : 0;
			GREM_ASSERT(static_cast<Model::IndexCount>(indexCount) == mesh.indexCount);
			const size_t vertexCount = glTFPositionAccessor.count;
			GREM_ASSERT(static_cast<Model::VertexCount>(vertexCount) == mesh.vertexCount);

			switch (glTFMeshPrimitive.mode) {
				case gltf::Mesh::Primitive::Mode::POINTS: mesh.primitiveType = Model::PrimitiveType::POINTS; break;
				case gltf::Mesh::Primitive::Mode::LINES: mesh.primitiveType = Model::PrimitiveType::LINES; break;
				case gltf::Mesh::Primitive::Mode::LINE_LOOP: throw resource::Error{"Line loop primitive mode is not supported."};
				case gltf::Mesh::Primitive::Mode::LINE_STRIP: mesh.primitiveType = Model::PrimitiveType::LINE_STRIP; break;
				case gltf::Mesh::Primitive::Mode::TRIANGLES: mesh.primitiveType = Model::PrimitiveType::TRIANGLES; break;
				case gltf::Mesh::Primitive::Mode::TRIANGLE_STRIP: mesh.primitiveType = Model::PrimitiveType::TRIANGLE_STRIP; break;
				case gltf::Mesh::Primitive::Mode::TRIANGLE_FAN: throw resource::Error{"Triangle fan primitive mode is not supported."};
			}

			GREM_ASSERT(meshDataByteOffset % 4 == 0);
			mesh.meshDataOffset = static_cast<Model::ValueOffset>(meshDataByteOffset / 4);

			Span<byte> indexData{};
			if (glTFMeshPrimitive.indices) {
				indexData = Span{model.meshData}.subspan(meshDataByteOffset, indexCount * sizeof(uint32_t));
				meshDataByteOffset += indexData.size_bytes();
				readAccessorValues<uint32_t>(indexData, "indices", {gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::U16, gltf::Accessor::ComponentType::U32},
					{gltf::Accessor::Type::SCALAR}, glTFAsset.accessors[*glTFMeshPrimitive.indices]);
				for (const uint32_t index : Span{std::launder(reinterpret_cast<const uint32_t*>(indexData.data())), indexCount}) {
					if (index >= static_cast<uint32_t>(vertexCount)) {
						throw resource::Error{"Mesh primitive index is out of range of its vertex buffer."};
					}
				}
			}

			const Span<byte> positionData = Span{model.meshData}.subspan(meshDataByteOffset, vertexCount * sizeof(vec3));
			meshDataByteOffset += positionData.size_bytes();
			readAccessorValues<vec3>(positionData, "POSITION", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC3}, glTFPositionAccessor);
			mesh.boundingBox = {
				.min{(*glTFPositionAccessor.min)[0], (*glTFPositionAccessor.min)[1], (*glTFPositionAccessor.min)[2]},
				.max{(*glTFPositionAccessor.max)[0], (*glTFPositionAccessor.max)[1], (*glTFPositionAccessor.max)[2]},
			};
			const Span<const vec3> positions{std::launder(reinterpret_cast<const vec3*>(positionData.data())), vertexCount};
			float boundingRadiusSquared = length2(positions.front());
			for (const vec3 position : positions.subspan(1)) {
				boundingRadiusSquared = max(boundingRadiusSquared, length2(position));
			}
			mesh.boundingRadius = sqrt(boundingRadiusSquared);

			const Span<byte> normalData = Span{model.meshData}.subspan(meshDataByteOffset, vertexCount * sizeof(iA2B10G10R10vec4norm));
			meshDataByteOffset += normalData.size_bytes();

			const Span<byte> tangentData = Span{model.meshData}.subspan(meshDataByteOffset, vertexCount * sizeof(iA2B10G10R10vec4norm));
			meshDataByteOffset += tangentData.size_bytes();

			Span<byte> textureCoordinatesChannel0Data{};
			if ((mesh.vertexFlags & Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0) {
				textureCoordinatesChannel0Data = Span{model.meshData}.subspan(meshDataByteOffset, vertexCount * sizeof(vec2));
				meshDataByteOffset += textureCoordinatesChannel0Data.size_bytes();
				readAccessorValues<vec2>(textureCoordinatesChannel0Data, "TEXCOORD_0",
					{gltf::Accessor::ComponentType::F32, gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::U16}, {gltf::Accessor::Type::VEC2},
					glTFAsset.accessors[*glTFMeshPrimitive.attributes.texcoord0]);
			}

			Span<byte> textureCoordinatesChannel1Data{};
			if ((mesh.vertexFlags & Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0) {
				textureCoordinatesChannel1Data = Span{model.meshData}.subspan(meshDataByteOffset, vertexCount * sizeof(vec2));
				meshDataByteOffset += textureCoordinatesChannel1Data.size_bytes();
				readAccessorValues<vec2>(textureCoordinatesChannel1Data, "TEXCOORD_1",
					{gltf::Accessor::ComponentType::F32, gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::U16}, {gltf::Accessor::Type::VEC2},
					glTFAsset.accessors[*glTFMeshPrimitive.attributes.texcoord1]);
			}

			Span<byte> colorData{};
			if ((mesh.vertexFlags & Model::VERTEX_COLORED) != 0) {
				colorData = Span{model.meshData}.subspan(meshDataByteOffset, vertexCount * sizeof(u8vec4norm));
				meshDataByteOffset += colorData.size_bytes();
				const gltf::Accessor& glTFColorAccessor = glTFAsset.accessors[*glTFMeshPrimitive.attributes.color0];
				if (glTFColorAccessor.type == gltf::Accessor::Type::VEC3) {
					readAccessorValues<u8vec4norm>(StridedSpan{colorData.data(), vertexCount, sizeof(u8vec4norm)}, "COLOR_0",
						{gltf::Accessor::ComponentType::F32, gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::U16}, {gltf::Accessor::Type::VEC3},
						glTFColorAccessor);
					constexpr u8norm ALPHA_ONE = 1.0f;
					for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
						memcpy(colorData.data() + vertexIndex * sizeof(u8vec4norm) + sizeof(u8norm) * 3, &ALPHA_ONE, sizeof(u8norm));
					}
				} else {
					readAccessorValues<u8vec4norm>(colorData, "COLOR_0",
						{gltf::Accessor::ComponentType::F32, gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::U16}, {gltf::Accessor::Type::VEC4},
						glTFColorAccessor);
				}
			}

			const gltf::Material* const glTFMaterial = (glTFMeshPrimitive.material) ? &glTFAsset.materials[*glTFMeshPrimitive.material] : nullptr;
			const size_t normalTextureCoordinatesChannel = (glTFMaterial && glTFMaterial->normalTexture) ? glTFMaterial->normalTexture->textureCoordinatesChannel : 0;
			const Span<byte> normalTextureCoordinatesData = (normalTextureCoordinatesChannel == 1) ? textureCoordinatesChannel1Data : textureCoordinatesChannel0Data;
			const bool hasNormalTextureCoordinates = (normalTextureCoordinatesChannel == 1) ? ((mesh.vertexFlags & Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0)
			                                                                                : ((mesh.vertexFlags & Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0);
			const bool hasMorphedNormalTextureCoordinates =
				(normalTextureCoordinatesChannel == 1)
					? ((mesh.vertexFlags & Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1) != 0)
					: ((mesh.vertexFlags & Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0) != 0);

			constexpr iA2B10G10R10vec4norm DEFAULT_NORMAL{1.0f, 0.0f, 0.0f, 0.0f};
			constexpr iA2B10G10R10vec4norm DEFAULT_TANGENT{0.0f, 1.0f, 0.0f, 1.0f};
			if (glTFMeshPrimitive.attributes.normal) {
				readAccessorValues<iA2B10G10R10vec4norm>(normalData, "NORMAL", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC3},
					glTFAsset.accessors[*glTFMeshPrimitive.attributes.normal]);
				if (glTFMeshPrimitive.attributes.tangent) {
					readAccessorValues<iA2B10G10R10vec4norm>(tangentData, "TANGENT", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC4},
						glTFAsset.accessors[*glTFMeshPrimitive.attributes.tangent]);
				} else if (glTFMeshPrimitive.mode == gltf::Mesh::Primitive::Mode::TRIANGLES) {
					const Span<const uint32_t> indices =
						(indexCount == 0) ? Span<const uint32_t>{} : Span{std::launder(reinterpret_cast<const uint32_t*>(indexData.data())), indexCount};
					const Span<const iA2B10G10R10vec4norm> normals{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(normalData.data())), vertexCount};
					const Span<const vec2> normalTextureCoordinates =
						(hasNormalTextureCoordinates) ? Span{std::launder(reinterpret_cast<const vec2*>(normalTextureCoordinatesData.data())), vertexCount} : Span<const vec2>{};
					Model::generateTriangleTangents(tangentData, positions, normals, normalTextureCoordinates, indices);
				} else {
					fillWithValues<iA2B10G10R10vec4norm>(tangentData, DEFAULT_TANGENT);
				}
			} else if (glTFMeshPrimitive.mode == gltf::Mesh::Primitive::Mode::TRIANGLES) {
				const Span<const uint32_t> indices =
					(indexCount == 0) ? Span<const uint32_t>{} : Span{std::launder(reinterpret_cast<const uint32_t*>(indexData.data())), indexCount};
				Model::generateTriangleNormals(normalData, positions, indices);
				const Span<const iA2B10G10R10vec4norm> normals{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(normalData.data())), vertexCount};
				const Span<const vec2> normalTextureCoordinates =
					(hasNormalTextureCoordinates) ? Span{std::launder(reinterpret_cast<const vec2*>(normalTextureCoordinatesData.data())), vertexCount} : Span<const vec2>{};
				Model::generateTriangleTangents(tangentData, positions, normals, normalTextureCoordinates, indices);
			} else {
				fillWithValues<iA2B10G10R10vec4norm>(normalData, DEFAULT_NORMAL);
				fillWithValues<iA2B10G10R10vec4norm>(tangentData, DEFAULT_TANGENT);
			}

			constexpr u8vec4norm DEFAULT_JOINT_WEIGHTS{1.0f, 0.0f, 0.0f, 0.0f};
			Span<byte> jointIndicesData{};
			Span<byte> jointWeightsData{};
			if ((mesh.vertexFlags & Model::VERTEX_SKINNED) != 0) {
				jointIndicesData = Span{model.meshData}.subspan(meshDataByteOffset, vertexCount * sizeof(u8vec4));
				meshDataByteOffset += jointIndicesData.size_bytes();
				jointWeightsData = Span{model.meshData}.subspan(meshDataByteOffset, vertexCount * sizeof(u8vec4norm));
				meshDataByteOffset += jointWeightsData.size_bytes();
				if (glTFMeshPrimitive.attributes.joints0) {
					readAccessorValues<u8vec4>(jointIndicesData, "JOINTS_0", {gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::U16}, {gltf::Accessor::Type::VEC4},
						glTFAsset.accessors[*glTFMeshPrimitive.attributes.joints0]);
				} else {
					fillWithZeroedValues<u8vec4>(jointIndicesData);
				}
				if (glTFMeshPrimitive.attributes.weights0) {
					readAccessorValues<u8vec4norm>(jointWeightsData, "WEIGHTS_0",
						{gltf::Accessor::ComponentType::F32, gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::U16}, {gltf::Accessor::Type::VEC4},
						glTFAsset.accessors[*glTFMeshPrimitive.attributes.weights0]);
				} else {
					fillWithValues<u8vec4norm>(jointWeightsData, DEFAULT_JOINT_WEIGHTS);
				}
			}

			GREM_ASSERT(morphTargetDataByteOffset % 4 == 0);
			mesh.morphTargetDataOffset = static_cast<Model::ValueOffset>(morphTargetDataByteOffset / 4);
			mesh.morphTargetCount = static_cast<Model::MorphTargetCount>(glTFMeshPrimitive.targets.size());

			const size_t morphedPositionAttributeOffset = 0;
			const size_t morphedNormalAttributeOffset =
				morphedPositionAttributeOffset + static_cast<size_t>((mesh.vertexFlags & Model::VERTEX_MORPHED_POSITION) != 0) * sizeof(vec3);
			const size_t morphedTangentAttributeOffset = morphedNormalAttributeOffset + static_cast<size_t>((mesh.vertexFlags & Model::VERTEX_MORPHED_NORMAL) != 0) * sizeof(vec3);
			const size_t morphedTextureCoordinatesChannel0AttributeOffset =
				morphedTangentAttributeOffset + static_cast<size_t>((mesh.vertexFlags & Model::VERTEX_MORPHED_TANGENT) != 0) * sizeof(vec3);
			const size_t morphedTextureCoordinatesChannel1AttributeOffset =
				morphedTextureCoordinatesChannel0AttributeOffset +
				static_cast<size_t>((mesh.vertexFlags & Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0) != 0) * sizeof(vec2);
			const size_t morphedColorAttributeOffset = morphedTextureCoordinatesChannel1AttributeOffset +
			                                           static_cast<size_t>((mesh.vertexFlags & Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1) != 0) * sizeof(vec2);
			const size_t morphedVertexByteStride = morphedColorAttributeOffset + static_cast<size_t>((mesh.vertexFlags & Model::VERTEX_MORPHED_COLOR) != 0) * sizeof(vec4);
			GREM_ASSERT(morphedVertexByteStride % 4 == 0);
			GREM_ASSERT(morphedVertexByteStride / 4 == mesh.morphedVertexStride);

			const size_t morphTargetStrideBytes = vertexCount * morphedVertexByteStride;
			for (const gltf::Mesh::Primitive::Attributes& glTFMorphTargetAttributes : glTFMeshPrimitive.targets) {
				StridedSpan<byte> textureCoordinatesChannel0OffsetData{};
				if ((mesh.vertexFlags & Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0) != 0) {
					textureCoordinatesChannel0OffsetData = StridedSpan{model.morphTargetData.data() + morphTargetDataByteOffset + morphedTextureCoordinatesChannel0AttributeOffset,
						vertexCount, morphedVertexByteStride};
					if (glTFMorphTargetAttributes.texcoord0) {
						readAccessorValues<vec2>(textureCoordinatesChannel0OffsetData, "TEXCOORD_0", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC2},
							glTFAsset.accessors[*glTFMorphTargetAttributes.texcoord0]);
					} else {
						fillWithZeroedValues<vec2>(textureCoordinatesChannel0OffsetData);
					}
				}

				StridedSpan<byte> textureCoordinatesChannel1OffsetData{};
				if ((mesh.vertexFlags & Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1) != 0) {
					textureCoordinatesChannel1OffsetData = StridedSpan{model.morphTargetData.data() + morphTargetDataByteOffset + morphedTextureCoordinatesChannel1AttributeOffset,
						vertexCount, morphedVertexByteStride};
					if (glTFMorphTargetAttributes.texcoord1) {
						readAccessorValues<vec2>(textureCoordinatesChannel1OffsetData, "TEXCOORD_1", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC2},
							glTFAsset.accessors[*glTFMorphTargetAttributes.texcoord1]);
					} else {
						fillWithZeroedValues<vec2>(textureCoordinatesChannel1OffsetData);
					}
				}

				const StridedSpan<byte> normalTextureCoordinatesOffsetData =
					(normalTextureCoordinatesChannel == 1) ? textureCoordinatesChannel1OffsetData : textureCoordinatesChannel0OffsetData;

				StridedSpan<byte> colorOffsetData{};
				if ((mesh.vertexFlags & Model::VERTEX_MORPHED_COLOR) != 0) {
					colorOffsetData = StridedSpan{model.morphTargetData.data() + morphTargetDataByteOffset + morphedColorAttributeOffset, vertexCount, morphedVertexByteStride};
					if (glTFMorphTargetAttributes.color0) {
						const gltf::Accessor& glTFColorOffsetsAccessor = glTFAsset.accessors[*glTFMorphTargetAttributes.color0];
						readAccessorValues<vec4>(colorOffsetData, "COLOR_0",
							{gltf::Accessor::ComponentType::F32, gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::U16},
							{gltf::Accessor::Type::VEC3, gltf::Accessor::Type::VEC4}, glTFColorOffsetsAccessor);
					} else {
						fillWithZeroedValues<vec4>(colorOffsetData);
					}
				}

				StridedSpan<byte> positionOffsetData{};
				if ((mesh.vertexFlags & Model::VERTEX_MORPHED_POSITION) != 0) {
					positionOffsetData =
						StridedSpan{model.morphTargetData.data() + morphTargetDataByteOffset + morphedPositionAttributeOffset, vertexCount, morphedVertexByteStride};
					if (glTFMorphTargetAttributes.position) {
						readAccessorValues<vec3>(positionOffsetData, "POSITION", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC3},
							glTFAsset.accessors[*glTFMorphTargetAttributes.position]);
					} else {
						fillWithZeroedValues<vec3>(positionOffsetData);
					}
				}

				StridedSpan<byte> normalOffsetData{};
				StridedSpan<byte> tangentOffsetData{};
				bool generatedTangents = false;
				if ((mesh.vertexFlags & Model::VERTEX_MORPHED_NORMAL) != 0) {
					normalOffsetData = StridedSpan{model.morphTargetData.data() + morphTargetDataByteOffset + morphedNormalAttributeOffset, vertexCount, morphedVertexByteStride};
					if (glTFMorphTargetAttributes.normal) {
						readAccessorValues<vec3>(normalOffsetData, "NORMAL", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC3},
							glTFAsset.accessors[*glTFMorphTargetAttributes.normal]);
					} else {
						tangentOffsetData =
							StridedSpan{model.morphTargetData.data() + morphTargetDataByteOffset + morphedTangentAttributeOffset, vertexCount, morphedVertexByteStride};
						if (glTFMeshPrimitive.mode == gltf::Mesh::Primitive::Mode::TRIANGLES) {
							const Span<const uint32_t> indices =
								(indexCount == 0) ? Span<const uint32_t>{} : Span{std::launder(reinterpret_cast<const uint32_t*>(indexData.data())), indexCount};
							const Span<const vec3> positions{std::launder(reinterpret_cast<const vec3*>(positionData.data())), vertexCount};
							const Span<const iA2B10G10R10vec4norm> normals{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(normalData.data())), vertexCount};
							const Span<const iA2B10G10R10vec4norm> tangents{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(tangentData.data())), vertexCount};
							Allocation<vec3> morphedPositionsStorage{};
							Span<const vec3> morphedPositions = positions;
							if ((mesh.vertexFlags & Model::VERTEX_MORPHED_POSITION) != 0) {
								morphedPositionsStorage.resize(vertexCount);
								const StridedSpan<const vec3> positionOffsets{std::launder(reinterpret_cast<const vec3*>(positionOffsetData.base())), vertexCount,
									morphedVertexByteStride};
								transform(positions, positionOffsets, morphedPositionsStorage.begin(), ADD);
								morphedPositions = morphedPositionsStorage;
							}
							Allocation<vec2> morphedNormalTextureCoordinatesStorage{};
							Span<const vec2> morphedNormalTextureCoordinates{};
							if (hasMorphedNormalTextureCoordinates) {
								morphedNormalTextureCoordinatesStorage.resize(vertexCount);
								const Span<const vec2> normalTextureCoordinates{std::launder(reinterpret_cast<const vec2*>(normalTextureCoordinatesData.data())), vertexCount};
								const StridedSpan<const vec2> normalTextureCoordinatesOffsets{
									std::launder(reinterpret_cast<const vec2*>(normalTextureCoordinatesOffsetData.base())), vertexCount, morphedVertexByteStride};
								transform(normalTextureCoordinates, normalTextureCoordinatesOffsets, morphedNormalTextureCoordinatesStorage.begin(),
									[](vec2 a, vec2 b) -> vec2 { return vec2{vec2{a} + b}; });
								morphedNormalTextureCoordinates = morphedNormalTextureCoordinatesStorage;
							}
							// Generate the morphed normals and tangents using the morphed vertex positions.
							Allocation<iA2B10G10R10vec4norm> generatedNormals(vertexCount);
							Allocation<iA2B10G10R10vec4norm> generatedTangents(vertexCount);
							Model::generateTriangleNormals(generatedNormals, morphedPositions, indices);
							Model::generateTriangleTangents(generatedTangents, morphedPositions, generatedNormals, morphedNormalTextureCoordinates, indices);
							// Convert the generated morphed normals and tangents into relative offsets from their bind-pose values.
							const StridedSpan<vec3> normalOffsets{std::launder(reinterpret_cast<vec3*>(normalOffsetData.base())), vertexCount, morphedVertexByteStride};
							const StridedSpan<vec3> tangentOffsets{std::launder(reinterpret_cast<vec3*>(tangentOffsetData.base())), vertexCount, morphedVertexByteStride};
							transform(generatedNormals, normals, normalOffsets.begin(), [](iA2B10G10R10vec4norm a, iA2B10G10R10vec4norm b) -> vec3 { return vec3{a} - vec3{b}; });
							transform(generatedTangents, tangents, tangentOffsets.begin(),
								[](iA2B10G10R10vec4norm a, iA2B10G10R10vec4norm b) -> vec3 { return vec3{a} - vec3{b}; });
						} else {
							fillWithZeroedValues<vec3>(normalOffsetData);
							fillWithZeroedValues<vec3>(tangentOffsetData);
						}
						generatedTangents = true;
					}
				}

				if ((mesh.vertexFlags & Model::VERTEX_MORPHED_TANGENT) != 0) {
					if (!generatedTangents) {
						tangentOffsetData =
							StridedSpan{model.morphTargetData.data() + morphTargetDataByteOffset + morphedTangentAttributeOffset, vertexCount, morphedVertexByteStride};
						if (glTFMorphTargetAttributes.tangent) {
							readAccessorValues<vec3>(tangentOffsetData, "TANGENT", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC3},
								glTFAsset.accessors[*glTFMorphTargetAttributes.tangent]);
						} else if (glTFMeshPrimitive.mode == gltf::Mesh::Primitive::Mode::TRIANGLES) {
							const Span<const uint32_t> indices =
								(indexCount == 0) ? Span<const uint32_t>{} : Span{std::launder(reinterpret_cast<const uint32_t*>(indexData.data())), indexCount};
							const Span<const vec3> positions{std::launder(reinterpret_cast<const vec3*>(positionData.data())), vertexCount};
							const Span<const iA2B10G10R10vec4norm> normals{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(normalData.data())), vertexCount};
							const Span<const iA2B10G10R10vec4norm> tangents{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(tangentData.data())), vertexCount};
							Allocation<vec3> morphedPositionsStorage{};
							Span<const vec3> morphedPositions = positions;
							if ((mesh.vertexFlags & Model::VERTEX_MORPHED_POSITION) != 0) {
								morphedPositionsStorage.resize(vertexCount);
								const StridedSpan<const vec3> positionOffsets{std::launder(reinterpret_cast<const vec3*>(positionOffsetData.base())), vertexCount,
									morphedVertexByteStride};
								transform(positions, positionOffsets, morphedPositionsStorage.begin(), ADD);
								morphedPositions = morphedPositionsStorage;
							}
							Allocation<iA2B10G10R10vec4norm> morphedNormalsStorage{};
							Span<const iA2B10G10R10vec4norm> morphedNormals = normals;
							if ((mesh.vertexFlags & Model::VERTEX_MORPHED_NORMAL) != 0) {
								morphedNormalsStorage.resize(vertexCount);
								const StridedSpan<const vec3> normalOffsets{std::launder(reinterpret_cast<const vec3*>(normalOffsetData.base())), vertexCount,
									morphedVertexByteStride};
								transform(normals, normalOffsets, morphedNormalsStorage.begin(),
									[](iA2B10G10R10vec4norm a, vec3 b) -> iA2B10G10R10vec4norm { return iA2B10G10R10vec4norm{vec3{a} + b, 0.0f}; });
								morphedNormals = morphedNormalsStorage;
							}
							Allocation<vec2> morphedNormalTextureCoordinatesStorage{};
							Span<const vec2> morphedNormalTextureCoordinates{};
							if (hasMorphedNormalTextureCoordinates) {
								morphedNormalTextureCoordinatesStorage.resize(vertexCount);
								const Span<const vec2> normalTextureCoordinates{std::launder(reinterpret_cast<const vec2*>(normalTextureCoordinatesData.data())), vertexCount};
								const StridedSpan<const vec2> normalTextureCoordinatesOffsets{
									std::launder(reinterpret_cast<const vec2*>(normalTextureCoordinatesOffsetData.base())), vertexCount, morphedVertexByteStride};
								transform(normalTextureCoordinates, normalTextureCoordinatesOffsets, morphedNormalTextureCoordinatesStorage.begin(),
									[](vec2 a, vec2 b) -> vec2 { return vec2{vec2{a} + b}; });
								morphedNormalTextureCoordinates = morphedNormalTextureCoordinatesStorage;
							}
							// Generate the morphed tangents using the morphed vertex positions.
							Allocation<iA2B10G10R10vec4norm> generatedTangents(vertexCount);
							Model::generateTriangleTangents(generatedTangents, morphedPositions, morphedNormals, morphedNormalTextureCoordinates, indices);
							// Convert the generated morphed tangents into relative offsets from their bind-pose values.
							const StridedSpan<vec3> tangentOffsets{std::launder(reinterpret_cast<vec3*>(tangentOffsetData.base())), vertexCount, morphedVertexByteStride};
							transform(generatedTangents, tangents, tangentOffsets.begin(),
								[](iA2B10G10R10vec4norm a, iA2B10G10R10vec4norm b) -> vec3 { return vec3{a} - vec3{b}; });
						} else {
							fillWithZeroedValues<vec3>(tangentOffsetData);
						}
					}
				}

				morphTargetDataByteOffset += morphTargetStrideBytes;
			}

			++meshIndex;
		}
	}
}

void GlTFModelLoader::loadNodes() {
	GREM_PROFILE_FUNCTION();

	Model::JointIndex staticJointIndex = 1;
	Model::JointIndex dynamicJointIndex = model.staticJointCount;
	Model::MorphTargetWeightIndex morphTargetWeightIndex = 0;
	Model::InstanceIndex instanceIndex = 0;
	Model::LightIndex lightIndex = 0;
	Model::PhysicsObjectIndex physicsObjectIndex = 0;

	model.bindPose.localJoints.front() = {
		.translation = options.rootTranslation,
		.rotation = options.rootRotation,
		.scale = options.rootScale,
		.visible = true,
	};
	model.jointParentIndices.front() = 0;

	traverseRootNodes([&](gltf::NodeIndex glTFRootNodeIndex) -> void {
		traverseNodeTree(glTFRootNodeIndex,
			[&, parentVisible = true, determinant = 1.0f, parentJointIndex = Model::JointIndex{0},
				parentPhysicsObjectIndex = Limits<Model::PhysicsObjectIndex>::MAX](gltf::NodeIndex glTFNodeIndex) mutable -> void {
				const gltf::Node& glTFNode = glTFAsset.nodes[glTFNodeIndex];
				TemporaryNodeData& nodeData = nodesData[glTFNodeIndex];

				if (glTFNode.extensions.KHR_node_visibility && !glTFNode.extensions.KHR_node_visibility->visible) {
					parentVisible = false;
				}

				determinant *= glTFNode.scale.x * glTFNode.scale.y * glTFNode.scale.z;

				if (nodeData.isDynamicJoint || nodeData.isStaticJoint) {
					Model::JointIndex& jointIndex = (nodeData.isDynamicJoint) ? dynamicJointIndex : staticJointIndex;
					model.bindPose.localJoints[jointIndex] = {
						.translation = glTFNode.translation,
						.rotation = glTFNode.rotation,
						.scale = glTFNode.scale,
						.visible = parentVisible,
					};
					model.jointParentIndices[jointIndex] = parentJointIndex;
					if (!glTFNode.name.empty()) {
						model.jointMap[glTFNode.name] = jointIndex;
					}
					parentJointIndex = jointIndex;
					++jointIndex;
				}

				nodeData.jointIndex = parentJointIndex;

				if (glTFNode.mesh) {
					const gltf::Mesh& glTFMesh = glTFAsset.meshes[*glTFNode.mesh];
					TemporaryMeshData& meshData = meshesData[*glTFNode.mesh];

					if (glTFNode.skin) {
						meshData.glTFSkinIndex = *glTFNode.skin;
					}

					const Model::ValueOffset skinDataOffset = (glTFNode.skin) ? skinsData[*glTFNode.skin].skinDataOffset : 0;
					const size_t morphTargetCount = (glTFMesh.primitives.empty()) ? 0 : glTFMesh.primitives.front().targets.size();

					nodeData.morphTargetWeightOffset = morphTargetWeightIndex;
					nodeData.morphTargetWeightCount = static_cast<Model::MorphTargetWeightCount>(morphTargetCount);

					if (!glTFNode.weights.empty()) {
						copy(glTFNode.weights, model.bindPose.localMorphTargetWeights.begin() + morphTargetWeightIndex);
					} else if (!glTFMesh.weights.empty()) {
						copy(glTFMesh.weights, model.bindPose.localMorphTargetWeights.begin() + morphTargetWeightIndex);
					} else {
						fill(Span{model.bindPose.localMorphTargetWeights.data() + morphTargetWeightIndex, morphTargetCount}, 0.0f);
					}

					Model::MeshIndex meshIndex = meshData.meshOffset;
					for (const gltf::Mesh::Primitive& glTFMeshPrimitive : glTFMesh.primitives) {
						if (!glTFMeshPrimitive.attributes.position) {
							continue;
						}

						Model::InstanceFlags instanceFlags{};
						if (determinant < 0.0f) {
							instanceFlags |= Model::INSTANCE_REVERSE_WINDING_ORDER;
						}

						model.instances[instanceIndex] = {
							.materialIndex = (glTFMeshPrimitive.material) ? static_cast<Model::MaterialIndex>(*glTFMeshPrimitive.material)
					                                                      : static_cast<Model::MaterialIndex>(model.materials.size()),
							.meshIndex = meshIndex,
							.skinDataOffset = skinDataOffset,
							.morphTargetWeightOffset = morphTargetWeightIndex,
							.jointIndex = parentJointIndex,
							.instanceFlags = instanceFlags,
						};
						++instanceIndex;
						++meshIndex;
					}

					morphTargetWeightIndex += static_cast<Model::MorphTargetWeightCount>(morphTargetCount);
				}

				if (!options.excludeLights && glTFNode.extensions.KHR_lights_punctual) {
					const gltf::Asset::Extension::KHRLightsPunctual::Light& glTFLight =
						glTFAsset.extensions.KHR_lights_punctual->lights[glTFNode.extensions.KHR_lights_punctual->light];
					if (!glTFLight.name.empty()) {
						model.lightMap[glTFLight.name] = lightIndex;
					}
					switch (glTFLight.type) {
						case gltf::Asset::Extension::KHRLightsPunctual::Light::Type::DIRECTIONAL:
							model.lights[lightIndex] = Model::Light{
								Model::DirectionalLight{
									.color = glTFLight.color,
									.intensity = glTFLight.intensity,
								},
								parentJointIndex,
							};
							break;
						case gltf::Asset::Extension::KHRLightsPunctual::Light::Type::POINT:
							model.lights[lightIndex] = Model::Light{
								Model::PointLight{
									.color = glTFLight.color,
									.intensity = glTFLight.intensity,
									.range = glTFLight.range.value_or(0.0f),
								},
								parentJointIndex,
							};
							break;
						case gltf::Asset::Extension::KHRLightsPunctual::Light::Type::SPOT:
							model.lights[lightIndex] = Model::Light{
								Model::SpotLight{
									.color = glTFLight.color,
									.intensity = glTFLight.intensity,
									.range = glTFLight.range.value_or(0.0f),
									.innerConeAngle = glTFLight.spot->innerConeAngle,
									.outerConeAngle = glTFLight.spot->outerConeAngle,
								},
								parentJointIndex,
							};
							break;
					}
					++lightIndex;
				}

				if (!options.excludePhysics && glTFNode.extensions.KHR_physics_rigid_bodies && glTFNode.extensions.KHR_physics_rigid_bodies->motion) {
					parentPhysicsObjectIndex = physicsObjectIndex;
					++physicsObjectIndex;
				}

				nodeData.physicsObjectIndex = parentPhysicsObjectIndex;
			});
	});

	if (!model.instances.empty()) {
		Model::Transformation bindPoseTransformation{};
		bindPoseTransformation.assign(mat4{1.0f}, model.bindPose.localJoints, model.bindPose.localMorphTargetWeights, model.jointParentIndices);

		model.bindPoseBoundingBox = {.min{Limits<float>::MAX}, .max{Limits<float>::MIN}};
		model.bindPoseBoundingRadius = 0.0f;
		for (const Model::Instance& instance : model.instances) {
			const Model::Mesh& mesh = model.meshes[instance.meshIndex];
			const mat4 jointMatrix =
				((mesh.vertexFlags & Model::VERTEX_SKINNED) != 0) ? bindPoseTransformation.jointMatrices.front() : bindPoseTransformation.jointMatrices[instance.jointIndex];
			const vec3 translation{jointMatrix[3]};
			const float maxScale = sqrt(maxComponent(vec3{length2(vec3{jointMatrix[0]}), length2(vec3{jointMatrix[1]}), length2(vec3{jointMatrix[2]})}));
			const Box<3, float> boundingBox = getTransformedBoundingBox(jointMatrix, mesh.boundingBox);
			model.bindPoseBoundingBox.min = min(model.bindPoseBoundingBox.min, boundingBox.min);
			model.bindPoseBoundingBox.max = max(model.bindPoseBoundingBox.max, boundingBox.max);
			model.bindPoseBoundingRadius = max(model.bindPoseBoundingRadius, length(translation) + maxScale * mesh.boundingRadius);
		}
	}
}

void GlTFModelLoader::translateJointIndices() {
	GREM_PROFILE_FUNCTION();

	Model::MeshIndex meshIndex = 0;

	for (gltf::MeshIndex glTFMeshIndex = 0; glTFMeshIndex < glTFAsset.meshes.size(); ++glTFMeshIndex) {
		const gltf::Mesh& glTFMesh = glTFAsset.meshes[glTFMeshIndex];
		const TemporaryMeshData& meshData = meshesData[glTFMeshIndex];

		for (size_t glTFMeshPrimitiveIndex = 0; glTFMeshPrimitiveIndex < glTFMesh.primitives.size(); ++glTFMeshPrimitiveIndex) {
			const gltf::Mesh::Primitive& glTFMeshPrimitive = glTFMesh.primitives[glTFMeshPrimitiveIndex];
			if (!glTFMeshPrimitive.attributes.position) {
				continue;
			}

			if (meshData.glTFSkinIndex) {
				const Model::Mesh& mesh = model.meshes[meshIndex];
				const Model::VertexFlags vertexFlags = mesh.vertexFlags;
				if ((vertexFlags & Model::VERTEX_SKINNED) != 0) {
					const gltf::SkinIndex glTFSkinIndex = *meshData.glTFSkinIndex;
					const gltf::Skin& glTFSkin = glTFAsset.skins[glTFSkinIndex];
					const size_t indexCount = static_cast<size_t>(mesh.indexCount);
					const size_t vertexCount = static_cast<size_t>(mesh.vertexCount);
					size_t meshDataByteOffset = static_cast<size_t>(mesh.meshDataOffset) * 4;
					meshDataByteOffset += indexCount * sizeof(uint32_t) + vertexCount * (sizeof(vec3) + sizeof(iA2B10G10R10vec4norm) + sizeof(iA2B10G10R10vec4norm));
					meshDataByteOffset += vertexCount * sizeof(vec2) * static_cast<size_t>((vertexFlags & Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0);
					meshDataByteOffset += vertexCount * sizeof(vec2) * static_cast<size_t>((vertexFlags & Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0);
					meshDataByteOffset += vertexCount * sizeof(u8vec4norm) * static_cast<size_t>((vertexFlags & Model::VERTEX_COLORED) != 0);
					const Span<u8vec4> jointIndices{std::launder(reinterpret_cast<u8vec4*>(model.meshData.data() + meshDataByteOffset)), vertexCount};
					for (u8vec4& joints : jointIndices) {
						for (size_t jointIndexIndex = 0; jointIndexIndex < 4; ++jointIndexIndex) {
							uint8_t& jointIndex = joints[jointIndexIndex];
							const size_t skinJointIndex = static_cast<size_t>(jointIndex);
							if (skinJointIndex >= glTFSkin.joints.size()) {
								throw resource::Error{formatString("Joint index in JOINTS_0 data of mesh primitive {} in mesh {} (\"{}\") is out of range for skin {} (\"{}\").",
									glTFMeshPrimitiveIndex, glTFMeshIndex, glTFMesh.name, glTFSkinIndex, glTFSkin.name)};
							}
							jointIndex = nodesData[glTFSkin.joints[skinJointIndex]].jointIndex;
						}
					}
				}
			}

			++meshIndex;
		}
	}

	for (const gltf::Skin& glTFSkin : glTFAsset.skins) {
		if (glTFSkin.skeleton && !glTFSkin.name.empty()) {
			model.skinMap[glTFSkin.name] = nodesData[*glTFSkin.skeleton].jointIndex;
		}
	}
}

void GlTFModelLoader::loadAnimations() {
	if (options.excludeAnimations) {
		return;
	}

	GREM_PROFILE_FUNCTION();

	Model::AnimationIndex animationIndex = 0;
	Model::AnimationChannelIndex animationChannelIndex = 0;
	Model::ValueOffset keyframeInputTimePointIndex = 0;
	size_t keyframeOutputValueDataByteOffset = 0;

	for (const gltf::Animation& glTFAnimation : glTFAsset.animations) {
		const Model::AnimationChannelIndex animationChannelOffset = animationChannelIndex;

		for (const gltf::Animation::Channel& glTFAnimationChannel : glTFAnimation.channels) {
			if (!glTFAnimationChannel.target.node) {
				continue;
			}

			const TemporaryNodeData& nodeData = nodesData[*glTFAnimationChannel.target.node];
			const gltf::Animation::Sampler& glTFAnimationSampler = glTFAnimation.samplers[glTFAnimationChannel.sampler];
			const gltf::Accessor& glTFInputAccessor = glTFAsset.accessors[glTFAnimationSampler.input];
			const gltf::Accessor& glTFOutputAccessor = glTFAsset.accessors[glTFAnimationSampler.output];
			const size_t inputCount = glTFInputAccessor.count;
			const size_t outputCount = glTFOutputAccessor.count;

			const float minTimePoint = (*glTFInputAccessor.min)[0];
			const float maxTimePoint = (*glTFInputAccessor.max)[0];
			const Model::ValueOffset keyframeInputTimePointOffset = static_cast<Model::ValueOffset>(keyframeInputTimePointIndex);
			GREM_ASSERT(keyframeOutputValueDataByteOffset % 4 == 0);
			const Model::ValueOffset keyframeOutputValueOffset = static_cast<Model::ValueOffset>(keyframeOutputValueDataByteOffset / 4);

			const Span<float> inputTimePoints{model.keyframeInputTimePoints.data() + keyframeInputTimePointIndex, inputCount};
			readAccessorValues<float>(asWritableBytes(inputTimePoints), "input", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::SCALAR}, glTFInputAccessor);
			keyframeInputTimePointIndex += static_cast<Model::ValueOffset>(inputCount);

			uint32_t targetOffset{};
			Model::MorphTargetWeightCount targetMorphTargetWeightCount = 0;
			Model::AnimationPath targetPath{};
			switch (glTFAnimationChannel.target.path) {
				case gltf::Animation::Channel::Target::Path::TRANSLATION: {
					targetOffset = nodeData.jointIndex;
					targetPath = Model::AnimationPath::JOINT_TRANSLATION;
					const Span<byte> translationData{model.keyframeOutputValueData.data() + keyframeOutputValueDataByteOffset, outputCount * sizeof(vec3)};
					keyframeOutputValueDataByteOffset += translationData.size_bytes();
					readAccessorValues<vec3>(translationData, "output", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC3}, glTFOutputAccessor);
					break;
				}
				case gltf::Animation::Channel::Target::Path::ROTATION: {
					targetOffset = nodeData.jointIndex;
					targetPath = Model::AnimationPath::JOINT_ROTATION;
					const Span<byte> rotationData{model.keyframeOutputValueData.data() + keyframeOutputValueDataByteOffset, outputCount * sizeof(vec4)};
					keyframeOutputValueDataByteOffset += rotationData.size_bytes();
					readAccessorValues<vec4>(rotationData, "output",
						{gltf::Accessor::ComponentType::F32, gltf::Accessor::ComponentType::I8, gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::I16,
							gltf::Accessor::ComponentType::U16},
						{gltf::Accessor::Type::VEC4}, glTFOutputAccessor);
					break;
				}
				case gltf::Animation::Channel::Target::Path::SCALE: {
					targetOffset = nodeData.jointIndex;
					targetPath = Model::AnimationPath::JOINT_SCALE;
					const Span<byte> scaleData{model.keyframeOutputValueData.data() + keyframeOutputValueDataByteOffset, outputCount * sizeof(vec3)};
					keyframeOutputValueDataByteOffset += scaleData.size_bytes();
					readAccessorValues<vec3>(scaleData, "output", {gltf::Accessor::ComponentType::F32}, {gltf::Accessor::Type::VEC3}, glTFOutputAccessor);
					break;
				}
				case gltf::Animation::Channel::Target::Path::WEIGHTS: {
					targetOffset = nodeData.morphTargetWeightOffset;
					targetPath = Model::AnimationPath::MORPH_TARGET_WEIGHTS;
					targetMorphTargetWeightCount = static_cast<Model::MorphTargetWeightCount>(
						((glTFAnimationSampler.interpolation == gltf::Animation::Sampler::Interpolation::CUBIC_SPLINE) ? outputCount / 3 : outputCount) / inputCount);
					const Span<byte> weightData{model.keyframeOutputValueData.data() + keyframeOutputValueDataByteOffset, outputCount * sizeof(float)};
					keyframeOutputValueDataByteOffset += weightData.size_bytes();
					readAccessorValues<float>(weightData, "output",
						{gltf::Accessor::ComponentType::F32, gltf::Accessor::ComponentType::I8, gltf::Accessor::ComponentType::U8, gltf::Accessor::ComponentType::I16,
							gltf::Accessor::ComponentType::U16},
						{gltf::Accessor::Type::SCALAR}, glTFOutputAccessor);
					break;
				}
			}

			Model::InterpolationMode interpolationMode{};
			switch (glTFAnimationSampler.interpolation) {
				case gltf::Animation::Sampler::Interpolation::LINEAR: interpolationMode = Model::InterpolationMode::LINEAR; break;
				case gltf::Animation::Sampler::Interpolation::STEP: interpolationMode = Model::InterpolationMode::STEP; break;
				case gltf::Animation::Sampler::Interpolation::CUBIC_SPLINE: interpolationMode = Model::InterpolationMode::CUBIC_SPLINE; break;
			}

			model.animationChannels[animationChannelIndex] = {
				.minTimePoint = minTimePoint,
				.maxTimePoint = maxTimePoint,
				.keyframeCount = static_cast<Model::KeyframeCount>(inputCount),
				.keyframeInputTimePointOffset = keyframeInputTimePointOffset,
				.keyframeOutputValueOffset = keyframeOutputValueOffset,
				.targetOffset = targetOffset,
				.targetMorphTargetWeightCount = targetMorphTargetWeightCount,
				.targetPath = targetPath,
				.interpolationMode = interpolationMode,
			};

			++animationChannelIndex;
		}

		const Model::AnimationChannelCount animationChannelCount = animationChannelIndex - animationChannelOffset;
		if (animationChannelCount > 0) {
			const Span<const Model::AnimationChannel> animationChannels = Span{model.animationChannels}.subspan(animationChannelOffset, animationChannelCount);
			float minTimePoint = animationChannels.front().minTimePoint;
			float maxTimePoint = animationChannels.front().maxTimePoint;
			for (const Model::AnimationChannel& animationChannel : animationChannels.subspan(1)) {
				minTimePoint = min(minTimePoint, animationChannel.minTimePoint);
				maxTimePoint = max(maxTimePoint, animationChannel.maxTimePoint);
			}
			model.animations[animationIndex] = {
				.minTimePoint = minTimePoint,
				.maxTimePoint = maxTimePoint,
				.animationChannelOffset = animationChannelOffset,
			};
			if (!glTFAnimation.name.empty()) {
				model.animationMap[glTFAnimation.name] = animationIndex;
			}
			++animationIndex;
		}
	}
}

void GlTFModelLoader::loadCollidersAndPhysics() {
	if (options.excludeColliders && options.excludePhysics) {
		return;
	}

	GREM_PROFILE_FUNCTION();

	Model::Transformation bindPoseTransformation{};
	bindPoseTransformation.assign(mat4{1.0f}, model.bindPose.localJoints, model.bindPose.localMorphTargetWeights, model.jointParentIndices);

	const auto readCollider = [&](const gltf::Node& glTFNode, const mat4& globalTransformation, const gltf::Node::Extension::KHRPhysicsRigidBodies::Geometry& glTFGeometry,
								  Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::CollisionFilterIndex> glTFCollisionFilterIndex) -> Model::Collider {
		Model::Collider result{
			.shape{},
			.layers{},
			.detectionLayers = ~Model::CollisionLayers{},
			.noDetectionLayers{},
			.responseLayers = ~Model::CollisionLayers{},
			.noResponseLayers{},
		};
		if (glTFGeometry.shape) {
			GREM_MATCH(glTFAsset.extensions.KHR_implicit_shapes->shapes[*glTFGeometry.shape]) {
				GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Plane& glTFPlane) {
					result.shape = Model::PlaneShape{
						.sizeX = glTFPlane.sizeX.value_or(Limits<float>::INF),
						.sizeZ = glTFPlane.sizeZ.value_or(Limits<float>::INF),
						.doubleSided = glTFPlane.doubleSided,
					};
					break;
				}
				GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Sphere& glTFSphere) {
					result.shape = Model::SphereShape{.radius = glTFSphere.radius};
					break;
				}
				GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Box& glTFBox) {
					result.shape = Model::BoxShape{.size = glTFBox.size};
					break;
				}
				GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Cylinder& glTFCylinder) {
					result.shape = Model::CylinderShape{.halfLength = glTFCylinder.height * 0.5f, .bottomRadius = glTFCylinder.radiusBottom, .topRadius = glTFCylinder.radiusTop};
					break;
				}
				GREM_CASE(const gltf::Asset::Extension::KHRImplicitShapes::Capsule& glTFCapsule) {
					result.shape = Model::CapsuleShape{.halfLength = glTFCapsule.height * 0.5f, .bottomRadius = glTFCapsule.radiusBottom, .topRadius = glTFCapsule.radiusTop};
					break;
				}
			}
		} else if (glTFGeometry.mesh) {
			size_t totalGeometryIndexCount = 0;
			size_t totalGeometryVertexCount = 0;

			const gltf::Mesh& glTFMesh = glTFAsset.meshes[*glTFGeometry.mesh];
			const TemporaryMeshData& meshData = meshesData[*glTFGeometry.mesh];

			Model::MeshIndex meshIndex = meshData.meshOffset;
			for (const gltf::Mesh::Primitive& glTFMeshPrimitive : glTFMesh.primitives) {
				if (!glTFMeshPrimitive.attributes.position) {
					continue;
				}
				const Model::Mesh& mesh = model.meshes[meshIndex];
				if (glTFGeometry.convexHull || mesh.primitiveType == Model::PrimitiveType::TRIANGLES) {
					const size_t indexCount = static_cast<size_t>(mesh.indexCount);
					const size_t vertexCount = static_cast<size_t>(mesh.vertexCount);
					totalGeometryIndexCount += (glTFGeometry.convexHull) ? 0 : (indexCount == 0) ? vertexCount : indexCount;
					totalGeometryVertexCount += vertexCount;
				}
				++meshIndex;
			}

			Allocation<uint32_t> geometryIndices(totalGeometryIndexCount);
			Allocation<vec3> geometryVertices(totalGeometryVertexCount);
			size_t geometryIndicesOffset = 0;
			size_t geometryVerticesOffset = 0;
			Buffer<float> morphTargetWeights{};

			const size_t morphTargetCount = (glTFMesh.primitives.empty()) ? 0 : glTFMesh.primitives.front().targets.size();
			morphTargetWeights.resize(morphTargetCount);
			if (!glTFNode.weights.empty()) {
				copy(glTFNode.weights, morphTargetWeights.begin());
			} else if (!glTFMesh.weights.empty()) {
				copy(glTFMesh.weights, morphTargetWeights.begin());
			} else {
				fill(morphTargetWeights, 0.0f);
			}

			meshIndex = meshData.meshOffset;
			for (const gltf::Mesh::Primitive& glTFMeshPrimitive : glTFMesh.primitives) {
				if (!glTFMeshPrimitive.attributes.position) {
					continue;
				}

				const Model::Mesh& mesh = model.meshes[meshIndex];
				if (glTFGeometry.convexHull || mesh.primitiveType == Model::PrimitiveType::TRIANGLES) {
					const Model::VertexFlags vertexFlags = mesh.vertexFlags;
					const size_t indexCount = static_cast<size_t>(mesh.indexCount);
					const size_t vertexCount = static_cast<size_t>(mesh.vertexCount);
					size_t meshDataByteOffset = static_cast<size_t>(mesh.meshDataOffset) * 4;
					if (!glTFGeometry.convexHull) {
						if (indexCount == 0) {
							iota(Span{geometryIndices}.subspan(geometryIndicesOffset, vertexCount), static_cast<uint32_t>(geometryVerticesOffset));
						} else {
							const Span<const uint32_t> indices{std::launder(reinterpret_cast<const uint32_t*>(model.meshData.data() + meshDataByteOffset)), indexCount};
							for (size_t i = 0; i < indexCount; ++i) {
								geometryIndices[geometryIndicesOffset + i] = static_cast<uint32_t>(static_cast<uint32_t>(geometryVerticesOffset) + indices[i]);
							}
						}
					}
					meshDataByteOffset += indexCount * sizeof(uint32_t);
					if (vertexCount > 0) {
						const Span<const vec3> positions{std::launder(reinterpret_cast<const vec3*>(model.meshData.data() + meshDataByteOffset)), vertexCount};
						for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
							geometryVertices[geometryVerticesOffset + vertexIndex] = vec3{positions[vertexIndex]};
						}
					}
					if ((vertexFlags & Model::VERTEX_MORPHED_POSITION) != 0) {
						const size_t morphTargetDataByteOffset = static_cast<size_t>(mesh.morphTargetDataOffset) * 4;
						const size_t morphedVertexByteStride = static_cast<size_t>(mesh.morphedVertexStride) * 4;
						const StridedSpan<const vec3> positionOffsets{std::launder(reinterpret_cast<const vec3*>(model.morphTargetData.data() + morphTargetDataByteOffset)),
							vertexCount, morphedVertexByteStride};
						for (size_t i = 0; i < vertexCount; ++i) {
							geometryVertices[geometryVerticesOffset + i] += positionOffsets[i];
						}
					}
					if ((vertexFlags & Model::VERTEX_SKINNED) != 0) {
						const Span<const mat4> inverseBindPoseMatrices{
							std::launder(reinterpret_cast<const mat4*>(model.skinData.data() + static_cast<size_t>(skinsData[*glTFNode.skin].skinDataOffset) * 4)),
							model.bindPose.localJoints.size()};
						meshDataByteOffset += vertexCount * (sizeof(vec3) + sizeof(iA2B10G10R10vec4norm) + sizeof(iA2B10G10R10vec4norm));
						meshDataByteOffset += vertexCount * sizeof(vec2) * static_cast<size_t>((vertexFlags & Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0);
						meshDataByteOffset += vertexCount * sizeof(vec2) * static_cast<size_t>((vertexFlags & Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0);
						meshDataByteOffset += vertexCount * sizeof(u8vec4norm) * static_cast<size_t>((vertexFlags & Model::VERTEX_COLORED) != 0);
						const Span<const u8vec4> jointIndices{std::launder(reinterpret_cast<const u8vec4*>(model.meshData.data() + meshDataByteOffset)), vertexCount};
						meshDataByteOffset += vertexCount * sizeof(u8vec4);
						const Span<const u8vec4norm> jointWeights{std::launder(reinterpret_cast<const u8vec4norm*>(model.meshData.data() + meshDataByteOffset)), vertexCount};
						const mat4 inverseTransformationMatrix = inverse(globalTransformation);
						for (size_t i = 0; i < vertexCount; ++i) {
							const mat4 jointMatrix =
								inverseTransformationMatrix * bindPoseTransformation.jointMatrices[jointIndices[i][0]] * inverseBindPoseMatrices[jointIndices[i][0]] *
									static_cast<float>(jointWeights[i][0]) +
								bindPoseTransformation.jointMatrices[jointIndices[i][1]] * inverseBindPoseMatrices[jointIndices[i][1]] * static_cast<float>(jointWeights[i][1]) +
								bindPoseTransformation.jointMatrices[jointIndices[i][2]] * inverseBindPoseMatrices[jointIndices[i][2]] * static_cast<float>(jointWeights[i][2]) +
								bindPoseTransformation.jointMatrices[jointIndices[i][3]] * inverseBindPoseMatrices[jointIndices[i][3]] * static_cast<float>(jointWeights[i][3]);
							geometryVertices[geometryVerticesOffset + i] = vec3{jointMatrix * vec4{geometryVertices[geometryVerticesOffset + i], 1.0f}};
						}
					}

					geometryIndicesOffset += (glTFGeometry.convexHull) ? 0 : (indexCount == 0) ? vertexCount : indexCount;
					geometryVerticesOffset += vertexCount;
				}

				++meshIndex;
			}

			GREM_ASSERT(geometryIndicesOffset == totalGeometryIndexCount);
			GREM_ASSERT(geometryVerticesOffset == totalGeometryVertexCount);

			if (glTFGeometry.convexHull) {
				result.shape = Model::ConvexPolytopeShape::create(geometryVertices, 32);
			} else {
				result.shape = Model::TriangleMeshShape::create(std::move(geometryVertices), std::move(geometryIndices));
			}
		}

		if (glTFCollisionFilterIndex) {
			const gltf::Asset::Extension::KHRPhysicsRigidBodies::CollisionFilter& glTFCollisionFilter =
				glTFAsset.extensions.KHR_physics_rigid_bodies->collisionFilters[*glTFCollisionFilterIndex];
			const auto readCollisionLayers = [&](Span<const String> layers) -> Model::CollisionLayers {
				Model::CollisionLayers result{};
				for (const String& layer : layers) {
					const auto [it, inserted] = model.collisionLayerMap.emplace(layer, model.collisionLayerMap.size());
					if (it->second >= Model::MAX_COLLISION_LAYER_COUNT) {
						throw resource::Error{"Too many collision layers in asset."};
					}
					result[it->second] = true;
				}
				return result;
			};
			result.layers = readCollisionLayers(glTFCollisionFilter.collisionSystems);
			if (!glTFCollisionFilter.collideWithSystems.empty()) {
				result.detectionLayers = readCollisionLayers(glTFCollisionFilter.collideWithSystems);
			}
			result.noDetectionLayers = readCollisionLayers(glTFCollisionFilter.notCollideWithSystems);

			result.responseLayers = result.detectionLayers;
			result.noResponseLayers = result.noDetectionLayers;
		} else {
			const auto [it, inserted] = model.collisionLayerMap.emplace(String{}, model.collisionLayerMap.size());
			if (it->second >= Model::MAX_COLLISION_LAYER_COUNT) {
				throw resource::Error{"Too many collision layers in asset."};
			}
			result.layers[it->second] = true;
		}
		return result;
	};

	Model::PhysicsObjectIndex physicsObjectIndex = 0;
	Model::PhysicsJointIndex physicsJointIndex = 0;

	traverseRootNodes([&](gltf::NodeIndex glTFRootNodeIndex) -> void {
		traverseNodeTree(glTFRootNodeIndex, [&, globalTransformation = mat4{1.0f}](gltf::NodeIndex glTFNodeIndex) mutable -> void {
			const gltf::Node& glTFNode = glTFAsset.nodes[glTFNodeIndex];
			const TemporaryNodeData& nodeData = nodesData[glTFNodeIndex];

			globalTransformation = globalTransformation * translateRotateScale(glTFNode.translation, glTFNode.rotation, glTFNode.scale);

			if (glTFNode.extensions.KHR_physics_rigid_bodies) {
				if (const Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::Collider>& glTFCollider = glTFNode.extensions.KHR_physics_rigid_bodies->collider) {
					model.jointColliders[nodeData.jointIndex] = readCollider(glTFNode, globalTransformation, glTFCollider->geometry, glTFCollider->collisionFilter);
				} else if (const Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::Trigger>& glTFTrigger = glTFNode.extensions.KHR_physics_rigid_bodies->trigger) {
					if (glTFTrigger->geometry) {
						Model::Collider collider = readCollider(glTFNode, globalTransformation, *glTFTrigger->geometry, glTFTrigger->collisionFilter);
						collider.responseLayers = {};
						collider.noResponseLayers = {};
						model.jointColliders[nodeData.jointIndex] = std::move(collider);
					}
				}

				if (!options.excludePhysics) {
					model.jointPhysicsObjectIndices[nodeData.jointIndex] = nodeData.physicsObjectIndex;

					if (const Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::Motion>& glTFMotion = glTFNode.extensions.KHR_physics_rigid_bodies->motion) {
						Model::PhysicsObject physicsObject{
							.jointIndex = nodeData.jointIndex,
							.mass = 0.0f,
							.centerOfMass = glTFMotion->centerOfMass,
							.principalMomentsOfInertia{},
							.inertiaOrientation = glTFMotion->inertiaOrientation,
							.initialLinearVelocity = glTFMotion->linearVelocity,
							.initialAngularVelocity = glTFMotion->angularVelocity,
							.gravityFactor = glTFMotion->gravityFactor,
							.staticFriction = 0.6f,
							.dynamicFriction = 0.6f,
							.rollingResistance = 0.01f,
							.restitution = 0.0f,
							.frictionCombine = Model::FrictionCombine::MINIMUM,
							.restitutionCombine = Model::RestitutionCombine::MAXIMUM,
						};
						if (glTFMotion->mass) {
							physicsObject.mass = (*glTFMotion->mass == 0.0f) ? Limits<float>::INF : *glTFMotion->mass;
						}
						if (glTFMotion->inertiaDiagonal) {
							physicsObject.principalMomentsOfInertia.x = (glTFMotion->inertiaDiagonal->x == 0.0f) ? Limits<float>::INF : glTFMotion->inertiaDiagonal->x;
							physicsObject.principalMomentsOfInertia.y = (glTFMotion->inertiaDiagonal->y == 0.0f) ? Limits<float>::INF : glTFMotion->inertiaDiagonal->y;
							physicsObject.principalMomentsOfInertia.z = (glTFMotion->inertiaDiagonal->z == 0.0f) ? Limits<float>::INF : glTFMotion->inertiaDiagonal->z;
						}
						if (const Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::Collider>& glTFCollider = glTFNode.extensions.KHR_physics_rigid_bodies->collider) {
							if (glTFCollider->physicsMaterial) {
								const gltf::Asset::Extension::KHRPhysicsRigidBodies::Material& glTFPhysicsMaterial =
									glTFAsset.extensions.KHR_physics_rigid_bodies->physicsMaterials[*glTFCollider->physicsMaterial];
								physicsObject.staticFriction = glTFPhysicsMaterial.staticFriction;
								physicsObject.dynamicFriction = glTFPhysicsMaterial.dynamicFriction;
								if (glTFPhysicsMaterial.dynamicFriction == 0) {
									physicsObject.rollingResistance = 0.0f;
								}
								physicsObject.restitution = glTFPhysicsMaterial.restitution;
								if (glTFPhysicsMaterial.frictionCombine) {
									physicsObject.frictionCombine = static_cast<Model::FrictionCombine>(*glTFPhysicsMaterial.frictionCombine);
								}
								if (glTFPhysicsMaterial.restitutionCombine) {
									physicsObject.restitutionCombine = static_cast<Model::RestitutionCombine>(*glTFPhysicsMaterial.restitutionCombine);
								}
							}
						}
						model.physicsObjects[physicsObjectIndex] = physicsObject;
						++physicsObjectIndex;
					}

					if (const Optional<gltf::Node::Extension::KHRPhysicsRigidBodies::Joint>& glTFNodeJoint = glTFNode.extensions.KHR_physics_rigid_bodies->joint) {
						const TemporaryNodeData& connectedNodeData = nodesData[glTFNodeJoint->connectedNode];
						const gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint& glTFJoint = glTFAsset.extensions.KHR_physics_rigid_bodies->physicsJoints[glTFNodeJoint->joint];
						Model::PhysicsJoint physicsJoint{
							.objectIndices{nodeData.physicsObjectIndex, connectedNodeData.physicsObjectIndex},
							.jointIndices{nodeData.jointIndex, connectedNodeData.jointIndex},
							.driveIgnoresMassX = false,
							.driveIgnoresMassY = false,
							.driveIgnoresMassZ = false,
							.driveIgnoresMomentOfInertiaX = false,
							.driveIgnoresMomentOfInertiaY = false,
							.driveIgnoresMomentOfInertiaZ = false,
							.enableCollision = glTFNodeJoint->enableCollision,
							.minDistances{-Limits<float>::INF},
							.maxDistances{Limits<float>::INF},
							.minAngles{-Limits<float>::INF},
							.maxAngles{Limits<float>::INF},
							.linearStiffnesses{Limits<float>::INF},
							.angularStiffnesses{Limits<float>::INF},
							.linearDamping{},
							.angularDamping{},
							.maxForce{},
							.maxTorque{},
							.targetPosition{},
							.targetAngles{},
							.targetLinearVelocity{},
							.targetAngularVelocity{},
							.linearDriveDamping{},
							.angularDriveDamping{},
						};
						for (const gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit& glTFLimit : glTFJoint.limits) {
							GREM_MATCH(glTFLimit.axes) {
								GREM_CASE(const gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::LinearAxes& glTFLinearAxes) {
									if (glTFLimit.min) {
										if (glTFLinearAxes.x) {
											physicsJoint.minDistances.x = *glTFLimit.min;
										}
										if (glTFLinearAxes.y) {
											physicsJoint.minDistances.y = *glTFLimit.min;
										}
										if (glTFLinearAxes.z) {
											physicsJoint.minDistances.z = *glTFLimit.min;
										}
									}
									if (glTFLimit.max) {
										if (glTFLinearAxes.x) {
											physicsJoint.maxDistances.x = *glTFLimit.max;
										}
										if (glTFLinearAxes.y) {
											physicsJoint.maxDistances.y = *glTFLimit.max;
										}
										if (glTFLinearAxes.z) {
											physicsJoint.maxDistances.z = *glTFLimit.max;
										}
									}
									if (glTFLimit.stiffness) {
										if (glTFLinearAxes.x) {
											physicsJoint.linearStiffnesses.x = *glTFLimit.stiffness;
										}
										if (glTFLinearAxes.y) {
											physicsJoint.linearStiffnesses.y = *glTFLimit.stiffness;
										}
										if (glTFLinearAxes.z) {
											physicsJoint.linearStiffnesses.z = *glTFLimit.stiffness;
										}
									}
									if (glTFLinearAxes.x) {
										physicsJoint.linearDamping.x = glTFLimit.damping;
									}
									if (glTFLinearAxes.y) {
										physicsJoint.linearDamping.y = glTFLimit.damping;
									}
									if (glTFLinearAxes.z) {
										physicsJoint.linearDamping.z = glTFLimit.damping;
									}
									break;
								}
								GREM_CASE(const gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint::Limit::AngularAxes& glTFAngularAxes) {
									if (glTFLimit.min) {
										if (glTFAngularAxes.x) {
											physicsJoint.minAngles.x = *glTFLimit.min;
										}
										if (glTFAngularAxes.y) {
											physicsJoint.minAngles.y = *glTFLimit.min;
										}
										if (glTFAngularAxes.z) {
											physicsJoint.minAngles.z = *glTFLimit.min;
										}
									}
									if (glTFLimit.max) {
										if (glTFAngularAxes.x) {
											physicsJoint.maxAngles.x = *glTFLimit.max;
										}
										if (glTFAngularAxes.y) {
											physicsJoint.maxAngles.y = *glTFLimit.max;
										}
										if (glTFAngularAxes.z) {
											physicsJoint.maxAngles.z = *glTFLimit.max;
										}
									}
									if (glTFLimit.stiffness) {
										if (glTFAngularAxes.x) {
											physicsJoint.angularStiffnesses.x = *glTFLimit.stiffness;
										}
										if (glTFAngularAxes.y) {
											physicsJoint.angularStiffnesses.y = *glTFLimit.stiffness;
										}
										if (glTFAngularAxes.z) {
											physicsJoint.angularStiffnesses.z = *glTFLimit.stiffness;
										}
									}
									if (glTFAngularAxes.x) {
										physicsJoint.angularDamping.x = glTFLimit.damping;
									}
									if (glTFAngularAxes.y) {
										physicsJoint.angularDamping.y = glTFLimit.damping;
									}
									if (glTFAngularAxes.z) {
										physicsJoint.angularDamping.z = glTFLimit.damping;
									}
									break;
								}
							}
						}
						for (const gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive& glTFDrive : glTFJoint.drives) {
							switch (glTFDrive.type) {
								case gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Type::LINEAR:
									if (glTFDrive.mode == gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Mode::ACCELERATION) {
										switch (glTFDrive.axis) {
											case 0: physicsJoint.driveIgnoresMassX = true; break;
											case 1: physicsJoint.driveIgnoresMassY = true; break;
											case 2: physicsJoint.driveIgnoresMassZ = true; break;
											default: unreachable();
										}
									}
									if (glTFDrive.maxForce) {
										physicsJoint.maxForce[glTFDrive.axis] = *glTFDrive.maxForce;
									}
									if (glTFDrive.positionTarget) {
										physicsJoint.targetPosition[glTFDrive.axis] = *glTFDrive.positionTarget;
									}
									if (glTFDrive.velocityTarget) {
										physicsJoint.targetLinearVelocity[glTFDrive.axis] = *glTFDrive.velocityTarget;
									}
									physicsJoint.linearDriveDamping[glTFDrive.axis] = glTFDrive.damping;
									break;
								case gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Type::ANGULAR:
									if (glTFDrive.mode == gltf::Asset::Extension::KHRPhysicsRigidBodies::Joint::Drive::Mode::ACCELERATION) {
										switch (glTFDrive.axis) {
											case 0: physicsJoint.driveIgnoresMomentOfInertiaX = true; break;
											case 1: physicsJoint.driveIgnoresMomentOfInertiaY = true; break;
											case 2: physicsJoint.driveIgnoresMomentOfInertiaZ = true; break;
											default: unreachable();
										}
									}
									if (glTFDrive.maxForce) {
										physicsJoint.maxTorque[glTFDrive.axis] = *glTFDrive.maxForce;
									}
									if (glTFDrive.positionTarget) {
										physicsJoint.targetAngles[glTFDrive.axis] = *glTFDrive.positionTarget;
									}
									if (glTFDrive.velocityTarget) {
										physicsJoint.targetAngularVelocity[glTFDrive.axis] = *glTFDrive.velocityTarget;
									}
									physicsJoint.angularDriveDamping[glTFDrive.axis] = glTFDrive.damping;
									break;
							}
						}
						model.physicsJoints[physicsJointIndex] = physicsJoint;
						++physicsJointIndex;
					}
				}
			}
		});
	});
}

class OBJModelLoader {
public:
	OBJModelLoader(Model& model, const obj::Asset& objAsset, const obj::mtl::Library& materialLibrary,
		FunctionView<Model::Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage, const ModelOptions& options)
		: model(model)
		, objAsset(objAsset)
		, materialLibrary(materialLibrary)
		, loadImage(loadImage)
		, options(options) {}

	void load() && {
		GREM_PROFILE_FUNCTION();

		allocateMeshes();
		allocateTextures();
		allocateMaterials();
		allocateNodes();
		loadMeshes();
		loadTextures();
		loadMaterials();
		loadNodes();
	}

private:
	void allocateMeshes();
	void allocateTextures();
	void allocateMaterials();
	void allocateNodes();
	void loadMeshes();
	void loadTextures();
	void loadMaterials();
	void loadNodes();

	Model& model;
	const obj::Asset& objAsset;
	const obj::mtl::Library& materialLibrary;
	FunctionView<Model::Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage;
	ModelOptions options;
	HashMap<StringView, size_t> textureIndexMap{};
	size_t nextTextureIndex = 0;
};

void OBJModelLoader::allocateMeshes() {
	GREM_PROFILE_FUNCTION();

	size_t totalMeshCount = 0;
	for (const obj::Node& objNode : objAsset.nodes) {
		totalMeshCount += objNode.meshes.size();
	}

	if (totalMeshCount >= static_cast<size_t>(Limits<Model::MeshCount>::MAX)) {
		throw std::length_error{"Too many meshes in asset."};
	}
	model.meshes.resize(totalMeshCount);
}

void OBJModelLoader::allocateTextures() {
	GREM_PROFILE_FUNCTION();

	for (const obj::Node& objNode : objAsset.nodes) {
		for (const obj::Mesh& objMesh : objNode.meshes) {
			if (!objMesh.materialName.empty()) {
				if (const auto it = findBy<&obj::mtl::Material::name>(materialLibrary.materials, objMesh.materialName); it != materialLibrary.materials.end()) {
					const obj::mtl::Material& objMaterial = *it;
					if (!objMaterial.diffuseMapName.empty()) {
						if (textureIndexMap.try_emplace(objMaterial.diffuseMapName, nextTextureIndex).second) {
							++nextTextureIndex;
						}
					}
					if (!objMaterial.occlusionRoughnessMetallicMapName.empty()) {
						if (textureIndexMap.try_emplace(objMaterial.occlusionRoughnessMetallicMapName, nextTextureIndex).second) {
							++nextTextureIndex;
						}
					}
					if (!objMaterial.ambientMapName.empty()) {
						if (objMaterial.occlusionRoughnessMetallicMapName.empty()) {
							if (textureIndexMap.try_emplace(objMaterial.ambientMapName, nextTextureIndex).second) {
								++nextTextureIndex;
							}
						} else if (objMaterial.ambientMapName != objMaterial.occlusionRoughnessMetallicMapName) {
							throw resource::Error{"Occlusion texture and metallic-roughness texture must be packed into a single texture."};
						}
					}
					if (!objMaterial.normalMapName.empty()) {
						if (textureIndexMap.try_emplace(objMaterial.normalMapName, nextTextureIndex).second) {
							++nextTextureIndex;
						}
					} else if (!objMaterial.bumpMapName.empty()) {
						if (textureIndexMap.try_emplace(objMaterial.bumpMapName, nextTextureIndex).second) {
							++nextTextureIndex;
						}
					}
					if (!objMaterial.emissiveMapName.empty()) {
						if (textureIndexMap.try_emplace(objMaterial.emissiveMapName, nextTextureIndex).second) {
							++nextTextureIndex;
						}
					}
				}
			}
		}
	}
	model.textures.resize(nextTextureIndex);
}

void OBJModelLoader::allocateMaterials() {
	GREM_PROFILE_FUNCTION();

	model.materials.resize(model.meshes.size());
}

void OBJModelLoader::allocateNodes() {
	GREM_PROFILE_FUNCTION();

	model.instances.resize(model.meshes.size());
}

void OBJModelLoader::loadMeshes() {
	GREM_PROFILE_FUNCTION();

	struct FaceVertexHash {
		[[nodiscard]] size_t operator()(const obj::FaceVertex& faceVertex) const {
			return getHash(faceVertex.vertexIndex, faceVertex.textureCoordinateIndex, faceVertex.normalIndex);
		}
	};

	struct FaceVertexEqual {
		[[nodiscard]] bool operator()(const obj::FaceVertex& a, const obj::FaceVertex& b) const {
			return a.vertexIndex == b.vertexIndex && a.textureCoordinateIndex == b.textureCoordinateIndex && a.normalIndex == b.normalIndex;
		}
	};

	HashMap<obj::FaceVertex, uint32_t, FaceVertexHash, FaceVertexEqual> vertexMap{};
	Buffer<byte> meshData{};
	Buffer<uint32_t> indices{};
	Buffer<vec3> positions{};
	Buffer<iA2B10G10R10vec4norm> normals{};
	Buffer<vec2> textureCoordinates{};

	Model::MeshIndex meshIndex = 0;
	for (const obj::Node& objNode : objAsset.nodes) {
		for (const obj::Mesh& objMesh : objNode.meshes) {
			vertexMap.clear();
			indices.clear();
			positions.clear();
			normals.clear();
			textureCoordinates.clear();

			bool needsNormalsGenerated = false;
			for (const obj::Face& objFace : objMesh.faces) {
				if (objFace.vertices.size() >= 3) {
					for (size_t faceVertexIndex = 1; faceVertexIndex + 1 < objFace.vertices.size(); ++faceVertexIndex) {
						for (const size_t faceVertexIndex : {size_t{0}, faceVertexIndex, faceVertexIndex + 1}) {
							const obj::FaceVertex& faceVertex = objFace.vertices[faceVertexIndex];
							size_t vertexIndex = positions.size();
							if (const auto [it, inserted] = vertexMap.emplace(faceVertex, vertexIndex); inserted) {
								positions.push_back((faceVertex.vertexIndex < objAsset.vertices.size()) ? objAsset.vertices[faceVertex.vertexIndex] : vec3{0.0f, 0.0f, 0.0f});
								if (faceVertex.normalIndex < objAsset.normals.size()) {
									normals.push_back(iA2B10G10R10vec4norm{objAsset.normals[faceVertex.normalIndex], 0.0f});
								} else {
									needsNormalsGenerated = true;
								}
								textureCoordinates.push_back((faceVertex.textureCoordinateIndex < objAsset.textureCoordinates.size())
																 ? objAsset.textureCoordinates[faceVertex.textureCoordinateIndex]
																 : vec2{});
							} else {
								vertexIndex = it->second;
							}
							indices.push_back(static_cast<uint32_t>(vertexIndex));
						}
					}
				}
			}

			const size_t indexCount = indices.size();
			const size_t vertexCount = positions.size();
			GREM_ASSERT(needsNormalsGenerated || normals.size() == vertexCount);
			GREM_ASSERT(textureCoordinates.size() == vertexCount);

			if (indexCount > static_cast<size_t>(Limits<Model::IndexCount>::MAX)) {
				throw std::length_error{"Too many indices in mesh."};
			}
			if (vertexCount > static_cast<size_t>(Limits<Model::VertexCount>::MAX)) {
				throw std::length_error{"Too many vertices in mesh."};
			}

			Box<3, float> boundingBox{};
			float boundingRadius = 0.0f;
			if (!positions.empty()) {
				boundingBox = {.min{vec3{positions.front()}}, .max{vec3{positions.front()}}};
				float boundingRadiusSquared = length2(vec3{positions.front()});
				for (const vec3 position : Span{positions}.subspan(1)) {
					boundingBox.min = min(boundingBox.min, position);
					boundingBox.max = max(boundingBox.max, position);
					boundingRadiusSquared = max(boundingRadiusSquared, length2(position));
				}
				boundingRadius = sqrt(boundingRadiusSquared);
			}

			GREM_ASSERT(meshData.size() % 4 == 0);

			model.meshes[meshIndex] = {
				.primitiveType = Model::PrimitiveType::TRIANGLES,
				.indexCount = static_cast<Model::IndexCount>(indexCount),
				.vertexCount = static_cast<Model::VertexCount>(vertexCount),
				.morphTargetCount = 0,
				.morphedVertexStride = 0,
				.meshDataOffset = static_cast<Model::ValueOffset>(meshData.size() / 4),
				.morphTargetDataOffset = 0,
				.boundingBox = boundingBox,
				.boundingRadius = boundingRadius,
				.vertexFlags = Model::VERTEX_TEXTURED_ON_CHANNEL_0,
			};

			if (needsNormalsGenerated) {
				normals.resize(vertexCount);
				Model::generateTriangleNormals(normals, positions, indices);
			}

			const size_t indicesByteOffset = meshData.size();
			const size_t indicesByteSize = indexCount * sizeof(uint32_t);
			const size_t positionsByteOffset = indicesByteOffset + indicesByteSize;
			const size_t positionsByteSize = vertexCount * sizeof(vec3);
			const size_t normalsByteOffset = positionsByteOffset + positionsByteSize;
			const size_t normalsByteSize = vertexCount * sizeof(iA2B10G10R10vec4norm);
			const size_t tangentsByteOffset = normalsByteOffset + normalsByteSize;
			const size_t tangentsByteSize = vertexCount * sizeof(iA2B10G10R10vec4norm);
			const size_t textureCoordinatesByteOffset = tangentsByteOffset + tangentsByteSize;
			const size_t textureCoordinatesByteSize = vertexCount * sizeof(vec2);
			const size_t meshDataByteSize = textureCoordinatesByteOffset + textureCoordinatesByteSize;
			meshData.resize(meshData.size() + meshDataByteSize);
			if (indicesByteSize > 0) {
				memcpy(meshData.data() + indicesByteOffset, indices.data(), indicesByteSize);
			}
			if (positionsByteSize > 0) {
				memcpy(meshData.data() + positionsByteOffset, positions.data(), positionsByteSize);
			}
			if (normalsByteSize > 0) {
				memcpy(meshData.data() + normalsByteOffset, normals.data(), normalsByteSize);
			}
			Model::generateTriangleTangents(StridedSpan{meshData.data() + tangentsByteOffset, vertexCount, sizeof(iA2B10G10R10vec4norm)}, positions, normals, textureCoordinates,
				indices);
			if (textureCoordinatesByteSize > 0) {
				memcpy(meshData.data() + textureCoordinatesByteOffset, textureCoordinates.data(), textureCoordinatesByteSize);
			}

			++meshIndex;
		}
	}

	GREM_ASSERT(meshData.size() % 4 == 0);
	if (meshData.size() / 4 > static_cast<size_t>(Limits<Model::ValueOffset>::MAX)) {
		throw std::length_error{"Too much mesh data in asset."};
	}

	model.meshData = std::move(meshData);

	if (!model.meshes.empty()) {
		model.bindPoseBoundingBox = model.meshes.front().boundingBox;
		model.bindPoseBoundingRadius = model.meshes.front().boundingRadius;
		for (const Model::Mesh& mesh : Span{model.meshes}.subspan(1)) {
			model.bindPoseBoundingBox.min = min(model.bindPoseBoundingBox.min, mesh.boundingBox.min);
			model.bindPoseBoundingBox.max = max(model.bindPoseBoundingBox.max, mesh.boundingBox.max);
			model.bindPoseBoundingRadius = max(model.bindPoseBoundingRadius, mesh.boundingRadius);
		}
	}
}

void OBJModelLoader::loadTextures() {
	GREM_PROFILE_FUNCTION();

	for (const obj::Node& objNode : objAsset.nodes) {
		for (const obj::Mesh& objMesh : objNode.meshes) {
			if (!objMesh.materialName.empty()) {
				if (const auto it = findBy<&obj::mtl::Material::name>(materialLibrary.materials, objMesh.materialName); it != materialLibrary.materials.end()) {
					const obj::mtl::Material& objMaterial = *it;
					if (!objMaterial.diffuseMapName.empty()) {
						model.textures[textureIndexMap.at(objMaterial.diffuseMapName)] = {
							.image = loadImage(objMaterial.diffuseMapName, ImageOptions{.requiredType = ImageType::IMAGE_2D}),
							.minificationFilter = Model::MinificationFilter::LINEAR_MIPMAP_LINEAR,
							.magnificationFilter = Model::MagnificationFilter::LINEAR,
							.horizontalWrappingMode = Model::WrappingMode::REPEAT,
							.verticalWrappingMode = Model::WrappingMode::REPEAT,
						};
					}
					if (!objMaterial.occlusionRoughnessMetallicMapName.empty()) {
						model.textures[textureIndexMap.at(objMaterial.occlusionRoughnessMetallicMapName)] = {
							.image = loadImage(objMaterial.occlusionRoughnessMetallicMapName, ImageOptions{.requiredType = ImageType::IMAGE_2D}),
							.minificationFilter = Model::MinificationFilter::LINEAR_MIPMAP_LINEAR,
							.magnificationFilter = Model::MagnificationFilter::LINEAR,
							.horizontalWrappingMode = Model::WrappingMode::REPEAT,
							.verticalWrappingMode = Model::WrappingMode::REPEAT,
						};
					}
					if (!objMaterial.ambientMapName.empty() && objMaterial.occlusionRoughnessMetallicMapName.empty()) {
						model.textures[textureIndexMap.at(objMaterial.ambientMapName)] = {
							.image = loadImage(objMaterial.ambientMapName, ImageOptions{.requiredType = ImageType::IMAGE_2D}),
							.minificationFilter = Model::MinificationFilter::LINEAR_MIPMAP_LINEAR,
							.magnificationFilter = Model::MagnificationFilter::LINEAR,
							.horizontalWrappingMode = Model::WrappingMode::REPEAT,
							.verticalWrappingMode = Model::WrappingMode::REPEAT,
						};
					}
					if (!objMaterial.normalMapName.empty()) {
						model.textures[textureIndexMap.at(objMaterial.normalMapName)] = {
							.image = loadImage(objMaterial.normalMapName, ImageOptions{.requiredType = ImageType::IMAGE_2D}),
							.minificationFilter = Model::MinificationFilter::LINEAR_MIPMAP_LINEAR,
							.magnificationFilter = Model::MagnificationFilter::LINEAR,
							.horizontalWrappingMode = Model::WrappingMode::REPEAT,
							.verticalWrappingMode = Model::WrappingMode::REPEAT,
						};
					} else if (!objMaterial.bumpMapName.empty()) {
						model.textures[textureIndexMap.at(objMaterial.bumpMapName)] = {
							.image = loadImage(objMaterial.bumpMapName, ImageOptions{.requiredType = ImageType::IMAGE_2D}),
							.minificationFilter = Model::MinificationFilter::LINEAR_MIPMAP_LINEAR,
							.magnificationFilter = Model::MagnificationFilter::LINEAR,
							.horizontalWrappingMode = Model::WrappingMode::REPEAT,
							.verticalWrappingMode = Model::WrappingMode::REPEAT,
						};
					}
					if (!objMaterial.emissiveMapName.empty()) {
						model.textures[textureIndexMap.at(objMaterial.emissiveMapName)] = {
							.image = loadImage(objMaterial.emissiveMapName, ImageOptions{.requiredType = ImageType::IMAGE_2D}),
							.minificationFilter = Model::MinificationFilter::LINEAR_MIPMAP_LINEAR,
							.magnificationFilter = Model::MagnificationFilter::LINEAR,
							.horizontalWrappingMode = Model::WrappingMode::REPEAT,
							.verticalWrappingMode = Model::WrappingMode::REPEAT,
						};
					}
				}
			}
		}
	}
}

void OBJModelLoader::loadMaterials() {
	GREM_PROFILE_FUNCTION();

	Model::MaterialIndex materialIndex = 0;

	for (const obj::Node& objNode : objAsset.nodes) {
		for (const obj::Mesh& objMesh : objNode.meshes) {
			Model::Material material{
				.materialType = Model::MaterialType::METALLIC_ROUGHNESS,
				.baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f},
				.occlusionStrength = 1.0f,
				.roughnessFactor = 1.0f,
				.metallicFactor = 0.0f,
				.normalScale = 1.0f,
				.emissiveFactor{0.0f, 0.0f, 0.0f},
				.alphaCutoff = 0.5f,
				.indexOfRefraction = 1.5f,
				.baseColorMap{
					.textureIndex = static_cast<Model::TextureIndex>(model.textures.size()),
					.textureOffset{0.0f, 0.0f},
					.textureBasis{1.0f},
				},
				.occlusionRoughnessMetallicMap{
					.textureIndex = static_cast<Model::TextureIndex>(model.textures.size()),
					.textureOffset{0.0f, 0.0f},
					.textureBasis{1.0f},
				},
				.normalMap{
					.textureIndex = static_cast<Model::TextureIndex>(model.textures.size()),
					.textureOffset{0.0f, 0.0f},
					.textureBasis{1.0f},
				},
				.emissiveMap{
					.textureIndex = static_cast<Model::TextureIndex>(model.textures.size()),
					.textureOffset{0.0f, 0.0f},
					.textureBasis{1.0f},
				},
				.fragmentFlags{},
			};

			if (!objMesh.materialName.empty()) {
				if (const auto it = findBy<&obj::mtl::Material::name>(materialLibrary.materials, objMesh.materialName); it != materialLibrary.materials.end()) {
					const obj::mtl::Material& objMaterial = *it;

					material.baseColorFactor = vec4{objMaterial.diffuseColor, clamp(objMaterial.dissolveFactor, 0.0f, 1.0f)};
					material.emissiveFactor = objMaterial.emissiveColor;
					material.occlusionStrength =
						(objMaterial.occlusionRoughnessMetallicMapName.empty())
							? clamp(objMaterial.ambientColor.x * objMaterial.ambientColor.y * objMaterial.ambientColor.z, 0.0f, 1.0f)
							: 1.0f;
					material.roughnessFactor =
						(objMaterial.roughnessFactor) ? *objMaterial.roughnessFactor
						: (objMaterial.occlusionRoughnessMetallicMapName.empty())
							? 1.0f - clamp(0.25f * pow(objMaterial.specularExponent, 0.2f), 0.0f, 1.0f)
							: 1.0f;
					material.metallicFactor = (objMaterial.metallicFactor) ? *objMaterial.metallicFactor : (objMaterial.occlusionRoughnessMetallicMapName.empty()) ? 0.0f : 1.0f;

					if (!objMaterial.diffuseMapName.empty()) {
						material.baseColorMap.textureIndex = textureIndexMap.at(objMaterial.diffuseMapName);
						const ImageView image = match(model.textures[material.baseColorMap.textureIndex].image)([](const auto& image) -> ImageView { return image; });
						if (isPotentiallyTransparentImageFormat(image.getFormat()) &&
							(objMaterial.illuminationModel == obj::mtl::IlluminationModel::BLINN_PHONG_RAYTRACE_GLASS ||
								objMaterial.illuminationModel == obj::mtl::IlluminationModel::BLINN_PHONG_REFLECT_GLASS)) {
							material.fragmentFlags |= Model::FRAGMENT_ALPHA_BLENDED;
						}
						material.fragmentFlags |= Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0;
					}
					if (!objMaterial.occlusionRoughnessMetallicMapName.empty()) {
						material.occlusionRoughnessMetallicMap.textureIndex = textureIndexMap.at(objMaterial.occlusionRoughnessMetallicMapName);
						material.fragmentFlags |= Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0;
					}
					if (!objMaterial.ambientMapName.empty()) {
						if (objMaterial.occlusionRoughnessMetallicMapName.empty()) {
							material.occlusionRoughnessMetallicMap.textureIndex = textureIndexMap.at(objMaterial.ambientMapName);
						}
						material.fragmentFlags |= Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0;
					}
					if (!objMaterial.normalMapName.empty()) {
						material.normalMap.textureIndex = textureIndexMap.at(objMaterial.normalMapName);
						material.fragmentFlags |= Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0;
					} else if (!objMaterial.bumpMapName.empty()) {
						material.normalMap.textureIndex = textureIndexMap.at(objMaterial.bumpMapName);
						material.fragmentFlags |= Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0;
					}
					if (!objMaterial.emissiveMapName.empty()) {
						material.emissiveMap.textureIndex = textureIndexMap.at(objMaterial.emissiveMapName);
						material.fragmentFlags |= Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0;
					}
					if (material.baseColorFactor.w != 1.0f) {
						material.fragmentFlags |= Model::FRAGMENT_ALPHA_BLENDED;
					}
				}
			}

			model.materials[materialIndex] = material;

			++materialIndex;
		}
	}
}

void OBJModelLoader::loadNodes() {
	GREM_PROFILE_FUNCTION();

	Model::MeshIndex meshIndex = 0;
	Model::MaterialIndex materialIndex = 0;
	Model::InstanceIndex instanceIndex = 0;

	model.bindPose.localJoints = {{
		.translation = options.rootTranslation,
		.rotation = options.rootRotation,
		.scale = options.rootScale,
		.visible = true,
	}};
	model.jointParentIndices = {0};
	model.jointColliders = {nullopt};
	model.jointPhysicsObjectIndices = {Limits<Model::PhysicsObjectIndex>::MAX};
	model.staticJointCount = 1;
	for (const obj::Node& objNode : objAsset.nodes) {
		for (const obj::Mesh& objMesh : objNode.meshes) {
			(void)objMesh;

			model.instances[instanceIndex] = {
				.materialIndex = materialIndex,
				.meshIndex = meshIndex,
				.skinDataOffset = 0,
				.morphTargetWeightOffset = 0,
				.jointIndex = 0,
				.instanceFlags{},
			};

			++meshIndex;
			++materialIndex;
			++instanceIndex;
		}
	}
}

} // namespace

Model::PoseReference Model::PoseReference::applyAnimation(const AnimationState& animationState) const {
	GREM_ASSERT(localJoints);
	GREM_ASSERT(localMorphTargetWeights);
	const AnimationView animation = animationState.animation;
	float animationTime = 0.0f;
	if (animationState.looping) {
		const Duration maxTimePoint = duration_cast<Duration>(FloatSeconds{animation.maxTimePoint});
		if (maxTimePoint > Duration{}) {
			animationTime = duration_cast<FloatSeconds>(animationState.time % maxTimePoint).count();
		}
	} else {
		animationTime = duration_cast<FloatSeconds>(animationState.time).count();
	}
	for (const AnimationChannel& channel : animation.channels) {
		GREM_ASSERT(channel.keyframeCount > 0);
		const Span<const float> keyframeInputTimePoints = animation.keyframeInputTimePoints.subspan(channel.keyframeInputTimePointOffset, channel.keyframeCount);
		const byte* const keyframeOutputValues = animation.keyframeOutputValueData.data() + static_cast<size_t>(channel.keyframeOutputValueOffset) * 4;
		size_t nextKeyframeIndex =
			min(static_cast<size_t>(upperBound(keyframeInputTimePoints, animationTime) - keyframeInputTimePoints.begin()), keyframeInputTimePoints.size() - 1);
		size_t keyframeIndex{};
		if (animationState.looping) {
			keyframeIndex = (nextKeyframeIndex == 0) ? keyframeInputTimePoints.size() - 1 : nextKeyframeIndex - 1;
		} else {
			if (animationTime > animation.maxTimePoint) {
				keyframeIndex = nextKeyframeIndex;
			} else {
				keyframeIndex = (nextKeyframeIndex == 0) ? 0 : nextKeyframeIndex - 1;
			}
		}
		switch (channel.interpolationMode) {
			case InterpolationMode::LINEAR: {
				const float keyframeDuration = keyframeInputTimePoints[nextKeyframeIndex] - keyframeInputTimePoints[keyframeIndex];
				const float alpha = (keyframeIndex == nextKeyframeIndex) ? 0.0f : (animationTime - keyframeInputTimePoints[keyframeIndex]) / keyframeDuration;
				switch (channel.targetPath) {
					case AnimationPath::JOINT_ROTATION: {
						const quat* const keyframeRotations = std::launder(reinterpret_cast<const quat*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const quat newValue = slerp(keyframeRotations[keyframeIndex], keyframeRotations[nextKeyframeIndex], alpha);
						localJoints[channel.targetOffset].rotation = slerp(localJoints[channel.targetOffset].rotation, newValue, blendWeight);
						break;
					}
					case AnimationPath::JOINT_SCALE: {
						const vec3* const keyframeScales = std::launder(reinterpret_cast<const vec3*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const vec3 newValue = mix(keyframeScales[keyframeIndex], keyframeScales[nextKeyframeIndex], alpha);
						localJoints[channel.targetOffset].scale = mix(localJoints[channel.targetOffset].scale, newValue, blendWeight);
						break;
					}
					case AnimationPath::JOINT_TRANSLATION: {
						const vec3* const keyframeTranslations = std::launder(reinterpret_cast<const vec3*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const vec3 newValue = mix(keyframeTranslations[keyframeIndex], keyframeTranslations[nextKeyframeIndex], alpha);
						localJoints[channel.targetOffset].translation = mix(localJoints[channel.targetOffset].translation, newValue, blendWeight);
						break;
					}
					case AnimationPath::MORPH_TARGET_WEIGHTS: {
						const float* const keyframeMorphTargetWeights = std::launder(reinterpret_cast<const float*>(keyframeOutputValues));
						for (size_t i = 0; i < channel.targetMorphTargetWeightCount; ++i) {
							const float blendWeight =
								animationState.blendWeight *
								((animationState.morphTargetWeightBlendWeights.empty()) ? 1.0f : animationState.morphTargetWeightBlendWeights[channel.targetOffset + i]);
							const float newValue = mix(keyframeMorphTargetWeights[keyframeIndex * channel.targetMorphTargetWeightCount + i],
								keyframeMorphTargetWeights[nextKeyframeIndex * channel.targetMorphTargetWeightCount + i], alpha);
							localMorphTargetWeights[channel.targetOffset + i] = mix(localMorphTargetWeights[channel.targetOffset + i], newValue, blendWeight);
						}
						break;
					}
				}
				break;
			}
			case InterpolationMode::STEP: {
				switch (channel.targetPath) {
					case AnimationPath::JOINT_ROTATION: {
						const quat* const keyframeRotations = std::launder(reinterpret_cast<const quat*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const quat newValue = keyframeRotations[keyframeIndex];
						localJoints[channel.targetOffset].rotation = slerp(localJoints[channel.targetOffset].rotation, newValue, blendWeight);
						break;
					}
					case AnimationPath::JOINT_SCALE: {
						const vec3* const keyframeScales = std::launder(reinterpret_cast<const vec3*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const vec3 newValue = keyframeScales[keyframeIndex];
						localJoints[channel.targetOffset].scale = mix(localJoints[channel.targetOffset].scale, newValue, blendWeight);
						break;
					}
					case AnimationPath::JOINT_TRANSLATION: {
						const vec3* const keyframeTranslations = std::launder(reinterpret_cast<const vec3*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const vec3 newValue = keyframeTranslations[keyframeIndex];
						localJoints[channel.targetOffset].translation = mix(localJoints[channel.targetOffset].translation, newValue, blendWeight);
						break;
					}
					case AnimationPath::MORPH_TARGET_WEIGHTS: {
						const float* const keyframeMorphTargetWeights = std::launder(reinterpret_cast<const float*>(keyframeOutputValues));
						for (size_t i = 0; i < channel.targetMorphTargetWeightCount; ++i) {
							const float blendWeight =
								animationState.blendWeight *
								((animationState.morphTargetWeightBlendWeights.empty()) ? 1.0f : animationState.morphTargetWeightBlendWeights[channel.targetOffset + i]);
							const float newValue = keyframeMorphTargetWeights[keyframeIndex * channel.targetMorphTargetWeightCount + i];
							localMorphTargetWeights[channel.targetOffset + i] = mix(localMorphTargetWeights[channel.targetOffset + i], newValue, blendWeight);
						}
						break;
					}
				}
				break;
			}
			case InterpolationMode::CUBIC_SPLINE: {
				constexpr auto cubicSpline = [](const auto& keyframeValues, size_t keyframeIndex, size_t nextKeyframeIndex, float keyframeDuration, float alpha) {
					const auto value = keyframeValues[keyframeIndex * 3 + 1];
					const auto outTangent = keyframeValues[keyframeIndex * 3 + 2];
					const auto nextInTangent = keyframeValues[nextKeyframeIndex * 3];
					const auto nextValue = keyframeValues[nextKeyframeIndex * 3 + 1];
					const float alphaSquared = alpha * alpha;
					const float alphaCubed = alpha * alpha * alpha;
					return (2.0f * alphaCubed - 3.0f * alphaSquared + 1.0f) * value +                   //
					       keyframeDuration * (alphaCubed - 2.0f * alphaSquared + alpha) * outTangent + //
					       (-2.0f * alphaCubed + 3.0f * alphaSquared) * nextValue +                     //
					       keyframeDuration * (alphaCubed - alphaSquared) * nextInTangent;
				};
				const float keyframeDuration = keyframeInputTimePoints[nextKeyframeIndex] - keyframeInputTimePoints[keyframeIndex];
				const float alpha = (keyframeIndex == nextKeyframeIndex) ? 0.0f : (animationTime - keyframeInputTimePoints[keyframeIndex]) / keyframeDuration;
				switch (channel.targetPath) {
					case AnimationPath::JOINT_ROTATION: {
						const quat* const keyframeRotations = std::launder(reinterpret_cast<const quat*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const quat newValue = normalize(cubicSpline(keyframeRotations, keyframeIndex, nextKeyframeIndex, keyframeDuration, alpha));
						localJoints[channel.targetOffset].rotation = slerp(localJoints[channel.targetOffset].rotation, newValue, blendWeight);
						break;
					}
					case AnimationPath::JOINT_SCALE: {
						const vec3* const keyframeScales = std::launder(reinterpret_cast<const vec3*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const vec3 newValue = cubicSpline(keyframeScales, keyframeIndex, nextKeyframeIndex, keyframeDuration, alpha);
						localJoints[channel.targetOffset].scale = mix(localJoints[channel.targetOffset].scale, newValue, blendWeight);
						break;
					}
					case AnimationPath::JOINT_TRANSLATION: {
						const vec3* const keyframeTranslations = std::launder(reinterpret_cast<const vec3*>(keyframeOutputValues));
						const float blendWeight =
							animationState.blendWeight * ((animationState.jointBlendWeights.empty()) ? 1.0f : animationState.jointBlendWeights[channel.targetOffset]);
						const vec3 newValue = cubicSpline(keyframeTranslations, keyframeIndex, nextKeyframeIndex, keyframeDuration, alpha);
						localJoints[channel.targetOffset].translation = mix(localJoints[channel.targetOffset].translation, newValue, blendWeight);
						break;
					}
					case AnimationPath::MORPH_TARGET_WEIGHTS: {
						const float* const keyframeMorphTargetWeights = std::launder(reinterpret_cast<const float*>(keyframeOutputValues));
						for (size_t i = 0; i < channel.targetMorphTargetWeightCount; ++i) {
							const float blendWeight =
								animationState.blendWeight *
								((animationState.morphTargetWeightBlendWeights.empty()) ? 1.0f : animationState.morphTargetWeightBlendWeights[channel.targetOffset + i]);
							const float newValue = cubicSpline(keyframeMorphTargetWeights, keyframeIndex * channel.targetMorphTargetWeightCount + i,
								nextKeyframeIndex * channel.targetMorphTargetWeightCount + i, keyframeDuration, alpha);
							localMorphTargetWeights[channel.targetOffset + i] = mix(localMorphTargetWeights[channel.targetOffset + i], newValue, blendWeight);
						}
						break;
					}
				}
				break;
			}
		}
	}
	return *this;
}

void Model::TransformationReference::pose(Span<const Joint> localJoints, Span<const float> localMorphTargetWeights, Span<const JointIndex> modelJointParentIndices) const {
	GREM_ASSERT(jointMatrices);
	GREM_ASSERT(jointsVisible);
	GREM_ASSERT(morphTargetWeights || localMorphTargetWeights.empty());
	GREM_ASSERT(!localJoints.empty());
	GREM_ASSERT(localJoints.size() == modelJointParentIndices.size());

	for (size_t i = 0; i < localJoints.size(); ++i) {
		const JointIndex jointParentIndex = modelJointParentIndices[i];
		const Joint& joint = localJoints[i];
		jointMatrices[i] = jointMatrices[jointParentIndex] * translateRotateScale(joint.translation, joint.rotation, joint.scale);
		jointsVisible[i] = jointsVisible[jointParentIndex] && joint.visible;
	}

	for (size_t i = 0; i < localMorphTargetWeights.size(); ++i) {
		morphTargetWeights[i] += localMorphTargetWeights[i];
	}
}

void Model::generateTriangleNormals(StridedSpan<byte> normalData, StridedSpan<const vec3> positions, StridedSpan<const uint32_t> indices) {
	GREM_PROFILE_FUNCTION();

	constexpr auto addTriangleNormalInfluence = [](Span<vec3> normals, StridedSpan<const vec3> positions, size_t indexA, size_t indexB, size_t indexC) -> void {
		const vec3 ab = positions[indexB] - positions[indexA];
		const vec3 ac = positions[indexC] - positions[indexA];
		const vec3 bc = positions[indexC] - positions[indexB];

		const vec3 doubleTriangleAreaScaledNormalDirection = cross(ab, ac);

		const float squaredLengthAB = length2(ab);
		const float squaredLengthAC = length2(ac);
		const float squaredLengthBC = length2(bc);

		const float lengthAB = (squaredLengthAB >= 1e-6f) ? sqrt(squaredLengthAB) : 1e-3f;
		const float lengthAC = (squaredLengthAC >= 1e-6f) ? sqrt(squaredLengthAC) : 1e-3f;
		const float lengthBC = (squaredLengthBC >= 1e-6f) ? sqrt(squaredLengthBC) : 1e-3f;

		const float influenceA = dot(ab, ac) / (lengthAB * lengthAC);
		const float influenceB = -dot(ab, bc) / (lengthAB * lengthBC);
		const float influenceC = dot(ac, bc) / (lengthAC * lengthBC);

		const float angleA = (influenceA >= 1.0f) ? 0.0f : (influenceA <= -1.0f) ? numbers::PI : acos(influenceA);
		const float angleB = (influenceB >= 1.0f) ? 0.0f : (influenceB <= -1.0f) ? numbers::PI : acos(influenceB);
		const float angleC = (influenceC >= 1.0f) ? 0.0f : (influenceC <= -1.0f) ? numbers::PI : acos(influenceC);

		normals[indexA] += doubleTriangleAreaScaledNormalDirection * angleA;
		normals[indexB] += doubleTriangleAreaScaledNormalDirection * angleB;
		normals[indexC] += doubleTriangleAreaScaledNormalDirection * angleC;
	};

	const size_t vertexCount = positions.size();
	GREM_ASSERT(normalData.size() == vertexCount);
	GREM_ASSERT(normalData.stride() >= sizeof(iA2B10G10R10vec4norm));

	if (vertexCount == 0) {
		return;
	}

	Allocation<vec3> generatedNormals(vertexCount, vec3{});
	if (indices.empty()) {
		for (size_t triangleIndex = 0; triangleIndex + 3 <= vertexCount; triangleIndex += 3) {
			const size_t indexA = triangleIndex;
			const size_t indexB = triangleIndex + 1;
			const size_t indexC = triangleIndex + 2;

			addTriangleNormalInfluence(generatedNormals, positions, indexA, indexB, indexC);
		}
	} else {
		for (size_t triangleIndex = 0; triangleIndex + 3 <= indices.size(); triangleIndex += 3) {
			const size_t indexA = static_cast<size_t>(indices[triangleIndex]);
			const size_t indexB = static_cast<size_t>(indices[triangleIndex + 1]);
			const size_t indexC = static_cast<size_t>(indices[triangleIndex + 2]);

			addTriangleNormalInfluence(generatedNormals, positions, indexA, indexB, indexC);
		}
	}

	for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
		const iA2B10G10R10vec4norm generatedNormal{normalize(generatedNormals[vertexIndex]), 0.0f};
		memcpy(reinterpret_cast<iA2B10G10R10vec4norm*>(normalData.base() + vertexIndex * normalData.stride()), &generatedNormal, sizeof(iA2B10G10R10vec4norm));
	}
}

void Model::generateTriangleTangents(StridedSpan<byte> tangentData, StridedSpan<const vec3> positions, StridedSpan<const iA2B10G10R10vec4norm> normals,
	StridedSpan<const vec2> textureCoordinates, StridedSpan<const uint32_t> indices) {
	GREM_PROFILE_FUNCTION();

	constexpr auto addTriangleTangentInfluence = [](Span<vec3> tangents, Span<vec3> bitangents, StridedSpan<const vec3> positions, StridedSpan<const iA2B10G10R10vec4norm> normals,
													 StridedSpan<const vec2> textureCoordinates, size_t indexA, size_t indexB, size_t indexC) -> void {
		const vec3 a = positions[indexA];
		const vec3 b = positions[indexB];
		const vec3 c = positions[indexC];
		const vec3 ab = b - a;
		const vec3 ac = c - a;
		const vec3 bc = c - b;

		if (textureCoordinates.empty()) {
			const Array horizontalSquares{
				length2(vec2{ab.x, ab.z}),
				length2(vec2{ac.x, ac.z}),
				length2(vec2{bc.x, bc.z}),
			};
			vec3 tangentDirection{};
			switch (maxElement(horizontalSquares) - horizontalSquares.begin()) {
				case 0: tangentDirection = ab; break;
				case 1: tangentDirection = ac; break;
				case 2: tangentDirection = bc; break;
				default: unreachable();
			}
			tangentDirection.y = 0.0f;
			vec3 bitangentDirection = cross(vec3{normals[indexA]}, tangentDirection);
			if (bitangentDirection.y < 0.0f) {
				tangentDirection = -tangentDirection;
				bitangentDirection = -bitangentDirection;
			}

			tangentDirection = tryNormalize(tangentDirection).value_or(vec3{});
			bitangentDirection = tryNormalize(bitangentDirection).value_or(vec3{});

			const float squaredLengthAB = length2(ab);
			const float squaredLengthAC = length2(ac);
			const float squaredLengthBC = length2(bc);

			const float lengthAB = (squaredLengthAB >= 1e-6f) ? sqrt(squaredLengthAB) : 1e-3f;
			const float lengthAC = (squaredLengthAC >= 1e-6f) ? sqrt(squaredLengthAC) : 1e-3f;
			const float lengthBC = (squaredLengthBC >= 1e-6f) ? sqrt(squaredLengthBC) : 1e-3f;

			const float influenceA = dot(ab, ac) / (lengthAB * lengthAC);
			const float influenceB = -dot(ab, bc) / (lengthAB * lengthBC);
			const float influenceC = dot(ac, bc) / (lengthAC * lengthBC);

			const float angleA = (influenceA >= 1.0f) ? 0.0f : (influenceA <= -1.0f) ? numbers::PI : acos(influenceA);
			const float angleB = (influenceB >= 1.0f) ? 0.0f : (influenceB <= -1.0f) ? numbers::PI : acos(influenceB);
			const float angleC = (influenceC >= 1.0f) ? 0.0f : (influenceC <= -1.0f) ? numbers::PI : acos(influenceC);

			tangents[indexA] += tangentDirection * angleA;
			tangents[indexB] += tangentDirection * angleB;
			tangents[indexC] += tangentDirection * angleC;

			bitangents[indexA] += bitangentDirection * angleA;
			bitangents[indexB] += bitangentDirection * angleB;
			bitangents[indexC] += bitangentDirection * angleC;
		} else {
			const vec2 uvA = textureCoordinates[indexA];
			const vec2 uvB = textureCoordinates[indexB];
			const vec2 uvC = textureCoordinates[indexC];
			const vec2 uvAB = uvB - uvA;
			const vec2 uvAC = uvC - uvA;

			const float signedDoubleUVArea = uvAB.y * uvAC.x - uvAB.x * uvAC.y;
			const float inverseSignedDoubleUVArea = (signedDoubleUVArea < 1e-6f) ? 1.0f : 1.0f / signedDoubleUVArea;
			const vec3 tangentDirection = inverseSignedDoubleUVArea * (ac * uvAB.y - ab * uvAC.y);
			const vec3 bitangentDirection = inverseSignedDoubleUVArea * (ac * uvAB.x - ab * uvAC.x);

			tangents[indexA] += tangentDirection;
			tangents[indexB] += tangentDirection;
			tangents[indexC] += tangentDirection;

			bitangents[indexA] += bitangentDirection;
			bitangents[indexB] += bitangentDirection;
			bitangents[indexC] += bitangentDirection;
		}
	};

	const size_t vertexCount = positions.size();
	GREM_ASSERT(tangentData.size() == vertexCount);
	GREM_ASSERT(tangentData.stride() >= sizeof(iA2B10G10R10vec4norm));
	GREM_ASSERT(normals.size() == vertexCount);
	GREM_ASSERT(textureCoordinates.empty() || textureCoordinates.size() == vertexCount);

	if (vertexCount == 0) {
		return;
	}

	Allocation<vec3> generatedTangentsAndBitangents(vertexCount * 2, vec3{0.0f, 0.0f, 0.0f});
	const Span<vec3> generatedTangents = Span{generatedTangentsAndBitangents}.first(vertexCount);
	const Span<vec3> generatedBitangents = Span{generatedTangentsAndBitangents}.subspan(vertexCount);

	if (indices.empty()) {
		GREM_ASSERT(vertexCount % 3 == 0);
		for (size_t triangleIndex = 0; triangleIndex + 3 <= positions.size(); triangleIndex += 3) {
			const size_t indexA = triangleIndex;
			const size_t indexB = triangleIndex + 1;
			const size_t indexC = triangleIndex + 2;
			addTriangleTangentInfluence(generatedTangents, generatedBitangents, positions, normals, textureCoordinates, indexA, indexB, indexC);
		}
	} else {
		GREM_ASSERT(indices.size() % 3 == 0);
		for (size_t triangleIndex = 0; triangleIndex + 3 <= indices.size(); triangleIndex += 3) {
			const size_t indexA = static_cast<size_t>(indices[triangleIndex]);
			const size_t indexB = static_cast<size_t>(indices[triangleIndex + 1]);
			const size_t indexC = static_cast<size_t>(indices[triangleIndex + 2]);
			addTriangleTangentInfluence(generatedTangents, generatedBitangents, positions, normals, textureCoordinates, indexA, indexB, indexC);
		}
	}

	for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
		const vec3 normal{normals[vertexIndex]};
		const vec3 tangent = generatedTangents[vertexIndex];
		const vec3 bitangent = generatedBitangents[vertexIndex];
		const vec3 orthogonalizedTangent = tangent - normal * dot(tangent, normal);
		const vec3 orthogonalizedBitangent = bitangent - normal * dot(bitangent, normal);
		const vec3 orthogonalizedXAxis = vec3{1.0f, 0.0f, 0.0f} - normal * normal.x;
		const vec3 orthogonalizedYAxis = vec3{0.0f, 1.0f, 0.0f} - normal * normal.y;
		const iA2B10G10R10vec4norm generatedTangent{
			tryNormalize(orthogonalizedTangent)
				.value_or(tryNormalize(orthogonalizedBitangent).value_or(tryNormalize(orthogonalizedXAxis).value_or(normalize(orthogonalizedYAxis)))),
			copysign(1.0f, dot(cross(normal, tangent), bitangent)),
		};
		memcpy(reinterpret_cast<iA2B10G10R10vec4norm*>(tangentData.base() + vertexIndex * tangentData.stride()), &generatedTangent, sizeof(iA2B10G10R10vec4norm));
	}
}

ModelFileType Model::determineFileType(Span<const byte> fileContents) noexcept {
	const StringView fileContentsAsString{reinterpret_cast<const char*>(fileContents.data()), fileContents.size()};
	if (fileContentsAsString.starts_with("glTF")) {
		return ModelFileType::GLTF_BINARY;
	}
	for (char32_t codePoint : unicode::UTF8View{fileContentsAsString}) {
		if (!json::isWhitespace(codePoint)) {
			if (codePoint == char32_t{'{'}) {
				return ModelFileType::GLTF;
			}
			break;
		}
	}
	const char* p = fileContentsAsString.data();
	const char* const end = fileContentsAsString.data() + min(fileContentsAsString.size(), size_t{256});
	while (p != end) {
		if (*p == '#') {
			do {
				++p;
			} while (p != end && *p != '\n');
			continue;
		}
		if (!ascii::isValidCharacter(*p)) {
			return ModelFileType::UNKNOWN;
		}
		++p;
	}
	return ModelFileType::OBJ;
}

Model::Model(StridedSpan<const vec3> positions, StridedSpan<const uint32_t> indices, StridedSpan<const iA2B10G10R10vec4norm> normals,
	StridedSpan<const iA2B10G10R10vec4norm> tangents, StridedSpan<const vec2> textureCoordinatesChannel0, StridedSpan<const vec2> textureCoordinatesChannel1,
	StridedSpan<const u8vec4norm> colors, const Optional<Material>& material) {
	assign(positions, indices, normals, tangents, textureCoordinatesChannel0, textureCoordinatesChannel1, colors, material);
}

Model::Model(const ConvexPolytope3D& polytope, const mat4& transformation) {
	assign(polytope, transformation);
}

Model::Model(const Filesystem& filesystem, CStringView filepath, const ModelOptions& options) {
	load(filesystem, filepath, options);
}

Model::Model(const gltf::Asset& asset, FunctionView<Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
	FunctionView<Variant<Allocation<byte>, Span<const byte>>(CStringView relativeFilepath)> loadBufferData, const ModelOptions& options) {
	load(asset, loadImage, loadBufferData, options);
}

Model::Model(const obj::Asset& asset, const obj::mtl::Library& materialLibrary, FunctionView<Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
	const ModelOptions& options) {
	load(asset, materialLibrary, loadImage, options);
}

void Model::assign(StridedSpan<const vec3> positions, StridedSpan<const uint32_t> indices, StridedSpan<const iA2B10G10R10vec4norm> normals,
	StridedSpan<const iA2B10G10R10vec4norm> tangents, StridedSpan<const vec2> textureCoordinatesChannel0, StridedSpan<const vec2> textureCoordinatesChannel1,
	StridedSpan<const u8vec4norm> colors, const Optional<Material>& material) {
	reset();

	GREM_PROFILE_FUNCTION();

	const size_t indexCount = indices.size();
	const size_t vertexCount = positions.size();
	GREM_ASSERT(indexCount % 3 == 0);
	GREM_ASSERT(allOf(indices, [&](uint32_t index) -> bool { return index < vertexCount; }));
	GREM_ASSERT(normals.empty() || normals.size() == vertexCount);
	GREM_ASSERT(tangents.empty() || tangents.size() == vertexCount);
	GREM_ASSERT(textureCoordinatesChannel0.empty() || textureCoordinatesChannel0.size() == vertexCount);
	GREM_ASSERT(textureCoordinatesChannel1.empty() || textureCoordinatesChannel1.size() == vertexCount);
	GREM_ASSERT(colors.empty() || colors.size() == vertexCount);

	if (indexCount > static_cast<size_t>(Limits<Model::IndexCount>::MAX)) {
		throw std::length_error{"Too many indices in mesh."};
	}
	if (vertexCount > static_cast<size_t>(Limits<Model::VertexCount>::MAX)) {
		throw std::length_error{"Too many vertices in mesh."};
	}

	meshData.resize(indexCount * sizeof(uint32_t) + vertexCount * (sizeof(vec3) + sizeof(iA2B10G10R10vec4norm) + sizeof(iA2B10G10R10vec4norm)) +
					textureCoordinatesChannel0.size() * sizeof(vec2) + textureCoordinatesChannel1.size() * sizeof(vec2) + colors.size() * sizeof(u8vec4norm));
	byte* meshDataPointer = meshData.data();

	writeValues(meshDataPointer, indices);
	meshDataPointer += indexCount * sizeof(uint32_t);

	Box<3, float> boundingBox{};
	float boundingRadius = 0.0f;
	VertexFlags vertexFlags{};
	if (vertexCount > 0) {
		writeValues(meshDataPointer, positions);
		meshDataPointer += vertexCount * sizeof(vec3);

		if (normals.empty()) {
			generateTriangleNormals(StridedSpan{meshDataPointer, vertexCount, sizeof(iA2B10G10R10vec4norm)}, positions, indices);
			normals = Span{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(meshDataPointer)), vertexCount};
		} else {
			writeValues(meshDataPointer, normals);
		}
		meshDataPointer += vertexCount * sizeof(iA2B10G10R10vec4norm);

		if (tangents.empty()) {
			const size_t normalTextureCoordinatesChannel = (material && (material->fragmentFlags & FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1) != 0) ? 1 : 0;
			const StridedSpan<const vec2> normalTextureCoordinates = (normalTextureCoordinatesChannel == 1) ? textureCoordinatesChannel1 : textureCoordinatesChannel0;
			generateTriangleTangents(StridedSpan{meshDataPointer, vertexCount, sizeof(iA2B10G10R10vec4norm)}, positions, normals, normalTextureCoordinates, indices);
			tangents = Span{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(meshDataPointer)), vertexCount};
		} else {
			writeValues(meshDataPointer, tangents);
		}
		meshDataPointer += vertexCount * sizeof(iA2B10G10R10vec4norm);

		if (!textureCoordinatesChannel0.empty()) {
			writeValues(meshDataPointer, textureCoordinatesChannel0);
			meshDataPointer += vertexCount * sizeof(vec2);
			vertexFlags |= VERTEX_TEXTURED_ON_CHANNEL_0;
		}

		if (!textureCoordinatesChannel1.empty()) {
			writeValues(meshDataPointer, textureCoordinatesChannel1);
			meshDataPointer += vertexCount * sizeof(vec2);
			vertexFlags |= VERTEX_TEXTURED_ON_CHANNEL_1;
		}

		if (!colors.empty()) {
			writeValues(meshDataPointer, colors);
			meshDataPointer += vertexCount * sizeof(u8vec4norm);
			vertexFlags |= VERTEX_COLORED;
		}

		boundingBox = {.min{positions.front()}, .max{positions.front()}};
		float boundingRadiusSquared = length2(positions.front());
		for (const vec3 position : positions.subspan(1)) {
			boundingBox.min = min(boundingBox.min, position);
			boundingBox.max = max(boundingBox.max, position);
			boundingRadiusSquared = max(boundingRadiusSquared, length2(position));
		}
		boundingRadius = sqrt(boundingRadiusSquared);
	}

	bindPose.localJoints = {{
		.translation{0.0f, 0.0f, 0.0f},
		.rotation{0.0f, 0.0f, 0.0f, 1.0f},
		.scale{1.0f, 1.0f, 1.0f},
		.visible = true,
	}};
	jointParentIndices = {0};
	jointColliders = {nullopt};
	jointPhysicsObjectIndices = {Limits<Model::PhysicsObjectIndex>::MAX};
	staticJointCount = 1;

	meshes = {{
		.primitiveType = PrimitiveType::TRIANGLES,
		.indexCount = static_cast<IndexCount>(indexCount),
		.vertexCount = static_cast<VertexCount>(vertexCount),
		.morphTargetCount = 0,
		.morphedVertexStride = 0,
		.meshDataOffset = 0,
		.morphTargetDataOffset = 0,
		.boundingBox = boundingBox,
		.boundingRadius = boundingRadius,
		.vertexFlags = vertexFlags,
	}};
	instances = {{
		.materialIndex = 0,
		.meshIndex = 0,
		.skinDataOffset = 0,
		.morphTargetWeightOffset = 0,
		.jointIndex = 0,
		.instanceFlags{},
	}};
	if (material) {
		materials = {*material};
	}
	bindPoseBoundingBox = boundingBox;
	bindPoseBoundingRadius = boundingRadius;
}

void Model::assign(const ConvexPolytope3D& polytope, const mat4& transformation) {
	reset();

	GREM_PROFILE_FUNCTION();

	const Span<const ConvexPolytopeVertex3D> vertices = polytope.getVertices();
	const Span<const ConvexPolytopeEdge3D> edges = polytope.getEdges();
	const Span<const ConvexPolytopeFace3D> faces = polytope.getFaces();
	GREM_ASSERT(edges.size() % 2 == 0);

	Buffer<vec3> positions{};
	positions.reserve(vertices.size());
	for (const ConvexPolytopeVertex3D& vertex : vertices) {
		positions.push_back(vec3{transformation * vec4{vertex, 1.0f}});
	}

	Buffer<uint32_t> indices{};
	indices.reserve(faces.size() * 6); // Estimate 2 triangles per face.
	for (const ConvexPolytopeFace3D& face : faces) {
		const ConvexPolytopeEdgeIndex firstEdgeIndex = face.firstEdgeIndex;
		const ConvexPolytopeVertexIndex firstVertexIndex = edges[firstEdgeIndex].vertexIndex;
		ConvexPolytopeEdgeIndex edgeIndex = edges[firstEdgeIndex].nextEdgeIndex;
		while (edgeIndex != firstEdgeIndex && edges[edgeIndex].nextEdgeIndex != firstEdgeIndex) {
			indices.push_back(static_cast<uint32_t>(firstVertexIndex));
			indices.push_back(static_cast<uint32_t>(edges[edgeIndex].vertexIndex));
			edgeIndex = edges[edgeIndex].nextEdgeIndex;
			indices.push_back(static_cast<uint32_t>(edges[edgeIndex].vertexIndex));
		}
	}

	assign(positions, indices);
}

void Model::load(const Filesystem& filesystem, CStringView filepath, const ModelOptions& options) {
	reset();

	GREM_PROFILE_BLOCK_DYNAMIC(formatString("Load model {}", filepath));

	try {
		const String fileContentsAsString = filesystem.readInputFileString(filepath);
		try {
			String filepathPrefix{filepath};
			if (const size_t filepathLastSlashPosition = filepathPrefix.find_last_of("/\\"); filepathLastSlashPosition != String::npos) {
				filepathPrefix.erase(filepathLastSlashPosition + 1, String::npos);
			} else {
				filepathPrefix.clear();
			}

			const auto loadImage = [&](CStringView relativeFilepath, const ImageOptions& options) -> Image {
				const String fullFilepath = filepathPrefix + relativeFilepath.c_str();
				if (filesystem.inputFileExists(fullFilepath)) {
					return resource::Image{filesystem, fullFilepath, options};
				}
				return resource::Image{filesystem, relativeFilepath, options};
			};

			const auto loadBufferData = [&](CStringView relativeFilepath) -> Variant<Allocation<byte>, Span<const byte>> {
				const String fullFilepath = filepathPrefix + relativeFilepath.c_str();
				if (filesystem.inputFileExists(fullFilepath)) {
					return filesystem.readInputFile(fullFilepath);
				}
				return filesystem.readInputFile(relativeFilepath);
			};

			const Span<const byte> fileContents = asBytes(Span{fileContentsAsString});
			const ModelFileType fileType = determineFileType(fileContents);
			switch (fileType) {
				case ModelFileType::UNKNOWN: throw resource::Error{"Unknown file type."};
				case ModelFileType::OBJ: {
					const obj::Asset objAsset = obj::Asset::parse(fileContentsAsString);
					obj::mtl::Library materialLibrary{};
					for (const String& materialLibraryFilename : objAsset.materialLibraryFilenames) {
						materialLibrary.load(filesystem.readInputFileString(filepathPrefix + materialLibraryFilename));
					}
					OBJModelLoader{*this, objAsset, materialLibrary, loadImage, options}.load();
					break;
				}
				case ModelFileType::GLTF: [[fallthrough]];
				case ModelFileType::GLTF_BINARY: {
					const gltf::Asset glTFAsset = (fileType == ModelFileType::GLTF_BINARY) ? gltf::Asset::parseBinary(fileContents) : gltf::Asset::parse(fileContentsAsString);
					GlTFModelLoader{*this, glTFAsset, loadImage, loadBufferData, options}.load();
					break;
				}
			}
		} catch (...) {
			Error::throwWithNestedFilepath(filepath);
		}
	} catch (...) {
		Error::throwWithNested(Error{"Failed to load model."});
	}
}

void Model::load(const gltf::Asset& asset, FunctionView<Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
	FunctionView<Variant<Allocation<byte>, Span<const byte>>(CStringView relativeFilepath)> loadBufferData, const ModelOptions& options) {
	reset();

	GREM_PROFILE_FUNCTION();
	try {
		GlTFModelLoader{*this, asset, loadImage, loadBufferData, options}.load();
	} catch (...) {
		Error::throwWithNested(Error{"Failed to load model."});
	}
}

void Model::load(const obj::Asset& asset, const obj::mtl::Library& materialLibrary, FunctionView<Image(CStringView relativeFilepath, const ImageOptions& options)> loadImage,
	const ModelOptions& options) {
	reset();

	GREM_PROFILE_FUNCTION();
	try {
		OBJModelLoader{*this, asset, materialLibrary, loadImage, options}.load();
	} catch (...) {
		Error::throwWithNested(Error{"Failed to load model."});
	}
}

} // namespace grem::resource
