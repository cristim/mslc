// EXPECT: error most vexing parse
// An array declarator after a parenthesised name. Apple: "array size is not a constant expression".
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  float3 v(float((o)[gid]));
  o[0] = v.x;
}
