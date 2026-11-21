// EXPECT: error read on a multisampled texture takes a coordinate and a sample index
// Apple: no matching member function for read (a multisampled texture has no lod).
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d_ms<float> a [[texture(0)]], device float4* o [[buffer(0)]], uint2 t [[thread_position_in_grid]]) {
  o[0] = a.read(t, 1, 1);
}
