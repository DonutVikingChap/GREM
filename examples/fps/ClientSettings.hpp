// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CLIENT_SETTINGS_HPP
#define GREM_EXAMPLES_FPS_CLIENT_SETTINGS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/core/time.hpp>

struct ClientSettings {
	struct AudioSettings {
		float outputVolume = 0.5f;
		bool showAudioStats = false;
	};

	struct GraphicsSettings {
		uint32_t verticalRenderResolution = 0;
		uint32_t maxMultisampleCount = 4;
		bool enableBloom = true;
		bool enableBlur = false;
		bool useVerticalSplitScreenLayout = false;
		bool showLightProbeVolumesDebugVisualization = false;
		bool showFPS = true;
		bool showPerformanceStats = false;
	};

	struct ConnectionSettings {
		bool enableIncomingFakeLag = false;
		float incomingFakeLagMeanMilliseconds = 25.0f;
		float incomingFakeLagStddevMilliseconds = 3.0f;
		bool enableOutgoingFakeLag = false;
		float outgoingFakeLagMeanMilliseconds = 25.0f;
		float outgoingFakeLagStddevMilliseconds = 3.0f;
		bool enableIncomingFakeLoss = false;
		float incomingFakeLossPercent = 1.0f;
		bool enableOutgoingFakeLoss = false;
		float outgoingFakeLossPercent = 1.0f;
		bool showConnectionStats = false;
		bool showTimeline = false;
	};

	struct WorldSettings {
		bool showPosition = false;
		bool showHUD = true;
	};

	struct ChatSettings {
		Duration displayDuration = 10_seconds;
		bool displayAll = false;
	};

	[[nodiscard]] static ClientSettings load(const Filesystem& filesystem, CStringView filepath) {
		ClientSettings result{};
		if (Optional<String> fileContents = filesystem.tryReadInputFileString(filepath)) {
			json::deserializeFromString(std::move(*fileContents), result);

			result.audio.outputVolume = clamp(result.audio.outputVolume, 0.0f, 1.0f);
			result.graphics.maxMultisampleCount = clamp(roundUpToPowerOf2(static_cast<uint32_t>(result.graphics.maxMultisampleCount)), uint32_t{1}, uint32_t{16});
		}
		return result;
	}

	AudioSettings audio{};
	GraphicsSettings graphics{};
	ConnectionSettings connection{};
	WorldSettings world{};
	ChatSettings chat{};

	void save(Filesystem& filesystem, CStringView filepath) const {
		const String fileContents = json::serializeToString(*this);
		filesystem.createParentOutputDirectories(filepath);
		filesystem.openEmptyOutputFile(filepath).write(fileContents);
	}
};

#endif
