// EXPECT: error a Vulkan storage image view is a single level
// Apple accepts get_num_mip_levels on a storage texture; a Vulkan storage image has no level query.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read> a [[texture(0)]], device uint* o [[buffer(0)]]) {
  o[0] = a.get_num_mip_levels();
}
