// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_STATE_PRESERVER_HPP
#define GREM_GRAPHICS_OPENGL_STATE_PRESERVER_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/math.hpp>

#include "opengl.hpp"

namespace grem::graphics {

namespace detail {

template <GLenum Name, auto Reset>
class IntegerStatePreserver {
public:
	[[nodiscard]] IntegerStatePreserver() noexcept {
		glGetIntegerv(Name, &oldValue);
	}

	~IntegerStatePreserver() {
		Reset(oldValue);
	}

	IntegerStatePreserver(const IntegerStatePreserver&) = delete;
	IntegerStatePreserver(IntegerStatePreserver&&) = delete;
	IntegerStatePreserver& operator=(const IntegerStatePreserver&) = delete;
	IntegerStatePreserver& operator=(IntegerStatePreserver&&) = delete;

private:
	GLint oldValue = 0;
};

template <auto Reset>
class DynamicIntegerStatePreserver {
public:
	[[nodiscard]] explicit DynamicIntegerStatePreserver(GLenum name) noexcept
		: name(name) {
		glGetIntegerv(name, &oldValue);
	}

	~DynamicIntegerStatePreserver() {
		Reset(name, oldValue);
	}

	DynamicIntegerStatePreserver(const DynamicIntegerStatePreserver&) = delete;
	DynamicIntegerStatePreserver(DynamicIntegerStatePreserver&&) = delete;
	DynamicIntegerStatePreserver& operator=(const DynamicIntegerStatePreserver&) = delete;
	DynamicIntegerStatePreserver& operator=(DynamicIntegerStatePreserver&&) = delete;

private:
	GLenum name;
	GLint oldValue = 0;
};

template <GLenum Name, auto Reset>
class FloatStatePreserver {
public:
	[[nodiscard]] FloatStatePreserver() noexcept {
		glGetFloatv(Name, &oldValue);
	}

	~FloatStatePreserver() {
		Reset(oldValue);
	}

	FloatStatePreserver(const FloatStatePreserver&) = delete;
	FloatStatePreserver(FloatStatePreserver&&) = delete;
	FloatStatePreserver& operator=(const FloatStatePreserver&) = delete;
	FloatStatePreserver& operator=(FloatStatePreserver&&) = delete;

private:
	float oldValue;
};

template <GLenum Name, auto Reset>
class Vec4StatePreserver {
public:
	[[nodiscard]] Vec4StatePreserver() noexcept {
		glGetFloatv(Name, oldValues);
	}

	~Vec4StatePreserver() {
		Reset(vec4{oldValues[0], oldValues[1], oldValues[2], oldValues[3]});
	}

	Vec4StatePreserver(const Vec4StatePreserver&) = delete;
	Vec4StatePreserver(Vec4StatePreserver&&) = delete;
	Vec4StatePreserver& operator=(const Vec4StatePreserver&) = delete;
	Vec4StatePreserver& operator=(Vec4StatePreserver&&) = delete;

private:
	float oldValues[4];
};

class TextureBindingPreserver {
public:
	[[nodiscard]] explicit TextureBindingPreserver(GLenum target) noexcept
		: target(target) {
		GLenum name{};
		switch (target) {
			case GL_TEXTURE_2D: name = GL_TEXTURE_BINDING_2D; break;
			case GL_TEXTURE_2D_ARRAY: name = GL_TEXTURE_BINDING_2D_ARRAY; break;
			case GL_TEXTURE_CUBE_MAP: name = GL_TEXTURE_BINDING_CUBE_MAP; break;
			default: unreachable();
		}
		glGetIntegerv(name, &oldValue);
	}

	~TextureBindingPreserver() {
		glBindTexture(target, static_cast<GLuint>(oldValue));
	}

	TextureBindingPreserver(const TextureBindingPreserver&) = delete;
	TextureBindingPreserver(TextureBindingPreserver&&) = delete;
	TextureBindingPreserver& operator=(const TextureBindingPreserver&) = delete;
	TextureBindingPreserver& operator=(TextureBindingPreserver&&) = delete;

private:
	GLenum target;
	GLint oldValue = 0;
};

template <GLenum Name>
class EnablementPreserver {
public:
	[[nodiscard]] EnablementPreserver() noexcept {
		glGetBooleanv(Name, &oldValue);
	}

	~EnablementPreserver() {
		if (oldValue != GL_FALSE) {
			glEnable(Name);
		} else {
			glDisable(Name);
		}
	}

	EnablementPreserver(const EnablementPreserver&) = delete;
	EnablementPreserver(EnablementPreserver&&) = delete;
	EnablementPreserver& operator=(const EnablementPreserver&) = delete;
	EnablementPreserver& operator=(EnablementPreserver&&) = delete;

private:
	GLboolean oldValue = GL_FALSE;
};

using UniformBufferBindingPreserver = IntegerStatePreserver<GL_UNIFORM_BUFFER_BINDING, +[](GLint value) -> void {
	if (value == 0 || glIsBuffer(static_cast<GLuint>(value))) {
		glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(value));
	}
}>;

using ArrayBufferBindingPreserver = IntegerStatePreserver<GL_ARRAY_BUFFER_BINDING, +[](GLint value) -> void {
	if (value == 0 || glIsBuffer(static_cast<GLuint>(value))) {
		glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(value));
	}
}>;

using TextureBinding2DPreserver = IntegerStatePreserver<GL_TEXTURE_BINDING_2D, +[](GLint value) -> void {
	if (value == 0 || glIsTexture(static_cast<GLuint>(value))) {
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(value));
	}
}>;

using TextureBinding2DArrayPreserver = IntegerStatePreserver<GL_TEXTURE_BINDING_2D_ARRAY, +[](GLint value) -> void {
	if (value == 0 || glIsTexture(static_cast<GLuint>(value))) {
		glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(value));
	}
}>;

using TextureBindingCubeMapPreserver = IntegerStatePreserver<GL_TEXTURE_BINDING_CUBE_MAP, +[](GLint value) -> void {
	if (value == 0 || glIsTexture(static_cast<GLuint>(value))) {
		glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(value));
	}
}>;

using PackAlignmentPreserver = IntegerStatePreserver<GL_PACK_ALIGNMENT, +[](GLint value) -> void {
	glPixelStorei(GL_PACK_ALIGNMENT, value);
}>;

using PackRowLengthPreserver = IntegerStatePreserver<GL_PACK_ROW_LENGTH, +[](GLint value) -> void {
	glPixelStorei(GL_PACK_ROW_LENGTH, value);
}>;

using PackSkipPixelsPreserver = IntegerStatePreserver<GL_PACK_SKIP_PIXELS, +[](GLint value) -> void {
	glPixelStorei(GL_PACK_SKIP_PIXELS, value);
}>;

using PackSkipRowsPreserver = IntegerStatePreserver<GL_PACK_SKIP_ROWS, +[](GLint value) -> void {
	glPixelStorei(GL_PACK_SKIP_ROWS, value);
}>;

#ifndef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
using PackImageHeightPreserver = IntegerStatePreserver<GL_PACK_IMAGE_HEIGHT, +[](GLint value) -> void {
	glPixelStorei(GL_PACK_IMAGE_HEIGHT, value);
}>;

using PackSkipImagesPreserver = IntegerStatePreserver<GL_PACK_SKIP_IMAGES, +[](GLint value) -> void {
	glPixelStorei(GL_PACK_SKIP_IMAGES, value);
}>;
#endif

using UnpackAlignmentPreserver = IntegerStatePreserver<GL_UNPACK_ALIGNMENT, +[](GLint value) -> void {
	glPixelStorei(GL_UNPACK_ALIGNMENT, value);
}>;

using UnpackRowLengthPreserver = IntegerStatePreserver<GL_UNPACK_ROW_LENGTH, +[](GLint value) -> void {
	glPixelStorei(GL_UNPACK_ROW_LENGTH, value);
}>;

using UnpackSkipPixelsPreserver = IntegerStatePreserver<GL_UNPACK_SKIP_PIXELS, +[](GLint value) -> void {
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, value);
}>;

using UnpackSkipRowsPreserver = IntegerStatePreserver<GL_UNPACK_SKIP_ROWS, +[](GLint value) -> void {
	glPixelStorei(GL_UNPACK_SKIP_ROWS, value);
}>;

using UnpackImageHeightPreserver = IntegerStatePreserver<GL_UNPACK_IMAGE_HEIGHT, +[](GLint value) -> void {
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, value);
}>;

using UnpackSkipImagesPreserver = IntegerStatePreserver<GL_UNPACK_SKIP_IMAGES, +[](GLint value) -> void {
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, value);
}>;

using VertexArrayBindingPreserver = IntegerStatePreserver<GL_VERTEX_ARRAY_BINDING, +[](GLint value) -> void {
	glBindVertexArray(static_cast<GLuint>(value));
}>;

using CurrentProgramPreserver = IntegerStatePreserver<GL_CURRENT_PROGRAM, +[](GLint value) -> void {
	if (value == 0 || glIsProgram(static_cast<GLuint>(value))) {
		glUseProgram(static_cast<GLuint>(value));
	}
}>;

using ReadFramebufferBindingPreserver = IntegerStatePreserver<GL_READ_FRAMEBUFFER_BINDING, +[](GLint value) -> void {
	if (value == 0 || glIsFramebuffer(static_cast<GLuint>(value))) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(value));
	}
}>;

using DrawFramebufferBindingPreserver = IntegerStatePreserver<GL_DRAW_FRAMEBUFFER_BINDING, +[](GLint value) -> void {
	if (value == 0 || glIsFramebuffer(static_cast<GLuint>(value))) {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(value));
	}
}>;

using FramebufferBindingPreserver = DynamicIntegerStatePreserver<+[](GLenum name, GLint value) -> void {
	if (value == 0 || glIsFramebuffer(static_cast<GLuint>(value))) {
		glBindFramebuffer((name == GL_READ_FRAMEBUFFER_BINDING) ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(value));
	}
}>;

using RenderbufferBindingPreserver = IntegerStatePreserver<GL_RENDERBUFFER_BINDING, +[](GLint value) -> void {
	if (value == 0 || glIsRenderbuffer(static_cast<GLuint>(value))) {
		glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(value));
	}
}>;

using ColorClearValuePreserver = Vec4StatePreserver<GL_COLOR_CLEAR_VALUE, +[](vec4 value) -> void {
	glClearColor(value.x, value.y, value.z, value.w);
}>;

using DepthClearValuePreserver = FloatStatePreserver<GL_DEPTH_CLEAR_VALUE, +[](float value) -> void {
#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE
	glClearDepthf(value);
#else
	glClearDepth(value);
#endif
}>;

using StencilClearValuePreserver = IntegerStatePreserver<GL_STENCIL_CLEAR_VALUE, +[](GLint value) -> void {
	glClearStencil(value);
}>;

using ScissorTestPreserver = EnablementPreserver<GL_SCISSOR_TEST>;

} // namespace detail

} // namespace grem::graphics

#endif
