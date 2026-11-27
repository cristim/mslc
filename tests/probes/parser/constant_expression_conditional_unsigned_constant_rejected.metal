// EXPECT: error would wrap as an unsigned one
// As constant_expression_conditional_unsigned_other_value_rejected, with a named uint constant.
#include <metal_stdlib>
using namespace metal;
constant uint U = 1;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { float a[(false ? U : -1) > 0 ? 4 : 2]; out[i] = float(sizeof(a)/4); }
