// EXPECT: error division by zero in a constant expression
//
// Apple: "expression is not an integral constant expression".
#include <metal_stdlib>
using namespace metal;
enum One { A = 1 / 0 };
kernel void enum_value_division_by_zero_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
