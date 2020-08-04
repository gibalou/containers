set(reader_SOURCE "mkv/matroska_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_MKV")

option(ENABLE_READER_MKV "Enable MKV reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_MKV)
containers_add_module(reader_mkv ${reader_SOURCE} ${reader_DEFS})
endif ()
