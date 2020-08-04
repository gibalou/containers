set(reader_SOURCE "qsynth/qsynth_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_QSYNTH")

option(ENABLE_READER_QSYNTH "Enable QSYNTH reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_QSYNTH)
containers_add_module(reader_qsynth ${reader_SOURCE} ${reader_DEFS})
endif ()
