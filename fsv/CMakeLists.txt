set(reader_SOURCE "fsv/fsv_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_FSV")

option(ENABLE_READER_FSV "Enable FSV reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_FSV)
containers_add_module(reader_fsv ${reader_SOURCE} ${reader_DEFS})
endif ()
