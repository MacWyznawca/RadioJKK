include( ${PROJECT_DIR}/main/embed_stations.cmake )
set(COMPONENT_SRCS "radio_jkk.c" "RawSplit/raw_split.c" "jkk_audio_main.c" "jkk_audio_sdwrite.c"
                   )
set(COMPONENT_ADD_INCLUDEDIRS .)

register_component()


 #   EMBED_TXTFILES  "../stations.txt"
