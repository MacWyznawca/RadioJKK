set(srcs  "radio_jkk.c" 
                    "RawSplit/raw_split.c"
                    "jkk_audio_main.c" 
                    "jkk_audio_sdwrite.c"
                    "jkk_nvs.c"
                    "jkk_settings.c"
                   )

if(CONFIG_JKK_RADIO_USING_I2C_LCD)
    list(APPEND srcs "display/jkk_mono_lcd.c")
endif()

idf_component_register(SRCS "${srcs}"
                    EMBED_TXTFILES ../stations.txt)

set(COMPONENT_ADD_INCLUDEDIRS .)
