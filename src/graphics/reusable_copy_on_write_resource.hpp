// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_REUSABLE_COPY_ON_WRITE_RESOURCE_HPP
#define GREM_GRAPHICS_REUSABLE_COPY_ON_WRITE_RESOURCE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/profiling.hpp>

#include <utility> // std::move, std::exchange, std::forward

namespace grem::graphics {

namespace detail {

// CRTP base class for shared resources that use a copy-on-write strategy to
// ensure exclusive access on modification.
template <typename ResourceImplementation>
struct ReusableCopyOnWriteResourceBase {
	// Linked list of previous states of this resource that previously had to be
	// copied in order to ensure exclusive access to the main resource state
	// (because a shared reference to it existed somewhere).
	//
	// The next time a copy needs to be made, this list will be traversed to
	// look for an old resource whose reference count has dropped back to 1,
	// meaning it has become safe to reuse. If none is found, a new entry is
	// added to the list. (See ensureExclusiveResourceAccess() below.)
	SharedPointer<ResourceImplementation> oldResource{};
};

template <typename ResourceImplementation, typename... Args>
inline void ensureExclusiveResourceAccess(SharedPointer<ResourceImplementation>& implementation, auto createNewResource, auto resetOldResource, Args&&... args) {
	if (implementation.use_count() == 1) {
		[[likely]];
		return;
	}

	GREM_PROFILE_FUNCTION();
	for (ResourceImplementation* handle = implementation.get(); handle->oldResource; handle = handle->oldResource.get()) {
		if (handle->oldResource.use_count() == 1) {
			// implementation -> [implementation] -> ... -> [handle] -> [oldResource] -> [oldOldResource] -> ...

			SharedPointer<ResourceImplementation> oldOldResource = std::exchange(handle->oldResource->oldResource, nullptr);
			// implementation -> [implementation] -> ... -> [handle] -> [oldResource] -> nullptr
			// oldOldResource -> [oldOldResource] -> ...

			resetOldResource(*handle->oldResource, std::forward<Args>(args)...);
			SharedPointer<ResourceImplementation> oldResource = std::move(handle->oldResource);
			// implementation -> [implementation] -> ... -> [handle] -> nullptr
			// oldOldResource -> [oldOldResource] -> ...
			// oldResource    -> [oldResource] -> nullptr

			handle->oldResource = std::move(oldOldResource);
			// implementation -> [implementation] -> ... -> [handle] -> [oldOldResource] -> ...
			// oldResource    -> [oldResource] -> nullptr

			oldResource->oldResource = std::move(implementation);
			// implementation -> nullptr
			// oldResource    -> [oldResource] -> [implementation] -> ... -> [handle] -> [oldOldResource] -> ...

			implementation = std::move(oldResource);
			// implementation -> [oldResource] -> [implementation] -> ... -> [handle] -> [oldOldResource] -> ...
			return;
		}
	}

	// implementation -> [implementation] -> [oldResource] -> ...

	SharedPointer<ResourceImplementation> newResource = createNewResource(std::forward<Args>(args)...);
	// implementation -> [implementation] -> [oldResource] -> ...
	// newResource    -> [newResource] -> nullptr

	newResource->oldResource = std::move(implementation);
	// implementation -> nullptr
	// newResource    -> [newResource] -> [implementation] -> [oldResource] -> ...

	implementation = std::move(newResource);
	// implementation -> [newResource] -> [implementation] -> [oldResource] -> ...
}

} // namespace detail

} // namespace grem::graphics

#endif
