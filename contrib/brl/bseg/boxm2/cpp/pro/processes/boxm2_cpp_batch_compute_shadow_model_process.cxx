// This is brl/bseg/boxm2/cpp/pro/processes/boxm2_cpp_batch_compute_shadow_model_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file
// \brief  Processes to update the scene using a set of data blocks in a batch mode
//
// \author Ozge C. Ozcanli
// \date May 12, 2011

#include <vcl_fstream.h>
#include <boxm2/io/boxm2_stream_cache.h>
#include <boxm2/io/boxm2_cache.h>
#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_block.h>
#include <boxm2/boxm2_data_base.h>

//brdb stuff
#include <brdb/brdb_value.h>
#include <boxm2/boxm2_util.h>

#include <boxm2/cpp/algo/boxm2_shadow_model_functor.h>
#include <boxm2/cpp/algo/boxm2_data_serial_iterator.h>
//: run batch update
namespace boxm2_cpp_batch_compute_shadow_model_process_globals
{
  const unsigned n_inputs_ = 3;
  const unsigned n_outputs_ = 1;
}

bool boxm2_cpp_batch_compute_shadow_model_process_cons(bprb_func_process& pro)
{
  using namespace boxm2_cpp_batch_compute_shadow_model_process_globals;

  //process takes 3 inputs
  // 0) scene
  // 1) cache
  // 2) stream cache
  vcl_vector<vcl_string> input_types_(n_inputs_);
  input_types_[0] = "boxm2_scene_sptr";
  input_types_[1] = "boxm2_cache_sptr";
  input_types_[2] = "boxm2_stream_cache_sptr";
  // process has 1 output (ambient light)
  vcl_vector<vcl_string>  output_types_(n_outputs_);
  output_types_[0] = "float";

  return pro.set_input_types(input_types_) && pro.set_output_types(output_types_);
}

bool boxm2_cpp_batch_compute_shadow_model_process(bprb_func_process& pro)
{
  using namespace boxm2_cpp_batch_compute_shadow_model_process_globals;

  if ( pro.n_inputs() < n_inputs_ ) {
      vcl_cout << pro.name() << ": The number of inputs should be " << n_inputs_<< vcl_endl;
      return false;
  }
  //get the inputs
  unsigned i = 0;
  boxm2_scene_sptr scene =pro.get_input<boxm2_scene_sptr>(i++);
  boxm2_cache_sptr cache= pro.get_input<boxm2_cache_sptr>(i++);
  boxm2_stream_cache_sptr str_cache = pro.get_input<boxm2_stream_cache_sptr>(i++);

  vcl_string data_type;
  bool foundDataType = false;
  vcl_vector<vcl_string> apps = scene->appearances();
  int appTypeSize;
  for (unsigned int i=0; i<apps.size(); ++i) {
    if ( apps[i] == boxm2_data_traits<BOXM2_MOG3_GREY>::prefix() )
    {
      data_type = apps[i];
      foundDataType = true;
      appTypeSize = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_MOG3_GREY>::prefix());
    }
  }
  if (!foundDataType) {
    vcl_cout<<"boxm2_cpp_batch_compute_shadow_model_process ERROR: scene doesn't have BOXM2_MOG3_GREY or BOXM2_MOG3_GREY_16 data type"<<vcl_endl;
    return false;
  }

  // assumes that the data of each image has been created in the data models previously
  // iterate the scene block by block and write to output
  vcl_vector<boxm2_block_id> blk_ids = scene->get_block_ids();
  vcl_vector<boxm2_block_id>::iterator id;
  id = blk_ids.begin();

  float weights=0.0;
  float weighted_intensities=0.0;
  float ambient_light=0.0;
  for (id = blk_ids.begin(); id != blk_ids.end(); id++) {
    boxm2_block     *  blk   = cache->get_block(*id);
    // pass num_bytes = 0 to make sure disc is read if not already in memory
    boxm2_data_base *  sunvis  = cache->get_data_base(*id,boxm2_data_traits<BOXM2_AUX0>::prefix("sunvis"),0,true);
    //vcl_cout << "buffer length of sunvis: " << sunvis->buffer_length() << '\n';
    boxm2_compute_ambient_functor data_functor;
    data_functor.init_data(sunvis, str_cache,weights,weighted_intensities, float(blk->sub_block_dim().x()), blk->max_level());
    int sunvisTypeSize = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_AUX0>::prefix());
    int data_buff_length = (int) (sunvis->buffer_length()/sunvisTypeSize);
    boxm2_data_serial_iterator<boxm2_compute_ambient_functor>(data_buff_length,data_functor);
  }
  if (weights<1e-10f)
    ambient_light = weighted_intensities/weights;

  pro.set_output_val<float>(0, ambient_light);
  return true;
}

