// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_GAME_SYSTEMS_HPP
#define GREM_EXAMPLES_FPS_GAME_SYSTEMS_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/Error.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/execution/resource.hpp>

#include "NamedType.hpp"
#include "System.hpp"

#include <stdexcept> // std::invalid_argument

struct SystemType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const SystemType&) const = default;
	[[nodiscard]] auto operator<=>(const SystemType&) const = default;
};

struct SystemsLayerType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const SystemsLayerType&) const = default;
	[[nodiscard]] auto operator<=>(const SystemsLayerType&) const = default;
};

struct StateResourceType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const StateResourceType&) const = default;
	[[nodiscard]] auto operator<=>(const StateResourceType&) const = default;
};

struct IntermediateResourceType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const IntermediateResourceType&) const = default;
	[[nodiscard]] auto operator<=>(const IntermediateResourceType&) const = default;
};

struct ClientsideResourceType : NamedType {
	using NamedType::NamedType;

	[[nodiscard]] bool operator==(const ClientsideResourceType&) const = default;
	[[nodiscard]] auto operator<=>(const ClientsideResourceType&) const = default;
};

struct StateResourceDescription {
	template <exec::resource T>
	[[nodiscard]] static constexpr StateResourceDescription create(CStringView name) noexcept {
		return {
			.name = name,
			.add = [](ResourceRegistry& resources) -> void { resources.addResource<T>(); },
			.remove = [](ResourceRegistry& resources) noexcept -> void { resources.removeResource<T>(); },
			.hasStateDelta = [](const ResourceRegistry& oldResources, const ResourceRegistry& newResources) -> bool {
				return oldResources.getResource<T>() != newResources.getResource<T>();
			},
			.serializeState = [](const ResourceRegistry& resources, Writer output) -> void { serialize(resources.getResource<T>(), output); },
			.deserializeState = [](ResourceRegistry& resources, SpanReader input) -> void {
				if (!deserialize(resources.getResource<T>(), input)) {
					throw std::invalid_argument{"Invalid resource layout."};
				}
			},
		};
	}

	CStringView name;
	void (*add)(ResourceRegistry& resources);
	void (*remove)(ResourceRegistry& resources) noexcept;
	bool (*hasStateDelta)(const ResourceRegistry& oldResources, const ResourceRegistry& newResources);
	void (*serializeState)(const ResourceRegistry& resources, Writer output);
	void (*deserializeState)(ResourceRegistry& resources, SpanReader input);
};

struct IntermediateResourceDescription {
	template <exec::resource T>
	[[nodiscard]] static constexpr IntermediateResourceDescription create(CStringView name) noexcept {
		return {
			.name = name,
			.add = [](ResourceRegistry& resources) -> void { resources.addSharedResource<T>(); },
			.remove = [](ResourceRegistry& resources) noexcept -> void { resources.removeResource<T>(); },
		};
	}

	CStringView name;
	void (*add)(ResourceRegistry& resources);
	void (*remove)(ResourceRegistry& resources) noexcept;
};

struct ClientsideResourceDescription {
	template <exec::resource T>
	[[nodiscard]] static constexpr ClientsideResourceDescription create(CStringView name) noexcept {
		return {
			.name = name,
			.add = [](ResourceRegistry& resources) -> void { resources.addSharedResource<T>(); },
			.remove = [](ResourceRegistry& resources) noexcept -> void { resources.removeResource<T>(); },
		};
	}

	CStringView name;
	void (*add)(ResourceRegistry& resources);
	void (*remove)(ResourceRegistry& resources) noexcept;
};

struct SystemsLayer {
	ArrayList<System*> systemList{};
	ArrayList<StateResourceType> stateResources{};
	ArrayList<IntermediateResourceType> intermediateResources{};
	ArrayList<ClientsideResourceType> clientsideResources{};
};

class GameSystems {
public:
	GameSystems(const Filesystem& filesystem, CStringView filepath);

	[[nodiscard]] auto getStateResourceDescriptions() const noexcept {
		return Subrange{stateResourceDescriptions};
	}

	[[nodiscard]] const StateResourceDescription* findStateResourceDescription(StateResourceType stateResourceType) const noexcept {
		if (const auto it = stateResourceDescriptions.find(stateResourceType); it != stateResourceDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const StateResourceDescription& getStateResourceDescription(StateResourceType stateResourceType) const {
		const StateResourceDescription* const result = findStateResourceDescription(stateResourceType);
		if (!result) {
			throw Error{"Specified StateResourceType missing from systems."};
		}
		return *result;
	}

	[[nodiscard]] auto getIntermediateResourceDescriptions() const noexcept {
		return Subrange{intermediateResourceDescriptions};
	}

	[[nodiscard]] const IntermediateResourceDescription* findIntermediateResourceDescription(IntermediateResourceType intermediateResourceType) const noexcept {
		if (const auto it = intermediateResourceDescriptions.find(intermediateResourceType); it != intermediateResourceDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const IntermediateResourceDescription& getIntermediateResourceDescription(IntermediateResourceType intermediateResourceType) const {
		const IntermediateResourceDescription* const result = findIntermediateResourceDescription(intermediateResourceType);
		if (!result) {
			throw Error{"Specified IntermediateResourceType missing from systems."};
		}
		return *result;
	}

	[[nodiscard]] auto getClientsideResourceDescriptions() const noexcept {
		return Subrange{clientsideResourceDescriptions};
	}

	[[nodiscard]] const ClientsideResourceDescription* findClientsideResourceDescription(ClientsideResourceType clientsideResourceType) const noexcept {
		if (const auto it = clientsideResourceDescriptions.find(clientsideResourceType); it != clientsideResourceDescriptions.end()) {
			return &it->second;
		}
		return nullptr;
	}

	[[nodiscard]] const ClientsideResourceDescription& getClientsideResourceDescription(ClientsideResourceType clientsideResourceType) const {
		const ClientsideResourceDescription* const result = findClientsideResourceDescription(clientsideResourceType);
		if (!result) {
			throw Error{"Specified ClientsideResourceType missing from systems."};
		}
		return *result;
	}

	[[nodiscard]] System& getSystem(SystemType systemType) const {
		const auto it = systems.find(systemType);
		if (it == systems.end()) {
			throw Error{"Missing specified SystemType."};
		}
		return *it->second;
	}

	[[nodiscard]] const SystemsLayer& getLayer(SystemsLayerType systemsLayerType) const {
		const auto it = layers.find(systemsLayerType);
		if (it == layers.end()) {
			throw Error{"Missing specified SystemsLayerType."};
		}
		return it->second;
	}

private:
	HashMap<StateResourceType, StateResourceDescription> stateResourceDescriptions{};
	HashMap<IntermediateResourceType, IntermediateResourceDescription> intermediateResourceDescriptions{};
	HashMap<ClientsideResourceType, ClientsideResourceDescription> clientsideResourceDescriptions{};
	HashMap<SystemType, UniquePointer<System>> systems{};
	HashMap<SystemsLayerType, SystemsLayer> layers{};
};

#endif
