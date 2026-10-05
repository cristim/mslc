// EXPECT: error not an integer constant expression
//
// Apple: "integral constant expression must have integral or unscoped enumeration type, not 'float'".
#include <metal_stdlib>
using namespace metal;
enum One { A = 1.5 };
kernel void enum_value_float_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
