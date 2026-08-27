#ifndef GREM_IMGUI_OPENGL_BUILTIN_SHADERS_IMGUI_GENERATED_HPP
#define GREM_IMGUI_OPENGL_BUILTIN_SHADERS_IMGUI_GENERATED_HPP

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

inline constexpr CStringView GUI_DEFAULT_VERTEX_SHADER_CODE = R"GLSL(vec4 GREM_convertStraightToPremultipliedAlpha(vec4 rgba){return vec4(rgba.rgb*rgba.a,rgba.a);}vec4 GREM_convertPremultipliedToStraightAlpha(vec4 rgba){return vec4((rgba.a>0.0001)?rgba.rgb/rgba.a:vec3(0.0),rgba.a);}vec4 GREM_blendAOverB(vec4 a,vec4 b){float alpha=a.a+b.a*(1.0-a.a);return vec4((abs(alpha)<0.0001)?b.rgb:(a.rgb*a.a+b.rgb*b.a*(1.0-a.a))/alpha,alpha);}float GREM_convertLinearToSRGB(float x){return(x<=0.0031308)?x*12.92:1.055*pow(x,1.0/2.4)-0.055;}vec3 GREM_convertLinearToSRGB(vec3 rgb){return vec3(GREM_convertLinearToSRGB(rgb.r),GREM_convertLinearToSRGB(rgb.g),GREM_convertLinearToSRGB(rgb.b));}vec4 GREM_convertLinearToSRGB(vec4 rgba){return vec4(GREM_convertLinearToSRGB(rgba.r),GREM_convertLinearToSRGB(rgba.g),GREM_convertLinearToSRGB(rgba.b),rgba.a);}float GREM_convertSRGBToLinear(float x){return(x<=0.04045)?x/12.92:pow((x+0.055)/1.055,2.4);}vec3 GREM_convertSRGBToLinear(vec3 rgb){return vec3(GREM_convertSRGBToLinear(rgb.r),GREM_convertSRGBToLinear(rgb.g),GREM_convertSRGBToLinear(rgb.b));}vec4 GREM_convertSRGBToLinear(vec4 rgba){return vec4(GREM_convertSRGBToLinear(rgba.r),GREM_convertSRGBToLinear(rgba.g),GREM_convertSRGBToLinear(rgba.b),rgba.a);}void main(){fragmentTextureCoordinates=vec2(vertexTextureCoordinates.x,1.0-vertexTextureCoordinates.y);fragmentColor=GREM_convertPremultipliedToStraightAlpha(GREM_convertSRGBToLinear(GREM_convertStraightToPremultipliedAlpha(vertexColor)));gl_Position=vec4(guiOffset+vertexPosition*guiScale,0.0,1.0);gl_Position.y=-gl_Position.y;})GLSL";
inline constexpr CStringView GUI_PLAIN_FRAGMENT_SHADER_CODE = R"GLSL(void main(){outputColor=fragmentColor*GREM_textureSample2D(mainTexture,fragmentTextureCoordinates);})GLSL";

} // namespace detail

} // namespace grem::graphics

#endif
