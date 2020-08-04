set(reader_ps_SOURCE "mpeg/ps_reader.c")
set(reader_ps_DEFS "-DENABLE_CONTAINER_READER_PS")
set(reader_ts_SOURCE "mpeg/ts_reader.c")
set(reader_ts_DEFS "-DENABLE_CONTAINER_READER_TS")

option(ENABLE_READER_PS "Enable MPEG PS reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_PS)
containers_add_module(reader_ps ${reader_ps_SOURCE} ${reader_ps_DEFS})
endif ()

option(ENABLE_READER_TS "Enable MPEG TS reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_TS)
containers_add_module(reader_ts ${reader_ts_SOURCE} ${reader_ts_DEFS})
endif ()
