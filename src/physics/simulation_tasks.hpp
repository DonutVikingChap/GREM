// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_SIMULATION_TASKS_HPP
#define GREM_PHYSICS_SIMULATION_TASKS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/BitBuffer.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/physics/EntityID.hpp>
#include <GREM/physics/Simulation.hpp>

namespace grem::physics {

namespace detail {

struct InvalidatedJoints : Buffer<EntityID> {};

using ContactIndex = uint32_t;
struct ActiveContactList : Buffer<ContactIndex> {};

class ContactColor {
public:
	void clear() noexcept {
		objectBits.clear();
		contactIndices.clear();
	}

	bool tryAddContact(ContactIndex contactIndex, EntityID::Index objectIndexA, bool isCorrectableA, EntityID::Index objectIndexB, bool isCorrectableB) {
		if ((isCorrectableA && objectIndexA < objectBits.size() && objectBits[objectIndexA]) || //
			(isCorrectableB && objectIndexB < objectBits.size() && objectBits[objectIndexB])) {
			return false;
		}
		contactIndices.push_back(contactIndex);
		if (isCorrectableA) {
			if (objectIndexA >= objectBits.size()) {
				[[unlikely]];
				objectBits.resize(objectIndexA + 1, uint8_t{});
			}
			objectBits[objectIndexA] = true;
		}
		if (isCorrectableB) {
			if (objectIndexB >= objectBits.size()) {
				[[unlikely]];
				objectBits.resize(objectIndexB + 1, uint8_t{});
			}
			objectBits[objectIndexB] = true;
		}
		return true;
	}

	[[nodiscard]] Span<const ContactIndex> getContactIndices() const noexcept {
		return contactIndices;
	}

private:
	BitBuffer objectBits{};
	Buffer<ContactIndex> contactIndices{};
};

template <size_t N>
struct ContactColorGraph : Array<ContactColor, SimulationOptions<N>::MAX_CONTACT_COLOR_COUNT> {
	template <size_t ColorIndex>
	[[nodiscard]] static Span<const ContactIndex> getColoredContactIndices(const ContactColorGraph& contactColorGraph) {
		return contactColorGraph[ColorIndex].getContactIndices();
	}
};

struct ContactColorOverflow : Buffer<ContactIndex> {};

template <size_t N>
struct ContactManifoldInvalidation {
	struct DelayedContactManifold {
		ContactIndex contactIndex;
		uint32_t manifoldIndex;
		InplaceArrayList<Position<N>, 2> featurePoints;
		uint32_t facePointOffset;
		uint32_t facePointCount;
		Length1D largestPenetrationDepth;
	};

	ArrayList<Position<N>> voidedPoints{};
	ArrayList<Position<N>> facePoints{};
	ArrayList<DelayedContactManifold> delayedContactManifolds{};
	ArrayList<ContactIndex> affectedContacts{};
};

GREM_API(physics) void updateBroadphase2D(EntityRegistry2D& registry, ResourceRegistry2D& resources);
GREM_API(physics) void updateBroadphase3D(EntityRegistry3D& registry, ResourceRegistry3D& resources);

GREM_API(physics) void scheduleStep2D(Scheduler2D& scheduler, const SimulationOptions2D& simulationOptions, const ScheduleStepOptions2D& scheduleStepOptions);
GREM_API(physics) void scheduleStep3D(Scheduler3D& scheduler, const SimulationOptions3D& simulationOptions, const ScheduleStepOptions3D& scheduleStepOptions);

GREM_API(physics) void scheduleBroadphaseUpdate2D(Scheduler2D& scheduler, const SimulationOptions2D& simulationOptions);
GREM_API(physics) void scheduleBroadphaseUpdate3D(Scheduler3D& scheduler, const SimulationOptions3D& simulationOptions);

GREM_API(physics) void scheduleCollisionDetection2D(Scheduler2D& scheduler, const SimulationOptions2D& simulationOptions);
GREM_API(physics) void scheduleCollisionDetection3D(Scheduler3D& scheduler, const SimulationOptions3D& simulationOptions);

} // namespace detail

} // namespace grem::physics

#endif
