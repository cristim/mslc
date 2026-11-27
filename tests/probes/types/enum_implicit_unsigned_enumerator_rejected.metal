// EXPECT: error would wrap as an unsigned one
// An enumerator without an initializer has the type of the one before it inside the enum, so B is unsigned
// and B - 9 wraps.
#include <metal_stdlib>
using namespace metal;
enum E { A = 3u, B, C = B - 9 };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = float(C); }
