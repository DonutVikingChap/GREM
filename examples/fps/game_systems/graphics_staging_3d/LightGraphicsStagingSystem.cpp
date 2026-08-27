// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/OrderedMap.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/execution/Executor.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/graphics_3d/Lights3D.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../Graphics.hpp"
#include "../../Schema.hpp"
#include "../../SynchronizedEntityMap.hpp"
#include "../../System.hpp"
#include "../../WorldView.hpp"
#include "../../game_components.hpp"
#include "../../game_data.hpp"

class LightGraphicsStagingSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void addRequiredResources(ResourceRegistry& resources, Audio*, Graphics* graphics, exec::Task::ParallelCount) override {
		if (!graphics) {
			throw Error{"LightGraphicsStagingSystem requires graphics."};
		}
		resources.addSharedResource<LightInstances>();
	}

	void removeResources(ResourceRegistry& resources, Audio*, Graphics* graphics) noexcept override {
		resources.removeResource<LightInstances>();
		if (graphics) {
			graphics->lights.clearLights();
		}
	}

	void stage3DGraphicsSharedBetweenLocalPlayers(exec::Executor&, Graphics& graphics, const WorldView& worldView) override {
		GREM_PROFILE_FUNCTION();

		LightInstances& lightInstances = const_cast<LightInstances&>(worldView.subtickResources.getResource<LightInstances>());

		// Ambient lights:
		unmarkLightInstances(lightInstances.ambientLightInstances);
		stageAndMarkEntityLightInstances<&gfx::Lights3D::createAmbientLight, &gfx::Lights3D::setAmbientLightOptions, const gfx::AmbientLightOptions3D>(graphics.lights,
			lightInstances.ambientLightInstances, worldView, [&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> Optional<gfx::AmbientLightOptions3D> {
				// Ambient light entity.
				return gfx::AmbientLightOptions3D{
					.color = entity.getInterpolatedAttribute<&gfx::AmbientLightOptions3D::color>(),
				};
			});
		destroyUnmarkedLightInstances(graphics.lights, lightInstances.ambientLightInstances);

		// Sun lights:
		unmarkLightInstances(lightInstances.sunLightInstances);
		stageAndMarkEntityLightInstances<&gfx::Lights3D::createSunLight, &gfx::Lights3D::setSunLightOptions, const gfx::SunLightOptions3D>(graphics.lights,
			lightInstances.sunLightInstances, worldView, [&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> Optional<gfx::SunLightOptions3D> {
				// Sun light entity.
				return gfx::SunLightOptions3D{
					.direction = entity.getInterpolatedAttribute<&gfx::SunLightOptions3D::direction>(),
					.color = entity.getInterpolatedAttribute<&gfx::SunLightOptions3D::color>(),
					.shadowMapped = entity.getNewAttribute<&gfx::SunLightOptions3D::shadowMapped>(),
				};
			});
		destroyUnmarkedLightInstances(graphics.lights, lightInstances.sunLightInstances);

		// Directional lights:
		unmarkLightInstances(lightInstances.directionalLightInstances);
		stageAndMarkEntityLightInstances<&gfx::Lights3D::createDirectionalLight, &gfx::Lights3D::setDirectionalLightOptions, const gfx::DirectionalLightOptions3D>(graphics.lights,
			lightInstances.directionalLightInstances, worldView, [&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> Optional<gfx::DirectionalLightOptions3D> {
				// Directional light entity.
				return gfx::DirectionalLightOptions3D{
					.direction = entity.getInterpolatedAttribute<&gfx::DirectionalLightOptions3D::direction>(),
					.color = entity.getInterpolatedAttribute<&gfx::DirectionalLightOptions3D::color>(),
					.shadowMapped = entity.getNewAttribute<&gfx::DirectionalLightOptions3D::shadowMapped>(),
				};
			});
		destroyUnmarkedLightInstances(graphics.lights, lightInstances.directionalLightInstances);

		// Point lights:
		unmarkLightInstances(lightInstances.pointLightInstances);
		stageAndMarkEntityLightInstances<&gfx::Lights3D::createPointLight, &gfx::Lights3D::setPointLightOptions, const gfx::PointLightOptions3D>(graphics.lights,
			lightInstances.pointLightInstances, worldView, [&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> Optional<gfx::PointLightOptions3D> {
				// Point light entity.
				return gfx::PointLightOptions3D{
					.position = entity.getInterpolatedAttribute<&gfx::PointLightOptions3D::position>(),
					.range = entity.getInterpolatedAttribute<&gfx::PointLightOptions3D::range>(),
					.color = entity.getInterpolatedAttribute<&gfx::PointLightOptions3D::color>(),
					.shadowMapped = entity.getNewAttribute<&gfx::PointLightOptions3D::shadowMapped>(),
				};
			});
		stageAndMarkEntityLightInstances<&gfx::Lights3D::createPointLight, &gfx::Lights3D::setPointLightOptions, const WeaponState, const WeaponType>(graphics.lights,
			lightInstances.pointLightInstances, worldView, [&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> Optional<gfx::PointLightOptions3D> {
				// Weapon muzzle flash point light.
				const Timestamp lastFiredTimestamp = entity.getNewAttribute<&WeaponState::lastFiredTimestamp>();
				const Timestamp timestamp = worldView.getEntityDisplayTimestamp(entity.entityIDs.second.getFlags());
				const phys::Time timeSinceFire = getTimeBetween(lastFiredTimestamp, timestamp, worldView.tickInterval);
				const WeaponDescription& weaponDescription = worldView.subtickResources.getResource<Schema>().getWeaponDescription(entity.getNewComponent<WeaponType>());
				if (lastFiredTimestamp == Timestamp{} || timeSinceFire < 0 || timeSinceFire >= weaponDescription.proceduralAnimation.muzzleFlashLightDuration) {
					return {};
				}

				const Optional<WorldTransformation> weaponDisplayTransformation = worldView.getWeaponEntityDisplayTransformation(entity);
				if (!weaponDisplayTransformation) {
					return {};
				}

				const phys::Position3D muzzleFlashPosition = (*weaponDisplayTransformation)(weaponDescription.proceduralAnimation.muzzleFlashLocalOffset);
				const phys::Distance muzzleFlashRange = weaponDescription.proceduralAnimation.muzzleFlashLightRange;
				const float muzzleFlashAmount = (1.0f - (timeSinceFire / weaponDescription.proceduralAnimation.muzzleFlashLightDuration));
				const Color muzzleFlashColor = weaponDescription.proceduralAnimation.muzzleFlashLightColor * Color::fromAlpha(muzzleFlashAmount);
				return gfx::PointLightOptions3D{
					.position = muzzleFlashPosition.in(phys::METERS),
					.range = muzzleFlashRange.in(phys::METERS),
					.color = muzzleFlashColor,
					.shadowMapped = true,
				};
			});
		destroyUnmarkedLightInstances(graphics.lights, lightInstances.pointLightInstances);

		// Spot lights:
		unmarkLightInstances(lightInstances.spotLightInstances);
		stageAndMarkEntityLightInstances<&gfx::Lights3D::createSpotLight, &gfx::Lights3D::setSpotLightOptions, const gfx::SpotLightOptions3D>(graphics.lights,
			lightInstances.spotLightInstances, worldView, [&](SynchronizedEntityID, EntityType, InterpolatedEntityView entity) -> Optional<gfx::SpotLightOptions3D> {
				// Spot light entity.
				return gfx::SpotLightOptions3D{
					.position = entity.getInterpolatedAttribute<&gfx::SpotLightOptions3D::position>(),
					.direction = entity.getInterpolatedAttribute<&gfx::SpotLightOptions3D::direction>(),
					.range = entity.getInterpolatedAttribute<&gfx::SpotLightOptions3D::range>(),
					.innerConeAngle = entity.getInterpolatedAttribute<&gfx::SpotLightOptions3D::innerConeAngle>(),
					.outerConeAngle = entity.getInterpolatedAttribute<&gfx::SpotLightOptions3D::outerConeAngle>(),
					.color = entity.getInterpolatedAttribute<&gfx::SpotLightOptions3D::color>(),
					.shadowMapped = entity.getNewAttribute<&gfx::SpotLightOptions3D::shadowMapped>(),
				};
			});
		stageAndMarkEntityLightInstances<&gfx::Lights3D::createSpotLight, &gfx::Lights3D::setSpotLightOptions, const phys::Position3D, const Aim, const FlashlightState>(
			graphics.lights, lightInstances.spotLightInstances, worldView,
			[&](SynchronizedEntityID synchronizedEntityID, EntityType, InterpolatedEntityView entity) -> Optional<gfx::SpotLightOptions3D> {
				// Flashlight spot light.
				if (!entity.getNewAttribute<&FlashlightState::on>()) {
					return {};
				}

				const phys::Position3D aimPosition =
					entity.getInterpolatedComponentWithMargin<phys::Position3D>(SnapshotInterpolationView::TELEPORTATION_MARGIN) + entity.getInterpolatedAttribute<&Aim::offset>();
				phys::PitchYaw displayAimAngles = entity.getInterpolatedAttribute<&Aim::angles>();
				if (entity.hasComponent<PlayerID>() && entity.getNewComponent<PlayerID>() == worldView.playerID) {
					const EntityID subtickEntityID = worldView.subtickResources.getResource<SynchronizedEntityMap>().findEntity(worldView.subtickRegistry, synchronizedEntityID);
					if (const LocalPlayerPerspective* const localPlayerPerspective = worldView.subtickRegistry.findComponent<LocalPlayerPerspective>(subtickEntityID)) {
						displayAimAngles = localPlayerPerspective->aimAngles;
					}
				}
				const phys::Direction3D aimDirection = convertAnglesToForwardDirection(displayAimAngles);
				const phys::Position3D flashlightPosition = aimPosition + phys::Y_AXIS_3D * -0.2_meters + aimDirection * 0.5_meters;
				const phys::Distance flashlightRange = entity.getInterpolatedAttribute<&FlashlightState::range>();
				const phys::Angle flashlightInnerConeAngle = entity.getInterpolatedAttribute<&FlashlightState::innerConeAngle>();
				const phys::Angle flashlightOuterConeAngle = entity.getInterpolatedAttribute<&FlashlightState::outerConeAngle>();
				const Color flashlightColor = entity.getInterpolatedAttribute<&FlashlightState::color>();
				return gfx::SpotLightOptions3D{
					.position = flashlightPosition.in(phys::METERS),
					.direction = aimDirection,
					.range = flashlightRange.in(phys::METERS),
					.innerConeAngle = flashlightInnerConeAngle.in(phys::RADIANS),
					.outerConeAngle = flashlightOuterConeAngle.in(phys::RADIANS),
					.color = flashlightColor,
					.shadowMapped = true,
				};
			});
		destroyUnmarkedLightInstances(graphics.lights, lightInstances.spotLightInstances);
	}

private:
	struct LightInstance {
		gfx::LightID id;
		bool found;
	};

	struct LightInstances {
		OrderedMap<SynchronizedEntityID, LightInstance> ambientLightInstances{};
		OrderedMap<SynchronizedEntityID, LightInstance> sunLightInstances{};
		OrderedMap<SynchronizedEntityID, LightInstance> directionalLightInstances{};
		OrderedMap<SynchronizedEntityID, LightInstance> pointLightInstances{};
		OrderedMap<SynchronizedEntityID, LightInstance> spotLightInstances{};
	};

	static void unmarkLightInstances(OrderedMap<SynchronizedEntityID, LightInstance>& instances) {
		for (auto&& [entityID, lightInstance] : instances) {
			lightInstance.found = false;
		}
	}

	template <auto CreateLight, auto SetLightOptions, typename... ComponentsAndExclusions>
	static void stageAndMarkEntityLightInstances(gfx::Lights3D& lights, OrderedMap<SynchronizedEntityID, LightInstance>& instances, const WorldView& worldView,
		auto getLightOptions) {
		worldView.forEachEntityWithComponents<ComponentsAndExclusions...>(
			[&](SynchronizedEntityID synchronizedEntityID, EntityType entityType, InterpolatedEntityView entity) -> void {
				if (const auto lightOptions = getLightOptions(synchronizedEntityID, entityType, entity)) {
					if (const auto it = instances.find(synchronizedEntityID); it != instances.end()) {
						it->second.found = true;
						(lights.*SetLightOptions)(it->second.id, *lightOptions);
					} else {
						const gfx::LightID lightID = (lights.*CreateLight)(*lightOptions);
						try {
							instances.emplace(synchronizedEntityID, LightInstance{.id = lightID, .found = true});
						} catch (...) {
							lights.destroyLight(lightID);
							throw;
						}
					}
				}
			});
	}

	static void destroyUnmarkedLightInstances(gfx::Lights3D& lights, OrderedMap<SynchronizedEntityID, LightInstance>& instances) {
		for (const auto& [entityID, lightInstance] : instances) {
			if (!lightInstance.found) {
				lights.destroyLight(lightInstance.id);
			}
		}
		erase_if(instances, [&](const auto& kv) -> bool { return !kv.second.found; });
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createLightGraphicsStagingSystem() { // NOLINT(misc-use-internal-linkage)
	return new LightGraphicsStagingSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
