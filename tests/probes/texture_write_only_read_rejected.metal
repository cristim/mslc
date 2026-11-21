// EXPECT: error write-only textures lower only
// Apple: no member function read in access::write texture2d.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::write> a [[texture(0)]], device float4* o [[buffer(0)]], uint2 t [[thread_position_in_grid]]) {
  o[0] = a.read(t);
}
