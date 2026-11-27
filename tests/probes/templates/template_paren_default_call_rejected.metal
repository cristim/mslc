// EXPECT: error the default argument of the template parameter "N" of "pl" is not lowered
#include <metal_stdlib>
using namespace metal;
template <int N = (1 > 0) + int(2), typename T> T pl(T x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = pl(1.0f); }
