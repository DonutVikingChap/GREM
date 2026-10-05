// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_SYSTEMS_OBJECT_HPP
#define GREM_EXAMPLES_PLATFORMER_SYSTEMS_OBJECT_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

struct System;

struct ObjectPreviousPosition : phys::Position3D {};
struct ObjectPreviousOrientation : phys::Orientation3D {};
struct ObjectPreviousScale : phys::Scale3D {};
struct ObjectPreviousLinearVelocity : phys::LinearVelocity3D {};

struct ObjectModelInstance {
	const gfx::Model3D* model;
	phys::LocalTransformation3D localTransformation{};
	res::Model::Pose pose{};
	res::Model::Transformation transformation{};
	bool visible = false;
};

struct ObjectAnimationState {
	struct AnimationLayer {
		res::Model::AnimationIndex animationIndex;
		phys::Time time;
		phys::Coefficient blendWeight;
		bool looping;

		[[nodiscard]] bool operator==(const AnimationLayer&) const = default;
	};

	SmallArrayList<AnimationLayer, 5> animationLayers{};
	SmallArrayList<AnimationLayer, 5> previousAnimationLayers = animationLayers;
	SmallArrayList<phys::Coefficient, 4> morphTargetWeights{};
	SmallArrayList<phys::Coefficient, 4> previousMorphTargetWeights = morphTargetWeights;
};

extern System* const objectSystem;
extern System* const objectPhysicsSystem;

void createObject(phys::EntityBuilder3D& entity, const phys::ResourceRegistry3D& resources, const gfx::Model3D& model, phys::ObjectOptions3D&& options);

void createAnimatedObject(phys::EntityBuilder3D& entity, const phys::ResourceRegistry3D& resources, const gfx::Model3D& model, phys::ObjectOptions3D&& options);

#endif
