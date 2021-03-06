//This is brl/bseg/bvpl/bvpl_octree/pro/processes/bvpl_plane_propagate_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file
// \brief  A process for spreading the plane information of the cell to the neighbors
// \author Gamze Tunali
// \date   Mar 19, 2010
//
// \verbatim
//  Modifications
//
// \endverbatim

#include <vcl_string.h>
#include <vcl_fstream.h>

#include <brdb/brdb_value.h>
#include <bprb/bprb_parameters.h>

#include <boct/boct_tree.h>

#include <boxm/boxm_scene_base.h>
#include <boxm/boxm_scene.h>

#include <boxm/sample/boxm_edge_tangent_sample.h>

#include <bvpl/bvpl_neighb_operator.h>
#include <bvpl/kernels/bvpl_kernel_iterator.h>
#include <bvpl/kernels/bvpl_kernel.h>
#include <bvpl/bvpl_octree/bvpl_octree_neighbors.h>
#include <vcl_set.h>

struct ltstr
{
  bool operator()(const boxm_plane_obs<float> & p1, const boxm_plane_obs<float> & p2) const
  {
      if(p1.plane_.a()<p2.plane_.a())
          return true;
      else if(p1.plane_.a()>p2.plane_.a())
          return false;
      else 
      {
          if(p1.plane_.b()<p2.plane_.b())
              return true;
          else if(p1.plane_.b()>p2.plane_.b())
              return false;
          else 
          {
              if(p1.plane_.c()<p2.plane_.c())
                  return true;
              else if(p1.plane_.c()>p2.plane_.c())
                  return false;
              else 
              {
                  if(p1.plane_.d()<p2.plane_.d())
                      return true;
                  else 
                      return false;
              }
          }
      }
  }
};

namespace bvpl_plane_propagate_process_globals
{
  const unsigned int n_inputs_ = 4;
  const unsigned int n_outputs_ = 1;
  //Define parameters here
}


//: set input and output types
bool bvpl_plane_propagate_process_cons(bprb_func_process& pro)
{
  using namespace bvpl_plane_propagate_process_globals;

  // process takes 4 inputs:
  //input[0]: The scene
  //input[1]: the scene path for the output scene
  //input[2]: block prefix for the output scene
  //input[3]: the filename for the new scene xml file
  vcl_vector<vcl_string> input_types_(n_inputs_);
  input_types_[0] = "boxm_scene_base_sptr";
  input_types_[1] = "vcl_string";
  input_types_[2] = "vcl_string";
  input_types_[3] = "vcl_string";

  // process has 1 output:
  // output[0]: the new scene with updated cell information
  vcl_vector<vcl_string> output_types_(n_outputs_);
  output_types_[0] = "boxm_scene_base_sptr";

  if (!pro.set_input_types(input_types_))
    return false;

  if (!pro.set_output_types(output_types_))
    return false;

  return true;
}

//: Execute the process
bool bvpl_plane_propagate_process(bprb_func_process& pro)
{
  using namespace bvpl_plane_propagate_process_globals;
  

  // check number of inputs
  if (pro.n_inputs() != n_inputs_)
  {
    vcl_cout << pro.name() << "The number of inputs should be " << n_inputs_ << vcl_endl;
    return false;
  }

  // get the inputs
  boxm_scene_base_sptr scene_base = pro.get_input<boxm_scene_base_sptr>(0);
  vcl_string scene_path = pro.get_input<vcl_string>(1);
  vcl_string block_prefix = pro.get_input<vcl_string>(2);
  vcl_string scene_filename = pro.get_input<vcl_string>(3);
  boxm_scene_base_sptr output_scene_sptr;
  // only applies to the edge_line type of scenes
  if (scene_base->appearence_model() == BOXM_EDGE_TANGENT_LINE) {
    typedef boct_tree<short,boxm_edge_tangent_sample<float> > tree_type;
    typedef boct_tree_cell<short,boxm_edge_tangent_sample<float> > cell_type;
    boxm_scene<tree_type> *scene=dynamic_cast<boxm_scene<tree_type>*>(scene_base.ptr());
    if (!scene) {
       vcl_cerr << "error casting scene_base to scene\n";
       return false;
    }
    
    boxm_scene<tree_type> *output_scene=new boxm_scene<tree_type>(*scene);
    output_scene->set_paths(scene_path, block_prefix);
    output_scene_sptr = output_scene;
    // create a kernel for 3x3 neighborhood
    bvpl_kernel_iterator k_iter;
    vgl_point_3d<int> min_pt(-1,-1,-1);
    vgl_point_3d<int> max_pt(1,1,1);
    for (int i=min_pt.x(); i<=max_pt.x(); i++) {
      for (int j=min_pt.y(); j<=max_pt.y(); j++) {
        for (int k=min_pt.z(); k<=max_pt.z(); k++) {
          k_iter.insert(vgl_point_3d<int>(i,j,k),bvpl_kernel_dispatch(1.0f));
        }
      }
    }
    bvpl_kernel_sptr kernel= new bvpl_kernel(k_iter, vnl_float_3(0,0,1), vnl_float_3(0,1,0), 0.0f, vgl_vector_3d<int>(3,3,3),min_pt,max_pt);
    
    boxm_block_iterator<tree_type> iter(scene);
    boxm_block_iterator<tree_type> output_iter(output_scene);
    iter.begin();
    output_iter.begin();

    while (!iter.end())
    {
      scene->load_block(iter.index());
      output_scene->load_block(iter.index());
      boxm_block<tree_type>* block = *iter;
      boxm_block<tree_type>* output_block = *output_iter;

      tree_type* tree = block->get_tree();
      vcl_vector<cell_type *> cells = tree->leaf_cells();

      tree_type* output_tree=tree->clone();
      vcl_vector<cell_type *> output_cells = output_tree->leaf_cells();

      bvpl_octree_neighbors<boxm_edge_tangent_sample<float> > oper(tree);
      for (unsigned i=0; i<cells.size(); i++) {
        cell_type *cell=cells[i];
        cell_type *output_cell=output_cells[i];
        vcl_vector<cell_type *> neighb_cells;
        oper.neighbors(kernel, cells[i], neighb_cells);
        vcl_set<boxm_plane_obs<float>,ltstr> planes;

        for (unsigned n=0; n<neighb_cells.size(); n++) {
          cell_type *neighbor = neighb_cells[n];
          boct_loc_code<short> cell_code=cell->code_;
          boct_loc_code<short> n_code = neighbor->code_;
          bool itself = n_code.isequal(&cell_code);
         if (!itself) {
             vcl_vector<boxm_plane_obs<float> > observations = neighbor->data().obs_list();
             boxm_edge_tangent_sample<float> data=output_cell->data();
             data.insert(observations);
             output_cell->set_data(data);
            }
        }
      }
      //  vcl_set<boxm_plane_obs<float>,ltstr >::iterator iter1=planes.begin();
      //  boxm_edge_tangent_sample<float> data;
      //  for(;iter1!=planes.end();iter1++)
      //  {
      //      data.insert(*iter1);
      //  }
      //  output_cell->set_data(data);
      //}
      output_block->delete_tree();
      output_block->set_tree(output_tree);
      output_scene->write_active_block();
      iter++;
    }
    output_scene->write_scene(scene_filename);
  }

  //store output
  pro.set_output_val<boxm_scene_base_sptr>(0, output_scene_sptr);
  return true;
}
