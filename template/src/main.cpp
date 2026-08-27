#include <GREM/aliases.hpp>
#include <GREM/application.hpp>
#include <GREM/audio.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>
#include <GREM/graphics_3d.hpp>

// Define cross-platform entry point that makes main() more consistent.
#include <GREM/entry_point.hpp>

namespace {

struct ApplicationOptions {
	// TODO: Decide which of these options to keep exposed to the command line, and their initial values.
	app::ApplicationOptions application{
		.tickInterval = 1.0_seconds / 60,
		.maxAccumulatedTickTime = 1.0_seconds,
		.minFrameTime = 1.0_seconds / 480,
		.maxFrameTime = 1.0_seconds / 5,
		.frameRateLimiterSleepEnabled = true,
	};
	evt::EventPumpOptions eventPump{
		.enableControllerSupport = true,
		.useRawMouseInput = true,
	};
	gfx::WindowOptions window{
		.title = "Application",
		.size{800, 600},
		.multisampleCount = 1,
		.opacity = 1.0f,
		.hidden = false,
		.focus = true,
		.minimized = false,
		.resizable = true,
		.fullscreen = false,
		.borderless = false,
		.hideFromTaskbar = false,
		.alwaysOnTop = false,
		.relativeMouseMode = false,
	};
	gfx::DeviceOptions device{
		.shaderCacheInputFilepath = "shader_cache.dat",
		.shaderCacheOutputFilepath = "shader_cache.dat",
	};
	gfx::SwapchainOptions swapchain{
		.maxBufferedFrameCount = 0,
		.useVerticalSynchronization = false,
	};
	gfx::Renderer2DOptions renderer2D{};
	gfx::Renderer3DOptions renderer3D{};
	aud::SoundStageOptions soundStage{
		.outputChannelCount = 2,
		.outputVolume = 1.0f,
		.speedOfSound = 343.3f,
		.maxSimultaneousSoundInstanceCount = 128,
		.useRoundoff = true,
		.enableStatistics = false,
	};
	evt::InputManagerOptions inputManager{
		.preferences{
			.mouseSensitivity{convertDegreesToRadians(0.022f)},
			.mouseWheelScrollSensitivity{1.0f},
			.touchMotionSensitivity{1.0f},
			.controllerLeftStickSensitivity{1.0f / 32767.0f},
			.controllerRightStickSensitivity{1.0f / 32767.0f},
			.controllerLeftStickCurveExponent = 1.0f,
			.controllerRightStickCurveExponent = 1.0f,
			.touchPressureLowerDeadzone = 0.25f,
			.touchPressureUpperDeadzone = 0.9f,
			.controllerLeftStickInnerDeadzone = 0.25f,
			.controllerLeftStickOuterDeadzone = 0.9f,
			.controllerRightStickInnerDeadzone = 0.25f,
			.controllerRightStickOuterDeadzone = 0.9f,
			.controllerLeftTriggerLowerDeadzone = 0.2f,
			.controllerLeftTriggerUpperDeadzone = 0.9f,
			.controllerRightTriggerLowerDeadzone = 0.2f,
			.controllerRightTriggerUpperDeadzone = 0.9f,
		},
		.emitOutputEvents = false,
	};
	float verticalFieldOfViewInDegrees = 74.0f;
	// TODO: Add more app-specific options here.
};

class Application final : public app::Application {
public:
	static constexpr CStringView APPLICATION_CONFIGURATION_FILEPATH = "configuration/application.json";
	static constexpr CStringView INPUT_CONFIGURATION_FILEPATH = "configuration/input.json";

	enum class Action : uint8_t {
		CONFIRM = 0,
		CANCEL = 1,
		MOVE_UP = 2,
		MOVE_DOWN = 3,
		MOVE_LEFT = 4,
		MOVE_RIGHT = 5,
	};

	Application(Filesystem& filesystem, const ApplicationOptions& options)
		: app::Application(options.application)
		, eventPump(options.eventPump)
		, window(options.window)
		, device(filesystem, window, options.device)
		, swapchain(device, window, options.swapchain)
		, renderer2D(device, options.renderer2D)
		, renderer3D(device, renderer2D, options.renderer3D)
		, soundStage(options.soundStage)
		, inputManager(options.inputManager)
		, verticalFieldOfView(convertDegreesToRadians(options.verticalFieldOfViewInDegrees)) {
		inputManager.loadConfiguration<Action>(filesystem, INPUT_CONFIGURATION_FILEPATH);

		resize(window.getDrawableSize());

		(void)filesystem; // TODO: Load assets, initialize state, etc.
	}

protected:
	void update(app::FrameInfo frameInfo) override {
		handleEvents();

		soundStage.update(listener);

		(void)frameInfo; // TODO: Update movement directions, aim angles, etc.
	}

	void tick(app::TickInfo tickInfo) override {
		(void)tickInfo; // TODO: Tick game state, step physics simulation, etc.
	}

	void display(app::FrameInfo frameInfo) override {
		(void)frameInfo; // TODO: Interpolate animations, camera, listener, etc.

		gfx::RenderPass renderPass{device, swapchain, gfx::ClearValues{.color = Color::BLACK}, viewport};

		instances3D.clear();
		// TODO: Add the world's 3D model instances.
		renderer3D.drawUnlitFrame(renderPass, {instances3D}, camera3D);

		instances2D.clear();
		// TODO: Add the user interface's 2D shape/texture/text instances.
		renderer2D.drawFrame(renderPass, {instances2D}, camera2D);

		device.render(renderPass);

		device.present(swapchain);
	}

private:
	void resize(Extent2D newDrawableSize) {
		viewport.region = {.size = newDrawableSize};
		camera2D.setProjection(gfx::OrthographicProjection2D{.size = newDrawableSize});
		camera3D.setProjection(gfx::PerspectiveProjection3D{
			.verticalFieldOfView = verticalFieldOfView,
			.aspectRatio = newDrawableSize.getAspectRatio(),
		});
	}

	void handleEvents() {
		inputManager.update();
		for (const evt::Event& event : eventPump.pollEvents()) {
			GREM_MATCH(event) {
				GREM_CASE(const evt::ApplicationQuitRequestedEvent& quitRequested) {
					quit();
					break;
				}
				GREM_CASE(const evt::WindowDrawableSizeChangedEvent& drawableSizeChanged) {
					if (drawableSizeChanged.windowID == window.getID()) {
						resize(drawableSizeChanged.windowDrawableSize);
					}
					break;
				}
				GREM_CASE_DEFAULT(const auto& other) break;
			}

			inputManager.handleEvent(event);
		}
	}

	evt::EventPump eventPump;
	gfx::Window window;
	gfx::Device device;
	gfx::Swapchain swapchain;
	gfx::Renderer2D renderer2D;
	gfx::Renderer3D renderer3D;
	gfx::Camera2D camera2D{device};
	gfx::Camera3D camera3D{device};
	gfx::Viewport viewport{};
	gfx::Instances2D instances2D{device, renderer2D};
	gfx::Instances3D instances3D{device, renderer3D};
	aud::SoundStage soundStage;
	aud::Listener listener{};
	evt::InputManager inputManager;
	float verticalFieldOfView;
};

} // namespace

int main(int argc, char* argv[]) {
	try {
		app::VirtualFilesystem filesystem{argv[0]};
		// TODO: Set organizationName and applicationName, or remove the following statement to not have an output directory.
		filesystem.setOutputDirectory(filesystem.createStandardOutputDirectory({
			.organizationName = "",
			.applicationName = "",
		}));
		filesystem.mountInputArchive("data");
		filesystem.mountInputArchive(filesystem.getOutputDirectory());

		ApplicationOptions applicationOptions = json::deserializeFromString<ApplicationOptions>(filesystem.readInputFileString(Application::APPLICATION_CONFIGURATION_FILEPATH));
		try {
			cli::parseCommandLineOptions(applicationOptions, argc, argv);
		} catch (const cli::Error& e) {
			eprintln("{}", e.what());
			return app::ExitCode::FAILURE;
		}

		Application application{filesystem, applicationOptions};
		application.run();
	} catch (...) {
		const String message = Error::formatCurrentExceptionMessage();
		eprintln("{}", message);
		evt::SimpleMessageBox::show(evt::MessageType::ERROR_MESSAGE, "Error", message);
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
