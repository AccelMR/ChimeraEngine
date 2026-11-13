ch_add_plugin(chFontCodec FOLDER Codecs CODEC)

# The font file is read by chCore's Font, which already holds stb_truetype.
target_link_libraries(chFontCodec PRIVATE chCore)
