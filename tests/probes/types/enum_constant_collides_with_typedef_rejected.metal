// EXPECT: error redefinition of "A"
//
// Apple: "redefinition of 'A' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
typedef float A;
enum One { A };
kernel void enum_constant_collides_with_typedef_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
