// This is brl/bpro/core/vil_pro/processes/vil_stretch_image_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file

#include "../vil_math_functors.h"
#include <bprb/bprb_parameters.h>
#include <vil/vil_image_view.h>
#include <vil/vil_convert.h>

//: Constructor
bool vil_stretch_image_process_cons(bprb_func_process& pro)
{
  //input
  bool ok=false;
  vcl_vector<vcl_string> input_types(4);
  input_types[0] = "vil_image_view_base_sptr";  
  input_types[1] = "float";   // min limit
  input_types[2] = "float";   // max limit
  input_types[3] = "vcl_string"; //output image pixel type:
                                // "byte | short | float"
  ok = pro.set_input_types(input_types);
  if (!ok) return ok;

  //output
  vcl_vector<vcl_string> output_types;
  output_types.push_back("vil_image_view_base_sptr");
  ok = pro.set_output_types(output_types);
  if (!ok) return ok;
  return true;
}

//: Execute the process
bool vil_stretch_image_process(bprb_func_process& pro)
{

   // Sanity check
  if(!pro.verify_inputs()){
    vcl_cerr << "vil_stretch_image_process: Invalid inputs" << vcl_endl;
    return false;
  }

  //Retrieve image from input
  vil_image_view_base_sptr image = pro.get_input<vil_image_view_base_sptr>(0);

  //Retrieve limits
  float min_limit = pro.get_input<float>(1);
  float max_limit = pro.get_input<float>(2);
  if(min_limit>=max_limit){
    vcl_cerr << "vil_stretch_image_process: invalid stretch limits\n";
      return false;
  }
    
  vcl_string out_type = pro.get_input<vcl_string>(3);
  // retrieve float image
  vil_image_view_base_sptr fimage = vil_convert_cast(float(), image);
  vil_image_view<float> fimg = *fimage;
  unsigned ni = fimg.ni(), nj = fimg.nj(), np = fimg.nplanes();
  if(out_type=="byte"){
    float scale = 255.0f/(max_limit-min_limit);
    for(unsigned j = 0; j<nj; ++j) 
      for(unsigned i = 0; i<ni; ++i) 
        for(unsigned p = 0; p<np; ++p){
          float v = scale*(fimg(i,j,p)-min_limit);
          if(v>255.0f) v= 255.0f; if(v<0.0f) v = 0.0f;
          fimg(i,j,p) = v;
        }
    vil_image_view_base_sptr temp  = vil_convert_cast(vxl_byte(), fimage);
    pro.set_output_val<vil_image_view_base_sptr>(0, temp);
  }
  else if(out_type=="short"){
    float scale = 65536.0f/(max_limit-min_limit);
    for(unsigned j = 0; j<nj; ++j) 
      for(unsigned i = 0; i<ni; ++i) 
        for(unsigned p = 0; p<np; ++p){
          float v = scale*(fimg(i,j,p)-min_limit);
          if(v>65536.0f) v= 65536.0f; if(v<0.0f) v = 0.0f;
          fimg(i,j,p) = v;
        }
    vil_image_view_base_sptr temp  = vil_convert_cast(vxl_uint_16(), fimage);
    pro.set_output_val<vil_image_view_base_sptr>(0, temp);
  }
  else if(out_type=="float"){
    float scale = 1.0f/(max_limit-min_limit);
    for(unsigned j = 0; j<nj; ++j) 
      for(unsigned i = 0; i<ni; ++i) 
        for(unsigned p = 0; p<np; ++p){
          float v = scale*(fimg(i,j,p)-min_limit);
          if(v>1.0f) v = 1.0f; if(v<0.0f) v=0.0f;
          fimg(i,j,p) = v;
        }
    pro.set_output_val<vil_image_view_base_sptr>(0, fimage);
  }
  else{
    vcl_cerr << "vil_stretch_image_process: unknown output type\n";
      return false;
  }
return true;
  

 
  return true;
}

