// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_DATA_COLOR_HPP
#define GREM_CORE_DATA_COLOR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>

namespace grem {

/**
 * Floating-point linear RGBA color in the Rec. 709 gamut with 32 bits per
 * component.
 */
class Color {
public:
	/**
	 * Color transfer function.
	 */
	enum class TransferFunction : uint8_t {
		LINEAR, ///< Linear transfer function.
		SRGB,   ///< sRGB transfer function.
	};

	static const Color INVISIBLE;
	static const Color ALICE_BLUE;
	static const Color ANTIQUE_WHITE;
	static const Color AQUA;
	static const Color AQUAMARINE;
	static const Color AZURE;
	static const Color BEIGE;
	static const Color BISQUE;
	static const Color BLACK;
	static const Color BLANCHED_ALMOND;
	static const Color BLUE;
	static const Color BLUE_VIOLET;
	static const Color BROWN;
	static const Color BURLY_WOOD;
	static const Color CADET_BLUE;
	static const Color CHARTREUSE;
	static const Color CHOCOLATE;
	static const Color CORAL;
	static const Color CORNFLOWER_BLUE;
	static const Color CORNSILK;
	static const Color CRIMSON;
	static const Color CYAN;
	static const Color DARK_BLUE;
	static const Color DARK_CYAN;
	static const Color DARK_GOLDEN_ROD;
	static const Color DARK_GRAY;
	static const Color DARK_GREY;
	static const Color DARK_GREEN;
	static const Color DARK_KHAKI;
	static const Color DARK_MAGENTA;
	static const Color DARK_OLIVE_GREEN;
	static const Color DARK_ORANGE;
	static const Color DARK_ORCHID;
	static const Color DARK_RED;
	static const Color DARK_SALMON;
	static const Color DARK_SEA_GREEN;
	static const Color DARK_SLATE_BLUE;
	static const Color DARK_SLATE_GRAY;
	static const Color DARK_SLATE_GREY;
	static const Color DARK_TURQUOISE;
	static const Color DARK_VIOLET;
	static const Color DEEP_PINK;
	static const Color DEEP_SKY_BLUE;
	static const Color DIM_GRAY;
	static const Color DIM_GREY;
	static const Color DODGER_BLUE;
	static const Color FIRE_BRICK;
	static const Color FLORAL_WHITE;
	static const Color FOREST_GREEN;
	static const Color FUCHSIA;
	static const Color GAINSBORO;
	static const Color GHOST_WHITE;
	static const Color GOLD;
	static const Color GOLDEN_ROD;
	static const Color GRAY;
	static const Color GREY;
	static const Color GREEN;
	static const Color GREEN_YELLOW;
	static const Color HONEY_DEW;
	static const Color HOT_PINK;
	static const Color INDIAN_RED;
	static const Color INDIGO;
	static const Color IVORY;
	static const Color KHAKI;
	static const Color LAVENDER;
	static const Color LAVENDER_BLUSH;
	static const Color LAWN_GREEN;
	static const Color LEMON_CHIFFON;
	static const Color LIGHT_BLUE;
	static const Color LIGHT_CORAL;
	static const Color LIGHT_CYAN;
	static const Color LIGHT_GOLDEN_ROD_YELLOW;
	static const Color LIGHT_GRAY;
	static const Color LIGHT_GREY;
	static const Color LIGHT_GREEN;
	static const Color LIGHT_PINK;
	static const Color LIGHT_SALMON;
	static const Color LIGHT_SEA_GREEN;
	static const Color LIGHT_SKY_BLUE;
	static const Color LIGHT_SLATE_GRAY;
	static const Color LIGHT_SLATE_GREY;
	static const Color LIGHT_STEEL_BLUE;
	static const Color LIGHT_YELLOW;
	static const Color LIME;
	static const Color LIME_GREEN;
	static const Color LINEN;
	static const Color MAGENTA;
	static const Color MAROON;
	static const Color MEDIUM_AQUA_MARINE;
	static const Color MEDIUM_BLUE;
	static const Color MEDIUM_ORCHID;
	static const Color MEDIUM_PURPLE;
	static const Color MEDIUM_SEA_GREEN;
	static const Color MEDIUM_SLATE_BLUE;
	static const Color MEDIUM_SPRING_GREEN;
	static const Color MEDIUM_TURQUOISE;
	static const Color MEDIUM_VIOLET_RED;
	static const Color MIDNIGHT_BLUE;
	static const Color MINT_CREAM;
	static const Color MISTY_ROSE;
	static const Color MOCCASIN;
	static const Color NAVAJO_WHITE;
	static const Color NAVY;
	static const Color OLD_LACE;
	static const Color OLIVE;
	static const Color OLIVE_DRAB;
	static const Color ORANGE;
	static const Color ORANGE_RED;
	static const Color ORCHID;
	static const Color PALE_GOLDEN_ROD;
	static const Color PALE_GREEN;
	static const Color PALE_TURQUOISE;
	static const Color PALE_VIOLET_RED;
	static const Color PAPAYA_WHIP;
	static const Color PEACH_PUFF;
	static const Color PERU;
	static const Color PINK;
	static const Color PLUM;
	static const Color POWDER_BLUE;
	static const Color PURPLE;
	static const Color REBECCA_PURPLE;
	static const Color RED;
	static const Color ROSY_BROWN;
	static const Color ROYAL_BLUE;
	static const Color SADDLE_BROWN;
	static const Color SALMON;
	static const Color SANDY_BROWN;
	static const Color SEA_GREEN;
	static const Color SEA_SHELL;
	static const Color SIENNA;
	static const Color SILVER;
	static const Color SKY_BLUE;
	static const Color SLATE_BLUE;
	static const Color SLATE_GRAY;
	static const Color SLATE_GREY;
	static const Color SNOW;
	static const Color SPRING_GREEN;
	static const Color STEEL_BLUE;
	static const Color TAN;
	static const Color TEAL;
	static const Color THISTLE;
	static const Color TOMATO;
	static const Color TURQUOISE;
	static const Color VIOLET;
	static const Color WHEAT;
	static const Color WHITE;
	static const Color WHITE_SMOKE;
	static const Color YELLOW;
	static const Color YELLOW_GREEN;

	/**
	 * Convert a color component in linear space to the sRGB color space.
	 *
	 * \param x normalized linear color component to convert.
	 *
	 * \return the converted normalized sRGB color component value.
	 */
	[[nodiscard]] static float convertLinearToSRGB(float x) {
		return (x <= 0.0031308f) ? x * 12.92f : 1.055f * pow(x, 1.0f / 2.4f) - 0.055f;
	}

	/**
	 * Convert a color in linear space to the sRGB color space.
	 *
	 * \param rgb normalized linear color to convert.
	 *
	 * \return the converted normalized sRGB color value.
	 */
	[[nodiscard]] static vec3 convertLinearToSRGB(vec3 rgb) {
		return {
			convertLinearToSRGB(rgb.x),
			convertLinearToSRGB(rgb.y),
			convertLinearToSRGB(rgb.z),
		};
	}

	/**
	 * Convert a color in linear space to the sRGB color space.
	 *
	 * \param rgba normalized linear color to convert.
	 *
	 * \return the converted normalized sRGB color value.
	 */
	[[nodiscard]] static vec4 convertLinearToSRGB(vec4 rgba) {
		return {
			convertLinearToSRGB(rgba.x),
			convertLinearToSRGB(rgba.y),
			convertLinearToSRGB(rgba.z),
			rgba.w,
		};
	}

	/**
	 * Convert a color component in the sRGB color space to linear space.
	 *
	 * \param x normalized sRGB color component to convert.
	 *
	 * \return the converted normalized linear color component value.
	 */
	[[nodiscard]] static float convertSRGBToLinear(float x) {
		return (x <= 0.04045f) ? x / 12.92f : pow((x + 0.055f) / 1.055f, 2.4f);
	}

	/**
	 * Convert a color in the sRGB color space to linear space.
	 *
	 * \param rgb normalized sRGB color to convert.
	 *
	 * \return the converted normalized linear color value.
	 */
	[[nodiscard]] static vec3 convertSRGBToLinear(vec3 rgb) {
		return {
			convertSRGBToLinear(rgb.x),
			convertSRGBToLinear(rgb.y),
			convertSRGBToLinear(rgb.z),
		};
	}

	/**
	 * Convert a color in the sRGB color space to linear space.
	 *
	 * \param rgba normalized sRGB color to convert.
	 *
	 * \return the converted normalized linear color value.
	 */
	[[nodiscard]] static vec4 convertSRGBToLinear(vec4 rgba) {
		return {
			convertSRGBToLinear(rgba.x),
			convertSRGBToLinear(rgba.y),
			convertSRGBToLinear(rgba.z),
			rgba.w,
		};
	}

	/**
	 * Convert a linear color from straight to pre-multiplied alpha.
	 *
	 * \param rgba normalized linear color with straight alpha to convert.
	 *
	 * \return the converted normalized linear color value with pre-multiplied
	 *         alpha.
	 */
	[[nodiscard]] static vec4 convertStraightToPremultipliedAlpha(vec4 rgba) {
		return vec4{vec3{rgba} * rgba.w, rgba.w};
	}

	/**
	 * Convert a linear color from pre-multiplied to straight alpha.
	 *
	 * \param rgba normalized linear color with pre-multiplied alpha to convert.
	 *
	 * \return the converted normalized linear color value with straight alpha.
	 */
	[[nodiscard]] static vec4 convertPremultipliedToStraightAlpha(vec4 rgba) {
		return vec4{(rgba.w > 0.0001f) ? vec3{rgba} / rgba.w : vec3{}, rgba.w};
	}

	/**
	 * Create a grayscale color from a linear scalar.
	 *
	 * \param x value of the red, green and blue components.
	 * \param a value of the alpha component. Defaults to fully opaque, i.e. a
	 *        value of 1.
	 *
	 * \return a color with the given component values.
	 */
	[[nodiscard]] static constexpr Color fromLinear(float x, float a = 1.0f) noexcept {
		return Color{vec4{x, x, x, a}};
	}

	/**
	 * Create a color from linear RGB components.
	 *
	 * \param r value of the red color component.
	 * \param g value of the green color component.
	 * \param b value of the blue color component.
	 * \param a value of the alpha component. Defaults to fully opaque, i.e. a
	 *        value of 1.
	 *
	 * \return a color with the given component values.
	 */
	[[nodiscard]] static constexpr Color fromLinear(float r, float g, float b, float a = 1.0f) noexcept {
		return Color{vec4{r, g, b, a}};
	}

	/**
	 * Create a color from a vector of 3 components, XYZ, that map to the linear
	 * color components RGB, respectively.
	 *
	 * The alpha component is set to fully opaque, i.e. a value of 1.
	 *
	 * \param rgb input vector containing values for the red, green, and blue
	 *        components.
	 *
	 * \return a color with the given component values.
	 */
	[[nodiscard]] static constexpr Color fromLinear(vec3 rgb) noexcept {
		return Color{vec4{rgb, 1.0f}};
	}

	/**
	 * Construct a color from a vector of 4 components, XYZW, that map to the
	 * linear color components RGBA, respectively.
	 *
	 * \param rgba input vector containing values for the red, green, blue and
	 *        alpha components.
	 *
	 * \return a color with the given component values.
	 */
	[[nodiscard]] static constexpr Color fromLinear(vec4 rgba) noexcept {
		return Color{rgba};
	}

	/**
	 * Create a color from a vector of 3 components, XYZ, and a scalar, A, that
	 * map to the linear color components RGBA, respectively.
	 *
	 * \param rgb input vector containing values for the red, green, and blue
	 *        components.
	 * \param a value of the alpha component.
	 *
	 * \return a color with the given component values.
	 */
	[[nodiscard]] static constexpr Color fromLinear(vec3 rgb, float a) noexcept {
		return Color{vec4{rgb, a}};
	}

	/**
	 * Create a color from sRGB-encoded components.
	 *
	 * \param r sRGB-encoded value of the red color component.
	 * \param g sRGB-encoded value of the green color component.
	 * \param b sRGB-encoded value of the blue color component.
	 * \param a value of the alpha component, which is not sRGB-encoded.
	 *          Defaults to fully opaque.
	 *
	 * \return a linear color corresponding to the given components.
	 */
	[[nodiscard]] static Color fromSRGB(u8norm r, u8norm g, u8norm b, u8norm a = 1.0f) noexcept {
		return fromLinear(          //
			convertSRGBToLinear(r), //
			convertSRGBToLinear(g), //
			convertSRGBToLinear(b), //
			a);
	}

	/**
	 * Create a color from unnormalized sRGB-encoded components.
	 *
	 * \param r unnormalized sRGB-encoded value of the red color component.
	 * \param g unnormalized sRGB-encoded value of the green color component.
	 * \param b unnormalized sRGB-encoded value of the blue color component.
	 * \param a unnormalized value of the alpha component, which is not
	 *          sRGB-encoded. Defaults to fully opaque.
	 *
	 * \return a linear color corresponding to the given components.
	 */
	[[nodiscard]] static Color fromSRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept {
		return fromLinear(                            //
			convertSRGBToLinear(bit_cast<u8norm>(r)), //
			convertSRGBToLinear(bit_cast<u8norm>(g)), //
			convertSRGBToLinear(bit_cast<u8norm>(b)), //
			bit_cast<u8norm>(a));
	}

	/**
	 * Create a color from a vector of 3 sRGB-encoded components, XYZ, and a
	 * scalar, A, that map to the color components RGBA, respectively.
	 *
	 * \param rgb input vector containing sRGB-encoded values for the red,
	 *        green and blue components.
	 * \param a linear value of the alpha component.
	 *
	 * \return a linear color corresponding to the given components.
	 */
	[[nodiscard]] static Color fromSRGB(u8vec3norm rgb, u8norm a = 1.0f) noexcept {
		return fromSRGB(rgb.x, rgb.y, rgb.z, a);
	}

	/**
	 * Create a color from a vector of 3 unnormalized sRGB-encoded components,
	 * XYZ, and a scalar, A, that map to the color components RGBA,
	 * respectively.
	 *
	 * \param rgb input vector containing unnormalized sRGB-encoded values for
	 *        the red, green and blue components.
	 * \param a unnormalized linear value of the alpha component.
	 *
	 * \return a linear color corresponding to the given components.
	 */
	[[nodiscard]] static Color fromSRGB(u8vec3 rgb, uint8_t a = 255) noexcept {
		return fromSRGB(rgb.x, rgb.y, rgb.z, a);
	}

	/**
	 * Create a color from a vector of 3 floating-point sRGB components, XYZ,
	 * and a scalar, A, that map to the color components RGBA, respectively.
	 *
	 * \param rgb input vector containing floating-point sRGB values for the
	 *        red, green and blue components.
	 * \param a linear value of the alpha component.
	 *
	 * \return a linear color corresponding to the given components.
	 */
	[[nodiscard]] static Color fromSRGB(vec3 rgb, float a = 1.0f) noexcept {
		return fromLinear(convertSRGBToLinear(rgb), a);
	}

	/**
	 * Create a color from a vector of 3 sRGB-encoded components, XYZ, and 1
	 * linear component, W, that map to the color components RGBA, respectively.
	 *
	 * \param rgba input vector containing sRGB-encoded values for the red,
	 *        green and blue components, and the linear value of the alpha
	 *        component.
	 *
	 * \return a linear color corresponding to the given components.
	 */
	[[nodiscard]] static Color fromSRGB(u8vec4norm rgba) noexcept {
		return fromSRGB(rgba.x, rgba.y, rgba.z, rgba.w);
	}

	/**
	 * Create a color from a vector of 3 unnormalized sRGB-encoded components,
	 * XYZ, and 1 linear component, W, that map to the color components RGBA,
	 * respectively.
	 *
	 * \param rgba input vector containing unnormalized sRGB-encoded values for
	 *        the red, green and blue components, and the linear value of the
	 *        alpha component.
	 *
	 * \return a linear color corresponding to the given components.
	 */
	[[nodiscard]] static Color fromSRGB(u8vec4 rgba) noexcept {
		return fromSRGB(rgba.x, rgba.y, rgba.z, rgba.w);
	}

	/**
	 * Create a color from a vector of 3 floating-point sRGB components, XYZ,
	 * and 1 linear component, W, that map to the color components RGBA,
	 * respectively.
	 *
	 * \param rgba input vector containing floating-point sRGB values for the
	 *        red, green and blue components, and the linear value of the alpha
	 *        component.
	 *
	 * \return a linear color corresponding to the given components.
	 */
	[[nodiscard]] static Color fromSRGB(vec4 rgba) noexcept {
		return fromLinear(convertSRGBToLinear(rgba));
	}

	/**
	 * Create a color from a linear alpha component.
	 *
	 * \param a value of the alpha component.
	 *
	 * \return a color with a value of 1 in each non-alpha component, and the
	 *         given value in the alpha component.
	 */
	[[nodiscard]] static constexpr Color fromAlpha(float a) noexcept {
		return Color{vec4{1.0f, 1.0f, 1.0f, a}};
	}

	/**
	 * Construct a transparent color with a value of 0 in all components.
	 */
	constexpr Color() noexcept = default;

	/**
	 * Compare this color to another for equality.
	 *
	 * \param other the color to compare this one to.
	 *
	 * \return true if the colors are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const Color& other) const noexcept = default;

	/**
	 * Convert the color to a vector with 3 components, XYZ, that are mapped
	 * from the color components RGB, respectively.
	 *
	 * \return the RGB components of the color as a vector.
	 */
	[[nodiscard]] constexpr vec3 toLinearRGB() const noexcept {
		return vec3{rgba};
	}

	/**
	 * Convert the color to a vector with 4 components, XYZW, that are mapped
	 * from the color components RGBA, respectively.
	 *
	 * \return the RGBA components of the color as a vector.
	 */
	[[nodiscard]] constexpr vec4 toLinearRGBA() const noexcept {
		return rgba;
	}

	/**
	 * Convert the color to a vector with 3 sRGB-encoded components, XYZ, that
	 * are mapped from the color components RGB, respectively.
	 *
	 * \return an SRGB-encoded vector corresponding to the RGB components of the
	 *         color.
	 */
	[[nodiscard]] u8vec3norm toSRGB() const noexcept {
		return u8vec3norm{toFloatSRGB()};
	}

	/**
	 * Convert the color to a vector with 3 floating-point sRGB components, XYZ,
	 * that are mapped from the color components RGB, respectively.
	 *
	 * \return a floating-point SRGB vector corresponding to the RGB components
	 *         of the color.
	 */
	[[nodiscard]] vec3 toFloatSRGB() const noexcept {
		return vec3{toFloatSRGBA()};
	}

	/**
	 * Convert the color to a vector with 3 sRGB-encoded components, XYZ, and 1
	 * linear component, W, that are mapped from the color components RGBA,
	 * respectively.
	 *
	 * \return an SRGB-encoded vector corresponding to the RGBA components of
	 *         the color.
	 */
	[[nodiscard]] u8vec4norm toSRGBA() const noexcept {
		return u8vec4norm{toFloatSRGBA()};
	}

	/**
	 * Convert the color to a vector with 3 floating-point sRGB components, XYZ,
	 * and 1 linear component, W, that are mapped from the color components
	 * RGBA, respectively.
	 *
	 * \return a floating-point SRGB vector corresponding to the RGBA components
	 *         of the color.
	 */
	[[nodiscard]] vec4 toFloatSRGBA() const noexcept {
		return vec4{
			convertLinearToSRGB(rgba.x),
			convertLinearToSRGB(rgba.y),
			convertLinearToSRGB(rgba.z),
			rgba.w,
		};
	}

	/**
	 * Multiply the component values of this color with their respective
	 * component values in another color.
	 *
	 * \param other the other color to multiply this color by.
	 *
	 * \return `*this`, for chaining.
	 */
	constexpr Color& operator*=(const Color& other) noexcept {
		rgba *= other.rgba;
		return *this;
	}

	/**
	 * Divide the component values of this color by their respective component
	 * values in another color.
	 *
	 * \param other the other color to divide this color by.
	 *
	 * \return `*this`, for chaining.
	 */
	constexpr Color& operator/=(const Color& other) noexcept {
		rgba /= other.rgba;
		return *this;
	}

	/**
	 * Multiply the RGB component values of this color by the corresponding
	 * values in a 3-component vector.
	 *
	 * \param rgbCoefficients vector containing the values to multiply the red,
	 *        green, and blue components by.
	 *
	 * \return `*this`, for chaining.
	 */
	constexpr Color& operator*=(vec3 rgbCoefficients) noexcept {
		rgba.x *= rgbCoefficients.x;
		rgba.y *= rgbCoefficients.y;
		rgba.z *= rgbCoefficients.z;
		return *this;
	}

	/**
	 * Multiply the RGBA component values of this color by the corresponding
	 * values in a 4-component vector.
	 *
	 * \param rgbaCoefficients vector containing the values to multiply the red,
	 *        green, blue, and alpha components by.
	 *
	 * \return `*this`, for chaining.
	 */
	constexpr Color& operator*=(vec4 rgbaCoefficients) noexcept {
		rgba *= rgbaCoefficients;
		return *this;
	}

	/**
	 * Divide the RGB component values of this color by the corresponding
	 * values in a 3-component vector.
	 *
	 * \param rgbDenominators vector containing the values to divide the red,
	 *        green, and blue components by.
	 *
	 * \return `*this`, for chaining.
	 */
	constexpr Color& operator/=(vec3 rgbDenominators) noexcept {
		rgba.x /= rgbDenominators.x;
		rgba.y /= rgbDenominators.y;
		rgba.z /= rgbDenominators.z;
		return *this;
	}

	/**
	 * Divide the RGBA component values of this color by the corresponding
	 * values in a 4-component vector.
	 *
	 * \param rgbaDenominators vector containing the values to divide the red,
	 *        green, blue, and alpha components by.
	 *
	 * \return `*this`, for chaining.
	 */
	constexpr Color& operator/=(vec4 rgbaDenominators) noexcept {
		rgba /= rgbaDenominators;
		return *this;
	}

	/**
	 * Get the result of component-wise multiplication between two colors.
	 *
	 * \param a left-hand side of the multiplication.
	 * \param b right-hand side of the multiplication.
	 *
	 * \return a color containing a * b, component-wise.
	 */
	[[nodiscard]] friend constexpr Color operator*(const Color& a, const Color& b) {
		return Color{a.rgba * b.rgba};
	}

	/**
	 * Get the result of component-wise division between two colors.
	 *
	 * \param a left-hand side of the division.
	 * \param b right-hand side of the division.
	 *
	 * \return a color containing a / b, component-wise.
	 */
	[[nodiscard]] friend constexpr Color operator/(const Color& a, const Color& b) {
		return Color{a.rgba / b.rgba};
	}

	/**
	 * Linearly blend between two colors based on an alpha value.
	 *
	 * \param a color to blend from.
	 * \param b color to blend towards.
	 * \param alpha amount to blend each color.
	 *
	 * \return A component-wise linear interpolation of the given colors.
	 */
	[[nodiscard]] friend constexpr Color mix(Color a, Color b, float alpha) {
		return Color::fromLinear(mix(a.rgba, b.rgba, alpha));
	}

private:
	constexpr explicit Color(vec4 rgba) noexcept
		: rgba(rgba) {}

	vec4 rgba{0.0f, 0.0f, 0.0f, 0.0f};
};

// clang-format off
inline constexpr Color Color::INVISIBLE                 {};
inline constexpr Color Color::ALICE_BLUE                {vec4{0.87136712f, 0.93868573f, 1.00000000f, 1.0f}}; // #F0F8FF
inline constexpr Color Color::ANTIQUE_WHITE             {vec4{0.95597335f, 0.83076988f, 0.67954247f, 1.0f}}; // #FAEBD7
inline constexpr Color Color::AQUA                      {vec4{0.00000000f, 1.00000000f, 1.00000000f, 1.0f}}; // #00FFFF
inline constexpr Color Color::AQUAMARINE                {vec4{0.21223076f, 1.00000000f, 0.65837482f, 1.0f}}; // #7FFFD4
inline constexpr Color Color::AZURE                     {vec4{0.87136712f, 1.00000000f, 1.00000000f, 1.0f}}; // #F0FFFF
inline constexpr Color Color::BEIGE                     {vec4{0.91309865f, 0.91309865f, 0.71569350f, 1.0f}}; // #F5F5DC
inline constexpr Color Color::BISQUE                    {vec4{1.00000000f, 0.77582222f, 0.55201140f, 1.0f}}; // #FFE4C4
inline constexpr Color Color::BLACK                     {vec4{0.00000000f, 0.00000000f, 0.00000000f, 1.0f}}; // #000000
inline constexpr Color Color::BLANCHED_ALMOND           {vec4{1.00000000f, 0.83076988f, 0.61049557f, 1.0f}}; // #FFEBCD
inline constexpr Color Color::BLUE                      {vec4{0.00000000f, 0.00000000f, 1.00000000f, 1.0f}}; // #0000FF
inline constexpr Color Color::BLUE_VIOLET               {vec4{0.25415209f, 0.02415763f, 0.76052450f, 1.0f}}; // #8A2BE2
inline constexpr Color Color::BROWN                     {vec4{0.37626212f, 0.02315337f, 0.02315337f, 1.0f}}; // #A52A2A
inline constexpr Color Color::BURLY_WOOD                {vec4{0.73046074f, 0.47932018f, 0.24228112f, 1.0f}}; // #DEB887
inline constexpr Color Color::CADET_BLUE                {vec4{0.11443537f, 0.34191442f, 0.35153260f, 1.0f}}; // #5F9EA0
inline constexpr Color Color::CHARTREUSE                {vec4{0.21223076f, 1.00000000f, 0.00000000f, 1.0f}}; // #7FFF00
inline constexpr Color Color::CHOCOLATE                 {vec4{0.64447968f, 0.14126329f, 0.01298303f, 1.0f}}; // #D2691E
inline constexpr Color Color::CORAL                     {vec4{1.00000000f, 0.21223076f, 0.08021982f, 1.0f}}; // #FF7F50
inline constexpr Color Color::CORNFLOWER_BLUE           {vec4{0.12743768f, 0.30054379f, 0.84687323f, 1.0f}}; // #6495ED
inline constexpr Color Color::CORNSILK                  {vec4{1.00000000f, 0.93868573f, 0.71569350f, 1.0f}}; // #FFF8DC
inline constexpr Color Color::CRIMSON                   {vec4{0.71569350f, 0.00699541f, 0.04518620f, 1.0f}}; // #DC143C
inline constexpr Color Color::CYAN                      {vec4{0.00000000f, 1.00000000f, 1.00000000f, 1.0f}}; // #00FFFF
inline constexpr Color Color::DARK_BLUE                 {vec4{0.00000000f, 0.00000000f, 0.25818285f, 1.0f}}; // #00008B
inline constexpr Color Color::DARK_CYAN                 {vec4{0.00000000f, 0.25818285f, 0.25818285f, 1.0f}}; // #008B8B
inline constexpr Color Color::DARK_GOLDEN_ROD           {vec4{0.47932018f, 0.23839757f, 0.00334654f, 1.0f}}; // #B8860B
inline constexpr Color Color::DARK_GRAY                 {vec4{0.39675523f, 0.39675523f, 0.39675523f, 1.0f}}; // #A9A9A9
inline constexpr Color Color::DARK_GREY                 {vec4{0.39675523f, 0.39675523f, 0.39675523f, 1.0f}}; // #A9A9A9
inline constexpr Color Color::DARK_GREEN                {vec4{0.00000000f, 0.12743768f, 0.00000000f, 1.0f}}; // #006400
inline constexpr Color Color::DARK_KHAKI                {vec4{0.50888132f, 0.47353150f, 0.14702727f, 1.0f}}; // #BDB76B
inline constexpr Color Color::DARK_MAGENTA              {vec4{0.25818285f, 0.00000000f, 0.25818285f, 1.0f}}; // #8B008B
inline constexpr Color Color::DARK_OLIVE_GREEN          {vec4{0.09084171f, 0.14702727f, 0.02842604f, 1.0f}}; // #556B2F
inline constexpr Color Color::DARK_ORANGE               {vec4{1.00000000f, 0.26225066f, 0.00000000f, 1.0f}}; // #FF8C00
inline constexpr Color Color::DARK_ORCHID               {vec4{0.31854678f, 0.03189603f, 0.60382734f, 1.0f}}; // #9932CC // NOLINT(modernize-use-std-numbers)
inline constexpr Color Color::DARK_RED                  {vec4{0.25818285f, 0.00000000f, 0.00000000f, 1.0f}}; // #8B0000
inline constexpr Color Color::DARK_SALMON               {vec4{0.81484657f, 0.30498731f, 0.19461783f, 1.0f}}; // #E9967A
inline constexpr Color Color::DARK_SEA_GREEN            {vec4{0.27467731f, 0.50288646f, 0.27467731f, 1.0f}}; // #8FBC8F
inline constexpr Color Color::DARK_SLATE_BLUE           {vec4{0.06480327f, 0.04666509f, 0.25818285f, 1.0f}}; // #483D8B
inline constexpr Color Color::DARK_SLATE_GRAY           {vec4{0.02842604f, 0.07818742f, 0.07818742f, 1.0f}}; // #2F4F4F
inline constexpr Color Color::DARK_SLATE_GREY           {vec4{0.02842604f, 0.07818742f, 0.07818742f, 1.0f}}; // #2F4F4F
inline constexpr Color Color::DARK_TURQUOISE            {vec4{0.00000000f, 0.61720656f, 0.63759687f, 1.0f}}; // #00CED1
inline constexpr Color Color::DARK_VIOLET               {vec4{0.29613827f, 0.00000000f, 0.65140564f, 1.0f}}; // #9400D3
inline constexpr Color Color::DEEP_PINK                 {vec4{1.00000000f, 0.00699541f, 0.29177065f, 1.0f}}; // #FF1493
inline constexpr Color Color::DEEP_SKY_BLUE             {vec4{0.00000000f, 0.52099557f, 1.00000000f, 1.0f}}; // #00BFFF
inline constexpr Color Color::DIM_GRAY                  {vec4{0.14126329f, 0.14126329f, 0.14126329f, 1.0f}}; // #696969
inline constexpr Color Color::DIM_GREY                  {vec4{0.14126329f, 0.14126329f, 0.14126329f, 1.0f}}; // #696969
inline constexpr Color Color::DODGER_BLUE               {vec4{0.01298303f, 0.27889426f, 1.00000000f, 1.0f}}; // #1E90FF
inline constexpr Color Color::FIRE_BRICK                {vec4{0.44520119f, 0.01599629f, 0.01599629f, 1.0f}}; // #B22222
inline constexpr Color Color::FLORAL_WHITE              {vec4{1.00000000f, 0.95597335f, 0.87136712f, 1.0f}}; // #FFFAF0
inline constexpr Color Color::FOREST_GREEN              {vec4{0.01599629f, 0.25818285f, 0.01599629f, 1.0f}}; // #228B22
inline constexpr Color Color::FUCHSIA                   {vec4{1.00000000f, 0.00000000f, 1.00000000f, 1.0f}}; // #FF00FF
inline constexpr Color Color::GAINSBORO                 {vec4{0.71569350f, 0.71569350f, 0.71569350f, 1.0f}}; // #DCDCDC
inline constexpr Color Color::GHOST_WHITE               {vec4{0.93868573f, 0.93868573f, 1.00000000f, 1.0f}}; // #F8F8FF
inline constexpr Color Color::GOLD                      {vec4{1.00000000f, 0.67954247f, 0.00000000f, 1.0f}}; // #FFD700
inline constexpr Color Color::GOLDEN_ROD                {vec4{0.70110189f, 0.37626212f, 0.01444384f, 1.0f}}; // #DAA520
inline constexpr Color Color::GRAY                      {vec4{0.21586050f, 0.21586050f, 0.21586050f, 1.0f}}; // #808080
inline constexpr Color Color::GREY                      {vec4{0.21586050f, 0.21586050f, 0.21586050f, 1.0f}}; // #808080
inline constexpr Color Color::GREEN                     {vec4{0.00000000f, 0.21586050f, 0.00000000f, 1.0f}}; // #008000
inline constexpr Color Color::GREEN_YELLOW              {vec4{0.41788507f, 1.00000000f, 0.02842604f, 1.0f}}; // #ADFF2F
inline constexpr Color Color::HONEY_DEW                 {vec4{0.87136712f, 1.00000000f, 0.87136712f, 1.0f}}; // #F0FFF0
inline constexpr Color Color::HOT_PINK                  {vec4{1.00000000f, 0.14126329f, 0.45641102f, 1.0f}}; // #FF69B4
inline constexpr Color Color::INDIAN_RED                {vec4{0.61049557f, 0.10702310f, 0.10702310f, 1.0f}}; // #CD5C5C
inline constexpr Color Color::INDIGO                    {vec4{0.07036010f, 0.00000000f, 0.22322796f, 1.0f}}; // #4B0082
inline constexpr Color Color::IVORY                     {vec4{1.00000000f, 1.00000000f, 0.87136712f, 1.0f}}; // #FFFFF0
inline constexpr Color Color::KHAKI                     {vec4{0.87136712f, 0.79129794f, 0.26225066f, 1.0f}}; // #F0E68C
inline constexpr Color Color::LAVENDER                  {vec4{0.79129794f, 0.79129794f, 0.95597335f, 1.0f}}; // #E6E6FA
inline constexpr Color Color::LAVENDER_BLUSH            {vec4{1.00000000f, 0.87136712f, 0.91309865f, 1.0f}}; // #FFF0F5
inline constexpr Color Color::LAWN_GREEN                {vec4{0.20155625f, 0.97344529f, 0.00000000f, 1.0f}}; // #7CFC00
inline constexpr Color Color::LEMON_CHIFFON             {vec4{1.00000000f, 0.95597335f, 0.61049557f, 1.0f}}; // #FFFACD
inline constexpr Color Color::LIGHT_BLUE                {vec4{0.41788507f, 0.68668531f, 0.79129794f, 1.0f}}; // #ADD8E6
inline constexpr Color Color::LIGHT_CORAL               {vec4{0.87136712f, 0.21586050f, 0.21586050f, 1.0f}}; // #F08080
inline constexpr Color Color::LIGHT_CYAN                {vec4{0.74540421f, 1.00000000f, 1.00000000f, 1.0f}}; // #E0FFFF
inline constexpr Color Color::LIGHT_GOLDEN_ROD_YELLOW   {vec4{0.95597335f, 0.95597335f, 0.64447968f, 1.0f}}; // #FAFAD2
inline constexpr Color Color::LIGHT_GRAY                {vec4{0.65140564f, 0.65140564f, 0.65140564f, 1.0f}}; // #D3D3D3
inline constexpr Color Color::LIGHT_GREY                {vec4{0.65140564f, 0.65140564f, 0.65140564f, 1.0f}}; // #D3D3D3
inline constexpr Color Color::LIGHT_GREEN               {vec4{0.27889426f, 0.85499261f, 0.27889426f, 1.0f}}; // #90EE90
inline constexpr Color Color::LIGHT_PINK                {vec4{1.00000000f, 0.46778380f, 0.53327640f, 1.0f}}; // #FFB6C1
inline constexpr Color Color::LIGHT_SALMON              {vec4{1.00000000f, 0.35153260f, 0.19461783f, 1.0f}}; // #FFA07A
inline constexpr Color Color::LIGHT_SEA_GREEN           {vec4{0.01444384f, 0.44520119f, 0.40197778f, 1.0f}}; // #20B2AA
inline constexpr Color Color::LIGHT_SKY_BLUE            {vec4{0.24228112f, 0.61720656f, 0.95597335f, 1.0f}}; // #87CEFA
inline constexpr Color Color::LIGHT_SLATE_GRAY          {vec4{0.18447499f, 0.24620133f, 0.31854678f, 1.0f}}; // #778899 // NOLINT(modernize-use-std-numbers)
inline constexpr Color Color::LIGHT_SLATE_GREY          {vec4{0.18447499f, 0.24620133f, 0.31854678f, 1.0f}}; // #778899 // NOLINT(modernize-use-std-numbers)
inline constexpr Color Color::LIGHT_STEEL_BLUE          {vec4{0.43415364f, 0.55201140f, 0.73046074f, 1.0f}}; // #B0C4DE // NOLINT(modernize-use-std-numbers)
inline constexpr Color Color::LIGHT_YELLOW              {vec4{1.00000000f, 1.00000000f, 0.74540421f, 1.0f}}; // #FFFFE0
inline constexpr Color Color::LIME                      {vec4{0.00000000f, 1.00000000f, 0.00000000f, 1.0f}}; // #00FF00
inline constexpr Color Color::LIME_GREEN                {vec4{0.03189603f, 0.61049557f, 0.03189603f, 1.0f}}; // #32CD32
inline constexpr Color Color::LINEN                     {vec4{0.95597335f, 0.87136712f, 0.79129794f, 1.0f}}; // #FAF0E6
inline constexpr Color Color::MAGENTA                   {vec4{1.00000000f, 0.00000000f, 1.00000000f, 1.0f}}; // #FF00FF
inline constexpr Color Color::MAROON                    {vec4{0.21586050f, 0.00000000f, 0.00000000f, 1.0f}}; // #800000
inline constexpr Color Color::MEDIUM_AQUA_MARINE        {vec4{0.13286832f, 0.61049557f, 0.40197778f, 1.0f}}; // #66CDAA
inline constexpr Color Color::MEDIUM_BLUE               {vec4{0.00000000f, 0.00000000f, 0.61049557f, 1.0f}}; // #0000CD
inline constexpr Color Color::MEDIUM_ORCHID             {vec4{0.49102085f, 0.09084171f, 0.65140564f, 1.0f}}; // #BA55D3
inline constexpr Color Color::MEDIUM_PURPLE             {vec4{0.29177065f, 0.16202938f, 0.70837578f, 1.0f}}; // #9370DB
inline constexpr Color Color::MEDIUM_SEA_GREEN          {vec4{0.04518620f, 0.45078578f, 0.16513219f, 1.0f}}; // #3CB371
inline constexpr Color Color::MEDIUM_SLATE_BLUE         {vec4{0.19806932f, 0.13843162f, 0.85499261f, 1.0f}}; // #7B68EE
inline constexpr Color Color::MEDIUM_SPRING_GREEN       {vec4{0.00000000f, 0.95597335f, 0.32314321f, 1.0f}}; // #00FA9A
inline constexpr Color Color::MEDIUM_TURQUOISE          {vec4{0.06480327f, 0.63759687f, 0.60382734f, 1.0f}}; // #48D1CC
inline constexpr Color Color::MEDIUM_VIOLET_RED         {vec4{0.57112483f, 0.00749903f, 0.23455058f, 1.0f}}; // #C71585
inline constexpr Color Color::MIDNIGHT_BLUE             {vec4{0.00972122f, 0.00972122f, 0.16202938f, 1.0f}}; // #191970
inline constexpr Color Color::MINT_CREAM                {vec4{0.91309865f, 1.00000000f, 0.95597335f, 1.0f}}; // #F5FFFA
inline constexpr Color Color::MISTY_ROSE                {vec4{1.00000000f, 0.77582222f, 0.75294222f, 1.0f}}; // #FFE4E1
inline constexpr Color Color::MOCCASIN                  {vec4{1.00000000f, 0.77582222f, 0.46207700f, 1.0f}}; // #FFE4B5
inline constexpr Color Color::NAVAJO_WHITE              {vec4{1.00000000f, 0.73046074f, 0.41788507f, 1.0f}}; // #FFDEAD
inline constexpr Color Color::NAVY                      {vec4{0.00000000f, 0.00000000f, 0.21586050f, 1.0f}}; // #000080
inline constexpr Color Color::OLD_LACE                  {vec4{0.98225055f, 0.91309865f, 0.79129794f, 1.0f}}; // #FDF5E6
inline constexpr Color Color::OLIVE                     {vec4{0.21586050f, 0.21586050f, 0.00000000f, 1.0f}}; // #808000
inline constexpr Color Color::OLIVE_DRAB                {vec4{0.14702727f, 0.27049779f, 0.01680738f, 1.0f}}; // #6B8E23
inline constexpr Color Color::ORANGE                    {vec4{1.00000000f, 0.37626212f, 0.00000000f, 1.0f}}; // #FFA500
inline constexpr Color Color::ORANGE_RED                {vec4{1.00000000f, 0.05951124f, 0.00000000f, 1.0f}}; // #FF4500
inline constexpr Color Color::ORCHID                    {vec4{0.70110189f, 0.16202938f, 0.67244316f, 1.0f}}; // #DA70D6
inline constexpr Color Color::PALE_GOLDEN_ROD           {vec4{0.85499261f, 0.80695226f, 0.40197778f, 1.0f}}; // #EEE8AA
inline constexpr Color Color::PALE_GREEN                {vec4{0.31398871f, 0.96468625f, 0.31398871f, 1.0f}}; // #98FB98
inline constexpr Color Color::PALE_TURQUOISE            {vec4{0.42869050f, 0.85499261f, 0.85499261f, 1.0f}}; // #AFEEEE
inline constexpr Color Color::PALE_VIOLET_RED           {vec4{0.70837578f, 0.16202938f, 0.29177065f, 1.0f}}; // #DB7093
inline constexpr Color Color::PAPAYA_WHIP               {vec4{1.00000000f, 0.86315721f, 0.66538730f, 1.0f}}; // #FFEFD5
inline constexpr Color Color::PEACH_PUFF                {vec4{1.00000000f, 0.70110189f, 0.48514994f, 1.0f}}; // #FFDAB9
inline constexpr Color Color::PERU                      {vec4{0.61049557f, 0.23455058f, 0.04970657f, 1.0f}}; // #CD853F
inline constexpr Color Color::PINK                      {vec4{1.00000000f, 0.52711513f, 0.59720179f, 1.0f}}; // #FFC0CB
inline constexpr Color Color::PLUM                      {vec4{0.72305513f, 0.35153260f, 0.72305513f, 1.0f}}; // #DDA0DD
inline constexpr Color Color::POWDER_BLUE               {vec4{0.43415364f, 0.74540421f, 0.79129794f, 1.0f}}; // #B0E0E6 // NOLINT(modernize-use-std-numbers)
inline constexpr Color Color::PURPLE                    {vec4{0.21586050f, 0.00000000f, 0.21586050f, 1.0f}}; // #800080
inline constexpr Color Color::REBECCA_PURPLE            {vec4{0.13286832f, 0.03310477f, 0.31854678f, 1.0f}}; // #663399 // NOLINT(modernize-use-std-numbers)
inline constexpr Color Color::RED                       {vec4{1.00000000f, 0.00000000f, 0.00000000f, 1.0f}}; // #FF0000
inline constexpr Color Color::ROSY_BROWN                {vec4{0.50288646f, 0.27467731f, 0.27467731f, 1.0f}}; // #BC8F8F
inline constexpr Color Color::ROYAL_BLUE                {vec4{0.05286065f, 0.14126329f, 0.75294222f, 1.0f}}; // #4169E1
inline constexpr Color Color::SADDLE_BROWN              {vec4{0.25818285f, 0.05951124f, 0.00651209f, 1.0f}}; // #8B4513
inline constexpr Color Color::SALMON                    {vec4{0.95597335f, 0.21586050f, 0.16826940f, 1.0f}}; // #FA8072
inline constexpr Color Color::SANDY_BROWN               {vec4{0.90466117f, 0.37123768f, 0.11697067f, 1.0f}}; // #F4A460
inline constexpr Color Color::SEA_GREEN                 {vec4{0.02732089f, 0.25818285f, 0.09530747f, 1.0f}}; // #2E8B57
inline constexpr Color Color::SEA_SHELL                 {vec4{1.00000000f, 0.91309865f, 0.85499261f, 1.0f}}; // #FFF5EE
inline constexpr Color Color::SIENNA                    {vec4{0.35153260f, 0.08437621f, 0.02624122f, 1.0f}}; // #A0522D
inline constexpr Color Color::SILVER                    {vec4{0.52711513f, 0.52711513f, 0.52711513f, 1.0f}}; // #C0C0C0
inline constexpr Color Color::SKY_BLUE                  {vec4{0.24228112f, 0.61720656f, 0.83076988f, 1.0f}}; // #87CEEB
inline constexpr Color Color::SLATE_BLUE                {vec4{0.14412847f, 0.10224173f, 0.61049557f, 1.0f}}; // #6A5ACD
inline constexpr Color Color::SLATE_GRAY                {vec4{0.16202938f, 0.21586050f, 0.27889426f, 1.0f}}; // #708090
inline constexpr Color Color::SLATE_GREY                {vec4{0.16202938f, 0.21586050f, 0.27889426f, 1.0f}}; // #708090
inline constexpr Color Color::SNOW                      {vec4{1.00000000f, 0.95597335f, 0.95597335f, 1.0f}}; // #FFFAFA
inline constexpr Color Color::SPRING_GREEN              {vec4{0.00000000f, 1.00000000f, 0.21223076f, 1.0f}}; // #00FF7F
inline constexpr Color Color::STEEL_BLUE                {vec4{0.06124605f, 0.22322796f, 0.45641102f, 1.0f}}; // #4682B4
inline constexpr Color Color::TAN                       {vec4{0.64447968f, 0.45641102f, 0.26225066f, 1.0f}}; // #D2B48C
inline constexpr Color Color::TEAL                      {vec4{0.00000000f, 0.21586050f, 0.21586050f, 1.0f}}; // #008080
inline constexpr Color Color::THISTLE                   {vec4{0.68668531f, 0.52099557f, 0.68668531f, 1.0f}}; // #D8BFD8
inline constexpr Color Color::TOMATO                    {vec4{1.00000000f, 0.12477182f, 0.06301002f, 1.0f}}; // #FF6347
inline constexpr Color Color::TURQUOISE                 {vec4{0.05126946f, 0.74540421f, 0.63075714f, 1.0f}}; // #40E0D0
inline constexpr Color Color::VIOLET                    {vec4{0.85499261f, 0.22322796f, 0.85499261f, 1.0f}}; // #EE82EE
inline constexpr Color Color::WHEAT                     {vec4{0.91309865f, 0.73046074f, 0.45078578f, 1.0f}}; // #F5DEB3
inline constexpr Color Color::WHITE                     {vec4{1.00000000f, 1.00000000f, 1.00000000f, 1.0f}}; // #FFFFFF
inline constexpr Color Color::WHITE_SMOKE               {vec4{0.91309865f, 0.91309865f, 0.91309865f, 1.0f}}; // #F5F5F5
inline constexpr Color Color::YELLOW                    {vec4{1.00000000f, 1.00000000f, 0.00000000f, 1.0f}}; // #FFFF00
inline constexpr Color Color::YELLOW_GREEN              {vec4{0.32314321f, 0.61049557f, 0.03189603f, 1.0f}}; // #9ACD32
// clang-format on

} // namespace grem

#endif
