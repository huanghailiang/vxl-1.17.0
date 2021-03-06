#include "boxm2_util.h"
#include <boxm2/boxm2_data_traits.h>
//:
// \file

#include <bpgl/algo/bpgl_project.h>
#include <boct/boct_bit_tree.h>

//vgl includes
#include <vgl/vgl_point_3d.h>
#include <vgl/vgl_homg_point_3d.h>
#include <vgl/vgl_box_3d.h>
#include <vgl/vgl_box_2d.h>
#include <vil/vil_convert.h>

//vnl includes
#include <vnl/vnl_random.h>
#include <vnl/algo/vnl_svd.h>

//vil includes
#include <vil/vil_image_view.h>

//vcl io stuff
#include <vcl_iostream.h>
#include <vcl_fstream.h>
#include <vcl_cstdio.h>
#include <vcl_algorithm.h>

//vul file includes
#include <vul/vul_file.h>
#include <vul/vul_file_iterator.h>


void boxm2_util::random_permutation(int* buffer, int size)
{
  vnl_random random(9667566);

  //shuffle the buffer
  for (int i=0; i<size; i++)
  {
    //swap current value with random one after it
    int curr = buffer[i];
    int rInd = random.lrand32(i, size-1);
    buffer[i] = buffer[rInd];
    buffer[rInd] = curr;
  }
}

float boxm2_util::clamp(float x, float a, float b)
{
    return x < a ? a : (x > b ? b : x);
}

//: returns a list of cameras from specified directory
vcl_vector<vpgl_perspective_camera<double>* > boxm2_util::cameras_from_directory(vcl_string dir)
{
  vcl_vector<vpgl_perspective_camera<double>* > toReturn;
  if (!vul_file::is_directory(dir.c_str()) ) {
    vcl_cerr<<"Cam dir is not a directory\n";
    return toReturn;
  }

  //get all of the cam and image files, sort them
  vcl_string camglob=dir+"/*.txt";
  vul_file_iterator file_it(camglob.c_str());
  vcl_vector<vcl_string> cam_files;
  while (file_it) {
    vcl_string camName(file_it());
    cam_files.push_back(camName);
    ++file_it;
  }
  vcl_sort(cam_files.begin(), cam_files.end());

  //take sorted lists and load from file
  vcl_vector<vcl_string>::iterator iter;
  for (iter = cam_files.begin(); iter != cam_files.end(); ++iter)
  {
    //load camera from file
    vcl_ifstream ifs(iter->c_str());
    vpgl_perspective_camera<double>* pcam =new vpgl_perspective_camera<double>;
    if (!ifs.is_open()) {
      vcl_cerr << "Failed to open file " << *iter << '\n';
      return toReturn;
    }
    else  {
      ifs >> *pcam;
    }

    toReturn.push_back(pcam);
  }
  return toReturn;
}

//: returns a single camera from file
vpgl_camera_double_sptr boxm2_util::camera_from_file(vcl_string camfile)
{
  //load camera from file
  vcl_ifstream ifs(camfile.c_str());
  vpgl_perspective_camera<double>* pcam =new vpgl_perspective_camera<double>;
  if (!ifs.is_open()) {
    vcl_cerr << "Failed to open file " << camfile << '\n';
    return vpgl_camera_double_sptr(pcam);
  }
  else {
    ifs >> *pcam;
  }
  return vpgl_camera_double_sptr(pcam);
}

//: returns a list of image strings from directory
vcl_vector<vcl_string> boxm2_util::images_from_directory(vcl_string dir)
{
#if 0
  vcl_vector<vcl_string> img_files;
  if (!vul_file::is_directory(dir.c_str())) {
    vcl_cerr<<"img dir is not a directory\n";
    return img_files;
  }
  vcl_string imgglob=dir+"/*.???";
  vul_file_iterator img_file_it(imgglob.c_str());
  while (img_file_it) {
    vcl_string imgName(img_file_it());
    img_files.push_back(imgName);
    ++img_file_it;
  }
  vcl_sort(img_files.begin(), img_files.end());
  return img_files;
#endif
  return boxm2_util::files_from_dir(dir, "???");
}

//: returns a list of image strings from directory
vcl_vector<vcl_string> boxm2_util::camfiles_from_directory(vcl_string dir)
{
#if 0
  vcl_vector<vcl_string> cam_files;
  if (!vul_file::is_directory(dir.c_str())) {
    vcl_cerr<<"cam dir is not a directory\n";
    return cam_files;
  }
  vcl_string camglob=dir+"/*.txt";
  vul_file_iterator file_it(camglob.c_str());
  while (file_it) {
    vcl_string camName(file_it());
    cam_files.push_back(camName);
    ++file_it;
  }
  vcl_sort(cam_files.begin(), cam_files.end());
  return cam_files;
#endif
  return boxm2_util::files_from_dir(dir, "txt");
}

vcl_vector<vcl_string> boxm2_util::files_from_dir(vcl_string dir, vcl_string ext)
{
  vcl_vector<vcl_string> files;
  if (!vul_file::is_directory(dir.c_str())) {
    vcl_cerr<<"dir does not exist: "<<dir<<vcl_endl;
    return files;
  }
  vcl_string glob = dir + "/*." + ext;
  vul_file_iterator file_it(glob.c_str());
  while (file_it) {
    vcl_string name(file_it());
    files.push_back(name);
    ++file_it;
  }
  vcl_sort(files.begin(), files.end());
  return files;
}

//Constructs a camera given elevation, azimuth (degrees), radius, and bounding box.
vpgl_perspective_camera<double>*
boxm2_util::construct_camera( double elevation,
                              double azimuth,
                              double radius,
                              unsigned ni,
                              unsigned nj,
                              vgl_box_3d<double> bb,
                              bool fit_bb)
{
  double dni = static_cast<double>(ni);
  double dnj = static_cast<double>(nj);

  //
  //find a camera that will project the scene bounding box
  //entirely inside the image
  //
  // 1) determine the stare point (center of bounding box)
  vgl_point_3d<double> cn = bb.centroid();
  vgl_homg_point_3d<double> stpt(cn.x(), cn.y(), cn.z());

  // 2) determine camera center
  // the viewsphere radius is set to 10x the bounding box diameter
  double r = radius; // = vcl_sqrt(w*w + h*h + d*d); // where w=bb.width() etc.
  //r *= 10;
  double deg_to_rad = vnl_math::pi/180;
  double el = elevation*deg_to_rad, az = azimuth*deg_to_rad;
  double cx = r*vcl_sin(el)*vcl_cos(az);
  double cy = r*vcl_sin(el)*vcl_sin(az);
  double cz = r*vcl_cos(el);
  vgl_point_3d<double> cent(cx+cn.x(), cy+cn.y(), cz+cn.z());

  // 3) start with a unit focal length and position the camera
  vpgl_calibration_matrix<double> K(1.0, vgl_point_2d<double>(ni/2, nj/2));
  vgl_rotation_3d<double> R;
  vpgl_perspective_camera<double>* cam =
    new vpgl_perspective_camera<double>(K, cent, R);

  //stare at the center of the scene
  vgl_vector_3d<double> up(0.0, 1.0, 0.0);
  if (vcl_fabs(el)<1.0e-3)
    cam->look_at(stpt, up);
  else
    cam->look_at(stpt);

  //4) Adjust the focal length so that the box projects into the image
  // project the bounding box
  double id = vcl_sqrt(dni*dni + dnj*dnj);
  double f;
  if (fit_bb) {
    vgl_box_2d<double> image_bb = bpgl_project::project_bounding_box(*cam, bb);
    // get 2-d box diameter and image diameter
    double bw = image_bb.width(), bh = image_bb.height();
    double bd = vcl_sqrt(bw*bw + bh*bh);
    //find the adjusted focal length
    f = id/bd;
  }
  else {
    f = id;
  }
  K.set_focal_length(f);
  cam->set_calibration(K);

#ifdef DEBUG
  vcl_cout<<"Camera : " <<*cam<<vcl_endl;
#endif
  return cam;
}


//: searches through the list of perspective cameras and returns a pointer to the one that most closely aligns with the normal
//  \returns negative one if the list is empty
int boxm2_util::find_nearest_cam(vgl_vector_3d<double>& normal,
                                 vcl_vector<vpgl_perspective_camera<double>* >& cams)
{
  if (cams.empty()) {
    return -1;
  }

  //find minimal dot product amongst cams/images
  double minAngle = 10e20;
  unsigned minCam = -1;
  for (unsigned int i=0; i<cams.size(); ++i) {
    double dotProd = dot_product( normal, -1*cams[i]->principal_axis());
    double ang = vcl_acos(dotProd); // vcl_fabs(angle(normal, -1*cams[i]->principal_axis()));
#ifdef DEBUG
    if ( vcl_fabs(normal.z()) > .8 ) {
      vcl_cout<<"Face normal: "<<normal<<"  principal axis: "<<cams[i]->principal_axis()<<'\n'
              <<" and angle: " <<ang * vnl_math::deg_per_rad<<vcl_endl;
    }
#endif // DEBUG
    if (ang < minAngle && ang < vnl_math::pi/3.0) {
      minAngle = ang;
      minCam = i;
    }
  }

  //return the min cam
  return minCam;
}


bool boxm2_util::copy_file(vcl_string file, vcl_string dest)
{
  vcl_string line;
  vcl_ifstream myfile (file.c_str());
  vcl_ofstream outfile(dest.c_str());
  if (myfile.is_open() && outfile.is_open())
  {
    while ( myfile.good() )
    {
      getline (myfile,line);
      outfile << line << '\n';
    }
    myfile.close();
    outfile.close();
  }
  else {
    vcl_cerr<<"Couldn't open " << file << " or " << dest << vcl_endl;
    return false;
  }
  return true;
}

bool boxm2_util::generate_html(int height, int width, int nrows, int ncols, vcl_string dest)
{
  char html[4*1024];
  vcl_sprintf(html,
              "<!DOCTYPE html>\n\
<html lang='en'>\n\
<head>\n\
  <meta charset='utf-8' content='text/html' http-equiv='Content-type' />\n\
  <title>Volume Visualizer</title>\n\
  <!-- Reel/Jquery Script Includes -->\n\
  <script src='js/jquery.min.js' type='text/javascript'></script>\n\
  <script src='js/jquery.reel-min.js' type='text/javascript'></script>\n\
  <script src='js/jquery.disabletextselect-min.js' type='text/javascript'></script>\n\
  <script src='js/jquery.mousewheel-min.js' type='text/javascript'></script>\n\
  <script src='js/js.js' type='text/javascript'></script>\n\
  <script src='js/miniZoomPan.js' type='text/javascript'></script>\n\
  <style  src='css/miniZoomPan.css' type='text/css' />\n\
  <meta name='viewport' content='width = %d' />\n\
  <style>\n\
    html, body{ margin: 0; background: #000 url(iphone.instructions.gif) no-repeat 0 %dpx; }\n\
  </style>\n\
  <!-- ZOOM Script -->\n\
  <script type= 'text/javascript'>/*<![CDATA[*/\n\
      $(function() {\n\
        $('#zoom01').miniZoomPan({\n\
            sW: %d,\n\
            sH: %d,\n\
            lW: %d,\n\
            lH: %d\n\
          })\n\
      });\n\
      /*]]>*/\n\
  </script>\n\
</head>\n\
<body>\n\
  <div id='zoom01'>\n\
    <img id='image' src='img/scene_0_0.jpg' width='%dpx' height=%dpx' />\n\
  </div>\n\
  <script type='text/javascript'>\n\
    $(document).ready(function() {\n\
      $('#image').reel({\n\
        frame: 1,\n\
        footage: %d,\n\
        frames: %d,\n\
        rows: %d,\n\
        row: 1,\n\
        path: 'img/',\n\
        image: 'img/scene_0_0.jpg',\n\
        images: scene_frames(),\n\
        horizontal: true,\n\
      });\n\
    });\n\
  </script>\n\
</body>\n\
</html>",
              width,
              width,
              width,
              height,
              2*width,
              2*height,
              width,
              height,
              ncols,
              ncols,
              nrows);

  //write to destination file
  vcl_ofstream outfile(dest.c_str());
  if (outfile.is_open())
  {
    outfile << html;
    outfile.close();
    return true;
  }
  else {
    vcl_cerr<<"Couldn't open " << dest << vcl_endl;
    return false;
  }
}


bool boxm2_util::generate_jsfunc(vbl_array_2d<vcl_string> img_files, vcl_string dest)
{
  vcl_string js = "function scene_frames(frames) {\n var stack = [ ";

  //go through the array in img_files
  for (unsigned int row=0; row<img_files.rows(); ++row) {
    for (unsigned int col=0; col<img_files.cols(); ++col) {
      if (row != 0 || col != 0)
        js += ", "; //don't put a comma at the beginning...
      js += "'" + img_files(row, col) + "'";
    }
  }
  js += "]\nreturn stack\n}";

  //write to destination file
  vcl_ofstream outfile(dest.c_str());
  if (outfile.is_open())
  {
    outfile << js;
    outfile.close();
    return true;
  }
  else {
    vcl_cerr<<"Couldn't open " << dest << vcl_endl;
    return false;
  }
}


// private helper method prepares an input image to be processed by update
vil_image_view_base_sptr boxm2_util::prepare_input_image(vil_image_view_base_sptr loaded_image, bool force_grey)
{
  //then it's an RGB image (assumes byte image...)
  if (loaded_image->nplanes() == 3 || loaded_image->nplanes() == 4)
  {
    //if not forcing RGB image to be grey
    if (!force_grey)
    {
#ifdef DEBUG
      vcl_cout<<"preparing rgb image"<<vcl_endl;
#endif
      //load image from file and format it into RGBA
      vil_image_view_base_sptr n_planes = vil_convert_to_n_planes(4, loaded_image);
      vil_image_view_base_sptr comp_image = vil_convert_to_component_order(n_planes);
      vil_image_view<vil_rgba<vxl_byte> >* rgba_view = new vil_image_view<vil_rgba<vxl_byte> >(comp_image);

      //make sure all alpha values are set to 255 (1)
      vil_image_view<vil_rgba<vxl_byte> >::iterator iter;
      for (iter = rgba_view->begin(); iter != rgba_view->end(); ++iter) {
        (*iter) = vil_rgba<vxl_byte>(iter->R(), iter->G(), iter->B(), 255);
      }
      vil_image_view_base_sptr toReturn(rgba_view);
      return toReturn;
    }
    else
    {
#ifdef DEBUG
      vcl_cout<<"preparing rgb as input to grey scale float image"<<vcl_endl;
#endif
      //load image from file and format it into grey
      vil_image_view<vxl_byte>* inimg    = dynamic_cast<vil_image_view<vxl_byte>* >(loaded_image.ptr());
      vil_image_view<float>     gimg(loaded_image->ni(), loaded_image->nj());
      vil_convert_planes_to_grey<vxl_byte, float>(*inimg, gimg);

      //stretch it into 0-1 range
      vil_image_view<float>*    floatimg = new vil_image_view<float>(loaded_image->ni(), loaded_image->nj());
      vil_convert_stretch_range_limited(gimg, *floatimg, 0.0f, 255.0f, 0.0f, 1.0f);
      vil_image_view_base_sptr toReturn(floatimg);
      return toReturn;
    }
  }

  //else if loaded planes is just one...
  if (loaded_image->nplanes() == 1)
  {
#ifdef DEBUG
    vcl_cout<<"Preparing grey scale image"<<vcl_endl;
#endif
    //prepare floatimg for stretched img
    vil_image_view<float>* floatimg;
    if (vil_image_view<vxl_byte> *img_byte = dynamic_cast<vil_image_view<vxl_byte>*>(loaded_image.ptr()))
    {
      floatimg = new vil_image_view<float>(loaded_image->ni(), loaded_image->nj(), 1);
      vil_convert_stretch_range_limited(*img_byte, *floatimg, vxl_byte(0), vxl_byte(255), 0.0f, 1.0f);
    }
    else if (vil_image_view<unsigned short> *img_byte = dynamic_cast<vil_image_view<unsigned short>*>(loaded_image.ptr()))
    {
      floatimg = new vil_image_view<float>(loaded_image->ni(), loaded_image->nj(), 1);
      vil_convert_stretch_range_limited(*img_byte, *floatimg,(unsigned short)0,(unsigned short)65535,  0.0f, 1.0f); // hardcoded to be fixed.
    }
    else if (vil_image_view<float> *img_float = dynamic_cast<vil_image_view<float>*>(loaded_image.ptr()))
    {
      return vil_image_view_base_sptr(img_float);
    }
    else {
      vcl_cerr << "Failed to load image\n";
      return 0;
    }
    vil_image_view_base_sptr toReturn(floatimg);
    return toReturn;
  }

  //otherwise it's messed up, return a null pointer
  vcl_cerr<<"Failed to recognize input image type\n";
  return 0;
}

vil_rgba<vxl_byte> boxm2_util::mean_pixel(vil_image_view<vil_rgba<vxl_byte> >& img)
{
  double mean[] = {0.0, 0.0, 0.0, 0.0};
  int count = 0;
  for (unsigned int i=0; i<img.ni(); ++i) {
    for (unsigned int j=0; j<img.nj(); ++j) {
      mean[0] += (double) (img(i,j).R());
      mean[1] += (double) (img(i,j).G());
      mean[2] += (double) (img(i,j).B());
      mean[3] += (double) (img(i,j).A());
      ++count;
    }
  }
  return vil_rgba<vxl_byte>( (vxl_byte) (mean[0]/count),
                             (vxl_byte) (mean[1]/count),
                             (vxl_byte) (mean[2]/count),
                             255 );
}

bsta_histogram_sptr
boxm2_util::generate_image_histogram(vil_image_view_base_sptr  img, unsigned int numbins)
{
  bsta_histogram<float> * hist= new bsta_histogram<float>(0.0,1.0,numbins);
  if (vil_image_view<float> * float_img = dynamic_cast<vil_image_view<float> *> (img.ptr()))
  {
    for (unsigned i =0;i<float_img->ni();i++)
      for (unsigned j =0;j<float_img->nj();j++)
        hist->upcount((*float_img)(i,j) ,1 );
  }

  return hist;
}


bool boxm2_util::verify_appearance(boxm2_scene& scene, const vcl_vector<vcl_string>&valid_types, vcl_string& data_type, int& appTypeSize )
{
  bool foundDataType = false;
  vcl_vector<vcl_string> apps = scene.appearances();
  for (unsigned int i=0; i<apps.size(); ++i) {
    //look for valid types
    for (unsigned int c=0; c<valid_types.size(); ++c) {
      if (apps[i] == valid_types[c]) {
        foundDataType = true;
        data_type = apps[i];
        appTypeSize = (int) boxm2_data_info::datasize( apps[i] );
      }
    }
  }
  return foundDataType;
}


bool boxm2_util::query_point(boxm2_scene_sptr& scene,
                             boxm2_cache_sptr& cache,
                             const vgl_point_3d<double>& point,
                             float& prob, float& intensity)
{
  vgl_point_3d<double> local;
  boxm2_block_id id;
  if (!scene->contains(point, id, local))
    return false;

  int index_x=(int)vcl_floor(local.x());
  int index_y=(int)vcl_floor(local.y());
  int index_z=(int)vcl_floor(local.z());
  boxm2_block * blk=cache->get_block(id);
  boxm2_block_metadata mdata = scene->get_block_metadata_const(id);
  vnl_vector_fixed<unsigned char,16> treebits=blk->trees()(index_x,index_y,index_z);
  boct_bit_tree tree(treebits.data_block(),mdata.max_level_);
  int bit_index=tree.traverse(local);
  int depth=tree.depth_at(bit_index);
  int data_offset=tree.get_data_index(bit_index,false);
#if 0
  data_offset += 0x10000*((int)treebits[12]*0x100+(int)treebits[13]);
#endif
  boxm2_data_base *  alpha_base  = cache->get_data_base(id,boxm2_data_traits<BOXM2_ALPHA>::prefix());
  boxm2_data<BOXM2_ALPHA> *alpha_data=new boxm2_data<BOXM2_ALPHA>(alpha_base->data_buffer(),alpha_base->buffer_length(),alpha_base->block_id());

  float alpha=alpha_data->data()[data_offset];

  float side_len=static_cast<float>(mdata.sub_block_dim_.x()/((float)(1<<depth)));
#ifdef DEBUG
  vcl_cout<<" DATA OFFSET "<<side_len<<vcl_endl;
#endif

  //store cell probability
  prob=1.0f-vcl_exp(-alpha*side_len);
  boxm2_data_base *  int_base  = cache->get_data_base(id,boxm2_data_traits<BOXM2_MOG3_GREY>::prefix());
  boxm2_data<BOXM2_MOG3_GREY> *int_data=new boxm2_data<BOXM2_MOG3_GREY>(int_base->data_buffer(),int_base->buffer_length(),int_base->block_id());
  // intensity
  intensity=(float)int_data->data()[data_offset][0]/255.0f;

  return true;
}

