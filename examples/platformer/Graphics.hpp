// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_PLATFORMER_GRAPHICS_HPP
#define GREM_EXAMPLES_PLATFORMER_GRAPHICS_HPP

#include <GREM/GREM.hpp>
#include <GREM/aliases.hpp>

#include "WorldView.hpp"
#include "shaders.hpp"

#include <utility> // std::move

struct GraphicsOptions {
	phys::Quantity<1, phys::Degrees> fov = 74_degrees;
	String font{};
	uint32_t fontSize = 16;
};

struct Graphics {
	[[nodiscard]] static phys::Box3D getBoundingBox(const gfx::Model3D& model, res::Model::TransformationView transformation) {
		Box<3, float> boundingBox{.min{Limits<float>::MAX}, .max{Limits<float>::MIN}};
		for (const gfx::Model3D::Node& node : model.getNodes()) {
			const mat4 jointMatrix = transformation.jointMatrices[((node.shaderConfiguration.vertexFlags & res::Model::VERTEX_SKINNED) != 0) ? 0 : node.jointIndex];
			const Box<3, float> nodeBoundingBox = getTransformedBoundingBox(jointMatrix, node.boundingBox);
			boundingBox.min = min(boundingBox.min, nodeBoundingBox.min);
			boundingBox.max = max(boundingBox.max, nodeBoundingBox.max);
		}
		return boundingBox * phys::METERS;
	}

	[[nodiscard]] static phys::Box3D getBindPoseBoundingBox(const gfx::Model3D& model, const mat4& transformation) {
		return getTransformedBoundingBox(transformation, model.getBindPoseBoundingBox()) * phys::METERS;
	}

	GraphicsOptions options;
	gfx::Device device;
	gfx::Swapchain swapchain;
	gfx::Renderer2D renderer2D;
	gfx::Renderer3D renderer3D;
	gfx::Camera2D camera2D;
	gfx::Camera3D camera3D;
	gfx::Viewport viewport{};
	gfx::Font2D mainFont;
	gfx::Text2D temporaryText{};
	gfx::SpriteAtlas spriteAtlas;
	gfx::Instances2D instances2D;
	gfx::Instances3D instances3D;
	PlatformerModelShaderPipelineSet modelShaderPipelineSet;
	PlatformerModelShaderBuffers modelShaderBuffers;
	ArrayList<PlatformerModelShaderLight> lights{};
	ArrayList<PlatformerModelShaderBlobShadow> blobShadows{};
	Buffer<ArrayList<PlatformerModelShaderItem>> itemChunks{};
	ArrayList<PlatformerModelShaderItem> items{};
	ArrayList<PlatformerModelShaderTile> tiles{};
	LooseQuadtree<uint32_t> blobShadowIndexQuadtree{};

	Graphics(Filesystem& filesystem, gfx::Window& window, const gfx::Renderer2DOptions& renderer2DOptions, const gfx::Renderer3DOptions& renderer3DOptions, GraphicsOptions options)
		: options(std::move(options))
		, device(filesystem, window)
		, swapchain(device, window)
		, renderer2D(device, renderer2DOptions)
		, renderer3D(device, renderer2D, renderer3DOptions)
		, camera2D(device)
		, camera3D(device)
		, mainFont(filesystem, this->options.font)
		, temporaryText()
		, spriteAtlas(device)
		, instances2D(device, renderer2D)
		, instances3D(device, renderer3D)
		, modelShaderPipelineSet(loadModelShader(filesystem))
		, modelShaderBuffers(device) {}

	void resize(Extent2D newDrawableSize) {
		viewport.region.size = newDrawableSize;
		camera2D.setProjection(gfx::OrthographicProjection2D{.size = newDrawableSize});
		camera3D.setProjection(gfx::PerspectiveProjection3D{
			.verticalFieldOfView = options.fov.in(phys::RADIANS),
			.aspectRatio = newDrawableSize.getAspectRatio(),
			.nearZ = 0.1f,
			.farZ = 5000.0f,
		});
	}

	void reloadShaders(const Filesystem& filesystem) {
		modelShaderPipelineSet = loadModelShader(filesystem);
	}

	void clearInstances() noexcept {
		instances2D.clear();
		instances3D.clear();
		lights.clear();
		blobShadows.clear();
	}

	void put2DText(Offset2D position, Color color, StringView string, float scale = 1.0f, gfx::TextAlign alignment = {}) {
		temporaryText.assign(mainFont, options.fontSize, string);
		instances2D.putTextInstance(temporaryText, {.position = position + Offset2D{1, -1}, .scale{scale}, .alignment = alignment, .color = Color::BLACK});
		instances2D.putTextInstance(temporaryText, {.position = position, .scale{scale}, .alignment = alignment, .color = color});
	}

	void flushModelShaderBuffers(exec::Executor& executor, Extent2D framebufferSize, const WorldView& worldView) {
		GREM_PROFILE_FUNCTION();

		constexpr vec2 TILE_SIZE{static_cast<float>(PlatformerModelShaderTile::TILE_SIZE)};
		const vec2 viewportSize{viewport.region.size};
		const mat4 inverseViewProjectionMatrix = inverse(camera3D.getProjectionMatrix() * camera3D.getViewMatrix());
		const u32vec2 tileCounts = (u32vec2{viewport.region.size} + u32vec2{PlatformerModelShaderTile::TILE_SIZE - 1}) / PlatformerModelShaderTile::TILE_SIZE;

		{
			GREM_PROFILE_BLOCK("Upload parameters");
			modelShaderBuffers.upload<PlatformerModelShaderParameterBuffer>(PlatformerModelShaderParameters{
				.modelShaderViewportOffset{viewport.region.offset},
				.modelShaderFramebufferHeight = static_cast<float>(framebufferSize.height + (framebufferSize.height & 1)),
				.modelShaderTileCounts = tileCounts,
				.modelShaderInverseTileSize = 1.0f / static_cast<float>(PlatformerModelShaderTile::TILE_SIZE),
			});
		}

		{
			GREM_PROFILE_BLOCK("Upload lights");
			modelShaderBuffers.upload<PlatformerModelShaderLightBuffer>(lights);
		}

		{
			GREM_PROFILE_BLOCK("Upload blob shadows");
			modelShaderBuffers.upload<PlatformerModelShaderBlobShadowBuffer>(blobShadows);
		}

		{
			GREM_PROFILE_BLOCK("Build blob shadow quadtree");
			const Box<2, float> horizontalBounds{
				.min{worldView.visibleBounds.min.x, worldView.visibleBounds.min.z},
				.max{worldView.visibleBounds.max.x, worldView.visibleBounds.max.z},
			};
			blobShadowIndexQuadtree.reset(horizontalBounds, 1.0f);
			for (uint32_t blobShadowIndex = 0; blobShadowIndex < blobShadows.size(); ++blobShadowIndex) {
				const PlatformerModelShaderBlobShadow& blobShadow = blobShadows[blobShadowIndex];
				blobShadowIndexQuadtree.insert(blobShadow.getHorizontalBoundingBox(), blobShadowIndex);
			}
		}

		{
			GREM_PROFILE_BLOCK("Build tile grid of items");

			const size_t chunkCount = static_cast<size_t>(executor.getMaxParallelism());
			itemChunks.clear();
			itemChunks.resize(chunkCount);

			const size_t tileCount = static_cast<size_t>(tileCounts.x) * static_cast<size_t>(tileCounts.y);
			tiles.clear();
			tiles.resize(tileCount);

			const size_t chunkSize = (tileCount + chunkCount - 1) / chunkCount;
			executor.executeParallelIndexedOperation(0, chunkCount, [&](size_t chunkIndex) -> void {
				const size_t chunkBegin = min(chunkIndex * chunkSize, tileCount);
				const size_t chunkEnd = min(chunkBegin + chunkSize, tileCount);
				ArrayList<PlatformerModelShaderItem>& itemChunk = itemChunks[chunkIndex];
				itemChunk.clear();
				for (size_t tileIndex = chunkBegin; tileIndex < chunkEnd; ++tileIndex) {
					const size_t tileY = tileIndex / static_cast<size_t>(tileCounts.x);
					const size_t tileX = tileIndex % static_cast<size_t>(tileCounts.x);

					Frustum<float> tileFrustum{};
					{
						GREM_PROFILE_BLOCK("Build frustum");
						const float x = static_cast<float>(tileX) * TILE_SIZE.x;
						const float y = static_cast<float>(tileY) * TILE_SIZE.y;
						const Array<vec3, 8> tileFrustumCornersInViewportRelativeScreenSpace{
							vec3{vec2{x, y}, 1.0f},
							vec3{vec2{x, y + TILE_SIZE.y}, 1.0f},
							vec3{vec2{x + TILE_SIZE.x, y}, 1.0f},
							vec3{vec2{x + TILE_SIZE.x, y + TILE_SIZE.y}, 1.0f},
							vec3{vec2{x, y}, 0.0f},
							vec3{vec2{x, y + TILE_SIZE.y}, 0.0f},
							vec3{vec2{x + TILE_SIZE.x, y}, 0.0f},
							vec3{vec2{x + TILE_SIZE.x, y + TILE_SIZE.y}, 0.0f},
						};
						const Array<vec3, 8> tileFrustumCornersInWorldSpace =
							meta::transform(tileFrustumCornersInViewportRelativeScreenSpace, [&](vec3 cornerInViewportRelativeScreenSpace) -> vec3 {
								const vec4 cornerInNDCSpace{
									(vec2{cornerInViewportRelativeScreenSpace} / viewportSize) * 2.0f - vec2{1.0f},
									cornerInViewportRelativeScreenSpace.z,
									1.0f,
								};
								const vec4 cornerInWorldSpace = inverseViewProjectionMatrix * cornerInNDCSpace;
								return vec3{cornerInWorldSpace} / cornerInWorldSpace.w;
							});
						tileFrustum = Frustum<float>::fromCorners(tileFrustumCornersInWorldSpace);
					}

					const uint32_t itemOffset = static_cast<uint32_t>(itemChunk.size());

					uint8_t lightCount = 0;
					{
						GREM_PROFILE_BLOCK("Test lights");
						for (uint32_t lightIndex = 0; lightCount < 255 && lightIndex < lights.size(); ++lightIndex) {
							const PlatformerModelShaderLight& light = lights[lightIndex];
							if (light.lightTypeAndRangeAndConeCosines.x == PlatformerModelShaderLight::LIGHT_TYPE_DIRECTIONAL ||
								tileFrustum.isPotentiallyIntersecting(Sphere<3, float>{.center = vec3{light.lightPosition}, .radius = light.lightTypeAndRangeAndConeCosines.y})) {
								itemChunk.push_back(PlatformerModelShaderItem{.itemIndex = lightIndex});
								++lightCount;
							}
						}
					}

					uint8_t blobShadowCount = 0;
					{
						GREM_PROFILE_BLOCK("Test blob shadow quadtree");
						blobShadowIndexQuadtree.traverseElements(
							[&](uint32_t blobShadowIndex) -> bool {
								if (blobShadowCount >= 255) {
									return true;
								}
								const PlatformerModelShaderBlobShadow& blobShadow = blobShadows[blobShadowIndex];
								if (tileFrustum.isPotentiallyIntersecting(blobShadow.getBoundingBox(worldView.visibleBounds))) {
									itemChunk.push_back(PlatformerModelShaderItem{.itemIndex = blobShadowIndex});
									++blobShadowCount;
								}
								return false;
							},
							[&](const Box<2, float>& box) -> bool {
								return tileFrustum.isPotentiallyIntersecting(Box<3, float>{
									.min{box.min.x, worldView.visibleBounds.min.y, box.min.y},
									.max{box.max.x, worldView.visibleBounds.max.y, box.max.y},
								});
							});
					}

					tiles[tileIndex] = PlatformerModelShaderTile{
						.itemOffset = itemOffset,
						.itemCounts = (uint32_t{lightCount} << 24) | (uint32_t{blobShadowCount} << 16),
					};
				}
			});

			uint32_t itemOffset = 0;
			for (size_t chunkIndex = 0; chunkIndex < itemChunks.size(); ++chunkIndex) {
				const size_t chunkBegin = min(chunkIndex * chunkSize, tileCount);
				const size_t chunkEnd = min(chunkBegin + chunkSize, tileCount);
				for (size_t tileIndex = chunkBegin; tileIndex < chunkEnd; ++tileIndex) {
					tiles[tileIndex].itemOffset += itemOffset;
				}
				itemOffset += static_cast<uint32_t>(itemChunks[chunkIndex].size());
			}

			items.clear();
			for (const ArrayList<PlatformerModelShaderItem>& itemChunk : itemChunks) {
				items.append_range(itemChunk);
			}
		}

		{
			GREM_PROFILE_BLOCK("Upload items");
			modelShaderBuffers.upload<PlatformerModelShaderItemBuffer>(items);
		}

		{
			GREM_PROFILE_BLOCK("Upload tiles");
			modelShaderBuffers.upload<PlatformerModelShaderTileBuffer>(tiles);
		}
	}

	[[nodiscard]] PlatformerModelShaderPipelineSet loadModelShader(const Filesystem& filesystem) {
		return PlatformerModelShaderPipelineSet{
			device,
			renderer3D.getDefaultModel3DVertexShader(),
			gfx::Model3D::DEFAULT_VERTEX_SHADER_CONSTANTS,
			PlatformerModelFragmentShader{device, filesystem, "shaders/model.frag"},
			[](const gfx::Model3D::ShaderConfiguration& shaderConfiguration) -> PlatformerModelFragmentShaderConstants {
				const gfx::Model3D::FragmentShaderConstants constants = gfx::Model3D::DEFAULT_FRAGMENT_SHADER_CONSTANTS(shaderConfiguration);
				return {
					.FRAGMENT_HDR = constants.FRAGMENT_HDR,
					.FRAGMENT_ALPHA_MASKED = constants.FRAGMENT_ALPHA_MASKED,
					.FRAGMENT_ALPHA_BLENDED = constants.FRAGMENT_ALPHA_BLENDED,
					.FRAGMENT_DOUBLE_SIDED = constants.FRAGMENT_DOUBLE_SIDED,
					.FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0 = constants.FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_0,
					.FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1 = constants.FRAGMENT_BASE_COLOR_MAPPED_ON_CHANNEL_1,
					.FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0 = constants.FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_0,
					.FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1 = constants.FRAGMENT_METALLIC_ROUGHNESS_MAPPED_ON_CHANNEL_1,
					.FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0 = constants.FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_0,
					.FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1 = constants.FRAGMENT_OCCLUSION_MAPPED_ON_CHANNEL_1,
					.FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0 = constants.FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_0,
					.FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1 = constants.FRAGMENT_NORMAL_MAPPED_ON_CHANNEL_1,
					.FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0 = constants.FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_0,
					.FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1 = constants.FRAGMENT_EMISSIVE_MAPPED_ON_CHANNEL_1,
					.LIGHT_TYPE_DIRECTIONAL = PlatformerModelShaderLight::LIGHT_TYPE_DIRECTIONAL,
					.LIGHT_TYPE_POINT = PlatformerModelShaderLight::LIGHT_TYPE_POINT,
					.LIGHT_TYPE_SPOT = PlatformerModelShaderLight::LIGHT_TYPE_SPOT,
				};
			},
			gfx::Model3D::DEFAULT_SHADER_PIPELINE_OPTIONS,
		};
	}

	[[nodiscard]] ArrayList<gfx::SpriteID> loadSpriteAnimation(const Filesystem& filesystem, CStringView filepath, size_t frameCount, vec2 frameSize) {
		ArrayList<gfx::SpriteID> result{};
		const gfx::SpriteID spriteID = spriteAtlas.insertSprite(res::Image{filesystem, filepath});
		const vec2 spriteSize = spriteAtlas.getSprite(spriteID).size;
		const uint32_t frameWidth = static_cast<uint32_t>(frameSize.x * spriteSize.x);
		const uint32_t frameHeight = static_cast<uint32_t>(frameSize.y * spriteSize.y);
		const uint32_t frameCountX = static_cast<uint32_t>(spriteSize.x) / frameWidth;
		const uint32_t frameCountY = static_cast<uint32_t>(spriteSize.y) / frameHeight;
		for (uint32_t y = 0; y < frameCountY; ++y) {
			if (result.size() >= frameCount) {
				break;
			}
			const uint32_t offsetY = static_cast<uint32_t>(spriteSize.y) - frameHeight * (1 + y);
			for (uint32_t x = 0; x < frameCountX; ++x) {
				if (result.size() >= frameCount) {
					break;
				}
				const uint32_t offsetX = frameWidth * x;
				result.push_back(spriteAtlas.createSubSprite(spriteID,
					Region2D{
						.offset{static_cast<int32_t>(offsetX), static_cast<int32_t>(offsetY)},
						.size{frameWidth, frameHeight},
					}));
			}
		}
		if (result.empty()) {
			throw Error{"No frames in sprite animation."};
		}
		return result;
	}
};

#endif
