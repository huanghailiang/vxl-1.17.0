# contrib/brl/bseg/boxm2_multi/exe/CMakeLists.txt
INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR}/bseg/boxm2/reg/ocl/ )

ADD_EXECUTABLE( boxm2_ocl_reg_mutual_info_exe
                boxm2_ocl_reg_mutual_info_exe.cxx
              )
TARGET_LINK_LIBRARIES( boxm2_ocl_reg_mutual_info_exe boxm2_ocl_reg  vbl vgl)