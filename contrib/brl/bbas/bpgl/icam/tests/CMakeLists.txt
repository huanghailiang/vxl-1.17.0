# vpgl/icam/tests/CMakeLists.txt

ADD_EXECUTABLE( icam_test_all
  test_driver.cxx
  test_minimizer.cxx
  test_icam_transform.cxx
  test_rotation_only.cxx
)

TARGET_LINK_LIBRARIES( icam_test_all icam vgl_algo vgl vnl vil vul testlib)

ADD_TEST( icam_test_minimizer         icam_test_all test_minimizer )
ADD_TEST( icam_test_transform         icam_test_all test_icam_transform )
ADD_TEST( icam_test_rotation_only     icam_test_all test_rotation_only )

ADD_EXECUTABLE( icam_test_include test_include.cxx )
TARGET_LINK_LIBRARIES( icam_test_include icam)
