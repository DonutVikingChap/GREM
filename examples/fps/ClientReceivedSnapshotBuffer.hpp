// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CLIENT_RECEIVED_SNAPSHOT_BUFFER_HPP
#define GREM_EXAMPLES_FPS_CLIENT_RECEIVED_SNAPSHOT_BUFFER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/fundamentals.hpp>

#include "Snapshot.hpp"
#include "Timestamp.hpp"

class ClientReceivedSnapshotBuffer {
public:
	void reset(TickIndex tickIndex) noexcept {
		GREM_ASSERT(isSorted(snapshots, Snapshot::Compare{}));

		snapshots.clear();
		latestReceivedSnapshotTickIndex = tickIndex;
	}

	bool insertReceivedSnapshot(const Snapshot& initialSnapshot, TickIndex oldTickIndex, TickIndex newTickIndex, auto applyDelta) {
		GREM_ASSERT(newTickIndex > TickIndex{});
		GREM_ASSERT(isSorted(snapshots, Snapshot::Compare{}));

		auto itNewSnapshot = lowerBound(snapshots, newTickIndex, Snapshot::Compare{});
		if (itNewSnapshot != snapshots.end() && itNewSnapshot->tickIndex == newTickIndex) {
			return false;
		}

		size_t newSnapshotIndex = static_cast<size_t>(itNewSnapshot - snapshots.begin());
		while (snapshots.size() >= SNAPSHOT_BUFFER_WINDOW_SIZE + SNAPSHOT_BUFFER_WINDOW_MARGIN) {
			if (newSnapshotIndex == 0) {
				return false;
			}
			snapshots.pop_front();
			--newSnapshotIndex;
		}

		itNewSnapshot = snapshots.insert_unspecified_value(snapshots.begin() + static_cast<ptrdiff_t>(newSnapshotIndex));
		if (oldTickIndex == TickIndex{}) {
			*itNewSnapshot = initialSnapshot;
		} else {
			const auto itOldSnapshot = lowerBound(snapshots, oldTickIndex, Snapshot::Compare{});
			const bool foundOldSnapshot = itOldSnapshot != snapshots.end() && itOldSnapshot->tickIndex == oldTickIndex;
			if (!foundOldSnapshot) {
				snapshots.erase(itNewSnapshot);
				return false;
			}
			*itNewSnapshot = *itOldSnapshot;
		}

		if (newTickIndex > latestReceivedSnapshotTickIndex) {
			latestReceivedSnapshotTickIndex = newTickIndex;
		}
		applyDelta(*itNewSnapshot);
		GREM_ASSERT(itNewSnapshot->tickIndex == newTickIndex);
		return true;
	}

	[[nodiscard]] SnapshotBufferView getSnapshots() const {
		return snapshots;
	}

	[[nodiscard]] bool isSnapshotReceived(TickIndex tickIndex) const {
		const auto it = lowerBound(snapshots, tickIndex, Snapshot::Compare{});
		return it != snapshots.end() && it->tickIndex == tickIndex;
	}

	[[nodiscard]] TickIndex getLatestReceivedSnapshotTickIndex() const {
		return latestReceivedSnapshotTickIndex;
	}

private:
	SnapshotBuffer snapshots{};
	DoubleEndedQueue<bool> snapshotsReceived{};
	TickIndex latestReceivedSnapshotTickIndex{};
};

#endif
