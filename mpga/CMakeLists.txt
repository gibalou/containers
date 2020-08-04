set(reader_SOURCE "mpga/mpga_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_MPGA")

option(ENABLE_READER_MPGA "Enable MPEG audio reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_MPGA)
containers_add_module(reader_mpga ${reader_SOURCE} ${reader_DEFS})
endif ()
