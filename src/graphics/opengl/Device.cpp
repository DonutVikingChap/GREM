// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/FeatureSupport.hpp>
#include <GREM/graphics/RenderPass.hpp>
#include <GREM/graphics/Swapchain.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/Window.hpp>
#include <GREM/graphics/shaders.hpp>

#include "DeviceImplementation.hpp"
#include "RenderPassImplementation.hpp"
#include "TextureImplementation.hpp"

namespace grem::graphics {

void DeviceImplementation::cleanupRenderPassesAvailableForReuse() {
	for (const SharedPointer<RenderPassImplementation>& renderPass : renderPassesForReuse) {
		if (renderPass.use_count() == 1) {
			renderPass->reset();
		}
	}
}

Device::Device(Window& window, const DeviceOptions& options)
	: implementation(UniquePointer<DeviceImplementation>::create(window, options)) {
	constexpr uint32_t INITIAL_STORAGE_BUFFER_RESOLUTION = 64;
	implementation->storageBufferTexture = Texture::create(*this, TextureType::TEXTURE_2D, TextureFormat::R32G32B32A32_FLOAT, Extent2D{INITIAL_STORAGE_BUFFER_RESOLUTION}, 1,
		nullptr, TextureSamplerOptions::UNFILTERED);
	implementation->storageBufferSquareAllocator.expandTo(INITIAL_STORAGE_BUFFER_RESOLUTION);
}

Device::Device(Filesystem&, Window& window, const DeviceOptions& options)
	: Device(window, options) {}

Device::~Device() = default;

void Device::blit(TextureRegion2DReference renderTarget, TextureRegion2DConstReference renderSource, TextureFilter filter) {
	GREM_ASSERT(renderTarget.texture && *renderTarget.texture && renderTarget.texture->get()->maxMultisampleCount <= 1);
	GREM_ASSERT(renderSource.texture && *renderSource.texture && renderSource.texture->get()->maxMultisampleCount <= 1);
	implementation->blit(
		TextureSubresourceReference{
			.texture = renderTarget.texture,
			.subresource{
				.aspects = renderTarget.region.aspects,
				.layer = static_cast<uint32_t>(renderTarget.region.offset.z),
				.mipLevel = renderTarget.region.mipLevel,
			},
		},
		Region2D{.offset{.x = renderTarget.region.offset.x, .y = renderTarget.region.offset.y}, .size = renderTarget.region.size}, renderSource, filter);
}

void Device::blit(TextureSubresourceReference renderTarget, Offset2D targetOffset, TextureRegion2DConstReference renderSource) {
	GREM_ASSERT(renderTarget.texture && *renderTarget.texture);
	GREM_ASSERT(renderSource.texture && *renderSource.texture);
	GREM_ASSERT(renderTarget.texture->get()->maxMultisampleCount <= 1 || renderTarget.texture->get()->maxMultisampleCount == renderSource.texture->get()->maxMultisampleCount);
	implementation->blit(renderTarget, Region2D{.offset = targetOffset, .size = renderSource.region.size}, renderSource, TextureFilter::NEAREST);
}

void Device::render(const RenderPass& renderPass) {
	GREM_ASSERT(&renderPass.get()->device == this);
	implementation->currentPresentationSubmission.totalRenderPassStatistics += renderPass.getStatistics();
	++implementation->currentPresentationSubmission.totalRenderPassCount;
	implementation->cleanupRenderPassesAvailableForReuse();
	implementation->cleanupExpiredFramebufferContexts();
	renderPass.get()->render();
}

void Device::await() noexcept {
	implementation->await();
}

bool Device::awaitPresentation(const Swapchain&, PresentationSubmissionID, Duration) noexcept {
	return false;
}

Device::PresentationSubmission Device::present(Swapchain& swapchain) {
	return implementation->present(swapchain);
}

const FeatureSupport& Device::getSupportedFeatures() const noexcept {
	return implementation->supportedFeatures;
}

} // namespace grem::graphics
