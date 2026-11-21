// EXPECT: error is not a return type and a name that mslc reads when it is instantiated
#include <metal_stdlib>
using namespace metal;
template <typename T> A<B<T>> hh(T x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = hh(1.0f); }
