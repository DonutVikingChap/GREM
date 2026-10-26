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
	SharedPointer<ResourceImplementation> previousResource{};
	ResourceImplementation* nextResource = nullptr;
};

template <typename ResourceImplementation, typename... Args>
inline void ensureExclusiveResourceAccess(SharedPointer<ResourceImplementation>& implementation, auto createNewResource, auto resetOldResource, Args&&... args) {
	GREM_ASSERT(implementation);
	if (implementation.use_count() == 1) {
		[[likely]];
		return;
	}

	GREM_PROFILE_FUNCTION();
	for (ResourceImplementation* handle = implementation.get(); handle->previousResource; handle = handle->previousResource.get()) {
		if (handle->previousResource.use_count() == 1) {
			SharedPointer<ResourceImplementation> previousPreviousResource = std::exchange(handle->previousResource->previousResource, nullptr);
			ResourceImplementation* const nextResource = std::exchange(handle->previousResource->nextResource, nullptr);
			try {
				resetOldResource(*handle->previousResource, std::forward<Args>(args)...);
			} catch (...) {
				handle->previousResource->previousResource = std::move(previousPreviousResource);
				handle->previousResource->nextResource = nextResource;
				throw;
			}
			SharedPointer<ResourceImplementation> previousResource = std::exchange(handle->previousResource, nullptr);
			previousResource->previousResource = std::move(previousPreviousResource);
			previousResource->nextResource = nextResource;

			GREM_ASSERT(implementation->nextResource);
			implementation->nextResource->previousResource = std::move(implementation);

			implementation = std::move(previousResource);
			return;
		}
	}

	SharedPointer<ResourceImplementation> newResource = createNewResource(std::forward<Args>(args)...);
	newResource->nextResource = (implementation->nextResource) ? implementation->nextResource : implementation.get();
	implementation->nextResource = newResource.get();
	newResource->previousResource = std::move(implementation);
	implementation = std::move(newResource);
}

} // namespace detail

} // namespace grem::graphics

#endif
