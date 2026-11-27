// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// vec<T, N> with a template parameter as T, in a class template's field and a function template's
// parameter, instantiated with float and int.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Pair { vec<T, 4> lo; vec<T, 4> hi; };
template <typename T> static T total(vec<T, 4> v) { return v.x + v.y + v.z + v.w; }
kernel void k(device const float4* in [[buffer(0)]], device float* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  Pair<float> pf; pf.lo = in[i]; pf.hi = in[i] * 2.0f;
  Pair<int> pi; pi.lo = int4(in[i]); pi.hi = int4(3);
  out[i] = total(pf.lo) + total(pf.hi) + float(total(pi.lo) + total(pi.hi));
}
