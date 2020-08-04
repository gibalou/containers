set(reader_SOURCE "flash/flv_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_FLV")

option(ENABLE_READER_FLV "Enable FLV reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_FLV)
containers_add_module(reader_flv ${reader_SOURCE} ${reader_DEFS})
endif ()
