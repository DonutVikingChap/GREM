// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics/SpriteAtlas.hpp>
#include <GREM/graphics_2d/Instances2D.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../AssetCache.hpp"
#include "../../EntityType.hpp"
#include "../../Graphics.hpp"
#include "../../Schema.hpp"
#include "../../Snapshot.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"
#include "../../game_components.hpp"

#include <utility> // std::move

class SpriteGraphicsStagingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"SpriteGraphicsStagingSystem requires graphics."};
		}
		resources.addSharedResource<SpriteAssets>(graphics->device, resources.getResource<AssetCache>(), resources.getResource<Schema>());
		resources.addSharedResource<SpriteInstances>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics*) noexcept override {
		resources.removeResource<SpriteInstances>();
		resources.removeResource<SpriteAssets>();
	}

	void reloadAssets(ResourceRegistry& resources, Audio*, Graphics* graphics) override {
		GREM_ASSERT(graphics);
		resources.getResource<SpriteAssets>() = SpriteAssets{graphics->device, resources.getResource<AssetCache>(), resources.getResource<Schema>()};
	}

	void stageLocalPlayer3DGraphics(exec::Executor& executor, Graphics& graphics, const WorldView& worldView, const LocalPlayerID&, const gfx::Viewport&,
		const gfx::Camera3D& camera) override {
		GREM_PROFILE_FUNCTION();

		const SpriteAssets& spriteAssets = worldView.subtickResources.getResource<SpriteAssets>();
		SpriteInstances& spriteInstances = const_cast<SpriteInstances&>(worldView.subtickResources.getResource<SpriteInstances>());
		const Schema& schema = worldView.subtickResources.getResource<Schema>();

		const phys::Position3D cameraPosition = (transpose(mat3{camera.getViewMatrix()}) * -vec3{camera.getViewMatrix()[3]}) * phys::METERS;

		{
			GREM_PROFILE_BLOCK("Allocate sprite instances");
			spriteInstances.instances.clear();
			worldView.forEachEntityWithComponents<const SpriteType>([&](SynchronizedEntityID synchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
				if (const auto it = spriteAssets.info.find(entity.getNewComponent<SpriteType>()); it != spriteAssets.info.end()) {
					spriteInstances.instances.push_back(SpriteInstance{
						.spriteInfoIndex = static_cast<uint32_t>(it.getIndex()),
						.entityFlags = entity.entityIDs.second.getFlags(),
						.synchronizedEntityID = synchronizedEntityID,
						.spriteID{},
						.instance{},
					});
				}
			});
		}

		{
			GREM_PROFILE_BLOCK("Pose sprite instances");
			executor.executeParallelOperation(spriteInstances.instances, [&](SpriteInstance& instance) -> void {
				const auto& [spriteType, spriteInfo] = spriteAssets.info.getAtIndex(instance.spriteInfoIndex);
				GREM_ASSERT(!spriteInfo.frameSpriteIDs.empty());

				const EntityID::Flags entityFlags = instance.entityFlags;
				const SynchronizedEntityID synchronizedEntityID = instance.synchronizedEntityID;
				const Optional<InterpolatedEntityView> entity = worldView.findEntity(entityFlags, synchronizedEntityID);
				if (!entity || !entity->hasComponent<SpriteAnimationState>()) {
					return;
				}

				const Optional<WorldTransformation> displayTransformation = worldView.getEntityDisplayTransformation(*entity);
				if (!displayTransformation) {
					return;
				}

				const Timestamp timestamp = worldView.getEntityDisplayTimestamp(entityFlags);
				const phys::Time animationTime = getTimeBetween(entity->getNewAttribute<&SpriteAnimationState::animationStartTimestamp>(), timestamp, worldView.tickInterval);
				if (animationTime < 0) {
					return;
				}

				const SpriteDescription& spriteDescription = schema.getSpriteDescription(spriteType);
				size_t frameIndex = static_cast<size_t>(animationTime * spriteDescription.frameRate);
				if (spriteDescription.looping) {
					frameIndex %= spriteInfo.frameSpriteIDs.size();
				} else if (frameIndex >= spriteInfo.frameSpriteIDs.size()) {
					return;
				}

				const phys::Orientation3D alignedOrientation =
					worldView.getAlignedOrientation(displayTransformation->orientation * spriteDescription.localOrientation, spriteDescription.orientationAlignment, camera);

				const phys::Position3D spritePosition = (*displayTransformation)(spriteDescription.localOffset);
				const phys::Orientation3D spriteOrientation = alignedOrientation;
				const phys::Length2D spriteSize = spriteDescription.size * inverse(spriteDescription.localOrientation)(displayTransformation->scale).get(phys::X, phys::Y);
				if (!worldView.isPotentiallyVisible(phys::Sphere3D{.center = spritePosition, .radius = length(spriteSize)})) {
					return;
				}

				const phys::Direction3D viewDirection = normalize(cameraPosition - spritePosition);
				const phys::Scale1D unalignedAngleCosine = abs(dot(viewDirection, displayTransformation->orientation(phys::Z_AXIS_3D)));
				const phys::Scale1D alignedAngleCosine = abs(dot(viewDirection, spriteOrientation(phys::Z_AXIS_3D)));
				const phys::Coefficient alpha = min(1_x,
					mix(1_x, unalignedAngleCosine, spriteDescription.unalignedAngularFadeFactor) * mix(1_x, alignedAngleCosine, spriteDescription.alignedAngularFadeFactor));
				if (alpha <= 0) {
					return;
				}

				instance.spriteID = spriteInfo.frameSpriteIDs[frameIndex];
				instance.instance = {
					.position = spritePosition.in(phys::METERS),
					.orientation = spriteOrientation,
					.size = spriteSize.in(phys::METERS),
					.origin{0.5f, 0.5f},
					.color = spriteDescription.tintColor * Color::fromAlpha(alpha),
				};
			});
		}

		{
			GREM_PROFILE_BLOCK("Put sprite instances");
			for (const SpriteInstance& instance : spriteInstances.instances) {
				if (instance.spriteID) {
					graphics.localPlayerVisibleInWorldInstances3D.putSpriteInstance(spriteAssets.spriteAtlas, instance.spriteID, instance.instance);
				}
			}
		}

		{
			GREM_PROFILE_BLOCK("Pose and put muzzle flash sprite instances");
			worldView.forEachEntityWithComponents<const WeaponState, const WeaponType>([&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
				const Timestamp lastFiredTimestamp = entity.getNewAttribute<&WeaponState::lastFiredTimestamp>();
				const Timestamp timestamp = worldView.getEntityDisplayTimestamp(entity.entityIDs.second.getFlags());
				const phys::Time timeSinceFire = getTimeBetween(lastFiredTimestamp, timestamp, worldView.tickInterval);
				if (lastFiredTimestamp == Timestamp{} || timeSinceFire < 0) {
					return;
				}

				const Optional<WorldTransformation> weaponDisplayTransformation = worldView.getWeaponEntityDisplayTransformation(entity);
				if (!weaponDisplayTransformation) {
					return;
				}

				const WeaponDescription& weaponDescription = schema.getWeaponDescription(entity.getNewComponent<WeaponType>());
				const rng::Xoroshiro128PlusPlusEngine::result_type seed =
					static_cast<rng::Xoroshiro128PlusPlusEngine::result_type>(lastFiredTimestamp.getTickIndex() - TickIndex{});
				rng::Xoroshiro128PlusPlusEngine numberGenerator{seed};
				rng::UniformRealDistribution<float> rollDistribution{0.0f, 2.0f * numbers::PI};
				rng::UniformRealDistribution<float> scaleDistribution{0.75f, 1.0f};
				const phys::Orientation3D muzzleFlashLocalOrientation =
					phys::Orientation3D::roll(rollDistribution(numberGenerator)) * weaponDescription.proceduralAnimation.muzzleFlashLocalOrientation;
				const phys::Position3D muzzleFlashPosition = (*weaponDisplayTransformation)(weaponDescription.proceduralAnimation.muzzleFlashLocalOffset);
				const phys::Orientation3D muzzleFlashOrientation = weaponDisplayTransformation->orientation * muzzleFlashLocalOrientation;
				const phys::Scale3D muzzleFlashScale = weaponDisplayTransformation->scale * scaleDistribution(numberGenerator);
				for (const SpriteType spriteType : weaponDescription.muzzleFlashSpriteTypes) {
					const auto itSpriteInfo = spriteAssets.info.find(spriteType);
					if (itSpriteInfo == spriteAssets.info.end()) {
						continue;
					}
					const SpriteAssets::SpriteInfo& spriteInfo = itSpriteInfo->second;
					GREM_ASSERT(!spriteInfo.frameSpriteIDs.empty());

					const SpriteDescription& spriteDescription = schema.getSpriteDescription(spriteType);
					const size_t frameIndex = static_cast<size_t>(timeSinceFire * spriteDescription.frameRate);
					if (frameIndex >= spriteInfo.frameSpriteIDs.size()) {
						continue;
					}

					const phys::Position3D spritePosition =
						muzzleFlashPosition + weaponDisplayTransformation->orientation(weaponDisplayTransformation->scale * spriteDescription.localOffset);
					const phys::Orientation3D spriteOrientation =
						worldView.getAlignedOrientation(muzzleFlashOrientation * spriteDescription.localOrientation, spriteDescription.orientationAlignment, camera);
					const phys::Length2D spriteSize = spriteDescription.size * inverse(spriteDescription.localOrientation)(muzzleFlashScale).get(phys::X, phys::Y);
					if (!worldView.isPotentiallyVisible(phys::Sphere3D{.center = spritePosition, .radius = length(spriteSize)})) {
						continue;
					}

					const phys::Direction3D viewDirection = normalize(cameraPosition - spritePosition);
					const phys::Scale1D unalignedAngleCosine = abs(dot(viewDirection, muzzleFlashOrientation(phys::Z_AXIS_3D)));
					const phys::Scale1D alignedAngleCosine = abs(dot(viewDirection, spriteOrientation(phys::Z_AXIS_3D)));
					const phys::Coefficient alpha = min(1_x,
						mix(1_x, unalignedAngleCosine, spriteDescription.unalignedAngularFadeFactor) * mix(1_x, alignedAngleCosine, spriteDescription.alignedAngularFadeFactor));
					if (alpha <= 0) {
						continue;
					}

					const gfx::SpriteID spriteID = spriteInfo.frameSpriteIDs[frameIndex];
					graphics.localPlayerVisibleInWorldInstances3D.putSpriteInstance(spriteAssets.spriteAtlas, spriteID,
						gfx::SpriteInstance3D{
							.position = spritePosition.in(phys::METERS),
							.orientation = spriteOrientation,
							.size = spriteSize.in(phys::METERS),
							.origin{0.5f, 0.5f},
							.color = spriteDescription.tintColor * Color::fromAlpha(alpha),
							.distanceOrderingBias = spriteDescription.distanceOrderingBias.in(phys::METERS),
						});
				}
			});
		}
	}

private:
	struct SpriteAssets {
		struct SpriteInfo {
			ArrayList<gfx::SpriteID> frameSpriteIDs{};
		};

		gfx::SpriteAtlas spriteAtlas;
		HashMap<SpriteType, SpriteInfo> info{};

		SpriteAssets(gfx::Device& device, AssetCache& assetCache, const Schema& schema)
			: spriteAtlas(device, {.padding = 8}) {
			info.reserve(schema.getSpriteDescriptions().size());
			for (const auto& [spriteType, spriteDescription] : schema.getSpriteDescriptions()) {
				gfx::SpriteOptions::Flip flip = gfx::SpriteOptions::NO_FLIP;
				if (spriteDescription.flipHorizontally) {
					flip |= gfx::SpriteOptions::FLIP_HORIZONTALLY;
				}
				if (spriteDescription.flipVertically) {
					flip |= gfx::SpriteOptions::FLIP_VERTICALLY;
				}
				const gfx::SpriteID spriteID = spriteAtlas.insertSprite(*assetCache.getImage(spriteDescription.imageFilepath), {.flip = flip});
				SpriteInfo spriteInfo{};
				if (spriteDescription.frameCount == 1 && spriteDescription.framePadding == 0) {
					spriteInfo.frameSpriteIDs = {spriteID};
				} else {
					const vec2 spriteSize = spriteAtlas.getSprite(spriteID).size;
					const uint32_t paddedFrameWidth = static_cast<uint32_t>(spriteDescription.frameSize.x * spriteSize.x) + spriteDescription.framePadding * 2;
					const uint32_t paddedFrameHeight = static_cast<uint32_t>(spriteDescription.frameSize.y * spriteSize.y) + spriteDescription.framePadding * 2;
					const uint32_t frameCountX = static_cast<uint32_t>(spriteSize.x) / paddedFrameWidth;
					const uint32_t frameCountY = static_cast<uint32_t>(spriteSize.y) / paddedFrameHeight;
					for (uint32_t y = 0; y < frameCountY; ++y) {
						if (spriteInfo.frameSpriteIDs.size() >= spriteDescription.frameCount) {
							break;
						}
						const uint32_t offsetY = (spriteDescription.framesTopToBottom)
						                             ? static_cast<uint32_t>(spriteSize.y) - spriteDescription.framePadding - paddedFrameHeight * (1 + y)
						                             : spriteDescription.framePadding + paddedFrameHeight * y;
						for (uint32_t x = 0; x < frameCountX; ++x) {
							if (spriteInfo.frameSpriteIDs.size() >= spriteDescription.frameCount) {
								break;
							}
							const uint32_t offsetX = (spriteDescription.framesRightToLeft)
							                             ? static_cast<uint32_t>(spriteSize.x) - spriteDescription.framePadding - paddedFrameWidth * (1 + x)
							                             : spriteDescription.framePadding + paddedFrameWidth * x;
							spriteInfo.frameSpriteIDs.push_back(spriteAtlas.createSubSprite(spriteID,
								Region2D{.offset{static_cast<int32_t>(offsetX), static_cast<int32_t>(offsetY)}, .size{paddedFrameWidth, paddedFrameHeight}}));
						}
					}
					if (spriteInfo.frameSpriteIDs.empty()) {
						throw Error{"No frames in sprite."};
					}
				}
				info.emplace(spriteType, std::move(spriteInfo));
			}
		}
	};

	struct SpriteInstance {
		uint32_t spriteInfoIndex;
		EntityID::Flags entityFlags;
		SynchronizedEntityID synchronizedEntityID;
		gfx::SpriteID spriteID;
		gfx::SpriteInstance3D instance;
	};

	struct SpriteInstances {
		ArrayList<SpriteInstance> instances{};
	};
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createSpriteGraphicsStagingSystem() { // NOLINT(misc-use-internal-linkage)
	return new SpriteGraphicsStagingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
