// EXPECT: error the texturecube_array method "read" is not lowered yet
// Apple accepts read on a sampled cube array; Vulkan's OpImageFetch has no cube dimension.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texturecube_array<float> a [[texture(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.read(uint2(1), 1u, 1u);
}
