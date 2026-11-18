# ---------- libwebp ----------
# Only its decoder library is used. It is static, so the codec needs no system package, and
# EXCLUDE_FROM_ALL keeps the encoder, the tools and their image libraries out of the build.
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_LIBWEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
# The codec decodes one image per import thread; libwebp's own worker thread is not needed.
set(WEBP_USE_THREAD OFF CACHE BOOL "" FORCE)
# Static libraries make libwebp build position independent code, which the plugin needs.
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(Externals/libwebp SYSTEM EXCLUDE_FROM_ALL)
ch_set_folder_for_directory(Externals/libwebp Externals/libwebp)

# ---------- Codec ----------
ch_add_plugin(chImageCodec FOLDER Codecs CODEC)

target_link_libraries(chImageCodec PRIVATE chCore webpdecoder)

# stb_image is header-only; SYSTEM keeps our warning level out of its code.
target_include_directories(chImageCodec SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Externals/stb)
# libwebp's own include folder is its source root; this gives the installed form "webp/decode.h".
target_include_directories(chImageCodec SYSTEM PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/Externals/libwebp/src)
