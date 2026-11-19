/* Builds miniaudio once, with Ogg Vorbis through stb_vorbis: miniaudio only decodes Vorbis
   when the stb_vorbis declarations come before its implementation. The defines are set on
   the target (CMakeLists.txt), so chCore's files that include miniaudio.h see the same. */
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
