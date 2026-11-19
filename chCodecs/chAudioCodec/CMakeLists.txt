ch_add_plugin(chAudioCodec FOLDER Codecs CODEC)

# The sound file is read by chCore's AudioClip, which already holds the decoders.
target_link_libraries(chAudioCodec PRIVATE chCore)
