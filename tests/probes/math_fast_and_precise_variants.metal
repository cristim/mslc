// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=192
// The fast and precise variants of the library's functions are the functions themselves, on float:
// metal::fast::normalize, fast::length (through "using namespace metal;"), metal::precise::sqrt,
// precise::distance, fast::select, fast::clamp and precise::saturate. Thread i normalizes (3, 0, 4)
// scaled by i + 1 to (0.6, 0, 0.8), takes the length and distance 5(i + 1), the square root of (i + 1)^2,
// selects 7, clamps i + 1 to [2, 4] and saturates 1.5 to 1. The scopes declare float overloads only, so
// integer and half scalars convert to float: fast::clamp(int(i) + 5, 0, 3) / 2 is 1.5 (an int clamp would
// give 1), and precise::sqrt of a half is a float of 4 bytes.
#include <metal_stdlib>
using namespace metal;
kernel void math_fast_and_precise_variants(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  const float s = float(i + 1);
  const float3 v = float3(3.0f, 0.0f, 4.0f) * s;
  const float3 n = metal::fast::normalize(v);
  out[i * 12 + 0] = n.x;
  out[i * 12 + 1] = n.y;
  out[i * 12 + 2] = n.z;
  out[i * 12 + 3] = fast::length(v);
  out[i * 12 + 4] = metal::precise::sqrt(s * s);
  out[i * 12 + 5] = precise::distance(v, float3(0.0f));
  out[i * 12 + 6] = fast::select(3.0f, 7.0f, s > 0.0f);
  out[i * 12 + 7] = fast::clamp(s, 2.0f, 4.0f);
  out[i * 12 + 8] = precise::saturate(1.5f);
  out[i * 12 + 9] = metal::fast::clamp(int(i) + 5, 0, 3) / 2;
  const half h = half(s * s);
  auto r = metal::precise::sqrt(h);
  out[i * 12 + 10] = float(sizeof(r));
  out[i * 12 + 11] = fast::select(1, 2, true) / 4;
}
