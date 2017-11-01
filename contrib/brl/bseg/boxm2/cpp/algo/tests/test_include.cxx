#include <boxm2/cpp/algo/boxm2_apply_filter_function.h>
#include <boxm2/cpp/algo/boxm2_batch_functors.h>
#include <boxm2/cpp/algo/boxm2_batch_opt2_functors.h>
#include <boxm2/cpp/algo/boxm2_batch_opt2_phongs_functors.h>
#include <boxm2/cpp/algo/boxm2_cast_cone_ray_function.h>
#include <boxm2/cpp/algo/boxm2_cast_intensities_functor.h>
#include <boxm2/cpp/algo/boxm2_cast_ray_function.h>
#include <boxm2/cpp/algo/boxm2_change_detection_functor.h>
#include <boxm2/cpp/algo/boxm2_compute_derivative_function.h>
#include <boxm2/cpp/algo/boxm2_compute_nonsurface_histogram_functor.h>
#include <boxm2/cpp/algo/boxm2_compute_normal_albedo_functor.h>
#include <boxm2/cpp/algo/boxm2_compute_normal_albedo_functor_opt.h>
#include <boxm2/cpp/algo/boxm2_compute_phongs_model_functor.h>
#include <boxm2/cpp/algo/boxm2_cone_update_image_functor.h>
#include <boxm2/cpp/algo/boxm2_create_mog_image_functor.h>
#include <boxm2/cpp/algo/boxm2_data_serial_iterator.h>
#include <boxm2/cpp/algo/boxm2_export_oriented_point_cloud_function.h>
#include <boxm2/cpp/algo/boxm2_export_stack_images_function.h>
#include <boxm2/cpp/algo/boxm2_filter_block_function.h>
#include <boxm2/cpp/algo/boxm2_gauss_grey_processor.h>
#include <boxm2/cpp/algo/boxm2_gauss_rgb_processor.h>
#include <boxm2/cpp/algo/boxm2_image_density_functor.h>
#include <boxm2/cpp/algo/boxm2_mean_intensities_batch_functor.h>
#include <boxm2/cpp/algo/boxm2_merge_block_function.h>
#include <boxm2/cpp/algo/boxm2_mog3_grey_processor.h>
#include <boxm2/cpp/algo/boxm2_phongs_model_processor.h>
#include <boxm2/cpp/algo/boxm2_points_to_volume_function.h>
#include <boxm2/cpp/algo/boxm2_ray_probe_functor.h>
#include <boxm2/cpp/algo/boxm2_refine_block_function.h>
#include <boxm2/cpp/algo/boxm2_render_cone_functor.h>
#include <boxm2/cpp/algo/boxm2_render_exp_depth_functor.h>
#include <boxm2/cpp/algo/boxm2_render_exp_image_functor.h>
#include <boxm2/cpp/algo/boxm2_render_functions.h>
#include <boxm2/cpp/algo/boxm2_shadow_model_functor.h>
#include <boxm2/cpp/algo/boxm2_synoptic_function_functors.h>
#include <boxm2/cpp/algo/boxm2_update_functions.h>
#include <boxm2/cpp/algo/boxm2_update_image_functor.h>
#include <boxm2/cpp/algo/boxm2_update_using_quality_functor.h>
#include <boxm2/cpp/algo/boxm2_update_with_shadow_functor.h>

int main() { return 0; }