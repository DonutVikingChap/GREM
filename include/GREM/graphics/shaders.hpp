// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_SHADERS_HPP
#define GREM_GRAPHICS_SHADERS_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Color.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/SmallArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/graphics/ConstantDescription.hpp>
#include <GREM/graphics/FieldDescription.hpp>
#include <GREM/graphics/Mesh.hpp>
#include <GREM/graphics/VertexAttributeDescription.hpp>
#include <GREM/graphics/buffer_layouts.hpp>

#include <type_traits> // std::is_empty_v, std::remove_cvref_t, std::bool_constant, std::false_type, std::true_type
#include <typeindex>   // std::type_index
#include <typeinfo>    // IWYU pragma: keep // typeid
#include <utility>     // std::move
#include <vector>      // std::vector

namespace grem {

class Filesystem; // Forward declaration, to avoid including Filesystem.hpp.

} // namespace grem

namespace grem::graphics {

class Device;     // Forward declaration, to avoid including Device.hpp.
class RenderPass; // Forward declaration, to avoid a circular include of RenderPass.hpp.

struct VertexShaderImplementation;   ///< Backend-specific implementation of VertexShader.
struct FragmentShaderImplementation; ///< Backend-specific implementation of FragmentShader.
struct ShaderPipelineImplementation; ///< Backend-specific implementation of ShaderPipeline.

/**
 * Depth buffer mode used in a ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::depthBufferMode
 */
enum class DepthBufferMode : uint8_t {
	/**
	 * Ignore the depth buffer.
	 */
	NONE,

	/**
	 * Evaluate the depth test defined by the DepthTestPredicate to determine
	 * whether the pixel should be rendered or discarded.
	 *
	 * If the test fails, the new pixel is discarded and will not be rendered.
	 * If the test succeeds, the new pixel is rendered, and the depth buffer
	 * value is overwritten with the new depth value.
	 *
	 * \note In 3D, depth testing is necessary to avoid 3D objects and faces
	 *       being incorrectly rendered on top of each other. However, for 2D
	 *       shaders, the depth test should typically be disabled in favor of
	 *       using the painter's algorithm (back-to-front rendering) instead.
	 */
	USE_DEPTH_TEST,

	/**
	 * Evaluate the depth test defined by the DepthTestPredicate to determine
	 * whether the pixel should be rendered or discarded.
	 *
	 * If the test fails, the new pixel is discarded and will not be rendered.
	 * If the test succeeds, the new pixel is rendered, but the depth value is
	 * left untouched.
	 */
	USE_DEPTH_TEST_READ_ONLY,
};

/**
 * Depth test predicate used in a ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::depthTestPredicate
 */
enum class DepthTestPredicate : uint8_t {
	/**
	 * The depth test always fails.
	 */
	NEVER_PASS,

	/**
	 * The depth test passes if and only if the new depth value is less than the
	 * old depth value.
	 */
	LESS,

	/**
	 * The depth test passes if and only if the new depth value is less than or
	 * equal to the old depth value.
	 */
	LESS_OR_EQUAL,

	/**
	 * The depth test passes if and only if the new depth value is greater than
	 * the old depth value.
	 */
	GREATER,

	/**
	 * The depth test passes if and only if the new depth value is greater than
	 * or equal to the old depth value.
	 */
	GREATER_OR_EQUAL,

	/**
	 * The depth test passes if and only if the new depth value is equal to the
	 * old depth value.
	 */
	EQUAL,

	/**
	 * The depth test passes if and only if the new depth value is not equal to
	 * the old depth value.
	 */
	NOT_EQUAL,

	/**
	 * The depth test always passes.
	 */
	ALWAYS_PASS,
};

/**
 * Stencil buffer mode used in a ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::stencilBufferMode
 */
enum class StencilBufferMode : uint8_t {
	/**
	 * Ignore the stencil buffer.
	 */
	NONE,

	/**
	 * Evaluate the stencil test defined by the StencilTestPredicate to
	 * determine whether the pixel should be rendered or discarded, then perform
	 * the corresponding StencilBufferOperation on the stencil buffer value.
	 *
	 * If the test fails, the new pixel is discarded and will not be rendered.
	 */
	USE_STENCIL_TEST,
};

/**
 * Stencil test predicate used in a ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::stencilTestPredicate
 */
enum class StencilTestPredicate : uint8_t {
	/**
	 * The stencil test always fails.
	 */
	NEVER_PASS,

	/**
	 * The stencil test passes if and only if the given reference value is less
	 * than the current value in the stencil buffer.
	 *
	 * The stencil value and the reference value are both masked with the given
	 * mask before performing the check.
	 */
	LESS,

	/**
	 * The stencil test passes if and only if the given reference value is less
	 * than or equal to the current value in the stencil buffer.
	 *
	 * The stencil value and the reference value are both masked with the given
	 * mask before performing the check.
	 */
	LESS_OR_EQUAL,

	/**
	 * The stencil test passes if and only if the given reference value is
	 * greater than the current value in the stencil buffer.
	 *
	 * The stencil value and the reference value are both masked with the given
	 * mask before performing the check.
	 */
	GREATER,

	/**
	 * The stencil test passes if and only if the given reference value is
	 * greater than or equal to the current value in the stencil buffer.
	 *
	 * The stencil value and the reference value are both masked with the given
	 * mask before performing the check.
	 */
	GREATER_OR_EQUAL,

	/**
	 * The stencil test passes if and only if the given reference value is equal
	 * to the current value in the stencil buffer.
	 *
	 * The stencil value and the reference value are both masked with the given
	 * mask before performing the check.
	 */
	EQUAL,

	/**
	 * The stencil test passes if and only if the given reference value is not
	 * equal to the current value in the stencil buffer.
	 *
	 * The stencil value and the reference value are both masked with the given
	 * mask before performing the check.
	 */
	NOT_EQUAL,

	/**
	 * The stencil test always passes.
	 */
	ALWAYS_PASS,
};

/**
 * Operation to perform after evaluating the stencil test in a
 * ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::stencilBufferOperationOnStencilTestFail
 * \sa ShaderPipelineOptions::stencilBufferOperationOnDepthTestFail
 * \sa ShaderPipelineOptions::stencilBufferOperationOnPass
 */
enum class StencilBufferOperation : uint8_t {
	/**
	 * Keep the current value in the stencil buffer.
	 */
	KEEP,

	/**
	 * Set the stencil buffer value to 0.
	 */
	SET_TO_ZERO,

	/**
	 * Set the stencil buffer value to the given reference value.
	 */
	REPLACE,

	/**
	 * Increment the stencil buffer value by 1, unless it is already maxed out.
	 */
	INCREMENT_AND_CLAMP,

	/**
	 * Increment the stencil buffer value by 1, or wrap around to 0 if it was
	 * maxed out.
	 */
	INCREMENT_AND_WRAP,

	/**
	 * Decrement the stencil buffer value by 1, unless it is already 0.
	 */
	DECREMENT_AND_CLAMP,

	/**
	 * Decrement the stencil buffer value by 1, or wrap around to the maximum
	 * value if it was 0.
	 */
	DECREMENT_AND_WRAP,

	/**
	 * Toggle each bit in the stencil buffer value.
	 */
	BITWISE_INVERT,
};

/**
 * Graphical primitive type formed by mesh vertices, used in a
 * ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::primitiveType
 */
enum class PrimitiveType : uint8_t {
	POINTS = 0, ///< Individual points. \hideinitializer
	LINES = 1,  ///< Each consecutive pair of points forms an individual line segment. \hideinitializer
	// Note: LINE_LOOP is intentionally omitted since Vulkan does not support it.
	LINE_STRIP = 3,     ///< Each point, except the first, forms a line segment to the previous point. \hideinitializer
	TRIANGLES = 4,      ///< Each consecutive triple of points forms an individual filled triangle. \hideinitializer
	TRIANGLE_STRIP = 5, ///< Each point, except the first two, forms a filled triangle with the previous two points. \hideinitializer
};

/**
 * Polygon mode used in a ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::polygonMode
 */
enum class PolygonMode : uint8_t {
	POINT, ///< Polygon vertices are drawn as single-pixel points.
	LINE,  ///< Polygon edges are drawn as single-pixel-wide line segments.
	FILL,  ///< Polygon faces are filled in.
};

/**
 * Face culling mode used in a ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::faceCullingMode
 */
enum class FaceCullingMode : uint8_t {
	/**
	 * Ignore facing, don't cull primitives based on their winding order.
	 */
	NONE,

	/**
	 * Discard back-facing faces.
	 */
	CULL_BACK_FACES,

	/**
	 * Discard front-facing faces.
	 */
	CULL_FRONT_FACES,

	/**
	 * Discard all faces, only render primitives without faces, such as lines
	 * and points.
	 */
	CULL_FRONT_AND_BACK_FACES,
};

/**
 * Front face winding order used in a ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::frontFace
 */
enum class FrontFace : uint8_t {
	COUNTERCLOCKWISE = 0, ///< Consider faces with counterclockwise winding order to be front-facing. \hideinitializer
	CLOCKWISE = 1,        ///< Consider faces with clockwise winding order to be front-facing. \hideinitializer
};

/**
 * Blend factor used in the BlendState of a ShaderPipelineOptions.
 *
 * \sa BlendState
 */
enum class BlendFactor : uint8_t {
	ZERO,                         ///< Multiply all components by 0.
	ONE,                          ///< Multiply all components by 1.
	SOURCE_COLOR,                 ///< Multiply all components by the source components.
	ONE_MINUS_SOURCE_COLOR,       ///< Multiply all components by 1 minus each source component.
	DESTINATION_COLOR,            ///< Multiply all components by the destination components.
	ONE_MINUS_DESTINATION_COLOR,  ///< Multiply all components by 1 minus each destination component.
	SOURCE_ALPHA,                 ///< Multiply all components by the source alpha.
	ONE_MINUS_SOURCE_ALPHA,       ///< Multiply all components by 1 minus the source alpha.
	DESTINATION_ALPHA,            ///< Multiply all components by the destination alpha.
	ONE_MINUS_DESTINATION__ALPHA, ///< Multiply all components by 1 minus the destination alpha.
	CONSTANT_COLOR,               ///< Multiply all components by the constant components.
	ONE_MINUS_CONSTANT_COLOR,     ///< Multiply all components by 1 minus each constant component.
	CONSTANT_ALPHA,               ///< Multiply all components by the constant alpha.
	ONE_MINUS_CONSTANT_ALPHA,     ///< Multiply all components by 1 minus the constant alpha.
	SOURCE_ALPHA_SATURATE,        ///< Multiply the color components by the smaller of the source alpha or 1 minus the destination alpha, and multiply the alpha component by 1.
};

/**
 * Blend operation used in the BlendState of a ShaderPipelineOptions.
 *
 * \sa BlendState
 */
enum class BlendOperation : uint8_t {
	ADD,              ///< Add the blended components.
	SUBTRACT,         ///< Subtract the blended destination components from the blended source components.
	REVERSE_SUBTRACT, ///< Subtract the blended source components from the blended destination components.
	MIN,              ///< Take the minimum of the components.
	MAX,              ///< Take the maximum of the components.
};

/**
 * Blend state used in a ShaderPipelineOptions.
 *
 * \sa ShaderPipelineOptions::blendState
 */
struct BlendState {
	static const BlendState ALPHA_BLENDING_STRAIGHT;      ///< Blend state to use for straight alpha blending using the standard "over" operator.
	static const BlendState ALPHA_BLENDING_PREMULTIPLIED; ///< Blend state to use for pre-multiplied alpha blending using the standard "over" operator.

	BlendFactor sourceColorBlendFactor = BlendFactor::ONE;       ///< Blend factor of the source color.
	BlendFactor destinationColorBlendFactor = BlendFactor::ZERO; ///< Blend factor of the destination color.
	BlendOperation colorBlendOperation = BlendOperation::ADD;    ///< Color blend operation.
	BlendFactor sourceAlphaBlendFactor = BlendFactor::ONE;       ///< Blend factor of the source alpha.
	BlendFactor destinationAlphaBlendFactor = BlendFactor::ZERO; ///< Blend factor of the destination alpha.
	BlendOperation alphaBlendOperation = BlendOperation::ADD;    ///< Alpha blend operation.
	Color blendConstants = Color::WHITE;                         ///< Constant blend color.

	/**
	 * Compare this blend state to another for equality.
	 *
	 * \param other the blend state to compare this one to.
	 *
	 * \return true if the blend states are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const BlendState& other) const noexcept = default;
};

inline constexpr BlendState BlendState::ALPHA_BLENDING_STRAIGHT{
	.sourceColorBlendFactor = BlendFactor::SOURCE_ALPHA,
	.destinationColorBlendFactor = BlendFactor::ONE_MINUS_SOURCE_ALPHA,
	.sourceAlphaBlendFactor = BlendFactor::ONE,
	.destinationAlphaBlendFactor = BlendFactor::ONE_MINUS_SOURCE_ALPHA,
};

inline constexpr BlendState BlendState::ALPHA_BLENDING_PREMULTIPLIED{
	.sourceColorBlendFactor = BlendFactor::ONE,
	.destinationColorBlendFactor = BlendFactor::ONE_MINUS_SOURCE_ALPHA,
	.sourceAlphaBlendFactor = BlendFactor::ONE,
	.destinationAlphaBlendFactor = BlendFactor::ONE_MINUS_SOURCE_ALPHA,
};

/**
 * Configuration of a graphics pipeline.
 */
struct ShaderPipelineOptions {
	/**
	 * How to treat the depth buffer for each pixel being rendered.
	 *
	 * \sa #depthTestPredicate
	 */
	DepthBufferMode depthBufferMode = DepthBufferMode::USE_DEPTH_TEST;

	/**
	 * The condition to check when evaluating the depth test.
	 *
	 * \sa #depthBufferMode
	 */
	DepthTestPredicate depthTestPredicate = DepthTestPredicate::LESS_OR_EQUAL;

	/**
	 * How to treat the stencil buffer for each pixel being rendered.
	 *
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 * \sa #stencilTestBackFacePredicate
	 * \sa #stencilTestBackFaceReferenceValue
	 * \sa #stencilTestBackFaceMask
	 * \sa #stencilBufferOperationOnBackFaceStencilTestFail
	 * \sa #stencilBufferOperationOnBackFaceDepthTestFail
	 * \sa #stencilBufferOperationOnBackFacePass
	 */
	StencilBufferMode stencilBufferMode = StencilBufferMode::NONE;

	/**
	 * The condition to check when evaluating the stencil test for front faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	StencilTestPredicate stencilTestFrontFacePredicate = StencilTestPredicate::ALWAYS_PASS;

	/**
	 * The reference value to compare the stencil buffer value against when
	 * evaluating the stencil test for front faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	int8_t stencilTestFrontFaceReferenceValue = 0;

	/**
	 * The bit pattern to mask the reference value and stencil value with
	 * before performing the stencil test for front faces.
	 *
	 * The set bits in the mask indicate the relevant bits that will be
	 * compared.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	uint8_t stencilTestFrontFaceMask = 0xFF;

	/**
	 * The operation to perform on the stencil buffer when the stencil test
	 * fails for front faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	StencilBufferOperation stencilBufferOperationOnFrontFaceStencilTestFail = StencilBufferOperation::KEEP;

	/**
	 * The operation to perform on the stencil buffer when the stencil test
	 * passes, but the depth test fails, for front faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	StencilBufferOperation stencilBufferOperationOnFrontFaceDepthTestFail = StencilBufferOperation::KEEP;

	/**
	 * The operation to perform on the stencil buffer when the stencil test and
	 * the depth test both pass for front faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 */
	StencilBufferOperation stencilBufferOperationOnFrontFacePass = StencilBufferOperation::KEEP;

	/**
	 * The condition to check when evaluating the stencil test for back faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	StencilTestPredicate stencilTestBackFacePredicate = StencilTestPredicate::ALWAYS_PASS;

	/**
	 * The reference value to compare the stencil buffer value against when
	 * evaluating the stencil test for back faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	int8_t stencilTestBackFaceReferenceValue = 0;

	/**
	 * The bit pattern to mask the reference value and stencil value with
	 * before performing the stencil test for back faces.
	 *
	 * The set bits in the mask indicate the relevant bits that will be
	 * compared.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	uint8_t stencilTestBackFaceMask = 0xFF;

	/**
	 * The operation to perform on the stencil buffer when the stencil test
	 * fails for back faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	StencilBufferOperation stencilBufferOperationOnBackFaceStencilTestFail = StencilBufferOperation::KEEP;

	/**
	 * The operation to perform on the stencil buffer when the stencil test
	 * passes, but the depth test fails, for back faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFacePass
	 */
	StencilBufferOperation stencilBufferOperationOnBackFaceDepthTestFail = StencilBufferOperation::KEEP;

	/**
	 * The operation to perform on the stencil buffer when the stencil test and
	 * the depth test both pass for back faces.
	 *
	 * \sa #stencilBufferMode
	 * \sa #stencilTestFrontFacePredicate
	 * \sa #stencilTestFrontFaceReferenceValue
	 * \sa #stencilTestFrontFaceMask
	 * \sa #stencilBufferOperationOnFrontFaceStencilTestFail
	 * \sa #stencilBufferOperationOnFrontFaceDepthTestFail
	 */
	StencilBufferOperation stencilBufferOperationOnBackFacePass = StencilBufferOperation::KEEP;

	/**
	 * How to form primitives out of consecutive mesh vertices.
	 */
	PrimitiveType primitiveType = PrimitiveType::TRIANGLES;

	/**
	 * How to render polygons.
	 */
	PolygonMode polygonMode = PolygonMode::FILL;

	/**
	 * How to treat the facing of primitives while rendering.
	 *
	 * The facing is determined by the winding order of the vertices on each
	 * rendered face. The face is considered to be front-facing if it has the
	 * winding order specified by #frontFace.
	 *
	 * \sa #frontFace
	 */
	FaceCullingMode faceCullingMode = FaceCullingMode::CULL_BACK_FACES;

	/**
	 * The winding order of front-facing faces.
	 *
	 * \sa #faceCullingMode
	 */
	FrontFace frontFace = FrontFace::COUNTERCLOCKWISE;

	/**
	 * How to blend the pixel color with the output while rendering.
	 */
	Optional<BlendState> blendState{};

	/**
	 * Slope-scaled depth bias factor.
	 *
	 * \sa #depthBiasConstantFactor
	 */
	float depthBiasSlopeFactor = 0.0f;

	/**
	 * Constant depth bias factor.
	 *
	 * \sa #depthBiasSlopeFactor
	 */
	float depthBiasConstantFactor = 0.0f;

	/**
	 * Compare this configuration to another for equality.
	 *
	 * \param other the configuration to compare this one to.
	 *
	 * \return true if the configurations are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const ShaderPipelineOptions& other) const noexcept = default;
};

/**
 * Options for compiling shader code.
 */
struct ShaderCompilationOptions {
	/**
	 * Whether to optimize the generated code or leave it more debuggable.
	 */
	bool optimize = true;
};

/**
 * Configuration options for loading a VertexShader.
 */
struct VertexShaderOptions {
	/**
	 * Under the Vulkan backend, if appending ".spv" to the specified filepath
	 * yields the path of a valid file, load that file instead of the specified
	 * filepath.
	 *
	 * \sa #compiledFileDirectory
	 */
	bool useCorrespondingCompiledFileIfAvailable = true;

	/**
	 * If non-empty, and #useCorrespondingCompiledFileIfAvailable is true,
	 * specifies the directory in which to look for the corresponding compiled
	 * file instead of looking in the same directory as the specified file.
	 */
	CStringView compiledFileDirectory{};
};

/**
 * Configuration options for loading a FragmentShader.
 */
struct FragmentShaderOptions {
	/**
	 * Under the Vulkan backend, if appending ".spv" to the specified filepath
	 * yields the path of a valid file, load that file instead of the specified
	 * filepath.
	 *
	 * \sa #compiledFileDirectory
	 */
	bool useCorrespondingCompiledFileIfAvailable = true;

	/**
	 * If non-empty, and #useCorrespondingCompiledFileIfAvailable is true,
	 * specifies the directory in which to look for the corresponding compiled
	 * file instead of looking in the same directory as the specified file.
	 */
	CStringView compiledFileDirectory{};
};

namespace detail {

class VertexShaderBase {
public:
	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<VertexShaderImplementation> lock() const noexcept {
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
	[[nodiscard]] VertexShaderImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	[[nodiscard]] GREM_API(graphics) static std::vector<uint32_t> compileGLSLToVulkanSPIRVImplementation(CStringView sourceCode, const Filesystem* filesystem, CStringView filepath,
		const ShaderCompilationOptions& compilationOptions, Span<const ConstantDescription> constantDescriptions,
		Span<const VertexAttributeDescription> vertexAttributeDescriptions, Span<const ParameterDescription> parameterDescriptions,
		Span<const FieldDescription> instanceAttributeDescriptions, Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts);

	GREM_API(graphics)
	VertexShaderBase(Device& device, CStringView sourceCode, const Filesystem* filesystem, CStringView filepath, Span<const ConstantDescription> constantDescriptions,
		std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions, Optional<MeshIndexType> indexType,
		Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
		Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts);

	GREM_API(graphics)
	VertexShaderBase(Device& device, Span<const uint32_t> code, Span<const ConstantDescription> constantDescriptions, std::type_index meshTypeIndex,
		Span<const VertexAttributeDescription> vertexAttributeDescriptions, Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions,
		Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride, Span<const FieldDescription> outputFieldDescriptions,
		Span<const BufferLayoutReference> bufferLayouts);

	GREM_API(graphics)
	VertexShaderBase(Device& device, const Filesystem& filesystem, CStringView filepath, const VertexShaderOptions& options, Span<const ConstantDescription> constantDescriptions,
		std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions, Optional<MeshIndexType> indexType,
		Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
		Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts);

private:
	SharedPointer<VertexShaderImplementation> implementation{};
};

class FragmentShaderBase {
public:
	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<FragmentShaderImplementation> lock() const noexcept {
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
	[[nodiscard]] FragmentShaderImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	[[nodiscard]] GREM_API(graphics) static std::vector<uint32_t> compileGLSLToVulkanSPIRVImplementation(CStringView sourceCode, const Filesystem* filesystem, CStringView filepath,
		const ShaderCompilationOptions& compilationOptions, Span<const ConstantDescription> constantDescriptions, Span<const ParameterDescription> parameterDescriptions,
		Span<const FieldDescription> instanceAttributeDescriptions, Span<const FieldDescription> inputFieldDescriptions, Span<const FieldDescription> outputFieldDescriptions,
		Span<const BufferLayoutReference> bufferLayouts);

	GREM_API(graphics)
	FragmentShaderBase(Device& device, CStringView sourceCode, const Filesystem* filesystem, CStringView filepath, Span<const ConstantDescription> constantDescriptions,
		std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions, Optional<MeshIndexType> indexType,
		Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride,
		Span<const FieldDescription> inputFieldDescriptions, Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts);

	GREM_API(graphics)
	FragmentShaderBase(Device& device, Span<const uint32_t> code, Span<const ConstantDescription> constantDescriptions, std::type_index meshTypeIndex,
		Span<const VertexAttributeDescription> vertexAttributeDescriptions, Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions,
		Span<const FieldDescription> instanceAttributeDescriptions, uint32_t instanceStride, Span<const FieldDescription> inputFieldDescriptions,
		Span<const FieldDescription> outputFieldDescriptions, Span<const BufferLayoutReference> bufferLayouts);

	GREM_API(graphics)
	FragmentShaderBase(Device& device, const Filesystem& filesystem, CStringView filepath, const FragmentShaderOptions& options,
		Span<const ConstantDescription> constantDescriptions, std::type_index meshTypeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions,
		Optional<MeshIndexType> indexType, Span<const ParameterDescription> parameterDescriptions, Span<const FieldDescription> instanceAttributeDescriptions,
		uint32_t instanceStride, Span<const FieldDescription> inputFieldDescriptions, Span<const FieldDescription> outputFieldDescriptions,
		Span<const BufferLayoutReference> bufferLayouts);

private:
	SharedPointer<FragmentShaderImplementation> implementation{};
};

class ShaderPipelineBase {
public:
	/**
	 * Get a lock for the underlying resource implementation.
	 *
	 * \return a shared resource handle to the underlying resource.
	 *
	 * \note The type of the returned resource is backend-specific and has no
	 *       meaning to application code.
	 */
	[[nodiscard]] SharedPointer<ShaderPipelineImplementation> lock() const noexcept {
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
	[[nodiscard]] ShaderPipelineImplementation* get() const noexcept {
		return implementation.get();
	}

protected:
	GREM_API(graphics)
	ShaderPipelineBase(Device& device, std::type_index meshTypeIndex, SharedPointer<VertexShaderImplementation> vertexShaderHandle,
		Span<const ConstantDescription> vertexShaderConstantDescriptions, Span<const byte> vertexShaderConstantData,
		SharedPointer<FragmentShaderImplementation> fragmentShaderHandle, Span<const ConstantDescription> fragmentShaderConstantDescriptions,
		Span<const byte> fragmentShaderConstantData, const ShaderPipelineOptions& shaderPipelineOptions);

private:
	SharedPointer<ShaderPipelineImplementation> implementation{};
};

[[nodiscard]] inline StringView getVertexAttributeTypeName(VertexAttributeType vertexAttributeType) noexcept {
	switch (vertexAttributeType) {
		case VertexAttributeType::U8NORM: [[fallthrough]];
		case VertexAttributeType::I8NORM: [[fallthrough]];
		case VertexAttributeType::U16NORM: [[fallthrough]];
		case VertexAttributeType::I16NORM: [[fallthrough]];
		case VertexAttributeType::F16: [[fallthrough]];
		case VertexAttributeType::F32: return "float";
		case VertexAttributeType::U8: [[fallthrough]];
		case VertexAttributeType::U16: [[fallthrough]];
		case VertexAttributeType::U32: return "uint";
		case VertexAttributeType::I8: [[fallthrough]];
		case VertexAttributeType::I16: [[fallthrough]];
		case VertexAttributeType::I32: return "int";
		case VertexAttributeType::U8VEC2NORM: [[fallthrough]];
		case VertexAttributeType::I8VEC2NORM: [[fallthrough]];
		case VertexAttributeType::U16VEC2NORM: [[fallthrough]];
		case VertexAttributeType::I16VEC2NORM: [[fallthrough]];
		case VertexAttributeType::F16VEC2: [[fallthrough]];
		case VertexAttributeType::F32VEC2: return "vec2";
		case VertexAttributeType::U8VEC2: [[fallthrough]];
		case VertexAttributeType::U16VEC2: [[fallthrough]];
		case VertexAttributeType::U32VEC2: return "uvec2";
		case VertexAttributeType::I8VEC2: [[fallthrough]];
		case VertexAttributeType::I16VEC2: [[fallthrough]];
		case VertexAttributeType::I32VEC2: return "ivec2";
		case VertexAttributeType::F32VEC3: return "vec3";
		case VertexAttributeType::U32VEC3: return "uvec3";
		case VertexAttributeType::I32VEC3: return "ivec3";
		case VertexAttributeType::U8VEC4NORM: [[fallthrough]];
		case VertexAttributeType::I8VEC4NORM: [[fallthrough]];
		case VertexAttributeType::UA2B10G10R10VEC4NORM: [[fallthrough]];
		case VertexAttributeType::IA2B10G10R10VEC4NORM: [[fallthrough]];
		case VertexAttributeType::U16VEC4NORM: [[fallthrough]];
		case VertexAttributeType::I16VEC4NORM: [[fallthrough]];
		case VertexAttributeType::F16VEC4: [[fallthrough]];
		case VertexAttributeType::F32VEC4: return "vec4";
		case VertexAttributeType::U8VEC4: [[fallthrough]];
		case VertexAttributeType::U16VEC4: [[fallthrough]];
		case VertexAttributeType::U32VEC4: return "uvec4";
		case VertexAttributeType::I8VEC4: [[fallthrough]];
		case VertexAttributeType::I16VEC4: [[fallthrough]];
		case VertexAttributeType::I32VEC4: return "ivec4";
	}
	return {};
}

[[nodiscard]] inline size_t getFieldAttributeCount(FieldType fieldType) noexcept {
	switch (fieldType) {
		case FieldType::INT: [[fallthrough]];
		case FieldType::IVEC2: [[fallthrough]];
		case FieldType::IVEC3: [[fallthrough]];
		case FieldType::IVEC4: [[fallthrough]];
		case FieldType::UINT: [[fallthrough]];
		case FieldType::UVEC2: [[fallthrough]];
		case FieldType::UVEC3: [[fallthrough]];
		case FieldType::UVEC4: [[fallthrough]];
		case FieldType::FLOAT: [[fallthrough]];
		case FieldType::VEC2: [[fallthrough]];
		case FieldType::VEC3: [[fallthrough]];
		case FieldType::VEC4: return 1;
		case FieldType::MAT2: return 2;
		case FieldType::MAT3: return 3;
		case FieldType::MAT4: return 4;
	}
	return 0;
}

[[nodiscard]] inline StringView getFieldTypeName(FieldType fieldType) noexcept {
	switch (fieldType) {
		case FieldType::INT: return "int";
		case FieldType::IVEC2: return "ivec2";
		case FieldType::IVEC3: return "ivec3";
		case FieldType::IVEC4: return "ivec4";
		case FieldType::UINT: return "uint";
		case FieldType::UVEC2: return "uvec2";
		case FieldType::UVEC3: return "uvec3";
		case FieldType::UVEC4: return "uvec4";
		case FieldType::FLOAT: return "float";
		case FieldType::VEC2: return "vec2";
		case FieldType::VEC3: return "vec3";
		case FieldType::VEC4: return "vec4";
		case FieldType::MAT2: return "mat2";
		case FieldType::MAT3: return "mat3";
		case FieldType::MAT4: return "mat4";
	}
	return {};
}

[[nodiscard]] inline char getVec4ComponentCharacter(size_t componentIndex) {
	switch (componentIndex) {
		case 0: return 'x';
		case 1: return 'y';
		case 2: return 'z';
		case 3: return 'w';
	}
	unreachable();
}

[[nodiscard]] inline bool isValidNameStartCharacter(char ch) noexcept {
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

[[nodiscard]] inline bool isValidNameCharacter(char ch) noexcept {
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch == '_') || (ch >= '0' && ch <= '9');
}

[[nodiscard]] inline bool isValidName(CStringView name) noexcept {
	if (name.empty() || !isValidNameStartCharacter(name.front())) {
		return false;
	}
	for (const char ch : name) { // NOLINT(readability-use-anyofallof) // std::all_of doesn't support sentinels.
		if (!isValidNameCharacter(ch)) {
			return false;
		}
	}
	return true;
}

GREM_API(graphics) void writeInputAttributeDeclarations(String& output, size_t& attributeIndex, Span<const VertexAttributeDescription> vertexAttributeDescriptions);
GREM_API(graphics) void writeInputAttributeDeclarations(String& output, size_t& attributeIndex, Span<const FieldDescription> fieldDescriptions);

GREM_API(graphics)
void writeVec4BufferGetters(String& output, StringView getterFunctionNamePrefix, Span<const FieldDescription> fieldDescriptions,
	FunctionView<void(String& output, StringView nameString, StringView indexString)> writeFetchDeclaration);

using AllocatedStringBuffer = SmallArrayList<Allocation<char>, 16>;
using ExpandedStringBuffer = SmallArrayList<const char*, 16>;

[[nodiscard]] GREM_API(graphics) ExpandedStringBuffer
	expandIncludes(AllocatedStringBuffer& allocatedStrings, Span<const char* const> sourceStrings, const Filesystem* filesystem, CStringView filepath);

} // namespace detail

/**
 * Compiled GPU vertex shader stage.
 *
 * \tparam Mesh concrete mesh type that will be rendered by this shader,
 *         defining its input layout.
 * \tparam Constants user-defined aggregate type of external constants that are
 *         declared in the shader.
 * \tparam Outputs user-defined aggregate type of fields that are declared in
 *         the shader and passed to the next pipeline stage.
 * \tparam Buffers list of buffers that the shader reads from.
 */
template <typename Mesh, typename Constants, typename Outputs, typename... Buffers>
class VertexShader;

template <typename Vertex, typename Index, typename Parameters, typename Instance, typename Constants, typename Outputs, typename... Buffers>
class VertexShader<graphics::Mesh<Vertex, Index, Parameters, Instance>, Constants, Outputs, Buffers...> : public detail::VertexShaderBase {
public:
	static_assert(shader_constant_struct<Constants>);
	static_assert(field_struct<Outputs>);

	/** Mesh type of the shader. */
	using mesh_type = graphics::Mesh<Vertex, Index, Parameters, Instance>;

	/** Constants type of the shader. */
	using constants_type = Constants;

	/** Outputs type of the shader. */
	using outputs_type = Outputs;

	/** Buffer type list of the shader. */
	using buffer_types = meta::TypeList<Buffers...>;

	/**
	 * Compile a vertex shader from GLSL source code into SPIR-V code for
	 * Vulkan for this specic shader type.
	 *
	 * \param sourceCode string of GLSL source code for the vertex shader.
	 * \param compilationOptions compilation options, see
	 *        ShaderCompilationOptions.
	 *
	 * \return the compiled SPIR-V code for the shader module.
	 *
	 * \throws graphics::Error on failure to compile the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The compiled SPIR-V module is only compatible with this specific
	 *          vertex shader type and its exact given template arguments.
	 * \warning This function only works under the Vulkan graphics backend, and
	 *          also requires GLSL compilation to be enabled.
	 */
	[[nodiscard]] static std::vector<uint32_t> compileGLSLToVulkanSPIRV(CStringView sourceCode, const ShaderCompilationOptions& compilationOptions = {}) {
		return detail::VertexShaderBase::compileGLSLToVulkanSPIRVImplementation(sourceCode, nullptr, {}, compilationOptions, detail::CONSTANT_DESCRIPTIONS<Constants>,
			detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>, detail::FIELD_DESCRIPTIONS<Outputs>,
			detail::BUFFER_LAYOUT_REFERENCES<Buffers...>);
	}

	/**
	 * Compile a vertex shader from GLSL source code into SPIR-V code for
	 * Vulkan for this specic shader type, with support for file inclusion.
	 *
	 * \param sourceCode string of GLSL source code for the vertex shader.
	 * \param filesystem filesystem to load included files from.
	 * \param filepath filepath of the given source code, whose parent directory
	 *        double-quoted include directives will be relative to.
	 * \param compilationOptions compilation options, see
	 *        ShaderCompilationOptions.
	 *
	 * \return the compiled SPIR-V code for the shader module.
	 *
	 * \throws File::Error on failure to open a file.
	 * \throws graphics::Error on failure to compile the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The compiled SPIR-V module is only compatible with this specific
	 *          vertex shader type and its exact given template arguments.
	 * \warning This function only works under the Vulkan graphics backend, and
	 *          also requires GLSL compilation to be enabled.
	 */
	[[nodiscard]] static std::vector<uint32_t> compileGLSLToVulkanSPIRV(CStringView sourceCode, const Filesystem& filesystem, CStringView filepath = {},
		const ShaderCompilationOptions& compilationOptions = {}) {
		return detail::VertexShaderBase::compileGLSLToVulkanSPIRVImplementation(sourceCode, &filesystem, filepath, compilationOptions, detail::CONSTANT_DESCRIPTIONS<Constants>,
			detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>, detail::FIELD_DESCRIPTIONS<Outputs>,
			detail::BUFFER_LAYOUT_REFERENCES<Buffers...>);
	}

	/**
	 * Create a vertex shader from GLSL source code.
	 *
	 * \param device device to create the shader stage for. Must outlive the
	 *        shader stage.
	 * \param sourceCode string of GLSL source code for the vertex shader.
	 *
	 * \return the vertex shader stage.
	 *
	 * \throws graphics::Error on failure to create the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function requires GLSL compilation to be supported by the
	 *          current graphics backend, which can be queried by calling
	 *          `device.getSupportedFeatures()` and reading the field
	 *          `FeatureSupport::supportsGLSLShaderCode`.
	 */
	[[nodiscard]] static VertexShader create(Device& device, CStringView sourceCode) {
		return VertexShader{device, sourceCode, nullptr, {}};
	}

	/**
	 * Create a vertex shader from GLSL source code, with support for file
	 * inclusion.
	 *
	 * \param device device to create the shader stage for.
	 *        Must outlive the shader stage.
	 * \param sourceCode string of GLSL source code for the vertex shader.
	 * \param filesystem filesystem to load included files from.
	 * \param filepath filepath of the given source code, whose parent directory
	 *        double-quoted include directives will be relative to.
	 *
	 * \return the vertex shader stage.
	 *
	 * \throws File::Error on failure to open a file.
	 * \throws graphics::Error on failure to create the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function requires GLSL compilation to be supported by the
	 *          current graphics backend, which can be queried by calling
	 *          `device.getSupportedFeatures()` and reading the field
	 *          `FeatureSupport::supportsGLSLShaderCode`.
	 */
	[[nodiscard]] static VertexShader create(Device& device, CStringView sourceCode, const Filesystem& filesystem, CStringView filepath = {}) {
		return VertexShader{device, sourceCode, &filesystem, filepath};
	}

	/**
	 * Create a vertex shader from SPIR-V code.
	 *
	 * \param device device to create the shader stage for. Must outlive the
	 *        shader stage.
	 * \param code SPIR-V code for the vertex shader. Must be compatible with
	 *        the shader's template parameters.
	 *
	 * \return the vertex shader stage.
	 *
	 * \throws graphics::Error on failure to create the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function requires SPIR-V code to be supported by the
	 *          current graphics backend, which can be queried by calling
	 *          `device.getSupportedFeatures()` and reading the field
	 *          `FeatureSupport::supportsSPIRVShaderCode`.
	 * \warning Supplying invalid SPIR-V, or SPIR-V that is incompatible with
	 *          the template parameters of this shader, yields undefined
	 *          behavior.
	 */
	[[nodiscard]] static VertexShader create(Device& device, Span<const uint32_t> code) {
		return VertexShader{device, code};
	}

	/**
	 * Load a vertex shader from a file.
	 *
	 * The supported file formats are:
	 * - GLSL (.glsl, .vert)
	 * - SPIR-V (.spv)
	 *
	 * \param device device to create the shader stage for. Must outlive the
	 *        shader stage.
	 * \param filesystem filesystem to load the file from.
	 * \param filepath input filepath of the shader file to load.
	 * \param options vertex shader options, see VertexShaderOptions.
	 *
	 * \throws File::Error on failure to open a file.
	 * \throws graphics::Error on failure to create the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The file format is determined entirely from the file contents; the
	 *       filename extension is not taken into account.
	 *
	 * \warning This function requires the file format to be supported by the
	 *          current graphics backend, which can be queried by calling
	 *          `device.getSupportedFeatures()`.
	 * \warning If the file format is SPIR-V, supplying invalid SPIR-V, or
	 *          SPIR-V that is incompatible with the template parameters of this
	 *          shader, yields undefined behavior.
	 */
	explicit VertexShader(Device& device, const Filesystem& filesystem, CStringView filepath, const VertexShaderOptions& options = {})
		: detail::VertexShaderBase(device, filesystem, filepath, options, detail::CONSTANT_DESCRIPTIONS<Constants>, typeid(mesh_type),
			  detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, detail::MESH_INDEX_TYPE<Index>, detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>,
			  (std::is_empty_v<Instance>) ? 0 : sizeof(Instance), detail::FIELD_DESCRIPTIONS<Outputs>, detail::BUFFER_LAYOUT_REFERENCES<Buffers...>) {}

private:
	explicit VertexShader(Device& device, CStringView sourceCode, const Filesystem* filesystem, CStringView filepath)
		: detail::VertexShaderBase(device, sourceCode, filesystem, filepath, detail::CONSTANT_DESCRIPTIONS<Constants>, typeid(mesh_type),
			  detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, detail::MESH_INDEX_TYPE<Index>, detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>,
			  (std::is_empty_v<Instance>) ? 0 : sizeof(Instance), detail::FIELD_DESCRIPTIONS<Outputs>, detail::BUFFER_LAYOUT_REFERENCES<Buffers...>) {}

	explicit VertexShader(Device& device, Span<const uint32_t> code)
		: detail::VertexShaderBase(device, code, detail::CONSTANT_DESCRIPTIONS<Constants>, typeid(mesh_type), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>,
			  detail::MESH_INDEX_TYPE<Index>, detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>, (std::is_empty_v<Instance>) ? 0 : sizeof(Instance),
			  detail::FIELD_DESCRIPTIONS<Outputs>, detail::BUFFER_LAYOUT_REFERENCES<Buffers...>) {}
};

/**
 * Compiled GPU fragment shader stage.
 *
 * \tparam Mesh concrete mesh type that will be rendered by this shader.
 * \tparam Inputs user-defined aggregate type of fields that are passed into the
 *         shader from the previous pipeline stage.
 * \tparam Constants user-defined aggregate type of external constants that are
 *         declared in the shader.
 * \tparam Outputs user-defined aggregate type of fields that are declared in
 *         the shader and passed to the next pipeline stage.
 * \tparam Buffers list of buffers that the shader reads from.
 */
template <typename Mesh, typename Inputs, typename Constants, typename Outputs, typename... Buffers>
class FragmentShader;

template <typename Vertex, typename Index, typename Parameters, typename Instance, typename Inputs, typename Constants, typename Outputs, typename... Buffers>
class FragmentShader<graphics::Mesh<Vertex, Index, Parameters, Instance>, Inputs, Constants, Outputs, Buffers...> : public detail::FragmentShaderBase {
public:
	static_assert(shader_constant_struct<Constants>);
	static_assert(field_struct<Inputs>);
	static_assert(field_struct<Outputs>);

	/** Mesh type of the shader. */
	using mesh_type = graphics::Mesh<Vertex, Index, Parameters, Instance>;

	/** Inputs type of the shader. */
	using inputs_type = Inputs;

	/** Constants type of the shader. */
	using constants_type = Constants;

	/** Outputs type of the shader. */
	using outputs_type = Outputs;

	/** Buffer type list of the shader. */
	using buffer_types = meta::TypeList<Buffers...>;

	/**
	 * Compile a fragment shader from GLSL source code into SPIR-V code for
	 * Vulkan for this specic shader type.
	 *
	 * \param sourceCode string of GLSL source code for the fragment shader.
	 * \param compilationOptions compilation options, see
	 *        ShaderCompilationOptions.
	 *
	 * \return the compiled SPIR-V code for the shader module.
	 *
	 * \throws graphics::Error on failure to compile the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The compiled SPIR-V module is only compatible with this specific
	 *          fragment shader type and its exact given template arguments.
	 * \warning This function only works under the Vulkan graphics backend, and
	 *          also requires GLSL compilation to be enabled.
	 */
	[[nodiscard]] static std::vector<uint32_t> compileGLSLToVulkanSPIRV(CStringView sourceCode, const ShaderCompilationOptions& compilationOptions = {}) {
		return detail::FragmentShaderBase::compileGLSLToVulkanSPIRVImplementation(sourceCode, nullptr, {}, compilationOptions, detail::CONSTANT_DESCRIPTIONS<Constants>,
			detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>, detail::FIELD_DESCRIPTIONS<Inputs>, detail::FIELD_DESCRIPTIONS<Outputs>,
			detail::BUFFER_LAYOUT_REFERENCES<Buffers...>);
	}

	/**
	 * Compile a fragment shader from GLSL source code into SPIR-V code for
	 * Vulkan for this specic shader type, with support for file inclusion.
	 *
	 * \param sourceCode string of GLSL source code for the fragment shader.
	 * \param filesystem filesystem to load included files from.
	 * \param filepath filepath of the given source code, whose parent directory
	 *        double-quoted include directives will be relative to.
	 * \param compilationOptions compilation options, see
	 *        ShaderCompilationOptions.
	 *
	 * \return the compiled SPIR-V code for the shader module.
	 *
	 * \throws File::Error on failure to open a file.
	 * \throws graphics::Error on failure to compile the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The compiled SPIR-V module is only compatible with this specific
	 *          fragment shader type and its exact given template arguments.
	 * \warning This function only works under the Vulkan graphics backend, and
	 *          also requires GLSL compilation to be enabled.
	 */
	[[nodiscard]] static std::vector<uint32_t> compileGLSLToVulkanSPIRV(CStringView sourceCode, const Filesystem& filesystem, CStringView filepath = {},
		const ShaderCompilationOptions& compilationOptions = {}) {
		return detail::FragmentShaderBase::compileGLSLToVulkanSPIRVImplementation(sourceCode, &filesystem, filepath, compilationOptions, detail::CONSTANT_DESCRIPTIONS<Constants>,
			detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>, detail::FIELD_DESCRIPTIONS<Inputs>, detail::FIELD_DESCRIPTIONS<Outputs>,
			detail::BUFFER_LAYOUT_REFERENCES<Buffers...>);
	}

	/**
	 * Create a fragment shader from GLSL source code.
	 *
	 * \param device device to create the shader stage for. Must outlive the
	 *        shader stage.
	 * \param sourceCode string of GLSL source code for the fragment shader.
	 *
	 * \return the fragment shader stage.
	 *
	 * \throws graphics::Error on failure to create the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function requires GLSL compilation to be supported by the
	 *          current graphics backend, which can be queried by calling
	 *          `device.getSupportedFeatures()` and reading the field
	 *          `FeatureSupport::supportsGLSLShaderCode`.
	 */
	[[nodiscard]] static FragmentShader create(Device& device, CStringView sourceCode) {
		return FragmentShader{device, sourceCode, nullptr, {}};
	}

	/**
	 * Create a fragment shader from GLSL source code, with support for file
	 * inclusion.
	 *
	 * \param device device to create the shader stage for. Must outlive the
	 *        shader stage.
	 * \param sourceCode string of GLSL source code for the fragment shader.
	 * \param filesystem filesystem to load included files from.
	 * \param filepath filepath of the given source code, whose parent directory
	 *        double-quoted include directives will be relative to.
	 *
	 * \return the fragment shader stage.
	 *
	 * \throws File::Error on failure to open a file.
	 * \throws graphics::Error on failure to create the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function requires GLSL compilation to be supported by the
	 *          current graphics backend, which can be queried by calling
	 *          `device.getSupportedFeatures()` and reading the field
	 *          `FeatureSupport::supportsGLSLShaderCode`.
	 */
	[[nodiscard]] static FragmentShader create(Device& device, CStringView sourceCode, const Filesystem& filesystem, CStringView filepath = {}) {
		return FragmentShader{device, sourceCode, &filesystem, filepath};
	}

	/**
	 * Create a fragment shader from SPIR-V code.
	 *
	 * \param device device to create the shader stage for. Must outlive the
	 *        shader stage.
	 * \param code SPIR-V code for the fragment shader. Must be compatible with
	 *        the shader's template parameters.
	 *
	 * \return the fragment shader stage.
	 *
	 * \throws graphics::Error on failure to create the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning This function requires SPIR-V code to be supported by the
	 *          current graphics backend, which can be queried by calling
	 *          `device.getSupportedFeatures()` and reading the field
	 *          `FeatureSupport::supportsSPIRVShaderCode`.
	 * \warning Supplying invalid SPIR-V, or SPIR-V that is incompatible with
	 *          the template parameters of this shader, yields undefined
	 *          behavior.
	 */
	[[nodiscard]] static FragmentShader create(Device& device, Span<const uint32_t> code) {
		return FragmentShader{device, code};
	}

	/**
	 * Load a fragment shader from a file.
	 *
	 * The supported file formats are:
	 * - GLSL (.glsl, .frag)
	 * - SPIR-V (.spv)
	 *
	 * \param device device to create the shader stage for. Must outlive the
	 *        shader stage.
	 * \param filesystem filesystem to load the file from.
	 * \param filepath input filepath of the shader file to load.
	 * \param options fragment shader options, see FragmentShaderOptions.
	 *
	 * \throws File::Error on failure to open a file.
	 * \throws graphics::Error on failure to create the shader.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note The file format is determined entirely from the file contents; the
	 *       filename extension is not taken into account.
	 *
	 * \warning This function requires the file format to be supported by the
	 *          current graphics backend, which can be queried by calling
	 *          `device.getSupportedFeatures()`.
	 * \warning If the file format is SPIR-V, supplying invalid SPIR-V, or
	 *          SPIR-V that is incompatible with the template parameters of this
	 *          shader, yields undefined behavior.
	 */
	explicit FragmentShader(Device& device, const Filesystem& filesystem, CStringView filepath, const FragmentShaderOptions& options = {})
		: detail::FragmentShaderBase(device, filesystem, filepath, options, detail::CONSTANT_DESCRIPTIONS<Constants>, typeid(mesh_type),
			  detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, detail::MESH_INDEX_TYPE<Index>, detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>,
			  (std::is_empty_v<Instance>) ? 0 : sizeof(Instance), detail::FIELD_DESCRIPTIONS<Inputs>, detail::FIELD_DESCRIPTIONS<Outputs>,
			  detail::BUFFER_LAYOUT_REFERENCES<Buffers...>) {}

private:
	explicit FragmentShader(Device& device, CStringView sourceCode, const Filesystem* filesystem, CStringView filepath)
		: detail::FragmentShaderBase(device, sourceCode, filesystem, filepath, detail::CONSTANT_DESCRIPTIONS<Constants>, typeid(mesh_type),
			  detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>, detail::MESH_INDEX_TYPE<Index>, detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>,
			  (std::is_empty_v<Instance>) ? 0 : sizeof(Instance), detail::FIELD_DESCRIPTIONS<Inputs>, detail::FIELD_DESCRIPTIONS<Outputs>,
			  detail::BUFFER_LAYOUT_REFERENCES<Buffers...>) {}

	explicit FragmentShader(Device& device, Span<const uint32_t> code)
		: detail::FragmentShaderBase(device, code, detail::CONSTANT_DESCRIPTIONS<Constants>, typeid(mesh_type), detail::VERTEX_ATTRIBUTE_DESCRIPTIONS<Vertex>,
			  detail::MESH_INDEX_TYPE<Index>, detail::PARAMETER_DESCRIPTIONS<Parameters>, detail::FIELD_DESCRIPTIONS<Instance>, (std::is_empty_v<Instance>) ? 0 : sizeof(Instance),
			  detail::FIELD_DESCRIPTIONS<Inputs>, detail::FIELD_DESCRIPTIONS<Outputs>, detail::BUFFER_LAYOUT_REFERENCES<Buffers...>) {}
};

namespace detail {

template <typename VertexShader, typename FragmentShader>
struct is_valid_shader_pipeline_combination : std::false_type {};

template <typename Mesh, typename VertexShaderConstants, typename VertexShaderOutputs, typename... VertexShaderBuffers, typename FragmentShaderConstants,
	typename FragmentShaderOutputs, typename... FragmentShaderBuffers>
struct is_valid_shader_pipeline_combination<VertexShader<Mesh, VertexShaderConstants, VertexShaderOutputs, VertexShaderBuffers...>,
	FragmentShader<Mesh, VertexShaderOutputs, FragmentShaderConstants, FragmentShaderOutputs, FragmentShaderBuffers...>>
	: meta::type_list_starts_with<meta::TypeList<FragmentShaderBuffers...>, meta::TypeList<VertexShaderBuffers...>> {};

template <typename Mesh, typename VertexShaderConstants, typename VertexShaderOutputs, typename... VertexShaderBuffers, typename FragmentShaderConstants,
	typename FragmentShaderOutputs>
struct is_valid_shader_pipeline_combination<VertexShader<Mesh, VertexShaderConstants, VertexShaderOutputs, VertexShaderBuffers...>,
	FragmentShader<Mesh, VertexShaderOutputs, FragmentShaderConstants, FragmentShaderOutputs>> : std::true_type {};

template <typename VertexShader, typename FragmentShader>
inline constexpr bool is_valid_shader_pipeline_combination_v = is_valid_shader_pipeline_combination<VertexShader, FragmentShader>::value;

}; // namespace detail

/**
 * Linked GPU shader pipeline.
 *
 * \tparam Mesh concrete mesh type that will be rendered by this shader.
 */
template <typename Mesh>
class ShaderPipeline : public detail::ShaderPipelineBase {
public:
	/** Mesh type of the shader pipeline. */
	using mesh_type = Mesh;

	/**
	 * Link a shader pipeline.
	 *
	 * \param device device to create the shader pipeline for. Must outlive the
	 *        shader pipeline.
	 * \param vertexShader vertex shader stage of the shader pipeline.
	 * \param vertexShaderConstants constant values to supply to the vertex
	 *        shader.
	 * \param fragmentShader fragment shader stage of the shader pipeline.
	 * \param fragmentShaderConstants constant values to supply to the fragment
	 *        shader.
	 * \param shaderPipelineOptions configuration options, see
	 *        ShaderPipelineOptions.
	 *
	 * \throws graphics::Error on failure to create the shader pipeline object
	 *         or link the shader pipeline.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	template <typename VertexShaderMesh, typename VertexShaderConstants, typename VertexShaderOutputs, typename... VertexShaderBuffers, typename FragmentShaderMesh,
		typename FragmentShaderInputs, typename FragmentShaderConstants, typename FragmentShaderOutputs, typename... FragmentShaderBuffers>
	ShaderPipeline(Device& device, const VertexShader<VertexShaderMesh, VertexShaderConstants, VertexShaderOutputs, VertexShaderBuffers...>& vertexShader,
		const VertexShaderConstants& vertexShaderConstants,
		const FragmentShader<FragmentShaderMesh, FragmentShaderInputs, FragmentShaderConstants, FragmentShaderOutputs, FragmentShaderBuffers...>& fragmentShader,
		const FragmentShaderConstants& fragmentShaderConstants, const ShaderPipelineOptions& shaderPipelineOptions)
		: ShaderPipeline(device, vertexShader.lock(), vertexShaderConstants, fragmentShader.lock(), fragmentShaderConstants, shaderPipelineOptions) {
		static_assert(same_as<VertexShaderMesh, Mesh>, "Vertex shader's' mesh type does not match the shader pipeline.");
		static_assert(same_as<FragmentShaderMesh, Mesh>, "Fragment shader's' mesh type does not match the shader pipeline.");
		static_assert(same_as<VertexShaderOutputs, FragmentShaderInputs>, "Fragment shader's input type does not match the vertex shader's output type.");
		static_assert(detail::is_valid_shader_pipeline_combination_v<std::remove_cvref_t<decltype(vertexShader)>, std::remove_cvref_t<decltype(fragmentShader)>>,
			"Incompatible vertex and fragment shaders.");
	}

	/**
	 * Link a shader pipeline.
	 *
	 * \param device device to create the shader pipeline for. Must outlive the
	 *        shader pipeline.
	 * \param vertexShaderHandle handle to the vertex shader stage of the shader
	 *        pipeline.
	 * \param vertexShaderConstants constant values to supply to the vertex
	 *        shader.
	 * \param fragmentShaderHandle handle to the fragment shader stage of the
	 *        shader pipeline.
	 * \param fragmentShaderConstants constant values to supply to the fragment
	 *        shader.
	 * \param shaderPipelineOptions configuration options, see
	 *        ShaderPipelineOptions.
	 *
	 * \throws graphics::Error on failure to create the shader pipeline object
	 *         or link the shader pipeline.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \warning The vertex shader must use the constants specified in the
	 *          VertexShaderConstants template parameter.
	 * \warning The fragment shader must use the constants specified in the
	 *          FragmentShaderConstants template parameter.
	 * \warning The vertex and fragment shaders must use the mesh type specified
	 *          in the Mesh template parameter of the shader pipeline.
	 * \warning The fragment shader's input type must match the vertex shader's
	 *          output type.
	 * \warning Either all buffers used by the vertex shader must also be used
	 *          by the fragment shader, and be listed before any additional
	 *          buffers used by only the fragment shader, or the fragment shader
	 *          must use no buffers at all.
	 */
	template <typename VertexShaderConstants, typename FragmentShaderConstants>
	ShaderPipeline(Device& device, SharedPointer<VertexShaderImplementation> vertexShaderHandle, const VertexShaderConstants& vertexShaderConstants,
		SharedPointer<FragmentShaderImplementation> fragmentShaderHandle, const FragmentShaderConstants& fragmentShaderConstants,
		const ShaderPipelineOptions& shaderPipelineOptions)
		: detail::ShaderPipelineBase(device, typeid(Mesh), std::move(vertexShaderHandle), detail::CONSTANT_DESCRIPTIONS<VertexShaderConstants>,
			  asBytes(Span{&vertexShaderConstants, 1}), std::move(fragmentShaderHandle), detail::CONSTANT_DESCRIPTIONS<FragmentShaderConstants>,
			  asBytes(Span{&fragmentShaderConstants, 1}), shaderPipelineOptions) {}
};

} // namespace grem::graphics

#endif
