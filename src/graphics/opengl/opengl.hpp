// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_GRAPHICS_OPENGL_OPENGL_HPP
#define GREM_GRAPHICS_OPENGL_OPENGL_HPP

#include <GREM/build_config.hpp>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h> // IWYU pragma: export // GL..., gl...
#else

#include <GREM/core/fundamentals.hpp>

#ifdef APIENTRY
#define GLAPIENTRY APIENTRY
#elif defined(_WIN32)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif

#define GLAPI extern

using GLenum = unsigned;
using GLboolean = unsigned char;
using GLbitfield = unsigned;
using GLvoid = void;
using GLbyte = grem::int8_t;
using GLubyte = grem::uint8_t;
using GLshort = grem::int16_t;
using GLushort = grem::uint16_t;
using GLint = int;
using GLuint = unsigned;
using GLclampx = grem::int32_t;
using GLsizei = int;
using GLfloat = grem::float32_t;
using GLclampf = grem::float32_t;
using GLdouble = grem::float64_t;
using GLclampd = grem::float64_t;
using GLchar = char;
using GLhalf = grem::uint16_t;
using GLfixed = grem::int32_t;
using GLintptr = grem::intptr_t;
using GLsizeiptr = grem::ssize_t;
using GLint64 = grem::int64_t;
using GLuint64 = grem::uint64_t;
using GLsync = struct __GLsync*;

#ifdef GREM_PRIVATE_GRAPHICS_OPENGL_USE_ES_PROFILE

#define GL_ACTIVE_ATTRIBUTES                             0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH                   0x8B8A
#define GL_ACTIVE_TEXTURE                                0x84E0
#define GL_ACTIVE_UNIFORMS                               0x8B86
#define GL_ACTIVE_UNIFORM_BLOCKS                         0x8A36
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH          0x8A35
#define GL_ACTIVE_UNIFORM_MAX_LENGTH                     0x8B87
#define GL_ALIASED_LINE_WIDTH_RANGE                      0x846E
#define GL_ALIASED_POINT_SIZE_RANGE                      0x846D
#define GL_ALPHA                                         0x1906
#define GL_ALPHA_BITS                                    0x0D55
#define GL_ALREADY_SIGNALED                              0x911A
#define GL_ALWAYS                                        0x0207
#define GL_ANY_SAMPLES_PASSED                            0x8C2F
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE               0x8D6A
#define GL_ARRAY_BUFFER                                  0x8892
#define GL_ARRAY_BUFFER_BINDING                          0x8894
#define GL_ATTACHED_SHADERS                              0x8B85
#define GL_BACK                                          0x0405
#define GL_BLEND                                         0x0BE2
#define GL_BLEND_COLOR                                   0x8005
#define GL_BLEND_DST_ALPHA                               0x80CA
#define GL_BLEND_DST_RGB                                 0x80C8
#define GL_BLEND_EQUATION                                0x8009
#define GL_BLEND_EQUATION_ALPHA                          0x883D
#define GL_BLEND_EQUATION_RGB                            0x8009
#define GL_BLEND_SRC_ALPHA                               0x80CB
#define GL_BLEND_SRC_RGB                                 0x80C9
#define GL_BLUE                                          0x1905
#define GL_BLUE_BITS                                     0x0D54
#define GL_BOOL                                          0x8B56
#define GL_BOOL_VEC2                                     0x8B57
#define GL_BOOL_VEC3                                     0x8B58
#define GL_BOOL_VEC4                                     0x8B59
#define GL_BUFFER_ACCESS_FLAGS                           0x911F
#define GL_BUFFER_MAPPED                                 0x88BC
#define GL_BUFFER_MAP_LENGTH                             0x9120
#define GL_BUFFER_MAP_OFFSET                             0x9121
#define GL_BUFFER_MAP_POINTER                            0x88BD
#define GL_BUFFER_SIZE                                   0x8764
#define GL_BUFFER_USAGE                                  0x8765
#define GL_BYTE                                          0x1400
#define GL_CCW                                           0x0901
#define GL_CLAMP_TO_EDGE                                 0x812F
#define GL_COLOR                                         0x1800
#define GL_COLOR_ATTACHMENT0                             0x8CE0
#define GL_COLOR_ATTACHMENT1                             0x8CE1
#define GL_COLOR_ATTACHMENT10                            0x8CEA
#define GL_COLOR_ATTACHMENT11                            0x8CEB
#define GL_COLOR_ATTACHMENT12                            0x8CEC
#define GL_COLOR_ATTACHMENT13                            0x8CED
#define GL_COLOR_ATTACHMENT14                            0x8CEE
#define GL_COLOR_ATTACHMENT15                            0x8CEF
#define GL_COLOR_ATTACHMENT16                            0x8CF0
#define GL_COLOR_ATTACHMENT17                            0x8CF1
#define GL_COLOR_ATTACHMENT18                            0x8CF2
#define GL_COLOR_ATTACHMENT19                            0x8CF3
#define GL_COLOR_ATTACHMENT2                             0x8CE2
#define GL_COLOR_ATTACHMENT20                            0x8CF4
#define GL_COLOR_ATTACHMENT21                            0x8CF5
#define GL_COLOR_ATTACHMENT22                            0x8CF6
#define GL_COLOR_ATTACHMENT23                            0x8CF7
#define GL_COLOR_ATTACHMENT24                            0x8CF8
#define GL_COLOR_ATTACHMENT25                            0x8CF9
#define GL_COLOR_ATTACHMENT26                            0x8CFA
#define GL_COLOR_ATTACHMENT27                            0x8CFB
#define GL_COLOR_ATTACHMENT28                            0x8CFC
#define GL_COLOR_ATTACHMENT29                            0x8CFD
#define GL_COLOR_ATTACHMENT3                             0x8CE3
#define GL_COLOR_ATTACHMENT30                            0x8CFE
#define GL_COLOR_ATTACHMENT31                            0x8CFF
#define GL_COLOR_ATTACHMENT4                             0x8CE4
#define GL_COLOR_ATTACHMENT5                             0x8CE5
#define GL_COLOR_ATTACHMENT6                             0x8CE6
#define GL_COLOR_ATTACHMENT7                             0x8CE7
#define GL_COLOR_ATTACHMENT8                             0x8CE8
#define GL_COLOR_ATTACHMENT9                             0x8CE9
#define GL_COLOR_BUFFER_BIT                              0x00004000
#define GL_COLOR_CLEAR_VALUE                             0x0C22
#define GL_COLOR_WRITEMASK                               0x0C23
#define GL_COMPARE_REF_TO_TEXTURE                        0x884E
#define GL_COMPARE_R_TO_TEXTURE                          0x884E
#define GL_COMPILE_STATUS                                0x8B81
#define GL_COMPRESSED_R11_EAC                            0x9270
#define GL_COMPRESSED_RG11_EAC                           0x9272
#define GL_COMPRESSED_RGB8_ETC2                          0x9274
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2      0x9276
#define GL_COMPRESSED_RGBA8_ETC2_EAC                     0x9278
#define GL_COMPRESSED_SIGNED_R11_EAC                     0x9271
#define GL_COMPRESSED_SIGNED_RG11_EAC                    0x9273
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC              0x9279
#define GL_COMPRESSED_SRGB8_ETC2                         0x9275
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2     0x9277
#define GL_COMPRESSED_TEXTURE_FORMATS                    0x86A3
#define GL_CONDITION_SATISFIED                           0x911C
#define GL_CONSTANT_ALPHA                                0x8003
#define GL_CONSTANT_COLOR                                0x8001
#define GL_COPY_READ_BUFFER                              0x8F36
#define GL_COPY_READ_BUFFER_BINDING                      0x8F36
#define GL_COPY_WRITE_BUFFER                             0x8F37
#define GL_COPY_WRITE_BUFFER_BINDING                     0x8F37
#define GL_CULL_FACE                                     0x0B44
#define GL_CULL_FACE_MODE                                0x0B45
#define GL_CURRENT_PROGRAM                               0x8B8D
#define GL_CURRENT_QUERY                                 0x8865
#define GL_CURRENT_VERTEX_ATTRIB                         0x8626
#define GL_CW                                            0x0900
#define GL_DECR                                          0x1E03
#define GL_DECR_WRAP                                     0x8508
#define GL_DELETE_STATUS                                 0x8B80
#define GL_DEPTH                                         0x1801
#define GL_DEPTH24_STENCIL8                              0x88F0
#define GL_DEPTH32F_STENCIL8                             0x8CAD
#define GL_DEPTH_ATTACHMENT                              0x8D00
#define GL_DEPTH_BITS                                    0x0D56
#define GL_DEPTH_BUFFER_BIT                              0x00000100
#define GL_DEPTH_CLEAR_VALUE                             0x0B73
#define GL_DEPTH_COMPONENT                               0x1902
#define GL_DEPTH_COMPONENT16                             0x81A5
#define GL_DEPTH_COMPONENT24                             0x81A6
#define GL_DEPTH_COMPONENT32F                            0x8CAC
#define GL_DEPTH_FUNC                                    0x0B74
#define GL_DEPTH_RANGE                                   0x0B70
#define GL_DEPTH_STENCIL                                 0x84F9
#define GL_DEPTH_STENCIL_ATTACHMENT                      0x821A
#define GL_DEPTH_TEST                                    0x0B71
#define GL_DEPTH_WRITEMASK                               0x0B72
#define GL_DITHER                                        0x0BD0
#define GL_DONT_CARE                                     0x1100
#define GL_DRAW_BUFFER0                                  0x8825
#define GL_DRAW_BUFFER1                                  0x8826
#define GL_DRAW_BUFFER10                                 0x882F
#define GL_DRAW_BUFFER11                                 0x8830
#define GL_DRAW_BUFFER12                                 0x8831
#define GL_DRAW_BUFFER13                                 0x8832
#define GL_DRAW_BUFFER14                                 0x8833
#define GL_DRAW_BUFFER15                                 0x8834
#define GL_DRAW_BUFFER2                                  0x8827
#define GL_DRAW_BUFFER3                                  0x8828
#define GL_DRAW_BUFFER4                                  0x8829
#define GL_DRAW_BUFFER5                                  0x882A
#define GL_DRAW_BUFFER6                                  0x882B
#define GL_DRAW_BUFFER7                                  0x882C
#define GL_DRAW_BUFFER8                                  0x882D
#define GL_DRAW_BUFFER9                                  0x882E
#define GL_DRAW_FRAMEBUFFER                              0x8CA9
#define GL_DRAW_FRAMEBUFFER_BINDING                      0x8CA6
#define GL_DST_ALPHA                                     0x0304
#define GL_DST_COLOR                                     0x0306
#define GL_DYNAMIC_COPY                                  0x88EA
#define GL_DYNAMIC_DRAW                                  0x88E8
#define GL_DYNAMIC_READ                                  0x88E9
#define GL_ELEMENT_ARRAY_BUFFER                          0x8893
#define GL_ELEMENT_ARRAY_BUFFER_BINDING                  0x8895
#define GL_EQUAL                                         0x0202
#define GL_EXTENSIONS                                    0x1F03
#define GL_FALSE                                         0
#define GL_FASTEST                                       0x1101
#define GL_FIXED                                         0x140C
#define GL_FLOAT                                         0x1406
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV                0x8DAD
#define GL_FLOAT_MAT2                                    0x8B5A
#define GL_FLOAT_MAT2x3                                  0x8B65
#define GL_FLOAT_MAT2x4                                  0x8B66
#define GL_FLOAT_MAT3                                    0x8B5B
#define GL_FLOAT_MAT3x2                                  0x8B67
#define GL_FLOAT_MAT3x4                                  0x8B68
#define GL_FLOAT_MAT4                                    0x8B5C
#define GL_FLOAT_MAT4x2                                  0x8B69
#define GL_FLOAT_MAT4x3                                  0x8B6A
#define GL_FLOAT_VEC2                                    0x8B50
#define GL_FLOAT_VEC3                                    0x8B51
#define GL_FLOAT_VEC4                                    0x8B52
#define GL_FRAGMENT_SHADER                               0x8B30
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT               0x8B8B
#define GL_FRAMEBUFFER                                   0x8D40
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE             0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE              0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING         0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE         0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE             0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE             0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME            0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE            0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE               0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE           0x8217
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE  0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER          0x8CD4
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL          0x8CD2
#define GL_FRAMEBUFFER_BINDING                           0x8CA6
#define GL_FRAMEBUFFER_COMPLETE                          0x8CD5
#define GL_FRAMEBUFFER_DEFAULT                           0x8218
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT             0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS             0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT     0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE            0x8D56
#define GL_FRAMEBUFFER_UNDEFINED                         0x8219
#define GL_FRAMEBUFFER_UNSUPPORTED                       0x8CDD
#define GL_FRONT                                         0x0404
#define GL_FRONT_AND_BACK                                0x0408
#define GL_FRONT_FACE                                    0x0B46
#define GL_FUNC_ADD                                      0x8006
#define GL_FUNC_REVERSE_SUBTRACT                         0x800B
#define GL_FUNC_SUBTRACT                                 0x800A
#define GL_GENERATE_MIPMAP_HINT                          0x8192
#define GL_GEQUAL                                        0x0206
#define GL_GREATER                                       0x0204
#define GL_GREEN                                         0x1904
#define GL_GREEN_BITS                                    0x0D53
#define GL_HALF_FLOAT                                    0x140B
#define GL_HIGH_FLOAT                                    0x8DF2
#define GL_HIGH_INT                                      0x8DF5
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT              0x8B9B
#define GL_IMPLEMENTATION_COLOR_READ_TYPE                0x8B9A
#define GL_INCR                                          0x1E02
#define GL_INCR_WRAP                                     0x8507
#define GL_INFO_LOG_LENGTH                               0x8B84
#define GL_INT                                           0x1404
#define GL_INTERLEAVED_ATTRIBS                           0x8C8C
#define GL_INT_2_10_10_10_REV                            0x8D9F
#define GL_INT_SAMPLER_2D                                0x8DCA
#define GL_INT_SAMPLER_2D_ARRAY                          0x8DCF
#define GL_INT_SAMPLER_3D                                0x8DCB
#define GL_INT_SAMPLER_CUBE                              0x8DCC
#define GL_INT_VEC2                                      0x8B53
#define GL_INT_VEC3                                      0x8B54
#define GL_INT_VEC4                                      0x8B55
#define GL_INVALID_ENUM                                  0x0500
#define GL_INVALID_FRAMEBUFFER_OPERATION                 0x0506
#define GL_INVALID_INDEX                                 0xFFFFFFFF
#define GL_INVALID_OPERATION                             0x0502
#define GL_INVALID_VALUE                                 0x0501
#define GL_INVERT                                        0x150A
#define GL_KEEP                                          0x1E00
#define GL_LEQUAL                                        0x0203
#define GL_LESS                                          0x0201
#define GL_LINEAR                                        0x2601
#define GL_LINEAR_MIPMAP_LINEAR                          0x2703
#define GL_LINEAR_MIPMAP_NEAREST                         0x2701
#define GL_LINES                                         0x0001
#define GL_LINE_LOOP                                     0x0002
#define GL_LINE_STRIP                                    0x0003
#define GL_LINE_WIDTH                                    0x0B21
#define GL_LINK_STATUS                                   0x8B82
#define GL_LOW_FLOAT                                     0x8DF0
#define GL_LOW_INT                                       0x8DF3
#define GL_LUMINANCE                                     0x1909
#define GL_LUMINANCE_ALPHA                               0x190A
#define GL_MAJOR_VERSION                                 0x821B
#define GL_MAP_FLUSH_EXPLICIT_BIT                        0x0010
#define GL_MAP_INVALIDATE_BUFFER_BIT                     0x0008
#define GL_MAP_INVALIDATE_RANGE_BIT                      0x0004
#define GL_MAP_READ_BIT                                  0x0001
#define GL_MAP_UNSYNCHRONIZED_BIT                        0x0020
#define GL_MAP_WRITE_BIT                                 0x0002
#define GL_MAX                                           0x8008
#define GL_MAX_3D_TEXTURE_SIZE                           0x8073
#define GL_MAX_ARRAY_TEXTURE_LAYERS                      0x88FF
#define GL_MAX_COLOR_ATTACHMENTS                         0x8CDF
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS      0x8A33
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS              0x8B4D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS                   0x8A2E
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS        0x8A31
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE                     0x851C
#define GL_MAX_DRAW_BUFFERS                              0x8824
#define GL_MAX_ELEMENTS_INDICES                          0x80E9
#define GL_MAX_ELEMENTS_VERTICES                         0x80E8
#define GL_MAX_ELEMENT_INDEX                             0x8D6B
#define GL_MAX_FRAGMENT_INPUT_COMPONENTS                 0x9125
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS                   0x8A2D
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS               0x8B49
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS                  0x8DFD
#define GL_MAX_PROGRAM_TEXEL_OFFSET                      0x8905
#define GL_MAX_RENDERBUFFER_SIZE                         0x84E8
#define GL_MAX_SAMPLES                                   0x8D57
#define GL_MAX_SERVER_WAIT_TIMEOUT                       0x9111
#define GL_MAX_TEXTURE_IMAGE_UNITS                       0x8872
#define GL_MAX_TEXTURE_LOD_BIAS                          0x84FD
#define GL_MAX_TEXTURE_SIZE                              0x0D33
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS 0x8C8A
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS       0x8C8B
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS    0x8C80
#define GL_MAX_UNIFORM_BLOCK_SIZE                        0x8A30
#define GL_MAX_UNIFORM_BUFFER_BINDINGS                   0x8A2F
#define GL_MAX_VARYING_COMPONENTS                        0x8B4B
#define GL_MAX_VARYING_FLOATS                            0x8B4B
#define GL_MAX_VARYING_VECTORS                           0x8DFC
#define GL_MAX_VERTEX_ATTRIBS                            0x8869
#define GL_MAX_VERTEX_OUTPUT_COMPONENTS                  0x9122
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS                0x8B4C
#define GL_MAX_VERTEX_UNIFORM_BLOCKS                     0x8A2B
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS                 0x8B4A
#define GL_MAX_VERTEX_UNIFORM_VECTORS                    0x8DFB
#define GL_MAX_VIEWPORT_DIMS                             0x0D3A
#define GL_MEDIUM_FLOAT                                  0x8DF1
#define GL_MEDIUM_INT                                    0x8DF4
#define GL_MIN                                           0x8007
#define GL_MINOR_VERSION                                 0x821C
#define GL_MIN_PROGRAM_TEXEL_OFFSET                      0x8904
#define GL_MIRRORED_REPEAT                               0x8370
#define GL_NEAREST                                       0x2600
#define GL_NEAREST_MIPMAP_LINEAR                         0x2702
#define GL_NEAREST_MIPMAP_NEAREST                        0x2700
#define GL_NEVER                                         0x0200
#define GL_NICEST                                        0x1102
#define GL_NONE                                          0
#define GL_NOTEQUAL                                      0x0205
#define GL_NO_ERROR                                      0
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS                0x86A2
#define GL_NUM_EXTENSIONS                                0x821D
#define GL_NUM_PROGRAM_BINARY_FORMATS                    0x87FE
#define GL_NUM_SAMPLE_COUNTS                             0x9380
#define GL_NUM_SHADER_BINARY_FORMATS                     0x8DF9
#define GL_OBJECT_TYPE                                   0x9112
#define GL_ONE                                           1
#define GL_ONE_MINUS_CONSTANT_ALPHA                      0x8004
#define GL_ONE_MINUS_CONSTANT_COLOR                      0x8002
#define GL_ONE_MINUS_DST_ALPHA                           0x0305
#define GL_ONE_MINUS_DST_COLOR                           0x0307
#define GL_ONE_MINUS_SRC_ALPHA                           0x0303
#define GL_ONE_MINUS_SRC_COLOR                           0x0301
#define GL_OUT_OF_MEMORY                                 0x0505
#define GL_PACK_ALIGNMENT                                0x0D05
#define GL_PACK_ROW_LENGTH                               0x0D02
#define GL_PACK_SKIP_PIXELS                              0x0D04
#define GL_PACK_SKIP_ROWS                                0x0D03
#define GL_PIXEL_PACK_BUFFER                             0x88EB
#define GL_PIXEL_PACK_BUFFER_BINDING                     0x88ED
#define GL_PIXEL_UNPACK_BUFFER                           0x88EC
#define GL_PIXEL_UNPACK_BUFFER_BINDING                   0x88EF
#define GL_POINTS                                        0x0000
#define GL_POLYGON_OFFSET_FACTOR                         0x8038
#define GL_POLYGON_OFFSET_FILL                           0x8037
#define GL_POLYGON_OFFSET_UNITS                          0x2A00
#define GL_PRIMITIVE_RESTART_FIXED_INDEX                 0x8D69
#define GL_PROGRAM_BINARY_FORMATS                        0x87FF
#define GL_PROGRAM_BINARY_LENGTH                         0x8741
#define GL_PROGRAM_BINARY_RETRIEVABLE_HINT               0x8257
#define GL_QUERY_RESULT                                  0x8866
#define GL_QUERY_RESULT_AVAILABLE                        0x8867
#define GL_R11F_G11F_B10F                                0x8C3A
#define GL_R16F                                          0x822D
#define GL_R16I                                          0x8233
#define GL_R16UI                                         0x8234
#define GL_R32F                                          0x822E
#define GL_R32I                                          0x8235
#define GL_R32UI                                         0x8236
#define GL_R8                                            0x8229
#define GL_R8I                                           0x8231
#define GL_R8UI                                          0x8232
#define GL_R8_SNORM                                      0x8F94
#define GL_RASTERIZER_DISCARD                            0x8C89
#define GL_READ_BUFFER                                   0x0C02
#define GL_READ_FRAMEBUFFER                              0x8CA8
#define GL_READ_FRAMEBUFFER_BINDING                      0x8CAA
#define GL_RED                                           0x1903
#define GL_RED_BITS                                      0x0D52
#define GL_RED_INTEGER                                   0x8D94
#define GL_RENDERBUFFER                                  0x8D41
#define GL_RENDERBUFFER_ALPHA_SIZE                       0x8D53
#define GL_RENDERBUFFER_BINDING                          0x8CA7
#define GL_RENDERBUFFER_BLUE_SIZE                        0x8D52
#define GL_RENDERBUFFER_DEPTH_SIZE                       0x8D54
#define GL_RENDERBUFFER_GREEN_SIZE                       0x8D51
#define GL_RENDERBUFFER_HEIGHT                           0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT                  0x8D44
#define GL_RENDERBUFFER_RED_SIZE                         0x8D50
#define GL_RENDERBUFFER_SAMPLES                          0x8CAB
#define GL_RENDERBUFFER_STENCIL_SIZE                     0x8D55
#define GL_RENDERBUFFER_WIDTH                            0x8D42
#define GL_RENDERER                                      0x1F01
#define GL_REPEAT                                        0x2901
#define GL_REPLACE                                       0x1E01
#define GL_RG                                            0x8227
#define GL_RG16F                                         0x822F
#define GL_RG16I                                         0x8239
#define GL_RG16UI                                        0x823A
#define GL_RG32F                                         0x8230
#define GL_RG32I                                         0x823B
#define GL_RG32UI                                        0x823C
#define GL_RG8                                           0x822B
#define GL_RG8I                                          0x8237
#define GL_RG8UI                                         0x8238
#define GL_RG8_SNORM                                     0x8F95
#define GL_RGB                                           0x1907
#define GL_RGB10_A2                                      0x8059
#define GL_RGB10_A2UI                                    0x906F
#define GL_RGB16F                                        0x881B
#define GL_RGB16I                                        0x8D89
#define GL_RGB16UI                                       0x8D77
#define GL_RGB32F                                        0x8815
#define GL_RGB32I                                        0x8D83
#define GL_RGB32UI                                       0x8D71
#define GL_RGB565                                        0x8D62
#define GL_RGB5_A1                                       0x8057
#define GL_RGB8                                          0x8051
#define GL_RGB8I                                         0x8D8F
#define GL_RGB8UI                                        0x8D7D
#define GL_RGB8_SNORM                                    0x8F96
#define GL_RGB9_E5                                       0x8C3D
#define GL_RGBA                                          0x1908
#define GL_RGBA16F                                       0x881A
#define GL_RGBA16I                                       0x8D88
#define GL_RGBA16UI                                      0x8D76
#define GL_RGBA32F                                       0x8814
#define GL_RGBA32I                                       0x8D82
#define GL_RGBA32UI                                      0x8D70
#define GL_RGBA4                                         0x8056
#define GL_RGBA8                                         0x8058
#define GL_RGBA8I                                        0x8D8E
#define GL_RGBA8UI                                       0x8D7C
#define GL_RGBA8_SNORM                                   0x8F97
#define GL_RGBA_INTEGER                                  0x8D99
#define GL_RGB_INTEGER                                   0x8D98
#define GL_RG_INTEGER                                    0x8228
#define GL_SAMPLER_2D                                    0x8B5E
#define GL_SAMPLER_2D_ARRAY                              0x8DC1
#define GL_SAMPLER_2D_ARRAY_SHADOW                       0x8DC4
#define GL_SAMPLER_2D_SHADOW                             0x8B62
#define GL_SAMPLER_3D                                    0x8B5F
#define GL_SAMPLER_BINDING                               0x8919
#define GL_SAMPLER_CUBE                                  0x8B60
#define GL_SAMPLER_CUBE_SHADOW                           0x8DC5
#define GL_SAMPLES                                       0x80A9
#define GL_SAMPLE_ALPHA_TO_COVERAGE                      0x809E
#define GL_SAMPLE_BUFFERS                                0x80A8
#define GL_SAMPLE_COVERAGE                               0x80A0
#define GL_SAMPLE_COVERAGE_INVERT                        0x80AB
#define GL_SAMPLE_COVERAGE_VALUE                         0x80AA
#define GL_SCISSOR_BOX                                   0x0C10
#define GL_SCISSOR_TEST                                  0x0C11
#define GL_SEPARATE_ATTRIBS                              0x8C8D
#define GL_SHADER_BINARY_FORMATS                         0x8DF8
#define GL_SHADER_COMPILER                               0x8DFA
#define GL_SHADER_SOURCE_LENGTH                          0x8B88
#define GL_SHADER_TYPE                                   0x8B4F
#define GL_SHADING_LANGUAGE_VERSION                      0x8B8C
#define GL_SHORT                                         0x1402
#define GL_SIGNALED                                      0x9119
#define GL_SIGNED_NORMALIZED                             0x8F9C
#define GL_SRC_ALPHA                                     0x0302
#define GL_SRC_ALPHA_SATURATE                            0x0308
#define GL_SRC_COLOR                                     0x0300
#define GL_SRGB                                          0x8C40
#define GL_SRGB8                                         0x8C41
#define GL_SRGB8_ALPHA8                                  0x8C43
#define GL_STATIC_COPY                                   0x88E6
#define GL_STATIC_DRAW                                   0x88E4
#define GL_STATIC_READ                                   0x88E5
#define GL_STENCIL                                       0x1802
#define GL_STENCIL_ATTACHMENT                            0x8D20
#define GL_STENCIL_BACK_FAIL                             0x8801
#define GL_STENCIL_BACK_FUNC                             0x8800
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL                  0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS                  0x8803
#define GL_STENCIL_BACK_REF                              0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK                       0x8CA4
#define GL_STENCIL_BACK_WRITEMASK                        0x8CA5
#define GL_STENCIL_BITS                                  0x0D57
#define GL_STENCIL_BUFFER_BIT                            0x00000400
#define GL_STENCIL_CLEAR_VALUE                           0x0B91
#define GL_STENCIL_FAIL                                  0x0B94
#define GL_STENCIL_FUNC                                  0x0B92
#define GL_STENCIL_INDEX8                                0x8D48
#define GL_STENCIL_PASS_DEPTH_FAIL                       0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS                       0x0B96
#define GL_STENCIL_REF                                   0x0B97
#define GL_STENCIL_TEST                                  0x0B90
#define GL_STENCIL_VALUE_MASK                            0x0B93
#define GL_STENCIL_WRITEMASK                             0x0B98
#define GL_STREAM_COPY                                   0x88E2
#define GL_STREAM_DRAW                                   0x88E0
#define GL_STREAM_READ                                   0x88E1
#define GL_SUBPIXEL_BITS                                 0x0D50
#define GL_SYNC_CONDITION                                0x9113
#define GL_SYNC_FENCE                                    0x9116
#define GL_SYNC_FLAGS                                    0x9115
#define GL_SYNC_FLUSH_COMMANDS_BIT                       0x00000001
#define GL_SYNC_GPU_COMMANDS_COMPLETE                    0x9117
#define GL_SYNC_STATUS                                   0x9114
#define GL_TEXTURE                                       0x1702
#define GL_TEXTURE0                                      0x84C0
#define GL_TEXTURE1                                      0x84C1
#define GL_TEXTURE10                                     0x84CA
#define GL_TEXTURE11                                     0x84CB
#define GL_TEXTURE12                                     0x84CC
#define GL_TEXTURE13                                     0x84CD
#define GL_TEXTURE14                                     0x84CE
#define GL_TEXTURE15                                     0x84CF
#define GL_TEXTURE16                                     0x84D0
#define GL_TEXTURE17                                     0x84D1
#define GL_TEXTURE18                                     0x84D2
#define GL_TEXTURE19                                     0x84D3
#define GL_TEXTURE2                                      0x84C2
#define GL_TEXTURE20                                     0x84D4
#define GL_TEXTURE21                                     0x84D5
#define GL_TEXTURE22                                     0x84D6
#define GL_TEXTURE23                                     0x84D7
#define GL_TEXTURE24                                     0x84D8
#define GL_TEXTURE25                                     0x84D9
#define GL_TEXTURE26                                     0x84DA
#define GL_TEXTURE27                                     0x84DB
#define GL_TEXTURE28                                     0x84DC
#define GL_TEXTURE29                                     0x84DD
#define GL_TEXTURE3                                      0x84C3
#define GL_TEXTURE30                                     0x84DE
#define GL_TEXTURE31                                     0x84DF
#define GL_TEXTURE4                                      0x84C4
#define GL_TEXTURE5                                      0x84C5
#define GL_TEXTURE6                                      0x84C6
#define GL_TEXTURE7                                      0x84C7
#define GL_TEXTURE8                                      0x84C8
#define GL_TEXTURE9                                      0x84C9
#define GL_TEXTURE_2D                                    0x0DE1
#define GL_TEXTURE_2D_ARRAY                              0x8C1A
#define GL_TEXTURE_3D                                    0x806F
#define GL_TEXTURE_BASE_LEVEL                            0x813C
#define GL_TEXTURE_BINDING_2D                            0x8069
#define GL_TEXTURE_BINDING_2D_ARRAY                      0x8C1D
#define GL_TEXTURE_BINDING_3D                            0x806A
#define GL_TEXTURE_BINDING_CUBE_MAP                      0x8514
#define GL_TEXTURE_COMPARE_FUNC                          0x884D
#define GL_TEXTURE_COMPARE_MODE                          0x884C
#define GL_TEXTURE_CUBE_MAP                              0x8513
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X                   0x8516
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y                   0x8518
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z                   0x851A
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X                   0x8515
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y                   0x8517
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z                   0x8519
#define GL_TEXTURE_IMMUTABLE_FORMAT                      0x912F
#define GL_TEXTURE_IMMUTABLE_LEVELS                      0x82DF
#define GL_TEXTURE_MAG_FILTER                            0x2800
#define GL_TEXTURE_MAX_LEVEL                             0x813D
#define GL_TEXTURE_MAX_LOD                               0x813B
#define GL_TEXTURE_MIN_FILTER                            0x2801
#define GL_TEXTURE_MIN_LOD                               0x813A
#define GL_TEXTURE_SWIZZLE_A                             0x8E45
#define GL_TEXTURE_SWIZZLE_B                             0x8E44
#define GL_TEXTURE_SWIZZLE_G                             0x8E43
#define GL_TEXTURE_SWIZZLE_R                             0x8E42
#define GL_TEXTURE_WRAP_R                                0x8072
#define GL_TEXTURE_WRAP_S                                0x2802
#define GL_TEXTURE_WRAP_T                                0x2803
#define GL_TIMEOUT_EXPIRED                               0x911B
#define GL_TIMEOUT_IGNORED                               0xFFFFFFFFFFFFFFFF
#define GL_TRANSFORM_FEEDBACK                            0x8E22
#define GL_TRANSFORM_FEEDBACK_ACTIVE                     0x8E24
#define GL_TRANSFORM_FEEDBACK_BINDING                    0x8E25
#define GL_TRANSFORM_FEEDBACK_BUFFER                     0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE              0x8E24
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING             0x8C8F
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE                0x8C7F
#define GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED              0x8E23
#define GL_TRANSFORM_FEEDBACK_BUFFER_SIZE                0x8C85
#define GL_TRANSFORM_FEEDBACK_BUFFER_START               0x8C84
#define GL_TRANSFORM_FEEDBACK_PAUSED                     0x8E23
#define GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN         0x8C88
#define GL_TRANSFORM_FEEDBACK_VARYINGS                   0x8C83
#define GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH         0x8C76
#define GL_TRIANGLES                                     0x0004
#define GL_TRIANGLE_FAN                                  0x0006
#define GL_TRIANGLE_STRIP                                0x0005
#define GL_TRUE                                          1
#define GL_UNIFORM_ARRAY_STRIDE                          0x8A3C
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS                 0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES          0x8A43
#define GL_UNIFORM_BLOCK_BINDING                         0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE                       0x8A40
#define GL_UNIFORM_BLOCK_INDEX                           0x8A3A
#define GL_UNIFORM_BLOCK_NAME_LENGTH                     0x8A41
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER   0x8A46
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER     0x8A44
#define GL_UNIFORM_BUFFER                                0x8A11
#define GL_UNIFORM_BUFFER_BINDING                        0x8A28
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT               0x8A34
#define GL_UNIFORM_BUFFER_SIZE                           0x8A2A
#define GL_UNIFORM_BUFFER_START                          0x8A29
#define GL_UNIFORM_IS_ROW_MAJOR                          0x8A3E
#define GL_UNIFORM_MATRIX_STRIDE                         0x8A3D
#define GL_UNIFORM_NAME_LENGTH                           0x8A39
#define GL_UNIFORM_OFFSET                                0x8A3B
#define GL_UNIFORM_SIZE                                  0x8A38
#define GL_UNIFORM_TYPE                                  0x8A37
#define GL_UNPACK_ALIGNMENT                              0x0CF5
#define GL_UNPACK_IMAGE_HEIGHT                           0x806E
#define GL_UNPACK_ROW_LENGTH                             0x0CF2
#define GL_UNPACK_SKIP_IMAGES                            0x806D
#define GL_UNPACK_SKIP_PIXELS                            0x0CF4
#define GL_UNPACK_SKIP_ROWS                              0x0CF3
#define GL_UNSIGNALED                                    0x9118
#define GL_UNSIGNED_BYTE                                 0x1401
#define GL_UNSIGNED_INT                                  0x1405
#define GL_UNSIGNED_INT_10F_11F_11F_REV                  0x8C3B
#define GL_UNSIGNED_INT_24_8                             0x84FA
#define GL_UNSIGNED_INT_2_10_10_10_REV                   0x8368
#define GL_UNSIGNED_INT_5_9_9_9_REV                      0x8C3E
#define GL_UNSIGNED_INT_SAMPLER_2D                       0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY                 0x8DD7
#define GL_UNSIGNED_INT_SAMPLER_3D                       0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_CUBE                     0x8DD4
#define GL_UNSIGNED_INT_VEC2                             0x8DC6
#define GL_UNSIGNED_INT_VEC3                             0x8DC7
#define GL_UNSIGNED_INT_VEC4                             0x8DC8
#define GL_UNSIGNED_NORMALIZED                           0x8C17
#define GL_UNSIGNED_SHORT                                0x1403
#define GL_UNSIGNED_SHORT_4_4_4_4                        0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1                        0x8034
#define GL_UNSIGNED_SHORT_5_6_5                          0x8363
#define GL_VALIDATE_STATUS                               0x8B83
#define GL_VENDOR                                        0x1F00
#define GL_VERSION                                       0x1F02
#define GL_VERTEX_ARRAY_BINDING                          0x85B5
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING            0x889F
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR                   0x88FE
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED                   0x8622
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER                   0x88FD
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED                0x886A
#define GL_VERTEX_ATTRIB_ARRAY_POINTER                   0x8645
#define GL_VERTEX_ATTRIB_ARRAY_SIZE                      0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE                    0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE                      0x8625
#define GL_VERTEX_SHADER                                 0x8B31
#define GL_VIEWPORT                                      0x0BA2
#define GL_WAIT_FAILED                                   0x911D
#define GL_ZERO                                          0

using PFNGLACTIVETEXTUREPROC = void(GLAPIENTRY*)(GLenum texture);
using PFNGLATTACHSHADERPROC = void(GLAPIENTRY*)(GLuint program, GLuint shader);
using PFNGLBEGINQUERYPROC = void(GLAPIENTRY*)(GLenum target, GLuint id);
using PFNGLBEGINTRANSFORMFEEDBACKPROC = void(GLAPIENTRY*)(GLenum primitiveMode);
using PFNGLBINDATTRIBLOCATIONPROC = void(GLAPIENTRY*)(GLuint program, GLuint index, const GLchar* name);
using PFNGLBINDBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLuint buffer);
using PFNGLBINDBUFFERBASEPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLuint buffer);
using PFNGLBINDBUFFERRANGEPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
using PFNGLBINDFRAMEBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLuint framebuffer);
using PFNGLBINDRENDERBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLuint renderbuffer);
using PFNGLBINDSAMPLERPROC = void(GLAPIENTRY*)(GLuint unit, GLuint sampler);
using PFNGLBINDTEXTUREPROC = void(GLAPIENTRY*)(GLenum target, GLuint texture);
using PFNGLBINDTRANSFORMFEEDBACKPROC = void(GLAPIENTRY*)(GLenum target, GLuint id);
using PFNGLBINDVERTEXARRAYPROC = void(GLAPIENTRY*)(GLuint array);
using PFNGLBLENDCOLORPROC = void(GLAPIENTRY*)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
using PFNGLBLENDEQUATIONPROC = void(GLAPIENTRY*)(GLenum mode);
using PFNGLBLENDEQUATIONSEPARATEPROC = void(GLAPIENTRY*)(GLenum modeRGB, GLenum modeAlpha);
using PFNGLBLENDFUNCPROC = void(GLAPIENTRY*)(GLenum sfactor, GLenum dfactor);
using PFNGLBLENDFUNCSEPARATEPROC = void(GLAPIENTRY*)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
using PFNGLBLITFRAMEBUFFERPROC = void(GLAPIENTRY*)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
	GLenum filter);
using PFNGLBUFFERDATAPROC = void(GLAPIENTRY*)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
using PFNGLBUFFERSUBDATAPROC = void(GLAPIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
using PFNGLCHECKFRAMEBUFFERSTATUSPROC = GLenum(GLAPIENTRY*)(GLenum target);
using PFNGLCLEARPROC = void(GLAPIENTRY*)(GLbitfield mask);
using PFNGLCLEARBUFFERFIPROC = void(GLAPIENTRY*)(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
using PFNGLCLEARBUFFERFVPROC = void(GLAPIENTRY*)(GLenum buffer, GLint drawbuffer, const GLfloat* value);
using PFNGLCLEARBUFFERIVPROC = void(GLAPIENTRY*)(GLenum buffer, GLint drawbuffer, const GLint* value);
using PFNGLCLEARBUFFERUIVPROC = void(GLAPIENTRY*)(GLenum buffer, GLint drawbuffer, const GLuint* value);
using PFNGLCLEARCOLORPROC = void(GLAPIENTRY*)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
using PFNGLCLEARDEPTHFPROC = void(GLAPIENTRY*)(GLfloat d);
using PFNGLCLEARSTENCILPROC = void(GLAPIENTRY*)(GLint s);
using PFNGLCLIENTWAITSYNCPROC = GLenum(GLAPIENTRY*)(GLsync sync, GLbitfield flags, GLuint64 timeout);
using PFNGLCOLORMASKPROC = void(GLAPIENTRY*)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
using PFNGLCOMPILESHADERPROC = void(GLAPIENTRY*)(GLuint shader);
using PFNGLCOMPRESSEDTEXIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
	const void* data);
using PFNGLCOMPRESSEDTEXIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border,
	GLsizei imageSize, const void* data);
using PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
	GLsizei imageSize, const void* data);
using PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
	GLenum format, GLsizei imageSize, const void* data);
using PFNGLCOPYBUFFERSUBDATAPROC = void(GLAPIENTRY*)(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
using PFNGLCOPYTEXIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
using PFNGLCOPYTEXSUBIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLCOPYTEXSUBIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLCREATEPROGRAMPROC = GLuint(GLAPIENTRY*)();
using PFNGLCREATESHADERPROC = GLuint(GLAPIENTRY*)(GLenum type);
using PFNGLCULLFACEPROC = void(GLAPIENTRY*)(GLenum mode);
using PFNGLDELETEBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* buffers);
using PFNGLDELETEFRAMEBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* framebuffers);
using PFNGLDELETEPROGRAMPROC = void(GLAPIENTRY*)(GLuint program);
using PFNGLDELETEQUERIESPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* ids);
using PFNGLDELETERENDERBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* renderbuffers);
using PFNGLDELETESAMPLERSPROC = void(GLAPIENTRY*)(GLsizei count, const GLuint* samplers);
using PFNGLDELETESHADERPROC = void(GLAPIENTRY*)(GLuint shader);
using PFNGLDELETESYNCPROC = void(GLAPIENTRY*)(GLsync sync);
using PFNGLDELETETEXTURESPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* textures);
using PFNGLDELETETRANSFORMFEEDBACKSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* ids);
using PFNGLDELETEVERTEXARRAYSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* arrays);
using PFNGLDEPTHFUNCPROC = void(GLAPIENTRY*)(GLenum func);
using PFNGLDEPTHMASKPROC = void(GLAPIENTRY*)(GLboolean flag);
using PFNGLDEPTHRANGEFPROC = void(GLAPIENTRY*)(GLfloat n, GLfloat f);
using PFNGLDETACHSHADERPROC = void(GLAPIENTRY*)(GLuint program, GLuint shader);
using PFNGLDISABLEPROC = void(GLAPIENTRY*)(GLenum cap);
using PFNGLDISABLEVERTEXATTRIBARRAYPROC = void(GLAPIENTRY*)(GLuint index);
using PFNGLDRAWARRAYSPROC = void(GLAPIENTRY*)(GLenum mode, GLint first, GLsizei count);
using PFNGLDRAWARRAYSINSTANCEDPROC = void(GLAPIENTRY*)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
using PFNGLDRAWBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, const GLenum* bufs);
using PFNGLDRAWELEMENTSPROC = void(GLAPIENTRY*)(GLenum mode, GLsizei count, GLenum type, const void* indices);
using PFNGLDRAWELEMENTSINSTANCEDPROC = void(GLAPIENTRY*)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
using PFNGLDRAWRANGEELEMENTSPROC = void(GLAPIENTRY*)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices);
using PFNGLENABLEPROC = void(GLAPIENTRY*)(GLenum cap);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void(GLAPIENTRY*)(GLuint index);
using PFNGLENDQUERYPROC = void(GLAPIENTRY*)(GLenum target);
using PFNGLENDTRANSFORMFEEDBACKPROC = void(GLAPIENTRY*)();
using PFNGLFENCESYNCPROC = GLsync(GLAPIENTRY*)(GLenum condition, GLbitfield flags);
using PFNGLFINISHPROC = void(GLAPIENTRY*)();
using PFNGLFLUSHPROC = void(GLAPIENTRY*)();
using PFNGLFLUSHMAPPEDBUFFERRANGEPROC = void(GLAPIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr length);
using PFNGLFRAMEBUFFERRENDERBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
using PFNGLFRAMEBUFFERTEXTURE2DPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
using PFNGLFRAMEBUFFERTEXTURELAYERPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
using PFNGLFRONTFACEPROC = void(GLAPIENTRY*)(GLenum mode);
using PFNGLGENBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* buffers);
using PFNGLGENFRAMEBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* framebuffers);
using PFNGLGENQUERIESPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* ids);
using PFNGLGENRENDERBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* renderbuffers);
using PFNGLGENSAMPLERSPROC = void(GLAPIENTRY*)(GLsizei count, GLuint* samplers);
using PFNGLGENTEXTURESPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* textures);
using PFNGLGENTRANSFORMFEEDBACKSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* ids);
using PFNGLGENVERTEXARRAYSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* arrays);
using PFNGLGENERATEMIPMAPPROC = void(GLAPIENTRY*)(GLenum target);
using PFNGLGETACTIVEATTRIBPROC = void(GLAPIENTRY*)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
using PFNGLGETACTIVEUNIFORMPROC = void(GLAPIENTRY*)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
using PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC = void(GLAPIENTRY*)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName);
using PFNGLGETACTIVEUNIFORMBLOCKIVPROC = void(GLAPIENTRY*)(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
using PFNGLGETACTIVEUNIFORMSIVPROC = void(GLAPIENTRY*)(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
using PFNGLGETATTACHEDSHADERSPROC = void(GLAPIENTRY*)(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
using PFNGLGETATTRIBLOCATIONPROC = GLint(GLAPIENTRY*)(GLuint program, const GLchar* name);
using PFNGLGETBOOLEANVPROC = void(GLAPIENTRY*)(GLenum pname, GLboolean* data);
using PFNGLGETBUFFERPARAMETERI64VPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint64* params);
using PFNGLGETBUFFERPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETBUFFERPOINTERVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, void** params);
using PFNGLGETERRORPROC = GLenum(GLAPIENTRY*)();
using PFNGLGETFLOATVPROC = void(GLAPIENTRY*)(GLenum pname, GLfloat* data);
using PFNGLGETFRAGDATALOCATIONPROC = GLint(GLAPIENTRY*)(GLuint program, const GLchar* name);
using PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLenum pname, GLint* params);
using PFNGLGETINTEGER64I_VPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLint64* data);
using PFNGLGETINTEGER64VPROC = void(GLAPIENTRY*)(GLenum pname, GLint64* data);
using PFNGLGETINTEGERI_VPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLint* data);
using PFNGLGETINTEGERVPROC = void(GLAPIENTRY*)(GLenum pname, GLint* data);
using PFNGLGETINTERNALFORMATIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint* params);
using PFNGLGETPROGRAMBINARYPROC = void(GLAPIENTRY*)(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, void* binary);
using PFNGLGETPROGRAMINFOLOGPROC = void(GLAPIENTRY*)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLGETPROGRAMIVPROC = void(GLAPIENTRY*)(GLuint program, GLenum pname, GLint* params);
using PFNGLGETQUERYOBJECTUIVPROC = void(GLAPIENTRY*)(GLuint id, GLenum pname, GLuint* params);
using PFNGLGETQUERYIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETRENDERBUFFERPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETSAMPLERPARAMETERFVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLfloat* params);
using PFNGLGETSAMPLERPARAMETERIVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLint* params);
using PFNGLGETSHADERINFOLOGPROC = void(GLAPIENTRY*)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLGETSHADERPRECISIONFORMATPROC = void(GLAPIENTRY*)(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);
using PFNGLGETSHADERSOURCEPROC = void(GLAPIENTRY*)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source);
using PFNGLGETSHADERIVPROC = void(GLAPIENTRY*)(GLuint shader, GLenum pname, GLint* params);
using PFNGLGETSTRINGPROC = const GLubyte*(GLAPIENTRY*)(GLenum name);
using PFNGLGETSTRINGIPROC = const GLubyte*(GLAPIENTRY*)(GLenum name, GLuint index);
using PFNGLGETSYNCIVPROC = void(GLAPIENTRY*)(GLsync sync, GLenum pname, GLsizei count, GLsizei* length, GLint* values);
using PFNGLGETTEXPARAMETERFVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLfloat* params);
using PFNGLGETTEXPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETTRANSFORMFEEDBACKVARYINGPROC = void(GLAPIENTRY*)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name);
using PFNGLGETUNIFORMBLOCKINDEXPROC = GLuint(GLAPIENTRY*)(GLuint program, const GLchar* uniformBlockName);
using PFNGLGETUNIFORMINDICESPROC = void(GLAPIENTRY*)(GLuint program, GLsizei uniformCount, const GLchar* const* uniformNames, GLuint* uniformIndices);
using PFNGLGETUNIFORMLOCATIONPROC = GLint(GLAPIENTRY*)(GLuint program, const GLchar* name);
using PFNGLGETUNIFORMFVPROC = void(GLAPIENTRY*)(GLuint program, GLint location, GLfloat* params);
using PFNGLGETUNIFORMIVPROC = void(GLAPIENTRY*)(GLuint program, GLint location, GLint* params);
using PFNGLGETUNIFORMUIVPROC = void(GLAPIENTRY*)(GLuint program, GLint location, GLuint* params);
using PFNGLGETVERTEXATTRIBIIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLint* params);
using PFNGLGETVERTEXATTRIBIUIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLuint* params);
using PFNGLGETVERTEXATTRIBPOINTERVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, void** pointer);
using PFNGLGETVERTEXATTRIBFVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLfloat* params);
using PFNGLGETVERTEXATTRIBIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLint* params);
using PFNGLHINTPROC = void(GLAPIENTRY*)(GLenum target, GLenum mode);
using PFNGLINVALIDATEFRAMEBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLsizei numAttachments, const GLenum* attachments);
using PFNGLINVALIDATESUBFRAMEBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLISBUFFERPROC = GLboolean(GLAPIENTRY*)(GLuint buffer);
using PFNGLISENABLEDPROC = GLboolean(GLAPIENTRY*)(GLenum cap);
using PFNGLISFRAMEBUFFERPROC = GLboolean(GLAPIENTRY*)(GLuint framebuffer);
using PFNGLISPROGRAMPROC = GLboolean(GLAPIENTRY*)(GLuint program);
using PFNGLISQUERYPROC = GLboolean(GLAPIENTRY*)(GLuint id);
using PFNGLISRENDERBUFFERPROC = GLboolean(GLAPIENTRY*)(GLuint renderbuffer);
using PFNGLISSAMPLERPROC = GLboolean(GLAPIENTRY*)(GLuint sampler);
using PFNGLISSHADERPROC = GLboolean(GLAPIENTRY*)(GLuint shader);
using PFNGLISSYNCPROC = GLboolean(GLAPIENTRY*)(GLsync sync);
using PFNGLISTEXTUREPROC = GLboolean(GLAPIENTRY*)(GLuint texture);
using PFNGLISTRANSFORMFEEDBACKPROC = GLboolean(GLAPIENTRY*)(GLuint id);
using PFNGLISVERTEXARRAYPROC = GLboolean(GLAPIENTRY*)(GLuint array);
using PFNGLLINEWIDTHPROC = void(GLAPIENTRY*)(GLfloat width);
using PFNGLLINKPROGRAMPROC = void(GLAPIENTRY*)(GLuint program);
using PFNGLMAPBUFFERRANGEPROC = void*(GLAPIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
using PFNGLPAUSETRANSFORMFEEDBACKPROC = void(GLAPIENTRY*)();
using PFNGLPIXELSTOREIPROC = void(GLAPIENTRY*)(GLenum pname, GLint param);
using PFNGLPOLYGONOFFSETPROC = void(GLAPIENTRY*)(GLfloat factor, GLfloat units);
using PFNGLPROGRAMBINARYPROC = void(GLAPIENTRY*)(GLuint program, GLenum binaryFormat, const void* binary, GLsizei length);
using PFNGLPROGRAMPARAMETERIPROC = void(GLAPIENTRY*)(GLuint program, GLenum pname, GLint value);
using PFNGLREADBUFFERPROC = void(GLAPIENTRY*)(GLenum src);
using PFNGLREADPIXELSPROC = void(GLAPIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
using PFNGLRELEASESHADERCOMPILERPROC = void(GLAPIENTRY*)();
using PFNGLRENDERBUFFERSTORAGEPROC = void(GLAPIENTRY*)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
using PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC = void(GLAPIENTRY*)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
using PFNGLRESUMETRANSFORMFEEDBACKPROC = void(GLAPIENTRY*)();
using PFNGLSAMPLECOVERAGEPROC = void(GLAPIENTRY*)(GLfloat value, GLboolean invert);
using PFNGLSAMPLERPARAMETERFPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLfloat param);
using PFNGLSAMPLERPARAMETERFVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, const GLfloat* param);
using PFNGLSAMPLERPARAMETERIPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLint param);
using PFNGLSAMPLERPARAMETERIVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, const GLint* param);
using PFNGLSCISSORPROC = void(GLAPIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLSHADERBINARYPROC = void(GLAPIENTRY*)(GLsizei count, const GLuint* shaders, GLenum binaryFormat, const void* binary, GLsizei length);
using PFNGLSHADERSOURCEPROC = void(GLAPIENTRY*)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
using PFNGLSTENCILFUNCPROC = void(GLAPIENTRY*)(GLenum func, GLint ref, GLuint mask);
using PFNGLSTENCILFUNCSEPARATEPROC = void(GLAPIENTRY*)(GLenum face, GLenum func, GLint ref, GLuint mask);
using PFNGLSTENCILMASKPROC = void(GLAPIENTRY*)(GLuint mask);
using PFNGLSTENCILMASKSEPARATEPROC = void(GLAPIENTRY*)(GLenum face, GLuint mask);
using PFNGLSTENCILOPPROC = void(GLAPIENTRY*)(GLenum fail, GLenum zfail, GLenum zpass);
using PFNGLSTENCILOPSEPARATEPROC = void(GLAPIENTRY*)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
using PFNGLTEXIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
	const void* pixels);
using PFNGLTEXIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format,
	GLenum type, const void* pixels);
using PFNGLTEXPARAMETERFPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLfloat param);
using PFNGLTEXPARAMETERFVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, const GLfloat* params);
using PFNGLTEXPARAMETERIPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint param);
using PFNGLTEXPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, const GLint* params);
using PFNGLTEXSTORAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
using PFNGLTEXSTORAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
using PFNGLTEXSUBIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
	const void* pixels);
using PFNGLTEXSUBIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
	GLenum format, GLenum type, const void* pixels);
using PFNGLTRANSFORMFEEDBACKVARYINGSPROC = void(GLAPIENTRY*)(GLuint program, GLsizei count, const GLchar* const* varyings, GLenum bufferMode);
using PFNGLUNIFORM1FPROC = void(GLAPIENTRY*)(GLint location, GLfloat v0);
using PFNGLUNIFORM1FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLfloat* value);
using PFNGLUNIFORM1IPROC = void(GLAPIENTRY*)(GLint location, GLint v0);
using PFNGLUNIFORM1IVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLint* value);
using PFNGLUNIFORM1UIPROC = void(GLAPIENTRY*)(GLint location, GLuint v0);
using PFNGLUNIFORM1UIVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLuint* value);
using PFNGLUNIFORM2FPROC = void(GLAPIENTRY*)(GLint location, GLfloat v0, GLfloat v1);
using PFNGLUNIFORM2FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLfloat* value);
using PFNGLUNIFORM2IPROC = void(GLAPIENTRY*)(GLint location, GLint v0, GLint v1);
using PFNGLUNIFORM2IVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLint* value);
using PFNGLUNIFORM2UIPROC = void(GLAPIENTRY*)(GLint location, GLuint v0, GLuint v1);
using PFNGLUNIFORM2UIVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLuint* value);
using PFNGLUNIFORM3FPROC = void(GLAPIENTRY*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
using PFNGLUNIFORM3FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLfloat* value);
using PFNGLUNIFORM3IPROC = void(GLAPIENTRY*)(GLint location, GLint v0, GLint v1, GLint v2);
using PFNGLUNIFORM3IVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLint* value);
using PFNGLUNIFORM3UIPROC = void(GLAPIENTRY*)(GLint location, GLuint v0, GLuint v1, GLuint v2);
using PFNGLUNIFORM3UIVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLuint* value);
using PFNGLUNIFORM4FPROC = void(GLAPIENTRY*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
using PFNGLUNIFORM4FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLfloat* value);
using PFNGLUNIFORM4IPROC = void(GLAPIENTRY*)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
using PFNGLUNIFORM4IVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLint* value);
using PFNGLUNIFORM4UIPROC = void(GLAPIENTRY*)(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
using PFNGLUNIFORM4UIVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLuint* value);
using PFNGLUNIFORMBLOCKBINDINGPROC = void(GLAPIENTRY*)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
using PFNGLUNIFORMMATRIX2FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX2X3FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX2X4FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX3FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX3X2FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX3X4FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX4FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX4X2FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX4X3FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNMAPBUFFERPROC = GLboolean(GLAPIENTRY*)(GLenum target);
using PFNGLUSEPROGRAMPROC = void(GLAPIENTRY*)(GLuint program);
using PFNGLVALIDATEPROGRAMPROC = void(GLAPIENTRY*)(GLuint program);
using PFNGLVERTEXATTRIB1FPROC = void(GLAPIENTRY*)(GLuint index, GLfloat x);
using PFNGLVERTEXATTRIB1FVPROC = void(GLAPIENTRY*)(GLuint index, const GLfloat* v);
using PFNGLVERTEXATTRIB2FPROC = void(GLAPIENTRY*)(GLuint index, GLfloat x, GLfloat y);
using PFNGLVERTEXATTRIB2FVPROC = void(GLAPIENTRY*)(GLuint index, const GLfloat* v);
using PFNGLVERTEXATTRIB3FPROC = void(GLAPIENTRY*)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
using PFNGLVERTEXATTRIB3FVPROC = void(GLAPIENTRY*)(GLuint index, const GLfloat* v);
using PFNGLVERTEXATTRIB4FPROC = void(GLAPIENTRY*)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
using PFNGLVERTEXATTRIB4FVPROC = void(GLAPIENTRY*)(GLuint index, const GLfloat* v);
using PFNGLVERTEXATTRIBDIVISORPROC = void(GLAPIENTRY*)(GLuint index, GLuint divisor);
using PFNGLVERTEXATTRIBI4IPROC = void(GLAPIENTRY*)(GLuint index, GLint x, GLint y, GLint z, GLint w);
using PFNGLVERTEXATTRIBI4IVPROC = void(GLAPIENTRY*)(GLuint index, const GLint* v);
using PFNGLVERTEXATTRIBI4UIPROC = void(GLAPIENTRY*)(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
using PFNGLVERTEXATTRIBI4UIVPROC = void(GLAPIENTRY*)(GLuint index, const GLuint* v);
using PFNGLVERTEXATTRIBIPOINTERPROC = void(GLAPIENTRY*)(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
using PFNGLVERTEXATTRIBPOINTERPROC = void(GLAPIENTRY*)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
using PFNGLVIEWPORTPROC = void(GLAPIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLWAITSYNCPROC = void(GLAPIENTRY*)(GLsync sync, GLbitfield flags, GLuint64 timeout);

extern "C" {
GLAPI PFNGLACTIVETEXTUREPROC glActiveTexture;
GLAPI PFNGLATTACHSHADERPROC glAttachShader;
GLAPI PFNGLBEGINQUERYPROC glBeginQuery;
GLAPI PFNGLBEGINTRANSFORMFEEDBACKPROC glBeginTransformFeedback;
GLAPI PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
GLAPI PFNGLBINDBUFFERPROC glBindBuffer;
GLAPI PFNGLBINDBUFFERBASEPROC glBindBufferBase;
GLAPI PFNGLBINDBUFFERRANGEPROC glBindBufferRange;
GLAPI PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
GLAPI PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
GLAPI PFNGLBINDSAMPLERPROC glBindSampler;
GLAPI PFNGLBINDTEXTUREPROC glBindTexture;
GLAPI PFNGLBINDTRANSFORMFEEDBACKPROC glBindTransformFeedback;
GLAPI PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
GLAPI PFNGLBLENDCOLORPROC glBlendColor;
GLAPI PFNGLBLENDEQUATIONPROC glBlendEquation;
GLAPI PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
GLAPI PFNGLBLENDFUNCPROC glBlendFunc;
GLAPI PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
GLAPI PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
GLAPI PFNGLBUFFERDATAPROC glBufferData;
GLAPI PFNGLBUFFERSUBDATAPROC glBufferSubData;
GLAPI PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
GLAPI PFNGLCLEARPROC glClear;
GLAPI PFNGLCLEARBUFFERFIPROC glClearBufferfi;
GLAPI PFNGLCLEARBUFFERFVPROC glClearBufferfv;
GLAPI PFNGLCLEARBUFFERIVPROC glClearBufferiv;
GLAPI PFNGLCLEARBUFFERUIVPROC glClearBufferuiv;
GLAPI PFNGLCLEARCOLORPROC glClearColor;
GLAPI PFNGLCLEARDEPTHFPROC glClearDepthf;
GLAPI PFNGLCLEARSTENCILPROC glClearStencil;
GLAPI PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
GLAPI PFNGLCOLORMASKPROC glColorMask;
GLAPI PFNGLCOMPILESHADERPROC glCompileShader;
GLAPI PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
GLAPI PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;
GLAPI PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
GLAPI PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;
GLAPI PFNGLCOPYBUFFERSUBDATAPROC glCopyBufferSubData;
GLAPI PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
GLAPI PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
GLAPI PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D;
GLAPI PFNGLCREATEPROGRAMPROC glCreateProgram;
GLAPI PFNGLCREATESHADERPROC glCreateShader;
GLAPI PFNGLCULLFACEPROC glCullFace;
GLAPI PFNGLDELETEBUFFERSPROC glDeleteBuffers;
GLAPI PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
GLAPI PFNGLDELETEPROGRAMPROC glDeleteProgram;
GLAPI PFNGLDELETEQUERIESPROC glDeleteQueries;
GLAPI PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
GLAPI PFNGLDELETESAMPLERSPROC glDeleteSamplers;
GLAPI PFNGLDELETESHADERPROC glDeleteShader;
GLAPI PFNGLDELETESYNCPROC glDeleteSync;
GLAPI PFNGLDELETETEXTURESPROC glDeleteTextures;
GLAPI PFNGLDELETETRANSFORMFEEDBACKSPROC glDeleteTransformFeedbacks;
GLAPI PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
GLAPI PFNGLDEPTHFUNCPROC glDepthFunc;
GLAPI PFNGLDEPTHMASKPROC glDepthMask;
GLAPI PFNGLDEPTHRANGEFPROC glDepthRangef;
GLAPI PFNGLDETACHSHADERPROC glDetachShader;
GLAPI PFNGLDISABLEPROC glDisable;
GLAPI PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
GLAPI PFNGLDRAWARRAYSPROC glDrawArrays;
GLAPI PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
GLAPI PFNGLDRAWBUFFERSPROC glDrawBuffers;
GLAPI PFNGLDRAWELEMENTSPROC glDrawElements;
GLAPI PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced;
GLAPI PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;
GLAPI PFNGLENABLEPROC glEnable;
GLAPI PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
GLAPI PFNGLENDQUERYPROC glEndQuery;
GLAPI PFNGLENDTRANSFORMFEEDBACKPROC glEndTransformFeedback;
GLAPI PFNGLFENCESYNCPROC glFenceSync;
GLAPI PFNGLFINISHPROC glFinish;
GLAPI PFNGLFLUSHPROC glFlush;
GLAPI PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange;
GLAPI PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
GLAPI PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
GLAPI PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer;
GLAPI PFNGLFRONTFACEPROC glFrontFace;
GLAPI PFNGLGENBUFFERSPROC glGenBuffers;
GLAPI PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
GLAPI PFNGLGENQUERIESPROC glGenQueries;
GLAPI PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
GLAPI PFNGLGENSAMPLERSPROC glGenSamplers;
GLAPI PFNGLGENTEXTURESPROC glGenTextures;
GLAPI PFNGLGENTRANSFORMFEEDBACKSPROC glGenTransformFeedbacks;
GLAPI PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
GLAPI PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
GLAPI PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;
GLAPI PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;
GLAPI PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glGetActiveUniformBlockName;
GLAPI PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv;
GLAPI PFNGLGETACTIVEUNIFORMSIVPROC glGetActiveUniformsiv;
GLAPI PFNGLGETATTACHEDSHADERSPROC glGetAttachedShaders;
GLAPI PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
GLAPI PFNGLGETBOOLEANVPROC glGetBooleanv;
GLAPI PFNGLGETBUFFERPARAMETERI64VPROC glGetBufferParameteri64v;
GLAPI PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
GLAPI PFNGLGETBUFFERPOINTERVPROC glGetBufferPointerv;
GLAPI PFNGLGETERRORPROC glGetError;
GLAPI PFNGLGETFLOATVPROC glGetFloatv;
GLAPI PFNGLGETFRAGDATALOCATIONPROC glGetFragDataLocation;
GLAPI PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv;
GLAPI PFNGLGETINTEGER64I_VPROC glGetInteger64i_v;
GLAPI PFNGLGETINTEGER64VPROC glGetInteger64v;
GLAPI PFNGLGETINTEGERI_VPROC glGetIntegeri_v;
GLAPI PFNGLGETINTEGERVPROC glGetIntegerv;
GLAPI PFNGLGETINTERNALFORMATIVPROC glGetInternalformativ;
GLAPI PFNGLGETPROGRAMBINARYPROC glGetProgramBinary;
GLAPI PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
GLAPI PFNGLGETPROGRAMIVPROC glGetProgramiv;
GLAPI PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;
GLAPI PFNGLGETQUERYIVPROC glGetQueryiv;
GLAPI PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv;
GLAPI PFNGLGETSAMPLERPARAMETERFVPROC glGetSamplerParameterfv;
GLAPI PFNGLGETSAMPLERPARAMETERIVPROC glGetSamplerParameteriv;
GLAPI PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
GLAPI PFNGLGETSHADERPRECISIONFORMATPROC glGetShaderPrecisionFormat;
GLAPI PFNGLGETSHADERSOURCEPROC glGetShaderSource;
GLAPI PFNGLGETSHADERIVPROC glGetShaderiv;
GLAPI PFNGLGETSTRINGPROC glGetString;
GLAPI PFNGLGETSTRINGIPROC glGetStringi;
GLAPI PFNGLGETSYNCIVPROC glGetSynciv;
GLAPI PFNGLGETTEXPARAMETERFVPROC glGetTexParameterfv;
GLAPI PFNGLGETTEXPARAMETERIVPROC glGetTexParameteriv;
GLAPI PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glGetTransformFeedbackVarying;
GLAPI PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
GLAPI PFNGLGETUNIFORMINDICESPROC glGetUniformIndices;
GLAPI PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
GLAPI PFNGLGETUNIFORMFVPROC glGetUniformfv;
GLAPI PFNGLGETUNIFORMIVPROC glGetUniformiv;
GLAPI PFNGLGETUNIFORMUIVPROC glGetUniformuiv;
GLAPI PFNGLGETVERTEXATTRIBIIVPROC glGetVertexAttribIiv;
GLAPI PFNGLGETVERTEXATTRIBIUIVPROC glGetVertexAttribIuiv;
GLAPI PFNGLGETVERTEXATTRIBPOINTERVPROC glGetVertexAttribPointerv;
GLAPI PFNGLGETVERTEXATTRIBFVPROC glGetVertexAttribfv;
GLAPI PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;
GLAPI PFNGLHINTPROC glHint;
GLAPI PFNGLINVALIDATEFRAMEBUFFERPROC glInvalidateFramebuffer;
GLAPI PFNGLINVALIDATESUBFRAMEBUFFERPROC glInvalidateSubFramebuffer;
GLAPI PFNGLISBUFFERPROC glIsBuffer;
GLAPI PFNGLISENABLEDPROC glIsEnabled;
GLAPI PFNGLISFRAMEBUFFERPROC glIsFramebuffer;
GLAPI PFNGLISPROGRAMPROC glIsProgram;
GLAPI PFNGLISQUERYPROC glIsQuery;
GLAPI PFNGLISRENDERBUFFERPROC glIsRenderbuffer;
GLAPI PFNGLISSAMPLERPROC glIsSampler;
GLAPI PFNGLISSHADERPROC glIsShader;
GLAPI PFNGLISSYNCPROC glIsSync;
GLAPI PFNGLISTEXTUREPROC glIsTexture;
GLAPI PFNGLISTRANSFORMFEEDBACKPROC glIsTransformFeedback;
GLAPI PFNGLISVERTEXARRAYPROC glIsVertexArray;
GLAPI PFNGLLINEWIDTHPROC glLineWidth;
GLAPI PFNGLLINKPROGRAMPROC glLinkProgram;
GLAPI PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
GLAPI PFNGLPAUSETRANSFORMFEEDBACKPROC glPauseTransformFeedback;
GLAPI PFNGLPIXELSTOREIPROC glPixelStorei;
GLAPI PFNGLPOLYGONOFFSETPROC glPolygonOffset;
GLAPI PFNGLPROGRAMBINARYPROC glProgramBinary;
GLAPI PFNGLPROGRAMPARAMETERIPROC glProgramParameteri;
GLAPI PFNGLREADBUFFERPROC glReadBuffer;
GLAPI PFNGLREADPIXELSPROC glReadPixels;
GLAPI PFNGLRELEASESHADERCOMPILERPROC glReleaseShaderCompiler;
GLAPI PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
GLAPI PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
GLAPI PFNGLRESUMETRANSFORMFEEDBACKPROC glResumeTransformFeedback;
GLAPI PFNGLSAMPLECOVERAGEPROC glSampleCoverage;
GLAPI PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf;
GLAPI PFNGLSAMPLERPARAMETERFVPROC glSamplerParameterfv;
GLAPI PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
GLAPI PFNGLSAMPLERPARAMETERIVPROC glSamplerParameteriv;
GLAPI PFNGLSCISSORPROC glScissor;
GLAPI PFNGLSHADERBINARYPROC glShaderBinary;
GLAPI PFNGLSHADERSOURCEPROC glShaderSource;
GLAPI PFNGLSTENCILFUNCPROC glStencilFunc;
GLAPI PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate;
GLAPI PFNGLSTENCILMASKPROC glStencilMask;
GLAPI PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate;
GLAPI PFNGLSTENCILOPPROC glStencilOp;
GLAPI PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate;
GLAPI PFNGLTEXIMAGE2DPROC glTexImage2D;
GLAPI PFNGLTEXIMAGE3DPROC glTexImage3D;
GLAPI PFNGLTEXPARAMETERFPROC glTexParameterf;
GLAPI PFNGLTEXPARAMETERFVPROC glTexParameterfv;
GLAPI PFNGLTEXPARAMETERIPROC glTexParameteri;
GLAPI PFNGLTEXPARAMETERIVPROC glTexParameteriv;
GLAPI PFNGLTEXSTORAGE2DPROC glTexStorage2D;
GLAPI PFNGLTEXSTORAGE3DPROC glTexStorage3D;
GLAPI PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
GLAPI PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
GLAPI PFNGLTRANSFORMFEEDBACKVARYINGSPROC glTransformFeedbackVaryings;
GLAPI PFNGLUNIFORM1FPROC glUniform1f;
GLAPI PFNGLUNIFORM1FVPROC glUniform1fv;
GLAPI PFNGLUNIFORM1IPROC glUniform1i;
GLAPI PFNGLUNIFORM1IVPROC glUniform1iv;
GLAPI PFNGLUNIFORM1UIPROC glUniform1ui;
GLAPI PFNGLUNIFORM1UIVPROC glUniform1uiv;
GLAPI PFNGLUNIFORM2FPROC glUniform2f;
GLAPI PFNGLUNIFORM2FVPROC glUniform2fv;
GLAPI PFNGLUNIFORM2IPROC glUniform2i;
GLAPI PFNGLUNIFORM2IVPROC glUniform2iv;
GLAPI PFNGLUNIFORM2UIPROC glUniform2ui;
GLAPI PFNGLUNIFORM2UIVPROC glUniform2uiv;
GLAPI PFNGLUNIFORM3FPROC glUniform3f;
GLAPI PFNGLUNIFORM3FVPROC glUniform3fv;
GLAPI PFNGLUNIFORM3IPROC glUniform3i;
GLAPI PFNGLUNIFORM3IVPROC glUniform3iv;
GLAPI PFNGLUNIFORM3UIPROC glUniform3ui;
GLAPI PFNGLUNIFORM3UIVPROC glUniform3uiv;
GLAPI PFNGLUNIFORM4FPROC glUniform4f;
GLAPI PFNGLUNIFORM4FVPROC glUniform4fv;
GLAPI PFNGLUNIFORM4IPROC glUniform4i;
GLAPI PFNGLUNIFORM4IVPROC glUniform4iv;
GLAPI PFNGLUNIFORM4UIPROC glUniform4ui;
GLAPI PFNGLUNIFORM4UIVPROC glUniform4uiv;
GLAPI PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;
GLAPI PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;
GLAPI PFNGLUNIFORMMATRIX2X3FVPROC glUniformMatrix2x3fv;
GLAPI PFNGLUNIFORMMATRIX2X4FVPROC glUniformMatrix2x4fv;
GLAPI PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
GLAPI PFNGLUNIFORMMATRIX3X2FVPROC glUniformMatrix3x2fv;
GLAPI PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv;
GLAPI PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
GLAPI PFNGLUNIFORMMATRIX4X2FVPROC glUniformMatrix4x2fv;
GLAPI PFNGLUNIFORMMATRIX4X3FVPROC glUniformMatrix4x3fv;
GLAPI PFNGLUNMAPBUFFERPROC glUnmapBuffer;
GLAPI PFNGLUSEPROGRAMPROC glUseProgram;
GLAPI PFNGLVALIDATEPROGRAMPROC glValidateProgram;
GLAPI PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;
GLAPI PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;
GLAPI PFNGLVERTEXATTRIB2FPROC glVertexAttrib2f;
GLAPI PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;
GLAPI PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f;
GLAPI PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;
GLAPI PFNGLVERTEXATTRIB4FPROC glVertexAttrib4f;
GLAPI PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;
GLAPI PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;
GLAPI PFNGLVERTEXATTRIBI4IPROC glVertexAttribI4i;
GLAPI PFNGLVERTEXATTRIBI4IVPROC glVertexAttribI4iv;
GLAPI PFNGLVERTEXATTRIBI4UIPROC glVertexAttribI4ui;
GLAPI PFNGLVERTEXATTRIBI4UIVPROC glVertexAttribI4uiv;
GLAPI PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;
GLAPI PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
GLAPI PFNGLVIEWPORTPROC glViewport;
GLAPI PFNGLWAITSYNCPROC glWaitSync;
}

#else

#define GL_ACTIVE_ATTRIBUTES                             0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH                   0x8B8A
#define GL_ACTIVE_TEXTURE                                0x84E0
#define GL_ACTIVE_UNIFORMS                               0x8B86
#define GL_ACTIVE_UNIFORM_BLOCKS                         0x8A36
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH          0x8A35
#define GL_ACTIVE_UNIFORM_MAX_LENGTH                     0x8B87
#define GL_ALIASED_LINE_WIDTH_RANGE                      0x846E
#define GL_ALPHA                                         0x1906
#define GL_ALREADY_SIGNALED                              0x911A
#define GL_ALWAYS                                        0x0207
#define GL_AND                                           0x1501
#define GL_AND_INVERTED                                  0x1504
#define GL_AND_REVERSE                                   0x1502
#define GL_ANY_SAMPLES_PASSED                            0x8C2F
#define GL_ARRAY_BUFFER                                  0x8892
#define GL_ARRAY_BUFFER_BINDING                          0x8894
#define GL_ATTACHED_SHADERS                              0x8B85
#define GL_BACK                                          0x0405
#define GL_BACK_LEFT                                     0x0402
#define GL_BACK_RIGHT                                    0x0403
#define GL_BGR                                           0x80E0
#define GL_BGRA                                          0x80E1
#define GL_BGRA_INTEGER                                  0x8D9B
#define GL_BGR_INTEGER                                   0x8D9A
#define GL_BLEND                                         0x0BE2
#define GL_BLEND_COLOR                                   0x8005
#define GL_BLEND_DST                                     0x0BE0
#define GL_BLEND_DST_ALPHA                               0x80CA
#define GL_BLEND_DST_RGB                                 0x80C8
#define GL_BLEND_EQUATION                                0x8009
#define GL_BLEND_EQUATION_ALPHA                          0x883D
#define GL_BLEND_EQUATION_RGB                            0x8009
#define GL_BLEND_SRC                                     0x0BE1
#define GL_BLEND_SRC_ALPHA                               0x80CB
#define GL_BLEND_SRC_RGB                                 0x80C9
#define GL_BLUE                                          0x1905
#define GL_BLUE_INTEGER                                  0x8D96
#define GL_BOOL                                          0x8B56
#define GL_BOOL_VEC2                                     0x8B57
#define GL_BOOL_VEC3                                     0x8B58
#define GL_BOOL_VEC4                                     0x8B59
#define GL_BUFFER_ACCESS                                 0x88BB
#define GL_BUFFER_ACCESS_FLAGS                           0x911F
#define GL_BUFFER_MAPPED                                 0x88BC
#define GL_BUFFER_MAP_LENGTH                             0x9120
#define GL_BUFFER_MAP_OFFSET                             0x9121
#define GL_BUFFER_MAP_POINTER                            0x88BD
#define GL_BUFFER_SIZE                                   0x8764
#define GL_BUFFER_USAGE                                  0x8765
#define GL_BYTE                                          0x1400
#define GL_CCW                                           0x0901
#define GL_CLAMP_READ_COLOR                              0x891C
#define GL_CLAMP_TO_BORDER                               0x812D
#define GL_CLAMP_TO_EDGE                                 0x812F
#define GL_CLEAR                                         0x1500
#define GL_CLIP_DISTANCE0                                0x3000
#define GL_CLIP_DISTANCE1                                0x3001
#define GL_CLIP_DISTANCE2                                0x3002
#define GL_CLIP_DISTANCE3                                0x3003
#define GL_CLIP_DISTANCE4                                0x3004
#define GL_CLIP_DISTANCE5                                0x3005
#define GL_CLIP_DISTANCE6                                0x3006
#define GL_CLIP_DISTANCE7                                0x3007
#define GL_COLOR                                         0x1800
#define GL_COLOR_ATTACHMENT0                             0x8CE0
#define GL_COLOR_ATTACHMENT1                             0x8CE1
#define GL_COLOR_ATTACHMENT10                            0x8CEA
#define GL_COLOR_ATTACHMENT11                            0x8CEB
#define GL_COLOR_ATTACHMENT12                            0x8CEC
#define GL_COLOR_ATTACHMENT13                            0x8CED
#define GL_COLOR_ATTACHMENT14                            0x8CEE
#define GL_COLOR_ATTACHMENT15                            0x8CEF
#define GL_COLOR_ATTACHMENT16                            0x8CF0
#define GL_COLOR_ATTACHMENT17                            0x8CF1
#define GL_COLOR_ATTACHMENT18                            0x8CF2
#define GL_COLOR_ATTACHMENT19                            0x8CF3
#define GL_COLOR_ATTACHMENT2                             0x8CE2
#define GL_COLOR_ATTACHMENT20                            0x8CF4
#define GL_COLOR_ATTACHMENT21                            0x8CF5
#define GL_COLOR_ATTACHMENT22                            0x8CF6
#define GL_COLOR_ATTACHMENT23                            0x8CF7
#define GL_COLOR_ATTACHMENT24                            0x8CF8
#define GL_COLOR_ATTACHMENT25                            0x8CF9
#define GL_COLOR_ATTACHMENT26                            0x8CFA
#define GL_COLOR_ATTACHMENT27                            0x8CFB
#define GL_COLOR_ATTACHMENT28                            0x8CFC
#define GL_COLOR_ATTACHMENT29                            0x8CFD
#define GL_COLOR_ATTACHMENT3                             0x8CE3
#define GL_COLOR_ATTACHMENT30                            0x8CFE
#define GL_COLOR_ATTACHMENT31                            0x8CFF
#define GL_COLOR_ATTACHMENT4                             0x8CE4
#define GL_COLOR_ATTACHMENT5                             0x8CE5
#define GL_COLOR_ATTACHMENT6                             0x8CE6
#define GL_COLOR_ATTACHMENT7                             0x8CE7
#define GL_COLOR_ATTACHMENT8                             0x8CE8
#define GL_COLOR_ATTACHMENT9                             0x8CE9
#define GL_COLOR_BUFFER_BIT                              0x00004000
#define GL_COLOR_CLEAR_VALUE                             0x0C22
#define GL_COLOR_LOGIC_OP                                0x0BF2
#define GL_COLOR_WRITEMASK                               0x0C23
#define GL_COMPARE_REF_TO_TEXTURE                        0x884E
#define GL_COMPILE_STATUS                                0x8B81
#define GL_COMPRESSED_RED                                0x8225
#define GL_COMPRESSED_RED_RGTC1                          0x8DBB
#define GL_COMPRESSED_RG                                 0x8226
#define GL_COMPRESSED_RGB                                0x84ED
#define GL_COMPRESSED_RGBA                               0x84EE
#define GL_COMPRESSED_RG_RGTC2                           0x8DBD
#define GL_COMPRESSED_SIGNED_RED_RGTC1                   0x8DBC
#define GL_COMPRESSED_SIGNED_RG_RGTC2                    0x8DBE
#define GL_COMPRESSED_SRGB                               0x8C48
#define GL_COMPRESSED_SRGB_ALPHA                         0x8C49
#define GL_COMPRESSED_TEXTURE_FORMATS                    0x86A3
#define GL_CONDITION_SATISFIED                           0x911C
#define GL_CONSTANT_ALPHA                                0x8003
#define GL_CONSTANT_COLOR                                0x8001
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT             0x00000002
#define GL_CONTEXT_CORE_PROFILE_BIT                      0x00000001
#define GL_CONTEXT_FLAGS                                 0x821E
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT           0x00000001
#define GL_CONTEXT_PROFILE_MASK                          0x9126
#define GL_COPY                                          0x1503
#define GL_COPY_INVERTED                                 0x150C
#define GL_COPY_READ_BUFFER                              0x8F36
#define GL_COPY_WRITE_BUFFER                             0x8F37
#define GL_CULL_FACE                                     0x0B44
#define GL_CULL_FACE_MODE                                0x0B45
#define GL_CURRENT_PROGRAM                               0x8B8D
#define GL_CURRENT_QUERY                                 0x8865
#define GL_CURRENT_VERTEX_ATTRIB                         0x8626
#define GL_CW                                            0x0900
#define GL_DECR                                          0x1E03
#define GL_DECR_WRAP                                     0x8508
#define GL_DELETE_STATUS                                 0x8B80
#define GL_DEPTH                                         0x1801
#define GL_DEPTH24_STENCIL8                              0x88F0
#define GL_DEPTH32F_STENCIL8                             0x8CAD
#define GL_DEPTH_ATTACHMENT                              0x8D00
#define GL_DEPTH_BUFFER_BIT                              0x00000100
#define GL_DEPTH_CLAMP                                   0x864F
#define GL_DEPTH_CLEAR_VALUE                             0x0B73
#define GL_DEPTH_COMPONENT                               0x1902
#define GL_DEPTH_COMPONENT16                             0x81A5
#define GL_DEPTH_COMPONENT24                             0x81A6
#define GL_DEPTH_COMPONENT32                             0x81A7
#define GL_DEPTH_COMPONENT32F                            0x8CAC
#define GL_DEPTH_FUNC                                    0x0B74
#define GL_DEPTH_RANGE                                   0x0B70
#define GL_DEPTH_STENCIL                                 0x84F9
#define GL_DEPTH_STENCIL_ATTACHMENT                      0x821A
#define GL_DEPTH_TEST                                    0x0B71
#define GL_DEPTH_WRITEMASK                               0x0B72
#define GL_DITHER                                        0x0BD0
#define GL_DONT_CARE                                     0x1100
#define GL_DOUBLE                                        0x140A
#define GL_DOUBLEBUFFER                                  0x0C32
#define GL_DRAW_BUFFER                                   0x0C01
#define GL_DRAW_BUFFER0                                  0x8825
#define GL_DRAW_BUFFER1                                  0x8826
#define GL_DRAW_BUFFER10                                 0x882F
#define GL_DRAW_BUFFER11                                 0x8830
#define GL_DRAW_BUFFER12                                 0x8831
#define GL_DRAW_BUFFER13                                 0x8832
#define GL_DRAW_BUFFER14                                 0x8833
#define GL_DRAW_BUFFER15                                 0x8834
#define GL_DRAW_BUFFER2                                  0x8827
#define GL_DRAW_BUFFER3                                  0x8828
#define GL_DRAW_BUFFER4                                  0x8829
#define GL_DRAW_BUFFER5                                  0x882A
#define GL_DRAW_BUFFER6                                  0x882B
#define GL_DRAW_BUFFER7                                  0x882C
#define GL_DRAW_BUFFER8                                  0x882D
#define GL_DRAW_BUFFER9                                  0x882E
#define GL_DRAW_FRAMEBUFFER                              0x8CA9
#define GL_DRAW_FRAMEBUFFER_BINDING                      0x8CA6
#define GL_DST_ALPHA                                     0x0304
#define GL_DST_COLOR                                     0x0306
#define GL_DYNAMIC_COPY                                  0x88EA
#define GL_DYNAMIC_DRAW                                  0x88E8
#define GL_DYNAMIC_READ                                  0x88E9
#define GL_ELEMENT_ARRAY_BUFFER                          0x8893
#define GL_ELEMENT_ARRAY_BUFFER_BINDING                  0x8895
#define GL_EQUAL                                         0x0202
#define GL_EQUIV                                         0x1509
#define GL_EXTENSIONS                                    0x1F03
#define GL_FALSE                                         0
#define GL_FASTEST                                       0x1101
#define GL_FILL                                          0x1B02
#define GL_FIRST_VERTEX_CONVENTION                       0x8E4D
#define GL_FIXED_ONLY                                    0x891D
#define GL_FLOAT                                         0x1406
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV                0x8DAD
#define GL_FLOAT_MAT2                                    0x8B5A
#define GL_FLOAT_MAT2x3                                  0x8B65
#define GL_FLOAT_MAT2x4                                  0x8B66
#define GL_FLOAT_MAT3                                    0x8B5B
#define GL_FLOAT_MAT3x2                                  0x8B67
#define GL_FLOAT_MAT3x4                                  0x8B68
#define GL_FLOAT_MAT4                                    0x8B5C
#define GL_FLOAT_MAT4x2                                  0x8B69
#define GL_FLOAT_MAT4x3                                  0x8B6A
#define GL_FLOAT_VEC2                                    0x8B50
#define GL_FLOAT_VEC3                                    0x8B51
#define GL_FLOAT_VEC4                                    0x8B52
#define GL_FRAGMENT_SHADER                               0x8B30
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT               0x8B8B
#define GL_FRAMEBUFFER                                   0x8D40
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE             0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE              0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING         0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE         0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE             0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE             0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_LAYERED                0x8DA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME            0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE            0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE               0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE           0x8217
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE  0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER          0x8CD4
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL          0x8CD2
#define GL_FRAMEBUFFER_BINDING                           0x8CA6
#define GL_FRAMEBUFFER_COMPLETE                          0x8CD5
#define GL_FRAMEBUFFER_DEFAULT                           0x8218
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT             0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER            0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS          0x8DA8
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT     0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE            0x8D56
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER            0x8CDC
#define GL_FRAMEBUFFER_SRGB                              0x8DB9
#define GL_FRAMEBUFFER_UNDEFINED                         0x8219
#define GL_FRAMEBUFFER_UNSUPPORTED                       0x8CDD
#define GL_FRONT                                         0x0404
#define GL_FRONT_AND_BACK                                0x0408
#define GL_FRONT_FACE                                    0x0B46
#define GL_FRONT_LEFT                                    0x0400
#define GL_FRONT_RIGHT                                   0x0401
#define GL_FUNC_ADD                                      0x8006
#define GL_FUNC_REVERSE_SUBTRACT                         0x800B
#define GL_FUNC_SUBTRACT                                 0x800A
#define GL_GEOMETRY_INPUT_TYPE                           0x8917
#define GL_GEOMETRY_OUTPUT_TYPE                          0x8918
#define GL_GEOMETRY_SHADER                               0x8DD9
#define GL_GEOMETRY_VERTICES_OUT                         0x8916
#define GL_GEQUAL                                        0x0206
#define GL_GREATER                                       0x0204
#define GL_GREEN                                         0x1904
#define GL_GREEN_INTEGER                                 0x8D95
#define GL_HALF_FLOAT                                    0x140B
#define GL_INCR                                          0x1E02
#define GL_INCR_WRAP                                     0x8507
#define GL_INFO_LOG_LENGTH                               0x8B84
#define GL_INT                                           0x1404
#define GL_INTERLEAVED_ATTRIBS                           0x8C8C
#define GL_INT_2_10_10_10_REV                            0x8D9F
#define GL_INT_SAMPLER_1D                                0x8DC9
#define GL_INT_SAMPLER_1D_ARRAY                          0x8DCE
#define GL_INT_SAMPLER_2D                                0x8DCA
#define GL_INT_SAMPLER_2D_ARRAY                          0x8DCF
#define GL_INT_SAMPLER_2D_MULTISAMPLE                    0x9109
#define GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY              0x910C
#define GL_INT_SAMPLER_2D_RECT                           0x8DCD
#define GL_INT_SAMPLER_3D                                0x8DCB
#define GL_INT_SAMPLER_BUFFER                            0x8DD0
#define GL_INT_SAMPLER_CUBE                              0x8DCC
#define GL_INT_VEC2                                      0x8B53
#define GL_INT_VEC3                                      0x8B54
#define GL_INT_VEC4                                      0x8B55
#define GL_INVALID_ENUM                                  0x0500
#define GL_INVALID_FRAMEBUFFER_OPERATION                 0x0506
#define GL_INVALID_INDEX                                 0xFFFFFFFF
#define GL_INVALID_OPERATION                             0x0502
#define GL_INVALID_VALUE                                 0x0501
#define GL_INVERT                                        0x150A
#define GL_KEEP                                          0x1E00
#define GL_LAST_VERTEX_CONVENTION                        0x8E4E
#define GL_LEFT                                          0x0406
#define GL_LEQUAL                                        0x0203
#define GL_LESS                                          0x0201
#define GL_LINE                                          0x1B01
#define GL_LINEAR                                        0x2601
#define GL_LINEAR_MIPMAP_LINEAR                          0x2703
#define GL_LINEAR_MIPMAP_NEAREST                         0x2701
#define GL_LINES                                         0x0001
#define GL_LINES_ADJACENCY                               0x000A
#define GL_LINE_LOOP                                     0x0002
#define GL_LINE_SMOOTH                                   0x0B20
#define GL_LINE_SMOOTH_HINT                              0x0C52
#define GL_LINE_STRIP                                    0x0003
#define GL_LINE_STRIP_ADJACENCY                          0x000B
#define GL_LINE_WIDTH                                    0x0B21
#define GL_LINE_WIDTH_GRANULARITY                        0x0B23
#define GL_LINE_WIDTH_RANGE                              0x0B22
#define GL_LINK_STATUS                                   0x8B82
#define GL_LOGIC_OP_MODE                                 0x0BF0
#define GL_LOWER_LEFT                                    0x8CA1
#define GL_MAJOR_VERSION                                 0x821B
#define GL_MAP_FLUSH_EXPLICIT_BIT                        0x0010
#define GL_MAP_INVALIDATE_BUFFER_BIT                     0x0008
#define GL_MAP_INVALIDATE_RANGE_BIT                      0x0004
#define GL_MAP_READ_BIT                                  0x0001
#define GL_MAP_UNSYNCHRONIZED_BIT                        0x0020
#define GL_MAP_WRITE_BIT                                 0x0002
#define GL_MAX                                           0x8008
#define GL_MAX_3D_TEXTURE_SIZE                           0x8073
#define GL_MAX_ARRAY_TEXTURE_LAYERS                      0x88FF
#define GL_MAX_CLIP_DISTANCES                            0x0D32
#define GL_MAX_COLOR_ATTACHMENTS                         0x8CDF
#define GL_MAX_COLOR_TEXTURE_SAMPLES                     0x910E
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS      0x8A33
#define GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS      0x8A32
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS              0x8B4D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS                   0x8A2E
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS        0x8A31
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE                     0x851C
#define GL_MAX_DEPTH_TEXTURE_SAMPLES                     0x910F
#define GL_MAX_DRAW_BUFFERS                              0x8824
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS                  0x88FC
#define GL_MAX_ELEMENTS_INDICES                          0x80E9
#define GL_MAX_ELEMENTS_VERTICES                         0x80E8
#define GL_MAX_FRAGMENT_INPUT_COMPONENTS                 0x9125
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS                   0x8A2D
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS               0x8B49
#define GL_MAX_GEOMETRY_INPUT_COMPONENTS                 0x9123
#define GL_MAX_GEOMETRY_OUTPUT_COMPONENTS                0x9124
#define GL_MAX_GEOMETRY_OUTPUT_VERTICES                  0x8DE0
#define GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS              0x8C29
#define GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS          0x8DE1
#define GL_MAX_GEOMETRY_UNIFORM_BLOCKS                   0x8A2C
#define GL_MAX_GEOMETRY_UNIFORM_COMPONENTS               0x8DDF
#define GL_MAX_INTEGER_SAMPLES                           0x9110
#define GL_MAX_PROGRAM_TEXEL_OFFSET                      0x8905
#define GL_MAX_RECTANGLE_TEXTURE_SIZE                    0x84F8
#define GL_MAX_RENDERBUFFER_SIZE                         0x84E8
#define GL_MAX_SAMPLES                                   0x8D57
#define GL_MAX_SAMPLE_MASK_WORDS                         0x8E59
#define GL_MAX_SERVER_WAIT_TIMEOUT                       0x9111
#define GL_MAX_TEXTURE_BUFFER_SIZE                       0x8C2B
#define GL_MAX_TEXTURE_IMAGE_UNITS                       0x8872
#define GL_MAX_TEXTURE_LOD_BIAS                          0x84FD
#define GL_MAX_TEXTURE_SIZE                              0x0D33
#define GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS 0x8C8A
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS       0x8C8B
#define GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS    0x8C80
#define GL_MAX_UNIFORM_BLOCK_SIZE                        0x8A30
#define GL_MAX_UNIFORM_BUFFER_BINDINGS                   0x8A2F
#define GL_MAX_VARYING_COMPONENTS                        0x8B4B
#define GL_MAX_VARYING_FLOATS                            0x8B4B
#define GL_MAX_VERTEX_ATTRIBS                            0x8869
#define GL_MAX_VERTEX_OUTPUT_COMPONENTS                  0x9122
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS                0x8B4C
#define GL_MAX_VERTEX_UNIFORM_BLOCKS                     0x8A2B
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS                 0x8B4A
#define GL_MAX_VIEWPORT_DIMS                             0x0D3A
#define GL_MIN                                           0x8007
#define GL_MINOR_VERSION                                 0x821C
#define GL_MIN_PROGRAM_TEXEL_OFFSET                      0x8904
#define GL_MIRRORED_REPEAT                               0x8370
#define GL_MULTISAMPLE                                   0x809D
#define GL_NAND                                          0x150E
#define GL_NEAREST                                       0x2600
#define GL_NEAREST_MIPMAP_LINEAR                         0x2702
#define GL_NEAREST_MIPMAP_NEAREST                        0x2700
#define GL_NEVER                                         0x0200
#define GL_NICEST                                        0x1102
#define GL_NONE                                          0
#define GL_NOOP                                          0x1505
#define GL_NOR                                           0x1508
#define GL_NOTEQUAL                                      0x0205
#define GL_NO_ERROR                                      0
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS                0x86A2
#define GL_NUM_EXTENSIONS                                0x821D
#define GL_OBJECT_TYPE                                   0x9112
#define GL_ONE                                           1
#define GL_ONE_MINUS_CONSTANT_ALPHA                      0x8004
#define GL_ONE_MINUS_CONSTANT_COLOR                      0x8002
#define GL_ONE_MINUS_DST_ALPHA                           0x0305
#define GL_ONE_MINUS_DST_COLOR                           0x0307
#define GL_ONE_MINUS_SRC1_ALPHA                          0x88FB
#define GL_ONE_MINUS_SRC1_COLOR                          0x88FA
#define GL_ONE_MINUS_SRC_ALPHA                           0x0303
#define GL_ONE_MINUS_SRC_COLOR                           0x0301
#define GL_OR                                            0x1507
#define GL_OR_INVERTED                                   0x150D
#define GL_OR_REVERSE                                    0x150B
#define GL_OUT_OF_MEMORY                                 0x0505
#define GL_PACK_ALIGNMENT                                0x0D05
#define GL_PACK_IMAGE_HEIGHT                             0x806C
#define GL_PACK_LSB_FIRST                                0x0D01
#define GL_PACK_ROW_LENGTH                               0x0D02
#define GL_PACK_SKIP_IMAGES                              0x806B
#define GL_PACK_SKIP_PIXELS                              0x0D04
#define GL_PACK_SKIP_ROWS                                0x0D03
#define GL_PACK_SWAP_BYTES                               0x0D00
#define GL_PIXEL_PACK_BUFFER                             0x88EB
#define GL_PIXEL_PACK_BUFFER_BINDING                     0x88ED
#define GL_PIXEL_UNPACK_BUFFER                           0x88EC
#define GL_PIXEL_UNPACK_BUFFER_BINDING                   0x88EF
#define GL_POINT                                         0x1B00
#define GL_POINTS                                        0x0000
#define GL_POINT_FADE_THRESHOLD_SIZE                     0x8128
#define GL_POINT_SIZE                                    0x0B11
#define GL_POINT_SIZE_GRANULARITY                        0x0B13
#define GL_POINT_SIZE_RANGE                              0x0B12
#define GL_POINT_SPRITE_COORD_ORIGIN                     0x8CA0
#define GL_POLYGON_MODE                                  0x0B40
#define GL_POLYGON_OFFSET_FACTOR                         0x8038
#define GL_POLYGON_OFFSET_FILL                           0x8037
#define GL_POLYGON_OFFSET_LINE                           0x2A02
#define GL_POLYGON_OFFSET_POINT                          0x2A01
#define GL_POLYGON_OFFSET_UNITS                          0x2A00
#define GL_POLYGON_SMOOTH                                0x0B41
#define GL_POLYGON_SMOOTH_HINT                           0x0C53
#define GL_PRIMITIVES_GENERATED                          0x8C87
#define GL_PRIMITIVE_RESTART                             0x8F9D
#define GL_PRIMITIVE_RESTART_INDEX                       0x8F9E
#define GL_PROGRAM_POINT_SIZE                            0x8642
#define GL_PROVOKING_VERTEX                              0x8E4F
#define GL_PROXY_TEXTURE_1D                              0x8063
#define GL_PROXY_TEXTURE_1D_ARRAY                        0x8C19
#define GL_PROXY_TEXTURE_2D                              0x8064
#define GL_PROXY_TEXTURE_2D_ARRAY                        0x8C1B
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE                  0x9101
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY            0x9103
#define GL_PROXY_TEXTURE_3D                              0x8070
#define GL_PROXY_TEXTURE_CUBE_MAP                        0x851B
#define GL_PROXY_TEXTURE_RECTANGLE                       0x84F7
#define GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION      0x8E4C
#define GL_QUERY_BY_REGION_NO_WAIT                       0x8E16
#define GL_QUERY_BY_REGION_WAIT                          0x8E15
#define GL_QUERY_COUNTER_BITS                            0x8864
#define GL_QUERY_NO_WAIT                                 0x8E14
#define GL_QUERY_RESULT                                  0x8866
#define GL_QUERY_RESULT_AVAILABLE                        0x8867
#define GL_QUERY_WAIT                                    0x8E13
#define GL_R11F_G11F_B10F                                0x8C3A
#define GL_R16                                           0x822A
#define GL_R16F                                          0x822D
#define GL_R16I                                          0x8233
#define GL_R16UI                                         0x8234
#define GL_R16_SNORM                                     0x8F98
#define GL_R32F                                          0x822E
#define GL_R32I                                          0x8235
#define GL_R32UI                                         0x8236
#define GL_R3_G3_B2                                      0x2A10
#define GL_R8                                            0x8229
#define GL_R8I                                           0x8231
#define GL_R8UI                                          0x8232
#define GL_R8_SNORM                                      0x8F94
#define GL_RASTERIZER_DISCARD                            0x8C89
#define GL_READ_BUFFER                                   0x0C02
#define GL_READ_FRAMEBUFFER                              0x8CA8
#define GL_READ_FRAMEBUFFER_BINDING                      0x8CAA
#define GL_READ_ONLY                                     0x88B8
#define GL_READ_WRITE                                    0x88BA
#define GL_RED                                           0x1903
#define GL_RED_INTEGER                                   0x8D94
#define GL_RENDERBUFFER                                  0x8D41
#define GL_RENDERBUFFER_ALPHA_SIZE                       0x8D53
#define GL_RENDERBUFFER_BINDING                          0x8CA7
#define GL_RENDERBUFFER_BLUE_SIZE                        0x8D52
#define GL_RENDERBUFFER_DEPTH_SIZE                       0x8D54
#define GL_RENDERBUFFER_GREEN_SIZE                       0x8D51
#define GL_RENDERBUFFER_HEIGHT                           0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT                  0x8D44
#define GL_RENDERBUFFER_RED_SIZE                         0x8D50
#define GL_RENDERBUFFER_SAMPLES                          0x8CAB
#define GL_RENDERBUFFER_STENCIL_SIZE                     0x8D55
#define GL_RENDERBUFFER_WIDTH                            0x8D42
#define GL_RENDERER                                      0x1F01
#define GL_REPEAT                                        0x2901
#define GL_REPLACE                                       0x1E01
#define GL_RG                                            0x8227
#define GL_RG16                                          0x822C
#define GL_RG16F                                         0x822F
#define GL_RG16I                                         0x8239
#define GL_RG16UI                                        0x823A
#define GL_RG16_SNORM                                    0x8F99
#define GL_RG32F                                         0x8230
#define GL_RG32I                                         0x823B
#define GL_RG32UI                                        0x823C
#define GL_RG8                                           0x822B
#define GL_RG8I                                          0x8237
#define GL_RG8UI                                         0x8238
#define GL_RG8_SNORM                                     0x8F95
#define GL_RGB                                           0x1907
#define GL_RGB10                                         0x8052
#define GL_RGB10_A2                                      0x8059
#define GL_RGB10_A2UI                                    0x906F
#define GL_RGB12                                         0x8053
#define GL_RGB16                                         0x8054
#define GL_RGB16F                                        0x881B
#define GL_RGB16I                                        0x8D89
#define GL_RGB16UI                                       0x8D77
#define GL_RGB16_SNORM                                   0x8F9A
#define GL_RGB32F                                        0x8815
#define GL_RGB32I                                        0x8D83
#define GL_RGB32UI                                       0x8D71
#define GL_RGB4                                          0x804F
#define GL_RGB5                                          0x8050
#define GL_RGB5_A1                                       0x8057
#define GL_RGB8                                          0x8051
#define GL_RGB8I                                         0x8D8F
#define GL_RGB8UI                                        0x8D7D
#define GL_RGB8_SNORM                                    0x8F96
#define GL_RGB9_E5                                       0x8C3D
#define GL_RGBA                                          0x1908
#define GL_RGBA12                                        0x805A
#define GL_RGBA16                                        0x805B
#define GL_RGBA16F                                       0x881A
#define GL_RGBA16I                                       0x8D88
#define GL_RGBA16UI                                      0x8D76
#define GL_RGBA16_SNORM                                  0x8F9B
#define GL_RGBA2                                         0x8055
#define GL_RGBA32F                                       0x8814
#define GL_RGBA32I                                       0x8D82
#define GL_RGBA32UI                                      0x8D70
#define GL_RGBA4                                         0x8056
#define GL_RGBA8                                         0x8058
#define GL_RGBA8I                                        0x8D8E
#define GL_RGBA8UI                                       0x8D7C
#define GL_RGBA8_SNORM                                   0x8F97
#define GL_RGBA_INTEGER                                  0x8D99
#define GL_RGB_INTEGER                                   0x8D98
#define GL_RG_INTEGER                                    0x8228
#define GL_RIGHT                                         0x0407
#define GL_SAMPLER_1D                                    0x8B5D
#define GL_SAMPLER_1D_ARRAY                              0x8DC0
#define GL_SAMPLER_1D_ARRAY_SHADOW                       0x8DC3
#define GL_SAMPLER_1D_SHADOW                             0x8B61
#define GL_SAMPLER_2D                                    0x8B5E
#define GL_SAMPLER_2D_ARRAY                              0x8DC1
#define GL_SAMPLER_2D_ARRAY_SHADOW                       0x8DC4
#define GL_SAMPLER_2D_MULTISAMPLE                        0x9108
#define GL_SAMPLER_2D_MULTISAMPLE_ARRAY                  0x910B
#define GL_SAMPLER_2D_RECT                               0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW                        0x8B64
#define GL_SAMPLER_2D_SHADOW                             0x8B62
#define GL_SAMPLER_3D                                    0x8B5F
#define GL_SAMPLER_BINDING                               0x8919
#define GL_SAMPLER_BUFFER                                0x8DC2
#define GL_SAMPLER_CUBE                                  0x8B60
#define GL_SAMPLER_CUBE_SHADOW                           0x8DC5
#define GL_SAMPLES                                       0x80A9
#define GL_SAMPLES_PASSED                                0x8914
#define GL_SAMPLE_ALPHA_TO_COVERAGE                      0x809E
#define GL_SAMPLE_ALPHA_TO_ONE                           0x809F
#define GL_SAMPLE_BUFFERS                                0x80A8
#define GL_SAMPLE_COVERAGE                               0x80A0
#define GL_SAMPLE_COVERAGE_INVERT                        0x80AB
#define GL_SAMPLE_COVERAGE_VALUE                         0x80AA
#define GL_SAMPLE_MASK                                   0x8E51
#define GL_SAMPLE_MASK_VALUE                             0x8E52
#define GL_SAMPLE_POSITION                               0x8E50
#define GL_SCISSOR_BOX                                   0x0C10
#define GL_SCISSOR_TEST                                  0x0C11
#define GL_SEPARATE_ATTRIBS                              0x8C8D
#define GL_SET                                           0x150F
#define GL_SHADER_SOURCE_LENGTH                          0x8B88
#define GL_SHADER_TYPE                                   0x8B4F
#define GL_SHADING_LANGUAGE_VERSION                      0x8B8C
#define GL_SHORT                                         0x1402
#define GL_SIGNALED                                      0x9119
#define GL_SIGNED_NORMALIZED                             0x8F9C
#define GL_SMOOTH_LINE_WIDTH_GRANULARITY                 0x0B23
#define GL_SMOOTH_LINE_WIDTH_RANGE                       0x0B22
#define GL_SMOOTH_POINT_SIZE_GRANULARITY                 0x0B13
#define GL_SMOOTH_POINT_SIZE_RANGE                       0x0B12
#define GL_SRC1_ALPHA                                    0x8589
#define GL_SRC1_COLOR                                    0x88F9
#define GL_SRC_ALPHA                                     0x0302
#define GL_SRC_ALPHA_SATURATE                            0x0308
#define GL_SRC_COLOR                                     0x0300
#define GL_SRGB                                          0x8C40
#define GL_SRGB8                                         0x8C41
#define GL_SRGB8_ALPHA8                                  0x8C43
#define GL_SRGB_ALPHA                                    0x8C42
#define GL_STATIC_COPY                                   0x88E6
#define GL_STATIC_DRAW                                   0x88E4
#define GL_STATIC_READ                                   0x88E5
#define GL_STENCIL                                       0x1802
#define GL_STENCIL_ATTACHMENT                            0x8D20
#define GL_STENCIL_BACK_FAIL                             0x8801
#define GL_STENCIL_BACK_FUNC                             0x8800
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL                  0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS                  0x8803
#define GL_STENCIL_BACK_REF                              0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK                       0x8CA4
#define GL_STENCIL_BACK_WRITEMASK                        0x8CA5
#define GL_STENCIL_BUFFER_BIT                            0x00000400
#define GL_STENCIL_CLEAR_VALUE                           0x0B91
#define GL_STENCIL_FAIL                                  0x0B94
#define GL_STENCIL_FUNC                                  0x0B92
#define GL_STENCIL_INDEX                                 0x1901
#define GL_STENCIL_INDEX1                                0x8D46
#define GL_STENCIL_INDEX16                               0x8D49
#define GL_STENCIL_INDEX4                                0x8D47
#define GL_STENCIL_INDEX8                                0x8D48
#define GL_STENCIL_PASS_DEPTH_FAIL                       0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS                       0x0B96
#define GL_STENCIL_REF                                   0x0B97
#define GL_STENCIL_TEST                                  0x0B90
#define GL_STENCIL_VALUE_MASK                            0x0B93
#define GL_STENCIL_WRITEMASK                             0x0B98
#define GL_STEREO                                        0x0C33
#define GL_STREAM_COPY                                   0x88E2
#define GL_STREAM_DRAW                                   0x88E0
#define GL_STREAM_READ                                   0x88E1
#define GL_SUBPIXEL_BITS                                 0x0D50
#define GL_SYNC_CONDITION                                0x9113
#define GL_SYNC_FENCE                                    0x9116
#define GL_SYNC_FLAGS                                    0x9115
#define GL_SYNC_FLUSH_COMMANDS_BIT                       0x00000001
#define GL_SYNC_GPU_COMMANDS_COMPLETE                    0x9117
#define GL_SYNC_STATUS                                   0x9114
#define GL_TEXTURE                                       0x1702
#define GL_TEXTURE0                                      0x84C0
#define GL_TEXTURE1                                      0x84C1
#define GL_TEXTURE10                                     0x84CA
#define GL_TEXTURE11                                     0x84CB
#define GL_TEXTURE12                                     0x84CC
#define GL_TEXTURE13                                     0x84CD
#define GL_TEXTURE14                                     0x84CE
#define GL_TEXTURE15                                     0x84CF
#define GL_TEXTURE16                                     0x84D0
#define GL_TEXTURE17                                     0x84D1
#define GL_TEXTURE18                                     0x84D2
#define GL_TEXTURE19                                     0x84D3
#define GL_TEXTURE2                                      0x84C2
#define GL_TEXTURE20                                     0x84D4
#define GL_TEXTURE21                                     0x84D5
#define GL_TEXTURE22                                     0x84D6
#define GL_TEXTURE23                                     0x84D7
#define GL_TEXTURE24                                     0x84D8
#define GL_TEXTURE25                                     0x84D9
#define GL_TEXTURE26                                     0x84DA
#define GL_TEXTURE27                                     0x84DB
#define GL_TEXTURE28                                     0x84DC
#define GL_TEXTURE29                                     0x84DD
#define GL_TEXTURE3                                      0x84C3
#define GL_TEXTURE30                                     0x84DE
#define GL_TEXTURE31                                     0x84DF
#define GL_TEXTURE4                                      0x84C4
#define GL_TEXTURE5                                      0x84C5
#define GL_TEXTURE6                                      0x84C6
#define GL_TEXTURE7                                      0x84C7
#define GL_TEXTURE8                                      0x84C8
#define GL_TEXTURE9                                      0x84C9
#define GL_TEXTURE_1D                                    0x0DE0
#define GL_TEXTURE_1D_ARRAY                              0x8C18
#define GL_TEXTURE_2D                                    0x0DE1
#define GL_TEXTURE_2D_ARRAY                              0x8C1A
#define GL_TEXTURE_2D_MULTISAMPLE                        0x9100
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY                  0x9102
#define GL_TEXTURE_3D                                    0x806F
#define GL_TEXTURE_ALPHA_SIZE                            0x805F
#define GL_TEXTURE_ALPHA_TYPE                            0x8C13
#define GL_TEXTURE_BASE_LEVEL                            0x813C
#define GL_TEXTURE_BINDING_1D                            0x8068
#define GL_TEXTURE_BINDING_1D_ARRAY                      0x8C1C
#define GL_TEXTURE_BINDING_2D                            0x8069
#define GL_TEXTURE_BINDING_2D_ARRAY                      0x8C1D
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE                0x9104
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY          0x9105
#define GL_TEXTURE_BINDING_3D                            0x806A
#define GL_TEXTURE_BINDING_BUFFER                        0x8C2C
#define GL_TEXTURE_BINDING_CUBE_MAP                      0x8514
#define GL_TEXTURE_BINDING_RECTANGLE                     0x84F6
#define GL_TEXTURE_BLUE_SIZE                             0x805E
#define GL_TEXTURE_BLUE_TYPE                             0x8C12
#define GL_TEXTURE_BORDER_COLOR                          0x1004
#define GL_TEXTURE_BUFFER                                0x8C2A
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING             0x8C2D
#define GL_TEXTURE_COMPARE_FUNC                          0x884D
#define GL_TEXTURE_COMPARE_MODE                          0x884C
#define GL_TEXTURE_COMPRESSED                            0x86A1
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE                 0x86A0
#define GL_TEXTURE_COMPRESSION_HINT                      0x84EF
#define GL_TEXTURE_CUBE_MAP                              0x8513
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X                   0x8516
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y                   0x8518
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z                   0x851A
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X                   0x8515
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y                   0x8517
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z                   0x8519
#define GL_TEXTURE_CUBE_MAP_SEAMLESS                     0x884F
#define GL_TEXTURE_DEPTH                                 0x8071
#define GL_TEXTURE_DEPTH_SIZE                            0x884A
#define GL_TEXTURE_DEPTH_TYPE                            0x8C16
#define GL_TEXTURE_FIXED_SAMPLE_LOCATIONS                0x9107
#define GL_TEXTURE_GREEN_SIZE                            0x805D
#define GL_TEXTURE_GREEN_TYPE                            0x8C11
#define GL_TEXTURE_HEIGHT                                0x1001
#define GL_TEXTURE_INTERNAL_FORMAT                       0x1003
#define GL_TEXTURE_LOD_BIAS                              0x8501
#define GL_TEXTURE_MAG_FILTER                            0x2800
#define GL_TEXTURE_MAX_LEVEL                             0x813D
#define GL_TEXTURE_MAX_LOD                               0x813B
#define GL_TEXTURE_MIN_FILTER                            0x2801
#define GL_TEXTURE_MIN_LOD                               0x813A
#define GL_TEXTURE_RECTANGLE                             0x84F5
#define GL_TEXTURE_RED_SIZE                              0x805C
#define GL_TEXTURE_RED_TYPE                              0x8C10
#define GL_TEXTURE_SAMPLES                               0x9106
#define GL_TEXTURE_SHARED_SIZE                           0x8C3F
#define GL_TEXTURE_STENCIL_SIZE                          0x88F1
#define GL_TEXTURE_SWIZZLE_A                             0x8E45
#define GL_TEXTURE_SWIZZLE_B                             0x8E44
#define GL_TEXTURE_SWIZZLE_G                             0x8E43
#define GL_TEXTURE_SWIZZLE_R                             0x8E42
#define GL_TEXTURE_SWIZZLE_RGBA                          0x8E46
#define GL_TEXTURE_WIDTH                                 0x1000
#define GL_TEXTURE_WRAP_R                                0x8072
#define GL_TEXTURE_WRAP_S                                0x2802
#define GL_TEXTURE_WRAP_T                                0x2803
#define GL_TIMEOUT_EXPIRED                               0x911B
#define GL_TIMEOUT_IGNORED                               0xFFFFFFFFFFFFFFFF
#define GL_TIMESTAMP                                     0x8E28
#define GL_TIME_ELAPSED                                  0x88BF
#define GL_TRANSFORM_FEEDBACK_BUFFER                     0x8C8E
#define GL_TRANSFORM_FEEDBACK_BUFFER_BINDING             0x8C8F
#define GL_TRANSFORM_FEEDBACK_BUFFER_MODE                0x8C7F
#define GL_TRANSFORM_FEEDBACK_BUFFER_SIZE                0x8C85
#define GL_TRANSFORM_FEEDBACK_BUFFER_START               0x8C84
#define GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN         0x8C88
#define GL_TRANSFORM_FEEDBACK_VARYINGS                   0x8C83
#define GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH         0x8C76
#define GL_TRIANGLES                                     0x0004
#define GL_TRIANGLES_ADJACENCY                           0x000C
#define GL_TRIANGLE_FAN                                  0x0006
#define GL_TRIANGLE_STRIP                                0x0005
#define GL_TRIANGLE_STRIP_ADJACENCY                      0x000D
#define GL_TRUE                                          1
#define GL_UNIFORM_ARRAY_STRIDE                          0x8A3C
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS                 0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES          0x8A43
#define GL_UNIFORM_BLOCK_BINDING                         0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE                       0x8A40
#define GL_UNIFORM_BLOCK_INDEX                           0x8A3A
#define GL_UNIFORM_BLOCK_NAME_LENGTH                     0x8A41
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER   0x8A46
#define GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER   0x8A45
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER     0x8A44
#define GL_UNIFORM_BUFFER                                0x8A11
#define GL_UNIFORM_BUFFER_BINDING                        0x8A28
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT               0x8A34
#define GL_UNIFORM_BUFFER_SIZE                           0x8A2A
#define GL_UNIFORM_BUFFER_START                          0x8A29
#define GL_UNIFORM_IS_ROW_MAJOR                          0x8A3E
#define GL_UNIFORM_MATRIX_STRIDE                         0x8A3D
#define GL_UNIFORM_NAME_LENGTH                           0x8A39
#define GL_UNIFORM_OFFSET                                0x8A3B
#define GL_UNIFORM_SIZE                                  0x8A38
#define GL_UNIFORM_TYPE                                  0x8A37
#define GL_UNPACK_ALIGNMENT                              0x0CF5
#define GL_UNPACK_IMAGE_HEIGHT                           0x806E
#define GL_UNPACK_LSB_FIRST                              0x0CF1
#define GL_UNPACK_ROW_LENGTH                             0x0CF2
#define GL_UNPACK_SKIP_IMAGES                            0x806D
#define GL_UNPACK_SKIP_PIXELS                            0x0CF4
#define GL_UNPACK_SKIP_ROWS                              0x0CF3
#define GL_UNPACK_SWAP_BYTES                             0x0CF0
#define GL_UNSIGNALED                                    0x9118
#define GL_UNSIGNED_BYTE                                 0x1401
#define GL_UNSIGNED_BYTE_2_3_3_REV                       0x8362
#define GL_UNSIGNED_BYTE_3_3_2                           0x8032
#define GL_UNSIGNED_INT                                  0x1405
#define GL_UNSIGNED_INT_10F_11F_11F_REV                  0x8C3B
#define GL_UNSIGNED_INT_10_10_10_2                       0x8036
#define GL_UNSIGNED_INT_24_8                             0x84FA
#define GL_UNSIGNED_INT_2_10_10_10_REV                   0x8368
#define GL_UNSIGNED_INT_5_9_9_9_REV                      0x8C3E
#define GL_UNSIGNED_INT_8_8_8_8                          0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV                      0x8367
#define GL_UNSIGNED_INT_SAMPLER_1D                       0x8DD1
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY                 0x8DD6
#define GL_UNSIGNED_INT_SAMPLER_2D                       0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY                 0x8DD7
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE           0x910A
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY     0x910D
#define GL_UNSIGNED_INT_SAMPLER_2D_RECT                  0x8DD5
#define GL_UNSIGNED_INT_SAMPLER_3D                       0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_BUFFER                   0x8DD8
#define GL_UNSIGNED_INT_SAMPLER_CUBE                     0x8DD4
#define GL_UNSIGNED_INT_VEC2                             0x8DC6
#define GL_UNSIGNED_INT_VEC3                             0x8DC7
#define GL_UNSIGNED_INT_VEC4                             0x8DC8
#define GL_UNSIGNED_NORMALIZED                           0x8C17
#define GL_UNSIGNED_SHORT                                0x1403
#define GL_UNSIGNED_SHORT_1_5_5_5_REV                    0x8366
#define GL_UNSIGNED_SHORT_4_4_4_4                        0x8033
#define GL_UNSIGNED_SHORT_4_4_4_4_REV                    0x8365
#define GL_UNSIGNED_SHORT_5_5_5_1                        0x8034
#define GL_UNSIGNED_SHORT_5_6_5                          0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV                      0x8364
#define GL_UPPER_LEFT                                    0x8CA2
#define GL_VALIDATE_STATUS                               0x8B83
#define GL_VENDOR                                        0x1F00
#define GL_VERSION                                       0x1F02
#define GL_VERTEX_ARRAY_BINDING                          0x85B5
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING            0x889F
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR                   0x88FE
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED                   0x8622
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER                   0x88FD
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED                0x886A
#define GL_VERTEX_ATTRIB_ARRAY_POINTER                   0x8645
#define GL_VERTEX_ATTRIB_ARRAY_SIZE                      0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE                    0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE                      0x8625
#define GL_VERTEX_PROGRAM_POINT_SIZE                     0x8642
#define GL_VERTEX_SHADER                                 0x8B31
#define GL_VIEWPORT                                      0x0BA2
#define GL_WAIT_FAILED                                   0x911D
#define GL_WRITE_ONLY                                    0x88B9
#define GL_XOR                                           0x1506
#define GL_ZERO                                          0

using PFNGLACTIVETEXTUREPROC = void(GLAPIENTRY*)(GLenum texture);
using PFNGLATTACHSHADERPROC = void(GLAPIENTRY*)(GLuint program, GLuint shader);
using PFNGLBEGINCONDITIONALRENDERPROC = void(GLAPIENTRY*)(GLuint id, GLenum mode);
using PFNGLBEGINQUERYPROC = void(GLAPIENTRY*)(GLenum target, GLuint id);
using PFNGLBEGINTRANSFORMFEEDBACKPROC = void(GLAPIENTRY*)(GLenum primitiveMode);
using PFNGLBINDATTRIBLOCATIONPROC = void(GLAPIENTRY*)(GLuint program, GLuint index, const GLchar* name);
using PFNGLBINDBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLuint buffer);
using PFNGLBINDBUFFERBASEPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLuint buffer);
using PFNGLBINDBUFFERRANGEPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
using PFNGLBINDFRAGDATALOCATIONPROC = void(GLAPIENTRY*)(GLuint program, GLuint color, const GLchar* name);
using PFNGLBINDFRAGDATALOCATIONINDEXEDPROC = void(GLAPIENTRY*)(GLuint program, GLuint colorNumber, GLuint index, const GLchar* name);
using PFNGLBINDFRAMEBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLuint framebuffer);
using PFNGLBINDRENDERBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLuint renderbuffer);
using PFNGLBINDSAMPLERPROC = void(GLAPIENTRY*)(GLuint unit, GLuint sampler);
using PFNGLBINDTEXTUREPROC = void(GLAPIENTRY*)(GLenum target, GLuint texture);
using PFNGLBINDVERTEXARRAYPROC = void(GLAPIENTRY*)(GLuint array);
using PFNGLBLENDCOLORPROC = void(GLAPIENTRY*)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
using PFNGLBLENDEQUATIONPROC = void(GLAPIENTRY*)(GLenum mode);
using PFNGLBLENDEQUATIONSEPARATEPROC = void(GLAPIENTRY*)(GLenum modeRGB, GLenum modeAlpha);
using PFNGLBLENDFUNCPROC = void(GLAPIENTRY*)(GLenum sfactor, GLenum dfactor);
using PFNGLBLENDFUNCSEPARATEPROC = void(GLAPIENTRY*)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
using PFNGLBLITFRAMEBUFFERPROC = void(GLAPIENTRY*)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
	GLenum filter);
using PFNGLBUFFERDATAPROC = void(GLAPIENTRY*)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
using PFNGLBUFFERSUBDATAPROC = void(GLAPIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
using PFNGLCHECKFRAMEBUFFERSTATUSPROC = GLenum(GLAPIENTRY*)(GLenum target);
using PFNGLCLAMPCOLORPROC = void(GLAPIENTRY*)(GLenum target, GLenum clamp);
using PFNGLCLEARPROC = void(GLAPIENTRY*)(GLbitfield mask);
using PFNGLCLEARBUFFERFIPROC = void(GLAPIENTRY*)(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
using PFNGLCLEARBUFFERFVPROC = void(GLAPIENTRY*)(GLenum buffer, GLint drawbuffer, const GLfloat* value);
using PFNGLCLEARBUFFERIVPROC = void(GLAPIENTRY*)(GLenum buffer, GLint drawbuffer, const GLint* value);
using PFNGLCLEARBUFFERUIVPROC = void(GLAPIENTRY*)(GLenum buffer, GLint drawbuffer, const GLuint* value);
using PFNGLCLEARCOLORPROC = void(GLAPIENTRY*)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
using PFNGLCLEARDEPTHPROC = void(GLAPIENTRY*)(GLdouble depth);
using PFNGLCLEARSTENCILPROC = void(GLAPIENTRY*)(GLint s);
using PFNGLCLIENTWAITSYNCPROC = GLenum(GLAPIENTRY*)(GLsync sync, GLbitfield flags, GLuint64 timeout);
using PFNGLCOLORMASKPROC = void(GLAPIENTRY*)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
using PFNGLCOLORMASKIPROC = void(GLAPIENTRY*)(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
using PFNGLCOMPILESHADERPROC = void(GLAPIENTRY*)(GLuint shader);
using PFNGLCOMPRESSEDTEXIMAGE1DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void* data);
using PFNGLCOMPRESSEDTEXIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize,
	const void* data);
using PFNGLCOMPRESSEDTEXIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border,
	GLsizei imageSize, const void* data);
using PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data);
using PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
	GLsizei imageSize, const void* data);
using PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
	GLenum format, GLsizei imageSize, const void* data);
using PFNGLCOPYBUFFERSUBDATAPROC = void(GLAPIENTRY*)(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
using PFNGLCOPYTEXIMAGE1DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
using PFNGLCOPYTEXIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
using PFNGLCOPYTEXSUBIMAGE1DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
using PFNGLCOPYTEXSUBIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLCOPYTEXSUBIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLCREATEPROGRAMPROC = GLuint(GLAPIENTRY*)();
using PFNGLCREATESHADERPROC = GLuint(GLAPIENTRY*)(GLenum type);
using PFNGLCULLFACEPROC = void(GLAPIENTRY*)(GLenum mode);
using PFNGLDELETEBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* buffers);
using PFNGLDELETEFRAMEBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* framebuffers);
using PFNGLDELETEPROGRAMPROC = void(GLAPIENTRY*)(GLuint program);
using PFNGLDELETEQUERIESPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* ids);
using PFNGLDELETERENDERBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* renderbuffers);
using PFNGLDELETESAMPLERSPROC = void(GLAPIENTRY*)(GLsizei count, const GLuint* samplers);
using PFNGLDELETESHADERPROC = void(GLAPIENTRY*)(GLuint shader);
using PFNGLDELETESYNCPROC = void(GLAPIENTRY*)(GLsync sync);
using PFNGLDELETETEXTURESPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* textures);
using PFNGLDELETEVERTEXARRAYSPROC = void(GLAPIENTRY*)(GLsizei n, const GLuint* arrays);
using PFNGLDEPTHFUNCPROC = void(GLAPIENTRY*)(GLenum func);
using PFNGLDEPTHMASKPROC = void(GLAPIENTRY*)(GLboolean flag);
using PFNGLDEPTHRANGEPROC = void(GLAPIENTRY*)(GLdouble n, GLdouble f);
using PFNGLDETACHSHADERPROC = void(GLAPIENTRY*)(GLuint program, GLuint shader);
using PFNGLDISABLEPROC = void(GLAPIENTRY*)(GLenum cap);
using PFNGLDISABLEVERTEXATTRIBARRAYPROC = void(GLAPIENTRY*)(GLuint index);
using PFNGLDISABLEIPROC = void(GLAPIENTRY*)(GLenum target, GLuint index);
using PFNGLDRAWARRAYSPROC = void(GLAPIENTRY*)(GLenum mode, GLint first, GLsizei count);
using PFNGLDRAWARRAYSINSTANCEDPROC = void(GLAPIENTRY*)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
using PFNGLDRAWBUFFERPROC = void(GLAPIENTRY*)(GLenum buf);
using PFNGLDRAWBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, const GLenum* bufs);
using PFNGLDRAWELEMENTSPROC = void(GLAPIENTRY*)(GLenum mode, GLsizei count, GLenum type, const void* indices);
using PFNGLDRAWELEMENTSBASEVERTEXPROC = void(GLAPIENTRY*)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex);
using PFNGLDRAWELEMENTSINSTANCEDPROC = void(GLAPIENTRY*)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
using PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC = void(GLAPIENTRY*)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex);
using PFNGLDRAWRANGEELEMENTSPROC = void(GLAPIENTRY*)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices);
using PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC = void(GLAPIENTRY*)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices, GLint basevertex);
using PFNGLENABLEPROC = void(GLAPIENTRY*)(GLenum cap);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void(GLAPIENTRY*)(GLuint index);
using PFNGLENABLEIPROC = void(GLAPIENTRY*)(GLenum target, GLuint index);
using PFNGLENDCONDITIONALRENDERPROC = void(GLAPIENTRY*)();
using PFNGLENDQUERYPROC = void(GLAPIENTRY*)(GLenum target);
using PFNGLENDTRANSFORMFEEDBACKPROC = void(GLAPIENTRY*)();
using PFNGLFENCESYNCPROC = GLsync(GLAPIENTRY*)(GLenum condition, GLbitfield flags);
using PFNGLFINISHPROC = void(GLAPIENTRY*)();
using PFNGLFLUSHPROC = void(GLAPIENTRY*)();
using PFNGLFLUSHMAPPEDBUFFERRANGEPROC = void(GLAPIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr length);
using PFNGLFRAMEBUFFERRENDERBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
using PFNGLFRAMEBUFFERTEXTUREPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLuint texture, GLint level);
using PFNGLFRAMEBUFFERTEXTURE1DPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
using PFNGLFRAMEBUFFERTEXTURE2DPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
using PFNGLFRAMEBUFFERTEXTURE3DPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
using PFNGLFRAMEBUFFERTEXTURELAYERPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
using PFNGLFRONTFACEPROC = void(GLAPIENTRY*)(GLenum mode);
using PFNGLGENBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* buffers);
using PFNGLGENFRAMEBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* framebuffers);
using PFNGLGENQUERIESPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* ids);
using PFNGLGENRENDERBUFFERSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* renderbuffers);
using PFNGLGENSAMPLERSPROC = void(GLAPIENTRY*)(GLsizei count, GLuint* samplers);
using PFNGLGENTEXTURESPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* textures);
using PFNGLGENVERTEXARRAYSPROC = void(GLAPIENTRY*)(GLsizei n, GLuint* arrays);
using PFNGLGENERATEMIPMAPPROC = void(GLAPIENTRY*)(GLenum target);
using PFNGLGETACTIVEATTRIBPROC = void(GLAPIENTRY*)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
using PFNGLGETACTIVEUNIFORMPROC = void(GLAPIENTRY*)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
using PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC = void(GLAPIENTRY*)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName);
using PFNGLGETACTIVEUNIFORMBLOCKIVPROC = void(GLAPIENTRY*)(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
using PFNGLGETACTIVEUNIFORMNAMEPROC = void(GLAPIENTRY*)(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName);
using PFNGLGETACTIVEUNIFORMSIVPROC = void(GLAPIENTRY*)(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
using PFNGLGETATTACHEDSHADERSPROC = void(GLAPIENTRY*)(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
using PFNGLGETATTRIBLOCATIONPROC = GLint(GLAPIENTRY*)(GLuint program, const GLchar* name);
using PFNGLGETBOOLEANI_VPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLboolean* data);
using PFNGLGETBOOLEANVPROC = void(GLAPIENTRY*)(GLenum pname, GLboolean* data);
using PFNGLGETBUFFERPARAMETERI64VPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint64* params);
using PFNGLGETBUFFERPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETBUFFERPOINTERVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, void** params);
using PFNGLGETBUFFERSUBDATAPROC = void(GLAPIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr size, void* data);
using PFNGLGETCOMPRESSEDTEXIMAGEPROC = void(GLAPIENTRY*)(GLenum target, GLint level, void* img);
using PFNGLGETDOUBLEVPROC = void(GLAPIENTRY*)(GLenum pname, GLdouble* data);
using PFNGLGETERRORPROC = GLenum(GLAPIENTRY*)();
using PFNGLGETFLOATVPROC = void(GLAPIENTRY*)(GLenum pname, GLfloat* data);
using PFNGLGETFRAGDATAINDEXPROC = GLint(GLAPIENTRY*)(GLuint program, const GLchar* name);
using PFNGLGETFRAGDATALOCATIONPROC = GLint(GLAPIENTRY*)(GLuint program, const GLchar* name);
using PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum attachment, GLenum pname, GLint* params);
using PFNGLGETINTEGER64I_VPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLint64* data);
using PFNGLGETINTEGER64VPROC = void(GLAPIENTRY*)(GLenum pname, GLint64* data);
using PFNGLGETINTEGERI_VPROC = void(GLAPIENTRY*)(GLenum target, GLuint index, GLint* data);
using PFNGLGETINTEGERVPROC = void(GLAPIENTRY*)(GLenum pname, GLint* data);
using PFNGLGETMULTISAMPLEFVPROC = void(GLAPIENTRY*)(GLenum pname, GLuint index, GLfloat* val);
using PFNGLGETPROGRAMINFOLOGPROC = void(GLAPIENTRY*)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLGETPROGRAMIVPROC = void(GLAPIENTRY*)(GLuint program, GLenum pname, GLint* params);
using PFNGLGETQUERYOBJECTI64VPROC = void(GLAPIENTRY*)(GLuint id, GLenum pname, GLint64* params);
using PFNGLGETQUERYOBJECTIVPROC = void(GLAPIENTRY*)(GLuint id, GLenum pname, GLint* params);
using PFNGLGETQUERYOBJECTUI64VPROC = void(GLAPIENTRY*)(GLuint id, GLenum pname, GLuint64* params);
using PFNGLGETQUERYOBJECTUIVPROC = void(GLAPIENTRY*)(GLuint id, GLenum pname, GLuint* params);
using PFNGLGETQUERYIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETRENDERBUFFERPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETSAMPLERPARAMETERIIVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLint* params);
using PFNGLGETSAMPLERPARAMETERIUIVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLuint* params);
using PFNGLGETSAMPLERPARAMETERFVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLfloat* params);
using PFNGLGETSAMPLERPARAMETERIVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLint* params);
using PFNGLGETSHADERINFOLOGPROC = void(GLAPIENTRY*)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLGETSHADERSOURCEPROC = void(GLAPIENTRY*)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* source);
using PFNGLGETSHADERIVPROC = void(GLAPIENTRY*)(GLuint shader, GLenum pname, GLint* params);
using PFNGLGETSTRINGPROC = const GLubyte*(GLAPIENTRY*)(GLenum name);
using PFNGLGETSTRINGIPROC = const GLubyte*(GLAPIENTRY*)(GLenum name, GLuint index);
using PFNGLGETSYNCIVPROC = void(GLAPIENTRY*)(GLsync sync, GLenum pname, GLsizei count, GLsizei* length, GLint* values);
using PFNGLGETTEXIMAGEPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum format, GLenum type, void* pixels);
using PFNGLGETTEXLEVELPARAMETERFVPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum pname, GLfloat* params);
using PFNGLGETTEXLEVELPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLenum pname, GLint* params);
using PFNGLGETTEXPARAMETERIIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETTEXPARAMETERIUIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLuint* params);
using PFNGLGETTEXPARAMETERFVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLfloat* params);
using PFNGLGETTEXPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint* params);
using PFNGLGETTRANSFORMFEEDBACKVARYINGPROC = void(GLAPIENTRY*)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name);
using PFNGLGETUNIFORMBLOCKINDEXPROC = GLuint(GLAPIENTRY*)(GLuint program, const GLchar* uniformBlockName);
using PFNGLGETUNIFORMINDICESPROC = void(GLAPIENTRY*)(GLuint program, GLsizei uniformCount, const GLchar* const* uniformNames, GLuint* uniformIndices);
using PFNGLGETUNIFORMLOCATIONPROC = GLint(GLAPIENTRY*)(GLuint program, const GLchar* name);
using PFNGLGETUNIFORMFVPROC = void(GLAPIENTRY*)(GLuint program, GLint location, GLfloat* params);
using PFNGLGETUNIFORMIVPROC = void(GLAPIENTRY*)(GLuint program, GLint location, GLint* params);
using PFNGLGETUNIFORMUIVPROC = void(GLAPIENTRY*)(GLuint program, GLint location, GLuint* params);
using PFNGLGETVERTEXATTRIBIIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLint* params);
using PFNGLGETVERTEXATTRIBIUIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLuint* params);
using PFNGLGETVERTEXATTRIBPOINTERVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, void** pointer);
using PFNGLGETVERTEXATTRIBDVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLdouble* params);
using PFNGLGETVERTEXATTRIBFVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLfloat* params);
using PFNGLGETVERTEXATTRIBIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum pname, GLint* params);
using PFNGLHINTPROC = void(GLAPIENTRY*)(GLenum target, GLenum mode);
using PFNGLISBUFFERPROC = GLboolean(GLAPIENTRY*)(GLuint buffer);
using PFNGLISENABLEDPROC = GLboolean(GLAPIENTRY*)(GLenum cap);
using PFNGLISENABLEDIPROC = GLboolean(GLAPIENTRY*)(GLenum target, GLuint index);
using PFNGLISFRAMEBUFFERPROC = GLboolean(GLAPIENTRY*)(GLuint framebuffer);
using PFNGLISPROGRAMPROC = GLboolean(GLAPIENTRY*)(GLuint program);
using PFNGLISQUERYPROC = GLboolean(GLAPIENTRY*)(GLuint id);
using PFNGLISRENDERBUFFERPROC = GLboolean(GLAPIENTRY*)(GLuint renderbuffer);
using PFNGLISSAMPLERPROC = GLboolean(GLAPIENTRY*)(GLuint sampler);
using PFNGLISSHADERPROC = GLboolean(GLAPIENTRY*)(GLuint shader);
using PFNGLISSYNCPROC = GLboolean(GLAPIENTRY*)(GLsync sync);
using PFNGLISTEXTUREPROC = GLboolean(GLAPIENTRY*)(GLuint texture);
using PFNGLISVERTEXARRAYPROC = GLboolean(GLAPIENTRY*)(GLuint array);
using PFNGLLINEWIDTHPROC = void(GLAPIENTRY*)(GLfloat width);
using PFNGLLINKPROGRAMPROC = void(GLAPIENTRY*)(GLuint program);
using PFNGLLOGICOPPROC = void(GLAPIENTRY*)(GLenum opcode);
using PFNGLMAPBUFFERPROC = void*(GLAPIENTRY*)(GLenum target, GLenum access);
using PFNGLMAPBUFFERRANGEPROC = void*(GLAPIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
using PFNGLMULTIDRAWARRAYSPROC = void(GLAPIENTRY*)(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount);
using PFNGLMULTIDRAWELEMENTSPROC = void(GLAPIENTRY*)(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount);
using PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC = void(GLAPIENTRY*)(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount,
	const GLint* basevertex);
using PFNGLPIXELSTOREFPROC = void(GLAPIENTRY*)(GLenum pname, GLfloat param);
using PFNGLPIXELSTOREIPROC = void(GLAPIENTRY*)(GLenum pname, GLint param);
using PFNGLPOINTPARAMETERFPROC = void(GLAPIENTRY*)(GLenum pname, GLfloat param);
using PFNGLPOINTPARAMETERFVPROC = void(GLAPIENTRY*)(GLenum pname, const GLfloat* params);
using PFNGLPOINTPARAMETERIPROC = void(GLAPIENTRY*)(GLenum pname, GLint param);
using PFNGLPOINTPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum pname, const GLint* params);
using PFNGLPOINTSIZEPROC = void(GLAPIENTRY*)(GLfloat size);
using PFNGLPOLYGONMODEPROC = void(GLAPIENTRY*)(GLenum face, GLenum mode);
using PFNGLPOLYGONOFFSETPROC = void(GLAPIENTRY*)(GLfloat factor, GLfloat units);
using PFNGLPRIMITIVERESTARTINDEXPROC = void(GLAPIENTRY*)(GLuint index);
using PFNGLPROVOKINGVERTEXPROC = void(GLAPIENTRY*)(GLenum mode);
using PFNGLQUERYCOUNTERPROC = void(GLAPIENTRY*)(GLuint id, GLenum target);
using PFNGLREADBUFFERPROC = void(GLAPIENTRY*)(GLenum src);
using PFNGLREADPIXELSPROC = void(GLAPIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
using PFNGLRENDERBUFFERSTORAGEPROC = void(GLAPIENTRY*)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
using PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC = void(GLAPIENTRY*)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
using PFNGLSAMPLECOVERAGEPROC = void(GLAPIENTRY*)(GLfloat value, GLboolean invert);
using PFNGLSAMPLEMASKIPROC = void(GLAPIENTRY*)(GLuint maskNumber, GLbitfield mask);
using PFNGLSAMPLERPARAMETERIIVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, const GLint* param);
using PFNGLSAMPLERPARAMETERIUIVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, const GLuint* param);
using PFNGLSAMPLERPARAMETERFPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLfloat param);
using PFNGLSAMPLERPARAMETERFVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, const GLfloat* param);
using PFNGLSAMPLERPARAMETERIPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, GLint param);
using PFNGLSAMPLERPARAMETERIVPROC = void(GLAPIENTRY*)(GLuint sampler, GLenum pname, const GLint* param);
using PFNGLSCISSORPROC = void(GLAPIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLSHADERSOURCEPROC = void(GLAPIENTRY*)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
using PFNGLSTENCILFUNCPROC = void(GLAPIENTRY*)(GLenum func, GLint ref, GLuint mask);
using PFNGLSTENCILFUNCSEPARATEPROC = void(GLAPIENTRY*)(GLenum face, GLenum func, GLint ref, GLuint mask);
using PFNGLSTENCILMASKPROC = void(GLAPIENTRY*)(GLuint mask);
using PFNGLSTENCILMASKSEPARATEPROC = void(GLAPIENTRY*)(GLenum face, GLuint mask);
using PFNGLSTENCILOPPROC = void(GLAPIENTRY*)(GLenum fail, GLenum zfail, GLenum zpass);
using PFNGLSTENCILOPSEPARATEPROC = void(GLAPIENTRY*)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
using PFNGLTEXBUFFERPROC = void(GLAPIENTRY*)(GLenum target, GLenum internalformat, GLuint buffer);
using PFNGLTEXIMAGE1DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void* pixels);
using PFNGLTEXIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
	const void* pixels);
using PFNGLTEXIMAGE2DMULTISAMPLEPROC = void(GLAPIENTRY*)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
using PFNGLTEXIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format,
	GLenum type, const void* pixels);
using PFNGLTEXIMAGE3DMULTISAMPLEPROC = void(GLAPIENTRY*)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth,
	GLboolean fixedsamplelocations);
using PFNGLTEXPARAMETERIIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, const GLint* params);
using PFNGLTEXPARAMETERIUIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, const GLuint* params);
using PFNGLTEXPARAMETERFPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLfloat param);
using PFNGLTEXPARAMETERFVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, const GLfloat* params);
using PFNGLTEXPARAMETERIPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, GLint param);
using PFNGLTEXPARAMETERIVPROC = void(GLAPIENTRY*)(GLenum target, GLenum pname, const GLint* params);
using PFNGLTEXSUBIMAGE1DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels);
using PFNGLTEXSUBIMAGE2DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
	const void* pixels);
using PFNGLTEXSUBIMAGE3DPROC = void(GLAPIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
	GLenum format, GLenum type, const void* pixels);
using PFNGLTRANSFORMFEEDBACKVARYINGSPROC = void(GLAPIENTRY*)(GLuint program, GLsizei count, const GLchar* const* varyings, GLenum bufferMode);
using PFNGLUNIFORM1FPROC = void(GLAPIENTRY*)(GLint location, GLfloat v0);
using PFNGLUNIFORM1FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLfloat* value);
using PFNGLUNIFORM1IPROC = void(GLAPIENTRY*)(GLint location, GLint v0);
using PFNGLUNIFORM1IVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLint* value);
using PFNGLUNIFORM1UIPROC = void(GLAPIENTRY*)(GLint location, GLuint v0);
using PFNGLUNIFORM1UIVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLuint* value);
using PFNGLUNIFORM2FPROC = void(GLAPIENTRY*)(GLint location, GLfloat v0, GLfloat v1);
using PFNGLUNIFORM2FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLfloat* value);
using PFNGLUNIFORM2IPROC = void(GLAPIENTRY*)(GLint location, GLint v0, GLint v1);
using PFNGLUNIFORM2IVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLint* value);
using PFNGLUNIFORM2UIPROC = void(GLAPIENTRY*)(GLint location, GLuint v0, GLuint v1);
using PFNGLUNIFORM2UIVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLuint* value);
using PFNGLUNIFORM3FPROC = void(GLAPIENTRY*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
using PFNGLUNIFORM3FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLfloat* value);
using PFNGLUNIFORM3IPROC = void(GLAPIENTRY*)(GLint location, GLint v0, GLint v1, GLint v2);
using PFNGLUNIFORM3IVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLint* value);
using PFNGLUNIFORM3UIPROC = void(GLAPIENTRY*)(GLint location, GLuint v0, GLuint v1, GLuint v2);
using PFNGLUNIFORM3UIVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLuint* value);
using PFNGLUNIFORM4FPROC = void(GLAPIENTRY*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
using PFNGLUNIFORM4FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLfloat* value);
using PFNGLUNIFORM4IPROC = void(GLAPIENTRY*)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
using PFNGLUNIFORM4IVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLint* value);
using PFNGLUNIFORM4UIPROC = void(GLAPIENTRY*)(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
using PFNGLUNIFORM4UIVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, const GLuint* value);
using PFNGLUNIFORMBLOCKBINDINGPROC = void(GLAPIENTRY*)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
using PFNGLUNIFORMMATRIX2FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX2X3FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX2X4FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX3FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX3X2FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX3X4FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX4FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX4X2FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORMMATRIX4X3FVPROC = void(GLAPIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNMAPBUFFERPROC = GLboolean(GLAPIENTRY*)(GLenum target);
using PFNGLUSEPROGRAMPROC = void(GLAPIENTRY*)(GLuint program);
using PFNGLVALIDATEPROGRAMPROC = void(GLAPIENTRY*)(GLuint program);
using PFNGLVERTEXATTRIB1DPROC = void(GLAPIENTRY*)(GLuint index, GLdouble x);
using PFNGLVERTEXATTRIB1DVPROC = void(GLAPIENTRY*)(GLuint index, const GLdouble* v);
using PFNGLVERTEXATTRIB1FPROC = void(GLAPIENTRY*)(GLuint index, GLfloat x);
using PFNGLVERTEXATTRIB1FVPROC = void(GLAPIENTRY*)(GLuint index, const GLfloat* v);
using PFNGLVERTEXATTRIB1SPROC = void(GLAPIENTRY*)(GLuint index, GLshort x);
using PFNGLVERTEXATTRIB1SVPROC = void(GLAPIENTRY*)(GLuint index, const GLshort* v);
using PFNGLVERTEXATTRIB2DPROC = void(GLAPIENTRY*)(GLuint index, GLdouble x, GLdouble y);
using PFNGLVERTEXATTRIB2DVPROC = void(GLAPIENTRY*)(GLuint index, const GLdouble* v);
using PFNGLVERTEXATTRIB2FPROC = void(GLAPIENTRY*)(GLuint index, GLfloat x, GLfloat y);
using PFNGLVERTEXATTRIB2FVPROC = void(GLAPIENTRY*)(GLuint index, const GLfloat* v);
using PFNGLVERTEXATTRIB2SPROC = void(GLAPIENTRY*)(GLuint index, GLshort x, GLshort y);
using PFNGLVERTEXATTRIB2SVPROC = void(GLAPIENTRY*)(GLuint index, const GLshort* v);
using PFNGLVERTEXATTRIB3DPROC = void(GLAPIENTRY*)(GLuint index, GLdouble x, GLdouble y, GLdouble z);
using PFNGLVERTEXATTRIB3DVPROC = void(GLAPIENTRY*)(GLuint index, const GLdouble* v);
using PFNGLVERTEXATTRIB3FPROC = void(GLAPIENTRY*)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
using PFNGLVERTEXATTRIB3FVPROC = void(GLAPIENTRY*)(GLuint index, const GLfloat* v);
using PFNGLVERTEXATTRIB3SPROC = void(GLAPIENTRY*)(GLuint index, GLshort x, GLshort y, GLshort z);
using PFNGLVERTEXATTRIB3SVPROC = void(GLAPIENTRY*)(GLuint index, const GLshort* v);
using PFNGLVERTEXATTRIB4NBVPROC = void(GLAPIENTRY*)(GLuint index, const GLbyte* v);
using PFNGLVERTEXATTRIB4NIVPROC = void(GLAPIENTRY*)(GLuint index, const GLint* v);
using PFNGLVERTEXATTRIB4NSVPROC = void(GLAPIENTRY*)(GLuint index, const GLshort* v);
using PFNGLVERTEXATTRIB4NUBPROC = void(GLAPIENTRY*)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
using PFNGLVERTEXATTRIB4NUBVPROC = void(GLAPIENTRY*)(GLuint index, const GLubyte* v);
using PFNGLVERTEXATTRIB4NUIVPROC = void(GLAPIENTRY*)(GLuint index, const GLuint* v);
using PFNGLVERTEXATTRIB4NUSVPROC = void(GLAPIENTRY*)(GLuint index, const GLushort* v);
using PFNGLVERTEXATTRIB4BVPROC = void(GLAPIENTRY*)(GLuint index, const GLbyte* v);
using PFNGLVERTEXATTRIB4DPROC = void(GLAPIENTRY*)(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
using PFNGLVERTEXATTRIB4DVPROC = void(GLAPIENTRY*)(GLuint index, const GLdouble* v);
using PFNGLVERTEXATTRIB4FPROC = void(GLAPIENTRY*)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
using PFNGLVERTEXATTRIB4FVPROC = void(GLAPIENTRY*)(GLuint index, const GLfloat* v);
using PFNGLVERTEXATTRIB4IVPROC = void(GLAPIENTRY*)(GLuint index, const GLint* v);
using PFNGLVERTEXATTRIB4SPROC = void(GLAPIENTRY*)(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
using PFNGLVERTEXATTRIB4SVPROC = void(GLAPIENTRY*)(GLuint index, const GLshort* v);
using PFNGLVERTEXATTRIB4UBVPROC = void(GLAPIENTRY*)(GLuint index, const GLubyte* v);
using PFNGLVERTEXATTRIB4UIVPROC = void(GLAPIENTRY*)(GLuint index, const GLuint* v);
using PFNGLVERTEXATTRIB4USVPROC = void(GLAPIENTRY*)(GLuint index, const GLushort* v);
using PFNGLVERTEXATTRIBDIVISORPROC = void(GLAPIENTRY*)(GLuint index, GLuint divisor);
using PFNGLVERTEXATTRIBI1IPROC = void(GLAPIENTRY*)(GLuint index, GLint x);
using PFNGLVERTEXATTRIBI1IVPROC = void(GLAPIENTRY*)(GLuint index, const GLint* v);
using PFNGLVERTEXATTRIBI1UIPROC = void(GLAPIENTRY*)(GLuint index, GLuint x);
using PFNGLVERTEXATTRIBI1UIVPROC = void(GLAPIENTRY*)(GLuint index, const GLuint* v);
using PFNGLVERTEXATTRIBI2IPROC = void(GLAPIENTRY*)(GLuint index, GLint x, GLint y);
using PFNGLVERTEXATTRIBI2IVPROC = void(GLAPIENTRY*)(GLuint index, const GLint* v);
using PFNGLVERTEXATTRIBI2UIPROC = void(GLAPIENTRY*)(GLuint index, GLuint x, GLuint y);
using PFNGLVERTEXATTRIBI2UIVPROC = void(GLAPIENTRY*)(GLuint index, const GLuint* v);
using PFNGLVERTEXATTRIBI3IPROC = void(GLAPIENTRY*)(GLuint index, GLint x, GLint y, GLint z);
using PFNGLVERTEXATTRIBI3IVPROC = void(GLAPIENTRY*)(GLuint index, const GLint* v);
using PFNGLVERTEXATTRIBI3UIPROC = void(GLAPIENTRY*)(GLuint index, GLuint x, GLuint y, GLuint z);
using PFNGLVERTEXATTRIBI3UIVPROC = void(GLAPIENTRY*)(GLuint index, const GLuint* v);
using PFNGLVERTEXATTRIBI4BVPROC = void(GLAPIENTRY*)(GLuint index, const GLbyte* v);
using PFNGLVERTEXATTRIBI4IPROC = void(GLAPIENTRY*)(GLuint index, GLint x, GLint y, GLint z, GLint w);
using PFNGLVERTEXATTRIBI4IVPROC = void(GLAPIENTRY*)(GLuint index, const GLint* v);
using PFNGLVERTEXATTRIBI4SVPROC = void(GLAPIENTRY*)(GLuint index, const GLshort* v);
using PFNGLVERTEXATTRIBI4UBVPROC = void(GLAPIENTRY*)(GLuint index, const GLubyte* v);
using PFNGLVERTEXATTRIBI4UIPROC = void(GLAPIENTRY*)(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
using PFNGLVERTEXATTRIBI4UIVPROC = void(GLAPIENTRY*)(GLuint index, const GLuint* v);
using PFNGLVERTEXATTRIBI4USVPROC = void(GLAPIENTRY*)(GLuint index, const GLushort* v);
using PFNGLVERTEXATTRIBIPOINTERPROC = void(GLAPIENTRY*)(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
using PFNGLVERTEXATTRIBP1UIPROC = void(GLAPIENTRY*)(GLuint index, GLenum type, GLboolean normalized, GLuint value);
using PFNGLVERTEXATTRIBP1UIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
using PFNGLVERTEXATTRIBP2UIPROC = void(GLAPIENTRY*)(GLuint index, GLenum type, GLboolean normalized, GLuint value);
using PFNGLVERTEXATTRIBP2UIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
using PFNGLVERTEXATTRIBP3UIPROC = void(GLAPIENTRY*)(GLuint index, GLenum type, GLboolean normalized, GLuint value);
using PFNGLVERTEXATTRIBP3UIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
using PFNGLVERTEXATTRIBP4UIPROC = void(GLAPIENTRY*)(GLuint index, GLenum type, GLboolean normalized, GLuint value);
using PFNGLVERTEXATTRIBP4UIVPROC = void(GLAPIENTRY*)(GLuint index, GLenum type, GLboolean normalized, const GLuint* value);
using PFNGLVERTEXATTRIBPOINTERPROC = void(GLAPIENTRY*)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
using PFNGLVIEWPORTPROC = void(GLAPIENTRY*)(GLint x, GLint y, GLsizei width, GLsizei height);
using PFNGLWAITSYNCPROC = void(GLAPIENTRY*)(GLsync sync, GLbitfield flags, GLuint64 timeout);

extern "C" {
GLAPI PFNGLACTIVETEXTUREPROC glActiveTexture;
GLAPI PFNGLATTACHSHADERPROC glAttachShader;
GLAPI PFNGLBEGINCONDITIONALRENDERPROC glBeginConditionalRender;
GLAPI PFNGLBEGINQUERYPROC glBeginQuery;
GLAPI PFNGLBEGINTRANSFORMFEEDBACKPROC glBeginTransformFeedback;
GLAPI PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
GLAPI PFNGLBINDBUFFERPROC glBindBuffer;
GLAPI PFNGLBINDBUFFERBASEPROC glBindBufferBase;
GLAPI PFNGLBINDBUFFERRANGEPROC glBindBufferRange;
GLAPI PFNGLBINDFRAGDATALOCATIONPROC glBindFragDataLocation;
GLAPI PFNGLBINDFRAGDATALOCATIONINDEXEDPROC glBindFragDataLocationIndexed;
GLAPI PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
GLAPI PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
GLAPI PFNGLBINDSAMPLERPROC glBindSampler;
GLAPI PFNGLBINDTEXTUREPROC glBindTexture;
GLAPI PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
GLAPI PFNGLBLENDCOLORPROC glBlendColor;
GLAPI PFNGLBLENDEQUATIONPROC glBlendEquation;
GLAPI PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
GLAPI PFNGLBLENDFUNCPROC glBlendFunc;
GLAPI PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
GLAPI PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
GLAPI PFNGLBUFFERDATAPROC glBufferData;
GLAPI PFNGLBUFFERSUBDATAPROC glBufferSubData;
GLAPI PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
GLAPI PFNGLCLAMPCOLORPROC glClampColor;
GLAPI PFNGLCLEARPROC glClear;
GLAPI PFNGLCLEARBUFFERFIPROC glClearBufferfi;
GLAPI PFNGLCLEARBUFFERFVPROC glClearBufferfv;
GLAPI PFNGLCLEARBUFFERIVPROC glClearBufferiv;
GLAPI PFNGLCLEARBUFFERUIVPROC glClearBufferuiv;
GLAPI PFNGLCLEARCOLORPROC glClearColor;
GLAPI PFNGLCLEARDEPTHPROC glClearDepth;
GLAPI PFNGLCLEARSTENCILPROC glClearStencil;
GLAPI PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
GLAPI PFNGLCOLORMASKPROC glColorMask;
GLAPI PFNGLCOLORMASKIPROC glColorMaski;
GLAPI PFNGLCOMPILESHADERPROC glCompileShader;
GLAPI PFNGLCOMPRESSEDTEXIMAGE1DPROC glCompressedTexImage1D;
GLAPI PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
GLAPI PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;
GLAPI PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D;
GLAPI PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
GLAPI PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;
GLAPI PFNGLCOPYBUFFERSUBDATAPROC glCopyBufferSubData;
GLAPI PFNGLCOPYTEXIMAGE1DPROC glCopyTexImage1D;
GLAPI PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
GLAPI PFNGLCOPYTEXSUBIMAGE1DPROC glCopyTexSubImage1D;
GLAPI PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
GLAPI PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D;
GLAPI PFNGLCREATEPROGRAMPROC glCreateProgram;
GLAPI PFNGLCREATESHADERPROC glCreateShader;
GLAPI PFNGLCULLFACEPROC glCullFace;
GLAPI PFNGLDELETEBUFFERSPROC glDeleteBuffers;
GLAPI PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
GLAPI PFNGLDELETEPROGRAMPROC glDeleteProgram;
GLAPI PFNGLDELETEQUERIESPROC glDeleteQueries;
GLAPI PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
GLAPI PFNGLDELETESAMPLERSPROC glDeleteSamplers;
GLAPI PFNGLDELETESHADERPROC glDeleteShader;
GLAPI PFNGLDELETESYNCPROC glDeleteSync;
GLAPI PFNGLDELETETEXTURESPROC glDeleteTextures;
GLAPI PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
GLAPI PFNGLDEPTHFUNCPROC glDepthFunc;
GLAPI PFNGLDEPTHMASKPROC glDepthMask;
GLAPI PFNGLDEPTHRANGEPROC glDepthRange;
GLAPI PFNGLDETACHSHADERPROC glDetachShader;
GLAPI PFNGLDISABLEPROC glDisable;
GLAPI PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
GLAPI PFNGLDISABLEIPROC glDisablei;
GLAPI PFNGLDRAWARRAYSPROC glDrawArrays;
GLAPI PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;
GLAPI PFNGLDRAWBUFFERPROC glDrawBuffer;
GLAPI PFNGLDRAWBUFFERSPROC glDrawBuffers;
GLAPI PFNGLDRAWELEMENTSPROC glDrawElements;
GLAPI PFNGLDRAWELEMENTSBASEVERTEXPROC glDrawElementsBaseVertex;
GLAPI PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced;
GLAPI PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glDrawElementsInstancedBaseVertex;
GLAPI PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;
GLAPI PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glDrawRangeElementsBaseVertex;
GLAPI PFNGLENABLEPROC glEnable;
GLAPI PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
GLAPI PFNGLENABLEIPROC glEnablei;
GLAPI PFNGLENDCONDITIONALRENDERPROC glEndConditionalRender;
GLAPI PFNGLENDQUERYPROC glEndQuery;
GLAPI PFNGLENDTRANSFORMFEEDBACKPROC glEndTransformFeedback;
GLAPI PFNGLFENCESYNCPROC glFenceSync;
GLAPI PFNGLFINISHPROC glFinish;
GLAPI PFNGLFLUSHPROC glFlush;
GLAPI PFNGLFLUSHMAPPEDBUFFERRANGEPROC glFlushMappedBufferRange;
GLAPI PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
GLAPI PFNGLFRAMEBUFFERTEXTUREPROC glFramebufferTexture;
GLAPI PFNGLFRAMEBUFFERTEXTURE1DPROC glFramebufferTexture1D;
GLAPI PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
GLAPI PFNGLFRAMEBUFFERTEXTURE3DPROC glFramebufferTexture3D;
GLAPI PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer;
GLAPI PFNGLFRONTFACEPROC glFrontFace;
GLAPI PFNGLGENBUFFERSPROC glGenBuffers;
GLAPI PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
GLAPI PFNGLGENQUERIESPROC glGenQueries;
GLAPI PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
GLAPI PFNGLGENSAMPLERSPROC glGenSamplers;
GLAPI PFNGLGENTEXTURESPROC glGenTextures;
GLAPI PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
GLAPI PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
GLAPI PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;
GLAPI PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;
GLAPI PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glGetActiveUniformBlockName;
GLAPI PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv;
GLAPI PFNGLGETACTIVEUNIFORMNAMEPROC glGetActiveUniformName;
GLAPI PFNGLGETACTIVEUNIFORMSIVPROC glGetActiveUniformsiv;
GLAPI PFNGLGETATTACHEDSHADERSPROC glGetAttachedShaders;
GLAPI PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
GLAPI PFNGLGETBOOLEANI_VPROC glGetBooleani_v;
GLAPI PFNGLGETBOOLEANVPROC glGetBooleanv;
GLAPI PFNGLGETBUFFERPARAMETERI64VPROC glGetBufferParameteri64v;
GLAPI PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
GLAPI PFNGLGETBUFFERPOINTERVPROC glGetBufferPointerv;
GLAPI PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData;
GLAPI PFNGLGETCOMPRESSEDTEXIMAGEPROC glGetCompressedTexImage;
GLAPI PFNGLGETDOUBLEVPROC glGetDoublev;
GLAPI PFNGLGETERRORPROC glGetError;
GLAPI PFNGLGETFLOATVPROC glGetFloatv;
GLAPI PFNGLGETFRAGDATAINDEXPROC glGetFragDataIndex;
GLAPI PFNGLGETFRAGDATALOCATIONPROC glGetFragDataLocation;
GLAPI PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv;
GLAPI PFNGLGETINTEGER64I_VPROC glGetInteger64i_v;
GLAPI PFNGLGETINTEGER64VPROC glGetInteger64v;
GLAPI PFNGLGETINTEGERI_VPROC glGetIntegeri_v;
GLAPI PFNGLGETINTEGERVPROC glGetIntegerv;
GLAPI PFNGLGETMULTISAMPLEFVPROC glGetMultisamplefv;
GLAPI PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
GLAPI PFNGLGETPROGRAMIVPROC glGetProgramiv;
GLAPI PFNGLGETQUERYOBJECTI64VPROC glGetQueryObjecti64v;
GLAPI PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv;
GLAPI PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64v;
GLAPI PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;
GLAPI PFNGLGETQUERYIVPROC glGetQueryiv;
GLAPI PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv;
GLAPI PFNGLGETSAMPLERPARAMETERIIVPROC glGetSamplerParameterIiv;
GLAPI PFNGLGETSAMPLERPARAMETERIUIVPROC glGetSamplerParameterIuiv;
GLAPI PFNGLGETSAMPLERPARAMETERFVPROC glGetSamplerParameterfv;
GLAPI PFNGLGETSAMPLERPARAMETERIVPROC glGetSamplerParameteriv;
GLAPI PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
GLAPI PFNGLGETSHADERSOURCEPROC glGetShaderSource;
GLAPI PFNGLGETSHADERIVPROC glGetShaderiv;
GLAPI PFNGLGETSTRINGPROC glGetString;
GLAPI PFNGLGETSTRINGIPROC glGetStringi;
GLAPI PFNGLGETSYNCIVPROC glGetSynciv;
GLAPI PFNGLGETTEXIMAGEPROC glGetTexImage;
GLAPI PFNGLGETTEXLEVELPARAMETERFVPROC glGetTexLevelParameterfv;
GLAPI PFNGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv;
GLAPI PFNGLGETTEXPARAMETERIIVPROC glGetTexParameterIiv;
GLAPI PFNGLGETTEXPARAMETERIUIVPROC glGetTexParameterIuiv;
GLAPI PFNGLGETTEXPARAMETERFVPROC glGetTexParameterfv;
GLAPI PFNGLGETTEXPARAMETERIVPROC glGetTexParameteriv;
GLAPI PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glGetTransformFeedbackVarying;
GLAPI PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
GLAPI PFNGLGETUNIFORMINDICESPROC glGetUniformIndices;
GLAPI PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
GLAPI PFNGLGETUNIFORMFVPROC glGetUniformfv;
GLAPI PFNGLGETUNIFORMIVPROC glGetUniformiv;
GLAPI PFNGLGETUNIFORMUIVPROC glGetUniformuiv;
GLAPI PFNGLGETVERTEXATTRIBIIVPROC glGetVertexAttribIiv;
GLAPI PFNGLGETVERTEXATTRIBIUIVPROC glGetVertexAttribIuiv;
GLAPI PFNGLGETVERTEXATTRIBPOINTERVPROC glGetVertexAttribPointerv;
GLAPI PFNGLGETVERTEXATTRIBDVPROC glGetVertexAttribdv;
GLAPI PFNGLGETVERTEXATTRIBFVPROC glGetVertexAttribfv;
GLAPI PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;
GLAPI PFNGLHINTPROC glHint;
GLAPI PFNGLISBUFFERPROC glIsBuffer;
GLAPI PFNGLISENABLEDPROC glIsEnabled;
GLAPI PFNGLISENABLEDIPROC glIsEnabledi;
GLAPI PFNGLISFRAMEBUFFERPROC glIsFramebuffer;
GLAPI PFNGLISPROGRAMPROC glIsProgram;
GLAPI PFNGLISQUERYPROC glIsQuery;
GLAPI PFNGLISRENDERBUFFERPROC glIsRenderbuffer;
GLAPI PFNGLISSAMPLERPROC glIsSampler;
GLAPI PFNGLISSHADERPROC glIsShader;
GLAPI PFNGLISSYNCPROC glIsSync;
GLAPI PFNGLISTEXTUREPROC glIsTexture;
GLAPI PFNGLISVERTEXARRAYPROC glIsVertexArray;
GLAPI PFNGLLINEWIDTHPROC glLineWidth;
GLAPI PFNGLLINKPROGRAMPROC glLinkProgram;
GLAPI PFNGLLOGICOPPROC glLogicOp;
GLAPI PFNGLMAPBUFFERPROC glMapBuffer;
GLAPI PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
GLAPI PFNGLMULTIDRAWARRAYSPROC glMultiDrawArrays;
GLAPI PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements;
GLAPI PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC glMultiDrawElementsBaseVertex;
GLAPI PFNGLPIXELSTOREFPROC glPixelStoref;
GLAPI PFNGLPIXELSTOREIPROC glPixelStorei;
GLAPI PFNGLPOINTPARAMETERFPROC glPointParameterf;
GLAPI PFNGLPOINTPARAMETERFVPROC glPointParameterfv;
GLAPI PFNGLPOINTPARAMETERIPROC glPointParameteri;
GLAPI PFNGLPOINTPARAMETERIVPROC glPointParameteriv;
GLAPI PFNGLPOINTSIZEPROC glPointSize;
GLAPI PFNGLPOLYGONMODEPROC glPolygonMode;
GLAPI PFNGLPOLYGONOFFSETPROC glPolygonOffset;
GLAPI PFNGLPRIMITIVERESTARTINDEXPROC glPrimitiveRestartIndex;
GLAPI PFNGLPROVOKINGVERTEXPROC glProvokingVertex;
GLAPI PFNGLQUERYCOUNTERPROC glQueryCounter;
GLAPI PFNGLREADBUFFERPROC glReadBuffer;
GLAPI PFNGLREADPIXELSPROC glReadPixels;
GLAPI PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
GLAPI PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
GLAPI PFNGLSAMPLECOVERAGEPROC glSampleCoverage;
GLAPI PFNGLSAMPLEMASKIPROC glSampleMaski;
GLAPI PFNGLSAMPLERPARAMETERIIVPROC glSamplerParameterIiv;
GLAPI PFNGLSAMPLERPARAMETERIUIVPROC glSamplerParameterIuiv;
GLAPI PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf;
GLAPI PFNGLSAMPLERPARAMETERFVPROC glSamplerParameterfv;
GLAPI PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
GLAPI PFNGLSAMPLERPARAMETERIVPROC glSamplerParameteriv;
GLAPI PFNGLSCISSORPROC glScissor;
GLAPI PFNGLSHADERSOURCEPROC glShaderSource;
GLAPI PFNGLSTENCILFUNCPROC glStencilFunc;
GLAPI PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate;
GLAPI PFNGLSTENCILMASKPROC glStencilMask;
GLAPI PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate;
GLAPI PFNGLSTENCILOPPROC glStencilOp;
GLAPI PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate;
GLAPI PFNGLTEXBUFFERPROC glTexBuffer;
GLAPI PFNGLTEXIMAGE1DPROC glTexImage1D;
GLAPI PFNGLTEXIMAGE2DPROC glTexImage2D;
GLAPI PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample;
GLAPI PFNGLTEXIMAGE3DPROC glTexImage3D;
GLAPI PFNGLTEXIMAGE3DMULTISAMPLEPROC glTexImage3DMultisample;
GLAPI PFNGLTEXPARAMETERIIVPROC glTexParameterIiv;
GLAPI PFNGLTEXPARAMETERIUIVPROC glTexParameterIuiv;
GLAPI PFNGLTEXPARAMETERFPROC glTexParameterf;
GLAPI PFNGLTEXPARAMETERFVPROC glTexParameterfv;
GLAPI PFNGLTEXPARAMETERIPROC glTexParameteri;
GLAPI PFNGLTEXPARAMETERIVPROC glTexParameteriv;
GLAPI PFNGLTEXSUBIMAGE1DPROC glTexSubImage1D;
GLAPI PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
GLAPI PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
GLAPI PFNGLTRANSFORMFEEDBACKVARYINGSPROC glTransformFeedbackVaryings;
GLAPI PFNGLUNIFORM1FPROC glUniform1f;
GLAPI PFNGLUNIFORM1FVPROC glUniform1fv;
GLAPI PFNGLUNIFORM1IPROC glUniform1i;
GLAPI PFNGLUNIFORM1IVPROC glUniform1iv;
GLAPI PFNGLUNIFORM1UIPROC glUniform1ui;
GLAPI PFNGLUNIFORM1UIVPROC glUniform1uiv;
GLAPI PFNGLUNIFORM2FPROC glUniform2f;
GLAPI PFNGLUNIFORM2FVPROC glUniform2fv;
GLAPI PFNGLUNIFORM2IPROC glUniform2i;
GLAPI PFNGLUNIFORM2IVPROC glUniform2iv;
GLAPI PFNGLUNIFORM2UIPROC glUniform2ui;
GLAPI PFNGLUNIFORM2UIVPROC glUniform2uiv;
GLAPI PFNGLUNIFORM3FPROC glUniform3f;
GLAPI PFNGLUNIFORM3FVPROC glUniform3fv;
GLAPI PFNGLUNIFORM3IPROC glUniform3i;
GLAPI PFNGLUNIFORM3IVPROC glUniform3iv;
GLAPI PFNGLUNIFORM3UIPROC glUniform3ui;
GLAPI PFNGLUNIFORM3UIVPROC glUniform3uiv;
GLAPI PFNGLUNIFORM4FPROC glUniform4f;
GLAPI PFNGLUNIFORM4FVPROC glUniform4fv;
GLAPI PFNGLUNIFORM4IPROC glUniform4i;
GLAPI PFNGLUNIFORM4IVPROC glUniform4iv;
GLAPI PFNGLUNIFORM4UIPROC glUniform4ui;
GLAPI PFNGLUNIFORM4UIVPROC glUniform4uiv;
GLAPI PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;
GLAPI PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;
GLAPI PFNGLUNIFORMMATRIX2X3FVPROC glUniformMatrix2x3fv;
GLAPI PFNGLUNIFORMMATRIX2X4FVPROC glUniformMatrix2x4fv;
GLAPI PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
GLAPI PFNGLUNIFORMMATRIX3X2FVPROC glUniformMatrix3x2fv;
GLAPI PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv;
GLAPI PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
GLAPI PFNGLUNIFORMMATRIX4X2FVPROC glUniformMatrix4x2fv;
GLAPI PFNGLUNIFORMMATRIX4X3FVPROC glUniformMatrix4x3fv;
GLAPI PFNGLUNMAPBUFFERPROC glUnmapBuffer;
GLAPI PFNGLUSEPROGRAMPROC glUseProgram;
GLAPI PFNGLVALIDATEPROGRAMPROC glValidateProgram;
GLAPI PFNGLVERTEXATTRIB1DPROC glVertexAttrib1d;
GLAPI PFNGLVERTEXATTRIB1DVPROC glVertexAttrib1dv;
GLAPI PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;
GLAPI PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;
GLAPI PFNGLVERTEXATTRIB1SPROC glVertexAttrib1s;
GLAPI PFNGLVERTEXATTRIB1SVPROC glVertexAttrib1sv;
GLAPI PFNGLVERTEXATTRIB2DPROC glVertexAttrib2d;
GLAPI PFNGLVERTEXATTRIB2DVPROC glVertexAttrib2dv;
GLAPI PFNGLVERTEXATTRIB2FPROC glVertexAttrib2f;
GLAPI PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;
GLAPI PFNGLVERTEXATTRIB2SPROC glVertexAttrib2s;
GLAPI PFNGLVERTEXATTRIB2SVPROC glVertexAttrib2sv;
GLAPI PFNGLVERTEXATTRIB3DPROC glVertexAttrib3d;
GLAPI PFNGLVERTEXATTRIB3DVPROC glVertexAttrib3dv;
GLAPI PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f;
GLAPI PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;
GLAPI PFNGLVERTEXATTRIB3SPROC glVertexAttrib3s;
GLAPI PFNGLVERTEXATTRIB3SVPROC glVertexAttrib3sv;
GLAPI PFNGLVERTEXATTRIB4NBVPROC glVertexAttrib4Nbv;
GLAPI PFNGLVERTEXATTRIB4NIVPROC glVertexAttrib4Niv;
GLAPI PFNGLVERTEXATTRIB4NSVPROC glVertexAttrib4Nsv;
GLAPI PFNGLVERTEXATTRIB4NUBPROC glVertexAttrib4Nub;
GLAPI PFNGLVERTEXATTRIB4NUBVPROC glVertexAttrib4Nubv;
GLAPI PFNGLVERTEXATTRIB4NUIVPROC glVertexAttrib4Nuiv;
GLAPI PFNGLVERTEXATTRIB4NUSVPROC glVertexAttrib4Nusv;
GLAPI PFNGLVERTEXATTRIB4BVPROC glVertexAttrib4bv;
GLAPI PFNGLVERTEXATTRIB4DPROC glVertexAttrib4d;
GLAPI PFNGLVERTEXATTRIB4DVPROC glVertexAttrib4dv;
GLAPI PFNGLVERTEXATTRIB4FPROC glVertexAttrib4f;
GLAPI PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;
GLAPI PFNGLVERTEXATTRIB4IVPROC glVertexAttrib4iv;
GLAPI PFNGLVERTEXATTRIB4SPROC glVertexAttrib4s;
GLAPI PFNGLVERTEXATTRIB4SVPROC glVertexAttrib4sv;
GLAPI PFNGLVERTEXATTRIB4UBVPROC glVertexAttrib4ubv;
GLAPI PFNGLVERTEXATTRIB4UIVPROC glVertexAttrib4uiv;
GLAPI PFNGLVERTEXATTRIB4USVPROC glVertexAttrib4usv;
GLAPI PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;
GLAPI PFNGLVERTEXATTRIBI1IPROC glVertexAttribI1i;
GLAPI PFNGLVERTEXATTRIBI1IVPROC glVertexAttribI1iv;
GLAPI PFNGLVERTEXATTRIBI1UIPROC glVertexAttribI1ui;
GLAPI PFNGLVERTEXATTRIBI1UIVPROC glVertexAttribI1uiv;
GLAPI PFNGLVERTEXATTRIBI2IPROC glVertexAttribI2i;
GLAPI PFNGLVERTEXATTRIBI2IVPROC glVertexAttribI2iv;
GLAPI PFNGLVERTEXATTRIBI2UIPROC glVertexAttribI2ui;
GLAPI PFNGLVERTEXATTRIBI2UIVPROC glVertexAttribI2uiv;
GLAPI PFNGLVERTEXATTRIBI3IPROC glVertexAttribI3i;
GLAPI PFNGLVERTEXATTRIBI3IVPROC glVertexAttribI3iv;
GLAPI PFNGLVERTEXATTRIBI3UIPROC glVertexAttribI3ui;
GLAPI PFNGLVERTEXATTRIBI3UIVPROC glVertexAttribI3uiv;
GLAPI PFNGLVERTEXATTRIBI4BVPROC glVertexAttribI4bv;
GLAPI PFNGLVERTEXATTRIBI4IPROC glVertexAttribI4i;
GLAPI PFNGLVERTEXATTRIBI4IVPROC glVertexAttribI4iv;
GLAPI PFNGLVERTEXATTRIBI4SVPROC glVertexAttribI4sv;
GLAPI PFNGLVERTEXATTRIBI4UBVPROC glVertexAttribI4ubv;
GLAPI PFNGLVERTEXATTRIBI4UIPROC glVertexAttribI4ui;
GLAPI PFNGLVERTEXATTRIBI4UIVPROC glVertexAttribI4uiv;
GLAPI PFNGLVERTEXATTRIBI4USVPROC glVertexAttribI4usv;
GLAPI PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;
GLAPI PFNGLVERTEXATTRIBP1UIPROC glVertexAttribP1ui;
GLAPI PFNGLVERTEXATTRIBP1UIVPROC glVertexAttribP1uiv;
GLAPI PFNGLVERTEXATTRIBP2UIPROC glVertexAttribP2ui;
GLAPI PFNGLVERTEXATTRIBP2UIVPROC glVertexAttribP2uiv;
GLAPI PFNGLVERTEXATTRIBP3UIPROC glVertexAttribP3ui;
GLAPI PFNGLVERTEXATTRIBP3UIVPROC glVertexAttribP3uiv;
GLAPI PFNGLVERTEXATTRIBP4UIPROC glVertexAttribP4ui;
GLAPI PFNGLVERTEXATTRIBP4UIVPROC glVertexAttribP4uiv;
GLAPI PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
GLAPI PFNGLVIEWPORTPROC glViewport;
GLAPI PFNGLWAITSYNCPROC glWaitSync;
}

#endif
#endif

#endif
