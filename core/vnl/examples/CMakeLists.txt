# vxl/vnl/examples/CMakeLists.txt

LINK_LIBRARIES( vnl_algo vnl vul )

ADD_EXECUTABLE(vnl_amoeba                vnl_amoeba.cxx)
ADD_EXECUTABLE(vnl_complex_svd           vnl_complex_svd.cxx)
ADD_EXECUTABLE(vnl_planefit              vnl_planefit.cxx)
ADD_EXECUTABLE(vnl_polyroots             vnl_polyroots.cxx)
ADD_EXECUTABLE(vnl_polynomial_factoring  vnl_polynomial_factoring.cxx)
ADD_EXECUTABLE(vnl_polynomial_RPN        vnl_polynomial_RPN.cxx)
ADD_EXECUTABLE(vnl_rosenbrock            vnl_rosenbrock.cxx)
ADD_EXECUTABLE(vnl_svd                   vnl_svd.cxx)
ADD_EXECUTABLE(vnl_svd_economy           vnl_svd_economy.cxx)
ADD_EXECUTABLE(vnl_lsqr_test             vnl_lsqr_test.cxx)
ADD_EXECUTABLE(vnl_sample_beta           vnl_sample_beta.cxx)
ADD_EXECUTABLE(vnl_sparse_matrix_example vnl_sparse_matrix_example.cxx)
ADD_EXECUTABLE(vnl_complex_squareroot    vnl_complex_squareroot.cxx)

ADD_EXECUTABLE(time_fastops              time_fastops.cxx)
ADD_EXECUTABLE(calculate                 calculate.cxx)
