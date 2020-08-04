set(reader_SOURCE "wav/wav_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_WAV")

option(ENABLE_READER_WAV "Enable WAV reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_WAV)
containers_add_module(reader_wav ${reader_SOURCE} ${reader_DEFS})
endif ()
