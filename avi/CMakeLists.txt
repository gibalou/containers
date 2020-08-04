set(reader_SOURCE "avi/avi_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_AVI")
set(writer_SOURCE "avi/avi_writer.c")
set(writer_DEFS "-DENABLE_CONTAINER_WRITER_AVI")

option(ENABLE_READER_AVI "Enable AVI reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_AVI)
containers_add_module(reader_avi ${reader_SOURCE} ${reader_DEFS})
endif ()

option(ENABLE_WRITER_AVI "Enable AVI writer" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_WRITER_AVI)
containers_add_module(writer_avi ${writer_SOURCE} ${writer_DEFS})
endif ()
