// This is brl/bpro/core/vpgl_pro/processes/vpgl_nitf_footprint_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file

#include <bprb/bprb_parameters.h>
#include <vcl_iostream.h>
#include <vil/vil_image_resource.h>
#include <vil/vil_load.h>
#include <vil/file_formats/vil_nitf2_image.h>
#include <vpgl/file_formats/vpgl_nitf_rational_camera.h>
#include <vul/vul_file.h>

//: initialization
bool vpgl_nitf_footprint_process_cons(bprb_func_process& pro)
{
  //this process takes 2 inputs:
  // 1: (vcl_string) the filename containing a list of images to evaluate
  // 2: (vcl_string) the filename of the kml file to write footprints to

  vcl_vector<vcl_string> input_types;
  input_types.push_back("vcl_string");
  input_types.push_back("vcl_string");

  return pro.set_input_types(input_types);
}


// write kml header
void kml_init(vcl_ofstream &ofs)
{
  ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>\n"
      << vcl_endl;
  return;
}


void kml_poly(vcl_ofstream &ofs, vcl_string name, vcl_string description, vnl_double_2 ul, vnl_double_2 ur, vnl_double_2 ll, vnl_double_2 lr)
{
  ofs << "<Placemark>\n"
      << "  <name>" << name << "</name>\n"
      << "  <description>" << description << "</description>\n"
      << "  <Style>\n"
      << "    <PolyStyle>\n"
      << "      <colorMode>random</colorMode>\n"
      << "      <color>ffffffff</color>\n"
      << "      <fill>0</fill>\n"
      << "    </PolyStyle>\n"
      << "    <LineStyle>\n"
      << "      <colorMode>random</colorMode>\n"
      << "      <color>ffffffff</color>\n"
      << "      <width>3</width>\n"
      << "    </LineStyle>\n"
      << "  </Style>\n"
      << "  <Polygon>\n"
      << "   <tessellate>1</tessellate>\n"
      << "   <outerBoundaryIs>\n"
      << "     <LinearRing>\n"
      << "       <coordinates>\n"
      << "          "
      << ul[1] << ',' << ul[0] << ",0  "
      << ur[1] << ',' << ur[0] << ",0  "
      << lr[1] << ',' << lr[0] << ",0  "
      << ll[1] << ',' << ll[0] << ",0  "
      << ul[1] << ',' << ul[0] << ",0\n"
      << "       </coordinates>\n"
      << "     </LinearRing>\n"
      << "   </outerBoundaryIs>\n"
      << "  </Polygon>\n"
      << "</Placemark>\n" << vcl_endl;

  return;
}

void kml_finish(vcl_ofstream &ofs)
{
  ofs << "</Document></kml>" << vcl_endl;
}

//: Execute the process
bool vpgl_nitf_footprint_process(bprb_func_process& pro)
{
  if (pro.n_inputs() != 2) {
    vcl_cout << "vpgl_nitf_footprint_process: Number of inputs is " << pro.n_inputs() << ", should be 2" << vcl_endl;
    return false;
  }

  // get the inputs
  vcl_string in_img_list = pro.get_input<vcl_string>(0);
  vcl_string footprint_filename = pro.get_input<vcl_string>(1);

  vcl_ofstream ofs(footprint_filename.c_str());
  if (!ofs.good()) {
    vcl_cerr << "ERROR: vpgl_nitf_footprint_process : Error opening " << footprint_filename << " for write.\n";
    return false;
  }

  kml_init(ofs);

  vcl_ifstream ifs(in_img_list.c_str());
  vcl_string nitf_path;

  while (ifs.good()) {
    ifs >> nitf_path;

    vil_image_resource_sptr image =
        vil_load_image_resource(nitf_path.c_str());
    if (!image)
    {
      vcl_cout << "NITF image load failed in vpgl_nitf_footprint_process\n";
      return false;
    }

    vcl_string format = image->file_format();
    vcl_string prefix = format.substr(0,4);

    if (prefix != "nitf")
    {
      vcl_cout << "source image is not NITF in vpgl_nitf_footprint_process\n";
      return false;
    }

    //cast to an nitf2_image
    vil_nitf2_image *nitf_image = static_cast<vil_nitf2_image*>(image.ptr());
    vpgl_nitf_rational_camera *nitf_cam=new vpgl_nitf_rational_camera(nitf_image, true);

    vnl_double_2 ul = nitf_cam->upper_left();
    vnl_double_2 ur = nitf_cam->upper_right();
    vnl_double_2 ll = nitf_cam->lower_left();
    vnl_double_2 lr = nitf_cam->lower_right();

    vcl_string nitf_id = vul_file::strip_directory(nitf_path);
    vcl_string desc = nitf_path + " footprint";

    kml_poly(ofs, nitf_id, desc, ul, ur, ll, lr);
  }
  kml_finish(ofs);

  return true;
}

