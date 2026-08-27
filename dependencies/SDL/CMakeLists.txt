message(STATUS "Configuring SDL...")

set(SDL_AUDIO ${GREM_ENABLE_AUDIO_MODULE})
set(SDL_VIDEO ${GREM_ENABLE_GRAPHICS_MODULE})
set(SDL_GPU OFF)
set(SDL_RENDER OFF)
set(SDL_CAMERA OFF)
set(SDL_JOYSTICK ${GREM_ENABLE_EVENTS_MODULE})
set(SDL_HAPTIC OFF)
set(SDL_HIDAPI OFF)
set(SDL_POWER OFF)
set(SDL_SENSOR OFF)
set(SDL_DIALOG OFF)
set(SDL_TRAY OFF)

set(SDL_INSTALL OFF)
set(SDL_UNINSTALL OFF)
set(SDL_TEST_LIBRARY OFF)
set(SDL_TESTS OFF)
set(SDL_EXAMPLES OFF)

set(SDL_AVX OFF)
set(SDL_AVX2 OFF)
set(SDL_AVX512F OFF)
set(SDL_SSE ${GREM_USE_SSE_INTRINSICS})
set(SDL_SSE2 ${GREM_USE_SSE_INTRINSICS})
set(SDL_SSE3 OFF)
set(SDL_SSE4_1 OFF)
set(SDL_SSE4_2 OFF)
set(SDL_MMX OFF)
set(SDL_ALTIVEC OFF)
set(SDL_ARMNEON OFF)
set(SDL_LSX OFF)
set(SDL_LASX OFF)

set(SDL_LIBC ON)
set(SDL_SYSTEM_ICONV OFF)
set(SDL_DISKAUDIO OFF)
set(SDL_DUMMYAUDIO OFF)
set(SDL_DUMMYVIDEO OFF)
set(SDL_OPENVR OFF)
set(SDL_OFFSCREEN OFF)
set(SDL_DUMMYCAMERA OFF)

set(SDL_EMSCRIPTEN_PERSISTENT_PATH "" CACHE STRING "Path to mount Emscripten IDBFS at startup or '' to disable" FORCE)
set(SDL_VENDOR_INFO "${PROJECT_NAME}-${PROJECT_VERSION}" CACHE STRING "Vendor name and/or version to add to SDL_REVISION" FORCE)

if(GREM_USE_RELEASE_ASSERTIONS)
	set(SDL_ASSERTIONS "enabled")
else()
	set(SDL_ASSERTIONS "auto")
endif()

if(BUILD_SHARED_LIBS)
	set(SDL_SHARED ON)
	set(SDL_STATIC OFF)
else()
	set(SDL_STATIC ON)
	set(SDL_SHARED OFF)
endif()

if(GREM_ENABLE_GRAPHICS_MODULE)
	if(GREM_GRAPHICS_BACKEND STREQUAL "OpenGL")
		if(GREM_GRAPHICS_OPENGL_USE_ES_PROFILE)
			set(SDL_OPENGL OFF)
			set(SDL_OPENGLES ON)
		else()
			set(SDL_OPENGL ON)
			set(SDL_OPENGLES OFF)
		endif()
		set(SDL_VULKAN OFF)
	elseif(GREM_GRAPHICS_BACKEND STREQUAL "Vulkan")
		set(SDL_OPENGL OFF)
		set(SDL_OPENGLES OFF)
		set(SDL_VULKAN ON)
		unset(HAVE_OPENGL CACHE)
		unset(HAVE_OPENGLES_V1 CACHE)
		unset(HAVE_OPENGLES_V2 CACHE)
		unset(HAVE_OPENGL_EGL CACHE)
		unset(HAVE_OPENGL_GLX CACHE)
	endif()
else()
	set(SDL_OPENGL OFF)
	set(SDL_OPENGLES OFF)
	set(SDL_VULKAN OFF)
endif()

add_compile_definitions(SDL_USE_BUILTIN_OPENGL_DEFINITIONS)

FetchContent_Declare(SDL
	GIT_REPOSITORY https://github.com/libsdl-org/SDL
	GIT_TAG 147a8ee32dbf9ac02f3794964490687b6bbda1bc # 3.4.14
	EXCLUDE_FROM_ALL
	SYSTEM)

FetchContent_MakeAvailable(SDL)

file(READ "${sdl_SOURCE_DIR}/LICENSE.txt" GREM_SDL_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_LICENSE_FILE_CONTENTS "${GREM_SDL_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright \\(C\\) 1997-.... Sam Lantinga <slouken@libsdl\\.org>" GREM_SDL_COPYRIGHT_FILE_CONTENTS "${GREM_SDL_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/include/SDL3/SDL_opengl.h" GREM_SDL_MESA_GL_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_MESA_GL_LICENSE_FILE_CONTENTS "${GREM_SDL_MESA_GL_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Mesa 3-D graphics library.*OTHER DEALINGS IN THE SOFTWARE\\.\n" GREM_SDL_MESA_GL_LICENSE_FILE_CONTENTS "${GREM_SDL_MESA_GL_LICENSE_FILE_CONTENTS}")
string(REGEX REPLACE "\n \\* ?" "\n" GREM_SDL_MESA_GL_LICENSE_FILE_CONTENTS "${GREM_SDL_MESA_GL_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/src/events/imKStoUCS.c" GREM_SDL_IMKSTOUCS_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_IMKSTOUCS_LICENSE_FILE_CONTENTS "${GREM_SDL_IMKSTOUCS_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright \\(C\\) 2003-2006,2008 Jamey Sharp, Josh Triplett.*DEALINGS IN THE SOFTWARE\\.\n" GREM_SDL_IMKSTOUCS_LICENSE_FILE_CONTENTS "${GREM_SDL_IMKSTOUCS_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/src/filesystem/unix/SDL_sysfilesystem.c" GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE_FILE_CONTENTS "${GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright \\(c\\) 2007 Red Hat, Inc\\..*SOFTWARE\\.\n" GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE_FILE_CONTENTS "${GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE_FILE_CONTENTS}")
string(REPLACE "\n  " "\n" GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE_FILE_CONTENTS "${GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/src/video/miniz.h" GREM_SDL_MINIZ_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_MINIZ_LICENSE_FILE_CONTENTS "${GREM_SDL_MINIZ_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "This is free and unencumbered software released into the public domain\\..*<http://unlicense\\.org/>\n" GREM_SDL_MINIZ_LICENSE_FILE_CONTENTS "${GREM_SDL_MINIZ_LICENSE_FILE_CONTENTS}")
string(REPLACE "\n  " "\n" GREM_SDL_MINIZ_LICENSE_FILE_CONTENTS "${GREM_SDL_MINIZ_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/src/video/stb_image.h" GREM_SDL_STB_IMAGE_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_STB_IMAGE_LICENSE_FILE_CONTENTS "${GREM_SDL_STB_IMAGE_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "This is free and unencumbered software released into the public domain\\..*DEALINGS IN THE SOFTWARE\\.\n" GREM_SDL_STB_IMAGE_LICENSE_FILE_CONTENTS "${GREM_SDL_STB_IMAGE_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/src/video/SDL_rotate.c" GREM_SDL_ROTOZOOM_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_ROTOZOOM_LICENSE_FILE_CONTENTS "${GREM_SDL_ROTOZOOM_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright \\(C\\) 2001-2011  Andreas Schiffler.*ferzkopp dot net\n" GREM_SDL_ROTOZOOM_LICENSE_FILE_CONTENTS "${GREM_SDL_ROTOZOOM_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/src/video/x11/edid-parse.c" GREM_SDL_EDID_PARSE_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_EDID_PARSE_LICENSE_FILE_CONTENTS "${GREM_SDL_EDID_PARSE_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright 2007 Red Hat, Inc\\..*DEALINGS IN THE SOFTWARE.\n" GREM_SDL_EDID_PARSE_LICENSE_FILE_CONTENTS "${GREM_SDL_EDID_PARSE_LICENSE_FILE_CONTENTS}")
string(REGEX REPLACE "\n \\* ?" "\n" GREM_SDL_EDID_PARSE_LICENSE_FILE_CONTENTS "${GREM_SDL_EDID_PARSE_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/src/video/x11/xsettings-client.c" GREM_SDL_XSETTINGS_CLIENT_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_XSETTINGS_CLIENT_LICENSE_FILE_CONTENTS "${GREM_SDL_XSETTINGS_CLIENT_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright © 2001, 2007 Red Hat, Inc\\..*Owen Taylor, Red Hat, Inc\\.\n" GREM_SDL_XSETTINGS_CLIENT_LICENSE_FILE_CONTENTS "${GREM_SDL_XSETTINGS_CLIENT_LICENSE_FILE_CONTENTS}")
string(REGEX REPLACE "\n \\* ?" "\n" GREM_SDL_XSETTINGS_CLIENT_LICENSE_FILE_CONTENTS "${GREM_SDL_XSETTINGS_CLIENT_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/src/video/yuv2rgb/LICENSE" GREM_SDL_YUV2RGB_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_YUV2RGB_LICENSE_FILE_CONTENTS "${GREM_SDL_YUV2RGB_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/wayland-protocols/wayland.xml" GREM_SDL_WAYLAND_XML_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_WAYLAND_XML_LICENSE_FILE_CONTENTS "${GREM_SDL_WAYLAND_XML_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright © 2008-2011 Kristian Høgsberg.*SOFTWARE\\.\n" GREM_SDL_WAYLAND_XML_LICENSE_FILE_CONTENTS "${GREM_SDL_WAYLAND_XML_LICENSE_FILE_CONTENTS}")
string(REPLACE "\n    " "\n" GREM_SDL_WAYLAND_XML_LICENSE_FILE_CONTENTS "${GREM_SDL_WAYLAND_XML_LICENSE_FILE_CONTENTS}")

file(READ "${sdl_SOURCE_DIR}/wayland-protocols/frog-color-management-v1.xml" GREM_SDL_FROG_PROTOCOLS_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SDL_FROG_PROTOCOLS_LICENSE_FILE_CONTENTS "${GREM_SDL_FROG_PROTOCOLS_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright © 2023 Joshua Ashton for Valve Software.*DEALINGS IN THE SOFTWARE\\.\n" GREM_SDL_FROG_PROTOCOLS_LICENSE_FILE_CONTENTS "${GREM_SDL_FROG_PROTOCOLS_LICENSE_FILE_CONTENTS}")
string(REPLACE "\n    " "\n" GREM_SDL_FROG_PROTOCOLS_LICENSE_FILE_CONTENTS "${GREM_SDL_FROG_PROTOCOLS_LICENSE_FILE_CONTENTS}")

set(GREM_SDL_KHR_MIT_LICENSE_TEXT "\
Permission is hereby granted, free of charge, to any person obtaining a\n\
copy of this software and/or associated documentation files (the\n\
\"Materials\"), to deal in the Materials without restriction, including\n\
without limitation the rights to use, copy, modify, merge, publish,\n\
distribute, sublicense, and/or sell copies of the Materials, and to\n\
permit persons to whom the Materials are furnished to do so, subject to\n\
the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included\n\
in all copies or substantial portions of the Materials.\n\
\n\
THE MATERIALS ARE PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n\
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n\
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.\n\
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY\n\
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,\n\
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE\n\
MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.\n\
")

set(GREM_SDL_KHR_APACHE_2_0_LICENSE_TEXT "\
                                 Apache License\n\
                           Version 2.0, January 2004\n\
                        http://www.apache.org/licenses/\n\
\n\
   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION\n\
\n\
   1. Definitions.\n\
\n\
      \"License\" shall mean the terms and conditions for use, reproduction,\n\
      and distribution as defined by Sections 1 through 9 of this document.\n\
\n\
      \"Licensor\" shall mean the copyright owner or entity authorized by\n\
      the copyright owner that is granting the License.\n\
\n\
      \"Legal Entity\" shall mean the union of the acting entity and all\n\
      other entities that control, are controlled by, or are under common\n\
      control with that entity. For the purposes of this definition,\n\
      \"control\" means (i) the power, direct or indirect, to cause the\n\
      direction or management of such entity, whether by contract or\n\
      otherwise, or (ii) ownership of fifty percent (50%) or more of the\n\
      outstanding shares, or (iii) beneficial ownership of such entity.\n\
\n\
      \"You\" (or \"Your\") shall mean an individual or Legal Entity\n\
      exercising permissions granted by this License.\n\
\n\
      \"Source\" form shall mean the preferred form for making modifications,\n\
      including but not limited to software source code, documentation\n\
      source, and configuration files.\n\
\n\
      \"Object\" form shall mean any form resulting from mechanical\n\
      transformation or translation of a Source form, including but\n\
      not limited to compiled object code, generated documentation,\n\
      and conversions to other media types.\n\
\n\
      \"Work\" shall mean the work of authorship, whether in Source or\n\
      Object form, made available under the License, as indicated by a\n\
      copyright notice that is included in or attached to the work\n\
      (an example is provided in the Appendix below).\n\
\n\
      \"Derivative Works\" shall mean any work, whether in Source or Object\n\
      form, that is based on (or derived from) the Work and for which the\n\
      editorial revisions, annotations, elaborations, or other modifications\n\
      represent, as a whole, an original work of authorship. For the purposes\n\
      of this License, Derivative Works shall not include works that remain\n\
      separable from, or merely link (or bind by name) to the interfaces of,\n\
      the Work and Derivative Works thereof.\n\
\n\
      \"Contribution\" shall mean any work of authorship, including\n\
      the original version of the Work and any modifications or additions\n\
      to that Work or Derivative Works thereof, that is intentionally\n\
      submitted to Licensor for inclusion in the Work by the copyright owner\n\
      or by an individual or Legal Entity authorized to submit on behalf of\n\
      the copyright owner. For the purposes of this definition, \"submitted\"\n\
      means any form of electronic, verbal, or written communication sent\n\
      to the Licensor or its representatives, including but not limited to\n\
      communication on electronic mailing lists, source code control systems,\n\
      and issue tracking systems that are managed by, or on behalf of, the\n\
      Licensor for the purpose of discussing and improving the Work, but\n\
      excluding communication that is conspicuously marked or otherwise\n\
      designated in writing by the copyright owner as \"Not a Contribution.\"\n\
\n\
      \"Contributor\" shall mean Licensor and any individual or Legal Entity\n\
      on behalf of whom a Contribution has been received by Licensor and\n\
      subsequently incorporated within the Work.\n\
\n\
   2. Grant of Copyright License. Subject to the terms and conditions of\n\
      this License, each Contributor hereby grants to You a perpetual,\n\
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable\n\
      copyright license to reproduce, prepare Derivative Works of,\n\
      publicly display, publicly perform, sublicense, and distribute the\n\
      Work and such Derivative Works in Source or Object form.\n\
\n\
   3. Grant of Patent License. Subject to the terms and conditions of\n\
      this License, each Contributor hereby grants to You a perpetual,\n\
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable\n\
      (except as stated in this section) patent license to make, have made,\n\
      use, offer to sell, sell, import, and otherwise transfer the Work,\n\
      where such license applies only to those patent claims licensable\n\
      by such Contributor that are necessarily infringed by their\n\
      Contribution(s) alone or by combination of their Contribution(s)\n\
      with the Work to which such Contribution(s) was submitted. If You\n\
      institute patent litigation against any entity (including a\n\
      cross-claim or counterclaim in a lawsuit) alleging that the Work\n\
      or a Contribution incorporated within the Work constitutes direct\n\
      or contributory patent infringement, then any patent licenses\n\
      granted to You under this License for that Work shall terminate\n\
      as of the date such litigation is filed.\n\
\n\
   4. Redistribution. You may reproduce and distribute copies of the\n\
      Work or Derivative Works thereof in any medium, with or without\n\
      modifications, and in Source or Object form, provided that You\n\
      meet the following conditions:\n\
\n\
      (a) You must give any other recipients of the Work or\n\
          Derivative Works a copy of this License; and\n\
\n\
      (b) You must cause any modified files to carry prominent notices\n\
          stating that You changed the files; and\n\
\n\
      (c) You must retain, in the Source form of any Derivative Works\n\
          that You distribute, all copyright, patent, trademark, and\n\
          attribution notices from the Source form of the Work,\n\
          excluding those notices that do not pertain to any part of\n\
          the Derivative Works; and\n\
\n\
      (d) If the Work includes a \"NOTICE\" text file as part of its\n\
          distribution, then any Derivative Works that You distribute must\n\
          include a readable copy of the attribution notices contained\n\
          within such NOTICE file, excluding those notices that do not\n\
          pertain to any part of the Derivative Works, in at least one\n\
          of the following places: within a NOTICE text file distributed\n\
          as part of the Derivative Works; within the Source form or\n\
          documentation, if provided along with the Derivative Works; or,\n\
          within a display generated by the Derivative Works, if and\n\
          wherever such third-party notices normally appear. The contents\n\
          of the NOTICE file are for informational purposes only and\n\
          do not modify the License. You may add Your own attribution\n\
          notices within Derivative Works that You distribute, alongside\n\
          or as an addendum to the NOTICE text from the Work, provided\n\
          that such additional attribution notices cannot be construed\n\
          as modifying the License.\n\
\n\
      You may add Your own copyright statement to Your modifications and\n\
      may provide additional or different license terms and conditions\n\
      for use, reproduction, or distribution of Your modifications, or\n\
      for any such Derivative Works as a whole, provided Your use,\n\
      reproduction, and distribution of the Work otherwise complies with\n\
      the conditions stated in this License.\n\
\n\
   5. Submission of Contributions. Unless You explicitly state otherwise,\n\
      any Contribution intentionally submitted for inclusion in the Work\n\
      by You to the Licensor shall be under the terms and conditions of\n\
      this License, without any additional terms or conditions.\n\
      Notwithstanding the above, nothing herein shall supersede or modify\n\
      the terms of any separate license agreement you may have executed\n\
      with Licensor regarding such Contributions.\n\
\n\
   6. Trademarks. This License does not grant permission to use the trade\n\
      names, trademarks, service marks, or product names of the Licensor,\n\
      except as required for reasonable and customary use in describing the\n\
      origin of the Work and reproducing the content of the NOTICE file.\n\
\n\
   7. Disclaimer of Warranty. Unless required by applicable law or\n\
      agreed to in writing, Licensor provides the Work (and each\n\
      Contributor provides its Contributions) on an \"AS IS\" BASIS,\n\
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or\n\
      implied, including, without limitation, any warranties or conditions\n\
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A\n\
      PARTICULAR PURPOSE. You are solely responsible for determining the\n\
      appropriateness of using or redistributing the Work and assume any\n\
      risks associated with Your exercise of permissions under this License.\n\
\n\
   8. Limitation of Liability. In no event and under no legal theory,\n\
      whether in tort (including negligence), contract, or otherwise,\n\
      unless required by applicable law (such as deliberate and grossly\n\
      negligent acts) or agreed to in writing, shall any Contributor be\n\
      liable to You for damages, including any direct, indirect, special,\n\
      incidental, or consequential damages of any character arising as a\n\
      result of this License or out of the use or inability to use the\n\
      Work (including but not limited to damages for loss of goodwill,\n\
      work stoppage, computer failure or malfunction, or any and all\n\
      other commercial damages or losses), even if such Contributor\n\
      has been advised of the possibility of such damages.\n\
\n\
   9. Accepting Warranty or Additional Liability. While redistributing\n\
      the Work or Derivative Works thereof, You may choose to offer,\n\
      and charge a fee for, acceptance of support, warranty, indemnity,\n\
      or other liability obligations and/or rights consistent with this\n\
      License. However, in accepting such obligations, You may act only\n\
      on Your own behalf and on Your sole responsibility, not on behalf\n\
      of any other Contributor, and only if You agree to indemnify,\n\
      defend, and hold each Contributor harmless for any liability\n\
      incurred by, or claims asserted against, such Contributor by reason\n\
      of your accepting any such warranty or additional liability.\n\
\n\
   END OF TERMS AND CONDITIONS\n\
\n\
   APPENDIX: How to apply the Apache License to your work.\n\
\n\
      To apply the Apache License to your work, attach the following\n\
      boilerplate notice, with the fields enclosed by brackets \"[]\"\n\
      replaced with your own identifying information. (Don't include\n\
      the brackets!)  The text should be enclosed in the appropriate\n\
      comment syntax for the file format. We also recommend that a\n\
      file or class name and description of purpose be included on the\n\
      same \"printed page\" as the copyright notice for easier\n\
      identification within third-party archives.\n\
\n\
   Copyright [yyyy] [name of copyright owner]\n\
\n\
   Licensed under the Apache License, Version 2.0 (the \"License\");\n\
   you may not use this file except in compliance with the License.\n\
   You may obtain a copy of the License at\n\
\n\
       http://www.apache.org/licenses/LICENSE-2.0\n\
\n\
   Unless required by applicable law or agreed to in writing, software\n\
   distributed under the License is distributed on an \"AS IS\" BASIS,\n\
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n\
   See the License for the specific language governing permissions and\n\
   limitations under the License.\n\
")

set(GREM_SDL_TITLE "SDL (3.4.14)" PARENT_SCOPE)
set(GREM_SDL_USE "Used by GREM." PARENT_SCOPE)
set(GREM_SDL_LINK "https://github.com/libsdl-org/SDL/tree/147a8ee32dbf9ac02f3794964490687b6bbda1bc" PARENT_SCOPE)
set(GREM_SDL_COPYRIGHT "${GREM_SDL_COPYRIGHT_FILE_CONTENTS}" PARENT_SCOPE)
set(GREM_SDL_LICENSE "${GREM_SDL_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_KHR_PLATFORM_TITLE "Khronos platform-specific types and definitions" PARENT_SCOPE)
set(GREM_SDL_KHR_PLATFORM_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_KHR_PLATFORM_LINK "https://github.com/KhronosGroup/EGL-Registry/blob/main/api/KHR/khrplatform.h" PARENT_SCOPE)
set(GREM_SDL_KHR_PLATFORM_COPYRIGHT "Copyright (c) 2008-2018 The Khronos Group Inc." PARENT_SCOPE)
set(GREM_SDL_KHR_PLATFORM_LICENSE "${GREM_SDL_KHR_MIT_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_EGL_PLATFORM_TITLE "Platform-specific types and definitions for egl.h" PARENT_SCOPE)
set(GREM_SDL_EGL_PLATFORM_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_EGL_PLATFORM_LINK "https://github.com/KhronosGroup/EGL-Registry/blob/main/api/EGL/eglplatform.h" PARENT_SCOPE)
set(GREM_SDL_EGL_PLATFORM_COPYRIGHT "Copyright 2007-2020 The Khronos Group Inc." PARENT_SCOPE)
set(GREM_SDL_EGL_PLATFORM_LICENSE "${GREM_SDL_KHR_APACHE_2_0_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_EGL_TITLE "egl.h and eglext.h" PARENT_SCOPE)
set(GREM_SDL_EGL_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_EGL_LINKS "https://github.com/KhronosGroup/EGL-Registry/blob/main/api/EGL/egl.h\n\nhttps://github.com/KhronosGroup/EGL-Registry/blob/main/api/EGL/eglext.h" PARENT_SCOPE)
set(GREM_SDL_EGL_COPYRIGHT "Copyright 2013-2020 The Khronos Group Inc." PARENT_SCOPE)
set(GREM_SDL_EGL_LICENSE "${GREM_SDL_KHR_APACHE_2_0_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_MESA_GL_TITLE "Mesa's gl.h" PARENT_SCOPE)
set(GREM_SDL_MESA_GL_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_MESA_GL_LINK "https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/include/GL/gl.h" PARENT_SCOPE)
set(GREM_SDL_MESA_GL_COPYRIGHT "\
Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.\n\
Copyright (C) 2009  VMware, Inc.  All Rights Reserved." PARENT_SCOPE)
set(GREM_SDL_MESA_GL_LICENSE "${GREM_SDL_MESA_GL_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_GLEXT_TITLE "glext.h" PARENT_SCOPE)
set(GREM_SDL_GLEXT_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_GLEXT_LINK "https://github.com/KhronosGroup/OpenGL-Registry/blob/main/api/GL/glext.h" PARENT_SCOPE)
set(GREM_SDL_GLEXT_COPYRIGHT "Copyright 2013-2020 The Khronos Group Inc." PARENT_SCOPE)
set(GREM_SDL_GLEXT_LICENSE "${GREM_SDL_KHR_MIT_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_GL2_PLATFORM_TITLE "Platform-specific types and definitions for OpenGL ES 2.X" PARENT_SCOPE)
set(GREM_SDL_GL2_PLATFORM_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_GL2_PLATFORM_LINK "https://github.com/KhronosGroup/OpenGL-Registry/blob/main/api/GLES2/gl2platform.h" PARENT_SCOPE)
set(GREM_SDL_GL2_PLATFORM_COPYRIGHT "Copyright 2017-2020 The Khronos Group Inc." PARENT_SCOPE)
set(GREM_SDL_GL2_PLATFORM_LICENSE "${GREM_SDL_KHR_APACHE_2_0_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_GL2_TITLE "gl2.h and gl2ext.h" PARENT_SCOPE)
set(GREM_SDL_GL2_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_GL2_LINKS "https://github.com/KhronosGroup/OpenGL-Registry/blob/main/api/GLES2/gl2.h\n\nhttps://github.com/KhronosGroup/OpenGL-Registry/blob/main/api/GLES2/gl2ext.h" PARENT_SCOPE)
set(GREM_SDL_GL2_COPYRIGHT "Copyright 2013-2020 The Khronos Group Inc." PARENT_SCOPE)
set(GREM_SDL_GL2_LICENSE "${GREM_SDL_KHR_MIT_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_GL3_PLATFORM_TITLE "Platform-specific types and definitions for OpenGL ES 3.X" PARENT_SCOPE)
set(GREM_SDL_GL3_PLATFORM_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_GL3_PLATFORM_LINK "https://github.com/KhronosGroup/OpenGL-Registry/blob/main/api/GLES3/gl3platform.h" PARENT_SCOPE)
set(GREM_SDL_GL3_PLATFORM_COPYRIGHT "Copyright 2017-2020 The Khronos Group Inc." PARENT_SCOPE)
set(GREM_SDL_GL3_PLATFORM_LICENSE "${GREM_SDL_KHR_APACHE_2_0_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_GL3_TITLE "gl3.h, gl31.h and gl32.h" PARENT_SCOPE)
set(GREM_SDL_GL3_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_GL3_LINKS "https://github.com/KhronosGroup/OpenGL-Registry/blob/main/api/GLES3/gl3.h\n\nhttps://github.com/KhronosGroup/OpenGL-Registry/blob/main/api/GLES3/gl31.h\n\nhttps://github.com/KhronosGroup/OpenGL-Registry/blob/main/api/GLES3/gl32.h" PARENT_SCOPE)
set(GREM_SDL_GL3_COPYRIGHT "Copyright 2013-2020 The Khronos Group Inc." PARENT_SCOPE)
set(GREM_SDL_GL3_LICENSE "${GREM_SDL_KHR_MIT_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_VULKAN_HEADERS_TITLE "Vulkan-Headers" PARENT_SCOPE)
set(GREM_SDL_VULKAN_HEADERS_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_VULKAN_HEADERS_LINK "https://github.com/KhronosGroup/Vulkan-Headers" PARENT_SCOPE)
set(GREM_SDL_VULKAN_HEADERS_COPYRIGHT "\
Copyright 2015-2024 The Khronos Group Inc.\n\
\n\
Copyright 2015-2023 The Khronos Group Inc.\n\
Copyright 2015-2023 Valve Corporation\n\
Copyright 2015-2023 LunarG, Inc.\n\
\n\
Copyright 2014-2024 The Khronos Group Inc.\
" PARENT_SCOPE)
set(GREM_SDL_VULKAN_HEADERS_LICENSE "${GREM_SDL_KHR_APACHE_2_0_LICENSE_TEXT}" PARENT_SCOPE)

set(GREM_SDL_DIRECTX_HEADERS_TITLE "DirectX Headers" PARENT_SCOPE)
set(GREM_SDL_DIRECTX_HEADERS_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_DIRECTX_HEADERS_LINK "https://github.com/microsoft/DirectX-Headers" PARENT_SCOPE)
set(GREM_SDL_DIRECTX_HEADERS_COPYRIGHT "Copyright (c) Microsoft Corporation" PARENT_SCOPE)
set(GREM_SDL_DIRECTX_HEADERS_LICENSE "\
MIT License\n\
\n\
Permission is hereby granted, free of charge, to any person obtaining a copy\n\
of this software and associated documentation files (the \"Software\"), to deal\n\
in the Software without restriction, including without limitation the rights\n\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
copies of the Software, and to permit persons to whom the Software is\n\
furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included in all\n\
copies or substantial portions of the Software.\n\
\n\
THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n\
SOFTWARE.\n\
" PARENT_SCOPE)

set(GREM_SDL_IMKSTOUCS_TITLE "XFree86's imKStoUCS" PARENT_SCOPE)
set(GREM_SDL_IMKSTOUCS_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_IMKSTOUCS_LINK "https://www.xfree86.org/" PARENT_SCOPE)
set(GREM_SDL_IMKSTOUCS_COPYRIGHT "\
Copyright (C) 2003-2006,2008 Jamey Sharp, Josh Triplett\n\
Copyright © 2009 Red Hat, Inc.\n\
Copyright 1990-1992,1999,2000,2004,2009,2010 Oracle and/or its affiliates.\
" PARENT_SCOPE)
set(GREM_SDL_IMKSTOUCS_LICENSE "${GREM_SDL_IMKSTOUCS_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_XDG_USER_DIR_LOOKUP_TITLE "xdg-user-dirs's xdg_user_dir_lookup.c" PARENT_SCOPE)
set(GREM_SDL_XDG_USER_DIR_LOOKUP_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_XDG_USER_DIR_LOOKUP_LINK "https://gitlab.freedesktop.org/xdg/xdg-user-dirs/-/blob/master/xdg-user-dir-lookup.c" PARENT_SCOPE)
set(GREM_SDL_XDG_USER_DIR_LOOKUP_COPYRIGHT "Copyright (c) 2007 Red Hat, Inc." PARENT_SCOPE)
set(GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE "${GREM_SDL_XDG_USER_DIR_LOOKUP_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_MINIZ_TITLE "miniz (v1.15 r4)" PARENT_SCOPE)
set(GREM_SDL_MINIZ_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_MINIZ_LINK "https://github.com/richgel999/miniz/tree/28f5066e332590c8a68fa4870e89233e72ce7a44" PARENT_SCOPE)
set(GREM_SDL_MINIZ_CREDITS "Rich Geldreich" PARENT_SCOPE)
set(GREM_SDL_MINIZ_LICENSE "${GREM_SDL_MINIZ_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_STB_IMAGE_TITLE "stb_image (v2.30)" PARENT_SCOPE)
set(GREM_SDL_STB_IMAGE_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_STB_IMAGE_LINK "https://github.com/nothings/stb/" PARENT_SCOPE)
set(GREM_SDL_STB_IMAGE_CREDITS "Sean Barrett" PARENT_SCOPE)
set(GREM_SDL_STB_IMAGE_LICENSE "${GREM_SDL_STB_IMAGE_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_ROTOZOOM_TITLE "SDL_gfx's SDL_rotozoom.c (2.0.23)" PARENT_SCOPE)
set(GREM_SDL_ROTOZOOM_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_ROTOZOOM_LINK "https://sourceforge.net/p/sdlgfx/code/65/tree/SDL_rotozoom.c" PARENT_SCOPE)
set(GREM_SDL_ROTOZOOM_COPYRIGHT "Copyright (C) 2001-2011  Andreas Schiffler" PARENT_SCOPE)
set(GREM_SDL_ROTOZOOM_LICENSE "${GREM_SDL_ROTOZOOM_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_EDID_PARSE_TITLE "libgnome-desktop's edid.h and edid-parse.c" PARENT_SCOPE)
set(GREM_SDL_EDID_PARSE_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_EDID_PARSE_LINKS "https://gitlab.gnome.org/GNOME/gnome-desktop/-/blob/1d4cd46c2aa21ce96640b4ee19f314142f200b84/libgnome-desktop/edid.h\n\nhttps://gitlab.gnome.org/GNOME/gnome-desktop/-/blob/1d4cd46c2aa21ce96640b4ee19f314142f200b84/libgnome-desktop/edid-parse.c" PARENT_SCOPE)
set(GREM_SDL_EDID_PARSE_COPYRIGHT "Copyright 2007 Red Hat, Inc." PARENT_SCOPE)
set(GREM_SDL_EDID_PARSE_LICENSE "${GREM_SDL_EDID_PARSE_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_XSETTINGS_CLIENT_TITLE "gtk's xsettings-client.h and xsettings-client.c" PARENT_SCOPE)
set(GREM_SDL_XSETTINGS_CLIENT_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_XSETTINGS_CLIENT_LINKS "https://gitlab.gnome.org/GNOME/gtk/-/blob/3b95bc271da8416b7a8754e8d95bf0cfc498a4d0/gdk/x11/xsettings-client.h\n\nhttps://gitlab.gnome.org/GNOME/gtk/-/blob/3b95bc271da8416b7a8754e8d95bf0cfc498a4d0/gdk/x11/xsettings-client.c" PARENT_SCOPE)
set(GREM_SDL_XSETTINGS_CLIENT_COPYRIGHT "\
Copyright © 2001, 2007 Red Hat, Inc.\n\
Copyright 2024 Igalia S.L.\
" PARENT_SCOPE)
set(GREM_SDL_XSETTINGS_CLIENT_LICENSE "${GREM_SDL_XSETTINGS_CLIENT_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_YUV2RGB_TITLE "yuv2rgb" PARENT_SCOPE)
set(GREM_SDL_YUV2RGB_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_YUV2RGB_LINK "https://github.com/descampsa/yuv2rgb" PARENT_SCOPE)
set(GREM_SDL_YUV2RGB_COPYRIGHT "Copyright (c) 2016, Adrien Descamps" PARENT_SCOPE)
set(GREM_SDL_YUV2RGB_LICENSE "${GREM_SDL_YUV2RGB_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_WAYLAND_XML_TITLE "wayland.xml" PARENT_SCOPE)
set(GREM_SDL_WAYLAND_XML_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_WAYLAND_XML_LINK "https://gitlab.freedesktop.org/wayland/wayland/-/blob/7e47803e43282d7c26751d8f4cd98aa4868da67b/protocol/wayland.xml" PARENT_SCOPE)
set(GREM_SDL_WAYLAND_XML_COPYRIGHT "\
Copyright © 2008-2011 Kristian Høgsberg\n\
Copyright © 2010-2011 Intel Corporation\n\
Copyright © 2012-2013 Collabora, Ltd.\
" PARENT_SCOPE)
set(GREM_SDL_WAYLAND_XML_LICENSE "${GREM_SDL_WAYLAND_XML_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SDL_WAYLAND_PROTOCOLS_TITLE "wayland-protocols" PARENT_SCOPE)
set(GREM_SDL_WAYLAND_PROTOCOLS_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_WAYLAND_PROTOCOLS_LINK "https://gitlab.freedesktop.org/wayland/wayland-protocols" PARENT_SCOPE)
set(GREM_SDL_WAYLAND_PROTOCOLS_COPYRIGHT "\
Copyright © 2008-2013 Kristian Høgsberg\n\
Copyright © 2010-2013 Intel Corporation\n\
Copyright © 2013      Rafael Antognolli\n\
Copyright © 2013      Jasper St. Pierre\n\
Copyright © 2014      Jonas Ådahl\n\
Copyright © 2014      Jason Ekstrand\n\
Copyright © 2014-2015 Collabora, Ltd.\n\
Copyright © 2015      Red Hat Inc.\n\
\n\
Copyright © 2012, 2013 Intel Corporation\n\
Copyright © 2013-2016 Collabora, Ltd.\n\
Copyright 2014 © Stephen \"Lyude\" Chandler Paul\n\
Copyright © 2015 Samsung Electronics Co., Ltd\n\
Copyright © 2015-2017 Samsung Electronics Co., Ltd\n\
Copyright © 2015, 2016 Red Hat\n\
Copyright 2015-2016 © Red Hat, Inc.\n\
Copyright © 2015-2016 Red Hat Inc.\n\
Copyright © 2015-2017 Red Hat Inc.\n\
Copyright © 2015, 2016 Jan Arne Petersen\n\
Copyright © 2017 Collabora, Ltd.\n\
Copyright © 2017 Red Hat Inc.\n\
Copyright © 2017, 2018 Red Hat, Inc.\n\
Copyright © 2018       Purism SPC\n\
Copyright © 2018 Simon Ser\n\
Copyright 2018 The Chromium Authors\n\
Copyright 2019 Erwin Burema\n\
Copyright 2019 Sebastian Wick\n\
Copyright © 2020 Aleix Pol Gonzalez <aleixpol@kde.org>\n\
Copyright © 2020 Carlos Garnacho <carlosg@gnome.org>\n\
Copyright 2020 AMD\n\
Copyright 2020-2024 Collabora, Ltd.\n\
Copyright © 2022 Kenny Levinsen\n\
Copyright 2022-2025 Red Hat, Inc.\n\
Copyright © 2023 Carlos Garnacho\n\
Copyright 2023 Simon Ser\n\
Copyright © 2023-2024 Matthias Klumpp\n\
Copyright ©      2024 David Edmundson\n\
Copyright © 2024 Matthias Klumpp\n\
Copyright © 2024 Neal Gompa\n\
Copyright © 2024 Vlad Zahorodnii\n\
Copyright © 2024 Xaver Hugl\
" PARENT_SCOPE)
set(GREM_SDL_WAYLAND_PROTOCOLS_LICENSE "\
Permission is hereby granted, free of charge, to any person obtaining a\n\
copy of this software and associated documentation files (the \"Software\"),\n\
to deal in the Software without restriction, including without limitation\n\
the rights to use, copy, modify, merge, publish, distribute, sublicense,\n\
and/or sell copies of the Software, and to permit persons to whom the\n\
Software is furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice (including the next\n\
paragraph) shall be included in all copies or substantial portions of the\n\
Software.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL\n\
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n\
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER\n\
DEALINGS IN THE SOFTWARE.\n\
" PARENT_SCOPE)

set(GREM_SDL_FROG_PROTOCOLS_TITLE "frog-protocols" PARENT_SCOPE)
set(GREM_SDL_FROG_PROTOCOLS_USE "Used by SDL." PARENT_SCOPE)
set(GREM_SDL_FROG_PROTOCOLS_LINK "https://github.com/misyltoad/frog-protocols" PARENT_SCOPE)
set(GREM_SDL_FROG_PROTOCOLS_COPYRIGHT "\
Copyright © 2023 Joshua Ashton for Valve Software\n\
Copyright © 2023 Xaver Hugl\
" PARENT_SCOPE)
set(GREM_SDL_FROG_PROTOCOLS_LICENSE "${GREM_SDL_FROG_PROTOCOLS_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_SDL
	GREM_SDL_KHR_PLATFORM
	GREM_SDL_EGL_PLATFORM
	GREM_SDL_EGL
	GREM_SDL_MESA_GL
	GREM_SDL_GLEXT
	GREM_SDL_GL2_PLATFORM
	GREM_SDL_GL2
	GREM_SDL_GL3_PLATFORM
	GREM_SDL_GL3
	GREM_SDL_VULKAN_HEADERS
	GREM_SDL_DIRECTX_HEADERS
	GREM_SDL_IMKSTOUCS
	GREM_SDL_XDG_USER_DIR_LOOKUP
	GREM_SDL_MINIZ
	GREM_SDL_STB_IMAGE
	GREM_SDL_ROTOZOOM
	GREM_SDL_EDID_PARSE
	GREM_SDL_XSETTINGS_CLIENT
	GREM_SDL_YUV2RGB
	GREM_SDL_WAYLAND_XML
	GREM_SDL_WAYLAND_PROTOCOLS
	GREM_SDL_FROG_PROTOCOLS
	PARENT_SCOPE)

if(BUILD_SHARED_LIBS)
	target_compile_options(SDL3-shared PRIVATE
		$<$<AND:$<C_COMPILER_ID:GNU>,$<COMPILE_LANGUAGE:C>>:        -Wno-discarded-qualifiers>
		$<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:    -Wno-incompatible-pointer-types-discards-qualifiers -Wno-unused-but-set-global -Wno-unknown-warning-option>)

	set(GREM_AUDIO_PRIVATE_DEPENDENCIES ${GREM_AUDIO_PRIVATE_DEPENDENCIES} SDL3-shared PARENT_SCOPE)
	set(GREM_EVENTS_PRIVATE_DEPENDENCIES ${GREM_EVENTS_PRIVATE_DEPENDENCIES} SDL3-shared PARENT_SCOPE)
	set(GREM_GRAPHICS_PRIVATE_DEPENDENCIES ${GREM_GRAPHICS_PRIVATE_DEPENDENCIES} SDL3-shared PARENT_SCOPE)
else()
	target_compile_options(SDL3-static PRIVATE
		$<$<AND:$<C_COMPILER_ID:GNU>,$<COMPILE_LANGUAGE:C>>:        -Wno-discarded-qualifiers>
		$<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:    -Wno-incompatible-pointer-types-discards-qualifiers -Wno-unused-but-set-global -Wno-unknown-warning-option>)

	set(GREM_AUDIO_PRIVATE_DEPENDENCIES ${GREM_AUDIO_PRIVATE_DEPENDENCIES} SDL3-static PARENT_SCOPE)
	set(GREM_EVENTS_PRIVATE_DEPENDENCIES ${GREM_EVENTS_PRIVATE_DEPENDENCIES} SDL3-static PARENT_SCOPE)
	set(GREM_GRAPHICS_PRIVATE_DEPENDENCIES ${GREM_GRAPHICS_PRIVATE_DEPENDENCIES} SDL3-static PARENT_SCOPE)
endif()
