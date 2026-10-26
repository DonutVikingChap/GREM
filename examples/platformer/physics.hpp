// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_PHYSICS_HPP
#define GREM_EXAMPLES_PLATFORMER_PHYSICS_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

inline constexpr phys::CollisionLayer COLLISION_LAYER_DEFAULT{0};
inline constexpr phys::CollisionLayer COLLISION_LAYER_LEVEL_GEOMETRY{1};
inline constexpr phys::CollisionLayer COLLISION_LAYER_PROP{2};
inline constexpr phys::CollisionLayer COLLISION_LAYER_PHYSICS_OBJECT{3};
inline constexpr phys::CollisionLayer COLLISION_LAYER_PLAYER{4};
inline constexpr phys::CollisionLayer COLLISION_LAYER_CAMERA{5};
inline constexpr phys::CollisionLayer COLLISION_LAYER_COLLECTIBLE{6};
inline constexpr phys::CollisionLayer COLLISION_LAYER_VEHICLE{7};

[[nodiscard]] inline res::Model::Transformation buildBindPoseTransformation(const res::Model& model) {
	res::Model::Transformation bindPoseTransformation{};
	bindPoseTransformation.assign(mat4{1.0f}, model.bindPose.localJoints, model.bindPose.localMorphTargetWeights, model.jointParentIndices);
	return bindPoseTransformation;
}

[[nodiscard]] inline phys::Shape3D buildModelShape(
	const res::Model& model, res::Model::TransformationView transformation,
	FunctionView<bool(res::Model::JointIndex jointIndex)> jointFilter = [](res::Model::JointIndex) -> bool { return true; }) {
	Buffer<ConvexPolytopeVertex3D> vertices{};
	for (const res::Model::Instance& instance : model.instances) {
		if (!jointFilter(instance.jointIndex)) {
			continue;
		}
		const res::Model::Mesh& mesh = model.meshes.at(instance.meshIndex);
		const mat4 jointMatrix = transformation.jointMatrices[((mesh.vertexFlags & res::Model::VERTEX_SKINNED) != 0) ? 0 : instance.jointIndex];
		const Span<const vec3> meshPositions{std::launder(reinterpret_cast<const vec3*>(
												 model.meshData.data() + static_cast<size_t>(mesh.meshDataOffset) * 4 + static_cast<size_t>(mesh.indexCount) * sizeof(uint32_t))),
			static_cast<size_t>(mesh.vertexCount)};
		vertices.reserve(vertices.size() + meshPositions.size());
		for (const vec3 position : meshPositions) {
			vertices.push_back(ConvexPolytopeVertex3D{vec3{jointMatrix * vec4{position, 1.0f}}});
		}
	}
	return phys::ConvexPolytopeShape3D{vertices, 16};
}

#endif
