// EXPECT: error templates: template f(vec<T, 4>)
// A scalar argument makes vec<T, 4> no candidate, and the diagnostic names the parameter as written.
#include <metal_stdlib>
using namespace metal;
template <typename T> static T f(vec<T, 4> a) { return a.x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(1.0f); }
