// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_CONTROL_HPP
#define GREM_CORE_CONTROL_HPP

#include <GREM/build_config.hpp>

namespace grem {

/**
 * Proportional-derivative feedback loop for adjusting a control variable
 * towards a desired setpoint based on a measured process variable in the form
 * of a discrete-time input signal.
 *
 * \tparam T signal value type. Must be a type that behaves like a number.
 */
template <typename T>
class PDController {
public:
	/**
	 * Default-construct a PD controller.
	 */
	constexpr PDController() = default;

	/**
	 * Construct a PD controller with specific initial values.
	 *
	 * \param error initial value to set the latest error value to.
	 */
	constexpr PDController(T error)
		: error(error) {}

	/**
	 * Reset the PD controller to the default state.
	 */
	constexpr void reset() {
		error = {};
	}

	/**
	 * Reset the PD controller to a specific value.
	 *
	 * \param newError new value to set the latest error value to.
	 */
	constexpr void reset(T newError) {
		error = newError;
	}

	/**
	 * Update the controller state and get the next control variable value to
     * use in order to steer the process variable towards a given setpoint based
     * on the next discrete input signal sample value.
	 *
	 * \param newSignalError error between the new sample value of the input
	 *        signal and the desired setpoint to steer towards, calculated as
	 *        `desiredSignalValue - newSignalValue`.
	 * \param proportionalGain how much the current error should contribute to
     *        the output.
	 * \param derivativeGain how much the change in error should contribute to
     *        the output. Should be inversely proportional to the sample
     *        interval duration.
	 */
	template <typename Coefficient>
	[[nodiscard]] constexpr auto update(T newSignalError, Coefficient proportionalGain, Coefficient derivativeGain) {
		const T currentError = newSignalError;
		const T changeInError = currentError - error;
		const auto result = currentError * proportionalGain + changeInError * derivativeGain;
		error = currentError;
		return result;
	}

private:
	T error{};
};

/**
 * Proportional-integral feedback loop for adjusting a control variable towards
 * a desired setpoint based on a measured process variable in the form of a
 * discrete-time input signal.
 *
 * \tparam T signal value type. Must be a type that behaves like a number.
 */
template <typename T>
class PIController {
public:
	/**
	 * Default-construct a PI controller.
	 */
	constexpr PIController() = default;

	/**
	 * Construct a PI controller with specific initial values.
	 *
	 * \param integral initial value to set the accumulated integral value to.
	 */
	constexpr PIController(T integral)
		: integral(integral) {}

	/**
	 * Reset the PI controller to the default state.
	 */
	constexpr void reset() {
		integral = {};
	}

	/**
	 * Reset the PI controller to a specific value.
	 *
	 * \param newIntegral new value to set the accumulated integral value to.
	 */
	constexpr void reset(T newIntegral) {
		integral = newIntegral;
	}

	/**
	 * Update the controller state and get the next control variable value to
     * use in order to steer the process variable towards a given setpoint based
     * on the next discrete input signal sample value.
	 *
	 * \param newSignalError error between the new sample value of the input
	 *        signal and the desired setpoint to steer towards, calculated as
	 *        `desiredSignalValue - newSignalValue`.
	 * \param proportionalGain how much the current error should contribute to
     *        the output.
	 * \param integralGain how much the accumulated error should contribute to
     *        the output. Should be proportional to the sample interval
     *        duration.
	 */
	template <typename Coefficient>
	[[nodiscard]] constexpr auto update(T newSignalError, Coefficient proportionalGain, Coefficient integralGain) {
		const T currentError = newSignalError;
		const T accumulatedError = integral + currentError;
		const auto result = currentError * proportionalGain + accumulatedError * integralGain;
		integral = accumulatedError;
		return result;
	}

private:
	T integral{};
};

/**
 * Proportional-integral-derivative feedback loop for adjusting a control
 * variable towards a desired setpoint based on a measured process variable in
 * the form of a discrete-time input signal.
 *
 * \tparam T signal value type. Must be a type that behaves like a number.
 */
template <typename T>
class PIDController {
public:
	/**
	 * Default-construct a PID controller.
	 */
	constexpr PIDController() = default;

	/**
	 * Construct a PID controller with specific initial values.
	 *
	 * \param error initial value to set the latest error value to.
	 * \param integral initial value to set the accumulated integral value to.
	 */
	constexpr PIDController(T error, T integral)
		: error(error)
		, integral(integral) {}

	/**
	 * Reset the PID controller to the default state.
	 */
	constexpr void reset() {
		error = {};
		integral = {};
	}

	/**
	 * Reset the PID controller to specific values.
	 *
	 * \param newError new value to set the latest error value to.
	 * \param newIntegral new value to set the accumulated integral value to.
	 */
	constexpr void reset(T newError, T newIntegral) {
		error = newError;
		integral = newIntegral;
	}

	/**
	 * Reset the PID controller's latest error to a specific value.
	 *
	 * \param newError new value to set the latest error value to.
	 */
	constexpr void resetError(T newError = {}) {
		error = newError;
	}

	/**
	 * Reset the PID controller's accumulated integral to a specific value.
	 *
	 * \param newIntegral new value to set the accumulated integral value to.
	 */
	constexpr void resetIntegral(T newIntegral = {}) {
		integral = newIntegral;
	}

	/**
	 * Update the controller state and get the next control variable value to
     * use in order to steer the process variable towards a given setpoint based
     * on the next discrete input signal sample value.
	 *
	 * \param newSignalError error between the new sample value of the input
	 *        signal and the desired setpoint to steer towards, calculated as
	 *        `desiredSignalValue - newSignalValue`.
	 * \param proportionalGain how much the current error should contribute to
     *        the output.
	 * \param integralGain how much the accumulated error should contribute to
     *        the output. Should be proportional to the sample interval
     *        duration.
	 * \param derivativeGain how much the change in error should contribute to
     *        the output. Should be inversely proportional to the sample
     *        interval duration.
	 */
	template <typename Coefficient>
	[[nodiscard]] constexpr auto update(T newSignalError, Coefficient proportionalGain, Coefficient integralGain, Coefficient derivativeGain) {
		const T currentError = newSignalError;
		const T accumulatedError = integral + currentError;
		const T changeInError = currentError - error;
		const auto result = currentError * proportionalGain + accumulatedError * integralGain + changeInError * derivativeGain;
		error = currentError;
		integral = accumulatedError;
		return result;
	}

private:
	T error{};
	T integral{};
};

} // namespace grem

#endif
