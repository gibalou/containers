set(reader_SOURCE "binary/binary_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_BINARY")
set(writer_SOURCE "binary/binary_writer.c")
set(writer_DEFS "-DENABLE_CONTAINER_WRITER_BINARY")


option(ENABLE_READER_BINARY "Enable BINARY reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_BINARY)
containers_add_module(reader_binary ${reader_SOURCE} ${reader_DEFS})
endif ()

option(ENABLE_WRITER_BINARY "Enable BINARY writer" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_WRITER_BINARY)
containers_add_module(writer_binary ${writer_SOURCE} ${writer_DEFS})
endif ()
