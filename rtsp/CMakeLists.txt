set(reader_SOURCE "rtsp/rtsp_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_RTSP")

option(ENABLE_READER_RTSP "Enable RTSP reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_RTSP)
containers_add_module(reader_rtsp ${reader_SOURCE} ${reader_DEFS})
endif ()
