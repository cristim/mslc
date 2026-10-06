#include <metal_stdlib>
using namespace metal;
namespace
{
  constant float kGain = 3.0;
  float boost(float x) { return x * kGain; }
}  // namespace
namespace { float again(float x) { return boost(x) + kGain; } }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = boost(float(i)) + again(float(i)) + kGain; }
