// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_OBJECTS_HPP
#define GREM_GRAPHICS_OPENGL_OBJECTS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/graphics/Error.hpp>

#include "opengl.hpp"

namespace grem::graphics {

namespace detail {

struct TextureDeleter {
	void operator()(GLuint handle) const noexcept {
		glDeleteTextures(1, &handle);
	}
};

using TextureObject = UniqueHandle<GLuint, TextureDeleter>;

[[nodiscard]] inline TextureObject createTextureObject() {
	GLuint handleValue{};
	glGenTextures(1, &handleValue);
	if (handleValue == 0) {
		throw graphics::Error{"Failed to create texture object."};
	}
	return TextureObject{handleValue};
}

struct BufferDeleter {
	void operator()(GLuint handle) const noexcept {
		glDeleteBuffers(1, &handle);
	}
};

using BufferObject = UniqueHandle<GLuint, BufferDeleter>;

[[nodiscard]] inline BufferObject createBufferObject() {
	GLuint handleValue{};
	glGenBuffers(1, &handleValue);
	if (handleValue == 0) {
		throw graphics::Error{"Failed to create buffer object."};
	}
	return BufferObject{handleValue};
}

struct VertexArrayDeleter {
	void operator()(GLuint handle) const noexcept {
		glDeleteVertexArrays(1, &handle);
	}
};

using VertexArrayObject = UniqueHandle<GLuint, VertexArrayDeleter>;

[[nodiscard]] inline VertexArrayObject createVertexArrayObject() {
	GLuint handleValue{};
	glGenVertexArrays(1, &handleValue);
	if (handleValue == 0) {
		throw graphics::Error{"Failed to create vertex array object."};
	}
	return VertexArrayObject{handleValue};
}

struct ShaderDeleter {
	void operator()(GLuint handle) const noexcept {
		glDeleteShader(handle);
	}
};

using ShaderObject = UniqueHandle<GLuint, ShaderDeleter>;

[[nodiscard]] inline ShaderObject createShaderObject(GLenum type) {
	const GLuint handleValue = glCreateShader(type);
	if (handleValue == 0) {
		throw graphics::Error{"Failed to create shader object."};
	}
	return ShaderObject{handleValue};
}

struct ProgramDeleter {
	void operator()(GLuint handle) const noexcept {
		glDeleteProgram(handle);
	}
};

using ProgramObject = UniqueHandle<GLuint, ProgramDeleter>;

[[nodiscard]] inline ProgramObject createProgramObject() {
	const GLuint handleValue = glCreateProgram();
	if (handleValue == 0) {
		throw graphics::Error{"Failed to create shader program object."};
	}
	return ProgramObject{handleValue};
}

struct FramebufferDeleter {
	void operator()(GLuint handle) const noexcept {
		glDeleteFramebuffers(1, &handle);
	}
};

using FramebufferObject = UniqueHandle<GLuint, FramebufferDeleter>;

[[nodiscard]] inline FramebufferObject createFramebufferObject() {
	GLuint handleValue{};
	glGenFramebuffers(1, &handleValue);
	if (handleValue == 0) {
		throw graphics::Error{"Failed to create framebuffer object."};
	}
	return FramebufferObject{handleValue};
}

struct RenderbufferDeleter {
	void operator()(GLuint handle) const noexcept {
		glDeleteRenderbuffers(1, &handle);
	}
};

using RenderbufferObject = UniqueHandle<GLuint, RenderbufferDeleter>;

[[nodiscard]] inline RenderbufferObject createRenderbufferObject() {
	GLuint handleValue{};
	glGenRenderbuffers(1, &handleValue);
	if (handleValue == 0) {
		throw graphics::Error{"Failed to create renderbuffer object."};
	}
	return RenderbufferObject{handleValue};
}

} // namespace detail

} // namespace grem::graphics

#endif
