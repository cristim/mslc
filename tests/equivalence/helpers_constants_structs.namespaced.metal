#include <metal_stdlib>
using namespace metal;
namespace geo {
  constant float kScale = 2.0;
  struct Pair { float a; float b; };
  float scaled(float x) { return x * kScale; }
  Pair make(float x) { Pair p; p.a = scaled(x); p.b = x; return p; }
}
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  geo::Pair p = geo::make(float(i));
  out[i] = p.a + p.b + geo::kScale;
}
