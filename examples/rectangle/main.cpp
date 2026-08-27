// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

/**
 * # Rectangle Example
 *
 * This example shows a very basic application that renders a lime green
 * rectangle at a fixed size in the middle of a resizable window.
 */

#include <GREM/aliases.hpp>
#include <GREM/application.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>

// Define cross-platform entry point that makes main() more consistent.
#include <GREM/entry_point.hpp>

namespace {

class RectangleApplication final : public app::Application {
public:
	RectangleApplication() {
		resize(window.getDrawableSize());
	}

protected:
	void update(app::FrameInfo) override {
		for (const evt::Event& event : eventPump.pollEvents()) {
			match(event)( //
				[&](const evt::ApplicationQuitRequestedEvent&) { quit(); },
				[&](const evt::WindowDrawableSizeChangedEvent& drawableSizeChanged) {
					if (drawableSizeChanged.windowID == window.getID()) {
						resize(drawableSizeChanged.windowDrawableSize);
					}
				},
				[&](const auto&) {});
		}
	}

	void display(app::FrameInfo) override {
		instances2D.clear();
		instances2D.putRectangleInstance({
			.position = viewport.region.offset + viewport.region.size / 2,
			.size{100.0f, 60.0f},
			.origin{0.5f, 0.5f},
			.color = Color::LIME,
		});

		gfx::RenderPass renderPass{device, swapchain, gfx::ClearValues{.color = Color::BLACK}, viewport};
		renderer2D.drawFrame(renderPass, {instances2D}, camera2D);
		device.render(renderPass);

		device.present(swapchain);
	}

private:
	void resize(Extent2D newDrawableSize) {
		camera2D.setProjection(gfx::OrthographicProjection2D{.size = newDrawableSize});
		viewport.region = {.size = newDrawableSize};
	}

	evt::EventPump eventPump{};
	gfx::Window window{{.title = "Rectangle"}};
	gfx::Device device{window};
	gfx::Swapchain swapchain{device, window};
	gfx::Renderer2D renderer2D{device};
	gfx::Instances2D instances2D{device, renderer2D};
	gfx::Camera2D camera2D{device};
	gfx::Viewport viewport{};
};

} // namespace

int main(int, char*[]) {
	RectangleApplication application{};
	application.run();
	return app::ExitCode::SUCCESS;
}
