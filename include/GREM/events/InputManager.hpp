// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EVENTS_INPUT_MANAGER_HPP
#define GREM_EVENTS_INPUT_MANAGER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/attributes.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/BitArray.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formats/ascii.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/events/Event.hpp>
#include <GREM/events/Input.hpp>

#include <memory>      // std::allocator
#include <type_traits> // std::underlying_type_t
#include <utility>     // std::move

namespace grem::json {
struct Reader; // Forward declaration, to avoid including json.hpp.
template <template <typename> typename Allocator>
class VariantBase;                                 // Forward declaration, to avoid including json.hpp.
using Variant = json::VariantBase<std::allocator>; // Forward declaration, to avoid including json.hpp.
} // namespace grem::json

namespace grem::events {

class InputManager; // Forward declaration.

namespace detail {

template <typename Action>
[[nodiscard]] inline const HashMap<String, uint8_t>& getActionMap() {
	static const HashMap<String, uint8_t> actionMap = []() -> HashMap<String, uint8_t> {
		HashMap<String, uint8_t> result{};
		meta::forEachNamedEnumerand<Action>([&](StringView name, auto action) -> void {
			result[ascii::convertToUppercaseString(name)] = static_cast<uint8_t>(static_cast<std::underlying_type_t<Action>>(action()));
		});
		return result;
	}();
	return actionMap;
}

template <typename Action>
inline constexpr Array<StringView, meta::enum_size_v<Action>> ACTION_NAMES = []() -> Array<StringView, meta::enum_size_v<Action>> {
	Array<StringView, meta::enum_size_v<Action>> result{};
	meta::forEachIndexedNamedEnumerand<Action>([&](auto index, StringView name, auto) -> void { result[index] = name; });
	return result;
}();

} // namespace detail

/**
 * User preferences for an InputManager.
 */
struct InputManagerPreferences {
	/**
	 * Mouse sensitivity coefficient.
	 *
	 * The influence of mouse movement on its bound output values will be
	 * multiplied by this value. This should adjust the original mouse motion,
	 * which is measured in screen coordinates, such that it becomes more in
	 * line with the canonical 0 to 1 range of a key press.
	 */
	vec2 mouseSensitivity{convertDegreesToRadians(0.022f)};

	/**
	 * Mouse wheel scroll sensitivity coefficient.
	 *
	 * The influence of mouse wheel scrolling on its bound output values will be
	 * multiplied by this value.
	 */
	vec2 mouseWheelScrollSensitivity{1.0f};

	/**
	 * Touch finger motion sensitivity coefficient.
	 *
	 * The influence of finger movement on its bound output values will be
	 * multiplied by this value before being applied.
	 */
	vec2 touchMotionSensitivity{1.0f};

	/**
	 * Controller left analog stick sensitivity coefficient.
	 *
	 * The influence of controller left analog stick movement on its bound
	 * output values will be multiplied by this value. This should adjust the
	 * original stick position, which is measured from -32768 to 32767, such
	 * that it becomes more in line with the canonical 0 to 1 range of a key press.
	 */
	vec2 controllerLeftStickSensitivity{1.0f / 32767.0f};

	/**
	 * Controller right analog stick sensitivity coefficient.
	 *
	 * The influence of controller right analog stick movement on its bound
	 * output values will be multiplied by this value. This should adjust the
	 * original stick position, which is measured from -32768 to 32767, such
	 * that it becomes more in line with the canonical 0 to 1 range of a key press.
	 */
	vec2 controllerRightStickSensitivity{1.0f / 32767.0f};

	/**
	 * Controller left analog stick curve exponent.
	 *
	 * The stick's normalized deadzone-adjusted distance will be raised to this
	 * power before sensitivity is applied. This affects the shape of the
	 * sensitivity curve. Lower values yield a faster response near the center
	 * that trails off towards the edges, while higher values yield a slower
	 * response near the center that ramps up more quickly near the edges. A
	 * value of 1 yields a perfectly linear response.
	 */
	float controllerLeftStickCurveExponent = 1.0f;

	/**
	 * Controller right analog stick curve exponent.
	 *
	 * The stick's normalized deadzone-adjusted distance will be raised to this
	 * power before sensitivity is applied. This affects the shape of the
	 * sensitivity curve. Lower values yield a faster response near the center
	 * that trails off towards the edges, while higher values yield a slower
	 * response near the center that ramps up more quickly near the edges. A
	 * value of 1 yields a perfectly linear response.
	 */
	float controllerRightStickCurveExponent = 1.0f;

	/**
	 * Touch finger pressure lower deadzone fraction.
	 *
	 * When the pressure amount is less than or equal to this value, the actual
	 * position will be ignored and treated as if it was 0 in order to avoid
	 * fluctuations and accidental inputs when the finger is at rest.
	 */
	float touchPressureLowerDeadzone = 0.25f;

	/**
	 * Touch finger pressure upper deadzone fraction.
	 *
	 * This is the maximum pressure amount needed to reach a deadzone-adjusted
	 * value of 1, beyond which higher pressures will be ignored.
	 */
	float touchPressureUpperDeadzone = 0.9f;

	/**
	 * Controller left analog stick inner deadzone fraction.
	 *
	 * When the stick is at a position whose fractional distance from the center
	 * is less than or equal to this value, the actual position will be ignored
	 * and treated as if it was (0, 0) in order to avoid fluctuations or
	 * drifting when the stick is at rest.
	 */
	float controllerLeftStickInnerDeadzone = 0.25f;

	/**
	 * Controller left analog stick outer deadzone fraction.
	 *
	 * This is the maximum fractional stick distance from the center needed to
	 * reach a deadzone-adjusted distance of 1, beyond which further stick
	 * distances will be ignored.
	 */
	float controllerLeftStickOuterDeadzone = 0.9f;

	/**
	 * Controller right analog stick inner deadzone fraction.
	 *
	 * When the stick is at a position whose fractional distance from the center
	 * is less than or equal to this value, the actual position will be ignored
	 * and treated as if it was (0, 0) in order to avoid fluctuations or
	 * drifting when the stick is at rest.
	 */
	float controllerRightStickInnerDeadzone = 0.25f;

	/**
	 * Controller right analog stick outer deadzone fraction.
	 *
	 * This is the maximum fractional stick distance from the center needed to
	 * reach a deadzone-adjusted distance of 1, beyond which further stick
	 * distances will be ignored.
	 */
	float controllerRightStickOuterDeadzone = 0.9f;

	/**
	 * Controller left trigger lower deadzone fraction.
	 *
	 * When the trigger is at a position whose fractional distance from the rest
	 * position is less than or equal to this value, the actual position will be
	 * ignored and treated as if it was 0 in order to avoid fluctuations and
	 * accidental inputs when the trigger is at rest.
	 */
	float controllerLeftTriggerLowerDeadzone = 0.2f;

	/**
	 * Controller left trigger upper deadzone fraction.
	 *
	 * This is the maximum fractional trigger press amount needed to reach a
	 * deadzone-adjusted value of 1, beyond which higher press amounts will be
	 * ignored.
	 */
	float controllerLeftTriggerUpperDeadzone = 0.9f;

	/**
	 * Controller right trigger lower deadzone fraction.
	 *
	 * When the trigger is at a position whose fractional distance from the rest
	 * position is less than or equal to this value, the actual position will be
	 * ignored and treated as if it was 0 in order to avoid fluctuations and
	 * accidental inputs when the trigger is at rest.
	 */
	float controllerRightTriggerLowerDeadzone = 0.2f;

	/**
	 * Controller right trigger upper deadzone fraction.
	 *
	 * This is the maximum fractional trigger press amount needed to reach a
	 * deadzone-adjusted value of 1, beyond which higher press amounts will be
	 * ignored.
	 */
	float controllerRightTriggerUpperDeadzone = 0.9f;
};

/**
 * Configuration options for an InputManager.
 */
struct InputManagerOptions {
	/**
	 * User preferences configuration, see InputManagerPreferences.
	 */
	InputManagerPreferences preferences{};

	/**
	 * Whether the input manager should emit output events or not.
	 *
	 * \warning If set to true, the built-up list of events should be polled at
	 *          a regular interval, such as every tick of the application.
	 *          Otherwise, they will keep piling up until the application runs
	 *          out of memory.
	 */
	bool emitOutputEvents = false;
};

/**
 * System for mapping physical #Input controls to abstract output numbers and
 * processing input events that control their associated values.
 *
 * By keeping an instance of this class and continuously feeding it the events
 * associated with a specific user received from an EventPump, it can serve as
 * the canonical representation of that user's input state across the program.
 * After handling the events received in a frame, the input manager can be
 * queried for the current state of any specific physical inputs, or the values
 * of the abstract outputs to which they are bound, as well as the corresponding
 * state on the previous frame. This combination also allows the inputs or
 * outputs which were just pressed since the previous frame to be derived as
 * well.
 *
 * The supported input types include keyboard, mouse, touch and game controller
 * devices, which can all be bound to and affect the same abstract output(s)
 * simultaneously.
 *
 * If there are multiple connected input devices of the same type, such as
 * multiple game controllers, their outputs cannot be differentiated between
 * within a single input manager. Therefore, if any filtering of events by their
 * source/user is desired, such as in a splitscreen game, the filtering must be
 * done manually _before_ calling handleEvent(), for example by having one input
 * manager per source/user and choosing which one to feed each input event to by
 * searching for the user matching the event's
 * ControllerEventBase::controllerID.
 *
 * ## Configuration JSON format
 *
 * When saving and loading an input manager configuration, the expected JSON
 * format is a JSON object consisting of optional properties corresponding to
 * the fields of InputManagerPreferences, in addition to a property named
 * "bindings", whose value is an object where each property has the lowercase
 * identifier of the input to bind as its key and the case-insensitive name(s)
 * of the actions to add to it as its value, either as an array or a single
 * string.
 *
 * All sensitivity values specified in the configuration represent multipliers
 * of the initial sensitivity values provided in the input manager options, and
 * may be specified as either single numbers or 2D vectors.
 *
 * For example, given this action enum:
 * ```cpp
 * enum class Action : uint8_t {
 *     CONFIRM = 0,
 *     CANCEL = 1,
 *     JUMP = 2,
 *     MOVE_LEFT = 3,
 *     MOVE_RIGHT = 4,
 *     MOVE_DOWN = 5,
 *     MOVE_UP = 6,
 * };
 * ```
 * The following JSON would be a valid example of a possible
 * configuration:
 * ```json
 * {
 *     "mouseSensitivity": 1.5,
 *     "controllerLeftStickInnerDeadzone": 0.3,
 *     "bindings": {
 *         "key_a": "move_left",
 *         "key_d": "move_right",
 *         "key_s": "move_down",
 *         "key_w": "move_up",
 *         "key_space": ["JUMP", "CONFIRM"],
 *         "key_escape": "CANCEL"
 *     }
 * }
 * ```
 */
class InputManager {
public:
	/**
	 * State value of a control associated with some input(s) and/or output(s).
	 */
	struct ControlState {
		int32_t activePresses; ///< Current number of presses of the control.
		float value;           ///< Current floating-point value of the control, affected by sensitivity and deadzone on applicable inputs.
	};

	/**
	 * State value of a 2-dimensional control associated with some input(s)
	 * and/or output(s).
	 */
	struct ControlState2D {
		i32vec2 activePresses; ///< Current number of presses of the control.
		vec2 value;            ///< Current floating-point value of the control, affected by sensitivity and deadzone on applicable inputs.
	};

	/**
	 * State value of a 3-dimensional control associated with some input(s)
	 * and/or output(s).
	 */
	struct ControlState3D {
		i32vec3 activePresses; ///< Current number of presses of the control.
		vec3 value;            ///< Current floating-point value of the control, affected by sensitivity and deadzone on applicable inputs.
	};

	/**
	 * Relative change of the state value between frames of a control associated
	 * with some input(s) and/or output(s).
	 */
	struct ControlDelta {
		int32_t addedPresses; ///< Number of presses added to the control.
		float motion;         ///< Floating-point value added to the control, affected by sensitivity and deadzone on applicable inputs.
	};

	/**
	 * Relative change of the state value between frames of a 2-dimensional
	 * control associated with some input(s) and/or output(s).
	 */
	struct ControlDelta2D {
		i32vec2 addedPresses; ///< Number of presses added to the control.
		vec2 motion;          ///< Floating-point value added to the control, affected by sensitivity and deadzone on applicable inputs.
	};

	/**
	 * Relative change of the state value between frames of a 3-dimensional
	 * control associated with some input(s) and/or output(s).
	 */
	struct ControlDelta3D {
		i32vec3 addedPresses; ///< Number of presses added to the control.
		vec3 motion;          ///< Floating-point value added to the control, affected by sensitivity and deadzone on applicable inputs.
	};

	/**
	 * Abstract output number.
	 */
	using OutputIndex = uint8_t;

	/**
	 * The maximum supported number of separate outputs that the input manager
	 * can keep track of.
	 */
	static constexpr OutputIndex OUTPUT_COUNT = 64;

	/**
	 * Output event base.
	 */
	class OutputEventBase {
	public:
		/**
		 * Get the timestamp of the event.
		 *
		 * \return the time when the event occured.
		 */
		[[nodiscard]] GREM_ALWAYS_INLINE TimePoint getTimestamp() const noexcept {
			return timestamp;
		}

		/**
		 * Get the affected output number.
		 *
		 * \return the output number that the event affected.
		 *
		 * \sa isAction()
		 */
		[[nodiscard]] GREM_ALWAYS_INLINE OutputIndex getOutputIndex() const noexcept {
			return outputIndex;
		}

		/**
		 * Check if the affected output is a certain "action" of any enum type,
		 * which is interpreted as corresponding to the output number equal to
		 * its underlying value.
		 *
		 * \param action
		 *
		 * \sa getOutputIndex()
		 */
		template <enumeration Action>
		[[nodiscard]] GREM_ALWAYS_INLINE bool isAction(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
			return outputIndex == getOutputIndex(action);
		}

		/**
		 * Get the raw total absolute value of the output after the event took
		 * place.
		 *
		 * \return the absolute state of the output after the event.
		 */
		[[nodiscard]] GREM_ALWAYS_INLINE ControlState getState() const noexcept {
			return state;
		}

		/**
		 * Get the raw total relative value that was applied to the output by
		 * the event.
		 *
		 * \return the relative state of the output applied by the event.
		 */
		[[nodiscard]] GREM_ALWAYS_INLINE ControlDelta getDelta() const noexcept {
			return delta;
		}

	private:
		friend InputManager;

		constexpr OutputEventBase(TimePoint timestamp, OutputIndex outputIndex, ControlState state, ControlDelta delta) noexcept
			: timestamp(timestamp)
			, outputIndex(outputIndex)
			, state(state)
			, delta(delta) {}

		TimePoint timestamp;
		OutputIndex outputIndex;
		ControlState state;
		ControlDelta delta;
	};

	/** Output value changed. */
	struct OutputMoved : OutputEventBase {};

	/** Output was pressed. */
	struct OutputPressed : OutputEventBase {};

	/** Output was released. */
	struct OutputReleased : OutputEventBase {};

	/**
	 * Data structure containing information about an output event.
	 */
	struct OutputEvent : Variant<OutputMoved, OutputPressed, OutputReleased> {};

	/**
	 * A single configured binding from a physical input to a set of abstract
	 * output numbers.
	 */
	struct Binding {
		Input input;                          ///< Input that is bound.
		ArrayList<OutputIndex> outputIndices; ///< Set of abstract output numbers that the input is bound to.
	};

	/**
	 * Construct an input manager with the default options.
	 */
	InputManager()
		: InputManager(InputManagerOptions{}) {}

	/**
	 * Construct an input manager.
	 *
	 * \param options initial configuration of the input manager, see
	 *        InputManagerOptions.
	 */
	GREM_API(events) explicit InputManager(const InputManagerOptions& options);

	/**
	 * Apply the configuration specified in a JSON configuration file, adding to
	 * the previous bindings if any already existed for the same inputs.
	 *
	 * \param fileContents contents of the JSON file containing the
	 *        configuration to apply.
	 * \param filepath optional input filepath of the JSON file, for error
	 *        reporting.
	 * \param getOutputIndex function that returns the abstract output number
	 *        corresponding to the given action name, or an empty optional if
	 *        the action name is invalid.
	 * \param readExtraProperty function for reading any custom extra properties
	 *        in the top-level JSON object that are not part of the
	 *        configuration format.
	 *
	 * \throws events::Error on failure to parse the file contents, or if
	 *         invalid input/action names were specified.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 *
	 * \sa addBinding()
	 * \sa unbindAll()
	*/
	GREM_API(events)
	void loadConfiguration(
		String fileContents, CStringView filepath, FunctionView<Optional<OutputIndex>(StringView actionName)> getOutputIndex,
		FunctionView<void(StringView key, json::Reader& reader)> readExtraProperty = [](StringView, json::Reader&) -> void {});

	/**
	 * Apply the configuration specified in a JSON configuration file, adding to
	 * the previous bindings if any already existed for the same inputs.
	 *
	 * \param fileContents contents of the JSON file containing the
	 *        configuration to apply.
	 * \param getOutputIndex function that returns the abstract output number
	 *        corresponding to the given action name, or an empty optional if
	 *        the action name is invalid.
	 * \param readExtraProperty function for reading any custom extra properties
	 *        in the top-level JSON object that are not part of the
	 *        configuration format.
	 *
	 * \throws events::Error on failure to parse the file contents, or if
	 *         invalid input/action names were specified.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 *
	 * \sa addBinding()
	 * \sa unbindAll()
	*/
	void loadConfiguration(
		String fileContents, FunctionView<Optional<OutputIndex>(StringView actionName)> getOutputIndex,
		FunctionView<void(StringView key, json::Reader& reader)> readExtraProperty = [](StringView, json::Reader&) -> void {}) {
		loadConfiguration(std::move(fileContents), {}, getOutputIndex, readExtraProperty);
	}

	/**
	 * Apply the configuration specified in a JSON configuration file, assuming
	 * a set of abstract output numbers associated with a given enum type,
	 * adding to the previous bindings if any already existed for the same
	 * inputs.
	 *
	 * \tparam Action enum type with a fixed underlying type, where each
	 *         consecutive valid enumerand starting at value 0 corresponds to
	 *         the output number of the same value.
	 *
	 * \param fileContents contents of the JSON file containing the
	 *        configuration to apply.
	 * \param filepath optional input filepath of the JSON file, for error
	 *        reporting.
	 * \param readExtraProperty function for reading any custom extra properties
	 *        in the top-level JSON object that are not part of the
	 *        configuration format.
	 *
	 * \throws events::Error on failure to parse the file contents, or if
	 *         invalid input/action names were specified.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 *
	 * \sa addBinding()
	 * \sa unbindAll()
	*/
	template <enumeration Action>
	void loadConfiguration(
		String fileContents, CStringView filepath = {}, FunctionView<void(StringView key, json::Reader& reader)> readExtraProperty = [](StringView, json::Reader&) -> void {}) {
		loadConfiguration(
			std::move(fileContents), filepath,
			[](StringView actionName) -> Optional<OutputIndex> {
				const HashMap<String, uint8_t>& actionMap = detail::getActionMap<Action>();
				if (const auto it = actionMap.find(ascii::convertToUppercaseString(actionName)); it != actionMap.end()) {
					return it->second;
				}
				return {};
			},
			readExtraProperty);
	}

	/**
	 * Apply the configuration specified in a JSON configuration file, adding to
	 * the previous bindings if any already existed for the same inputs.
	 *
	 * \param filesystem filesystem to load the file from.
	 * \param filepath input filepath of the JSON file to load.
	 * \param getOutputIndex function that returns the abstract output number
	 *        corresponding to the given action name, or an empty optional if
	 *        the action name is invalid.
	 * \param readExtraProperty function for reading any custom extra properties
	 *        in the top-level JSON object that are not part of the
	 *        configuration format.
	 *
	 * \throws File::Error on failure to open or read from the file.
	 * \throws events::Error on failure to parse the file contents, or if
	 *         invalid input/action names were specified.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 *
	 * \sa addBinding()
	 * \sa unbindAll()
	*/
	void loadConfiguration(
		const Filesystem& filesystem, CStringView filepath, FunctionView<Optional<OutputIndex>(StringView actionName)> getOutputIndex,
		FunctionView<void(StringView key, json::Reader& reader)> readExtraProperty = [](StringView, json::Reader&) -> void {}) {
		loadConfiguration(filesystem.readInputFileString(filepath), filepath, getOutputIndex, readExtraProperty);
	}

	/**
	 * Apply the configuration specified in a JSON configuration file, assuming
	 * a set of abstract output numbers associated with a given enum type,
	 * adding to the previous bindings if any already existed for the same
	 * inputs.
	 *
	 * \tparam Action enum type with a fixed underlying type, where each
	 *         consecutive valid enumerand starting at value 0 corresponds to
	 *         the output number of the same value.
	 *
	 * \param filesystem filesystem to load the file from.
	 * \param filepath input filepath of the JSON file to load.
	 * \param readExtraProperty function for reading any custom extra properties
	 *        in the top-level JSON object that are not part of the
	 *        configuration format.
	 *
	 * \throws File::Error on failure to open or read from the file.
	 * \throws events::Error on failure to parse the file contents, or if
	 *         invalid input/action names were specified.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 *
	 * \sa addBinding()
	 * \sa unbindAll()
	*/
	template <enumeration Action>
	void loadConfiguration(
		const Filesystem& filesystem, CStringView filepath, FunctionView<void(StringView key, json::Reader& reader)> readExtraProperty = [](StringView, json::Reader&) -> void {}) {
		loadConfiguration<Action>(filesystem.readInputFileString(filepath), filepath, readExtraProperty);
	}

	/**
	 * Save the current preferences and bindings to a JSON configuration string.
	 *
	 * \param getActionName function that returns the action name corresponding
	 *        to a specific output number, or an empty optional if the output
	 *        number is invalid.
	 * \param extraProperties custom extra properties to write to the top-level
	 *        JSON object that are not part of the configuration format.
	 *
	 * \return the contents of the JSON configuration file.
	 *
	 * \throws events::Error on failure to create a valid JSON configuration, or
	 *         if the input manager contains bindings with outputs that are not
	 *         in the specified action set.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 */
	[[nodiscard]] GREM_API(events) String
		saveConfiguration(FunctionView<Optional<String>(OutputIndex outputIndex)> getActionName, Span<const Pair<StringView, json::Variant>> extraProperties = {}) const;

	/**
	 * Save the current preferences and bindings to a JSON configuration string
	 * for a set of abstract output numbers.
	 *
	 * \tparam Action enum type with a fixed underlying type, where each
	 *         consecutive valid enumerand starting at value 0 corresponds to
	 *         the output number of the same value.
	 *
	 * \param extraProperties custom extra properties to write to the top-level
	 *        JSON object that are not part of the configuration format.
	 *
	 * \return the contents of the JSON configuration file.
	 *
	 * \throws events::Error on failure to create a valid JSON configuration, or
	 *         if the input manager contains bindings with outputs that are not
	 *         in the specified action set.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 */
	template <enumeration Action>
	[[nodiscard]] String saveConfiguration(Span<const Pair<StringView, json::Variant>> extraProperties = {}) const {
		return saveConfiguration(
			[](OutputIndex outputIndex) -> Optional<String> {
				const Span<const StringView> actionNames = detail::ACTION_NAMES<Action>;
				if (outputIndex < actionNames.size()) {
					return ascii::convertToLowercaseString(actionNames[outputIndex]);
				}
				return {};
			},
			extraProperties);
	}

	/**
	 * Save the current preferences and bindings to a JSON configuration file.
	 *
	 * \param filesystem filesystem to save the file to.
	 * \param filepath output filepath at which to save the JSON file.
	 * \param getActionName function that returns the action name corresponding
	 *        to a specific output number, or an empty optional if the output
	 *        number is invalid.
	 * \param extraProperties custom extra properties to write to the top-level
	 *        JSON object that are not part of the configuration format.
	 *
	 * \throws File::Error on failure to create or write to the file.
	 * \throws events::Error on failure to write a valid JSON configuration to
	 *         the file, or if the input manager contains bindings with outputs
	 *         that are not in the specified action set.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note %Any parent directories of the specified output filepath will be
	 *       created if they don't already exist.
	 * \note %Any preferences that have the same value as the initial values
	 *       from when the input manager was constructed are not written to the
	 *       configuration.
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 */
	void saveConfiguration(Filesystem& filesystem, CStringView filepath, FunctionView<Optional<String>(OutputIndex outputIndex)> getActionName,
		Span<const Pair<StringView, json::Variant>> extraProperties = {}) const {
		filesystem.createParentOutputDirectories(filepath);
		filesystem.openEmptyOutputFile(filepath).write(saveConfiguration(getActionName, extraProperties));
	}

	/**
	 * Save the current preferences and bindings to a JSON configuration file
	 * for a set of abstract output numbers.
	 *
	 * \tparam Action enum type with a fixed underlying type, where each
	 *         consecutive valid enumerand starting at value 0 corresponds to
	 *         the output number of the same value.
	 *
	 * \param filesystem filesystem to save the file to.
	 * \param filepath output filepath at which to save the JSON file.
	 * \param extraProperties custom extra properties to write to the top-level
	 *        JSON object that are not part of the configuration format.
	 *
	 * \throws File::Error on failure to create or write to the file.
	 * \throws events::Error on failure to write a valid JSON configuration to
	 *         the file, or if the input manager contains bindings with outputs
	 *         that are not in the specified action set.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note %Any parent directories of the specified output filepath will be
	 *       created if they don't already exist.
	 * \note %Any preferences that have the same value as the initial values
	 *       from when the input manager was constructed are not written to the
	 *       configuration.
	 * \note See the detailed description of the InputManager class for the
	 *       specification of the configuration file JSON format.
	 */
	template <enumeration Action>
	void saveConfiguration(Filesystem& filesystem, CStringView filepath, Span<const Pair<StringView, json::Variant>> extraProperties = {}) const {
		filesystem.createParentOutputDirectories(filepath);
		filesystem.openEmptyOutputFile(filepath).write(saveConfiguration<Action>(extraProperties));
	}

	/**
	 * Advance the internal state to the next frame of inputs.
	 *
	 * This effectively shifts any inputs/outputs which are currently considered
	 * to be pressed to the previous frame, and resets the relative input state.
	 *
	 * \note This function should typically be called once every frame during
	 *       the application::Application::update() callback.
	 * \note This function does not affect events.
	 *
	 * \sa handleEvent()
	 * \sa pollOutputEvents()
	 */
	void update() {
		previousState = currentState;
		relativeState = {};
	}

	/**
	 * Handle an event from an EventPump, which may cause updates to the
	 * internal input/output state of the current frame.
	 *
	 * \param event event to handle.
	 *
	 * \note This function should typically be called during the
	 *       application::Application::update() callback, after polling events
	 *       from an EventPump.
	 *
	 * \sa update()
	 */
	GREM_API(events) void handleEvent(const Event& event);

	/**
	 * Poll events from the input manager and update the internal event buffer.
	 *
	 * \return a non-owning read-only view over the polled events, stored in the
	 *         internal event buffer, which is valid until the next call to
	 *         pollEvents() or until the input manager is destroyed, whichever
	 *         happens first.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 * 
	 * \note This function requires output events to be enabled in order to
	 *       produce any events, see InputManagerOptions::emitOutputEvents.
	 *
	 * \sa InputManagerOptions::emitOutputEvents
	 * \sa setEmitOutputEvents()
	 * \sa update()
	 * \sa getLatestPolledOutputEvents()
	 */
	Span<const OutputEvent> pollOutputEvents() {
		polledOutputEvents.clear();
		polledOutputEvents.swap(pendingOutputEvents);
		return polledOutputEvents;
	}

	/**
	 * Get the latest output events in the internal event buffer that occured
	 * since the last call to pollOutputEvents().
	 *
	 * \return a non-owning read-only view over the output events, stored in the
	 *         internal event buffer, which is valid until the next call to
	 *         pollEvents() or until the input manager is destroyed, whichever
	 *         happens first.
	 *
	 * \sa update()
	 * \sa pollOutputEvents()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Span<const OutputEvent> getLatestPolledOutputEvents() const noexcept {
		return polledOutputEvents;
	}

	/**
	 * Bind a physical input to a set of abstract output numbers, overriding the
	 * previous binding if one already existed for the same input.
	 *
	 * \param input physical input to set the binding for.
	 * \param outputIndices set of valid output numbers between 0 (inclusive)
	 *        and #OUTPUT_COUNT (exclusive) that the input should control.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If outputIndices is empty, the input will be unbound.
	 *
	 * \sa addBinding()
	 * \sa unbind()
	 * \sa unbindAll()
	 */
	GREM_API(events) void bind(Input input, ArrayList<OutputIndex> outputIndices);

	/**
	 * Bind a physical input to a set of abstract output numbers, adding to the
	 * previous binding if one already existed for the same input.
	 *
	 * \param input physical input to add the binding to.
	 * \param outputIndices additional set of valid output numbers between 0
	 *        (inclusive) and #OUTPUT_COUNT (exclusive) that the input should
	 *        control.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa bind()
	 * \sa unbind()
	 * \sa unbindAll()
	 */
	GREM_API(events) void addBindings(Input input, Span<const OutputIndex> outputIndices);

	/**
	 * Bind a physical input to an abstract output number, adding to the
	 * previous binding if one already existed for the same input.
	 *
	 * \param input physical input to add the binding to.
	 * \param outputIndex additional valid output number between 0 (inclusive)
	 *        and #OUTPUT_COUNT (exclusive) that the input should control.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa bind()
	 * \sa unbind()
	 * \sa unbindAll()
	 */
	GREM_ALWAYS_INLINE void addBinding(Input input, OutputIndex outputIndex) {
		addBindings(input, Span{&outputIndex, 1});
	}

	/**
	 * Remove all bindings from a specific input.
	 *
	 * \param input physical input to remove the binding from.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa bind()
	 * \sa addBinding()
	 * \sa unbindAll()
	 */
	GREM_API(events) void unbind(Input input) noexcept;

	/**
	 * Remove all bindings from all inputs.
	 *
	 * \sa bind()
	 * \sa addBinding()
	 * \sa unbind()
	 * \sa resetAllStates()
	 */
	GREM_API(events) void unbindAll() noexcept;

	/**
	 * Reset the states of all inputs and outputs for the current and previous
	 * frames, and clear all polled and pending events.
	 *
	 * \note Does not affect bindings or options/preferences.
	 *
	 * \sa unbindAll()
	 * \sa releaseAll()
	 */
	void resetAllStates() noexcept {
		currentState = {};
		previousState = {};
		relativeState = {};
		polledOutputEvents.clear();
		pendingOutputEvents.clear();
	}

	/**
	 * Set the contribution of an input to its bound outputs to a new state.
	 *
	 * \param timestamp time point at which the state change occured.
	 * \param input physical input to set the state of.
	 * \param newState state to set the input's contribution to.
	 *
	 * \note This function is called automatically by the input manager when an
	 *       input is pressed by handleEvent(), and should typically not be
	 *       called manually.
	 *
	 * \sa addRelativeState()
	 */
	GREM_API(events) void setCurrentState(TimePoint timestamp, Input input, ControlState newState);

	/**
	 * Set the external contribution to an output to a new state.
	 *
	 * \param timestamp time point at which the state change occured.
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to set the state of.
	 * \param newState state to set the output's external contribution to.
	 *
	 * \note This function is called automatically by the input manager when an
	 *       input that is bound to the output is activated by handleEvent(),
	 *       and should typically not be called manually.
	 *
	 * \sa addRelativeState()
	 */
	GREM_API(events) void setCurrentState(TimePoint timestamp, OutputIndex outputIndex, ControlState newState);

	/**
	 * Like setCurrentState(TimePoint, OutputIndex, ControlState),
	 * but accepts an "action" of any enum type, which is interpreted as
	 * corresponding to the output number equal to its underlying value.
	 *
	 * \sa setCurrentState(TimePoint, OutputIndex, ControlState)
	 */
	template <enumeration Action>
	GREM_ALWAYS_INLINE void setCurrentState(TimePoint timestamp, Action action, ControlState newState = {.activePresses = 1, .value = 1.0f})
		requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		setCurrentState(timestamp, getOutputIndex(action), newState);
	}

	/**
	 * Apply a relative offset to the contribution of an input to its bound
	 * outputs for this frame only.
	 *
	 * \param timestamp time point at which the state change occured.
	 * \param input physical input to apply the delta to.
	 * \param delta relative offset to apply to all bound outputs for this
	 *        frame.
	 *
	 * \note This function is called automatically by the input manager when an
	 *       input is triggered by handleEvent(), and should typically not be
	 *       called manually.
	 *
	 * \sa setCurrentState()
	 */
	GREM_API(events) void addRelativeState(TimePoint timestamp, Input input, ControlDelta delta);

	/**
	 * Apply a relative offset to the external contribution to an output for
	 * this frame only.
	 *
	 * \param timestamp time point at which the state change occured.
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to apply the delta to.
	 * \param delta relative offset to apply to the output for this frame.
	 *
	 * \note This function is called automatically by the input manager when an
	 *       input that is bound to the output is activated by handleEvent(),
	 *       and should typically not be called manually.
	 *
	 * \sa setCurrentState()
	 */
	GREM_API(events) void addRelativeState(TimePoint timestamp, OutputIndex outputIndex, ControlDelta delta);

	/**
	 * Like addRelativeState(TimePoint, OutputIndex, ControlDelta),
	 * but accepts an "action" of any enum type, which is interpreted as
	 * corresponding to the output number equal to its underlying value.
	 *
	 * \sa addRelativeState(TimePoint, OutputIndex, ControlDelta)
	 */
	template <enumeration Action>
	GREM_ALWAYS_INLINE void addRelativeState(TimePoint timestamp, Action action, ControlDelta delta) requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		addRelativeState(timestamp, getOutputIndex(action), delta);
	}

	/**
	 * Release all currently held inputs and outputs for the current frame,
	 * reset the positions of all inputs, and reset the contributions of all
	 * inputs and outputs to 0.
	 *
	 * \param timestamp time point at which the release occured.
	 *
	 * \sa resetAllState()
	 * \sa setCurrentState()
	 * \sa addRelativeState()
	 */
	GREM_API(events) void releaseAll(TimePoint timestamp);

	/**
	 * Set the user preferences of the input manager.
	 *
	 * \param preferences new user preferences, see InputManagerPreferences.
	 *
	 * \sa InputManagerOptions::preferences
	 * \sa getPreferences()
	 */
	GREM_ALWAYS_INLINE void setPreferences(const InputManagerPreferences& preferences) noexcept {
		options.preferences = preferences;
	}

	/**
	 * Set whether output events should be emitted or not.
	 *
	 * \param emitOutputEvents new state.
	 *
	 * \sa InputManagerOptions::emitOutputEvents
	 *
	 * \warning If set to true, the built-up list of events should be polled at
	 *          a regular interval, such as every tick of the application.
	 *          Otherwise, they will keep piling up until the application runs
	 *          out of memory.
	 *
	 * \sa pollOutputEvents()
	 */
	GREM_ALWAYS_INLINE void setEmitOutputEvents(bool emitOutputEvents) noexcept {
		options.emitOutputEvents = emitOutputEvents;
	}

	/**
	 * Get the current user preferences of the input manager.
	 *
	 * \return a read-only reference to the current preferences, see
	 *         InputManagerPreferences.
	 *
	 * \sa setPreferences()
	 * \sa getInitialPreferences()
	 * \sa getOptions()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const InputManagerPreferences& getPreferences() const noexcept {
		return options.preferences;
	}

	/**
	 * Get the initial user preferences of the input manager.
	 *
	 * \return a read-only reference to the initial preferences, see
	 *         InputManagerPreferences.
	 *
	 * \sa getPreferences()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const InputManagerPreferences& getInitialPreferences() const noexcept {
		return initialPreferences;
	}

	/**
	 * Get the current configuration options of the input manager.
	 *
	 * \return a read-only reference to the current options, see
	 *         InputManagerOptions.
	 *
	 * \sa getPreferences()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE const InputManagerOptions& getOptions() const noexcept {
		return options;
	}

	/**
	 * Check if this input manager has any active bindings for any input.
	 *
	 * \return true if there exists some input that is currently mapped to a set
	 *         of outputs, false otherwise.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool hasAnyBindings() const noexcept {
		return !bindings.empty();
	}

	/**
	 * Get all active bindings of this input manager.
	 *
	 * \return an iterable input range of all of the bindings that currently
	 *         exist between a physical input and a set of abstract outputs.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \sa hasAnyBindings()
	 * \sa getBoundOutputs()
	 * \sa getBoundInputs()
	 */
	[[nodiscard]] Allocation<Binding> getBindings() const {
		Allocation<Binding> result(bindings.size());
		auto it = result.begin();
		for (const auto& [input, outputIndices] : bindings) {
			*it++ = Binding{.input = input, .outputIndices = outputIndices};
		}
		return result;
	}

	/**
	 * Get the set of outputs that a specific input is currently bound to.
	 *
	 * \param input physical input for which to search for a binding.
	 *
	 * \return a read-only view over the bound outputs, valid until the set of
	 *         bindings is modified.
	 *
	 * \sa getBindings()
	 * \sa getBoundInputs()
	 */
	[[nodiscard]] Span<const OutputIndex> getBoundOutputs(Input input) const noexcept {
		if (const auto it = bindings.find(input); it != bindings.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Get the set of inputs that are bound to a specific output.
	 *
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to get the bound inputs of.
	 *
	 * \return a read-only view over the bound inputs, valid until the set of
	 *         bindings is modified.
	 *
	 * \sa getBindings()
	 * \sa getBoundOutputs()
	 */
	[[nodiscard]] Span<const Input> getBoundInputs(OutputIndex outputIndex) const noexcept {
		if (const auto it = boundInputs.find(outputIndex); it != boundInputs.end()) {
			return it->second;
		}
		return {};
	}

	/**
	 * Like getBoundInputs(OutputIndex) const noexcept, but accepts an "action" of
	 * any enum type, which is interpreted as corresponding to the output number
	 * equal to its underlying value.
	 *
	 * \sa getBoundInputs(OutputIndex) const noexcept
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE Span<const Input> getBoundInputs(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getBoundInputs(getOutputIndex(action));
	}

	/**
	 * Get the latest known mouse position processed by the input manager.
	 *
	 * \return if the mouse has a known position, returns a 2D vector, in screen
	 *         coordinates (typically pixels), that represents it. Otherwise,
	 *         returns an empty optional.
	 *
	 * \note Instead of reading the state of the mouse directly, prefer to use
	 *       the getCurrentState() function with an abstract output number or
	 *       action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getPreviousMousePosition()
	 * \sa getRelativeMouseMotion()
	 * \sa mouseJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<vec2> getCurrentMousePosition() const noexcept {
		return currentState.mousePosition;
	}

	/**
	 * Get the second last known mouse position processed by the input manager.
	 *
	 * \return if the mouse had a known position on the previous frame, returns
	 *         a 2D vector, in screen coordinates (typically pixels), that
	 *         represents it. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the mouse directly, prefer to use
	 *       the getPreviousState() function with an abstract output number or
	 *       action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentMousePosition()
	 * \sa getRelativeMouseMotion()
	 * \sa mouseJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<vec2> getPreviousMousePosition() const noexcept {
		return previousState.mousePosition;
	}

	/**
	 * Get the relative motion between the last two known mouse positions
	 * processed by the input manager.
	 *
	 * \return if the mouse has a known position, and also had a known position
	 *         on the previous frame, returns a relative 2D vector, in screen
	 *         coordinates (typically pixels), that represents the motion from
	 *         the previous position to the current position. Otherwise, returns
	 *         an empty optional.
	 *
	 * \note Instead of reading the state of the mouse directly, prefer to use
	 *       the getRelativeState() function with an abstract output number or
	 *       action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getMousePosition()
	 * \sa getPreviousMousePosition()
	 * \sa mouseJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<vec2> getRelativeMouseMotion() const noexcept {
		if (currentState.mousePosition && previousState.mousePosition) {
			return *currentState.mousePosition - *previousState.mousePosition;
		}
		return {};
	}

	/**
	 * Check if the mouse just moved on the current frame.
	 *
	 * \return true if any mouse motion was processed in the current frame,
	 *         false otherwise.
	 *
	 * \note Instead of reading the state of the mouse directly, prefer to use
	 *       the isPressed(), justPressed() and justReleased() functions with an
	 *       abstract output number or action enum whenever possible, since this
	 *       can allow the user to bind a different form of input, such as a
	 *       button, to the control instead, according to their preferences.
	 *
	 * \sa getCurrentMousePosition()
	 * \sa getPreviousMousePosition()
	 * \sa getRelativeMouseMotion()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool mouseJustMoved() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::MOUSE_MOTION_UP)] ||   //
		       relativeState.transientInputPresses[getInputIndex(Input::MOUSE_MOTION_DOWN)] || //
		       relativeState.transientInputPresses[getInputIndex(Input::MOUSE_MOTION_LEFT)] || //
		       relativeState.transientInputPresses[getInputIndex(Input::MOUSE_MOTION_RIGHT)];
	}

	/**
	 * Get the relative distance scrolled horizontally by the mouse wheel in the
	 * last frame processed by the input manager.
	 *
	 * \return a relative 2D vector, in screen coordinates (typically pixels),
	 *         that represents how much the mouse wheel just scrolled.
	 *
	 * \note Instead of reading the state of the mouse directly, prefer to use
	 *       the getCurrentState() function with an abstract output number or
	 *       action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa mouseWheelJustScrolledHorizontally()
	 * \sa mouseWheelJustScrolledVertically()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE vec2 getRelativeMouseWheelScroll() const noexcept {
		return relativeState.transientMouseWheelScroll;
	}

	/**
	 * Check if the mouse wheel was just scrolled horizontally on the current
	 * frame.
	 *
	 * \return true if any horizontal mouse wheel motion was processed in the
	 *         current frame, false otherwise.
	 *
	 * \note Instead of reading the state of the mouse directly, prefer to use
	 *       the isPressed(), justPressed() and justReleased() functions with an
	 *       abstract output number or action enum whenever possible, since this
	 *       can allow the user to bind a different form of input, such as a
	 *       button, to the control instead, according to their preferences.
	 *
	 * \sa getRelativeMouseWheelScroll()
	 * \sa mouseWheelJustScrolledVertically()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool mouseWheelJustScrolledHorizontally() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::MOUSE_SCROLL_LEFT)] || //
		       relativeState.transientInputPresses[getInputIndex(Input::MOUSE_SCROLL_RIGHT)];
	}

	/**
	 * Check if the mouse wheel was just scrolled vertically on the current
	 * frame.
	 *
	 * \return true if any vertical mouse wheel motion was processed in the
	 *         current frame, false otherwise.
	 *
	 * \note Instead of reading the state of the mouse directly, prefer to use
	 *       the isPressed(), justPressed() and justReleased() functions with an
	 *       abstract output number or action enum whenever possible, since this
	 *       can allow the user to bind a different form of input, such as a
	 *       button, to the control instead, according to their preferences.
	 *
	 * \sa getRelativeMouseWheelScroll()
	 * \sa mouseWheelJustScrolledHorizontally()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool mouseWheelJustScrolledVertically() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::MOUSE_SCROLL_UP)] || //
		       relativeState.transientInputPresses[getInputIndex(Input::MOUSE_SCROLL_DOWN)];
	}

	/**
	 * Get the latest known touch finger position processed by the input
	 * manager.
	 *
	 * \return if the finger has a known position, returns a 2D vector, in
	 *         normalized coordinates [0, 1], that represents it. Otherwise,
	 *         returns an empty optional.
	 *
	 * \note Instead of reading the state of the finger directly, prefer to
	 *       use the getCurrentState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getPreviousTouchPosition()
	 * \sa getRelativeTouchMotion()
	 * \sa getCurrentTouchPressure()
	 * \sa touchJustMoved()
	 * \sa touchJustChangedPressure()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<vec2> getCurrentTouchPosition() const noexcept {
		return currentState.touchPosition;
	}

	/**
	 * Get the second last known touch finger position processed by the input
	 * manager.
	 *
	 * \return if the finger had a known position on the previous frame, returns
	 *         a 2D vector, in normalized coordinates [0, 1], that represents
	 *         it. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the finger directly, prefer to
	 *       use the getPreviousState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentTouchPosition()
	 * \sa getRelativeTouchMotion()
	 * \sa touchJustMoved()
	 * \sa touchJustChangedPressure()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<vec2> getPreviousTouchPosition() const noexcept {
		return previousState.touchPosition;
	}

	/**
	 * Get the relative motion between the last two known touch finger positions
	 * processed by the input manager.
	 *
	 * \return if the finger had a known position on the previous frame, and
	 *         also has a known position on the current frame, returns a
	 *         relative 2D vector, in normalized coordinates [-1, 1], that
	 *         represents the motion from the previous position to the current
	 *         position. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the finger directly, prefer to
	 *       use the getRelativeState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentTouchPosition()
	 * \sa getRelativeTouchMotion()
	 * \sa touchJustMoved()
	 * \sa touchJustChangedPressure()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<vec2> getRelativeTouchMotion() const noexcept {
		if (currentState.touchPosition && previousState.touchPosition) {
			return *currentState.touchPosition - *previousState.touchPosition;
		}
		return {};
	}

	/**
	 * Check if the touch finger just moved on the current frame.
	 *
	 * \return true if any finger motion was processed in the current frame,
	 *         false otherwise.
	 *
	 * \note Instead of reading the state of the finger directly, prefer to use
	 *       the isPressed(), justPressed() and justReleased() functions with an
	 *       abstract output number or action enum whenever possible, since this
	 *       can allow the user to bind a different form of input, such as a
	 *       button, to the control instead, according to their preferences.
	 *
	 * \sa getTouchPosition()
	 * \sa getTouchPressure()
	 * \sa touchJustChangedPressure()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool touchJustMoved() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::TOUCH_FINGER_MOTION_UP)] ||    //
		       relativeState.transientInputPresses[getInputIndex(Input::TOUCH_FINGER_MOTION_DOWN)] ||  //
		       relativeState.transientInputPresses[getInputIndex(Input::TOUCH_FINGER_MOTION_LEFT)] ||  //
		       relativeState.transientInputPresses[getInputIndex(Input::TOUCH_FINGER_MOTION_RIGHT)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::TOUCH_FINGER_MOTION_UP)] ||   //
		       relativeState.transientInputReleases[getInputIndex(Input::TOUCH_FINGER_MOTION_DOWN)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::TOUCH_FINGER_MOTION_LEFT)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::TOUCH_FINGER_MOTION_RIGHT)];
	}

	/**
	 * Get the latest known touch finger pressure processed by the input
	 * manager.
	 *
	 * \return if the finger has a known pressure, returns a float in the range
	 *         [0, 1], that represents it. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the finger directly, prefer to
	 *       use the getCurrentState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentTouchPosition()
	 * \sa getPreviousTouchPressure()
	 * \sa getRelativeTouchPressure()
	 * \sa touchJustMoved()
	 * \sa touchJustChangedPressure()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float> getCurrentTouchPressure() const noexcept {
		return currentState.touchPressure;
	}

	/**
	 * Get the second last known touch finger pressure processed by the input
	 * manager.
	 *
	 * \return if the finger had a known pressure on the previous frame, returns
	 *         a float in the range [0, 1], that represents it. Otherwise,
	 *         returns an empty optional.
	 *
	 * \note Instead of reading the state of the finger directly, prefer to
	 *       use the getPreviousState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentTouchPressure()
	 * \sa getRelativeTouchPressure()
	 * \sa touchJustMoved()
	 * \sa touchJustChangedPressure()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float> getPreviousTouchPressure() const noexcept {
		return previousState.touchPressure;
	}

	/**
	 * Get the relative pressure between the last two known touch finger
	 * pressures processed by the input manager.
	 *
	 * \return if the finger had a known pressure on the previous frame, and
	 *         also has a known pressure on the current frame, returns a float
	 *         in the range [-1, 1], that represents the change in pressure from
	 *         the previous pressure to the current pressure. Otherwise, returns
	 *         an empty optional.
	 *
	 * \note Instead of reading the state of the finger directly, prefer to
	 *       use the getRelativeState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a joystick, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentTouchPressure()
	 * \sa getPreviousTouchPressure()
	 * \sa touchJustMoved()
	 * \sa touchJustChangedPressure()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<float> getRelativeTouchPressure() const noexcept {
		if (currentState.touchPressure && previousState.touchPressure) {
			return *currentState.touchPressure - *previousState.touchPressure;
		}
		return {};
	}

	/**
	 * Check if the touch finger just changed pressure on the current frame.
	 *
	 * \return true if any finger pressure change was processed in the current
	 *         frame, false otherwise.
	 *
	 * \note Instead of reading the state of the finger directly, prefer to use
	 *       the isPressed(), justPressed() and justReleased() functions with an
	 *       abstract output number or action enum whenever possible, since this
	 *       can allow the user to bind a different form of input, such as a
	 *       button, to the control instead, according to their preferences.
	 *
	 * \sa getTouchPosition()
	 * \sa getTouchPressure()
	 * \sa touchJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool touchJustChangedPressure() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::TOUCH_FINGER_PRESSURE)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::TOUCH_FINGER_PRESSURE)];
	}

	/**
	 * Get the latest known position of the left analog stick of the connected
	 * controller, if there is one.
	 *
	 * \return if a controller is connected and its left analog stick has a
	 *         known position, returns a 2D vector that represents it, where
	 *         each component is in the range [-32768, 32767]. Otherwise,
	 *         returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getCurrentState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a mouse, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getPreviousControllerLeftStickPosition()
	 * \sa getRelativeControllerLeftStickMotion()
	 * \sa controllerLeftStickJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<i16vec2> getCurrentControllerLeftStickPosition() const noexcept {
		return currentState.controllerLeftStickPosition;
	}

	/**
	 * Get the second last known position of the left analog stick of the
	 * connected controller, if there is one.
	 *
	 * \return if a controller is connected and its left analog stick had a
	 *         known position on the previous frame, returns a 2D vector that
	 *         represents it, where each component is in the range
	 *         [-32768, 32767]. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getPreviousState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a mouse, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentControllerLeftStickPosition()
	 * \sa getRelativeControllerLeftStickMotion()
	 * \sa controllerLeftStickJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<i16vec2> getPreviousControllerLeftStickPosition() const noexcept {
		return previousState.controllerLeftStickPosition;
	}

	/**
	 * Get the relative motion between the last two known positions of the left
	 * analog stick of the connected controller, if there is one.
	 *
	 * \return if a controller is connected and its left analog stick has a
	 *         known position, and also had a known position on the previous
	 *         frame, returns a relative 2D vector that represents the motion
	 *         from the previous position to the current position, where each
	 *         component is in the range [-65535, 65535]. Otherwise, returns an
	 *         empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getRelativeState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a mouse, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentControllerLeftStickPosition()
	 * \sa getPreviousControllerLeftStickPosition()
	 * \sa controllerLeftStickJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<i32vec2> getRelativeControllerLeftStickMotion() const noexcept {
		if (currentState.controllerLeftStickPosition && previousState.controllerLeftStickPosition) {
			return i32vec2{*currentState.controllerLeftStickPosition} - i32vec2{*previousState.controllerLeftStickPosition};
		}
		return {};
	}

	/**
	 * Check if the controller left analog stick just moved on the current
	 * frame.
	 *
	 * \return true if any left analog stick motion was processed in the current
	 *         frame, false otherwise.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the isPressed(), justPressed() and justReleased() functions
	 *       with an abstract output number or action enum whenever possible,
	 *       since this can allow the user to bind a different form of input,
	 *       such as a key, to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentControllerLeftStickPosition()
	 * \sa getPreviousControllerLeftStickPosition()
	 * \sa getRelativeControllerLeftStickMotion()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool controllerLeftStickJustMoved() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_LEFT_STICK_UP)] ||    //
		       relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_LEFT_STICK_DOWN)] ||  //
		       relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_LEFT_STICK_LEFT)] ||  //
		       relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_LEFT_STICK_RIGHT)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_LEFT_STICK_UP)] ||   //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_LEFT_STICK_DOWN)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_LEFT_STICK_LEFT)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_LEFT_STICK_RIGHT)];
	}

	/**
	 * Get the latest known position of the right analog stick of the connected
	 * controller, if there is one.
	 *
	 * \return if a controller is connected and its right analog stick has a
	 *         known position, returns a 2D vector that represents it, where
	 *         each component is in the range [-32768, 32767]. Otherwise,
	 *         returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getCurrentState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a mouse, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getPreviousControllerRightStickPosition()
	 * \sa getRelativeControllerRightStickMotion()
	 * \sa controllerRightStickJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<i16vec2> getCurrentControllerRightStickPosition() const noexcept {
		return currentState.controllerRightStickPosition;
	}

	/**
	 * Get the second last known position of the right analog stick of the
	 * connected controller, if there is one.
	 *
	 * \return if a controller is connected and its right analog stick had a
	 *         known position on the previous frame, returns a 2D vector that
	 *         represents it, where each component is in the range
	 *         [-32768, 32767]. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getPreviousState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a mouse, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentControllerRightStickPosition()
	 * \sa getRelativeControllerRightStickMotion()
	 * \sa controllerRightStickJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<i16vec2> getPreviousControllerRightStickPosition() const noexcept {
		return previousState.controllerRightStickPosition;
	}

	/**
	 * Get the relative motion between the last two known positions of the right
	 * analog stick of the connected controller, if there is one.
	 *
	 * \return if a controller is connected and its right analog stick has a
	 *         known position, and also had a known position on the previous
	 *         frame, returns a relative 2D vector that represents the motion
	 *         from the previous position to the current position, where each
	 *         component is in the range [-65535, 65535]. Otherwise, returns an
	 *         empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getRelativeState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a mouse, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentControllerRightStickPosition()
	 * \sa getPreviousControllerRightStickPosition()
	 * \sa controllerRightStickJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<i32vec2> getRelativeControllerRightStickMotion() const noexcept {
		if (currentState.controllerRightStickPosition && previousState.controllerRightStickPosition) {
			return i32vec2{*currentState.controllerRightStickPosition} - i32vec2{*previousState.controllerRightStickPosition};
		}
		return {};
	}

	/**
	 * Check if the controller right analog stick just moved on the current
	 * frame.
	 *
	 * \return true if any right analog stick motion was processed in the
	 *         current frame, false otherwise.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the isPressed(), justPressed() and justReleased() functions
	 *       with an abstract output number or action enum whenever possible,
	 *       since this can allow the user to bind a different form of input,
	 *       such as a key, to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentControllerRightStickPosition()
	 * \sa getPreviousControllerRightStickPosition()
	 * \sa getRelativeControllerRightStickMotion()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool controllerRightStickJustMoved() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_STICK_UP)] ||    //
		       relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_STICK_DOWN)] ||  //
		       relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_STICK_LEFT)] ||  //
		       relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_STICK_RIGHT)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_STICK_UP)] ||   //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_STICK_DOWN)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_STICK_LEFT)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_STICK_RIGHT)];
	}

	/**
	 * Get the latest known position of the left trigger of the connected
	 * controller, if there is one.
	 *
	 * \return if a controller is connected and its left trigger has a known
	 *         position, returns a value that represents it, in the range
	 *         [0, 32767]. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getCurrentState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a key, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getPreviousControllerLeftTriggerPosition()
	 * \sa getRelativeControllerLeftTriggerMotion()
	 * \sa controllerLeftTriggerJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int16_t> getCurrentControllerLeftTriggerPosition() const noexcept {
		return currentState.controllerLeftTriggerPosition;
	}

	/**
	 * Get the second last known position of the left trigger of the connected
	 * controller, if there is one.
	 *
	 * \return if a controller is connected and its left trigger had a known
	 *         position on the previous frame, returns a value that represents
	 *         it, in the range [0, 32767]. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getPreviousState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a key, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentControllerLeftTriggerPosition()
	 * \sa getRelativeControllerLeftTriggerMotion()
	 * \sa controllerLeftTriggerJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int16_t> getPreviousControllerLeftTriggerPosition() const noexcept {
		return previousState.controllerLeftTriggerPosition;
	}

	/**
	 * Get the relative motion between the last two known positions of the left
	 * trigger of the connected controller, if there is one.
	 *
	 * \return if a controller is connected and its left trigger has a known
	 *         position, and also had a known position on the previous frame,
	 *         returns a relative value that represents the motion from the
	 *         previous position to the current position, in the range
	 *         [-32767, 32767]. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getRelativeState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a key, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentControllerLeftTriggerPosition()
	 * \sa getPreviousControllerLeftTriggerPosition()
	 * \sa controllerLeftTriggerJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int32_t> getRelativeControllerLeftTriggerMotion() const noexcept {
		if (currentState.controllerLeftTriggerPosition && previousState.controllerLeftTriggerPosition) {
			return static_cast<int32_t>(*currentState.controllerLeftTriggerPosition) - static_cast<int32_t>(*previousState.controllerLeftTriggerPosition);
		}
		return {};
	}

	/**
	 * Check if the controller left trigger just moved on the current frame.
	 *
	 * \return true if any left trigger motion was processed in the current
	 *         frame, false otherwise.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the isPressed(), justPressed() and justReleased() functions
	 *       with an abstract output number or action enum whenever possible,
	 *       since this can allow the user to bind a different form of input,
	 *       such as a key, to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentControllerLeftTriggerPosition()
	 * \sa getPreviousControllerLeftTriggerPosition()
	 * \sa getRelativeControllerLeftTriggerMotion()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool controllerLeftTriggerJustMoved() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_LEFT_TRIGGER)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_LEFT_TRIGGER)];
	}

	/**
	 * Get the latest known position of the right trigger of the connected
	 * controller, if there is one.
	 *
	 * \return if a controller is connected and its right trigger has a known
	 *         position, returns a value that represents it, in the range
	 *         [0, 32767]. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getCurrentState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a key, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getPreviousControllerRightTriggerPosition()
	 * \sa getRelativeControllerRightTriggerMotion()
	 * \sa controllerRightTriggerJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int16_t> getCurrentControllerRightTriggerPosition() const noexcept {
		return currentState.controllerRightTriggerPosition;
	}

	/**
	 * Get the second last known position of the right trigger of the connected
	 * controller, if there is one.
	 *
	 * \return if a controller is connected and its right trigger had a known
	 *         position on the previous frame, returns a value that represents
	 *         it, in the range [0, 32767]. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getPreviousState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a key, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentControllerRightTriggerPosition()
	 * \sa getRelativeControllerRightTriggerMotion()
	 * \sa controllerRightTriggerJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int16_t> getPreviousControllerRightTriggerPosition() const noexcept {
		return previousState.controllerRightTriggerPosition;
	}

	/**
	 * Get the relative motion between the last two known positions of the right
	 * trigger of the connected controller, if there is one.
	 *
	 * \return if a controller is connected and its right trigger has a known
	 *         position, and also had a known position on the previous frame,
	 *         returns a relative value that represents the motion from the
	 *         previous position to the current position, in the range
	 *         [-32767, 32767]. Otherwise, returns an empty optional.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the getRelativeState() function with an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different form of input, such as a key, to the control
	 *       instead, according to their preferences.
	 *
	 * \sa getCurrentControllerRightTriggerPosition()
	 * \sa getPreviousControllerRightTriggerPosition()
	 * \sa controllerRightTriggerJustMoved()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<int32_t> getRelativeControllerRightTriggerMotion() const noexcept {
		if (currentState.controllerRightTriggerPosition && previousState.controllerRightTriggerPosition) {
			return static_cast<int32_t>(*currentState.controllerRightTriggerPosition) - static_cast<int32_t>(*previousState.controllerRightTriggerPosition);
		}
		return {};
	}

	/**
	 * Check if the controller right trigger just moved on the current frame.
	 *
	 * \return true if any right trigger motion was processed in the current
	 *         frame, false otherwise.
	 *
	 * \note Instead of reading the state of the controller directly, prefer to
	 *       use the isPressed(), justPressed() and justReleased() functions
	 *       with an abstract output number or action enum whenever possible,
	 *       since this can allow the user to bind a different form of input,
	 *       such as a key, to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentControllerRightTriggerPosition()
	 * \sa getPreviousControllerRightTriggerPosition()
	 * \sa getRelativeControllerRightTriggerMotion()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool controllerRightTriggerJustMoved() const noexcept {
		return relativeState.transientInputPresses[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_TRIGGER)] || //
		       relativeState.transientInputReleases[getInputIndex(Input::CONTROLLER_AXIS_RIGHT_TRIGGER)];
	}

	/**
	 * Check if a specific output is currently pressed, i.e. if its
	 * contributions from all of its bound inputs and external sources sum up to
	 * a value greater than 0.
	 *
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to check the associated state of.
	 *
	 * \return true if the output is active, false otherwise.
	 *
	 * \sa wasPreviouslyPressed()
	 * \sa justPressed()
	 * \sa justReleased()
	 * \sa isPressed(Input) const
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isPressed(OutputIndex outputIndex) const noexcept {
		return getCurrentState(outputIndex).activePresses > 0;
	}

	/**
	 * Check if a specific output was pressed on the previous frame, i.e. if its
	 * contributions from all of its bound inputs and external sources summed up
	 * to a value greater than 0.
	 *
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to check the associated state of.
	 *
	 * \return true if the output was active on the previous frame, false
	 *         otherwise.
	 *
	 * \sa justPressed()
	 * \sa justReleased()
	 * \sa isPressed(Input) const
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool wasPreviouslyPressed(OutputIndex outputIndex) const noexcept {
		return getPreviousState(outputIndex).activePresses > 0;
	}

	/**
	 * Check if a specific output had a press triggered on the current frame.
	 *
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to check the associated state of.
	 *
	 * \return true if a press of the output was triggered on the current frame,
	 *         false otherwise.
	 *
	 * \sa isPressed()
	 * \sa justReleased()
	 * \sa justPressed(Input) const
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool justPressed(OutputIndex outputIndex) const noexcept {
		return outputIndex < OUTPUT_COUNT && relativeState.transientOutputPresses[outputIndex];
	}

	/**
	 * Check if a specific output had a release triggered on the current frame.
	 *
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to check the associated state of.
	 *
	 * \return true if a release of the output was triggered on the current
	 *         frame, false otherwise.
	 *
	 * \sa isPressed()
	 * \sa justPressed()
	 * \sa justReleased(Input) const
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool justReleased(OutputIndex outputIndex) const noexcept {
		return outputIndex < OUTPUT_COUNT && relativeState.transientOutputReleases[outputIndex];
	}

	/**
	 * Get the current total absolute state of a specific output, which consists
	 * of the accumulated contributions from all of its bound inputs.
	 *
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to get the associated state of.
	 *
	 * \return the accumulated absolute state of the given output.
	 *
	 * \sa getPreviousState()
	 * \sa getRelativeState()
	 */
	[[nodiscard]] ControlState getCurrentState(OutputIndex outputIndex) const noexcept {
		if (outputIndex >= OUTPUT_COUNT) {
			[[unlikely]];
			return {.activePresses = 0, .value = 0.0f};
		}

		ControlState result = currentState.outputExternalStates[outputIndex];
		if (const auto it = boundInputs.find(outputIndex); it != boundInputs.end()) {
			for (const Input input : it->second) {
				const ControlState inputState = currentState.inputStates[getInputIndex(input)];
				result.activePresses += inputState.activePresses;
				result.value += inputState.value;
			}
		}
		return result;
	}

	/**
	 * Get the previous total absolute value of a specific output, which
	 * consists of the accumulated contributions from all of its bound inputs.
	 *
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to get the associated state of.
	 *
	 * \return the accumulated absolute state of the given output from the
	 *         previous frame.
	 *
	 * \sa getCurrentState()
	 * \sa getRelativeState()
	 */
	[[nodiscard]] ControlState getPreviousState(OutputIndex outputIndex) const noexcept {
		if (outputIndex >= OUTPUT_COUNT) {
			[[unlikely]];
			return {.activePresses = 0, .value = 0.0f};
		}

		ControlState result = previousState.outputExternalStates[outputIndex];
		if (const auto it = boundInputs.find(outputIndex); it != boundInputs.end()) {
			for (const Input input : it->second) {
				const ControlState inputState = currentState.inputStates[getInputIndex(input)];
				result.activePresses += inputState.activePresses;
				result.value += inputState.value;
			}
		}
		return result;
	}

	/**
	 * Get the total relative delta of a specific output, which consists of the
	 * accumulated contributions from all of its bound inputs.
	 *
	 * \param outputIndex valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to get the associated state of.
	 *
	 * \return the accumulated relative delta of the given output since the
	 *         previous frame.
	 *
	 * \sa getCurrentState()
	 * \sa getPreviousState()
	 */
	[[nodiscard]] ControlDelta getRelativeState(OutputIndex outputIndex) const noexcept {
		if (outputIndex >= OUTPUT_COUNT) {
			[[unlikely]];
			return {.addedPresses = 0, .motion = 0.0f};
		}

		ControlDelta result = relativeState.outputExternalDeltas[outputIndex];
		if (const auto it = boundInputs.find(outputIndex); it != boundInputs.end()) {
			for (const Input input : it->second) {
				const ControlDelta inputDelta = relativeState.inputDeltas[getInputIndex(input)];
				result.addedPresses += inputDelta.addedPresses;
				result.motion += inputDelta.motion;
			}
		}
		return result;
	}

	/**
	 * Get the current combined absolute state of two specific outputs along one
	 * axis, which consists of the accumulated contributions from all of their
	 * bound inputs.
	 *
	 * \param outputNegative valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the resulting value.
	 * \param outputPositive valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the resulting value.
	 *
	 * \return the accumulated absolute state of the given outputs.
	 *
	 * \sa getPreviousState1D()
	 * \sa getRelativeState1D()
	 */
	[[nodiscard]] ControlState getCurrentState1D(OutputIndex outputNegative, OutputIndex outputPositive) const noexcept {
		const ControlState stateNegative = getCurrentState(outputNegative);
		const ControlState statePositive = getCurrentState(outputPositive);
		return {
			.activePresses = max(statePositive.activePresses, int32_t{0}) - max(stateNegative.activePresses, int32_t{0}),
			.value = max(statePositive.value, 0.0f) - max(stateNegative.value, 0.0f),
		};
	}

	/**
	 * Get the previous combined absolute state of two specific outputs along
	 * one axis, which consists of the accumulated contributions from all of
	 * their bound inputs.
	 *
	 * \param outputNegative valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the resulting value.
	 * \param outputPositive valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the resulting value.
	 *
	 * \return the accumulated absolute state of the given outputs from the
	 *         previous frame.
	 *
	 * \sa getCurrentState1D()
	 * \sa getRelativeState1D()
	 */
	[[nodiscard]] ControlState getPreviousState1D(OutputIndex outputNegative, OutputIndex outputPositive) const noexcept {
		const ControlState stateNegative = getPreviousState(outputNegative);
		const ControlState statePositive = getPreviousState(outputPositive);
		return {
			.activePresses = max(statePositive.activePresses, int32_t{0}) - max(stateNegative.activePresses, int32_t{0}),
			.value = max(statePositive.value, 0.0f) - max(stateNegative.value, 0.0f),
		};
	}

	/**
	 * Get the combined relative delta of two specific outputs along one axis,
	 * which consists of the accumulated contributions from all of their bound
	 * inputs.
	 *
	 * \param outputNegative valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the resulting value.
	 * \param outputPositive valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the resulting value.
	 *
	 * \return the accumulated relative delta of the given outputs since the
	 *         previous frame.
	 *
	 * \sa getCurrentState1D()
	 * \sa getPreviousState1D()
	 */
	[[nodiscard]] ControlDelta getRelativeState1D(OutputIndex outputNegative, OutputIndex outputPositive) const noexcept {
		const ControlDelta deltaNegative = getRelativeState(outputNegative);
		const ControlDelta deltaPositive = getRelativeState(outputPositive);
		return {
			.addedPresses = max(deltaPositive.addedPresses, int32_t{0}) - max(deltaNegative.addedPresses, int32_t{0}),
			.motion = max(deltaPositive.motion, 0.0f) - max(deltaNegative.motion, 0.0f),
		};
	}

	/**
	 * Get the current combined absolute state of four specific outputs along
	 * two orthogonal axes, which consists of the accumulated contributions from
	 * all of their bound inputs.
	 *
	 * \param outputNegativeX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the x component of the resulting vector.
	 * \param outputPositiveX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the x component of the resulting vector.
	 * \param outputNegativeY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the y component of the resulting vector.
	 * \param outputPositiveY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the y component of the resulting vector.
	 *
	 * \return the accumulated absolute state of the given outputs.
	 *
	 * \remark This function is useful for controlling 2D movement based on four
	 *         directional inputs such as the arrow keys, a D-pad or a joystick.
	 *         When used for this purpose, it might be necessary to clamp the
	 *         length of the vector to a length of 1 before using it, to make
	 *         sure that the user cannot achieve a higher speed than intended by
	 *         binding multiple inputs to one direction and pressing them at the
	 *         same time such that they increase the accumulated value above 1.
	 *
	 * \sa getPreviousState2D()
	 * \sa getRelativeState2D()
	 */
	[[nodiscard]] ControlState2D getCurrentState2D(OutputIndex outputNegativeX, OutputIndex outputPositiveX, OutputIndex outputNegativeY,
		OutputIndex outputPositiveY) const noexcept {
		const ControlState stateX = getCurrentState1D(outputNegativeX, outputPositiveX);
		const ControlState stateY = getCurrentState1D(outputNegativeY, outputPositiveY);
		return {
			.activePresses{stateX.activePresses, stateY.activePresses},
			.value{stateX.value, stateY.value},
		};
	}

	/**
	 * Get the previous combined absolute state of four specific outputs along
	 * two orthogonal axes, which consists of the accumulated contributions from
	 * all of their bound inputs.
	 *
	 * \param outputNegativeX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the x component of the resulting vector.
	 * \param outputPositiveX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the x component of the resulting vector.
	 * \param outputNegativeY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the y component of the resulting vector.
	 * \param outputPositiveY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the y component of the resulting vector.
	 *
	 * \return the accumulated absolute state of the given outputs from the
	 *         previous frame.
	 *
	 * \sa getCurrentState2D()
	 * \sa getRelativeState2D()
	 */
	[[nodiscard]] ControlState2D getPreviousState2D(OutputIndex outputNegativeX, OutputIndex outputPositiveX, OutputIndex outputNegativeY,
		OutputIndex outputPositiveY) const noexcept {
		const ControlState stateX = getPreviousState1D(outputNegativeX, outputPositiveX);
		const ControlState stateY = getPreviousState1D(outputNegativeY, outputPositiveY);
		return {
			.activePresses{stateX.activePresses, stateY.activePresses},
			.value{stateX.value, stateY.value},
		};
	}

	/**
	 * Get the combined relative delta of four specific outputs along two
	 * orthogonal axes, which consists of the accumulated contributions from all
	 * of their bound inputs.
	 *
	 * \param outputNegativeX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the x component of the resulting vector.
	 * \param outputPositiveX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the x component of the resulting vector.
	 * \param outputNegativeY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the y component of the resulting vector.
	 * \param outputPositiveY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the y component of the resulting vector.
	 *
	 * \return the accumulated relative delta of the given outputs since the
	 *         previous frame.
	 *
	 * \sa getCurrentState2D()
	 * \sa getPreviousState2D()
	 */
	[[nodiscard]] ControlDelta2D getRelativeState2D(OutputIndex outputNegativeX, OutputIndex outputPositiveX, OutputIndex outputNegativeY,
		OutputIndex outputPositiveY) const noexcept {
		const ControlDelta deltaX = getRelativeState1D(outputNegativeX, outputPositiveX);
		const ControlDelta deltaY = getRelativeState1D(outputNegativeY, outputPositiveY);
		return {
			.addedPresses{deltaX.addedPresses, deltaY.addedPresses},
			.motion{deltaX.motion, deltaY.motion},
		};
	}

	/**
	 * Get the current combined absolute state of six specific outputs along
	 * three orthogonal axes, which consists of the accumulated contributions
	 * from all of their bound inputs.
	 *
	 * \param outputNegativeX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the x component of the resulting vector.
	 * \param outputPositiveX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the x component of the resulting vector.
	 * \param outputNegativeY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the y component of the resulting vector.
	 * \param outputPositiveY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the y component of the resulting vector.
	 * \param outputNegativeZ valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the z component of the resulting vector.
	 * \param outputPositiveZ valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the z component of the resulting vector.
	 *
	 * \return the accumulated absolute state of the given outputs.
	 *
	 * \remark This function is useful for controlling 3D translation based on
	 *         six directional inputs such as the arrow keys combined with two
	 *         extra keys for vertical motion. When used for this purpose, it
	 *         might be necessary to clamp the length of the vector to a length
	 *         of 1 before using it, to make sure that the user cannot achieve a
	 *         higher speed than intended by binding multiple inputs to one
	 *         direction and pressing them at the same time such that they
	 *         increase the accumulated value above 1.
	 *
	 * \sa getPreviousState3D()
	 * \sa getRelativeState3D()
	 */
	[[nodiscard]] ControlState3D getCurrentState3D(OutputIndex outputNegativeX, OutputIndex outputPositiveX, OutputIndex outputNegativeY, OutputIndex outputPositiveY,
		OutputIndex outputNegativeZ, OutputIndex outputPositiveZ) const noexcept {
		const ControlState stateX = getCurrentState1D(outputNegativeX, outputPositiveX);
		const ControlState stateY = getCurrentState1D(outputNegativeY, outputPositiveY);
		const ControlState stateZ = getCurrentState1D(outputNegativeZ, outputPositiveZ);
		return {
			.activePresses{stateX.activePresses, stateY.activePresses, stateZ.activePresses},
			.value{stateX.value, stateY.value, stateZ.value},
		};
	}

	/**
	 * Get the previous combined absolute state of six specific outputs along
	 * three orthogonal axes, which consists of the accumulated contributions
	 * from all of their bound inputs.
	 *
	 * \param outputNegativeX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the x component of the resulting vector.
	 * \param outputPositiveX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the x component of the resulting vector.
	 * \param outputNegativeY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the y component of the resulting vector.
	 * \param outputPositiveY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the y component of the resulting vector.
	 * \param outputNegativeZ valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the z component of the resulting vector.
	 * \param outputPositiveZ valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the z component of the resulting vector.
	 *
	 * \return the accumulated absolute state of the given outputs from the
	 *         previous frame.
	 *
	 * \sa getCurrentState3D()
	 * \sa getRelativeState3D()
	 */
	[[nodiscard]] ControlState3D getPreviousState3D(OutputIndex outputNegativeX, OutputIndex outputPositiveX, OutputIndex outputNegativeY, OutputIndex outputPositiveY,
		OutputIndex outputNegativeZ, OutputIndex outputPositiveZ) const noexcept {
		const ControlState stateX = getPreviousState1D(outputNegativeX, outputPositiveX);
		const ControlState stateY = getPreviousState1D(outputNegativeY, outputPositiveY);
		const ControlState stateZ = getPreviousState1D(outputNegativeZ, outputPositiveZ);
		return {
			.activePresses{stateX.activePresses, stateY.activePresses, stateZ.activePresses},
			.value{stateX.value, stateY.value, stateZ.value},
		};
	}

	/**
	 * Get the combined relative delta of six specific outputs along three
	 * orthogonal axes, which consists of the accumulated contributions from all
	 * of their bound inputs.
	 *
	 * \param outputNegativeX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the x component of the resulting vector.
	 * \param outputPositiveX valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the x component of the resulting vector.
	 * \param outputNegativeY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the y component of the resulting vector.
	 * \param outputPositiveY valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the y component of the resulting vector.
	 * \param outputNegativeZ valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the negative contribution to
	 *        the z component of the resulting vector.
	 * \param outputPositiveZ valid output number between 0 (inclusive) and
	 *        #OUTPUT_COUNT (exclusive) to use for the positive contribution to
	 *        the z component of the resulting vector.
	 *
	 * \return the accumulated relative delta of the given outputs since the
	 *         previous frame.
	 *
	 * \sa getCurrentState3D()
	 * \sa getPreviousState3D()
	 */
	[[nodiscard]] ControlDelta3D getRelativeState3D(OutputIndex outputNegativeX, OutputIndex outputPositiveX, OutputIndex outputNegativeY, OutputIndex outputPositiveY,
		OutputIndex outputNegativeZ, OutputIndex outputPositiveZ) const noexcept {
		const ControlDelta deltaX = getRelativeState1D(outputNegativeX, outputPositiveX);
		const ControlDelta deltaY = getRelativeState1D(outputNegativeY, outputPositiveY);
		const ControlDelta deltaZ = getRelativeState1D(outputNegativeZ, outputPositiveZ);
		return {
			.addedPresses{deltaX.addedPresses, deltaY.addedPresses, deltaZ.addedPresses},
			.motion{deltaX.motion, deltaY.motion, deltaZ.motion},
		};
	}

	/**
	 * Check if a specific input is currently in a pressed state.
	 *
	 * \param input input to check the associated state of.
	 *
	 * \return true if the input is pressed, false otherwise.
	 *
	 * \note Instead of checking the state of a physical input, prefer to use
	 *       the version of this function that takes an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different input to the control instead, according to their
	 *       preferences.
	 *
	 * \sa isPressed(OutputIndex) const
	 * \sa justPressed()
	 * \sa justReleased()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool isPressed(Input input) const noexcept {
		return getCurrentState(input).activePresses > 0;
	}

	/**
	 * Check if a specific input was in a pressed state on the previous frame.
	 *
	 * \param input input to check the associated state of.
	 *
	 * \return true if the input was in a pressed state on the previous frame,
	 *         false otherwise.
	 *
	 * \note Instead of checking the state of a physical input, prefer to use
	 *       the version of this function that takes an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different input to the control instead, according to their
	 *       preferences.
	 *
	 * \sa isPressed(OutputIndex) const
	 * \sa justPressed()
	 * \sa justReleased()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool wasPreviouslyPressed(Input input) const noexcept {
		return getPreviousState(input).activePresses > 0;
	}

	/**
	 * Check if a specific input had a press triggered on the current frame.
	 *
	 * \param input input to check the associated state of.
	 *
	 * \return true if a press of the input was triggered on the current frame,
	 *         false otherwise.
	 *
	 * \note Instead of checking the state of a physical input, prefer to use
	 *       the version of this function that takes an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different input to the control instead, according to their
	 *       preferences.
	 *
	 * \sa justPressed(OutputIndex) const
	 * \sa isPressed()
	 * \sa justReleased()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool justPressed(Input input) const noexcept {
		const size_t inputIndex = getInputIndex(input);
		return inputIndex < INPUT_COUNT && relativeState.transientInputPresses[inputIndex];
	}

	/**
	 * Check if a specific input had a release triggered on the current frame.
	 *
	 * \param input input to check the associated state of.
	 *
	 * \return true if a release of the input was triggered on the current
	 *         frame, false otherwise.
	 *
	 * \note Instead of checking the state of a physical input, prefer to use
	 *       the version of this function that takes an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different input to the control instead, according to their
	 *       preferences.
	 *
	 * \sa justReleased(OutputIndex) const
	 * \sa isPressed()
	 * \sa justPressed()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE bool justReleased(Input input) const noexcept {
		const size_t inputIndex = getInputIndex(input);
		return inputIndex < INPUT_COUNT && relativeState.transientInputReleases[inputIndex];
	}

	/**
	 * Get the current absolute value of a specific input.
	 *
	 * \param input input to get the associated value of.
	 *
	 * \return the absolute state of the given input.
	 *
	 * \note Instead of checking the state of a physical input, prefer to use
	 *       the version of this function that takes an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different input to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getPreviousState()
	 * \sa getRelativeState()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState getCurrentState(Input input) const noexcept {
		const size_t inputIndex = getInputIndex(input);
		return (inputIndex < INPUT_COUNT) ? currentState.inputStates[inputIndex] : ControlState{.activePresses = 0, .value = 0.0f};
	}

	/**
	 * Get the previous absolute value of a specific input.
	 *
	 * \param input input to get the associated value of.
	 *
	 * \return the absolute state of the given input from the previous frame.
	 *
	 * \note Instead of checking the state of a physical input, prefer to use
	 *       the version of this function that takes an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different input to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentState()
	 * \sa getRelativeState()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState getPreviousState(Input input) const noexcept {
		const size_t inputIndex = getInputIndex(input);
		return (inputIndex < INPUT_COUNT) ? previousState.inputStates[inputIndex] : ControlState{.activePresses = 0, .value = 0.0f};
	}

	/**
	 * Get the relative delta of a specific input.
	 *
	 * \param input input to get the associated value of.
	 *
	 * \return the relative delta of the given input since the previous frame.
	 *
	 * \note Instead of checking the state of a physical input, prefer to use
	 *       the version of this function that takes an abstract output number
	 *       or action enum whenever possible, since this can allow the user to
	 *       bind a different input to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentState()
	 * \sa getPreviousState()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ControlDelta getRelativeState(Input input) const noexcept {
		const size_t inputIndex = getInputIndex(input);
		return (inputIndex < INPUT_COUNT) ? relativeState.inputDeltas[inputIndex] : ControlDelta{.addedPresses = 0, .motion = 0.0f};
	}

	/**
	 * Get the current combined absolute state of two specific inputs along one
	 * axis.
	 *
	 * \param inputNegative input to use for the negative contribution to the
	 *        resulting value.
	 * \param inputPositive input to use for the positive contribution to the
	 *        resulting value.
	 *
	 * \return the absolute state of the given inputs.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getPreviousState1D()
	 * \sa getRelativeState1D()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState getCurrentState1D(Input inputNegative, Input inputPositive) const noexcept {
		const ControlState stateNegative = getCurrentState(inputNegative);
		const ControlState statePositive = getCurrentState(inputPositive);
		return {
			.activePresses = max(statePositive.activePresses, int32_t{0}) - max(stateNegative.activePresses, int32_t{0}),
			.value = max(statePositive.value, 0.0f) - max(stateNegative.value, 0.0f),
		};
	}

	/**
	 * Get the previous combined absolute state of two specific inputs along one
	 * axis.
	 *
	 * \param inputNegative input to use for the negative contribution to the
	 *        resulting value.
	 * \param inputPositive input to use for the positive contribution to the
	 *        resulting value.
	 *
	 * \return the absolute state of the given inputs from the previous frame.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentState1D()
	 * \sa getRelativeState1D()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState getPreviousState1D(Input inputNegative, Input inputPositive) const noexcept {
		const ControlState stateNegative = getPreviousState(inputNegative);
		const ControlState statePositive = getPreviousState(inputPositive);
		return {
			.activePresses = max(statePositive.activePresses, int32_t{0}) - max(stateNegative.activePresses, int32_t{0}),
			.value = max(statePositive.value, 0.0f) - max(stateNegative.value, 0.0f),
		};
	}

	/**
	 * Get the combined relative delta of two specific inputs along one axis.
	 *
	 * \param inputNegative input to use for the negative contribution to the
	 *        resulting value.
	 * \param inputPositive input to use for the positive contribution to the
	 *        resulting value.
	 *
	 * \return the relative delta of the given inputs since the previous frame.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentState1D()
	 * \sa getPreviousState1D()
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE ControlDelta getRelativeState1D(Input inputNegative, Input inputPositive) const noexcept {
		const ControlDelta deltaNegative = getRelativeState(inputNegative);
		const ControlDelta deltaPositive = getRelativeState(inputPositive);
		return {
			.addedPresses = max(deltaPositive.addedPresses, int32_t{0}) - max(deltaNegative.addedPresses, int32_t{0}),
			.motion = max(deltaPositive.motion, 0.0f) - max(deltaNegative.motion, 0.0f),
		};
	}

	/**
	 * Get the current combined absolute state of four specific inputs along two
	 * orthogonal axes.
	 *
	 * \param inputNegativeX input to use for the negative contribution to the x
	 *        component of the resulting vector.
	 * \param inputPositiveX input to use for the positive contribution to the x
	 *        component of the resulting vector.
	 * \param inputNegativeY input to use for the negative contribution to the y
	 *        component of the resulting vector.
	 * \param inputPositiveY input to use for the positive contribution to the y
	 *        component of the resulting vector.
	 *
	 * \return the absolute state of the given inputs.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getPreviousState2D()
	 * \sa getRelativeState2D()
	 */
	[[nodiscard]] ControlState2D getCurrentState2D(Input inputNegativeX, Input inputPositiveX, Input inputNegativeY, Input inputPositiveY) const noexcept {
		const ControlState stateX = getCurrentState1D(inputNegativeX, inputPositiveX);
		const ControlState stateY = getCurrentState1D(inputNegativeY, inputPositiveY);
		return {
			.activePresses{stateX.activePresses, stateY.activePresses},
			.value{stateX.value, stateY.value},
		};
	}

	/**
	 * Get the previous combined absolute state of four specific inputs along
	 * two orthogonal axes.
	 *
	 * \param inputNegativeX input to use for the negative contribution to the x
	 *        component of the resulting vector.
	 * \param inputPositiveX input to use for the positive contribution to the x
	 *        component of the resulting vector.
	 * \param inputNegativeY input to use for the negative contribution to the y
	 *        component of the resulting vector.
	 * \param inputPositiveY input to use for the positive contribution to the y
	 *        component of the resulting vector.
	 *
	 * \return the absolute state of the given inputs from the previous frame.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentState2D()
	 * \sa getRelativeState2D()
	 */
	[[nodiscard]] ControlState2D getPreviousState2D(Input inputNegativeX, Input inputPositiveX, Input inputNegativeY, Input inputPositiveY) const noexcept {
		const ControlState stateX = getPreviousState1D(inputNegativeX, inputPositiveX);
		const ControlState stateY = getPreviousState1D(inputNegativeY, inputPositiveY);
		return {
			.activePresses{stateX.activePresses, stateY.activePresses},
			.value{stateX.value, stateY.value},
		};
	}

	/**
	 * Get the combined relative delta of four specific inputs along two
	 * orthogonal axes.
	 *
	 * \param inputNegativeX input to use for the negative contribution to the x
	 *        component of the resulting vector.
	 * \param inputPositiveX input to use for the positive contribution to the x
	 *        component of the resulting vector.
	 * \param inputNegativeY input to use for the negative contribution to the y
	 *        component of the resulting vector.
	 * \param inputPositiveY input to use for the positive contribution to the y
	 *        component of the resulting vector.
	 *
	 * \return the relative delta of the given inputs since the previous frame.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentState()
	 * \sa getRelativeState()
	 * \sa getCurrentValue()
	 */
	[[nodiscard]] ControlDelta2D getRelativeState2D(Input inputNegativeX, Input inputPositiveX, Input inputNegativeY, Input inputPositiveY) const noexcept {
		const ControlDelta deltaX = getRelativeState1D(inputNegativeX, inputPositiveX);
		const ControlDelta deltaY = getRelativeState1D(inputNegativeY, inputPositiveY);
		return {
			.addedPresses{deltaX.addedPresses, deltaY.addedPresses},
			.motion{deltaX.motion, deltaY.motion},
		};
	}

	/**
	 * Get the current combined scaled absolute value of six specific inputs
	 * along three orthogonal axes.
	 *
	 * \param inputNegativeX input to use for the negative contribution to the x
	 *        component of the resulting vector.
	 * \param inputPositiveX input to use for the positive contribution to the x
	 *        component of the resulting vector.
	 * \param inputNegativeY input to use for the negative contribution to the y
	 *        component of the resulting vector.
	 * \param inputPositiveY input to use for the positive contribution to the y
	 *        component of the resulting vector.
	 * \param inputNegativeZ input to use for the negative contribution to the z
	 *        component of the resulting vector.
	 * \param inputPositiveZ input to use for the positive contribution to the z
	 *        component of the resulting vector.
	 *
	 * \return the absolute state of the given inputs.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getPreviousState3D()
	 * \sa getRelativeState3D()
	 */
	[[nodiscard]] ControlState3D getCurrentState3D(Input inputNegativeX, Input inputPositiveX, Input inputNegativeY, Input inputPositiveY, Input inputNegativeZ,
		Input inputPositiveZ) const noexcept {
		const ControlState stateX = getCurrentState1D(inputNegativeX, inputPositiveX);
		const ControlState stateY = getCurrentState1D(inputNegativeY, inputPositiveY);
		const ControlState stateZ = getCurrentState1D(inputNegativeZ, inputPositiveZ);
		return {
			.activePresses{stateX.activePresses, stateY.activePresses, stateZ.activePresses},
			.value{stateX.value, stateY.value, stateZ.value},
		};
	}

	/**
	 * Get the previous combined scaled absolute value of six specific inputs
	 * along three orthogonal axes.
	 *
	 * \param inputNegativeX input to use for the negative contribution to the x
	 *        component of the resulting vector.
	 * \param inputPositiveX input to use for the positive contribution to the x
	 *        component of the resulting vector.
	 * \param inputNegativeY input to use for the negative contribution to the y
	 *        component of the resulting vector.
	 * \param inputPositiveY input to use for the positive contribution to the y
	 *        component of the resulting vector.
	 * \param inputNegativeZ input to use for the negative contribution to the z
	 *        component of the resulting vector.
	 * \param inputPositiveZ input to use for the positive contribution to the z
	 *        component of the resulting vector.
	 *
	 * \return the absolute state of the given inputs from the previous frame.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentState3D()
	 * \sa getPreviousState3D()
	 */
	[[nodiscard]] ControlState3D getPreviousState3D(Input inputNegativeX, Input inputPositiveX, Input inputNegativeY, Input inputPositiveY, Input inputNegativeZ,
		Input inputPositiveZ) const noexcept {
		const ControlState stateX = getPreviousState1D(inputNegativeX, inputPositiveX);
		const ControlState stateY = getPreviousState1D(inputNegativeY, inputPositiveY);
		const ControlState stateZ = getPreviousState1D(inputNegativeZ, inputPositiveZ);
		return {
			.activePresses{stateX.activePresses, stateY.activePresses, stateZ.activePresses},
			.value{stateX.value, stateY.value, stateZ.value},
		};
	}

	/**
	 * Get the current combined scaled relative value of six specific outputs
	 * along three orthogonal axes.
	 *
	 * \param inputNegativeX input to use for the negative contribution to the x
	 *        component of the resulting vector.
	 * \param inputPositiveX input to use for the positive contribution to the x
	 *        component of the resulting vector.
	 * \param inputNegativeY input to use for the negative contribution to the y
	 *        component of the resulting vector.
	 * \param inputPositiveY input to use for the positive contribution to the y
	 *        component of the resulting vector.
	 * \param inputNegativeZ input to use for the negative contribution to the z
	 *        component of the resulting vector.
	 * \param inputPositiveZ input to use for the positive contribution to the z
	 *        component of the resulting vector.
	 *
	 * \return the relative delta of the given inputs since the previous frame.
	 *
	 * \note Instead of checking the state of physical inputs, prefer to use the
	 *       version of this function that takes abstract output numbers or
	 *       action enums whenever possible, since this can allow the user to
	 *       bind different inputs to the control instead, according to their
	 *       preferences.
	 *
	 * \sa getCurrentState3D()
	 * \sa getPreviousState3D()
	 */
	[[nodiscard]] ControlDelta3D getRelativeState3D(Input inputNegativeX, Input inputPositiveX, Input inputNegativeY, Input inputPositiveY, Input inputNegativeZ,
		Input inputPositiveZ) const noexcept {
		const ControlDelta deltaX = getRelativeState1D(inputNegativeX, inputPositiveX);
		const ControlDelta deltaY = getRelativeState1D(inputNegativeY, inputPositiveY);
		const ControlDelta deltaZ = getRelativeState1D(inputNegativeZ, inputPositiveZ);
		return {
			.addedPresses{deltaX.addedPresses, deltaY.addedPresses, deltaZ.addedPresses},
			.motion{deltaX.motion, deltaY.motion, deltaZ.motion},
		};
	}

	/**
	 * Like bind(Input, ArrayList<OutputIndex>), but accepts a pack of "actions" of any enum
	 * type, which are interpreted as corresponding to the output numbers equal
	 * to their underlying values.
	 *
	 * \sa bind(Input, ArrayList<OutputIndex>)
	 */
	template <enumeration... Actions>
	GREM_ALWAYS_INLINE void bind(Input input, Actions... actions) requires((!same_as<Actions, Input> && !same_as<Actions, OutputIndex>) && ...) {
		bind(input, {getOutputIndex(actions)...});
	}

	/**
	 * Like addBinding(Input, OutputIndex), but accepts a pack of "actions" of any
	 * enum type, which are interpreted as corresponding to the output numbers
	 * equal to their underlying values.
	 *
	 * \sa addBinding(Input, OutputIndex)
	 */
	template <enumeration Action>
	GREM_ALWAYS_INLINE void addBinding(Input input, Action action) requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		addBinding(input, getOutputIndex(action));
	}

	/**
	 * Like addBindings(Input, Span<const OutputIndex>), but accepts a pack of "actions" of any
	 * enum type, which are interpreted as corresponding to the output numbers
	 * equal to their underlying values.
	 *
	 * \sa addBindings(Input, Span<const OutputIndex>)
	 */
	template <enumeration... Actions>
	GREM_ALWAYS_INLINE void addBindings(Input input, Actions... actions) requires((!same_as<Actions, Input> && !same_as<Actions, OutputIndex>) && ...) {
		addBindings(input, {getOutputIndex(actions)...});
	}

	/**
	 * Like isPressed(OutputIndex) const, but accepts an "action" of any enum
	 * type, which is interpreted as corresponding to the output number equal to
	 * its underlying value.
	 *
	 * \sa isPressed(OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE bool isPressed(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return isPressed(getOutputIndex(action));
	}

	/**
	 * Like wasPreviouslyPressed(OutputIndex) const, but accepts an "action" of
	 * any enum type, which is interpreted as corresponding to the output number
	 * equal to its underlying value.
	 *
	 * \sa wasPreviouslyPressed(OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE bool wasPreviouslyPressed(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return wasPreviouslyPressed(getOutputIndex(action));
	}

	/**
	 * Like justPressed(OutputIndex) const, but accepts an "action" of any enum
	 * type, which is interpreted as corresponding to the output number equal to
	 * its underlying value.
	 *
	 * \sa justPressed(OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE bool justPressed(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return justPressed(getOutputIndex(action));
	}

	/**
	 * Like justReleased(OutputIndex) const, but accepts an "action" of any enum
	 * type, which is interpreted as corresponding to the output number equal to
	 * its underlying value.
	 *
	 * \sa justReleased(OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE bool justReleased(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return justReleased(getOutputIndex(action));
	}

	/**
	 * Like getCurrentState(OutputIndex) const, but accepts an "action" of any
	 * enum type, which is interpreted as corresponding to the output number
	 * equal to its underlying value.
	 *
	 * \sa getCurrentState(OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState getCurrentState(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getCurrentState(getOutputIndex(action));
	}

	/**
	 * Like getPreviousState(OutputIndex) const, but accepts an "action" of any
	 * enum type, which is interpreted as corresponding to the output number
	 * equal to its underlying value.
	 *
	 * \sa getPreviousState(OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState getPreviousState(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getPreviousState(getOutputIndex(action));
	}

	/**
	 * Like getRelativeState(OutputIndex) const, but accepts an "action" of
	 * any enum type, which is interpreted as corresponding to the output number
	 * equal to its underlying value.
	 *
	 * \sa getRelativeState(OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlDelta getRelativeState(Action action) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getRelativeState(getOutputIndex(action));
	}

	/**
	 * Like getCurrentState1D(OutputIndex, OutputIndex) const, but accepts
	 * "actions" of any enum type, which are interpreted as corresponding to
	 * output numbers equal to their underlying values.
	 *
	 * \sa getCurrentState1D(OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState getCurrentState1D(Action actionNegative, Action actionPositive) const noexcept
		requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getCurrentState1D(           //
			getOutputIndex(actionNegative), //
			getOutputIndex(actionPositive));
	}

	/**
	 * Like getPreviousState1D(OutputIndex, OutputIndex) const, but accepts
	 * "actions" of any enum type, which are interpreted as corresponding to
	 * output numbers equal to their underlying values.
	 *
	 * \sa getPreviousState1D(OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState getPreviousState1D(Action actionNegative, Action actionPositive) const noexcept
		requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getPreviousState1D(          //
			getOutputIndex(actionNegative), //
			getOutputIndex(actionPositive));
	}

	/**
	 * Like getRelativeState1D(OutputIndex, OutputIndex) const, but accepts
	 * "actions" of any enum type, which are interpreted as corresponding to
	 * output numbers equal to their underlying values.
	 *
	 * \sa getRelativeState1D(OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlDelta getRelativeState1D(Action actionNegative, Action actionPositive) const noexcept
		requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getRelativeState1D(          //
			getOutputIndex(actionNegative), //
			getOutputIndex(actionPositive));
	}

	/**
	 * Like getCurrentState2D(OutputIndex, OutputIndex, OutputIndex, OutputIndex) const,
	 * but accepts "actions" of any enum type, which are interpreted as
	 * corresponding to the output numbers equal to their underlying values.
	 *
	 * \sa getCurrentState2D(OutputIndex, OutputIndex, OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState2D getCurrentState2D(Action actionNegativeX, Action actionPositiveX, Action actionNegativeY, Action actionPositiveY) const noexcept
		requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getCurrentState2D(            //
			getOutputIndex(actionNegativeX), //
			getOutputIndex(actionPositiveX), //
			getOutputIndex(actionNegativeY), //
			getOutputIndex(actionPositiveY));
	}

	/**
	 * Like getPreviousState2D(OutputIndex, OutputIndex, OutputIndex, OutputIndex) const,
	 * but accepts "actions" of any enum type, which are interpreted as
	 * corresponding to the output numbers equal to their underlying values.
	 *
	 * \sa getPreviousState2D(OutputIndex, OutputIndex, OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState2D getPreviousState2D(Action actionNegativeX, Action actionPositiveX, Action actionNegativeY,
		Action actionPositiveY) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getPreviousState2D(           //
			getOutputIndex(actionNegativeX), //
			getOutputIndex(actionPositiveX), //
			getOutputIndex(actionNegativeY), //
			getOutputIndex(actionPositiveY));
	}

	/**
	 * Like getRelativeState2D(OutputIndex, OutputIndex, OutputIndex, OutputIndex) const,
	 * but accepts "actions" of any enum type, which are interpreted as
	 * corresponding to the output numbers equal to their underlying values.
	 *
	 * \sa getRelativeState2D(OutputIndex, OutputIndex, OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlDelta2D getRelativeState2D(Action actionNegativeX, Action actionPositiveX, Action actionNegativeY,
		Action actionPositiveY) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getRelativeState2D(           //
			getOutputIndex(actionNegativeX), //
			getOutputIndex(actionPositiveX), //
			getOutputIndex(actionNegativeY), //
			getOutputIndex(actionPositiveY));
	}

	/**
	 * Like getCurrentState3D(OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex) const,
	 * but accepts "actions" of any enum type, which are interpreted as
	 * corresponding to the output numbers equal to their underlying values.
	 *
	 * \sa getCurrentState3D(OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState3D getCurrentState3D(Action actionNegativeX, Action actionPositiveX, Action actionNegativeY, Action actionPositiveY,
		Action actionNegativeZ, Action actionPositiveZ) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getCurrentState3D(            //
			getOutputIndex(actionNegativeX), //
			getOutputIndex(actionPositiveX), //
			getOutputIndex(actionNegativeY), //
			getOutputIndex(actionPositiveY), //
			getOutputIndex(actionNegativeZ), //
			getOutputIndex(actionPositiveZ));
	}

	/**
	 * Like getPreviousState3D(OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex) const,
	 * but accepts "actions" of any enum type, which are interpreted as
	 * corresponding to the output numbers equal to their underlying values.
	 *
	 * \sa getPreviousState3D(OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlState3D getPreviousState3D(Action actionNegativeX, Action actionPositiveX, Action actionNegativeY, Action actionPositiveY,
		Action actionNegativeZ, Action actionPositiveZ) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getPreviousState3D(           //
			getOutputIndex(actionNegativeX), //
			getOutputIndex(actionPositiveX), //
			getOutputIndex(actionNegativeY), //
			getOutputIndex(actionPositiveY), //
			getOutputIndex(actionNegativeZ), //
			getOutputIndex(actionPositiveZ));
	}

	/**
	 * Like getRelativeState3D(OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex) const,
	 * but accepts "actions" of any enum type, which are interpreted as
	 * corresponding to the output numbers equal to their underlying values.
	 *
	 * \sa getRelativeState3D(OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex, OutputIndex) const
	 */
	template <enumeration Action>
	[[nodiscard]] GREM_ALWAYS_INLINE ControlDelta3D getRelativeState3D(Action actionNegativeX, Action actionPositiveX, Action actionNegativeY, Action actionPositiveY,
		Action actionNegativeZ, Action actionPositiveZ) const noexcept requires(!same_as<Action, Input> && !same_as<Action, OutputIndex>) {
		return getRelativeState3D(           //
			getOutputIndex(actionNegativeX), //
			getOutputIndex(actionPositiveX), //
			getOutputIndex(actionNegativeY), //
			getOutputIndex(actionPositiveY), //
			getOutputIndex(actionNegativeZ), //
			getOutputIndex(actionPositiveZ));
	}

private:
	struct AbsoluteState {
		Optional<vec2> mousePosition{};
		Optional<vec2> touchPosition{};
		Optional<float> touchPressure{};
		Optional<i16vec2> controllerLeftStickPosition{};
		Optional<i16vec2> controllerRightStickPosition{};
		Optional<int16_t> controllerLeftTriggerPosition{};
		Optional<int16_t> controllerRightTriggerPosition{};
		Array<ControlState, INPUT_COUNT> inputStates{};
		Array<ControlState, OUTPUT_COUNT> outputExternalStates{};
	};

	struct RelativeState {
		vec2 transientMouseWheelScroll{};
		Array<ControlDelta, INPUT_COUNT> inputDeltas{};
		Array<ControlDelta, OUTPUT_COUNT> outputExternalDeltas{};
		BitArray<INPUT_COUNT> transientInputPresses{};
		BitArray<INPUT_COUNT> transientInputReleases{};
		BitArray<OUTPUT_COUNT> transientOutputPresses{};
		BitArray<OUTPUT_COUNT> transientOutputReleases{};
	};

	template <typename Action>
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr OutputIndex getOutputIndex(Action action) noexcept {
		return static_cast<OutputIndex>(static_cast<std::underlying_type_t<Action>>(action));
	}

	GREM_API(events)
	void setCursorPosition(TimePoint timestamp, Optional<vec2>& position, vec2 newPosition, vec2 relativeMotion, vec2 sensitivity, Input inputLeft, Input inputRight, Input inputUp,
		Input inputDown);

	void setMousePosition(TimePoint timestamp, vec2 newPosition, vec2 relativeMotion) {
		setCursorPosition(timestamp, currentState.mousePosition, newPosition, relativeMotion, options.preferences.mouseSensitivity, Input::MOUSE_MOTION_LEFT,
			Input::MOUSE_MOTION_RIGHT, Input::MOUSE_MOTION_UP, Input::MOUSE_MOTION_DOWN);
	}

	GREM_API(events) void scrollMouseWheel(TimePoint timestamp, vec2 scrollAmount);

	void setTouchPosition(TimePoint timestamp, vec2 newPosition, vec2 relativeMotion) {
		setCursorPosition(timestamp, currentState.touchPosition, newPosition, relativeMotion, options.preferences.touchMotionSensitivity, Input::TOUCH_FINGER_MOTION_LEFT,
			Input::TOUCH_FINGER_MOTION_RIGHT, Input::TOUCH_FINGER_MOTION_UP, Input::TOUCH_FINGER_MOTION_DOWN);
	}

	GREM_API(events) void setTouchPressure(TimePoint timestamp, float newPressure);

	GREM_API(events)
	void setControllerStickPosition(TimePoint timestamp, Optional<i16vec2>& position, i16vec2 newPosition, vec2 sensitivity, float curveExponent, float innerDeadzone,
		float outerDeadzone, Input inputLeft, Input inputRight, Input inputUp, Input inputDown);

	void setControllerLeftStickPosition(TimePoint timestamp, i16vec2 newPosition) {
		setControllerStickPosition(timestamp, currentState.controllerLeftStickPosition, newPosition, options.preferences.controllerLeftStickSensitivity,
			options.preferences.controllerLeftStickCurveExponent, options.preferences.controllerLeftStickInnerDeadzone, options.preferences.controllerLeftStickOuterDeadzone,
			Input::CONTROLLER_AXIS_LEFT_STICK_LEFT, Input::CONTROLLER_AXIS_LEFT_STICK_RIGHT, Input::CONTROLLER_AXIS_LEFT_STICK_UP, Input::CONTROLLER_AXIS_LEFT_STICK_DOWN);
	}

	void setControllerRightStickPosition(TimePoint timestamp, i16vec2 newPosition) {
		setControllerStickPosition(timestamp, currentState.controllerRightStickPosition, newPosition, options.preferences.controllerRightStickSensitivity,
			options.preferences.controllerRightStickCurveExponent, options.preferences.controllerRightStickInnerDeadzone, options.preferences.controllerRightStickOuterDeadzone,
			Input::CONTROLLER_AXIS_RIGHT_STICK_LEFT, Input::CONTROLLER_AXIS_RIGHT_STICK_RIGHT, Input::CONTROLLER_AXIS_RIGHT_STICK_UP, Input::CONTROLLER_AXIS_RIGHT_STICK_DOWN);
	}

	GREM_API(events)
	void setControllerTriggerPosition(TimePoint timestamp, Optional<int16_t>& position, int16_t newPosition, float lowerDeadzone, float upperDeadzone, Input inputAxis);

	void setControllerLeftTriggerPosition(TimePoint timestamp, int16_t newPosition) {
		setControllerTriggerPosition(timestamp, currentState.controllerLeftTriggerPosition, newPosition, options.preferences.controllerLeftTriggerLowerDeadzone,
			options.preferences.controllerLeftTriggerUpperDeadzone, Input::CONTROLLER_AXIS_LEFT_TRIGGER);
	}

	void setControllerRightTriggerPosition(TimePoint timestamp, int16_t newPosition) {
		setControllerTriggerPosition(timestamp, currentState.controllerRightTriggerPosition, newPosition, options.preferences.controllerRightTriggerLowerDeadzone,
			options.preferences.controllerRightTriggerUpperDeadzone, Input::CONTROLLER_AXIS_RIGHT_TRIGGER);
	}

	InputManagerPreferences initialPreferences;
	InputManagerOptions options;
	HashMap<Input, ArrayList<OutputIndex>> bindings{};
	HashMap<OutputIndex, ArrayList<Input>> boundInputs{};
	AbsoluteState currentState{};
	AbsoluteState previousState{};
	RelativeState relativeState{};
	ArrayList<OutputEvent> polledOutputEvents{};
	ArrayList<OutputEvent> pendingOutputEvents{};
};

} // namespace grem::events

#endif
