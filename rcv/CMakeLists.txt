set(reader_SOURCE "rcv/rcv_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_RCV")

option(ENABLE_READER_RCV "Enable RCV reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_RCV)
containers_add_module(reader_rcv ${reader_SOURCE} ${reader_DEFS})
endif ()
