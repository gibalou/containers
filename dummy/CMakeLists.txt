set(writer_SOURCE "dummy/dummy_writer.c")
set(writer_DEFS "-DENABLE_CONTAINER_WRITER_DUMMY")

option(ENABLE_WRITER_DUMMY "Enable DUMMY writer" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_WRITER_DUMMY)
containers_add_module(writer_dummy ${writer_SOURCE} ${writer_DEFS})
endif ()
