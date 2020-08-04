set(reader_SOURCE "metadata/id3/id3_metadata_reader.c")
set(reader_DEFS "-DENABLE_CONTAINER_READER_METADATA_ID3")

option(ENABLE_READER_METADATA_ID3 "Enable ID3 metadata reader" OFF)
if (NOT DISABLE_CONTAINER_ALL OR ENABLE_READER_METADATA_ID3)
containers_add_module(reader_metadata_id3 ${reader_SOURCE} ${reader_DEFS})
endif ()
