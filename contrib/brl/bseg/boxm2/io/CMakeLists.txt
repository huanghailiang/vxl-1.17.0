# contrib/brl/bseg/boxm/CMakeLists.txt
INCLUDE_DIRECTORIES( ${GEL_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${GEL_INCLUDE_DIR}/mrc )

INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR}/bbas )
INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR}/bseg )

SET(boxm2_io_sources
    boxm2_asio_mgr.h       boxm2_asio_mgr.cxx
    boxm2_sio_mgr.h        boxm2_sio_mgr.cxx
    boxm2_cache.h          boxm2_cache.cxx
    boxm2_dumb_cache.h     boxm2_dumb_cache.cxx
    boxm2_nn_cache.h       boxm2_nn_cache.cxx
    boxm2_lru_cache.h      boxm2_lru_cache.cxx
    boxm2_stream_cache.h   boxm2_stream_cache.cxx boxm2_stream_cache.txx
    boxm2_stream_block_cache.h   boxm2_stream_block_cache.cxx
    boxm2_stream_scene_cache.h   boxm2_stream_scene_cache.cxx
   )
AUX_SOURCE_DIRECTORY(Templates boxm2_io_sources)

ADD_LIBRARY(boxm2_io  ${boxm2_io_sources})
TARGET_LINK_LIBRARIES(boxm2_io boxm2 expatpp vpgl baio vpgl vgl_xio vgl vbl vul vsl vcl)

#install the .h .txx and libs
INSTALL_TARGETS(/lib boxm2_io)
INSTALL_NOBASE_HEADER_FILES(/include/vxl/contrib/brl/bseg/boxm2/io ${boxm2_io_sources})

IF( BUILD_TESTING )
  SUBDIRS(tests)
ENDIF( BUILD_TESTING )
