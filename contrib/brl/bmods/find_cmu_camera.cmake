#
# Find CMU 1394 Camera on Windows 2000
# for details see http://www-2.cs.cmu.edu/~iwan/1394/
#
# date  : 6. March 2002
# author: Jens Rittscher (rittsche@crd.ge.com)
#

IF(WIN32)

  FIND_PATH( CMU1394CAMERA_VVID_INCLUDE_PATH 1394Camera.h
    ${vxl_SOURCE_DIR}/../1394camera6_3/include
  )

  FIND_LIBRARY( CMU1394CAMERA_VVID_LIBRARY_PATH 1394camera
    ${vxl_SOURCE_DIR}/../1394camera6_3/lib
  )

  IF(CMU1394CAMERA_VVID_INCLUDE_PATH)
    IF(CMU1394CAMERA_VVID_LIBRARY_PATH)
      IF(NOT HAS_CMU1394CAMERA)
       SET(HAS_CMU1394CAMERA "YES")
       INCLUDE_DIRECTORIES( ${CMU1394CAMERA_VVID_INCLUDE_PATH} )
       LINK_LIBRARIES( ${CMU1394CAMERA_VVID_LIBRARY_PATH} )
      ENDIF(NOT HAS_CMU1394CAMERA)
    ENDIF(CMU1394CAMERA_VVID_LIBRARY_PATH)
  ENDIF(CMU1394CAMERA_VVID_INCLUDE_PATH)

ENDIF(WIN32)
