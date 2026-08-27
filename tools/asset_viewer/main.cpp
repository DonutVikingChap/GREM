// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/application.hpp>
#include <GREM/audio.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>
#include <GREM/graphics_3d.hpp>
#include <GREM/imgui.hpp>
#include <GREM/resource.hpp>

#include <cinttypes>        // PRIX32
#include <imgui.h>          // Im...
#include <imgui_internal.h> // ImGui::DockBuilder...
#include <type_traits>      // std::in_place_type_t
#include <utility>          // std::move

// Define cross-platform entry point that makes main() more consistent.
#include <GREM/entry_point.hpp>

using namespace grem;

namespace app = grem::application;
namespace aud = grem::audio;
namespace evt = grem::events;
namespace gfx = grem::graphics;
namespace res = grem::resource;

namespace {

struct AssetViewerArguments {
	Optional<CStringView> assetFilepath{};
};

struct AssetViewerOptions {
	gfx::Renderer2DOptions r2D{};
	gfx::Renderer3DOptions r3D{};
	imgui::GraphicalUserInterfaceOptions gui{};
};

struct AssetViewerPreferences {
	enum class BackgroundType : uint8_t {
		NONE,
		SOLID,
		CHECKERBOARD,
	};

	static constexpr uint32_t DEFAULT_MULTISAMPLE_COUNT = 4;

	static constexpr BackgroundType DEFAULT_BACKGROUND_TYPE = BackgroundType::CHECKERBOARD;
	static constexpr vec4 DEFAULT_BACKGROUND_COLOR{0.75f, 0.75f, 0.75f, 1.0f};

	static constexpr vec4 DEFAULT_SKY_COLOR{1.0f, 1.0f, 1.0f, 1.0f};

	static constexpr float MIN_FOV = 1.0f;
	static constexpr float MAX_FOV = 170.0f;
	static constexpr float DEFAULT_FOV = 74.0f;

	static constexpr float MIN_NEAR_Z = 0.001f;
	static constexpr float MAX_NEAR_Z = 1000.0f;
	static constexpr float DEFAULT_NEAR_Z = 0.01f;

	static constexpr float MIN_FAR_Z = 0.001f;
	static constexpr float MAX_FAR_Z = 1000.0f;
	static constexpr float DEFAULT_FAR_Z = 500.0f;

	static constexpr float MIN_EXPOSURE = 0.01f;
	static constexpr float MAX_EXPOSURE = 100.0f;
	static constexpr float DEFAULT_EXPOSURE = 1.0f;

	static constexpr float DEFAULT_PITCH_SENSITIVITY = 0.2f;
	static constexpr float DEFAULT_YAW_SENSITIVITY = 0.2f;

	static constexpr float DEFAULT_SPEED_OF_SOUND = 343.3f;

	[[nodiscard]] static AssetViewerPreferences load(const Filesystem& filesystem, CStringView filepath) {
		AssetViewerPreferences result{};
		if (Optional<String> fileContents = filesystem.tryReadInputFileString(filepath)) {
			json::deserializeFromString(std::move(*fileContents), result);

			result.windowSize.width = max(result.windowSize.width, uint32_t{1});
			result.windowSize.height = max(result.windowSize.height, uint32_t{1});
			result.fov = clamp(result.fov, MIN_FOV, MAX_FOV);
			result.nearZ = clamp(result.nearZ, MIN_NEAR_Z, MAX_NEAR_Z);
			result.farZ = clamp(result.farZ, MIN_FAR_Z, MAX_FAR_Z);
			if (result.farZ <= result.nearZ) {
				result.farZ = result.nearZ + MIN_FAR_Z;
			}
			result.exposure = clamp(result.exposure, MIN_EXPOSURE, MAX_EXPOSURE);

			if (result.outputChannelCount >= 8) {
				result.outputChannelCount = 8;
			} else if (result.outputChannelCount >= 6) {
				result.outputChannelCount = 6;
			} else if (result.outputChannelCount >= 4) {
				result.outputChannelCount = 4;
			} else if (result.outputChannelCount >= 2) {
				result.outputChannelCount = 2;
			} else {
				result.outputChannelCount = 1;
			}
			result.outputVolume = clamp(result.outputVolume, 0.0f, 1.0f);
		}
		return result;
	}

	Optional<Offset2D> windowPosition{};
	Extent2D windowSize{1024, 768};
	bool fullscreen = false;
	bool vSync = true;
	uint32_t maxBufferedFrameCount = 0;
	uint32_t multisampleCount = DEFAULT_MULTISAMPLE_COUNT;
	BackgroundType backgroundType = DEFAULT_BACKGROUND_TYPE;
	vec4 backgroundColor = DEFAULT_BACKGROUND_COLOR;
	Optional<String> skyImageFilepath{};
	vec4 skyColor = DEFAULT_SKY_COLOR;
	float fov = DEFAULT_FOV;
	float nearZ = DEFAULT_NEAR_Z;
	float farZ = DEFAULT_FAR_Z;
	float exposure = DEFAULT_EXPOSURE;
	float pitchSensitivity = DEFAULT_PITCH_SENSITIVITY;
	float yawSensitivity = DEFAULT_YAW_SENSITIVITY;
	size_t outputChannelCount = 2;
	float outputVolume = 1.0f;
	float speedOfSound = DEFAULT_SPEED_OF_SOUND;
	bool useRoundoff = true;
	bool saveRecentlyOpenedFilepaths = true;

	void save(Filesystem& filesystem, CStringView filepath) const {
		const String fileContents = json::serializeToString(*this);
		filesystem.createParentOutputDirectories(filepath);
		filesystem.openEmptyOutputFile(filepath).write(fileContents);
	}
};

class AssetViewer final : public app::Application {
public:
	static constexpr CStringView APPLICATION_TITLE = "GREM Asset Viewer";
	static constexpr CStringView APPLICATION_VERSION = "1.0";
	static constexpr CStringView APPLICATION_ORGANIZATION = "GREM";

	AssetViewer(Filesystem& filesystem, String applicationDirectory, String configurationDirectory, const AssetViewerArguments& arguments, const AssetViewerOptions& options)
		: app::Application(app::ApplicationOptions{.tickInterval{}})
		, filesystem(filesystem)
		, applicationDirectory(std::move(applicationDirectory))
		, configurationDirectory(std::move(configurationDirectory))
		, preferencesConfigurationFilepath(formatString("{}/preferences.json", this->configurationDirectory))
		, guiConfigurationFilepath(formatString("{}/imgui.ini", this->configurationDirectory))
		, recentlyOpenedConfigurationFilepath(formatString("{}/recently_opened_files.txt", this->configurationDirectory))
		, preferences(AssetViewerPreferences::load(filesystem, preferencesConfigurationFilepath))
		, window(gfx::WindowOptions{
			  .title = "Asset Viewer",
			  .positionX = (preferences.windowPosition) ? preferences.windowPosition->x : Optional<int32_t>{},
			  .positionY = (preferences.windowPosition) ? preferences.windowPosition->y : Optional<int32_t>{},
			  .size = preferences.windowSize,
			  .fullscreen = preferences.fullscreen,
		  })
		, device(filesystem, window, gfx::DeviceOptions{.shaderCacheInputFilepath = formatString("{}/shader_cache.dat", this->configurationDirectory)})
		, swapchain(device, window, gfx::SwapchainOptions{.maxBufferedFrameCount = preferences.maxBufferedFrameCount, .useVerticalSynchronization = preferences.vSync})
		, renderer2D(device, options.r2D)
		, renderer3D(device, renderer2D, options.r3D)
		, gui(filesystem, eventPump, window, device, swapchain, renderer2D, options.gui) {
		const uint32_t maxSupportedMultisampleCount = device.getSupportedFeatures().maxSupportedMultisampleCount;
		preferences.multisampleCount = clamp(roundUpToPowerOf2(preferences.multisampleCount), uint32_t{1}, maxSupportedMultisampleCount);

		const float contentScale = window.getDisplay().getContentScale();

		ImGui::SetCurrentContext(gui.getContext());
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = guiConfigurationFilepath.c_str();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(contentScale);
		style.FontScaleDpi = contentScale;
		io.ConfigDpiScaleFonts = true;
		io.ConfigDpiScaleViewports = true;

		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;

		if (!guiConfigurationFilepath.empty()) {
			ImGui::LoadIniSettingsFromDisk(guiConfigurationFilepath.c_str());
		}

		if (!recentlyOpenedConfigurationFilepath.empty()) {
			loadRecentlyOpenedFilepathList();
		}

		if (!loadSky()) {
			preferences.skyImageFilepath.reset();
			preferences.skyColor = AssetViewerPreferences::DEFAULT_SKY_COLOR;
			loadSky();
		}

		if (soundStage) {
			reloadSoundStage();
		}

		try {
			eventPump.enableScreenSaver();
		} catch (...) {
		}

		if (arguments.assetFilepath) {
			openAsset(String{*arguments.assetFilepath});
		}
	}

	~AssetViewer() override {
		if (!preferencesConfigurationFilepath.empty()) {
			try {
				preferences.save(filesystem, preferencesConfigurationFilepath);
			} catch (...) {
			}
		}

		if (!guiConfigurationFilepath.empty()) {
			try {
				filesystem.createParentOutputDirectories(guiConfigurationFilepath);
				ImGui::SaveIniSettingsToDisk(guiConfigurationFilepath.c_str());
			} catch (...) {
			}
		}

		if (!recentlyOpenedConfigurationFilepath.empty() && preferences.saveRecentlyOpenedFilepaths) {
			try {
				saveRecentlyOpenedFilepathList();
			} catch (...) {
			}
		}
	}

protected:
	void update(app::FrameInfo frameInfo) override {
		for (const evt::Event& event : eventPump.pollEvents()) {
			GREM_MATCH(event) {
				GREM_CASE(const evt::ApplicationQuitRequestedEvent& quitRequested) {
					quit();
					break;
				}
				GREM_CASE(const evt::WindowMovedEvent& moved) {
					if (moved.windowID == window.getID()) {
						preferences.windowPosition = moved.windowPosition;
					}
					break;
				}
				GREM_CASE(const evt::WindowResizedEvent& resized) {
					if (resized.windowID == window.getID()) {
						preferences.windowSize = resized.windowSize;
					}
					break;
				}
				GREM_CASE(const evt::DroppedFileEvent& droppedFile) {
					assetFilepathToOpen = droppedFile.droppedFilepath;
					assetFilepathToOpenWasDropped = true;
					break;
				}
				GREM_CASE_DEFAULT(const auto& other) break;
			}

			gui.handleEvent(event);
		}

		gui.update(frameInfo.deltaTime);
		ImGui::NewFrame();

		bool clearRecentlyOpenedFilepaths = false;
		bool deleteConfigurationAndQuit = false;
		bool openAboutWindow = false;
		bool openThirdPartyLegalNoticesWindow = false;

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_W, ImGuiInputFlags_RouteAlways)) {
			closeActiveAsset();
		}
		if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_F4, ImGuiInputFlags_RouteAlways)) {
			quit();
		}
		if (ImGui::Shortcut(ImGuiKey_F11, ImGuiInputFlags_RouteAlways) || ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_Enter, ImGuiInputFlags_RouteAlways)) {
			window.setFullscreen(!window.isFullscreen());
		}

		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::BeginMenu("Open Recent")) {
					for (size_t i = recentlyOpenedFilepaths.size(); i-- > 0;) {
						const String& filepath = recentlyOpenedFilepaths[i];
						const CStringView filename = CStringView{filepath}.substr(filepath.find_last_of("/\\") + 1);
						ImGui::PushID(static_cast<int>(i));
						if (ImGui::MenuItem(filename.c_str())) {
							assetFilepathToOpen = recentlyOpenedFilepaths[i];
							assetFilepathToOpenWasDropped = false;
						}
						ImGui::SetItemTooltip("%s", filepath.c_str());
						ImGui::PopID();
					}
					ImGui::Separator();
					if (ImGui::Button("Clear Recently Opened Filepaths")) {
						clearRecentlyOpenedFilepaths = true;
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Close", "Ctrl+W", false, lastActiveAssetTabIndex.has_value())) {
					closeActiveAsset();
				}
				if (ImGui::MenuItem("Close All", nullptr, false, !assetTabs.empty())) {
					assetTabs.clear();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Quit", "Alt+F4")) {
					quit();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Window")) {
				if (ImGui::MenuItem("Fullscreen", "F11", window.isFullscreen())) {
					window.setFullscreen(!window.isFullscreen());
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Settings")) {
				if (ImGui::CollapsingHeader("Image Background")) {
					ImGui::Spacing();
					int backgroundType = static_cast<int>(preferences.backgroundType);
					if (ImGui::Combo("Background Type", &backgroundType, "None\0Solid\0Checkerboard\0")) {
						preferences.backgroundType = static_cast<AssetViewerPreferences::BackgroundType>(backgroundType);
					}

					if (preferences.backgroundType != AssetViewerPreferences::BackgroundType::NONE) {
						const vec4 srgbBackgroundColor = Color::convertLinearToSRGB(preferences.backgroundColor);
						float srgbBackgroundColorArray[4]{srgbBackgroundColor.x, srgbBackgroundColor.y, srgbBackgroundColor.z, srgbBackgroundColor.w};
						if (ImGui::ColorEdit4("Background Color", srgbBackgroundColorArray)) {
							preferences.backgroundColor = Color::convertSRGBToLinear(
								vec4{srgbBackgroundColorArray[0], srgbBackgroundColorArray[1], srgbBackgroundColorArray[2], srgbBackgroundColorArray[3]});
						}
					}

					ImGui::Spacing();
					if (ImGui::Button("Restore Defaults##Background")) {
						preferences.backgroundType = AssetViewerPreferences::DEFAULT_BACKGROUND_TYPE;
						preferences.backgroundColor = AssetViewerPreferences::DEFAULT_BACKGROUND_COLOR;
					}
					ImGui::Spacing();
				}

				if (ImGui::CollapsingHeader("Model Background")) {
					ImGui::Spacing();
					bool edited = false;

					if (preferences.skyImageFilepath) {
						const size_t lastSlashPosition = preferences.skyImageFilepath->find_last_of("/\\");
						const CStringView filename = CStringView{*preferences.skyImageFilepath}.substr(lastSlashPosition + 1);
						const String directory = preferences.skyImageFilepath->substr(0, lastSlashPosition);
						ImGui::TextLinkOpenURL(filename.c_str(), directory.c_str());
					} else {
						ImGui::Text("Drop Skybox Image Here:");
					}
					const float width = ImGui::CalcItemWidth();
					const float ratio = static_cast<float>(skyPreviewTexture.getHeight()) / static_cast<float>(skyPreviewTexture.getWidth());
					ImGui::Image(gui.getTextureID(skyPreviewTexture), ImVec2{width, width * ratio});
					ImGui::SetItemTooltip("Skybox");
					if (assetFilepathToOpen && assetFilepathToOpenWasDropped && ImGui::IsItemHovered()) {
						preferences.skyImageFilepath = *assetFilepathToOpen;
						preferences.skyColor = {1.0f, 1.0f, 1.0f, 1.0f};
						assetFilepathToOpen.reset();
						assetFilepathToOpenWasDropped = false;
						edited = true;
					}
					ImGui::SameLine();
					if (ImGui::Button("Remove Skybox")) {
						preferences.skyImageFilepath.reset();
						preferences.skyColor = AssetViewerPreferences::DEFAULT_SKY_COLOR;
						edited = true;
					}

					const vec4 srgbSkyColor = Color::convertLinearToSRGB(preferences.skyColor);
					float srgbSkyColorArray[4]{srgbSkyColor.x, srgbSkyColor.y, srgbSkyColor.z, srgbSkyColor.w};
					if (ImGui::ColorEdit4("Sky Color", srgbSkyColorArray)) {
						preferences.skyColor = Color::convertSRGBToLinear(vec4{srgbSkyColorArray[0], srgbSkyColorArray[1], srgbSkyColorArray[2], srgbSkyColorArray[3]});
						edited = true;
					}

					ImGui::Spacing();
					if (ImGui::Button("Restore Defaults##Sky")) {
						if (preferences.skyImageFilepath) {
							preferences.skyColor = {1.0f, 1.0f, 1.0f, 1.0f};
						} else {
							preferences.skyColor = AssetViewerPreferences::DEFAULT_SKY_COLOR;
						}
						edited = true;
					}

					if (edited) {
						if (!loadSky()) {
							preferences.skyImageFilepath.reset();
							preferences.skyColor = AssetViewerPreferences::DEFAULT_SKY_COLOR;
							loadSky();
						}
					}
					ImGui::Spacing();
				}

				if (ImGui::CollapsingHeader("Model Rendering")) {
					ImGui::Spacing();
					int multisampleCount = static_cast<int>(preferences.multisampleCount);
					const uint32_t maxMultisampleCount = device.getSupportedFeatures().maxSupportedMultisampleCount;
					ImGui::SliderInt("Multisample Count", &multisampleCount, 1, static_cast<int>(maxMultisampleCount), "%dx", ImGuiSliderFlags_AlwaysClamp);
					preferences.multisampleCount = clamp(roundUpToPowerOf2(static_cast<uint32_t>(multisampleCount)), uint32_t{1}, maxMultisampleCount);

					ImGui::Spacing();
					if (ImGui::Button("Restore Defaults##Rendering")) {
						preferences.multisampleCount = min(AssetViewerPreferences::DEFAULT_MULTISAMPLE_COUNT, maxMultisampleCount);
					}
					ImGui::Spacing();
				}

				if (ImGui::CollapsingHeader("Model Camera")) {
					ImGui::Spacing();

					ImGui::SliderFloat("Field Of View", &preferences.fov, AssetViewerPreferences::MIN_FOV, AssetViewerPreferences::MAX_FOV, "%.0f° (H @ 4:3)",
						ImGuiSliderFlags_AlwaysClamp);

					ImGui::SliderFloat("Near Z", &preferences.nearZ, AssetViewerPreferences::MIN_NEAR_Z, AssetViewerPreferences::MAX_NEAR_Z, "%.6f",
						ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);

					ImGui::SliderFloat("Far Z", &preferences.farZ, AssetViewerPreferences::MIN_FAR_Z, AssetViewerPreferences::MAX_FAR_Z, "%.6f",
						ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
					if (preferences.farZ <= preferences.nearZ) {
						preferences.farZ = preferences.nearZ + AssetViewerPreferences::MIN_FAR_Z;
					}

					ImGui::SliderFloat("Exposure", &preferences.exposure, static_cast<int>(AssetViewerPreferences::MIN_EXPOSURE),
						static_cast<int>(AssetViewerPreferences::MAX_EXPOSURE), "%.3f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);

					ImGui::SliderFloat("Pitch Sensitivity", &preferences.pitchSensitivity, -2.0f, 2.0f);
					ImGui::SliderFloat("Yaw Sensitivity", &preferences.yawSensitivity, -2.0f, 2.0f);

					ImGui::Spacing();
					if (ImGui::Button("Restore Defaults##Camera")) {
						preferences.fov = AssetViewerPreferences::DEFAULT_FOV;
						preferences.nearZ = AssetViewerPreferences::DEFAULT_NEAR_Z;
						preferences.farZ = AssetViewerPreferences::DEFAULT_FAR_Z;
						preferences.exposure = AssetViewerPreferences::DEFAULT_EXPOSURE;
						preferences.pitchSensitivity = AssetViewerPreferences::DEFAULT_PITCH_SENSITIVITY;
						preferences.yawSensitivity = AssetViewerPreferences::DEFAULT_YAW_SENSITIVITY;
					}
					ImGui::Spacing();
				}

				if (ImGui::CollapsingHeader("Sound Stage")) {
					ImGui::Spacing();
					bool edited = false;

					int channelConfigurationIndex = 0;
					switch (preferences.outputChannelCount) {
						case 2: channelConfigurationIndex = 1; break;
						case 4: channelConfigurationIndex = 2; break;
						case 6: channelConfigurationIndex = 3; break;
						case 8: channelConfigurationIndex = 4; break;
					}
					if (ImGui::Combo("Channels", &channelConfigurationIndex, "Mono\0Stereo\0Quad\u00005.1\u00007.1\0")) {
						switch (channelConfigurationIndex) {
							case 0: preferences.outputChannelCount = 1; break;
							case 1: preferences.outputChannelCount = 2; break;
							case 2: preferences.outputChannelCount = 4; break;
							case 3: preferences.outputChannelCount = 6; break;
							case 4: preferences.outputChannelCount = 8; break;
						}
						edited = true;
					}
					float volume = preferences.outputVolume * 100.0f;
					if (ImGui::SliderFloat("Volume", &volume, 0.0f, 100.0f, "%.0f%%")) {
						preferences.outputVolume = volume / 100.0f;
						if (soundStage) {
							soundStage->setOutputVolume(preferences.outputVolume);
						}
					}
					if (ImGui::DragFloat("Speed Of Sound", &preferences.speedOfSound, 1.0f, 0.0f, 0.0f, "%.3f m/s")) {
						if (soundStage) {
							soundStage->setSpeedOfSound(preferences.speedOfSound);
						}
					}
					edited |= ImGui::Checkbox("Use Roundoff", &preferences.useRoundoff);

					ImGui::Spacing();
					if (ImGui::Button("Restore Defaults##Audio")) {
						preferences.outputChannelCount = 2;
						preferences.outputVolume = 1.0f;
						preferences.speedOfSound = AssetViewerPreferences::DEFAULT_SPEED_OF_SOUND;
						preferences.useRoundoff = true;
						edited = true;
					}

					if (edited && soundStage) {
						reloadSoundStage();
					}
					ImGui::Spacing();
				}

				ImGui::Separator();
				ImGui::TextLinkOpenURL("Open Configuration Directory", configurationDirectory.c_str());
				ImGui::Checkbox("Save Recently Opened Filepaths", &preferences.saveRecentlyOpenedFilepaths);
				if (ImGui::Button("Clear Recently Opened Filepaths")) {
					clearRecentlyOpenedFilepaths = true;
				}
				if (ImGui::Button("Delete Configuration & Quit")) {
					deleteConfigurationAndQuit = true;
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help")) {
				if (ImGui::MenuItem("About##Help")) {
					openAboutWindow = true;
				}
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		if (openAboutWindow) {
			ImGui::OpenPopup("About");
		}
		if (bool remainOpen = true; ImGui::BeginPopupModal("About", &remainOpen)) {
			ImGui::Text("%s", APPLICATION_TITLE.c_str());
			ImGui::Spacing();
			ImGui::Text("Version: %s", APPLICATION_VERSION.c_str());
			ImGui::Spacing();
			ImGui::Text("GREM version: %s", getVersionName());
			ImGui::Text("Dear ImGui version: %s", ImGui::GetVersion());
			ImGui::Spacing();
			const gfx::FeatureSupport supportedFeatures = device.getSupportedFeatures();
			ImGui::Text("Video driver: %s", supportedFeatures.videoDriverName.c_str());
			ImGui::Text("Graphics API: %s %s", supportedFeatures.graphicsBackendAPIName.c_str(), supportedFeatures.graphicsBackendAPIVersionName.c_str());
			ImGui::Spacing();
			if (ImGui::TextLink("Third-party legal notices")) {
				openThirdPartyLegalNoticesWindow = true;
			}
			ImGui::EndPopup();
		}

		if (openThirdPartyLegalNoticesWindow) {
			ImGui::OpenPopup("Third-Party Legal Notices");
		}
		ImGui::SetNextWindowSize(ImVec2{800.0f, 600.0f}, ImGuiCond_Appearing);
		if (bool remainOpen = true; ImGui::BeginPopupModal("Third-Party Legal Notices", &remainOpen, ImGuiWindowFlags_HorizontalScrollbar)) {
			if (thirdPartyLegalNotices.empty()) {
				if (Optional<String> fileContents = filesystem.tryReadInputFileString(formatString("{}/ThirdPartyLegalNotices.md", applicationDirectory))) {
					thirdPartyLegalNotices = std::move(*fileContents);
				}
				if (thirdPartyLegalNotices.empty()) {
					thirdPartyLegalNotices =
						"Failed to load ThirdPartyLegalNotices.md.\n"
						"Please refer to the documentation provided with the application.";
				}
			}
			ImGui::Text("%s", thirdPartyLegalNotices.c_str());
			ImGui::EndPopup();
		}

		if (clearRecentlyOpenedFilepaths) {
			ImGui::OpenPopup("Clear Recently Opened Filepaths?");
		}
		if (ImGui::BeginPopupModal("Clear Recently Opened Filepaths?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Clear the list of recently opened files?");
			ImGui::Text("This operation cannot be undone!");
			ImGui::Separator();
			if (ImGui::Button("Clear Recently Opened Filepaths")) {
				try {
					filesystem.deleteOutputFile(recentlyOpenedConfigurationFilepath);
				} catch (const File::Error&) {
				}
				recentlyOpenedFilepaths.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (deleteConfigurationAndQuit) {
			ImGui::OpenPopup("Delete Configuration?");
		}
		if (ImGui::BeginPopupModal("Delete Configuration?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Delete the configuration and quit?\nThis will reset all user preferences and window layouts.");
			ImGui::Text("This operation cannot be undone!");
			ImGui::Separator();
			if (ImGui::Button("Delete Configuration & Quit")) {
				try {
					filesystem.deleteOutputFile(preferencesConfigurationFilepath);
				} catch (const File::Error&) {
				}
				try {
					filesystem.deleteOutputFile(guiConfigurationFilepath);
				} catch (const File::Error&) {
				}
				preferencesConfigurationFilepath.clear();
				guiConfigurationFilepath.clear();
				ImGui::GetIO().IniFilename = nullptr;
				quit();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		const ImGuiID dockspaceID = ImGui::GetID("Main Dockspace");
		if (!ImGui::DockBuilderGetNode(dockspaceID)) {
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->Size);

			ImGuiID remainingDockspaceID = dockspaceID;
			const ImGuiID propertiesDockID = ImGui::DockBuilderSplitNode(remainingDockspaceID, ImGuiDir_Right, 0.25f, nullptr, &remainingDockspaceID);
			const ImGuiID viewportDockID = remainingDockspaceID;

			ImGui::DockBuilderDockWindow("Properties", propertiesDockID);
			ImGui::DockBuilderDockWindow("Viewport", viewportDockID);
			ImGui::DockBuilderFinish(dockspaceID);
		}
		ImGui::DockSpaceOverViewport(dockspaceID);

		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		if (assetTabs.empty()) {
			const char* text = "Drop Asset Here";
			String loadingString{};
			if (assetFilepathToOpen) {
				const CStringView filename = CStringView{*assetFilepathToOpen}.substr(assetFilepathToOpen->find_last_of("/\\") + 1);
				loadingString = formatString("Loading {}...", filename);
				text = loadingString.c_str();
			}
			const ImVec2 windowSize = ImGui::GetWindowSize();
			const ImVec2 textSize = ImGui::CalcTextSize(text);
			ImGui::SetCursorPos((windowSize - textSize) * 0.5f);
			ImGui::Text("%s", text);
			if (!preferences.skyImageFilepath) {
				const char* const skyboxWarningTextA = "Warning: No skybox loaded.";
				const char* const skyboxWarningTextB = "(Settings -> Model Background)";
				const ImVec2 skyboxWarningTextSizeA = ImGui::CalcTextSize(skyboxWarningTextA);
				const ImVec2 skyboxWarningTextSizeB = ImGui::CalcTextSize(skyboxWarningTextB);
				ImGui::SetCursorPos((windowSize - skyboxWarningTextSizeA) * 0.5f + ImVec2{0.0f, textSize.y * 2.2f});
				ImGui::Text("%s", skyboxWarningTextA);
				ImGui::SetCursorPosX((windowSize.x - skyboxWarningTextSizeB.x) * 0.5f);
				ImGui::Text("%s", skyboxWarningTextB);
			}
		} else {
			if (ImGui::BeginTabBar("Assets", ImGuiTabBarFlags_Reorderable)) {
				for (size_t assetTabIndex = 0; assetTabIndex < assetTabs.size(); ++assetTabIndex) {
					AssetTab& assetTab = assetTabs[assetTabIndex];
					ImGuiTabItemFlags tabItemFlags{};
					if (assetTabIndexToActivate == assetTabIndex) {
						tabItemFlags |= ImGuiTabItemFlags_SetSelected;
						assetTabIndexToActivate.reset();
					}
					bool remainOpen = true;
					if (ImGui::BeginTabItem(CStringView{assetTab.filepath}.substr(assetTab.uniqueFilenameOffset).c_str(), &remainOpen, tabItemFlags)) {
						lastActiveAssetTabIndex = assetTabIndex;
						match(assetTab.assetView)([&](auto& assetView) -> void { //
							showAssetView(assetView);
						});
						ImGui::EndTabItem();
					}
					if (!remainOpen) {
						closeAsset(assetTabIndex);
						--assetTabIndex;
					}
				}

				ImGui::EndTabBar();
			}
		}
		ImGui::End();

		ImGui::Begin("Properties");
		if (lastActiveAssetTabIndex && *lastActiveAssetTabIndex < assetTabs.size()) {
			AssetTab& assetTab = assetTabs[*lastActiveAssetTabIndex];
			const size_t lastSlashPosition = assetTab.filepath.find_last_of("/\\");
			const CStringView filename = CStringView{assetTab.filepath}.substr(lastSlashPosition + 1);
			const String directory = assetTab.filepath.substr(0, lastSlashPosition);
			ImGui::TextLinkOpenURL(filename.c_str(), directory.c_str());
			ImGui::Separator();
			ImGui::LabelText("File Type", "%s", assetTab.fileTypeName.c_str());
			match(assetTab.assetView)([&](auto& assetView) -> void { //
				showAssetProperties(assetTab.filepath, assetView);
			});
		}
		ImGui::End();

		if (soundStage) {
			soundStage->update({});
		}
	}

	void display(app::FrameInfo) override {
		gfx::RenderPass renderPass{device, swapchain, gfx::ClearValues{.color = Color::BLACK}};

		ImGui::Render();
		gui.drawFrame(renderPass, *ImGui::GetDrawData());

		device.render(renderPass);

		device.present(swapchain);

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		if (assetFilepathToOpen) {
			openAsset(std::move(*assetFilepathToOpen));
			assetFilepathToOpen.reset();
			assetFilepathToOpenWasDropped = false;
		}
	}

private:
	static constexpr Array ZOOM_SCALES = [] {
		constexpr Array MAGNIFYING_ZOOM_SCALES{1.125f, 1.25f, 1.375f, 1.5f, 1.75f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f, 12.0f, 16.0f, 24.0f, 32.0f, 48.0f, 64.0f, 128.0f,
			256.0f, 512.0f, 1024.0f};
		constexpr size_t N = MAGNIFYING_ZOOM_SCALES.size();
		Array<float, N + 1 + N> result{};
		for (size_t i = 0; i < result.size(); ++i) {
			if (i < N) {
				result[i] = 1.0f / MAGNIFYING_ZOOM_SCALES[N - 1 - i];
			} else if (i == N) {
				result[i] = 1.0f;
			} else {
				result[i] = MAGNIFYING_ZOOM_SCALES[i - 1 - N];
			}
		}
		return result;
	}();
	static constexpr float MIN_ZOOM_SCALE = ZOOM_SCALES.front();
	static constexpr float MAX_ZOOM_SCALE = ZOOM_SCALES.back();

	static void stepZoomScale(float& zoomScale, float direction) {
		static_assert(isSorted(ZOOM_SCALES));
		if (direction > 0.0f) {
			if (const auto it = upperBound(ZOOM_SCALES, zoomScale); it != ZOOM_SCALES.end()) {
				zoomScale = *it;
			} else {
				zoomScale = MAX_ZOOM_SCALE;
			}
		} else if (direction < 0.0f) {
			if (const auto it = lowerBound(ZOOM_SCALES, zoomScale); it != ZOOM_SCALES.begin()) {
				zoomScale = *(it - 1);
			} else {
				zoomScale = MIN_ZOOM_SCALE;
			}
		}
	}

	[[nodiscard]] static Optional<res::ImageFormat> getTranscodedFormat(res::ImageFormat format) noexcept {
		switch (format) {
			case res::ImageFormat::KTX2_ETC1S_R_UINT_BLOCK: return res::ImageFormat::R8_UINT;
			case res::ImageFormat::KTX2_ETC1S_RG_UINT_BLOCK: return res::ImageFormat::R8G8_UINT;
			case res::ImageFormat::KTX2_ETC1S_RGB_UINT_BLOCK: [[fallthrough]];
			case res::ImageFormat::KTX2_ETC1S_RGBA_UINT_BLOCK: return res::ImageFormat::R8G8B8A8_UINT;
			case res::ImageFormat::KTX2_UASTC_R_UINT_BLOCK: return res::ImageFormat::R8_UINT;
			case res::ImageFormat::KTX2_UASTC_RG_UINT_BLOCK: return res::ImageFormat::R8G8_UINT;
			case res::ImageFormat::KTX2_UASTC_RGB_UINT_BLOCK: [[fallthrough]];
			case res::ImageFormat::KTX2_UASTC_RGBA_UINT_BLOCK: return res::ImageFormat::R8G8B8A8_UINT;
			default: break;
		}
		return format;
	}

	struct ImageAssetView {
		struct Layer {
			gfx::Texture texture;
		};

		struct MipLevel {
			ArrayList<Layer> layers{};
		};

		res::Image image;
		Optional<res::ImageFormat> transcodedFormat = getTranscodedFormat(image.getFormat());
		ArrayList<MipLevel> mipLevels{};
		size_t activeMipLevel = 0;
		size_t activeLayer = 0;
		float zoomScale = 1.0f;
		vec2 uvOffset{0.0f, 0.0f};
		vec2 uvScale{1.0f, 1.0f};
		float uvAngle = 0.0f;
		Optional<ImVec2> pixelToMatchMousePositionTo{};
		bool supportsSRGB = gfx::Texture::hasInternalSRGBFormat(transcodedFormat.value_or(image.getFormat()));
		bool srgb = supportsSRGB;
		bool supportsAlphaBlending =
			res::Image::isRGBAColorFormat(transcodedFormat.value_or(image.getFormat())) && res::Image::isRawFormat(transcodedFormat.value_or(image.getFormat()));
		bool alphaBlending = supportsAlphaBlending;
		bool linearFiltering = false;
		gfx::TextureWrappingMode horizontalWrappingMode = gfx::TextureWrappingMode::REPEAT;
		gfx::TextureWrappingMode verticalWrappingMode = gfx::TextureWrappingMode::REPEAT;

		ImageAssetView(gfx::Device& dev, res::Image&& image)
			: image(std::move(image)) {
			reloadTextures(dev);
		}

		void reloadTextures(gfx::Device& dev) {
			mipLevels.clear();

			res::ImageView sourceImage = image;
			Optional<res::Image> transcodedImage{};
			if (transcodedFormat) {
				transcodedImage.emplace(sourceImage.getType(), *transcodedFormat, sourceImage.getSize3D(), sourceImage.getMipLevelCount());
				sourceImage.transcodeTo(transcodedImage->data(), *transcodedFormat);
				sourceImage = *transcodedImage;
			}
			Optional<res::Image> imageWithoutAlpha{};
			if (supportsAlphaBlending && !alphaBlending) {
				imageWithoutAlpha.emplace(sourceImage.getType(), sourceImage.getFormat(), sourceImage.getSize3D(), sourceImage.getMipLevelCount(), sourceImage.getContents());
				switch (sourceImage.getFormat()) {
					case res::ImageFormat::R8G8B8A8_UINT:
						res::Image::setPixelsComponent<u8norm, 4, 3>(imageWithoutAlpha->getSize3D(), imageWithoutAlpha->getMipLevelCount(), imageWithoutAlpha->data(), 1.0f);
						break;
					case res::ImageFormat::R16G16B16A16_FLOAT:
						res::Image::setPixelsComponent<float16_t, 4, 3>(imageWithoutAlpha->getSize3D(), imageWithoutAlpha->getMipLevelCount(), imageWithoutAlpha->data(), 1.0f);
						break;
					case res::ImageFormat::R32G32B32A32_FLOAT:
						res::Image::setPixelsComponent<float32_t, 4, 3>(imageWithoutAlpha->getSize3D(), imageWithoutAlpha->getMipLevelCount(), imageWithoutAlpha->data(), 1.0f);
						break;
					default: alphaBlending = true; break;
				}
				sourceImage = *imageWithoutAlpha;
			}

			for (uint32_t mipLevel = 0; mipLevel < sourceImage.getMipLevelCount(); ++mipLevel) {
				MipLevel& mip = mipLevels.emplace_back();
				for (uint32_t layer = 0; layer < sourceImage.getDepth(); ++layer) {
					mip.layers.push_back({.texture{dev, sourceImage.getLayer(layer, mipLevel),
						gfx::TextureImageUploadOptions{
							.transferFunction = (srgb) ? Color::TransferFunction::SRGB : Color::TransferFunction::LINEAR,
							.convertToPremultipliedAlpha = false,
							.generateMipmap = false,
						},
						gfx::TextureSamplerOptions{
							.minificationFilter = (linearFiltering) ? gfx::TextureFilter::LINEAR : gfx::TextureFilter::NEAREST,
							.magnificationFilter = (linearFiltering) ? gfx::TextureFilter::LINEAR : gfx::TextureFilter::NEAREST,
							.mipmapMode = gfx::TextureMipmapMode::NONE,
							.horizontalWrappingMode = horizontalWrappingMode,
							.verticalWrappingMode = verticalWrappingMode,
							.maxAnisotropy = 1.0f,
						}}});
				}
			}
		}

		void resetView() {
			zoomScale = 1.0f;
		}
	};

	struct FontAssetView {
		gfx::Font2D font;
		Color color = Color::WHITE;
		Color backgroundColor = Color::INVISIBLE;
		Optional<gfx::OrthographicProjection2D> cameraProjection{};
		gfx::Camera2D camera2D;
		gfx::Texture renderTexture{};
		gfx::Instances2D instances2D;
		uint32_t characterSize = 16;
		vec2 scale{1.0f};
		Array<char, 1024> textBuffer;

		FontAssetView(gfx::Device& dev, gfx::Renderer2D& r2D, gfx::Font2D font)
			: font(std::move(font))
			, camera2D(dev)
			, instances2D(dev, r2D) {
			constexpr StringView INITIAL_STRING =
				"abcdefghijklmnopqrstuvwxyz\n"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
				"0123456789\n"
				"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\n"
				"\n"
				"The quick brown fox\n"
				"jumps over the lazy dog.";
			memcpy(textBuffer.data(), INITIAL_STRING.data(), INITIAL_STRING.size());
			textBuffer[INITIAL_STRING.size()] = '\0';
		}
	};

	struct ModelAssetView {
		static constexpr float DEFAULT_CAMERA_PITCH = convertDegreesToRadians(-30.0f);
		static constexpr float DEFAULT_CAMERA_YAW = convertDegreesToRadians(30.0f);
		static constexpr float DEFAULT_CAMERA_DISTANCE_BOUNDING_RADIUS_COEFFICIENT = 2.1f;

		struct ShaderPipelineSelector {
			gfx::Renderer3D* renderer3D;
			bool unlit = false;

			[[nodiscard]] const gfx::Model3D::ShaderPipeline& operator()(const gfx::Model3D::ShaderConfiguration& shaderConfiguration) const {
				GREM_ASSERT(renderer3D);
				if (unlit) {
					return renderer3D->getUnlitModel3DShaderPipelineSet()(shaderConfiguration);
				}
				return renderer3D->getPBRModel3DShaderPipelineSet()(shaderConfiguration);
			}
		};

		struct AnimationBlendLayer {
			Optional<res::Model::AnimationIndex> animationIndex{};
			float blendWeight = 1.0f;
			float timeScale = 1.0f;
			Duration timeOffset{};
			bool looping = true;
		};

		[[nodiscard]] static gfx::Lights3DOptions getLights3DOptions(float boundingRadius) {
			return {
				.cascadedShadowMapResolution = 1024,
				.pointLightShadowMapResolution = 1024,
				.spotLightShadowMapResolution = 1024,
				.shadowCascadeFrustumFarPlaneDistances{boundingRadius * 4.0f},
			};
		}

		ShaderPipelineSelector shaderPipelineSelector;
		gfx::Model3D model3D;
		res::Model model;
		size_t totalTriangleCount;
		size_t totalIndexCount;
		size_t totalVertexCount;
		gfx::Fog3DOptions fogOptions{};
		gfx::Fog3D fog;
		gfx::Sky3D sky;
		gfx::Decals3D decals;
		gfx::Lights3D lights;
		gfx::LightProbeVolumes3D lightProbeVolumes;
		gfx::ReflectionProbes3D reflectionProbes;
		Optional<gfx::PerspectiveProjection3D> cameraProjection{};
		gfx::Camera3D camera3D;
		gfx::Instances3D instances3D;
		res::Model::Pose pose = model3D.getBindPose();
		res::Model::Transformation transformation{};
		gfx::Texture renderColorTexture{};
		gfx::Texture renderDepthStencilTexture{};
		gfx::Texture resolvedColorTexture{};
		ArrayList<float> morphTargetWeights = pose.localMorphTargetWeights;
		vec3 translation{0.0f, 0.0f, 0.0f};
		vec3 rotation{0.0f, 0.0f, 0.0f};
		vec3 scale{1.0f, 1.0f, 1.0f};
		float cameraPitch = DEFAULT_CAMERA_PITCH;
		float cameraYaw = DEFAULT_CAMERA_YAW;
		float cameraRoll = 0.0f;
		float cameraDistance = model3D.getBindPoseBoundingRadius() * DEFAULT_CAMERA_DISTANCE_BOUNDING_RADIUS_COEFFICIENT;
		vec2 cameraPan{0.0f, 0.0f};
		ArrayList<gfx::Texture> texturePreviews{};
		ArrayList<String> textureNames{};
		ArrayList<String> materialNames{};
		ArrayList<String> jointNames{};
		ArrayList<String> animationNames{};
		ArrayList<String> lightNames{};
		ArrayList<AnimationBlendLayer> animationBlendLayers{};
		Duration animationTime{};
		Duration lastValidAnimationTime{-1};
		bool animationPaused = false;
		bool animationLooping = true;

		ModelAssetView(gfx::Device& device, gfx::Renderer3D& renderer3D, res::ImageView skyImage, Color skyColor, res::Model&& model)
			: shaderPipelineSelector{.renderer3D = &renderer3D}
			, model3D(device, renderer3D, model)
			, model(std::move(model))
			, totalTriangleCount(accumulate(this->model.meshes, size_t{0},
				  [](size_t sum, const res::Model::Mesh& mesh) -> size_t {
					  const size_t indexCount = (mesh.indexCount == 0) ? mesh.vertexCount : mesh.indexCount;
					  if (mesh.primitiveType == res::Model::PrimitiveType::TRIANGLES) {
						  sum += indexCount / 3;
					  } else if (mesh.primitiveType == res::Model::PrimitiveType::TRIANGLE_STRIP && indexCount > 2) {
						  sum += indexCount - 2;
					  }
					  return sum;
				  }))
			, totalIndexCount(accumulate(this->model.meshes, size_t{0},
				  [](size_t sum, const res::Model::Mesh& mesh) -> size_t {
					  const size_t indexCount = (mesh.indexCount == 0) ? mesh.vertexCount : mesh.indexCount;
					  return sum + indexCount;
				  }))
			, totalVertexCount(accumulate(this->model.meshes, size_t{0}, [](size_t sum, const res::Model::Mesh& mesh) -> size_t { return sum + mesh.vertexCount; }))
			, fog(device, fogOptions)
			, sky(device, gfx::Sky3DOptions{.color = skyColor})
			, decals(device)
			, lights(device, getLights3DOptions(model3D.getBindPoseBoundingRadius()))
			, lightProbeVolumes(device)
			, reflectionProbes(device)
			, camera3D(device)
			, instances3D(device, renderer3D) {
			texturePreviews.reserve(this->model.textures.size());
			for (const res::Model::Texture& texture : this->model.textures) {
				const res::ImageView image = match(texture.image)([&](const auto& image) -> res::ImageView { return image; });
				Optional<res::Image> transcodedImage{};
				if (const Optional<res::ImageFormat> transcodedFormat = getTranscodedFormat(image.getFormat())) {
					transcodedImage.emplace(image.getType(), *transcodedFormat, image.getSize3D(), image.getMipLevelCount());
					image.transcodeTo(transcodedImage->data(), *transcodedFormat);
				}
				texturePreviews.emplace_back(device, ((transcodedImage) ? res::ImageView{*transcodedImage} : image).getLayer(0),
					gfx::TextureImageUploadOptions{.convertToPremultipliedAlpha = false, .generateMipmap = false});
			}

			const auto collectNames = [](ArrayList<String>& names, size_t count, const auto& nameMap) -> void {
				names.resize(count);
				for (const auto& [name, index] : nameMap) {
					names[index] = formatString("[{}]: {}", index, name);
				}
				for (size_t i = 0; i < count; ++i) {
					if (names[i].empty()) {
						names[i] = formatString("[{}]", i);
					}
				}
			};

			collectNames(textureNames, this->model.textures.size(), this->model.textureMap);
			collectNames(materialNames, this->model.materials.size(), this->model.materialMap);
			collectNames(jointNames, this->model.bindPose.localJoints.size(), this->model.jointMap);
			collectNames(animationNames, this->model.animations.size(), this->model.animationMap);
			collectNames(lightNames, this->model.lights.size(), this->model.lightMap);

			if (!this->model.animations.empty()) {
				animationBlendLayers.emplace_back();
			}

			bakeSky(device, renderer3D, skyImage, skyColor);

			const float boundingRadius = model3D.getBindPoseBoundingRadius();
			transformation.assign(mat4{1.0f}, pose.localJoints, pose.localMorphTargetWeights, model3D.getJointParentIndices());
			for (const res::Model::Light& light : this->model.lights) {
				if (!transformation.jointsVisible[light.jointIndex]) {
					continue;
				}

				const auto [lightTranslation, lightRotation, lightScale] = decomposeTranslationRotationScale(transformation.jointMatrices[light.jointIndex]);
				GREM_MATCH(light) {
					GREM_CASE(const res::Model::DirectionalLight& directionalLight) {
						lights.createDirectionalLight({
							.direction = lightRotation * vec3{0.0f, 0.0f, -1.0f},
							.color = Color::fromLinear(directionalLight.color, directionalLight.intensity),
							.shadowMapNormalOffsetBiasConstantFactor = gfx::DirectionalLightOptions3D{}.shadowMapNormalOffsetBiasConstantFactor * boundingRadius * 0.5f,
						});
						break;
					}
					GREM_CASE(const res::Model::PointLight& pointLight) {
						lights.createPointLight({
							.position = lightTranslation,
							.range = pointLight.range,
							.color = Color::fromLinear(pointLight.color, pointLight.intensity),
							.shadowNearZ = boundingRadius * 0.001f,
							.shadowFarZ = boundingRadius * 4.0f,
							.shadowMapNormalOffsetBiasConstantFactor = gfx::PointLightOptions3D{}.shadowMapNormalOffsetBiasConstantFactor * boundingRadius * 0.5f,
						});
						break;
					}
					GREM_CASE(const res::Model::SpotLight& spotLight) {
						lights.createSpotLight({
							.position = lightTranslation,
							.direction = lightRotation * vec3{0.0f, 0.0f, -1.0f},
							.range = spotLight.range,
							.innerConeAngle = spotLight.innerConeAngle,
							.outerConeAngle = spotLight.outerConeAngle,
							.color = Color::fromLinear(spotLight.color, spotLight.intensity),
							.shadowNearZ = boundingRadius * 0.001f,
							.shadowFarZ = boundingRadius * 4.0f,
							.shadowMapNormalOffsetBiasConstantFactor = gfx::SpotLightOptions3D{}.shadowMapNormalOffsetBiasConstantFactor * boundingRadius * 0.5f,
						});
						break;
					}
				}
			}
		}

		void bakeSky(gfx::Device& dev, gfx::Renderer3D& r3D, res::ImageView image, Color color) {
			gfx::LightBaker3D{dev, r3D}.bakeSkybox(sky, image, gfx::Sky3DOptions{.color = color});
		}

		void resetTransformation() {
			translation = {0.0f, 0.0f, 0.0f};
			rotation = {0.0f, 0.0f, 0.0f};
			scale = {1.0f, 1.0f, 1.0f};
		}

		void resetCameraView() {
			cameraPitch = DEFAULT_CAMERA_PITCH;
			cameraYaw = DEFAULT_CAMERA_YAW;
			cameraRoll = 0.0f;
			cameraDistance = model3D.getBindPoseBoundingRadius() * DEFAULT_CAMERA_DISTANCE_BOUNDING_RADIUS_COEFFICIENT;
			cameraPan = {0.0f, 0.0f};
		}
	};

	struct SoundAssetView {
		enum class SpatializationMode : uint8_t {
			BACKGROUND,
			PANNING,
			POSITIONAL_3D,
		};

		aud::SoundOptions soundOptions{};
		aud::Sound sound;
		float volume = 1.0f;
		aud::SoundInstanceID soundInstanceID;
		bool looping = false;
		SpatializationMode spatializationMode = SpatializationMode::BACKGROUND;
		bool panAbsolute = false;
		aud::Panning panning{};
		float panFromCenter = 0.0f;
		vec3 position{};
		vec3 velocity{};

		SoundAssetView(aud::SoundStage& soundStage, const Filesystem& filesystem, CStringView filepath)
			: sound(filesystem, filepath, soundOptions)
			, soundInstanceID(soundStage.createPausedSoundInBackground(this->sound, volume)) {}

		void reloadSound(aud::SoundStage& snd, const Filesystem& fs, CStringView filepath) {
			const bool paused = snd.isSoundPaused(soundInstanceID);
			const Optional<Duration> time = snd.getSoundTime(soundInstanceID);
			snd.stopSound(soundInstanceID);
			sound = aud::Sound{fs, filepath, soundOptions};
			switch (spatializationMode) {
				case SpatializationMode::BACKGROUND: soundInstanceID = snd.createPausedSoundInBackground(sound, volume); break;
				case SpatializationMode::PANNING: soundInstanceID = snd.createPausedSound(sound, volume, panning); break;
				case SpatializationMode::POSITIONAL_3D: soundInstanceID = snd.createPaused3DSound(sound, position, velocity, volume); break;
			}
			snd.setSoundLooping(soundInstanceID, looping);
			if (time) {
				snd.seekToSoundTime(soundInstanceID, *time);
				if (!paused) {
					snd.resumeSound(soundInstanceID);
				}
			}
		}

		void reloadSoundInstance(aud::SoundStage& snd, Optional<Duration> timeOverride = {}) {
			const bool paused = snd.isSoundPaused(soundInstanceID);
			const Optional<Duration> time = snd.getSoundTime(soundInstanceID);
			snd.stopSound(soundInstanceID);
			switch (spatializationMode) {
				case SpatializationMode::BACKGROUND: soundInstanceID = snd.createPausedSoundInBackground(sound, volume); break;
				case SpatializationMode::PANNING: soundInstanceID = snd.createPausedSound(sound, volume, panning); break;
				case SpatializationMode::POSITIONAL_3D: soundInstanceID = snd.createPaused3DSound(sound, position, velocity, volume); break;
			}
			snd.setSoundLooping(soundInstanceID, looping);
			if (time) {
				snd.seekToSoundTime(soundInstanceID, timeOverride.value_or(*time));
				if (!paused) {
					snd.resumeSound(soundInstanceID);
				}
			} else if (timeOverride) {
				snd.seekToSoundTime(soundInstanceID, *timeOverride);
			}
		}
	};

	using AssetView = Variant<ImageAssetView, FontAssetView, ModelAssetView, SoundAssetView>;

	struct AssetTab {
		String filepath;
		size_t uniqueFilenameOffset;
		String fileTypeName;
		AssetView assetView;
	};

	bool loadSky() {
		try {
			res::Image newSkyImage = (preferences.skyImageFilepath)
			                             ? res::Image{filesystem, *preferences.skyImageFilepath}
			                             : res::Image{res::ImageType::IMAGE_2D, res::ImageFormat::R32G32B32A32_FLOAT, Extent2D{1}, 1, asBytes(Span{&preferences.skyColor, 1})};
			if (newSkyImage.getType() != res::ImageType::IMAGE_2D && newSkyImage.getType() != res::ImageType::IMAGE_CUBE) {
				throw Error{"Invalid skybox image type (must be 2D or cube)."};
			}
			Optional<res::Image> transcodedImage{};
			if (const Optional<res::ImageFormat> transcodedFormat = getTranscodedFormat(newSkyImage.getFormat())) {
				transcodedImage.emplace(newSkyImage.getType(), *transcodedFormat, newSkyImage.getSize3D(), newSkyImage.getMipLevelCount());
				newSkyImage.transcodeTo(transcodedImage->data(), *transcodedFormat);
			}
			gfx::Texture newSkyPreviewTexture{device, ((transcodedImage) ? *transcodedImage : newSkyImage).getLayer(0),
				{.convertToPremultipliedAlpha = false, .generateMipmap = false}};
			for (AssetTab& assetTab : assetTabs) {
				if (ModelAssetView* const assetView = assetTab.assetView.get_if<ModelAssetView>()) {
					assetView->bakeSky(device, renderer3D, newSkyImage, (preferences.skyImageFilepath) ? Color::fromLinear(preferences.skyColor) : Color::WHITE);
				}
			}
			skyImage = std::move(newSkyImage);
			skyPreviewTexture = std::move(newSkyPreviewTexture);
		} catch (...) {
			const String message =
				formatString("Failed to load {}:\n{}", (preferences.skyImageFilepath) ? *preferences.skyImageFilepath : "skybox", Error::formatCurrentExceptionMessage());
			eprintln("{}", message);
			evt::SimpleMessageBox::show(evt::MessageType::ERROR_MESSAGE, "Error", message);
			return false;
		}
		return true;
	}

	void reloadSoundStage() {
		if (soundStage) {
			for (AssetTab& assetTab : assetTabs) {
				if (SoundAssetView* const assetView = assetTab.assetView.get_if<SoundAssetView>()) {
					soundStage->stopSound(assetView->soundInstanceID);
					assetView->soundInstanceID = {};
				}
			}
		}
		soundStage.emplace(aud::SoundStageOptions{
			.outputChannelCount = preferences.outputChannelCount,
			.outputVolume = preferences.outputVolume,
			.speedOfSound = preferences.speedOfSound,
			.useRoundoff = preferences.useRoundoff,
			.enableStatistics = true,
		});
	}

	void loadRecentlyOpenedFilepathList() {
		if (const Optional<String> fileContents = filesystem.tryReadInputFileString(recentlyOpenedConfigurationFilepath)) {
			recentlyOpenedFilepaths.clear();

			StringView remaining = *fileContents;
			while (!remaining.empty()) {
				const StringView filepath = remaining.substr(0, remaining.find_first_of("\r\n"));
				if (!filepath.empty()) {
					recentlyOpenedFilepaths.emplace_back(filepath);
				}
				remaining.remove_prefix(filepath.size());
				if (remaining.empty()) {
					break;
				}
				const char ch = remaining.front();
				remaining.remove_prefix(1);
				if (ch == '\r') {
					if (!remaining.empty() && remaining.front() == '\n') {
						remaining.remove_prefix(1);
					}
				}
			}
		}
	}

	void saveRecentlyOpenedFilepathList() {
		filesystem.createParentOutputDirectories(recentlyOpenedConfigurationFilepath);
		OutputFileHandle file = filesystem.openEmptyOutputFile(recentlyOpenedConfigurationFilepath);
		for (const String& filepath : recentlyOpenedFilepaths) {
			file.write(filepath);
			file.write("\r\n");
		}
	}

	[[nodiscard]] Optional<AssetTab> createAssetTab(String filepath, size_t uniqueFilenameOffset) {
		const StringView filename = StringView{filepath}.substr(uniqueFilenameOffset);
		try {
			const Allocation<byte> fileContents = filesystem.readInputFile(filepath);
			String fileTypeName{};
			AssetView assetView = [&]() -> AssetView {
				const res::ImageFileType imageFileType = res::Image::determineFileType(fileContents);
				const res::ModelFileType modelFileType = res::Model::determineFileType(fileContents);
				AssetView::index_type assetViewTypeIndex{};
				if ((filepath.ends_with(".jpg") || filepath.ends_with(".jpeg")) && imageFileType == res::ImageFileType::JPEG) {
					fileTypeName = "JPEG";
					assetViewTypeIndex = variant_index_v<ImageAssetView, AssetView>;
				} else if (filepath.ends_with(".png") && imageFileType == res::ImageFileType::PNG) {
					fileTypeName = "PNG";
					assetViewTypeIndex = variant_index_v<ImageAssetView, AssetView>;
				} else if (filepath.ends_with(".hdr") && imageFileType == res::ImageFileType::HDR) {
					fileTypeName = "HDR";
					assetViewTypeIndex = variant_index_v<ImageAssetView, AssetView>;
				} else if (filepath.ends_with(".ktx2") && imageFileType == res::ImageFileType::KTX2) {
					fileTypeName = "KTX2";
					assetViewTypeIndex = variant_index_v<ImageAssetView, AssetView>;
				} else if (filepath.ends_with(".obj") && modelFileType == res::ModelFileType::OBJ) {
					fileTypeName = "OBJ";
					assetViewTypeIndex = variant_index_v<ModelAssetView, AssetView>;
				} else if (filepath.ends_with(".gltf") && modelFileType == res::ModelFileType::GLTF) {
					fileTypeName = "glTF";
					assetViewTypeIndex = variant_index_v<ModelAssetView, AssetView>;
				} else if (filepath.ends_with(".glb") && modelFileType == res::ModelFileType::GLTF_BINARY) {
					fileTypeName = "Binary glTF";
					assetViewTypeIndex = variant_index_v<ModelAssetView, AssetView>;
				} else if (filepath.ends_with(".ttf")) {
					fileTypeName = "TrueType";
					assetViewTypeIndex = variant_index_v<FontAssetView, AssetView>;
				} else if (filepath.ends_with(".otf")) {
					fileTypeName = "OpenType";
					assetViewTypeIndex = variant_index_v<FontAssetView, AssetView>;
				} else if (filepath.ends_with(".ogg")) {
					fileTypeName = "OGG Vorbis";
					assetViewTypeIndex = variant_index_v<SoundAssetView, AssetView>;
				} else if (filepath.ends_with(".wav")) {
					fileTypeName = "RIFF";
					assetViewTypeIndex = variant_index_v<SoundAssetView, AssetView>;
				} else if (filepath.ends_with(".flac")) {
					fileTypeName = "FLAC";
					assetViewTypeIndex = variant_index_v<SoundAssetView, AssetView>;
				} else if (filepath.ends_with(".mp3")) {
					fileTypeName = "MP3";
					assetViewTypeIndex = variant_index_v<SoundAssetView, AssetView>;
				} else if (imageFileType != res::ImageFileType::UNKNOWN) {
					switch (imageFileType) {
						case res::ImageFileType::UNKNOWN: unreachable();
						case res::ImageFileType::JPEG: fileTypeName = "JPEG"; break;
						case res::ImageFileType::PNG: fileTypeName = "PNG"; break;
						case res::ImageFileType::HDR: fileTypeName = "HDR"; break;
						case res::ImageFileType::KTX2: fileTypeName = "KTX2"; break;
					}
					assetViewTypeIndex = variant_index_v<ImageAssetView, AssetView>;
				} else if ((modelFileType == res::ModelFileType::GLTF || modelFileType == res::ModelFileType::GLTF_BINARY) && !filepath.ends_with(".json") &&
						   !filepath.ends_with(".json5")) {
					fileTypeName = (modelFileType == res::ModelFileType::GLTF_BINARY) ? "Binary glTF" : "glTF";
					assetViewTypeIndex = variant_index_v<ModelAssetView, AssetView>;
				} else {
					throw Error{"Unknown asset file type."};
				}
				return AssetView::visitIndex(assetViewTypeIndex,
					Overloaded{
						[&](std::in_place_type_t<ImageAssetView>) -> AssetView { //
							return ImageAssetView{device, res::Image{filesystem, filepath}};
						},
						[&](std::in_place_type_t<FontAssetView>) -> AssetView { //
							return FontAssetView{device, renderer2D, gfx::Font2D{filesystem, filepath}};
						},
						[&](std::in_place_type_t<ModelAssetView>) -> AssetView {
							return ModelAssetView{device, renderer3D, skyImage, (preferences.skyImageFilepath) ? Color::fromLinear(preferences.skyColor) : Color::WHITE,
								res::Model{filesystem, filepath}};
						},
						[&](std::in_place_type_t<SoundAssetView>) -> AssetView {
							if (!soundStage) {
								reloadSoundStage();
							}
							return SoundAssetView{*soundStage, filesystem, filepath};
						},
						[&]() -> AssetView { unreachable(); },
					});
			}();
			return AssetTab{
				.filepath = std::move(filepath),
				.uniqueFilenameOffset = uniqueFilenameOffset,
				.fileTypeName = std::move(fileTypeName),
				.assetView = std::move(assetView),
			};
		} catch (...) {
			const String message = formatString("Failed to load {}:\n{}", filename, Error::formatCurrentExceptionMessage());
			eprintln("{}", message);
			evt::SimpleMessageBox::show(evt::MessageType::ERROR_MESSAGE, "Error", message);
		}
		return {};
	}

	Optional<size_t> openAsset(String filepath) {
		size_t uniqueFilenameOffset = filepath.find_last_of("/\\") + 1;
		const StringView filename = StringView{filepath}.substr(uniqueFilenameOffset);

		const auto it = findBy<&AssetTab::filepath>(assetTabs, filepath);
		if (it == assetTabs.end()) {
			for (AssetTab& assetTab : assetTabs) {
				const StringView assetFilename = StringView{assetTab.filepath}.substr(assetTab.filepath.find_last_of("/\\") + 1);
				if (assetFilename == filename) {
					assetTab.uniqueFilenameOffset = 0;
					uniqueFilenameOffset = 0;
				}
			}
		}

		if (Optional<AssetTab> newAssetTab = createAssetTab(std::move(filepath), uniqueFilenameOffset)) {
			const size_t assetTabIndex = static_cast<size_t>(it - assetTabs.begin());
			if (it == assetTabs.end()) {
				assetTabs.push_back(std::move(*newAssetTab));
			} else {
				*it = std::move(*newAssetTab);
			}
			if (preferences.saveRecentlyOpenedFilepaths) {
				const String& assetTabFilepath = assetTabs[assetTabIndex].filepath;
				erase(recentlyOpenedFilepaths, assetTabFilepath);
				recentlyOpenedFilepaths.push_back(assetTabFilepath);
			}
			assetTabIndexToActivate = assetTabIndex;
			return assetTabIndex;
		}
		return {};
	}

	void closeActiveAsset() {
		if (lastActiveAssetTabIndex && *lastActiveAssetTabIndex < assetTabs.size()) {
			closeAsset(*lastActiveAssetTabIndex);
		}
	}

	void closeAsset(size_t assetTabIndex) {
		if (SoundAssetView* const assetView = assetTabs[assetTabIndex].assetView.get_if<SoundAssetView>()) {
			soundStage->stopSound(assetView->soundInstanceID);
		}
		assetTabs.erase(assetTabs.begin() + static_cast<ptrdiff_t>(assetTabIndex));
		if (assetTabIndex >= assetTabs.size()) {
			if (lastActiveAssetTabIndex == assetTabIndex) {
				lastActiveAssetTabIndex.reset();
				if (!assetTabs.empty()) {
					assetTabIndexToActivate = assetTabs.size() - 1;
				}
			}
			if (assetTabIndexToActivate == assetTabIndex) {
				if (assetTabs.empty()) {
					assetTabIndexToActivate.reset();
				} else {
					assetTabIndexToActivate = assetTabs.size() - 1;
				}
			}
		}
	}

	void showAssetView(ImageAssetView& assetView) {
		ImGui::BeginChild("Image Asset", ImVec2{0.0f, 0.0f}, 0, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		const ImGuiIO& io = ImGui::GetIO();
		if (assetView.activeMipLevel >= assetView.mipLevels.size()) {
			ImGui::EndChild();
			return;
		}
		const ImageAssetView::MipLevel& mip = assetView.mipLevels[assetView.activeMipLevel];
		if (assetView.activeLayer >= mip.layers.size()) {
			ImGui::EndChild();
			return;
		}
		const ImageAssetView::Layer& layer = mip.layers[assetView.activeLayer];
		const ImVec2 position = ImGui::GetCursorScreenPos();
		const ImVec2 size = ImVec2{static_cast<float>(layer.texture.getWidth()), static_cast<float>(layer.texture.getHeight())} * assetView.zoomScale;
		drawBackground(position, size, assetView.zoomScale);

		const ImVec2 pMin = position;
		const ImVec2 pMax = position + size;
		const mat3 uvTransformation = translateScale(vec2{0.0f, 1.0f}, vec2{1.0f, -1.0f}) * translateRotateScale(assetView.uvOffset, assetView.uvAngle, assetView.uvScale);
		const vec2 uv1{uvTransformation * vec3{0.0f, 1.0f, 1.0f}};
		const vec2 uv2{uvTransformation * vec3{1.0f, 1.0f, 1.0f}};
		const vec2 uv3{uvTransformation * vec3{1.0f, 0.0f, 1.0f}};
		const vec2 uv4{uvTransformation * vec3{0.0f, 0.0f, 1.0f}};
		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		drawList.AddImageQuad(gui.getTextureID(layer.texture), //
			ImVec2{pMin.x, pMin.y},                            //
			ImVec2{pMax.x, pMin.y},                            //
			ImVec2{pMax.x, pMax.y},                            //
			ImVec2{pMin.x, pMax.y},                            //
			ImVec2{uv1.x, uv1.y},                              //
			ImVec2{uv2.x, uv2.y},                              //
			ImVec2{uv3.x, uv3.y},                              //
			ImVec2{uv4.x, uv4.y});
		const ImVec2 offset = io.MousePos - position;
		const ImVec2 pixel = offset / assetView.zoomScale;

		ImGui::InvisibleButton("Image", size);
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::BeginItemTooltip()) {
			ImGui::Text("Scroll: Zoom");
			ImGui::Text("Left Click & Drag: Pan");
			ImGui::Separator();
			ImGui::Text("Zoom: %.3fx", assetView.zoomScale);
			ImGui::Text("Pixel Coordinates: (%d, %d)", static_cast<int>(pixel.x), static_cast<int>(pixel.y));
			ImGui::Text("Texel Coordinates: (%d, %d)", static_cast<int>(pixel.x), static_cast<int>(static_cast<float>(layer.texture.getHeight()) - pixel.y));
			ImGui::Text("UV Coordinates: (%.4f, %.4f)", offset.x / size.x, 1.0f - offset.y / size.y);
			ImGui::EndTooltip();
		}
		if (assetView.pixelToMatchMousePositionTo) {
			const ImVec2 newMousePos = position + *assetView.pixelToMatchMousePositionTo * assetView.zoomScale;
			ImGui::SetScrollX(ImGui::GetScrollX() + (newMousePos.x - io.MousePos.x));
			ImGui::SetScrollY(ImGui::GetScrollY() + (newMousePos.y - io.MousePos.y));
			assetView.pixelToMatchMousePositionTo.reset();
		}
		if (hovered) {
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
				ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
				ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
			}
			if (io.MouseWheel != 0.0f) {
				stepZoomScale(assetView.zoomScale, io.MouseWheel);
				assetView.pixelToMatchMousePositionTo = pixel;
				const ImVec2 newMousePos = position + *assetView.pixelToMatchMousePositionTo * assetView.zoomScale;
				ImGui::SetScrollX(ImGui::GetScrollX() + (newMousePos.x - io.MousePos.x));
				ImGui::SetScrollY(ImGui::GetScrollY() + (newMousePos.y - io.MousePos.y));
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
			assetView.resetView();
		}

		ImGui::EndChild();
	}

	void showAssetProperties(CStringView, ImageAssetView& assetView) {
		ImGui::SeparatorText("Image");

		const CStringView typeName = meta::getEnumerandName(assetView.image.getType());
		const CStringView formatName = meta::getEnumerandName(assetView.image.getFormat());

		ImGui::LabelText("Type", "%s", typeName.c_str());
		ImGui::LabelText("Format", "%s", formatName.c_str());
		ImGui::LabelText("Width", "%d px", static_cast<int>(assetView.image.getWidth()));
		ImGui::LabelText("Height", "%d px", static_cast<int>(assetView.image.getHeight()));
		ImGui::LabelText("Depth", "%d layer%s", static_cast<int>(assetView.image.getDepth()), (assetView.image.getDepth() == 1) ? "" : "s");
		ImGui::LabelText("Levels", "%d level%s", static_cast<int>(assetView.image.getMipLevelCount()), (assetView.image.getMipLevelCount() == 1) ? "" : "s");

		ImGui::SeparatorText("View");

		const size_t layerCount = assetView.image.getDepth();
		if (layerCount > 0) {
			if (ImGui::BeginCombo("Layer", formatString("{}/{}", assetView.activeLayer, layerCount - 1).c_str())) {
				for (size_t layer = 0; layer < layerCount; ++layer) {
					if (ImGui::Selectable(formatString("{}", layer).c_str(), layer == assetView.activeLayer)) {
						assetView.activeLayer = layer;
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered()) {
				const ImGuiIO& io = ImGui::GetIO();
				assetView.activeLayer =
					static_cast<size_t>(clamp(static_cast<int>(assetView.activeLayer) + clamp(static_cast<int>(io.MouseWheel), -1, 1), 0, static_cast<int>(layerCount - 1)));
			}
		}

		const size_t mipLevelCount = assetView.image.getMipLevelCount();
		if (mipLevelCount > 0) {
			if (ImGui::BeginCombo("Mip Level", formatString("{}/{}", assetView.activeMipLevel, mipLevelCount - 1).c_str())) {
				for (size_t mipLevel = 0; mipLevel < mipLevelCount; ++mipLevel) {
					if (ImGui::Selectable(formatString("{}", mipLevel).c_str(), mipLevel == assetView.activeMipLevel)) {
						assetView.activeMipLevel = mipLevel;
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered()) {
				const ImGuiIO& io = ImGui::GetIO();
				if (static_cast<int>(io.MouseWheel) < 0) {
					if (assetView.activeMipLevel > 0) {
						--assetView.activeMipLevel;
						assetView.zoomScale /= 2.0f;
					}
				} else if (static_cast<int>(io.MouseWheel) > 0) {
					if (assetView.activeMipLevel < mipLevelCount - 1) {
						++assetView.activeMipLevel;
						assetView.zoomScale *= 2.0f;
					}
				}
			}
		}

		float zoomScale = assetView.zoomScale;
		ImGui::InputFloat("Zoom", &zoomScale, 99999.0f, 99999.0f);
		if (zoomScale <= MIN_ZOOM_SCALE) {
			stepZoomScale(assetView.zoomScale, -1.0f);
		} else if (zoomScale >= MAX_ZOOM_SCALE) {
			stepZoomScale(assetView.zoomScale, 1.0f);
		} else {
			assetView.zoomScale = zoomScale;
		}

		ImGui::Spacing();

		if (ImGui::Button("Reset View (R)")) {
			assetView.resetView();
		}

		ImGui::SeparatorText("Transformation");

		float uvOffset[2]{assetView.uvOffset.x, assetView.uvOffset.y};
		if (ImGui::DragFloat2("UV Offset", uvOffset, 0.1f)) {
			assetView.uvOffset = {uvOffset[0], uvOffset[1]};
		}

		float uvScale[2]{assetView.uvScale.x, assetView.uvScale.y};
		if (ImGui::DragFloat2("UV Scale", uvScale, 0.1f)) {
			assetView.uvScale = {uvScale[0], uvScale[1]};
		}

		float uvAngle = convertRadiansToDegrees(wrap(assetView.uvAngle + numbers::PI, 2.0f * numbers::PI) - numbers::PI);
		if (ImGui::SliderFloat("UV Angle", &uvAngle, -180.0f, 180.0f, "%.0f°")) {
			assetView.uvAngle = convertDegreesToRadians(uvAngle);
		}

		if (ImGui::Button("Flip U")) {
			assetView.uvOffset.x = 1.0f - assetView.uvOffset.x;
			assetView.uvScale.x = -assetView.uvScale.x;
		}
		ImGui::SameLine();
		if (ImGui::Button("Flip V")) {
			assetView.uvOffset.y = 1.0f - assetView.uvOffset.y;
			assetView.uvScale.y = -assetView.uvScale.y;
		}
		ImGui::SameLine();
		if (ImGui::Button("-90°")) {
			assetView.uvAngle -= 0.5f * numbers::PI;
		}
		ImGui::SameLine();
		if (ImGui::Button("+90°")) {
			assetView.uvAngle += 0.5f * numbers::PI;
		}

		ImGui::Spacing();

		if (ImGui::Button("Reset Transformation")) {
			assetView.uvOffset = {0.0f, 0.0f};
			assetView.uvScale = {1.0f, 1.0f};
			assetView.uvAngle = 0.0f;
		}

		ImGui::SeparatorText("Sampling");
		{
			bool edited = false;
			if (assetView.supportsSRGB) {
				edited |= ImGui::Checkbox("Use sRGB Format", &assetView.srgb);
			}
			if (assetView.supportsAlphaBlending) {
				edited |= ImGui::Checkbox("Use Alpha Blending", &assetView.alphaBlending);
			}
			edited |= ImGui::Checkbox("Use Linear Filtering", &assetView.linearFiltering);
			int horizontalWrappingMode = static_cast<int>(assetView.horizontalWrappingMode);
			if (ImGui::Combo("Wrap U", &horizontalWrappingMode, "Repeat\0Clamp To Edge\0Mirrored Repeat\0")) {
				assetView.horizontalWrappingMode = static_cast<gfx::TextureWrappingMode>(horizontalWrappingMode);
				edited = true;
			}
			int verticalWrappingMode = static_cast<int>(assetView.verticalWrappingMode);
			if (ImGui::Combo("Wrap V", &verticalWrappingMode, "Repeat\0Clamp To Edge\0Mirrored Repeat\0")) {
				assetView.verticalWrappingMode = static_cast<gfx::TextureWrappingMode>(verticalWrappingMode);
				edited = true;
			}
			if (edited) {
				assetView.reloadTextures(device);
			}
		}
	}

	void showAssetView(FontAssetView& assetView) {
		const ImVec2 size = ImGui::GetContentRegionAvail();
		const Extent2D renderResolution{
			clamp(static_cast<uint32_t>(size.x), uint32_t{1}, uint32_t{4096}),
			clamp(static_cast<uint32_t>(size.y), uint32_t{1}, uint32_t{4096}),
		};
		if (assetView.renderTexture.getSize2D() != renderResolution) {
			assetView.renderTexture = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R8G8B8A8_SRGB, renderResolution, 1,
				gfx::UndefinedClearValues{}, gfx::TextureSamplerOptions::UNFILTERED);
		}

		const gfx::OrthographicProjection2D projection{
			.offset{0.0f, 0.0f},
			.size{size.x, size.y},
		};
		if (assetView.cameraProjection != projection) {
			assetView.camera2D.setProjectionAndView(projection, gfx::WorldView2D{});
			assetView.cameraProjection = projection;
		}

		assetView.instances2D.clear();
		assetView.instances2D.putTextStringInstance(assetView.font, assetView.textBuffer.data(),
			{
				.characterSize = assetView.characterSize,
				.position = floor(vec2{size.x, size.y} * 0.5f),
				.shapeScale = assetView.scale,
				.alignment = gfx::TextAlign::CENTER,
				.color = assetView.color,
			});

		{
			gfx::RenderPass renderPass{device, {assetView.renderTexture}, gfx::ClearValues{.color = assetView.backgroundColor}};
			renderer2D.drawFrame(renderPass, {assetView.instances2D}, assetView.camera2D);
			device.render(renderPass);
		}

		ImGui::Image(gui.getTextureID(assetView.renderTexture), size);
	}

	void showAssetProperties(CStringView, FontAssetView& assetView) {
		const float availableWidth = ImGui::GetContentRegionAvail().x;

		const gfx::Texture& atlasTexture = assetView.font.getAtlasTexture();
		const uint32_t atlasWidth = atlasTexture.getWidth();
		const ImTextureID atlasTextureID = (atlasTexture) ? gui.getTextureID(atlasTexture) : ImTextureID_Invalid;

		ImGui::SeparatorText("Font");

		const gfx::Font2D::LineMetrics lineMetrics = assetView.font.getLineMetrics(assetView.characterSize);
		ImGui::LabelText("Ascender", "%g", lineMetrics.ascender);
		ImGui::LabelText("Descender", "%g", lineMetrics.descender);
		ImGui::LabelText("Line Height", "%g", lineMetrics.height);

		ImGui::SeparatorText("View");

		bool editedAtlasPackerOptions = false;
		int characterSize = static_cast<int>(assetView.characterSize);
		editedAtlasPackerOptions |= ImGui::SliderInt("Char. Size", &characterSize, 1, 128);
		assetView.characterSize = static_cast<uint32_t>(max(characterSize, 1));

		float scaleX = assetView.scale.x;
		if (ImGui::DragFloat("Scale", &scaleX, 0.01f, 0.0f, 0.0f, (assetView.scale.x == assetView.scale.y) ? "%.3f" : "<non-uniform>")) {
			if (scaleX != 0.0f) {
				assetView.scale.y *= scaleX / assetView.scale.x;
				assetView.scale.x = scaleX;
				if (abs(assetView.scale.x - assetView.scale.y) < 0.001f) {
					assetView.scale.y = assetView.scale.x;
				}
			}
		}

		float scale[]{assetView.scale.x, assetView.scale.y};
		ImGui::DragFloat2("Scale X/Y", scale, 0.01f);
		assetView.scale = {scale[0], scale[1]};

		ImGui::Spacing();

		bool editedFontOptions = false;
		gfx::Font2DOptions fontOptions = assetView.font.getOptions();
		editedFontOptions |= ImGui::Checkbox("Use Linear Filtering", &fontOptions.useLinearFiltering);

		ImGui::Spacing();

		const vec4 srgbColor = assetView.color.toFloatSRGBA();
		float srgbColorArray[4]{srgbColor.x, srgbColor.y, srgbColor.z, srgbColor.w};
		if (ImGui::ColorEdit4("Color", srgbColorArray)) {
			assetView.color = Color::fromLinear(Color::convertSRGBToLinear(vec4{srgbColorArray[0], srgbColorArray[1], srgbColorArray[2], srgbColorArray[3]}));
		}

		const vec4 srgbBackgroundColor = assetView.backgroundColor.toFloatSRGBA();
		float srgbBackgroundColorArray[4]{srgbBackgroundColor.x, srgbBackgroundColor.y, srgbBackgroundColor.z, srgbBackgroundColor.w};
		if (ImGui::ColorEdit4("Bg Color", srgbBackgroundColorArray)) {
			assetView.backgroundColor = Color::fromLinear(
				Color::convertSRGBToLinear(vec4{srgbBackgroundColorArray[0], srgbBackgroundColorArray[1], srgbBackgroundColorArray[2], srgbBackgroundColorArray[3]}));
		}

		ImGui::Spacing();

		if (ImGui::Button("Reset View")) {
			editedAtlasPackerOptions |= assetView.characterSize != 16;
			assetView.characterSize = 16;
			assetView.scale = vec2{1.0f};
			editedFontOptions |= fontOptions.useLinearFiltering;
			fontOptions.useLinearFiltering = false;
			assetView.color = Color::WHITE;
			assetView.backgroundColor = Color::INVISIBLE;
		}

		ImGui::SeparatorText("Text");

		editedAtlasPackerOptions |=
			ImGui::InputTextMultiline("##Text", assetView.textBuffer.data(), assetView.textBuffer.size(), ImVec2{availableWidth, 100.0f}, ImGuiInputTextFlags_AllowTabInput);

		ImGui::SeparatorText("Atlas");

		int initialResolution = static_cast<int>(fontOptions.atlasPackerOptions.initialResolution);
		editedAtlasPackerOptions |= ImGui::SliderInt("Initial Res.", &initialResolution, 1, 4096, "%d px");
		fontOptions.atlasPackerOptions.initialResolution = static_cast<uint32_t>(max(initialResolution, 1));

		int padding = static_cast<int>(fontOptions.atlasPackerOptions.padding);
		editedAtlasPackerOptions |= ImGui::SliderInt("Padding", &padding, 0, 16, "%d px");
		fontOptions.atlasPackerOptions.padding = static_cast<uint32_t>(max(padding, 0));

		int alignment = static_cast<int>(fontOptions.atlasPackerOptions.alignment);
		editedAtlasPackerOptions |= ImGui::SliderInt("Alignment", &alignment, 1, 16, "%d px");
		fontOptions.atlasPackerOptions.alignment = roundUpToPowerOf2(static_cast<uint32_t>(max(alignment, 1)));

		editedFontOptions |= editedAtlasPackerOptions;
		if (editedFontOptions) {
			assetView.font.setOptions(device, fontOptions);
			if (editedAtlasPackerOptions) {
				assetView.font.clearRenderedGlyphs();
			}
		}

		ImGui::Spacing();

		if (atlasTextureID != ImTextureID_Invalid) {
			const float width = min(availableWidth, static_cast<float>(atlasWidth));
			ImGui::Image(atlasTextureID, ImVec2{width, width});
		}

		ImGui::LabelText("Atlas Width", "%u px", static_cast<unsigned>(atlasWidth));
	}

	void showAssetView(ModelAssetView& assetView) {
		if (assetView.animationTime != assetView.lastValidAnimationTime) {
			assetView.pose = assetView.model3D.getBindPose();
			const bool hasAnimation =
				anyOf(assetView.animationBlendLayers, [&](const ModelAssetView::AnimationBlendLayer& layer) -> bool { return layer.animationIndex.has_value(); });
			if (hasAnimation) {
				for (const ModelAssetView::AnimationBlendLayer& layer : assetView.animationBlendLayers) {
					if (layer.animationIndex) {
						Duration animationTime = assetView.animationTime;
						if (layer.timeScale != 1.0f) {
							if (trunc(layer.timeScale) == layer.timeScale) {
								animationTime *= static_cast<Duration::rep>(layer.timeScale);
							} else {
								animationTime = duration_cast<Duration>(duration_cast<FloatSeconds>(animationTime) * layer.timeScale);
							}
						}
						assetView.pose.applyAnimation(res::Model::AnimationState{
							.animation = assetView.model.getAnimationAtIndex(*layer.animationIndex),
							.time = layer.timeOffset + animationTime,
							.blendWeight = layer.blendWeight,
							.looping = layer.looping,
						});
					}
				}
			}
			if (hasAnimation && !assetView.animationPaused) {
				assetView.morphTargetWeights = assetView.pose.localMorphTargetWeights;
			} else {
				assetView.pose.localMorphTargetWeights = assetView.morphTargetWeights;
			}
			assetView.transformation.assign(translateRotateScale(assetView.translation, convertAnglesToQuaternion(assetView.rotation), assetView.scale), assetView.pose.localJoints,
				assetView.pose.localMorphTargetWeights, assetView.model3D.getJointParentIndices());
			assetView.instances3D.clear();
			assetView.instances3D.putShadedModelInstance(assetView.shaderPipelineSelector, assetView.model3D, assetView.transformation);
			assetView.lastValidAnimationTime = assetView.animationTime;
		}

		const ImVec2 size = ImGui::GetContentRegionAvail();
		const Extent2D renderResolution{
			clamp(static_cast<uint32_t>(size.x), uint32_t{1}, uint32_t{4096}),
			clamp(static_cast<uint32_t>(size.y), uint32_t{1}, uint32_t{4096}),
		};
		const bool isMultisampled = preferences.multisampleCount >= 2;
		if (assetView.resolvedColorTexture.getSize2D() != renderResolution || isMultisampled != static_cast<bool>(assetView.renderColorTexture) ||
			(isMultisampled && assetView.renderColorTexture.getMaxMultisampleCount() != preferences.multisampleCount)) {
			if (isMultisampled) {
				assetView.renderColorTexture =
					gfx::Texture::createRenderbuffer(device, gfx::TextureFormat::R8G8B8A8_SRGB, renderResolution, preferences.multisampleCount, gfx::UndefinedClearValues{});
			} else {
				assetView.renderColorTexture = {};
			}
			assetView.renderDepthStencilTexture =
				gfx::Texture::createRenderbuffer(device, gfx::TextureFormat::D32_FLOAT_S8_UINT, renderResolution, preferences.multisampleCount, gfx::UndefinedClearValues{});
			assetView.resolvedColorTexture = gfx::Texture::create(device, gfx::TextureType::TEXTURE_2D, gfx::TextureFormat::R8G8B8A8_SRGB, renderResolution, 1,
				gfx::UndefinedClearValues{}, gfx::TextureSamplerOptions::UNFILTERED);
		}

		const gfx::PerspectiveProjection3D projection{
			.verticalFieldOfView = 2.0f * atan((3.0f / 4.0f) * tan(convertDegreesToRadians(preferences.fov) * 0.5f)),
			.aspectRatio = renderResolution.getAspectRatio(),
			.nearZ = preferences.nearZ,
			.farZ = preferences.farZ,
		};
		if (assetView.cameraProjection != projection) {
			assetView.camera3D.setProjection(projection);
			assetView.cameraProjection = projection;
		}

		const quat cameraOrientation = convertAnglesToQuaternion(assetView.cameraPitch, assetView.cameraYaw, assetView.cameraRoll);
		const vec3 cameraPosition = cameraOrientation * vec3{assetView.cameraPan, assetView.cameraDistance};
		assetView.camera3D.setViewAndOptions(gfx::WorldView3D{.position = cameraPosition, .orientation = cameraOrientation}, {.exposure = preferences.exposure});

		{
			gfx::RenderPass renderPass =
				(isMultisampled) ? gfx::RenderPass{device, assetView.resolvedColorTexture, gfx::DiscardIntermediateValues{},
									   {assetView.renderColorTexture, assetView.renderDepthStencilTexture}, gfx::ClearValues{}}
								 : gfx::RenderPass{device, {assetView.resolvedColorTexture, assetView.renderDepthStencilTexture}, gfx::ClearValues{}};
			renderer3D.drawPBRFrame(renderPass, {assetView.instances3D}, assetView.camera3D, assetView.fog, assetView.sky, assetView.decals, assetView.lights,
				assetView.lightProbeVolumes, assetView.reflectionProbes);
			device.render(renderPass);
		}

		ImGui::Image(gui.getTextureID(assetView.resolvedColorTexture), size);

		if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
			assetView.resetCameraView();
		}

		if (ImGui::IsItemHovered()) {
			const ImGuiIO& io = ImGui::GetIO();

			float stepDistance = maxComponent(assetView.model3D.getBindPoseBoundingBox().max - assetView.model3D.getBindPoseBoundingBox().min) * 0.1f;
			if (io.KeyCtrl) {
				stepDistance *= 10.0f;
			}
			if (io.KeyAlt) {
				stepDistance *= 0.01f;
			}

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
				assetView.cameraPitch -= io.MouseDelta.y * convertDegreesToRadians(preferences.pitchSensitivity);
				assetView.cameraYaw -= io.MouseDelta.x * convertDegreesToRadians(preferences.yawSensitivity);
			}
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
				assetView.cameraRoll -= io.MouseDelta.x * convertDegreesToRadians(preferences.yawSensitivity);
			}
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
				assetView.cameraPan -= vec2{io.MouseDelta.x, -io.MouseDelta.y} * stepDistance * 0.01f;
			}
			assetView.cameraDistance -= io.MouseWheel * stepDistance;
			assetView.cameraDistance = clamp(assetView.cameraDistance, 0.0f, preferences.farZ);
		}
	}

	void showAssetProperties(CStringView, ModelAssetView& assetView) {
		ImGui::SeparatorText("Model");

		const size_t colliderCount = anyOf(assetView.model.jointColliders, [](const Optional<res::Model::Collider>& collider) -> bool { return collider.has_value(); });
		if (ImGui::CollapsingHeader("Statistics")) {
			ImGui::LabelText("Triangles", "%d", static_cast<int>(assetView.totalTriangleCount));
			ImGui::SetItemTooltip("Total number of triangles in the model's meshes");
			ImGui::LabelText("Indices", "%d", static_cast<int>(assetView.totalIndexCount));
			ImGui::SetItemTooltip("Total number of vertex indices in the model's meshes\n(including indices implied by non-indexed meshes)");
			ImGui::LabelText("Vertices", "%d", static_cast<int>(assetView.totalVertexCount));
			ImGui::SetItemTooltip("Total number of vertices in the model's meshes");
			ImGui::Separator();
			ImGui::LabelText("Meshes", "%d", static_cast<int>(assetView.model.meshes.size()));
			ImGui::SetItemTooltip("Number of meshes in the model");
			ImGui::LabelText("Textures", "%d", static_cast<int>(assetView.model.textures.size()));
			ImGui::SetItemTooltip("Number of textures in the model");
			ImGui::LabelText("Materials", "%d", static_cast<int>(assetView.model.materials.size()));
			ImGui::SetItemTooltip("Number of materials in the model");
			ImGui::Separator();
			ImGui::LabelText("Instances", "%d", static_cast<int>(assetView.model.instances.size()));
			ImGui::SetItemTooltip("Number of mesh instances in the model");
			ImGui::LabelText("Joints", "%d", static_cast<int>(assetView.model.bindPose.localJoints.size()));
			ImGui::SetItemTooltip("Number of joints/bones in the model");
			ImGui::Separator();
			ImGui::LabelText("Morph Targets", "%d", static_cast<int>(assetView.model.bindPose.localMorphTargetWeights.size()));
			ImGui::SetItemTooltip("Number of morph targets in the model");
			ImGui::LabelText("Anim. Channels", "%d", static_cast<int>(assetView.model.animationChannels.size()));
			ImGui::SetItemTooltip("Number of animation channels in the model");
			ImGui::LabelText("Animations", "%d", static_cast<int>(assetView.model.animations.size()));
			ImGui::SetItemTooltip("Number of animations in the model");
			ImGui::Separator();
			ImGui::LabelText("Lights", "%d", static_cast<int>(assetView.model.lights.size()));
			ImGui::SetItemTooltip("Number of lights in the model");
			ImGui::Separator();
			ImGui::LabelText("Colliders", "%d", static_cast<int>(colliderCount));
			ImGui::SetItemTooltip("Number of joints/bones with colliders in the model");
			ImGui::LabelText("Phys. Objects", "%d", static_cast<int>(assetView.model.physicsObjects.size()));
			ImGui::SetItemTooltip("Number of physics objects in the model");
			ImGui::LabelText("Phys. Joints", "%d", static_cast<int>(assetView.model.physicsJoints.size()));
			ImGui::SetItemTooltip("Number of physics joints in the model");
		}
		if (ImGui::CollapsingHeader("Bind-Pose Bounds")) {
			const Box<3, float> boundingBox = assetView.model3D.getBindPoseBoundingBox();
			ImGui::LabelText("X", "[% .3f, % .3f] m", boundingBox.min.x, boundingBox.max.x);
			ImGui::SetItemTooltip("Range of the model's vertex positions on the X axis in the bind pose");
			ImGui::LabelText("Y", "[% .3f, % .3f] m", boundingBox.min.y, boundingBox.max.y);
			ImGui::SetItemTooltip("Range of the model's vertex positions on the Y axis in the bind pose");
			ImGui::LabelText("Z", "[% .3f, % .3f] m", boundingBox.min.z, boundingBox.max.z);
			ImGui::SetItemTooltip("Range of the model's vertex positions on the Z axis in the bind pose");
			ImGui::LabelText("Radius", "%.3f m", assetView.model3D.getBindPoseBoundingRadius());
			ImGui::SetItemTooltip("Bounding radius of the model's vertex positions in the bind pose");
		}

		ImGui::SeparatorText("Camera");

		if (ImGui::CollapsingHeader("View")) {
			const float defaultCameraDistance = assetView.model3D.getBindPoseBoundingRadius() * ModelAssetView::DEFAULT_CAMERA_DISTANCE_BOUNDING_RADIUS_COEFFICIENT;
			ImGui::SliderFloat("Distance##Camera", &assetView.cameraDistance, 0.0f, defaultCameraDistance * 4.0f, "%.3f m");
			ImGui::SetItemTooltip("In Viewport: Scroll");
			float pitchAngleInDegrees = convertRadiansToDegrees(wrap(assetView.cameraPitch + numbers::PI, 2.0f * numbers::PI) - numbers::PI);
			if (ImGui::SliderFloat("Pitch##Camera", &pitchAngleInDegrees, -180.0f, 180.0f, "%.0f°")) {
				assetView.cameraPitch = convertDegreesToRadians(pitchAngleInDegrees);
			}
			ImGui::SetItemTooltip("In Viewport: Left Click & Drag");
			float yawAngleInDegrees = convertRadiansToDegrees(wrap(assetView.cameraYaw + numbers::PI, 2.0f * numbers::PI) - numbers::PI);
			if (ImGui::SliderFloat("Yaw##Camera", &yawAngleInDegrees, -180.0f, 180.0f, "%.0f°")) {
				assetView.cameraYaw = convertDegreesToRadians(yawAngleInDegrees);
			}
			ImGui::SetItemTooltip("In Viewport: Left Click & Drag");
			float rollAngleInDegrees = convertRadiansToDegrees(wrap(assetView.cameraRoll + numbers::PI, 2.0f * numbers::PI) - numbers::PI);
			if (ImGui::SliderFloat("Roll##Camera", &rollAngleInDegrees, -180.0f, 180.0f, "%.0f°")) {
				assetView.cameraRoll = convertDegreesToRadians(rollAngleInDegrees);
			}
			ImGui::SetItemTooltip("In Viewport: Middle Click & Drag");
			float pan[2]{assetView.cameraPan.x, assetView.cameraPan.y};
			ImGui::SliderFloat2("Pan##Camera", pan, -defaultCameraDistance, defaultCameraDistance, "%.3f m");
			assetView.cameraPan = {pan[0], pan[1]};
			ImGui::SetItemTooltip("In Viewport: Right Click & Drag");

			ImGui::Spacing();

			if (ImGui::Button("Reset View (R)")) {
				assetView.resetCameraView();
			}
		}

		ImGui::SeparatorText("Shader");

		if (ImGui::BeginCombo("Pipeline", (assetView.shaderPipelineSelector.unlit) ? "Unlit" : "PBR")) {
			if (ImGui::Selectable("PBR", !assetView.shaderPipelineSelector.unlit)) {
				assetView.shaderPipelineSelector.unlit = false;
				assetView.lastValidAnimationTime = Duration{-1};
			}
			if (ImGui::Selectable("Unlit", assetView.shaderPipelineSelector.unlit)) {
				assetView.shaderPipelineSelector.unlit = true;
				assetView.lastValidAnimationTime = Duration{-1};
			}
			ImGui::EndCombo();
		}

		if (!assetView.animationBlendLayers.empty()) {
			ImGui::SeparatorText("Animation");

			const Duration maxTimePoint = accumulate(assetView.animationBlendLayers, Duration{}, [&](Duration end, const ModelAssetView::AnimationBlendLayer& layer) -> Duration {
				if (!layer.animationIndex) {
					return end;
				}
				return max(end, duration_cast<Duration>(FloatSeconds{assetView.model.animations[*layer.animationIndex].maxTimePoint}));
			});
			if (maxTimePoint > Duration{}) {
				if (!assetView.animationPaused) {
					assetView.animationTime += getLatestFrameInfo().deltaTime;
				}
				if (assetView.animationLooping) {
					if (assetView.animationBlendLayers.size() < 2) {
						assetView.animationTime %= maxTimePoint;
					}
				} else if (assetView.animationTime >= maxTimePoint) {
					assetView.animationTime = maxTimePoint - Duration{1};
					assetView.animationPaused = true;
				}
			} else {
				assetView.animationTime = {};
			}
			float animationTime = duration_cast<FloatSeconds>(assetView.animationTime).count();
			const float animationTimeEnd = duration_cast<FloatSeconds>(maxTimePoint).count();
			if (ImGui::SliderFloat("##Animation Time", &animationTime, 0.0f, animationTimeEnd, "%.3f s")) {
				if (maxTimePoint > Duration{}) {
					assetView.animationTime = clamp(duration_cast<Duration>(FloatSeconds{animationTime}), Duration{}, maxTimePoint - Duration{1});
				}
			}
			ImGui::SetItemTooltip("Animation Time");
			ImGui::SameLine();
			if (ImGui::Button((assetView.animationPaused) ? "|>" : "||") || ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
				if (assetView.animationPaused && assetView.animationTime >= maxTimePoint - Duration{1}) {
					assetView.animationTime = {};
				}
				assetView.animationPaused = !assetView.animationPaused;
			}
			ImGui::SetItemTooltip((assetView.animationPaused) ? "Play (Space)" : "Pause (Space)");
			ImGui::SameLine();
			ImGui::Checkbox("##Looping", &assetView.animationLooping);
			ImGui::SetItemTooltip("Looping");

			ImGui::Separator();

			for (size_t animationLayerIndex = 0; animationLayerIndex < assetView.animationBlendLayers.size(); ++animationLayerIndex) {
				ModelAssetView::AnimationBlendLayer& layer = assetView.animationBlendLayers[animationLayerIndex];
				bool edited = false;
				ImGui::PushID(static_cast<int>(animationLayerIndex));
				if (assetView.animationBlendLayers.size() >= 2) {
					ImGui::Text("Layer %d", static_cast<int>(animationLayerIndex));
					ImGui::SameLine();
					edited |= ImGui::Checkbox("Looping", &layer.looping);
				}
				if (ImGui::BeginListBox("Animation")) {
					if (ImGui::Selectable("None", !layer.animationIndex)) {
						if (assetView.animationBlendLayers.size() == 1) {
							assetView.animationTime = {};
						}
						layer.animationIndex.reset();
						layer.blendWeight = (animationLayerIndex == 0) ? 1.0f : 0.5f;
						layer.timeScale = 1.0f;
						layer.timeOffset = {};
						layer.looping = true;
						edited = true;
					}
					for (size_t i = 0; i < assetView.animationNames.size(); ++i) {
						if (ImGui::Selectable(assetView.animationNames[i].c_str(), layer.animationIndex == i)) {
							if (assetView.animationBlendLayers.size() == 1) {
								assetView.animationTime = {};
							}
							layer.animationIndex = static_cast<res::Model::AnimationIndex>(i);
							layer.blendWeight = (animationLayerIndex == 0) ? 1.0f : 0.5f;
							layer.timeScale = 1.0f;
							layer.timeOffset = {};
							layer.looping = true;
							edited = true;
						}
					}
					ImGui::EndListBox();
				}
				if (layer.animationIndex) {
					edited |= ImGui::SliderFloat("Blend Weight", &layer.blendWeight, 0.0f, 1.0f);
					edited |= ImGui::InputFloat("Time Scale", &layer.timeScale, 0.25f, 1.0f, "%.2fx");
					if (assetView.animationBlendLayers.size() >= 2) {
						float timeOffset = duration_cast<FloatSeconds>(layer.timeOffset).count();
						if (ImGui::SliderFloat("Time Offset", &timeOffset, -animationTimeEnd, animationTimeEnd, "%.3f s")) {
							layer.timeOffset = duration_cast<Duration>(FloatSeconds{timeOffset});
							edited = true;
						}
					}
				}
				if (edited) {
					assetView.lastValidAnimationTime = Duration{-1};
				}
				ImGui::Spacing();
				ImGui::PopID();
			}

			if (ImGui::Button("+ Add Layer")) {
				assetView.animationBlendLayers.push_back({.blendWeight = 0.5f});
			}
			if (assetView.animationBlendLayers.size() >= 2) {
				ImGui::SameLine();
				if (ImGui::Button("- Remove Layer")) {
					assetView.animationBlendLayers.pop_back();
					if (assetView.animationBlendLayers.size() == 1) {
						assetView.animationBlendLayers.front().timeOffset = {};
						assetView.animationBlendLayers.front().looping = true;
					}
				}
			}
		}

		if (!assetView.morphTargetWeights.empty()) {
			ImGui::SeparatorText("Morph Targets");

			bool edited = false;

			for (size_t i = 0; i < assetView.morphTargetWeights.size(); ++i) {
				edited |= ImGui::SliderFloat(formatString("Weight [{}]", i).c_str(), &assetView.morphTargetWeights[i], 0.0f, 1.0f);
			}

			if (ImGui::Button("Reset Weights")) {
				assetView.morphTargetWeights = assetView.model.bindPose.localMorphTargetWeights;
				edited = true;
			}

			if (edited) {
				assetView.lastValidAnimationTime = Duration{-1};
			}
		}

		ImGui::SeparatorText("Transformation");

		if (ImGui::CollapsingHeader("Root Joint")) {
			bool edited = false;

			const float defaultCameraDistance = assetView.model3D.getBindPoseBoundingRadius() * ModelAssetView::DEFAULT_CAMERA_DISTANCE_BOUNDING_RADIUS_COEFFICIENT;
			float translation[3]{assetView.translation.x, assetView.translation.y, assetView.translation.z};
			edited |= ImGui::SliderFloat3("Translation##Transformation", translation, -defaultCameraDistance, defaultCameraDistance, "%.3f m");
			assetView.translation = {translation[0], translation[1], translation[2]};
			float pitchAngleInDegrees = convertRadiansToDegrees(wrap(assetView.rotation.x + numbers::PI, 2.0f * numbers::PI) - numbers::PI);
			if (ImGui::SliderFloat("Pitch##Transformation", &pitchAngleInDegrees, -180.0f, 180.0f, "%.0f°")) {
				assetView.rotation.x = convertDegreesToRadians(pitchAngleInDegrees);
				edited = true;
			}
			float yawAngleInDegrees = convertRadiansToDegrees(wrap(assetView.rotation.y + numbers::PI, 2.0f * numbers::PI) - numbers::PI);
			if (ImGui::SliderFloat("Yaw##Transformation", &yawAngleInDegrees, -180.0f, 180.0f, "%.0f°")) {
				assetView.rotation.y = convertDegreesToRadians(yawAngleInDegrees);
				edited = true;
			}
			float rollAngleInDegrees = convertRadiansToDegrees(wrap(assetView.rotation.z + numbers::PI, 2.0f * numbers::PI) - numbers::PI);
			if (ImGui::SliderFloat("Roll##Transformation", &rollAngleInDegrees, -180.0f, 180.0f, "%.0f°")) {
				assetView.rotation.z = convertDegreesToRadians(rollAngleInDegrees);
				edited = true;
			}
			float scale[3]{assetView.scale.x, assetView.scale.y, assetView.scale.z};
			edited |= ImGui::DragFloat3("Scale##Transformation", scale, 0.1f);
			assetView.scale = {scale[0], scale[1], scale[2]};

			ImGui::Spacing();
			if (ImGui::Button("Reset Transformation")) {
				assetView.resetTransformation();
				edited = true;
			}

			if (edited) {
				assetView.lastValidAnimationTime = Duration{-1};
			}
		}

		ImGui::SeparatorText("Data");

		if (!assetView.model.meshes.empty()) {
			if (ImGui::CollapsingHeader("Meshes")) {
				for (size_t i = 0; i < assetView.model.meshes.size(); ++i) {
					if (ImGui::TreeNode(formatString("Mesh [{}]", i).c_str())) {
						const res::Model::Mesh& mesh = assetView.model.meshes[i];

						const char* primitiveTypeName = "Unknown";
						switch (mesh.primitiveType) {
							case res::Model::PrimitiveType::POINTS: primitiveTypeName = "POINTS"; break;
							case res::Model::PrimitiveType::LINES: primitiveTypeName = "LINES"; break;
							case res::Model::PrimitiveType::LINE_STRIP: primitiveTypeName = "LINE_STRIP"; break;
							case res::Model::PrimitiveType::TRIANGLES: primitiveTypeName = "TRIANGLES"; break;
							case res::Model::PrimitiveType::TRIANGLE_STRIP: primitiveTypeName = "TRIANGLE_STRIP"; break;
						}
						ImGui::BulletText("Primitive Type: %s", primitiveTypeName);

						ImGui::BulletText("Indices: %d", static_cast<int>(mesh.indexCount));
						ImGui::BulletText("Vertices: %d", static_cast<int>(mesh.vertexCount));
						ImGui::BulletText("Morph Targets: %d", static_cast<int>(mesh.morphTargetCount));

						ImGui::BulletText("Bounds X: [% .3f, % .3f] m", mesh.boundingBox.min.x, mesh.boundingBox.max.x);
						ImGui::SetItemTooltip("Range of the mesh's local vertex positions on the X axis");
						ImGui::BulletText("Bounds Y: [% .3f, % .3f] m", mesh.boundingBox.min.y, mesh.boundingBox.max.y);
						ImGui::SetItemTooltip("Range of the mesh's local vertex positions on the Y axis");
						ImGui::BulletText("Bounds Z: [% .3f, % .3f] m", mesh.boundingBox.min.z, mesh.boundingBox.max.z);
						ImGui::SetItemTooltip("Range of the mesh's local vertex positions on the Z axis");
						ImGui::BulletText("Bounding Radius: %.3f m", mesh.boundingRadius);
						ImGui::SetItemTooltip("Bounding radius of the mesh's local vertex positions");

						if (ImGui::TreeNode("Vertex Flags")) {
							ImGui::BulletText("Textured On Channel 0: %s", ((mesh.vertexFlags & res::Model::VERTEX_TEXTURED_ON_CHANNEL_0) != 0) ? "Yes" : "No");
							ImGui::BulletText("Textured On Channel 1: %s", ((mesh.vertexFlags & res::Model::VERTEX_TEXTURED_ON_CHANNEL_1) != 0) ? "Yes" : "No");
							ImGui::BulletText("Colored: %s", ((mesh.vertexFlags & res::Model::VERTEX_COLORED) != 0) ? "Yes" : "No");
							ImGui::BulletText("Skinned: %s", ((mesh.vertexFlags & res::Model::VERTEX_SKINNED) != 0) ? "Yes" : "No");
							ImGui::BulletText("Morphed Position: %s", ((mesh.vertexFlags & res::Model::VERTEX_MORPHED_POSITION) != 0) ? "Yes" : "No");
							ImGui::BulletText("Morphed Normal: %s", ((mesh.vertexFlags & res::Model::VERTEX_MORPHED_NORMAL) != 0) ? "Yes" : "No");
							ImGui::BulletText("Morphed Tangent: %s", ((mesh.vertexFlags & res::Model::VERTEX_MORPHED_TANGENT) != 0) ? "Yes" : "No");
							ImGui::BulletText("Morphed Texture Coordinates Channel 0: %s",
								((mesh.vertexFlags & res::Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_0) != 0) ? "Yes" : "No");
							ImGui::BulletText("Morphed Texture Coordinates Channel 1: %s",
								((mesh.vertexFlags & res::Model::VERTEX_MORPHED_TEXTURE_COORDINATES_CHANNEL_1) != 0) ? "Yes" : "No");
							ImGui::BulletText("Morphed Color: %s", ((mesh.vertexFlags & res::Model::VERTEX_MORPHED_COLOR) != 0) ? "Yes" : "No");
							ImGui::TreePop();
						}
						ImGui::TreePop();
					}
				}
			}
		}

		if (!assetView.model.textures.empty()) {
			if (ImGui::CollapsingHeader("Textures")) {
				for (size_t i = 0; i < assetView.model.textures.size(); ++i) {
					if (ImGui::TreeNode(formatString("Texture {}", assetView.textureNames[i]).c_str())) {
						const res::Model::Texture& texture = assetView.model.textures[i];
						const res::ImageView image = match(texture.image)([&](const auto& image) -> res::ImageView { return image; });

						const gfx::Texture& previewTexture = assetView.texturePreviews[i];
						const float width = ImGui::CalcItemWidth();
						const float ratio = static_cast<float>(previewTexture.getHeight()) / static_cast<float>(previewTexture.getWidth());
						ImGui::Image(gui.getTextureID(previewTexture), ImVec2{width, width * ratio});

						const CStringView typeName = meta::getEnumerandName(image.getType());
						const CStringView formatName = meta::getEnumerandName(image.getFormat());

						ImGui::BulletText("Type: %s", typeName.c_str());
						ImGui::BulletText("Format: %s", formatName.c_str());
						ImGui::BulletText("Width: %d px", static_cast<int>(image.getWidth()));
						ImGui::BulletText("Height: %d px", static_cast<int>(image.getHeight()));
						ImGui::BulletText("Depth: %d layer%s", static_cast<int>(image.getDepth()), (image.getDepth() == 1) ? "" : "s");
						ImGui::BulletText("Levels: %d level%s", static_cast<int>(image.getMipLevelCount()), (image.getMipLevelCount() == 1) ? "" : "s");

						const char* minificationFilterName = "Unknown";
						switch (texture.minificationFilter) {
							case res::Model::MinificationFilter::UNSPECIFIED: minificationFilterName = "UNSPECIFIED"; break;
							case res::Model::MinificationFilter::NEAREST: minificationFilterName = "NEAREST"; break;
							case res::Model::MinificationFilter::LINEAR: minificationFilterName = "LINEAR"; break;
							case res::Model::MinificationFilter::NEAREST_MIPMAP_NEAREST: minificationFilterName = "NEAREST_MIPMAP_NEAREST"; break;
							case res::Model::MinificationFilter::LINEAR_MIPMAP_NEAREST: minificationFilterName = "LINEAR_MIPMAP_NEAREST"; break;
							case res::Model::MinificationFilter::NEAREST_MIPMAP_LINEAR: minificationFilterName = "NEAREST_MIPMAP_LINEAR"; break;
							case res::Model::MinificationFilter::LINEAR_MIPMAP_LINEAR: minificationFilterName = "LINEAR_MIPMAP_LINEAR"; break;
						}
						ImGui::BulletText("Min. Filter: %s", minificationFilterName);
						ImGui::SetItemTooltip("Minification Filter");

						const char* magnificationFilterName = "Unknown";
						switch (texture.magnificationFilter) {
							case res::Model::MagnificationFilter::UNSPECIFIED: magnificationFilterName = "UNSPECIFIED"; break;
							case res::Model::MagnificationFilter::NEAREST: magnificationFilterName = "NEAREST"; break;
							case res::Model::MagnificationFilter::LINEAR: magnificationFilterName = "LINEAR"; break;
						}
						ImGui::BulletText("Mag. Filter: %s", magnificationFilterName);
						ImGui::SetItemTooltip("Magnification Filter");

						const char* horizontalWrappingModeName = "Unknown";
						switch (texture.horizontalWrappingMode) {
							case res::Model::WrappingMode::REPEAT: horizontalWrappingModeName = "REPEAT"; break;
							case res::Model::WrappingMode::MIRRORED_REPEAT: horizontalWrappingModeName = "MIRRORED_REPEAT"; break;
							case res::Model::WrappingMode::CLAMP_TO_EDGE: horizontalWrappingModeName = "CLAMP_TO_EDGE"; break;
						}
						ImGui::BulletText("Wrap. Mode (U): %s", horizontalWrappingModeName);
						ImGui::SetItemTooltip("Horizontal Wrapping Mode");

						const char* verticalWrappingModeName = "Unknown";
						switch (texture.verticalWrappingMode) {
							case res::Model::WrappingMode::REPEAT: verticalWrappingModeName = "REPEAT"; break;
							case res::Model::WrappingMode::MIRRORED_REPEAT: verticalWrappingModeName = "MIRRORED_REPEAT"; break;
							case res::Model::WrappingMode::CLAMP_TO_EDGE: verticalWrappingModeName = "CLAMP_TO_EDGE"; break;
						}
						ImGui::BulletText("Wrap. Mode (V): %s", verticalWrappingModeName);
						ImGui::SetItemTooltip("Vertical Wrapping Mode");

						ImGui::TreePop();
					}
				}
			}
		}

		if (!assetView.model.materials.empty()) {
			if (ImGui::CollapsingHeader("Materials")) {
				for (size_t i = 0; i < assetView.model.materials.size(); ++i) {
					if (ImGui::TreeNode(formatString("Material {}", assetView.materialNames[i]).c_str())) {
						const res::Model::Material& material = assetView.model.materials[i];

						const char* materialTypeName = "Unknown";
						switch (material.materialType) {
							case res::Model::MaterialType::METALLIC_ROUGHNESS: materialTypeName = "METALLIC_ROUGHNESS"; break;
							case res::Model::MaterialType::UNLIT: materialTypeName = "UNLIT"; break;
						}
						ImGui::BulletText("Material Type: %s", materialTypeName);

						ImGui::BulletText("Base Color Factor: (%.3f, %.3f, %.3f, %.3f)", material.baseColorFactor.x, material.baseColorFactor.y, material.baseColorFactor.z,
							material.baseColorFactor.w);
						if (ImGui::BeginItemTooltip()) {
							ImGui::ColorButton("Base Color",
								ImColor{material.baseColorFactor.x, material.baseColorFactor.y, material.baseColorFactor.z, material.baseColorFactor.w});
							ImGui::EndTooltip();
						}

						ImGui::BulletText("Occlusion Strength: %.3f", material.occlusionStrength);
						ImGui::BulletText("Roughness Factor: %.3f", material.roughnessFactor);
						ImGui::BulletText("Metallic Factor: %.3f", material.metallicFactor);
						ImGui::BulletText("Normal Scale: %.3f", material.normalScale);
						ImGui::BulletText("Emissive Factor: (%.3f, %.3f, %.3f)", material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z);
						if (ImGui::BeginItemTooltip()) {
							const vec3 srgbColor = Color::convertLinearToSRGB(material.emissiveFactor);
							ImGui::ColorButton("Emissive", ImColor{srgbColor.x, srgbColor.y, srgbColor.z}, ImGuiColorEditFlags_NoAlpha);
							ImGui::EndTooltip();
						}
						ImGui::BulletText("Alpha Cutoff: %.3f", material.alphaCutoff);
						const bool hasBaseColorTexture = material.baseColorMap.textureIndex < assetView.textureNames.size();
						ImGui::BulletText("Base Color Texture: %s", (hasBaseColorTexture) ? assetView.textureNames[material.baseColorMap.textureIndex].c_str() : "None");
						if (hasBaseColorTexture) {
							ImGui::Indent();
							ImGui::Image(gui.getTextureID(assetView.texturePreviews[material.baseColorMap.textureIndex]), ImVec2{64.0f, 64.0f});
							ImGui::BulletText("Offset: (%.3f, %.3f)", material.baseColorMap.textureOffset.x, material.baseColorMap.textureOffset.y);
							ImGui::BulletText("Basis X: (%.3f, %.3f)", material.baseColorMap.textureBasis[0].x, material.baseColorMap.textureBasis[0].y);
							ImGui::BulletText("Basis Y: (%.3f, %.3f)", material.baseColorMap.textureBasis[1].x, material.baseColorMap.textureBasis[1].y);
							ImGui::Unindent();
						}
						const bool hasORMTexture = material.occlusionRoughnessMetallicMap.textureIndex < assetView.textureNames.size();
						ImGui::BulletText("Occlusion-Roughness-Metallic Texture: %s",
							(hasORMTexture) ? assetView.textureNames[material.occlusionRoughnessMetallicMap.textureIndex].c_str() : "None");
						if (hasORMTexture) {
							ImGui::Indent();
							ImGui::Image(gui.getTextureID(assetView.texturePreviews[material.occlusionRoughnessMetallicMap.textureIndex]), ImVec2{64.0f, 64.0f});
							ImGui::BulletText("Offset: (%.3f, %.3f)", material.occlusionRoughnessMetallicMap.textureOffset.x,
								material.occlusionRoughnessMetallicMap.textureOffset.y);
							ImGui::BulletText("Basis X: (%.3f, %.3f)", material.occlusionRoughnessMetallicMap.textureBasis[0].x,
								material.occlusionRoughnessMetallicMap.textureBasis[0].y);
							ImGui::BulletText("Basis Y: (%.3f, %.3f)", material.occlusionRoughnessMetallicMap.textureBasis[1].x,
								material.occlusionRoughnessMetallicMap.textureBasis[1].y);
							ImGui::Unindent();
						}
						const bool hasNormalMapTexture = material.normalMap.textureIndex < assetView.textureNames.size();
						ImGui::BulletText("Normal Map Texture: %s", (hasNormalMapTexture) ? assetView.textureNames[material.normalMap.textureIndex].c_str() : "None");
						if (hasNormalMapTexture) {
							ImGui::Indent();
							ImGui::Image(gui.getTextureID(assetView.texturePreviews[material.normalMap.textureIndex]), ImVec2{64.0f, 64.0f});
							ImGui::BulletText("Offset: (%.3f, %.3f)", material.normalMap.textureOffset.x, material.normalMap.textureOffset.y);
							ImGui::BulletText("Basis X: (%.3f, %.3f)", material.normalMap.textureBasis[0].x, material.normalMap.textureBasis[0].y);
							ImGui::BulletText("Basis Y: (%.3f, %.3f)", material.normalMap.textureBasis[1].x, material.normalMap.textureBasis[1].y);
							ImGui::Unindent();
						}
						const bool hasEmissiveTexture = material.emissiveMap.textureIndex < assetView.textureNames.size();
						ImGui::BulletText("Emissive Texture: %s", (hasEmissiveTexture) ? assetView.textureNames[material.emissiveMap.textureIndex].c_str() : "None");
						if (hasEmissiveTexture) {
							ImGui::Indent();
							ImGui::Image(gui.getTextureID(assetView.texturePreviews[material.emissiveMap.textureIndex]), ImVec2{64.0f, 64.0f});
							ImGui::BulletText("Offset: (%.3f, %.3f)", material.emissiveMap.textureOffset.x, material.emissiveMap.textureOffset.y);
							ImGui::BulletText("Basis X: (%.3f, %.3f)", material.emissiveMap.textureBasis[0].x, material.emissiveMap.textureBasis[0].y);
							ImGui::BulletText("Basis Y: (%.3f, %.3f)", material.emissiveMap.textureBasis[1].x, material.emissiveMap.textureBasis[1].y);
							ImGui::Unindent();
						}

						if (ImGui::TreeNode("Fragment Flags")) {
							ImGui::BulletText("Alpha Masked: %s", ((material.fragmentFlags & res::Model::FRAGMENT_ALPHA_MASKED) != 0) ? "Yes" : "No");
							ImGui::BulletText("Alpha Blended: %s", ((material.fragmentFlags & res::Model::FRAGMENT_ALPHA_BLENDED) != 0) ? "Yes" : "No");
							ImGui::BulletText("Double-Sided: %s", ((material.fragmentFlags & res::Model::FRAGMENT_DOUBLE_SIDED) != 0) ? "Yes" : "No");
							ImGui::BulletText("Base Color Mapped On Channel 0: %s",
								((material.fragmentFlags & res::Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0) != 0) ? "Yes" : "No");
							ImGui::BulletText("Base Color Mapped On Channel 1: %s",
								((material.fragmentFlags & res::Model::FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1) != 0) ? "Yes" : "No");
							ImGui::BulletText("Metallic-Roughness Mapped On Channel 0: %s",
								((material.fragmentFlags & res::Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0) != 0) ? "Yes" : "No");
							ImGui::BulletText("Metallic-Roughness Mapped On Channel 1: %s",
								((material.fragmentFlags & res::Model::FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1) != 0) ? "Yes" : "No");
							ImGui::BulletText("Occlusion Mapped On Channel 0: %s",
								((material.fragmentFlags & res::Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0) != 0) ? "Yes" : "No");
							ImGui::BulletText("Occlusion Mapped On Channel 1: %s",
								((material.fragmentFlags & res::Model::FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1) != 0) ? "Yes" : "No");
							ImGui::BulletText("Normal Mapped On Channel 0: %s", ((material.fragmentFlags & res::Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0) != 0) ? "Yes" : "No");
							ImGui::BulletText("Normal Mapped On Channel 1: %s", ((material.fragmentFlags & res::Model::FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1) != 0) ? "Yes" : "No");
							ImGui::BulletText("Emissive Mapped On Channel 0: %s",
								((material.fragmentFlags & res::Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0) != 0) ? "Yes" : "No");
							ImGui::BulletText("Emissive Mapped On Channel 1: %s",
								((material.fragmentFlags & res::Model::FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1) != 0) ? "Yes" : "No");
							ImGui::TreePop();
						}

						ImGui::TreePop();
					}
				}
			}
		}

		if (!assetView.model.instances.empty()) {
			if (ImGui::CollapsingHeader("Instances")) {
				for (size_t i = 0; i < assetView.model.instances.size(); ++i) {
					if (ImGui::TreeNode(formatString("Instance [{}]", i).c_str())) {
						const res::Model::Instance& instance = assetView.model.instances[i];

						ImGui::BulletText("Material: %s",
							(instance.materialIndex < assetView.materialNames.size()) ? assetView.materialNames[instance.materialIndex].c_str() : "Default");
						ImGui::BulletText("Mesh: [%d]", static_cast<int>(instance.meshIndex));
						ImGui::BulletText("Joint: %s", assetView.jointNames[instance.jointIndex].c_str());

						if (ImGui::TreeNode("Instance Flags")) {
							ImGui::BulletText("Reverse Winding Order: %s", ((instance.instanceFlags & res::Model::INSTANCE_REVERSE_WINDING_ORDER) != 0) ? "Yes" : "No");
							ImGui::TreePop();
						}

						ImGui::TreePop();
					}
				}
			}
		}

		if (ImGui::CollapsingHeader("Joints")) {
			for (size_t i = 0; i < assetView.pose.localJoints.size(); ++i) {
				if (ImGui::TreeNode(formatString("Joint {}", assetView.jointNames[i]).c_str())) {
					const res::Model::Joint& localJoint = assetView.pose.localJoints[i];
					const auto [translation, rotation, scale] = decomposeTranslationRotationScale(assetView.transformation.jointMatrices[i]);
					if (i > 0) {
						ImGui::BulletText("Parent Joint: %s", assetView.jointNames[assetView.model.jointParentIndices[i]].c_str());
					}
					ImGui::BulletText("Translation: (%.3f, %.3f, %.3f) m", translation.x, translation.y, translation.z);
					ImGui::SetItemTooltip("Local Translation: (%.3f, %.3f, %.3f)", localJoint.translation.x, localJoint.translation.y, localJoint.translation.z);
					ImGui::BulletText("Rotation: %.3fi + %.3fj + %.3fk + %.3f", rotation.x, rotation.y, rotation.z, rotation.w);
					ImGui::SetItemTooltip("Local Rotation: %.3fi + %.3fj + %.3fk + %.3f", localJoint.rotation.x, localJoint.rotation.y, localJoint.rotation.z,
						localJoint.rotation.w);
					ImGui::BulletText("Scale: (%.3f, %.3f, %.3f)", scale.x, scale.y, scale.z);
					ImGui::SetItemTooltip("Local Scale: (%.3f, %.3f, %.3f)", localJoint.scale.x, localJoint.scale.y, localJoint.scale.z);
					ImGui::BulletText("Visible: %s", (localJoint.visible) ? "Yes" : "No");
					ImGui::TreePop();
				}
			}
		}

		if (!assetView.model.lights.empty()) {
			if (ImGui::CollapsingHeader("Lights")) {
				for (size_t i = 0; i < assetView.model.lights.size(); ++i) {
					if (ImGui::TreeNode(formatString("Light {}", assetView.lightNames[i]).c_str())) {
						const res::Model::Light& light = assetView.model.lights[i];
						ImGui::BulletText("Joint: %s", assetView.jointNames[light.jointIndex].c_str());
						const auto [translation, rotation, scale] = decomposeTranslationRotationScale(assetView.transformation.jointMatrices[light.jointIndex]);
						GREM_MATCH(light) {
							GREM_CASE(const res::Model::DirectionalLight& directionalLight) {
								const vec3 direction = rotation * vec3{0.0f, 0.0f, -1.0f};
								ImGui::BulletText("Direction: (%.3f, %.3f, %.3f)", direction.x, direction.y, direction.z);
								ImGui::BulletText("Color: (%.3f, %.3f, %.3f)", directionalLight.color.x, directionalLight.color.y, directionalLight.color.z);
								if (ImGui::BeginItemTooltip()) {
									ImGui::ColorButton("Color", ImColor{directionalLight.color.x, directionalLight.color.y, directionalLight.color.z}, ImGuiColorEditFlags_NoAlpha);
									ImGui::EndTooltip();
								}
								ImGui::BulletText("Intensity: %.3f lx", directionalLight.intensity);
								break;
							}
							GREM_CASE(const res::Model::PointLight& pointLight) {
								ImGui::BulletText("Position: (%.3f, %.3f, %.3f) m", translation.x, translation.y, translation.z);
								if (pointLight.range > 0.0f) {
									ImGui::BulletText("Range: %.3f m", pointLight.range);
								}
								ImGui::BulletText("Color: (%.3f, %.3f, %.3f)", pointLight.color.x, pointLight.color.y, pointLight.color.z);
								if (ImGui::BeginItemTooltip()) {
									const vec3 srgbColor = Color::convertLinearToSRGB(pointLight.color);
									ImGui::ColorButton("Color", ImColor{srgbColor.x, srgbColor.y, srgbColor.z}, ImGuiColorEditFlags_NoAlpha);
									ImGui::EndTooltip();
								}
								ImGui::BulletText("Intensity: %.3f cd", pointLight.intensity);
								break;
							}
							GREM_CASE(const res::Model::SpotLight& spotLight) {
								const vec3 direction = rotation * vec3{0.0f, 0.0f, -1.0f};
								ImGui::BulletText("Position: (%.3f, %.3f, %.3f) m", translation.x, translation.y, translation.z);
								ImGui::BulletText("Direction: (%.3f, %.3f, %.3f)", direction.x, direction.y, direction.z);
								if (spotLight.range > 0.0f) {
									ImGui::BulletText("Range: %.3f m", spotLight.range);
								}
								ImGui::BulletText("Inner Cone Angle: %.0f°", convertRadiansToDegrees(spotLight.innerConeAngle));
								ImGui::BulletText("Outer Cone Angle: %.0f°", convertRadiansToDegrees(spotLight.outerConeAngle));
								ImGui::BulletText("Color: (%.3f, %.3f, %.3f)", spotLight.color.x, spotLight.color.y, spotLight.color.z);
								if (ImGui::BeginItemTooltip()) {
									const vec3 srgbColor = Color::convertLinearToSRGB(spotLight.color);
									ImGui::ColorButton("Color", ImColor{srgbColor.x, srgbColor.y, srgbColor.z}, ImGuiColorEditFlags_NoAlpha);
									ImGui::EndTooltip();
								}
								ImGui::BulletText("Intensity: %.3f cd", spotLight.intensity);
								break;
							}
						}
						ImGui::TreePop();
					}
				}
			}
		}

		if (colliderCount > 0) {
			if (ImGui::CollapsingHeader("Colliders")) {
				for (size_t i = 0; i < assetView.model.jointColliders.size(); ++i) {
					if (const Optional<res::Model::Collider>& collider = assetView.model.jointColliders[i]) {
						if (ImGui::TreeNode(formatString("Collider @ Joint {}", assetView.jointNames[i]).c_str())) {
							GREM_MATCH(collider->shape) {
								GREM_CASE(const res::Model::PlaneShape& plane) {
									if (ImGui::TreeNode("Plane Shape")) {
										ImGui::BulletText("Size X: %.3f m", plane.sizeX);
										ImGui::BulletText("Size Z: %.3f m", plane.sizeZ);
										ImGui::BulletText("Double-Sided?: %s", (plane.doubleSided) ? "Yes" : "No");
										ImGui::TreePop();
									}
									break;
								}
								GREM_CASE(const res::Model::SphereShape& sphere) {
									if (ImGui::TreeNode("Sphere Shape")) {
										ImGui::BulletText("Radius: %.3f m", sphere.radius);
										ImGui::TreePop();
									}
									break;
								}
								GREM_CASE(const res::Model::BoxShape& box) {
									if (ImGui::TreeNode("Box Shape")) {
										ImGui::BulletText("Size: (%.3f, %.3f, %.3f) m", box.size.x, box.size.y, box.size.z);
										ImGui::TreePop();
									}
									break;
								}
								GREM_CASE(const res::Model::CylinderShape& cylinder) {
									if (ImGui::TreeNode("Cylinder Shape")) {
										ImGui::BulletText("Half Length: %.3f m", cylinder.halfLength);
										ImGui::BulletText("Bottom Radius: %.3f m", cylinder.bottomRadius);
										ImGui::BulletText("Top Radius: %.3f m", cylinder.topRadius);
										ImGui::TreePop();
									}
									break;
								}
								GREM_CASE(const res::Model::CapsuleShape& capsule) {
									if (ImGui::TreeNode("Capsule Shape")) {
										ImGui::BulletText("Half Length: %.3f m", capsule.halfLength);
										ImGui::BulletText("Bottom Radius: %.3f m", capsule.bottomRadius);
										ImGui::BulletText("Top Radius: %.3f m", capsule.topRadius);
										ImGui::TreePop();
									}
									break;
								}
								GREM_CASE(const res::Model::ConvexPolytopeShape& convexPolytope) {
									if (ImGui::TreeNode("Convex Polytope Shape")) {
										ImGui::BulletText("Vertices: %d", static_cast<int>(convexPolytope->getVertices().size()));
										ImGui::BulletText("Edges: %d", static_cast<int>(convexPolytope->getEdges().size()));
										ImGui::BulletText("Faces: %d", static_cast<int>(convexPolytope->getFaces().size()));
										ImGui::TreePop();
									}
									break;
								}
								GREM_CASE(const res::Model::TriangleMeshShape& triangleMesh) {
									if (ImGui::TreeNode("Triangle Mesh Shape")) {
										ImGui::BulletText("Triangles: %d", static_cast<int>(triangleMesh->getIndices().size() / 3));
										ImGui::BulletText("Vertices: %d", static_cast<int>(triangleMesh->getVertices().size()));
										ImGui::BulletText("Indices: %d", static_cast<int>(triangleMesh->getIndices().size()));
										ImGui::TreePop();
									}
									break;
								}
							}
							ImGui::BulletText("Layers: 0x%08" PRIX32, static_cast<uint32_t>(collider->layers.toInteger()));
							ImGui::BulletText("Detection Layers: 0x%08" PRIX32, static_cast<uint32_t>(collider->detectionLayers.toInteger()));
							ImGui::BulletText("No-Detection Layers: 0x%08" PRIX32, static_cast<uint32_t>(collider->noDetectionLayers.toInteger()));
							ImGui::BulletText("Response Layers: 0x%08" PRIX32, static_cast<uint32_t>(collider->responseLayers.toInteger()));
							ImGui::BulletText("No-Response Layers: 0x%08" PRIX32, static_cast<uint32_t>(collider->noResponseLayers.toInteger()));
							ImGui::TreePop();
						}
					}
				}
			}
		}

		if (!assetView.model.physicsObjects.empty()) {
			if (ImGui::CollapsingHeader("Physics Objects")) {
				for (size_t i = 0; i < assetView.model.physicsObjects.size(); ++i) {
					if (ImGui::TreeNode(formatString("Physics Object [{}]", i).c_str())) {
						const res::Model::PhysicsObject& physicsObject = assetView.model.physicsObjects[i];
						ImGui::BulletText("Joint: %s", assetView.jointNames[physicsObject.jointIndex].c_str());
						ImGui::BulletText("Mass: %.3f kg", physicsObject.mass);
						ImGui::BulletText("Principal Moments Of Inertia: (%.3f, %.3f, %.3f) kg⋅m²", physicsObject.principalMomentsOfInertia.x,
							physicsObject.principalMomentsOfInertia.y, physicsObject.principalMomentsOfInertia.z);
						ImGui::BulletText("Inertia Orientation: %.3fi + %.3fj + %.3fk + %.3f", physicsObject.inertiaOrientation.x, physicsObject.inertiaOrientation.y,
							physicsObject.inertiaOrientation.z, physicsObject.inertiaOrientation.w);
						ImGui::BulletText("Initial Linear Velocity: (%.3f, %.3f, %.3f) m/s", physicsObject.initialLinearVelocity.x, physicsObject.initialLinearVelocity.y,
							physicsObject.initialLinearVelocity.z);
						ImGui::BulletText("Initial Angular Velocity: (%.3f, %.3f, %.3f) rad/s", physicsObject.initialAngularVelocity.x, physicsObject.initialAngularVelocity.y,
							physicsObject.initialAngularVelocity.z);
						ImGui::BulletText("Gravity Factor: %.3fx", physicsObject.gravityFactor);
						ImGui::BulletText("Static Friction: %.3f", physicsObject.staticFriction);
						ImGui::BulletText("Dynamic Friction: %.3f", physicsObject.dynamicFriction);
						ImGui::BulletText("Restitution: %.3f", physicsObject.restitution);
						const char* frictionCombineName = "Unknown";
						switch (physicsObject.frictionCombine) {
							case res::Model::FrictionCombine::AVERAGE: frictionCombineName = "Average"; break;
							case res::Model::FrictionCombine::MINIMUM: frictionCombineName = "Minimum"; break;
							case res::Model::FrictionCombine::MAXIMUM: frictionCombineName = "Maximum"; break;
							case res::Model::FrictionCombine::MULTIPLY: frictionCombineName = "Multiply"; break;
						}
						ImGui::BulletText("Friction Combine: %s", frictionCombineName);
						const char* restitutionCombineName = "Unknown";
						switch (physicsObject.restitutionCombine) {
							case res::Model::RestitutionCombine::AVERAGE: restitutionCombineName = "Average"; break;
							case res::Model::RestitutionCombine::MINIMUM: restitutionCombineName = "Minimum"; break;
							case res::Model::RestitutionCombine::MAXIMUM: restitutionCombineName = "Maximum"; break;
							case res::Model::RestitutionCombine::MULTIPLY: restitutionCombineName = "Multiply"; break;
						}
						ImGui::BulletText("Restitution Combine: %s", restitutionCombineName);
						ImGui::TreePop();
					}
				}
			}
		}

		if (!assetView.model.physicsJoints.empty()) {
			if (ImGui::CollapsingHeader("Physics Joints")) {
				for (size_t i = 0; i < assetView.model.physicsJoints.size(); ++i) {
					if (ImGui::TreeNode(formatString("Physics Joint [{}]", i).c_str())) {
						const res::Model::PhysicsJoint& physicsJoint = assetView.model.physicsJoints[i];
						if (physicsJoint.objectIndices.first == Limits<res::Model::PhysicsObjectIndex>::MAX) {
							ImGui::BulletText("Objects: (null, [%d])", static_cast<int>(physicsJoint.objectIndices.second));
						} else if (physicsJoint.objectIndices.second == Limits<res::Model::PhysicsObjectIndex>::MAX) {
							ImGui::BulletText("Objects: ([%d], null)", static_cast<int>(physicsJoint.objectIndices.first));
						} else {
							ImGui::BulletText("Objects: ([%d], [%d])", static_cast<int>(physicsJoint.objectIndices.first), static_cast<int>(physicsJoint.objectIndices.second));
						}
						ImGui::BulletText("Joints: (%s, %s)", assetView.jointNames[physicsJoint.jointIndices.first].c_str(),
							assetView.jointNames[physicsJoint.jointIndices.second].c_str());
						ImGui::BulletText("Drive Ignores Mass: (%s, %s, %s)", (physicsJoint.driveIgnoresMassX) ? "Yes" : "No", (physicsJoint.driveIgnoresMassY) ? "Yes" : "No",
							(physicsJoint.driveIgnoresMassZ) ? "Yes" : "No");
						ImGui::BulletText("Drive Ignores Moment Of Inertia: (%s, %s, %s)", (physicsJoint.driveIgnoresMomentOfInertiaX) ? "Yes" : "No",
							(physicsJoint.driveIgnoresMomentOfInertiaY) ? "Yes" : "No", (physicsJoint.driveIgnoresMomentOfInertiaZ) ? "Yes" : "No");
						ImGui::BulletText("Enable Collision: %s", (physicsJoint.enableCollision) ? "Yes" : "No");
						ImGui::BulletText("Min. Distances: (%.3f, %.3f, %.3f) m", physicsJoint.minDistances.x, physicsJoint.minDistances.y, physicsJoint.minDistances.z);
						ImGui::BulletText("Max. Distances: (%.3f, %.3f, %.3f) m", physicsJoint.maxDistances.x, physicsJoint.maxDistances.y, physicsJoint.maxDistances.z);
						ImGui::BulletText("Min. Angles: (%.0f, %.0f, %.0f)°", convertRadiansToDegrees(physicsJoint.minAngles.x), convertRadiansToDegrees(physicsJoint.minAngles.y),
							convertRadiansToDegrees(physicsJoint.minAngles.z));
						ImGui::BulletText("Max. Angles: (%.0f, %.0f, %.0f)°", convertRadiansToDegrees(physicsJoint.maxAngles.x), convertRadiansToDegrees(physicsJoint.maxAngles.y),
							convertRadiansToDegrees(physicsJoint.maxAngles.z));
						ImGui::BulletText("Linear Stiffnesses: (%.3f, %.3f, %.3f)", physicsJoint.linearStiffnesses.x, physicsJoint.linearStiffnesses.y,
							physicsJoint.linearStiffnesses.z);
						ImGui::BulletText("Angular Stiffnesses: (%.3f, %.3f, %.3f)", physicsJoint.angularStiffnesses.x, physicsJoint.angularStiffnesses.y,
							physicsJoint.angularStiffnesses.z);
						ImGui::BulletText("Linear Damping: (%.3f, %.3f, %.3f)", physicsJoint.linearDamping.x, physicsJoint.linearDamping.y, physicsJoint.linearDamping.z);
						ImGui::BulletText("Angular Damping: (%.3f, %.3f, %.3f)", physicsJoint.angularDamping.x, physicsJoint.angularDamping.y, physicsJoint.angularDamping.z);
						ImGui::BulletText("Max. Force: (%.3f, %.3f, %.3f) N", physicsJoint.maxForce.x, physicsJoint.maxForce.y, physicsJoint.maxForce.z);
						ImGui::BulletText("Max. Torque: (%.3f, %.3f, %.3f) N⋅m", physicsJoint.maxTorque.x, physicsJoint.maxTorque.y, physicsJoint.maxTorque.z);
						ImGui::BulletText("Target Position: (%.3f, %.3f, %.3f) m", physicsJoint.targetPosition.x, physicsJoint.targetPosition.y, physicsJoint.targetPosition.z);
						ImGui::BulletText("Target Angles: (%.3f, %.3f, %.3f)°", convertRadiansToDegrees(physicsJoint.targetAngles.x),
							convertRadiansToDegrees(physicsJoint.targetAngles.y), convertRadiansToDegrees(physicsJoint.targetAngles.z));
						ImGui::BulletText("Target Linear Velocity: (%.3f, %.3f, %.3f) m/s", physicsJoint.targetLinearVelocity.x, physicsJoint.targetLinearVelocity.y,
							physicsJoint.targetLinearVelocity.z);
						ImGui::BulletText("Target Angular Velocity: (%.3f, %.3f, %.3f) rad/s", physicsJoint.targetAngularVelocity.x, physicsJoint.targetAngularVelocity.y,
							physicsJoint.targetAngularVelocity.z);
						ImGui::BulletText("Linear Drive Damping: (%.3f, %.3f, %.3f)", physicsJoint.linearDriveDamping.x, physicsJoint.linearDriveDamping.y,
							physicsJoint.linearDriveDamping.z);
						ImGui::BulletText("Angular Drive Damping: (%.3f, %.3f, %.3f)", physicsJoint.angularDriveDamping.x, physicsJoint.angularDriveDamping.y,
							physicsJoint.angularDriveDamping.z);
						ImGui::TreePop();
					}
				}
			}
		}
	}

	void showAssetView(SoundAssetView& assetView) {
		audioOutputChannelVolumeStatistics.resize(soundStage->getOutputChannelCount());
		for (size_t i = 0; i < audioOutputChannelVolumeStatistics.size(); ++i) {
			audioOutputChannelVolumeStatistics[i] = soundStage->getOutputChannelVolumeStatistics(static_cast<uint32_t>(i));
		}
		const auto& volume = audioOutputChannelVolumeStatistics;
		const auto wave = soundStage->getOutputWaveStatistics();
		const auto fft = soundStage->calculateOutputFastFourierTransformStatistics();
		const float width = ImGui::CalcItemWidth();
		const float height = 64.0f;
		const float volumeWidth = 16.0f * static_cast<float>(volume.size());
		const float cursorX = (ImGui::GetContentRegionAvail().x - width) * 0.5f;

		ImGui::Spacing();
		ImGui::SetCursorPosX(cursorX);
		ImGui::PlotHistogram("##Volume", volume.data(), static_cast<int>(volume.size()), 0, nullptr, 0.0f, 1.0f, ImVec2{volumeWidth, height});
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Volume");
		}
		ImGui::SameLine();
		ImGui::PlotLines("##Wave", wave.data(), static_cast<int>(wave.size()), 0, nullptr, -1.0f, 1.0f, ImVec2{width - volumeWidth - ImGui::GetStyle().ItemSpacing.x, height});
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Wave");
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(cursorX);
		ImGui::PlotHistogram("##FFT", fft.data(), static_cast<int>(fft.size()), 0, nullptr, 0.0f, 128.0f, ImVec2{width, 128});
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("FFT");
		}

		ImGui::Spacing();
		ImGui::Separator();

		const bool paused = soundStage->isSoundStopped(assetView.soundInstanceID) || soundStage->isSoundPaused(assetView.soundInstanceID);
		const char* const pauseButtonText = (paused) ? "|>" : "||";
		ImGui::SetCursorPosX(cursorX - ImGui::CalcTextSize(pauseButtonText).x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
		if (ImGui::Button(pauseButtonText) || ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
			if (paused) {
				if (soundStage->isSoundStopped(assetView.soundInstanceID)) {
					assetView.reloadSoundInstance(*soundStage);
				}
				soundStage->resumeSound(assetView.soundInstanceID);
			} else {
				soundStage->pauseSound(assetView.soundInstanceID);
			}
		}
		ImGui::SetItemTooltip((paused) ? "Play (Space)" : "Pause (Space)");
		ImGui::SameLine();
		const Optional<Duration> timePoint = soundStage->getSoundTime(assetView.soundInstanceID);
		const Duration maxTimePoint = assetView.sound.getDuration();
		float soundTime = duration_cast<FloatSeconds>(timePoint.value_or(maxTimePoint)).count();
		const float soundTimeEnd = duration_cast<FloatSeconds>(maxTimePoint).count();
		if (ImGui::SliderFloat("##Sound Time", &soundTime, 0.0f, soundTimeEnd, "%.3f s", ImGuiSliderFlags_AlwaysClamp)) {
			soundStage->pauseSound(assetView.soundInstanceID);
			soundStage->seekToSoundTime(assetView.soundInstanceID, duration_cast<Duration>(FloatSeconds{soundTime}));
			if (soundStage->isSoundStopped(assetView.soundInstanceID)) {
				assetView.reloadSoundInstance(*soundStage, duration_cast<Duration>(FloatSeconds{soundTime}));
			}
		}
		ImGui::SetItemTooltip("Sound Time");
		ImGui::SameLine();
		if (ImGui::Checkbox("##Looping", &assetView.looping)) {
			soundStage->setSoundLooping(assetView.soundInstanceID, assetView.looping);
		}
		ImGui::SetItemTooltip("Looping");
	}

	void showAssetProperties(CStringView filepath, SoundAssetView& assetView) {
		ImGui::SeparatorText("Sound");

		ImGui::LabelText("Duration", "%.3f s", duration_cast<FloatSeconds>(assetView.sound.getDuration()).count());

		float volume = assetView.volume * 100.0f;
		if (ImGui::SliderFloat("Volume", &volume, 0, 100, "%.0f%%")) {
			assetView.volume = volume / 100.0f;
			soundStage->setSoundVolume(assetView.soundInstanceID, assetView.volume);
		}

		ImGui::SeparatorText("Spatialization");
		int spatializationModeIndex = static_cast<int>(assetView.spatializationMode);
		if (ImGui::Combo("Mode", &spatializationModeIndex, "Background\0Panning\u00003D\0")) {
			assetView.spatializationMode = static_cast<SoundAssetView::SpatializationMode>(spatializationModeIndex);
			assetView.reloadSoundInstance(*soundStage);
		}
		switch (assetView.spatializationMode) {
			case SoundAssetView::SpatializationMode::BACKGROUND: break;
			case SoundAssetView::SpatializationMode::PANNING: {
				bool edited = false;

				edited |= ImGui::Checkbox("Absolute", &assetView.panAbsolute);
				if (assetView.panAbsolute) {
					float panning[2]{assetView.panning.leftVolume * 100.0f, assetView.panning.rightVolume * 100.0f};
					if (ImGui::SliderFloat2("L/R Volume", panning, 0.0f, 100.0f, "%.0f%%")) {
						assetView.panning.leftVolume = panning[0] / 100.0f;
						assetView.panning.rightVolume = panning[1] / 100.0f;
						edited = true;
					}
				} else {
					edited |= ImGui::DragFloat("Offset", &assetView.panFromCenter, 0.001f, 0.0f, 0.0f, "%.3f m");
					assetView.panning = aud::Panning::fromNormalizedCenterOffset(assetView.panFromCenter);
				}

				if (edited) {
					soundStage->setSoundPanning(assetView.soundInstanceID, assetView.panning);
				}
				break;
			}
			case SoundAssetView::SpatializationMode::POSITIONAL_3D: {
				float position[3]{assetView.position.x, assetView.position.y, assetView.position.z};
				if (ImGui::DragFloat3("Position [m]", position, 0.001f)) {
					assetView.position = {position[0], position[1], position[2]};
					soundStage->setSoundPosition(assetView.soundInstanceID, assetView.position);
				}

				float velocity[3]{assetView.velocity.x, assetView.velocity.y, assetView.velocity.z};
				if (ImGui::DragFloat3("Velocity [m/s]", velocity, 0.1f)) {
					assetView.velocity = {velocity[0], velocity[1], velocity[2]};
					soundStage->setSoundVelocity(assetView.soundInstanceID, assetView.velocity);
				}

				bool edited = false;

				ImGui::SeparatorText("Attenuation");
				int attenuationModelIndex = static_cast<int>(assetView.soundOptions.attenuationModel);
				if (ImGui::Combo("Model", &attenuationModelIndex, "None\0Inverse Distance\0Linear Distance\0Exponential Distance\0")) {
					assetView.soundOptions.attenuationModel = static_cast<aud::SoundAttenuationModel>(attenuationModelIndex);
					edited = true;
				}
				switch (assetView.soundOptions.attenuationModel) {
					case aud::SoundAttenuationModel::NO_ATTENUATION: break;
					case aud::SoundAttenuationModel::INVERSE_DISTANCE: [[fallthrough]];
					case aud::SoundAttenuationModel::EXPONENTIAL_DISTANCE:
						edited |= ImGui::SliderFloat("Min. Distance", &assetView.soundOptions.minDistance, 0.001f, 1000.0f, "%.3f m", ImGuiSliderFlags_Logarithmic);
						assetView.soundOptions.minDistance = max(assetView.soundOptions.minDistance, 0.001f);
						edited |= ImGui::SliderFloat("Max. Distance", &assetView.soundOptions.maxDistance, 0.001f, 1000.0f,
							(assetView.soundOptions.maxDistance == Limits<float>::MAX) ? "MAX" : "%.3f m", ImGuiSliderFlags_Logarithmic);
						assetView.soundOptions.maxDistance = max(assetView.soundOptions.maxDistance, 0.001f);
						assetView.soundOptions.maxDistance = max(assetView.soundOptions.maxDistance, assetView.soundOptions.minDistance);
						edited |= ImGui::SliderFloat("Rolloff Factor", &assetView.soundOptions.rolloffFactor, 0.001f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
						assetView.soundOptions.rolloffFactor = max(assetView.soundOptions.rolloffFactor, 0.001f);
						break;
					case aud::SoundAttenuationModel::LINEAR_DISTANCE:
						edited |= ImGui::SliderFloat("Min. Distance", &assetView.soundOptions.minDistance, 0.001f, 1000.0f, "%.3f m", ImGuiSliderFlags_Logarithmic);
						assetView.soundOptions.minDistance = max(assetView.soundOptions.minDistance, 0.001f);
						edited |= ImGui::SliderFloat("Max. Distance", &assetView.soundOptions.maxDistance, 0.001f, 1000.0f,
							(assetView.soundOptions.maxDistance == Limits<float>::MAX) ? "MAX" : "%.3f m", ImGuiSliderFlags_Logarithmic);
						assetView.soundOptions.maxDistance = max(assetView.soundOptions.maxDistance, 0.001f);
						assetView.soundOptions.maxDistance = max(assetView.soundOptions.maxDistance, assetView.soundOptions.minDistance);
						edited |= ImGui::SliderFloat("Rolloff Factor", &assetView.soundOptions.rolloffFactor, 0.0f, 1.0f, "%.3f");
						assetView.soundOptions.rolloffFactor = clamp(assetView.soundOptions.rolloffFactor, 0.0f, 1.0f);
						break;
				}
				if (assetView.soundOptions.maxDistance >= 1000.0f) {
					assetView.soundOptions.maxDistance = Limits<float>::MAX;
				}
				ImGui::SeparatorText("Misc.");
				edited |= ImGui::SliderFloat("Doppler Factor", &assetView.soundOptions.dopplerFactor, 0.0f, 10.0f, "%.3f");
				assetView.soundOptions.dopplerFactor = max(assetView.soundOptions.dopplerFactor, 0.0f);
				edited |= ImGui::Checkbox("Use Distance Delay", &assetView.soundOptions.useDistanceDelay);
				if (edited) {
					assetView.reloadSound(*soundStage, filesystem, filepath);
				}
				break;
			}
		}
	}

	void drawSolidColor(ImVec2 position, ImVec2 size, ImColor color) {
		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		drawList.AddRectFilled(position, position + size, color);
	}

	void drawCheckerboard(ImVec2 position, ImVec2 size, ImColor color, float zoomScale) {
		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		const ImVec2 end{position.x + size.x, position.y + size.y};
		const float squareSize = max(max(size.x, size.y) / 64.0f, 16.0f * zoomScale);
		bool checkerboardColorFlipX = false;
		bool checkerboardColorFlipY = false;
		for (float y = position.y; y < end.y; y += squareSize) {
			for (float x = position.x; x < end.x; x += squareSize) {
				const float endY = min(y + squareSize, end.y);
				const float endX = min(x + squareSize, end.x);
				ImColor squareColor = color;
				if (checkerboardColorFlipX != checkerboardColorFlipY) {
					squareColor.Value.x *= (2.0f / 3.0f);
					squareColor.Value.y *= (2.0f / 3.0f);
					squareColor.Value.z *= (2.0f / 3.0f);
				}
				drawList.AddRectFilled(ImVec2{x, y}, ImVec2{endX, endY}, squareColor);
				checkerboardColorFlipX = !checkerboardColorFlipX;
			}
			checkerboardColorFlipX = false;
			checkerboardColorFlipY = !checkerboardColorFlipY;
		}
	}

	void drawBackground(ImVec2 position, ImVec2 size, float zoomScale) {
		const vec4 rgba = preferences.backgroundColor;
		const ImColor color{rgba.x, rgba.y, rgba.z, rgba.w};
		switch (preferences.backgroundType) {
			case AssetViewerPreferences::BackgroundType::NONE: break;
			case AssetViewerPreferences::BackgroundType::SOLID: drawSolidColor(position, size, color); break;
			case AssetViewerPreferences::BackgroundType::CHECKERBOARD: drawCheckerboard(position, size, color, zoomScale); break;
		}
	}

	Filesystem& filesystem;
	String applicationDirectory;
	String configurationDirectory;
	String preferencesConfigurationFilepath;
	String guiConfigurationFilepath;
	String recentlyOpenedConfigurationFilepath;
	String thirdPartyLegalNotices{};
	AssetViewerPreferences preferences;
	ArrayList<String> recentlyOpenedFilepaths{};
	evt::EventPump eventPump{};
	gfx::Window window;
	gfx::Device device;
	gfx::Swapchain swapchain;
	gfx::Renderer2D renderer2D;
	gfx::Renderer3D renderer3D;
	imgui::GraphicalUserInterface gui;
	Optional<aud::SoundStage> soundStage{};
	ArrayList<float> audioOutputChannelVolumeStatistics{};
	res::Image skyImage{};
	gfx::Texture skyPreviewTexture{};
	Region2D screenViewport{};
	ArrayList<AssetTab> assetTabs{};
	Optional<size_t> lastActiveAssetTabIndex{};
	Optional<size_t> assetTabIndexToActivate{};
	Optional<String> assetFilepathToOpen{};
	bool assetFilepathToOpenWasDropped = false;
};

} // namespace

int main(int argc, char* argv[]) {
	try {
		NativeFilesystem filesystem{};

		String applicationDirectory{argv[0]};
		if (const size_t lastSlashPosition = applicationDirectory.find_last_of("/\\"); lastSlashPosition != CStringView::npos && lastSlashPosition > 0) {
			applicationDirectory = applicationDirectory.substr(0, lastSlashPosition);
		} else {
			applicationDirectory = ".";
		}

		String configurationDirectory = app::VirtualFilesystem{argv[0]}.createStandardOutputDirectory({
			.organizationName = AssetViewer::APPLICATION_ORGANIZATION,
			.applicationName = AssetViewer::APPLICATION_TITLE,
		});
		if (configurationDirectory.empty()) {
			configurationDirectory = ".";
		}

		AssetViewerArguments applicationArguments{};
		AssetViewerOptions applicationOptions{};
		try {
			cli::parseCommandLine(applicationArguments, applicationOptions, argc, argv);
		} catch (const cli::Error& e) {
			eprintln("{}", e.what());
			return app::ExitCode::FAILURE;
		}

		AssetViewer assetViewer{filesystem, std::move(applicationDirectory), std::move(configurationDirectory), applicationArguments, applicationOptions};
		assetViewer.run();
	} catch (...) {
		const String message = Error::formatCurrentExceptionMessage();
		eprintln("{}", message);
		evt::SimpleMessageBox::show(evt::MessageType::ERROR_MESSAGE, "Error", message);
		return app::ExitCode::FAILURE;
	}
	return app::ExitCode::SUCCESS;
}
