// EXPECT: error a variable named "A" would hide the enum constant
//
// Apple accepts the loop variable; mslc reports it, as for any other variable.
#include <metal_stdlib>
using namespace metal;
enum One { A };
kernel void enum_constant_shadowed_by_loop_variable_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ for (uint A = 0; A < 2; ++A) { out[i] = A; } }
