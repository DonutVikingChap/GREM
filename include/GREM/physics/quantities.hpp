// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_PHYSICS_QUANTITIES_HPP
#define GREM_PHYSICS_QUANTITIES_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/attributes.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/ConstantString.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/geometry.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/physics/Error.hpp>

#ifdef GREM_USE_SSE_INTRINSICS
#include <immintrin.h> // _MM_..., __m128..., _mm_...
#endif

#include <type_traits> // std::is_void_v, std::conditional_t, std::remove_..._t, std::bool_constant, std::false_type, std::true_type
#include <utility>     // std::declval

namespace grem::physics {

namespace detail {

[[nodiscard]] static consteval long double constexprSqrt(long double value, long double current, long double previous) {
	if (current == previous) {
		return current;
	}
	return constexprSqrt(value, 0.5l * (current + value / current), current);
}

[[nodiscard]] static consteval long double constexprSqrt(long double value) {
	return constexprSqrt(value, value, 0.0l);
}

} // namespace detail

/**
 * Concept that checks if a type is a valid physical dimension type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept dimension = requires {
	{ std::remove_reference_t<T>::MASS } -> same_as<const int&>;
	{ std::remove_reference_t<T>::LENGTH } -> same_as<const int&>;
	{ std::remove_reference_t<T>::TIME } -> same_as<const int&>;
} && !requires { typename std::remove_reference_t<T>::DimensionType; };

/**
 * Physical dimension type template.
 *
 * Part of the definition of a physical unit type.
 *
 * \tparam Mass mass exponent.
 * \tparam Length length exponent.
 * \tparam Time time exponent.
 */
template <int Mass, int Length, int Time>
struct Dimension {
	static constexpr int MASS = Mass;     ///< Mass exponent of the dimension.
	static constexpr int LENGTH = Length; ///< Length exponent of the dimension.
	static constexpr int TIME = Time;     ///< Time exponent of the dimension.

	/**
	 * Get an ASCII string representation of the symbol of the SI unit for this
	 * dimension, e.g. "kg m^2/s" if the dimension is ANGULAR_MOMENTUM.
	 *
	 * \return a String or StringView of the SI unit symbol string.
	 */
	[[nodiscard]] static constexpr auto getSIUnitSymbolString();

	/**
	 * Compare this dimension to another for equality.
	 *
	 * \param other the dimension to compare this one to.
	 *
	 * \return true if the dimensions are equal, false otherwise.
	 */
	[[nodiscard]] consteval bool operator==(const Dimension& other) const noexcept {
		(void)other;
		return true;
	}

	/**
	 * Compare this dimension to another for equality.
	 *
	 * \param other the dimension to compare this one to.
	 *
	 * \return true if the dimensions are equal, false otherwise.
	 */
	template <int OtherMass, int OtherLength, int OtherTime>
	[[nodiscard]] consteval bool operator==(const Dimension<OtherMass, OtherLength, OtherTime>& other) const noexcept {
		(void)other;
		return Mass == OtherMass && Length == OtherLength && Time == OtherTime;
	}

	/**
	 * Get the dimension resulting from taking the additive identity of a
	 * quantity of this dimension.
	 *
	 * \return the resulting dimension.
	 */
	[[nodiscard]] consteval auto operator+() const {
		return Dimension{};
	}

	/**
	 * Get the dimension resulting from taking the additive inverse of a
	 * quantity of this dimension.
	 *
	 * \return the resulting dimension.
	 */
	[[nodiscard]] consteval auto operator-() const {
		return Dimension{};
	}

	/**
	 * Get the dimension resulting from adding a quantity of another dimension
	 * to a quantity of this dimension.
	 *
	 * \param other the dimension to add to this one.
	 *
	 * \return the resulting dimension.
	 */
	[[nodiscard]] consteval auto operator+(const Dimension& other) const {
		(void)other;
		return Dimension{};
	}

	/**
	 * Get the dimension resulting from subtracting a quantity of another
	 * dimension from a quantity of this dimension.
	 *
	 * \param other the dimension to subtract from this one.
	 *
	 * \return the resulting dimension.
	 */
	[[nodiscard]] consteval auto operator-(const Dimension& other) const {
		(void)other;
		return Dimension{};
	}

	/**
	 * Get the dimension resulting from multiplying a quantity of this dimension
	 * with a quantity of another dimension.
	 *
	 * \param other the dimension to multiply this one with.
	 *
	 * \return the resulting dimension.
	 */
	template <int OtherMass, int OtherLength, int OtherTime>
	[[nodiscard]] consteval auto operator*(const Dimension<OtherMass, OtherLength, OtherTime>& other) const {
		(void)other;
		return Dimension<Mass + OtherMass, Length + OtherLength, Time + OtherTime>{};
	}

	/**
	 * Get the dimension resulting from dividing a quantity of this dimension by
	 * a quantity of another dimension.
	 *
	 * \param other the dimension to divide this one by.
	 *
	 * \return the resulting dimension.
	 */
	template <int OtherMass, int OtherLength, int OtherTime>
	[[nodiscard]] consteval auto operator/(const Dimension<OtherMass, OtherLength, OtherTime>& other) const {
		(void)other;
		return Dimension<Mass - OtherMass, Length - OtherLength, Time - OtherTime>{};
	}

	/**
	 * Get the dimension resulting from taking the square root of a quantity of
	 * a dimension.
	 *
	 * \param dimension the dimension to take the square root of.
	 *
	 * \return the resulting dimension.
	 */
	[[nodiscard]] friend consteval auto sqrt(const Dimension& dimension) {
		static_assert((Mass / 2) * 2 == Mass, "Invalid mass dimension of square root.");
		static_assert((Length / 2) * 2 == Length, "Invalid length dimension of square root.");
		static_assert((Time / 2) * 2 == Time, "Invalid time dimension of square root.");
		(void)dimension;
		return Dimension<Mass / 2, Length / 2, Time / 2>{};
	}
};

inline constexpr Dimension<0, 0, 0> DIMENSIONLESS{};                                 ///< Identity dimension.
inline constexpr Dimension<1, 0, 0> MASS{};                                          ///< M
inline constexpr Dimension<0, 1, 0> LENGTH{};                                        ///<      L
inline constexpr Dimension<0, 0, 1> TIME{};                                          ///<           T
inline constexpr Dimension ANGLE = DIMENSIONLESS;                                    ///<
inline constexpr Dimension AREA = LENGTH * LENGTH;                                   ///<      L^2
inline constexpr Dimension VOLUME = LENGTH * LENGTH * LENGTH;                        ///<      L^3
inline constexpr Dimension DENSITY = MASS / VOLUME;                                  ///< M    L^-3
inline constexpr Dimension FREQUENCY = DIMENSIONLESS / TIME;                         ///<           T^-1
inline constexpr Dimension ABSEMENT = LENGTH * TIME;                                 ///<      L    T
inline constexpr Dimension WAVENUMBER = DIMENSIONLESS / LENGTH;                      ///<      L^-1
inline constexpr Dimension VELOCITY = LENGTH / TIME;                                 ///<      L    T^-1
inline constexpr Dimension ACCELERATION = VELOCITY / TIME;                           ///<      L    T^-2
inline constexpr Dimension FORCE = MASS * ACCELERATION;                              ///< M    L    T^-2
inline constexpr Dimension TORQUE = FORCE * LENGTH;                                  ///< M    L^2  T^-2
inline constexpr Dimension IMPULSE = FORCE * TIME;                                   ///< M    L    T^-1
inline constexpr Dimension MOMENTUM = MASS * VELOCITY;                               ///< M    L    T^-1
inline constexpr Dimension MOMENT_OF_INERTIA = MASS * LENGTH * LENGTH;               ///< M    L^2
inline constexpr Dimension ANGULAR_ABSEMENT = ANGLE * TIME;                          ///<           T
inline constexpr Dimension ANGULAR_VELOCITY = ANGLE / TIME;                          ///<           T^-1
inline constexpr Dimension ANGULAR_ACCELERATION = ANGULAR_VELOCITY / TIME;           ///<           T^-2
inline constexpr Dimension ANGULAR_IMPULSE = TORQUE * TIME;                          ///< M    L^2  T^-1
inline constexpr Dimension ANGULAR_MOMENTUM = MOMENT_OF_INERTIA * ANGULAR_VELOCITY;  ///< M    L^2  T^-1
inline constexpr Dimension INTEGRATED_POSITIONAL_IMPULSE = IMPULSE * TIME;           ///< M    L
inline constexpr Dimension INTEGRATED_POSITIONAL_MOMENTUM = MOMENTUM * TIME;         ///< M    L
inline constexpr Dimension INTEGRATED_ROTATIONAL_IMPULSE = ANGULAR_IMPULSE * TIME;   ///< M    L^2
inline constexpr Dimension INTEGRATED_ROTATIONAL_MOMENTUM = ANGULAR_MOMENTUM * TIME; ///< M    L^2
inline constexpr Dimension MASS_FLOW_RATE = MASS / TIME;                             ///< M         T^-1
inline constexpr Dimension VOLUMETRIC_FLOW_RATE = VOLUME / TIME;                     ///<      L^3  T^-1
inline constexpr Dimension WORK = FORCE * LENGTH;                                    ///< M    L^2  T^-2
inline constexpr Dimension POWER = WORK / TIME;                                      ///< M    L^2  T^-3
inline constexpr Dimension ENERGY = POWER * TIME;                                    ///< M    L^2  T^-2

template <int Mass, int Length, int Time>
constexpr auto Dimension<Mass, Length, Time>::getSIUnitSymbolString() {
	constexpr Dimension<Mass, Length, Time> D{};
	if constexpr (D == DIMENSIONLESS) {
		return StringView{};
	} else if constexpr (D == ANGLE) {
		return StringView{"rad"};
	} else if constexpr (D == physics::MASS) {
		return StringView{"kg"};
	} else if constexpr (D == physics::LENGTH) {
		return StringView{"m"};
	} else if constexpr (D == physics::TIME) {
		return StringView{"s"};
	} else if constexpr (D == AREA) {
		return StringView{"m^2"};
	} else if constexpr (D == VOLUME) {
		return StringView{"m^3"};
	} else if constexpr (D == DENSITY) {
		return StringView{"kg/m^3"};
	} else if constexpr (D == FREQUENCY) {
		return StringView{"Hz"};
	} else if constexpr (D == ABSEMENT) {
		return StringView{"m s"};
	} else if constexpr (D == WAVENUMBER) {
		return StringView{"m^-1"};
	} else if constexpr (D == VELOCITY) {
		return StringView{"m/s"};
	} else if constexpr (D == ACCELERATION) {
		return StringView{"m/s^2"};
	} else if constexpr (D == FORCE) {
		return StringView{"N"};
	} else if constexpr (D == TORQUE) {
		return StringView{"N m"};
	} else if constexpr (D == IMPULSE) {
		return StringView{"N s"};
	} else if constexpr (D == MOMENTUM) {
		return StringView{"kg m/s"};
	} else if constexpr (D == MOMENT_OF_INERTIA) {
		return StringView{"kg m^2"};
	} else if constexpr (D == ANGULAR_ABSEMENT) {
		return StringView{"rad s"};
	} else if constexpr (D == ANGULAR_VELOCITY) {
		return StringView{"rad/s"};
	} else if constexpr (D == ANGULAR_ACCELERATION) {
		return StringView{"rad/s^2"};
	} else if constexpr (D == ANGULAR_IMPULSE) {
		return StringView{"N m s"};
	} else if constexpr (D == ANGULAR_MOMENTUM) {
		return StringView{"kg m^2/s"};
	} else if constexpr (D == INTEGRATED_POSITIONAL_IMPULSE) {
		return StringView{"kg m"};
	} else if constexpr (D == INTEGRATED_POSITIONAL_MOMENTUM) {
		return StringView{"kg m"};
	} else if constexpr (D == INTEGRATED_ROTATIONAL_IMPULSE) {
		return StringView{"N m s^2"};
	} else if constexpr (D == INTEGRATED_ROTATIONAL_MOMENTUM) {
		return StringView{"kg m^2"};
	} else if constexpr (D == MASS_FLOW_RATE) {
		return StringView{"kg/s"};
	} else if constexpr (D == VOLUMETRIC_FLOW_RATE) {
		return StringView{"m^3/s"};
	} else if constexpr (D == WORK) {
		return StringView{"J"};
	} else if constexpr (D == POWER) {
		return StringView{"W"};
	} else if constexpr (D == ENERGY) {
		return StringView{"J"};
	} else {
		constexpr auto formatBaseUnit = [](String& output, int exponent, StringView string) -> void {
			if (exponent != 0) {
				if (!output.empty()) {
					output.push_back(' ');
				}
				output.append(string);
				if (exponent != 1) {
					output.append(formatString("^{}", exponent));
				}
			}
		};
		String result{};
		formatBaseUnit(result, Mass, "kg");
		formatBaseUnit(result, Length, "m");
		formatBaseUnit(result, Time, "s");
		return result;
	}
}

/**
 * Concept that checks if a type is a valid physical magnitude type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept magnitude = requires {
	{ std::remove_reference_t<T>::VALUE } -> same_as<const long double&>;
	{ std::remove_reference_t<T>::IS_ABSOLUTE } -> same_as<const bool&>;
} && !requires { typename std::remove_reference_t<T>::MagnitudeType; };

/**
 * Physical magnitude type template.
 *
 * Part of the definition of a physical unit type.
 *
 * \tparam Value quantity multiplier.
 * \tparam IsAbsolute simplified indicator of the magnitude's reference frame
 *         (true for global world space, false for other).
 */
template <long double Value, bool IsAbsolute = false>
struct Magnitude {
	static constexpr long double VALUE = Value;     ///< Quantity multiplier of the magnitude.
	static constexpr bool IS_ABSOLUTE = IsAbsolute; ///< Simplified indicator of the magnitude's reference frame (true for global world space, false for other).

	/**
	 * Compare this magnitude to another for equality.
	 *
	 * \param other the magnitude to compare this one to.
	 *
	 * \return true if the magnitudes are equal, false otherwise.
	 */
	[[nodiscard]] consteval bool operator==(const Magnitude& other) const noexcept {
		(void)other;
		return true;
	}

	/**
	 * Compare this magnitude to another for equality.
	 *
	 * \param other the magnitude to compare this one to.
	 *
	 * \return true if the magnitudes are equal, false otherwise.
	 */
	template <long double OtherValue, bool OtherIsAbsolute>
	[[nodiscard]] consteval bool operator==(const Magnitude<OtherValue, OtherIsAbsolute>& other) const noexcept {
		(void)other;
		return Value == OtherValue && IsAbsolute == OtherIsAbsolute;
	}

	/**
	 * Get the magnitude resulting from taking the additive identity of a
	 * quantity of this magnitude.
	 *
	 * \return the resulting magnitude.
	 *
	 * \throws physics::Error if the additive identity cannot be taken.
	 */
	[[nodiscard]] consteval auto operator+() const {
		if (IsAbsolute) {
			throw physics::Error{"Cannot unary-add a non-relative unit."};
		}
		return Magnitude{};
	}

	/**
	 * Get the magnitude resulting from taking the additive inverse of a
	 * quantity of this magnitude.
	 *
	 * \return the resulting magnitude.
	 *
	 * \throws physics::Error if the additive inverse cannot be taken.
	 */
	[[nodiscard]] consteval auto operator-() const {
		if (IsAbsolute) {
			throw physics::Error{"Cannot negate a non-relative unit."};
		}
		return Magnitude{};
	}

	/**
	 * Get the magnitude resulting from adding a quantity of another magnitude
	 * to a quantity of this magnitude.
	 *
	 * \param other the magnitude to add to this one.
	 *
	 * \return the resulting magnitude.
	 *
	 * \throws physics::Error if quantities of the given magnitudes cannot be
	 *         added.
	 */
	template <long double OtherValue, bool OtherIsAbsolute>
	[[nodiscard]] consteval auto operator+(const Magnitude<OtherValue, OtherIsAbsolute>& other) const {
		(void)other;
		if (Value != OtherValue) {
			throw physics::Error{"Cannot add units of different magnitudes."};
		}
		if (IsAbsolute && OtherIsAbsolute) {
			throw physics::Error{"Cannot add two non-relative units."};
		}
		return Magnitude<Value, IsAbsolute || OtherIsAbsolute>{};
	}

	/**
	 * Get the magnitude resulting from subtracting a quantity of another
	 * magnitude from a quantity of this magnitude.
	 *
	 * \param other the magnitude to subtract from this one.
	 *
	 * \return the resulting magnitude.
	 *
	 * \throws physics::Error if quantities of the given magnitudes cannot be
	 *         subtracted.
	 */
	template <long double OtherValue, bool OtherIsAbsolute>
	[[nodiscard]] consteval auto operator-(const Magnitude<OtherValue, OtherIsAbsolute>& other) const {
		(void)other;
		if (Value != OtherValue) {
			throw physics::Error{"Cannot subtract units of different magnitudes."};
		}
		if (!IsAbsolute && OtherIsAbsolute) {
			throw physics::Error{"Cannot subtract a non-relative unit from a relative unit."};
		}
		return Magnitude<Value, IsAbsolute && !OtherIsAbsolute>{};
	}

	/**
	 * Get the magnitude resulting from multiplying a quantity of this magnitude
	 * with a quantity of another magnitude.
	 *
	 * \param other the magnitude to multiply this one with.
	 *
	 * \return the resulting magnitude.
	 *
	 * \throws physics::Error if quantities of the given magnitudes cannot be
	 *         multiplied.
	 */
	template <long double OtherValue, bool OtherIsAbsolute>
	[[nodiscard]] consteval auto operator*(const Magnitude<OtherValue, OtherIsAbsolute>& other) const {
		(void)other;
		if (IsAbsolute && OtherIsAbsolute) {
			throw physics::Error{"Cannot multiply two non-relative units."};
		}
		return Magnitude<Value * OtherValue, IsAbsolute || OtherIsAbsolute>{};
	}

	/**
	 * Get the magnitude resulting from dividing a quantity of this magnitude by
	 * a quantity of another magnitude.
	 *
	 * \param other the magnitude to divide this one by.
	 *
	 * \return the resulting magnitude.
	 *
	 * \throws physics::Error if quantities of the given magnitudes cannot be
	 *         divided.
	 */
	template <long double OtherValue, bool OtherIsAbsolute>
	[[nodiscard]] consteval auto operator/(const Magnitude<OtherValue, OtherIsAbsolute>& other) const {
		(void)other;
		if (IsAbsolute || OtherIsAbsolute) {
			throw physics::Error{"Cannot divide non-relative units."};
		}
		return Magnitude<Value / OtherValue, false>{};
	}

	/**
	 * Get the magnitude resulting from taking the square root of a quantity of
	 * a magnitude.
	 *
	 * \param magnitude the magnitude to take the square root of.
	 *
	 * \return the resulting magnitude.
	 */
	[[nodiscard]] friend consteval auto sqrt(const Magnitude& magnitude) {
		(void)magnitude;
		return Magnitude<detail::constexprSqrt(Value), IsAbsolute>{};
	}
};

namespace detail {

template <typename UnitT>
concept can_unary_add = !UnitT::MagnitudeType::IS_ABSOLUTE;

template <typename UnitT>
concept can_negate = !UnitT::MagnitudeType::IS_ABSOLUTE;

template <typename UnitT1, typename... UnitTs>
concept can_add = ((UnitT1::DIMENSION == UnitTs::DIMENSION) && ...) && ((UnitT1::MagnitudeType::VALUE == UnitTs::MagnitudeType::VALUE) && ...) &&
                  !(UnitT1::MagnitudeType::IS_ABSOLUTE && (UnitTs::MagnitudeType::IS_ABSOLUTE && ...));

template <typename UnitT1, typename UnitT2>
concept can_subtract = UnitT1::DIMENSION == UnitT2::DIMENSION && UnitT1::MagnitudeType::VALUE == UnitT2::MagnitudeType::VALUE &&
                       (UnitT1::MagnitudeType::IS_ABSOLUTE || !UnitT2::MagnitudeType::IS_ABSOLUTE);

template <typename UnitT1, typename... UnitTs>
concept can_multiply = !(UnitT1::MagnitudeType::IS_ABSOLUTE && (UnitTs::MagnitudeType::IS_ABSOLUTE && ...));

template <typename UnitT1, typename UnitT2>
concept can_divide = !UnitT1::MagnitudeType::IS_ABSOLUTE && !UnitT2::MagnitudeType::IS_ABSOLUTE;

} // namespace detail

/**
 * Concept that checks if a type is a valid physical unit type.
 *
 * \tparam T the type to check.
 */
template <typename T>
concept unit = requires {
	typename std::remove_reference_t<T>::DimensionType;
	{ std::remove_reference_t<T>::DIMENSION } -> dimension;
	{ std::remove_reference_t<T>::DIMENSION } -> same_as<const typename std::remove_reference_t<T>::DimensionType&>;
	{ std::remove_reference_t<T>::MAGNITUDE } -> magnitude;
	{ std::remove_reference_t<T>::MAGNITUDE } -> same_as<const typename std::remove_reference_t<T>::MagnitudeType&>;
} && !requires { typename std::remove_reference_t<T>::Unit; };

/// \cond
template <unit UnitT>
struct canonical_unit {
	using type = UnitT;
};
/// \endcond

/**
 * Canonical unit type of a given unit.
 *
 * \tparam UnitT unit type to get the canonical unit type of.
 */
template <unit UnitT>
using canonical_unit_t = typename canonical_unit<UnitT>::type;

/**
 * Physical base unit type template.
 *
 * \tparam D dimension of the unit.
 * \tparam M magnitude of the unit.
 */
template <Dimension D, Magnitude M>
struct BaseUnit {
	using DimensionType = std::remove_cvref_t<decltype(D)>; ///< Dimension type of the unit.
	using MagnitudeType = std::remove_cvref_t<decltype(M)>; ///< Magintude type of the unit.
	static constexpr DimensionType DIMENSION = D;           ///< Dimension of the unit.
	static constexpr MagnitudeType MAGNITUDE = M;           ///< Magintude of the unit.

	/**
	 * Get an ASCII string representation of the unit symbol of this unit, e.g.
	 * "kg m^2/s" if D is ANGULAR_MOMENTUM and M is 1.
	 *
	 * \return a String or StringView of the unit symbol string.
	 */
	[[nodiscard]] static constexpr auto getSymbolString() {
		if constexpr (MagnitudeType::VALUE == 1.0l) {
			return DimensionType::getSIUnitSymbolString();
		} else {
			return formatString("({} {})", MagnitudeType::VALUE, DimensionType::getSIUnitSymbolString());
		}
	}

	/**
	 * Implicitly convert this unit to another unit, where allowed.
	 *
	 * \tparam OtherUnitT unit type to convert to.
	 *
	 * \return the converted unit.
	 */
	template <unit OtherUnitT>
	[[nodiscard]] consteval operator OtherUnitT() const
		requires(OtherUnitT::DIMENSION == D && OtherUnitT::MagnitudeType::VALUE == MagnitudeType::VALUE && OtherUnitT::MagnitudeType::IS_ABSOLUTE && !MagnitudeType::IS_ABSOLUTE) {
		return OtherUnitT{};
	}

	/**
	 * Get the canonical unit resulting from taking the additive identity of a
	 * quantity of this unit.
	 *
	 * \return the resulting unit.
	 */
	[[nodiscard]] consteval auto operator+() const {
		static_assert(detail::can_unary_add<BaseUnit>, "This unit cannot be unary-added.");
		return canonical_unit_t<BaseUnit<+DIMENSION, +MAGNITUDE>>{};
	}

	/**
	 * Get the canonical unit resulting from taking the additive inverse of a
	 * quantity of this unit.
	 *
	 * \return the resulting unit.
	 */
	[[nodiscard]] consteval auto operator-() const {
		static_assert(detail::can_negate<BaseUnit>, "This unit cannot be negated.");
		return canonical_unit_t<BaseUnit<-DIMENSION, -MAGNITUDE>>{};
	}

	/**
	 * Get the canonical unit resulting from adding a quantity of another unit
	 * to a quantity of this unit.
	 *
	 * \param other the unit to add to this one.
	 *
	 * \return the resulting unit.
	 */
	template <unit OtherUnitT>
	[[nodiscard]] consteval auto operator+(const OtherUnitT& other) const {
		static_assert(detail::can_add<BaseUnit, OtherUnitT>, "These units cannot be added.");
		(void)other;
		return canonical_unit_t<BaseUnit<DIMENSION + OtherUnitT::DIMENSION, MAGNITUDE + OtherUnitT::MAGNITUDE>>{};
	}

	/**
	 * Get the canonical unit resulting from subtracting a quantity of another
	 * unit from a quantity of this unit.
	 *
	 * \param other the unit to subtract from this one.
	 *
	 * \return the resulting unit.
	 */
	template <unit OtherUnitT>
	[[nodiscard]] consteval auto operator-(const OtherUnitT& other) const {
		static_assert(detail::can_subtract<BaseUnit, OtherUnitT>, "These units cannot be subtracted.");
		(void)other;
		return canonical_unit_t<BaseUnit<DIMENSION - OtherUnitT::DIMENSION, MAGNITUDE - OtherUnitT::MAGNITUDE>>{};
	}

	/**
	 * Get the canonical unit resulting from multiplying a quantity of this unit
	 * with a quantity of another unit.
	 *
	 * \param other the unit to multiply this one with.
	 *
	 * \return the resulting unit.
	 */
	template <unit OtherUnitT>
	[[nodiscard]] consteval auto operator*(const OtherUnitT& other) const {
		static_assert(detail::can_multiply<BaseUnit, OtherUnitT>, "These units cannot be multiplied.");
		(void)other;
		return canonical_unit_t<BaseUnit<DIMENSION * OtherUnitT::DIMENSION, MAGNITUDE * OtherUnitT::MAGNITUDE>>{};
	}

	/**
	 * Get the canonical unit resulting from dividing a quantity of this unit by
	 * a quantity of another unit.
	 *
	 * \param other the unit to divide this one by.
	 *
	 * \return the resulting unit.
	 */
	template <unit OtherUnitT>
	[[nodiscard]] consteval auto operator/(const OtherUnitT& other) const {
		static_assert(detail::can_divide<BaseUnit, OtherUnitT>, "These units cannot be divided.");
		(void)other;
		return canonical_unit_t<BaseUnit<DIMENSION / OtherUnitT::DIMENSION, MAGNITUDE / OtherUnitT::MAGNITUDE>>{};
	}
};

/**
 * Compare two physical units for equality.
 *
 * \param a first unit.
 * \param b second unit.
 *
 * \return true if the units are equal, false otherwise.
 */
template <unit UnitT1, unit UnitT2>
[[nodiscard]] consteval bool operator==(UnitT1 a, UnitT2 b) {
	(void)a;
	(void)b;
	return UnitT1::DIMENSION == UnitT2::DIMENSION && UnitT1::MAGNITUDE == UnitT2::MAGNITUDE;
}

/**
 * Get the unit resulting from taking the additive identity of a quantity of a
 * unit.
 *
 * This is a special case where the resulting unit is not necessarily canonical,
 * in order to preserve more information than the general BaseUnit::operator+().
 *
 * \param a unit to get the additive identity of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto operator+(const UnitT& a) requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
	(void)a;
	return UnitT{};
}

/**
 * Get the unit resulting from taking the additive inverse of a quantity of a
 * unit.
 *
 * This is a special case where the resulting unit is not necessarily canonical,
 * in order to preserve more information than the general BaseUnit::operator-().
 *
 * \param a unit to get the additive inverse of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto operator-(const UnitT& a) requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
	(void)a;
	return UnitT{};
}

/**
 * Get the unit resulting from adding two quantities of the same unit.
 *
 * This is a special case where the resulting unit is not necessarily canonical,
 * in order to preserve more information than the general BaseUnit::operator+().
 *
 * \param a first unit.
 * \param b second unit.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto operator+(const UnitT& a, const UnitT& b) requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
	(void)a;
	(void)b;
	return UnitT{};
}

/**
 * Get the unit resulting from subtracting two quantities of the same unit.
 *
 * This is a special case where the resulting unit is not necessarily canonical,
 * in order to preserve more information than the general BaseUnit::operator-().
 *
 * \param a first unit.
 * \param b second unit.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto operator-(const UnitT& a, const UnitT& b) requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
	(void)a;
	(void)b;
	return UnitT{};
}

/**
 * Get the unit resulting from multiplying a unit by a dimensionless unit with a
 * magnitude of 1.
 *
 * This is a special case where the resulting unit is not necessarily canonical,
 * in order to preserve more information than the general BaseUnit::operator*().
 *
 * \param a first unit.
 * \param b second unit.
 *
 * \return the resulting unit.
 */
template <unit UnitT1, unit UnitlessT2>
[[nodiscard]] consteval auto operator*(const UnitT1& a, const UnitlessT2& b)
	requires(UnitlessT2::DIMENSION == DIMENSIONLESS && UnitlessT2::MagnitudeType::VALUE == 1.0l && !UnitlessT2::MagnitudeType::IS_ABSOLUTE) {
	(void)a;
	(void)b;
	return UnitT1{};
}

/**
 * Get the unit resulting from multiplying a dimensionless unit with a magnitude
 * of 1 with another unit.
 *
 * This is a special case where the resulting unit is not necessarily canonical,
 * in order to preserve more information than the general BaseUnit::operator*().
 *
 * \param a first unit.
 * \param b second unit.
 *
 * \return the resulting unit.
 */
template <unit UnitlessT1, unit UnitT2>
[[nodiscard]] consteval auto operator*(const UnitlessT1& a, const UnitT2& b)
	requires(UnitlessT1::DIMENSION == DIMENSIONLESS && UnitlessT1::MagnitudeType::VALUE == 1.0l && !UnitlessT1::MagnitudeType::IS_ABSOLUTE) {
	(void)a;
	(void)b;
	return UnitT2{};
}

/**
 * Get the unit resulting from dividing a unit by a dimensionless unit with a
 * magnitude of 1.
 *
 * This is a special case where the resulting unit is not necessarily canonical,
 * in order to preserve more information than the general BaseUnit::operator/().
 *
 * \param a first unit.
 * \param b second unit.
 *
 * \return the resulting unit.
 */
template <unit UnitT1, unit UnitlessT2>
[[nodiscard]] consteval auto operator/(const UnitT1& a, const UnitlessT2& b)
	requires(UnitlessT2::DIMENSION == DIMENSIONLESS && UnitlessT2::MagnitudeType::VALUE == 1.0l && !UnitlessT2::MagnitudeType::IS_ABSOLUTE) {
	(void)a;
	(void)b;
	return UnitT1{};
}

/// \cond
template <unit UnitT>
struct in_base_units {
	using type = BaseUnit<UnitT::DIMENSION, UnitT::MAGNITUDE>;
};
/// \endcond

/**
 * Corresponding instantiation of the BaseUnit type template of a given unit
 * type.
 *
 * \tparam UnitT unit type to get the base unit type of.
 */
template <unit UnitT>
using in_base_units_t = typename in_base_units<UnitT>::type;

/**
 * The base unit type resulting from adding a set of quantities of the given
 * units.
 *
 * \tparam UnitT1 first unit type to add.
 * \tparam UnitTs other unit types to add.
 */
template <typename UnitT1, typename... UnitTs>
requires(detail::can_add<UnitT1, UnitTs...>) using BaseSum = BaseUnit<(UnitT1::DIMENSION + ... + UnitTs::DIMENSION), (UnitT1::MAGNITUDE + ... + UnitTs::MAGNITUDE)>;

/**
 * The canonical unit type resulting from adding a set of quantities of the
 * given units.
 *
 * \tparam UnitT1 first unit type to add.
 * \tparam UnitTs other unit types to add.
 */
template <typename UnitT1, typename... UnitTs>
using Sum = canonical_unit_t<BaseSum<UnitT1, UnitTs...>>;

/**
 * The base unit type resulting from subtracting two quantities of the given
 * units.
 *
 * \tparam UnitT1 unit type to subtract from.
 * \tparam UnitT2 unit type to subtract by.
 */
template <typename UnitT1, typename UnitT2>
requires(detail::can_subtract<UnitT1, UnitT2>) using BaseDifference = BaseUnit<(UnitT1::DIMENSION - UnitT2::DIMENSION), (UnitT1::MAGNITUDE - UnitT2::MAGNITUDE)>;

/**
 * The canonical unit type resulting from subtracting two quantities of the
 * given units.
 *
 * \tparam UnitT1 unit type to subtract from.
 * \tparam UnitT2 unit type to subtract by.
 */
template <typename UnitT1, typename UnitT2>
using Difference = canonical_unit_t<BaseDifference<UnitT1, UnitT2>>;

/**
 * The base unit type resulting from multiplying a set of quantities of the
 * given units.
 *
 * \tparam UnitT1 first unit type to multiply.
 * \tparam UnitTs other unit types to multiply.
 */
template <typename UnitT1, typename... UnitTs>
requires(detail::can_multiply<UnitT1, UnitTs...>) using BaseProduct = BaseUnit<(UnitT1::DIMENSION * ... * UnitTs::DIMENSION), (UnitT1::MAGNITUDE * ... * UnitTs::MAGNITUDE)>;

/**
 * The canonical unit type resulting from multiplying a set of quantities of the
 * given units.
 *
 * \tparam UnitT1 first unit type to multiply.
 * \tparam UnitTs other unit types to multiply.
 */
template <typename UnitT1, typename... UnitTs>
using Product = canonical_unit_t<BaseProduct<UnitT1, UnitTs...>>;

/**
 * The base unit type resulting from dividing two quantities of the given units.
 *
 * \tparam UnitT1 unit type to divide.
 * \tparam UnitT2 unit type to divide by.
 */
template <typename UnitT1, typename UnitT2>
requires(detail::can_divide<UnitT1, UnitT2>) using BaseQuotient = BaseUnit<(UnitT1::DIMENSION / UnitT2::DIMENSION), (UnitT1::MAGNITUDE / UnitT2::MAGNITUDE)>;

/**
 * The canonical unit type resulting from dividing two quantities of the given
 * units.
 *
 * \tparam UnitT1 unit type to divide.
 * \tparam UnitT2 unit type to divide by.
 */
template <typename UnitT1, typename UnitT2>
using Quotient = canonical_unit_t<BaseQuotient<UnitT1, UnitT2>>;

/**
 * The base unit type corresponding to a given unit with its magnitude scaled by
 * a given multiplier.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 * \tparam Multiple scale to multiply the unit's magnitude by.
 */
template <typename UnitT, long double Multiple>
using BaseMultipleOf = BaseUnit<UnitT::DIMENSION, UnitT::MAGNITUDE * Magnitude<Multiple>{}>;

/**
 * The canonical unit type corresponding to a given unit with its magnitude
 * scaled by a given multiplier.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 * \tparam Multiple scale to multiply the unit's magnitude by.
 */
template <typename UnitT, long double Multiple>
using MultipleOf = canonical_unit_t<BaseMultipleOf<UnitT, Multiple>>;

/**
 * Get the canonical unit corresponding to a given unit with its magnitude
 * scaled by a given multiplier.
 *
 * \tparam Multiple scale to multiply the unit's magnitude by.
 *
 * \param a unit to scale the magnitude of.
 *
 * \return the resulting unit.
 */
template <long double Multiple, unit UnitT>
[[nodiscard]] consteval auto multipleOf(UnitT a) {
	(void)a;
	return MultipleOf<UnitT, Multiple>{};
}

/**
 * The base unit type corresponding to a given unit with its magnitude made
 * relative.
 *
 * \tparam UnitT unit type to get the relative unit of.
 */
template <typename UnitT>
using BaseRelative = BaseUnit<UnitT::DIMENSION, Magnitude<UnitT::MagnitudeType::VALUE, false>{}>;

/**
 * The canonical unit type corresponding to a given unit with its magnitude made
 * relative.
 *
 * \tparam UnitT unit type to get the relative unit of.
 */
template <typename UnitT>
using Relative = canonical_unit_t<BaseRelative<UnitT>>;

/**
 * Get the canonical unit corresponding to a given unit with its magnitude made
 * relative.
 *
 * \param a unit to get the relative unit of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto relative(UnitT a) {
	(void)a;
	return Relative<UnitT>{};
}

/**
 * The base unit type corresponding to a given unit with its magnitude made
 * absolute.
 *
 * \tparam UnitT unit type to get the absolute unit of.
 */
template <typename UnitT>
using BaseAbsolute = BaseUnit<UnitT::DIMENSION, Magnitude<UnitT::MagnitudeType::VALUE, true>{}>;

/**
 * The canonical unit type corresponding to a given unit with its magnitude made
 * absolute.
 *
 * \tparam UnitT unit type to get the absolute unit of.
 */
template <typename UnitT>
struct Absolute : BaseAbsolute<UnitT> {
	/** \copydoc BaseUnit::getSymbolString() */
	[[nodiscard]] static constexpr auto getSymbolString() {
		return BaseAbsolute<UnitT>::getSymbolString();
	}

	using BaseAbsolute<UnitT>::BaseAbsolute;

	/**
	 * Implicitly convert a unit to its absolute unit.
	 *
	 * \param other unit to convert.
	 */
	consteval Absolute(UnitT other) {
		(void)other;
	}
};

/// \cond
template <Dimension D, Magnitude M>
requires(decltype(M)::IS_ABSOLUTE) struct canonical_unit<BaseUnit<D, M>> {
	using type = Absolute<Relative<BaseUnit<D, M>>>;
};
/// \endcond

/**
 * Get the canonical unit corresponding to a given unit with its magnitude made
 * absolute.
 *
 * \param a unit to get the absolute unit of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto absolute(UnitT a) {
	(void)a;
	return Absolute<UnitT>{};
}

/**
 * The base unit type resulting from taking the square root of a quantity of the
 * given unit.
 *
 * \tparam UnitT unit type to get the square root of.
 */
template <typename UnitT>
using BaseSqrt = BaseUnit<sqrt(UnitT::DIMENSION), sqrt(UnitT::MAGNITUDE)>;

/**
 * The canonical unit type resulting from taking the square root of a quantity
 * of the given unit.
 *
 * \tparam UnitT unit type to get the square root of.
 */
template <typename UnitT>
using Sqrt = canonical_unit_t<BaseSqrt<UnitT>>;

/**
 * Get the canonical unit resulting from taking the square root of a quantity of
 * a given unit.
 *
 * \param a unit to get the square root of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto sqrt(UnitT a) {
	(void)a;
	return Sqrt<UnitT>{};
}

/**
 * The base unit type resulting from taking the multiplicative inverse of a
 * quantity of the given unit.
 *
 * \tparam UnitT unit type to get the multiplicative inverse of.
 */
template <typename UnitT>
using BaseReciprocal = BaseUnit<DIMENSIONLESS / UnitT::DIMENSION, Magnitude<1.0l / UnitT::MagnitudeType::VALUE, UnitT::MagnitudeType::IS_ABSOLUTE>{}>;

/**
 * The canonical unit type resulting from taking the multiplicative inverse of a
 * quantity of the given unit.
 *
 * \tparam UnitT unit type to get the multiplicative inverse of.
 */
template <typename UnitT>
using Reciprocal = canonical_unit_t<BaseReciprocal<UnitT>>;

/**
 * Get the canonical unit resulting from taking the multiplicative inverse of a
 * quantity of a given unit.
 *
 * \param a unit to get the multiplicative inverse of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto per(UnitT a) {
	(void)a;
	return Reciprocal<UnitT>{};
}

/**
 * The base unit type resulting from squaring a quantity of the given unit.
 *
 * \tparam UnitT unit type to get the square of.
 */
template <typename UnitT>
using BaseSquare = BaseProduct<UnitT, UnitT>;

/**
 * The canonical unit type resulting from squaring a quantity of the given unit.
 *
 * \tparam UnitT unit type to get the square of.
 */
template <typename UnitT>
using Square = canonical_unit_t<BaseSquare<UnitT>>;

/**
 * Get the canonical unit resulting from squaring a quantity of a given unit.
 *
 * \param a unit to get the square of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto square(UnitT a) {
	(void)a;
	return Square<UnitT>{};
}

/**
 * The base unit type resulting from taking the cube of a quantity of the given
 * unit.
 *
 * \tparam UnitT unit type to get the cube of.
 */
template <typename UnitT>
using BaseCubic = BaseProduct<UnitT, UnitT, UnitT>;

/**
 * The canonical unit type resulting from taking the cube of a quantity of the
 * given unit.
 *
 * \tparam UnitT unit type to get the cube of.
 */
template <typename UnitT>
using Cubic = canonical_unit_t<BaseCubic<UnitT>>;

/**
 * Get the canonical unit resulting from taking the cube of a quantity of a
 * given unit.
 *
 * \param a unit to get the cube of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto cubic(UnitT a) {
	(void)a;
	return Cubic<UnitT>{};
}

/**
 * The base unit type corresponding to a given unit with its magnitude scaled by
 * 1000.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 */
template <typename UnitT>
using BaseKilo = BaseMultipleOf<UnitT, 1000.0l>;

/**
 * The canonical unit type corresponding to a given unit with its magnitude
 * scaled by 1000.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 */
template <typename UnitT>
using Kilo = canonical_unit_t<BaseKilo<UnitT>>;

/**
 * Get the canonical unit corresponding to a given unit with its magnitude
 * scaled by 1000.
 *
 * \param a unit to scale the magnitude of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto kilo(UnitT a) {
	(void)a;
	return Kilo<UnitT>{};
}

/**
 * The base unit type corresponding to a given unit with its magnitude scaled by
 * 1/10.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 */
template <typename UnitT>
using BaseDeci = BaseMultipleOf<UnitT, 0.1l>;

/**
 * The canonical unit type corresponding to a given unit with its magnitude
 * scaled by 1/10.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 */
template <typename UnitT>
using Deci = canonical_unit_t<BaseDeci<UnitT>>;

/**
 * Get the canonical unit corresponding to a given unit with its magnitude
 * scaled by 1/10.
 *
 * \param a unit to scale the magnitude of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto deci(UnitT a) {
	(void)a;
	return Deci<UnitT>{};
}

/**
 * The base unit type corresponding to a given unit with its magnitude scaled by
 * 1/100.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 */
template <typename UnitT>
using BaseCenti = BaseMultipleOf<UnitT, 0.01l>;

/**
 * The canonical unit type corresponding to a given unit with its magnitude
 * scaled by 1/100.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 */
template <typename UnitT>
using Centi = canonical_unit_t<BaseCenti<UnitT>>;

/**
 * Get the canonical unit corresponding to a given unit with its magnitude
 * scaled by 1/100.
 *
 * \param a unit to scale the magnitude of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto centi(UnitT a) {
	(void)a;
	return Centi<UnitT>{};
}

/**
 * The base unit type corresponding to a given unit with its magnitude scaled by
 * 1/1000.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 */
template <typename UnitT>
using BaseMilli = BaseMultipleOf<UnitT, 0.001l>;

/**
 * The canonical unit type corresponding to a given unit with its magnitude
 * scaled by 1/1000.
 *
 * \tparam UnitT unit type to scale the magnitude of.
 */
template <typename UnitT>
using Milli = canonical_unit_t<BaseMilli<UnitT>>;

/**
 * Get the canonical unit corresponding to a given unit with its magnitude
 * scaled by 1/1000.
 *
 * \param a unit to scale the magnitude of.
 *
 * \return the resulting unit.
 */
template <unit UnitT>
[[nodiscard]] consteval auto milli(UnitT a) {
	(void)a;
	return Milli<UnitT>{};
}

/**
 * The base relative SI unit type for a given dimension.
 *
 * \tparam D dimension to get the SI unit of.
 */
template <Dimension D>
using BaseSIUnit = BaseUnit<D, Magnitude<1.0l>{}>;

/**
 * The canonical relative SI unit type for a given dimension.
 *
 * \tparam D dimension to get the SI unit of.
 */
template <Dimension D>
using SIUnit = canonical_unit_t<BaseSIUnit<D>>;

/**
 * The canonical relative SI unit for a given dimension.
 *
 * \tparam D dimension to get the SI unit of.
 */
template <Dimension D>
inline constexpr SIUnit<D> SI_UNIT{};

// Common unit definitions:
/// \cond
#define GREM_PRIVATE_PHYSICS_DEFINE_UNIT(Name, UPPERCASE_NAME, symbol, ...) \
	struct Name : __VA_ARGS__ { \
		[[nodiscard]] static constexpr StringView getSymbolString() { \
			return symbol; \
		} \
	}; \
	inline constexpr Name UPPERCASE_NAME{};

#define GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Name, UPPERCASE_NAME, symbol, ...) \
	GREM_PRIVATE_PHYSICS_DEFINE_UNIT(Name, UPPERCASE_NAME, symbol, __VA_ARGS__) \
	template <> \
	struct canonical_unit<in_base_units_t<__VA_ARGS__>> { \
		using type = Name; \
	};

GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Unitless, UNITLESS, {}, BaseSIUnit<DIMENSIONLESS>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(Radians, RADIANS, "rad", BaseSIUnit<ANGLE>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Degrees, DEGREES, "deg", BaseMultipleOf<Radians, 0.01745329251994329576923690768489l>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Turns, TURNS, "turns", BaseMultipleOf<Radians, 6.28318530718l>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Kilograms, KILOGRAMS, "kg", BaseSIUnit<MASS>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Grams, GRAMS, "g", BaseMilli<Kilograms>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Meters, METERS, "m", BaseSIUnit<LENGTH>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Kilometers, KILOMETERS, "km", BaseKilo<Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Decimeters, DECIMETERS, "dm", BaseDeci<Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Centimeters, CENTIMETERS, "cm", BaseCenti<Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Millimeters, MILLIMETERS, "mm", BaseMilli<Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Seconds, SECONDS, "s", BaseSIUnit<TIME>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Milliseconds, MILLISECONDS, "ms", BaseMilli<Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Minutes, MINUTES, "min", BaseMultipleOf<Seconds, 60.0l>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Hours, HOURS, "h", BaseMultipleOf<Minutes, 60.0l>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(SquareMeters, SQUARE_METERS, "m^2", BaseSquare<Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(SquareKilometers, SQUARE_KILOMETERS, "km^2", BaseSquare<Kilometers>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(SquareDecimeters, SQUARE_DECIMETERS, "dm^2", BaseSquare<Decimeters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(SquareCentimeters, SQUARE_CENTIMETERS, "cm^2", BaseSquare<Centimeters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(SquareMillimeters, SQUARE_MILLIMETERS, "mm^2", BaseSquare<Millimeters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(CubicMeters, CUBIC_METERS, "m^3", BaseCubic<Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(CubicDecimeters, CUBIC_DECIMETERS, "dm^3", BaseCubic<Decimeters>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(CubicCentimeters, CUBIC_CENTIMETERS, "cm^3", BaseCubic<Centimeters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(CubicMillimeters, CUBIC_MILLIMETERS, "mm^3", BaseCubic<Millimeters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Liters, LITERS, "l", CubicDecimeters)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Deciliters, DECILITERS, "dl", BaseDeci<Liters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Centiliters, CENTILITERS, "cl", BaseCenti<Liters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Milliliters, MILLILITERS, "ml", CubicCentimeters)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(KilogramsPerCubicMeter, KILOGRAMS_PER_CUBIC_METER, "kg/m^3", BaseQuotient<Kilograms, CubicMeters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(GramsPerCubicCentimeter, GRAMS_PER_CUBIC_CENTIMETER, "g/cm^3", BaseQuotient<Grams, CubicCentimeters>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(PerSecond, PER_SECOND, "per second", BaseReciprocal<Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(PerMillisecond, PER_MILLISECOND, "per millisecond", BaseReciprocal<Milliseconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(PerMinute, PER_MINUTE, "per minute", BaseReciprocal<Minutes>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(PerHour, PER_HOUR, "per hour", BaseReciprocal<Hours>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(PerMeter, PER_METER, "per meter", BaseReciprocal<Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(PerKilogram, PER_KILOGRAM, "per kilogram", BaseReciprocal<Kilograms>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Hertz, HERTZ, "Hz", PerSecond)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(MeterSeconds, METER_SECONDS, "m s", BaseProduct<Meters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(MetersPerSecond, METERS_PER_SECOND, "m/s", BaseQuotient<Meters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(DecimetersPerSecond, DECIMETERS_PER_SECOND, "dm/s", BaseQuotient<Decimeters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(CentimetersPerSecond, CENTIMETERS_PER_SECOND, "cm/s", BaseQuotient<Centimeters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(MillimetersPerSecond, MILLIMETERS_PER_SECOND, "mm/s", BaseQuotient<Millimeters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(KilometersPerHour, KILOMETERS_PER_HOUR, "km/h", BaseQuotient<Kilometers, Hours>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(MetersPerSecondSquared, METERS_PER_SECOND_SQUARED, "m/s^2", BaseQuotient<Meters, BaseSquare<Seconds>>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(KilogramMetersPerSecondSquared, KILOGRAM_METERS_PER_SECOND_SQUARED, "kg m/s^2", BaseProduct<Kilograms, MetersPerSecondSquared>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Newton, NEWTON, "N", KilogramMetersPerSecondSquared)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(KilogramMeters, KILOGRAM_METERS, "kg m", BaseProduct<Kilograms, Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(KilogramSquareMeters, KILOGRAM_SQUARE_METERS, "kg m^2", BaseProduct<Kilograms, SquareMeters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(PerKilogramSquareMeter, PER_KILOGRAM_SQUARE_METER, "per kilogram square meter", BaseReciprocal<KilogramSquareMeters>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(NewtonMeters, NEWTON_METERS, "N m", BaseProduct<Newton, Meters>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(KilogramSquareMetersPerSecondSquared, KILOGRAM_SQUARE_METERS_PER_SECOND_SQUARED, "kg m^s/s^2",
	BaseQuotient<KilogramSquareMeters, BaseSquare<Seconds>>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(NewtonSeconds, NEWTON_SECONDS, "N s", BaseProduct<Newton, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(KilogramMetersPerSecond, KILOGRAM_METERS_PER_SECOND, "kg m/s", BaseQuotient<KilogramMeters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(RadianSeconds, RADIAN_SECONDS, "rad s", BaseProduct<Radians, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(DegreeSeconds, DEGREE_SECONDS, "deg s", BaseProduct<Degrees, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(TurnSeconds, TURN_SECONDS, "turns s", BaseProduct<Turns, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(RadiansPerSecond, RADIANS_PER_SECOND, "rad/s", BaseQuotient<Radians, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(DegreesPerSecond, DEGREES_PER_SECOND, "deg/s", BaseQuotient<Degrees, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(TurnsPerSecond, TURNS_PER_SECOND, "turns/s", BaseQuotient<Turns, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(RadiansPerSecondSquared, RADIANS_PER_SECOND_SQUARED, "rad/s^2", BaseQuotient<Radians, BaseSquare<Seconds>>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(NewtonMeterSeconds, NEWTON_METER_SECONDS, "N m s", BaseProduct<Newton, Meters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(KilogramSquareMetersPerSecond, KILOGRAM_SQUARE_METERS_PER_SECOND, "kg m^s/s", BaseQuotient<KilogramSquareMeters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(NewtonMeterSecondsSquared, NEWTON_METER_SECONDS_SQUARED, "N m s^2", BaseProduct<Newton, Meters, BaseSquare<Seconds>>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(KilogramsPerSecond, KILOGRAMS_PER_SECOND, "kg/s", BaseQuotient<Kilograms, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(CubicMetersPerSecond, CUBIC_METERS_PER_SECOND, "m^3/s", BaseQuotient<CubicMeters, Seconds>)
GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT(Watts, WATTS, "W", BaseQuotient<KilogramSquareMeters, Cubic<Seconds>>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(KilogramSquareMetersPerSecondCubed, KILOGRAM_SQUARE_METERS_PER_SECOND_CUBED, "kg m^2/s^3", BaseQuotient<KilogramSquareMeters, Cubic<Seconds>>)
GREM_PRIVATE_PHYSICS_DEFINE_UNIT(Joules, JOULES, "J", BaseQuotient<KilogramSquareMeters, BaseSquare<Seconds>>)

#undef GREM_PRIVATE_PHYSICS_DEFINE_UNIT
#undef GREM_PRIVATE_PHYSICS_DEFINE_CANONICAL_UNIT

static_assert(sqrt(SQUARE_METERS) == METERS);
static_assert(sqrt(square(SQUARE_METERS)) == SQUARE_METERS);
static_assert(sqrt(square(square(SQUARE_METERS))) == square(SQUARE_METERS));
/// \endcond

/**
 * Abstract physical quantity representing the origin of the global world space
 * reference frame, or the relative value 0 of any unit.
 *
 * Designed to be implicitly convertible from a 0 literal, or constructed from a
 * compile-time value of 0, but nothing else.
 */
struct Zero {
	/**
	 * Explicitly construct a zero from a compile-time float.
	 *
	 * \param value value to construct the zero from. Must be equal to 0.0f.
	 *
	 * \throws physics::Error if the value is not equal to 0.0f.
	 */
	GREM_ALWAYS_INLINE consteval explicit Zero(float value) {
		if (!(value == 0.0f)) {
			throw physics::Error{"Zero constructed from a non-zero value."};
		}
	}

	/**
	 * Explicitly construct a zero from a compile-time int.
	 *
	 * \param value value to construct the zero from. Must be equal to 0.
	 *
	 * \throws physics::Error if the value is not equal to 0.
	 */
	GREM_ALWAYS_INLINE consteval explicit Zero(int value) {
		if (value != 0) {
			throw physics::Error{"Zero constructed from a non-zero value."};
		}
	}

	/**
	 * Implicitly convert a compile-time 0 literal to a zero.
	 *
	 * This constructor will match 0 literals since the C++ standard has a
	 * special case where a literal 0 may implicitly convert to a null pointer,
	 * whereas other integer values may not.
	 *
	 * \param value a literal 0. Must be equal to nullptr.
	 *
	 * \throws physics::Error if the value is not equal to nullptr.
	 */
	GREM_ALWAYS_INLINE consteval Zero(Zero* value) {
		if (value != nullptr) {
			throw physics::Error{"Zero constructed from a non-zero value."};
		}
	}
};

/**
 * Tag type for specifying a specific vector components.
 *
 * \tparam Index index of the corresponding vector component.
 */
template <size_t Index>
struct ComponentSwizzle {
	static constexpr size_t INDEX = Index; ///< Index of the corresponding vector component.
};
inline constexpr ComponentSwizzle<0> X{}; ///< Tag specifying the X component at index 0 of a vector.
inline constexpr ComponentSwizzle<1> Y{}; ///< Tag specifying the Y component at index 1 of a vector.
inline constexpr ComponentSwizzle<2> Z{}; ///< Tag specifying the Z component at index 2 of a vector.

/**
 * Physical quantity type.
 *
 * \tparam N number of dimensions of the world space, specifying the number of
 *         vector components of the quantity (must be 1, 2 or 3).
 * \tparam UnitT unit type of the quantity.
 */
template <size_t N, typename UnitT>
struct Quantity;

/**
 * Physical quantity type representing an angular quantity in world space, with
 * one component in 2D and three components in 3D.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 * \tparam UnitT unit type of the quantity.
 */
template <size_t N, typename UnitT>
requires(N == 2 || N == 3) //
using AngularQuantity = std::conditional_t<N == 3, Quantity<3, UnitT>, Quantity<1, UnitT>>;

template <typename UnitT>
struct Quantity<1, UnitT> {
	using Unit = UnitT;
	using DimensionType = typename Unit::DimensionType;
	using MagnitudeType = typename Unit::MagnitudeType;
	static constexpr size_t RANK = 1;
	static constexpr Unit UNIT{};
	static constexpr DimensionType DIMENSION{};
	static constexpr MagnitudeType MAGNITUDE{};
	static const Quantity MIN;
	static const Quantity MAX;
	static const Quantity SMALLEST_NORMAL;
	static const Quantity SMALLEST_SUBNORMAL;
	static const Quantity MACHINE_EPSILON;
	static const Quantity INF;
	static const Quantity QUIET_NAN;

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Quantity reinterpret(Quantity<1, OtherUnitT> other) {
		return Quantity{other._private_value};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Quantity reinterpret(float value) {
		return Quantity{value};
	}

	float _private_value = 0.0f;

	[[nodiscard]] constexpr float _private_getUnderlyingValue() const {
		return _private_value;
	}

	GREM_ALWAYS_INLINE constexpr Quantity() = default;
	GREM_ALWAYS_INLINE constexpr Quantity(const Quantity&) = default;
	GREM_ALWAYS_INLINE constexpr Quantity(Quantity&&) noexcept = default;
	GREM_ALWAYS_INLINE constexpr Quantity& operator=(const Quantity&) = default;
	GREM_ALWAYS_INLINE constexpr Quantity& operator=(Quantity&&) noexcept = default;
	GREM_ALWAYS_INLINE constexpr ~Quantity() = default;

	GREM_ALWAYS_INLINE constexpr Quantity(Zero) {}

	GREM_ALWAYS_INLINE constexpr Quantity(float value) requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l)
		: _private_value(value) {}

	template <unit OtherUnitT>
	GREM_ALWAYS_INLINE constexpr Quantity(const Quantity<1, OtherUnitT>& q)
		requires(!same_as<OtherUnitT, UnitT> && OtherUnitT::DIMENSION == DIMENSION &&
				 (OtherUnitT::MagnitudeType::IS_ABSOLUTE == MagnitudeType::IS_ABSOLUTE || !OtherUnitT::MagnitudeType::IS_ABSOLUTE))
		: Quantity(Quantity::reinterpret(q * static_cast<float>(OtherUnitT::MagnitudeType::VALUE / MagnitudeType::VALUE))) {}

	template <typename Rep, typename Period>
	GREM_ALWAYS_INLINE constexpr Quantity(const DurationBase<Rep, Period>& duration) requires(DIMENSION == TIME && MagnitudeType::VALUE == 1.0l && !MagnitudeType::IS_ABSOLUTE)
		: _private_value(duration_cast<FloatSeconds>(duration).count() / static_cast<float>(MagnitudeType::VALUE)) {}

	GREM_ALWAYS_INLINE constexpr Quantity& operator=(Zero) {
		_private_value = 0.0f;
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Quantity& other) const noexcept {
		return _private_value == other._private_value;
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Quantity<1, OtherUnitT>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
		return _private_value == other._private_value;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(const Quantity& other) const noexcept {
		return _private_value <=> other._private_value;
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(const Quantity<1, OtherUnitT>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
		return _private_value <=> other._private_value;
	}

	template <arithmetic Arithmetic>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Arithmetic other) const noexcept requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l) {
		return _private_value == static_cast<float>(other);
	}

	template <arithmetic Arithmetic>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(Arithmetic other) const noexcept requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l) {
		return _private_value <=> static_cast<float>(other);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Zero) const noexcept {
		return *this == Quantity{};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(Zero) const noexcept {
		return *this <=> Quantity{};
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const DurationBase<Rep, Period>& other) const noexcept requires(DIMENSION == TIME && !MagnitudeType::IS_ABSOLUTE) {
		if constexpr (same_as<Rep, float>) {
			return FloatSeconds{*this} == duration_cast<FloatSeconds>(other);
		} else {
			return Duration{*this} == duration_cast<Duration>(other);
		}
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(const DurationBase<Rep, Period>& other) const noexcept requires(DIMENSION == TIME && !MagnitudeType::IS_ABSOLUTE) {
		if constexpr (same_as<Rep, float>) {
			return FloatSeconds{*this} <=> duration_cast<FloatSeconds>(other);
		} else {
			return Duration{*this} <=> duration_cast<Duration>(other);
		}
	}

	GREM_ALWAYS_INLINE constexpr operator float() const requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l) {
		return _private_value;
	}

	GREM_ALWAYS_INLINE constexpr operator FloatSeconds() const requires(DIMENSION == TIME && !MagnitudeType::IS_ABSOLUTE) {
		return FloatSeconds{_private_value * static_cast<float>(MagnitudeType::VALUE)};
	}

	GREM_ALWAYS_INLINE constexpr operator Duration() const requires(DIMENSION == TIME && !MagnitudeType::IS_ABSOLUTE) {
		return duration_cast<Duration>(FloatSeconds{_private_value * static_cast<float>(MagnitudeType::VALUE)});
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, Unitless> in(OtherUnitT) const requires(OtherUnitT::DIMENSION == DIMENSION) {
		if constexpr (OtherUnitT::MagnitudeType::VALUE == MagnitudeType::VALUE) {
			return Quantity<1, Unitless>{_private_value};
		} else {
			constexpr float factor = static_cast<float>(MagnitudeType::VALUE / OtherUnitT::MagnitudeType::VALUE);
			return _private_getUnderlyingValue() * factor;
		}
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, OtherUnitT> as(OtherUnitT) const
		requires(OtherUnitT::DIMENSION == DIMENSION && (OtherUnitT::MagnitudeType::IS_ABSOLUTE == MagnitudeType::IS_ABSOLUTE || OtherUnitT::MagnitudeType::IS_ABSOLUTE)) {
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 1;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t getMinIndex() const {
		return 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t getMaxIndex() const {
		return 0;
	}

private:
	template <size_t OtherN, typename OtherUnitT>
	friend struct Quantity;

	GREM_ALWAYS_INLINE constexpr explicit Quantity(float value) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: _private_value(value) {}
};

template <typename UnitT>
inline constexpr Quantity<1, UnitT> Quantity<1, UnitT>::MIN{Limits<float>::MIN};

template <typename UnitT>
inline constexpr Quantity<1, UnitT> Quantity<1, UnitT>::MAX{Limits<float>::MAX};

template <typename UnitT>
inline constexpr Quantity<1, UnitT> Quantity<1, UnitT>::SMALLEST_NORMAL{Limits<float>::SMALLEST_NORMAL};

template <typename UnitT>
inline constexpr Quantity<1, UnitT> Quantity<1, UnitT>::SMALLEST_SUBNORMAL{Limits<float>::SMALLEST_SUBNORMAL};

template <typename UnitT>
inline constexpr Quantity<1, UnitT> Quantity<1, UnitT>::MACHINE_EPSILON{Limits<float>::MACHINE_EPSILON};

template <typename UnitT>
inline constexpr Quantity<1, UnitT> Quantity<1, UnitT>::INF{Limits<float>::INF};

template <typename UnitT>
inline constexpr Quantity<1, UnitT> Quantity<1, UnitT>::QUIET_NAN{Limits<float>::QUIET_NAN};

/**
 * Read-only view of a specific component of a physical vector quantity.
 *
 * \tparam N number of dimensions of the world space, specifying the number of
 *         vector components of the quantity (must be 2 or 3).
 * \tparam UnitT unit type of the quantity.
 * \tparam Index index of the referenced component in the quantity vector.
 */
template <size_t N, typename UnitT, size_t Index>
class ConstComponentProxy {
public:
	static_assert(N == 2 || N == 3);
	static_assert(Index < N);

	using Unit = UnitT;
	using DimensionType = typename Unit::DimensionType;
	using MagnitudeType = typename Unit::MagnitudeType;
	static constexpr size_t RANK = 1;
	static constexpr Unit UNIT{};
	static constexpr DimensionType DIMENSION{};
	static constexpr MagnitudeType MAGNITUDE{};

	GREM_ALWAYS_INLINE constexpr ConstComponentProxy(const Quantity<N, UnitT>& quantity) noexcept
		: quantity(&quantity) {}

	GREM_ALWAYS_INLINE constexpr operator Quantity<1, UnitT>() const noexcept {
		return getValue();
	}

	GREM_ALWAYS_INLINE constexpr operator float() const requires(UnitT::DIMENSION == DIMENSIONLESS && UnitT::MagnitudeType::VALUE == 1.0l) {
		return getValue();
	}

	GREM_ALWAYS_INLINE constexpr operator FloatSeconds() const requires(UnitT{} == SECONDS) {
		return getValue();
	}

	GREM_ALWAYS_INLINE constexpr operator Duration() const requires(UnitT::DIMENSION == TIME && !UnitT::MagnitudeType::IS_ABSOLUTE) {
		return getValue();
	}

	template <size_t OtherN, typename OtherUnitT, size_t OtherIndex>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const ConstComponentProxy<OtherN, OtherUnitT, OtherIndex>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
		return getValue() == Quantity<1, OtherUnitT>{other};
	}

	template <size_t OtherN, typename OtherUnitT, size_t OtherIndex>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(const ConstComponentProxy<OtherN, OtherUnitT, OtherIndex>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
		return getValue() <=> Quantity<1, OtherUnitT>{other};
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Quantity<1, OtherUnitT>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
		return getValue() == other;
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(const Quantity<1, OtherUnitT>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
		return getValue() <=> other;
	}

	template <arithmetic Arithmetic>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Arithmetic other) const noexcept requires(UnitT::DIMENSION == DIMENSIONLESS && UnitT::MagnitudeType::VALUE == 1.0l) {
		return getValue() == other;
	}

	template <arithmetic Arithmetic>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(Arithmetic other) const noexcept requires(UnitT::DIMENSION == DIMENSIONLESS && UnitT::MagnitudeType::VALUE == 1.0l)
	{
		return getValue() <=> other;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Zero) const noexcept {
		return getValue() == Quantity<1, UnitT>{};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(Zero) const noexcept {
		return getValue() <=> Quantity<1, UnitT>{};
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const DurationBase<Rep, Period>& other) const noexcept
		requires(UnitT::DIMENSION == TIME && !UnitT::MagnitudeType::IS_ABSOLUTE) {
		return getValue() == other;
	}

	template <typename Rep, typename Period>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto operator<=>(const DurationBase<Rep, Period>& other) const noexcept
		requires(UnitT::DIMENSION == TIME && !UnitT::MagnitudeType::IS_ABSOLUTE) {
		return getValue() <=> other;
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, Unitless> in(OtherUnitT unit) const requires(OtherUnitT::DIMENSION == DIMENSION) {
		return getValue().in(unit);
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, OtherUnitT> as(OtherUnitT unit) const
		requires(OtherUnitT::DIMENSION == DIMENSION && (OtherUnitT::MagnitudeType::IS_ABSOLUTE == MagnitudeType::IS_ABSOLUTE || OtherUnitT::MagnitudeType::IS_ABSOLUTE)) {
		return getValue().as(unit);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 1;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t getMinIndex() const {
		return 0;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t getMaxIndex() const {
		return 0;
	}

protected:
	const Quantity<N, UnitT>* quantity;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, UnitT> getValue() const noexcept {
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			if constexpr (Index == 0) {
				return Quantity<1, UnitT>::reinterpret(_mm_cvtss_f32(quantity->_private_value));
			} else {
				return Quantity<1, UnitT>::reinterpret(_mm_cvtss_f32(_mm_shuffle_ps(quantity->_private_value, quantity->_private_value, _MM_SHUFFLE(Index, Index, Index, Index))));
			}
		} else {
			return Quantity<1, UnitT>::reinterpret(quantity->_private_value[Index]);
		}
#else
		return Quantity<1, UnitT>::reinterpret(quantity->_private_value[Index]);
#endif
	}
};

/**
 * Reference to a specific component of a physical vector quantity.
 *
 * \tparam N number of dimensions of the world space, specifying the number of
 *         vector components of the quantity (must be 2 or 3).
 * \tparam UnitT unit type of the quantity.
 * \tparam Index index of the referenced component in the quantity vector.
 */
template <size_t N, typename UnitT, size_t Index>
class ComponentProxy : public ConstComponentProxy<N, UnitT, Index> {
public:
	GREM_ALWAYS_INLINE constexpr ComponentProxy(Quantity<N, UnitT>& quantity) noexcept
		: ConstComponentProxy<N, UnitT, Index>(quantity) {}

	GREM_ALWAYS_INLINE ComponentProxy operator=( // NOLINT(misc-unconventional-assign-operator, cppcoreguidelines-c-copy-assignment-signature)
		Quantity<1, UnitT> value) const noexcept {
		Quantity<N, UnitT>* const quantity = const_cast<Quantity<N, UnitT>*>(this->quantity);
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			alignas(16) float components[4];
			_mm_store_ps(components, quantity->_private_value);
			components[Index] = value._private_value;
			if constexpr (Index == 2) {
				components[3] = value._private_value;
			}
			quantity->_private_value = _mm_load_ps(components);
		} else {
			quantity->_private_value[Index] = value._private_value;
		}
#else
		quantity->_private_value[Index] = value._private_value;
#endif
		return *this;
	}

	GREM_ALWAYS_INLINE ComponentProxy operator=( // NOLINT(misc-unconventional-assign-operator, cppcoreguidelines-c-copy-assignment-signature)
		Zero) const noexcept {
		Quantity<N, UnitT>* const quantity = const_cast<Quantity<N, UnitT>*>(this->quantity);
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			alignas(16) float components[4];
			_mm_store_ps(components, quantity->_private_value);
			components[Index] = 0.0f;
			if constexpr (Index == 2) {
				components[3] = 0.0f;
			}
			quantity->_private_value = _mm_load_ps(components);
		} else {
			quantity->_private_value[Index] = 0.0f;
		}
#else
		quantity->_private_value[Index] = 0.0f;
#endif
		return *this;
	}

	template <unit OtherUnitT>
	GREM_ALWAYS_INLINE ComponentProxy operator+=(Quantity<1, OtherUnitT> value) const noexcept {
		static_assert(OtherUnitT::DIMENSION == UnitT::DIMENSION && OtherUnitT::MagnitudeType::VALUE == UnitT::MagnitudeType::VALUE && !OtherUnitT::MagnitudeType::IS_ABSOLUTE,
			"Quantity of this unit cannot be added.");
		Quantity<N, UnitT>* const quantity = const_cast<Quantity<N, UnitT>*>(this->quantity);
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			alignas(16) float components[4];
			_mm_store_ps(components, quantity->_private_value);
			components[Index] += value._private_value;
			if constexpr (Index == 2) {
				components[3] += value._private_value;
			}
			quantity->_private_value = _mm_load_ps(components);
		} else {
			quantity->_private_value[Index] += value._private_value;
		}
#else
		quantity->_private_value[Index] += value._private_value;
#endif
		return *this;
	}

	template <unit OtherUnitT>
	GREM_ALWAYS_INLINE ComponentProxy operator-=(Quantity<1, OtherUnitT> value) const noexcept {
		static_assert(OtherUnitT::DIMENSION == UnitT::DIMENSION && OtherUnitT::MagnitudeType::VALUE == UnitT::MagnitudeType::VALUE && !OtherUnitT::MagnitudeType::IS_ABSOLUTE,
			"Quantity of this unit cannot be subtracted.");
		Quantity<N, UnitT>* const quantity = const_cast<Quantity<N, UnitT>*>(this->quantity);
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			alignas(16) float components[4];
			_mm_store_ps(components, quantity->_private_value);
			components[Index] -= value._private_value;
			if constexpr (Index == 2) {
				components[3] -= value._private_value;
			}
			quantity->_private_value = _mm_load_ps(components);
		} else {
			quantity->_private_value[Index] -= value._private_value;
		}
#else
		quantity->_private_value[Index] -= value._private_value;
#endif
		return *this;
	}

	GREM_ALWAYS_INLINE ComponentProxy operator*=(float value) const noexcept {
		Quantity<N, UnitT>* const quantity = const_cast<Quantity<N, UnitT>*>(this->quantity);
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			alignas(16) float components[4];
			_mm_store_ps(components, quantity->_private_value);
			components[Index] *= value;
			if constexpr (Index == 2) {
				components[3] *= value;
			}
			quantity->_private_value = _mm_load_ps(components);
		} else {
			quantity->_private_value[Index] *= value;
		}
#else
		quantity->_private_value[Index] *= value;
#endif
		return *this;
	}

	GREM_ALWAYS_INLINE ComponentProxy operator/=(float value) const noexcept {
		Quantity<N, UnitT>* const quantity = const_cast<Quantity<N, UnitT>*>(this->quantity);
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			alignas(16) float components[4];
			_mm_store_ps(components, quantity->_private_value);
			components[Index] /= value;
			if constexpr (Index == 2) {
				components[3] /= value;
			}
			quantity->_private_value = _mm_load_ps(components);
		} else {
			quantity->_private_value[Index] /= value;
		}
#else
		quantity->_private_value[Index] /= value;
#endif
		return *this;
	}
};

template <typename UnitT>
struct Quantity<2, UnitT> {
	using Component = Quantity<1, UnitT>;
	using Unit = UnitT;
	using DimensionType = typename Unit::DimensionType;
	using MagnitudeType = typename Unit::MagnitudeType;
	static constexpr size_t RANK = 2;
	static constexpr Unit UNIT{};
	static constexpr DimensionType DIMENSION{};
	static constexpr MagnitudeType MAGNITUDE{};
	static const Quantity MIN;
	static const Quantity MAX;
	static const Quantity SMALLEST_NORMAL;
	static const Quantity SMALLEST_SUBNORMAL;
	static const Quantity MACHINE_EPSILON;
	static const Quantity INF;
	static const Quantity QUIET_NAN;

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Quantity reinterpret(Quantity<2, OtherUnitT> other) {
		return Quantity{other._private_value};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Quantity reinterpret(vec<2, float> value) {
		return Quantity{value};
	}

	vec<2, float> _private_value{};

	[[nodiscard]] constexpr vec<2, float> _private_getUnderlyingValue() const {
		return _private_value;
	}

	GREM_ALWAYS_INLINE constexpr Quantity() = default;
	GREM_ALWAYS_INLINE constexpr Quantity(const Quantity&) = default;
	GREM_ALWAYS_INLINE constexpr Quantity(Quantity&&) noexcept = default;
	GREM_ALWAYS_INLINE constexpr Quantity& operator=(const Quantity&) = default;
	GREM_ALWAYS_INLINE constexpr Quantity& operator=(Quantity&&) noexcept = default;
	GREM_ALWAYS_INLINE constexpr ~Quantity() = default;

	GREM_ALWAYS_INLINE constexpr Quantity(Zero) {}

	GREM_ALWAYS_INLINE constexpr explicit Quantity(Component value)
		: Quantity(value, value) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Component x, Component y)
		: _private_value{x._private_value, y._private_value} {}

	GREM_ALWAYS_INLINE constexpr Quantity(Zero, Component y) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(Component{}, y) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Component x, Zero) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(x, Component{}) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Zero, Zero) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l) {}

	GREM_ALWAYS_INLINE constexpr Quantity(vec<2, float> v) requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l)
		: Quantity(v.x, v.y) {}

	template <unit OtherUnitT>
	GREM_ALWAYS_INLINE constexpr Quantity(const Quantity<2, OtherUnitT>& q)
		requires(!same_as<OtherUnitT, UnitT> && OtherUnitT::DIMENSION == DIMENSION &&
				 (OtherUnitT::MagnitudeType::IS_ABSOLUTE == MagnitudeType::IS_ABSOLUTE || !OtherUnitT::MagnitudeType::IS_ABSOLUTE))
		: Quantity(Quantity::reinterpret(q * static_cast<float>(OtherUnitT::MagnitudeType::VALUE / MagnitudeType::VALUE))) {}

	GREM_ALWAYS_INLINE constexpr Quantity& operator=(Zero) {
		_private_value = {};
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Quantity& other) const noexcept {
		return _private_value == other._private_value;
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Quantity<2, OtherUnitT>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
		return _private_value == other._private_value;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Zero) const noexcept {
		return *this == Quantity{};
	}

	template <size_t Index>
	GREM_ALWAYS_INLINE void set(ComponentSwizzle<Index> Swizzle, Component value) noexcept {
		(*this)[Swizzle] = value;
	}

	template <size_t Index>
	GREM_ALWAYS_INLINE void set(ComponentSwizzle<Index> Swizzle, Zero) noexcept {
		(*this)[Swizzle] = 0;
	}

	GREM_ALWAYS_INLINE void setX(Component x) noexcept {
		set(X, x);
	}

	GREM_ALWAYS_INLINE void setX(Zero) noexcept {
		setX(Component{});
	}

	GREM_ALWAYS_INLINE void setY(Component y) noexcept {
		set(Y, y);
	}

	GREM_ALWAYS_INLINE void setY(Zero) noexcept {
		setY(Component{});
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component get(ComponentSwizzle<Index> Swizzle) const noexcept {
		return (*this)[Swizzle];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Component get(Zero) const noexcept {
		return 0;
	}

	template <size_t IndexX, size_t IndexY>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, ComponentSwizzle<IndexY> SwizzleY) const noexcept {
		return {get(SwizzleX), get(SwizzleY)};
	}

	template <size_t IndexX>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, Zero) const noexcept {
		return {get(SwizzleX), 0};
	}

	template <size_t IndexY>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> get(Zero, ComponentSwizzle<IndexY> SwizzleY) const noexcept {
		return {0, get(SwizzleY)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> get(Zero, Zero) const noexcept {
		return {};
	}

	template <size_t IndexX, size_t IndexY, size_t IndexZ>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, ComponentSwizzle<IndexY> SwizzleY,
		ComponentSwizzle<IndexZ> SwizzleZ) const noexcept {
		return {get(SwizzleX), get(SwizzleY), get(SwizzleZ)};
	}

	template <size_t IndexX, size_t IndexY>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, ComponentSwizzle<IndexY> SwizzleY, Zero) const noexcept {
		return {get(SwizzleX, SwizzleY), 0};
	}

	template <size_t IndexX, size_t IndexZ>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, Zero, ComponentSwizzle<IndexZ> SwizzleZ) const noexcept {
		return {get(SwizzleX), 0, get(SwizzleZ)};
	}

	template <size_t IndexY, size_t IndexZ>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(Zero, ComponentSwizzle<IndexY> SwizzleY, ComponentSwizzle<IndexZ> SwizzleZ) const noexcept {
		return {0, get(SwizzleY), get(SwizzleZ)};
	}

	template <size_t IndexX>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, Zero, Zero) const noexcept {
		return {get(SwizzleX), 0, 0};
	}

	template <size_t IndexY>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(Zero, ComponentSwizzle<IndexY> SwizzleY, Zero) const noexcept {
		return {0, get(SwizzleY), 0};
	}

	template <size_t IndexZ>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(Zero, Zero, ComponentSwizzle<IndexZ> SwizzleZ) const noexcept {
		return {0, 0, get(SwizzleZ)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(Zero, Zero, Zero) const noexcept {
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Component getX() const noexcept {
		return get(X);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Component getY() const noexcept {
		return get(Y);
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE ComponentProxy<2, UnitT, Index> operator[](ComponentSwizzle<Index>) noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return *this;
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE ConstComponentProxy<2, UnitT, Index> operator[](ComponentSwizzle<Index>) const noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return *this;
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE ComponentProxy<2, UnitT, Index> operator[](meta::Constant<Index>) noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return *this;
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE ConstComponentProxy<2, UnitT, Index> operator[](meta::Constant<Index>) const noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr operator vec<2, float>() const requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l) {
		return vec<2, float>{static_cast<float>(getX()), static_cast<float>(getY())};
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, Unitless> in(OtherUnitT) const requires(OtherUnitT::DIMENSION == DIMENSION) {
		if constexpr (OtherUnitT::MagnitudeType::VALUE == MagnitudeType::VALUE) {
			return Quantity<2, Unitless>{_private_value};
		} else {
			constexpr float factor = static_cast<float>(MagnitudeType::VALUE / OtherUnitT::MagnitudeType::VALUE);
			return _private_getUnderlyingValue() * factor;
		}
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<2, OtherUnitT> as(OtherUnitT) const
		requires(OtherUnitT::DIMENSION == DIMENSION && (OtherUnitT::MagnitudeType::IS_ABSOLUTE == MagnitudeType::IS_ABSOLUTE || OtherUnitT::MagnitudeType::IS_ABSOLUTE)) {
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 2;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t getMinIndex() const {
		const Component x = getX();
		const Component y = getY();
		return static_cast<size_t>(y < x);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t getMaxIndex() const {
		const Component x = getX();
		const Component y = getY();
		return static_cast<size_t>(y > x);
	}

private:
	template <size_t OtherN, typename OtherUnitT>
	friend struct Quantity;

	template <size_t OtherN, typename OtherUnitT, size_t Index>
	friend class ConstComponentProxy;

	template <size_t OtherN, typename OtherUnitT, size_t Index>
	friend class ComponentProxy;

	GREM_ALWAYS_INLINE constexpr explicit Quantity(vec<2, float> value) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: _private_value(value) {}
};

template <typename UnitT>
inline constexpr Quantity<2, UnitT> Quantity<2, UnitT>::MIN{vec<2, float>{Limits<float>::MIN}};

template <typename UnitT>
inline constexpr Quantity<2, UnitT> Quantity<2, UnitT>::MAX{vec<2, float>{Limits<float>::MAX}};

template <typename UnitT>
inline constexpr Quantity<2, UnitT> Quantity<2, UnitT>::SMALLEST_NORMAL{vec<2, float>{Limits<float>::SMALLEST_NORMAL}};

template <typename UnitT>
inline constexpr Quantity<2, UnitT> Quantity<2, UnitT>::SMALLEST_SUBNORMAL{vec<2, float>{Limits<float>::SMALLEST_SUBNORMAL}};

template <typename UnitT>
inline constexpr Quantity<2, UnitT> Quantity<2, UnitT>::MACHINE_EPSILON{vec<2, float>{Limits<float>::MACHINE_EPSILON}};

template <typename UnitT>
inline constexpr Quantity<2, UnitT> Quantity<2, UnitT>::INF{vec<2, float>{Limits<float>::INF}};

template <typename UnitT>
inline constexpr Quantity<2, UnitT> Quantity<2, UnitT>::QUIET_NAN{vec<2, float>{Limits<float>::QUIET_NAN}};

template <typename UnitT>
struct alignas(16) Quantity<3, UnitT> {
	using Component = Quantity<1, UnitT>;
	using Unit = UnitT;
	using DimensionType = typename Unit::DimensionType;
	using MagnitudeType = typename Unit::MagnitudeType;
	static constexpr size_t RANK = 3;
	static constexpr Unit UNIT{};
	static constexpr DimensionType DIMENSION{};
	static constexpr MagnitudeType MAGNITUDE{};
	static const Quantity ZERO;
	static const Quantity MIN;
	static const Quantity MAX;
	static const Quantity SMALLEST_NORMAL;
	static const Quantity SMALLEST_SUBNORMAL;
	static const Quantity MACHINE_EPSILON;
	static const Quantity INF;
	static const Quantity QUIET_NAN;

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Quantity reinterpret(Quantity<3, OtherUnitT> other) {
		return Quantity{other._private_value};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Quantity reinterpret(vec<3, float> value) {
		return Quantity{value};
	}

#ifdef GREM_USE_SSE_INTRINSICS
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Quantity reinterpret(__m128 value) {
		return Quantity{value};
	}

	__m128 _private_value{};
#else
	vec<3, float> _private_value{};
#endif

	[[nodiscard]] constexpr vec<3, float> _private_getUnderlyingValue() const {
#ifdef GREM_USE_SSE_INTRINSICS
		alignas(16) float components[4]{};
		_mm_store_ps(components, _private_value);
		return {components[0], components[1], components[2]};
#else
		return _private_value;
#endif
	}

	GREM_ALWAYS_INLINE constexpr Quantity() = default;
	GREM_ALWAYS_INLINE constexpr Quantity(const Quantity&) = default;
	GREM_ALWAYS_INLINE constexpr Quantity(Quantity&&) noexcept = default;
	GREM_ALWAYS_INLINE constexpr Quantity& operator=(const Quantity&) = default;
	GREM_ALWAYS_INLINE constexpr Quantity& operator=(Quantity&&) noexcept = default;
	GREM_ALWAYS_INLINE constexpr ~Quantity() = default;

	GREM_ALWAYS_INLINE constexpr Quantity(Zero) {}

	GREM_ALWAYS_INLINE constexpr explicit Quantity(Component value)
		: Quantity(value, value, value) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Component x, Component y, Component z)
#ifdef GREM_USE_SSE_INTRINSICS
		: _private_value{x._private_value, y._private_value, z._private_value, z._private_value}
#else
		: _private_value{x._private_value, y._private_value, z._private_value}
#endif
	{
	}

	GREM_ALWAYS_INLINE constexpr Quantity(Zero, Component y, Component z) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(Component{}, y, z) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Component x, Zero, Component z) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(x, Component{}, z) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Zero, Zero, Component z) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(Component{}, Component{}, z) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Component x, Component y, Zero) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(x, y, Component{}) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Zero, Component y, Zero) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(Component{}, y, Component{}) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Component x, Zero, Zero) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(x, Component{}, Component{}) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Zero, Zero, Zero) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(Component{}, Component{}, Component{}) {}

	GREM_ALWAYS_INLINE constexpr Quantity(vec<3, float> v) requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l)
		: Quantity(v.x, v.y, v.z) {}

	template <unit OtherUnitT>
	GREM_ALWAYS_INLINE constexpr Quantity(const Quantity<3, OtherUnitT>& q)
		requires(!same_as<OtherUnitT, UnitT> && OtherUnitT::DIMENSION == DIMENSION &&
				 (OtherUnitT::MagnitudeType::IS_ABSOLUTE == MagnitudeType::IS_ABSOLUTE || !OtherUnitT::MagnitudeType::IS_ABSOLUTE))
		: Quantity(Quantity::reinterpret(q * static_cast<float>(OtherUnitT::MagnitudeType::VALUE / MagnitudeType::VALUE))) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Quantity<2, UnitT> xy, Component z)
		: Quantity(xy.getX(), xy.getY(), z) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Quantity<2, UnitT> xy, Zero) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(xy.getX(), xy.getY(), Component{}) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Component x, Quantity<2, UnitT> yz)
		: Quantity(x, yz.getX(), yz.getY()) {}

	GREM_ALWAYS_INLINE constexpr Quantity(Zero, Quantity<2, UnitT> yz) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: Quantity(Component{}, yz.getX(), yz.getY()) {}

	GREM_ALWAYS_INLINE constexpr Quantity& operator=(Zero) {
#ifdef GREM_USE_SSE_INTRINSICS
		_private_value = __m128{};
#else
		_private_value = {};
#endif
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Quantity& other) const noexcept {
#ifdef GREM_USE_SSE_INTRINSICS
		return (_mm_movemask_ps(_mm_cmpeq_ps(_private_value, other._private_value)) & 0b111) == 0b111;
#else
		return _private_value == other._private_value;
#endif
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Quantity<3, OtherUnitT>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
#ifdef GREM_USE_SSE_INTRINSICS
		return (_mm_movemask_ps(_mm_cmpeq_ps(_private_value, other._private_value)) & 0b111) == 0b111;
#else
		return _private_value == other._private_value;
#endif
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Zero) const noexcept {
		return *this == Quantity{};
	}

	template <size_t Index>
	GREM_ALWAYS_INLINE void set(ComponentSwizzle<Index> Swizzle, Component value) noexcept {
		(*this)[Swizzle] = value;
	}

	template <size_t Index>
	GREM_ALWAYS_INLINE void set(ComponentSwizzle<Index> Swizzle, Zero) noexcept {
		set(Swizzle, Component{});
	}

	GREM_ALWAYS_INLINE void setX(Component x) noexcept {
		set(X, x);
	}

	GREM_ALWAYS_INLINE void setX(Zero) noexcept {
		setX(Component{});
	}

	GREM_ALWAYS_INLINE void setY(Component y) noexcept {
		set(Y, y);
	}

	GREM_ALWAYS_INLINE void setY(Zero) noexcept {
		setY(Component{});
	}

	GREM_ALWAYS_INLINE void setZ(Component z) noexcept {
		set(Z, z);
	}

	GREM_ALWAYS_INLINE void setZ(Zero) noexcept {
		setZ(Component{});
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component get(ComponentSwizzle<Index> Swizzle) const noexcept {
		return (*this)[Swizzle];
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Component get(Zero) const noexcept {
		return 0;
	}

	template <size_t IndexX, size_t IndexY>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, ComponentSwizzle<IndexY> SwizzleY) const noexcept {
		return {get(SwizzleX), get(SwizzleY)};
	}

	template <size_t IndexX>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, Zero) const noexcept {
		return {get(SwizzleX), 0};
	}

	template <size_t IndexY>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> get(Zero, ComponentSwizzle<IndexY> SwizzleY) const noexcept {
		return {0, get(SwizzleY)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> get(Zero, Zero) const noexcept {
		return {};
	}

	template <size_t IndexX, size_t IndexY, size_t IndexZ>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get([[maybe_unused]] ComponentSwizzle<IndexX> SwizzleX, [[maybe_unused]] ComponentSwizzle<IndexY> SwizzleY,
		[[maybe_unused]] ComponentSwizzle<IndexZ> SwizzleZ) const noexcept {
#if GREM_USE_SSE_INTRINSICS
		return Quantity<3, UnitT>::reinterpret(_mm_shuffle_ps(_private_value, _private_value, _MM_SHUFFLE(IndexZ, IndexZ, IndexY, IndexX)));
#else
		return {get(SwizzleX), get(SwizzleY), get(SwizzleZ)};
#endif
	}

	template <size_t IndexX, size_t IndexY>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, ComponentSwizzle<IndexY> SwizzleY, Zero) const noexcept {
		return {get(SwizzleX, SwizzleY), 0};
	}

	template <size_t IndexX, size_t IndexZ>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, Zero, ComponentSwizzle<IndexZ> SwizzleZ) const noexcept {
		return {get(SwizzleX), 0, get(SwizzleZ)};
	}

	template <size_t IndexY, size_t IndexZ>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(Zero, ComponentSwizzle<IndexY> SwizzleY, ComponentSwizzle<IndexZ> SwizzleZ) const noexcept {
		return {0, get(SwizzleY), get(SwizzleZ)};
	}

	template <size_t IndexX>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(ComponentSwizzle<IndexX> SwizzleX, Zero, Zero) const noexcept {
		return {get(SwizzleX), 0, 0};
	}

	template <size_t IndexY>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(Zero, ComponentSwizzle<IndexY> SwizzleY, Zero) const noexcept {
		return {0, get(SwizzleY), 0};
	}

	template <size_t IndexZ>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(Zero, Zero, ComponentSwizzle<IndexZ> SwizzleZ) const noexcept {
		return {0, 0, get(SwizzleZ)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> get(Zero, Zero, Zero) const noexcept {
		return {};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Component getX() const noexcept {
		return get(X);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Component getY() const noexcept {
		return get(Y);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Component getZ() const noexcept {
		return get(Z);
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE ComponentProxy<3, UnitT, Index> operator[](ComponentSwizzle<Index>) noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return *this;
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE ConstComponentProxy<3, UnitT, Index> operator[](ComponentSwizzle<Index>) const noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return *this;
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE ComponentProxy<3, UnitT, Index> operator[](meta::Constant<Index>) noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return *this;
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE ConstComponentProxy<3, UnitT, Index> operator[](meta::Constant<Index>) const noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return *this;
	}

	GREM_ALWAYS_INLINE constexpr operator vec<3, float>() const requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l) {
		return vec<3, float>{static_cast<float>(getX()), static_cast<float>(getY()), static_cast<float>(getZ())};
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, Unitless> in(OtherUnitT) const requires(OtherUnitT::DIMENSION == DIMENSION) {
		if constexpr (OtherUnitT::MagnitudeType::VALUE == MagnitudeType::VALUE) {
			return Quantity<3, Unitless>{_private_value};
		} else {
			constexpr float factor = static_cast<float>(MagnitudeType::VALUE / OtherUnitT::MagnitudeType::VALUE);
			return _private_getUnderlyingValue() * factor;
		}
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<3, OtherUnitT> as(OtherUnitT) const
		requires(OtherUnitT::DIMENSION == DIMENSION && (OtherUnitT::MagnitudeType::IS_ABSOLUTE == MagnitudeType::IS_ABSOLUTE || OtherUnitT::MagnitudeType::IS_ABSOLUTE)) {
		return *this;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 3;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t getMinIndex() const {
		size_t result = 0;
		Component minValue = getX();
		const Component y = getY();
		if (y < minValue) {
			result = 1;
			minValue = y;
		}
		const Component z = getZ();
		if (z < minValue) {
			result = 2;
		}
		return result;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE size_t getMaxIndex() const {
		size_t result = 0;
		Component maxValue = getX();
		const Component y = getY();
		if (y > maxValue) {
			result = 1;
			maxValue = y;
		}
		const Component z = getZ();
		if (z > maxValue) {
			result = 2;
		}
		return result;
	}

private:
	template <size_t OtherN, typename OtherUnitT>
	friend struct Quantity;

	template <size_t OtherN, typename OtherUnitT, size_t Index>
	friend class ConstComponentProxy;

	template <size_t OtherN, typename OtherUnitT, size_t Index>
	friend class ComponentProxy;

#ifdef GREM_USE_SSE_INTRINSICS
	GREM_ALWAYS_INLINE constexpr explicit Quantity(__m128 value)
		: _private_value(value) {}

	GREM_ALWAYS_INLINE constexpr explicit Quantity(vec<3, float> value) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: _private_value{value.x, value.y, value.z, value.z} {}
#else
	GREM_ALWAYS_INLINE constexpr explicit Quantity(vec<3, float> value) requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: _private_value(value) {}
#endif
};

template <typename UnitT>
inline constexpr Quantity<3, UnitT> Quantity<3, UnitT>::MIN{vec<3, float>{Limits<float>::MIN}};

template <typename UnitT>
inline constexpr Quantity<3, UnitT> Quantity<3, UnitT>::MAX{vec<3, float>{Limits<float>::MAX}};

template <typename UnitT>
inline constexpr Quantity<3, UnitT> Quantity<3, UnitT>::SMALLEST_NORMAL{vec<3, float>{Limits<float>::SMALLEST_NORMAL}};

template <typename UnitT>
inline constexpr Quantity<3, UnitT> Quantity<3, UnitT>::SMALLEST_SUBNORMAL{vec<3, float>{Limits<float>::SMALLEST_SUBNORMAL}};

template <typename UnitT>
inline constexpr Quantity<3, UnitT> Quantity<3, UnitT>::MACHINE_EPSILON{vec<3, float>{Limits<float>::MACHINE_EPSILON}};

template <typename UnitT>
inline constexpr Quantity<3, UnitT> Quantity<3, UnitT>::INF{vec<3, float>{Limits<float>::INF}};

template <typename UnitT>
inline constexpr Quantity<3, UnitT> Quantity<3, UnitT>::QUIET_NAN{vec<3, float>{Limits<float>::QUIET_NAN}};

namespace detail {

template <size_t N, typename T>
consteval void derivedFromQuantityTest(const Quantity<N, T>&);

template <typename T>
concept derived_from_quantity = requires(const T t) { derivedFromQuantityTest(t); };

template <size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<N, UnitT> GREM_VECTORCALL toQuantity(Quantity<N, UnitT> value) noexcept {
	return value;
}

template <size_t N, typename UnitT, size_t Index>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, UnitT> GREM_VECTORCALL toQuantity(ConstComponentProxy<N, UnitT, Index> value) noexcept {
	return value;
}

template <size_t N, typename UnitT, size_t Index>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, UnitT> GREM_VECTORCALL toQuantity(ComponentProxy<N, UnitT, Index> value) noexcept {
	return value;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, Unitless> GREM_VECTORCALL toQuantity(float value) noexcept {
	return value;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<2, Unitless> GREM_VECTORCALL toQuantity(vec<2, float> value) noexcept {
	return value;
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<3, Unitless> GREM_VECTORCALL toQuantity(vec<3, float> value) noexcept {
	return value;
}

template <typename Rep, typename Period>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL toQuantity(const DurationBase<Rep, Period>& value) noexcept {
	return Quantity<1, MultipleOf<Seconds, static_cast<long double>(Period::num) / static_cast<long double>(Period::den)>>{value};
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, Seconds> GREM_VECTORCALL toQuantity(Duration value) noexcept {
	return value;
}

GREM_ALWAYS_INLINE constexpr void GREM_VECTORCALL toQuantity(int) noexcept {}

template <typename T>
concept not_void = !std::is_void_v<T>;

template <typename T>
concept quantity_convertible = requires(const T t) {
	{ toQuantity(t) } -> not_void;
};

template <typename T>
inline constexpr size_t quantity_rank_v = decltype(toQuantity(std::declval<T>()))::RANK;

template <typename T>
using unit_type_t = typename decltype(toQuantity(std::declval<T>()))::Unit;

template <typename T>
struct is_quantity_or_component_proxy : std::bool_constant<derived_from_quantity<T>> {};

template <size_t N, typename UnitT, size_t Index>
struct is_quantity_or_component_proxy<ConstComponentProxy<N, UnitT, Index>> : std::true_type {};

template <size_t N, typename UnitT, size_t Index>
struct is_quantity_or_component_proxy<ComponentProxy<N, UnitT, Index>> : std::true_type {};

template <typename T>
inline constexpr bool is_quantity_or_component_proxy_v = is_quantity_or_component_proxy<T>::value;

template <typename T>
concept quantity_unary_operand = is_quantity_or_component_proxy_v<std::remove_cvref_t<T>>;

template <typename T>
concept quantity_binary_operand_a = quantity_convertible<T>;

template <typename T, typename A>
concept quantity_binary_operand_b = quantity_unary_operand<T> || (quantity_unary_operand<A> && quantity_convertible<T>);

template <typename T>
concept quantity_ternary_operand_a = quantity_convertible<T>;

template <typename T, typename A>
concept quantity_ternary_operand_b = quantity_convertible<T>;

template <typename T, typename A, typename B>
concept quantity_ternary_operand_c = quantity_unary_operand<T> || ((quantity_unary_operand<A> || quantity_unary_operand<B>) && quantity_convertible<T>);

} // namespace detail

/**
 * Boolean quantity.
 *
 * Unitless.
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using Mask = std::conditional_t<N == 1, bool, vec<N, bool>>;
using Mask1D = Mask<1>; ///< Boolean quantity in 1-dimensional space. Unitless.
using Mask2D = Mask<2>; ///< Boolean quantity in 2-dimensional space. Unitless.
using Mask3D = Mask<3>; ///< Boolean quantity in 3-dimensional space. Unitless.

/**
 * Angular boolean quantity.
 *
 * Unitless.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularMask = std::conditional_t<N == 3, Mask<N>, Mask<1>>;
using AngularMask2D = AngularMask<2>; ///< Angular boolean quantity in 2-dimensional space. Unitless.
using AngularMask3D = AngularMask<3>; ///< Angular boolean quantity in 2-dimensional space. Unitless.

using grem::clamp;
using grem::max;
using grem::min;
using grem::select;

template <size_t N, typename UnitT, unit OtherUnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator*(Quantity<N, UnitT> value, OtherUnitT) {
	return Quantity<N, Product<UnitT, OtherUnitT>>::reinterpret(value);
}

template <unit OtherUnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator*(detail::quantity_convertible auto value, OtherUnitT unit) {
	return detail::toQuantity(value) * unit;
}

template <unit OtherUnitT, size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator*(OtherUnitT, Quantity<N, UnitT> value) {
	return Quantity<N, Product<OtherUnitT, UnitT>>::reinterpret(value);
}

template <unit OtherUnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator*(OtherUnitT unit, detail::quantity_convertible auto value) {
	return unit * detail::toQuantity(value);
}

template <size_t N, typename UnitT, unit OtherUnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator/(Quantity<N, UnitT> value, OtherUnitT) {
	return Quantity<N, Quotient<UnitT, OtherUnitT>>::reinterpret(value);
}

template <unit OtherUnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator/(detail::quantity_convertible auto value, OtherUnitT unit) {
	return detail::toQuantity(value) / unit;
}

template <unit OtherUnitT, size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator/(OtherUnitT, Quantity<N, UnitT> value) {
	return Quantity<N, Quotient<OtherUnitT, UnitT>>::reinterpret(1.0f / value);
}

template <unit OtherUnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator/(OtherUnitT unit, detail::quantity_convertible auto value) {
	return unit / detail::toQuantity(value);
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator+(A a) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be unary-added because it represents an absolute position.");
	return a;
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator-(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be negated because it represents an absolute position.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N == 3) {
		return Quantity<N, UnitT>::reinterpret(_mm_sub_ps(_mm_setzero_ps(), detail::toQuantity(a)._private_value));
	} else {
		return Quantity<N, UnitT>::reinterpret(-detail::toQuantity(a)._private_value);
	}
#else
	return Quantity<N, UnitT>::reinterpret(-detail::toQuantity(a)._private_value);
#endif
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator+(A a, B b) {
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	using UnitT1 = detail::unit_type_t<A>;
	using UnitT2 = detail::unit_type_t<B>;
	static_assert(detail::can_add<UnitT1, UnitT2>, "Quantities of these units cannot be added.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N1 == 3 && N2 == 3) {
		return Quantity<3, Sum<UnitT1, UnitT2>>::reinterpret(_mm_add_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
	} else {
		return Quantity<grem::max(N1, N2), Sum<UnitT1, UnitT2>>::reinterpret(
			detail::toQuantity(a)._private_getUnderlyingValue() + detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return Quantity<grem::max(N1, N2), Sum<UnitT1, UnitT2>>::reinterpret(detail::toQuantity(a)._private_value + detail::toQuantity(b)._private_value);
#endif
}

template <size_t N, typename UnitT>
GREM_ALWAYS_INLINE constexpr Quantity<N, UnitT>& GREM_VECTORCALL operator+=(Quantity<N, UnitT>& a, detail::quantity_convertible auto b) {
	return a = a + Quantity<N, Relative<UnitT>>{detail::toQuantity(b)};
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator-(A a, B b) {
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	using UnitT1 = detail::unit_type_t<A>;
	using UnitT2 = detail::unit_type_t<B>;
	static_assert(detail::can_subtract<UnitT1, UnitT2>, "Quantities of these units cannot be subtracted.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N1 == 3 && N2 == 3) {
		return Quantity<3, Difference<UnitT1, UnitT2>>::reinterpret(_mm_sub_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
	} else {
		return Quantity<grem::max(N1, N2), Difference<UnitT1, UnitT2>>::reinterpret(
			detail::toQuantity(a)._private_getUnderlyingValue() - detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return Quantity<grem::max(N1, N2), Difference<UnitT1, UnitT2>>::reinterpret(detail::toQuantity(a)._private_value - detail::toQuantity(b)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator-(A a, Zero) requires(!convertible_to<A, float>) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	return Quantity<N, Relative<UnitT>>::reinterpret(detail::toQuantity(a)._private_value);
}

template <detail::quantity_unary_operand B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator-(Zero, B b) requires(!convertible_to<B, float>) {
	return -b;
}

template <size_t N, typename UnitT>
GREM_ALWAYS_INLINE constexpr Quantity<N, UnitT>& GREM_VECTORCALL operator-=(Quantity<N, UnitT>& a, detail::quantity_convertible auto b) {
	return a = a - Quantity<N, Relative<UnitT>>{detail::toQuantity(b)};
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator*(A a, B b) {
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	using UnitT1 = detail::unit_type_t<A>;
	using UnitT2 = detail::unit_type_t<B>;
	static_assert(detail::can_multiply<UnitT1, UnitT2>, "Quantities of these units cannot be multiplied.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N1 == 3 && N2 == 3) {
		return Quantity<3, Product<UnitT1, UnitT2>>::reinterpret(_mm_mul_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
	} else if constexpr (N1 == 1 && N2 == 3) {
		return Quantity<3, Product<UnitT1, UnitT2>>::reinterpret(_mm_mul_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
	} else if constexpr (N1 == 3 && N2 == 1) {
		return Quantity<3, Product<UnitT1, UnitT2>>::reinterpret(_mm_mul_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
	} else {
		return Quantity<grem::max(N1, N2), Product<UnitT1, UnitT2>>::reinterpret(
			detail::toQuantity(a)._private_getUnderlyingValue() * detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return Quantity<grem::max(N1, N2), Product<UnitT1, UnitT2>>::reinterpret(detail::toQuantity(a)._private_value * detail::toQuantity(b)._private_value);
#endif
}

template <size_t N, typename UnitT>
GREM_ALWAYS_INLINE constexpr Quantity<N, UnitT>& GREM_VECTORCALL operator*=(Quantity<N, UnitT>& a, detail::quantity_convertible auto b) {
	return a = a * detail::toQuantity(b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL operator/(A a, B b) {
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	using UnitT1 = detail::unit_type_t<A>;
	using UnitT2 = detail::unit_type_t<B>;
	static_assert(detail::can_divide<UnitT1, UnitT2>, "Quantities of these units cannot be divided.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N1 == 3 && N2 == 3) {
		return Quantity<3, Quotient<UnitT1, UnitT2>>::reinterpret(_mm_div_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
	} else if constexpr (N1 == 1 && N2 == 3) {
		return Quantity<3, Quotient<UnitT1, UnitT2>>::reinterpret(_mm_div_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
	} else if constexpr (N1 == 3 && N2 == 1) {
		return Quantity<3, Quotient<UnitT1, UnitT2>>::reinterpret(_mm_div_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
	} else {
		return Quantity<grem::max(N1, N2), Quotient<UnitT1, UnitT2>>::reinterpret(
			detail::toQuantity(a)._private_getUnderlyingValue() / detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return Quantity<grem::max(N1, N2), Quotient<UnitT1, UnitT2>>::reinterpret(detail::toQuantity(a)._private_value / detail::toQuantity(b)._private_value);
#endif
}

template <size_t N, typename UnitT>
GREM_ALWAYS_INLINE constexpr Quantity<N, UnitT>& GREM_VECTORCALL operator/=(Quantity<N, UnitT>& a, detail::quantity_convertible auto b) {
	return a = a / detail::toQuantity(b);
}

template <size_t N1, typename UnitT1, size_t N2, typename UnitT2>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL min(Quantity<N1, UnitT1> a, Quantity<N2, UnitT2> b) {
	static_assert(UnitT1{} == UnitT2{}, "Quantities of different units cannot be selected between.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N1 == 3 && N2 == 3) {
		return Quantity<3, UnitT1>::reinterpret(_mm_min_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
	} else if constexpr (N1 == 1 && N2 == 3) {
		return Quantity<3, UnitT1>::reinterpret(_mm_min_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
	} else if constexpr (N1 == 3 && N2 == 1) {
		return Quantity<3, UnitT1>::reinterpret(_mm_min_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
	} else {
		return Quantity<grem::max(N1, N2), UnitT1>::reinterpret(
			grem::min(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue()));
	}
#else
	return Quantity<grem::max(N1, N2), UnitT1>::reinterpret(grem::min(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
#endif
}

template <size_t N1, typename UnitT1, size_t N2, typename UnitT2>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL max(Quantity<N1, UnitT1> a, Quantity<N2, UnitT2> b) {
	static_assert(UnitT1{} == UnitT2{}, "Quantities of different units cannot be selected between.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N1 == 3 && N2 == 3) {
		return Quantity<3, UnitT1>::reinterpret(_mm_max_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
	} else if constexpr (N1 == 1 && N2 == 3) {
		return Quantity<3, UnitT1>::reinterpret(_mm_max_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
	} else if constexpr (N1 == 3 && N2 == 1) {
		return Quantity<3, UnitT1>::reinterpret(_mm_max_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
	} else {
		return Quantity<grem::max(N1, N2), UnitT1>::reinterpret(
			grem::max(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue()));
	}
#else
	return Quantity<grem::max(N1, N2), UnitT1>::reinterpret(grem::max(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
#endif
}

template <size_t N1, typename UnitT1, size_t N2, typename UnitT2, size_t N3, typename UnitT3>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL clamp(Quantity<N1, UnitT1> a, Quantity<N2, UnitT2> b, Quantity<N3, UnitT3> c) {
	return min(max(a, b), c);
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL sum(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!detail::unit_type_t<A>::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be added to itself because it represents an absolute position.");
	if constexpr (N == 1) {
		return detail::toQuantity(a);
	} else {
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			constexpr float32_t ZEROS = bit_cast<float32_t>(uint32_t{0});
			constexpr float32_t ONES = bit_cast<float32_t>(uint32_t{0xFFFFFFFF});

			const auto q = detail::toQuantity(a);
			const __m128 y_x_z_z = _mm_shuffle_ps(q._private_value, q._private_value, _MM_SHUFFLE(2, 2, 0, 1));
			const __m128 y_x_0_0 = _mm_and_ps(y_x_z_z, _mm_set_ps(ZEROS, ZEROS, ONES, ONES));
			const __m128 xy_xy_z_z = _mm_add_ps(q._private_value, y_x_0_0);
			const __m128 z_z_0_0 = _mm_movehl_ps(y_x_0_0, xy_xy_z_z);
			const __m128 xyz_xy_z_z = _mm_add_ss(xy_xy_z_z, z_z_0_0);
			return Quantity<1, UnitT>::reinterpret(_mm_cvtss_f32(xyz_xy_z_z));
		} else {
			return Quantity<1, UnitT>::reinterpret(grem::sum(detail::toQuantity(a)._private_value));
		}
#else
		return Quantity<1, UnitT>::reinterpret(grem::sum(detail::toQuantity(a)._private_value));
#endif
	}
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL product(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be multiplied by itself because it represents an absolute position.");
	if constexpr (N == 1) {
		return detail::toQuantity(a);
	} else if constexpr (N == 2) {
		return Quantity<1, Square<UnitT>>::reinterpret(grem::product(detail::toQuantity(a)._private_getUnderlyingValue()));
	} else if constexpr (N == 3) {
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			const auto q = detail::toQuantity(a);
			const __m128 y_x_z_z = _mm_shuffle_ps(q._private_value, q._private_value, _MM_SHUFFLE(2, 2, 0, 1));
			const __m128 y_x_1_1 = _mm_movelh_ps(y_x_z_z, _mm_set1_ps(1.0f));
			const __m128 xy_xy_z_z = _mm_mul_ps(q._private_value, y_x_1_1);
			const __m128 z_z_1_1 = _mm_movehl_ps(y_x_1_1, xy_xy_z_z);
			const __m128 xyz_xy_z_z = _mm_mul_ss(xy_xy_z_z, z_z_1_1);
			return Quantity<1, Cubic<UnitT>>::reinterpret(_mm_cvtss_f32(xyz_xy_z_z));
		} else {
			return Quantity<1, Cubic<UnitT>>::reinterpret(grem::product(detail::toQuantity(a)._private_value));
		}
#else
		return Quantity<1, Cubic<UnitT>>::reinterpret(grem::product(detail::toQuantity(a)._private_value));
#endif
	}
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL minComponent(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	if constexpr (N == 1) {
		return detail::toQuantity(a);
	} else {
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			const auto q = detail::toQuantity(a);
			const auto minOfXY = min(q, q.get(Y, Z, Z));
			return min(minOfXY, q.get(Z, Z, Z)).getX();
		} else {
			return Quantity<1, UnitT>::reinterpret(grem::minComponent(detail::toQuantity(a)._private_value));
		}
#else
		return Quantity<1, UnitT>::reinterpret(grem::minComponent(detail::toQuantity(a)._private_value));
#endif
	}
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL maxComponent(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	if constexpr (N == 1) {
		return detail::toQuantity(a);
	} else {
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			const auto q = detail::toQuantity(a);
			const auto maxOfXY = max(q, q.get(Y, Z, Z));
			return max(maxOfXY, q.get(Z, Z, Z)).getX();
		} else {
			return Quantity<1, UnitT>::reinterpret(grem::maxComponent(detail::toQuantity(a)._private_value));
		}
#else
		return Quantity<1, UnitT>::reinterpret(grem::maxComponent(detail::toQuantity(a)._private_value));
#endif
	}
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE auto GREM_VECTORCALL isfinite(A a) {
	return grem::isfinite(detail::toQuantity(a)._private_getUnderlyingValue());
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE auto GREM_VECTORCALL isinf(A a) {
	return grem::isinf(detail::toQuantity(a)._private_getUnderlyingValue());
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE auto GREM_VECTORCALL isnan(A a) {
#ifdef GREM_USE_SSE_INTRINSICS
	constexpr size_t N = detail::quantity_rank_v<A>;
	if constexpr (N == 3) {
		const auto q = detail::toQuantity(a);
		const int mask = _mm_movemask_ps(_mm_cmpunord_ps(q._private_value, q._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else {
		return grem::isnan(detail::toQuantity(a)._private_value);
	}
#else
	return grem::isnan(detail::toQuantity(a)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE auto GREM_VECTORCALL signbit(A a) {
#ifdef GREM_USE_SSE_INTRINSICS
	constexpr size_t N = detail::quantity_rank_v<A>;
	if constexpr (N == 3) {
		const int mask = _mm_movemask_ps(detail::toQuantity(a)._private_value);
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else {
		return grem::signbit(detail::toQuantity(a)._private_value);
	}
#else
	return grem::signbit(detail::toQuantity(a)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL sign(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N == 3) {
		return Quantity<3, Unitless>::reinterpret(_mm_or_ps(_mm_and_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(-1.0f)), _mm_set1_ps(1.0f)));
	} else {
		return Quantity<N, Unitless>{grem::sign(detail::toQuantity(a)._private_value)};
	}
#else
	return Quantity<N, Unitless>{grem::sign(detail::toQuantity(a)._private_value)};
#endif
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE auto GREM_VECTORCALL fmod(A a, B b) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot wrap each other.");
	return Quantity<N, UnitT>::reinterpret(grem::fmod(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue()));
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE auto GREM_VECTORCALL wrap(A a, B b) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot wrap each other.");
	return Quantity<N, UnitT>::reinterpret(grem::wrap(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE auto GREM_VECTORCALL abs(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot have its absolute value taken because it represents an absolute position.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N == 3) {
		const auto q = detail::toQuantity(a);
		return max(-q, q);
	} else {
		return Quantity<N, UnitT>::reinterpret(grem::abs(detail::toQuantity(a)._private_value));
	}
#else
	return Quantity<N, UnitT>::reinterpret(grem::abs(detail::toQuantity(a)._private_value));
#endif
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL dot(A a, B b) {
	return sum(a * b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL cross(A a, B b) {
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	using UnitT1 = detail::unit_type_t<A>;
	using UnitT2 = detail::unit_type_t<B>;
	static_assert(detail::can_multiply<UnitT1, UnitT2>, "Quantities of these units cannot be multiplied.");
	if constexpr (N1 == 1 && N2 == 1) {
		return Quantity<1, Product<UnitT1, UnitT2>>{};
	} else if constexpr (N1 == 2 && N2 == 1) {
		const vec<2, float> v = detail::toQuantity(a)._private_getUnderlyingValue();
		return Quantity<2, Product<UnitT1, UnitT2>>::reinterpret(vec<2, float>{v.y, -v.x} * detail::toQuantity(b)._private_getUnderlyingValue());
	} else if constexpr (N1 == 1 && N2 == 2) {
		const vec<2, float> v = detail::toQuantity(b)._private_getUnderlyingValue();
		return Quantity<2, Product<UnitT1, UnitT2>>::reinterpret(detail::toQuantity(a)._private_getUnderlyingValue() * vec<2, float>{-v.y, v.x});
	} else if constexpr (N1 == 2 && N2 == 2) {
		return Quantity<1, Product<UnitT1, UnitT2>>::reinterpret(
			grem::cross(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue()));
	} else if constexpr (N1 == 3 && N2 == 3) {
#ifdef GREM_USE_SSE_INTRINSICS
		const auto qA = detail::toQuantity(a);
		const auto qB = detail::toQuantity(b);
		return (qB.get(Y, Z, X) * qA - qA.get(Y, Z, X) * qB).get(Y, Z, X);
#else
		return Quantity<3, Product<UnitT1, UnitT2>>::reinterpret(grem::cross(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
#endif
	} else {
		return Quantity<grem::max(N1, N2), Product<UnitT1, UnitT2>>::reinterpret(
			grem::cross(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue()));
	}
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL sqrt(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot have its square root taken because it represents an absolute position.");
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N == 3) {
		return Quantity<3, Sqrt<UnitT>>::reinterpret(_mm_sqrt_ps(detail::toQuantity(a)._private_value));
	} else {
		return Quantity<N, Sqrt<UnitT>>::reinterpret(grem::sqrt(detail::toQuantity(a)._private_value));
	}
#else
	return Quantity<N, Sqrt<UnitT>>::reinterpret(grem::sqrt(detail::toQuantity(a)._private_value));
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL inversesqrt(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot have its reciprocal square root taken because it represents an absolute position.");
	return Quantity<N, Reciprocal<Sqrt<UnitT>>>::reinterpret(grem::inversesqrt(detail::toQuantity(a)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL length2(A a) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot have its square length taken because it represents an absolute position.");
	return dot(a, a);
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL length(A a) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot have its length taken because it represents an absolute position.");
	return sqrt(length2(a));
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL distance(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of these units cannot have their distance measured.");
	return length(a - b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL distance2(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of these units cannot have their squared distance measured.");
	return length2(a - b);
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL getAngle(A a) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot have its angle taken because it represents an absolute position.");
	return Quantity<1, Radians>::reinterpret(grem::getAngle(detail::toQuantity(a)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL tryGetAngle(A a) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot have its angle taken because it represents an absolute position.");
	if (const auto result = grem::tryGetAngle(detail::toQuantity(a)._private_getUnderlyingValue())) {
		return Optional<Quantity<1, Radians>>{Quantity<1, Radians>::reinterpret(*result)};
	}
	return Optional<Quantity<1, Radians>>{};
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL cos(A a) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(UnitT::DIMENSION == ANGLE, "Quantity of this unit cannot have its cosine taken because it is not an angle.");
	return Quantity<1, Unitless>::reinterpret(grem::cos(detail::toQuantity(a).in(RADIANS)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL sin(A a) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(UnitT::DIMENSION == ANGLE, "Quantity of this unit cannot have its cosine taken because it is not an angle.");
	return Quantity<1, Unitless>::reinterpret(grem::sin(detail::toQuantity(a).in(RADIANS)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL tan(A a) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(UnitT::DIMENSION == ANGLE, "Quantity of this unit cannot have its cosine taken because it is not an angle.");
	return Quantity<1, Unitless>::reinterpret(grem::tan(detail::toQuantity(a).in(RADIANS)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL acos(A a) {
	return Quantity<1, Radians>::reinterpret(grem::acos(detail::toQuantity(a)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL asin(A a) {
	return Quantity<1, Radians>::reinterpret(grem::asin(detail::toQuantity(a)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL atan(A a) {
	return Quantity<1, Radians>::reinterpret(grem::atan(detail::toQuantity(a)._private_getUnderlyingValue()));
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL atan2(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be passed to atan2.");
	return Quantity<1, Radians>::reinterpret(grem::atan2(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue()));
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL tryDivide(A a, B b) {
	using R = decltype(a / b);
	using UnitT1 = detail::unit_type_t<A>;
	using UnitT2 = detail::unit_type_t<B>;
	static_assert(detail::can_divide<UnitT1, UnitT2>, "Quantities of these units cannot be divided.");
	if (const auto quotient = grem::tryDivide(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue())) {
		return Optional<R>{R::reinterpret(*quotient)};
	}
	return Optional<R>{};
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL reflect(A a, B normal) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be reflected because it represents an absolute position.");
	static_assert(detail::unit_type_t<B>{} == UNITLESS, "Quantity of this unit cannot be reflected against.");
	return Quantity<N, UnitT>::reinterpret(grem::reflect(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(normal)._private_getUnderlyingValue()));
}

template <detail::quantity_ternary_operand_a A, detail::quantity_ternary_operand_b<A> B, detail::quantity_ternary_operand_c<A, B> C>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL mix(A a, B b, C alpha) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::quantity_rank_v<B> == N, "Quantities of different ranks cannot be interpolated.");
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be interpolated.");
	static_assert(detail::unit_type_t<C>{} == UNITLESS, "Alpha quantity must be unitless.");
	return Quantity<N, UnitT>::reinterpret((a - 0) * (C{1.0f} - alpha) + (b - 0) * alpha);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL step(A edge, B value) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::quantity_rank_v<B> == N, "Quantities of different ranks cannot be passed to step.");
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be passed to step.");
	return Quantity<N, Unitless>::reinterpret(grem::step(detail::toQuantity(edge)._private_getUnderlyingValue(), detail::toQuantity(value)._private_getUnderlyingValue()));
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL select(auto condition, A ifTrue, B ifFalse) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::quantity_rank_v<B> == N, "Quantities of different ranks cannot be selected between.");
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be selected between.");
	if constexpr (N == 1) {
		return Quantity<1, UnitT>::reinterpret(
			grem::select(vec<1, bool>{condition}, vec<1, float>{detail::toQuantity(ifTrue)._private_value}, vec<1, float>{detail::toQuantity(ifFalse)._private_value}).x);
	} else {
#ifdef GREM_USE_SSE_INTRINSICS
		if constexpr (N == 3) {
			const bvec3 conditions{condition};
			const __m128 mask = _mm_castsi128_ps(_mm_sub_epi32(
				_mm_set_epi32(static_cast<int>(conditions.z), static_cast<int>(conditions.z), static_cast<int>(conditions.y), static_cast<int>(conditions.x)), _mm_set1_epi32(1)));
			return Quantity<3, UnitT>::reinterpret(
				_mm_or_ps(_mm_and_ps(mask, detail::toQuantity(ifFalse)._private_value), _mm_andnot_ps(mask, detail::toQuantity(ifTrue)._private_value)));
		} else {
			return Quantity<N, UnitT>::reinterpret(grem::select(condition, detail::toQuantity(ifTrue)._private_value, detail::toQuantity(ifFalse)._private_value));
		}
#else
		return Quantity<N, UnitT>::reinterpret(grem::select(condition, detail::toQuantity(ifTrue)._private_value, detail::toQuantity(ifFalse)._private_value));
#endif
	}
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B, typename Frequency, typename Duration>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL expDecay(A value, B targetValue, Frequency decayRate, Duration decayTime) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::quantity_rank_v<B> == N, "Cannot decay towards a quantity of a different rank.");
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Cannot decay towards a quantity of a different unit.");
	return Quantity<N, UnitT>::reinterpret(
		grem::expDecay(detail::toQuantity(value)._private_getUnderlyingValue(), detail::toQuantity(targetValue)._private_getUnderlyingValue(), decayRate, decayTime));
}

template <detail::quantity_unary_operand A, typename Frequency, typename Duration>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL expDecay(A value, Frequency decayRate, Duration decayTime) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	return Quantity<N, UnitT>::reinterpret(grem::expDecay(detail::toQuantity(value)._private_getUnderlyingValue(), decayRate, decayTime));
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL midpoint(A a, B b) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::quantity_rank_v<B> == N, "Cannot take the midpoint of two quantities of different ranks.");
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Cannot take the midpoint of two quantities of different units.");
	return Quantity<N, UnitT>::reinterpret(((a - 0) + (b - 0)) * 0.5f);
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL round(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	return Quantity<N, UnitT>::reinterpret(grem::round(detail::toQuantity(a)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL floor(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	return Quantity<N, UnitT>::reinterpret(grem::floor(detail::toQuantity(a)._private_getUnderlyingValue()));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL ceil(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	return Quantity<N, UnitT>::reinterpret(grem::ceil(detail::toQuantity(a)._private_getUnderlyingValue()));
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL equal(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be compared for equality.");
#ifdef GREM_USE_SSE_INTRINSICS
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	if constexpr (N1 == 3 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmpeq_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 1 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmpeq_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 3 && N2 == 1) {
		const int mask = _mm_movemask_ps(_mm_cmpeq_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else {
		return grem::equal(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return grem::equal(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL equal(A a, Zero) {
	return equal(a, A{});
}

template <detail::quantity_unary_operand B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL equal(Zero, B b) {
	return equal(B{}, b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL notEqual(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be compared for inequality.");
#ifdef GREM_USE_SSE_INTRINSICS
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	if constexpr (N1 == 3 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmpne_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 1 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmpne_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 3 && N2 == 1) {
		const int mask = _mm_movemask_ps(_mm_cmpne_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else {
		return grem::notEqual(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return grem::notEqual(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL notEqual(A a, Zero) {
	return notEqual(a, A{});
}

template <detail::quantity_unary_operand B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL notEqual(Zero, B b) {
	return notEqual(B{}, b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL lessThan(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be compared.");
#ifdef GREM_USE_SSE_INTRINSICS
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	if constexpr (N1 == 3 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmplt_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 1 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmplt_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 3 && N2 == 1) {
		const int mask = _mm_movemask_ps(_mm_cmplt_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else {
		return grem::lessThan(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return grem::lessThan(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL lessThan(A a, Zero) {
	return lessThan(a, A{});
}

template <detail::quantity_unary_operand B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL lessThan(Zero, B b) {
	return lessThan(B{}, b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL lessThanEqual(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be compared.");
#ifdef GREM_USE_SSE_INTRINSICS
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	if constexpr (N1 == 3 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmple_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 1 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmple_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 3 && N2 == 1) {
		const int mask = _mm_movemask_ps(_mm_cmple_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else {
		return grem::lessThanEqual(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return grem::lessThanEqual(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL lessThanEqual(A a, Zero) {
	return lessThanEqual(a, A{});
}

template <detail::quantity_unary_operand B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL lessThanEqual(Zero, B b) {
	return lessThanEqual(B{}, b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL greaterThan(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be compared.");
#ifdef GREM_USE_SSE_INTRINSICS
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	if constexpr (N1 == 3 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmpgt_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 1 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmpgt_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 3 && N2 == 1) {
		const int mask = _mm_movemask_ps(_mm_cmpgt_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else {
		return grem::greaterThan(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return grem::greaterThan(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL greaterThan(A a, Zero) {
	return greaterThan(a, A{});
}

template <detail::quantity_unary_operand B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL greaterThan(Zero, B b) {
	return greaterThan(B{}, b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL greaterThanEqual(A a, B b) {
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be compared.");
#ifdef GREM_USE_SSE_INTRINSICS
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	if constexpr (N1 == 3 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmpge_ps(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 1 && N2 == 3) {
		const int mask = _mm_movemask_ps(_mm_cmpge_ps(_mm_set1_ps(detail::toQuantity(a)._private_value), detail::toQuantity(b)._private_value));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else if constexpr (N1 == 3 && N2 == 1) {
		const int mask = _mm_movemask_ps(_mm_cmpge_ps(detail::toQuantity(a)._private_value, _mm_set1_ps(detail::toQuantity(b)._private_value)));
		return bvec3{static_cast<bool>(mask & 0b001), static_cast<bool>(mask & 0b010), static_cast<bool>(mask & 0b100)};
	} else {
		return grem::greaterThanEqual(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue());
	}
#else
	return grem::greaterThanEqual(detail::toQuantity(a)._private_value, detail::toQuantity(b)._private_value);
#endif
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL greaterThanEqual(A a, Zero) {
	return greaterThanEqual(a, A{});
}

template <detail::quantity_unary_operand B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL greaterThanEqual(Zero, B b) {
	return greaterThanEqual(B{}, b);
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL clampLength(A value, B maxLength) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(detail::unit_type_t<B>{} == UnitT{}, "Quantities of different units cannot be clamped.");
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot have its length clamped because it represents an absolute position.");
	return Quantity<N, UnitT>::reinterpret(grem::clampLength(detail::toQuantity(value)._private_getUnderlyingValue(), detail::toQuantity(maxLength)._private_getUnderlyingValue()));
}

template <size_t N, typename UnitT>
struct TensorQuantity {
	using Component = Quantity<N, UnitT>;
	using Unit = UnitT;
	using DimensionType = typename Unit::DimensionType;
	using MagnitudeType = typename Unit::MagnitudeType;
	static constexpr size_t RANK = N;
	static constexpr Unit UNIT{};
	static constexpr DimensionType DIMENSION{};
	static constexpr MagnitudeType MAGNITUDE{};

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr TensorQuantity reinterpret(const TensorQuantity<N, OtherUnitT>& other) {
		return TensorQuantity{other._private_value};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr TensorQuantity reinterpret(const mat<N, N, float>& value) {
		return TensorQuantity{value};
	}

	mat<N, N, float> _private_value{1.0f};

	constexpr TensorQuantity() noexcept = default;

	constexpr TensorQuantity(const mat<N, N, float>& value) noexcept requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l)
		: _private_value(value) {}

	constexpr TensorQuantity(Quantity<1, UnitT> value) noexcept
		: _private_value{value._private_value} {}

	constexpr TensorQuantity(Component x, Component y) noexcept requires(N == 2)
		: _private_value{x._private_getUnderlyingValue(), y._private_getUnderlyingValue()} {}

	constexpr TensorQuantity(Component x, Component y, Component z) noexcept requires(N == 3)
		: _private_value{x._private_getUnderlyingValue(), y._private_getUnderlyingValue(), z._private_getUnderlyingValue()} {}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr operator const mat<N, N, float>&() const requires(DIMENSION == DIMENSIONLESS && MagnitudeType::VALUE == 1.0l) {
		return _private_value;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<N, Relative<UnitT>> getScale() const noexcept {
		if constexpr (N == 2) {
			return {length(Component::reinterpret(_private_value[0])), length(Component::reinterpret(_private_value[1]))};
		} else if constexpr (N == 3) {
			return {length(Component::reinterpret(_private_value[0])), length(Component::reinterpret(_private_value[1])), length(Component::reinterpret(_private_value[2]))};
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<N, Square<Relative<UnitT>>> getScale2() const noexcept {
		if constexpr (N == 2) {
			return {length2(Component::reinterpret(_private_value[0])), length2(Component::reinterpret(_private_value[1]))};
		} else if constexpr (N == 3) {
			return {length2(Component::reinterpret(_private_value[0])), length2(Component::reinterpret(_private_value[1])), length2(Component::reinterpret(_private_value[2]))};
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const TensorQuantity& other) const noexcept {
		return _private_value == other._private_value;
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const TensorQuantity<N, OtherUnitT>& other) const noexcept requires(UnitT{} == OtherUnitT{}) {
		return _private_value == other._private_value;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Zero) const noexcept {
		return *this == TensorQuantity{mat<N, N, float>{0.0f}};
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component operator[](ComponentSwizzle<Index>) noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return Component::reinterpret(_private_value[Index]);
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component operator[](ComponentSwizzle<Index>) const noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return Component::reinterpret(_private_value[Index]);
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component operator[](meta::Constant<Index>) noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return Component::reinterpret(_private_value[Index]);
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component operator[](meta::Constant<Index>) const noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return Component::reinterpret(_private_value[Index]);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return N;
	}

private:
	constexpr explicit TensorQuantity(const mat<N, N, float>& value) noexcept requires(DIMENSION != DIMENSIONLESS || MagnitudeType::VALUE != 1.0l)
		: _private_value(value) {}
};

template <size_t N, typename UnitT>
requires(N == 2 || N == 3) //
using AngularTensorQuantity = std::conditional_t<N == 3, TensorQuantity<3, UnitT>, Quantity<1, UnitT>>;

template <size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto inverse(Quantity<N, UnitT> a) {
	return Quantity<N, Reciprocal<UnitT>>::reinterpret(1.0f / a._private_getUnderlyingValue());
}

template <size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto inverse(const TensorQuantity<N, UnitT>& a) {
	return TensorQuantity<N, Reciprocal<UnitT>>::reinterpret(grem::inverse(a._private_value));
}

template <size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto transpose(const TensorQuantity<N, UnitT>& a) {
	return TensorQuantity<N, Reciprocal<UnitT>>::reinterpret(grem::transpose(a._private_value));
}

template <size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto inverseTranspose(const TensorQuantity<N, UnitT>& a) {
	return TensorQuantity<N, UnitT>::reinterpret(grem::inverseTranspose(a._private_value));
}

template <size_t N, typename UnitT1, typename UnitT2>
[[nodiscard]] GREM_ALWAYS_INLINE auto operator+(const TensorQuantity<N, UnitT1>& a, const TensorQuantity<N, UnitT2>& b) {
	static_assert(detail::can_add<UnitT1, UnitT2>, "Quantities of these units cannot be added.");
	return TensorQuantity<N, Sum<UnitT1, UnitT2>>::reinterpret(a._private_value + b._private_value);
}

template <size_t N, typename UnitT1, typename UnitT2>
[[nodiscard]] GREM_ALWAYS_INLINE auto operator-(const TensorQuantity<N, UnitT1>& a, const TensorQuantity<N, UnitT2>& b) {
	static_assert(detail::can_subtract<UnitT1, UnitT2>, "Quantities of these units cannot be subtracted.");
	return TensorQuantity<N, Difference<UnitT1, UnitT2>>::reinterpret(a._private_value - b._private_value);
}

template <size_t N, typename UnitT1, typename UnitT2>
[[nodiscard]] GREM_ALWAYS_INLINE auto operator*(const TensorQuantity<N, UnitT1>& a, const TensorQuantity<N, UnitT2>& b) {
	static_assert(detail::can_multiply<UnitT1, UnitT2>, "Quantities of these units cannot be multiplied.");
	return TensorQuantity<N, Product<UnitT1, UnitT2>>::reinterpret(a._private_value * b._private_value);
}

template <size_t N1, typename UnitT1, detail::quantity_convertible B>
[[nodiscard]] GREM_ALWAYS_INLINE auto operator*(const TensorQuantity<N1, UnitT1>& a, B b) {
	constexpr size_t N2 = detail::quantity_rank_v<B>;
	using UnitT2 = detail::unit_type_t<B>;
	static_assert(detail::can_multiply<UnitT1, UnitT2>, "Quantities of these units cannot be multiplied.");
	return Quantity<grem::max(N1, N2), Product<UnitT1, UnitT2>>::reinterpret(a._private_value * detail::toQuantity(b)._private_getUnderlyingValue());
}

template <size_t N2, typename UnitT2, detail::quantity_convertible A>
[[nodiscard]] GREM_ALWAYS_INLINE auto operator*(A a, const TensorQuantity<N2, UnitT2>& b) {
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	using UnitT1 = detail::unit_type_t<A>;
	static_assert(detail::can_multiply<UnitT1, UnitT2>, "Quantities of these units cannot be multiplied.");
	return Quantity<grem::max(N1, N2), Product<UnitT1, UnitT2>>::reinterpret(detail::toQuantity(a)._private_getUnderlyingValue() * b._private_value);
}

template <size_t N, typename UnitT1, typename UnitT2>
[[nodiscard]] GREM_ALWAYS_INLINE auto dot(const TensorQuantity<N, UnitT1>& a, const TensorQuantity<N, UnitT2>& b) {
	static_assert(detail::can_multiply<UnitT1, UnitT2>, "Quantities of these units cannot be multiplied.");
	if constexpr (N == 2) {
		return Quantity<2, Product<UnitT1, UnitT2>>{
			dot(a[X], b[X]),
			dot(a[Y], b[Y]),
		};
	} else if constexpr (N == 3) {
		return Quantity<3, Product<UnitT1, UnitT2>>{
			dot(a[X], b[X]),
			dot(a[Y], b[Y]),
			dot(a[Z], b[Z]),
		};
	}
}

template <detail::quantity_convertible A, size_t N2, typename UnitT2>
[[nodiscard]] GREM_ALWAYS_INLINE auto cross(A a, const TensorQuantity<N2, UnitT2>& b) {
	constexpr size_t N1 = detail::quantity_rank_v<A>;
	using UnitT1 = detail::unit_type_t<A>;
	static_assert(detail::can_multiply<UnitT1, UnitT2>, "Quantities of these units cannot be multiplied.");
	if constexpr (N1 == 2 && N2 == 2) {
		return length(Quantity<2, Product<UnitT1, UnitT2>>{
			cross(a, b[X]),
			cross(a, b[Y]),
		});
	} else if constexpr (N1 == 3 && N2 == 3) {
		return TensorQuantity<3, Product<UnitT1, UnitT2>>{
			cross(a, b[X]),
			cross(a, b[Y]),
			cross(a, b[Z]),
		};
	}
}

template <size_t N, typename UnitT>
[[nodiscard]] decltype(auto) getNormalComponent(Quantity<N, UnitT>& v) {
	if constexpr (N == 1) {
		return v;
	} else if constexpr (N == 2) {
		return v[Y];
	} else if constexpr (N == 3) {
		return v[Z];
	}
}

template <size_t N, typename UnitT>
[[nodiscard]] decltype(auto) getNormalComponent(const Quantity<N, UnitT>& v) {
	if constexpr (N == 1) {
		return v;
	} else if constexpr (N == 2) {
		return v[Y];
	} else if constexpr (N == 3) {
		return v[Z];
	}
}

template <size_t N, typename UnitT>
[[nodiscard]] auto getTangentialComponent(const Quantity<N, UnitT>& v) {
	if constexpr (N == 1) {
		return v;
	} else if constexpr (N == 2) {
		return v.getX();
	} else if constexpr (N == 3) {
		return v.get(X, Y);
	}
}

/**
 * Dimensionless scale.
 *
 * Unitless.
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using Scale = Quantity<N, Unitless>;
using Scale1D = Scale<1>;    ///< Dimensionless scale in 1-dimensional space. Unitless.
using Scale2D = Scale<2>;    ///< Dimensionless scale in 2-dimensional space. Unitless.
using Scale3D = Scale<3>;    ///< Dimensionless scale in 3-dimensional space. Unitless.
using Coefficient = Scale1D; ///< Unitless scalar quantity.

/**
 * Basis matrix.
 *
 * Unitless.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using Basis = TensorQuantity<N, Unitless>;
using Basis2D = Basis<2>; ///< Basis matrix in 2-dimensional space. Unitless.
using Basis3D = Basis<3>; ///< Basis matrix in 3-dimensional space. Unitless.

/**
 * Angular dimensionless scale.
 *
 * Unitless.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularScale = AngularQuantity<N, Unitless>;
using AngularScale2D = AngularScale<2>; ///< Angular dimensionless scale in 2-dimensional space. Unitless.
using AngularScale3D = AngularScale<3>; ///< Angular dimensionless scale in 3-dimensional space. Unitless.

/**
 * Angle quantity.
 *
 * Unit: Radians.
 */
using Angle = Quantity<1, Radians>;

/**
 * Pi constant.
 */
inline constexpr Angle PI{numbers::PI};

/**
 * %Absolute angles in world space.
 *
 * Unit: Radians.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using Angles = AngularQuantity<N, Absolute<Radians>>;

/**
 * %Absolute angle in 2-dimensional space.
 *
 * Unit: Radians.
 */
using Roll = Angles<2>;

/**
 * %Absolute angles in 3-dimensional space without roll.
 *
 * Unit: Radians.
 */
using PitchYaw = Quantity<2, Absolute<Radians>>;

/**
 * %Absolute angles in 3-dimensional space.
 *
 * Unit: Radians.
 */
using PitchYawRoll = Angles<3>;

/**
 * %Relative rotation of roll.
 *
 * Unit: Radians.
 */
using RollRotation = Angle;

/**
 * %Relative rotations of pitch and yaw.
 *
 * Unit: Radians.
 */
using PitchYawRotations = Quantity<2, Radians>;

/**
 * %Relative rotations of pitch, yaw and roll.
 *
 * Unit: Radians.
 */
using PitchYawRollRotations = Quantity<3, Radians>;

/**
 * %Relative rotation quantity.
 *
 * Unit: Radians.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using Rotation = AngularQuantity<N, Radians>;
using Rotation2D = Rotation<2>; ///< %Relative rotation in 2-dimensional space. Unit: Radians.
using Rotation3D = Rotation<3>; ///< %Relative rotation in 3-dimensional space. Unit: Radians.

/**
 * Normalized unit vector.
 *
 * Unitless.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct Direction : Scale<N> {
	static_assert(N == 2 || N == 3);

	using typename Scale<N>::Component;
	using typename Scale<N>::Unit;
	using typename Scale<N>::DimensionType;
	using typename Scale<N>::MagnitudeType;
	using Scale<N>::RANK;
	using Scale<N>::UNIT;
	using Scale<N>::DIMENSION;
	using Scale<N>::MAGNITUDE;

	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr Direction reinterpret(auto other) {
		return Direction{Scale<N>::reinterpret(other)};
	}

	GREM_ALWAYS_INLINE constexpr Direction() noexcept
		: Scale<N>([]() -> Scale<N> {
			if constexpr (N == 3) {
				return {0.0f, 0.0f, -1.0f};
			} else {
				return {1.0f, 0.0f};
			}
		}()) {}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Direction& other) const noexcept {
		return static_cast<const Scale<N>&>(*this) == static_cast<const Scale<N>&>(other);
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Quantity<2, OtherUnitT>& other) const noexcept requires(UNIT == OtherUnitT{}) {
		return static_cast<const Scale<N>&>(*this) == other;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Zero other) const noexcept {
		return static_cast<const Scale<N>&>(*this) == other;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Direction GREM_VECTORCALL operator-() const {
		return Direction{-static_cast<const Scale<N>&>(*this)};
	}

private:
	GREM_ALWAYS_INLINE constexpr explicit Direction(Scale<N> scale)
		: Scale<N>(scale) {}
};
using Direction2D = Direction<2>; ///< Normalized unit vector in 2-dimensional space. Unitless.
using Direction3D = Direction<3>; ///< Normalized unit vector in 3-dimensional space. Unitless.

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL flipSignIf(A a, bool b) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
#ifdef GREM_USE_SSE_INTRINSICS
	if constexpr (N == 3) {
		return Quantity<N, UnitT>::reinterpret(_mm_xor_ps(_mm_set1_ps(bit_cast<float32_t>(static_cast<uint32_t>(b) << 31)), detail::toQuantity(a)._private_value));
	} else {
		return Quantity<N, UnitT>::reinterpret(grem::flipSignIf(detail::toQuantity(a)._private_value, b));
	}
#else
	return Quantity<N, UnitT>::reinterpret(grem::flipSignIf(detail::toQuantity(a)._private_value, b));
#endif
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL flipSignIf(Direction<N> a, bool b) {
	return Direction<N>::reinterpret(flipSignIf(static_cast<const Scale<N>&>(a), b));
}

template <detail::quantity_binary_operand_a A, detail::quantity_binary_operand_b<A> B>
[[nodiscard]] GREM_ALWAYS_INLINE auto GREM_VECTORCALL copysign(A a, B b) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	return Quantity<N, UnitT>::reinterpret(grem::copysign(detail::toQuantity(a)._private_getUnderlyingValue(), detail::toQuantity(b)._private_getUnderlyingValue()));
}

template <size_t N, detail::quantity_binary_operand_b<Direction<N>> B>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL copysign(Direction<N> a, B b) {
	return Direction<N>::reinterpret(copysign(static_cast<const Scale<N>&>(a), b));
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL normalize(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be normalized because it represents an absolute position.");
	const Scale<N> scale = Scale<N>::reinterpret(a);
	return Direction<N>::reinterpret(scale / sqrt(length2(scale)));
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL normalize(Direction<N> a) {
	return a;
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL tryNormalize(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be normalized because it represents an absolute position.");
	const Scale<N> scale = Scale<N>::reinterpret(a);
	const Scale1D lengthSquared = length2(scale);
	if (lengthSquared > length2(Scale1D::MACHINE_EPSILON)) {
		return Optional<Direction<N>>{Direction<N>::reinterpret(scale / sqrt(lengthSquared))};
	}
	return Optional<Direction<N>>{};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto GREM_VECTORCALL tryNormalize(Direction<N> a) {
	return Optional<Direction<N>>{a};
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto rotate90DegreesCounterclockwise(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(N == 2, "Quantity cannot be rotated around an implicit axis because it is not 2D.");
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be rotated because it represents an absolute position.");
	return Quantity<2, UnitT>{-a.getY(), a.getX()};
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto rotate90DegreesCounterclockwise(Direction2D a) {
	return Direction2D::reinterpret(Scale2D{-a._private_value.y, a._private_value.x});
}

template <detail::quantity_unary_operand A>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto rotate90DegreesClockwise(A a) {
	constexpr size_t N = detail::quantity_rank_v<A>;
	using UnitT = detail::unit_type_t<A>;
	static_assert(N == 2, "Quantity cannot be rotated around an implicit axis because it is not 2D.");
	static_assert(!UnitT::MagnitudeType::IS_ABSOLUTE, "Quantity of this unit cannot be rotated because it represents an absolute position.");
	return Quantity<2, UnitT>{a.getY(), -a.getX()};
}

[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto rotate90DegreesClockwise(Direction2D a) {
	return Direction2D::reinterpret(Scale2D{a._private_value.y, -a._private_value.x});
}

/**
 * Basis whose columns are all unit vectors that are orthogonal to each other.
 *
 * Unitless.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct OrthonormalBasis : Basis<N> {
	static_assert(N == 2 || N == 3);

	using Component = Direction<N>;
	using typename Basis<N>::Unit;
	using typename Basis<N>::DimensionType;
	using typename Basis<N>::MagnitudeType;
	using Basis<N>::RANK;
	using Basis<N>::UNIT;
	using Basis<N>::DIMENSION;
	using Basis<N>::MAGNITUDE;

	[[nodiscard]] GREM_ALWAYS_INLINE static constexpr OrthonormalBasis reinterpret(const auto& value) {
		return OrthonormalBasis{Basis<N>::reinterpret(value)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const OrthonormalBasis& other) const noexcept {
		return static_cast<const Basis<N>&>(*this) == static_cast<const Basis<N>&>(other);
	}

	template <typename OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const TensorQuantity<N, OtherUnitT>& other) const noexcept requires(UNIT == OtherUnitT{}) {
		return static_cast<const Basis<N>&>(*this) == other;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(Zero other) const noexcept {
		return static_cast<const Basis<N>&>(*this) == other;
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component operator[](ComponentSwizzle<Index>) noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return Component::reinterpret(this->_private_value[Index]);
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component operator[](ComponentSwizzle<Index>) const noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return Component::reinterpret(this->_private_value[Index]);
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component operator[](meta::Constant<Index>) noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return Component::reinterpret(this->_private_value[Index]);
	}

	template <size_t Index>
	[[nodiscard]] GREM_ALWAYS_INLINE Component operator[](meta::Constant<Index>) const noexcept {
		static_assert(Index < RANK, "Swizzle component index out of range.");
		return Component::reinterpret(this->_private_value[Index]);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr OrthonormalBasis GREM_VECTORCALL operator-() const {
		return OrthonormalBasis{-static_cast<const Basis<N>&>(*this)};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE OrthonormalBasis GREM_VECTORCALL operator*(const OrthonormalBasis& other) const {
		return OrthonormalBasis::reinterpret(static_cast<const Basis<N>&>(*this) * static_cast<const Basis<N>&>(other));
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Direction<N> GREM_VECTORCALL operator*(Direction<N> direction) const {
		return Direction<N>::reinterpret(static_cast<const Basis<N>&>(*this) * static_cast<const Scale<N>&>(direction));
	}

private:
	GREM_ALWAYS_INLINE constexpr explicit OrthonormalBasis(const Basis<N>& basis)
		: Basis<N>(basis) {}
};
using OrthonormalBasis2D = OrthonormalBasis<2>; ///< Basis in 2-dimensional space whose columns are all unit vectors that are orthogonal to each other. Unitless.
using OrthonormalBasis3D = OrthonormalBasis<3>; ///< Basis in 3-dimensional space whose columns are all unit vectors that are orthogonal to each other. Unitless.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto transpose(const OrthonormalBasis<N>& a) {
	return OrthonormalBasis<N>::reinterpret(grem::transpose(a._private_value));
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto inverse(const OrthonormalBasis<N>& a) {
	return transpose(a);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE constexpr auto inverseTranspose(const OrthonormalBasis<N>& a) {
	return a;
}

/**
 * %Absolute orientation in world space.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
struct Orientation;

/**
 * %Absolute orientation in 2-dimensional space.
 *
 * Unit: Radians.
 */
template <>
struct Orientation<2> {
	using Unit = Absolute<Radians>;
	using DimensionType = typename Unit::DimensionType;
	using MagnitudeType = typename Unit::MagnitudeType;
	static constexpr size_t RANK = 1;
	static constexpr Unit UNIT{};
	static constexpr DimensionType DIMENSION{};
	static constexpr MagnitudeType MAGNITUDE{};

	float _private_value = 0.0f;

	GREM_ALWAYS_INLINE constexpr Orientation() noexcept = default;

	GREM_ALWAYS_INLINE constexpr Orientation(float angle) noexcept
		: _private_value(angle) {}

	template <typename OtherUnitT>
	GREM_ALWAYS_INLINE constexpr Orientation(Quantity<1, OtherUnitT> angle) noexcept requires(OtherUnitT::DIMENSION == DIMENSION)
		: _private_value(angle.in(UNIT)) {}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr operator float() const {
		return _private_value;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr operator Quantity<1, Unit>() const {
		return Quantity<1, Unit>{_private_value};
	}

	template <unit OtherUnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE constexpr Quantity<1, Unitless> in(OtherUnitT) const requires(OtherUnitT::DIMENSION == DIMENSION) {
		if constexpr (OtherUnitT::MagnitudeType::VALUE == MagnitudeType::VALUE) {
			return Quantity<1, Unitless>{_private_value};
		} else {
			constexpr float factor = static_cast<float>(MagnitudeType::VALUE / OtherUnitT::MagnitudeType::VALUE);
			return _private_value * factor;
		}
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Orientation&) const noexcept = default;

	template <typename UnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<2, UnitT> GREM_VECTORCALL operator()(Quantity<2, UnitT> b) const {
		return (mat2{grem::rotate(_private_value)} * b._private_getUnderlyingValue()) * UnitT{};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Direction2D GREM_VECTORCALL operator()(Direction2D b) const {
		return Direction2D::reinterpret(mat2{grem::rotate(_private_value)} * b._private_value);
	}

	template <typename UnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<1, UnitT> GREM_VECTORCALL operator()(Quantity<1, UnitT> b) const {
		return b;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 1;
	}
};

/**
 * %Absolute orientation in 3-dimensional space.
 *
 * Unit: Radians.
 */
template <>
struct Orientation<3> {
	using Component = Quantity<1, Absolute<Radians>>;
	using Unit = Absolute<Radians>;
	using DimensionType = typename Unit::DimensionType;
	using MagnitudeType = typename Unit::MagnitudeType;
	static constexpr size_t RANK = 4;
	static constexpr Unit UNIT{};
	static constexpr DimensionType DIMENSION{};
	static constexpr MagnitudeType MAGNITUDE{};

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation rotation(Scale3D from, Scale3D to) {
		return Orientation{grem::rotation(vec3{from}, vec3{to})};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation lookAt(Scale3D direction, Scale3D up) {
		return Orientation{grem::quatLookAt(vec3{direction}, vec3{up})};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation angleAxis(Angle angle, Scale3D axis) {
		return Orientation{grem::angleAxis(float{angle.in(RADIANS)}, vec3{axis})};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation pitch(Angle angle) {
		return Orientation{grem::pitch(float{angle})};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation yaw(Angle angle) {
		return Orientation{grem::yaw(float{angle})};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation roll(Angle angle) {
		return Orientation{grem::roll(float{angle})};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation fromAngles(Angle pitchAngle, Angle yawAngle, Angle rollAngle) {
		return Orientation{grem::convertAnglesToQuaternion(float{pitchAngle}, float{yawAngle}, float{rollAngle})};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation fromAngles(PitchYawRoll angles) {
		return Orientation{grem::convertAnglesToQuaternion(vec3{angles})};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE static Orientation fromBasis(const Basis3D& basis) {
		return Orientation{grem::convert3x3MatrixToQuaternion(mat3{basis})};
	}

	quat _private_value{0.0f, 0.0f, 0.0f, 1.0f};

	GREM_ALWAYS_INLINE constexpr Orientation() noexcept = default;

	GREM_ALWAYS_INLINE constexpr Orientation(quat quaternion) noexcept
		: _private_value(quaternion) {}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr operator quat() const {
		return _private_value;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Orientation&) const noexcept = default;

	template <typename UnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<3, UnitT> GREM_VECTORCALL operator()(Quantity<3, UnitT> b) const {
		return (_private_value * b._private_getUnderlyingValue()) * UnitT{};
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Direction3D GREM_VECTORCALL operator()(Direction3D b) const {
		return Direction3D::reinterpret(_private_value * b._private_getUnderlyingValue());
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr size_t size() const noexcept {
		return 4;
	}
};

using Orientation2D = Orientation<2>; ///< %Absolute orientation in 2-dimensional space. Unit: Radians.
using Orientation3D = Orientation<3>; ///< %Absolute orientation in 3-dimensional space. Unit: Radians.

[[nodiscard]] GREM_ALWAYS_INLINE bool GREM_VECTORCALL isfinite(Orientation2D a) {
	return grem::isfinite(a._private_value);
}

[[nodiscard]] GREM_ALWAYS_INLINE bvec4 GREM_VECTORCALL isfinite(Orientation3D a) {
	return grem::isfinite(vec4{a._private_value.x, a._private_value.y, a._private_value.z, a._private_value.w});
}

[[nodiscard]] GREM_ALWAYS_INLINE bool GREM_VECTORCALL isinf(Orientation2D a) {
	return grem::isinf(a._private_value);
}

[[nodiscard]] GREM_ALWAYS_INLINE bvec4 GREM_VECTORCALL isinf(Orientation3D a) {
	return grem::isinf(vec4{a._private_value.x, a._private_value.y, a._private_value.z, a._private_value.w});
}

[[nodiscard]] GREM_ALWAYS_INLINE bool GREM_VECTORCALL isnan(Orientation2D a) {
	return grem::isnan(a._private_value);
}

[[nodiscard]] GREM_ALWAYS_INLINE bvec4 GREM_VECTORCALL isnan(Orientation3D a) {
	return grem::isnan(vec4{a._private_value.x, a._private_value.y, a._private_value.z, a._private_value.w});
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation2D GREM_VECTORCALL inverse(Orientation2D a) {
	return Orientation2D{-a._private_value};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation3D GREM_VECTORCALL inverse(Orientation3D a) {
	return Orientation3D{grem::conjugate(quat{a})};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation2D GREM_VECTORCALL operator+(Orientation2D a, Rotation2D b) {
	return Orientation2D{float{a} + b.in(RADIANS)};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation2D GREM_VECTORCALL operator+(Orientation2D a, Scale1D b) {
	return Orientation2D{float{a} + b};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation3D GREM_VECTORCALL operator+(Orientation3D a, Rotation3D b) {
	return Orientation3D{grem::normalize(quat{a} + 0.5f * (quat{vec3{b.in(RADIANS)}, 0.0f} * quat{a}))};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation3D GREM_VECTORCALL operator+(Orientation3D a, Scale3D b) {
	return Orientation3D{grem::normalize(quat{a} + 0.5f * (quat{vec3{b}, 0.0f} * quat{a}))};
}

template <size_t N>
GREM_ALWAYS_INLINE Orientation<N>& operator+=(Orientation<N>& a, Rotation<N> b) {
	return a = a + b;
}

[[nodiscard]] GREM_ALWAYS_INLINE Rotation2D GREM_VECTORCALL operator-(Orientation2D a, Orientation2D b) {
	const float x = Angle{a}.in(RADIANS);
	const float y = Angle{b}.in(RADIANS);
	constexpr float HALF_TURN = numbers::PI;
	constexpr float FULL_TURN = 2.0f * HALF_TURN;
	return (grem::wrap(x - y + HALF_TURN, FULL_TURN) - HALF_TURN) * RADIANS;
}

[[nodiscard]] GREM_ALWAYS_INLINE Rotation3D GREM_VECTORCALL operator-(Orientation3D a, Orientation3D b) {
	const quat delta = quat{a} * quat{inverse(b)};
	const vec3 difference = 2.0f * vec3{delta.x, delta.y, delta.z};
	return grem::flipSignIf(difference, grem::signbit(delta.w)) * RADIANS;
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation2D GREM_VECTORCALL operator-(Orientation2D a, Rotation2D b) {
	return Orientation2D{float{a} - b.in(RADIANS)};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation3D GREM_VECTORCALL operator-(Orientation3D a, Rotation3D b) {
	return a + -b;
}

template <size_t N>
GREM_ALWAYS_INLINE Orientation<N>& operator-=(Orientation<N>& a, Rotation<N> b) {
	return a = a - b;
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation2D GREM_VECTORCALL operator*(Orientation2D a, Orientation2D b) {
	return Orientation2D{float{a} + float{b}};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation3D GREM_VECTORCALL operator*(Orientation3D a, Orientation3D b) {
	return Orientation3D{grem::normalize(quat{a} * quat{b})};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation2D GREM_VECTORCALL mix(Orientation2D a, Orientation2D b, Coefficient alpha) {
	return Orientation2D{grem::mix(float{a}, float{b}, float{alpha}) * Orientation2D::UNIT};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation2D GREM_VECTORCALL mix(Orientation2D a, Orientation2D b, float alpha) {
	return Orientation2D{grem::mix(float{a}, float{b}, alpha) * Orientation2D::UNIT};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation3D GREM_VECTORCALL mix(Orientation3D a, Orientation3D b, Coefficient alpha) {
	return Orientation3D{grem::mix(quat{a}, quat{b}, float{alpha})};
}

[[nodiscard]] GREM_ALWAYS_INLINE Orientation3D GREM_VECTORCALL mix(Orientation3D a, Orientation3D b, float alpha) {
	return Orientation3D{grem::mix(quat{a}, quat{b}, alpha)};
}

template <size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE Basis<N> scale(Quantity<N, UnitT> s) {
	if constexpr (N == 2) {
		return TensorQuantity<N, UnitT>::reinterpret(mat<2, 2, float>{
			vec<2, float>{s.getX()._private_value, 0.0f},
			vec<2, float>{0.0f, s.getY()._private_value},
		});
	} else if constexpr (N == 3) {
		return TensorQuantity<N, UnitT>::reinterpret(mat<3, 3, float>{
			vec<3, float>{s.getX()._private_value, 0.0f, 0.0f},
			vec<3, float>{0.0f, s.getY()._private_value, 0.0f},
			vec<3, float>{0.0f, 0.0f, s.getZ()._private_value},
		});
	}
}

[[nodiscard]] GREM_ALWAYS_INLINE OrthonormalBasis2D rotate(RollRotation r) {
	return OrthonormalBasis2D::reinterpret(mat2{grem::rotate(float{r})});
}

[[nodiscard]] GREM_ALWAYS_INLINE OrthonormalBasis3D rotate(PitchYawRollRotations r) {
	return OrthonormalBasis3D::reinterpret(mat3{grem::rotate(quat{Orientation3D{} + r})});
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE OrthonormalBasis<N> rotate(Orientation<N> r) {
	if constexpr (N == 2) {
		return OrthonormalBasis2D::reinterpret(mat2{grem::rotate(float{r})});
	} else if constexpr (N == 3) {
		return OrthonormalBasis3D::reinterpret(mat3{grem::rotate(quat{r})});
	}
}

[[nodiscard]] GREM_ALWAYS_INLINE OrthonormalBasis2D rotate(Roll angle) {
	return OrthonormalBasis2D::reinterpret(mat2{grem::rotate(float{angle})});
}

[[nodiscard]] GREM_ALWAYS_INLINE OrthonormalBasis3D rotate(PitchYawRoll angles) {
	return OrthonormalBasis3D::reinterpret(mat3{grem::rotate(vec3{angles})});
}

template <size_t N, typename UnitT>
[[nodiscard]] GREM_ALWAYS_INLINE TensorQuantity<N, UnitT> rotateScale(Orientation<N> r, Quantity<N, UnitT> s) {
	if constexpr (N == 3) {
		const quat rotation{r};
		const vec3 scale{s._private_getUnderlyingValue()};
		return TensorQuantity<3, UnitT>::reinterpret(mat3{
			(1.0f - 2.0f * (rotation.y * rotation.y + rotation.z * rotation.z)) * scale.x,
			(rotation.x * rotation.y + rotation.z * rotation.w) * scale.x * 2.0f,
			(rotation.x * rotation.z - rotation.y * rotation.w) * scale.x * 2.0f,
			(rotation.x * rotation.y - rotation.z * rotation.w) * scale.y * 2.0f,
			(1.0f - 2.0f * (rotation.x * rotation.x + rotation.z * rotation.z)) * scale.y,
			(rotation.y * rotation.z + rotation.x * rotation.w) * scale.y * 2.0f,
			(rotation.x * rotation.z + rotation.y * rotation.w) * scale.z * 2.0f,
			(rotation.y * rotation.z - rotation.x * rotation.w) * scale.z * 2.0f,
			(1.0f - 2.0f * (rotation.x * rotation.x + rotation.y * rotation.y)) * scale.z,
		});
	} else {
		return rotate(r) * scale(s);
	}
}

[[nodiscard]] GREM_ALWAYS_INLINE Direction2D convertAnglesToForwardDirection(Roll angles) {
	return Direction2D::reinterpret(grem::convertAnglesToForwardDirection(float{angles}));
}

[[nodiscard]] GREM_ALWAYS_INLINE Direction3D convertAnglesToForwardDirection(PitchYaw angles) {
	return Direction3D::reinterpret(grem::convertAnglesToForwardDirection(vec2{angles}));
}

/**
 * Identity basis for the given number of dimensions.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
inline constexpr OrthonormalBasis<N> BASIS;
template <>
inline constexpr OrthonormalBasis<2> BASIS<2> = OrthonormalBasis<2>::reinterpret(mat2{vec2{1.0f, 0.0f}, vec2{0.0f, 1.0f}});
template <>
inline constexpr OrthonormalBasis<3> BASIS<3> = OrthonormalBasis<3>::reinterpret(mat3{vec3{1.0f, 0.0f, 0.0f}, vec3{0.0f, 1.0f, 0.0f}, vec3{0.0f, 0.0f, 1.0f}});
inline constexpr OrthonormalBasis2D BASIS_2D = BASIS<2>; ///< Identity basis in 2-dimensional space.
inline constexpr OrthonormalBasis3D BASIS_3D = BASIS<3>; ///< Identity basis in 3-dimensional space.

/**
 * The X axis in the space with the given number of dimensions.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
inline constexpr Direction<N> X_AXIS;
template <>
inline constexpr Direction<2> X_AXIS<2> = Direction<2>::reinterpret(vec2{1.0f, 0.0f});
template <>
inline constexpr Direction<3> X_AXIS<3> = Direction<3>::reinterpret(vec3{1.0f, 0.0f, 0.0f});
inline constexpr Direction2D X_AXIS_2D = X_AXIS<2>; ///< The X axis in 2-dimensional space.
inline constexpr Direction3D X_AXIS_3D = X_AXIS<3>; ///< The X axis in 3-dimensional space.

/**
 * The Y axis in the space with the given number of dimensions.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
inline constexpr Direction<N> Y_AXIS;
template <>
inline constexpr Direction<2> Y_AXIS<2> = Direction<2>::reinterpret(vec2{0.0f, 1.0f});
template <>
inline constexpr Direction<3> Y_AXIS<3> = Direction<3>::reinterpret(vec3{0.0f, 1.0f, 0.0f});
inline constexpr Direction2D Y_AXIS_2D = Y_AXIS<2>; ///< The Y axis in 2-dimensional space.
inline constexpr Direction3D Y_AXIS_3D = Y_AXIS<3>; ///< The Y axis in 3-dimensional space.

/**
 * The Z axis in the space with the given number of dimensions.
 *
 * \tparam N number of dimensions of the world space (must be 3).
 */
template <size_t N>
inline constexpr Direction<N> Z_AXIS;
template <>
inline constexpr Direction<3> Z_AXIS<3> = Direction<3>::reinterpret(vec3{0.0f, 0.0f, 1.0f});
inline constexpr Direction3D Z_AXIS_3D = Z_AXIS<3>; ///< The Z axis in 3-dimensional space.

/**
 * Get the smallest signed angle around an axis between two relative vectors.
 *
 * \param axis axis to get the angle around.
 * \param a first vector.
 * \param b second vector.
 *
 * \return the smallest signed angle around the given axis between the
 *         directions of a and b.
 */
template <typename UnitT>
[[nodiscard]] inline Angle getAngleDifferenceAroundAxis(Direction3D axis, Quantity<3, UnitT> a, Quantity<3, UnitT> b) requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
	return atan2(dot(cross(a, b), axis), dot(a, b));
}

/**
 * %Relative distance/length offset quantity.
 *
 * Unit: Meters.
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using Length = Quantity<N, Meters>;
using Length1D = Length<1>; ///< %Relative distance/length offset in 1-dimensional space. Unit: Meters.
using Length2D = Length<2>; ///< %Relative distance/length offset in 2-dimensional space. Unit: Meters.
using Length3D = Length<3>; ///< %Relative distance/length offset in 3-dimensional space. Unit: Meters.

/**
 * Tensor of length quantities.
 *
 * Unit: Meters.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using LengthTensor = TensorQuantity<N, Meters>;
using LengthTensor2D = LengthTensor<2>; ///< Tensor of length quantities in 2-dimensional space. Unit: Meters.
using LengthTensor3D = LengthTensor<3>; ///< Tensor of length quantities in 3-dimensional space. Unit: Meters.

/**
 * %Absolute position in world space.
 *
 * Unit: Meters.
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using Position = Quantity<N, Absolute<Meters>>;
using Position1D = Position<1>; ///< %Absolute position in 1-dimensional space. Unit: Meters.
using Position2D = Position<2>; ///< %Absolute position in 2-dimensional space. Unit: Meters.
using Position3D = Position<3>; ///< %Absolute position in 3-dimensional space. Unit: Meters.

/**
 * Scalar length quantity.
 *
 * Alias of Length1D, but semantically implies that the value is expected to be
 * non-negative.
 *
 * Unit: Meters.
 */
using Distance = Length1D;

/**
 * Scalar squared length quantity.
 *
 * Unit: Square meters.
 */
using SquaredLength = Quantity<1, SquareMeters>;

/**
 * Scalar squared length quantity.
 *
 * Unit: Square meters.
 */
using SquaredDistance = SquaredLength;

/**
 * Affine transformation in local space.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class LocalTransformation {
public:
	GREM_ALWAYS_INLINE constexpr LocalTransformation() noexcept = default;

	GREM_ALWAYS_INLINE constexpr LocalTransformation(Basis<N> basis)
		: offset(0)
		, basis(basis) {}

	GREM_ALWAYS_INLINE constexpr LocalTransformation(Length<N> offset, Basis<N> basis)
		: offset(offset)
		, basis(basis) {}

	GREM_ALWAYS_INLINE constexpr LocalTransformation(Zero, Basis<N> basis)
		: offset(0)
		, basis(basis) {}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr operator mat3() const requires(N == 2) {
		mat3 result{basis._private_value};
		result[2] = vec3{offset._private_getUnderlyingValue(), 1.0f};
		return result;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr operator mat4() const requires(N == 3) {
		mat4 result{basis._private_value};
		result[3] = vec4{offset._private_getUnderlyingValue(), 1.0f};
		return result;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const LocalTransformation&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> GREM_VECTORCALL operator()(Length<N> b) const {
		return offset + basis * b;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> GREM_VECTORCALL getOffset() const {
		return offset;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Basis<N> GREM_VECTORCALL getBasis() const {
		return basis;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Distance GREM_VECTORCALL getDistance(Length<N> localOffset) const {
		return length(basis * localOffset);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Distance GREM_VECTORCALL getDistance2(Length<N> localOffset) const {
		return length2(basis * localOffset);
	}

	template <typename UnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<N, UnitT> GREM_VECTORCALL getRelative(Quantity<N, UnitT> localQuantity) const requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
		return basis * localQuantity;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Direction<N> GREM_VECTORCALL getDirection(Direction<N> direction) const {
		return normalize(basis * direction);
	}

private:
	Length<N> offset{};
	Basis<N> basis{};
};
using LocalTransformation2D = LocalTransformation<2>; ///< Affine transformation in 2-dimensional local space.
using LocalTransformation3D = LocalTransformation<3>; ///< Affine transformation in 3-dimensional local space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation<N> GREM_VECTORCALL translateRotateScale(Length<N> localOffset, Orientation<N> localOrientation, Scale<N> localScale) {
	return LocalTransformation<N>{localOffset, rotateScale(localOrientation, localScale)};
}

[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation3D GREM_VECTORCALL translateRotateScale(Length3D localOffset, quat localOrientation, Scale3D localScale) {
	return LocalTransformation3D{localOffset, rotateScale(Orientation3D{localOrientation}, localScale)};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation<N> GREM_VECTORCALL translateRotate(Length<N> localOffset, Orientation<N> localOrientation) {
	return LocalTransformation<N>{localOffset, rotate(localOrientation)};
}

[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation3D GREM_VECTORCALL translateRotate(Length3D localOffset, quat localOrientation) {
	return LocalTransformation3D{localOffset, rotate(Orientation3D{localOrientation})};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation<N> GREM_VECTORCALL translateScale(Length<N> localOffset, Scale<N> localScale) {
	return LocalTransformation<N>{localOffset, physics::scale(localScale)};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation<N> GREM_VECTORCALL translate(Length<N> localOffset) {
	return LocalTransformation<N>{localOffset, Basis<N>{}};
}

/**
 * Affine transformation in world space.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class Transformation {
public:
	GREM_ALWAYS_INLINE constexpr Transformation() noexcept = default;

	GREM_ALWAYS_INLINE constexpr Transformation(Position<N> origin, Basis<N> basis)
		: origin(origin)
		, basis(basis) {}

	GREM_ALWAYS_INLINE constexpr Transformation(Zero, Basis<N> basis)
		: origin(0)
		, basis(basis) {}

	GREM_ALWAYS_INLINE constexpr explicit Transformation(Position<N> origin, const LocalTransformation<N>& localTransformation)
		: origin(origin + localTransformation.getOffset())
		, basis(localTransformation.getBasis()) {}

	GREM_ALWAYS_INLINE constexpr explicit Transformation(Zero, const LocalTransformation<N>& localTransformation)
		: origin(localTransformation.getOffset())
		, basis(localTransformation.getBasis()) {}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr operator mat3() const requires(N == 2) {
		mat3 result{basis._private_value};
		result[2] = vec3{origin._private_getUnderlyingValue(), 1.0f};
		return result;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr operator mat4() const requires(N == 3) {
		mat4 result{basis._private_value};
		result[3] = vec4{origin._private_getUnderlyingValue(), 1.0f};
		return result;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const Transformation&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE Position<N> GREM_VECTORCALL operator()(Length<N> b) const {
		return origin + basis * b;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Position<N> GREM_VECTORCALL getOrigin() const {
		return origin;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Basis<N> GREM_VECTORCALL getBasis() const {
		return basis;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Distance GREM_VECTORCALL getDistance(Length<N> localOffset) const {
		return length(basis * localOffset);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Distance GREM_VECTORCALL getDistance2(Length<N> localOffset) const {
		return length2(basis * localOffset);
	}

	template <typename UnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<N, UnitT> GREM_VECTORCALL getRelative(Quantity<N, UnitT> localQuantity) const requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
		return basis * localQuantity;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Direction<N> GREM_VECTORCALL getDirection(Direction<N> direction) const {
		return normalize(basis * direction);
	}

private:
	Position<N> origin{};
	Basis<N> basis{};
};
using Transformation2D = Transformation<2>; ///< Affine transformation in 2-dimensional space.
using Transformation3D = Transformation<3>; ///< Affine transformation in 3-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE Transformation<N> GREM_VECTORCALL translateRotateScale(Position<N> position, Orientation<N> orientation, Scale<N> scale) {
	return Transformation<N>{position, rotateScale(orientation, scale)};
}

[[nodiscard]] GREM_ALWAYS_INLINE Transformation3D GREM_VECTORCALL translateRotateScale(Position3D position, quat orientation, Scale3D scale) {
	return translateRotateScale(position, Orientation3D{orientation}, scale);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE Transformation<N> GREM_VECTORCALL translateRotate(Position<N> position, Orientation<N> orientation) {
	return Transformation<N>{position, rotate(orientation)};
}

[[nodiscard]] GREM_ALWAYS_INLINE Transformation3D GREM_VECTORCALL translateRotate(Position3D position, quat orientation) {
	return Transformation3D{position, rotate(Orientation3D{orientation})};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE Transformation<N> GREM_VECTORCALL translateScale(Position<N> position, Scale<N> scale) {
	return Transformation<N>{position, physics::scale(scale)};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE Transformation<N> GREM_VECTORCALL translate(Position<N> position) {
	return Transformation<N>{position, Basis<N>{}};
}

template <size_t N>
class InverseTransformation; // Forward declaration.

/**
 * Inverse of an affine transformation in local space.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class InverseLocalTransformation {
public:
	GREM_ALWAYS_INLINE constexpr InverseLocalTransformation() noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const InverseLocalTransformation&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> GREM_VECTORCALL operator()(Length<N> b) const {
		return inverseBasis * (b - offset);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> GREM_VECTORCALL getOriginalOffset() const {
		return offset;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Basis<N> GREM_VECTORCALL getBasis() const {
		return inverseBasis;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Distance GREM_VECTORCALL getDistance(Length<N> localOffset) const {
		return length(inverseBasis * localOffset);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Distance GREM_VECTORCALL getDistance2(Length<N> localOffset) const {
		return length2(inverseBasis * localOffset);
	}

	template <typename UnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<N, UnitT> GREM_VECTORCALL getRelative(Quantity<N, UnitT> localQuantity) const requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
		return inverseBasis * localQuantity;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Direction<N> GREM_VECTORCALL getDirection(Direction<N> direction) const {
		return normalize(inverseBasis * direction);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation<N> GREM_VECTORCALL operator*(LocalTransformation<N> b) const {
		return LocalTransformation<N>{inverseBasis * (b.getOffset() - offset), inverseBasis * b.getBasis()};
	}

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseLocalTransformation<OtherN> GREM_VECTORCALL inverseTranslateRotateScale(Length<OtherN> localOffset, Orientation<OtherN> localOrientation,
		Scale<OtherN> localScale);

	GREM_ALWAYS_INLINE friend InverseLocalTransformation<3> GREM_VECTORCALL inverseTranslateRotateScale(Length3D localOffset, quat localOrientation, Scale3D localScale);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseLocalTransformation<OtherN> GREM_VECTORCALL inverseTranslateRotate(Length<OtherN> localOffset, Orientation<OtherN> localOrientation);

	GREM_ALWAYS_INLINE friend InverseLocalTransformation<3> GREM_VECTORCALL inverseTranslateRotate(Length3D localOffset, quat localOrientation);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseLocalTransformation<OtherN> GREM_VECTORCALL inverseTranslateScale(Length<OtherN> localOffset, Scale<OtherN> localScale);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseLocalTransformation<OtherN> GREM_VECTORCALL inverseTranslate(Length<OtherN> localOffset);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend LocalTransformation<OtherN> GREM_VECTORCALL inverse(InverseLocalTransformation<OtherN> a);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseLocalTransformation<OtherN> GREM_VECTORCALL inverse(LocalTransformation<OtherN> a);

private:
	GREM_ALWAYS_INLINE constexpr InverseLocalTransformation(Length<N> offset, Basis<N> inverseBasis)
		: offset(offset)
		, inverseBasis(inverseBasis) {}

	Length<N> offset{};
	Basis<N> inverseBasis{};
};
using InverseLocalTransformation2D = InverseLocalTransformation<2>; ///< Inverse of an affine transformation in 2-dimensional local space.
using InverseLocalTransformation3D = InverseLocalTransformation<3>; ///< Inverse of an affine transformation in 3-dimensional local space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseLocalTransformation<N> GREM_VECTORCALL inverseTranslateRotateScale(Length<N> localOffset, Orientation<N> localOrientation,
	Scale<N> localScale) {
	return InverseLocalTransformation<N>{localOffset, scale(1.0f / localScale) * rotate(inverse(localOrientation))};
}

[[nodiscard]] GREM_ALWAYS_INLINE InverseLocalTransformation<3> GREM_VECTORCALL inverseTranslateRotateScale(Length3D localOffset, quat localOrientation, Scale3D localScale) {
	return inverseTranslateRotateScale(localOffset, Orientation3D{localOrientation}, localScale);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseLocalTransformation<N> GREM_VECTORCALL inverseTranslateRotate(Length<N> localOffset, Orientation<N> localOrientation) {
	return InverseLocalTransformation<N>{localOffset, rotate(inverse(localOrientation))};
}

[[nodiscard]] GREM_ALWAYS_INLINE InverseLocalTransformation3D GREM_VECTORCALL inverseTranslateRotate(Length3D localOffset, quat localOrientation) {
	return inverseTranslateRotate(localOffset, Orientation3D{localOrientation});
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseLocalTransformation<N> GREM_VECTORCALL inverseTranslateScale(Length<N> localOffset, Scale<N> localScale) {
	return InverseLocalTransformation<N>{localOffset, scale(1.0f / localScale)};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseLocalTransformation<N> GREM_VECTORCALL inverseTranslate(Length<N> localOffset) {
	return InverseLocalTransformation<N>{localOffset, Basis<N>{}};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation<N> GREM_VECTORCALL inverse(InverseLocalTransformation<N> a) {
	return LocalTransformation<N>{a.offset, inverse(a.inverseBasis)};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseLocalTransformation<N> GREM_VECTORCALL inverse(LocalTransformation<N> a) {
	return InverseLocalTransformation<N>{a.getOffset(), inverse(a.getBasis())};
}

/**
 * Inverse of an affine transformation in world space.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
class InverseTransformation {
public:
	GREM_ALWAYS_INLINE constexpr InverseTransformation() noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE constexpr bool operator==(const InverseTransformation&) const noexcept = default;

	[[nodiscard]] GREM_ALWAYS_INLINE Length<N> GREM_VECTORCALL operator()(Position<N> b) const {
		return inverseBasis * (b - origin);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Position<N> GREM_VECTORCALL getOriginalOrigin() const {
		return origin;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Basis<N> GREM_VECTORCALL getBasis() const {
		return inverseBasis;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Distance GREM_VECTORCALL getDistance(Length<N> localOffset) const {
		return length(inverseBasis * localOffset);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Distance GREM_VECTORCALL getDistance2(Length<N> localOffset) const {
		return length2(inverseBasis * localOffset);
	}

	template <typename UnitT>
	[[nodiscard]] GREM_ALWAYS_INLINE Quantity<N, UnitT> GREM_VECTORCALL getRelative(Quantity<N, UnitT> localQuantity) const requires(!UnitT::MagnitudeType::IS_ABSOLUTE) {
		return inverseBasis * localQuantity;
	}

	[[nodiscard]] GREM_ALWAYS_INLINE Direction<N> GREM_VECTORCALL getDirection(Direction<N> direction) const {
		return normalize(inverseBasis * direction);
	}

	[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation<N> GREM_VECTORCALL operator*(Transformation<N> b) const {
		return LocalTransformation<N>{inverseBasis * (b.getOrigin() - origin), inverseBasis * b.getBasis()};
	}

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseTransformation<OtherN> GREM_VECTORCALL inverseTranslateRotateScale(Position<OtherN> position, Orientation<OtherN> orientation,
		Scale<OtherN> scale);

	GREM_ALWAYS_INLINE friend InverseTransformation<3> GREM_VECTORCALL inverseTranslateRotateScale(Position3D position, quat orientation, Scale3D scale);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseTransformation<OtherN> GREM_VECTORCALL inverseTranslateRotate(Position<OtherN> position, Orientation<OtherN> orientation);

	GREM_ALWAYS_INLINE friend InverseTransformation<3> GREM_VECTORCALL inverseTranslateRotate(Position3D position, quat orientation);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseTransformation<OtherN> GREM_VECTORCALL inverseTranslateScale(Position<OtherN> position, Scale<OtherN> scale);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseTransformation<OtherN> GREM_VECTORCALL inverseTranslate(Position<OtherN> position);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend Transformation<OtherN> GREM_VECTORCALL inverse(InverseTransformation<OtherN> a);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseTransformation<OtherN> GREM_VECTORCALL inverse(Transformation<OtherN> a);

	template <size_t OtherN>
	GREM_ALWAYS_INLINE friend InverseTransformation<OtherN> GREM_VECTORCALL inverse(Transformation<OtherN> a);

private:
	GREM_ALWAYS_INLINE constexpr InverseTransformation(Position<N> origin, Basis<N> inverseBasis)
		: origin(origin)
		, inverseBasis(inverseBasis) {}

	Position<N> origin{};
	Basis<N> inverseBasis{};
};
using InverseTransformation2D = InverseTransformation<2>; ///< Inverse of an affine transformation in 2-dimensional space.
using InverseTransformation3D = InverseTransformation<3>; ///< Inverse of an affine transformation in 3-dimensional space.

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseTransformation<N> GREM_VECTORCALL inverseTranslateRotateScale(Position<N> position, Orientation<N> orientation, Scale<N> scale) {
	return InverseTransformation<N>{position, physics::scale(1.0f / scale) * physics::rotate(inverse(orientation))};
}

[[nodiscard]] GREM_ALWAYS_INLINE InverseTransformation<3> GREM_VECTORCALL inverseTranslateRotateScale(Position3D position, quat orientation, Scale3D scale) {
	return inverseTranslateRotateScale(position, Orientation3D{orientation}, scale);
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseTransformation<N> GREM_VECTORCALL inverseTranslateRotate(Position<N> position, Orientation<N> orientation) {
	return InverseTransformation<N>{position, physics::rotate(inverse(orientation))};
}

[[nodiscard]] GREM_ALWAYS_INLINE InverseTransformation<3> GREM_VECTORCALL inverseTranslateRotate(Position3D position, quat orientation) {
	return inverseTranslateRotate(position, Orientation3D{orientation});
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseTransformation<N> GREM_VECTORCALL inverseTranslateScale(Position<N> position, Scale<N> scale) {
	return InverseTransformation<N>{position, physics::scale(1.0f / scale)};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseTransformation<N> GREM_VECTORCALL inverseTranslate(Position<N> position) {
	return InverseTransformation<N>{position, Basis<N>{}};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE Transformation<N> GREM_VECTORCALL inverse(InverseTransformation<N> a) {
	return Transformation<N>{a.origin, inverse(a.inverseBasis)};
}

template <size_t N>
[[nodiscard]] GREM_ALWAYS_INLINE InverseTransformation<N> GREM_VECTORCALL inverse(Transformation<N> a) {
	return InverseTransformation<N>{a.getOrigin(), inverse(a.getBasis())};
}

[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation2D GREM_VECTORCALL operator*(LocalTransformation2D a, LocalTransformation2D b) {
	return LocalTransformation2D{a.getOffset() + a.getBasis() * b.getOffset(), a.getBasis() * b.getBasis()};
}

[[nodiscard]] GREM_ALWAYS_INLINE LocalTransformation3D GREM_VECTORCALL operator*(LocalTransformation3D a, LocalTransformation3D b) {
	return LocalTransformation3D{a.getOffset() + a.getBasis() * b.getOffset(), a.getBasis() * b.getBasis()};
}

[[nodiscard]] GREM_ALWAYS_INLINE Transformation2D GREM_VECTORCALL operator*(Transformation2D a, LocalTransformation2D b) {
	return Transformation2D{a.getOrigin() + a.getBasis() * b.getOffset(), a.getBasis() * b.getBasis()};
}

[[nodiscard]] GREM_ALWAYS_INLINE Transformation3D GREM_VECTORCALL operator*(Transformation3D a, LocalTransformation3D b) {
	return Transformation3D{a.getOrigin() + a.getBasis() * b.getOffset(), a.getBasis() * b.getBasis()};
}

/**
 * Scalar mass quantity.
 *
 * Unit: Kilograms.
 */
using Mass = Quantity<1, Kilograms>;

/**
 * Scalar inverse mass quantity.
 *
 * Unit: Per kilogram.
 */
using InverseMass = Quantity<1, PerKilogram>;

/**
 * Scalar relative time duration quantity.
 *
 * Unit: Seconds.
 */
using Time = Quantity<1, Seconds>;

/**
 * Scalar area quantity.
 *
 * Unit: Square meters.
 */
using Area = Quantity<1, SquareMeters>;

/**
 * Scalar squared area quantity.
 *
 * Unit: Square meters squared.
 */
using SquaredArea = Quantity<1, Square<SquareMeters>>;

/**
 * Scalar volume quantity.
 *
 * Unit: Cubic meters.
 */
using Volume = Quantity<1, CubicMeters>;

/**
 * Scalar squared volume quantity.
 *
 * Unit: Cubic meters squared.
 */
using SquaredVolume = Quantity<1, Square<CubicMeters>>;

/**
 * Scalar density quantity.
 *
 * Unit: Kilograms per cubic meter.
 */
using Density = Quantity<1, KilogramsPerCubicMeter>;

/**
 * Frequency quantity.
 *
 * Unit: Hertz (AKA per second).
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using Rate = Quantity<N, Hertz>;
using Rate1D = Rate<1>; ///< Frequency in 1-dimensional space. Unit: Hertz (AKA per second).
using Rate2D = Rate<2>; ///< Frequency in 2-dimensional space. Unit: Hertz (AKA per second).
using Rate3D = Rate<3>; ///< Frequency in 3-dimensional space. Unit: Hertz (AKA per second).

/**
 * Scalar frequency quantity.
 *
 * Alias of Rate1D, but semantically implies that the value is expected to be
 * non-negative.
 *
 * Unit: Hertz (AKA per second).
 */
using Frequency = Rate1D;

/**
 * Angular frequency quantity.
 *
 * Unit: Hertz (AKA per second).
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularFrequency = AngularQuantity<N, Hertz>;
using AngularFrequency2D = AngularFrequency<2>; ///< Angular frequency in 2-dimensional space. Unit: Hertz (AKA per second).
using AngularFrequency3D = AngularFrequency<3>; ///< Angular frequency in 3-dimensional space. Unit: Hertz (AKA per second).

/**
 * Linear absement quantity.
 *
 * Unit: Meter seconds.
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using LinearAbsement = Quantity<N, MeterSeconds>;
using LinearAbsement1D = LinearAbsement<1>; ///< Linear absement in 1-dimensional space. Unit: Meter seconds.
using LinearAbsement2D = LinearAbsement<2>; ///< Linear absement in 2-dimensional space. Unit: Meter seconds.
using LinearAbsement3D = LinearAbsement<3>; ///< Linear absement in 3-dimensional space. Unit: Meter seconds.

/**
 * Scalar wavenumber quantity.
 *
 * Unit: Per meter.
 */
using Wavenumber = Quantity<1, PerMeter>;

/**
 * Linear velocity quantity.
 *
 * Unit: Meters per second.
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using LinearVelocity = Quantity<N, MetersPerSecond>;
using LinearVelocity1D = LinearVelocity<1>; ///< Linear velocity in 1-dimensional space. Unit: Meters per second.
using LinearVelocity2D = LinearVelocity<2>; ///< Linear velocity in 2-dimensional space. Unit: Meters per second.
using LinearVelocity3D = LinearVelocity<3>; ///< Linear velocity in 3-dimensional space. Unit: Meters per second.

/**
 * Scalar linear velocity quantity.
 *
 * Alias of LinearVelocity1D, but semantically implies that the value is
 * expected to be non-negative.
 *
 * Unit: Meters per second.
 */
using Speed = LinearVelocity1D;

/**
 * Tensor of linear velocity quantities.
 *
 * Unit: Meters per second.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using LinearVelocityTensor = TensorQuantity<N, MetersPerSecond>;
using LinearVelocityTensor2D = LinearVelocityTensor<2>; ///< Tensor of linear velocity quantities in 2-dimensional space. Unit: Meters per second.
using LinearVelocityTensor3D = LinearVelocityTensor<3>; ///< Tensor of linear velocity quantities in 3-dimensional space. Unit: Meters per second.

/**
 * Scalar squared speed quantity.
 *
 * Unit: Square meters per second squared.
 */
using SquaredSpeed = Quantity<1, Square<MetersPerSecond>>;

/**
 * Linear acceleration quantity.
 *
 * Unit: Meters per second squared.
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using LinearAcceleration = Quantity<N, MetersPerSecondSquared>;
using LinearAcceleration1D = LinearAcceleration<1>; ///< Linear acceleration in 1-dimensional space. Unit: Meters per second squared.
using LinearAcceleration2D = LinearAcceleration<2>; ///< Linear acceleration in 2-dimensional space. Unit: Meters per second squared.
using LinearAcceleration3D = LinearAcceleration<3>; ///< Linear acceleration in 3-dimensional space. Unit: Meters per second squared.
using Acceleration = LinearAcceleration1D;          ///< Linear acceleration quantity. Unit: Meters per second squared.

/**
 * Scalar squared acceleration quantity.
 *
 * Unit: Square meters per second^4.
 */
using SquaredAcceleration = Quantity<1, Square<MetersPerSecondSquared>>;

/**
 * Tensor of linear acceleration quantities.
 *
 * Unit: Meters per second squared.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using LinearAccelerationTensor = TensorQuantity<N, MetersPerSecondSquared>;
using LinearAccelerationTensor2D = LinearAccelerationTensor<2>; ///< Tensor of linear acceleration quantities in 2-dimensional space. Unit: Meters per second squared.
using LinearAccelerationTensor3D = LinearAccelerationTensor<3>; ///< Tensor of linear acceleration quantities in 3-dimensional space. Unit: Meters per second squared.

/**
 * Force quantity.
 *
 * Unit: Newton (AKA kilogram meters per second squared).
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using Force = Quantity<N, Newton>;
using Force1D = Force<1>; ///< Force in 1-dimensional space. Unit: Newton (AKA kilogram meters per second squared).
using Force2D = Force<2>; ///< Force in 2-dimensional space. Unit: Newton (AKA kilogram meters per second squared).
using Force3D = Force<3>; ///< Force in 3-dimensional space. Unit: Newton (AKA kilogram meters per second squared).

/**
 * Tensor of force quantities.
 *
 * Unit: Newton (AKA kilogram meters per second squared).
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using ForceTensor = TensorQuantity<N, Newton>;
using ForceTensor2D = ForceTensor<2>; ///< Tensor of force quantities in 2-dimensional space. Unit: Newton (AKA kilogram meters per second squared).
using ForceTensor3D = ForceTensor<3>; ///< Tensor of force quantities in 3-dimensional space. Unit: Newton (AKA kilogram meters per second squared).

/**
 * Torque quantity.
 *
 * Unit: Newton meters (AKA kilogram square meters per second squared).
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using Torque = AngularQuantity<N, NewtonMeters>;
using Torque2D = Torque<2>; ///< Torque in 2-dimensional space. Unit: Newton meters (AKA kilogram square meters per second squared).
using Torque3D = Torque<3>; ///< Torque in 3-dimensional space. Unit: Newton meters (AKA kilogram square meters per second squared).

/**
 * Tensor of torque quantities.
 *
 * Unit: Newton meters (AKA kilogram square meters per second squared).
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using TorqueTensor = TensorQuantity<N, NewtonMeters>;
using TorqueTensor2D = TorqueTensor<2>; ///< Tensor of torque quantities in 2-dimensional space. Unit: Newton meters (AKA kilogram square meters per second squared).
using TorqueTensor3D = TorqueTensor<3>; ///< Tensor of torque quantities in 3-dimensional space. Unit: Newton meters (AKA kilogram square meters per second squared).

/**
 * Linear impulse quantity.
 *
 * Unit: Newton seconds (AKA kilogram meters per second).
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using LinearImpulse = Quantity<N, NewtonSeconds>;
using LinearImpulse1D = LinearImpulse<1>; ///< Linear impulse in 1-dimensional space. Unit: Newton seconds (AKA kilogram meters per second).
using LinearImpulse2D = LinearImpulse<2>; ///< Linear impulse in 2-dimensional space. Unit: Newton seconds (AKA kilogram meters per second).
using LinearImpulse3D = LinearImpulse<3>; ///< Linear impulse in 3-dimensional space. Unit: Newton seconds (AKA kilogram meters per second).

/**
 * Scalar linear impulse quantity.
 *
 * Alias of LinearImpulse1D, but semantically implies that the value is expected
 * to be non-negative.
 *
 * Unit: Newton seconds (AKA kilogram meters per second).
 */
using Impulse = LinearImpulse1D;

/**
 * Tensor of linear impulse quantities.
 *
 * Unit: Newton seconds (AKA kilogram meters per second).
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using LinearImpulseTensor = TensorQuantity<N, NewtonSeconds>;
using LinearImpulseTensor2D = LinearImpulseTensor<2>; ///< Tensor of linear impulse quantities in 2-dimensional space. Unit: Newton seconds (AKA kilogram meters per second).
using LinearImpulseTensor3D = LinearImpulseTensor<3>; ///< Tensor of linear impulse quantities in 3-dimensional space. Unit: Newton seconds (AKA kilogram meters per second).

/**
 * Linear momentum quantity.
 *
 * Unit: Kilogram meters per second.
 *
 * \tparam N number of dimensions of the world space (must be 1, 2 or 3).
 */
template <size_t N>
using LinearMomentum = Quantity<N, KilogramMetersPerSecond>;
using LinearMomentum1D = LinearMomentum<1>; ///< Linear momentum in 1-dimensional space. Unit: Kilogram meters per second.
using LinearMomentum2D = LinearMomentum<2>; ///< Linear momentum in 2-dimensional space. Unit: Kilogram meters per second.
using LinearMomentum3D = LinearMomentum<3>; ///< Linear momentum in 3-dimensional space. Unit: Kilogram meters per second.

/**
 * Scalar linear momentum quantity.
 *
 * Alias of LinearMomentum1D, but semantically implies that the value is
 * expected to be non-negative.
 *
 * Unit: Kilogram meters per second.
 */
using Momentum = LinearMomentum1D;

/**
 * Tensor of linear momentum quantities.
 *
 * Unit: Kilogram meters per second.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using LinearMomentumTensor = TensorQuantity<N, KilogramMetersPerSecond>;
using LinearMomentumTensor2D = LinearMomentumTensor<2>; ///< Tensor of linear momentum quantities in 2-dimensional space. Unit: Kilogram meters per second.
using LinearMomentumTensor3D = LinearMomentumTensor<3>; ///< Tensor of linear momentum quantities in 3-dimensional space. Unit: Kilogram meters per second.

/**
 * Moment of inertia quantity.
 *
 * Unit: Kilogram square meters.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using MomentOfInertia = AngularQuantity<N, KilogramSquareMeters>;
using MomentOfInertia2D = MomentOfInertia<2>; ///< Moment of inertia in 2-dimensional space. Unit: Kilogram square meters.
using MomentOfInertia3D = MomentOfInertia<3>; ///< Moment of inertia in 3-dimensional space. Unit: Kilogram square meters.

/**
 * Tensor of moment of inertia quantities.
 *
 * Unit: Kilogram square meters.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using MomentOfInertiaTensor = AngularTensorQuantity<N, KilogramSquareMeters>;
using MomentOfInertiaTensor2D = MomentOfInertiaTensor<2>; ///< Tensor of moment of inertia quantities in 2-dimensional space. Unit: Kilogram square meters.
using MomentOfInertiaTensor3D = MomentOfInertiaTensor<3>; ///< Tensor of moment of inertia quantities in 3-dimensional space. Unit: Kilogram square meters.

/**
 * Inverse moment of inertia quantity.
 *
 * Unit: Per kilogram square meter.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using InverseMomentOfInertia = AngularQuantity<N, PerKilogramSquareMeter>;
using InverseMomentOfInertia2D = InverseMomentOfInertia<2>; ///< Inverse moment of inertia in 2-dimensional space. Unit: Kilogram square meters.
using InverseMomentOfInertia3D = InverseMomentOfInertia<3>; ///< Inverse moment of inertia in 3-dimensional space. Unit: Kilogram square meters.

/**
 * Tensor of inverse moment of inertia quantities.
 *
 * Unit: Per kilogram square meter.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using InverseMomentOfInertiaTensor = AngularTensorQuantity<N, PerKilogramSquareMeter>;
using InverseMomentOfInertiaTensor2D = InverseMomentOfInertiaTensor<2>; ///< Tensor of inverse moment of inertia quantities in 2-dimensional space. Unit: Per kilogram square meter.
using InverseMomentOfInertiaTensor3D = InverseMomentOfInertiaTensor<3>; ///< Tensor of inverse moment of inertia quantities in 3-dimensional space. Unit: Per kilogram square meter.

/**
 * Principal moments of inertia in inertia major axis space.
 *
 * Unit: Kilogram square meters.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using PrincipalMomentsOfInertia = AngularQuantity<N, KilogramSquareMeters>;
using PrincipalMomentsOfInertia2D = PrincipalMomentsOfInertia<2>; ///< Principal moments of inertia in 2-dimensional inertia major axis space. Unit: kilogram square meters.
using PrincipalMomentsOfInertia3D = PrincipalMomentsOfInertia<3>; ///< Principal moments of inertia in 3-dimensional inertia major axis space. Unit: kilogram square meters.

/**
 * Local inertia orientation of an object, rotating from inertia major axis
 * space to shape-local space.
 *
 * Unit: Radians.
 *
 * \tparam N number of dimensions of the object space (must be 2 or 3).
 */
template <size_t N>
struct LocalInertiaOrientation;

template <>
struct LocalInertiaOrientation<2> {
	constexpr operator Orientation2D() const noexcept {
		return {};
	}
};

template <>
struct LocalInertiaOrientation<3> : Orientation3D {};

using LocalInertiaOrientation2D = LocalInertiaOrientation<2>; ///< Local inertia orientation of an object in 2-dimensional space. Unit: Radians.
using LocalInertiaOrientation3D = LocalInertiaOrientation<3>; ///< Local inertia orientation of an object in 3-dimensional space. Unit: Radians.

/**
 * Inverse principal moments of inertia in inertia major axis space.
 *
 * Unit: Per kilogram square squared.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using InversePrincipalMomentsOfInertia = AngularQuantity<N, Reciprocal<KilogramSquareMeters>>;
using InversePrincipalMomentsOfInertia2D =
	InversePrincipalMomentsOfInertia<2>; ///< Inverse principal moments of inertia in 2-dimensional inertia major axis space. Unit: Per kilogram square meter.
using InversePrincipalMomentsOfInertia3D =
	InversePrincipalMomentsOfInertia<3>; ///< Inverse principal moments of inertia in 3-dimensional inertia major axis space. Unit: Per kilogram square meter.

/**
 * Moment arm quantity.
 *
 * Unit: Meters.
 *
 * \tparam N numbers of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using MomentArm = AngularQuantity<N, Meters>;
using MomentArm2D = MomentArm<2>; ///< Moment arm in 2-dimensional space. Unit: Meters.
using MomentArm3D = MomentArm<3>; ///< Moment arm in 3-dimensional space. Unit: Meters.

/**
 * Tensor of moment arm quantities.
 *
 * Unit: Meters.
 *
 * \tparam N numbers of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using MomentArmTensor = AngularTensorQuantity<N, Meters>;
using MomentArmTensor2D = MomentArmTensor<2>; ///< Tensor of moment arm quantities in 2-dimensional space. Unit: Meters.
using MomentArmTensor3D = MomentArmTensor<3>; ///< Tensor of moment arm quantities in 3-dimensional space. Unit: Meters.

/**
 * Angular absement quantity.
 *
 * Unit: Radian seconds.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularAbsement = AngularQuantity<N, RadianSeconds>;
using AngularAbsement2D = AngularAbsement<2>; ///< Angular absement in 2-dimensional space. Unit: Radian seconds.
using AngularAbsement3D = AngularAbsement<3>; ///< Angular absement in 3-dimensional space. Unit: Radian seconds.

/**
 * Angular velocity quantity.
 *
 * Unit: Radians per second.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularVelocity = AngularQuantity<N, RadiansPerSecond>;
using AngularVelocity2D = AngularVelocity<2>; ///< Angular velocity in 2-dimensional space. Unit: Radians per second.
using AngularVelocity3D = AngularVelocity<3>; ///< Angular velocity in 3-dimensional space. Unit: Radians per second.

/**
 * Scalar angular speed quantity.
 *
 * Unit: Radians per second.
 */
using AngularSpeed = Quantity<1, RadiansPerSecond>;

/**
 * Tensor of angular velocity quantities.
 *
 * Unit: Radians per second.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularVelocityTensor = AngularTensorQuantity<N, RadiansPerSecond>;
using AngularVelocityTensor2D = AngularVelocityTensor<2>; ///< Tensor of angular velocity quantities in 2-dimensional space. Unit: Radians per second.
using AngularVelocityTensor3D = AngularVelocityTensor<3>; ///< Tensor of angular velocity quantities in 3-dimensional space. Unit: Radians per second.

/**
 * Scalar squared angular speed quantity.
 *
 * Unit: Square radians per second squared.
 */
using SquaredAngularSpeed = Quantity<1, Square<RadiansPerSecond>>;

/**
 * Time derivative of roll.
 *
 * Unit: Radians per second.
 */
using RollRate = AngularSpeed;

/**
 * Time derivatives of pitch and yaw.
 *
 * Unit: Radians per second.
 */
using PitchYawRates = Quantity<2, RadiansPerSecond>;

/**
 * Time derivatives of pitch, yaw and roll.
 *
 * Unit: Radians per second.
 */
using PitchYawRollRates = Quantity<3, RadiansPerSecond>;

/**
 * Angular acceleration quantity.
 *
 * Unit: Radians per second squared.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularAcceleration = AngularQuantity<N, RadiansPerSecondSquared>;
using AngularAcceleration2D = AngularAcceleration<2>; ///< Angular acceleration in 2-dimensional space. Unit: Radians per second squared.
using AngularAcceleration3D = AngularAcceleration<3>; ///< Angular acceleration in 3-dimensional space. Unit: Radians per second squared.

/**
 * Tensor of angular acceleration quantities.
 *
 * Unit: Radians per second squared.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularAccelerationTensor = AngularTensorQuantity<N, RadiansPerSecondSquared>;
using AngularAccelerationTensor2D = AngularAccelerationTensor<2>; ///< Tensor of angular acceleration quantities in 2-dimensional space. Unit: Radians per second squared.
using AngularAccelerationTensor3D = AngularAccelerationTensor<3>; ///< Tensor of angular acceleration quantities in 3-dimensional space. Unit: Radians per second squared.

/**
 * Angular impulse quantity.
 *
 * Unit: Newton meter seconds (AKA kilogram square meters per second).
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularImpulse = AngularQuantity<N, NewtonMeterSeconds>;
using AngularImpulse2D = AngularImpulse<2>; ///< Angular impulse in 2-dimensional space. Unit: Newton meter seconds.
using AngularImpulse3D = AngularImpulse<3>; ///< Angular impulse in 3-dimensional space. Unit: Newton meter seconds.

/**
 * Tensor of angular impulse quantities.
 *
 * Unit: Newton meter seconds (AKA kilogram square meters per second).
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularImpulseTensor = AngularTensorQuantity<N, NewtonMeterSeconds>;
using AngularImpulseTensor2D =
	AngularImpulseTensor<2>; ///< Tensor of angular impulse quantities in 2-dimensional space. Unit: Newton meter seconds (AKA kilogram square meters per second).
using AngularImpulseTensor3D =
	AngularImpulseTensor<3>; ///< Tensor of angular impulse quantities in 3-dimensional space. Unit: Newton meter seconds (AKA kilogram square meters per second).

/**
 * Angular momentum quantity.
 *
 * Unit: Kilogram square meters per second.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularMomentum = AngularQuantity<N, KilogramSquareMetersPerSecond>;
using AngularMomentum2D = AngularMomentum<2>; ///< Angular momentum in 2-dimensional space. Unit: kilogram square meters per second.
using AngularMomentum3D = AngularMomentum<3>; ///< Angular momentum in 3-dimensional space. Unit: kilogram square meters per second.

/**
 * Tensor of angular momentum quantities.
 *
 * Unit: Kilogram square meters per second.
 *
 * \tparam N number of dimensions of the world space (must be 2 or 3).
 */
template <size_t N>
using AngularMomentumTensor = AngularTensorQuantity<N, KilogramSquareMetersPerSecond>;
using AngularMomentumTensor2D = AngularMomentumTensor<2>; ///< Tensor of angular momentum quantities in 2-dimensional space. Unit: Kilogram square meters per second.
using AngularMomentumTensor3D = AngularMomentumTensor<3>; ///< Tensor of angular momentum quantities in 3-dimensional space. Unit: Kilogram square meters per second.

/**
 * Scalar mass flow rate quantity.
 *
 * Unit: Kilograms per second.
 */
using MassFlowRate = Quantity<1, KilogramsPerSecond>;

/**
 * Scalar volumetric flow rate quantity.
 *
 * Unit: Cubic meters per second.
 */
using VolumetricFlowRate = Quantity<1, CubicMetersPerSecond>;

/**
 * Scalar mechanical work quantity.
 *
 * Unit: Joules (AKA kilogram square meters per second squared).
 */
using Work = Quantity<1, Joules>;

/**
 * Scalar mechanical power quantity.
 *
 * Unit: Watts (AKA kilogram square meters per second cubed).
 */
using Power = Quantity<1, Watts>;

/**
 * Scalar energy quantity.
 *
 * Unit: Joules (AKA kilogram square meters per second squared).
 */
using Energy = Quantity<1, Joules>;

namespace literals {

using namespace time_literals;

[[nodiscard]] consteval Coefficient operator""_x(long double value) {
	return Coefficient{static_cast<float>(value)};
}

[[nodiscard]] consteval Coefficient operator""_x(unsigned long long value) {
	return Coefficient{static_cast<float>(value)};
}

[[nodiscard]] consteval Coefficient operator""_percent(long double value) {
	return Coefficient{static_cast<float>(value * 0.01l)};
}

[[nodiscard]] consteval Coefficient operator""_percent(unsigned long long value) {
	return Coefficient{static_cast<float>(value) * 0.01f};
}

[[nodiscard]] consteval Angle operator""_radians(long double value) {
	return static_cast<float>(value) * RADIANS;
}

[[nodiscard]] consteval Angle operator""_radians(unsigned long long value) {
	return static_cast<float>(value) * RADIANS;
}

[[nodiscard]] consteval Angle operator""_radian(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_radian\". For values other than 1, use the plural form \"_radians\" instead."};
	}
	return operator""_radians(value);
}

[[nodiscard]] consteval Angle operator""_radian(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_radian\". For values other than 1, use the plural form \"_radians\" instead."};
	}
	return operator""_radians(value);
}

[[nodiscard]] consteval Angle operator""_degrees(long double value) {
	return static_cast<float>(value * 0.01745329251994329576923690768489l) * RADIANS;
}

[[nodiscard]] consteval Angle operator""_degrees(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.01745329251994329576923690768489l) * RADIANS;
}

[[nodiscard]] consteval Angle operator""_degree(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_degree\". For values other than 1, use the plural form \"_degrees\" instead."};
	}
	return operator""_degrees(value);
}

[[nodiscard]] consteval Angle operator""_degree(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_degree\". For values other than 1, use the plural form \"_degrees\" instead."};
	}
	return operator""_degrees(value);
}

[[nodiscard]] consteval Angle operator""_turns(long double value) {
	return static_cast<float>(value * 6.28318530718l) * RADIANS;
}

[[nodiscard]] consteval Angle operator""_turns(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 6.28318530718l) * RADIANS;
}

[[nodiscard]] consteval Angle operator""_turn(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_turn\". For values other than 1, use the plural form \"_turns\" instead."};
	}
	return operator""_turns(value);
}

[[nodiscard]] consteval Angle operator""_turn(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_turn\". For values other than 1, use the plural form \"_turns\" instead."};
	}
	return operator""_turns(value);
}

[[nodiscard]] consteval Mass operator""_kilograms(long double value) {
	return static_cast<float>(value) * KILOGRAMS;
}

[[nodiscard]] consteval Mass operator""_kilograms(unsigned long long value) {
	return static_cast<float>(value) * KILOGRAMS;
}

[[nodiscard]] consteval Mass operator""_kilogram(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_kilogram\". For values other than 1, use the plural form \"_kilograms\" instead."};
	}
	return operator""_kilograms(value);
}

[[nodiscard]] consteval Mass operator""_kilogram(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_kilogram\". For values other than 1, use the plural form \"_kilograms\" instead."};
	}
	return operator""_kilograms(value);
}

[[nodiscard]] consteval Mass operator""_grams(long double value) {
	return static_cast<float>(value * 0.001l) * KILOGRAMS;
}

[[nodiscard]] consteval Mass operator""_grams(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.001l) * KILOGRAMS;
}

[[nodiscard]] consteval Mass operator""_gram(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_gram\". For values other than 1, use the plural form \"_grams\" instead."};
	}
	return operator""_grams(value);
}

[[nodiscard]] consteval Mass operator""_gram(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_gram\". For values other than 1, use the plural form \"_grams\" instead."};
	}
	return operator""_grams(value);
}

[[nodiscard]] consteval Distance operator""_meters(long double value) {
	return static_cast<float>(value) * METERS;
}

[[nodiscard]] consteval Distance operator""_meters(unsigned long long value) {
	return static_cast<float>(value) * METERS;
}

[[nodiscard]] consteval Distance operator""_meter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_meter\". For values other than 1, use the plural form \"_meters\" instead."};
	}
	return operator""_meters(value);
}

[[nodiscard]] consteval Distance operator""_meter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_meter\". For values other than 1, use the plural form \"_meters\" instead."};
	}
	return operator""_meters(value);
}

[[nodiscard]] consteval Distance operator""_kilometers(long double value) {
	return static_cast<float>(value * 1000.0l) * METERS;
}

[[nodiscard]] consteval Distance operator""_kilometers(unsigned long long value) {
	return static_cast<float>(value * 1000ull) * METERS;
}

[[nodiscard]] consteval Distance operator""_kilometer(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_kilometer\". For values other than 1, use the plural form \"_kilometers\" instead."};
	}
	return operator""_kilometers(value);
}

[[nodiscard]] consteval Distance operator""_kilometer(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_kilometer\". For values other than 1, use the plural form \"_kilometers\" instead."};
	}
	return operator""_kilometers(value);
}

[[nodiscard]] consteval Distance operator""_decimeters(long double value) {
	return static_cast<float>(value * 0.1l) * METERS;
}

[[nodiscard]] consteval Distance operator""_decimeters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.1l) * METERS;
}

[[nodiscard]] consteval Distance operator""_decimeter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_decimeter\". For values other than 1, use the plural form \"_decimeters\" instead."};
	}
	return operator""_decimeters(value);
}

[[nodiscard]] consteval Distance operator""_decimeter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_decimeter\". For values other than 1, use the plural form \"_decimeters\" instead."};
	}
	return operator""_decimeters(value);
}

[[nodiscard]] consteval Distance operator""_centimeters(long double value) {
	return static_cast<float>(value * 0.01l) * METERS;
}

[[nodiscard]] consteval Distance operator""_centimeters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.01l) * METERS;
}

[[nodiscard]] consteval Distance operator""_centimeter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_centimeter\". For values other than 1, use the plural form \"_centimeters\" instead."};
	}
	return operator""_centimeters(value);
}

[[nodiscard]] consteval Distance operator""_centimeter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_centimeter\". For values other than 1, use the plural form \"_centimeters\" instead."};
	}
	return operator""_centimeters(value);
}

[[nodiscard]] consteval Distance operator""_millimeters(long double value) {
	return static_cast<float>(value * 0.001l) * METERS;
}

[[nodiscard]] consteval Distance operator""_millimeters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.001l) * METERS;
}

[[nodiscard]] consteval Distance operator""_millimeter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_millimeter\". For values other than 1, use the plural form \"_millimeters\" instead."};
	}
	return operator""_millimeters(value);
}

[[nodiscard]] consteval Distance operator""_millimeter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_millimeter\". For values other than 1, use the plural form \"_millimeters\" instead."};
	}
	return operator""_millimeters(value);
}

[[nodiscard]] consteval Area operator""_square_meters(long double value) {
	return static_cast<float>(value) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_meters(unsigned long long value) {
	return static_cast<float>(value) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_meter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_square_meter\". For values other than 1, use the plural form \"_square_meters\" instead."};
	}
	return operator""_square_meters(value);
}

[[nodiscard]] consteval Area operator""_square_meter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_square_meter\". For values other than 1, use the plural form \"_square_meters\" instead."};
	}
	return operator""_square_meters(value);
}

[[nodiscard]] consteval Area operator""_square_kilometers(long double value) {
	return static_cast<float>(value * 1000000.0l) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_kilometers(unsigned long long value) {
	return static_cast<float>(value * 1000000ull) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_kilometer(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_square_kilometer\". For values other than 1, use the plural form \"_square_kilometers\" instead."};
	}
	return operator""_square_kilometers(value);
}

[[nodiscard]] consteval Area operator""_square_kilometer(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_square_kilometer\". For values other than 1, use the plural form \"_square_kilometers\" instead."};
	}
	return operator""_square_kilometers(value);
}

[[nodiscard]] consteval Area operator""_square_decimeters(long double value) {
	return static_cast<float>(value * 0.01l) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_decimeters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.01l) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_decimeter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_square_decimeter\". For values other than 1, use the plural form \"_square_decimeters\" instead."};
	}
	return operator""_square_decimeters(value);
}

[[nodiscard]] consteval Area operator""_square_decimeter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_square_decimeter\". For values other than 1, use the plural form \"_square_decimeters\" instead."};
	}
	return operator""_square_decimeters(value);
}

[[nodiscard]] consteval Area operator""_square_centimeters(long double value) {
	return static_cast<float>(value * 0.0001l) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_centimeters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.0001l) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_centimeter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_square_centimeter\". For values other than 1, use the plural form \"_square_centimeters\" instead."};
	}
	return operator""_square_centimeters(value);
}

[[nodiscard]] consteval Area operator""_square_centimeter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_square_centimeter\". For values other than 1, use the plural form \"_square_centimeters\" instead."};
	}
	return operator""_square_centimeters(value);
}

[[nodiscard]] consteval Area operator""_square_millimeters(long double value) {
	return static_cast<float>(value * 0.000001l) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_millimeters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.000001l) * SQUARE_METERS;
}

[[nodiscard]] consteval Area operator""_square_millimeter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_square_millimeter\". For values other than 1, use the plural form \"_square_millimeters\" instead."};
	}
	return operator""_square_millimeters(value);
}

[[nodiscard]] consteval Area operator""_square_millimeter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_square_millimeter\". For values other than 1, use the plural form \"_square_millimeters\" instead."};
	}
	return operator""_square_millimeters(value);
}

[[nodiscard]] consteval Volume operator""_cubic_meters(long double value) {
	return static_cast<float>(value) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_cubic_meters(unsigned long long value) {
	return static_cast<float>(value) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_cubic_meter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_cubic_meter\". For values other than 1, use the plural form \"_cubic_meters\" instead."};
	}
	return operator""_cubic_meters(value);
}

[[nodiscard]] consteval Volume operator""_cubic_meter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_cubic_meter\". For values other than 1, use the plural form \"_cubic_meters\" instead."};
	}
	return operator""_cubic_meters(value);
}

[[nodiscard]] consteval Volume operator""_liters(long double value) {
	return static_cast<float>(value * 0.001l) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_liters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.001l) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_liter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_liter\". For values other than 1, use the plural form \"_liters\" instead."};
	}
	return operator""_liters(value);
}

[[nodiscard]] consteval Volume operator""_liter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_liter\". For values other than 1, use the plural form \"_liters\" instead."};
	}
	return operator""_liters(value);
}

[[nodiscard]] consteval Volume operator""_deciliters(long double value) {
	return static_cast<float>(value * 0.0001l) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_deciliters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.0001l) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_deciliter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_deciliter\". For values other than 1, use the plural form \"_deciliters\" instead."};
	}
	return operator""_deciliters(value);
}

[[nodiscard]] consteval Volume operator""_deciliter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_deciliter\". For values other than 1, use the plural form \"_deciliters\" instead."};
	}
	return operator""_deciliters(value);
}

[[nodiscard]] consteval Volume operator""_centiliters(long double value) {
	return static_cast<float>(value * 0.00001l) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_centiliters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.00001l) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_centiliter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_centiliter\". For values other than 1, use the plural form \"_centiliters\" instead."};
	}
	return operator""_centiliters(value);
}

[[nodiscard]] consteval Volume operator""_centiliter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_centiliter\". For values other than 1, use the plural form \"_centiliters\" instead."};
	}
	return operator""_centiliters(value);
}

[[nodiscard]] consteval Volume operator""_milliliters(long double value) {
	return static_cast<float>(value * 0.000001l) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_milliliters(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.000001l) * CUBIC_METERS;
}

[[nodiscard]] consteval Volume operator""_milliliter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_milliliter\". For values other than 1, use the plural form \"_milliliters\" instead."};
	}
	return operator""_milliliters(value);
}

[[nodiscard]] consteval Volume operator""_milliliter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_milliliter\". For values other than 1, use the plural form \"_milliliters\" instead."};
	}
	return operator""_milliliters(value);
}

[[nodiscard]] consteval Density operator""_kilograms_per_cubic_meter(long double value) {
	return static_cast<float>(value) * KILOGRAMS_PER_CUBIC_METER;
}

[[nodiscard]] consteval Density operator""_kilograms_per_cubic_meter(unsigned long long value) {
	return static_cast<float>(value) * KILOGRAMS_PER_CUBIC_METER;
}

[[nodiscard]] consteval Density operator""_kilogram_per_cubic_meter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_kilogram_per_cubic_meter\". For values other than 1, use the plural form \"_kilograms_per_cubic_meter\" instead."};
	}
	return operator""_kilograms_per_cubic_meter(value);
}

[[nodiscard]] consteval Density operator""_kilogram_per_cubic_meter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_kilogram_per_cubic_meter\". For values other than 1, use the plural form \"_kilograms_per_cubic_meter\" instead."};
	}
	return operator""_kilograms_per_cubic_meter(value);
}

[[nodiscard]] consteval Density operator""_grams_per_cubic_centimeter(long double value) {
	return static_cast<float>(value * 1000.0l) * KILOGRAMS_PER_CUBIC_METER;
}

[[nodiscard]] consteval Density operator""_grams_per_cubic_centimeter(unsigned long long value) {
	return static_cast<float>(value * 1000ull) * KILOGRAMS_PER_CUBIC_METER;
}

[[nodiscard]] consteval Density operator""_gram_per_cubic_centimeter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_gram_per_cubic_centimeter\". For values other than 1, use the plural form \"_grams_per_cubic_centimeter\" instead."};
	}
	return operator""_grams_per_cubic_centimeter(value);
}

[[nodiscard]] consteval Density operator""_gram_per_cubic_centimeter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_gram_per_cubic_centimeter\". For values other than 1, use the plural form \"_grams_per_cubic_centimeter\" instead."};
	}
	return operator""_grams_per_cubic_centimeter(value);
}

[[nodiscard]] consteval Frequency operator""_Hertz(long double value) {
	return static_cast<float>(value) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_Hertz(unsigned long long value) {
	return static_cast<float>(value) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_per_second(long double value) {
	return static_cast<float>(value) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_per_second(unsigned long long value) {
	return static_cast<float>(value) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_per_millisecond(long double value) {
	return static_cast<float>(value * 1000.0l) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_per_millisecond(unsigned long long value) {
	return static_cast<float>(value * 1000ull) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_per_minute(long double value) {
	return static_cast<float>(value * 0.0166666666667l) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_per_minute(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.0166666666667l) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_per_hour(long double value) {
	return static_cast<float>(value * 0.000277777777778l) * HERTZ;
}

[[nodiscard]] consteval Frequency operator""_per_hour(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.000277777777778l) * HERTZ;
}

[[nodiscard]] consteval LinearAbsement1D operator""_meter_seconds(long double value) {
	return static_cast<float>(value) * METER_SECONDS;
}

[[nodiscard]] consteval LinearAbsement1D operator""_meter_seconds(unsigned long long value) {
	return static_cast<float>(value) * METER_SECONDS;
}

[[nodiscard]] consteval LinearAbsement1D operator""_meter_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_meter_second\". For values other than 1, use the plural form \"_meter_seconds\" instead."};
	}
	return operator""_meter_seconds(value);
}

[[nodiscard]] consteval LinearAbsement1D operator""_meter_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_meter_second\". For values other than 1, use the plural form \"_meter_seconds\" instead."};
	}
	return operator""_meter_seconds(value);
}

[[nodiscard]] consteval Wavenumber operator""_per_meter(long double value) {
	return static_cast<float>(value) * PER_METER;
}

[[nodiscard]] consteval Wavenumber operator""_per_meter(unsigned long long value) {
	return static_cast<float>(value) * PER_METER;
}

[[nodiscard]] consteval Speed operator""_meters_per_second(long double value) {
	return static_cast<float>(value) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_meters_per_second(unsigned long long value) {
	return static_cast<float>(value) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_meter_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_meter_per_second\". For values other than 1, use the plural form \"_meters_per_second\" instead."};
	}
	return operator""_meters_per_second(value);
}

[[nodiscard]] consteval Speed operator""_meter_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_meter_per_second\". For values other than 1, use the plural form \"_meters_per_second\" instead."};
	}
	return operator""_meters_per_second(value);
}

[[nodiscard]] consteval Speed operator""_kilometers_per_second(long double value) {
	return static_cast<float>(value * 1000.0l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_kilometers_per_second(unsigned long long value) {
	return static_cast<float>(value * 1000ull) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_kilometer_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_kilometer_per_second\". For values other than 1, use the plural form \"_kilometers_per_second\" instead."};
	}
	return operator""_kilometers_per_second(value);
}

[[nodiscard]] consteval Speed operator""_kilometer_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_kilometer_per_second\". For values other than 1, use the plural form \"_kilometers_per_second\" instead."};
	}
	return operator""_kilometers_per_second(value);
}

[[nodiscard]] consteval Speed operator""_decimeters_per_second(long double value) {
	return static_cast<float>(value * 0.1l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_decimeters_per_second(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.1l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_decimeter_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_decimeter_per_second\". For values other than 1, use the plural form \"_decimeters_per_second\" instead."};
	}
	return operator""_decimeters_per_second(value);
}

[[nodiscard]] consteval Speed operator""_decimeter_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_decimeter_per_second\". For values other than 1, use the plural form \"_decimeters_per_second\" instead."};
	}
	return operator""_decimeters_per_second(value);
}

[[nodiscard]] consteval Speed operator""_centimeters_per_second(long double value) {
	return static_cast<float>(value * 0.01l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_centimeters_per_second(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.01l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_centimeter_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_centimeter_per_second\". For values other than 1, use the plural form \"_centimeters_per_second\" instead."};
	}
	return operator""_centimeters_per_second(value);
}

[[nodiscard]] consteval Speed operator""_centimeter_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_centimeter_per_second\". For values other than 1, use the plural form \"_centimeters_per_second\" instead."};
	}
	return operator""_centimeters_per_second(value);
}

[[nodiscard]] consteval Speed operator""_millimeters_per_second(long double value) {
	return static_cast<float>(value * 0.001l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_millimeters_per_second(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.001l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_millimeter_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_millimeter_per_second\". For values other than 1, use the plural form \"_millimeters_per_second\" instead."};
	}
	return operator""_millimeters_per_second(value);
}

[[nodiscard]] consteval Speed operator""_millimeter_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_millimeter_per_second\". For values other than 1, use the plural form \"_millimeters_per_second\" instead."};
	}
	return operator""_millimeters_per_second(value);
}

[[nodiscard]] consteval Speed operator""_kilometers_per_hour(long double value) {
	return static_cast<float>(value * 0.277777778l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_kilometers_per_hour(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.277777778l) * METERS_PER_SECOND;
}

[[nodiscard]] consteval Speed operator""_kilometer_per_hour(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_kilometer_per_hour\". For values other than 1, use the plural form \"_kilometers_per_hour\" instead."};
	}
	return operator""_kilometers_per_hour(value);
}

[[nodiscard]] consteval Speed operator""_kilometer_per_hour(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_kilometer_per_hour\". For values other than 1, use the plural form \"_kilometers_per_hour\" instead."};
	}
	return operator""_kilometers_per_hour(value);
}

[[nodiscard]] consteval Acceleration operator""_meters_per_second_squared(long double value) {
	return static_cast<float>(value) * METERS_PER_SECOND_SQUARED;
}

[[nodiscard]] consteval Acceleration operator""_meters_per_second_squared(unsigned long long value) {
	return static_cast<float>(value) * METERS_PER_SECOND_SQUARED;
}

[[nodiscard]] consteval Acceleration operator""_meter_per_second_squared(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_meter_per_second_squared\". For values other than 1, use the plural form \"_meters_per_second_squared\" instead."};
	}
	return operator""_meters_per_second_squared(value);
}

[[nodiscard]] consteval Acceleration operator""_meter_per_second_squared(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_meter_per_second_squared\". For values other than 1, use the plural form \"_meters_per_second_squared\" instead."};
	}
	return operator""_meters_per_second_squared(value);
}

[[nodiscard]] consteval Force1D operator""_Newton(long double value) {
	return static_cast<float>(value) * NEWTON;
}

[[nodiscard]] consteval Force1D operator""_Newton(unsigned long long value) {
	return static_cast<float>(value) * NEWTON;
}

[[nodiscard]] consteval Force1D operator""_kilogram_meters_per_second_squared(long double value) {
	return static_cast<float>(value) * NEWTON;
}

[[nodiscard]] consteval Force1D operator""_kilogram_meters_per_second_squared(unsigned long long value) {
	return static_cast<float>(value) * NEWTON;
}

[[nodiscard]] consteval Force1D operator""_kilogram_meter_per_second_squared(long double value) {
	if (value != 1.0l) {
		throw physics::Error{
			"Syntax error: Expected \"1.0_kilogram_meter_per_second_squared\". For values other than 1, use the plural form \"_kilogram_meters_per_second_squared\" instead."};
	}
	return operator""_kilogram_meters_per_second_squared(value);
}

[[nodiscard]] consteval Force1D operator""_kilogram_meter_per_second_squared(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{
			"Syntax error: Expected \"1_kilogram_meter_per_second_squared\". For values other than 1, use the plural form \"_kilogram_meters_per_second_squared\" instead."};
	}
	return operator""_kilogram_meters_per_second_squared(value);
}

[[nodiscard]] consteval Torque2D operator""_Newton_meters(long double value) {
	return static_cast<float>(value) * NEWTON_METERS;
}

[[nodiscard]] consteval Torque2D operator""_Newton_meters(unsigned long long value) {
	return static_cast<float>(value) * NEWTON_METERS;
}

[[nodiscard]] consteval Torque2D operator""_Newton_meter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_Newton_meter\". For values other than 1, use the plural form \"_Newton_meters\" instead."};
	}
	return operator""_Newton_meters(value);
}

[[nodiscard]] consteval Torque2D operator""_Newton_meter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_Newton_meter\". For values other than 1, use the plural form \"_Newton_meters\" instead."};
	}
	return operator""_Newton_meters(value);
}

[[nodiscard]] consteval Torque2D operator""_kilogram_square_meters_per_second_squared(long double value) {
	return static_cast<float>(value) * NEWTON_METERS;
}

[[nodiscard]] consteval Torque2D operator""_kilogram_square_meters_per_second_squared(unsigned long long value) {
	return static_cast<float>(value) * NEWTON_METERS;
}

[[nodiscard]] consteval Torque2D operator""_kilogram_square_meter_per_second_squared(long double value) {
	if (value != 1.0l) {
		throw physics::Error{
			"Syntax error: Expected \"1.0_kilogram_square_meter_per_second_squared\". For values other than 1, use the plural form "
			"\"_kilogram_square_meters_per_second_squared\" instead."};
	}
	return operator""_kilogram_square_meters_per_second_squared(value);
}

[[nodiscard]] consteval Torque2D operator""_kilogram_square_meter_per_second_squared(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{
			"Syntax error: Expected \"1_kilogram_square_meter_per_second_squared\". For values other than 1, use the plural form \"_kilogram_square_meters_per_second_squared\" "
			"instead."};
	}
	return operator""_kilogram_square_meters_per_second_squared(value);
}

[[nodiscard]] consteval Impulse operator""_Newton_seconds(long double value) {
	return static_cast<float>(value) * NEWTON_SECONDS;
}

[[nodiscard]] consteval Impulse operator""_Newton_seconds(unsigned long long value) {
	return static_cast<float>(value) * NEWTON_SECONDS;
}

[[nodiscard]] consteval Impulse operator""_Newton_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_Newton_second\". For values other than 1, use the plural form \"_Newton_seconds\" instead."};
	}
	return operator""_Newton_seconds(value);
}

[[nodiscard]] consteval Impulse operator""_Newton_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_Newton_second\". For values other than 1, use the plural form \"_Newton_seconds\" instead."};
	}
	return operator""_Newton_seconds(value);
}

[[nodiscard]] consteval Momentum operator""_kilogram_meters_per_second(long double value) {
	return static_cast<float>(value) * KILOGRAM_METERS_PER_SECOND;
}

[[nodiscard]] consteval Momentum operator""_kilogram_meters_per_second(unsigned long long value) {
	return static_cast<float>(value) * KILOGRAM_METERS_PER_SECOND;
}

[[nodiscard]] consteval Momentum operator""_kilogram_meter_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_kilogram_meter_per_second\". For values other than 1, use the plural form \"_kilogram_meters_per_second\" instead."};
	}
	return operator""_kilogram_meters_per_second(value);
}

[[nodiscard]] consteval Momentum operator""_kilogram_meter_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_kilogram_meter_per_second\". For values other than 1, use the plural form \"_kilogram_meters_per_second\" instead."};
	}
	return operator""_kilogram_meters_per_second(value);
}

[[nodiscard]] consteval MomentOfInertia2D operator""_kilogram_square_meters(long double value) {
	return static_cast<float>(value) * KILOGRAM_SQUARE_METERS;
}

[[nodiscard]] consteval MomentOfInertia2D operator""_kilogram_square_meters(unsigned long long value) {
	return static_cast<float>(value) * KILOGRAM_SQUARE_METERS;
}

[[nodiscard]] consteval MomentOfInertia2D operator""_kilogram_square_meter(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_kilogram_square_meter\". For values other than 1, use the plural form \"_kilogram_square_meters\" instead."};
	}
	return operator""_kilogram_square_meters(value);
}

[[nodiscard]] consteval MomentOfInertia2D operator""_kilogram_square_meter(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_kilogram_square_meter\". For values other than 1, use the plural form \"_kilogram_square_meters\" instead."};
	}
	return operator""_kilogram_square_meters(value);
}

[[nodiscard]] consteval AngularAbsement2D operator""_radian_seconds(long double value) {
	return static_cast<float>(value) * RADIAN_SECONDS;
}

[[nodiscard]] consteval AngularAbsement2D operator""_radian_seconds(unsigned long long value) {
	return static_cast<float>(value) * RADIAN_SECONDS;
}

[[nodiscard]] consteval AngularAbsement2D operator""_radian_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_radian_second\". For values other than 1, use the plural form \"_radian_seconds\" instead."};
	}
	return operator""_radian_seconds(value);
}

[[nodiscard]] consteval AngularAbsement2D operator""_radian_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_radian_second\". For values other than 1, use the plural form \"_radian_seconds\" instead."};
	}
	return operator""_radian_seconds(value);
}

[[nodiscard]] consteval AngularAbsement2D operator""_degree_seconds(long double value) {
	return static_cast<float>(value * 0.01745329251994329576923690768489l) * RADIAN_SECONDS;
}

[[nodiscard]] consteval AngularAbsement2D operator""_degree_seconds(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.01745329251994329576923690768489l) * RADIAN_SECONDS;
}

[[nodiscard]] consteval AngularAbsement2D operator""_degree_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_degree_second\". For values other than 1, use the plural form \"_degree_seconds\" instead."};
	}
	return operator""_degree_seconds(value);
}

[[nodiscard]] consteval AngularAbsement2D operator""_degree_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_degree_second\". For values other than 1, use the plural form \"_degree_seconds\" instead."};
	}
	return operator""_degree_seconds(value);
}

[[nodiscard]] consteval AngularAbsement2D operator""_turn_seconds(long double value) {
	return static_cast<float>(value * 6.28318530718l) * RADIAN_SECONDS;
}

[[nodiscard]] consteval AngularAbsement2D operator""_turn_seconds(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 6.28318530718l) * RADIAN_SECONDS;
}

[[nodiscard]] consteval AngularAbsement2D operator""_turn_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_turn_second\". For values other than 1, use the plural form \"_turn_seconds\" instead."};
	}
	return operator""_turn_seconds(value);
}

[[nodiscard]] consteval AngularAbsement2D operator""_turn_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_turn_second\". For values other than 1, use the plural form \"_turn_seconds\" instead."};
	}
	return operator""_turn_seconds(value);
}

[[nodiscard]] consteval AngularVelocity2D operator""_radians_per_second(long double value) {
	return static_cast<float>(value) * RADIANS_PER_SECOND;
}

[[nodiscard]] consteval AngularVelocity2D operator""_radians_per_second(unsigned long long value) {
	return static_cast<float>(value) * RADIANS_PER_SECOND;
}

[[nodiscard]] consteval AngularVelocity2D operator""_radian_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_radian_per_second\". For values other than 1, use the plural form \"_radians_per_second\" instead."};
	}
	return operator""_radians_per_second(value);
}

[[nodiscard]] consteval AngularVelocity2D operator""_radian_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_radian_per_second\". For values other than 1, use the plural form \"_radians_per_second\" instead."};
	}
	return operator""_radians_per_second(value);
}

[[nodiscard]] consteval AngularVelocity2D operator""_degrees_per_second(long double value) {
	return static_cast<float>(value * 0.01745329251994329576923690768489l) * RADIANS_PER_SECOND;
}

[[nodiscard]] consteval AngularVelocity2D operator""_degrees_per_second(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 0.01745329251994329576923690768489l) * RADIANS_PER_SECOND;
}

[[nodiscard]] consteval AngularVelocity2D operator""_degree_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_degree_per_second\". For values other than 1, use the plural form \"_degrees_per_second\" instead."};
	}
	return operator""_degrees_per_second(value);
}

[[nodiscard]] consteval AngularVelocity2D operator""_degree_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_degree_per_second\". For values other than 1, use the plural form \"_degrees_per_second\" instead."};
	}
	return operator""_degrees_per_second(value);
}

[[nodiscard]] consteval AngularVelocity2D operator""_turns_per_second(long double value) {
	return static_cast<float>(value * 6.28318530718l) * RADIANS_PER_SECOND;
}

[[nodiscard]] consteval AngularVelocity2D operator""_turns_per_second(unsigned long long value) {
	return static_cast<float>(static_cast<long double>(value) * 6.28318530718l) * RADIANS_PER_SECOND;
}

[[nodiscard]] consteval AngularVelocity2D operator""_turn_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_turn_per_second\". For values other than 1, use the plural form \"_turns_per_second\" instead."};
	}
	return operator""_turns_per_second(value);
}

[[nodiscard]] consteval AngularVelocity2D operator""_turn_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_turn_per_second\". For values other than 1, use the plural form \"_turns_per_second\" instead."};
	}
	return operator""_turns_per_second(value);
}

[[nodiscard]] consteval AngularAcceleration2D operator""_radians_per_second_squared(long double value) {
	return static_cast<float>(value) * RADIANS_PER_SECOND_SQUARED;
}

[[nodiscard]] consteval AngularAcceleration2D operator""_radians_per_second_squared(unsigned long long value) {
	return static_cast<float>(value) * RADIANS_PER_SECOND_SQUARED;
}

[[nodiscard]] consteval AngularAcceleration2D operator""_radian_per_second_squared(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_radian_per_second_squared\". For values other than 1, use the plural form \"_radians_per_second_squared\" instead."};
	}
	return operator""_radians_per_second_squared(value);
}

[[nodiscard]] consteval AngularAcceleration2D operator""_radian_per_second_squared(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_radian_per_second_squared\". For values other than 1, use the plural form \"_radians_per_second_squared\" instead."};
	}
	return operator""_radians_per_second_squared(value);
}

[[nodiscard]] consteval MassFlowRate operator""_kilograms_per_second(long double value) {
	return static_cast<float>(value) * KILOGRAMS_PER_SECOND;
}

[[nodiscard]] consteval MassFlowRate operator""_kilograms_per_second(unsigned long long value) {
	return static_cast<float>(value) * KILOGRAMS_PER_SECOND;
}

[[nodiscard]] consteval MassFlowRate operator""_kilogram_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_kilogram_per_second\". For values other than 1, use the plural form \"_kilograms_per_second\" instead."};
	}
	return operator""_kilograms_per_second(value);
}

[[nodiscard]] consteval MassFlowRate operator""_kilogram_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_kilogram_per_second\". For values other than 1, use the plural form \"_kilograms_per_second\" instead."};
	}
	return operator""_kilograms_per_second(value);
}

[[nodiscard]] consteval VolumetricFlowRate operator""_cubic_meters_per_second(long double value) {
	return static_cast<float>(value) * CUBIC_METERS_PER_SECOND;
}

[[nodiscard]] consteval VolumetricFlowRate operator""_cubic_meters_per_second(unsigned long long value) {
	return static_cast<float>(value) * CUBIC_METERS_PER_SECOND;
}

[[nodiscard]] consteval VolumetricFlowRate operator""_cubic_meter_per_second(long double value) {
	if (value != 1.0l) {
		throw physics::Error{"Syntax error: Expected \"1.0_cubic_meter_per_second\". For values other than 1, use the plural form \"_cubic_meters_per_second\" instead."};
	}
	return operator""_cubic_meters_per_second(value);
}

[[nodiscard]] consteval VolumetricFlowRate operator""_cubic_meter_per_second(unsigned long long value) {
	if (value != 1ull) {
		throw physics::Error{"Syntax error: Expected \"1_cubic_meter_per_second\". For values other than 1, use the plural form \"_cubic_meters_per_second\" instead."};
	}
	return operator""_cubic_meters_per_second(value);
}

[[nodiscard]] consteval Power operator""_Watts(long double value) {
	return static_cast<float>(value) * WATTS;
}

[[nodiscard]] consteval Power operator""_Watts(unsigned long long value) {
	return static_cast<float>(value) * WATTS;
}

[[nodiscard]] consteval Power operator""_Watt(long double value) {
	return operator""_Watts(value);
}

[[nodiscard]] consteval Power operator""_Watt(unsigned long long value) {
	return operator""_Watts(value);
}

[[nodiscard]] consteval Energy operator""_Joules(long double value) {
	return static_cast<float>(value) * JOULES;
}

[[nodiscard]] consteval Energy operator""_Joules(unsigned long long value) {
	return static_cast<float>(value) * JOULES;
}

[[nodiscard]] consteval Energy operator""_Joule(long double value) {
	return operator""_Joules(value);
}

[[nodiscard]] consteval Energy operator""_Joule(unsigned long long value) {
	return operator""_Joules(value);
}

} // namespace literals

} // namespace grem::physics

namespace grem {
namespace physics_literals = grem::physics::literals; // NOLINT(misc-unused-alias-decls)
} // namespace grem

template <typename UnitT>
struct grem::Formatter<grem::physics::Quantity<1, UnitT>> : Formatter<float> {
	StringView symbolSeparator = " ";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<float>::parseFormatSpecification(p);
		if (*p == 'z') {
			symbolSeparator = {};
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, physics::Quantity<1, UnitT> value) const {
		Formatter<float>::formatTo(output, value._private_getUnderlyingValue());
		const auto symbolString = UnitT::getSymbolString();
		if (!symbolString.empty()) {
			output.append(symbolSeparator);
			output.append(StringView{symbolString});
		}
	}
};

template <typename UnitT>
struct grem::Formatter<grem::physics::Quantity<2, UnitT>> : Formatter<vec2> {
	StringView symbolSeparator = " ";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<vec2>::parseFormatSpecification(p);
		if (*p == 'z') {
			symbolSeparator = {};
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, physics::Quantity<2, UnitT> value) const {
		Formatter<vec2>::formatTo(output, value._private_getUnderlyingValue());
		const auto symbolString = UnitT::getSymbolString();
		if (!symbolString.empty()) {
			output.append(symbolSeparator);
			output.append(StringView{symbolString});
		}
	}
};

template <typename UnitT>
struct grem::Formatter<grem::physics::Quantity<3, UnitT>> : Formatter<vec3> {
	StringView symbolSeparator = " ";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<vec3>::parseFormatSpecification(p);
		if (*p == 'z') {
			symbolSeparator = {};
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, physics::Quantity<3, UnitT> value) const {
		Formatter<vec3>::formatTo(output, value._private_getUnderlyingValue());
		const auto symbolString = UnitT::getSymbolString();
		if (!symbolString.empty()) {
			output.append(symbolSeparator);
			output.append(StringView{symbolString});
		}
	}
};

template <typename UnitT>
struct grem::Formatter<grem::physics::TensorQuantity<2, UnitT>> : Formatter<mat2> {
	StringView symbolSeparator = " ";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<mat2>::parseFormatSpecification(p);
		if (*p == 'z') {
			symbolSeparator = {};
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, physics::TensorQuantity<2, UnitT> value) const {
		Formatter<mat2>::formatTo(output, value._private_value);
		const auto symbolString = UnitT::getSymbolString();
		if (!symbolString.empty()) {
			output.append(symbolSeparator);
			output.append(StringView{symbolString});
		}
	}
};

template <typename UnitT>
struct grem::Formatter<grem::physics::TensorQuantity<3, UnitT>> : Formatter<mat3> {
	StringView symbolSeparator = " ";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<mat3>::parseFormatSpecification(p);
		if (*p == 'z') {
			symbolSeparator = {};
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, physics::TensorQuantity<3, UnitT> value) const {
		Formatter<mat3>::formatTo(output, value._private_value);
		const auto symbolString = UnitT::getSymbolString();
		if (!symbolString.empty()) {
			output.append(symbolSeparator);
			output.append(StringView{symbolString});
		}
	}
};

template <size_t N>
struct grem::Formatter<grem::physics::Direction<N>> : Formatter<grem::physics::Scale<N>> {};

template <size_t N>
struct grem::Formatter<grem::physics::OrthonormalBasis<N>> : Formatter<grem::physics::Basis<N>> {};

template <>
struct grem::Formatter<grem::physics::Orientation<2>> : Formatter<float> {
	StringView symbolSeparator = " ";

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		p = Formatter<float>::parseFormatSpecification(p);
		if (*p == 'z') {
			symbolSeparator = {};
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, physics::Orientation<2> value) const {
		Formatter<float>::formatTo(output, value._private_value);
		output.append(symbolSeparator);
		output.append("rad");
	}
};

template <>
struct grem::Formatter<grem::physics::Orientation<3>> : Formatter<quat> {
	void formatTo(FormatOutput& output, physics::Orientation<3> value) const {
		Formatter<quat>::formatTo(output, value._private_value);
	}
};

template <>
struct grem::Formatter<grem::physics::LocalInertiaOrientation<2>> : Formatter<physics::Orientation<2>> {
	void formatTo(FormatOutput& output, physics::LocalInertiaOrientation<2>) const {
		Formatter<physics::Orientation<2>>::formatTo(output, physics::Orientation<2>{});
	}
};

template <>
struct grem::Formatter<grem::physics::LocalInertiaOrientation<3>> : Formatter<physics::Orientation<3>> {
	void formatTo(FormatOutput& output, physics::LocalInertiaOrientation<3> value) const {
		Formatter<physics::Orientation<3>>::formatTo(output, value);
	}
};

namespace grem::json {

template <typename T>
struct Serializer; // Forward declaration, to avoid including json.hpp.

template <typename T>
struct Deserializer; // Forward declaration, to avoid including json.hpp.

} // namespace grem::json

template <grem::size_t N, typename UnitT>
struct grem::json::Serializer<grem::physics::Quantity<N, UnitT>> {
	void serializeTo(auto& writer, const grem::physics::Quantity<N, UnitT>& value) {
		writer.serialize(value._private_getUnderlyingValue());
	}
};

template <grem::size_t N, typename UnitT>
struct grem::json::Deserializer<grem::physics::Quantity<N, UnitT>> {
	void deserializeFrom(auto& reader, grem::physics::Quantity<N, UnitT>& value) {
		decltype(value._private_getUnderlyingValue()) underlyingValue{};
		reader.deserialize(underlyingValue);
		value = grem::physics::Quantity<N, UnitT>::reinterpret(underlyingValue);
	}
};

#endif
