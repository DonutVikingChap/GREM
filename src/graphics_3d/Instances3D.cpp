// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Font2D.hpp>
#include <GREM/graphics_2d/Text2D.hpp>
#include <GREM/graphics_3d/Instances3D.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/resource/Model.hpp>

#include <utility> // std::move

namespace grem::graphics {

namespace {

struct Model3DInfo {
	uint32_t inverseBindPoseMatrixRangeOffset;
	uint32_t jointCount;
	uint32_t morphTargetWeightCount;

	explicit Model3DInfo(const Model3D& model)
		: inverseBindPoseMatrixRangeOffset(model.getInverseBindPoseMatrixOffset())
		, jointCount(model.getJointCount())
		, morphTargetWeightCount(model.getMorphTargetWeightCount()) {}
};

struct Model3DInstanceInfo {
	vec4 tintColor;
	vec3 emissiveColor;
	vec3 emissiveFactor;
	uint32_t instanceIdentifier;
	uint32_t relativeUploadedTransformationIndex;

	explicit Model3DInstanceInfo(const ModelInstance3D& instance, uint32_t relativeUploadedTransformationIndex)
		: tintColor(instance.color.toLinearRGBA())
		, emissiveColor(instance.emissiveColor.toLinearRGB())
		, emissiveFactor(instance.emissiveFactor)
		, instanceIdentifier(instance.instanceIdentifier)
		, relativeUploadedTransformationIndex(relativeUploadedTransformationIndex) {}
};

struct Model3DNodeInfo {
	uint32_t localInverseBindPoseMatrixOffset;
	uint32_t localJointIndex;
	uint32_t localMorphTargetWeightOffset;

	explicit Model3DNodeInfo(const Model3D::Node& node)
		: localInverseBindPoseMatrixOffset(node.inverseBindPoseMatrixOffset)
		, localJointIndex((node.shaderConfiguration.vertexFlags & resource::Model::VERTEX_SKINNED) ? 0 : node.jointIndex)
		, localMorphTargetWeightOffset(node.morphTargetWeightOffset) {}

	[[nodiscard]] bool operator==(const Model3DNodeInfo&) const = default;
};

void pushModel3DInstance(InstanceBuffer<Model3D::Instance>& instanceBuffer, uint32_t jointRangeBase, uint32_t morphTargetWeightRangeBase, const Model3DInfo& modelInfo,
	const Model3DInstanceInfo& instanceInfo, const Model3DNodeInfo& nodeInfo) {
	instanceBuffer.push(Model3D::Instance{
		.instanceTintColor = instanceInfo.tintColor,
		.instanceEmissiveColor = instanceInfo.emissiveColor,
		.instanceEmissiveFactor = instanceInfo.emissiveFactor,
		.instanceInverseBindPoseMatrixOffset = modelInfo.inverseBindPoseMatrixRangeOffset + nodeInfo.localInverseBindPoseMatrixOffset,
		.instanceJointOffset = jointRangeBase + instanceInfo.relativeUploadedTransformationIndex * modelInfo.jointCount + nodeInfo.localJointIndex,
		.instanceMorphTargetWeightOffset =
			morphTargetWeightRangeBase + instanceInfo.relativeUploadedTransformationIndex * modelInfo.morphTargetWeightCount + nodeInfo.localMorphTargetWeightOffset,
		.instanceInstanceIdentifier = instanceInfo.instanceIdentifier,
	});
}

} // namespace

void Instances3D::putPBRModelInstance(const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance) {
	putShadedModelInstance(renderer3D->getPBRModel3DShaderPipelineSet(), model, transformation, instance);
}

void Instances3D::putPBRModelInstance(const Model3D& model, const mat4& transformation, const ModelInstance3D& instance) {
	const resource::Model::Pose& pose = model.getBindPose();
	renderer3D->temporaryModelTransformation.assign(transformation, pose.localJoints, pose.localMorphTargetWeights, model.getJointParentIndices());
	putPBRModelInstance(model, renderer3D->temporaryModelTransformation, instance);
}

void Instances3D::putPBRModelInstance(const Model3D& model, const mat4& transformation, resource::Model::PoseView pose, const ModelInstance3D& instance) {
	renderer3D->temporaryModelTransformation.assign(transformation, Span{pose.localJoints, model.getJointCount()},
		Span{pose.localMorphTargetWeights, model.getMorphTargetWeightCount()}, model.getJointParentIndices());
	putPBRModelInstance(model, renderer3D->temporaryModelTransformation, instance);
}

void Instances3D::putPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances) {
	putShadedModelInstances(renderer3D->getPBRModel3DShaderPipelineSet(), model, transformations, instances);
}

void Instances3D::putPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance) {
	putShadedModelInstances(renderer3D->getPBRModel3DShaderPipelineSet(), model, transformations, instance);
}

void Instances3D::putPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations) {
	putShadedModelInstances(renderer3D->getPBRModel3DShaderPipelineSet(), model, transformations);
}

void Instances3D::putVisiblePBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const ModelInstance3D> instances, StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getPBRModel3DShaderPipelineSet(), model, transformations, instances, instancesVisible);
}

void Instances3D::putVisiblePBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance,
	StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getPBRModel3DShaderPipelineSet(), model, transformations, instance, instancesVisible);
}

void Instances3D::putVisiblePBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getPBRModel3DShaderPipelineSet(), model, transformations, instancesVisible);
}

void Instances3D::putHDRPBRModelInstance(const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance) {
	putShadedModelInstance(renderer3D->getHDRPBRModel3DShaderPipelineSet(), model, transformation, instance);
}

void Instances3D::putHDRPBRModelInstance(const Model3D& model, const mat4& transformation, const ModelInstance3D& instance) {
	const resource::Model::Pose& pose = model.getBindPose();
	renderer3D->temporaryModelTransformation.assign(transformation, pose.localJoints, pose.localMorphTargetWeights, model.getJointParentIndices());
	putHDRPBRModelInstance(model, renderer3D->temporaryModelTransformation, instance);
}

void Instances3D::putHDRPBRModelInstance(const Model3D& model, const mat4& transformation, resource::Model::PoseView pose, const ModelInstance3D& instance) {
	renderer3D->temporaryModelTransformation.assign(transformation, Span{pose.localJoints, model.getJointCount()},
		Span{pose.localMorphTargetWeights, model.getMorphTargetWeightCount()}, model.getJointParentIndices());
	putHDRPBRModelInstance(model, renderer3D->temporaryModelTransformation, instance);
}

void Instances3D::putHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const ModelInstance3D> instances) {
	putShadedModelInstances(renderer3D->getHDRPBRModel3DShaderPipelineSet(), model, transformations, instances);
}

void Instances3D::putHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance) {
	putShadedModelInstances(renderer3D->getHDRPBRModel3DShaderPipelineSet(), model, transformations, instance);
}

void Instances3D::putHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations) {
	putShadedModelInstances(renderer3D->getHDRPBRModel3DShaderPipelineSet(), model, transformations);
}

void Instances3D::putVisibleHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const ModelInstance3D> instances, StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getHDRPBRModel3DShaderPipelineSet(), model, transformations, instances, instancesVisible);
}

void Instances3D::putVisibleHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance,
	StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getHDRPBRModel3DShaderPipelineSet(), model, transformations, instance, instancesVisible);
}

void Instances3D::putVisibleHDRPBRModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getHDRPBRModel3DShaderPipelineSet(), model, transformations, instancesVisible);
}

void Instances3D::putUnlitModelInstance(const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance) {
	putShadedModelInstance(renderer3D->getUnlitModel3DShaderPipelineSet(), model, transformation, instance);
}

void Instances3D::putUnlitModelInstance(const Model3D& model, const mat4& transformation, const ModelInstance3D& instance) {
	const resource::Model::Pose& pose = model.getBindPose();
	renderer3D->temporaryModelTransformation.assign(transformation, pose.localJoints, pose.localMorphTargetWeights, model.getJointParentIndices());
	putUnlitModelInstance(model, renderer3D->temporaryModelTransformation, instance);
}

void Instances3D::putUnlitModelInstance(const Model3D& model, const mat4& transformation, resource::Model::PoseView pose, const ModelInstance3D& instance) {
	renderer3D->temporaryModelTransformation.assign(transformation, Span{pose.localJoints, model.getJointCount()},
		Span{pose.localMorphTargetWeights, model.getMorphTargetWeightCount()}, model.getJointParentIndices());
	putUnlitModelInstance(model, renderer3D->temporaryModelTransformation, instance);
}

void Instances3D::putUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const ModelInstance3D> instances) {
	putShadedModelInstances(renderer3D->getUnlitModel3DShaderPipelineSet(), model, transformations, instances);
}

void Instances3D::putUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance) {
	putShadedModelInstances(renderer3D->getUnlitModel3DShaderPipelineSet(), model, transformations, instance);
}

void Instances3D::putUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations) {
	putShadedModelInstances(renderer3D->getUnlitModel3DShaderPipelineSet(), model, transformations);
}

void Instances3D::putVisibleUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const ModelInstance3D> instances, StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getUnlitModel3DShaderPipelineSet(), model, transformations, instances, instancesVisible);
}

void Instances3D::putVisibleUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance,
	StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getUnlitModel3DShaderPipelineSet(), model, transformations, instance, instancesVisible);
}

void Instances3D::putVisibleUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getUnlitModel3DShaderPipelineSet(), model, transformations, instancesVisible);
}

void Instances3D::putHDRUnlitModelInstance(const Model3D& model, resource::Model::TransformationView transformation, const ModelInstance3D& instance) {
	putShadedModelInstance(renderer3D->getHDRUnlitModel3DShaderPipelineSet(), model, transformation, instance);
}

void Instances3D::putHDRUnlitModelInstance(const Model3D& model, const mat4& transformation, const ModelInstance3D& instance) {
	const resource::Model::Pose& pose = model.getBindPose();
	renderer3D->temporaryModelTransformation.assign(transformation, pose.localJoints, pose.localMorphTargetWeights, model.getJointParentIndices());
	putHDRUnlitModelInstance(model, renderer3D->temporaryModelTransformation, instance);
}

void Instances3D::putHDRUnlitModelInstance(const Model3D& model, const mat4& transformation, resource::Model::PoseView pose, const ModelInstance3D& instance) {
	renderer3D->temporaryModelTransformation.assign(transformation, Span{pose.localJoints, model.getJointCount()},
		Span{pose.localMorphTargetWeights, model.getMorphTargetWeightCount()}, model.getJointParentIndices());
	putHDRUnlitModelInstance(model, renderer3D->temporaryModelTransformation, instance);
}

void Instances3D::putHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const ModelInstance3D> instances) {
	putShadedModelInstances(renderer3D->getHDRUnlitModel3DShaderPipelineSet(), model, transformations, instances);
}

void Instances3D::putHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance) {
	putShadedModelInstances(renderer3D->getHDRUnlitModel3DShaderPipelineSet(), model, transformations, instance);
}

void Instances3D::putHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations) {
	putShadedModelInstances(renderer3D->getHDRUnlitModel3DShaderPipelineSet(), model, transformations);
}

void Instances3D::putVisibleHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const ModelInstance3D> instances, StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getHDRUnlitModel3DShaderPipelineSet(), model, transformations, instances, instancesVisible);
}

void Instances3D::putVisibleHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations, const ModelInstance3D& instance,
	StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getHDRUnlitModel3DShaderPipelineSet(), model, transformations, instance, instancesVisible);
}

void Instances3D::putVisibleHDRUnlitModelInstances(const Model3D& model, StridedSpan<const resource::Model::TransformationView> transformations,
	StridedSpan<const bool> instancesVisible) {
	putVisibleShadedModelInstances(renderer3D->getHDRUnlitModel3DShaderPipelineSet(), model, transformations, instancesVisible);
}

void Instances3D::putFlatModelInstance(const Model2D& model, const FlatModelInstance3D& instance) {
	putShadedFlatModelInstance(renderer3D->getPlain3DTransformedModel2DShaderPipeline(), model, instance);
}

void Instances3D::putShadedFlatModelInstance(const Model2D::ShaderPipeline& shaderPipeline, const Model2D& model, const FlatModelInstance3D& instance) {
	putShadedFlatModelInstanceImplementation(shaderPipeline.lock(), model, instance);
}

void Instances3D::putTriangleInstance(const TriangleInstance3D& instance) {
	putShadedTriangleInstance(renderer3D->getPlain3DTransformedModel2DShaderPipeline(), instance);
}

void Instances3D::putShadedTriangleInstance(const Model2D::ShaderPipeline& shaderPipeline, const TriangleInstance3D& instance) {
	putShadedFlatModelInstanceImplementation(shaderPipeline.lock(), renderer3D->getUnitRightAngledTriangleModel2D(),
		FlatModelInstance3D{
			.texture = instance.texture,
			.transformation{
				vec4{instance.pointB - instance.pointA, 0.0f},
				vec4{instance.pointC - instance.pointA, 0.0f},
				vec4{0.0f, 0.0f, 1.0f, 0.0f},
				vec4{instance.pointA, 1.0f},
			},
			.textureOffset = instance.textureOffset,
			.textureBasis = instance.textureBasis,
			.color = instance.color,
			.emissiveColor = instance.emissiveColor,
			.distanceOrderingBias = instance.distanceOrderingBias,
		});
}

void Instances3D::putQuadInstance(const QuadInstance3D& instance) {
	putShadedQuadInstance(renderer3D->getPlain3DTransformedModel2DShaderPipeline(), instance);
}

void Instances3D::putShadedQuadInstance(const Model2D::ShaderPipeline& shaderPipeline, const QuadInstance3D& instance) {
	putShadedFlatModelInstanceImplementation(shaderPipeline.lock(), renderer3D->getUnitSquareModel2D(),
		FlatModelInstance3D{
			.texture = instance.texture,
			.transformation = translateRotateScale(instance.position, instance.orientation, vec3{instance.size, 1.0f}) * translate(vec3{-instance.origin, 0.0f}),
			.textureOffset = instance.textureOffset,
			.textureBasis = instance.textureBasis,
			.color = instance.color,
			.emissiveColor = instance.emissiveColor,
			.distanceOrderingBias = instance.distanceOrderingBias,
		});
}

void Instances3D::putSpriteInstance(const SpriteAtlas& spriteAtlas, SpriteID spriteID, const SpriteInstance3D& instance) {
	putShadedSpriteInstance(renderer3D->getPlain3DTransformedModel2DShaderPipeline(), spriteAtlas, spriteID, instance);
}

void Instances3D::putShadedSpriteInstance(const Model2D::ShaderPipeline& shaderPipeline, const SpriteAtlas& spriteAtlas, SpriteID spriteID, const SpriteInstance3D& instance) {
	const auto [textureOffset, textureScale] = spriteAtlas.getSpriteTextureOffsetAndScale(spriteID);
	putShadedFlatModelInstanceImplementation(shaderPipeline.lock(), renderer3D->getUnitSquareModel2D(),
		FlatModelInstance3D{
			.texture = &spriteAtlas.getAtlasTexture(),
			.transformation = translateRotateScale(instance.position, instance.orientation, vec3{instance.size, 1.0f}) * translate(vec3{-instance.origin, 0.0f}),
			.textureOffset = textureOffset,
			.textureBasis = mat2{scale(textureScale)},
			.color = instance.color,
			.emissiveColor = instance.emissiveColor,
			.distanceOrderingBias = instance.distanceOrderingBias,
		});
}

void Instances3D::putTextInstance(const Text2D& text, const TextInstance3D& instance) {
	putShadedTextInstance(renderer3D->getPlain3DTransformedTextShaderPipeline(), text, instance);
}

void Instances3D::putShadedTextInstance(const Model2D::ShaderPipeline& shaderPipeline, const Text2D& text, const TextInstance3D& instance) {
	putShadedTextInstanceImplementation(shaderPipeline.lock(), text, instance);
}

void Instances3D::putTextStringInstance(Font2D& font, UTF8StringView string, const TextStringInstance3D& instance) {
	putShadedTextStringInstance(renderer3D->getPlain3DTransformedTextShaderPipeline(), font, string, instance);
}

void Instances3D::putShadedTextStringInstance(const Model2D::ShaderPipeline& shaderPipeline, Font2D& font, UTF8StringView string, const TextStringInstance3D& instance) {
	renderer3D->temporaryText.assign(font, instance.characterSize, string, instance.shapeOrigin, instance.shapeScale);
	putShadedTextInstance(shaderPipeline, renderer3D->temporaryText,
		TextInstance3D{
			.position = instance.position,
			.orientation = instance.orientation,
			.scale = instance.scale,
			.alignment = instance.alignment,
			.color = instance.color,
			.distanceOrderingBias = instance.distanceOrderingBias,
		});
}

void Instances3D::putTextStringInstance(Font2D& font, StringView string, const TextStringInstance3D& instance) {
	static_assert(sizeof(char) == sizeof(char8_t));
	static_assert(alignof(char) == alignof(char8_t));
	putTextStringInstance(font, UTF8StringView{std::launder(reinterpret_cast<const char8_t*>(string.data())), string.size()}, instance);
}

void Instances3D::putShadedTextStringInstance(const Model2D::ShaderPipeline& shaderPipeline, Font2D& font, StringView string, const TextStringInstance3D& instance) {
	static_assert(sizeof(char) == sizeof(char8_t));
	static_assert(alignof(char) == alignof(char8_t));
	putShadedTextStringInstance(shaderPipeline, font, UTF8StringView{std::launder(reinterpret_cast<const char8_t*>(string.data())), string.size()}, instance);
}

resource::Model::Transformation& Instances3D::getTemporaryModelTransformation() {
	return renderer3D->temporaryModelTransformation;
}

void Instances3D::putShadedModelInstanceImplementation(
	FunctionView<SharedPointer<ShaderPipelineImplementation>(const Model3D::ShaderConfiguration& shaderConfiguration)> shaderPipelineSelectorAdapter, const Model3D& model,
	resource::Model::TransformationView transformation, const ModelInstance3D& instance) {
	GREM_ASSERT(model.getJointCount() > 0);

	if (model.getNodes().empty()) {
		[[unlikely]];
		return;
	}

	const Model3DInfo modelInfo{model};
	const Model3DInstanceInfo instanceInfo{instance, 0};

	const uint32_t jointRangeBase = static_cast<uint32_t>(joints.size());
	for (const mat4& jointMatrix : Span{transformation.jointMatrices, modelInfo.jointCount}) {
		joints.push_back(Model3D::JointFields{.jointMatrix = jointMatrix});
	}
	jointsDirty = true;

	const uint32_t morphTargetWeightRangeBase = static_cast<uint32_t>(morphTargetWeights.size());
	if (modelInfo.morphTargetWeightCount > 0) {
		for (const float weight : Span{transformation.morphTargetWeights, modelInfo.morphTargetWeightCount}) {
			morphTargetWeights.push_back(Model3D::MorphTargetWeightFields{.morphTargetWeight = weight});
		}
		morphTargetWeightsDirty = true;
	}

	uint32_t instanceIndex = model3DInstanceBuffer.size();
	Optional<Model3DNodeInfo> nodeInfo{};
	for (const Model3D::Node& node : model.getNodes()) {
		if (!transformation.jointsVisible[node.jointIndex]) {
			continue;
		}

		const Model3DNodeInfo newNodeInfo{node};
		if (newNodeInfo != nodeInfo) {
			instanceIndex = model3DInstanceBuffer.size();
			pushModel3DInstance(model3DInstanceBuffer, jointRangeBase, morphTargetWeightRangeBase, modelInfo, instanceInfo, newNodeInfo);
			nodeInfo = newNodeInfo;
		}

		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle = shaderPipelineSelectorAdapter(node.shaderConfiguration);
		SharedPointer<MeshImplementation> meshHandle = node.mesh.lock();
		if ((node.shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_ALPHA_BLENDED) != 0) {
			const mat4& jointMatrix = transformation.jointMatrices[newNodeInfo.localJointIndex];
			const vec3 jointScaleSquared{length2(vec3{jointMatrix[0]}), length2(vec3{jointMatrix[1]}), length2(vec3{jointMatrix[2]})};
			transparent3DDrawCommands.push_back(Transparent3DDrawCommand{
				.position = vec3{jointMatrix[3]},
				.instanceIndex = instanceIndex,
				.shaderPipelineHandle = std::move(shaderPipelineHandle),
				.meshHandle = std::move(meshHandle),
				.boundingRadius = node.boundingRadius * sqrt(maxComponent(jointScaleSquared)),
				.distanceOrderingBias = instance.distanceOrderingBias,
			});
		} else {
			opaqueModel3DDrawCommandBuffer.insert(std::move(shaderPipelineHandle), std::move(meshHandle), instanceIndex);
		}
	}
}

void Instances3D::putShadedModelInstancesImplementation(
	FunctionView<SharedPointer<ShaderPipelineImplementation>(const Model3D::ShaderConfiguration& shaderConfiguration)> shaderPipelineSelectorAdapter, const Model3D& model,
	StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances) {
	GREM_ASSERT(model.getJointCount() > 0);
	GREM_ASSERT(instances.size() == 1 || instances.size() == transformations.size());

	if (model.getNodes().empty() || transformations.empty()) {
		[[unlikely]];
		return;
	}

	const Model3DInfo modelInfo{model};

	const uint32_t jointRangeBase = static_cast<uint32_t>(joints.size());
	for (const resource::Model::TransformationView transformation : transformations) {
		for (const mat4& jointMatrix : Span{transformation.jointMatrices, modelInfo.jointCount}) {
			joints.push_back(Model3D::JointFields{.jointMatrix = jointMatrix});
		}
	}
	jointsDirty = true;

	const uint32_t morphTargetWeightRangeBase = static_cast<uint32_t>(morphTargetWeights.size());
	if (modelInfo.morphTargetWeightCount > 0) {
		for (const resource::Model::TransformationView transformation : transformations) {
			for (const float weight : Span{transformation.morphTargetWeights, modelInfo.morphTargetWeightCount}) {
				morphTargetWeights.push_back(Model3D::MorphTargetWeightFields{.morphTargetWeight = weight});
			}
		}
		morphTargetWeightsDirty = true;
	}

	for (const Model3D::Node& node : model.getNodes()) {
		const Model3DNodeInfo nodeInfo{node};

		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle = shaderPipelineSelectorAdapter(node.shaderConfiguration);
		SharedPointer<MeshImplementation> meshHandle = node.mesh.lock();
		if ((node.shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_ALPHA_BLENDED) != 0) {
			uint32_t relativeUploadedTransformationIndex = 0;
			for (size_t i = 0; i < transformations.size(); ++i) {
				if (transformations[i].jointsVisible[node.jointIndex]) {
					const ModelInstance3D& instance = (instances.size() == 1) ? instances.front() : instances[i];
					const Model3DInstanceInfo instanceInfo{instance, relativeUploadedTransformationIndex};

					const uint32_t instanceIndex = model3DInstanceBuffer.size();
					pushModel3DInstance(model3DInstanceBuffer, jointRangeBase, morphTargetWeightRangeBase, modelInfo, instanceInfo, nodeInfo);

					const mat4& jointMatrix = transformations[i].jointMatrices[nodeInfo.localJointIndex];
					const vec3 jointScaleSquared{length2(vec3{jointMatrix[0]}), length2(vec3{jointMatrix[1]}), length2(vec3{jointMatrix[2]})};
					transparent3DDrawCommands.push_back(Transparent3DDrawCommand{
						.position = vec3{jointMatrix[3]},
						.instanceIndex = instanceIndex,
						.shaderPipelineHandle = shaderPipelineHandle,
						.meshHandle = meshHandle,
						.boundingRadius = node.boundingRadius * sqrt(maxComponent(jointScaleSquared)),
						.distanceOrderingBias = instance.distanceOrderingBias,
					});
				}
				++relativeUploadedTransformationIndex;
			}
		} else {
			const uint32_t instanceOffset = model3DInstanceBuffer.size();

			uint32_t relativeUploadedTransformationIndex = 0;
			for (size_t i = 0; i < transformations.size(); ++i) {
				if (transformations[i].jointsVisible[node.jointIndex]) {
					const ModelInstance3D& instance = (instances.size() == 1) ? instances.front() : instances[i];
					const Model3DInstanceInfo instanceInfo{instance, relativeUploadedTransformationIndex};
					pushModel3DInstance(model3DInstanceBuffer, jointRangeBase, morphTargetWeightRangeBase, modelInfo, instanceInfo, nodeInfo);
				}
				++relativeUploadedTransformationIndex;
			}

			const uint32_t instanceCount = model3DInstanceBuffer.size() - instanceOffset;
			opaqueModel3DDrawCommandBuffer.insertRange(std::move(shaderPipelineHandle), std::move(meshHandle), instanceOffset, instanceCount);
		}
	}
}

void Instances3D::putVisibleShadedModelInstancesImplementation(
	FunctionView<SharedPointer<ShaderPipelineImplementation>(const Model3D::ShaderConfiguration& shaderConfiguration)> shaderPipelineSelectorAdapter, const Model3D& model,
	StridedSpan<const resource::Model::TransformationView> transformations, StridedSpan<const ModelInstance3D> instances, StridedSpan<const bool> instancesVisible) {
	GREM_ASSERT(model.getJointCount() > 0);
	GREM_ASSERT(instances.size() == 1 || instances.size() == transformations.size());
	GREM_ASSERT(instancesVisible.size() == transformations.size());

	if (model.getNodes().empty() || transformations.empty() || !contains(instancesVisible, true)) {
		[[unlikely]];
		return;
	}

	const Model3DInfo modelInfo{model};

	const uint32_t jointRangeBase = static_cast<uint32_t>(joints.size());
	for (size_t i = 0; i < transformations.size(); ++i) {
		if (instancesVisible[i]) {
			for (const mat4& jointMatrix : Span{transformations[i].jointMatrices, modelInfo.jointCount}) {
				joints.push_back(Model3D::JointFields{.jointMatrix = jointMatrix});
			}
		}
	}
	jointsDirty = true;

	const uint32_t morphTargetWeightRangeBase = static_cast<uint32_t>(morphTargetWeights.size());
	if (modelInfo.morphTargetWeightCount > 0) {
		for (size_t i = 0; i < transformations.size(); ++i) {
			if (instancesVisible[i]) {
				for (const float weight : Span{transformations[i].morphTargetWeights, modelInfo.morphTargetWeightCount}) {
					morphTargetWeights.push_back(Model3D::MorphTargetWeightFields{.morphTargetWeight = weight});
				}
			}
		}
		morphTargetWeightsDirty = true;
	}

	for (const Model3D::Node& node : model.getNodes()) {
		const Model3DNodeInfo nodeInfo{node};

		SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle = shaderPipelineSelectorAdapter(node.shaderConfiguration);
		SharedPointer<MeshImplementation> meshHandle = node.mesh.lock();
		if ((node.shaderConfiguration.fragmentFlags & resource::Model::FRAGMENT_ALPHA_BLENDED) != 0) {
			uint32_t relativeUploadedTransformationIndex = 0;
			for (size_t i = 0; i < transformations.size(); ++i) {
				if (instancesVisible[i]) {
					if (transformations[i].jointsVisible[node.jointIndex]) {
						const ModelInstance3D& instance = (instances.size() == 1) ? instances.front() : instances[i];
						const Model3DInstanceInfo instanceInfo{instance, relativeUploadedTransformationIndex};

						const uint32_t instanceIndex = model3DInstanceBuffer.size();
						pushModel3DInstance(model3DInstanceBuffer, jointRangeBase, morphTargetWeightRangeBase, modelInfo, instanceInfo, nodeInfo);

						const mat4& jointMatrix = transformations[i].jointMatrices[nodeInfo.localJointIndex];
						const vec3 jointScaleSquared{length2(vec3{jointMatrix[0]}), length2(vec3{jointMatrix[1]}), length2(vec3{jointMatrix[2]})};
						transparent3DDrawCommands.push_back(Transparent3DDrawCommand{
							.position = vec3{jointMatrix[3]},
							.instanceIndex = instanceIndex,
							.shaderPipelineHandle = shaderPipelineHandle,
							.meshHandle = meshHandle,
							.boundingRadius = node.boundingRadius * sqrt(maxComponent(jointScaleSquared)),
							.distanceOrderingBias = instance.distanceOrderingBias,
						});
					}
					++relativeUploadedTransformationIndex;
				}
			}
		} else {
			const uint32_t instanceOffset = model3DInstanceBuffer.size();

			uint32_t relativeUploadedTransformationIndex = 0;
			for (size_t i = 0; i < transformations.size(); ++i) {
				if (instancesVisible[i]) {
					if (transformations[i].jointsVisible[node.jointIndex]) {
						const ModelInstance3D& instance = (instances.size() == 1) ? instances.front() : instances[i];
						const Model3DInstanceInfo instanceInfo{instance, relativeUploadedTransformationIndex};
						pushModel3DInstance(model3DInstanceBuffer, jointRangeBase, morphTargetWeightRangeBase, modelInfo, instanceInfo, nodeInfo);
					}
					++relativeUploadedTransformationIndex;
				}
			}

			const uint32_t instanceCount = model3DInstanceBuffer.size() - instanceOffset;
			opaqueModel3DDrawCommandBuffer.insertRange(std::move(shaderPipelineHandle), std::move(meshHandle), instanceOffset, instanceCount);
		}
	}
}

void Instances3D::putShadedFlatModelInstanceImplementation(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, const Model2D& model,
	const FlatModelInstance3D& instance) {
	const Texture& texture = (instance.texture) ? *instance.texture : renderer3D->getWhiteTexture2D();
	const uint32_t instanceOffset = model2DInstanceBuffer.push(Model2D::Instance{
		.instancePosition{0.0f, 0.0f},
		.instanceBasis{1.0f},
		.instanceTextureOffset = instance.textureOffset,
		.instanceTextureBasis = instance.textureBasis,
		.instanceTintColor = instance.color.toLinearRGBA(),
		.instanceEmissiveColor = instance.emissiveColor.toLinearRGB(),
	});
	const vec3 scaleSquared{length2(vec3{instance.transformation[0]}), length2(vec3{instance.transformation[1]}), length2(vec3{instance.transformation[2]})};
	for (const Model2D::Node& node : model.getNodes()) {
		transparent2DDrawCommands.push_back(Transparent2DDrawCommand{
			.transformation = instance.transformation,
			.shaderPipelineHandle = std::move(shaderPipelineHandle),
			.meshHandle = node.mesh.lock(),
			.textureHandle = texture.lock(),
			.instanceOffset = instanceOffset,
			.boundingRadius = node.boundingRadius * sqrt(maxComponent(scaleSquared)),
			.distanceOrderingBias = instance.distanceOrderingBias,
		});
	}
}

void Instances3D::putShadedTextInstanceImplementation(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, const Text2D& text, const TextInstance3D& instance) {
	if (text.getShapedGlyphs().empty()) {
		[[unlikely]];
		return;
	}
	for (const Text2D::ShapedGlyph& shapedGlyph : text.getShapedGlyphs()) {
		GREM_ASSERT(shapedGlyph.font);
		shapedGlyph.font->renderGlyph(*device, shapedGlyph.characterSize, shapedGlyph.codePoint);
	}
	const mat4 textTransformation = translateRotateScale(instance.position, instance.orientation, vec3{instance.scale, 1.0f});
	const vec2 alignmentOffset = text.getAlignmentOffset(instance.alignment);
	const Box<2, float> boundingBox = text.getBoundingBox();
	const float boundingRadius = length(max(boundingBox.max - boundingBox.min - alignmentOffset, -alignmentOffset));
	SharedPointer<MeshImplementation> meshHandle = renderer3D->getUnitSquareModel2D().getNodes().front().mesh.lock();
	const Font2D* font = text.getShapedGlyphs().front().font;
	uint32_t instanceOffset = model2DInstanceBuffer.size();
	for (const Text2D::ShapedGlyph& shapedGlyph : text.getShapedGlyphs()) {
		if (shapedGlyph.font != font) {
			transparent2DDrawCommands.push_back(Transparent2DDrawCommand{
				.transformation = textTransformation,
				.shaderPipelineHandle = shaderPipelineHandle,
				.meshHandle = meshHandle,
				.textureHandle = font->getAtlasTexture().lock(),
				.instanceOffset = instanceOffset,
				.boundingRadius = boundingRadius,
				.distanceOrderingBias = instance.distanceOrderingBias,
			});
			font = shapedGlyph.font;
			instanceOffset = model2DInstanceBuffer.size();
		}
		const vec2 textureSize = shapedGlyph.font->getAtlasTexture().getSize2D();
		const Optional<Font2D::RenderedGlyphInfo> renderedGlyphInfo = shapedGlyph.font->findRenderedGlyphInfo(shapedGlyph.characterSize, shapedGlyph.codePoint);
		GREM_ASSERT(renderedGlyphInfo);
		model2DInstanceBuffer.push(Model2D::Instance{
			.instancePosition = shapedGlyph.shapedOffset + alignmentOffset,
			.instanceBasis = mat2{scale(shapedGlyph.shapedSize)},
			.instanceTextureOffset = renderedGlyphInfo->positionInAtlas / textureSize,
			.instanceTextureBasis = mat2{scale(renderedGlyphInfo->sizeInAtlas / textureSize)},
			.instanceTintColor = instance.color.toLinearRGBA(),
			.instanceEmissiveColor{},
		});
	}
	transparent2DDrawCommands.push_back(Transparent2DDrawCommand{
		.transformation = textTransformation,
		.shaderPipelineHandle = std::move(shaderPipelineHandle),
		.meshHandle = std::move(meshHandle),
		.textureHandle = font->getAtlasTexture().lock(),
		.instanceOffset = instanceOffset,
		.boundingRadius = boundingRadius,
		.distanceOrderingBias = instance.distanceOrderingBias,
	});
}

} // namespace grem::graphics
