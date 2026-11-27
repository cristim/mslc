// EXPECT: error no matching function for call to "f"
// QUIRK (helper parameter attributes): in a function template the check is made on the instance, as a
// substitution failure, as Apple's compiler reports for T = float ("candidate template ignored").
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x [[texture(0)]]) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(1.0f); }
