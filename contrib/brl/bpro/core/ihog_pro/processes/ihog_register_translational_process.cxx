// This is brl/bpro/core/ihog_pro/processes/ihog_register_translational_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file
// \brief A process to register two images by finding the best translational mapping.

#include <vcl_iostream.h>
#include <ihog/ihog_minimizer.h>
#include <ihog/ihog_world_roi.h>
#include <vil/vil_convert.h>


//: Init function
bool ihog_register_translational_process_cons(bprb_func_process& pro)
{
  //this process takes two inputs: 
  //  0) image 0
  //  1) image 1
  //  2) int        radius for an exhaustive search at the beginning, no exhaustive search if passed as 0
  bool ok=false;
  vcl_vector<vcl_string> input_types;
  input_types.push_back("vil_image_view_base_sptr");
  input_types.push_back("vil_image_view_base_sptr");
  input_types.push_back("vil_image_view_base_sptr");
  input_types.push_back("int");   
  ok = pro.set_input_types(input_types);

  if (!ok) return ok;

  vcl_vector<vcl_string> output_types;
  output_types.push_back("double");
  output_types.push_back("double");
  ok = pro.set_output_types(output_types);
  if (!ok) return ok;

  return true;
}

//: Execute the process
bool ihog_register_translational_process(bprb_func_process& pro)
{
  if (pro.n_inputs()!= pro.input_types().size()) {
    vcl_cout << "ihog_register_translational_process: The input number should be " << pro.input_types().size() << vcl_endl;
    return false;
  }
  // get the inputs
  vil_image_view<vxl_byte> img_0(pro.get_input<vil_image_view_base_sptr>(0));
  vil_image_view<vxl_byte> img_0_mask(pro.get_input<vil_image_view_base_sptr>(1));
  vil_image_view<vxl_byte> img_1(pro.get_input<vil_image_view_base_sptr>(2));
  int radius = pro.get_input<int>(3);

  unsigned ni = img_0.ni(), nj = img_0.nj();
  vil_image_view<float> img0(ni,nj);
  vil_convert_cast(img_0,img0);
  
  vil_image_view<float> img1(img_1.ni(),img_1.nj());
  vil_convert_cast(img_1, img1);

  vil_image_view<float> mask0(img_0_mask.ni(), img_0_mask.nj());
  vil_convert_stretch_range_limited(img_0_mask, mask0, vxl_byte(0), vxl_byte(255), 0.0f, 1.0f);

  vil_image_view<float> mask1(img_1.ni(),img_1.nj());
  mask1.fill(1.0f);

  int border = 2;
  ihog_world_roi roi(img0.ni()- 2*border,
                     img0.nj()- 2*border,
                     vgl_point_2d<double>(border,border));
  
  ihog_transform_2d init_xform;
  init_xform.set_translation_only(0,0);
  ihog_image<float> from_img(img0, init_xform);
  ihog_image<float> to_img(img1, ihog_transform_2d());
  ihog_image<float> mask1_img(mask1, ihog_transform_2d());
  ihog_image<float> mask0_img(mask0, ihog_transform_2d());
  ihog_minimizer minimizer(from_img, to_img, mask0_img, mask1_img, roi);
  
  minimizer.minimize_exhaustive_minfo(radius, init_xform);
  double error = minimizer.get_end_error();
  vcl_cout << "end_error = " << error << '\n'
           << "lm generated homography:\n"
           << init_xform.get_matrix() << vcl_endl << vcl_endl;
  //test result
  vgl_point_2d<double> p0 = init_xform.origin();
  
  //: sanity check
  if (vcl_abs(p0.x()) > radius || vcl_abs(p0.y()) > radius) {
    vcl_cout << "FAILED SANITY CHECK: TX = " << p0.x() << " TY = " << p0.y() << vcl_endl;
    pro.set_output_val<double>(0, 0.0);
    pro.set_output_val<double>(1, 0.0);
  } else {
    pro.set_output_val<double>(0,p0.x());
    pro.set_output_val<double>(1,p0.y());
  }
  return true;
}

