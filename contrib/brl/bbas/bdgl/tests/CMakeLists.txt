# brl/bbas/bdgl/tests/CMakeLists.txt

INCLUDE_DIRECTORIES(${GEL_INCLUDE_DIR} )

ADD_EXECUTABLE( bdgl_test_all
  test_driver.cxx
  test_curve_algs.cxx
  test_region_algs.cxx
  test_peano_curve.cxx
)

TARGET_LINK_LIBRARIES( bdgl_test_all bdgl vdgl vsol vgl testlib )

ADD_TEST(bdgl_test_curve_algs  ${EXECUTABLE_OUTPUT_PATH}/bdgl_test_all test_curve_algs)
ADD_TEST(bdgl_test_region_algs ${EXECUTABLE_OUTPUT_PATH}/bdgl_test_all test_region_algs)
ADD_TEST(bdgl_test_peano_curve ${EXECUTABLE_OUTPUT_PATH}/bdgl_test_all test_peano_curve)

ADD_EXECUTABLE( bdgl_test_include test_include.cxx )
TARGET_LINK_LIBRARIES( bdgl_test_include bdgl )
