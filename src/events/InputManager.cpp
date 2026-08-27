// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/events/Error.hpp>
#include <GREM/events/Input.hpp>
#include <GREM/events/InputManager.hpp>

#include <sstream>     // std::istringstream, std::ostringstream
#include <type_traits> // std::remove_cvref_t
#include <utility>     // std::move

namespace grem::events {

namespace {

// Equal to tan(pi / 8) or tan(22.5 degrees), i.e. sin(22.5 degrees) / cos(22.5 degrees) or y / x at 22.5 degrees on unit circle.
constexpr float DIAGONAL_RATIO_THRESHOLD = numbers::SQRT2 - 1.0f;

} // namespace

InputManager::InputManager(const InputManagerOptions& options)
	: initialPreferences(options.preferences)
	, options(options) {}

void InputManager::loadConfiguration(String fileContents, CStringView filepath, FunctionView<Optional<OutputIndex>(StringView actionName)> getOutputIndex,
	FunctionView<void(StringView key, json::Reader& reader)> readExtraProperty) {
	try {
		std::istringstream stream{std::move(fileContents)};
		json::Reader reader{stream};
		InputManagerPreferences preferences = getInitialPreferences();
		reader.readCustomObject([&](const json::SourceLocation&, const json::String& key) -> void {
			if (key == "bindings") {
				reader.readCustomObject([&](const json::SourceLocation& source, const json::String& key) -> void {
					if (const Input input = findInputByIdentifier(key); input != Input::UNKNOWN) {
						const auto bindAction = [&](const json::SourceLocation& source, const json::String& value) -> void {
							if (const Optional<OutputIndex> outputIndex = getOutputIndex(value)) {
								addBinding(input, *outputIndex);
							} else {
								throw json::Error{formatString("Invalid action name \"{}\".", value), source};
							}
						};
						if (reader.nextIsString()) {
							bindAction(source, reader.readString());
						} else {
							reader.readCustomArray([&](const json::SourceLocation& source) -> void { bindAction(source, reader.readString()); });
						}
					} else {
						throw json::Error{formatString("Invalid input identifier \"{}\".", key), source};
					}
				});
			} else {
				bool found = false;
				meta::forEachNamedField(preferences, [&](StringView name, auto& field) -> void {
					if (!found && key == name) {
						found = true;
						if constexpr (same_as<std::remove_cvref_t<decltype(field)>, vec2>) {
							field *= reader.deserialize<vec2>();
						} else {
							reader.deserialize(field);
						}
					}
				});
				if (!found) {
					readExtraProperty(key, reader);
				}
			}
		});
		setPreferences(preferences);
	} catch (const json::Error& e) {
		if (!filepath.empty()) {
			if (e.messageAttachesToPrecedingFilepath()) {
				String message = formatString("Failed to load configuration.\n{}:", filepath);
				e.writeMessage(message);
				throw json::Error{message, json::SourceLocation{.lineNumber = 0, .columnNumber = 0}};
			}
			throw json::Error{formatString("Failed to load configuration \"{}\":\n{}", filepath, e.what()), e.getSource()};
		}
		throw json::Error{formatString("Failed to load configuration:\n{}", e.what()), e.getSource()};
	}
}

String InputManager::saveConfiguration(FunctionView<Optional<String>(OutputIndex outputIndex)> getActionName, Span<const Pair<StringView, json::Variant>> extraProperties) const {
	try {
		std::ostringstream stream{};
		json::Writer writer{stream};
		writer.writeCustomObject([&](auto writeProperty) -> void {
			const InputManagerPreferences preferences = getPreferences();
			const InputManagerPreferences initialPreferences = getInitialPreferences();
			const auto& preferencesFields = meta::getFields(preferences);
			const auto& initialPreferencesFields = meta::getFields(initialPreferences);
			meta::forEachNamedFieldIndex<InputManagerPreferences>([&](StringView name, auto index) -> void {
				const auto& field = get<index>(preferencesFields);
				const auto& initialField = get<index>(initialPreferencesFields);
				if (field != initialField) {
					if constexpr (same_as<std::remove_cvref_t<decltype(field)>, vec2>) {
						writeProperty(name, field / initialField);
					} else {
						writeProperty(name, field);
					}
				}
			});

			for (const auto& [key, value] : extraProperties) {
				writeProperty(key, value);
			}

			writeProperty("bindings", getBindings(), [&](Span<const Binding> bindings) -> void {
				writer.writeCustomObject([&](auto writeProperty) -> void {
					for (const Binding& binding : bindings) {
						const StringView inputIdentifier = getInputIdentifier(binding.input);

						json::Array array{};
						for (const OutputIndex outputIndex : binding.outputIndices) {
							if (const Optional<String> actionName = getActionName(outputIndex)) {
								array.emplace_back(*actionName);
							} else {
								throw events::Error{
									formatString("Failed to save configuration:\n"
												 "Invalid output number {} in binding for input \"{}\".",
										outputIndex, inputIdentifier)};
							}
						}

						if (array.size() == 1) {
							writeProperty(inputIdentifier, std::move(array.front()));
						} else {
							writeProperty(inputIdentifier, std::move(array));
						}
					}
				});
			});
		});
		return std::move(stream).str();
	} catch (const json::Error&) {
		Error::throwWithNested(Error{"Failed to save configuration."});
	}
}

void InputManager::handleEvent(const Event& event) {
	GREM_MATCH(event) {
		GREM_CASE(const WindowKeyboardFocusLostEvent& focusLost) {
			releaseAll(focusLost.timestamp);
			break;
		}
		GREM_CASE(const WindowMouseFocusLostEvent& mouseFocusLost) {
			currentState.mousePosition.reset();
			currentState.touchPosition.reset();
			currentState.touchPressure.reset();
			previousState.mousePosition.reset();
			previousState.touchPosition.reset();
			previousState.touchPressure.reset();
			break;
		}
		GREM_CASE(const KeyPressedEvent& pressed) {
			setCurrentState(pressed.timestamp, pressed.getInput(), ControlState{.activePresses = 1, .value = 1.0f});
			break;
		}
		GREM_CASE(const KeyReleasedEvent& released) {
			setCurrentState(released.timestamp, released.getInput(), ControlState{.activePresses = 0, .value = 0.0f});
			break;
		}
		GREM_CASE(const MouseMovedEvent& moved) {
			setMousePosition(moved.timestamp, moved.mousePosition, moved.relativeMouseMotion);
			break;
		}
		GREM_CASE(const MouseButtonPressedEvent& pressed) {
			setCurrentState(pressed.timestamp, pressed.getInput(), ControlState{.activePresses = 1, .value = 1.0f});
			break;
		}
		GREM_CASE(const MouseButtonReleasedEvent& released) {
			setCurrentState(released.timestamp, released.getInput(), ControlState{.activePresses = 0, .value = 0.0f});
			break;
		}
		GREM_CASE(const MouseWheelScrolledEvent& scrolled) {
			scrollMouseWheel(scrolled.timestamp, scrolled.scrollAmount);
			break;
		}
		GREM_CASE(const ControllerAddedEvent& added) {
			releaseAll(added.timestamp);
			break;
		}
		GREM_CASE(const ControllerRemovedEvent& removed) {
			releaseAll(removed.timestamp);
			break;
		}
		GREM_CASE(const ControllerAxisMovedEvent& moved) {
			switch (moved.axis) {
				case ControllerAxis::LEFT_STICK_X:
					setControllerLeftStickPosition(moved.timestamp,
						{moved.axisValue, (currentState.controllerLeftStickPosition) ? currentState.controllerLeftStickPosition->y : int16_t{0}});
					break;
				case ControllerAxis::LEFT_STICK_Y:
					setControllerLeftStickPosition(moved.timestamp,
						{(currentState.controllerLeftStickPosition) ? currentState.controllerLeftStickPosition->x : int16_t{0}, moved.axisValue});
					break;
				case ControllerAxis::RIGHT_STICK_X:
					setControllerRightStickPosition(moved.timestamp,
						{moved.axisValue, (currentState.controllerRightStickPosition) ? currentState.controllerRightStickPosition->y : int16_t{0}});
					break;
				case ControllerAxis::RIGHT_STICK_Y:
					setControllerRightStickPosition(moved.timestamp,
						{(currentState.controllerRightStickPosition) ? currentState.controllerRightStickPosition->x : int16_t{0}, moved.axisValue});
					break;
				case ControllerAxis::LEFT_TRIGGER: setControllerLeftTriggerPosition(moved.timestamp, moved.axisValue); break;
				case ControllerAxis::RIGHT_TRIGGER: setControllerRightTriggerPosition(moved.timestamp, moved.axisValue); break;
				default: break;
			}
			break;
		}
		GREM_CASE(const ControllerButtonPressedEvent& pressed) {
			setCurrentState(pressed.timestamp, pressed.getInput(), ControlState{.activePresses = 1, .value = 1.0f});
			break;
		}
		GREM_CASE(const ControllerButtonReleasedEvent& released) {
			setCurrentState(released.timestamp, released.getInput(), ControlState{.activePresses = 0, .value = 0.0f});
			break;
		}
		GREM_CASE(const TouchMovedEvent& moved) {
			setTouchPosition(moved.timestamp, moved.normalizedFingerPosition, moved.relativeNormalizedFingerMotion);
			setTouchPressure(moved.timestamp, moved.normalizedFingerPressure);
			break;
		}
		GREM_CASE(const TouchPressedEvent& pressed) {
			setTouchPressure(pressed.timestamp, pressed.normalizedFingerPressure);
			setCurrentState(pressed.timestamp, Input::TOUCH_FINGER_TAP, ControlState{.activePresses = 1, .value = 1.0f});
			break;
		}
		GREM_CASE(const TouchReleasedEvent& released) {
			setTouchPressure(released.timestamp, released.normalizedFingerPressure);
			setCurrentState(released.timestamp, Input::TOUCH_FINGER_TAP, ControlState{.activePresses = 0, .value = 0.0f});
			break;
		}
		GREM_CASE_DEFAULT(const auto& other) break;
	}
}

void InputManager::bind(Input input, ArrayList<OutputIndex> outputIndices) {
	if (getInputIndex(input) >= INPUT_COUNT || input == Input::UNKNOWN) {
		[[unlikely]];
		return;
	}

	if (outputIndices.empty()) {
		unbind(input);
		return;
	}

	const auto [it, inserted] = bindings.try_emplace(input);
	try {
		for (size_t i = 0; i < outputIndices.size(); ++i) {
			const OutputIndex outputIndex = outputIndices[i];
			if (outputIndex < OUTPUT_COUNT) {
				try {
					ArrayList<Input>& inputs = boundInputs[outputIndex];
					if (contains(inputs, input)) {
						// Make sure the catch block doesn't call pop_back() for an element we never added.
						outputIndices[i] = OUTPUT_COUNT;
					} else {
						inputs.push_back(input);
					}
				} catch (...) {
					while (i-- > 0) {
						const OutputIndex oldOutputIndex = outputIndices[i];
						if (oldOutputIndex < OUTPUT_COUNT) {
							boundInputs.at(oldOutputIndex).pop_back();
						}
					}
					throw;
				}
			}
		}
	} catch (...) {
		if (inserted) {
			bindings.erase(it);
		}
		throw;
	}

	// Save the assignment of the new output indices until last, when we know all the bindings were successfully added.
	static_assert(nothrow_movable<decltype(outputIndices)>);
	it->second = std::move(outputIndices);
}

void InputManager::addBindings(Input input, Span<const OutputIndex> outputIndices) {
	if (getInputIndex(input) >= INPUT_COUNT || input == Input::UNKNOWN) {
		[[unlikely]];
		return;
	}

	const auto [it, inserted] = bindings.try_emplace(input);
	try {
		for (const OutputIndex outputIndex : outputIndices) {
			if (outputIndex < OUTPUT_COUNT) {
				ArrayList<Input>& inputs = boundInputs[outputIndex];
				if (!contains(inputs, input)) {
					inputs.push_back(input);
					try {
						it->second.push_back(outputIndex);
					} catch (...) {
						inputs.pop_back();
						throw;
					}
				}
			}
		}
	} catch (...) {
		if (inserted) {
			bindings.erase(it);
		}
		throw;
	}
}

void InputManager::unbind(Input input) noexcept {
	if (const auto it = bindings.find(input); it != bindings.end()) {
		for (const OutputIndex outputIndex : it->second) {
			if (const auto itBoundInputs = boundInputs.find(outputIndex); itBoundInputs != boundInputs.end()) {
				erase(itBoundInputs->second, input);
				if (itBoundInputs->second.empty()) {
					boundInputs.erase(itBoundInputs);
				}
			}
		}
		bindings.erase(it);
	}
}

void InputManager::unbindAll() noexcept {
	bindings.clear();
	boundInputs.clear();
}

void InputManager::setCurrentState(TimePoint timestamp, Input input, ControlState newState) {
	const size_t inputIndex = getInputIndex(input);
	if (inputIndex >= INPUT_COUNT || input == Input::UNKNOWN) {
		[[unlikely]];
		return;
	}

	const ControlState oldState = currentState.inputStates[inputIndex];
	const ControlDelta delta{
		.addedPresses = newState.activePresses - oldState.activePresses,
		.motion = newState.value - oldState.value,
	};
	currentState.inputStates[inputIndex] = newState;

	relativeState.inputDeltas[inputIndex].addedPresses += delta.addedPresses;
	relativeState.inputDeltas[inputIndex].motion += delta.motion;

	if (delta.addedPresses > 0) {
		relativeState.transientInputPresses[inputIndex] = true;
	} else if (delta.addedPresses < 0) {
		relativeState.transientInputReleases[inputIndex] = true;
	}

	if (const auto it = bindings.find(input); it != bindings.end()) {
		for (const OutputIndex outputIndex : it->second) {
			if (delta.addedPresses > 0) {
				relativeState.transientOutputPresses[outputIndex] = true;
			} else if (delta.addedPresses < 0) {
				relativeState.transientOutputReleases[outputIndex] = true;
			}

			const ControlState newOutputState = getCurrentState(outputIndex);
			const ControlState oldOutputState{
				.activePresses = newOutputState.activePresses - delta.addedPresses,
				.value = newOutputState.value - delta.motion,
			};
			if (options.emitOutputEvents) {
				if (delta.motion != 0.0f) {
					pendingOutputEvents.emplace_back(OutputMoved{OutputEventBase{timestamp, outputIndex, newOutputState, delta}});
				}
				if (newOutputState.activePresses > 0) {
					if (oldOutputState.activePresses <= 0) {
						pendingOutputEvents.emplace_back(OutputPressed{OutputEventBase{timestamp, outputIndex, newOutputState, ControlDelta{.addedPresses = 0, .motion = 0.0f}}});
					}
				} else {
					if (oldOutputState.activePresses > 0) {
						pendingOutputEvents.emplace_back(OutputReleased{OutputEventBase{timestamp, outputIndex, newOutputState, ControlDelta{.addedPresses = 0, .motion = 0.0f}}});
					}
				}
			}
		}
	}
}

void InputManager::setCurrentState(TimePoint timestamp, OutputIndex outputIndex, ControlState newState) {
	if (outputIndex >= OUTPUT_COUNT) {
		[[unlikely]];
		return;
	}

	const ControlState oldState = currentState.outputExternalStates[outputIndex];
	const ControlDelta delta{
		.addedPresses = newState.activePresses - oldState.activePresses,
		.motion = newState.value - oldState.value,
	};
	currentState.outputExternalStates[outputIndex] = newState;

	relativeState.outputExternalDeltas[outputIndex].addedPresses += delta.addedPresses;
	relativeState.outputExternalDeltas[outputIndex].motion += delta.motion;

	if (delta.addedPresses > 0) {
		relativeState.transientOutputPresses[outputIndex] = true;
	} else if (delta.addedPresses < 0) {
		relativeState.transientOutputReleases[outputIndex] = true;
	}

	if (options.emitOutputEvents) {
		if (delta.motion != 0.0f) {
			pendingOutputEvents.emplace_back(OutputMoved{OutputEventBase{timestamp, outputIndex, newState, delta}});
		}
		if (newState.activePresses > 0) {
			if (oldState.activePresses <= 0) {
				pendingOutputEvents.emplace_back(OutputPressed{OutputEventBase{timestamp, outputIndex, newState, ControlDelta{.addedPresses = 0, .motion = 0.0f}}});
			}
		} else {
			if (oldState.activePresses > 0) {
				pendingOutputEvents.emplace_back(OutputReleased{OutputEventBase{timestamp, outputIndex, newState, ControlDelta{.addedPresses = 0, .motion = 0.0f}}});
			}
		}
	}
}

void InputManager::addRelativeState(TimePoint timestamp, Input input, ControlDelta delta) {
	const size_t inputIndex = getInputIndex(input);
	if (inputIndex >= INPUT_COUNT || input == Input::UNKNOWN) {
		[[unlikely]];
		return;
	}

	relativeState.inputDeltas[inputIndex].addedPresses += delta.addedPresses;
	relativeState.inputDeltas[inputIndex].motion += delta.motion;

	if (delta.addedPresses > 0) {
		relativeState.transientInputPresses[inputIndex] = true;
	} else if (delta.addedPresses < 0) {
		relativeState.transientInputReleases[inputIndex] = true;
	}

	if (const auto it = bindings.find(input); it != bindings.end()) {
		for (const OutputIndex outputIndex : it->second) {
			if (delta.addedPresses > 0) {
				relativeState.transientOutputPresses[outputIndex] = true;
			} else if (delta.addedPresses < 0) {
				relativeState.transientOutputReleases[outputIndex] = true;
			}

			if (options.emitOutputEvents) {
				if (delta.motion != 0.0f) {
					const ControlState newOutputState = getCurrentState(outputIndex);
					pendingOutputEvents.emplace_back(OutputMoved{OutputEventBase{timestamp, outputIndex, newOutputState, delta}});
				}
			}
		}
	}
}

void InputManager::addRelativeState(TimePoint timestamp, OutputIndex outputIndex, ControlDelta delta) {
	if (outputIndex >= OUTPUT_COUNT) {
		[[unlikely]];
		return;
	}

	relativeState.outputExternalDeltas[outputIndex].addedPresses += delta.addedPresses;
	relativeState.outputExternalDeltas[outputIndex].motion += delta.motion;

	if (delta.addedPresses > 0) {
		relativeState.transientOutputPresses[outputIndex] = true;
	} else if (delta.addedPresses < 0) {
		relativeState.transientOutputReleases[outputIndex] = true;
	}

	if (options.emitOutputEvents) {
		if (delta.motion != 0.0f) {
			const ControlState newState = getCurrentState(outputIndex);
			pendingOutputEvents.emplace_back(OutputMoved{OutputEventBase{timestamp, outputIndex, newState, delta}});
		}
	}
}

void InputManager::releaseAll(TimePoint timestamp) {
	for (OutputIndex outputIndex = 0; outputIndex < OUTPUT_COUNT; ++outputIndex) {
		const ControlState state = getCurrentState(outputIndex);
		if (state.activePresses != 0 || state.value != 0.0f) {
			relativeState.transientOutputReleases[outputIndex] = true;

			if (options.emitOutputEvents) {
				pendingOutputEvents.emplace_back(OutputMoved{OutputEventBase{timestamp, outputIndex, ControlState{.activePresses = 0, .value = 0.0f},
					ControlDelta{.addedPresses = -state.activePresses, .motion = -state.value}}});
				if (state.activePresses > 0) {
					pendingOutputEvents.emplace_back(
						OutputReleased{OutputEventBase{timestamp, outputIndex, ControlState{.activePresses = 0, .value = 0.0f}, ControlDelta{.addedPresses = 0, .motion = 0.0f}}});
				}
			}
		}

		relativeState.outputExternalDeltas[outputIndex].addedPresses -= currentState.outputExternalStates[outputIndex].activePresses;
		relativeState.outputExternalDeltas[outputIndex].motion -= currentState.outputExternalStates[outputIndex].value;
	}

	for (size_t inputIndex = 0; inputIndex < INPUT_COUNT; ++inputIndex) {
		const ControlState state = currentState.inputStates[inputIndex];
		if (state.activePresses != 0 || state.value != 0.0f) {
			relativeState.inputDeltas[inputIndex].addedPresses -= state.activePresses;
			relativeState.inputDeltas[inputIndex].motion -= state.value;
			relativeState.transientInputReleases[inputIndex] = true;
		}
	}

	currentState = {};
}

void InputManager::setCursorPosition(TimePoint timestamp, Optional<vec2>& position, vec2 newPosition, vec2 relativeMotion, vec2 sensitivity, Input inputLeft, Input inputRight,
	Input inputUp, Input inputDown) {
	position = newPosition;

	const bool isHorizontal = abs(relativeMotion.x / relativeMotion.y) > DIAGONAL_RATIO_THRESHOLD;
	const bool isVertical = abs(relativeMotion.y / relativeMotion.x) > DIAGONAL_RATIO_THRESHOLD;
	const vec2 motion = relativeMotion * sensitivity;

	addRelativeState(timestamp, inputLeft, ControlDelta{.addedPresses = (isHorizontal) ? ((relativeMotion.x < 0.0f) ? 1 : -1) : 0, .motion = -motion.x});
	addRelativeState(timestamp, inputRight, ControlDelta{.addedPresses = (isHorizontal) ? ((relativeMotion.x < 0.0f) ? -1 : 1) : 0, .motion = motion.x});
	addRelativeState(timestamp, inputUp, ControlDelta{.addedPresses = (isVertical) ? ((relativeMotion.y < 0.0f) ? 1 : -1) : 0, .motion = -motion.y});
	addRelativeState(timestamp, inputDown, ControlDelta{.addedPresses = (isVertical) ? ((relativeMotion.y < 0.0f) ? -1 : 1) : 0, .motion = motion.y});
}

void InputManager::scrollMouseWheel(TimePoint timestamp, vec2 scrollAmount) {
	relativeState.transientMouseWheelScroll += scrollAmount;

	const vec2 motion = scrollAmount * options.preferences.mouseWheelScrollSensitivity;

	addRelativeState(timestamp, Input::MOUSE_SCROLL_LEFT, ControlDelta{.addedPresses = static_cast<int32_t>(-scrollAmount.x), .motion = -motion.x});
	addRelativeState(timestamp, Input::MOUSE_SCROLL_RIGHT, ControlDelta{.addedPresses = static_cast<int32_t>(scrollAmount.x), .motion = motion.x});
	addRelativeState(timestamp, Input::MOUSE_SCROLL_DOWN, ControlDelta{.addedPresses = static_cast<int32_t>(-scrollAmount.y), .motion = -motion.y});
	addRelativeState(timestamp, Input::MOUSE_SCROLL_UP, ControlDelta{.addedPresses = static_cast<int32_t>(scrollAmount.y), .motion = motion.y});
}

void InputManager::setControllerStickPosition(TimePoint timestamp, Optional<i16vec2>& position, i16vec2 newPosition, vec2 sensitivity, float curveExponent, float innerDeadzone,
	float outerDeadzone, Input inputLeft, Input inputRight, Input inputUp, Input inputDown) {
	position = newPosition;

	ControlState newStateLeft{.activePresses = 0, .value = 0.0f};
	ControlState newStateRight{.activePresses = 0, .value = 0.0f};
	ControlState newStateUp{.activePresses = 0, .value = 0.0f};
	ControlState newStateDown{.activePresses = 0, .value = 0.0f};

	const vec2 vector = max(vec2{-1.0f}, vec2{newPosition} / static_cast<float>(Limits<int16_t>::MAX));
	const float distance = length(vector);
	if (distance > innerDeadzone) {
		const bool isHorizontal = abs(vector.x / vector.y) > DIAGONAL_RATIO_THRESHOLD;
		const bool isVertical = abs(vector.y / vector.x) > DIAGONAL_RATIO_THRESHOLD;

		const float activeZoneEnd = (outerDeadzone >= 1.0f || innerDeadzone >= outerDeadzone) ? 1.0f : outerDeadzone;
		const float activeZoneRange = activeZoneEnd - innerDeadzone;
		const float deadzoneAdjustedDistance = (innerDeadzone <= 0.0f) ? distance : min((distance - innerDeadzone) / activeZoneRange, 1.0f);
		const float curveAndDeadzoneAdjustedDistance = (curveExponent <= 0.0f) ? deadzoneAdjustedDistance : pow(deadzoneAdjustedDistance, curveExponent);
		const vec2 value = vector * sensitivity * (32767.0f * curveAndDeadzoneAdjustedDistance / distance);

		newStateLeft = {.activePresses = (isHorizontal) ? ((newPosition.x < 0) ? 1 : -1) : 0, .value = -value.x};
		newStateRight = {.activePresses = (isHorizontal) ? ((newPosition.x < 0) ? -1 : 1) : 0, .value = value.x};
		newStateUp = {.activePresses = (isVertical) ? ((newPosition.y < 0) ? 1 : -1) : 0, .value = -value.y};
		newStateDown = {.activePresses = (isVertical) ? ((newPosition.y < 0) ? -1 : 1) : 0, .value = value.y};
	}

	setCurrentState(timestamp, inputLeft, newStateLeft);
	setCurrentState(timestamp, inputRight, newStateRight);
	setCurrentState(timestamp, inputUp, newStateUp);
	setCurrentState(timestamp, inputDown, newStateDown);
}

void InputManager::setControllerTriggerPosition(TimePoint timestamp, Optional<int16_t>& position, int16_t newPosition, float lowerDeadzone, float upperDeadzone, Input inputAxis) {
	position = newPosition;

	ControlState newState{.activePresses = 0, .value = 0.0f};

	const float signedDistance = max(-1.0f, static_cast<float>(newPosition) / static_cast<float>(Limits<int16_t>::MAX));
	if (signedDistance > lowerDeadzone) {
		const float activeZoneEnd = (upperDeadzone >= 1.0f || lowerDeadzone >= upperDeadzone) ? 1.0f : upperDeadzone;
		const float activeZoneRange = activeZoneEnd - lowerDeadzone;
		const float deadzoneAdjustedDistance = (lowerDeadzone <= 0.0f) ? signedDistance : min((signedDistance - lowerDeadzone) / activeZoneRange, 1.0f);
		const float value = deadzoneAdjustedDistance;

		newState = {.activePresses = 1, .value = value};
	}

	setCurrentState(timestamp, inputAxis, newState);
}

void InputManager::setTouchPressure(TimePoint timestamp, float newPressure) {
	const float lowerDeadzone = options.preferences.touchPressureLowerDeadzone;
	const float upperDeadzone = options.preferences.touchPressureUpperDeadzone;

	currentState.touchPressure = newPressure;

	ControlState newState{.activePresses = 0, .value = 0.0f};

	const float signedDistance = newPressure;
	if (signedDistance > lowerDeadzone) {
		const float activeZoneEnd = (upperDeadzone >= 1.0f || lowerDeadzone >= upperDeadzone) ? 1.0f : upperDeadzone;
		const float activeZoneRange = activeZoneEnd - lowerDeadzone;
		const float deadzoneAdjustedDistance = (lowerDeadzone <= 0.0f) ? signedDistance : min((signedDistance - lowerDeadzone) / activeZoneRange, 1.0f);
		const float value = deadzoneAdjustedDistance;

		newState = {.activePresses = 1, .value = value};
	}

	setCurrentState(timestamp, Input::TOUCH_FINGER_PRESSURE, newState);
}

} // namespace grem::events
