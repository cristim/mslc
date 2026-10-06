#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
float twice(float x) { return x + x; }
namespace N {
  constant float k = 5.0;
  float twice(float x) { return x * 2.5; }
  float both(float x) { float v = x; return ::twice(v) + twice(v) + N::k + ::k; }
}
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = N::both(float(i)) + k + twice(1.0); }
