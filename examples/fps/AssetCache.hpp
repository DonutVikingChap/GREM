// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_ASSET_CACHE_HPP
#define GREM_EXAMPLES_FPS_ASSET_CACHE_HPP

#include <GREM/aliases.hpp>
#include <GREM/audio/Sound.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Filesystem.hpp>
#include <GREM/core/system/synchronization.hpp>
#include <GREM/graphics/Device.hpp>
#include <GREM/graphics/Texture.hpp>
#include <GREM/graphics/shaders.hpp>
#include <GREM/graphics_2d/Font2D.hpp>
#include <GREM/graphics_3d/Model3D.hpp>
#include <GREM/graphics_3d/Renderer3D.hpp>
#include <GREM/resource/Image.hpp>
#include <GREM/resource/Model.hpp>

#include "Prefab.hpp"

#include <exception> // std::exception_ptr, std::current_exception, std::rethrow_exception
#include <typeindex> // std::type_index

class Schema;

class AssetCache {
public:
	explicit AssetCache(Filesystem& filesystem)
		: filesystem(filesystem) {}

	[[nodiscard]] Filesystem& getFilesystem() {
		return filesystem;
	}

	void cleanup() noexcept {
		GREM_PROFILE_FUNCTION();

		static constexpr auto assetExpired = [](const auto& kv) -> bool {
			if (!kv.second) {
				return true;
			}
			kv.second->loaded.wait(false, MemoryOrder::ACQUIRE);
			return kv.second->asset.use_count() <= 1;
		};

		ScopedLock lock{mutex};
		erase_if(images, assetExpired);
		erase_if(models, assetExpired);
		erase_if(vertexShaders, assetExpired);
		erase_if(fragmentShaders, assetExpired);
		erase_if(textures, assetExpired);
		erase_if(model3Ds, assetExpired);
		erase_if(fonts, assetExpired);
		erase_if(sounds, assetExpired);
		erase_if(prefabs, assetExpired);
	}

	void clear() noexcept {
		ScopedLock lock{mutex};
		images.clear();
		models.clear();
		vertexShaders.clear();
		fragmentShaders.clear();
		textures.clear();
		model3Ds.clear();
		fonts.clear();
		sounds.clear();
		prefabs.clear();
	}

	[[nodiscard]] const SharedPointer<res::Image>& getImage(CStringView filepath, const res::ImageOptions& options = {}) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<res::Image>>(images, AssetKeyProxy<res::ImageOptions>{filepath, options},
			[&] { return SharedPointer<res::Image>::create(filesystem, filepath, options); });
	}

	[[nodiscard]] const SharedPointer<res::Model>& getModel(CStringView filepath, const res::ModelOptions& options = {}) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<res::Model>>(models, AssetKeyProxy<res::ModelOptions>{filepath, options},
			[&] { return SharedPointer<res::Model>::create(filesystem, filepath, options); });
	}

	template <typename VertexShader>
	[[nodiscard]] const SharedPointer<gfx::VertexShaderImplementation>& getVertexShader(gfx::Device& device, CStringView filepath) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<gfx::VertexShaderImplementation>>(vertexShaders, AssetKeyProxy<std::type_index>{filepath, typeid(VertexShader)},
			[&] { return VertexShader{device, filesystem, filepath}.lock(); });
	}

	template <typename FragmentShader>
	[[nodiscard]] const SharedPointer<gfx::FragmentShaderImplementation>& getFragmentShader(gfx::Device& device, CStringView filepath) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<gfx::FragmentShaderImplementation>>(fragmentShaders, AssetKeyProxy<std::type_index>{filepath, typeid(FragmentShader)},
			[&] { return FragmentShader{device, filesystem, filepath}.lock(); });
	}

	[[nodiscard]] const SharedPointer<gfx::Texture>& getTexture(gfx::Device& device, CStringView filepath, const res::ImageOptions& imageOptions = {},
		const gfx::TextureImageUploadOptions& textureImageUploadOptions = {}, Optional<gfx::TextureSamplerOptions> textureSamplerOptions = {}) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<gfx::Texture>>(textures,
			AssetKeyProxy<TextureOptions>{filepath, TextureOptions{imageOptions, textureImageUploadOptions, textureSamplerOptions}},
			[&] { return SharedPointer<gfx::Texture>::create(device, *getImage(filepath, imageOptions), textureImageUploadOptions, textureSamplerOptions); });
	}

	[[nodiscard]] const SharedPointer<gfx::Model3D>& getModel3D(gfx::Device& device, gfx::Renderer3D& renderer3D, CStringView filepath, const res::ModelOptions& modelOptions = {},
		const gfx::Model3DOptions& model3DOptions = {}) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<gfx::Model3D>>(model3Ds, AssetKeyProxy<Model3DOptions>{filepath, Model3DOptions{modelOptions, model3DOptions}}, [&] {
			GREM_PROFILE_BLOCK_DYNAMIC(formatString("Upload model {}", filepath));
			return SharedPointer<gfx::Model3D>::create(device, renderer3D, *getModel(filepath, modelOptions), model3DOptions);
		});
	}

	[[nodiscard]] const SharedPointer<gfx::Font2D>& getFont(CStringView filepath, const gfx::Font2DOptions& options = {}) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<gfx::Font2D>>(fonts, AssetKeyProxy<gfx::Font2DOptions>{filepath, options},
			[&] { return SharedPointer<gfx::Font2D>::create(filesystem, filepath, options); });
	}

	[[nodiscard]] const SharedPointer<aud::Sound>& getSound(CStringView filepath, const aud::SoundOptions& options = {}) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<aud::Sound>>(sounds, AssetKeyProxy<aud::SoundOptions>{filepath, options},
			[&] { return SharedPointer<aud::Sound>::create(filesystem, filepath, options); });
	}

	[[nodiscard]] const SharedPointer<Prefab>& getPrefab(Schema& schema, CStringView filepath) {
		GREM_PROFILE_FUNCTION();

		return getAsset<SharedPointer<Prefab>>(prefabs, filepath, [&] { return SharedPointer<Prefab>::create(*this, schema, filepath); });
	}

private:
	template <typename Options>
	struct AssetKeyProxy {
		CStringView filepath;
		const Options& options;
	};

	template <typename Options>
	struct AssetKey {
		struct Hash {
			using is_transparent = void;

			[[nodiscard]] size_t operator()(AssetKeyProxy<Options> key) const {
				return getHash(key.filepath);
			}
		};

		struct Equal {
			using is_transparent = void;

			[[nodiscard]] bool operator()(AssetKeyProxy<Options> a, AssetKeyProxy<Options> b) const {
				return a.filepath == b.filepath && a.options == b.options;
			}
		};

		String filepath;
		Options options;

		AssetKey(const AssetKeyProxy<Options>& key)
			: filepath(key.filepath)
			, options(key.options) {}

		operator AssetKeyProxy<Options>() const noexcept {
			return {.filepath = filepath, .options = options};
		}
	};

	template <typename Asset>
	struct AssetStorage {
		Asset asset{};
		AtomicFlag loaded{};
		std::exception_ptr loadError{};
	};

	template <typename Options, typename Asset>
	using AssetMap = HashMap<AssetKey<Options>, SharedPointer<AssetStorage<Asset>>, typename AssetKey<Options>::Hash, typename AssetKey<Options>::Equal>;

	struct TransparentCStringHash {
		using is_transparent = void;

		[[nodiscard]] size_t operator()(CStringView string) const {
			return getHash(string);
		}
	};

	struct TransparentCStringKeyEqual {
		using is_transparent = void;

		[[nodiscard]] bool operator()(CStringView a, CStringView b) const {
			return a == b;
		}
	};

	struct TextureOptions {
		res::ImageOptions imageOptions;
		gfx::TextureImageUploadOptions textureImageUploadOptions;
		Optional<gfx::TextureSamplerOptions> textureSamplerOptions;

		[[nodiscard]] constexpr bool operator==(const TextureOptions&) const = default;
	};

	struct Model3DOptions {
		res::ModelOptions modelOptions;
		gfx::Model3DOptions model3DOptions;

		[[nodiscard]] constexpr bool operator==(const Model3DOptions&) const = default;
	};

	template <typename Asset>
	[[nodiscard]] const Asset& getAsset(auto& map, const auto& key, auto load) {
		SharedPointer<AssetStorage<Asset>> storage{};
		bool shouldLoad = false;
		{
			ScopedLock lock{mutex};
			SharedPointer<AssetStorage<Asset>>& v = map[key];
			if (!v) {
				v = SharedPointer<AssetStorage<Asset>>::create();
				shouldLoad = true;
			}
			storage = v;
		}
		if (shouldLoad) {
			try {
				storage->asset = load();
			} catch (...) {
				storage->loadError = std::current_exception();
			}
			storage->loaded.test_and_set(MemoryOrder::RELEASE);
			storage->loaded.notify_all();
		} else {
			storage->loaded.wait(false, MemoryOrder::ACQUIRE);
		}
		if (storage->loadError) {
			std::rethrow_exception(storage->loadError);
		}
		return storage->asset;
	}

	Filesystem& filesystem;
	Mutex mutex{};
	AssetMap<res::ImageOptions, SharedPointer<res::Image>> images{};
	AssetMap<res::ModelOptions, SharedPointer<res::Model>> models{};
	AssetMap<std::type_index, SharedPointer<gfx::VertexShaderImplementation>> vertexShaders{};
	AssetMap<std::type_index, SharedPointer<gfx::FragmentShaderImplementation>> fragmentShaders{};
	AssetMap<TextureOptions, SharedPointer<gfx::Texture>> textures{};
	AssetMap<Model3DOptions, SharedPointer<gfx::Model3D>> model3Ds{};
	AssetMap<gfx::Font2DOptions, SharedPointer<gfx::Font2D>> fonts{};
	AssetMap<aud::SoundOptions, SharedPointer<aud::Sound>> sounds{};
	HashMap<String, SharedPointer<AssetStorage<SharedPointer<Prefab>>>, TransparentCStringHash, TransparentCStringKeyEqual> prefabs{};
};

#endif
