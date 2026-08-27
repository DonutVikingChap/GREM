// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics_3d/Decals3D.hpp>

#include "../../AssetCache.hpp"
#include "../../Graphics.hpp"
#include "../../Schema.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"
#include "../../game_components.hpp"
#include "../../game_data.hpp"

class DecalGraphicsStagingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"DecalGraphicsStagingSystem requires graphics."};
		}
		resources.addSharedResource<DecalAssets>();
		resources.addSharedResource<DecalInstances>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics* graphics) noexcept override {
		resources.removeResource<DecalInstances>();
		resources.removeResource<DecalAssets>();
		if (graphics) {
			graphics->decals.clearDecalMaterials();
		}
	}

	void reloadAssets(ResourceRegistry& resources, Audio*, Graphics* graphics) override {
		resources.getResource<DecalAssets>() = DecalAssets{};
		if (graphics) {
			graphics->decals.clearDecalMaterials();
		}
	}

	void stage3DGraphicsSharedBetweenLocalPlayers(exec::Executor&, Graphics& graphics, const WorldView& worldView) override {
		GREM_PROFILE_FUNCTION();

		AssetCache& assetCache = const_cast<AssetCache&>(worldView.subtickResources.getResource<AssetCache>());
		const Schema& schema = worldView.subtickResources.getResource<Schema>();
		DecalAssets& decalAssets = const_cast<DecalAssets&>(worldView.subtickResources.getResource<DecalAssets>());
		DecalInstances& decalInstances = const_cast<DecalInstances&>(worldView.subtickResources.getResource<DecalInstances>());

		for (auto&& [synchronizedEntityID, decalInstance] : decalInstances.instances) {
			decalInstance.found = false;
		}

		worldView.forEachEntityWithComponents<const DecalMaterialType, const DecalAttachmentFrame>(
			[&](SynchronizedEntityID synchronizedEntityID, EntityType, InterpolatedEntityView entity) -> void {
				const SynchronizedEntityID synchronizedTargetEntityID = entity.getNewAttribute<&DecalAttachmentFrame::target>();
				const EntityID targetEntityID = entity.snapshotB.resources.getResource<SynchronizedEntityMap>().findEntity(entity.snapshotB.registry, synchronizedTargetEntityID);
				if (!targetEntityID) {
					return;
				}

				const Optional<InterpolatedEntityView> targetEntity = worldView.findEntity(targetEntityID.getFlags(), synchronizedTargetEntityID);
				if (!targetEntity) {
					return;
				}

				const Optional<WorldTransformation> targetDisplayTransformation = worldView.getEntityDisplayTransformation(*targetEntity);
				if (!targetDisplayTransformation) {
					return;
				}

				const DecalMaterialType decalMaterialType = entity.getNewComponent<DecalMaterialType>();
				const gfx::DecalOptions3D interpolatedDecalOptions{
					.position = (*targetDisplayTransformation)(entity.getInterpolatedAttribute<&DecalAttachmentFrame::localOffset>()).in(phys::METERS),
					.range = entity.getInterpolatedAttribute<&DecalAttachmentFrame::range>().in(phys::METERS),
					.orientation = targetDisplayTransformation->orientation * entity.getInterpolatedAttribute<&DecalAttachmentFrame::localOrientation>(),
					.size = entity.getInterpolatedAttribute<&DecalAttachmentFrame::size>().in(phys::METERS),
					.modelInstanceIdentifier = static_cast<uint32_t>(synchronizedTargetEntityID.value % (uint64_t{Limits<uint32_t>::MAX} + 1)),
				};
				if (const auto it = decalInstances.instances.find(synchronizedEntityID); it != decalInstances.instances.end()) {
					it->second.found = true;
					if (it->second.materialType == decalMaterialType) {
						const gfx::DecalID decalID = it->second.id;
						graphics.decals.setDecalPosition(decalID, interpolatedDecalOptions.position);
						graphics.decals.setDecalRange(decalID, interpolatedDecalOptions.range);
						graphics.decals.setDecalOrientation(decalID, interpolatedDecalOptions.orientation);
						graphics.decals.setDecalSize(decalID, interpolatedDecalOptions.size);
						graphics.decals.setDecalModelInstanceIdentifier(decalID, interpolatedDecalOptions.modelInstanceIdentifier);
					} else {
						const gfx::DecalID decalID =
							graphics.decals.createDecal(decalAssets.getDecalMaterialID(graphics.decals, assetCache, schema, decalMaterialType), interpolatedDecalOptions);
						graphics.decals.destroyDecal(it->second.id);
						it->second.materialType = decalMaterialType;
						it->second.id = decalID;
					}
				} else {
					const gfx::DecalID decalID =
						graphics.decals.createDecal(decalAssets.getDecalMaterialID(graphics.decals, assetCache, schema, decalMaterialType), interpolatedDecalOptions);
					try {
						decalInstances.instances.emplace(synchronizedEntityID, DecalInstance{.materialType = decalMaterialType, .id = decalID, .found = true});
					} catch (...) {
						graphics.decals.destroyDecal(decalID);
						throw;
					}
				}
			});

		for (const auto& [synchronizedEntityID, decalInstance] : decalInstances.instances) {
			if (!decalInstance.found) {
				graphics.decals.destroyDecal(decalInstance.id);
			}
		}

		erase_if(decalInstances.instances, [&](const auto& kv) -> bool { return !kv.second.found; });
	}

private:
	struct DecalInstance {
		DecalMaterialType materialType;
		gfx::DecalID id;
		bool found;
	};

	struct DecalAssets {
		struct Material {
			[[nodiscard]] static gfx::DecalMaterialID createDecalMaterial(gfx::Decals3D& decals, AssetCache& assetCache, const Schema& schema,
				DecalMaterialType decalMaterialType) {
				const DecalMaterialDescription& decalMaterialDescription = schema.getDecalMaterialDescription(decalMaterialType);
				return decals.createDecalMaterial({
					.baseColorMapImage =
						(decalMaterialDescription.baseColorMapImageFilepath) ? *assetCache.getImage(*decalMaterialDescription.baseColorMapImageFilepath) : res::ImageView{},
					.normalMapImage = (decalMaterialDescription.normalMapImageFilepath) ? *assetCache.getImage(*decalMaterialDescription.normalMapImageFilepath) : res::ImageView{},
					.occlusionRoughnessMetallicMapImage = (decalMaterialDescription.occlusionRoughnessMetallicMapImageFilepath)
				                                              ? *assetCache.getImage(*decalMaterialDescription.occlusionRoughnessMetallicMapImageFilepath)
				                                              : res::ImageView{},
					.emissiveMapImage =
						(decalMaterialDescription.emissiveMapImageFilepath) ? *assetCache.getImage(*decalMaterialDescription.emissiveMapImageFilepath) : res::ImageView{},
					.baseColorFactor = decalMaterialDescription.baseColorFactor,
					.occlusionStrength = decalMaterialDescription.occlusionStrength,
					.roughnessFactor = decalMaterialDescription.roughnessFactor,
					.metallicFactor = decalMaterialDescription.metallicFactor,
					.normalScale = decalMaterialDescription.normalScale,
					.emissiveFactor = decalMaterialDescription.emissiveFactor,
				});
			}

			gfx::DecalMaterialID materialID;

			Material(gfx::Decals3D& decals, AssetCache& assetCache, const Schema& schema, DecalMaterialType decalMaterialType)
				: materialID(createDecalMaterial(decals, assetCache, schema, decalMaterialType)) {}
		};

		HashMap<DecalMaterialType, Material> materials{};

		[[nodiscard]] gfx::DecalMaterialID getDecalMaterialID(gfx::Decals3D& decals, AssetCache& assetCache, const Schema& schema, DecalMaterialType decalMaterialType) {
			return materials.try_emplace(decalMaterialType, decals, assetCache, schema, decalMaterialType).first->second.materialID;
		}
	};

	struct DecalInstances {
		OrderedMap<SynchronizedEntityID, DecalInstance> instances{};
	};
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createDecalGraphicsStagingSystem() { // NOLINT(misc-use-internal-linkage)
	return new DecalGraphicsStagingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
