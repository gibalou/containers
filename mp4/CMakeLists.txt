set(reader_SOURCE "mp4/mp4_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_MP4")
set(writer_SOURCE "mp4/mp4_writer.c")
set(writer_DEFS "-DENABLE_CONTAINER_WRITER_MP4")

option(ENABLE_READER_MP4 "Enable MP4 reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_MP4)
containers_add_module(reader_mp4 ${reader_SOURCE} ${reader_DEFS})
endif ()

option(ENABLE_WRITER_MP4 "Enable MP4 writer" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_WRITER_MP4)
containers_add_module(writer_mp4 ${writer_SOURCE} ${writer_DEFS})
endif ()
