#ifndef GREM_GRAPHICS_2D_OPENGL_BUILTIN_SHADERS_GRAPHICS_2D_GENERATED_HPP
#define GREM_GRAPHICS_2D_OPENGL_BUILTIN_SHADERS_GRAPHICS_2D_GENERATED_HPP

/**
 * NOTE: This file is generated from the code in GREM's `shaders/` directory.
 *
 * To regenerate it, run this CMake workflow preset:
 * ```
 * cmake --workflow --preset bootstrap-builtin-shaders
 * ```
 */

#include <GREM/build_config.hpp>

#include <GREM/core/data/CStringView.hpp>

namespace grem::graphics {

namespace detail {

inline constexpr CStringView RENDERER_2D_DEFAULT_MODEL_2D_VERTEX_SHADER_CODE = R"GLSL(void main(){fragmentTextureCoordinates=instanceTextureOffset+instanceTextureBasis*vertexTextureCoordinates;fragmentTintColor=instanceTintColor;fragmentEmissiveColor=instanceEmissiveColor;gl_Position=vec4((cameraViewProjectionMatrix*vec3(instancePosition+instanceBasis*vertexPosition,1.0)).xy,0.0,1.0);})GLSL";
inline constexpr CStringView RENDERER_2D_PLAIN_MODEL_2D_FRAGMENT_SHADER_CODE = R"GLSL(void main(){vec4 mainTextureColor=GREM_textureSample2D(mainTexture,fragmentTextureCoordinates);float alpha=fragmentTintColor.a*mainTextureColor.a;outputColor=vec4(fragmentEmissiveColor*alpha+fragmentTintColor.rgb*mainTextureColor.rgb,alpha);})GLSL";
inline constexpr CStringView RENDERER_2D_TEXT_MODEL_2D_FRAGMENT_SHADER_CODE = R"GLSL(vec4 GREM_convertStraightToPremultipliedAlpha(vec4 rgba){return vec4(rgba.rgb*rgba.a,rgba.a);}vec4 GREM_convertPremultipliedToStraightAlpha(vec4 rgba){return vec4((rgba.a>0.0001)?rgba.rgb/rgba.a:vec3(0.0),rgba.a);}vec4 GREM_blendAOverB(vec4 a,vec4 b){float alpha=a.a+b.a*(1.0-a.a);return vec4((abs(alpha)<0.0001)?b.rgb:(a.rgb*a.a+b.rgb*b.a*(1.0-a.a))/alpha,alpha);}void main(){float mainTextureValue=GREM_textureSample2D(mainTexture,fragmentTextureCoordinates).r;outputColor=GREM_convertStraightToPremultipliedAlpha(vec4(fragmentEmissiveColor+fragmentTintColor.rgb,fragmentTintColor.a*mainTextureValue));})GLSL";
inline constexpr CStringView RENDERER_2D_TONEMAPPING_MODEL_2D_FRAGMENT_SHADER_CODE = R"GLSL(vec3 GREM_tonemap(vec3 color){const float F90=0.04;const float COMPRESSION_THRESHOLD=0.8-F90;const float DESATURATION_SPEED=0.15;float x=min(color.r,min(color.g,color.b));float offset=(x<=2.0*F90)?x-x*x*(1.0/(4.0*F90)):F90;color-=offset;float peak=max(color.r,max(color.g,color.b));if(peak<COMPRESSION_THRESHOLD){return color;}const float d=1.0-COMPRESSION_THRESHOLD;float newPeak=1.0-d*d/(peak+d-COMPRESSION_THRESHOLD);color*=newPeak/peak;float g=1.0-1.0/(DESATURATION_SPEED*(peak-newPeak)+1.0);return mix(color,newPeak*vec3(1.0),g);}void main(){vec4 mainTextureColor=GREM_textureSample2D(mainTexture,fragmentTextureCoordinates);float alpha=fragmentTintColor.a*mainTextureColor.a;outputColor=vec4(GREM_tonemap(fragmentEmissiveColor*alpha+fragmentTintColor.rgb*mainTextureColor.rgb),alpha);})GLSL";

} // namespace detail

} // namespace grem::graphics

#endif
