// This is brl/bseg/boxm2/view/pro/processes/boxm2_view_trajectory_next_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file
// \brief A process for getting the next camera in a boxm2_trajectory object 
//
// \author Andrew Miller
// \date Mar 10, 2011

#include <vcl_fstream.h>
#include <boxm2/view/boxm2_trajectory.h>
#include <vpgl/vpgl_camera_double_sptr.h>
#include <vpgl/vpgl_perspective_camera.h>
#include <vpgl/vpgl_calibration_matrix.h>

//brdb stuff
#include <brdb/brdb_value.h>

namespace boxm2_view_trajectory_next_process_globals
{
  const unsigned n_inputs_ = 1;
  const unsigned n_outputs_ = 1;
}

bool boxm2_view_trajectory_next_process_cons(bprb_func_process& pro)
{
  using namespace boxm2_view_trajectory_next_process_globals;

  //process takes 1 input
  vcl_vector<vcl_string> input_types_(n_inputs_);
  input_types_[0] = "boxm2_trajectory_sptr"; //Scene 

  // process has 1 output:
  vcl_vector<vcl_string>  output_types_(n_outputs_);
  output_types_[0] = "vpgl_camera_double_sptr";     //an initialized trajectory object

  return pro.set_input_types(input_types_) && pro.set_output_types(output_types_);
}

bool boxm2_view_trajectory_next_process(bprb_func_process& pro)
{
  using namespace boxm2_view_trajectory_next_process_globals;
  if ( pro.n_inputs() < n_inputs_ ){
    vcl_cout << pro.name() << ": The input number should be " << n_inputs_<< vcl_endl;
    return false;
  }

  //get the inputs
  unsigned argIdx = 0;
  boxm2_trajectory_sptr trajectory = pro.get_input<boxm2_trajectory_sptr>(argIdx++);

  //new trjaectory
  vpgl_camera_double_sptr cam = trajectory->next(); 
  
  // store trajectory pointer
  argIdx = 0;
  pro.set_output_val<vpgl_camera_double_sptr>(argIdx, cam);
  return true;
}
