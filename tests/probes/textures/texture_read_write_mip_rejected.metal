// EXPECT: error mip arguments are unsupported on access::read_write textures
// Apple accepts a mip argument and keeps it in the AIR; Vulkan storage images have no level operand here.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read_write> a [[texture(0)]], device float4* o [[buffer(0)]], uint2 t [[thread_position_in_grid]]) {
  o[0] = a.read(t, 1u);
}
