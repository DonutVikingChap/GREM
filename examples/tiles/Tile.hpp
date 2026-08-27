// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_TILE_HPP
#define GREM_EXAMPLES_TILES_TILE_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>

#include "TileCategories.hpp"

struct TileOptions {
	u8vec2 tilesetPosition{0, 0};     // Range: ([0, 63], [0, 63]).
	uint8_t tilesetIndex = 0;         // Range: [0, 15].
	bool flipX = false;               // Range: [0, 1].
	bool flipY = false;               // Range: [0, 1].
	uint8_t animationFrameStep = 1;   // Range: [1, 4].
	uint8_t animationFrameCountX = 1; // Range: [1, 16].
	uint8_t animationFrameCountY = 1; // Range: [1, 16].
	uint8_t animationFrameRate = 3;   // Range: [3, 48], must be divisible by 3.
};

struct Tile {
	[[nodiscard]] static constexpr Tile create(const TileOptions& options) {
		GREM_ASSERT(options.animationFrameStep > 0);
		GREM_ASSERT(options.animationFrameCountX > 0);
		GREM_ASSERT(options.animationFrameCountY > 0);
		GREM_ASSERT(options.animationFrameRate > 0);
		GREM_ASSERT(options.animationFrameRate % 3 == 0);
		Tile result{.value = 0};
		result.setTilesetPosition(options.tilesetPosition);
		result.setTilesetIndex(options.tilesetIndex);
		result.setFlipX(options.flipX);
		result.setFlipY(options.flipY);
		result.setAnimationFrameStep(options.animationFrameStep);
		result.setAnimationFrameCountX(options.animationFrameCountX);
		result.setAnimationFrameCountY(options.animationFrameCountY);
		result.setAnimationFrameRate(options.animationFrameRate);
		return result;
	}

	uint32_t value;

	constexpr void setTilesetPosition(u8vec2 newTilesetPosition) {
		const uint32_t tilesetX = newTilesetPosition.x;
		const uint32_t tilesetY = newTilesetPosition.y;
		GREM_ASSERT((tilesetX & ~0x3Fu) == 0);
		GREM_ASSERT((tilesetY & ~0x3Fu) == 0);
		value &= ~0x3Fu;
		value |= tilesetX & 0x3Fu; // 6
		value &= ~(0x3Fu << 6);
		value |= (tilesetY & 0x3Fu) << 6; // 6 |6
	}

	constexpr void setTilesetIndex(uint8_t newTilesetIndex) {
		const uint32_t tilesetZ = newTilesetIndex;
		GREM_ASSERT((tilesetZ & ~0xFu) == 0);
		value &= ~(0xFu << 12);
		value |= (tilesetZ & 0xFu) << 12; // 4 |6|6
	}

	constexpr void setFlipX(bool newFlipX) {
		const uint32_t flipX = static_cast<uint32_t>(newFlipX);
		GREM_ASSERT((flipX & ~0x1u) == 0);
		value &= ~(0x1u << 16);
		value |= (flipX & 0x1u) << 16; // 1 |4|6|6
	}

	constexpr void setFlipY(bool newFlipY) {
		const uint32_t flipY = static_cast<uint32_t>(newFlipY);
		GREM_ASSERT((flipY & ~0x1u) == 0);
		value &= ~(0x1u << 17);
		value |= (flipY & 0x1u) << 17; // 1 1|4|6|6
	}

	constexpr void setAnimationFrameStep(uint8_t newAnimationFrameStep) {
		const uint32_t animationFrameStepMinusOne = static_cast<uint32_t>(newAnimationFrameStep - 1);
		GREM_ASSERT((animationFrameStepMinusOne & ~0x3u) == 0);
		value &= ~(0x3u << 18);
		value |= (animationFrameStepMinusOne & 0x3u) << 18; // 2 |1|1|4|6|6
	}

	constexpr void setAnimationFrameCountX(uint8_t newAnimationFrameCountX) {
		const uint32_t animationFrameCountXMinusOne = static_cast<uint32_t>(newAnimationFrameCountX - 1);
		GREM_ASSERT((animationFrameCountXMinusOne & ~0xFu) == 0);
		value &= ~(0xFu << 20);
		value |= (animationFrameCountXMinusOne & 0xFu) << 20; // 4 |2|1|1|4|6|6
	}

	constexpr void setAnimationFrameCountY(uint8_t newAnimationFrameCountY) {
		const uint32_t animationFrameCountYMinusOne = static_cast<uint32_t>(newAnimationFrameCountY - 1);
		GREM_ASSERT((animationFrameCountYMinusOne & ~0xFu) == 0);
		value &= ~(0xFu << 24);
		value |= (animationFrameCountYMinusOne & 0xFu) << 24; // 4 |4|2|1|1|4|6|6
	}

	constexpr void setAnimationFrameRate(uint8_t newAnimationFrameRate) {
		const uint32_t animationFrameRateDividedBy3MinusOne = static_cast<uint32_t>(newAnimationFrameRate / 3 - 1);
		GREM_ASSERT((animationFrameRateDividedBy3MinusOne & ~0xFu) == 0);
		value &= ~(0xFu << 28);
		value |= (animationFrameRateDividedBy3MinusOne & 0xFu) << 28; // 4 |4|4|2|1|1|4|6|6
	}

	[[nodiscard]] constexpr u8vec2 getTilesetPosition() const noexcept {
		const uint32_t tilesetX = value & 0x3Fu;
		const uint32_t tilesetY = (value >> 6) & 0x3Fu;
		return u8vec2{static_cast<uint8_t>(tilesetX), static_cast<uint8_t>(tilesetY)};
	}

	[[nodiscard]] constexpr uint8_t getTilesetIndex() const noexcept {
		const uint32_t tilesetZ = (value >> 12) & 0xFu;
		return static_cast<uint8_t>(tilesetZ);
	}

	[[nodiscard]] constexpr bool getFlipX() const noexcept {
		const uint32_t flipX = (value >> 16) & 0x1u;
		return static_cast<bool>(flipX);
	}

	[[nodiscard]] constexpr bool getFlipY() const noexcept {
		const uint32_t flipY = (value >> 17) & 0x1u;
		return static_cast<bool>(flipY);
	}

	[[nodiscard]] constexpr uint8_t getAnimationFrameStep() const noexcept {
		const uint32_t animationFrameStepMinusOne = (value >> 18) & 0x3u;
		return static_cast<uint8_t>(animationFrameStepMinusOne + 1);
	}

	[[nodiscard]] constexpr uint8_t getAnimationFrameCountX() const noexcept {
		const uint32_t animationFrameCountXMinusOne = (value >> 20) & 0xFu;
		return static_cast<uint8_t>(animationFrameCountXMinusOne + 1);
	}

	[[nodiscard]] constexpr uint8_t getAnimationFrameCountY() const noexcept {
		const uint32_t animationFrameCountYMinusOne = (value >> 24) & 0xFu;
		return static_cast<uint8_t>(animationFrameCountYMinusOne + 1);
	}

	[[nodiscard]] constexpr uint8_t getAnimationFrameRate() const noexcept {
		const uint32_t animationFrameRateDividedBy3MinusOne = (value >> 28) & 0xFu;
		return static_cast<uint8_t>((animationFrameRateDividedBy3MinusOne + 1) * 3);
	}

	[[nodiscard]] constexpr bool operator==(const Tile&) const noexcept = default;
};
static_assert(trivially_copyable<Tile>);

struct CategorizedTile {
	Tile tile{};
	TileCategoryID categoryID{};
};

#endif
