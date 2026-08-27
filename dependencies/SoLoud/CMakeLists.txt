message(STATUS "Configuring SoLoud...")

FetchContent_Declare(SoLoud
	GIT_REPOSITORY https://github.com/jarikomppa/soloud
	GIT_TAG 1157475881da0d7f76102578255b937c7d4e8f57 # 2021-11-24 (master)
	SOURCE_SUBDIR "non-existent-directory")

FetchContent_MakeAvailable(SoLoud)

file(READ "${soloud_SOURCE_DIR}/LICENSE" GREM_SOLOUD_LICENSE_FILE_CONTENTS)
string(REPLACE "\r\n" "\n" GREM_SOLOUD_LICENSE_FILE_CONTENTS "${GREM_SOLOUD_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "SoLoud audio engine.*\n" GREM_SOLOUD_LICENSE_FILE_CONTENTS "${GREM_SOLOUD_LICENSE_FILE_CONTENTS}")
string(REGEX MATCH "Copyright \\(c\\) 2013-.... Jari Komppa" GREM_SOLOUD_COPYRIGHT_FILE_CONTENTS "${GREM_SOLOUD_LICENSE_FILE_CONTENTS}")

set(GREM_SOLOUD_TITLE "SoLoud" PARENT_SCOPE)
set(GREM_SOLOUD_USE "Used by GREM." PARENT_SCOPE)
set(GREM_SOLOUD_LINK "https://github.com/jarikomppa/soloud/tree/1157475881da0d7f76102578255b937c7d4e8f57" PARENT_SCOPE)
set(GREM_SOLOUD_COPYRIGHT "${GREM_SOLOUD_COPYRIGHT_FILE_CONTENTS}" PARENT_SCOPE)
set(GREM_SOLOUD_LICENSE "${GREM_SOLOUD_LICENSE_FILE_CONTENTS}" PARENT_SCOPE)

set(GREM_SOLOUD_STB_VORBIS_TITLE "stb_vorbis" PARENT_SCOPE)
set(GREM_SOLOUD_STB_VORBIS_USE "Used by SoLoud." PARENT_SCOPE)
set(GREM_SOLOUD_STB_VORBIS_LINK "https://nothings.org/stb_vorbis/" PARENT_SCOPE)
set(GREM_SOLOUD_STB_VORBIS_CREDITS "Sean Barrett" PARENT_SCOPE)
set(GREM_SOLOUD_STB_VORBIS_LICENSE "\
This is free and unencumbered software released into the public domain.\n\
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this\n\
software, either in source code form or as a compiled binary, for any purpose,\n\
commercial or non-commercial, and by any means.\n\
In jurisdictions that recognize copyright laws, the author or authors of this\n\
software dedicate any and all copyright interest in the software to the public\n\
domain. We make this dedication for the benefit of the public at large and to\n\
the detriment of our heirs and successors. We intend this dedication to be an\n\
overt act of relinquishment in perpetuity of all present and future rights to\n\
this software under copyright law.\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n\
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION\n\
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n\
" PARENT_SCOPE)

set(GREM_SOLOUD_DR_LIBS_TITLE "dr_flac, dr_mp3 and dr_wav" PARENT_SCOPE)
set(GREM_SOLOUD_DR_LIBS_USE "Used by SoLoud." PARENT_SCOPE)
set(GREM_SOLOUD_DR_LIBS_LINK "https://github.com/mackron/dr_libs" PARENT_SCOPE)
set(GREM_SOLOUD_DR_LIBS_CREDITS "David Reid" PARENT_SCOPE)
set(GREM_SOLOUD_DR_LIBS_LICENSE "\
This is free and unencumbered software released into the public domain.\n\
\n\
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this\n\
software, either in source code form or as a compiled binary, for any purpose,\n\
commercial or non-commercial, and by any means.\n\
\n\
In jurisdictions that recognize copyright laws, the author or authors of this\n\
software dedicate any and all copyright interest in the software to the public\n\
domain. We make this dedication for the benefit of the public at large and to\n\
the detriment of our heirs and successors. We intend this dedication to be an\n\
overt act of relinquishment in perpetuity of all present and future rights to\n\
this software under copyright law.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n\
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION\n\
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n\
\n\
For more information, please refer to <http://unlicense.org/>\n\
" PARENT_SCOPE)

set(GREM_SOLOUD_MINIMP3_TITLE "minimp3" PARENT_SCOPE)
set(GREM_SOLOUD_MINIMP3_USE "Base for dr_mp3." PARENT_SCOPE)
set(GREM_SOLOUD_MINIMP3_LINK "https://github.com/lieff/minimp3" PARENT_SCOPE)
set(GREM_SOLOUD_MINIMP3_CREDITS "lieff" PARENT_SCOPE)
set(GREM_SOLOUD_MINIMP3_LICENSE "\
CC0 1.0 Universal\n\
\n\
Statement of Purpose\n\
\n\
The laws of most jurisdictions throughout the world automatically confer\n\
exclusive Copyright and Related Rights (defined below) upon the creator and\n\
subsequent owner(s) (each and all, an \"owner\") of an original work of\n\
authorship and/or a database (each, a \"Work\").\n\
\n\
Certain owners wish to permanently relinquish those rights to a Work for the\n\
purpose of contributing to a commons of creative, cultural and scientific\n\
works (\"Commons\") that the public can reliably and without fear of later\n\
claims of infringement build upon, modify, incorporate in other works, reuse\n\
and redistribute as freely as possible in any form whatsoever and for any\n\
purposes, including without limitation commercial purposes. These owners may\n\
contribute to the Commons to promote the ideal of a free culture and the\n\
further production of creative, cultural and scientific works, or to gain\n\
reputation or greater distribution for their Work in part through the use and\n\
efforts of others.\n\
\n\
For these and/or other purposes and motivations, and without any expectation\n\
of additional consideration or compensation, the person associating CC0 with a\n\
Work (the \"Affirmer\"), to the extent that he or she is an owner of Copyright\n\
and Related Rights in the Work, voluntarily elects to apply CC0 to the Work\n\
and publicly distribute the Work under its terms, with knowledge of his or her\n\
Copyright and Related Rights in the Work and the meaning and intended legal\n\
effect of CC0 on those rights.\n\
\n\
1. Copyright and Related Rights. A Work made available under CC0 may be\n\
protected by copyright and related or neighboring rights (\"Copyright and\n\
Related Rights\"). Copyright and Related Rights include, but are not limited\n\
to, the following:\n\
\n\
  i. the right to reproduce, adapt, distribute, perform, display, communicate,\n\
  and translate a Work;\n\
\n\
  ii. moral rights retained by the original author(s) and/or performer(s);\n\
\n\
  iii. publicity and privacy rights pertaining to a person's image or likeness\n\
  depicted in a Work;\n\
\n\
  iv. rights protecting against unfair competition in regards to a Work,\n\
  subject to the limitations in paragraph 4(a), below;\n\
\n\
  v. rights protecting the extraction, dissemination, use and reuse of data in\n\
  a Work;\n\
\n\
  vi. database rights (such as those arising under Directive 96/9/EC of the\n\
  European Parliament and of the Council of 11 March 1996 on the legal\n\
  protection of databases, and under any national implementation thereof,\n\
  including any amended or successor version of such directive); and\n\
\n\
  vii. other similar, equivalent or corresponding rights throughout the world\n\
  based on applicable law or treaty, and any national implementations thereof.\n\
\n\
2. Waiver. To the greatest extent permitted by, but not in contravention of,\n\
applicable law, Affirmer hereby overtly, fully, permanently, irrevocably and\n\
unconditionally waives, abandons, and surrenders all of Affirmer's Copyright\n\
and Related Rights and associated claims and causes of action, whether now\n\
known or unknown (including existing as well as future claims and causes of\n\
action), in the Work (i) in all territories worldwide, (ii) for the maximum\n\
duration provided by applicable law or treaty (including future time\n\
extensions), (iii) in any current or future medium and for any number of\n\
copies, and (iv) for any purpose whatsoever, including without limitation\n\
commercial, advertising or promotional purposes (the \"Waiver\"). Affirmer makes\n\
the Waiver for the benefit of each member of the public at large and to the\n\
detriment of Affirmer's heirs and successors, fully intending that such Waiver\n\
shall not be subject to revocation, rescission, cancellation, termination, or\n\
any other legal or equitable action to disrupt the quiet enjoyment of the Work\n\
by the public as contemplated by Affirmer's express Statement of Purpose.\n\
\n\
3. Public License Fallback. Should any part of the Waiver for any reason be\n\
judged legally invalid or ineffective under applicable law, then the Waiver\n\
shall be preserved to the maximum extent permitted taking into account\n\
Affirmer's express Statement of Purpose. In addition, to the extent the Waiver\n\
is so judged Affirmer hereby grants to each affected person a royalty-free,\n\
non transferable, non sublicensable, non exclusive, irrevocable and\n\
unconditional license to exercise Affirmer's Copyright and Related Rights in\n\
the Work (i) in all territories worldwide, (ii) for the maximum duration\n\
provided by applicable law or treaty (including future time extensions), (iii)\n\
in any current or future medium and for any number of copies, and (iv) for any\n\
purpose whatsoever, including without limitation commercial, advertising or\n\
promotional purposes (the \"License\"). The License shall be deemed effective as\n\
of the date CC0 was applied by Affirmer to the Work. Should any part of the\n\
License for any reason be judged legally invalid or ineffective under\n\
applicable law, such partial invalidity or ineffectiveness shall not\n\
invalidate the remainder of the License, and in such case Affirmer hereby\n\
affirms that he or she will not (i) exercise any of his or her remaining\n\
Copyright and Related Rights in the Work or (ii) assert any associated claims\n\
and causes of action with respect to the Work, in either case contrary to\n\
Affirmer's express Statement of Purpose.\n\
\n\
4. Limitations and Disclaimers.\n\
\n\
  a. No trademark or patent rights held by Affirmer are waived, abandoned,\n\
  surrendered, licensed or otherwise affected by this document.\n\
\n\
  b. Affirmer offers the Work as-is and makes no representations or warranties\n\
  of any kind concerning the Work, express, implied, statutory or otherwise,\n\
  including without limitation warranties of title, merchantability, fitness\n\
  for a particular purpose, non infringement, or the absence of latent or\n\
  other defects, accuracy, or the present or absence of errors, whether or not\n\
  discoverable, all to the greatest extent permissible under applicable law.\n\
\n\
  c. Affirmer disclaims responsibility for clearing rights of other persons\n\
  that may apply to the Work or any use thereof, including without limitation\n\
  any person's Copyright and Related Rights in the Work. Further, Affirmer\n\
  disclaims responsibility for obtaining any necessary consents, permissions\n\
  or other rights required for any use of the Work.\n\
\n\
  d. Affirmer understands and acknowledges that Creative Commons is not a\n\
  party to this document and has no duty or obligation with respect to this\n\
  CC0 or use of the Work.\n\
\n\
For more information, please see\n\
<http://creativecommons.org/publicdomain/zero/1.0/>\n\
" PARENT_SCOPE)

set(GREM_SOLOUD_FFTREAL_TITLE "FFTReal" PARENT_SCOPE)
set(GREM_SOLOUD_FFTREAL_USE "Used by SoLoud." PARENT_SCOPE)
set(GREM_SOLOUD_FFTREAL_LINK "https://ldesoras.fr/" PARENT_SCOPE)
set(GREM_SOLOUD_FFTREAL_COPYRIGHT "Copyright (c) 1999-2010 Laurent de Soras" PARENT_SCOPE)
set(GREM_SOLOUD_FFTREAL_LICENSE "\
            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE\n\
                    Version 2, December 2004\n\
\n\
 Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>\n\
\n\
 Everyone is permitted to copy and distribute verbatim or modified\n\
 copies of this license document, and changing it is allowed as long\n\
 as the name is changed.\n\
\n\
            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE\n\
   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION\n\
\n\
  0. You just DO WHAT THE FUCK YOU WANT TO.\n\
" PARENT_SCOPE)

set(GREM_SOLOUD_FFT_TITLE "General Purpose FFT (Fast Fourier/Cosine/Sine Transform) Package" PARENT_SCOPE)
set(GREM_SOLOUD_FFT_USE "Used by SoLoud." PARENT_SCOPE)
set(GREM_SOLOUD_FFT_LINK "http://www.kurims.kyoto-u.ac.jp/~ooura/fft.html" PARENT_SCOPE)
set(GREM_SOLOUD_FFT_COPYRIGHT "Copyright Takuya OOURA, 1996-2001" PARENT_SCOPE)
set(GREM_SOLOUD_FFT_LICENSE "\
You may use, copy, modify and distribute this code for any purpose\n\
(include commercial use) and without fee. Please refer to this package\n\
when you modify this code.\n\
" PARENT_SCOPE)

set(GREM_SOLOUD_SMB_PITCH_SHIFT_TITLE "smbPitchShift.cpp" PARENT_SCOPE)
set(GREM_SOLOUD_SMB_PITCH_SHIFT_USE "Used by SoLoud." PARENT_SCOPE)
set(GREM_SOLOUD_SMB_PITCH_SHIFT_LINK "http://blogs.zynaptiq.com/bernsee" PARENT_SCOPE)
set(GREM_SOLOUD_SMB_PITCH_SHIFT_COPYRIGHT "COPYRIGHT 1999-2015 Stephan M. Bernsee <s.bernsee [AT] zynaptiq [DOT] com>" PARENT_SCOPE)
set(GREM_SOLOUD_SMB_PITCH_SHIFT_LICENSE "\
The Wide Open License (WOL)\n\
\n\
Permission to use, copy, modify, distribute and sell this software and its\n\
documentation for any purpose is hereby granted without fee, provided that\n\
the above copyright notice and this license appear in all source copies.\n\
THIS SOFTWARE IS PROVIDED \"AS IS\" WITHOUT EXPRESS OR IMPLIED WARRANTY OF\n\
ANY KIND. See http://www.dspguru.com/wol.htm for more information.\n\
" PARENT_SCOPE)

set(GREM_DEPENDENCY_PREFIXES ${GREM_DEPENDENCY_PREFIXES}
	GREM_SOLOUD
	GREM_SOLOUD_STB_VORBIS
	GREM_SOLOUD_DR_LIBS
	GREM_SOLOUD_MINIMP3
	GREM_SOLOUD_FFTREAL
	GREM_SOLOUD_FFT
	GREM_SOLOUD_SMB_PITCH_SHIFT
	PARENT_SCOPE)

add_library(SoLoud STATIC EXCLUDE_FROM_ALL)
target_sources(SoLoud
	PUBLIC FILE_SET HEADERS
		BASE_DIRS "${soloud_SOURCE_DIR}/include"
		FILES
			"${soloud_SOURCE_DIR}/include/soloud_audiosource.h"
			"${soloud_SOURCE_DIR}/include/soloud_biquadresonantfilter.h"
			"${soloud_SOURCE_DIR}/include/soloud_bus.h"
			"${soloud_SOURCE_DIR}/include/soloud_dcremovalfilter.h"
			"${soloud_SOURCE_DIR}/include/soloud_duckfilter.h"
			"${soloud_SOURCE_DIR}/include/soloud_echofilter.h"
			"${soloud_SOURCE_DIR}/include/soloud_eqfilter.h"
			"${soloud_SOURCE_DIR}/include/soloud_error.h"
			"${soloud_SOURCE_DIR}/include/soloud_fader.h"
			"${soloud_SOURCE_DIR}/include/soloud_fft.h"
			"${soloud_SOURCE_DIR}/include/soloud_fftfilter.h"
			"${soloud_SOURCE_DIR}/include/soloud_file_hack_off.h"
			"${soloud_SOURCE_DIR}/include/soloud_file_hack_on.h"
			"${soloud_SOURCE_DIR}/include/soloud_file.h"
			"${soloud_SOURCE_DIR}/include/soloud_filter.h"
			"${soloud_SOURCE_DIR}/include/soloud_flangerfilter.h"
			"${soloud_SOURCE_DIR}/include/soloud_internal.h"
			"${soloud_SOURCE_DIR}/include/soloud_lofifilter.h"
			"${soloud_SOURCE_DIR}/include/soloud_misc.h"
			"${soloud_SOURCE_DIR}/include/soloud_queue.h"
			"${soloud_SOURCE_DIR}/include/soloud_wav.h"
			"${soloud_SOURCE_DIR}/include/soloud_wavstream.h"
	PRIVATE
		"${soloud_SOURCE_DIR}/src/audiosource/wav/dr_impl.cpp"
		"${soloud_SOURCE_DIR}/src/audiosource/wav/soloud_wav.cpp"
		"${soloud_SOURCE_DIR}/src/audiosource/wav/soloud_wavstream.cpp"
		"${soloud_SOURCE_DIR}/src/audiosource/wav/stb_vorbis.c"
		"${soloud_SOURCE_DIR}/src/core/soloud.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_audiosource.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_bus.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_core_3d.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_core_basicops.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_core_faderops.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_core_filterops.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_core_getters.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_core_setters.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_core_voicegroup.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_core_voiceops.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_fader.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_fft.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_fft_lut.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_file.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_filter.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_misc.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_queue.cpp"
		"${soloud_SOURCE_DIR}/src/core/soloud_thread.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_biquadresonantfilter.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_dcremovalfilter.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_duckfilter.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_echofilter.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_eqfilter.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_fftfilter.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_flangerfilter.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_freeverbfilter.cpp"
		"${soloud_SOURCE_DIR}/src/filter/soloud_lofifilter.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/soloud_backend_sdl3.cpp")
target_compile_definitions(SoLoud PRIVATE WITH_SDL2_STATIC)
target_compile_options(SoLoud PRIVATE $<$<OR:$<C_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:Clang>>:-Wno-tautological-compare>)
if(BUILD_SHARED_LIBS)
	target_link_libraries(SoLoud PRIVATE SDL3-shared)
else()
	target_link_libraries(SoLoud PRIVATE SDL3-static)
endif()
set_target_properties(SoLoud PROPERTIES SYSTEM ON)

set(GREM_AUDIO_PRIVATE_DEPENDENCIES ${GREM_AUDIO_PRIVATE_DEPENDENCIES} SoLoud PARENT_SCOPE)
