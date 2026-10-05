// EXPECT: error a shift in a constant expression has a negative operand
//
// Apple accepts a shift by 32 or more with a warning. mslc folds only the shifts that have a value in an int.
#include <metal_stdlib>
using namespace metal;
enum One { A = 1 << 32 };
kernel void enum_value_bad_shift_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
