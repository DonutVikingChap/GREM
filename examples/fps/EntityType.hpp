// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_ENTITY_TYPE_HPP
#define GREM_EXAMPLES_FPS_ENTITY_TYPE_HPP

#include "NamedType.hpp"

struct EntityType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const EntityType&) const = default;
	[[nodiscard]] auto operator<=>(const EntityType&) const = default;
};

#endif
