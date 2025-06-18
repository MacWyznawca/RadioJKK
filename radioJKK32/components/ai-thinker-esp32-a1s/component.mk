#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ifdef CONFIG_AUDIO_BOARD_CUSTOM
COMPONENT_ADD_INCLUDEDIRS += ./ai_thinker_esp32_a1s
COMPONENT_SRCDIRS += ./ai_thinker_esp32_a1s
endif
