//This is brl/bpro/core/bbas_pro/processes/bbas_estimate_irradiance_process.cxx
//:
// \file
#include <bprb/bprb_func_process.h>
#include <bprb/bprb_parameters.h>
#include <bsta/bsta_histogram.h>
#include <vil/vil_image_view_base.h>
#include <vil/vil_image_view.h>
#include <vil/vil_convert.h>
#include <vil/vil_save.h>
#include <vcl_string.h>
#ifdef DEBUG
#include <vcl_iostream.h>
#endif

#include <brdb/brdb_value.h>
#include <brip/brip_vil_float_ops.h>

//:sets input and output types
bool bbas_estimate_irradiance_process_cons(bprb_func_process& pro)
{
  //inputs
  //0: The unnormalized image
  //1: sz the z component of the illumination direction

  vcl_vector<vcl_string> input_types_(3);
  input_types_[0] = "vil_image_view_base_sptr";
  input_types_[1] = "float"; // z component of sun direction
  input_types_[2] = "float"; // mean albedo value of scene

  if (!pro.set_input_types(input_types_))
    return false;
  //output
  vcl_vector<vcl_string> output_types_(2);
  output_types_[0] = "vil_image_view_base_sptr"; // floating point image with airlight removed
  output_types_[1] = "float"; // estimated irradiance
  return pro.set_output_types(output_types_);
}

bool bbas_estimate_irradiance_process(bprb_func_process& pro)
{

  //check number of inputs
  if(!pro.verify_inputs())
  {
    vcl_cout << pro.name() << " Invalid inputs " << vcl_endl;
    return false;
  }

  //get the inputs
  vil_image_view_base_sptr input_img = 
    pro.get_input<vil_image_view_base_sptr>(0);

  float sz = pro.get_input<float>(1);

  float mean_albedo = pro.get_input<float>(2);
  
  //check inputs validity
  if (!input_img) {
    vcl_cout << pro.name() <<" :--  image  is null!\n";
    return false;
  }
  unsigned ni_ = input_img->ni(); 
  unsigned nj_ = input_img->nj(); 
  unsigned nplanes_ = input_img->nplanes();
  if(nplanes_!=1){
    vcl_cout << pro.name() <<" :--  image  is not grey scale!\n";
    return false;
  }
  if(sz<0.1) {
    vcl_cout << pro.name() <<" :--  sun illumination angle too low\n";
    return false;
  }
  if(sz > 1.0) {
    vcl_cout << pro.name() << " :-- sun illumination angle invalid" << vcl_endl;
  }

  vil_image_view_base_sptr image_sptr = new vil_image_view<float>(input_img->ni(), input_img->nj());
  vil_image_view<float> image = *(static_cast<vil_image_view<float>*>(image_sptr.ptr()));
  
  if (input_img->pixel_format() == VIL_PIXEL_FORMAT_UINT_16) {
    vcl_cout << "###################################### FORMAT UINT16 " << vcl_endl;
    vxl_uint_16 max_val_16 = 1 << 11; // images use 11 bits
    vil_image_view<vxl_uint_16> img16 = *vil_convert_cast(vxl_uint_16(), input_img);
    vil_convert_stretch_range_limited(img16, image, vxl_uint_16(0), max_val_16, 0.0f, 1.0f);
  }
  else if (input_img->pixel_format() == VIL_PIXEL_FORMAT_BYTE) {
    vcl_cout << "###################################### FORMAT BYTE  " << vcl_endl;
    vil_image_view<vxl_byte> img8 = *vil_convert_cast(vxl_byte(), input_img);
    vil_convert_stretch_range_limited(img8, image, vxl_byte(0), vxl_byte(255), 0.0f, 1.0f);
  }
  else if (input_img->pixel_format() == VIL_PIXEL_FORMAT_FLOAT) {
     vcl_cout << "###################################### FORMAT FLOAT " << vcl_endl;
    // leave as is
    image.deep_copy(*vil_convert_cast(float(), input_img));
  }


  // compute histogram for image
  bsta_histogram<float> h(0.0f, 1.0f, 256);
  for(unsigned j = 0; j<nj_; ++j)
    for(unsigned i = 0; i<ni_; ++i)
      h.upcount(image(i, j), 1.0f);

  // compute airlight and scene irradiance
  float frac = 0.0001f;
  float airlight = h.value_with_area_below(frac);
  float mean = h.mean_vals(airlight, h.max());
  float irrad = (mean-airlight)/(sz * mean_albedo);
  
  vcl_cout << "airlight " << airlight << "  irradiance " << irrad << '\n';

  for (unsigned int j=0; j<image.nj(); ++j) {
    for (unsigned int i=0; i<image.ni(); ++i) {
      // clip lower range to 0
      image(i,j) = vcl_max(image(i,j) - airlight, 0.0f);
    }
  }

  
  pro.set_output_val<vil_image_view_base_sptr>(0, image_sptr);
  pro.set_output_val<float>(1, irrad);

  return true;
}

