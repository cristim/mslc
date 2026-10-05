// EXPECT: error a variable named "A" would hide the enum constant
//
// Same for a parameter.
#include <metal_stdlib>
using namespace metal;
enum One { A };
kernel void enum_constant_shadowed_by_parameter_rejected(device uint* out [[buffer(0)]], uint A [[thread_position_in_grid]])
{ out[0] = A; }
