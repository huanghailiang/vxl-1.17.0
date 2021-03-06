# This is core/vgl/tests/CMakeLists.txt

ADD_EXECUTABLE( vgl_test_all
  test_driver.cxx

  test_cartesian.cxx
  test_distance.cxx
  test_closest_point.cxx
  test_polygon.cxx
  test_convex.cxx
  test_polygon_scan_iterator.cxx
  test_triangle_scan_iterator.cxx
  test_ellipse_scan_iterator.cxx
  test_window_scan_iterator.cxx
  test_area.cxx
  test_clip.cxx
  test_sphere.cxx
  test_line_3d_2_points.cxx
  test_line_segment_3d.cxx
  test_triangle_3d.cxx
  test_triangle_3d_line_intersection.cxx
  test_infinite_line_3d.cxx
  test_ray_3d.cxx
)
TARGET_LINK_LIBRARIES( vgl_test_all vgl testlib )

ADD_TEST( vgl_test_cartesian                      ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_cartesian )
ADD_TEST( vgl_test_distance                       ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_distance )
ADD_TEST( vgl_test_closest_point                  ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_closest_point )
ADD_TEST( vgl_test_polygon                        ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_polygon )
ADD_TEST( vgl_test_convex                         ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_convex )
ADD_TEST( vgl_test_polygon_scan_iterator          ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_polygon_scan_iterator )
ADD_TEST( vgl_test_triangle_scan_iterator         ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_triangle_scan_iterator )
ADD_TEST( vgl_test_ellipse_scan_iterator          ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_ellipse_scan_iterator )
ADD_TEST( vgl_test_window_scan_iterator           ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_window_scan_iterator )
ADD_TEST( vgl_test_area                           ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_area )
ADD_TEST( vgl_test_clip                           ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_clip )
ADD_TEST( vgl_test_sphere                         ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_sphere)
ADD_TEST( vgl_test_line_3d_2_points               ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_line_3d_2_points)
ADD_TEST( vgl_test_line_segment_3d                ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_line_segment_3d)
ADD_TEST( vgl_test_triangle_3d                    ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_triangle_3d)
ADD_TEST( vgl_test_triangle_3d_line_intersection  ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_triangle_3d_line_intersection)
ADD_TEST( vgl_test_infinite_line_3d               ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_infinite_line_3d)
ADD_TEST( vgl_test_ray_3d                         ${EXECUTABLE_OUTPUT_PATH}/vgl_test_all test_ray_3d)

ADD_EXECUTABLE( vgl_test_include test_include.cxx )
TARGET_LINK_LIBRARIES( vgl_test_include vgl )
ADD_EXECUTABLE( vgl_test_template_include test_template_include.cxx )
TARGET_LINK_LIBRARIES( vgl_test_template_include vgl )
