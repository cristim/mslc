// EXPECT: valid
// RUN: grid=1 local=1
// An index after the cast and more arguments, or an operator after it, is an expression: a variable.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  float3 v(float(o[gid]), 1.0, 2.0);
  float3 w(float(o[gid]) * 2.0);
  o[0] = v.x + w.x;
}
