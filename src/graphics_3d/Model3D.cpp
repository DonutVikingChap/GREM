// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/resource/Model.hpp>

#include <new>     // std::launder
#include <utility> // std::move, std::exchange

namespace grem::graphics {

namespace {

void uploadModelData(ArrayList<Model3D::Node>& nodes, RangeAllocation<uint32_t>& inverseBindPoseMatrixRange, RangeAllocation<uint32_t>& morphTargetValueRange, Device& device,
	Renderer3D& renderer3D, const resource::Model& model, const Model3DOptions& options,
	FunctionView<SharedPointer<TextureImplementation>(Device&, const resource::Model::Image&, const TextureImageUploadOptions&, const TextureSamplerOptions&)> loadTexture) {
	GREM_PROFILE_BLOCK("Upload model data to GPU");

	struct TextureKey {
		struct Hash {
			[[nodiscard]] size_t operator()(const TextureKey& key) const {
				return getHash(key.textureIndex, key.textureImageUploadOptions.transferFunction, key.textureImageUploadOptions.convertToPremultipliedAlpha);
			}
		};

		resource::Model::TextureIndex textureIndex;
		TextureImageUploadOptions textureImageUploadOptions;

		[[nodiscard]] bool operator==(const TextureKey&) const = default;
	};

	HashMap<TextureKey, SharedPointer<TextureImplementation>, TextureKey::Hash> textureMap{};

	try {
		{
			GREM_PROFILE_BLOCK("Upload inverse bind-pose matrices");
			inverseBindPoseMatrixRange =
				renderer3D.uploadModel3DInverseBindPoseMatrices(Span{std::launder(reinterpret_cast<const Model3D::InverseBindPoseMatrixFields*>(model.skinData.data())),
					model.skinData.size() / sizeof(Model3D::InverseBindPoseMatrixFields)});
		}
		{
			GREM_PROFILE_BLOCK("Upload morph target values");
			morphTargetValueRange =
				renderer3D.uploadModel3DMorphTargetValues(Span{std::launder(reinterpret_cast<const Model3D::MorphTargetValueFields*>(model.morphTargetData.data())),
					model.morphTargetData.size() / sizeof(Model3D::MorphTargetValueFields)});
		}

		nodes.reserve(model.instances.size());

		Allocation<resource::Model::InstanceIndex> firstNodeIndices(model.meshes.size(), Limits<resource::Model::InstanceIndex>::MAX);
		Buffer<uint32_t> generatedIndices{};
		for (const resource::Model::Instance& instance : model.instances) {
			GREM_PROFILE_BLOCK("Upload mesh instance");

			const resource::Model::Mesh& mesh = model.meshes[instance.meshIndex];
			resource::Model::InstanceIndex& firstNodeIndex = firstNodeIndices[instance.meshIndex];

			Model3D::Parameters meshParameters{
				.meshBaseColorFactor{1.0f, 1.0f, 1.0f, 1.0f},
				.meshOcclusionRoughnessMetallicFactor{1.0f, 1.0f, 1.0f},
				.meshNormalScale = 1.0f,
				.meshEmissiveFactor{0.0f, 0.0f, 0.0f},
				.meshAlphaCutoff = 0.5f,
				.meshIndexOfRefraction = 1.5f,
				.meshBaseColorMap = renderer3D.getWhiteTexture2D(),
				.meshOcclusionRoughnessMetallicMap = renderer3D.getWhiteTexture2D(),
				.meshNormalMap = renderer3D.getFlatNormalTexture2D(),
				.meshEmissiveMap = renderer3D.getWhiteTexture2D(),
				.meshBaseColorMapTextureOffset{0.0f, 0.0f},
				.meshBaseColorMapTextureBasis{1.0f},
				.meshOcclusionRoughnessMetallicMapTextureOffset{0.0f, 0.0f},
				.meshOcclusionRoughnessMetallicMapTextureBasis{1.0f},
				.meshNormalMapTextureOffset{0.0f, 0.0f},
				.meshNormalMapTextureBasis{1.0f},
				.meshEmissiveMapTextureOffset{0.0f, 0.0f},
				.meshEmissiveMapTextureBasis{1.0f},
				.meshMorphTargetValueOffset = morphTargetValueRange.begin + mesh.morphTargetDataOffset,
				.meshMorphTargetCount = mesh.morphTargetCount,
				.meshMorphTargetStride = mesh.vertexCount * mesh.morphedVertexStride,
			};
			Model3D::ShaderConfiguration shaderConfiguration{
				.materialType = Model3D::MaterialType::METALLIC_ROUGHNESS,
				.primitiveType = static_cast<PrimitiveType>(mesh.primitiveType),
				.frontFace = (instance.instanceFlags & resource::Model::INSTANCE_REVERSE_WINDING_ORDER) ? FrontFace::CLOCKWISE : FrontFace::COUNTERCLOCKWISE,
				.vertexFlags = mesh.vertexFlags,
			};
			if (instance.materialIndex < static_cast<resource::Model::MaterialIndex>(model.materials.size())) {
				const resource::Model::Material& material = model.materials[instance.materialIndex];
				shaderConfiguration.materialType = static_cast<Model3D::MaterialType>(material.materialType);
				shaderConfiguration.fragmentFlags = material.fragmentFlags;
				meshParameters.meshBaseColorFactor = material.baseColorFactor;
				meshParameters.meshOcclusionRoughnessMetallicFactor = {material.occlusionStrength, material.roughnessFactor, material.metallicFactor};
				meshParameters.meshNormalScale = material.normalScale;
				meshParameters.meshEmissiveFactor = material.emissiveFactor;
				meshParameters.meshAlphaCutoff = material.alphaCutoff;
				meshParameters.meshIndexOfRefraction = material.indexOfRefraction;
				const auto getTextureInfo = [&](sampler2D& map, vec2& offset, mat2& basis, const resource::Model::Material::TextureInfo& textureInfo,
												const TextureImageUploadOptions& textureImageUploadOptions) -> void {
					if (textureInfo.textureIndex < model.textures.size()) {
						const auto [it, inserted] =
							textureMap.try_emplace(TextureKey{.textureIndex = textureInfo.textureIndex, .textureImageUploadOptions = textureImageUploadOptions});
						if (inserted) {
							const resource::Model::Texture& texture = model.textures[textureInfo.textureIndex];
							TextureSamplerOptions textureSamplerOptions{.maxAnisotropy = options.maxTextureSamplerAnisotropy};
							switch (texture.minificationFilter) {
								case resource::Model::MinificationFilter::UNSPECIFIED: break;
								case resource::Model::MinificationFilter::NEAREST:
									textureSamplerOptions.minificationFilter = TextureFilter::NEAREST;
									textureSamplerOptions.mipmapMode = TextureMipmapMode::NONE;
									break;
								case resource::Model::MinificationFilter::LINEAR:
									textureSamplerOptions.minificationFilter = TextureFilter::LINEAR;
									textureSamplerOptions.mipmapMode = TextureMipmapMode::NONE;
									break;
								case resource::Model::MinificationFilter::NEAREST_MIPMAP_NEAREST:
									textureSamplerOptions.minificationFilter = TextureFilter::NEAREST;
									textureSamplerOptions.mipmapMode = TextureMipmapMode::NEAREST;
									break;
								case resource::Model::MinificationFilter::LINEAR_MIPMAP_NEAREST:
									textureSamplerOptions.minificationFilter = TextureFilter::LINEAR;
									textureSamplerOptions.mipmapMode = TextureMipmapMode::NEAREST;
									break;
								case resource::Model::MinificationFilter::NEAREST_MIPMAP_LINEAR:
									textureSamplerOptions.minificationFilter = TextureFilter::NEAREST;
									textureSamplerOptions.mipmapMode = TextureMipmapMode::LINEAR;
									break;
								case resource::Model::MinificationFilter::LINEAR_MIPMAP_LINEAR:
									textureSamplerOptions.minificationFilter = TextureFilter::LINEAR;
									textureSamplerOptions.mipmapMode = TextureMipmapMode::LINEAR;
									break;
							}
							switch (texture.magnificationFilter) {
								case resource::Model::MagnificationFilter::UNSPECIFIED: break;
								case resource::Model::MagnificationFilter::NEAREST: textureSamplerOptions.magnificationFilter = TextureFilter::NEAREST; break;
								case resource::Model::MagnificationFilter::LINEAR: textureSamplerOptions.magnificationFilter = TextureFilter::LINEAR; break;
							}
							switch (texture.horizontalWrappingMode) {
								case resource::Model::WrappingMode::REPEAT: textureSamplerOptions.horizontalWrappingMode = TextureWrappingMode::REPEAT; break;
								case resource::Model::WrappingMode::MIRRORED_REPEAT: textureSamplerOptions.horizontalWrappingMode = TextureWrappingMode::MIRRORED_REPEAT; break;
								case resource::Model::WrappingMode::CLAMP_TO_EDGE: textureSamplerOptions.horizontalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE; break;
							}
							switch (texture.verticalWrappingMode) {
								case resource::Model::WrappingMode::REPEAT: textureSamplerOptions.verticalWrappingMode = TextureWrappingMode::REPEAT; break;
								case resource::Model::WrappingMode::MIRRORED_REPEAT: textureSamplerOptions.verticalWrappingMode = TextureWrappingMode::MIRRORED_REPEAT; break;
								case resource::Model::WrappingMode::CLAMP_TO_EDGE: textureSamplerOptions.verticalWrappingMode = TextureWrappingMode::CLAMP_TO_EDGE; break;
							}
							it->second = loadTexture(device, texture.image, textureImageUploadOptions, textureSamplerOptions);
						}
						map = it->second;
					}
					offset = textureInfo.textureOffset;
					basis = textureInfo.textureBasis;
				};

				{
					GREM_PROFILE_BLOCK("Upload base color texture");
					getTextureInfo(meshParameters.meshBaseColorMap, meshParameters.meshBaseColorMapTextureOffset, meshParameters.meshBaseColorMapTextureBasis,
						material.baseColorMap,
						{.transferFunction = Color::TransferFunction::SRGB,
							.convertToPremultipliedAlpha = (material.fragmentFlags & resource::Model::FRAGMENT_ALPHA_BLENDED) != 0,
							.generateMipmap = true});
				}
				{
					GREM_PROFILE_BLOCK("Upload ORM texture");
					getTextureInfo(meshParameters.meshOcclusionRoughnessMetallicMap, meshParameters.meshOcclusionRoughnessMetallicMapTextureOffset,
						meshParameters.meshOcclusionRoughnessMetallicMapTextureBasis, material.occlusionRoughnessMetallicMap,
						{.transferFunction = Color::TransferFunction::LINEAR, .convertToPremultipliedAlpha = false, .generateMipmap = true});
				}
				{
					GREM_PROFILE_BLOCK("Upload normal texture");
					getTextureInfo(meshParameters.meshNormalMap, meshParameters.meshNormalMapTextureOffset, meshParameters.meshNormalMapTextureBasis, material.normalMap,
						{.transferFunction = Color::TransferFunction::LINEAR, .convertToPremultipliedAlpha = false, .generateMipmap = true});
				}
				{
					GREM_PROFILE_BLOCK("Upload emissive texture");
					getTextureInfo(meshParameters.meshEmissiveMap, meshParameters.meshEmissiveMapTextureOffset, meshParameters.meshEmissiveMapTextureBasis, material.emissiveMap,
						{.transferFunction = Color::TransferFunction::SRGB, .convertToPremultipliedAlpha = false, .generateMipmap = true});
				}
			}

			if (firstNodeIndex == Limits<resource::Model::InstanceIndex>::MAX) {
				firstNodeIndex = static_cast<resource::Model::InstanceIndex>(nodes.size());

				VertexAttributeMask activeVertexAttributes{};
				const byte* data = model.meshData.data() + static_cast<size_t>(mesh.meshDataOffset) * 4;
				const size_t indexCount = static_cast<size_t>(mesh.indexCount);
				const size_t vertexCount = static_cast<size_t>(mesh.vertexCount);
				const resource::Model::VertexFlags vertexFlags = mesh.vertexFlags;
				Span<const uint32_t> indices{};
				if (indexCount > 0) {
					indices = Span{std::launder(reinterpret_cast<const uint32_t*>(data)), indexCount};
					data += indices.size_bytes();
				} else {
					const size_t generatedIndicesEnd = generatedIndices.size();
					if (vertexCount > generatedIndicesEnd) {
						generatedIndices.resize(vertexCount);
						iota(Span{generatedIndices}.subspan(generatedIndicesEnd), static_cast<uint32_t>(generatedIndicesEnd));
					}
					indices = Span{generatedIndices}.first(vertexCount);
				}
				activeVertexAttributes[0] = true;
				const Span<const vec3> positions{std::launder(reinterpret_cast<const vec3*>(data)), vertexCount};
				data += positions.size_bytes();
				activeVertexAttributes[1] = true;
				const Span<const iA2B10G10R10vec4norm> normals{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(data)), vertexCount};
				data += normals.size_bytes();
				activeVertexAttributes[2] = true;
				const Span<const iA2B10G10R10vec4norm> tangents{std::launder(reinterpret_cast<const iA2B10G10R10vec4norm*>(data)), vertexCount};
				data += tangents.size_bytes();
				Span<const vec2> textureCoordinatesChannel0{};
				if ((vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0) {
					activeVertexAttributes[3] = true;
					textureCoordinatesChannel0 = Span{std::launder(reinterpret_cast<const vec2*>(data)), vertexCount};
					data += textureCoordinatesChannel0.size_bytes();
				}
				Span<const vec2> textureCoordinatesChannel1{};
				if ((vertexFlags & resource::Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0) {
					activeVertexAttributes[4] = true;
					textureCoordinatesChannel1 = Span{std::launder(reinterpret_cast<const vec2*>(data)), vertexCount};
					data += textureCoordinatesChannel1.size_bytes();
				}
				Span<const u8vec4norm> colors{};
				if ((vertexFlags & resource::Model::VERTEX_COLORED) != 0) {
					activeVertexAttributes[5] = true;
					colors = Span{std::launder(reinterpret_cast<const u8vec4norm*>(data)), vertexCount};
					data += colors.size_bytes();
				}
				Span<const u8vec4> jointIndices{};
				Span<const u8vec4norm> jointWeights{};
				if ((vertexFlags & resource::Model::VERTEX_SKINNED) != 0) {
					activeVertexAttributes[6] = true;
					jointIndices = Span{std::launder(reinterpret_cast<const u8vec4*>(data)), vertexCount};
					data += jointIndices.size_bytes();
					activeVertexAttributes[7] = true;
					jointWeights = Span{std::launder(reinterpret_cast<const u8vec4norm*>(data)), vertexCount};
					data += jointWeights.size_bytes();
				}

				GREM_PROFILE_BLOCK("Upload vertices and indices");
				Model3D::Node& node = nodes.emplace_back(Model3D::Node{
					.mesh{device, meshParameters, activeVertexAttributes},
					.shaderConfiguration = shaderConfiguration,
					.inverseBindPoseMatrixOffset = static_cast<uint32_t>((static_cast<size_t>(instance.skinDataOffset) * 4) / sizeof(mat4)),
					.morphTargetWeightOffset = instance.morphTargetWeightOffset,
					.jointIndex = instance.jointIndex,
					.boundingBox = mesh.boundingBox,
					.boundingRadius = mesh.boundingRadius,
				});
				node.mesh.setIndicesAndVertexAttributes(indices, positions, normals, tangents, textureCoordinatesChannel0, textureCoordinatesChannel1, colors, jointIndices,
					jointWeights);
			} else {
				GREM_PROFILE_BLOCK("Upload vertices and indices");
				Model3D::Node& node = nodes.emplace_back(Model3D::Node{
					.mesh = nodes[firstNodeIndex].mesh,
					.shaderConfiguration = shaderConfiguration,
					.inverseBindPoseMatrixOffset = static_cast<uint32_t>((static_cast<size_t>(instance.skinDataOffset) * 4) / sizeof(mat4)),
					.morphTargetWeightOffset = instance.morphTargetWeightOffset,
					.jointIndex = instance.jointIndex,
					.boundingBox = mesh.boundingBox,
					.boundingRadius = mesh.boundingRadius,
				});
				node.mesh.setParameters(meshParameters);
			}
		}
	} catch (...) {
		renderer3D.releaseModel3DInverseBindPoseMatrices(std::exchange(inverseBindPoseMatrixRange, {}));
		renderer3D.releaseModel3DMorphTargetValues(std::exchange(morphTargetValueRange, {}));
		throw;
	}
}

} // namespace

Model3D::Model3D(Device& device, Renderer3D& renderer3D, const resource::Model& model, const Model3DOptions& options,
	FunctionView<SharedPointer<TextureImplementation>(Device&, const resource::Model::Image&, const TextureImageUploadOptions&, const TextureSamplerOptions&)> loadTexture)
	: m{
		  .device = &device,
		  .renderer3D = &renderer3D,
		  .bindPose = model.bindPose,
		  .jointParentIndices = model.jointParentIndices,
		  .animations = model.animations,
		  .animationChannels = model.animationChannels,
		  .keyframeInputTimePoints = model.keyframeInputTimePoints,
		  .keyframeOutputValueData = model.keyframeOutputValueData,
		  .staticJointCount = model.staticJointCount,
		  .bindPoseBoundingBox = model.bindPoseBoundingBox,
		  .bindPoseBoundingRadius = model.bindPoseBoundingRadius,
		  .jointMap = model.jointMap,
		  .skinMap = model.skinMap,
		  .animationMap = model.animationMap,
	  } {
	uploadModelData(m.nodes, m.inverseBindPoseMatrixRange, m.morphTargetValueRange, device, renderer3D, model, options, loadTexture);
}

Model3D::Model3D(Device& device, Renderer3D& renderer3D, resource::Model&& model, // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	const Model3DOptions& options,
	FunctionView<SharedPointer<TextureImplementation>(Device&, const resource::Model::Image&, const TextureImageUploadOptions&, const TextureSamplerOptions&)> loadTexture)
	: m{
		  .device = &device,
		  .renderer3D = &renderer3D,
		  .bindPose = std::move(model.bindPose),
		  .jointParentIndices = std::move(model.jointParentIndices),
		  .animations = std::move(model.animations),
		  .animationChannels = std::move(model.animationChannels),
		  .keyframeInputTimePoints = std::move(model.keyframeInputTimePoints),
		  .keyframeOutputValueData = std::move(model.keyframeOutputValueData),
		  .staticJointCount = model.staticJointCount,
		  .bindPoseBoundingBox = model.bindPoseBoundingBox,
		  .bindPoseBoundingRadius = model.bindPoseBoundingRadius,
		  .jointMap = std::move(model.jointMap),
		  .skinMap = std::move(model.skinMap),
		  .animationMap = std::move(model.animationMap),
	  } {
	uploadModelData(m.nodes, m.inverseBindPoseMatrixRange, m.morphTargetValueRange, device, renderer3D, model, options, loadTexture);
}

Model3D::~Model3D() {
	m.renderer3D->releaseModel3DInverseBindPoseMatrices(m.inverseBindPoseMatrixRange);
	m.renderer3D->releaseModel3DMorphTargetValues(m.morphTargetValueRange);
}

Model3D::Model3D(const Model3D& other)
	: m(other.m) {
	m.renderer3D->reacquireModel3DInverseBindPoseMatrices(m.inverseBindPoseMatrixRange);
	m.renderer3D->reacquireModel3DMorphTargetValues(m.morphTargetValueRange);
}

Model3D::Model3D(Model3D&& other) noexcept
	: m(std::move(other.m)) {
	other.m.inverseBindPoseMatrixRange = {};
	other.m.morphTargetValueRange = {};
}

Model3D& Model3D::operator=(const Model3D& other) {
	if (this == &other) {
		return *this;
	}
	m.renderer3D->releaseModel3DInverseBindPoseMatrices(m.inverseBindPoseMatrixRange);
	m.renderer3D->releaseModel3DMorphTargetValues(m.morphTargetValueRange);
	m = other.m;
	m.renderer3D->reacquireModel3DInverseBindPoseMatrices(other.m.inverseBindPoseMatrixRange);
	m.renderer3D->reacquireModel3DMorphTargetValues(other.m.morphTargetValueRange);
	return *this;
}

Model3D& Model3D::operator=(Model3D&& other) noexcept {
	if (this == &other) {
		return *this;
	}
	m.renderer3D->releaseModel3DInverseBindPoseMatrices(m.inverseBindPoseMatrixRange);
	m.renderer3D->releaseModel3DMorphTargetValues(m.morphTargetValueRange);
	m = std::move(other.m);
	other.m.inverseBindPoseMatrixRange = {};
	other.m.morphTargetValueRange = {};
	return *this;
}

} // namespace grem::graphics
