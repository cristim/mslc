// EXPECT: error a shift in a constant expression has a negative operand
//
// Apple accepts a left shift of a negative value (defined in C++20); mslc folds only shifts with a non-negative operand.
#include <metal_stdlib>
using namespace metal;
enum One { A = -1 << 1 };
kernel void enum_value_shift_of_negative_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
