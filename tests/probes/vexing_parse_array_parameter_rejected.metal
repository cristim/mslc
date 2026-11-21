// EXPECT: error most vexing parse
// float(o[gid]) is a parameter with an array declarator, so this is a function declaration. Apple refuses it
// ("array size is not a constant expression").
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  float3 v(float(o[gid]));
  o[0] = v.x;
}
