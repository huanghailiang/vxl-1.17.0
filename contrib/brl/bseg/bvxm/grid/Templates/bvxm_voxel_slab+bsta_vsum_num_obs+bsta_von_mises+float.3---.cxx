#include <bvxm/grid/bvxm_voxel_slab.txx>
#include <bsta/bsta_attributes.h>
#include <bsta/bsta_von_mises.h>
#include <bsta/io/bsta_io_von_mises.h>
#include <bsta/io/bsta_io_attributes.h>

typedef bsta_vsum_num_obs<bsta_von_mises<float, 3> > dir_dist_type;
BVXM_VOXEL_SLAB_INSTANTIATE(dir_dist_type);
