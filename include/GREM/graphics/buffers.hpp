// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_BUFFERS_HPP
#define GREM_GRAPHICS_BUFFERS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ConstantString.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/StridedSpan.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/ParameterDescription.hpp>
#include <GREM/graphics/buffer_layouts.hpp>
#include <GREM/graphics/shaders.hpp>

#include <cstddef> // std::nullptr_t
#include <utility> // std::move

namespace grem::graphics {

class Device;                 // Forward declaration, to avoid a circular include of Device.hpp.
struct TextureImplementation; // Forward declaration, to avoid including Texture.hpp.

struct UniformBufferImplementation;              ///< Backend-specific implementation of UniformBuffer.
struct StorageBufferImplementation;              ///< Backend-specific implementation of StorageBuffer.
struct BufferSetImplementation;                  ///< Backend-specific implementation of BufferSet.
struct InstanceBufferImplementation;             ///< Backend-specific implementation of InstanceBuffer.
struct DrawCommandBufferImplementation;          ///< Backend-specific implementation of DrawCommandBuffer.
struct UnorderedDrawCommandBufferImplementation; ///< Backend-specific implementation of UnorderedDrawCommandBuffer.

namespace detail {

class BufferSetBase;     // Forward declaration.
class StorageBufferBase; // Forward declaration.

class UniformBufferBase {
public:
	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<UniformBufferImplementation> lock() const {
		flush();
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] UniformBufferImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	GREM_API(graphics) UniformBufferBase(Device& device, BufferLayoutReference bufferLayout, size_t bufferSize, size_t textureParameterCount);

	GREM_API(graphics)
	void upload(Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures, Span<const ParameterDescription> parameterDescriptions);

	GREM_API(graphics) void flush() const;

private:
	friend BufferSetImplementation;
	friend BufferSetBase;
	friend StorageBufferBase;

	SharedPointer<UniformBufferImplementation> implementation{};
};

class StorageBufferBase {
public:
	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<StorageBufferImplementation> lock() const {
		flush();
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] StorageBufferImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	GREM_API(graphics) StorageBufferBase(Device& device, BufferLayoutReference bufferLayout, size_t elementSize);

	GREM_API(graphics) void upload(StridedSpan<const byte> elementsData, size_t elementSize);
	GREM_API(graphics) void write(uint32_t elementOffset, StridedSpan<const byte> elementsData, size_t elementSize);
	GREM_API(graphics) void flush() const;

private:
	friend BufferSetImplementation;
	friend BufferSetBase;
	friend UniformBufferBase;

	SharedPointer<StorageBufferImplementation> implementation{};
};

class BufferSetBase {
public:
	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<BufferSetImplementation> lock() const {
		flush();
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] BufferSetImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	GREM_API(graphics) BufferSetBase(Device& device, BufferLayoutReference bufferSetLayout, Allocation<SharedPointer<void>> buffers);

	GREM_API(graphics) void setBuffer(size_t bufferIndex, SharedPointer<void> newBuffer);
	GREM_API(graphics) void setBuffers(Span<SharedPointer<void>> newBuffers);

	GREM_API(graphics)
	void uploadToUniformBuffer(size_t bufferIndex, Span<const byte> newParameterValuesBytes, Span<SharedPointer<TextureImplementation>> newTextures,
		Span<const ParameterDescription> parameterDescriptions);

	GREM_API(graphics)
	void uploadToStorageBuffer(size_t bufferIndex, StridedSpan<const byte> elementsData, size_t elementSize);

	GREM_API(graphics)
	void writeToStorageBuffer(size_t bufferIndex, uint32_t elementOffset, StridedSpan<const byte> elementsData, size_t elementSize);

	GREM_API(graphics) void flush() const;

	[[nodiscard]] static SharedPointer<void> getBufferHandle(const UniformBufferBase& uniformBuffer) {
		return uniformBuffer.implementation;
	}

	[[nodiscard]] static SharedPointer<void> getBufferHandle(const StorageBufferBase& storageBuffer) {
		return storageBuffer.implementation;
	}

private:
	friend UniformBufferBase;
	friend StorageBufferBase;

	SharedPointer<BufferSetImplementation> implementation{};
};

class InstanceBufferBase {
public:
	/**
	 * Clear the contents of the buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) void clear();

	/**
	 * Get the number of instances currently in the buffer.
	 *
	 * \return the number of instances in the buffer.
	 */
	[[nodiscard]] GREM_API(graphics) uint32_t size() const noexcept;

	/**
	 * Check if the buffer is empty.
	 *
	 * \return true if the buffer is empty, false otherwise.
	 */
	[[nodiscard]] bool empty() const noexcept {
		return size() == 0;
	}

	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<InstanceBufferImplementation> lock() const {
		flush();
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] InstanceBufferImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	GREM_API(graphics) InstanceBufferBase(Device& device, size_t instanceSize);

	[[nodiscard]] GREM_API(graphics) uint32_t append(StridedSpan<const byte> instancesData, size_t instanceSize);

	GREM_API(graphics) void flush() const;

private:
	SharedPointer<InstanceBufferImplementation> implementation{};
};

class DrawCommandBufferBase {
public:
	/**
	 * Clear the contents of the buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) void clear();

	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<DrawCommandBufferImplementation> lock() const {
		flush();
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] DrawCommandBufferImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	GREM_API(graphics) DrawCommandBufferBase(Device& device);

	GREM_API(graphics)
	void append(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset, uint32_t instanceCount);

	GREM_API(graphics) void flush() const;

private:
	SharedPointer<DrawCommandBufferImplementation> implementation{};
};

class UnorderedDrawCommandBufferBase {
public:
	/**
	 * Clear the contents of the buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	GREM_API(graphics) void clear();

	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<UnorderedDrawCommandBufferImplementation> lock() const {
		flush();
		return implementation;
	}

	/**
	 * Get a pointer to the underlying resource implementation.
	 *
	 * \return a non-owning pointer to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] UnorderedDrawCommandBufferImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	GREM_API(graphics) UnorderedDrawCommandBufferBase(Device& device);

	GREM_API(graphics)
	void insert(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset, uint32_t instanceCount);

	GREM_API(graphics) void flush() const;

private:
	SharedPointer<UnorderedDrawCommandBufferImplementation> implementation{};
};

} // namespace detail

/**
 * GPU memory buffer containing a fixed-size struct of uniform parameters to be
 * read by a shader program.
 *
 * \tparam ParameterStruct user-defined aggregate type of parameters that the
 *         buffer should contain.
 * \tparam Name name of the uniform buffer. Must be globally unique among all
 *         buffer type instantiations.
 */
template <typename ParameterStruct, ConstantString Name>
class UniformBuffer : public detail::UniformBufferBase {
public:
	static_assert(parameter_struct<ParameterStruct>);

	/** Parameter struct type of the buffer. */
	using parameters_type = ParameterStruct;

	/** Layout of the buffer. */
	static constexpr UniformBufferLayout<ParameterStruct, Name> LAYOUT{};

	/** Type-erased reference to the layout of the buffer. */
	static constexpr BufferLayoutReference LAYOUT_REFERENCE = static_cast<UniformBufferLayoutReference>(LAYOUT);

	/** Name of the buffer. */
	static constexpr CStringView NAME = Name;

	/**
	 * Create a new buffer set up to contain the specified parameters, starting
	 * with uninitialized values.
	 *
	 * \param device device to create the buffer for. Must outlive the buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit UniformBuffer(Device& device)
		: detail::UniformBufferBase(device, LAYOUT_REFERENCE, detail::PARAMETER_VALUES_BYTES_SIZE<ParameterStruct>, detail::PARAMETER_TEXTURES_COUNT<ParameterStruct>) {}

	/**
	 * Upload new parameter values to the buffer for the next shader invocation
	 * that reads from it.
	 *
	 * \param newParameterValues new parameter values to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void upload(const ParameterStruct& newParameterValues) {
		alignas(sizeof(float) * 4) Array<byte, detail::PARAMETER_VALUES_BYTES_SIZE<ParameterStruct>> bytes;
		Array<SharedPointer<TextureImplementation>, detail::PARAMETER_TEXTURES_COUNT<ParameterStruct>> textures{};
		size_t byteOffset = 0;
		size_t textureIndex = 0;
		meta::forEachField(newParameterValues, [&]<typename Wrapper>(const Wrapper& parameter) -> void { //
			detail::alignParameter(byteOffset, detail::PARAMETER_TYPE_OF_PARAMETER_WRAPPER<Wrapper>, detail::ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<Wrapper>);
			detail::writeParameter(bytes.data() + byteOffset, parameter);
			detail::skipParameter(byteOffset, detail::PARAMETER_TYPE_OF_PARAMETER_WRAPPER<Wrapper>, detail::ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<Wrapper>);
			detail::writeParameterTextures(textures.data() + textureIndex, parameter);
			textureIndex += detail::TEXTURE_COUNT_OF_PARAMETER_WRAPPER<Wrapper>;
		});
		detail::UniformBufferBase::upload(bytes, textures, detail::PARAMETER_DESCRIPTIONS<ParameterStruct>);
	}
};

/**
 * GPU memory buffer containing an array of structs to be read by a shader
 * program.
 *
 * \tparam FieldStruct user-defined aggregate type with the fields that each
 *         element of the buffer should contain.
 * \tparam Name name of the storage buffer. Must be globally unique among all
 *         buffer type instantiations.
 */
template <typename FieldStruct, ConstantString Name>
class StorageBuffer : public detail::StorageBufferBase {
public:
	static_assert(field_struct<FieldStruct>);

	/** Field struct type of the buffer. */
	using fields_type = FieldStruct;

	/** Layout of the buffer. */
	static constexpr StorageBufferLayout<FieldStruct, Name> LAYOUT{};

	/** Type-erased reference to the layout of the buffer. */
	static constexpr BufferLayoutReference LAYOUT_REFERENCE = static_cast<StorageBufferLayoutReference>(LAYOUT);

	/** Name of the buffer. */
	static constexpr CStringView NAME = Name;

	/**
	 * Create a new empty buffer set up to contain the specified element type.
	 *
	 * \param device device to create the buffer for. Must outlive the buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit StorageBuffer(Device& device)
		: detail::StorageBufferBase(device, LAYOUT_REFERENCE, sizeof(FieldStruct)) {}

	/**
	 * Upload a new array of data to the buffer, overriding any previous
	 * contents.
	 *
	 * \param newElements new array of elements to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void upload(StridedSpan<const FieldStruct> newElements) {
		detail::StorageBufferBase::upload(as_strided_bytes(newElements), sizeof(FieldStruct));
	}

	/**
	 * Upload new elements to the buffer at a specific offset, overriding any
	 * data that previously occupied that location, and growing the buffer
	 * capacity if necessary.
	 *
	 * \param elementOffset offset at which to write the new elements.
	 * \param newElements new elements to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void write(uint32_t elementOffset, StridedSpan<const FieldStruct> newElements) {
		detail::StorageBufferBase::write(elementOffset, as_strided_bytes(newElements), sizeof(FieldStruct));
	}

	/**
	 * Upload a new element to the buffer at a specific index, overriding any
	 * element that previously occupied that location, and growing the buffer
	 * capacity if necessary.
	 *
	 * \param elementIndex index at which to write the new element.
	 * \param newElement new element to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void write(uint32_t elementIndex, const FieldStruct& newElement) {
		detail::StorageBufferBase::write(elementIndex, as_strided_bytes(StridedSpan{&newElement, 1}), sizeof(FieldStruct));
	}
};

/**
 * Collection of GPU memory buffers to be read by a shader.
 *
 * \tparam Buffers shader-readable buffer types to store in the buffer set. Each
 *         given type must be a unique specialization of UniformBuffer or
 *         StorageBuffer.
 */
template <typename... Buffers>
class BufferSet : public detail::BufferSetBase {
public:
	/** Layout of the buffer set. */
	static constexpr BufferSetLayout<Buffers...> LAYOUT{};

	/** Type-erased reference to the layout of the buffer set. */
	static constexpr BufferLayoutReference LAYOUT_REFERENCE = static_cast<BufferSetLayoutReference>(LAYOUT);

	/** Name of the buffer set. */
	static constexpr CStringView NAME = static_cast<BufferSetLayoutReference>(LAYOUT).name;

	/** Buffer type list of the buffer set. */
	using buffer_types = meta::TypeList<Buffers...>;

	/**
	 * Create a new set of empty buffers.
	 *
	 * \param device device to create the buffer set for. Must outlive the
	 *        buffer set.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit BufferSet(Device& device)
		: BufferSetBase(device, LAYOUT_REFERENCE, {getBufferHandle(Buffers{device})...}) {}

	/**
	 * Create an empty set of buffers that is invalid until all of its buffers
	 * have been reassigned.
	 *
	 * \param device device to create the buffer set for. Must outlive the
	 *        buffer set.
	 * \param nullPointer `nullptr`.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning A buffer set created with this constructor must be assigned a
	 *          set of valid buffers before it can be read, written or uploaded
	 *          to.
	 */
	BufferSet(Device& device, std::nullptr_t nullPointer)
		: BufferSetBase(device, LAYOUT_REFERENCE, Allocation<SharedPointer<void>>(sizeof...(Buffers), nullptr)) {
		(void)nullPointer;
	}

	/**
	 * Create a new buffer set from a set of existing buffers.
	 *
	 * \param device device to create the buffer set for. Must outlive the
	 *        buffer set.
	 * \param buffers buffers to create the buffer set from.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	explicit BufferSet(Device& device, const Buffers&... buffers)
		: BufferSetBase(device, LAYOUT_REFERENCE, {getBufferHandle(buffers)...}) {}

	/**
	 * Remove a buffer from the set, making the set invalid until all of its
	 * buffers have been reassigned.
	 *
	 * \tparam Buffer buffer type to remove. Must be one of the types specified
	 *         in the buffer set's buffer type list.
	 *
	 * \param nullPointer `nullptr`.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning After calling this function, the buffer set must be assigned a
	 *          set of valid buffers before it can be read, written or uploaded
	 *          to.
	 */
	template <typename Buffer>
	void setBuffer(std::nullptr_t nullPointer) requires(meta::type_list_contains_v<buffer_types, Buffer>) {
		(void)nullPointer;
		detail::BufferSetBase::setBuffer(meta::type_list_index_v<buffer_types, Buffer>, nullptr);
	}

	/**
	 * Assign an existing buffer to a slot in the buffer set.
	 *
	 * \tparam Buffer buffer type to assign. Must be one of the types specified
	 *         in the buffer set's buffer type list.
	 *
	 * \param buffer new buffer to assign.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename Buffer>
	void setBuffer(const Buffer& buffer) requires(meta::type_list_contains_v<buffer_types, Buffer>) {
		detail::BufferSetBase::setBuffer(meta::type_list_index_v<buffer_types, Buffer>, getBufferHandle(buffer));
	}

	/**
	 * Reset the buffer set to an empty set that is invalid until all of its
	 * buffers have been reassigned.
	 *
	 * \param nullPointer `nullptr`.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning After calling this function, the buffer set must be assigned a
	 *          set of valid buffers before it can be read, written or uploaded
	 *          to.
	 */
	void setBuffers(std::nullptr_t nullPointer) {
		(void)nullPointer;
		Array<SharedPointer<void>, sizeof...(Buffers)> newBuffers{};
		detail::BufferSetBase::setBuffers(newBuffers);
	}

	/**
	 * Assign an existing set of buffers to the buffer set.
	 *
	 * \param buffers new buffers to assign.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void setBuffers(const Buffers&... buffers) {
		Array<SharedPointer<void>, sizeof...(Buffers)> newBuffers{getBufferHandle(buffers)...};
		detail::BufferSetBase::setBuffers(newBuffers);
	}

	/**
	 * Upload new parameter values to a uniform buffer in the buffer set for the
	 * next shader invocation that reads from it.
	 *
	 * \tparam Buffer uniform buffer type to upload new values to. Must be one
	 *         of the types specified in the buffer set's buffer type list.
	 *
	 * \param newParameterValues new parameter values to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename Buffer>
	void upload(const typename Buffer::parameters_type& newParameterValues) requires(meta::type_list_contains_v<buffer_types, Buffer>) {
		using ParameterStruct = typename Buffer::parameters_type;
		alignas(sizeof(float) * 4) Array<byte, detail::PARAMETER_VALUES_BYTES_SIZE<ParameterStruct>> bytes;
		Array<SharedPointer<TextureImplementation>, detail::PARAMETER_TEXTURES_COUNT<ParameterStruct>> textures{};
		size_t byteOffset = 0;
		size_t textureIndex = 0;
		meta::forEachField(newParameterValues, [&]<typename Wrapper>(const Wrapper& parameter) -> void { //
			detail::alignParameter(byteOffset, detail::PARAMETER_TYPE_OF_PARAMETER_WRAPPER<Wrapper>, detail::ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<Wrapper>);
			detail::writeParameter(bytes.data() + byteOffset, parameter);
			detail::skipParameter(byteOffset, detail::PARAMETER_TYPE_OF_PARAMETER_WRAPPER<Wrapper>, detail::ARRAY_ELEMENT_COUNT_OF_PARAMETER_WRAPPER<Wrapper>);
			detail::writeParameterTextures(textures.data() + textureIndex, parameter);
			textureIndex += detail::TEXTURE_COUNT_OF_PARAMETER_WRAPPER<Wrapper>;
		});
		uploadToUniformBuffer(meta::type_list_index_v<buffer_types, Buffer>, bytes, textures, detail::PARAMETER_DESCRIPTIONS<ParameterStruct>);
	}

	/**
	 * Upload a new array of data to a storage buffer in the buffer set,
	 * overriding any previous contents.
	 *
	 * \tparam Buffer storage buffer type to upload new elements to. Must be one
	 *         of the types specified in the buffer set's buffer type list.
	 *
	 * \param newElements new array of elements to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename Buffer>
	void upload(StridedSpan<const typename Buffer::fields_type> newElements) requires(meta::type_list_contains_v<buffer_types, Buffer>) {
		uploadToStorageBuffer(meta::type_list_index_v<buffer_types, Buffer>, as_strided_bytes(newElements), sizeof(typename Buffer::fields_type));
	}

	/**
	 * Upload new elements to a storage buffer in the buffer set at a specific
	 * offset, overriding any data that previously occupied that location, and
	 * growing the buffer capacity if necessary.
	 *
	 * \tparam Buffer storage buffer type to upload new elements to. Must be one
	 *         of the types specified in the buffer set's buffer type list.
	 *
	 * \param elementOffset offset at which to write the new elements.
	 * \param newElements new elements to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename Buffer>
	void write(uint32_t elementOffset, StridedSpan<const typename Buffer::fields_type> newElements) requires(meta::type_list_contains_v<buffer_types, Buffer>) {
		writeToStorageBuffer(meta::type_list_index_v<buffer_types, Buffer>, elementOffset, as_strided_bytes(newElements), sizeof(typename Buffer::fields_type));
	}

	/**
	 * Upload a new element to the buffer at a specific index, overriding any
	 * element that previously occupied that location, and growing the buffer
	 * capacity if necessary.
	 *
	 * \tparam Buffer storage buffer type to upload new elements to. Must be one
	 *         of the types specified in the buffer set's buffer type list.
	 *
	 * \param elementIndex index at which to write the new element.
	 * \param newElement new element to upload.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename Buffer>
	void write(uint32_t elementIndex, const typename Buffer::fields_type& newElement) requires(meta::type_list_contains_v<buffer_types, Buffer>) {
		writeToStorageBuffer(meta::type_list_index_v<buffer_types, Buffer>, elementIndex, as_strided_bytes(StridedSpan{&newElement, 1}), sizeof(typename Buffer::fields_type));
	}
};

/**
 * Resource handle with shared ownership of a GPU memory buffer with
 * copy-on-write value semantics containing an array of structs to be used as
 * instances when drawing meshes to a RenderPass.
 *
 * \tparam Instance user-defined aggregate type with the fields that each
 *         element of the buffer should contain.
 */
template <typename Instance>
class InstanceBuffer : public detail::InstanceBufferBase {
public:
	static_assert(field_struct<Instance>);

	/** Instance type of the buffer. */
	using instance_type = Instance;

	/**
	 * Create a new empty buffer set up to contain the specified element type.
	 *
	 * \param device device to create the buffer for. Must outlive the buffer.
	 */
	explicit InstanceBuffer(Device& device)
		: detail::InstanceBufferBase(device, sizeof(Instance)) {}

	/**
	 * Append an array of instances to the buffer, growing the buffer capacity
	 * if necessary.
	 *
	 * \param instances instances to write.
	 *
	 * \return the index of the first instance written to the buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	uint32_t append(StridedSpan<const Instance> instances) {
		return detail::InstanceBufferBase::append(as_strided_bytes(instances), sizeof(Instance));
	}

	/**
	 * Append an instance to the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param instance instance to write.
	 *
	 * \return the index of the instance written to the buffer.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	uint32_t push(const Instance& instance) {
		return detail::InstanceBufferBase::append(as_strided_bytes(StridedSpan{&instance, 1}), sizeof(Instance));
	}
};

/**
 * Resource handle with shared ownership of a GPU memory buffer with
 * copy-on-write value semantics containing an array of draw commands to be used
 * when drawing meshes to a RenderPass.
 *
 * \tparam Mesh type of meshes to be drawn.
 */
template <typename Mesh>
class DrawCommandBuffer;

template <typename Vertex, typename Index, typename Parameters, typename Instance>
class DrawCommandBuffer<graphics::Mesh<Vertex, Index, Parameters, Instance>> : public detail::DrawCommandBufferBase {
public:
	/** Mesh type of the buffer. */
	using mesh_type = graphics::Mesh<Vertex, Index, Parameters, Instance>;

	/**
	 * Create a new empty buffer set up to contain draw commands.
	 *
	 * \param device device to create the buffer for. Must outlive the buffer.
	 */
	explicit DrawCommandBuffer(Device& device)
		: detail::DrawCommandBufferBase(device) {}

	/**
	 * Append a sequence of draw commands to the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipeline shader pipeline to draw the mesh with.
	 * \param mesh mesh to draw instances of.
	 * \param instanceOffset index of the first instance in the instance buffer
	 *        to draw.
	 * \param instanceCount number of instances in the instance buffer to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance range that was
	 *          specified here must be a valid range of instances in the
	 *          instance buffer.
	 */
	void append(const ShaderPipeline<mesh_type>& shaderPipeline, const mesh_type& mesh, uint32_t instanceOffset, uint32_t instanceCount) requires(!same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(shaderPipeline.lock(), mesh.lock(), instanceOffset, instanceCount);
	}

	/**
	 * Append a sequence of draw commands to the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipeline null shader pipeline. The actual shader pipeline
	 *        must be specified later using RenderPass::drawShaded() when the
	 *        buffer is drawn.
	 * \param mesh mesh to draw instances of.
	 * \param instanceOffset index of the first instance in the instance buffer
	 *        to draw.
	 * \param instanceCount number of instances in the instance buffer to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance range that was
	 *          specified here must be a valid range of instances in the
	 *          instance buffer.
	 */
	void append(std::nullptr_t shaderPipeline, const mesh_type& mesh, uint32_t instanceOffset, uint32_t instanceCount) requires(!same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(shaderPipeline, mesh.lock(), instanceOffset, instanceCount);
	}

	/**
	 * Append a sequence of draw commands to the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipelineHandle handle to the shader pipeline to draw the
	 *        mesh with. If this is nullptr, a shader override must be specified
	 *        later using RenderPass::drawShaded() when this buffer is drawn.
	 * \param meshHandle handle to the mesh to draw instances of. Must not be
	 *        nullptr.
	 * \param instanceOffset index of the first instance in the instance buffer
	 *        to draw.
	 * \param instanceCount number of instances in the instance buffer to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance range that was
	 *          specified here must be a valid range of instances in the
	 *          instance buffer.
	 */
	void append(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset, uint32_t instanceCount)
		requires(!same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(std::move(shaderPipelineHandle), std::move(meshHandle), instanceOffset, instanceCount);
	}

	/**
	 * Append a sequence of draw commands to the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipeline shader pipeline to draw the mesh with.
	 * \param mesh mesh to draw instances of.
	 * \param instanceCount number of instances to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void append(const ShaderPipeline<mesh_type>& shaderPipeline, const mesh_type& mesh, uint32_t instanceCount) requires(same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(shaderPipeline.lock(), mesh.lock(), 0, instanceCount);
	}

	/**
	 * Append a sequence of draw commands to the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipeline null shader pipeline. The actual shader pipeline
	 *        must be specified later using RenderPass::drawShaded() when the
	 *        buffer is drawn.
	 * \param mesh mesh to draw instances of.
	 * \param instanceCount number of instances to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void append(std::nullptr_t shaderPipeline, const mesh_type& mesh, uint32_t instanceCount) requires(same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(shaderPipeline, mesh.lock(), 0, instanceCount);
	}

	/**
	 * Append a sequence of draw commands to the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipelineHandle handle to the shader pipeline to draw the
	 *        mesh with. If this is nullptr, a shader override must be specified
	 *        later using RenderPass::drawShaded() when this buffer is drawn.
	 * \param meshHandle handle to the mesh to draw instances of. Must not be
	 *        nullptr.
	 * \param instanceCount number of instances to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void append(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceCount)
		requires(same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(std::move(shaderPipelineHandle), std::move(meshHandle), 0, instanceCount);
	}

	/**
	 * Append a draw command to the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipeline shader pipeline to draw the mesh with.
	 * \param mesh mesh to draw instances of.
	 * \param instanceIndex index of the instance in the instance buffer to
	 *        draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance index that was
	 *          specified here must be a valid index of an instance in the
	 *          instance buffer.
	 */
	void push(const ShaderPipeline<mesh_type>& shaderPipeline, const mesh_type& mesh, uint32_t instanceIndex) requires(!same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(shaderPipeline.lock(), mesh.lock(), instanceIndex, 1);
	}

	/**
	 * Append a draw command to the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipeline null shader pipeline. The actual shader pipeline
	 *        must be specified later using RenderPass::drawShaded() when the
	 *        buffer is drawn.
	 * \param mesh mesh to draw instances of.
	 * \param instanceIndex index of the instance in the instance buffer to
	 *        draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance index that was
	 *          specified here must be a valid index of an instance in the
	 *          instance buffer.
	 */
	void push(std::nullptr_t shaderPipeline, const mesh_type& mesh, uint32_t instanceIndex) requires(!same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(shaderPipeline, mesh.lock(), instanceIndex, 1);
	}

	/**
	 * Append a draw command to the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipelineHandle handle to the shader pipeline to draw the
	 *        mesh with. If this is nullptr, a shader override must be specified
	 *        later using RenderPass::drawShaded() when this buffer is drawn.
	 * \param meshHandle handle to the mesh to draw instances of. Must not be
	 *        nullptr.
	 * \param instanceIndex index of the instance in the instance buffer to
	 *        draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance index that was
	 *          specified here must be a valid index of an instance in the
	 *          instance buffer.
	 */
	void push(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceIndex)
		requires(!same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(std::move(shaderPipelineHandle), std::move(meshHandle), instanceIndex, 1);
	}

	/**
	 * Append a draw command to the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipeline shader pipeline to draw the mesh with.
	 * \param mesh mesh to draw instances of.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void push(const ShaderPipeline<mesh_type>& shaderPipeline, const mesh_type& mesh) requires(same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(shaderPipeline.lock(), mesh.lock(), 0, 1);
	}

	/**
	 * Append a draw command to the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipeline null shader pipeline. The actual shader pipeline
	 *        must be specified later using RenderPass::drawShaded() when the
	 *        buffer is drawn.
	 * \param mesh mesh to draw instances of.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void push(std::nullptr_t shaderPipeline, const mesh_type& mesh) requires(same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(shaderPipeline, mesh.lock(), 0, 1);
	}

	/**
	 * Append a draw command to the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipelineHandle handle to the shader pipeline to draw the
	 *        mesh with. If this is nullptr, a shader override must be specified
	 *        later using RenderPass::drawShaded() when this buffer is drawn.
	 * \param meshHandle handle to the mesh to draw instances of. Must not be
	 *        nullptr.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void push(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle) requires(same_as<Instance, NoInstance>) {
		detail::DrawCommandBufferBase::append(std::move(shaderPipelineHandle), std::move(meshHandle), 0, 1);
	}
};

/**
 * Resource handle with shared ownership of a GPU memory buffer with
 * copy-on-write value semantics containing an unordered set of draw commands to
 * be used when drawing meshes to a RenderPass.
 *
 * The unordered aspect of the buffer allows for optimizations such as batching
 * instances by shader and/or mesh, in a way that is optimal for the underlying
 * graphics API, regardless of the order in which the draw commands were
 * inserted.
 *
 * \tparam Mesh type of meshes to be drawn.
 */
template <typename Mesh>
class UnorderedDrawCommandBuffer;

template <typename Vertex, typename Index, typename Parameters, typename Instance>
class UnorderedDrawCommandBuffer<graphics::Mesh<Vertex, Index, Parameters, Instance>> : public detail::UnorderedDrawCommandBufferBase {
public:
	/** Mesh type of the buffer. */
	using mesh_type = graphics::Mesh<Vertex, Index, Parameters, Instance>;

	/**
	 * Create a new empty buffer set up to contain draw commands.
	 *
	 * \param device device to create the buffer for. Must outlive the buffer.
	 */
	explicit UnorderedDrawCommandBuffer(Device& device)
		: detail::UnorderedDrawCommandBufferBase(device) {}

	/**
	 * Insert a sequence of draw commands into the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipeline shader pipeline to draw the mesh with.
	 * \param mesh mesh to draw instances of.
	 * \param instanceOffset index of the first instance in the instance buffer
	 *        to draw.
	 * \param instanceCount number of instances in the instance buffer to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance range that was
	 *          specified here must be a valid range of instances in the
	 *          instance buffer.
	 */
	void insertRange(const ShaderPipeline<mesh_type>& shaderPipeline, const mesh_type& mesh, uint32_t instanceOffset, uint32_t instanceCount)
		requires(!same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(shaderPipeline.lock(), mesh.lock(), instanceOffset, instanceCount);
	}

	/**
	 * Insert a sequence of draw commands into the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipeline null shader pipeline. The actual shader pipeline
	 *        must be specified later using RenderPass::drawShadedUnordered()
	 *        when the buffer is drawn.
	 * \param mesh mesh to draw instances of.
	 * \param instanceOffset index of the first instance in the instance buffer
	 *        to draw.
	 * \param instanceCount number of instances in the instance buffer to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance range that was
	 *          specified here must be a valid range of instances in the
	 *          instance buffer.
	 */
	void insertRange(std::nullptr_t shaderPipeline, const mesh_type& mesh, uint32_t instanceOffset, uint32_t instanceCount) requires(!same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(shaderPipeline, mesh.lock(), instanceOffset, instanceCount);
	}

	/**
	 * Insert a sequence of draw commands into the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipelineHandle handle to the shader pipeline to draw the
	 *        mesh with. If this is nullptr, a shader override must be specified
	 *        later using RenderPass::drawShadedUnordered() when this buffer is
	 *        drawn.
	 * \param meshHandle handle to the mesh to draw instances of. Must not be
	 *        nullptr.
	 * \param instanceOffset index of the first instance in the instance buffer
	 *        to draw.
	 * \param instanceCount number of instances in the instance buffer to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance range that was
	 *          specified here must be a valid range of instances in the
	 *          instance buffer.
	 */
	void insertRange(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceOffset,
		uint32_t instanceCount) requires(!same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(std::move(shaderPipelineHandle), std::move(meshHandle), instanceOffset, instanceCount);
	}

	/**
	 * Insert a sequence of draw commands into the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipeline shader pipeline to draw the mesh with.
	 * \param mesh mesh to draw instances of.
	 * \param instanceCount number of instances to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void insertRange(const ShaderPipeline<mesh_type>& shaderPipeline, const mesh_type& mesh, uint32_t instanceCount) requires(same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(shaderPipeline.lock(), mesh.lock(), 0, instanceCount);
	}

	/**
	 * Insert a sequence of draw commands into the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipeline null shader pipeline. The actual shader pipeline
	 *        must be specified later using RenderPass::drawShadedUnordered()
	 *        when the buffer is drawn.
	 * \param mesh mesh to draw instances of.
	 * \param instanceCount number of instances to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void insertRange(std::nullptr_t shaderPipeline, const mesh_type& mesh, uint32_t instanceCount) requires(same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(shaderPipeline, mesh.lock(), 0, instanceCount);
	}

	/**
	 * Insert a sequence of draw commands into the buffer, growing the buffer
	 * capacity if necessary.
	 *
	 * \param shaderPipelineHandle handle to the shader pipeline to draw the
	 *        mesh with. If this is nullptr, a shader override must be specified
	 *        later using RenderPass::drawShadedUnordered() when this buffer is
	 *        drawn.
	 * \param meshHandle handle to the mesh to draw instances of. Must not be
	 *        nullptr.
	 * \param instanceCount number of instances to draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void insertRange(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceCount)
		requires(same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(std::move(shaderPipelineHandle), std::move(meshHandle), 0, instanceCount);
	}

	/**
	 * Insert a draw command into the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipeline shader pipeline to draw the mesh with.
	 * \param mesh mesh to draw instances of.
	 * \param instanceIndex index of the instance in the instance buffer to
	 *        draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance index that was
	 *          specified here must be a valid index of an instance in the
	 *          instance buffer.
	 */
	void insert(const ShaderPipeline<mesh_type>& shaderPipeline, const mesh_type& mesh, uint32_t instanceIndex) requires(!same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(shaderPipeline.lock(), mesh.lock(), instanceIndex, 1);
	}

	/**
	 * Insert a draw command into the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipeline null shader pipeline. The actual shader pipeline
	 *        must be specified later using RenderPass::drawShadedUnordered()
	 *        when the buffer is drawn.
	 * \param mesh mesh to draw instances of.
	 * \param instanceIndex index of the instance in the instance buffer to
	 *        draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance index that was
	 *          specified here must be a valid index of an instance in the
	 *          instance buffer.
	 */
	void insert(std::nullptr_t shaderPipeline, const mesh_type& mesh, uint32_t instanceIndex) requires(!same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(shaderPipeline, mesh.lock(), instanceIndex, 1);
	}

	/**
	 * Insert a draw command into the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipelineHandle handle to the shader pipeline to draw the
	 *        mesh with. If this is nullptr, a shader override must be specified
	 *        later using RenderPass::drawShadedUnordered() when this buffer is
	 *        drawn.
	 * \param meshHandle handle to the mesh to draw instances of. Must not be
	 *        nullptr.
	 * \param instanceIndex index of the instance in the instance buffer to
	 *        draw.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning When the draw command buffer is drawn together with a
	 *          corresponding instance buffer, the instance index that was
	 *          specified here must be a valid index of an instance in the
	 *          instance buffer.
	 */
	void insert(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle, uint32_t instanceIndex)
		requires(!same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(std::move(shaderPipelineHandle), std::move(meshHandle), instanceIndex, 1);
	}

	/**
	 * Insert a draw command into the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipeline shader pipeline to draw the mesh with.
	 * \param mesh mesh to draw instances of.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void insert(const ShaderPipeline<mesh_type>& shaderPipeline, const mesh_type& mesh) requires(same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(shaderPipeline.lock(), mesh.lock(), 0, 1);
	}

	/**
	 * Insert a draw command into the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipeline null shader pipeline. The actual shader pipeline
	 *        must be specified later using RenderPass::drawShadedUnordered()
	 *        when the buffer is drawn.
	 * \param mesh mesh to draw instances of.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void insert(std::nullptr_t shaderPipeline, const mesh_type& mesh) requires(same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(shaderPipeline, mesh.lock(), 0, 1);
	}

	/**
	 * Insert a draw command into the buffer, growing the buffer capacity if
	 * necessary.
	 *
	 * \param shaderPipelineHandle handle to the shader pipeline to draw the
	 *        mesh with. If this is nullptr, a shader override must be specified
	 *        later using RenderPass::drawShadedUnordered() when this buffer is
	 *        drawn.
	 * \param meshHandle handle to the mesh to draw instances of. Must not be
	 *        nullptr.
	 *
	 * \throws graphics::Error if resource creation failed.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	void insert(SharedPointer<ShaderPipelineImplementation> shaderPipelineHandle, SharedPointer<MeshImplementation> meshHandle) requires(same_as<Instance, NoInstance>) {
		detail::UnorderedDrawCommandBufferBase::insert(std::move(shaderPipelineHandle), std::move(meshHandle), 0, 1);
	}
};

} // namespace grem::graphics

#endif
