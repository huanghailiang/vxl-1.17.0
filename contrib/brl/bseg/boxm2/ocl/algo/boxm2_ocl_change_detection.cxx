// This is brl/bseg/boxm2/ocl/algo/boxm2_ocl_change_detection.cxx
//:
// \file
// \brief  A process for change detection
//
// \author Vishal Jain
// \date Mar 10, 2011

#include "boxm2_ocl_change_detection.h"

#include <vcl_fstream.h>
#include <vcl_algorithm.h>
#include <vcl_sstream.h>
#include <boxm2/ocl/boxm2_opencl_cache.h>
#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_block.h>
#include <boxm2/boxm2_data_base.h>
#include <boxm2/ocl/boxm2_ocl_util.h>
#include <boxm2/boxm2_util.h>
#include <boxm2/ocl/algo/boxm2_ocl_camera_converter.h>
#include <vil/vil_image_view.h>
#include <vil/vil_save.h>
#include <vil/vil_pyramid_image_view.h>
#include <vil/vil_resample_bilin.h>

//directory utility
#include <vul/vul_timer.h>
#include <vcl_where_root_dir.h>
#include <bocl/bocl_device.h>
#include <bocl/bocl_kernel.h>
#include <boxm2/ocl/algo/boxm2_ocl_render_expected_image_function.h>

//using variables in global namespace
using namespace boxm2_ocl_change_detection_globals;

//--------------------------------------------------
//verifies data type for scene
//--------------------------------------------------
bool boxm2_ocl_change_detection_globals::get_scene_appearances( boxm2_scene_sptr    scene,
                                                                vcl_string&         data_type,
                                                                vcl_string&         num_obs_type,
                                                                vcl_string&         options,
                                                                int&                apptypesize)
{
  vcl_vector<vcl_string> apps = scene->appearances();
  bool foundDataType = false, foundNumObsType = false;
  for (unsigned int i=0; i<apps.size(); ++i) {
    if ( apps[i] == boxm2_data_traits<BOXM2_MOG3_GREY>::prefix() )
    {
      data_type = apps[i];
      foundDataType = true;
      options=" -D MOG_TYPE_8 ";
    }
    else if ( apps[i] == boxm2_data_traits<BOXM2_MOG3_GREY_16>::prefix() )
    {
      data_type = apps[i];
      foundDataType = true;
      options=" -D MOG_TYPE_16 ";
    }
    else if ( apps[i] == boxm2_data_traits<BOXM2_NUM_OBS>::prefix() )
    {
      num_obs_type = apps[i];
      foundNumObsType = true;
    }
  }
  if (!foundDataType) {
    vcl_cout<<"boxm2_ocl_change_detection_process ERROR: scene doesn't have BOXM2_MOG3_GREY or BOXM2_MOG3_GREY_16 data type"<<vcl_endl;
    return false;
  }
  if (!foundNumObsType) {
    vcl_cout<<"boxm2_ocl_change_detection_process ERROR: scene doesn't have BOXM2_NUM_OBS type"<<vcl_endl;
    return false;
  }

  //set apptype size
  apptypesize = (int) boxm2_data_info::datasize(data_type);
  return true;
}

//declare static map
vcl_map<vcl_string, vcl_vector<bocl_kernel*> > boxm2_ocl_change_detection::kernels_;


//main change detect function
bool boxm2_ocl_change_detection::change_detect( vil_image_view<float>&    change_img,
                                                vil_image_view<vxl_byte>& rgb_change_img,
                                                bocl_device_sptr          device,
                                                boxm2_scene_sptr          scene,
                                                boxm2_opencl_cache_sptr   opencl_cache,
                                                vpgl_camera_double_sptr   cam,
                                                vil_image_view_base_sptr  img,
                                                vil_image_view_base_sptr  exp_img,
                                                int                       n,
                                                vcl_string                norm_type,
                                                bool                      pmax )
{
  float transfer_time=0.0f;
  float gpu_time=0.0f;

  //img dimensions, global and local threads (will be correctly set)
  unsigned ni=img->ni();
  unsigned nj=img->nj();
  vcl_size_t  local_threads [2] = {8,8};
  vcl_size_t  global_threads[2] = {8,8};

  //---- get scene info -----
  bool foundDataType = false, foundNumObsType = false;
  vcl_string data_type,num_obs_type,options;
  int apptypesize;
  get_scene_appearances( scene, data_type, num_obs_type, options, apptypesize);

  //specify max mode options
  if ( pmax )
    options += " -D USE_MAX_MODE  ";

  //grab kernel
  vcl_vector<bocl_kernel*>& kerns = get_kernels(device, options);

  //create a command queue.
  int status=0;
  cl_command_queue queue = clCreateCommandQueue(device->context(),*(device->device_id()),
                                                CL_QUEUE_PROFILING_ENABLE,&status);
  if (status!=0)
    return false;

  //----- PREP INPUT BUFFERS -------------
  //prepare input images
  vil_image_view_base_sptr float_img     = boxm2_util::prepare_input_image(img, true); //true for force gray scale
  vil_image_view_base_sptr float_exp_img = boxm2_util::prepare_input_image(exp_img, true);
  vil_image_view<float>*   img_view      = static_cast<vil_image_view<float>* >(float_img.ptr());
  vil_image_view<float>*   exp_img_view  = static_cast<vil_image_view<float>* >(float_exp_img.ptr());

  //prepare workspace size
  unsigned cl_ni    = RoundUp(img_view->ni(),local_threads[0]);
  unsigned cl_nj    = RoundUp(img_view->nj(),local_threads[1]);
  global_threads[0] = cl_ni;
  global_threads[1] = cl_nj;

  // create all buffers
  cl_float* ray_origins = new cl_float[4*cl_ni*cl_nj];
  cl_float* ray_directions = new cl_float[4*cl_ni*cl_nj];
  bocl_mem_sptr ray_o_buff = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(cl_float4), ray_origins, "ray_origins buffer");
  bocl_mem_sptr ray_d_buff = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(cl_float4), ray_directions, "ray_directions buffer");
  boxm2_ocl_camera_converter::compute_ray_image( device, queue, cam, cl_ni, cl_nj, ray_o_buff, ray_d_buff);

  //prepare image buffers (cpu)
  float* vis_buff               = new float[cl_ni*cl_nj];
  float* exp_image_buff         = new float[cl_ni*cl_nj];
  float* change_image_buff      = new float[cl_ni*cl_nj];
  float* change_exp_image_buff  = new float[cl_ni*cl_nj];
  float* input_buff             = new float[4*cl_ni*cl_nj];
  full_pyramid(float_img, input_buff, cl_ni, cl_nj);

  for (unsigned i=0;i<cl_ni*cl_nj;i++) {
    vis_buff[i]=1.0f;
    change_image_buff[i]=0.0f;
    change_exp_image_buff[i]=0.0f;
  }
  int count=0;
  for (unsigned int j=0;j<cl_nj;++j) {
    for (unsigned int i=0;i<cl_ni;++i) {
      //input_buff[count] = 0.0f;
      exp_image_buff[count]=0.0;
      if (i<img_view->ni() && j< img_view->nj()) {
          //input_buff[count]=(*img_view)(i,j);
          exp_image_buff[count]=(*exp_img_view)(i,j);
      }
      ++count;
    }
  }

  //prepare image buffers (GPU)
  //bocl_mem_sptr in_image=new bocl_mem(device->context(),input_buff, 4*cl_ni*cl_nj*sizeof(float),"input image buffer");
  bocl_mem_sptr in_image = opencl_cache->alloc_mem(4*cl_ni*cl_nj*sizeof(float), input_buff, "input image buffer");
  in_image->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr exp_image=new bocl_mem(device->context(),exp_image_buff,cl_ni*cl_nj*sizeof(float),"expected image buffer");
  bocl_mem_sptr exp_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), exp_image_buff, "expected image buffer");
  exp_image->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr change_image=new bocl_mem(device->context(),change_image_buff,cl_ni*cl_nj*sizeof(float),"change image buffer");
  bocl_mem_sptr change_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), change_image_buff, "change image buffer");
  change_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr change_exp_image=new bocl_mem(device->context(),change_exp_image_buff,cl_ni*cl_nj*sizeof(float),"change exp image buffer");
  bocl_mem_sptr change_exp_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), change_exp_image_buff, "change exp image buffer"); 
  change_exp_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr vis_image=new bocl_mem(device->context(),vis_buff,cl_ni*cl_nj*sizeof(float),"vis image buffer");
  bocl_mem_sptr vis_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), vis_buff, "vis image buffer"); 
  vis_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  // Image Dimensions
  int img_dim_buff[] = { 0, 0, img_view->ni(), img_view->nj() };
  bocl_mem_sptr img_dim=new bocl_mem(device->context(), img_dim_buff, sizeof(int)*4, "image dims");
  img_dim->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  // Output Array
  float output_arr[100];
  for (int i=0; i<100; ++i) output_arr[i] = 0.0f;
  bocl_mem_sptr  cl_output=new bocl_mem(device->context(), output_arr, sizeof(float)*100, "output buffer");
  cl_output->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  // bit lookup buffer
  cl_uchar lookup_arr[256];
  boxm2_ocl_util::set_bit_lookup(lookup_arr);
  bocl_mem_sptr lookup=new bocl_mem(device->context(), lookup_arr, sizeof(cl_uchar)*256, "bit lookup buffer");
  lookup->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

  //----------------------------------------------------------------------------
  // STEP ONE: Do 1x1 Change Detection Pass
  //----------------------------------------------------------------------------
  //For each ID in the visibility order, grab that block
  vcl_vector<boxm2_block_id> vis_order = scene->get_vis_blocks(cam);
  vcl_vector<boxm2_block_id>::iterator id;
  for (id = vis_order.begin(); id != vis_order.end(); ++id)
  {
    //choose correct render kernel
    boxm2_block_metadata mdata = scene->get_block_metadata(*id);
    bocl_kernel* kern =  kerns[0];

    //write the image values to the buffer
    vul_timer transfer;
    bocl_mem* blk       = opencl_cache->get_block(*id);
    bocl_mem* blk_info  = opencl_cache->loaded_block_info();
    bocl_mem* alpha     = opencl_cache->get_data<BOXM2_ALPHA>(*id);
    bocl_mem* mog       = opencl_cache->get_data(*id,data_type);
    transfer_time += (float) transfer.all();

    ////3. SET args
    kern->set_arg( blk_info );
    kern->set_arg( blk );
    kern->set_arg( alpha );
    kern->set_arg( mog );
    //kern->set_arg( persp_cam.ptr() );
    kern->set_arg( ray_o_buff.ptr() );
    kern->set_arg( ray_d_buff.ptr() );
    kern->set_arg( in_image.ptr() );
    kern->set_arg( exp_image.ptr() );
    kern->set_arg( change_image.ptr() );
    kern->set_arg( change_exp_image.ptr() );
    kern->set_arg( img_dim.ptr());
    kern->set_arg( cl_output.ptr() );
    kern->set_arg( lookup.ptr() );
    kern->set_arg( vis_image.ptr() );

    //local tree , cumsum buffer, imindex buffer
    kern->set_local_arg( local_threads[0]*local_threads[1]*sizeof(cl_uchar16) );
    kern->set_local_arg( local_threads[0]*local_threads[1]*10*sizeof(cl_uchar) );
    kern->set_local_arg( local_threads[0]*local_threads[1]*sizeof(cl_int) );

    //execute kernel
    kern->execute(queue, 2, local_threads, global_threads);
    clFinish(queue);
    gpu_time += kern->exec_time();

    //clear render kernel args so it can reset em on next execution
    kern->clear_args();
  }
  change_image->read_to_buffer(queue);
  change_exp_image->read_to_buffer(queue);
  vis_image->read_to_buffer(queue);

  //----------------------------------------------------------------------------
  // STEP TWO: If adaptive do second pass
  //----------------------------------------------------------------------------
  if ( n > 1 )
  {
    //do some bookkeeping before second pass
    float* prob_change_buff = new float[cl_ni*cl_nj];
    count=0;
    for (unsigned int j=0; j<cl_nj; ++j) {
      for (unsigned int i=0; i<cl_ni; ++i) {
        float change             = change_image_buff[count];
        float vis                = vis_buff[count];
        float prob_change        = 1.0f / (1.0f + change+vis);

        //store prob change, initialize max_change
        prob_change_buff[count] = prob_change;

        //reset those that are high enough
        if (prob_change > PROB_THRESH) {
          change_image_buff[count]     = 0.0f;
          //change_exp_image_buff[count] = 0.0f;
        }

        //store pass one vis buff, used later in normalize
        ++count;
      }
    }
    //set the two change images back up
    change_image->write_to_buffer(queue);
    //change_exp_image->write_to_buffer(queue);

    //set prob_change cl buffer
    bocl_mem_sptr prob_change = new bocl_mem(device->context(),prob_change_buff,cl_ni*cl_nj*sizeof(float),"pass one prob change buffer");
    prob_change->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

    //run CD for each pixel in a nxn neighborhood
    int half = n/2;
    for (int oi=-half; oi<=half; ++oi)
    {
      for (int oj=-half; oj<=half; ++oj)
      {
        int oi_buff[1] = {oi};
        int oj_buff[1] = {oj};
        bocl_mem_sptr oi_mem = new bocl_mem(device->context(),oi_buff, sizeof(int),"offset i buffer");
        bocl_mem_sptr oj_mem = new bocl_mem(device->context(),oj_buff, sizeof(int),"offset j buffer");
        oi_mem->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
        oj_mem->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

        //create gpu buff for p2_vis
        float* p2_vis_buff = new float[cl_nj*cl_ni];
        vcl_fill(p2_vis_buff, p2_vis_buff+cl_nj*cl_ni, 1.0f);
        bocl_mem_sptr p2_vis      = new bocl_mem(device->context(),p2_vis_buff, cl_ni*cl_nj*sizeof(float),"pass one visibility buffer");
        p2_vis->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

        //create nxn change buffers
        float* nxn_change_buff       = new float[cl_ni*cl_nj];
        float* nxn_change_exp_buff   = new float[cl_ni*cl_nj];
        vcl_fill(nxn_change_buff, nxn_change_buff+cl_nj*cl_ni, 0.0f);
        vcl_fill(nxn_change_exp_buff, nxn_change_exp_buff+cl_nj*cl_ni, 0.0f);
        bocl_mem_sptr nxn_change     = new bocl_mem(device->context(),nxn_change_buff, cl_ni*cl_nj*sizeof(float),"pass two change buffer");
        bocl_mem_sptr nxn_change_exp = new bocl_mem(device->context(),nxn_change_exp_buff, cl_ni*cl_nj*sizeof(float),"pass two change exp buffer");
        nxn_change->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
        nxn_change_exp->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

        //run change detection per block
        for (id = vis_order.begin(); id != vis_order.end(); ++id)
        {
          //choose correct render kernel
          boxm2_block_metadata mdata = scene->get_block_metadata(*id);
          bocl_kernel*         kern  = kerns[1];

          //write the image values to the buffer
          vul_timer transfer;
          bocl_mem* blk       = opencl_cache->get_block(*id);
          bocl_mem* blk_info  = opencl_cache->loaded_block_info();
          bocl_mem* alpha     = opencl_cache->get_data<BOXM2_ALPHA>(*id);
          bocl_mem* mog       = opencl_cache->get_data(*id,data_type);
          transfer_time      += (float) transfer.all();

          ////3. SET args
          kern->set_arg( blk_info );
          kern->set_arg( blk );
          kern->set_arg( alpha );
          kern->set_arg( mog );
          kern->set_arg( oi_mem.ptr() );    //offset i
          kern->set_arg( oj_mem.ptr() );    //offset j
          //kern->set_arg( persp_cam.ptr() );
          kern->set_arg( ray_o_buff.ptr() );
          kern->set_arg( ray_d_buff.ptr() );  
          kern->set_arg( in_image.ptr() );
          kern->set_arg( exp_image.ptr() );
          kern->set_arg( nxn_change.ptr() );        //pass two change image
          kern->set_arg( nxn_change_exp.ptr() );    //pass two change image exp
          kern->set_arg( prob_change.ptr() );         //pass one probability of change (for adaptive-ness)
          kern->set_arg( img_dim.ptr());
          kern->set_arg( cl_output.ptr() );
          kern->set_arg( lookup.ptr() );
          kern->set_arg( p2_vis.ptr() );

          //local tree , cumsum buffer, imindex buffer
          kern->set_local_arg( local_threads[0]*local_threads[1]*sizeof(cl_uchar16) );
          kern->set_local_arg( local_threads[0]*local_threads[1]*10*sizeof(cl_uchar) );
          kern->set_local_arg( local_threads[0]*local_threads[1]*sizeof(cl_int) );

          //execute kernel
          kern->execute(queue, 2, local_threads, global_threads);
          clFinish(queue);
          gpu_time += kern->exec_time();

          //clear render kernel args so it can reset em on next execution
          kern->clear_args();
        }
        nxn_change->read_to_buffer(queue);
        nxn_change_exp->read_to_buffer(queue);

        //set max change image
        for (unsigned int i=0; i<cl_ni*cl_nj; ++i) {
          //if 1x1 change was high enough...
          if (prob_change_buff[i] > PROB_THRESH) {
            //hack for change image, only run change image buff over smaller window
            //if (oi >= -(half-1) && oi <=(half-1) && oj >= -(half-1) && oj <=(half-1))
            if (nxn_change_buff[i] > change_image_buff[i])
              change_image_buff[i] = nxn_change_buff[i];

            //hack run change_exp_image_buff over larger window
            if (nxn_change_exp_buff[i] < change_exp_image_buff[i])
              change_exp_image_buff[i] = nxn_change_exp_buff[i];
          }
        }

        //clean up this pass
        delete [] p2_vis_buff;
        delete [] nxn_change_buff;
        delete [] nxn_change_exp_buff;
      }
    }

    //clean up adaptive image buffers
    delete [] prob_change_buff;

    //write to change buffer
    change_image->write_to_buffer(queue);
    change_exp_image->write_to_buffer(queue);
  }

  //----------------------------------------------------------------------------
  // STEP THREE: Do normalize pass on change image
  //----------------------------------------------------------------------------
  bocl_kernel* normalize_change_kernel =  kerns[2];
  {
    // Image Dimensions
    int rbelief_buff[1] = {0};
    if (norm_type=="raybelief")
      rbelief_buff[0] = 1;
    bocl_mem_sptr rbelief = new bocl_mem(device->context(), rbelief_buff, sizeof(int), "rbelief buffer");
    rbelief->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

    normalize_change_kernel->set_arg( change_image.ptr() );
    normalize_change_kernel->set_arg( change_exp_image.ptr() );
    normalize_change_kernel->set_arg( vis_image.ptr() );
    normalize_change_kernel->set_arg( img_dim.ptr());
    normalize_change_kernel->set_arg( rbelief.ptr());
    normalize_change_kernel->execute( queue, 2, local_threads, global_threads);
    clFinish(queue);
    gpu_time += normalize_change_kernel->exec_time();

    //clear render kernel args so it can reset em on next execution
    normalize_change_kernel->clear_args();
  }
  // read out expected image
  change_image->read_to_buffer(queue);
  vcl_cout<<"Change Detection GPU Time: " << gpu_time << " ms" << vcl_endl;

  //----------------------------------------------------------------------------
  //prep output images
  //----------------------------------------------------------------------------
  //store change image
  for (unsigned c=0;c<nj;c++)
    for (unsigned r=0;r<ni;r++)
      change_img(r,c) = change_image_buff[c*cl_ni+r];

  //store rgb change image
  for (unsigned c=0; c<nj; c++) {
    for (unsigned r=0; r<ni; r++) {
      rgb_change_img(r,c,0) = (vxl_byte) ( change_image_buff[c*cl_ni+r] * 255.0f);
      rgb_change_img(r,c,1) = (vxl_byte) ( (*img_view)(r,c) * 255.0f/8.0f );
      rgb_change_img(r,c,2) = (vxl_byte) 0;
      rgb_change_img(r,c,3) = (vxl_byte) 255;
    }
  }

  //cleanup the image buffers
  delete [] ray_origins;
  delete [] ray_directions;
  delete [] change_image_buff;
  delete [] change_exp_image_buff;
  delete [] vis_buff;
  delete [] exp_image_buff;
  delete [] input_buff;
  opencl_cache->unref_mem(in_image.ptr()); 
  opencl_cache->unref_mem(vis_image.ptr());
  opencl_cache->unref_mem(exp_image.ptr());
  opencl_cache->unref_mem(change_image.ptr());
  opencl_cache->unref_mem(change_exp_image.ptr());
  opencl_cache->unref_mem(ray_o_buff.ptr());
  opencl_cache->unref_mem(ray_d_buff.ptr());
  clReleaseCommandQueue(queue);
  return true;
}


//-------------------------------------------------
// Creates full in image (float4 pixels, each along one level of the pyramid
//-------------------------------------------------
void boxm2_ocl_change_detection::full_pyramid(vil_image_view_base_sptr in_img, float* img_buff, unsigned cl_ni, unsigned cl_nj)
{
  // half the size of the previous image
  vil_pyramid_image_view<float> pyramid(in_img, 4);

  // resized images
  unsigned ni = in_img->ni(), nj = in_img->nj();
  vil_image_view<float> half(ni,nj), quarter(ni,nj), eighth(ni,nj);
  vil_resample_bilin(pyramid(1), half, ni, nj);
  vil_resample_bilin(pyramid(2), quarter, ni, nj);
  vil_resample_bilin(pyramid(3), eighth, ni, nj);
#if 0
  vil_save(half, "half.tiff");
  vil_save(quarter, "quarter.tiff");
  vil_save(eighth, "eighth.tiff");
#endif
  //load up each level of pixels
  int idx=0;
  for (unsigned c=0; c<cl_nj; ++c) {
    for (unsigned r=0; r<cl_ni; ++r) {
      if (c<nj && r<ni) {
        img_buff[idx] = pyramid(0)(r,c);
        img_buff[idx+1] = half(r,c);
        img_buff[idx+2] = quarter(r,c);
        img_buff[idx+3] = eighth(r,c);
      }
      idx += 4;
    }
  }
}


//---------------------------------------------------
// compiles, caches and returns list of kernels
//---------------------------------------------------
vcl_vector<bocl_kernel*>& boxm2_ocl_change_detection::get_kernels(bocl_device_sptr device, vcl_string opts)
{
  // check to see if this device has compiled kernels already
  vcl_string identifier = device->device_identifier() + opts;
  if (kernels_.find(identifier) != kernels_.end())
    return kernels_[identifier];

  //if not, compile and cache them
  vcl_cout<<"===========Compiling multi update kernels===========\n"
          <<"  for device: "<<device->device_identifier()<<vcl_endl;

  //gather all render sources... seems like a lot for rendering...
  vcl_vector<vcl_string> src_paths;
  vcl_string source_dir = boxm2_ocl_util::ocl_src_root();
  src_paths.push_back(source_dir + "scene_info.cl");
  src_paths.push_back(source_dir + "cell_utils.cl");
  src_paths.push_back(source_dir + "bit/bit_tree_library_functions.cl");
  src_paths.push_back(source_dir + "backproject.cl");
  src_paths.push_back(source_dir + "statistics_library_functions.cl");
  src_paths.push_back(source_dir + "expected_functor.cl");
  src_paths.push_back(source_dir + "ray_bundle_library_opt.cl");
  src_paths.push_back(source_dir + "change/change_detection.cl");
  src_paths.push_back(source_dir + "bit/cast_ray_bit.cl");

  //set kernel options
  opts += " -D CHANGE -D DETERMINISTIC ";
  vcl_string options=opts;
  //opts += " -D STEP_CELL=step_cell_change_detection_uchar8_w_expected(aux_args.mog,aux_args.alpha,data_ptr,d*linfo->block_len,vis,aux_args.change,aux_args.change_exp,aux_args.intensity,aux_args.intensity_exp,bit_index) ";
  opts += " -D STEP_CELL=step_cell_change(aux_args,data_ptr,llid,d*linfo->block_len) ";

  //have kernel construct itself using the context and device
  bocl_kernel * ray_trace_kernel = new bocl_kernel();
  ray_trace_kernel->create_kernel( &device->context(),
                                   device->device_id(),
                                   src_paths,
                                   "change_detection_bit_scene",   //kernel name
                                   opts,              //options
                                   "boxm2 1x1 ocl change detection"); //kernel identifier (for error checking)

  //create nxn kernel
  vcl_stringstream pthresh;
  pthresh<<" -D PROB_THRESH="<<PROB_THRESH<<"  ";
  opts += pthresh.str();
  bocl_kernel * nxn_kernel = new bocl_kernel();
  nxn_kernel->create_kernel( &device->context(),
                             device->device_id(),
                             src_paths,
                             "nxn_change_detection",
                             opts,
                             "boxm2 nxn ocl change detection kernel");

  //create normalize image kernel
  vcl_vector<vcl_string> norm_src_paths;
  norm_src_paths.push_back(source_dir + "pixel_conversion.cl");
  norm_src_paths.push_back(source_dir + "bit/normalize_kernels.cl");
  bocl_kernel * normalize_render_kernel=new bocl_kernel();
  normalize_render_kernel->create_kernel( &device->context(),
                                          device->device_id(),
                                          norm_src_paths,
                                          "normalize_change_kernel",   //kernel name
                                          options,              //options
                                          "normalize change detection kernel"); //kernel identifier (for error checking)

  //store in a vector in the map and return
  vcl_vector<bocl_kernel*> vec_kernels;
  vec_kernels.push_back(ray_trace_kernel);
  vec_kernels.push_back(nxn_kernel);
  vec_kernels.push_back(normalize_render_kernel);
  kernels_[identifier] = vec_kernels;
  return kernels_[identifier];
}


//------------------------------------------------------
// calculates mutual information for a set of 2d samples
// of pixel values
//------------------------------------------------------
double boxm2_ocl_change_detection::mutual_information_2d(const vnl_vector<double>& X, const vnl_vector<double>& Y, int nbins)
{
  double scl = 1.0/(256.0/nbins);

  //compute the intensity histogram
  vbl_array_2d<double> p_xy(nbins, nbins, 0.0);
  unsigned nr = (unsigned)p_xy.rows(),
           nc = (unsigned)p_xy.cols();
  double total_weight = 0.0;
  for (unsigned i = 0; i<X.size(); ++i) {
    //match the gpu implementation, which does a floor opedration
    unsigned ix = static_cast<unsigned>(vcl_floor(X[i]*scl)),
             iy = static_cast<unsigned>(vcl_floor(Y[i]*scl));
    if (ix+1>(unsigned)nbins || iy+1>(unsigned)nbins)
      continue;
    p_xy[ix][iy] += 1.0;
    total_weight += 1.0;
  }

  // convert to probability
  for (int r = 0; r<nbins; ++r)
    for (int c = 0; c<nbins; ++c)
      p_xy[r][c] /= total_weight;

  // marginalize x,y to get p(x) and p(y)
  vbl_array_1d<double> px(nc,0.0);
  for (unsigned c = 0; c<nc; ++c)
    for (unsigned r = 0; r<nr; ++r)
      px[c] += p_xy[r][c];

  vbl_array_1d<double> py(nr,0.0);
  for (unsigned r = 0; r<nr; ++r)
    for (unsigned c = 0; c<nc; ++c)
      py[r] += p_xy[r][c];

  //calculate entropy of p(x) and p(y)
  double H_x = 0.0;
  for (unsigned c = 0; c<nc; ++c) {
    double prob = px[c];
    if (prob>0.0)
      H_x += prob*vcl_log(prob);
  }

  double H_y = 0.0;
  for (unsigned r = 0; r<nr; ++r) {
    double prob = py[r];
    if (prob>0.0)
      H_y += prob*vcl_log(prob);
  }

  //calculate joint entropy H(x,y)
  double H_xy = 0.0;
  for (unsigned r = 0; r<nr; ++r) {
    for (unsigned c = 0; c<nc; ++c) {
        double prc = p_xy[r][c];
        if (prc>0)
          H_xy += prc*vcl_log(prc);
    }
  }

  //calculate MI = H(X) + H(Y) - H(X,Y)
  double MI = H_x + H_y - H_xy;
  return -MI/vcl_log(2.0);
}


//==============================================================================
//Two pass change detection class methods
//==============================================================================
vcl_map<vcl_string, vcl_vector<bocl_kernel*> > boxm2_ocl_two_pass_change::kernels_;

bool boxm2_ocl_two_pass_change::change_detect(vil_image_view<float>&    change_img,
                                              bocl_device_sptr          device,
                                              boxm2_scene_sptr          scene,
                                              boxm2_opencl_cache_sptr   opencl_cache,
                                              vpgl_camera_double_sptr   cam,
                                              vil_image_view_base_sptr  img,
                                              vil_image_view_base_sptr  exp_img,
                                              int                       n,
                                              vcl_string                norm_type,
                                              bool                      pmax )
{
  float transfer_time=0.0f;
  float gpu_time=0.0f;

  //img dimensions, global and local threads (will be correctly set)
  unsigned ni=img->ni();
  unsigned nj=img->nj();
  vcl_size_t  local_threads [2] = {8,8};
  vcl_size_t  global_threads[2] = {8,8};

  //---- get scene info -----
  bool foundDataType = false, foundNumObsType = false;
  vcl_string data_type,num_obs_type,options;
  int apptypesize;
  get_scene_appearances( scene, data_type, num_obs_type, options, apptypesize);

  //specify max mode options
  if ( pmax )
    options += " -D USE_MAX_MODE  ";

  //grab kernel
  vcl_vector<bocl_kernel*>& kerns = get_kernels(device, options);

  //create a command queue.
  int status=0;
  cl_command_queue queue = clCreateCommandQueue(device->context(),*(device->device_id()),
                                                CL_QUEUE_PROFILING_ENABLE,&status);
  if (status!=0)
    return false;

  //----- PREP INPUT BUFFERS -------------
  //prepare input images
  vil_image_view_base_sptr float_img     = boxm2_util::prepare_input_image(img, true); //true for force gray scale
  vil_image_view_base_sptr float_exp_img = boxm2_util::prepare_input_image(exp_img, true);
  vil_image_view<float>*   img_view      = static_cast<vil_image_view<float>* >(float_img.ptr());
  vil_image_view<float>*   exp_img_view  = static_cast<vil_image_view<float>* >(float_exp_img.ptr());

  //prepare workspace size
  unsigned cl_ni    = RoundUp(img_view->ni(),local_threads[0]);
  unsigned cl_nj    = RoundUp(img_view->nj(),local_threads[1]);
  global_threads[0] = cl_ni;
  global_threads[1] = cl_nj;
  
  //set generic cam
  cl_float* ray_origins    = new cl_float[4*cl_ni*cl_nj];
  cl_float* ray_directions = new cl_float[4*cl_ni*cl_nj];
  bocl_mem_sptr ray_o_buff = new bocl_mem(device->context(), ray_origins,   cl_ni*cl_nj * sizeof(cl_float4), "ray_origins buffer");
  bocl_mem_sptr ray_d_buff = new bocl_mem(device->context(), ray_directions,cl_ni*cl_nj * sizeof(cl_float4), "ray_directions buffer");
  boxm2_ocl_camera_converter::compute_ray_image( device, queue, cam, cl_ni, cl_nj, ray_o_buff, ray_d_buff);

  //prepare image buffers (cpu)
  float* vis_buff               = new float[cl_ni*cl_nj];
  float* true_vis               = new float[cl_ni*cl_nj]; 
  float* exp_image_buff         = new float[cl_ni*cl_nj];
  float* change_image_buff      = new float[cl_ni*cl_nj];
  float* change_exp_image_buff  = new float[cl_ni*cl_nj];
  float* input_buff             = new float[cl_ni*cl_nj];
  float* max_background_buff    = new float[cl_ni*cl_nj]; 
  float* min_prob_exp_buff      = new float[cl_ni*cl_nj];
  vxl_byte* mask_buff           = new vxl_byte[cl_ni*cl_nj];  
  for (unsigned i=0;i<cl_ni*cl_nj;i++) {
    vis_buff[i]              = 1.0f;
    change_image_buff[i]     = 0.0f;
    change_exp_image_buff[i] = 0.0f;
    max_background_buff[i]   = 0.0f; 
    min_prob_exp_buff[i]     = 1.0f; 
    mask_buff[i]             = (vxl_byte) 1; 
  }
  int count=0;
  for (unsigned int j=0;j<cl_nj;++j) {
    for (unsigned int i=0;i<cl_ni;++i) {
      input_buff[count] = 0.0f;
      exp_image_buff[count]=0.0;
      if (i<img_view->ni() && j< img_view->nj()) {
          input_buff[count]     = (*img_view)(i,j);
          exp_image_buff[count] = (*exp_img_view)(i,j);
      }
      ++count;
    }
  }
  
  //prepare image buffers (GPU)
  bocl_mem_sptr in_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), input_buff, "input image buffer");
  in_image->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr exp_image=new bocl_mem(device->context(),exp_image_buff,cl_ni*cl_nj*sizeof(float),"expected image buffer");
  bocl_mem_sptr exp_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), exp_image_buff, "expected image buffer");
  exp_image->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr change_image=new bocl_mem(device->context(),change_image_buff,cl_ni*cl_nj*sizeof(float),"change image buffer");
  bocl_mem_sptr change_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), change_image_buff, "change image buffer");
  change_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr change_exp_image=new bocl_mem(device->context(),change_exp_image_buff,cl_ni*cl_nj*sizeof(float),"change exp image buffer");
  bocl_mem_sptr change_exp_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), change_exp_image_buff, "change exp image buffer"); 
  change_exp_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr vis_image=new bocl_mem(device->context(),vis_buff,cl_ni*cl_nj*sizeof(float),"vis image buffer");
  bocl_mem_sptr vis_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), vis_buff, "vis image buffer"); 
  vis_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  //bocl_mem_sptr mask_image = new bocl_mem(device->context(),mask_buff,cl_ni*cl_nj*sizeof(vxl_byte),"change mask image buffer");
  bocl_mem_sptr mask_image = opencl_cache->alloc_mem(cl_ni*cl_nj*sizeof(vxl_byte), mask_buff, "change mask image buffer");
  mask_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  // Image Dimensions
  int img_dim_buff[] = { 0, 0, img_view->ni(), img_view->nj() };
  bocl_mem_sptr img_dim=new bocl_mem(device->context(), img_dim_buff, sizeof(int)*4, "image dims");
  img_dim->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  // Output Array
  float* output_arr = new float[cl_ni*cl_nj]; 
  bocl_mem_sptr cl_output=new bocl_mem(device->context(), output_arr, sizeof(float)*cl_ni*cl_nj, "output buffer");
  cl_output->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

  // bit lookup buffer
  cl_uchar lookup_arr[256];
  boxm2_ocl_util::set_bit_lookup(lookup_arr);
  bocl_mem_sptr lookup=new bocl_mem(device->context(), lookup_arr, sizeof(cl_uchar)*256, "bit lookup buffer");
  lookup->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

  //----------------------------------------------------------------------------
  //run CD for each pixel in a nxn neighborhood
  //----------------------------------------------------------------------------
  //--- order offsets so you do 0,0 first -----
  int half = n/2;
  vcl_vector<int> ois; ois.push_back(0); 
  vcl_vector<int> ojs; ojs.push_back(0); 
  for (int o=-half; o<=half; ++o) {
    if(o==0) continue;
    ois.push_back(o); 
    ojs.push_back(o); 
  }
  
  //iterate over offsets i, offsets j
  for (int offi=0; offi<ois.size(); ++offi)
  {
    for (int offj=0; offj<ojs.size(); ++offj)
    {
      int oi = ois[offi]; 
      int oj = ojs[offj]; 
      vcl_cout<<"CHANGE OFFSET ("<<oi<<","<<oj<<")"<<vcl_endl;
      bocl_mem_sptr oi_mem = new bocl_mem(device->context(), &oi, sizeof(int),"offset i buffer");
      bocl_mem_sptr oj_mem = new bocl_mem(device->context(), &oj, sizeof(int),"offset j buffer");
      oi_mem->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
      oj_mem->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

      //----- STEP ONE: per cell mean obs pass ---------
      //For each ID in the visibility order, grab that block
      vcl_vector<boxm2_block_id> vis_order = scene->get_vis_blocks( cam );
      vcl_vector<boxm2_block_id>::iterator id;
      vcl_cout<<"  STEP ONE "<<vcl_endl;
      for (id = vis_order.begin(); id != vis_order.end(); ++id)
      {
        //choose correct render kernel
        boxm2_block_metadata mdata = scene->get_block_metadata(*id);
        bocl_kernel* kern =  kerns[0];

        //-----write the image values to the buffer
        vul_timer transfer;
        bocl_mem* blk       = opencl_cache->get_block(*id);
        bocl_mem* blk_info  = opencl_cache->loaded_block_info();
        bocl_mem* alpha     = opencl_cache->get_data<BOXM2_ALPHA>(*id);
        
        // aux buffers (determine length first)
        boxm2_scene_info* info_buffer = (boxm2_scene_info*) blk_info->cpu_buffer();
        int alphaTypeSize = (int) boxm2_data_info::datasize(boxm2_data_traits<BOXM2_ALPHA>::prefix());
        info_buffer->data_buffer_length = (int) (alpha->num_bytes()/alphaTypeSize);
        blk_info->write_to_buffer(queue);    
        int auxTypeSize = (int) boxm2_data_info::datasize(boxm2_data_traits<BOXM2_AUX0>::prefix());
        bocl_mem *aux0  = opencl_cache->get_data<BOXM2_AUX0>(*id, info_buffer->data_buffer_length*auxTypeSize);
        auxTypeSize     = (int) boxm2_data_info::datasize(boxm2_data_traits<BOXM2_AUX1>::prefix());
        bocl_mem *aux1  = opencl_cache->get_data<BOXM2_AUX1>(*id, info_buffer->data_buffer_length*auxTypeSize);
        transfer_time += (float) transfer.all();

        //3. SET args
        kern->set_arg( blk_info );
        kern->set_arg( oi_mem.ptr() ); 
        kern->set_arg( oj_mem.ptr() ); 
        kern->set_arg( blk );
        kern->set_arg( alpha );
        kern->set_arg( aux0 ); 
        kern->set_arg( aux1 ); 
        kern->set_arg( lookup.ptr() ); 
        kern->set_arg( ray_o_buff.ptr() ); 
        kern->set_arg( ray_d_buff.ptr() ); 
        kern->set_arg( img_dim.ptr());
        kern->set_arg( in_image.ptr() );
        kern->set_arg( cl_output.ptr() );

        //local tree , cumsum buffer, imindex buffer
        kern->set_local_arg( local_threads[0]*local_threads[1]*sizeof(cl_uchar16) );
        kern->set_local_arg( local_threads[0]*local_threads[1]*10*sizeof(cl_uchar) );

        //execute kernel
        kern->execute(queue, 2, local_threads, global_threads);
        clFinish(queue);
        gpu_time += kern->exec_time();

        //clear render kernel args so it can reset em on next execution
        kern->clear_args();
      }
      
      //----- STEP TWO: prob background pas --------
      vcl_cout<<"  STEP TWO CHANGE PASS"<<vcl_endl;
      for (id = vis_order.begin(); id != vis_order.end(); ++id)
      {
        //choose correct render kernel
        boxm2_block_metadata mdata = scene->get_block_metadata(*id);
        bocl_kernel* kern =  kerns[1];

        //-----write the image values to the buffer
        vul_timer transfer;
        bocl_mem* blk       = opencl_cache->get_block(*id);
        bocl_mem* blk_info  = opencl_cache->loaded_block_info();
        bocl_mem* alpha     = opencl_cache->get_data<BOXM2_ALPHA>(*id);
        bocl_mem* mog       = opencl_cache->get_data(*id,data_type);
        
        // aux buffers (determine length first)
        boxm2_scene_info* info_buffer = (boxm2_scene_info*) blk_info->cpu_buffer();
        int alphaTypeSize = (int) boxm2_data_info::datasize(boxm2_data_traits<BOXM2_ALPHA>::prefix());
        info_buffer->data_buffer_length = (int) (alpha->num_bytes()/alphaTypeSize);
        blk_info->write_to_buffer((queue));    
        int auxTypeSize = (int) boxm2_data_info::datasize(boxm2_data_traits<BOXM2_AUX0>::prefix());
        bocl_mem *aux0  = opencl_cache->get_data<BOXM2_AUX0>(*id, info_buffer->data_buffer_length*auxTypeSize);
        auxTypeSize     = (int) boxm2_data_info::datasize(boxm2_data_traits<BOXM2_AUX1>::prefix());
        bocl_mem *aux1  = opencl_cache->get_data<BOXM2_AUX1>(*id, info_buffer->data_buffer_length*auxTypeSize);
        transfer_time += (float) transfer.all();

        //3. SET args
        kern->set_arg( blk_info ); 
        kern->set_arg( oi_mem.ptr() ); 
        kern->set_arg( oj_mem.ptr() ); 
        kern->set_arg( blk );
        kern->set_arg( alpha );
        kern->set_arg( mog );
        kern->set_arg( aux0 ); 
        kern->set_arg( aux1 ); 
        kern->set_arg( ray_o_buff.ptr() ); 
        kern->set_arg( ray_d_buff.ptr() );     
        kern->set_arg( in_image.ptr() );
        kern->set_arg( exp_image.ptr() );
        kern->set_arg( change_image.ptr() );
        kern->set_arg( change_exp_image.ptr() );
        kern->set_arg( img_dim.ptr());
        kern->set_arg( cl_output.ptr() );
        kern->set_arg( lookup.ptr() );
        kern->set_arg( vis_image.ptr() );
        
        //local tree , cumsum buffer, imindex buffer
        kern->set_local_arg( local_threads[0]*local_threads[1]*sizeof(cl_uchar16) );
        kern->set_local_arg( local_threads[0]*local_threads[1]*10*sizeof(cl_uchar) );
        kern->set_local_arg( local_threads[0]*local_threads[1]*sizeof(cl_int) );

        //execute kernel
        kern->execute(queue, 2, local_threads, global_threads);
        clFinish(queue);
        gpu_time += kern->exec_time();

        //clear render kernel args so it can reset em on next execution
        kern->clear_args();
      }
      change_image->read_to_buffer(queue);
      change_exp_image->read_to_buffer(queue);
      vis_image->read_to_buffer(queue);
     
      //----- STEP THREE: choose pixel-wise max background----
      // choose max change, min ray prob pixels
      for (unsigned int i=0; i<cl_ni*cl_nj; ++i) {
        if(change_image_buff[i] > max_background_buff[i])
          max_background_buff[i] = change_image_buff[i]; 
        if(change_exp_image_buff[i] < min_prob_exp_buff[i])
          min_prob_exp_buff[i] = change_exp_image_buff[i]; 
      }
      
      //----- store true vis -----
      if(oi==0 && oj==0)
        for(unsigned i=0; i<cl_ni*cl_nj; ++i) 
          true_vis[i] = vis_buff[i]; 
          
      //----- update mask image ------
      for(int i=0; i<cl_ni*cl_nj; ++i) {
        if(mask_buff[i]) {
          float change      = max_background_buff[count];
          float vis         = true_vis[count];
          float prob_change = 1.0f / (1.0f + change+vis);
          if(prob_change < PROB_THRESH)
            mask_buff[i] = (vxl_byte) 0; 
        }
      }
      mask_image->write_to_buffer(queue); 
    
      //----------------------------------------------------------------------------
      // ZERO OUT auxiliary data, reset vis and change buffers
      //----------------------------------------------------------------------------
      for (id = vis_order.begin(); id != vis_order.end(); ++id){
        //choose correct render kernel
        bocl_mem *aux0  = opencl_cache->get_data<BOXM2_AUX0>(*id);
        bocl_mem *aux1  = opencl_cache->get_data<BOXM2_AUX1>(*id);
        aux0->zero_gpu_buffer(queue); 
        aux1->zero_gpu_buffer(queue); 
      }
      for (unsigned i=0;i<cl_ni*cl_nj; ++i) {
        vis_buff[i]              = 1.0f;
        change_image_buff[i]     = 0.0f;
        change_exp_image_buff[i] = 0.0f;
      }
      vis_image->write_to_buffer(queue); 
      change_image->write_to_buffer(queue); 
      change_exp_image->write_to_buffer(queue); 
    }
  }
  
  //set change and change exp to max/min buffs
  for(unsigned i=0; i<cl_ni*cl_nj; ++i) {
    change_image_buff[i] = max_background_buff[i]; 
    change_exp_image_buff[i] = min_prob_exp_buff[i]; 
  }
  change_image->write_to_buffer(queue); 
  change_exp_image->write_to_buffer(queue); 
  
#if 1
  vil_image_view<float> pre_norm(cl_ni,cl_nj); 
  int c=0; 
  for(int j=0; j<cl_nj; ++j)
    for(int i=0; i<cl_ni; ++i)
      pre_norm(i,j) = change_exp_image_buff[c++];
  vil_save(pre_norm, "change_exp_image.tiff");
#endif

  //----------------------------------------------------------------------------
  // STEP THREE: Do normalize pass on change image
  //----------------------------------------------------------------------------
  bocl_kernel* normalize_change_kernel =  kerns[2];
  {
    // Image Dimensions
    int rbelief_buff[1] = {1};
    bocl_mem_sptr rbelief = new bocl_mem(device->context(), rbelief_buff, sizeof(int), "rbelief buffer");
    rbelief->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    
    //true vis buff
    bocl_mem_sptr vis_two = new bocl_mem(device->context(), true_vis, sizeof(float)*cl_ni*cl_nj, "oi=0,oj=0 visibility"); 
    vis_two->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR); 

    normalize_change_kernel->set_arg( change_image.ptr() );
    normalize_change_kernel->set_arg( change_exp_image.ptr() );
    normalize_change_kernel->set_arg( vis_two.ptr() );
    normalize_change_kernel->set_arg( img_dim.ptr());
    normalize_change_kernel->set_arg( rbelief.ptr());
    normalize_change_kernel->execute( queue, 2, local_threads, global_threads);
    clFinish(queue);
    gpu_time += normalize_change_kernel->exec_time();

    //clear render kernel args so it can reset em on next execution
    normalize_change_kernel->clear_args();
  }
  // read out expected image
  change_image->read_to_buffer(queue);
  vcl_cout<<"Change Detection GPU Time: " << gpu_time << " ms" << vcl_endl;

  //----------------------------------------------------------------------------
  //prep output images
  //----------------------------------------------------------------------------
  //store change image
  for (unsigned c=0;c<nj;c++)
    for (unsigned r=0;r<ni;r++)
      change_img(r,c) = change_image_buff[c*cl_ni+r];

  //cleanup the image buffers
  delete [] vis_buff;
  delete [] true_vis; 
  delete [] exp_image_buff;
  delete [] input_buff;
  delete [] output_arr;
  delete [] change_image_buff;
  delete [] change_exp_image_buff; 
  delete [] max_background_buff; 
  delete [] min_prob_exp_buff; 
  delete [] mask_buff; 
  opencl_cache->unref_mem(in_image.ptr()); 
  opencl_cache->unref_mem(vis_image.ptr());
  opencl_cache->unref_mem(exp_image.ptr());
  opencl_cache->unref_mem(change_image.ptr());
  opencl_cache->unref_mem(change_exp_image.ptr());
  opencl_cache->unref_mem(mask_image.ptr());
  clReleaseCommandQueue(queue);
  return true;
}

vcl_vector<bocl_kernel*>& boxm2_ocl_two_pass_change::get_kernels(bocl_device_sptr device, vcl_string opts)
{
  // check to see if this device has compiled kernels already
  vcl_string identifier = device->device_identifier() + opts;
  if (kernels_.find(identifier) != kernels_.end())
    return kernels_[identifier];

  //if not, compile and cache them
  vcl_cout<<"===========Compiling two pass change kernels===========\n"
          <<"  for device: "<<device->device_identifier()<<vcl_endl;

  //gather all render sources... seems like a lot for rendering...
  vcl_vector<vcl_string> src_paths;
  vcl_string source_dir = boxm2_ocl_util::ocl_src_root();
  src_paths.push_back(source_dir + "scene_info.cl");
  src_paths.push_back(source_dir + "cell_utils.cl");
  src_paths.push_back(source_dir + "bit/bit_tree_library_functions.cl");
  src_paths.push_back(source_dir + "backproject.cl");
  src_paths.push_back(source_dir + "statistics_library_functions.cl");
  src_paths.push_back(source_dir + "expected_functor.cl");
  src_paths.push_back(source_dir + "ray_bundle_library_opt.cl");
  src_paths.push_back(source_dir + "change/two_pass_change.cl");
  src_paths.push_back(source_dir + "bit/cast_ray_bit.cl");

  //pass one, seglen
  vcl_string seg_options = opts + " -D CHANGE_SEGLEN -D STEP_CELL=step_cell_seglen(aux_args,data_ptr,llid,d) "; 
  bocl_kernel* seg_len = new bocl_kernel();
  seg_len->create_kernel( &device->context(),
                           device->device_id(),
                           src_paths,
                           "change_seg_len",   //kernel name
                           seg_options,              //options
                           "boxm2 change seg len (pass 1)"); //kernel identifier (for error checking)

  //pass two, change detection
  vcl_string change_options = opts + " -D CHANGE -D STEP_CELL=step_cell_change(aux_args,data_ptr,llid,d) "; 
  bocl_kernel* change_kernel = new bocl_kernel();
  change_kernel->create_kernel( &device->context(),
                                 device->device_id(),
                                 src_paths,
                                 "two_pass_change_kernel",   //kernel name
                                 change_options,              //options
                                 "boxm2 two pass change kernel (pass 2)"); //kernel identifier (for error checking)

  //create nxn kernel
  //vcl_stringstream pthresh;
  //pthresh<<" -D PROB_THRESH="<<PROB_THRESH<<"  ";
  //opts += pthresh.str();
  //bocl_kernel * nxn_kernel = new bocl_kernel();
  //nxn_kernel->create_kernel( &device->context(),
                             //device->device_id(),
                             //src_paths,
                             //"nxn_change_detection",
                             //opts,
                             //"boxm2 nxn ocl change detection kernel");

  //create normalize image kernel
  vcl_vector<vcl_string> norm_src_paths;
  norm_src_paths.push_back(source_dir + "pixel_conversion.cl");
  norm_src_paths.push_back(source_dir + "bit/normalize_kernels.cl");
  vcl_string norm_opts = opts + " -D CHANGE ";
  bocl_kernel * normalize_render_kernel=new bocl_kernel();
  normalize_render_kernel->create_kernel( &device->context(),
                                          device->device_id(),
                                          norm_src_paths,
                                          "normalize_change_kernel",   //kernel name
                                           norm_opts,              //options
                                          "normalize change detection kernel"); //kernel identifier (for error checking)

  //store in a vector in the map and return
  vcl_vector<bocl_kernel*> vec_kernels;
  vec_kernels.push_back(seg_len);
  vec_kernels.push_back(change_kernel);
  vec_kernels.push_back(normalize_render_kernel);
  kernels_[identifier] = vec_kernels;
  return kernels_[identifier];
}


