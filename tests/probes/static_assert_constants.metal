// EXPECT: valid
// static_assert over sizeof, a named constant and adjacent message literals.
#include <metal_stdlib>
using namespace metal;
static_assert(sizeof(float4) == 16, "vec" " layout");
constant int N = 3;
static_assert(N * 2 == 6);
struct S { float a; int b; };
static_assert(sizeof(S) == 8, "S");
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = 1; }
