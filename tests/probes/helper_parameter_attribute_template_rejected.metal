// EXPECT: error has an attribute, [[texture(n)]], that its type
// QUIRK (helper parameter attributes): in a function template the check is made on the instance, where
// Apple's compiler reports a substitution failure for T = float.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x [[texture(0)]]) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(1.0f); }
