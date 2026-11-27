// EXPECT: valid
// A member function of a class template is instantiated only where it is used ([temp.inst]): the assertion
// in W<float>::get is not checked, as get is never called.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct W { T v; T get() const { static_assert(sizeof(T) == 2); return v; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { W<float> w{1.0f}; out[i] = w.v; }
