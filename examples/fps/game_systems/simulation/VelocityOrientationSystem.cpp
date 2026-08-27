// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/aliases.hpp>
#include <GREM/execution/Task.hpp>
#include <GREM/physics/quantities.hpp>

#include "../../System.hpp"
#include "../../game_components.hpp"

class VelocityOrientationSystem final : public System { // NOLINT(misc-use-internal-linkage)
public:
	void scheduleTick(Scheduler& scheduler, const ResourceRegistry&, exec::Task::ParallelCount) override {
		scheduler.addTask<orientTaggedObjectsByVelocity>("Orient tagged objects by velocity");
	}

private:
	static void orientTaggedObjectsByVelocity(Entities<phys::Orientation3D, const phys::LinearVelocity3D, const OrientPhysicsObjectByVelocity> entities) {
		for (auto&& [entityID, orientation, linearVelocity, orientPhysicsObjectByVelocity] : entities) {
			if (const Optional<phys::Direction3D> linearVelocityDirection = tryNormalize(linearVelocity)) {
				orientation = phys::Orientation3D::lookAt(*linearVelocityDirection, phys::Y_AXIS_3D) * orientPhysicsObjectByVelocity.localOrientation;
			}
		}
	}
};

#ifdef GREM_SHARED_LIBRARY
extern "C" GREM_EXPORT System* ExampleFPS_createVelocityOrientationSystem() { // NOLINT(misc-use-internal-linkage)
	return new VelocityOrientationSystem{};                                   // NOLINT(cppcoreguidelines-owning-memory)
}
#endif
