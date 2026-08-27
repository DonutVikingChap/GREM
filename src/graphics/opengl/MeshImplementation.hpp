// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_MESH_IMPLEMENTATION_HPP
#define GREM_GRAPHICS_OPENGL_MESH_IMPLEMENTATION_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/graphics/Error.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/Texture.hpp>

#include "../reusable_copy_on_write_resource.hpp"
#include "StatePreserver.hpp"
#include "objects.hpp"
#include "opengl.hpp"

#include <typeindex> // std::type_index

namespace grem::graphics {

struct MeshImplementation : detail::ReusableCopyOnWriteResourceBase<MeshImplementation> {
	struct WithoutVerticesTag {};

	struct BufferImplementation : detail::ReusableCopyOnWriteResourceBase<BufferImplementation> {
		[[nodiscard]] static SharedPointer<BufferImplementation> create() {
			return SharedPointer<BufferImplementation>::create();
		}

		detail::BufferObject object = detail::createBufferObject();
	};

	struct Attribute {
		bool isRawInteger;
		GLint size;
		GLenum type;
		GLsizei stride;
		uintptr_t offset;
	};

	static void appendInterleavedFieldAttributes(Buffer<Attribute>& attributes, uintptr_t& offset, FieldType fieldType, GLsizei stride) {
		switch (fieldType) {
			case FieldType::INT:
				attributes.push_back(Attribute{.isRawInteger = true, .size = 1, .type = GL_INT, .stride = stride, .offset = offset});
				offset += sizeof(int32_t);
				break;
			case FieldType::IVEC2:
				attributes.push_back(Attribute{.isRawInteger = true, .size = 2, .type = GL_INT, .stride = stride, .offset = offset});
				offset += sizeof(vec2);
				break;
			case FieldType::IVEC3:
				attributes.push_back(Attribute{.isRawInteger = true, .size = 3, .type = GL_INT, .stride = stride, .offset = offset});
				offset += sizeof(vec3);
				break;
			case FieldType::IVEC4:
				attributes.push_back(Attribute{.isRawInteger = true, .size = 4, .type = GL_INT, .stride = stride, .offset = offset});
				offset += sizeof(vec4);
				break;
			case FieldType::UINT:
				attributes.push_back(Attribute{.isRawInteger = true, .size = 1, .type = GL_UNSIGNED_INT, .stride = stride, .offset = offset});
				offset += sizeof(uint32_t);
				break;
			case FieldType::UVEC2:
				attributes.push_back(Attribute{.isRawInteger = true, .size = 2, .type = GL_UNSIGNED_INT, .stride = stride, .offset = offset});
				offset += sizeof(vec2);
				break;
			case FieldType::UVEC3:
				attributes.push_back(Attribute{.isRawInteger = true, .size = 3, .type = GL_UNSIGNED_INT, .stride = stride, .offset = offset});
				offset += sizeof(vec3);
				break;
			case FieldType::UVEC4:
				attributes.push_back(Attribute{.isRawInteger = true, .size = 4, .type = GL_UNSIGNED_INT, .stride = stride, .offset = offset});
				offset += sizeof(vec4);
				break;
			case FieldType::FLOAT:
				attributes.push_back(Attribute{.isRawInteger = false, .size = 1, .type = GL_FLOAT, .stride = stride, .offset = offset});
				offset += sizeof(float);
				break;
			case FieldType::VEC2:
				attributes.push_back(Attribute{.isRawInteger = false, .size = 2, .type = GL_FLOAT, .stride = stride, .offset = offset});
				offset += sizeof(vec2);
				break;
			case FieldType::VEC3:
				attributes.push_back(Attribute{.isRawInteger = false, .size = 3, .type = GL_FLOAT, .stride = stride, .offset = offset});
				offset += sizeof(vec3);
				break;
			case FieldType::VEC4:
				attributes.push_back(Attribute{.isRawInteger = false, .size = 4, .type = GL_FLOAT, .stride = stride, .offset = offset});
				offset += sizeof(vec4);
				break;
			case FieldType::MAT2:
				for (size_t i = 0; i < 2; ++i) {
					attributes.push_back(Attribute{.isRawInteger = false, .size = 2, .type = GL_FLOAT, .stride = stride, .offset = offset});
					offset += sizeof(vec2);
				}
				break;
			case FieldType::MAT3:
				for (size_t i = 0; i < 3; ++i) {
					attributes.push_back(Attribute{.isRawInteger = false, .size = 3, .type = GL_FLOAT, .stride = stride, .offset = offset});
					offset += sizeof(vec3);
				}
				break;
			case FieldType::MAT4:
				for (size_t i = 0; i < 4; ++i) {
					attributes.push_back(Attribute{.isRawInteger = false, .size = 4, .type = GL_FLOAT, .stride = stride, .offset = offset});
					offset += sizeof(vec4);
				}
				break;
		}
	}

	[[nodiscard]] static size_t initializeVertexAttribute(Attribute& attribute, VertexAttributeType vertexAttributeType) {
		switch (vertexAttributeType) {
			case VertexAttributeType::U8NORM:
				attribute.isRawInteger = false;
				attribute.size = 1;
				attribute.type = GL_UNSIGNED_BYTE;
				return sizeof(u8norm);
			case VertexAttributeType::I8NORM:
				attribute.isRawInteger = false;
				attribute.size = 1;
				attribute.type = GL_BYTE;
				return sizeof(i8norm);
			case VertexAttributeType::U8:
				attribute.isRawInteger = true;
				attribute.size = 1;
				attribute.type = GL_UNSIGNED_BYTE;
				return sizeof(uint8_t);
			case VertexAttributeType::I8:
				attribute.isRawInteger = true;
				attribute.size = 1;
				attribute.type = GL_BYTE;
				return sizeof(int8_t);
			case VertexAttributeType::U8VEC2NORM:
				attribute.isRawInteger = false;
				attribute.size = 2;
				attribute.type = GL_UNSIGNED_BYTE;
				return sizeof(u8vec2norm);
			case VertexAttributeType::I8VEC2NORM:
				attribute.isRawInteger = false;
				attribute.size = 2;
				attribute.type = GL_BYTE;
				return sizeof(i8vec2norm);
			case VertexAttributeType::U8VEC2:
				attribute.isRawInteger = true;
				attribute.size = 2;
				attribute.type = GL_UNSIGNED_BYTE;
				return sizeof(u8vec2);
			case VertexAttributeType::I8VEC2:
				attribute.isRawInteger = true;
				attribute.size = 2;
				attribute.type = GL_BYTE;
				return sizeof(i8vec2);
			case VertexAttributeType::U8VEC4NORM:
				attribute.isRawInteger = false;
				attribute.size = 4;
				attribute.type = GL_UNSIGNED_BYTE;
				return sizeof(u8vec4norm);
			case VertexAttributeType::I8VEC4NORM:
				attribute.isRawInteger = false;
				attribute.size = 4;
				attribute.type = GL_BYTE;
				return sizeof(i8vec4norm);
			case VertexAttributeType::U8VEC4:
				attribute.isRawInteger = true;
				attribute.size = 4;
				attribute.type = GL_UNSIGNED_BYTE;
				return sizeof(u8vec4);
			case VertexAttributeType::I8VEC4:
				attribute.isRawInteger = true;
				attribute.size = 4;
				attribute.type = GL_BYTE;
				return sizeof(i8vec4);
			case VertexAttributeType::UA2B10G10R10VEC4NORM:
				attribute.isRawInteger = false;
				attribute.size = 4;
				attribute.type = GL_UNSIGNED_INT_2_10_10_10_REV;
				return sizeof(uA2B10G10R10vec4norm);
			case VertexAttributeType::IA2B10G10R10VEC4NORM:
				attribute.isRawInteger = false;
				attribute.size = 4;
				attribute.type = GL_INT_2_10_10_10_REV;
				return sizeof(iA2B10G10R10vec4norm);
			case VertexAttributeType::U16NORM:
				attribute.isRawInteger = false;
				attribute.size = 1;
				attribute.type = GL_UNSIGNED_SHORT;
				return sizeof(u16norm);
			case VertexAttributeType::I16NORM:
				attribute.isRawInteger = false;
				attribute.size = 1;
				attribute.type = GL_SHORT;
				return sizeof(i16norm);
			case VertexAttributeType::U16:
				attribute.isRawInteger = true;
				attribute.size = 1;
				attribute.type = GL_UNSIGNED_SHORT;
				return sizeof(uint16_t);
			case VertexAttributeType::I16:
				attribute.isRawInteger = true;
				attribute.size = 1;
				attribute.type = GL_SHORT;
				return sizeof(int16_t);
			case VertexAttributeType::F16:
				attribute.isRawInteger = false;
				attribute.size = 1;
				attribute.type = GL_HALF_FLOAT;
				return sizeof(float16_t);
			case VertexAttributeType::U16VEC2NORM:
				attribute.isRawInteger = false;
				attribute.size = 2;
				attribute.type = GL_UNSIGNED_SHORT;
				return sizeof(u16vec2norm);
			case VertexAttributeType::I16VEC2NORM:
				attribute.isRawInteger = false;
				attribute.size = 2;
				attribute.type = GL_SHORT;
				return sizeof(i16vec2norm);
			case VertexAttributeType::U16VEC2:
				attribute.isRawInteger = true;
				attribute.size = 2;
				attribute.type = GL_UNSIGNED_SHORT;
				return sizeof(u16vec2);
			case VertexAttributeType::I16VEC2:
				attribute.isRawInteger = true;
				attribute.size = 2;
				attribute.type = GL_SHORT;
				return sizeof(i16vec2);
			case VertexAttributeType::F16VEC2:
				attribute.isRawInteger = false;
				attribute.size = 2;
				attribute.type = GL_HALF_FLOAT;
				return sizeof(f16vec2);
			case VertexAttributeType::U16VEC4NORM:
				attribute.isRawInteger = false;
				attribute.size = 4;
				attribute.type = GL_UNSIGNED_SHORT;
				return sizeof(u16vec4norm);
			case VertexAttributeType::I16VEC4NORM:
				attribute.isRawInteger = false;
				attribute.size = 4;
				attribute.type = GL_SHORT;
				return sizeof(i16vec4norm);
			case VertexAttributeType::U16VEC4:
				attribute.isRawInteger = true;
				attribute.size = 4;
				attribute.type = GL_UNSIGNED_SHORT;
				return sizeof(u16vec4);
			case VertexAttributeType::I16VEC4:
				attribute.isRawInteger = true;
				attribute.size = 4;
				attribute.type = GL_SHORT;
				return sizeof(i16vec4);
			case VertexAttributeType::F16VEC4:
				attribute.isRawInteger = false;
				attribute.size = 4;
				attribute.type = GL_HALF_FLOAT;
				return sizeof(f16vec4);
			case VertexAttributeType::U32:
				attribute.isRawInteger = true;
				attribute.size = 1;
				attribute.type = GL_UNSIGNED_INT;
				return sizeof(uint32_t);
			case VertexAttributeType::I32:
				attribute.isRawInteger = true;
				attribute.size = 1;
				attribute.type = GL_INT;
				return sizeof(int32_t);
			case VertexAttributeType::F32:
				attribute.isRawInteger = false;
				attribute.size = 1;
				attribute.type = GL_FLOAT;
				return sizeof(float);
			case VertexAttributeType::U32VEC2:
				attribute.isRawInteger = true;
				attribute.size = 2;
				attribute.type = GL_UNSIGNED_INT;
				return sizeof(u32vec2);
			case VertexAttributeType::I32VEC2:
				attribute.isRawInteger = true;
				attribute.size = 2;
				attribute.type = GL_INT;
				return sizeof(i32vec2);
			case VertexAttributeType::F32VEC2:
				attribute.isRawInteger = false;
				attribute.size = 2;
				attribute.type = GL_FLOAT;
				return sizeof(vec2);
			case VertexAttributeType::U32VEC3:
				attribute.isRawInteger = true;
				attribute.size = 3;
				attribute.type = GL_UNSIGNED_INT;
				return sizeof(u32vec3);
			case VertexAttributeType::I32VEC3:
				attribute.isRawInteger = true;
				attribute.size = 3;
				attribute.type = GL_INT;
				return sizeof(i32vec3);
			case VertexAttributeType::F32VEC3:
				attribute.isRawInteger = false;
				attribute.size = 3;
				attribute.type = GL_FLOAT;
				return sizeof(vec3);
			case VertexAttributeType::U32VEC4:
				attribute.isRawInteger = true;
				attribute.size = 4;
				attribute.type = GL_UNSIGNED_INT;
				return sizeof(u32vec4);
			case VertexAttributeType::I32VEC4:
				attribute.isRawInteger = true;
				attribute.size = 4;
				attribute.type = GL_INT;
				return sizeof(i32vec4);
			case VertexAttributeType::F32VEC4:
				attribute.isRawInteger = false;
				attribute.size = 4;
				attribute.type = GL_FLOAT;
				return sizeof(vec4);
		}
		unreachable();
	}

	static void appendInterleavedVertexAttributes(Buffer<Attribute>& attributes, uintptr_t& offset, VertexAttributeType vertexAttributeType, GLsizei stride) {
		Attribute attribute{.isRawInteger{}, .size{}, .type{}, .stride = stride, .offset = offset};
		offset += static_cast<uintptr_t>(initializeVertexAttribute(attribute, vertexAttributeType));
		attributes.push_back(attribute);
	}

	static void appendDeinterleavedVertexAttributes(Buffer<Attribute>& attributes, uintptr_t& baseOffset, VertexAttributeType vertexAttributeType, size_t count) {
		Attribute attribute{.isRawInteger{}, .size{}, .type{}, .stride{}, .offset = baseOffset};
		const size_t stride = initializeVertexAttribute(attribute, vertexAttributeType);
		attribute.stride = static_cast<GLsizei>(stride);
		baseOffset += static_cast<uintptr_t>(stride * count);
		attributes.push_back(attribute);
	}

	static void setupVertexAttributes(Span<const Attribute> vertexAttributes) {
		GLuint attributeIndex = 0;
		for (const Attribute& attribute : vertexAttributes) {
			glEnableVertexAttribArray(attributeIndex);
			if (attribute.isRawInteger) {
				glVertexAttribIPointer(attributeIndex, attribute.size, attribute.type, attribute.stride,
					reinterpret_cast<const void*>(attribute.offset)); // NOLINT(performance-no-int-to-ptr)
			} else {
				glVertexAttribPointer(attributeIndex, attribute.size, attribute.type, (attribute.type == GL_FLOAT || attribute.type == GL_HALF_FLOAT) ? GL_FALSE : GL_TRUE,
					attribute.stride,
					reinterpret_cast<const void*>(attribute.offset)); // NOLINT(performance-no-int-to-ptr)
			}
			++attributeIndex;
		}
	}

	static void setupInstanceAttributes(Span<const Attribute> vertexAttributes, Span<const Attribute> instanceAttributes) {
		GLuint attributeIndex = static_cast<GLuint>(vertexAttributes.size());
		for (const Attribute& attribute : instanceAttributes) {
			glEnableVertexAttribArray(attributeIndex);
			glVertexAttribDivisor(attributeIndex, 1u);
			if (attribute.isRawInteger) {
				glVertexAttribIPointer(attributeIndex, attribute.size, attribute.type, attribute.stride,
					reinterpret_cast<const void*>(attribute.offset)); // NOLINT(performance-no-int-to-ptr)
			} else {
				glVertexAttribPointer(attributeIndex, attribute.size, attribute.type, (attribute.type == GL_FLOAT || attribute.type == GL_HALF_FLOAT) ? GL_FALSE : GL_TRUE,
					attribute.stride,
					reinterpret_cast<const void*>(attribute.offset)); // NOLINT(performance-no-int-to-ptr)
			}
			++attributeIndex;
		}
	}

	[[nodiscard]] static SharedPointer<MeshImplementation> create(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes,
		Span<const VertexAttributeDescription> vertexAttributeDescriptions, Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions,
		Span<const FieldDescription> instanceAttributeDescriptions, size_t instanceStride) {
		return SharedPointer<MeshImplementation>::create(meshTypeIndex, activeVertexAttributes, vertexAttributeDescriptions, indexType, parameterDescriptions,
			instanceAttributeDescriptions, instanceStride);
	}

	[[nodiscard]] static SharedPointer<MeshImplementation> clone(const MeshImplementation& implementation) {
		return SharedPointer<MeshImplementation>::create(implementation);
	}

	[[nodiscard]] static SharedPointer<MeshImplementation> cloneWithoutVertices(const MeshImplementation& implementation) {
		return SharedPointer<MeshImplementation>::create(implementation, WithoutVerticesTag{});
	}

	detail::VertexArrayObject vertexArrayObject = detail::createVertexArrayObject();
	SharedPointer<BufferImplementation> vertexBufferObject{};
	SharedPointer<BufferImplementation> elementBufferObject{};
	SharedPointer<BufferImplementation> uniformBufferObject{};
	SharedPointer<BufferImplementation> instanceBufferObject{};
	ArrayList<SharedPointer<TextureImplementation>> textures{};
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	std::type_index meshTypeIndex;
	VertexAttributeMask activeVertexAttributes;
	Optional<MeshIndexType> indexType;
	Buffer<Attribute> vertexAttributes{};
	Buffer<Attribute> instanceAttributes{};

	MeshImplementation(std::type_index meshTypeIndex, VertexAttributeMask activeVertexAttributes, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
		Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions,
		size_t instanceStride)
		: meshTypeIndex(meshTypeIndex)
		, activeVertexAttributes(activeVertexAttributes)
		, indexType(indexType) {
		for (const VertexAttributeDescription& vertexAttributeDescription : vertexAttributeDescriptions) {
			uintptr_t baseOffset = 0;
			appendDeinterleavedVertexAttributes(vertexAttributes, baseOffset, vertexAttributeDescription.type, 0);
		}

		uintptr_t offset = 0;
		for (const FieldDescription& instanceAttributeDescription : instanceAttributeDescriptions) {
			appendInterleavedFieldAttributes(instanceAttributes, offset, instanceAttributeDescription.type, static_cast<GLsizei>(instanceStride));
		}

		const detail::VertexArrayBindingPreserver vertexArrayBindingPreserver{};
		glBindVertexArray(vertexArrayObject.get());

		vertexBufferObject = BufferImplementation::create();

		if (indexType) {
			elementBufferObject = BufferImplementation::create();
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferObject->object.get());
		}

		if (!parameterDescriptions.empty()) {
			uniformBufferObject = BufferImplementation::create();
		}

		if (!instanceAttributeDescriptions.empty()) {
			instanceBufferObject = BufferImplementation::create();

			const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
			glBindBuffer(GL_ARRAY_BUFFER, instanceBufferObject->object.get());
			setupInstanceAttributes(vertexAttributes, instanceAttributes);
		}
	}

	~MeshImplementation() = default;

	MeshImplementation(const MeshImplementation& other)
		: vertexBufferObject(other.vertexBufferObject)
		, elementBufferObject(other.elementBufferObject)
		, uniformBufferObject(other.uniformBufferObject)
		, instanceBufferObject(other.instanceBufferObject)
		, textures(other.textures)
		, vertexCount(other.vertexCount)
		, indexCount(other.indexCount)
		, meshTypeIndex(other.meshTypeIndex)
		, activeVertexAttributes(other.activeVertexAttributes)
		, indexType(other.indexType)
		, vertexAttributes(other.vertexAttributes)
		, instanceAttributes(other.instanceAttributes) {
		const detail::VertexArrayBindingPreserver vertexArrayBindingPreserver{};
		glBindVertexArray(vertexArrayObject.get());

		const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject->object.get());
		setupVertexAttributes(vertexAttributes);

		if (elementBufferObject) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferObject->object.get());
		}

		if (instanceBufferObject) {
			const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
			glBindBuffer(GL_ARRAY_BUFFER, instanceBufferObject->object.get());
			setupInstanceAttributes(vertexAttributes, instanceAttributes);
		}
	}

	MeshImplementation(const MeshImplementation& other, WithoutVerticesTag)
		: elementBufferObject(other.elementBufferObject)
		, uniformBufferObject(other.uniformBufferObject)
		, instanceBufferObject(other.instanceBufferObject)
		, textures(other.textures)
		, indexCount(other.indexCount)
		, meshTypeIndex(other.meshTypeIndex)
		, activeVertexAttributes(other.activeVertexAttributes)
		, indexType(other.indexType)
		, instanceAttributes(other.instanceAttributes) {
		const detail::VertexArrayBindingPreserver vertexArrayBindingPreserver{};
		glBindVertexArray(vertexArrayObject.get());

		vertexBufferObject = BufferImplementation::create();

		if (elementBufferObject) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferObject->object.get());
		}

		if (instanceBufferObject) {
			const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
			glBindBuffer(GL_ARRAY_BUFFER, instanceBufferObject->object.get());
			setupInstanceAttributes(vertexAttributes, instanceAttributes);
		}
	}

	MeshImplementation& operator=(const MeshImplementation& other) {
		if (this == &other) {
			return *this;
		}
		vertexArrayObject = detail::createVertexArrayObject();
		vertexBufferObject = other.vertexBufferObject;
		elementBufferObject = other.elementBufferObject;
		uniformBufferObject = other.uniformBufferObject;
		instanceBufferObject = other.instanceBufferObject;
		textures = other.textures;
		vertexCount = other.vertexCount;
		indexCount = other.indexCount;
		meshTypeIndex = other.meshTypeIndex;
		activeVertexAttributes = other.activeVertexAttributes;
		indexType = other.indexType;
		vertexAttributes = other.vertexAttributes;
		instanceAttributes = other.instanceAttributes;

		const detail::VertexArrayBindingPreserver vertexArrayBindingPreserver{};
		glBindVertexArray(vertexArrayObject.get());

		const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject->object.get());
		setupVertexAttributes(vertexAttributes);

		if (elementBufferObject) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferObject->object.get());
		}

		if (instanceBufferObject) {
			const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
			glBindBuffer(GL_ARRAY_BUFFER, instanceBufferObject->object.get());
			setupInstanceAttributes(vertexAttributes, instanceAttributes);
		}
		return *this;
	}

	MeshImplementation(MeshImplementation&&) noexcept = default;
	MeshImplementation& operator=(MeshImplementation&&) noexcept = default;

	void allocateWithoutVertices(const MeshImplementation& other) {
		vertexArrayObject = detail::createVertexArrayObject();
		vertexBufferObject = BufferImplementation::create();
		elementBufferObject = other.elementBufferObject;
		uniformBufferObject = other.uniformBufferObject;
		instanceBufferObject = other.instanceBufferObject;
		textures = other.textures;
		vertexCount = 0;
		indexCount = other.indexCount;
		meshTypeIndex = other.meshTypeIndex;
		activeVertexAttributes = other.activeVertexAttributes;
		indexType = other.indexType;
		vertexAttributes.clear();
		instanceAttributes = other.instanceAttributes;

		const detail::VertexArrayBindingPreserver vertexArrayBindingPreserver{};
		glBindVertexArray(vertexArrayObject.get());

		if (elementBufferObject) {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferObject->object.get());
		}

		if (instanceBufferObject) {
			const detail::ArrayBufferBindingPreserver arrayBufferBindingPreserver{};
			glBindBuffer(GL_ARRAY_BUFFER, instanceBufferObject->object.get());
			setupInstanceAttributes(vertexAttributes, instanceAttributes);
		}
	}
};

} // namespace grem::graphics

#endif
