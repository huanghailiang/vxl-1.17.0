// This is brl/bseg/boxm2/ocl/pro/processes/boxm2_ocl_render_gl_expected_image_process.cxx
//:
// \file
// \brief  A process for rendering the scene.
//
// \author Vishal Jain
// \date Mar 10, 2011

#include <bprb/bprb_func_process.h>

#include <vcl_fstream.h>
#include <vcl_algorithm.h>
#include <boxm2/ocl/boxm2_opencl_cache.h>
#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_block.h>
#include <boxm2/boxm2_data_base.h>
#include <boxm2/ocl/boxm2_ocl_util.h>
#include <boxm2/boxm2_util.h>
//brdb stuff
#include <brdb/brdb_value.h>

//directory utility
#include <vcl_where_root_dir.h>
#include <bocl/bocl_device.h>
#include <bocl/bocl_kernel.h>
#include <boxm2/ocl/algo/boxm2_ocl_render_expected_image_function.h>


namespace boxm2_ocl_render_gl_expected_image_process_globals
{
  const unsigned n_inputs_ = 9 ;
  const unsigned n_outputs_ = 1;
  vcl_size_t lthreads[2]={8,8};

  static vcl_map<vcl_string,vcl_vector<bocl_kernel*> > kernels;

  void compile_kernel(bocl_device_sptr device,vcl_vector<bocl_kernel*> & vec_kernels, vcl_string opts)
  {
    //gather all render sources... seems like a lot for rendering...
    vcl_vector<vcl_string> src_paths;
    vcl_string source_dir = boxm2_ocl_util::ocl_src_root();
    src_paths.push_back(source_dir + "scene_info.cl");
    src_paths.push_back(source_dir + "bit/bit_tree_library_functions.cl");
    src_paths.push_back(source_dir + "backproject.cl");
    src_paths.push_back(source_dir + "statistics_library_functions.cl");
    src_paths.push_back(source_dir + "expected_functor.cl");
    src_paths.push_back(source_dir + "ray_bundle_library_opt.cl");
    src_paths.push_back(source_dir + "bit/render_bit_scene.cl");
    src_paths.push_back(source_dir + "bit/cast_ray_bit.cl");

    //set kernel options
    //#define STEP_CELL step_cell_render(mixture_array, alpha_array, data_ptr, d, &vis, &expected_int);
    vcl_string options = opts + " -D RENDER ";
    options += " -D DETERMINISTIC ";
    options += " -D STEP_CELL=step_cell_render(aux_args.mog,aux_args.alpha,data_ptr,d*linfo->block_len,vis,aux_args.expint) ";

    //have kernel construct itself using the context and device
    bocl_kernel * ray_trace_kernel=new bocl_kernel();

    ray_trace_kernel->create_kernel( &device->context(),
                                     device->device_id(),
                                     src_paths,
                                     "render_bit_scene",   //kernel name
                                     options,              //options
                                     "boxm2 opencl render"); //kernel identifier (for error checking)
    vec_kernels.push_back(ray_trace_kernel);
    //create normalize image kernel
    vcl_vector<vcl_string> norm_src_paths;
    norm_src_paths.push_back(source_dir + "pixel_conversion.cl");
    norm_src_paths.push_back(source_dir + "bit/normalize_kernels.cl");
    bocl_kernel * normalize_render_kernel=new bocl_kernel();

    normalize_render_kernel->create_kernel( &device->context(),
                                            device->device_id(),
                                            norm_src_paths,
                                            "normalize_render_kernel_gl",   //kernel name
                                            " -D NORMALIZE_RENDER_GL ",              //options
                                            "normalize render kernel gl"); //kernel identifier (for error checking)

    vec_kernels.push_back(normalize_render_kernel);
  }
}

bool boxm2_ocl_render_gl_expected_image_process_cons(bprb_func_process& pro)
{
  using namespace boxm2_ocl_render_gl_expected_image_process_globals;

  //process takes 1 input
  vcl_vector<vcl_string> input_types_(n_inputs_);
  input_types_[0] = "bocl_device_sptr";
  input_types_[1] = "boxm2_scene_sptr";
  input_types_[2] = "boxm2_opencl_cache_sptr";
  input_types_[3] = "vpgl_camera_double_sptr";
  input_types_[4] = "unsigned";
  input_types_[5] = "unsigned";
  input_types_[6] = "bocl_mem_sptr"; // exp image buffer;
  input_types_[7] = "bocl_mem_sptr"; // exp image dimensions buffer;
  input_types_[8] = "vcl_string"; // identifier string to retrieve corresponding app data buffer;

  vcl_vector<vcl_string> output_types_(n_outputs_);
  output_types_[0] = "float";

  bool good = pro.set_input_types(input_types_) && pro.set_output_types(output_types_);

  brdb_value_sptr idx = new brdb_value_t<vcl_string>("");
  pro.set_input(8, idx); // set default value

  return good;
}

bool boxm2_ocl_render_gl_expected_image_process(bprb_func_process& pro)
{
  using namespace boxm2_ocl_render_gl_expected_image_process_globals;

  if ( pro.n_inputs() < n_inputs_ ) {
    vcl_cout << pro.name() << ": The input number should be " << n_inputs_<< vcl_endl;
    return false;
  }
  //get the inputs
  unsigned i = 0;
  bocl_device_sptr device= pro.get_input<bocl_device_sptr>(i++);
  boxm2_scene_sptr scene =pro.get_input<boxm2_scene_sptr>(i++);

  boxm2_opencl_cache_sptr opencl_cache= pro.get_input<boxm2_opencl_cache_sptr>(i++);
  vpgl_camera_double_sptr cam= pro.get_input<vpgl_camera_double_sptr>(i++);
  unsigned ni=pro.get_input<unsigned>(i++);
  unsigned nj=pro.get_input<unsigned>(i++);
  bocl_mem_sptr exp_image =pro.get_input<bocl_mem_sptr>(i++);
  bocl_mem_sptr exp_img_dim =pro.get_input<bocl_mem_sptr>(i++);
  vcl_string app_identifier = pro.get_input<vcl_string>(i++);

  //get scene data type and appTypeSize
  vcl_string data_type;
  int apptypesize; 
  vcl_vector<vcl_string> valid_types; 
  valid_types.push_back(boxm2_data_traits<BOXM2_MOG3_GREY>::prefix()); 
  valid_types.push_back(boxm2_data_traits<BOXM2_GAUSS_GREY>::prefix()); 
  valid_types.push_back(boxm2_data_traits<BOXM2_MOG3_GREY_16>::prefix()); 
  if( !boxm2_util::verify_appearance( *scene, valid_types, data_type, apptypesize ) ) {
    vcl_cout<<"boxm2_ocl_paint_batch ERROR: scene doesn't have BOXM2_MOG3_GREY or BOXM2_MOG3_GREY_16 data type"<<vcl_endl;
    return false;
  }

  //make sure apperance identifier is correct
  if (app_identifier.size() > 0) 
   data_type += "_" + app_identifier;
   
  //get initial options (MOG TYPE)
  vcl_string options = boxm2_ocl_util::mog_options(data_type);  

  //: create a command queue.
  int status=0;
  cl_command_queue queue = clCreateCommandQueue(device->context(),*(device->device_id()),
                                                CL_QUEUE_PROFILING_ENABLE,&status);
  if (status!=0) return false;
  vcl_string identifier=device->device_identifier()+options;

  // compile the kernel
  if (kernels.find(identifier)==kernels.end())
  {
    vcl_cout<<"===========Compiling kernels==========="<<vcl_endl;
    vcl_vector<bocl_kernel*> ks;
    compile_kernel(device,ks,options);
    kernels[identifier]=ks;
  }

  unsigned cl_ni=RoundUp(ni,lthreads[0]);
  unsigned cl_nj=RoundUp(nj,lthreads[1]);

  // visibility image
  float* vis_buff = new float[cl_ni*cl_nj];
  vcl_fill(vis_buff, vis_buff + cl_ni*cl_nj, 1.0f);
  bocl_mem_sptr vis_image = new bocl_mem(device->context(), vis_buff, cl_ni*cl_nj*sizeof(float), "exp image buffer");
  vis_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  // run expected image function
  float time = render_expected_image( scene, device, opencl_cache, queue,
                                      cam, exp_image, vis_image, exp_img_dim,
                                      data_type, kernels[identifier][0], lthreads, cl_ni, cl_nj, apptypesize);

  // normalize
  {
    vcl_size_t gThreads[] = {cl_ni,cl_nj};
    bocl_kernel* normalize_kern= kernels[identifier][1];
    normalize_kern->set_arg( exp_image.ptr() );
    normalize_kern->set_arg( vis_image.ptr() );
    normalize_kern->set_arg( exp_img_dim.ptr());
    normalize_kern->execute( queue, 2, lthreads, gThreads);
    clFinish(queue);

    //clear render kernel args so it can reset em on next execution
    normalize_kern->clear_args();
    time += normalize_kern->exec_time();
  }

  // read out expected image
  clReleaseCommandQueue(queue);
  
  delete[] vis_buff; 
  
  //store render time
  int argIdx = 0;
  pro.set_output_val<float>(argIdx, time);
  return true;
}
