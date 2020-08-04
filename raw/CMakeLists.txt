set(reader_SOURCE "raw/raw_video_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_RAWVIDEO")
set(writer_SOURCE "raw/raw_video_writer.c")
set(writer_DEFS "-DENABLE_CONTAINER_WRITER_RAWVIDEO")

option(ENABLE_READER_RAWVIDEO "Enable RAW VIDEO reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_RAWVIDEO)
containers_add_module(reader_raw_video ${reader_SOURCE} ${reader_DEFS})
endif ()

option(ENABLE_WRITER_RAWVIDEO "Enable RAW VIDEO writer" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_WRITER_RAWVIDEO)
containers_add_module(writer_raw_video ${writer_SOURCE} ${writer_DEFS})
endif ()
