// EXPECT: error a variable named "A" would hide the enum constant
//
// Apple accepts the local; mslc cannot tell the two apart without scopes in the parser, so it reports it.
#include <metal_stdlib>
using namespace metal;
enum One { A };
kernel void enum_constant_shadowed_by_local_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ uint A = 2; out[i] = A; }
