// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_TILES_LAYERS_MENU_LAYER_HPP
#define GREM_EXAMPLES_TILES_LAYERS_MENU_LAYER_HPP

#include <GREM/aliases.hpp>
#include <GREM/core.hpp>
#include <GREM/events.hpp>
#include <GREM/graphics.hpp>
#include <GREM/graphics_2d.hpp>

#include "../Graphics.hpp"
#include "../Layer.hpp"

#include <utility> // std::move

class MenuLayer final : public Layer {
public:
	enum class Action : uint8_t {
		CONFIRM,
		CANCEL,
		NEXT_ITEM,
		PREVIOUS_ITEM,
	};

	struct Item {
		String label;
		Optional<Continuation> onSelect;
		Function<String()> getValueString{};
	};

	using Items = ArrayList<Item>;

	MenuLayer(const Filesystem& filesystem, String title, Optional<Continuation> onCancel, Items items)
		: title(std::move(title))
		, onCancel(onCancel)
		, items(std::move(items))
		, itemBoundingBoxes(this->items.size()) {
		inputManager.loadConfiguration<Action>(filesystem, "configuration/menu.json");
	}

	~MenuLayer() override = default;

	void prepareForEvents() override {
		GREM_PROFILE_FUNCTION();

		inputManager.update();
	}

	bool handleEvent(const Graphics& graphics, const evt::Event& event) override {
		GREM_PROFILE_FUNCTION();

		inputManager.handleEvent(event);

		GREM_MATCH(event) {
			GREM_CASE(const evt::MouseButtonPressedEvent& pressed) {
				if (pressed.mouseButton == evt::MouseButton::LEFT) {
					hoveredItemIndex.reset();
					const vec2 position = graphics.convertScreenToRenderCoordinates(pressed.mousePosition);
					for (size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
						if (itemBoundingBoxes[itemIndex].contains(position)) {
							lastHoveredItemIndex = itemIndex;
							hoveredItemIndex = itemIndex;
							selectedItemIndex = itemIndex;
							return true;
						}
					}
				}
				break;
			}
			GREM_CASE(const evt::MouseMovedEvent& moved) {
				hoveredItemIndex.reset();
				const vec2 position = graphics.convertScreenToRenderCoordinates(moved.mousePosition);
				for (size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
					if (itemBoundingBoxes[itemIndex].contains(position)) {
						lastHoveredItemIndex = itemIndex;
						hoveredItemIndex = itemIndex;
						break;
					}
				}
				break;
			}
		}
		return false;
	}

	Continuation update(const Graphics&, Duration) override {
		GREM_PROFILE_FUNCTION();

		const bool next = inputManager.justPressed(Action::NEXT_ITEM);
		const bool previous = inputManager.justPressed(Action::PREVIOUS_ITEM);
		if (next && !previous) {
			if (!hoveredItemIndex) {
				hoveredItemIndex = lastHoveredItemIndex;
			}
			if (*hoveredItemIndex + 1 < items.size()) {
				++*hoveredItemIndex;
				lastHoveredItemIndex = *hoveredItemIndex;
			}
		} else if (previous && !next) {
			if (!hoveredItemIndex) {
				hoveredItemIndex = lastHoveredItemIndex;
			}
			if (*hoveredItemIndex > 0) {
				--*hoveredItemIndex;
				lastHoveredItemIndex = *hoveredItemIndex;
			}
		}

		if (inputManager.justPressed(Action::CANCEL)) {
			if (onCancel) {
				return *onCancel;
			}
		}

		if (inputManager.justPressed(Action::CONFIRM)) {
			selectedItemIndex = hoveredItemIndex;
		}

		if (selectedItemIndex) {
			const size_t itemIndex = *selectedItemIndex;
			selectedItemIndex.reset();
			if (const Optional<Continuation> onSelect = items[itemIndex].onSelect) {
				return *onSelect;
			}
		}

		return BreakFromLayerStack{};
	}

	void draw(gfx::Device&, Graphics& graphics, gfx::RenderPass& renderPass, float, size_t) override {
		GREM_PROFILE_FUNCTION();

		graphics.instances2D.clear();

		vec2 position{u32vec2{graphics.renderSize.width / 2, graphics.renderSize.height / 2 + graphics.renderSize.height / 4}};

		graphics.put2DText(position, Color::WHITE, title, 3.0f, gfx::TextAlign::CENTER);
		position.y -= 100.0f;

		for (size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
			const Item& item = items[itemIndex];
			const Color color = (itemIndex == hoveredItemIndex) ? Color::WHITE : Color::GRAY;
			String labelWithValue{};
			StringView label = item.label;
			if (item.getValueString) {
				labelWithValue = formatString("{}: {}", label, item.getValueString());
				label = labelWithValue;
			}
			itemBoundingBoxes[itemIndex] = graphics.put2DText(position, color, label, 2.0f, gfx::TextAlign::CENTER);
			position.y -= 40.0f;
		}

		graphics.renderer2D.drawFrame(renderPass, {graphics.instances2D}, graphics.camera2D);
	}

private:
	String title;
	Optional<Continuation> onCancel;
	Items items;
	ArrayList<Box<2, float>> itemBoundingBoxes;
	evt::InputManager inputManager{};
	size_t lastHoveredItemIndex = 0;
	Optional<size_t> hoveredItemIndex = size_t{0};
	Optional<size_t> selectedItemIndex{};
};

#endif
