// This is brl/bpro/core/icam_pro/processes/icam_create_view_sphere_process.cxx

//:
// \file
// \brief A process for generating cameras that view a scene
//
// \author Gamze D. Tunali
// \date January 23, 2010
//
// \verbatim
//  Modifications
//   <none yet>
// \endverbatim

#include <bprb/bprb_func_process.h>
#include <bprb/bprb_parameters.h>

#include <brdb/brdb_value.h>

#include <vpgl/vpgl_perspective_camera.h>
#include <vcl_string.h>
#include <vcl_iostream.h>
#include <vcl_sstream.h>

#include <icam/icam_view_sphere.h>
#include <icam/icam_view_metadata.h>

// global variables
namespace icam_create_view_sphere_process_globals
{
  // this process takes 0 inputs and 2 outputs
  const unsigned n_inputs_ = 0;
  const unsigned n_outputs_ = 2;
}

//: sets input and output types
bool icam_create_view_sphere_process_cons(bprb_func_process& pro)
{
  using namespace icam_create_view_sphere_process_globals;
  vcl_vector<vcl_string> input_types_(n_inputs_);

  vcl_vector<vcl_string> output_types_(n_outputs_);
  output_types_[0] = "vcl_string"; // the text file that contains the paths for the view point cameras
  output_types_[1] = "icam_view_sphere_sptr"; // view sphere smart pointer

  return pro.set_input_types(input_types_) && pro.set_output_types(output_types_);
}

//: creates a scene from parameters
bool icam_create_view_sphere_process(bprb_func_process& pro)
{
  if (!pro.verify_inputs()) {
    vcl_cout << pro.name() << "icam_create_view_sphere_process: invalid inputs" << vcl_endl;
    return false;
  }
  using namespace icam_create_view_sphere_process_globals;

  double elevation,view_angle,orig_x, orig_y, orig_z, radius;
  double dim_x, dim_y, dim_z;
  unsigned ni,nj;
  vcl_string path="";
  pro.parameters()->get_value("elevation", elevation);
  pro.parameters()->get_value("view_angle", view_angle);
  pro.parameters()->get_value("dim_x", dim_x);
  pro.parameters()->get_value("dim_y", dim_y);
  pro.parameters()->get_value("dim_z", dim_z);
  pro.parameters()->get_value("orig_x", orig_x);
  pro.parameters()->get_value("orig_y", orig_y);
  pro.parameters()->get_value("orig_z", orig_z);
  pro.parameters()->get_value("radius", radius);
  pro.parameters()->get_value("image_ni", ni);
  pro.parameters()->get_value("image_nj", nj);
  pro.parameters()->get_value("camera_path", path);

  vgl_box_3d<double> world_bb(orig_x, orig_y, orig_z, orig_x+dim_x, orig_y+dim_y, orig_z+dim_z);
  icam_view_sphere_sptr view_sphere =new icam_view_sphere(world_bb, radius);

  // generate the view points-cameras
  view_sphere->create_view_points(elevation, view_angle, ni, nj);

  vcl_map<unsigned, vpgl_camera_double_sptr> cameras;
  view_sphere->cameras(cameras);

  vcl_ofstream file(path.c_str());
  if (!file.is_open()) {
    vcl_cerr << "Failed to open file " << path << '\n';
    return false;
  }

  vcl_map<unsigned, vpgl_camera_double_sptr>::iterator it=cameras.begin();
  while (it != cameras.end()) {
    unsigned uid=it->first;
    vpgl_camera_double_sptr cam = it->second;
    vpgl_perspective_camera<double>* pers_cam = dynamic_cast<vpgl_perspective_camera<double>*>(cam.as_pointer());

    vcl_stringstream cam_path;
    cam_path << "camera" << uid << ".txt";
    vcl_ofstream ofs(cam_path.str().c_str());
    if (!file.is_open()) {
      vcl_cerr << "Failed to open file " << path << '\n';
      return false;
    }
    ofs << *pers_cam;
    ofs.close();

    file << uid << ' ' << cam_path.str() << vcl_endl;
    it++;
  }
  file.close();

  // return the txt file (the lidt of camera paths)
  pro.set_output_val<vcl_string>(0,path);
  pro.set_output_val<icam_view_sphere_sptr>(1, view_sphere);
  return true;
}
