set(reader_SOURCE "asf/asf_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_ASF")
set(writer_SOURCE "asf/asf_writer.c")
set(writer_DEFS "-DENABLE_CONTAINER_WRITER_ASF")

option(ENABLE_READER_ASF "Enable ASF reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_ASF)
containers_add_module(reader_asf ${reader_SOURCE} ${reader_DEFS})
endif ()

option(ENABLE_WRITER_ASF "Enable ASF writer" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_WRITER_ASF)
containers_add_module(writer_asf ${writer_SOURCE} ${writer_DEFS})
endif ()
