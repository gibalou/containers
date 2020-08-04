set(reader_SOURCE "rtp/rtp_reader.c rtp/rtp_h264.c rtp/rtp_mpeg4.c rtp/rtp_base64.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_RTP")

option(ENABLE_READER_RTP "Enable RTP reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_RTP)
containers_add_module(reader_rtp ${reader_SOURCE} ${reader_DEFS})
endif ()
