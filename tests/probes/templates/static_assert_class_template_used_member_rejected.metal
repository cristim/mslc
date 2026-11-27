// EXPECT: error static assertion failed: two bytes
// The member is used, so its assertion is checked for W<float>, and fails.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct W { T v; T get() const { static_assert(sizeof(T) == 2, "two bytes"); return v; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { W<float> w{1.0f}; out[i] = w.get(); }
