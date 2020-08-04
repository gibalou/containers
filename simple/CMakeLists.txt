set(reader_SOURCE "simple/simple_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_SIMPLE")
set(writer_SOURCE "simple/simple_writer.c")
set(writer_DEFS "-DENABLE_CONTAINER_WRITER_SIMPLE")

option(ENABLE_READER_SIMPLE "Enable SIMPLE reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_SIMPLE)
containers_add_module(reader_simple ${reader_SOURCE} ${reader_DEFS})
endif ()

option(ENABLE_WRITER_SIMPLE "Enable SIMPLE writer" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_WRITER_SIMPLE)
containers_add_module(writer_simple ${writer_SOURCE} ${writer_DEFS})
endif ()
