set(reader_SOURCE "rtpdump/rtpdump_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_RTPDUMP")
set(writer_SOURCE "rtpdump/rtpdump_writer.c")
set(writer_DEFS "-DENABLE_CONTAINER_WRITER_RTPDUMP")

option(ENABLE_READER_RTPDUMP "Enable RTPDUMP reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_RTPDUMP)
containers_add_module(reader_rtpdump ${reader_SOURCE} ${reader_DEFS})
endif ()

option(ENABLE_WRITER_RTPDUMP "Enable RTPDUMP writer" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_WRITER_RTPDUMP)
containers_add_module(writer_rtpdump ${writer_SOURCE} ${writer_DEFS})
endif ()
