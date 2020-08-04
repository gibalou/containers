set(reader_SOURCE "rv9/rv9_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_RV9")

option(ENABLE_READER_RV9 "Enable RV9 reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_RV9)
containers_add_module(reader_rv9 ${reader_SOURCE} ${reader_DEFS})
endif ()
