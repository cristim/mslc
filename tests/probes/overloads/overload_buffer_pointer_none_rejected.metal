// EXPECT: error no matching function for call to "get"
// A buffer of float reaches no pointer to another element type, and a const buffer no pointer that could write.
#include <metal_stdlib>
using namespace metal;
inline float get(device float* p) { return p[0]; }
inline float get(const device uint* p) { return float(p[0]); }
kernel void k(device float* out [[buffer(0)]], const device float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { out[i] = get(in); }
